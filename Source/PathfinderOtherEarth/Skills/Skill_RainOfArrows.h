// Skill_RainOfArrows.h
// Ranger Ultimate: arrows rain on a ground area. Every tick, each enemy inside takes damage and is
// slowed while it stays inside (slow wears off ~0.5 s after leaving). Hold To Aim: hold R to show
// where it lands (clamped to Range), release to cast.
//
// All numbers come from the skill's DT_SkillsCPP row:
//   Cooldown, Damage (per tick), Attack Damage Scaling, Can Crit, Radius, Effect Duration (zone seconds),
//   Secondary Magnitude (seconds between ticks), Magnitude (slow, 0.15 = 15%), Range, Hold To Aim.
// The Blueprint child (BP_Skill_RainOfArrows_Ranger) only picks the zone look and the target indicator.
//
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_RainOfArrows.h

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Skill_RainOfArrows.generated.h"

class ASkillZone;

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API USkill_RainOfArrows : public USkillBase
{
	GENERATED_BODY()

public:
	virtual void ActivateSkill_Implementation() override;

	/** Slow source name on enemies' Status Effect component. */
	static const FName SlowSourceName;

protected:
	virtual void NativeOnRemoved() override;

	/** Blueprint hook for cast FX / sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rain of Arrows")
	void OnRainStarted(FVector RainLocation);

	/** The zone actor (ground ring + falling arrows FX). Leave empty to use the plain C++ zone (no visuals). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rain of Arrows")
	TSubclassOf<ASkillZone> ZoneClass;

private:
	UFUNCTION()
	void HandleZoneTarget(AActor* Target);

	TWeakObjectPtr<ASkillZone> ActiveZone;
	float CurrentTickInterval = 0.5f;
};
