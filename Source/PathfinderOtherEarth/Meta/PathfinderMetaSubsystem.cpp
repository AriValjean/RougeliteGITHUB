// PathfinderMetaSubsystem.cpp

#include "Meta/PathfinderMetaSubsystem.h"
#include "Meta/PathfinderMetaSettings.h"
#include "Meta/PathfinderMetaSave.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "RoomFlow/RoomFlowSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogMeta, Log, All);

// ======================= Lifetime =======================

void UPathfinderMetaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UPathfinderMetaSettings* Settings = GetDefault<UPathfinderMetaSettings>();
	CurrencyTable = Settings->CurrencyTable.LoadSynchronous();
	UpgradeTable = Settings->MetaUpgradeTable.LoadSynchronous();

	if (!CurrencyTable)
	{
		UE_LOG(LogMeta, Warning, TEXT("No Currency Table set in Project Settings > Game > Meta Progression."));
	}
	if (!UpgradeTable)
	{
		UE_LOG(LogMeta, Warning, TEXT("No Meta Upgrade Table set in Project Settings > Game > Meta Progression."));
	}

	bool bSkipLoad = false;
#if WITH_EDITOR
	if (GIsEditor && GetDefault<UPathfinderMetaSettings>()->bFreshSaveEachPIESession)
	{
		bSkipLoad = true;
		UE_LOG(LogMeta, Log, TEXT("Fresh Save Each PIE Session is on: starting as a new player (save '%s' not loaded)."), *GetSaveSlotName());
	}
#endif
	if (!bSkipLoad)
	{
		LoadMeta();
	}

	RegisterConsoleCommands();

	// Per-run currencies (Gold) start at their Starting Amount, also when PIE starts straight in a run level.
	ResetRunCurrencies(false);

	// So Death Defiance works even when PIE starts straight in a run level.
	RefillDeathDefiance();
}

void UPathfinderMetaSubsystem::Deinitialize()
{
	UnregisterConsoleCommands();

	// Game is closing: save synchronously so the write finishes before shutdown.
	SaveMeta(false);
	Super::Deinitialize();
}

// ======================= Save / Load =======================

void UPathfinderMetaSubsystem::LoadMeta()
{
	const FString Slot = GetSaveSlotName();

	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UE_LOG(LogMeta, Log, TEXT("No meta save '%s' yet, starting fresh."), *Slot);
		return;
	}

	UPathfinderMetaSave* Save = Cast<UPathfinderMetaSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Save)
	{
		UE_LOG(LogMeta, Warning, TEXT("Meta save '%s' exists but could not be loaded."), *Slot);
		return;
	}

	Currencies = Save->Currencies;

	// Copy ranks, dropping upgrades that no longer exist and clamping to the table's current max rank.
	UpgradeRanks.Reset();
	for (const TPair<FName, int32>& Pair : Save->UpgradeRanks)
	{
		if (const FMetaUpgradeRow* Row = FindUpgrade(Pair.Key, false))
		{
			const int32 Rank = FMath::Clamp(Pair.Value, 0, Row->RankCosts.Num());
			if (Rank > 0)
			{
				UpgradeRanks.Add(Pair.Key, Rank);
			}
		}
	}

	UE_LOG(LogMeta, Log, TEXT("Loaded meta save '%s'."), *Slot);
}

void UPathfinderMetaSubsystem::SaveMeta(bool bAsync)
{
	UPathfinderMetaSave* Save = Cast<UPathfinderMetaSave>(UGameplayStatics::CreateSaveGameObject(UPathfinderMetaSave::StaticClass()));
	if (!Save)
	{
		return;
	}

	// Only permanent currencies go to disk.
	for (const TPair<FName, int32>& Pair : Currencies)
	{
		const FCurrencyRow* Row = FindCurrency(Pair.Key, false);
		if (Row && !Row->bResetsOnRunEnd)
		{
			Save->Currencies.Add(Pair.Key, Pair.Value);
		}
	}
	Save->UpgradeRanks = UpgradeRanks;

	const FString Slot = GetSaveSlotName();
	if (bAsync)
	{
		UGameplayStatics::AsyncSaveGameToSlot(Save, Slot, 0);
	}
	else
	{
		UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	}
}

// ======================= Lookups =======================

const FCurrencyRow* UPathfinderMetaSubsystem::FindCurrency(FName CurrencyId, bool bWarnIfMissing) const
{
	return CurrencyTable ? CurrencyTable->FindRow<FCurrencyRow>(CurrencyId, TEXT("MetaCurrency"), bWarnIfMissing) : nullptr;
}

const FMetaUpgradeRow* UPathfinderMetaSubsystem::FindUpgrade(FName UpgradeId, bool bWarnIfMissing) const
{
	return UpgradeTable ? UpgradeTable->FindRow<FMetaUpgradeRow>(UpgradeId, TEXT("MetaUpgrade"), bWarnIfMissing) : nullptr;
}

// ======================= Currency =======================

int32 UPathfinderMetaSubsystem::GetCurrency(FName CurrencyId) const
{
	return Currencies.FindRef(CurrencyId);
}

int32 UPathfinderMetaSubsystem::GetEarnedThisRun(FName CurrencyId) const
{
	return EarnedThisRun.FindRef(CurrencyId);
}

bool UPathfinderMetaSubsystem::GetCurrencyData(FName CurrencyId, FCurrencyRow& OutRow) const
{
	if (const FCurrencyRow* Row = FindCurrency(CurrencyId, false))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

void UPathfinderMetaSubsystem::AddCurrency(FName CurrencyId, int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	const FCurrencyRow* Row = FindCurrency(CurrencyId);
	if (!Row)
	{
		UE_LOG(LogMeta, Warning, TEXT("AddCurrency: '%s' is not a row in the currency table."), *CurrencyId.ToString());
		return;
	}

	int32& Value = Currencies.FindOrAdd(CurrencyId);
	Value += Amount;
	EarnedThisRun.FindOrAdd(CurrencyId) += Amount;

	OnCurrencyChanged.Broadcast(CurrencyId, Value, Amount);
	OnCurrencyEarned.Broadcast(CurrencyId, Amount);

	UE_LOG(LogMeta, Log, TEXT("Earned %d %s (total %d, this run %d)."), Amount, *CurrencyId.ToString(), Value, EarnedThisRun.FindRef(CurrencyId));

	if (!Row->bResetsOnRunEnd)
	{
		SaveMeta(true);
	}
}

bool UPathfinderMetaSubsystem::TrySpendCurrency(FName CurrencyId, int32 Amount)
{
	if (Amount < 0)
	{
		return false;
	}
	if (Amount == 0)
	{
		return true;
	}

	int32* Value = Currencies.Find(CurrencyId);
	if (!Value || *Value < Amount)
	{
		return false;
	}

	*Value -= Amount;
	OnCurrencyChanged.Broadcast(CurrencyId, *Value, -Amount);

	const FCurrencyRow* Row = FindCurrency(CurrencyId, false);
	if (Row && !Row->bResetsOnRunEnd)
	{
		SaveMeta(true);
	}
	return true;
}

void UPathfinderMetaSubsystem::SetCurrency(FName CurrencyId, int32 NewAmount)
{
	const FCurrencyRow* Row = FindCurrency(CurrencyId);
	if (!Row)
	{
		UE_LOG(LogMeta, Warning, TEXT("SetCurrency: '%s' is not a row in the currency table."), *CurrencyId.ToString());
		return;
	}

	NewAmount = FMath::Max(0, NewAmount);
	int32& Value = Currencies.FindOrAdd(CurrencyId);
	if (Value == NewAmount)
	{
		return;
	}

	const int32 Delta = NewAmount - Value;
	Value = NewAmount;
	OnCurrencyChanged.Broadcast(CurrencyId, Value, Delta);

	if (!Row->bResetsOnRunEnd)
	{
		SaveMeta(true);
	}
}

// ======================= Upgrades =======================

TArray<FName> UPathfinderMetaSubsystem::GetAllUpgradeIds() const
{
	TArray<FName> Ids;
	if (!UpgradeTable)
	{
		return Ids;
	}

	Ids = UpgradeTable->GetRowNames();
	Ids.Sort([this](const FName& A, const FName& B)
	{
		const FMetaUpgradeRow* RowA = FindUpgrade(A, false);
		const FMetaUpgradeRow* RowB = FindUpgrade(B, false);
		const int32 SortA = RowA ? RowA->SortOrder : 0;
		const int32 SortB = RowB ? RowB->SortOrder : 0;
		return SortA < SortB;
	});
	return Ids;
}

bool UPathfinderMetaSubsystem::GetUpgradeData(FName UpgradeId, FMetaUpgradeRow& OutRow) const
{
	if (const FMetaUpgradeRow* Row = FindUpgrade(UpgradeId, false))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

int32 UPathfinderMetaSubsystem::GetUpgradeRank(FName UpgradeId) const
{
	return UpgradeRanks.FindRef(UpgradeId);
}

int32 UPathfinderMetaSubsystem::GetMaxRank(FName UpgradeId) const
{
	const FMetaUpgradeRow* Row = FindUpgrade(UpgradeId, false);
	return Row ? Row->RankCosts.Num() : 0;
}

int32 UPathfinderMetaSubsystem::GetNextRankCost(FName UpgradeId) const
{
	const FMetaUpgradeRow* Row = FindUpgrade(UpgradeId, false);
	if (!Row)
	{
		return -1;
	}

	const int32 Rank = GetUpgradeRank(UpgradeId);
	return Row->RankCosts.IsValidIndex(Rank) ? Row->RankCosts[Rank] : -1;
}

float UPathfinderMetaSubsystem::GetUpgradeValue(FName UpgradeId) const
{
	const FMetaUpgradeRow* Row = FindUpgrade(UpgradeId, false);
	const int32 Rank = GetUpgradeRank(UpgradeId);
	if (!Row || Rank <= 0)
	{
		return 0.f;
	}
	return Row->RankValues.IsValidIndex(Rank - 1) ? Row->RankValues[Rank - 1] : 0.f;
}

bool UPathfinderMetaSubsystem::CanPurchaseUpgrade(FName UpgradeId) const
{
	const FMetaUpgradeRow* Row = FindUpgrade(UpgradeId, false);
	const int32 Cost = GetNextRankCost(UpgradeId);
	return Row && Cost >= 0 && GetCurrency(Row->CostCurrency) >= Cost;
}

bool UPathfinderMetaSubsystem::TryPurchaseUpgrade(FName UpgradeId)
{
	if (!CanPurchaseUpgrade(UpgradeId))
	{
		return false;
	}

	const FMetaUpgradeRow* Row = FindUpgrade(UpgradeId);
	if (!TrySpendCurrency(Row->CostCurrency, GetNextRankCost(UpgradeId)))
	{
		return false;
	}

	const int32 NewRank = ++UpgradeRanks.FindOrAdd(UpgradeId);
	OnUpgradeChanged.Broadcast(UpgradeId, NewRank);
	SaveMeta(true);

	if (Row->Effect == EMetaUpgradeEffect::DeathDefiance)
	{
		RefillDeathDefiance();
	}
	return true;
}

int32 UPathfinderMetaSubsystem::RefundAllUpgrades()
{
	// Add up what was paid for every rank, per currency.
	TMap<FName, int32> Refunds;
	for (const TPair<FName, int32>& Pair : UpgradeRanks)
	{
		const FMetaUpgradeRow* Row = FindUpgrade(Pair.Key, false);
		if (!Row)
		{
			continue;
		}
		const int32 RanksPaid = FMath::Min(Pair.Value, Row->RankCosts.Num());
		for (int32 i = 0; i < RanksPaid; ++i)
		{
			Refunds.FindOrAdd(Row->CostCurrency) += Row->RankCosts[i];
		}
	}

	TArray<FName> ResetIds;
	UpgradeRanks.GetKeys(ResetIds);
	UpgradeRanks.Reset();

	for (const FName& Id : ResetIds)
	{
		OnUpgradeChanged.Broadcast(Id, 0);
	}

	// A refund is not "earned", so it does not count toward the HUD's earned-this-run number.
	int32 TotalRefunded = 0;
	for (const TPair<FName, int32>& Pair : Refunds)
	{
		int32& Value = Currencies.FindOrAdd(Pair.Key);
		Value += Pair.Value;
		TotalRefunded += Pair.Value;
		OnCurrencyChanged.Broadcast(Pair.Key, Value, Pair.Value);
	}

	SaveMeta(true);
	RefillDeathDefiance();
	return TotalRefunded;
}

float UPathfinderMetaSubsystem::GetEffectTotal(EMetaUpgradeEffect Effect) const
{
	float Total = 0.f;
	for (const TPair<FName, int32>& Pair : UpgradeRanks)
	{
		const FMetaUpgradeRow* Row = FindUpgrade(Pair.Key, false);
		if (Row && Row->Effect == Effect)
		{
			Total += GetUpgradeValue(Pair.Key);
		}
	}
	return Total;
}

float UPathfinderMetaSubsystem::GetStatBonus(ECharacterStatType StatType, EModifierMode Mode) const
{
	float Total = 0.f;
	for (const TPair<FName, int32>& Pair : UpgradeRanks)
	{
		const FMetaUpgradeRow* Row = FindUpgrade(Pair.Key, false);
		if (Row && Row->Effect == EMetaUpgradeEffect::StatBonus && Row->StatType == StatType && Row->ModifierMode == Mode)
		{
			Total += GetUpgradeValue(Pair.Key);
		}
	}
	return Total;
}

// ======================= Run =======================

void UPathfinderMetaSubsystem::BeginRun()
{
	// Clear "earned this run" first, so widgets refreshing on the broadcasts below read 0.
	EarnedThisRun.Reset();

	// Gold and any other per-run currency go back to their Starting Amount.
	ResetRunCurrencies(true);
	RefillDeathDefiance();

	// Make every currency widget refresh (e.g. the run Memoria counter hides itself again).
	BroadcastAllCurrencies();
}

void UPathfinderMetaSubsystem::BroadcastAllCurrencies()
{
	for (const TPair<FName, int32>& Pair : Currencies)
	{
		OnCurrencyChanged.Broadcast(Pair.Key, Pair.Value, 0);
	}
}

bool UPathfinderMetaSubsystem::IsRunActive() const
{
	const UGameInstance* GI = GetGameInstance();
	const URoomFlowSubsystem* RoomFlow = GI ? GI->GetSubsystem<URoomFlowSubsystem>() : nullptr;
	return RoomFlow && RoomFlow->IsRunActive();
}

void UPathfinderMetaSubsystem::ResetRunCurrencies(bool bBroadcast)
{
	if (!CurrencyTable)
	{
		return;
	}

	CurrencyTable->ForeachRow<FCurrencyRow>(TEXT("MetaResetRunCurrencies"), [this, bBroadcast](const FName& Key, const FCurrencyRow& Row)
	{
		if (!Row.bResetsOnRunEnd)
		{
			return;
		}

		const int32 Old = Currencies.FindRef(Key);
		const int32 Start = FMath::Max(0, Row.StartingAmount);
		Currencies.Add(Key, Start);

		if (bBroadcast && Old != Start)
		{
			OnCurrencyChanged.Broadcast(Key, Start, Start - Old);
		}
	});
}

void UPathfinderMetaSubsystem::RefillDeathDefiance()
{
	DeathDefianceRemaining = FMath::RoundToInt(GetEffectTotal(EMetaUpgradeEffect::DeathDefiance));
	OnDeathDefianceChanged.Broadcast(DeathDefianceRemaining);
}

bool UPathfinderMetaSubsystem::TryConsumeDeathDefiance()
{
	if (DeathDefianceRemaining <= 0)
	{
		return false;
	}
	--DeathDefianceRemaining;
	OnDeathDefianceChanged.Broadcast(DeathDefianceRemaining);
	return true;
}

float UPathfinderMetaSubsystem::GetDeathDefianceHealFraction() const
{
	return GetDefault<UPathfinderMetaSettings>()->DeathDefianceHealFraction;
}

float UPathfinderMetaSubsystem::GetDeathDefianceInvulnerabilityTime() const
{
	return GetDefault<UPathfinderMetaSettings>()->DeathDefianceInvulnerabilityTime;
}

// ======================= Save slot / reset =======================

FString UPathfinderMetaSubsystem::GetSaveSlotName() const
{
	FString Slot = GetDefault<UPathfinderMetaSettings>()->SaveSlotName;
#if WITH_EDITOR
	// Play In Editor never touches a real player's save.
	if (GIsEditor)
	{
		Slot += TEXT("_PIE");
	}
#endif
	return Slot;
}

void UPathfinderMetaSubsystem::ResetAllProgress()
{
	// Clear "earned this run" first, so every widget that refreshes on the broadcasts below reads 0.
	EarnedThisRun.Reset();

	// Upgrades back to rank 0.
	TArray<FName> ResetIds;
	UpgradeRanks.GetKeys(ResetIds);
	UpgradeRanks.Reset();
	for (const FName& Id : ResetIds)
	{
		OnUpgradeChanged.Broadcast(Id, 0);
	}

	// Permanent currencies to 0. Set directly (not through SetCurrency) so no save is written
	// right before the file is deleted below.
	for (TPair<FName, int32>& Pair : Currencies)
	{
		const FCurrencyRow* Row = FindCurrency(Pair.Key, false);
		if (Row && !Row->bResetsOnRunEnd && Pair.Value != 0)
		{
			const int32 Old = Pair.Value;
			Pair.Value = 0;
			OnCurrencyChanged.Broadcast(Pair.Key, 0, -Old);
		}
	}

	// Per-run currencies back to their starting amount.
	ResetRunCurrencies(true);
	RefillDeathDefiance();

	// Make every currency widget refresh, even ones whose total didn't change.
	BroadcastAllCurrencies();

	const FString Slot = GetSaveSlotName();
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
	}

	UE_LOG(LogMeta, Warning, TEXT("All meta progress reset and save '%s' deleted."), *Slot);
	OnSaveReset.Broadcast();
}

// ======================= Debug console commands =======================

void UPathfinderMetaSubsystem::RegisterConsoleCommands()
{
#if !UE_BUILD_SHIPPING
	IConsoleManager& CM = IConsoleManager::Get();

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("Meta.ResetSave"),
		TEXT("Wipes all permanent progress and deletes the save file."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UPathfinderMetaSubsystem::Cmd_ResetSave),
		ECVF_Cheat));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("Meta.GiveMemoria"),
		TEXT("Meta.GiveMemoria <Amount> - adds Memoria."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UPathfinderMetaSubsystem::Cmd_GiveMemoria),
		ECVF_Cheat));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("Meta.GiveCurrency"),
		TEXT("Meta.GiveCurrency <CurrencyId> <Amount> - adds any currency row (e.g. Gold)."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UPathfinderMetaSubsystem::Cmd_GiveCurrency),
		ECVF_Cheat));
#endif
}

void UPathfinderMetaSubsystem::UnregisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		if (Cmd)
		{
			CM.UnregisterConsoleObject(Cmd);
		}
	}
	ConsoleCommands.Reset();
}

void UPathfinderMetaSubsystem::Cmd_ResetSave(const TArray<FString>& Args)
{
	ResetAllProgress();
}

void UPathfinderMetaSubsystem::Cmd_GiveMemoria(const TArray<FString>& Args)
{
	const int32 Amount = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 100;
	AddCurrency(TEXT("Memoria"), Amount);
}

void UPathfinderMetaSubsystem::Cmd_GiveCurrency(const TArray<FString>& Args)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogMeta, Warning, TEXT("Usage: Meta.GiveCurrency <CurrencyId> <Amount>"));
		return;
	}
	AddCurrency(FName(*Args[0]), FCString::Atoi(*Args[1]));
}
