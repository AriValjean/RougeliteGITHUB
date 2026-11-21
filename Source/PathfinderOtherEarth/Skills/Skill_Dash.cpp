// Skill_Dash.cpp

#include "Skills/Skill_Dash.h"
#include "Skills/SkillComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
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

	// Flat camera/crosshair forward (yaw only).
	const FVector AimForward = FRotator(0.f, Character->GetControlRotation().Yaw, 0.f).Vector();

	switch (Direction)
	{
	case EDashDirection::AwayFromAim:
		return -AimForward;
	case EDashDirection::TowardAim:
		return AimForward;
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

	const FVector DashDir = GetDashDirection();

	// Facing: a step back keeps looking at the crosshair, other dashes face where they go.
	if (bRotateToDashDirection)
	{
		const FVector FaceDir = (Direction == EDashDirection::AwayFromAim) ? -DashDir : DashDir;
		Character->SetActorRotation(FRotator(0.f, FaceDir.Rotation().Yaw, 0.f));
	}

	// State tags (other systems ask the skill component, e.g. damage checks State.Invincible).
	OwningComponent->AddStateTag(TAG_State_Dashing);
	if (bInvincibleDuringDash)
	{
		OwningComponent->AddStateTag(TAG_State_Invincible);
	}

	// Root Motion Source: fixed distance over fixed time, same on ground and in air.
	TSharedPtr<FRootMotionSource_ConstantForce> Force = MakeShared<FRootMotionSource_ConstantForce>();
	Force->InstanceName = TEXT("SkillDash");
	Force->AccumulateMode = ERootMotionAccumulateMode::Override;
	Force->Priority = 5;
	Force->Force = DashDir * (FinalDistance / FinalDuration);
	Force->Duration = FinalDuration;
	// Keep some momentum after the dash, but never faster than normal run speed.
	Force->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Force->FinishVelocityParams.ClampVelocity = Movement->MaxWalkSpeed;
	RootMotionID = Movement->ApplyRootMotionSource(Force);

	PlaySkillMontage();
	OnDashStarted(DashDir);

	World->GetTimerManager().SetTimer(DashTimer, this, &USkill_Dash::FinishDash, FinalDuration, false);
}

void USkill_Dash::FinishDash()
{
	RootMotionID = 0; // the source already ended on its own
	EndSkill();
}

void USkill_Dash::NativeOnSkillEnded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashTimer);
	}

	// Cut short (e.g. class changed mid-dash): stop the movement too.
	if (RootMotionID != 0)
	{
		if (ACharacter* Character = GetOwningCharacter())
		{
			Character->GetCharacterMovement()->RemoveRootMotionSourceByID(RootMotionID);
		}
		RootMotionID = 0;
	}

	if (OwningComponent)
	{
		OwningComponent->RemoveStateTag(TAG_State_Dashing);
		OwningComponent->RemoveStateTag(TAG_State_Invincible);
	}
}
