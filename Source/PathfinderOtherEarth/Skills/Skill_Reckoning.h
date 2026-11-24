// Skill_Reckoning.h
// Rogue Ultimate: detonate the Marked Dagger's mark.
//   Detonation = Damage (base, full Attack Damage scaling, can crit)
//              + Magnitude x damage stored on the marked target (0.5 = 50%)
//   Delivered as one normal hit, so the target's Defense applies once. The mark is consumed first,
//   so the detonation itself does not get the mark bonus.
//   Splash: enemies within Radius take SecondaryMagnitude x the detonation (0.25 = 25%).
// Can only be used while a living marked target is within Range and in line of sight,
// so the cooldown is never wasted.
//
// All numbers come from the DT_SkillsCPP row: Cooldown, Damage, Attack Damage Scaling, Can Crit,
// Magnitude, Range, Radius, Secondary Magnitude.
//
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_Reckoning.h

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Skill_Reckoning.generated.h"

class USkill_MarkedDagger;

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API USkill_Reckoning : public USkillBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivate_Implementation() const override;
	virtual void ActivateSkill_Implementation() override;

protected:
	/** Blueprint hook for the explosion FX / sound / camera shake. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Reckoning")
	void OnDetonated(AActor* Target, FVector DetonationLocation, float DetonationDamage, bool bWasCritical);

	/** Blueprint hook: one enemy caught in the splash (fires once per enemy). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Reckoning")
	void OnSplashHit(AActor* HitActor, float SplashDamage);

private:
	/** Finds the equipped Marked Dagger skill (any slot). */
	USkill_MarkedDagger* FindMarkSkill() const;

	/** Living, in range and visible from the player. */
	bool IsValidDetonationTarget(const AActor* Target) const;
};
