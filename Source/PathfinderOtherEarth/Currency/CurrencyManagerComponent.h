#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CurrencyManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldChangedSignature, int32, NewGold);

/**
 * Owns the player's gold. BPC_CurrencyManager becomes a data-only Blueprint child of this
 * (same pattern as BPC_WeaponManager and BPC_CombatSystem).
 *
 * Gold can only change through the functions below, so OnGoldChanged always fires
 * and the value can never go negative or past MaxGold.
 */
UCLASS(ClassGroup=(Currency), Blueprintable, meta=(BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UCurrencyManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCurrencyManagerComponent();

	/** Fires only when the gold total actually changes. Bind HUD widgets to this. */
	UPROPERTY(BlueprintAssignable, Category="Currency")
	FOnGoldChangedSignature OnGoldChanged;

	/** Adds gold (enemy drops, pickups). Zero or negative amounts are ignored. */
	UFUNCTION(BlueprintCallable, Category="Currency")
	void AddGold(int32 Amount);

	/** Removes gold, clamped at 0. For anything the player buys, use TrySpendGold instead. */
	UFUNCTION(BlueprintCallable, Category="Currency")
	void SubtractGold(int32 Amount);

	/**
	 * Purchase helper: checks the cost and subtracts it in one step.
	 * Returns false and spends nothing if the player can't afford it.
	 */
	UFUNCTION(BlueprintCallable, Category="Currency")
	bool TrySpendGold(int32 Cost);

	/** Sets gold directly. Use for restoring run state after level travel, or for run resets. */
	UFUNCTION(BlueprintCallable, Category="Currency")
	void SetGold(int32 NewAmount);

	UFUNCTION(BlueprintPure, Category="Currency")
	int32 GetGoldAmount() const { return CurrentGold; }

	UFUNCTION(BlueprintPure, Category="Currency")
	bool HasEnoughGold(int32 Cost) const { return Cost >= 0 && Cost <= CurrentGold; }

	/** Finds the currency component on any actor (e.g. Get Player Character). No cast to BP_PlayerCharacter needed. */
	UFUNCTION(BlueprintPure, Category="Currency")
	static UCurrencyManagerComponent* FindCurrencyManager(const AActor* Actor);

protected:
	/** Gold at the start of a run. Set in BPC_CurrencyManager's Class Defaults. */
	UPROPERTY(EditDefaultsOnly, Category="Currency", meta=(ClampMin="0"))
	int32 CurrentGold = 0;

	/** Hard cap so the total can't overflow. */
	UPROPERTY(EditDefaultsOnly, Category="Currency", meta=(ClampMin="0"))
	int32 MaxGold = 999999;

private:
	/** Single place gold is written. Clamps, then broadcasts only if the value changed. */
	void ApplyGold(int64 NewAmount);
};
