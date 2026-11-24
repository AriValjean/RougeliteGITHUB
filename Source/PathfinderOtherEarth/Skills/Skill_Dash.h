// Skill_Dash.h
// Configurable movement dash using a Root Motion Source (fixed distance over fixed time,
// unaffected by friction). Make one Blueprint child per class and only change the values:
//   Rogue  Shadowstep : Movement Input, 600 cm, 0.2 s
//   Ranger Step Back  : Away From Aim, ~450 cm, 0.18 s
//   Swordsman Charge  : Toward Aim, ~700 cm, 0.25 s, Deals Damage + Pass Through Enemies
// (The Wizard's blink is a teleport, which will be a separate skill class.)
//
// Damage dashes sweep a sphere along the path every frame and hit each enemy once per dash.
// Damage = Dash Damage (+ Damage skill modifiers) x Attack Damage stat, with a crit roll.

#pragma once

#include "CoreMinimal.h"
#include "Skills/SkillBase.h"
#include "Engine/EngineTypes.h"
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

	/** Blueprint hook for hit FX / sound / hit-stop when a damage dash hits an enemy. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dash|Damage")
	void OnDashHit(AActor* HitActor, float FinalDamage, bool bWasCritical);

	// ------------------------------------------------------------------
	// Movement
	// ------------------------------------------------------------------

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
	bool bAllowInAir = false;

	/**
	 * Away From Aim / Toward Aim only: while the player holds left or right (A / D), angle the dash
	 * this many degrees toward that side (e.g. step back-left at 30 deg). 0 = always straight.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float SideInputAngle = 30.f;

	/** Turn the character to face the dash (a step back keeps facing the crosshair instead). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bRotateToDashDirection = true;

	/** Dash through enemies instead of being blocked by them (needed for a pass-through charge). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bPassThroughEnemies = false;

	// ------------------------------------------------------------------
	// Damage (charge-style dashes)
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Damage")
	bool bDealsDamage = false;

	/** Base damage per enemy hit, before the Attack Damage stat and crits. Damage skill modifiers apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Damage", meta = (EditCondition = "bDealsDamage", ClampMin = "0.0"))
	float DashDamage = 20.f;

	/** Width of the hit sweep around the character. Radius skill modifiers apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Damage", meta = (EditCondition = "bDealsDamage", ClampMin = "1.0", Units = "cm"))
	float HitRadius = 100.f;

	/** Launch speed pushed onto hit enemies along the dash direction. 0 = no knockback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash|Damage", meta = (EditCondition = "bDealsDamage", ClampMin = "0.0"))
	float Knockback = 600.f;

private:
	void FinishDash();
	void SweepForHits();
	void RestoreCollision();

	FTimerHandle DashTimer;
	FTimerHandle HitTimer;
	uint16 RootMotionID = 0;

	FVector CurrentDashDirection = FVector::ForwardVector;
	FVector LastSweepLocation = FVector::ZeroVector;

	/** Enemies already hit this dash (each enemy is hit once per dash). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> HitActorsThisDash;

	bool bChangedCollision = false;
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
};
