// PathfinderMetaSubsystem.h
// Owns meta currencies (Memoria) and Enchantress upgrade ranks. Lives as long as the Game Instance,
// so it survives level travel; permanent data is written to a USaveGame slot.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Meta/MetaTypes.h"
#include "PathfinderMetaSubsystem.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMetaCurrencyChanged, FName, CurrencyId, int32, NewAmount, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMetaCurrencyEarned, FName, CurrencyId, int32, Amount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMetaUpgradeChanged, FName, UpgradeId, int32, NewRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMetaDeathDefianceChanged, int32, Remaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMetaSaveReset);

UCLASS()
class PATHFINDEROTHEREARTH_API UPathfinderMetaSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------- Currency ----------

	UFUNCTION(BlueprintPure, Category = "Meta|Currency")
	int32 GetCurrency(FName CurrencyId) const;

	/** How much of this currency was earned since the current run started (used to show/hide the HUD counter). */
	UFUNCTION(BlueprintPure, Category = "Meta|Currency")
	int32 GetEarnedThisRun(FName CurrencyId) const;

	/** Adds currency, fires On Currency Changed, and saves if the currency is permanent. */
	UFUNCTION(BlueprintCallable, Category = "Meta|Currency")
	void AddCurrency(FName CurrencyId, int32 Amount);

	/** Returns false (and changes nothing) if there isn't enough. */
	UFUNCTION(BlueprintCallable, Category = "Meta|Currency")
	bool TrySpendCurrency(FName CurrencyId, int32 Amount);

	/** Sets a currency directly (clamped at 0). Does NOT count as earned. Fires On Currency Changed if the value changed. */
	UFUNCTION(BlueprintCallable, Category = "Meta|Currency")
	void SetCurrency(FName CurrencyId, int32 NewAmount);

	UFUNCTION(BlueprintPure, Category = "Meta|Currency")
	bool GetCurrencyData(FName CurrencyId, FCurrencyRow& OutRow) const;

	// ---------- Upgrades ----------

	/** All upgrade row names, sorted by Sort Order (for building the Enchantress screen). */
	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	TArray<FName> GetAllUpgradeIds() const;

	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	bool GetUpgradeData(FName UpgradeId, FMetaUpgradeRow& OutRow) const;

	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	int32 GetUpgradeRank(FName UpgradeId) const;

	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	int32 GetMaxRank(FName UpgradeId) const;

	/** Cost of the next rank, or -1 if already maxed / unknown upgrade. */
	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	int32 GetNextRankCost(FName UpgradeId) const;

	/** The upgrade's total value at its current rank (0 if not bought). */
	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	float GetUpgradeValue(FName UpgradeId) const;

	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	bool CanPurchaseUpgrade(FName UpgradeId) const;

	/** Spends the cost and raises the rank by one. Returns false if maxed or not enough currency. */
	UFUNCTION(BlueprintCallable, Category = "Meta|Upgrades")
	bool TryPurchaseUpgrade(FName UpgradeId);

	/** Full free refund of every rank bought. Returns the total amount refunded. */
	UFUNCTION(BlueprintCallable, Category = "Meta|Upgrades")
	int32 RefundAllUpgrades();

	/** Sum of all bought upgrades with this effect (e.g. total Death Defiance charges, total free rerolls). */
	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	float GetEffectTotal(EMetaUpgradeEffect Effect) const;

	/** Sum of all bought Stat Bonus upgrades for one stat + mode. The Stats Manager reads this for the Meta bucket. */
	UFUNCTION(BlueprintPure, Category = "Meta|Upgrades")
	float GetStatBonus(ECharacterStatType StatType, EModifierMode Mode) const;

	// ---------- Run ----------

	/** Call when a new run starts: clears per-run currencies and "earned this run", refills Death Defiance. */
	UFUNCTION(BlueprintCallable, Category = "Meta|Run")
	void BeginRun();

	/** True while the player is in a run (Room Flow says so). False at camp. Use to hide run-only HUD. */
	UFUNCTION(BlueprintPure, Category = "Meta|Run")
	bool IsRunActive() const;

	UFUNCTION(BlueprintPure, Category = "Meta|Run")
	int32 GetDeathDefianceRemaining() const { return DeathDefianceRemaining; }

	/** Uses one charge if any are left. The Health component calls this on lethal damage. */
	UFUNCTION(BlueprintCallable, Category = "Meta|Run")
	bool TryConsumeDeathDefiance();

	UFUNCTION(BlueprintPure, Category = "Meta|Run")
	float GetDeathDefianceHealFraction() const;

	UFUNCTION(BlueprintPure, Category = "Meta|Run")
	float GetDeathDefianceInvulnerabilityTime() const;

	// ---------- Save ----------

	/** The save slot actually used: the Project Settings name, with "_PIE" added when playing in the editor. */
	UFUNCTION(BlueprintPure, Category = "Meta|Save")
	FString GetSaveSlotName() const;

	/** Wipes ALL permanent progress (currencies, upgrade ranks) and deletes the save file. Debug / "new game". */
	UFUNCTION(BlueprintCallable, Category = "Meta|Save")
	void ResetAllProgress();

	// ---------- Events ----------

	/** Fired after Reset All Progress. Other systems that keep permanent data (weapons, materials, class) reset on this. */
	UPROPERTY(BlueprintAssignable, Category = "Meta|Events")
	FOnMetaSaveReset OnSaveReset;

	UPROPERTY(BlueprintAssignable, Category = "Meta|Events")
	FOnMetaCurrencyChanged OnCurrencyChanged;

	/** Fired only when currency is EARNED (Add Currency), never for spending or refunds. Drives the "X Memoria obtained" popup. */
	UPROPERTY(BlueprintAssignable, Category = "Meta|Events")
	FOnMetaCurrencyEarned OnCurrencyEarned;

	UPROPERTY(BlueprintAssignable, Category = "Meta|Events")
	FOnMetaUpgradeChanged OnUpgradeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Meta|Events")
	FOnMetaDeathDefianceChanged OnDeathDefianceChanged;

private:
	void LoadMeta();
	void SaveMeta(bool bAsync);
	void RefillDeathDefiance();

	/** Sets every "resets each run" currency back to its Starting Amount. */
	void ResetRunCurrencies(bool bBroadcast);

	/** Fires On Currency Changed for every currency (Delta 0) so all currency widgets refresh. */
	void BroadcastAllCurrencies();
	const FCurrencyRow* FindCurrency(FName CurrencyId, bool bWarnIfMissing = true) const;
	const FMetaUpgradeRow* FindUpgrade(FName UpgradeId, bool bWarnIfMissing = true) const;

	UPROPERTY()
	TObjectPtr<UDataTable> CurrencyTable;

	UPROPERTY()
	TObjectPtr<UDataTable> UpgradeTable;

	UPROPERTY()
	TMap<FName, int32> Currencies;

	UPROPERTY()
	TMap<FName, int32> EarnedThisRun;

	UPROPERTY()
	TMap<FName, int32> UpgradeRanks;

	int32 DeathDefianceRemaining = 0;

	// ---------- Debug console commands (not in shipping builds) ----------
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();
	void Cmd_ResetSave(const TArray<FString>& Args);
	void Cmd_GiveMemoria(const TArray<FString>& Args);
	void Cmd_GiveCurrency(const TArray<FString>& Args);

	TArray<class IConsoleObject*> ConsoleCommands;
};
