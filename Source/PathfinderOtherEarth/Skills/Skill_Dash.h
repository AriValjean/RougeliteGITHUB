// Skill_Dash.h
// Configurable movement dash using a Root Motion Source (fixed distance over fixed time,
// works on the ground and in the air, unaffected by friction).
// Make one Blueprint child per class and only change the values:
//   Rogue  Shadowstep : Movement Input, 600 cm, 0.2 s
//   Ranger Step Back  : Away From Aim, ~450 cm, 0.18 s
//   Swordsman Charge  : Toward Aim, ~700 cm, 0.25 s
// (The Wizard's blink is a teleport, which will be a separate skill class.)

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Skill_Dash.generated.h"

UENUM(BlueprintType)
enum class EDashDirection : uint8
{
	MovementInput UMETA(DisplayName = "Movement Input (forward if idle)"),
	AwayFromAim   UMETA(DisplayName = "Away From Aim (step back)"),
	TowardAim     UMETA(DisplayName = "Toward Aim (charge)")
};

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API USkill_Dash : public USkillBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivate_Implementation() const override;
	virtual void ActivateSkill_Implementation() override;

	/** The direction this dash would go right now (flat, normalized). */
	UFUNCTION(BlueprintPure, Category = "Dash")
	FVector GetDashDirection() const;

protected:
	virtual void NativeOnSkillEnded() override;

	/** Blueprint hook for FX / sound when the dash starts. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dash")
	void OnDashStarted(FVector DashDirection);

	/** Base distance. Skill modifiers (Distance) from accessories change it at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", Units = "cm"))
	float DashDistance = 600.f;

	/** Base duration. Skill modifiers (Duration) from accessories change it at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.05", Units = "s"))
	float DashDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	EDashDirection Direction = EDashDirection::MovementInput;

	/** Adds State.Invincible for the whole dash. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bInvincibleDuringDash = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bAllowInAir = true;

	/** Turn the character to face the dash (a step back keeps facing the crosshair instead). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bRotateToDashDirection = true;

private:
	void FinishDash();

	FTimerHandle DashTimer;
	uint16 RootMotionID = 0;
};
