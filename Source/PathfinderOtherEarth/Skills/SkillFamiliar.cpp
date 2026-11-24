// SkillFamiliar.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/SkillFamiliar.cpp

#include "Skills/SkillFamiliar.h"
#include "Skills/SkillProjectile.h"
#include "Combat/HealthComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"

ASkillFamiliar::ASkillFamiliar()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ASkillFamiliar::StartFamiliar(float Duration, float FireInterval, float TargetRange, float BoltSpeed)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Range = FMath::Max(100.f, TargetRange);
	Speed = FMath::Max(100.f, BoltSpeed);
	SetActorLocation(GetFollowLocation());

	World->GetTimerManager().SetTimer(FireTimer, this, &ASkillFamiliar::Fire, FMath::Max(0.05f, FireInterval), true);
	World->GetTimerManager().SetTimer(DurationTimer, this, &ASkillFamiliar::Dismiss, FMath::Max(0.1f, Duration), false);

	OnSummoned();
}

void ASkillFamiliar::Dismiss()
{
	if (bDismissed)
	{
		return;
	}
	bDismissed = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
		World->GetTimerManager().ClearTimer(DurationTimer);
	}
	CurrentTarget.Reset();

	OnDismissed();
	OnFamiliarExpired.Broadcast();

	SetLifeSpan(FMath::Max(0.01f, LingerTime));
}

FVector ASkillFamiliar::GetFollowLocation() const
{
	const AActor* Player = GetOwner();
	if (!Player)
	{
		return GetActorLocation();
	}
	const FRotator Facing(0.f, Player->GetActorRotation().Yaw, 0.f);
	const FVector Bob(0.f, 0.f, FMath::Sin(BobTime * 2.f) * BobHeight);
	return Player->GetActorLocation() + Facing.RotateVector(FollowOffset) + Bob;
}

void ASkillFamiliar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GetOwner())
	{
		Dismiss();
		return;
	}

	// Smooth follow (slight lag feels alive).
	BobTime += DeltaSeconds;
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), GetFollowLocation(), DeltaSeconds, FollowSpeed));

	// Face the target, or the same way as the player.
	const AActor* Target = CurrentTarget.Get();
	const FVector LookAt = Target ? Target->GetActorLocation() - GetActorLocation() : GetOwner()->GetActorForwardVector();
	const FRotator WantedRotation(0.f, LookAt.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), WantedRotation, DeltaSeconds, 10.f));
}

bool ASkillFamiliar::IsValidTarget(const AActor* Actor) const
{
	const AActor* Player = GetOwner();
	if (!Actor || !Player || Actor == Player)
	{
		return false;
	}
	if (const APawn* Pawn = Cast<APawn>(Actor); Pawn && Pawn->IsPlayerControlled())
	{
		return false;
	}
	const UHealthComponent* Health = UHealthComponent::FindHealthComponent(Actor);
	if (!Health || Health->IsDead())
	{
		return false;
	}
	return FVector::DistSquared(Actor->GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(Range);
}

bool ASkillFamiliar::HasLineOfSight(const AActor* Actor) const
{
	const UWorld* World = GetWorld();
	if (!World || !Actor)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FamiliarSight), false, this);
	if (AActor* Player = GetOwner())
	{
		Params.AddIgnoredActor(Player);
		TArray<AActor*> Attached;
		Player->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
	}
	Params.AddIgnoredActor(Actor);

	// Walls block; characters (other enemies) don't.
	FCollisionResponseParams Responses;
	Responses.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	Responses.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);

	FHitResult Hit;
	return !World->LineTraceSingleByChannel(Hit, GetActorLocation(), Actor->GetActorLocation(), ECC_Visibility, Params, Responses);
}

AActor* ASkillFamiliar::FindTarget() const
{
	const UWorld* World = GetWorld();
	const AActor* Player = GetOwner();
	if (!World || !Player)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FamiliarTargets), false, Player);
	World->OverlapMultiByObjectType(Overlaps, Player->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Range), Params);

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsValidTarget(Actor))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), Player->GetActorLocation());
		if (DistSq < BestDistSq && HasLineOfSight(Actor))
		{
			Best = Actor;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

void ASkillFamiliar::Fire()
{
	UWorld* World = GetWorld();
	AActor* Player = GetOwner();
	if (bDismissed || !World || !Player)
	{
		return;
	}

	// Keep the current target while it's alive, in range and visible; otherwise pick the nearest.
	AActor* Target = CurrentTarget.Get();
	if (!IsValidTarget(Target) || !HasLineOfSight(Target))
	{
		Target = FindTarget();
		CurrentTarget = Target;
	}
	if (!Target)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Player;                       // bolts ignore the player and their weapons
	Params.Instigator = Cast<APawn>(Player);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* ClassToSpawn = BoltClass ? BoltClass.Get() : ASkillProjectile::StaticClass();

	const FVector Start = GetActorLocation();
	const FVector Direction = (Target->GetActorLocation() - Start).GetSafeNormal();
	if (ASkillProjectile* Bolt = World->SpawnActor<ASkillProjectile>(ClassToSpawn, Start, Direction.Rotation(), Params))
	{
		Bolt->OnProjectileHitTarget.AddDynamic(this, &ASkillFamiliar::HandleBoltHit);
		Bolt->SetHomingTarget(Target, HomingAcceleration);
		Bolt->Launch(Direction, Speed, Range * 2.f);  // extra distance so curving bolts still arrive
		OnBoltFired(Target);
	}
}

void ASkillFamiliar::HandleBoltHit(AActor* HitActor)
{
	OnBoltHitTarget.Broadcast(HitActor);
}
