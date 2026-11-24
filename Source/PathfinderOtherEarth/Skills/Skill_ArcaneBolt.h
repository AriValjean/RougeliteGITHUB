// Skill_ArcaneBolt.h
// Wizard Basic: an instant beam (like Xerath Q / Lux R). On cast, every enemy inside the rectangle
// takes damage at once. Nothing flies, so nothing can stop it early. The hold-to-aim rectangle IS the
// damage area: same start, direction, length and width.
//   TAP Q  : fires right away toward the crosshair.
//   HOLD Q : the rectangle shows where it will hit. You can still move; the Wizard turns to face the
//            camera. Release fires. Right click cancels (no cooldown spent).
// Goes through walls by default (like Lux R). Tick Blocked By Walls to stop the beam (and the
// rectangle) at the first wall instead.
//
// DT_SkillsCPP row (Basic_Wizard):
//   Cooldown, Damage (per enemy), Attack Damage Scaling, Can Crit,
//   Range  : beam length in cm           - Distance skill modifiers apply
//   Radius : HALF the beam width in cm (40 = 80 cm wide) - Radius skill modifiers apply
//   Hold To Aim: ticked
// Blueprint child (BP_Skill_ArcaneBolt_Wizard): Target Indicator Class (BP_LineIndicator) and
// optionally Beam Visual Class (a placeholder beam until there's a Niagara effect).
//
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_ArcaneBolt.h

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Skill_ArcaneBolt.generated.h"

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API USkill_ArcaneBolt : public USkillBase
{
	GENERATED_BODY()

public:
	USkill_ArcaneBolt();

	virtual void ActivateSkill_Implementation() override;

protected:
	virtual FVector ComputeAimLocation() const override;
	virtual float ComputeLineLength(const FVector& FlatDirection) const override;

	/** Blueprint hook for the beam FX / sound (e.g. a Niagara beam from Beam Start to Beam End). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane Bolt")
	void OnBeamFired(FVector BeamStart, FVector BeamEnd, FVector Direction);

	/** Blueprint hook per enemy hit. Hit Index: 1 = closest enemy, 2 = next... */
	UFUNCTION(BlueprintImplementableEvent, Category = "Arcane Bolt")
	void OnBeamHit(AActor* HitActor, float FinalDamage, int32 HitIndex);

	/** Off (default) = passes through walls. On = the beam and the rectangle stop at the first wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane Bolt")
	bool bBlockedByWalls = false;

	/** How far above / below the player's center the beam still hits (enemies on slopes and stairs). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane Bolt", meta = (ClampMin = "50.0", Units = "cm"))
	float BeamHalfHeight = 250.f;

	/**
	 * Optional placeholder beam: any Skill Target Indicator child (e.g. a copy of BP_LineIndicator with a
	 * bright material). It is spawned at the fire socket, sized to the beam, and removed after Beam Visual Time.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane Bolt")
	TSubclassOf<ASkillTargetIndicator> BeamVisualClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane Bolt", meta = (ClampMin = "0.05", Units = "s"))
	float BeamVisualTime = 0.25f;

	/** Mesh socket the beam comes from (the staff tip later). Falls back to chest height if missing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcane Bolt")
	FName FireSocket = TEXT("hand_r");

private:
	FVector GetFireLocation() const;
	FVector GetFlatAimDirection(const FVector& AimPoint) const;
};
