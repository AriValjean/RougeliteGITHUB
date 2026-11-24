// SkillZone.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/SkillZone.cpp

#include "Skills/SkillZone.h"
#include "Combat/HealthComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ASkillZone::ASkillZone()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ASkillZone::StartZone(float InRadius, float InDuration, float InTickInterval)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Radius = FMath::Max(1.f, InRadius);
	TickInterval = FMath::Max(0.1f, InTickInterval);
	const float Duration = FMath::Max(0.1f, InDuration);

	OnZoneStarted(Radius, Duration);

	DoTick();
	World->GetTimerManager().SetTimer(TickTimer, this, &ASkillZone::DoTick, TickInterval, true);
	World->GetTimerManager().SetTimer(EndTimer, this, &ASkillZone::FinishZone, Duration, false);
}

void ASkillZone::DoTick()
{
	UWorld* World = GetWorld();
	if (!World || bEnded)
	{
		return;
	}

	// Tall capsule standing on the zone's ground point, so enemies of any height inside the circle count.
	const float HalfHeight = Radius + 200.f;
	const FVector Center = GetActorLocation() + FVector(0.f, 0.f, 100.f);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkillZoneTick), false, GetOwner());
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);

	TSet<AActor*> Seen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == GetOwner() || Seen.Contains(Target))
		{
			continue;
		}
		Seen.Add(Target);

		if (const APawn* TargetPawn = Cast<APawn>(Target); TargetPawn && TargetPawn->IsPlayerControlled())
		{
			continue;
		}
		const UHealthComponent* Health = UHealthComponent::FindHealthComponent(Target);
		if (!Health || Health->IsDead())
		{
			continue;
		}

		OnZoneTickTarget.Broadcast(Target);
	}

	OnZoneTick();
}

void ASkillZone::FinishZone()
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimer);
		World->GetTimerManager().ClearTimer(EndTimer);
	}

	OnZoneEnded();
	SetLifeSpan(1.5f); // let FX fade out
}

void ASkillZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimer);
		World->GetTimerManager().ClearTimer(EndTimer);
	}
	Super::EndPlay(EndPlayReason);
}
