// HealthComponent.h
// Health, Defense and death for anything that can be hit (player and enemies).
// Replaces the health half of BPC_CombatSystem; BPC_CombatSystem becomes a data-only child of this class.
//
// Max Health and Defense are NOT stored here as base values: they are read from the owner's
// StatsManagerComponent (BPC_StatsManager / BPC_EnemyStatsManager) and refreshed automatically
// whenever that component fires On Stats Changed.
//
// Death Defiance: when Can Use Death Defiance is ticked (player only) and a lethal hit lands,
// one charge from the Meta subsystem is used instead of dying. Heal amount and the short
// invulnerability window are set in Project Settings > Game > Meta Progression.
//
// Drop into: Source/PathfinderOtherEarth/Combat/HealthComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PathfinderCombatTypes.h"
#include "HealthComponent.generated.h"

class UHealthComponent;
class UStatsManagerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChangedSignature, UHealthComponent*, HealthComponent, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamagedSignature, const FDamageInfo&, DamageInfo, float, FinalDamage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathSignature, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathDefiedSignature, int32, ChargesRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDamageTakenModifiersChanged);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	// ------------------------------------------------------------------
	// Lookup (replaces the BPI_Damageable interface)
	// ------------------------------------------------------------------

	/** Returns the actor's health component, or None if it can't be damaged. Use Is Valid on the result. */
	UFUNCTION(BlueprintPure, Category = "Health")
	static UHealthComponent* FindHealthComponent(const AActor* Actor);

	// ------------------------------------------------------------------
	// Damage & healing
	// ------------------------------------------------------------------

	/**
	 * Applies one hit. Final Damage = Amount x Damage Taken Multiplier x 100 / (Defense + 100).
	 * Does nothing (returns false) if already dead or invincible.
	 * Fires On Damaged, On Health Changed, and On Death when health reaches 0
	 * (or On Death Defied instead, if a Death Defiance charge was used).
	 */
	UFUNCTION(BlueprintCallable, Category = "Health", meta = (ReturnDisplayName = "Was Damaged"))
	bool TakeDamage(const FDamageInfo& DamageInfo, float& FinalDamage);

	/** Adds health, capped at Max Health. Does nothing when dead. Returns the new current health. */
	UFUNCTION(BlueprintCallable, Category = "Health", meta = (ReturnDisplayName = "New Health"))
	float Heal(float Amount);

	/**
	 * Re-reads Max Health and Defense from the stats component. Gaining Max Health also heals
	 * by the same amount; losing it never kills. Runs automatically on On Stats Changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void RecalculateDefensiveStats();

	/** Sets current health directly (run-state load). Call AFTER the stats component is initialized. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void RestoreCurrentHealth(float NewCurrentHealth);

	/**
	 * Kills instantly: ignores Defense, invincibility and Death Defiance. Fires On Death (Killer may be None)
	 * like a normal death. For debug commands and kill zones.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Kill(AActor* Killer);

	/** Invincible actors ignore Take Damage (dodge i-frames, cutscenes, debug). */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetInvincible(bool bNewInvincible);

	// ------------------------------------------------------------------
	// Damage taken modifiers (marks, vulnerability...)
	// ------------------------------------------------------------------

	/**
	 * Adds or replaces a "takes extra damage" effect from one source (e.g. "MarkedDagger").
	 * Bonus is a fraction: 0.2 = +20% damage taken. Cleared automatically on death.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health|Modifiers")
	void SetDamageTakenModifier(FName Source, float Bonus);

	UFUNCTION(BlueprintCallable, Category = "Health|Modifiers")
	void RemoveDamageTakenModifier(FName Source);

	UFUNCTION(BlueprintPure, Category = "Health|Modifiers")
	bool HasDamageTakenModifier(FName Source) const;

	/** 1 + every active bonus (never below 0). Take Damage multiplies incoming damage by this. */
	UFUNCTION(BlueprintPure, Category = "Health|Modifiers")
	float GetDamageTakenMultiplier() const;

	/** A mark / vulnerability was added or removed. Overhead icons can bind this. */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDamageTakenModifiersChanged OnDamageTakenModifiersChanged;

	// ------------------------------------------------------------------
	// Getters
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	/** Current / Max (0..1), for progress bars. 0 when Max Health is 0. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetDefense() const { return Defense; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	/** True while manually invincible OR inside the post-revive invulnerability window. */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsInvincible() const { return bIsInvincible || bDefianceInvulnerable; }

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	/** Any change to current or max health (damage, heal, stat change, load). Health bars bind this. */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnHealthChangedSignature OnHealthChanged;

	/** A hit landed. Final Damage is after Defense. Damage numbers and hit reactions bind this. */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDamagedSignature OnDamaged;

	/** Health reached 0. Killer is the damage's Instigator (may be None). Fires once. */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDeathSignature OnDeath;

	/** A lethal hit was cancelled by Death Defiance. Hook revive VFX, sound and HUD charge icons here. */
	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnDeathDefiedSignature OnDeathDefied;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Tick on the PLAYER's health component only. Lets lethal hits use a Death Defiance charge instead of killing. */
	UPROPERTY(EditAnywhere, Category = "Health")
	bool bCanUseDeathDefiance = false;

private:
	UFUNCTION()
	void HandleStatsChanged();

	void BroadcastHealthChanged();

	/** Called when health hits 0. Uses a charge and revives if possible; returns true if death was prevented. */
	bool TryDeathDefiance();

	void EndDefianceInvulnerability();

	UPROPERTY(Transient)
	TObjectPtr<UStatsManagerComponent> StatsComponent;

	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	float CurrentHealth = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	float MaxHealth = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	float Defense = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	bool bIsDead = false;

	/** Tick in the Details panel to test without taking damage. */
	UPROPERTY(EditAnywhere, Category = "Health")
	bool bIsInvincible = false;

	/** Separate from bIsInvincible so the revive window never clears a manual/debug invincibility. */
	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	bool bDefianceInvulnerable = false;

	/** False until the stats component has loaded a real base Max Health. Before that, stat changes are ignored. */
	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	bool bHealthInitialized = false;

	/** Source -> bonus fraction (e.g. MarkedDagger -> 0.2). */
	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	TMap<FName, float> DamageTakenModifiers;

	/** A Restore Current Health that arrived before the stats loaded. Applied on initialization. -1 = none. */
	float PendingRestoredHealth = -1.f;

	FTimerHandle DefianceInvulnerabilityTimer;
};
