// SkillBase.h
// Parent of every class skill (dash, basic, ultimate).
// USkillComponent creates one instance per equipped skill and calls CanActivate / ActivateSkill.
// Children: C++ (e.g. USkill_Dash, USkill_MarkedDagger) or Blueprint. A skill must call End Skill
// when its action is finished (lasting effects like a mark can outlive the skill being "active").
//
// Hold To Aim (ticked in the skill's DT_SkillsCPP row):
//   - Tap Threshold 0   : the indicator shows as soon as the key is pressed (Rain of Arrows).
//   - Tap Threshold > 0 : releasing before it is a TAP = quick cast with no indicator (Blink, Arcane Bolt).
//                         Holding past it shows the indicator; releasing casts there.
//   - Indicator Shape   : Circle = ground spot under the crosshair, Line = rectangle from the player.
//   - Lock Movement While Aiming / Face Aim While Aiming: per skill, in the Blueprint child's Class Defaults.
//
// Drop into: Source/PathfinderOtherEarth/Skills/SkillBase.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Items/PathfinderTypes.h"
#include "Combat/PathfinderCombatTypes.h"
#include "SkillBase.generated.h"

class ACharacter;
class AController;
class USkillComponent;
class ASkillTargetIndicator;

/** Where the Hold To Aim indicator is drawn. */
UENUM(BlueprintType)
enum class EAimIndicatorShape : uint8
{
	Circle UMETA(DisplayName = "Circle (ground spot under crosshair)"),
	Line   UMETA(DisplayName = "Line (rectangle from the player)")
};

UCLASS(Abstract, Blueprintable, BlueprintType)
class PATHFINDEROTHEREARTH_API USkillBase : public UObject
{
	GENERATED_BODY()

public:
	/** Called by USkillComponent right after it creates this skill. */
	void InitSkill(USkillComponent* InComponent, FName InSkillRow, ESkillSlot InSlot, const FSkillDefinition& InDefinition);

	/** Gives Blueprint children a world, so Delay, timers and Spawn Actor work inside skills. */
	virtual UWorld* GetWorld() const override;

	/** Extra checks before the skill can start (cooldown is already checked by the component). */
	UFUNCTION(BlueprintNativeEvent, Category = "Skill")
	bool CanActivate() const;

	/** Runs the skill. The cooldown has already started. Call End Skill when the skill is finished. */
	UFUNCTION(BlueprintNativeEvent, Category = "Skill")
	void ActivateSkill();

	/** Marks the skill as finished. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "Skill")
	void EndSkill();

	/** Plays the Montage from this skill's DT_SkillsCPP row, if one is set. Returns its length (0 if none). */
	UFUNCTION(BlueprintCallable, Category = "Skill")
	float PlaySkillMontage(float PlayRate = 1.f);

	/** Row value with this slot's skill modifiers applied: (Base + Flat) x (1 + Percent). */
	UFUNCTION(BlueprintPure, Category = "Skill")
	float GetModifiedValue(ESkillStat Stat, float BaseValue) const;

	// ------------------------------------------------------------------
	// Damage
	// ------------------------------------------------------------------

	/**
	 * Skill damage with the owner's stats:
	 *   BaseDamage x (1 + (Attack Damage - 1) x AttackDamageScaling)
	 *   then a crit roll (Critical Rate chance, Critical Damage multiplier) ONLY if Can Crit is true
	 *   AND the player has the Skill.CanCrit tag (abilities never crit by default).
	 * Returns raw damage before the target's Defense / marks.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Damage")
	float ComputeSkillDamage(float BaseDamage, float AttackDamageScaling, bool bCanCrit, bool& bIsCritical);

	/**
	 * Computes skill damage (see Compute Skill Damage) and sends it to the target's Health Component.
	 * Returns false if the target can't be damaged (no health component, dead, invincible).
	 * Final Damage is after the target's marks and Defense.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Damage")
	bool ApplySkillDamage(AActor* Target, float BaseDamage, float AttackDamageScaling, bool bCanCrit,
		EDamageResponse Response, float& FinalDamage, bool& bIsCritical);

	// ------------------------------------------------------------------
	// Aiming
	// ------------------------------------------------------------------

	/** World point under the crosshair: camera trace up to Range (end of the trace if nothing is hit). */
	UFUNCTION(BlueprintPure, Category = "Skill|Aim")
	FVector GetCrosshairAimPoint(float Range) const;

	/**
	 * Ground point under the crosshair, clamped to Range from the player (pointing past it or at the sky
	 * gives the point at max range in that direction). Used by ground-targeted skills and the aim indicator.
	 */
	UFUNCTION(BlueprintPure, Category = "Skill|Aim")
	FVector GetGroundTargetLocation(float Range) const;

	/** Hold To Aim: true while the key is held and the indicator is showing. */
	UFUNCTION(BlueprintPure, Category = "Skill|Aim")
	bool IsTargeting() const { return bIsTargeting; }

	/**
	 * Where the hold-to-aim indicator was when the key was released. Returns false (and leaves
	 * OutLocation untouched) if the skill wasn't aimed (a tap / quick cast). Call once in Activate Skill; it clears itself.
	 * Circle skills get the ground spot. Line skills get the crosshair point the line was aimed at.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Aim")
	bool ConsumeAimedLocation(FVector& OutLocation);

	/** Seconds the key must be held before the indicator shows. 0 = shows right away (no tap cast). */
	float GetTapThreshold() const { return TapThreshold; }

	/** True = the cooldown starts when the skill ENDS (summons, buffs), not when it is cast. */
	bool StartsCooldownOnEnd() const { return bCooldownStartsOnEnd; }

	/** Internal (Skill Component): show / hide the aim indicator. */
	void BeginTargeting();
	void EndTargeting(bool bKeepAimedLocation);

	// ------------------------------------------------------------------
	// Getters
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Skill")
	bool IsActive() const { return bIsActive; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	ACharacter* GetOwningCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Skill")
	USkillComponent* GetSkillComponent() const { return OwningComponent; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	ESkillSlot GetSlot() const { return Slot; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	FName GetSkillRow() const { return SkillRow; }

	UFUNCTION(BlueprintPure, Category = "Skill")
	const FSkillDefinition& GetDefinition() const { return Definition; }

	/** Internal: the component calls this right before ActivateSkill. */
	void MarkActive() { bIsActive = true; }

	/** Internal: the component calls this before throwing the skill away (kit rebuilt, player removed). */
	void NotifyRemoved();

protected:
	/** Blueprint hook: runs whenever the skill ends (finished normally or cut short). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill")
	void OnSkillEnded();

	/** Blueprint hook: Hold To Aim started (e.g. bow-draw sound, hand glow). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill|Aim")
	void OnTargetingStarted();

	/** Blueprint hook: Hold To Aim ended. Confirmed = released to cast; false = cancelled. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill|Aim")
	void OnTargetingEnded(bool bConfirmed);

	/** C++ hook for children: clean up (timers, tags, movement) when the skill ends. */
	virtual void NativeOnSkillEnded() {}

	/** C++ hook for children: clean up lasting effects (e.g. remove a mark) when the skill is removed. */
	virtual void NativeOnRemoved() {}

	/**
	 * Where the indicator goes while aiming (called 60x a second). Default:
	 *   Circle = Get Ground Target Location (row Range), Line = Get Crosshair Aim Point (row Range).
	 * Children override it to add rules (Blink stops the circle at walls).
	 */
	virtual FVector ComputeAimLocation() const;

	/** Line indicator: how long the rectangle is in Direction (row Range, shortened by walls). */
	virtual float ComputeLineLength(const FVector& FlatDirection) const;

	/**
	 * Makes a trace ignore the player AND everything they carry: actors attached to them and actors
	 * they own (weapons). Use for every aim / path trace so the player's own weapons never block it.
	 */
	void IgnoreOwnerAndGear(FCollisionQueryParams& Params) const;

	/**
	 * Settings for "world" traces (ground, walls, landing spots): hits whatever the player's capsule
	 * would bump into (static AND movable level geometry, any Blueprint floor), skips triggers,
	 * enemies and the player's own gear. Returns the trace channel to use with Params / Responses.
	 */
	ECollisionChannel GetWorldTraceSettings(FCollisionQueryParams& Params, FCollisionResponseParams& Responses) const;

	/**
	 * Off (default): the cooldown starts the moment the skill is cast.
	 * On: the cooldown starts when the skill ends (End Skill), for lasting skills like summons. The skill
	 * can't be cast again while it is still running.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	bool bCooldownStartsOnEnd = false;

	/** Hold To Aim: marker shown while aiming. Circle: sized to the row's Radius. Line: Radius x 2 wide. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Aim")
	TSubclassOf<ASkillTargetIndicator> TargetIndicatorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Aim")
	EAimIndicatorShape IndicatorShape = EAimIndicatorShape::Circle;

	/**
	 * Hold To Aim: release before this many seconds = tap (quick cast, no indicator).
	 * 0 = the indicator shows the moment the key is pressed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Aim", meta = (ClampMin = "0.0", Units = "s"))
	float TapThreshold = 0.f;

	/** While the indicator shows, WASD does nothing (the mouse still turns the camera and character). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Aim")
	bool bLockMovementWhileAiming = false;

	/** While the indicator shows, the character turns to face where the camera looks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill|Aim")
	bool bFaceAimWhileAiming = false;

	UPROPERTY(Transient)
	TObjectPtr<USkillComponent> OwningComponent;

	UPROPERTY(Transient)
	FName SkillRow;

	UPROPERTY(Transient)
	ESkillSlot Slot = ESkillSlot::Dash;

	UPROPERTY(Transient)
	FSkillDefinition Definition;

private:
	void UpdateTargeting();
	void SetAimMovementMode(bool bEnable);

	bool bIsActive = false;

	bool bIsTargeting = false;
	bool bHasAimedLocation = false;
	FVector AimedLocation = FVector::ZeroVector;
	FTimerHandle TargetingTimer;

	// Movement settings changed while aiming, restored when aiming ends.
	bool bAimModeApplied = false;
	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerDesiredRotation = false;
	TWeakObjectPtr<AController> LockedController;

	UPROPERTY(Transient)
	TObjectPtr<ASkillTargetIndicator> TargetIndicator;
};
