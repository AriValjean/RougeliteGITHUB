// WeaponBase.cpp
// Drop into: Source/PathfinderOtherEarth/Items/WeaponBase.cpp

#include "WeaponBase.h"

#include "Stats/StatsManagerComponent.h"
#include "Combat/DamageDebug.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponBase, Log, All);

AWeaponBase::AWeaponBase()
{
    // Tick settings are left to the Blueprint child (BP_WeaponBase) so current behaviour is unchanged.
}

float AWeaponBase::ComputeHitDamage(float BaseDamage, float SlotCritRate, float SlotCritDamage, bool& bIsCritical)
{
    bIsCritical = false;

    // The weapon manager spawns weapons with Owner = the character, so Get Owner is the character.
    // FindComponentByClass also finds Blueprint children of the C++ component (BPC_StatsManager).
    const AActor* OwnerActor = GetOwner();
    const UStatsManagerComponent* Stats = OwnerActor ? OwnerActor->FindComponentByClass<UStatsManagerComponent>() : nullptr;

    if (!Stats)
    {
       // No stats = no crit. Still return the base damage so the hit isn't lost.
       UE_LOG(LogWeaponBase, Warning, TEXT("ComputeHitDamage: %s has no owner with a StatsManagerComponent."), *GetNameSafe(this));
       return BaseDamage;
    }

    // --- Attack Damage stat: a multiplier (base 1.0 = 100%). Accessories / meta raise it. ---
    // Same rule as skills (USkillBase::ComputeSkillDamage), so weapons and skills scale together.
    float AttackDamageStat = 1.f;
    Stats->GetStatTotal(ECharacterStatType::AttackDamage, AttackDamageStat);
    const float ScaledDamage = BaseDamage * AttackDamageStat;

    // --- Crit chance: class stat + weapon slot bonus (fractions, 0.1 = 10%) ---
    float CritRateStat = 0.f;
    Stats->GetStatTotal(ECharacterStatType::CriticalRate, CritRateStat);
    const float CritChance = FMath::Clamp(CritRateStat + SlotCritRate, 0.f, 1.f);

    // Rolled once and stored, same as the old Set Rolled Crit.
    bIsCritical = FMath::FRand() < CritChance;

    if (!bIsCritical)
    {
       if (PathfinderDamageDebug::IsEnabled())
       {
          PathfinderDamageDebug::Print(FString::Printf(TEXT("[Weapon] %s: base %.1f x attack %.2f = %.1f (no crit, chance %.0f%%)"),
             *GetNameSafe(this), BaseDamage, AttackDamageStat, ScaledDamage, CritChance * 100.f));
       }
       return ScaledDamage;
    }

    // --- Crit multiplier: class stat (base 1.5) + weapon slot bonus ---
    float CritDamageStat = 0.f;
    Stats->GetStatTotal(ECharacterStatType::CriticalDamage, CritDamageStat);
    const float CritMultiplier = CritDamageStat + SlotCritDamage;

    if (PathfinderDamageDebug::IsEnabled())
    {
       PathfinderDamageDebug::Print(FString::Printf(TEXT("[Weapon] %s: base %.1f x attack %.2f = %.1f, CRIT x%.2f = %.1f"),
          *GetNameSafe(this), BaseDamage, AttackDamageStat, ScaledDamage, CritMultiplier, ScaledDamage * CritMultiplier));
    }
    return ScaledDamage * CritMultiplier;
}
