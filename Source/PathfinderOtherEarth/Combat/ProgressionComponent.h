// ProgressionComponent.h
// Player XP and level. Replaces the XP half of BPC_CombatSystem (Current EXP, EXP To Next Level,
// Player Level, AddEXP, Level Up math, On EXPChanged). Player only; enemies don't get this component.
//
// XP needed for each level = BaseXPToNextLevel x XPGrowthPerLevel ^ (Level - 1), rounded.
// With the defaults (100, 1.25): 100, 125, 156, 195, 244 ... Set Growth to 1 for a flat cost.
//
// Drop into: Source/PathfinderOtherEarth/Combat/ProgressionComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProgressionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnXPChangedSignature, float, CurrentXP, float, XPToNextLevel, int32, Level);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUpSignature, int32, NewLevel);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProgressionComponent();

	/**
	 * Adds XP and levels up as many times as the XP covers (a big kill can give several levels).
	 * Fires On Level Up once per level gained, then On XP Changed once.
	 */
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void AddXP(float Amount);

	/** Sets level and XP directly (run-state load). Fires On XP Changed. */
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void RestoreProgress(int32 NewLevel, float NewCurrentXP);

	/** XP needed to go from Level to Level + 1. */
	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetXPRequiredForLevel(int32 ForLevel) const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetLevel() const { return Level; }

	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetCurrentXP() const { return CurrentXP; }

	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetXPToNextLevel() const { return XPToNextLevel; }

	/** Current / needed (0..1), for the XP bar. */
	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetXPPercent() const;

	/** Same values the old On EXPChanged sent. The HUD XP bar binds this. */
	UPROPERTY(BlueprintAssignable, Category = "Progression|Events")
	FOnXPChangedSignature OnXPChanged;

	/** Fires once per level gained. Hook level-up rewards, sounds or FX here. */
	UPROPERTY(BlueprintAssignable, Category = "Progression|Events")
	FOnLevelUpSignature OnLevelUp;

protected:
	virtual void BeginPlay() override;

	/** XP needed to go from level 1 to level 2. */
	UPROPERTY(EditAnywhere, Category = "Progression|Tuning", meta = (ClampMin = "1"))
	float BaseXPToNextLevel = 100.f;

	/** Each level costs this many times the previous one. 1.0 = every level costs the same. */
	UPROPERTY(EditAnywhere, Category = "Progression|Tuning", meta = (ClampMin = "1.0"))
	float XPGrowthPerLevel = 1.25f;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Progression")
	int32 Level = 1;

	UPROPERTY(VisibleInstanceOnly, Category = "Progression")
	float CurrentXP = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Progression")
	float XPToNextLevel = 100.f;
};
