// Skill_Volley.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_Volley.cpp

#include "Skills/Skill_Volley.h"
#include "Skills/SkillComponent.h"
#include "Skills/SkillProjectile.h"
#include "Combat/StatusEffectComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

const FName USkill_Volley::SlowSourceName(TEXT("Volley"));

FVector USkill_Volley::GetFireLocation() const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return FVector::ZeroVector;
	}
	const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (CharacterMesh && !FireSocket.IsNone() && CharacterMesh->DoesSocketExist(FireSocket))
	{
		return CharacterMesh->GetSocketLocation(FireSocket);
	}
	return Character->GetActorLocation() + FVector(0.f, 0.f, 50.f);
}

void USkill_Volley::ActivateSkill_Implementation()
{
	ACharacter* Character = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		EndSkill();
		return;
	}

	const FVector Start = GetFireLocation();
	FVector AimDirection = (GetCrosshairAimPoint(Definition.Range) - Start).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = Character->GetActorForwardVector();
	}

	const int32 Count = FMath::Max(1, Definition.ProjectileCount);
	const float Spread = FMath::Clamp(Definition.SpreadAngle, 0.f, 180.f);
	const float Step = (Count > 1) ? Spread / (Count - 1) : 0.f;
	const float FirstAngle = (Count > 1) ? -Spread * 0.5f : 0.f;

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ASkillProjectile::StaticClass();

	// Even fan around the crosshair direction (turning left/right around the up axis).
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector ArrowDirection = AimDirection.RotateAngleAxis(FirstAngle + Step * i, FVector::UpVector);
		if (ASkillProjectile* Arrow = World->SpawnActor<ASkillProjectile>(ClassToSpawn, Start, ArrowDirection.Rotation(), Params))
		{
			Arrow->OnProjectileHitTarget.AddDynamic(this, &USkill_Volley::HandleArrowHit);
			Arrow->Launch(ArrowDirection, Definition.ProjectileSpeed, Definition.Range);
		}
	}

	PlaySkillMontage();
	OnVolleyFired(AimDirection);

	// Firing is instant; the arrows finish on their own.
	EndSkill();
}

void USkill_Volley::HandleArrowHit(AActor* HitActor)
{
	if (!HitActor || !OwningComponent)
	{
		return;
	}

	// Damage per arrow.
	const float BaseDamage = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Damage, Definition.Damage);
	float FinalDamage = 0.f;
	bool bCrit = false;
	if (BaseDamage > 0.f)
	{
		ApplySkillDamage(HitActor, BaseDamage, Definition.AttackDamageScaling, Definition.bCanCrit,
			EDamageResponse::DefaultReaction, FinalDamage, bCrit);
	}

	// Slow (Duration skill modifiers apply).
	if (UStatusEffectComponent* Status = UStatusEffectComponent::FindStatusEffects(HitActor))
	{
		const float SlowDuration = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Duration, Definition.EffectDuration);
		Status->ApplySlow(SlowSourceName, Definition.Magnitude, SlowDuration);
	}

	OnArrowHit(HitActor, FinalDamage);
}
