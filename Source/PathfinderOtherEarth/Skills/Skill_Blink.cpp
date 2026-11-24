// Skill_Blink.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/Skill_Blink.cpp

#include "Skills/Skill_Blink.h"
#include "Skills/SkillComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

USkill_Blink::USkill_Blink()
{
	// Tap = quick blink forward, hold = circle. Rooted while aiming, turns with the mouse.
	IndicatorShape = EAimIndicatorShape::Circle;
	TapThreshold = 0.2f;
	bLockMovementWhileAiming = true;
	bFaceAimWhileAiming = true;
}

bool USkill_Blink::CanActivate_Implementation() const
{
	if (!Super::CanActivate_Implementation())
	{
		return false;
	}
	const ACharacter* Character = GetOwningCharacter();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	return bAllowInAir || !Movement || !Movement->IsFalling();
}

FVector USkill_Blink::ComputeAimLocation() const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	// The circle sits on the safe spot, so what you see is exactly where you land.
	const float MaxRange = GetModifiedValue(ESkillStat::Distance, Definition.Range);
	FVector Safe;
	if (FindBlinkDestination(GetGroundTargetLocation(MaxRange), Safe))
	{
		return Safe;
	}

	// No room: circle stays under the player.
	return Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
}

bool USkill_Blink::FindBlinkDestination(FVector DesiredGround, FVector& OutGround) const
{
	const ACharacter* Character = GetOwningCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
	const float WalkableFloorZ = Movement ? Movement->GetWalkableFloorZ() : 0.7f;

	// Path check at a raised height so small bumps and stairs don't block the blink.
	const FVector Lift(0.f, 0.f, HalfHeight * 0.5f);
	const FVector Start = Character->GetActorLocation() + Lift;
	const FVector End = DesiredGround + FVector(0.f, 0.f, HalfHeight) + Lift;

	const FVector Path = End - Start;
	const float PathLength = Path.Size();
	if (PathLength < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FVector Direction = Path / PathLength;

	// 1) Walls: sweep a sphere against what the player's capsule collides with (level geometry,
	// static or movable). Enemies, triggers and the player's own weapons don't block a teleport.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BlinkPath), false);
	FCollisionResponseParams Responses;
	const ECollisionChannel Channel = GetWorldTraceSettings(Params, Responses);
	FHitResult WallHit;
	float Reach = PathLength;
	if (World->SweepSingleByChannel(WallHit, Start, End, FQuat::Identity, Channel,
		FCollisionShape::MakeSphere(CapsuleRadius * 0.6f), Params, Responses))
	{
		Reach = FMath::Max(0.f, WallHit.Distance - CapsuleRadius);
		UE_LOG(LogTemp, Verbose, TEXT("Blink: path blocked by %s (%s) after %.0f cm"),
			*GetNameSafe(WallHit.GetActor()), *GetNameSafe(WallHit.GetComponent()), WallHit.Distance);
	}

	// 2) Floor: from the furthest point back toward the player, take the first spot with walkable
	// floor below it (stops at ledges instead of dropping into a pit).
	constexpr int32 Steps = 8;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float Distance = Reach * (1.f - static_cast<float>(i) / Steps);
		if (Distance < CapsuleRadius)
		{
			break; // basically no movement left: no room to blink
		}

		const FVector Probe = Start + Direction * Distance;
		const FVector Down = Probe - FVector(0.f, 0.f, HalfHeight * 2.f + Lift.Z + MaxDropHeight);
		FHitResult FloorHit;
		if (World->LineTraceSingleByChannel(FloorHit, Probe, Down, Channel, Params, Responses)
			&& FloorHit.ImpactNormal.Z >= WalkableFloorZ)
		{
			OutGround = FloorHit.ImpactPoint;
			return true;
		}
	}

	if (bLogFailures)
	{
		UE_LOG(LogTemp, Warning, TEXT("Blink: no room. Path %.0f cm, blocked after %.0f cm by %s (%s). Desired ground %s"),
			PathLength, Reach, *GetNameSafe(WallHit.GetActor()), *GetNameSafe(WallHit.GetComponent()), *DesiredGround.ToString());
	}
	return false;
}

void USkill_Blink::ActivateSkill_Implementation()
{
	ACharacter* Character = GetOwningCharacter();
	if (!Character || !OwningComponent)
	{
		EndSkill();
		return;
	}

	const float HalfHeight = Character->GetSimpleCollisionHalfHeight();
	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

	// Held = the circle's spot (already made safe). Tap = straight ahead by the tap distance.
	FVector Ground;
	const bool bWasAimed = ConsumeAimedLocation(Ground);
	if (!bWasAimed)
	{
		const FVector Forward = Character->GetActorForwardVector().GetSafeNormal2D();
		const float TapDistance = GetModifiedValue(ESkillStat::Distance, Definition.Magnitude);
		if (!FindBlinkDestination(Feet + Forward * TapDistance, Ground))
		{
			Fizzle();
			return;
		}
	}

	const float CapsuleRadius = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
	if (FVector::Dist2D(Ground, Feet) < CapsuleRadius)
	{
		Fizzle(); // aimed circle had no room and sat under the player
		return;
	}

	const FVector From = Character->GetActorLocation();
	const FVector Destination = Ground + FVector(0.f, 0.f, HalfHeight + 2.f);

	OnBlinkDeparted(From);

	// Teleport To nudges the capsule out of anything it would overlap, and fails if it can't.
	if (!Character->TeleportTo(Destination, Character->GetActorRotation()))
	{
		Fizzle();
		return;
	}

	// Short invincibility on arrival.
	const float Invincibility = GetModifiedValue(ESkillStat::Duration, Definition.EffectDuration);
	if (Invincibility > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			OwningComponent->AddStateTag(TAG_State_Invincible);
			bGrantedInvincible = true;
			World->GetTimerManager().SetTimer(InvincibleTimer, this, &USkill_Blink::EndInvincibility, Invincibility, false);
		}
	}

	PlaySkillMontage();
	OnBlinkArrived(Character->GetActorLocation(), bWasAimed);

	// The teleport is instant; the invincibility timer finishes on its own.
	EndSkill();
}

void USkill_Blink::Fizzle()
{
	if (OwningComponent)
	{
		OwningComponent->ResetCooldown(GetSlot());
	}
	OnBlinkFailed();
	EndSkill();
}

void USkill_Blink::EndInvincibility()
{
	if (bGrantedInvincible && OwningComponent)
	{
		OwningComponent->RemoveStateTag(TAG_State_Invincible);
	}
	bGrantedInvincible = false;
}

void USkill_Blink::NativeOnRemoved()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InvincibleTimer);
	}
	EndInvincibility();
}
