// SkillFamiliar.h
// Reusable summoned helper that floats beside the player and shoots homing bolts (Arcane Familiar,
// and later any other pet / turret). It only picks targets and fires; the skill that summoned it
// listens to On Bolt Hit Target and applies the damage, so damage rules stay in one place.
//
// Targeting: nearest living enemy within Target Range of the PLAYER that the familiar can see (walls
// block, other enemies don't). It sticks to that target until it dies, leaves range or goes out of sight.
//
// Make a Blueprint child (BP_ArcaneFamiliar): set the Mesh (glowing orb) and Bolt Class.
//
// Drop into: Source/PathfinderOtherEarth/Skills/SkillFamiliar.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillFamiliar.generated.h"

class UStaticMeshComponent;
class ASkillProjectile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFamiliarBoltHit, AActor*, HitActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFamiliarExpired);

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API ASkillFamiliar : public AActor
{
	GENERATED_BODY()

public:
	ASkillFamiliar();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Starts following the Owner (set Owner when spawning) and shooting.
	 * Duration: seconds before it leaves. Fire Interval: seconds between bolts.
	 * Target Range: cm around the player. Bolt Speed: cm/s.
	 */
	UFUNCTION(BlueprintCallable, Category = "Familiar")
	void StartFamiliar(float Duration, float FireInterval, float TargetRange, float BoltSpeed);

	/** Leaves now (duration over, death, class change). Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "Familiar")
	void Dismiss();

	UFUNCTION(BlueprintPure, Category = "Familiar")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** A bolt hit an enemy. The summoning skill applies damage. */
	UPROPERTY(BlueprintAssignable, Category = "Familiar")
	FOnFamiliarBoltHit OnBoltHitTarget;

	/** The familiar is leaving (once). */
	UPROPERTY(BlueprintAssignable, Category = "Familiar")
	FOnFamiliarExpired OnFamiliarExpired;

protected:
	/** Blueprint hooks for FX / sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Familiar")
	void OnSummoned();

	UFUNCTION(BlueprintImplementableEvent, Category = "Familiar")
	void OnBoltFired(AActor* Target);

	/** Play a fade-out here. The actor is destroyed Linger Time later. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Familiar")
	void OnDismissed();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Familiar")
	TObjectPtr<USceneComponent> Root;

	/** The orb. No collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Familiar")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The bolt it shoots. Leave empty to use the plain C++ projectile (invisible, still works). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Familiar")
	TSubclassOf<ASkillProjectile> BoltClass;

	/** Where it floats, relative to the player's facing: X forward (negative = behind), Y right, Z up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Familiar")
	FVector FollowOffset = FVector(-60.f, 60.f, 110.f);

	/** How quickly it catches up to its spot. Lower = floatier, more lag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Familiar", meta = (ClampMin = "0.5"))
	float FollowSpeed = 6.f;

	/** Gentle up / down bob, in cm. 0 = none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Familiar", meta = (ClampMin = "0.0", Units = "cm"))
	float BobHeight = 8.f;

	/** How hard bolts curve toward the target (cm/s²). Higher = tighter turns, never misses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Familiar", meta = (ClampMin = "0.0"))
	float HomingAcceleration = 12000.f;

	/** Seconds the actor stays after Dismiss so a fade-out can play. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Familiar", meta = (ClampMin = "0.0", Units = "s"))
	float LingerTime = 0.5f;

private:
	void Fire();
	AActor* FindTarget() const;
	bool IsValidTarget(const AActor* Actor) const;
	bool HasLineOfSight(const AActor* Actor) const;
	FVector GetFollowLocation() const;

	UFUNCTION()
	void HandleBoltHit(AActor* HitActor);

	TWeakObjectPtr<AActor> CurrentTarget;
	float Range = 1500.f;
	float Speed = 2500.f;
	float BobTime = 0.f;
	bool bDismissed = false;

	FTimerHandle FireTimer;
	FTimerHandle DurationTimer;
};
