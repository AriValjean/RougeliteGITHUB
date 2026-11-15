#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CurrencyManagerComponent.generated.h"

class UPathfinderMetaSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldChangedSignature, int32, NewGold);

/**
 * The player's gold "wallet". BPC_CurrencyManager is a data-only Blueprint child of this.
 *
 * Gold itself now lives in UPathfinderMetaSubsystem (row "Gold" in DT_CurrenciesCPP, Resets On Run End ticked),
 * so it survives level travel on its own and Begin Run resets it. This component is a thin wrapper that keeps
 * the same functions and On Gold Changed event, so chests, the merchant, enemy drops and W_Currency need no changes.
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

	/** Adds gold (enemy drops, pickups). Zero or negative amounts are ignored. Capped at Max Gold. */
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

	/** Sets gold directly (debug, special effects). Run resets are handled by the Meta subsystem's Begin Run. */
	UFUNCTION(BlueprintCallable, Category="Currency")
	void SetGold(int32 NewAmount);

	UFUNCTION(BlueprintPure, Category="Currency")
	int32 GetGoldAmount() const;

	UFUNCTION(BlueprintPure, Category="Currency")
	bool HasEnoughGold(int32 Cost) const;

	/** Finds the currency component on any actor (e.g. Get Player Character). No cast to BP_PlayerCharacter needed. */
	UFUNCTION(BlueprintPure, Category="Currency")
	static UCurrencyManagerComponent* FindCurrencyManager(const AActor* Actor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Row name in DT_CurrenciesCPP. */
	UPROPERTY(EditDefaultsOnly, Category="Currency")
	FName GoldCurrencyId = TEXT("Gold");

	/** Hard cap so the total can't overflow. */
	UPROPERTY(EditDefaultsOnly, Category="Currency", meta=(ClampMin="0"))
	int32 MaxGold = 999999;

private:
	UFUNCTION()
	void HandleCurrencyChanged(FName CurrencyId, int32 NewAmount, int32 Delta);

	UPathfinderMetaSubsystem* GetMeta() const;
};
