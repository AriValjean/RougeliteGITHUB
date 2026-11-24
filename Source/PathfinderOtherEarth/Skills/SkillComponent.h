// SkillComponent.h
// Owns the player's class kit: Dash, Basic and Ultimate.
//
// Single source of truth:
//   - Current class    : read from UWeaponManagerComponent (kit rebuilds on its OnClassChanged)
//   - CurrentSubclass  : set by the subclass NPCs at the start of stage 3 (SetSubclass). Replaces E_Subclasses.
//   - Cooldowns        : stored as "ready at" world times; the UI asks GetCooldownRemaining.
//   - State tags       : State.Dashing, State.Invincible ... instead of separate booleans.
//   - Skill modifiers  : rebuilt from the accessories held (FAccessoryData.SkillModifiers) whenever they change.
//
// Kit lookup: DT_ClassKitsCPP row named after the class (Rogue ...). If a subclass is set, its row
// (Daredevil ...) overrides any slot it fills in. Skills come from DT_SkillsCPP.
//
// Cooldown = skill modifiers on the row's Cooldown, then the Cooldown Reduction stat
// (UStatsManagerComponent) for Basic and Ultimate only, never Dash.
//
// BPC_SkillManager is a data-only Blueprint child: set the two tables in Class Defaults.
//
// Drop into: Source/PathfinderOtherEarth/Skills/SkillComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Engine/TimerHandle.h"
#include "Items/PathfinderTypes.h"
#include "SkillComponent.generated.h"

class USkillBase;
class UDataTable;
class UWeaponManagerComponent;
class UStatsManagerComponent;
class UAccessoryManagerComponent;
class UHealthComponent;

PATHFINDEROTHEREARTH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dashing);
PATHFINDEROTHEREARTH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Invincible);
/** While the player has this tag (granted by an accessory), abilities can crit. Without it they never do. */
PATHFINDEROTHEREARTH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_CanCrit);

/** Runtime data for one filled slot. */
USTRUCT()
struct FEquippedSkill
{
	GENERATED_BODY()

	UPROPERTY()
	FName SkillRow;

	UPROPERTY()
	TObjectPtr<USkillBase> Skill;

	UPROPERTY()
	FSkillDefinition Definition;

	/** World time (seconds) when the skill can be used again. */
	double ReadyAtTime = 0.0;

	/** Length of the cooldown that was last started (after all reductions). Used by the UI sweep. */
	float LastCooldownDuration = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillKitChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillModifiersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillActivated, ESkillSlot, Slot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillCooldownStarted, ESkillSlot, Slot, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSubclassChanged, ESubclass, NewSubclass);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API USkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillComponent();

	// ------------------------------------------------------------------
	// Using skills (input calls this)
	// ------------------------------------------------------------------

	/** Checks cooldown and the skill's own rules, starts the cooldown, then runs the skill. Cancels any other skill's aiming. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	bool TryActivateSkill(ESkillSlot Slot);

	/**
	 * Key PRESSED. Hold To Aim skills start aiming (indicator right away, or after the skill's Tap Threshold);
	 * other skills just activate. Wire the key's Started here (and Completed to Release Skill Input).
	 */
	UFUNCTION(BlueprintCallable, Category = "Skills|Aim")
	void BeginSkillInput(ESkillSlot Slot);

	/**
	 * Key RELEASED. Casts the aimed skill at the indicator, or quick casts it if released before the
	 * skill's Tap Threshold (a tap). Does nothing for normal skills.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skills|Aim")
	void ReleaseSkillInput(ESkillSlot Slot);

	/** Stops aiming without casting or spending the cooldown (right click, menus...). Safe to call anytime. */
	UFUNCTION(BlueprintCallable, Category = "Skills|Aim")
	void CancelTargeting();

	/** True while a Hold To Aim key is held (including the short tap window before the indicator shows). */
	UFUNCTION(BlueprintPure, Category = "Skills|Aim")
	bool IsTargeting() const { return bIsTargetingSkill; }

	// ------------------------------------------------------------------
	// Class / subclass
	// ------------------------------------------------------------------

	/** Subclass NPCs call this. None = back to the base kit. Rebuilds the kit. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	void SetSubclass(ESubclass NewSubclass);

	UFUNCTION(BlueprintPure, Category = "Skills")
	ESubclass GetCurrentSubclass() const { return CurrentSubclass; }

	/** The class from the weapon manager (Rogue if it can't be found). */
	UFUNCTION(BlueprintPure, Category = "Skills")
	ECharacterClass GetCurrentClass() const;

	/** Ends running skills, clears the slots and fills them again from the kit tables. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	void RebuildKit();

	// ------------------------------------------------------------------
	// Queries (UI)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Skills")
	bool HasSkill(ESkillSlot Slot) const;

	/** The live skill object in this slot (None if empty). Lets skills talk to each other (e.g. Reckoning reads the mark). */
	UFUNCTION(BlueprintPure, Category = "Skills")
	USkillBase* GetSkill(ESkillSlot Slot) const;

	/** Name, icon, base cooldown ... of the skill in this slot. Returns false if the slot is empty. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	bool GetSkillDefinition(ESkillSlot Slot, FSkillDefinition& OutDefinition) const;

	/** Seconds left (0 when ready). */
	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetCooldownRemaining(ESkillSlot Slot) const;

	/** Full length of the current/last cooldown, after reductions. Remaining / Duration = sweep Percent. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetCooldownDuration(ESkillSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Skills")
	bool IsOnCooldown(ESkillSlot Slot) const { return GetCooldownRemaining(Slot) > 0.f; }

	/** Internal (Skill Base): a skill finished. Starts the cooldown of Cooldown Starts On End skills. */
	void NotifySkillEnded(USkillBase* Skill);

	/** Makes the skill ready right away (a skill that fizzled refunds itself; later: reset-cooldown accessories). */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	void ResetCooldown(ESkillSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Skills")
	bool IsAnySkillActive() const;

	/** Cooldown Reduction stat total from the Stats Manager, as a fraction (0.2 = 20%). 0 if missing. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetCooldownReductionPercent() const;

	// ------------------------------------------------------------------
	// Skill modifiers (accessories, shop ability cards)
	// ------------------------------------------------------------------

	/** (Base + Flat) x (1 + Percent) for this slot and stat. Skills call this for distance, damage, etc. */
	UFUNCTION(BlueprintPure, Category = "Skills|Modifiers")
	float GetModifiedSkillValue(ESkillSlot Slot, ESkillStat Stat, float BaseValue) const;

	/** Extra modifier from something that is NOT an accessory (debug, a temporary buff...). Percent is a fraction. */
	UFUNCTION(BlueprintCallable, Category = "Skills|Modifiers")
	void AddSkillModifier(ESkillSlot Slot, ESkillStat Stat, EModifierMode Mode, float Amount);

	/** Clears the extra (non-accessory) modifiers. Accessory modifiers always follow the accessories held. */
	UFUNCTION(BlueprintCallable, Category = "Skills|Modifiers")
	void ClearExtraSkillModifiers();

	/** Rebuilds accessory modifiers from every accessory held. Runs automatically on On Accessory Changed. */
	UFUNCTION(BlueprintCallable, Category = "Skills|Modifiers")
	void RecalculateAccessorySkillModifiers();

	// ------------------------------------------------------------------
	// State tags
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Skills|State")
	void AddStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Skills|State")
	void RemoveStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "Skills|State")
	bool HasStateTag(FGameplayTag Tag) const;

	/** Shortcut for damage code: is State.Invincible active? */
	UFUNCTION(BlueprintPure, Category = "Skills|State")
	bool IsInvincible() const;

	// ------------------------------------------------------------------
	// Event dispatchers
	// ------------------------------------------------------------------

	/** Slots were refilled (class or subclass changed). The skill bar refreshes icons. */
	UPROPERTY(BlueprintAssignable, Category = "Skills|Events")
	FOnSkillKitChanged OnKitChanged;

	/** A skill started. The skill bar plays the pop. */
	UPROPERTY(BlueprintAssignable, Category = "Skills|Events")
	FOnSkillActivated OnSkillActivated;

	/** A cooldown started. The skill bar starts the countdown. */
	UPROPERTY(BlueprintAssignable, Category = "Skills|Events")
	FOnSkillCooldownStarted OnCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "Skills|Events")
	FOnSubclassChanged OnSubclassChanged;

	/** Skill modifiers changed (new accessory etc.). Tooltips can refresh. */
	UPROPERTY(BlueprintAssignable, Category = "Skills|Events")
	FOnSkillModifiersChanged OnSkillModifiersChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** DT_SkillsCPP. The picker only shows tables using the C++ SkillDefinition row struct. */
	UPROPERTY(EditDefaultsOnly, Category = "Skills|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.SkillDefinition"))
	TObjectPtr<UDataTable> SkillTable;

	/** DT_ClassKitsCPP. The picker only shows tables using the C++ ClassKit row struct. */
	UPROPERTY(EditDefaultsOnly, Category = "Skills|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/PathfinderOtherEarth.ClassKit"))
	TObjectPtr<UDataTable> ClassKitTable;

private:
	/** Flat and Percent sums for one slot + stat. */
	struct FSkillModTotals
	{
		float Flat = 0.f;
		float Percent = 0.f;
	};

	static int32 MakeModKey(ESkillSlot Slot, ESkillStat Stat);
	static void AddToBucket(TMap<int32, FSkillModTotals>& Bucket, ESkillSlot Slot, ESkillStat Stat, EModifierMode Mode, float Amount);

	UFUNCTION()
	void HandleClassChanged(ECharacterClass NewClass);

	UFUNCTION()
	void HandleAccessoryChanged(FName AccessoryID, int32 NewCount);

	UFUNCTION()
	void HandleOwnerDeath(AActor* Killer);

	/** Cooldown for this slot right now: skill modifiers, then Cooldown Reduction (never on Dash). */
	float ComputeCooldown(ESkillSlot Slot, const FEquippedSkill& Equipped) const;

	/** Sets "ready at" and tells the skill bar. */
	void StartCooldown(ESkillSlot Slot);

	/** Tap window passed with the key still held: show the indicator. */
	void StartHeldTargeting();

	/** Runs one frame after BeginPlay, once the other managers have restored their saved state. */
	void InitializeAfterLoad();

	void EndAllSkills();
	void RemoveAllSkills();
	void BuildSlot(ESkillSlot Slot, FName SkillRow);
	const FClassKit* FindKitRow(FName RowName) const;

	/** Row exists in DT_SkillsCPP and has a non-abstract Skill Class. */
	bool IsUsableSkillRow(FName SkillRow) const;

	UPROPERTY()
	TMap<ESkillSlot, FEquippedSkill> EquippedSkills;

	UPROPERTY()
	ESubclass CurrentSubclass = ESubclass::None;

	UPROPERTY()
	FGameplayTagContainer StateTags;

	TMap<int32, FSkillModTotals> AccessoryModifiers;
	TMap<int32, FSkillModTotals> ExtraModifiers;

	bool bIsTargetingSkill = false;
	ESkillSlot TargetingSlot = ESkillSlot::Dash;
	FTimerHandle TapWindowTimer;

	TWeakObjectPtr<UWeaponManagerComponent> WeaponManager;
	TWeakObjectPtr<UStatsManagerComponent> StatsManager;
	TWeakObjectPtr<UAccessoryManagerComponent> AccessoryManager;
	TWeakObjectPtr<UHealthComponent> OwnerHealth;
};
