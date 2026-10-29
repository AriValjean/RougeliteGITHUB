// WeaponBase.cpp
// Drop into: Source/PathfinderOtherEarth/Items/WeaponBase.cpp

#include "WeaponBase.h"

#include "Stats/StatsManagerComponent.h"

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

	// --- Crit chance: class stat + weapon slot bonus (fractions, 0.1 = 10%) ---
	float CritRateStat = 0.f;
	Stats->GetStatTotal(ECharacterStatType::CriticalRate, CritRateStat);
	const float CritChance = FMath::Clamp(CritRateStat + SlotCritRate, 0.f, 1.f);

	// Rolled once and stored, same as the old Set Rolled Crit.
	bIsCritical = FMath::FRand() < CritChance;

	if (!bIsCritical)
	{
		return BaseDamage;
	}

	// --- Crit multiplier: class stat (base 1.5) + weapon slot bonus ---
	float CritDamageStat = 0.f;
	Stats->GetStatTotal(ECharacterStatType::CriticalDamage, CritDamageStat);
	const float CritMultiplier = CritDamageStat + SlotCritDamage;

	return BaseDamage * CritMultiplier;
}
