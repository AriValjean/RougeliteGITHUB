// Skill_ArcaneFamiliar.h
// Wizard Ultimate: summon a floating orb that follows the player and fires homing bolts at the
// nearest visible enemy. Instant on press (no aiming). The cooldown starts when it LEAVES
// (Cooldown Starts On End is on), so there's never more than one.
// It leaves when its duration is over, or early on death / class change / leaving the level.
//
// DT_SkillsCPP row (Ultimate_Wizard):
//   Cooldown              : 60 (starts when the familiar leaves)
//   Damage                : per bolt (8)         - Damage skill modifiers apply
//   Attack Damage Scaling : 1
//   Can Crit              : still needs the Skill.CanCrit accessory tag
//   Effect Duration       : how long it stays (12) - Duration skill modifiers apply
//   Secondary Magnitude   : seconds between bolts (0.5)
//   Range                 : targeting range around the player in cm (1500) - Distance skill modifiers apply
//   Projectile Speed      : bolt speed (2500)
//   Hold To Aim           : unticked
// Blueprint child (BP_Skill_ArcaneFamiliar_Wizard): Familiar Class = BP_ArcaneFamiliar.
//
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_ArcaneFamiliar.h

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Skill_ArcaneFamiliar.generated.h"

class ASkillFamiliar;

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API USkill_ArcaneFamiliar : public USkillBase
{
	GENERATED_BODY()

public:
	USkill_ArcaneFamiliar();

	virtual void ActivateSkill_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Arcane Familiar")
	ASkillFamiliar* GetActiveFamiliar() const { return ActiveFamiliar.Get(); }

protected:
	virtual void NativeOnSkillEnded() override;

	/** Blueprint hooks for FX / sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane Familiar")
	void OnFamiliarSummoned(ASkillFamiliar* Familiar);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane Familiar")
	void OnFamiliarHit(AActor* HitActor, float FinalDamage);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane Familiar")
	void OnFamiliarExpired();

	/** The orb actor (BP_ArcaneFamiliar). Leave empty to use the plain C++ one (invisible, still works). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane Familiar")
	TSubclassOf<ASkillFamiliar> FamiliarClass;

private:
	UFUNCTION()
	void HandleBoltHit(AActor* HitActor);

	UFUNCTION()
	void HandleFamiliarExpired();

	UPROPERTY(Transient)
	TObjectPtr<ASkillFamiliar> ActiveFamiliar;
};
