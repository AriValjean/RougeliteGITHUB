// HealthComponent.cpp
// Drop into: Source/PathfinderOtherEarth/Combat/HealthComponent.cpp

#include "HealthComponent.h"

#include "GameFramework/Actor.h"
#include "Stats/StatsManagerComponent.h"
#include "Skills/SkillComponent.h"
#include "Combat/DamageDebug.h"
#include "Meta/PathfinderMetaSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHealth, Log, All);

namespace
{
	/** True while the owner's skill component has State.Invincible (e.g. dash i-frames). */
	bool IsSkillInvincible(const AActor* Owner)
	{
		const USkillComponent* Skills = Owner ? Owner->FindComponentByClass<USkillComponent>() : nullptr;
		return Skills && Skills->IsInvincible();
	}
}

UHealthComponent::UHealthComponent()
{
	// Event-driven: nothing to do every frame.
	PrimaryComponentTick.bCanEverTick = false;
}

UHealthComponent* UHealthComponent::FindHealthComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
}

// ----------------------------------------------------------------------
// Lifetime
// ----------------------------------------------------------------------

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// FindComponentByClass also finds Blueprint children (BPC_StatsManager, BPC_EnemyStatsManager).
	StatsComponent = GetOwner() ? GetOwner()->FindComponentByClass<UStatsManagerComponent>() : nullptr;

	if (!StatsComponent)
	{
		UE_LOG(LogHealth, Warning, TEXT("%s: owner %s has no StatsManagerComponent, Max Health and Defense stay 0."),
			*GetNameSafe(this), *GetNameSafe(GetOwner()));
		return;
	}

	// Whichever order the components start in, health ends up correct:
	// - stats already loaded: this first recalc picks them up (0 -> 200 max also fills current to 200)
	// - stats load later: InitializeStats fires On Stats Changed, which lands in HandleStatsChanged.
	StatsComponent->OnStatsChanged.AddDynamic(this, &UHealthComponent::HandleStatsChanged);
	RecalculateDefensiveStats();
}

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StatsComponent)
	{
		StatsComponent->OnStatsChanged.RemoveDynamic(this, &UHealthComponent::HandleStatsChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DefianceInvulnerabilityTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UHealthComponent::HandleStatsChanged()
{
	RecalculateDefensiveStats();
}

// ----------------------------------------------------------------------
// Stats
// ----------------------------------------------------------------------

void UHealthComponent::RecalculateDefensiveStats()
{
	if (!StatsComponent)
	{
		return;
	}

	float NewMaxHealth = 0.f;
	StatsComponent->GetStatTotal(ECharacterStatType::MaxHealth, NewMaxHealth);
	StatsComponent->GetStatTotal(ECharacterStatType::Defense, Defense);

	// Components start in no guaranteed order. Until the stats component has loaded its row
	// (base Max Health > 0), any total is incomplete (e.g. only accessory bonuses), so ignore it.
	const bool bStatsLoaded = StatsComponent->GetStatBreakdown(ECharacterStatType::MaxHealth).Base > 0.f;
	if (!bStatsLoaded)
	{
		return;
	}

	// First real load = setting health up, not "gaining" max health.
	// Use a restore that arrived early (run-state load), otherwise start full.
	if (!bHealthInitialized)
	{
		bHealthInitialized = true;
		MaxHealth = NewMaxHealth;
		CurrentHealth = PendingRestoredHealth >= 0.f
			? FMath::Clamp(PendingRestoredHealth, FMath::Min(1.f, MaxHealth), MaxHealth)
			: MaxHealth;
		PendingRestoredHealth = -1.f;

		BroadcastHealthChanged();
		return;
	}

	// After that: carry the change in max health into current health, so +Max Health items also heal.
	// This writes MaxHealth (never a base value), so repeated calls don't compound.
	const float Delta = NewMaxHealth - MaxHealth;
	MaxHealth = NewMaxHealth;

	if (!bIsDead)
	{
		// Losing max health never kills: keep at least 1 health while alive.
		const float MinWhileAlive = FMath::Min(1.f, MaxHealth);
		CurrentHealth = FMath::Clamp(CurrentHealth + Delta, MinWhileAlive, MaxHealth);
	}

	BroadcastHealthChanged();
}

void UHealthComponent::RestoreCurrentHealth(float NewCurrentHealth)
{
	if (bIsDead)
	{
		return;
	}

	// Stats not loaded yet: hold the value and apply it when they are (see RecalculateDefensiveStats).
	if (!bHealthInitialized)
	{
		PendingRestoredHealth = NewCurrentHealth;
		return;
	}

	CurrentHealth = FMath::Clamp(NewCurrentHealth, FMath::Min(1.f, MaxHealth), MaxHealth);
	BroadcastHealthChanged();
}

void UHealthComponent::SetInvincible(bool bNewInvincible)
{
	bIsInvincible = bNewInvincible;
}

// ----------------------------------------------------------------------
// Damage & healing
// ----------------------------------------------------------------------

bool UHealthComponent::TakeDamage(const FDamageInfo& DamageInfo, float& FinalDamage)
{
	FinalDamage = 0.f;

	// Skill i-frames (dash) live on the skill component as State.Invincible.
	if (bIsDead || IsInvincible() || IsSkillInvincible(GetOwner()) || DamageInfo.Amount <= 0.f)
	{
		return false;
	}

	// Marks / vulnerability on this target (e.g. Marked Dagger +20%).
	const float TakenMultiplier = GetDamageTakenMultiplier();

	// RoR2-style diminishing returns: 100 Defense = half damage. Negative Defense is treated as 0.
	const float SafeDefense = FMath::Max(Defense, 0.f);
	const float DefenseFactor = 100.f / (SafeDefense + 100.f);
	FinalDamage = DamageInfo.Amount * TakenMultiplier * DefenseFactor;

	if (PathfinderDamageDebug::IsEnabled())
	{
		PathfinderDamageDebug::Print(FString::Printf(
			TEXT("[Hit] %s <- %s: incoming %.1f%s x taken %.2f x defense %.2f (Def %.0f) = %.1f"),
			*GetNameSafe(GetOwner()), *GetNameSafe(DamageInfo.Instigator), DamageInfo.Amount,
			DamageInfo.bIsCritical ? TEXT(" (CRIT)") : TEXT(""), TakenMultiplier, DefenseFactor, SafeDefense, FinalDamage),
			FColor::Orange);
	}

	// Never below 0, so health bars and run-state saves don't see negative health.
	CurrentHealth = FMath::Max(CurrentHealth - FinalDamage, 0.f);

	OnDamaged.Broadcast(DamageInfo, FinalDamage);

	// Lethal hit: try Death Defiance before dying.
	if (CurrentHealth <= 0.f && TryDeathDefiance())
	{
		BroadcastHealthChanged();
		return true;
	}

	BroadcastHealthChanged();

	if (CurrentHealth <= 0.f)
	{
		bIsDead = true;
		DamageTakenModifiers.Reset(); // marks end on death
		OnDamageTakenModifiersChanged.Broadcast();
		OnDeath.Broadcast(DamageInfo.Instigator);
	}

	return true;
}

void UHealthComponent::Kill(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	CurrentHealth = 0.f;
	bIsDead = true;
	DamageTakenModifiers.Reset();
	OnDamageTakenModifiersChanged.Broadcast();
	BroadcastHealthChanged();
	OnDeath.Broadcast(Killer);
}

bool UHealthComponent::TryDeathDefiance()
{
	if (!bCanUseDeathDefiance)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UPathfinderMetaSubsystem* Meta = GI ? GI->GetSubsystem<UPathfinderMetaSubsystem>() : nullptr;
	if (!Meta || !Meta->TryConsumeDeathDefiance())
	{
		return false;
	}

	// Revive at the configured fraction of max health (Project Settings > Game > Meta Progression).
	CurrentHealth = FMath::Max(MaxHealth * Meta->GetDeathDefianceHealFraction(), FMath::Min(1.f, MaxHealth));

	// Short invulnerability so the same attack (or a follow-up) can't kill straight away.
	const float InvulnTime = Meta->GetDeathDefianceInvulnerabilityTime();
	if (InvulnTime > 0.f)
	{
		bDefianceInvulnerable = true;
		GetWorld()->GetTimerManager().SetTimer(DefianceInvulnerabilityTimer, this, &UHealthComponent::EndDefianceInvulnerability, InvulnTime, false);
	}

	UE_LOG(LogHealth, Log, TEXT("%s: Death Defiance used, revived at %.0f health (%d charges left)."),
		*GetNameSafe(GetOwner()), CurrentHealth, Meta->GetDeathDefianceRemaining());

	OnDeathDefied.Broadcast(Meta->GetDeathDefianceRemaining());
	return true;
}

void UHealthComponent::EndDefianceInvulnerability()
{
	bDefianceInvulnerable = false;
}

float UHealthComponent::Heal(float Amount)
{
	if (bIsDead || Amount <= 0.f)
	{
		return CurrentHealth;
	}

	CurrentHealth = FMath::Min(CurrentHealth + Amount, MaxHealth);
	BroadcastHealthChanged();
	return CurrentHealth;
}

// ----------------------------------------------------------------------
// Getters / helpers
// ----------------------------------------------------------------------

float UHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;
}

void UHealthComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(this, CurrentHealth, MaxHealth);
}

// ----------------------------------------------------------------------
// Damage taken modifiers
// ----------------------------------------------------------------------

void UHealthComponent::SetDamageTakenModifier(FName Source, float Bonus)
{
	if (Source.IsNone() || bIsDead)
	{
		return;
	}
	DamageTakenModifiers.Add(Source, Bonus);
	OnDamageTakenModifiersChanged.Broadcast();
}

void UHealthComponent::RemoveDamageTakenModifier(FName Source)
{
	if (DamageTakenModifiers.Remove(Source) > 0)
	{
		OnDamageTakenModifiersChanged.Broadcast();
	}
}

bool UHealthComponent::HasDamageTakenModifier(FName Source) const
{
	return DamageTakenModifiers.Contains(Source);
}

float UHealthComponent::GetDamageTakenMultiplier() const
{
	float Multiplier = 1.f;
	for (const TPair<FName, float>& Pair : DamageTakenModifiers)
	{
		Multiplier += Pair.Value;
	}
	return FMath::Max(0.f, Multiplier);
}
