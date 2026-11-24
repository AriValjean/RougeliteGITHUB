// Skill_Reckoning.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_Reckoning.cpp

#include "Skills/Skill_Reckoning.h"
#include "Skills/Skill_MarkedDagger.h"
#include "Skills/SkillComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/DamageDebug.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

USkill_MarkedDagger* USkill_Reckoning::FindMarkSkill() const
{
	if (!OwningComponent)
	{
		return nullptr;
	}
	for (ESkillSlot SkillSlot : { ESkillSlot::Basic, ESkillSlot::Dash, ESkillSlot::Ultimate })
	{
		if (USkill_MarkedDagger* Mark = Cast<USkill_MarkedDagger>(OwningComponent->GetSkill(SkillSlot)))
		{
			return Mark;
		}
	}
	return nullptr;
}

bool USkill_Reckoning::IsValidDetonationTarget(const AActor* Target) const
{
	const ACharacter* Character = GetOwningCharacter();
	const UWorld* World = GetWorld();
	const UHealthComponent* Health = UHealthComponent::FindHealthComponent(Target);
	if (!Character || !World || !Health || Health->IsDead())
	{
		return false;
	}

	// In range.
	if (Definition.Range > 0.f && FVector::Dist(Character->GetActorLocation(), Target->GetActorLocation()) > Definition.Range)
	{
		return false;
	}

	// In line of sight (walls block it, other characters don't).
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ReckoningSight), false, Character);
	Params.AddIgnoredActor(Target);
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Character->GetActorLocation(), Target->GetActorLocation(),
		ECC_Visibility, Params);
	return !bBlocked;
}

bool USkill_Reckoning::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation())
	{
		return false;
	}
	const USkill_MarkedDagger* Mark = FindMarkSkill();
	return Mark && IsValidDetonationTarget(Mark->GetMarkedTarget());
}

void USkill_Reckoning::ActivateSkill_Implementation()
{
	USkill_MarkedDagger* Mark = FindMarkSkill();
	UWorld* World = GetWorld();
	AActor* Target = nullptr;
	float StoredDamage = 0.f;

	// Consume the mark FIRST, so the detonation doesn't get the mark bonus.
	if (!Mark || !World || !OwningComponent || !Mark->ConsumeMark(Target, StoredDamage) || !Target)
	{
		EndSkill();
		return;
	}

	UHealthComponent* TargetHealth = UHealthComponent::FindHealthComponent(Target);
	if (!TargetHealth)
	{
		EndSkill();
		return;
	}

	// Base part: normal skill damage (Attack Damage scaling + crit roll).
	const float BaseDamage = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Damage, Definition.Damage);
	bool bCrit = false;
	const float BasePart = ComputeSkillDamage(BaseDamage, Definition.AttackDamageScaling, Definition.bCanCrit, bCrit);

	// Stored part: a share of everything dealt while marked (already before Defense).
	const float StoredPart = StoredDamage * Definition.Magnitude;
	const float DetonationDamage = BasePart + StoredPart;

	if (PathfinderDamageDebug::IsEnabled())
	{
		PathfinderDamageDebug::Print(FString::Printf(
			TEXT("[Skill] Reckoning: base part %.1f + %.0f%% of stored %.1f (= %.1f) = %.1f before Defense"),
			BasePart, Definition.Magnitude * 100.f, StoredDamage, StoredPart, DetonationDamage), FColor::Magenta);
	}

	const FVector DetonationLocation = Target->GetActorLocation();
	ACharacter* Character = GetOwningCharacter();

	// Main hit: a normal hit, so Defense applies once.
	FDamageInfo MainHit;
	MainHit.Amount = DetonationDamage;
	MainHit.bIsCritical = bCrit;
	MainHit.DamageResponse = EDamageResponse::Stagger;
	MainHit.Instigator = Character;
	float MainFinal = 0.f;
	TargetHealth->TakeDamage(MainHit, MainFinal);

	// Splash around the target.
	const float SplashRadius = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Radius, Definition.Radius);
	if (SplashRadius > 0.f && Definition.SecondaryMagnitude > 0.f)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ReckoningSplash), false, Character);
		World->OverlapMultiByObjectType(Overlaps, DetonationLocation, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SplashRadius), Params);

		TSet<AActor*> AlreadyHit;
		AlreadyHit.Add(Target);
		if (Character)
		{
			AlreadyHit.Add(Character);
		}

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Other = Overlap.GetActor();
			if (!Other || AlreadyHit.Contains(Other))
			{
				continue;
			}
			AlreadyHit.Add(Other);

			if (const APawn* OtherPawn = Cast<APawn>(Other); OtherPawn && OtherPawn->IsPlayerControlled())
			{
				continue;
			}
			UHealthComponent* OtherHealth = UHealthComponent::FindHealthComponent(Other);
			if (!OtherHealth)
			{
				continue;
			}

			FDamageInfo Splash;
			Splash.Amount = DetonationDamage * Definition.SecondaryMagnitude;
			Splash.bIsCritical = bCrit;
			Splash.DamageResponse = EDamageResponse::DefaultReaction;
			Splash.Instigator = Character;
			float SplashFinal = 0.f;
			if (OtherHealth->TakeDamage(Splash, SplashFinal))
			{
				OnSplashHit(Other, SplashFinal);
			}
		}
	}

	PlaySkillMontage();
	OnDetonated(Target, DetonationLocation, MainFinal, bCrit);
	EndSkill();
}
