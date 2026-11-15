// MerchantComponent.cpp

#include "Shop/MerchantComponent.h"
#include "RoomFlow/RoomFlowSubsystem.h"
#include "Loot/PathfinderLootTypes.h"
#include "Accessory/AccessoryManagerComponent.h"
#include "Currency/CurrencyManagerComponent.h"
#include "Currency/PathfinderEconomySettings.h"
#include "Meta/PathfinderMetaSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogShop, Log, All);

UMerchantComponent::UMerchantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// "Relatively cheap" starting prices. Tune in BP_MerchantNPC's component details.
	BasePrices.Add(EItemRarity::Common, 20);
	BasePrices.Add(EItemRarity::Uncommon, 35);
	BasePrices.Add(EItemRarity::Rare, 60);
	BasePrices.Add(EItemRarity::Unique, 100);
	BasePrices.Add(EItemRarity::Legendary, 160);
}

void UMerchantComponent::BeginPlay()
{
	Super::BeginPlay();
	FallbackStream.Initialize(FMath::Rand());
}

// ---------------------------------------------------------------- Lookup

UMerchantComponent* UMerchantComponent::FindMerchant(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UMerchantComponent* Merchant = It->FindComponentByClass<UMerchantComponent>())
		{
			return Merchant;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------- Stage / prices

FRandomStream& UMerchantComponent::Rng()
{
	if (const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (URoomFlowSubsystem* RoomFlow = GI->GetSubsystem<URoomFlowSubsystem>())
		{
			if (RoomFlow->IsRunActive())
			{
				return RoomFlow->GetRandomStream();
			}
		}
	}
	return FallbackStream;
}

int32 UMerchantComponent::GetStage() const
{
	if (const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const URoomFlowSubsystem* RoomFlow = GI->GetSubsystem<URoomFlowSubsystem>())
		{
			if (RoomFlow->IsRunActive())
			{
				return FMath::Max(1, RoomFlow->GetOverallStageNumber());
			}
		}
	}
	return 1;
}

float UMerchantComponent::GetCostScale() const
{
	// Same curve as chest prices: (GrowthPerStage ^ (Stage - 1)) ^ CostExponent.
	const UPathfinderEconomySettings* Economy = GetDefault<UPathfinderEconomySettings>();
	const float StageScale = FMath::Pow(Economy->GrowthPerStage, static_cast<float>(GetStage() - 1));
	return FMath::Pow(StageScale, Economy->CostExponent);
}

int32 UMerchantComponent::GetPrice(EItemRarity Rarity) const
{
	const int32* Base = BasePrices.Find(Rarity);
	return FMath::RoundToInt((Base ? *Base : 0) * GetCostScale());
}

int32 UMerchantComponent::GetFreeRerollsPerShop() const
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPathfinderMetaSubsystem* Meta = GI ? GI->GetSubsystem<UPathfinderMetaSubsystem>() : nullptr;
	return Meta ? FMath::Max(0, FMath::RoundToInt(Meta->GetEffectTotal(EMetaUpgradeEffect::FreeShopRerolls))) : 0;
}

int32 UMerchantComponent::GetFreeRerollsRemaining() const
{
	return FMath::Max(0, GetFreeRerollsPerShop() - RerollsUsed);
}

int32 UMerchantComponent::GetRerollCost() const
{
	// Free rerolls from the Enchantress are used first.
	const int32 FreePerShop = GetFreeRerollsPerShop();
	if (RerollsUsed < FreePerShop)
	{
		return 0;
	}

	// Escalation only counts PAID rerolls, so the first paid reroll is the normal base cost.
	const int32 PaidRerollsUsed = RerollsUsed - FreePerShop;
	const float Cost = RerollBaseCost * GetCostScale() * FMath::Pow(RerollCostMultiplier, static_cast<float>(PaidRerollsUsed));
	return FMath::RoundToInt(Cost);
}

bool UMerchantComponent::CanAfford(int32 Cost, const AActor* Buyer)
{
	if (Cost <= 0)
	{
		return true;
	}
	const UCurrencyManagerComponent* Wallet = UCurrencyManagerComponent::FindCurrencyManager(Buyer);
	return Wallet && Wallet->HasEnoughGold(Cost);
}

// ---------------------------------------------------------------- Rolling

EItemRarity UMerchantComponent::RollRarity()
{
	if (!RarityWeightTable)
	{
		UE_LOG(LogShop, Warning, TEXT("Rarity Weight Table not set on the merchant; using Common."));
		return EItemRarity::Common;
	}

	// Highest "Shop_StageN" row at or below the current stage.
	const FRarityWeights* Row = nullptr;
	for (int32 Stage = GetStage(); Stage >= 1 && !Row; --Stage)
	{
		const FName RowName(*FString::Printf(TEXT("%s%d"), *RarityRowPrefix, Stage));
		Row = RarityWeightTable->FindRow<FRarityWeights>(RowName, TEXT("Shop RollRarity"), false);
	}
	if (!Row)
	{
		UE_LOG(LogShop, Warning, TEXT("No '%s1' row in the rarity table; using Common."), *RarityRowPrefix);
		return EItemRarity::Common;
	}

	const float Weights[5] = { Row->Common, Row->Uncommon, Row->Rare, Row->Unique, Row->Legendary };
	const EItemRarity Rarities[5] = { EItemRarity::Common, EItemRarity::Uncommon, EItemRarity::Rare, EItemRarity::Unique, EItemRarity::Legendary };

	float Total = 0.f;
	for (float W : Weights) { Total += FMath::Max(0.f, W); }
	if (Total <= 0.f)
	{
		return EItemRarity::Common;
	}

	float Roll = Rng().FRandRange(0.f, Total);
	for (int32 i = 0; i < 5; ++i)
	{
		Roll -= FMath::Max(0.f, Weights[i]);
		if (Roll <= 0.f)
		{
			return Rarities[i];
		}
	}
	return EItemRarity::Common;
}

bool UMerchantComponent::IsMaxedForPlayer(FName AccessoryId, const FAccessoryData& Row) const
{
	if (Row.MaxStacks <= 0)
	{
		return false;
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UAccessoryManagerComponent* Accessories = Player ? Player->FindComponentByClass<UAccessoryManagerComponent>() : nullptr;
	return Accessories && Accessories->GetStackCount(AccessoryId) >= Row.MaxStacks;
}

bool UMerchantComponent::HasShopItems(EShopCategory Category) const
{
	if (!AccessoryTable)
	{
		return false;
	}
	bool bAny = false;
	AccessoryTable->ForeachRow<FAccessoryData>(TEXT("HasShopItems"), [&](const FName&, const FAccessoryData& Row)
	{
		bAny |= (Row.bInShopPool && Row.ShopCategory == Category && Row.DropWeight > 0.f);
	});
	return bAny;
}

bool UMerchantComponent::PickAccessory(EShopCategory Category, EItemRarity StartRarity, const TSet<FName>& Exclude,
	FName& OutId, EItemRarity& OutRarity)
{
	if (!AccessoryTable)
	{
		UE_LOG(LogShop, Error, TEXT("Accessory Table not set on the merchant."));
		return false;
	}

	// Step down from the rolled rarity toward Common until something is available (never steps up,
	// so a stage without Legendary odds can't produce one).
	for (int32 R = static_cast<int32>(StartRarity); R >= static_cast<int32>(EItemRarity::Common); --R)
	{
		const EItemRarity Rarity = static_cast<EItemRarity>(R);

		TArray<FName> Ids;
		TArray<float> Weights;
		float Total = 0.f;

		AccessoryTable->ForeachRow<FAccessoryData>(TEXT("Shop PickAccessory"), [&](const FName& Key, const FAccessoryData& Row)
		{
			if (!Row.bInShopPool || Row.ShopCategory != Category || Row.Rarity != Rarity || Row.DropWeight <= 0.f) return;
			if (Exclude.Contains(Key) || IsMaxedForPlayer(Key, Row)) return;
			Ids.Add(Key);
			Weights.Add(Row.DropWeight);
			Total += Row.DropWeight;
		});

		if (Ids.Num() == 0)
		{
			continue;
		}

		float Roll = Rng().FRandRange(0.f, Total);
		int32 Picked = Ids.Num() - 1;
		for (int32 i = 0; i < Ids.Num(); ++i)
		{
			Roll -= Weights[i];
			if (Roll <= 0.f) { Picked = i; break; }
		}

		OutId = Ids[Picked];
		OutRarity = Rarity;
		return true;
	}
	return false;
}

void UMerchantComponent::EnsureStock()
{
	if (!bStockRolled)
	{
		RollStock();
	}
}

void UMerchantComponent::RollStock()
{
	bStockRolled = true;
	RerollsUsed = 0;
	Cards.Reset();

	TArray<EShopCategory> Slots = { EShopCategory::Ranged, EShopCategory::Melee, EShopCategory::Stat };
	if (GetStage() >= AbilityCardFromStage && HasShopItems(EShopCategory::Ability))
	{
		Slots.Add(EShopCategory::Ability);
	}

	TSet<FName> Taken;
	for (EShopCategory Category : Slots)
	{
		FShopCard Card;
		Card.Category = Category;

		FName Id;
		EItemRarity Rarity;
		if (PickAccessory(Category, RollRarity(), Taken, Id, Rarity))
		{
			Card.AccessoryId = Id;
			Card.Rarity = Rarity;
			Card.Price = GetPrice(Rarity);
			Taken.Add(Id);
		}
		else
		{
			UE_LOG(LogShop, Warning, TEXT("No shop items for category %s; card left empty."), *UEnum::GetValueAsString(Category));
		}
		Cards.Add(Card);
	}

	UE_LOG(LogShop, Log, TEXT("Shop stocked: %d cards (stage %d)."), Cards.Num(), GetStage());
	OnShopChanged.Broadcast();
}

const TArray<FShopCard>& UMerchantComponent::GetCards()
{
	EnsureStock();
	return Cards;
}

// ---------------------------------------------------------------- Buying / rerolling

bool UMerchantComponent::CanReroll(int32 CardIndex) const
{
	return Cards.IsValidIndex(CardIndex)
		&& !Cards[CardIndex].bSold
		&& !Cards[CardIndex].bRerolled
		&& !Cards[CardIndex].AccessoryId.IsNone();
}

bool UMerchantComponent::TryPurchase(int32 CardIndex, AActor* Buyer)
{
	EnsureStock();
	if (!Cards.IsValidIndex(CardIndex) || !AccessoryTable)
	{
		return false;
	}

	FShopCard& Card = Cards[CardIndex];
	if (Card.bSold || Card.AccessoryId.IsNone())
	{
		return false;
	}

	UCurrencyManagerComponent* Wallet = UCurrencyManagerComponent::FindCurrencyManager(Buyer);
	UAccessoryManagerComponent* Accessories = Buyer ? Buyer->FindComponentByClass<UAccessoryManagerComponent>() : nullptr;
	const FAccessoryData* Row = AccessoryTable->FindRow<FAccessoryData>(Card.AccessoryId, TEXT("TryPurchase"));
	if (!Wallet || !Accessories || !Row || IsMaxedForPlayer(Card.AccessoryId, *Row))
	{
		return false;
	}

	if (!Wallet->TrySpendGold(Card.Price))
	{
		return false;
	}

	if (!Accessories->AddAccessory(Card.AccessoryId))
	{
		Wallet->AddGold(Card.Price); // refund; shouldn't happen after the checks above
		return false;
	}

	Card.bSold = true;
	UE_LOG(LogShop, Log, TEXT("Bought '%s' for %d."), *Card.AccessoryId.ToString(), Card.Price);
	OnShopChanged.Broadcast();
	return true;
}

bool UMerchantComponent::TryReroll(int32 CardIndex, AActor* Buyer)
{
	EnsureStock();
	if (!CanReroll(CardIndex))
	{
		return false;
	}

	UCurrencyManagerComponent* Wallet = UCurrencyManagerComponent::FindCurrencyManager(Buyer);
	if (!Wallet)
	{
		return false;
	}

	const int32 Cost = GetRerollCost();
	if (Cost > 0 && !Wallet->HasEnoughGold(Cost))
	{
		return false;
	}

	// Never give back the same item or one already on another card.
	TSet<FName> Exclude;
	for (const FShopCard& Other : Cards)
	{
		if (!Other.AccessoryId.IsNone())
		{
			Exclude.Add(Other.AccessoryId);
		}
	}

	FShopCard& Card = Cards[CardIndex];
	FName NewId;
	EItemRarity NewRarity;
	if (!PickAccessory(Card.Category, RollRarity(), Exclude, NewId, NewRarity))
	{
		UE_LOG(LogShop, Log, TEXT("Reroll: no other %s items available; nothing spent."), *UEnum::GetValueAsString(Card.Category));
		return false;
	}

	// Free rerolls (Cost 0) skip the wallet entirely.
	if (Cost > 0 && !Wallet->TrySpendGold(Cost))
	{
		return false;
	}

	UE_LOG(LogShop, Log, TEXT("Rerolled '%s' -> '%s' for %d%s."), *Card.AccessoryId.ToString(), *NewId.ToString(), Cost,
		Cost == 0 ? TEXT(" (free)") : TEXT(""));

	Card.AccessoryId = NewId;
	Card.Rarity = NewRarity;
	Card.Price = GetPrice(NewRarity);
	Card.bRerolled = true;
	++RerollsUsed;

	OnShopChanged.Broadcast();
	return true;
}
