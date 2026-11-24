// Skill_Blink.h
// Wizard Dash: instant teleport.
//   TAP shift  : blink a set distance in the direction the character is facing.
//   HOLD shift : a circle appears under the crosshair (up to max range). The Wizard can't move but
//                turns with the mouse. Release = blink to the circle. Right click cancels (no cooldown spent).
// Never goes through walls (stops just short), always lands on walkable floor, never off a ledge into
// nothing. If there's no room to blink at all (e.g. facing a wall), it fizzles and the cooldown is refunded.
// Short invincibility on arrival. Can't be used in the air (same rule as every dash).
//
// DT_SkillsCPP row (Dash_Wizard):
//   Cooldown        : 4
//   Magnitude       : tap distance in cm (800)        - Distance skill modifiers apply
//   Range           : max hold-to-aim range in cm (1500) - Distance skill modifiers apply
//   Radius          : size of the circle in cm (100)
//   Effect Duration : invincibility on arrival in s (0.15) - Duration skill modifiers apply
//   Hold To Aim     : ticked
// Blueprint child (BP_Skill_Blink_Wizard): set Target Indicator Class to BP_TargetIndicator.
//
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_Blink.h

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Skill_Blink.generated.h"

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API USkill_Blink : public USkillBase
{
	GENERATED_BODY()

public:
	USkill_Blink();

	virtual bool CanActivate_Implementation() const override;
	virtual void ActivateSkill_Implementation() override;

	/**
	 * Safe landing spot (on the floor) as close to Desired Ground as possible: stops before walls and
	 * steps back from ledges. Returns false if there is no room to blink.
	 */
	UFUNCTION(BlueprintCallable, Category = "Blink")
	bool FindBlinkDestination(FVector DesiredGround, FVector& OutGround) const;

protected:
	virtual FVector ComputeAimLocation() const override;
	virtual void NativeOnRemoved() override;

	/** Blueprint hook: right before the teleport (vanish FX at the old spot). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Blink")
	void OnBlinkDeparted(FVector FromLocation);

	/** Blueprint hook: right after the teleport (appear FX). Was Aimed = held and released, false = tap. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Blink")
	void OnBlinkArrived(FVector ToLocation, bool bWasAimed);

	/** Blueprint hook: no room to blink, cooldown refunded (fizzle sound). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Blink")
	void OnBlinkFailed();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink")
	bool bAllowInAir = false;

	/** How far down a landing spot can be (blinking off a small ledge is fine, off a cliff is not). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxDropHeight = 600.f;

	/** Debug: print to the Output Log why a blink found no room (what blocked it). Turn off once it works. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink|Debug")
	bool bLogFailures = true;

private:
	void Fizzle();
	void EndInvincibility();

	FTimerHandle InvincibleTimer;
	bool bGrantedInvincible = false;
};
