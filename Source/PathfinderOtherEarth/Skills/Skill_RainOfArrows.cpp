// Skill_RainOfArrows.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_RainOfArrows.cpp

#include "Skills/Skill_RainOfArrows.h"
#include "Skills/SkillComponent.h"
#include "Skills/SkillZone.h"
#include "Combat/StatusEffectComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

const FName USkill_RainOfArrows::SlowSourceName(TEXT("RainOfArrows"));

void USkill_RainOfArrows::ActivateSkill_Implementation()
{
	ACharacter* Character = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || !OwningComponent)
	{
		EndSkill();
		return;
	}

	// Where the hold-to-aim indicator was on release; a direct cast uses the crosshair now.
	FVector Location;
	if (!ConsumeAimedLocation(Location))
	{
		Location = GetGroundTargetLocation(Definition.Range);
	}

	const float Radius = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Radius, Definition.Radius);
	const float Duration = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Duration, Definition.EffectDuration);
	CurrentTickInterval = Definition.SecondaryMagnitude > 0.f ? Definition.SecondaryMagnitude : 0.5f;

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* ClassToSpawn = ZoneClass ? ZoneClass.Get() : ASkillZone::StaticClass();

	if (ASkillZone* Zone = World->SpawnActor<ASkillZone>(ClassToSpawn, Location, FRotator::ZeroRotator, Params))
	{
		Zone->OnZoneTickTarget.AddDynamic(this, &USkill_RainOfArrows::HandleZoneTarget);
		Zone->StartZone(Radius, Duration, CurrentTickInterval);
		ActiveZone = Zone;
	}

	PlaySkillMontage();
	OnRainStarted(Location);

	// The cast is instant; the zone runs on its own.
	EndSkill();
}

void USkill_RainOfArrows::HandleZoneTarget(AActor* Target)
{
	if (!Target || !OwningComponent)
	{
		return;
	}

	const float BaseDamage = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Damage, Definition.Damage);
	if (BaseDamage > 0.f)
	{
		float FinalDamage = 0.f;
		bool bCrit = false;
		ApplySkillDamage(Target, BaseDamage, Definition.AttackDamageScaling, Definition.bCanCrit,
			EDamageResponse::None, FinalDamage, bCrit);
	}

	// Slowed while inside: refreshed every tick, wears off ~0.5 s after leaving.
	if (Definition.Magnitude > 0.f)
	{
		if (UStatusEffectComponent* Status = UStatusEffectComponent::FindStatusEffects(Target))
		{
			Status->ApplySlow(SlowSourceName, Definition.Magnitude, CurrentTickInterval + 0.5f);
		}
	}
}

void USkill_RainOfArrows::NativeOnRemoved()
{
	if (ASkillZone* Zone = ActiveZone.Get())
	{
		Zone->FinishZone();
	}
	ActiveZone.Reset();
}
