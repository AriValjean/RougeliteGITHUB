// StatsManagerComponent.cpp
// Drop into: Source/PathfinderOtherEarth/Stats/StatsManagerComponent.cpp

#include "StatsManagerComponent.h"

UStatsManagerComponent::UStatsManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStatsManagerComponent::InitializeStats()
{
	BaseStats.Empty();

	const FCharacterStatsRow* Row = ClassStatsRow.GetRow<FCharacterStatsRow>(TEXT("UStatsManagerComponent::InitializeStats"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ClassStatsRow '%s' not found in '%s' -- stats will be 0."),
			*GetNameSafe(GetOwner()),
			*ClassStatsRow.RowName.ToString(),
			ClassStatsRow.DataTable ? *ClassStatsRow.DataTable->GetName() : TEXT("None"));
		OnStatsChanged.Broadcast();
		return;
	}

	BaseStats.Add(ECharacterStatType::MaxHealth,         Row->BaseMaxHealth);
	BaseStats.Add(ECharacterStatType::Defense,           Row->BaseDefense);
	BaseStats.Add(ECharacterStatType::MovementSpeed,     Row->BaseMovementSpeed);
	BaseStats.Add(ECharacterStatType::AttackDamage,      Row->BaseAttackDamage);
	BaseStats.Add(ECharacterStatType::AttackSpeed,       Row->BaseAttackSpeed);
	BaseStats.Add(ECharacterStatType::CriticalRate,      Row->BaseCritRate);
	BaseStats.Add(ECharacterStatType::CriticalDamage,    Row->BaseCritDamage);
	BaseStats.Add(ECharacterStatType::CooldownReduction, Row->BaseCooldownReduction);

	SprintMultiplier = Row->SprintMultiplier;

	OnStatsChanged.Broadcast();
}

float UStatsManagerComponent::GetBaseStat(ECharacterStatType StatType) const
{
	const float* Found = BaseStats.Find(StatType);
	return Found ? *Found : 0.f;
}

void UStatsManagerComponent::AddModifier(ECharacterStatType StatType, EModifierMode Mode, float Amount)
{
	TMap<ECharacterStatType, float>& Target = (Mode == EModifierMode::Flat) ? FlatBonuses : PercentBonuses;
	Target.FindOrAdd(StatType) += Amount;

	OnStatsChanged.Broadcast();
}

float UStatsManagerComponent::GetTotal(ECharacterStatType Stat) const
{
	const float Base = GetBaseStat(Stat);
	const float* Flat = FlatBonuses.Find(Stat);
	const float* Percent = PercentBonuses.Find(Stat);

	return (Base + (Flat ? *Flat : 0.f)) * (1.f + (Percent ? *Percent : 0.f));
}

void UStatsManagerComponent::GetStatTotal(ECharacterStatType Stat, float& Total) const
{
	Total = GetTotal(Stat);
}

FStatBreakdown UStatsManagerComponent::GetStatBreakdown(ECharacterStatType StatType) const
{
	FStatBreakdown Breakdown;
	Breakdown.Base = GetBaseStat(StatType);
	Breakdown.Total = GetTotal(StatType);
	Breakdown.AccessoryBonus = Breakdown.Total - Breakdown.Base;
	Breakdown.MetaBonus = 0.f;
	Breakdown.bIsPercentDisplay = IsPercentStat(StatType);
	return Breakdown;
}

void UStatsManagerComponent::ResetRunBonuses()
{
	FlatBonuses.Empty();
	PercentBonuses.Empty();
	OnStatsChanged.Broadcast();
}

bool UStatsManagerComponent::IsPercentStat(ECharacterStatType StatType)
{
	return StatType == ECharacterStatType::CriticalRate
		|| StatType == ECharacterStatType::CriticalDamage
		|| StatType == ECharacterStatType::CooldownReduction;
}
