// StatusEffectComponent.h
// Timed status effects on a character (enemies for now). Starts with SLOW; built so Burn / Poison /
// Bleed (damage over time) can be added here later for the Wizard and bleed accessories.
//
// Movement speed: this component owns the character's speed while effects are active.
//   Max Walk Speed = Base Move Speed x Move Speed Multiplier (1 - strongest slow).
//   Anything that changes the character's normal speed (e.g. Start Sprint / Stop Sprint) must call
//   Set Base Move Speed instead of setting Max Walk Speed directly, or it would wipe the slow.
//
// Slows: the same Source refreshes its timer (and keeps the stronger value); different sources don't
// stack — the strongest active slow wins.
//
// Add it as a component to BP_EnemyBase (children like BP_Enemy_Slime inherit it).
//
// Drop into: Source/PathfinderOtherEarth/Combat/StatusEffectComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatusEffectComponent.generated.h"

class UCharacterMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatusEffectsChanged);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UStatusEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatusEffectComponent();

	/** Returns the actor's status effect component, or None. */
	UFUNCTION(BlueprintPure, Category = "Status Effects")
	static UStatusEffectComponent* FindStatusEffects(const AActor* Actor);

	// ------------------------------------------------------------------
	// Slow
	// ------------------------------------------------------------------

	/** Slows movement by Strength (0.2 = 20% slower) for Duration seconds. See header for stacking rules. */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Slow")
	void ApplySlow(FName Source, float Strength, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Status Effects|Slow")
	void RemoveSlow(FName Source);

	UFUNCTION(BlueprintPure, Category = "Status Effects|Slow")
	bool IsSlowed() const;

	/** 1 - strongest active slow (1 = normal speed). */
	UFUNCTION(BlueprintPure, Category = "Status Effects|Slow")
	float GetMoveSpeedMultiplier() const;

	// ------------------------------------------------------------------
	// Movement speed
	// ------------------------------------------------------------------

	/** Use this instead of setting Max Walk Speed directly (Start / Stop Sprint), so slows stay applied. */
	UFUNCTION(BlueprintCallable, Category = "Status Effects|Movement")
	void SetBaseMoveSpeed(float NewBaseSpeed);

	UFUNCTION(BlueprintPure, Category = "Status Effects|Movement")
	float GetBaseMoveSpeed() const { return BaseMoveSpeed; }

	/** A status effect was added, refreshed or ended. Overhead icons / tint can bind this. */
	UPROPERTY(BlueprintAssignable, Category = "Status Effects|Events")
	FOnStatusEffectsChanged OnStatusEffectsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FActiveSlow
	{
		float Strength = 0.f;
		double ExpireTime = 0.0;
	};

	UCharacterMovementComponent* GetMovement() const;
	double Now() const;
	void ApplyMoveSpeed();
	void PurgeExpired();
	void ScheduleNextExpiry();

	TMap<FName, FActiveSlow> Slows;

	/** Normal Max Walk Speed without slows (read at BeginPlay, then set via Set Base Move Speed). */
	float BaseMoveSpeed = 0.f;

	FTimerHandle ExpireTimer;
};
