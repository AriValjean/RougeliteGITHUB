// Skill_ArcaneBolt.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_ArcaneBolt.cpp

#include "Skills/Skill_ArcaneBolt.h"
#include "Skills/SkillComponent.h"
#include "Skills/SkillTargetIndicator.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

USkill_ArcaneBolt::USkill_ArcaneBolt()
{
	// Tap = fire now, hold = damage rectangle. Free to move while aiming, faces the camera.
	IndicatorShape = EAimIndicatorShape::Line;
	TapThreshold = 0.2f;
	bLockMovementWhileAiming = false;
	bFaceAimWhileAiming = true;
}

FVector USkill_ArcaneBolt::ComputeAimLocation() const
{
	return GetCrosshairAimPoint(GetModifiedValue(ESkillStat::Distance, Definition.Range));
}

float USkill_ArcaneBolt::ComputeLineLength(const FVector& FlatDirection) const
{
	const float Range = GetModifiedValue(ESkillStat::Distance, Definition.Range);
	if (!bBlockedByWalls)
	{
		return Range;
	}

	const ACharacter* Character = GetOwningCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return Range;
	}

	const FVector Start = Character->GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcaneBoltLine), false);
	FCollisionResponseParams Responses;
	const ECollisionChannel Channel = GetWorldTraceSettings(Params, Responses);
	if (World->LineTraceSingleByChannel(Hit, Start, Start + FlatDirection * Range, Channel, Params, Responses))
	{
		return Hit.Distance;
	}
	return Range;
}

FVector USkill_ArcaneBolt::GetFireLocation() const
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

FVector USkill_ArcaneBolt::GetFlatAimDirection(const FVector& AimPoint) const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return FVector::ForwardVector;
	}
	FVector Direction = AimPoint - Character->GetActorLocation();
	Direction.Z = 0.f;
	Direction = Direction.GetSafeNormal();
	return Direction.IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : Direction;
}

void USkill_ArcaneBolt::ActivateSkill_Implementation()
{
	ACharacter* Character = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || !OwningComponent)
	{
		EndSkill();
		return;
	}

	// Held = where the rectangle pointed on release. Tap = the crosshair now. Same math as the
	// rectangle (flat direction from the player, same length and width), so it hits exactly what it showed.
	FVector AimPoint;
	if (!ConsumeAimedLocation(AimPoint))
	{
		AimPoint = GetCrosshairAimPoint(GetModifiedValue(ESkillStat::Distance, Definition.Range));
	}
	const FVector Direction = GetFlatAimDirection(AimPoint);
	const float Length = ComputeLineLength(Direction);
	const float HalfWidth = FMath::Max(1.f, GetModifiedValue(ESkillStat::Radius, Definition.Radius));

	Character->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));

	// Damage area: a box from the player's center, Length long, Radius x 2 wide, Beam Half Height tall.
	const FVector Origin = Character->GetActorLocation();
	const FVector BoxCenter = Origin + Direction * (Length * 0.5f);
	const FQuat BoxRotation = Direction.Rotation().Quaternion();
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(Length * 0.5f, HalfWidth, BeamHalfHeight));

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcaneBoltBeam), false);
	IgnoreOwnerAndGear(Params);
	World->OverlapMultiByObjectType(Overlaps, BoxCenter, BoxRotation, FCollisionObjectQueryParams(ECC_Pawn), Box, Params);

	// One hit per enemy, closest first.
	TArray<AActor*> Targets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || Targets.Contains(Actor))
		{
			continue;
		}
		if (const APawn* Pawn = Cast<APawn>(Actor); Pawn && Pawn->IsPlayerControlled())
		{
			continue;
		}
		if (UHealthComponent::FindHealthComponent(Actor))
		{
			Targets.Add(Actor);
		}
	}
	Targets.Sort([&Origin](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), Origin) < FVector::DistSquared(B.GetActorLocation(), Origin);
	});

	const float BaseDamage = GetModifiedValue(ESkillStat::Damage, Definition.Damage);
	int32 HitIndex = 0;
	for (AActor* Target : Targets)
	{
		float FinalDamage = 0.f;
		bool bCrit = false;
		if (BaseDamage > 0.f)
		{
			ApplySkillDamage(Target, BaseDamage, Definition.AttackDamageScaling, Definition.bCanCrit,
				EDamageResponse::DefaultReaction, FinalDamage, bCrit);
		}
		OnBeamHit(Target, FinalDamage, ++HitIndex);
	}

	// Visuals: from the hand to the end of the damage area.
	const FVector BeamStart = GetFireLocation();
	const FVector BeamEnd = Origin + Direction * Length + FVector(0.f, 0.f, BeamStart.Z - Origin.Z);
	if (BeamVisualClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector VisualDirection = (BeamEnd - BeamStart).GetSafeNormal();
		if (ASkillTargetIndicator* Beam = World->SpawnActor<ASkillTargetIndicator>(BeamVisualClass, BeamStart,
			VisualDirection.Rotation(), SpawnParams))
		{
			Beam->SetIndicatorLine(FVector::Dist(BeamStart, BeamEnd), HalfWidth * 2.f);
			Beam->SetLifeSpan(BeamVisualTime);
		}
	}

	PlaySkillMontage();
	OnBeamFired(BeamStart, BeamEnd, Direction);

	// The beam is instant.
	EndSkill();
}
