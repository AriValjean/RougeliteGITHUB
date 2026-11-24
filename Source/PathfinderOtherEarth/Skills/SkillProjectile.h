// SkillProjectile.h
// Reusable flying projectile for skills (thrown daggers, bolts, magic missiles...).
// No gravity by default, flies straight, hits the first ENEMY it overlaps (anything with a
// Health Component that the player doesn't control), stops on walls, and despawns after its range.
// Pierce Targets: keeps flying through enemies and hits each one once (Arcane Bolt).
// The skill that spawned it listens to On Projectile Hit Target and decides what the hit does.
//
// Make a Blueprint child per look (e.g. BP_SkillProjectile_Dagger) and set the Mesh there.
//
// Drop into: Source/PathfinderOtherEarth/Skills/SkillProjectile.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillProjectileHit, AActor*, HitActor);

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API ASkillProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASkillProjectile();

	/** Starts flying in Direction at Speed. Despawns after travelling Max Distance. */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void Launch(const FVector& Direction, float Speed, float MaxDistance);

	/** Curves toward Target while flying (familiar bolts). None = fly straight. Call before or after Launch. */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SetHomingTarget(AActor* Target, float HomingAcceleration);

	/** Size of the hit sphere (default 12 cm). Call before Launch. */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SetHitRadius(float Radius);

	/** Fired when the projectile hits an enemy. Normal: once, right before it is destroyed. Pierce: once per enemy. */
	UPROPERTY(BlueprintAssignable, Category = "Projectile")
	FOnSkillProjectileHit OnProjectileHitTarget;

	/** Fly through enemies (each hit once) instead of stopping at the first. Walls still stop it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bPierceTargets = false;

protected:
	virtual void BeginPlay() override;

	/** Blueprint hook for impact FX / sound. Hit Actor is None when it hit a wall. Pierce: runs for every enemy hit too. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
	void OnImpact(FVector ImpactLocation, AActor* HitActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	/** Set the dagger (or other) mesh on this in the Blueprint child. No collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	bool IsValidTarget(const AActor* Actor) const;

	bool bHasHit = false;

	/** Pierce: enemies already hit by this projectile. */
	TArray<TWeakObjectPtr<AActor>> PiercedActors;
};
