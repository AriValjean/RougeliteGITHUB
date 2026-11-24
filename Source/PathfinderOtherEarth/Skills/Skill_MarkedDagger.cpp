// Skill_MarkedDagger.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_MarkedDagger.cpp

#include "Skills/Skill_MarkedDagger.h"
#include "Skills/SkillComponent.h"
#include "Skills/SkillProjectile.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

const FName USkill_MarkedDagger::MarkSourceName(TEXT("MarkedDagger"));

FVector USkill_MarkedDagger::GetThrowLocation() const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return FVector::ZeroVector;
	}
	const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (CharacterMesh && !ThrowSocket.IsNone() && CharacterMesh->DoesSocketExist(ThrowSocket))
	{
		return CharacterMesh->GetSocketLocation(ThrowSocket);
	}
	return Character->GetActorLocation() + FVector(0.f, 0.f, 50.f);
}

void USkill_MarkedDagger::ActivateSkill_Implementation()
{
	ACharacter* Character = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		EndSkill();
		return;
	}

	const FVector Start = GetThrowLocation();
	const FVector AimPoint = GetCrosshairAimPoint(Definition.Range);
	FVector Direction = (AimPoint - Start).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector();
	}

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ASkillProjectile::StaticClass();
	if (ASkillProjectile* Projectile = World->SpawnActor<ASkillProjectile>(ClassToSpawn, Start, Direction.Rotation(), Params))
	{
		Projectile->OnProjectileHitTarget.AddDynamic(this, &USkill_MarkedDagger::HandleProjectileHit);
		Projectile->Launch(Direction, Definition.ProjectileSpeed, Definition.Range);
	}

	PlaySkillMontage();

	// The throw is instant. The mark lives on after the skill has ended.
	EndSkill();
}

void USkill_MarkedDagger::HandleProjectileHit(AActor* HitActor)
{
	if (!HitActor || !OwningComponent)
	{
		return;
	}

	// Small hit first (so the dagger itself doesn't benefit from its own mark).
	const float BaseDamage = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Damage, Definition.Damage);
	if (BaseDamage > 0.f)
	{
		float FinalDamage = 0.f;
		bool bCrit = false;
		ApplySkillDamage(HitActor, BaseDamage, Definition.AttackDamageScaling, Definition.bCanCrit,
			EDamageResponse::DefaultReaction, FinalDamage, bCrit);
	}

	// Don't mark something the dagger just killed.
	const UHealthComponent* Health = UHealthComponent::FindHealthComponent(HitActor);
	if (Health && !Health->IsDead())
	{
		ApplyMark(HitActor);
	}
}

void USkill_MarkedDagger::ApplyMark(AActor* Target)
{
	UHealthComponent* Health = UHealthComponent::FindHealthComponent(Target);
	UWorld* World = GetWorld();
	if (!Health || !World || !OwningComponent)
	{
		return;
	}

	// Only one mark at a time.
	ClearMark();

	MarkedTarget = Target;
	StoredMarkDamage = 0.f;
	Health->SetDamageTakenModifier(MarkSourceName, Definition.Magnitude);
	Health->OnDeath.AddDynamic(this, &USkill_MarkedDagger::HandleMarkedTargetDeath);
	Health->OnDamaged.AddDynamic(this, &USkill_MarkedDagger::HandleMarkedTargetDamaged);

	// Mark icon above the enemy's head.
	if (MarkIndicatorClass)
	{
		const FVector IconLocation = Target->GetActorLocation()
			+ FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight() + IndicatorHeightOffset);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MarkIndicator = World->SpawnActor<AActor>(MarkIndicatorClass, IconLocation, FRotator::ZeroRotator, Params);
		if (MarkIndicator)
		{
			MarkIndicator->AttachToActor(Target, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	// Duration from the data table (Duration skill modifiers apply).
	const float Duration = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Duration, Definition.EffectDuration);
	if (Duration > 0.f)
	{
		World->GetTimerManager().SetTimer(MarkTimer, this, &USkill_MarkedDagger::ClearMark, Duration, false);
	}

	OnTargetMarked(Target);
}

void USkill_MarkedDagger::HandleMarkedTargetDamaged(const FDamageInfo& DamageInfo, float FinalDamage)
{
	// Store the hit BEFORE Defense but WITH marks (Amount x damage-taken multiplier, which includes this mark).
	// Reckoning then detonates a share of it as a normal hit, so Defense applies exactly once.
	if (const UHealthComponent* Health = UHealthComponent::FindHealthComponent(MarkedTarget.Get()))
	{
		StoredMarkDamage += DamageInfo.Amount * Health->GetDamageTakenMultiplier();
	}
}

bool USkill_MarkedDagger::ConsumeMark(AActor*& OutTarget, float& OutStoredDamage)
{
	OutTarget = MarkedTarget.Get();
	OutStoredDamage = StoredMarkDamage;
	if (!OutTarget)
	{
		return false;
	}
	ClearMark();
	return true;
}

void USkill_MarkedDagger::HandleMarkedTargetDeath(AActor* Killer)
{
	ClearMark();
}

void USkill_MarkedDagger::ClearMark()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MarkTimer);
	}

	if (MarkIndicator)
	{
		MarkIndicator->Destroy();
		MarkIndicator = nullptr;
	}

	AActor* OldTarget = MarkedTarget.Get();
	MarkedTarget.Reset();
	StoredMarkDamage = 0.f;
	if (!OldTarget)
	{
		return;
	}

	if (UHealthComponent* Health = UHealthComponent::FindHealthComponent(OldTarget))
	{
		Health->RemoveDamageTakenModifier(MarkSourceName);
		Health->OnDeath.RemoveDynamic(this, &USkill_MarkedDagger::HandleMarkedTargetDeath);
		Health->OnDamaged.RemoveDynamic(this, &USkill_MarkedDagger::HandleMarkedTargetDamaged);
	}

	OnMarkEnded(OldTarget);
}

void USkill_MarkedDagger::NativeOnRemoved()
{
	// Class changed / player removed: take the mark off the enemy too.
	ClearMark();
}
