// StatusEffectComponent.cpp
// Drop into: Source/PathfinderOtherEarth/Combat/StatusEffectComponent.cpp

#include "Combat/StatusEffectComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UStatusEffectComponent::UStatusEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UStatusEffectComponent* UStatusEffectComponent::FindStatusEffects(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UStatusEffectComponent>() : nullptr;
}

void UStatusEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	// Whatever the character starts with is its normal speed.
	if (const UCharacterMovementComponent* Movement = GetMovement())
	{
		BaseMoveSpeed = Movement->MaxWalkSpeed;
	}
}

void UStatusEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpireTimer);
	}
	Super::EndPlay(EndPlayReason);
}

UCharacterMovementComponent* UStatusEffectComponent::GetMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character ? Character->GetCharacterMovement() : nullptr;
}

double UStatusEffectComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

// ----------------------------------------------------------------------------
// Slow
// ----------------------------------------------------------------------------

void UStatusEffectComponent::ApplySlow(FName Source, float Strength, float Duration)
{
	if (Source.IsNone() || Strength <= 0.f || Duration <= 0.f)
	{
		return;
	}

	const float SafeStrength = FMath::Clamp(Strength, 0.f, 0.95f);
	const double NewExpire = Now() + Duration;

	// Same source: refresh the timer, keep the stronger value (never stacks past it).
	FActiveSlow& Slow = Slows.FindOrAdd(Source);
	Slow.Strength = FMath::Max(Slow.Strength, SafeStrength);
	Slow.ExpireTime = FMath::Max(Slow.ExpireTime, NewExpire);

	ApplyMoveSpeed();
	OnStatusEffectsChanged.Broadcast();
	ScheduleNextExpiry();
}

void UStatusEffectComponent::RemoveSlow(FName Source)
{
	if (Slows.Remove(Source) > 0)
	{
		ApplyMoveSpeed();
		OnStatusEffectsChanged.Broadcast();
		ScheduleNextExpiry();
	}
}

bool UStatusEffectComponent::IsSlowed() const
{
	return GetMoveSpeedMultiplier() < 1.f;
}

float UStatusEffectComponent::GetMoveSpeedMultiplier() const
{
	// Different sources don't stack: the strongest slow wins.
	float Strongest = 0.f;
	for (const TPair<FName, FActiveSlow>& Pair : Slows)
	{
		Strongest = FMath::Max(Strongest, Pair.Value.Strength);
	}
	return 1.f - Strongest;
}

void UStatusEffectComponent::PurgeExpired()
{
	const double Time = Now();
	bool bChanged = false;
	for (auto It = Slows.CreateIterator(); It; ++It)
	{
		if (It.Value().ExpireTime <= Time)
		{
			It.RemoveCurrent();
			bChanged = true;
		}
	}

	if (bChanged)
	{
		ApplyMoveSpeed();
		OnStatusEffectsChanged.Broadcast();
	}
	ScheduleNextExpiry();
}

void UStatusEffectComponent::ScheduleNextExpiry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	double Earliest = TNumericLimits<double>::Max();
	for (const TPair<FName, FActiveSlow>& Pair : Slows)
	{
		Earliest = FMath::Min(Earliest, Pair.Value.ExpireTime);
	}

	if (Slows.Num() == 0)
	{
		World->GetTimerManager().ClearTimer(ExpireTimer);
		return;
	}

	const float Delay = FMath::Max(0.01f, static_cast<float>(Earliest - Now()));
	World->GetTimerManager().SetTimer(ExpireTimer, this, &UStatusEffectComponent::PurgeExpired, Delay, false);
}

// ----------------------------------------------------------------------------
// Movement speed
// ----------------------------------------------------------------------------

void UStatusEffectComponent::SetBaseMoveSpeed(float NewBaseSpeed)
{
	BaseMoveSpeed = FMath::Max(0.f, NewBaseSpeed);
	ApplyMoveSpeed();
}

void UStatusEffectComponent::ApplyMoveSpeed()
{
	if (UCharacterMovementComponent* Movement = GetMovement())
	{
		Movement->MaxWalkSpeed = BaseMoveSpeed * GetMoveSpeedMultiplier();
	}
}
