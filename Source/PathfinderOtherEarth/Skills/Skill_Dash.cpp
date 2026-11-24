// Skill_Dash.cpp

#include "Skills/Skill_Dash.h"
#include "Skills/SkillComponent.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool USkill_Dash::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation())
	{
		return false;
	}
	const ACharacter* Character = GetOwningCharacter();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || Movement->MovementMode == MOVE_None)
	{
		return false;
	}
	return bAllowInAir || !Movement->IsFalling();
}

FVector USkill_Dash::GetDashDirection() const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return FVector::ForwardVector;
	}

	// Flat camera/crosshair forward and right (yaw only).
	const FRotator AimYaw(0.f, Character->GetControlRotation().Yaw, 0.f);
	const FVector AimForward = AimYaw.Vector();
	const FVector AimRight = FRotationMatrix(AimYaw).GetUnitAxis(EAxis::Y);

	// Holding left/right (A/D) relative to the camera: -1 = left, +1 = right, 0 = neither.
	float SideSign = 0.f;
	{
		FVector Input = Character->GetLastMovementInputVector();
		Input.Z = 0.f;
		if (Input.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			const float Side = FVector::DotProduct(Input.GetSafeNormal(), AimRight);
			SideSign = Side > 0.3f ? 1.f : (Side < -0.3f ? -1.f : 0.f);
		}
	}
	const float SideRadians = FMath::DegreesToRadians(SideInputAngle);

	switch (Direction)
	{
	case EDashDirection::AwayFromAim:
		// Straight back, tilted toward A/D by Side Input Angle.
		return (-AimForward * FMath::Cos(SideRadians * FMath::Abs(SideSign))
			+ AimRight * SideSign * FMath::Sin(SideRadians)).GetSafeNormal();
	case EDashDirection::TowardAim:
		return (AimForward * FMath::Cos(SideRadians * FMath::Abs(SideSign))
			+ AimRight * SideSign * FMath::Sin(SideRadians)).GetSafeNormal();
	case EDashDirection::MovementInput:
	default:
	{
		FVector Input = Character->GetLastMovementInputVector();
		Input.Z = 0.f;
		if (Input.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			return Input.GetSafeNormal();
		}
		return Character->GetActorForwardVector().GetSafeNormal2D();
	}
	}
}

void USkill_Dash::ActivateSkill_Implementation()
{
	ACharacter* Character = GetOwningCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	UWorld* World = GetWorld();
	if (!Movement || !World || !OwningComponent)
	{
		EndSkill();
		return;
	}

	// Accessories / ability cards can change these (e.g. +20% dash distance).
	const float FinalDistance = FMath::Max(0.f, OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Distance, DashDistance));
	const float FinalDuration = FMath::Max(0.05f, OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Duration, DashDuration));

	CurrentDashDirection = GetDashDirection();

	// Facing: a step back keeps looking at the crosshair, other dashes face where they go.
	if (bRotateToDashDirection)
	{
		// A step back keeps facing the crosshair exactly (even when angled by A/D).
		const float FaceYaw = (Direction == EDashDirection::AwayFromAim)
			? Character->GetControlRotation().Yaw
			: CurrentDashDirection.Rotation().Yaw;
		Character->SetActorRotation(FRotator(0.f, FaceYaw, 0.f));
	}

	// State tags (damage checks State.Invincible, CanPerformAction checks State.Dashing).
	OwningComponent->AddStateTag(TAG_State_Dashing);
	if (bInvincibleDuringDash)
	{
		OwningComponent->AddStateTag(TAG_State_Invincible);
	}

	// Let the capsule slide through enemies (restored when the dash ends).
	if (bPassThroughEnemies)
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			SavedPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			bChangedCollision = true;
		}
	}

	// Root Motion Source: fixed distance over fixed time.
	TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
	Force->InstanceName = TEXT("SkillDash");
	Force->AccumulateMode = ERootMotionAccumulateMode::Override;
	Force->Priority = 5;
	Force->Force = CurrentDashDirection * (FinalDistance / FinalDuration);
	Force->Duration = FinalDuration;
	// Keep some momentum after the dash, but never faster than normal run speed.
	Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Force->FinishVelocityParams.ClampVelocity = Movement->MaxWalkSpeed;
	RootMotionID = Movement->ApplyRootMotionSource(Force);

	// Damage dashes: sweep the path every frame.
	if (bDealsDamage)
	{
		HitActorsThisDash.Reset();
		LastSweepLocation = Character->GetActorLocation();
		SweepForHits(); // catches enemies already touching us at the start
		World->GetTimerManager().SetTimer(HitTimer, this, &USkill_Dash::SweepForHits, 1.f / 60.f, true);
	}

	PlaySkillMontage();
	OnDashStarted(CurrentDashDirection);

	World->GetTimerManager().SetTimer(DashTimer, this, &USkill_Dash::FinishDash, FinalDuration, false);
}

void USkill_Dash::SweepForHits()
{
	ACharacter* Character = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || !OwningComponent)
	{
		return;
	}

	const FVector Current = Character->GetActorLocation();
	const float Radius = FMath::Max(1.f, OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Radius, HitRadius));

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkillDashSweep), false, Character);
	World->SweepMultiByObjectType(Hits, LastSweepLocation, Current, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);
	LastSweepLocation = Current;

	const float BaseDamage = OwningComponent->GetModifiedSkillValue(GetSlot(), ESkillStat::Damage, DashDamage);

	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Target == Character || HitActorsThisDash.Contains(Target))
		{
			continue;
		}

		// Only enemies: skip anything the player controls and anything without health.
		if (const APawn* TargetPawn = Cast<APawn>(Target); TargetPawn && TargetPawn->IsPlayerControlled())
		{
			continue;
		}
		if (!UHealthComponent::FindHealthComponent(Target))
		{
			continue;
		}

		HitActorsThisDash.Add(Target);

		float FinalDamage = 0.f;
		bool bCrit = false;
		const EDamageResponse Response = Knockback > 0.f ? EDamageResponse::Knockback : EDamageResponse::DefaultReaction;
		if (!ApplySkillDamage(Target, BaseDamage, /*AttackDamageScaling*/ 1.f, /*bCanCrit*/ true, Response, FinalDamage, bCrit))
		{
			continue;
		}

		if (Knockback > 0.f)
		{
			if (ACharacter* TargetCharacter = Cast<ACharacter>(Target))
			{
				const FVector Launch = CurrentDashDirection * Knockback + FVector(0.f, 0.f, Knockback * 0.25f);
				TargetCharacter->LaunchCharacter(Launch, true, true);
			}
		}

		OnDashHit(Target, FinalDamage, bCrit);
	}
}

void USkill_Dash::FinishDash()
{
	RootMotionID = 0; // the source already ended on its own
	EndSkill();
}

void USkill_Dash::RestoreCollision()
{
	if (!bChangedCollision)
	{
		return;
	}
	if (ACharacter* Character = GetOwningCharacter())
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
		}
	}
	bChangedCollision = false;
}

void USkill_Dash::NativeOnSkillEnded()
{
	UWorld* World = GetWorld();

	// Last sweep for the final stretch of the dash, then stop sweeping.
	if (bDealsDamage && World && World->GetTimerManager().IsTimerActive(HitTimer))
	{
		SweepForHits();
	}
	if (World)
	{
		World->GetTimerManager().ClearTimer(DashTimer);
		World->GetTimerManager().ClearTimer(HitTimer);
	}
	HitActorsThisDash.Reset();

	// Cut short (e.g. class changed mid-dash): stop the movement too.
	if (RootMotionID != 0)
	{
		if (ACharacter* Character = GetOwningCharacter())
		{
			Character->GetCharacterMovement()->RemoveRootMotionSourceByID(RootMotionID);
		}
		RootMotionID = 0;
	}

	RestoreCollision();

	if (OwningComponent)
	{
		OwningComponent->RemoveStateTag(TAG_State_Dashing);
		OwningComponent->RemoveStateTag(TAG_State_Invincible);
	}
}
