// Skill_ArcaneFamiliar.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_ArcaneFamiliar.cpp

#include "Skills/Skill_ArcaneFamiliar.h"
#include "Skills/SkillComponent.h"
#include "Skills/SkillFamiliar.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

USkill_ArcaneFamiliar::USkill_ArcaneFamiliar()
{
	// The skill stays active while the familiar is out; the cooldown starts when it leaves.
	bCooldownStartsOnEnd = true;
}

void USkill_ArcaneFamiliar::ActivateSkill_Implementation()
{
	ACharacter* Character = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || !OwningComponent)
	{
		EndSkill();
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* ClassToSpawn = FamiliarClass ? FamiliarClass.Get() : ASkillFamiliar::StaticClass();

	ASkillFamiliar* Familiar = World->SpawnActor<ASkillFamiliar>(ClassToSpawn, Character->GetActorLocation(),
		Character->GetActorRotation(), Params);
	if (!Familiar)
	{
		EndSkill();
		return;
	}

	ActiveFamiliar = Familiar;
	Familiar->OnBoltHitTarget.AddDynamic(this, &USkill_ArcaneFamiliar::HandleBoltHit);
	Familiar->OnFamiliarExpired.AddDynamic(this, &USkill_ArcaneFamiliar::HandleFamiliarExpired);

	const float Duration = GetModifiedValue(ESkillStat::Duration, Definition.EffectDuration);
	const float FireInterval = Definition.SecondaryMagnitude > 0.f ? Definition.SecondaryMagnitude : 0.5f;
	const float TargetRange = GetModifiedValue(ESkillStat::Distance, Definition.Range);
	Familiar->StartFamiliar(Duration, FireInterval, TargetRange, Definition.ProjectileSpeed);

	PlaySkillMontage();
	OnFamiliarSummoned(Familiar);

	// No End Skill here: the skill ends when the familiar leaves (Handle Familiar Expired).
}

void USkill_ArcaneFamiliar::HandleBoltHit(AActor* HitActor)
{
	if (!HitActor || !OwningComponent)
	{
		return;
	}

	const float BaseDamage = GetModifiedValue(ESkillStat::Damage, Definition.Damage);
	float FinalDamage = 0.f;
	bool bCrit = false;
	if (BaseDamage > 0.f)
	{
		ApplySkillDamage(HitActor, BaseDamage, Definition.AttackDamageScaling, Definition.bCanCrit,
			EDamageResponse::DefaultReaction, FinalDamage, bCrit);
	}
	OnFamiliarHit(HitActor, FinalDamage);
}

void USkill_ArcaneFamiliar::HandleFamiliarExpired()
{
	ActiveFamiliar = nullptr;
	OnFamiliarExpired();
	EndSkill(); // starts the cooldown
}

void USkill_ArcaneFamiliar::NativeOnSkillEnded()
{
	// Ended early (death, class change, kit rebuilt): send the familiar away too.
	if (ASkillFamiliar* Familiar = ActiveFamiliar.Get())
	{
		ActiveFamiliar = nullptr;
		Familiar->OnFamiliarExpired.RemoveDynamic(this, &USkill_ArcaneFamiliar::HandleFamiliarExpired);
		Familiar->Dismiss();
	}
}
