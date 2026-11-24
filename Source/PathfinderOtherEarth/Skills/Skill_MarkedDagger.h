// Skill_MarkedDagger.h
// Rogue Basic: throw a dagger at the crosshair. The first enemy hit takes a small hit and is MARKED:
// it takes extra damage from everything for a while (or until it dies). Only one mark at a time:
// marking a new enemy removes the old mark.
//
// While marked, every hit the target takes is STORED (damage before Defense, including the mark bonus).
// Reckoning (Rogue Ultimate) consumes the mark and detonates a share of that stored damage.
//
// All numbers come from the skill's DT_SkillsCPP row:
//   Cooldown, Damage (small hit), Attack Damage Scaling, Can Crit (off for this skill),
//   Magnitude (mark bonus, 0.2 = +20% damage taken), Effect Duration (mark seconds),
//   Range, Projectile Speed.
// The Blueprint child (BP_Skill_MarkedDagger_Rogue) only picks the projectile look and the mark icon.
//
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_MarkedDagger.h

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Skill_MarkedDagger.generated.h"

class ASkillProjectile;

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API USkill_MarkedDagger : public USkillBase
{
	GENERATED_BODY()

public:
	virtual void ActivateSkill_Implementation() override;

	/** The currently marked enemy, or None. */
	UFUNCTION(BlueprintPure, Category = "Marked Dagger")
	AActor* GetMarkedTarget() const { return MarkedTarget.Get(); }

	/** Damage the marked target has taken since it was marked (before Defense, including the mark bonus). */
	UFUNCTION(BlueprintPure, Category = "Marked Dagger")
	float GetStoredMarkDamage() const { return StoredMarkDamage; }

	/**
	 * Removes the mark and hands back who was marked and how much damage was stored.
	 * Returns false if nothing is marked. Used by Reckoning.
	 */
	UFUNCTION(BlueprintCallable, Category = "Marked Dagger")
	bool ConsumeMark(AActor*& OutTarget, float& OutStoredDamage);

	/** Name used on the enemy's Health Component for this mark's damage-taken bonus. */
	static const FName MarkSourceName;

protected:
	virtual void NativeOnRemoved() override;

	/** Blueprint hooks for FX / sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Marked Dagger")
	void OnTargetMarked(AActor* Target);

	UFUNCTION(BlueprintImplementableEvent, Category = "Marked Dagger")
	void OnMarkEnded(AActor* Target);

	/** The flying dagger. Leave empty to use the plain C++ projectile (invisible, still works). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marked Dagger")
	TSubclassOf<ASkillProjectile> ProjectileClass;

	/** Actor shown above the marked enemy (placeholder icon for now). Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marked Dagger")
	TSubclassOf<AActor> MarkIndicatorClass;

	/** Height above the top of the enemy's capsule for the mark icon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marked Dagger", meta = (Units = "cm"))
	float IndicatorHeightOffset = 40.f;

	/** Mesh socket the dagger is thrown from. Falls back to above the character if the socket is missing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Marked Dagger")
	FName ThrowSocket = TEXT("hand_r");

private:
	UFUNCTION()
	void HandleProjectileHit(AActor* HitActor);

	UFUNCTION()
	void HandleMarkedTargetDeath(AActor* Killer);

	UFUNCTION()
	void HandleMarkedTargetDamaged(const FDamageInfo& DamageInfo, float FinalDamage);

	void ApplyMark(AActor* Target);
	void ClearMark();
	FVector GetThrowLocation() const;

	TWeakObjectPtr<AActor> MarkedTarget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> MarkIndicator;

	FTimerHandle MarkTimer;

	float StoredMarkDamage = 0.f;
};
