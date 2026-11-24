// SkillProjectile.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/SkillProjectile.cpp

#include "Skills/SkillProjectile.h"
#include "Combat/HealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"

ASkillProjectile::ASkillProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Collision: blocked by walls/props, overlaps characters (so we can pick enemies and ignore the thrower).
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->ProjectileGravityScale = 0.f;   // flies straight
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bAutoActivate = false;          // starts in Launch
}

void ASkillProjectile::BeginPlay()
{
	Super::BeginPlay();

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ASkillProjectile::HandleOverlap);
	Movement->OnProjectileStop.AddDynamic(this, &ASkillProjectile::HandleStop);

	// Never collide with whoever threw it, or anything attached to them (their weapon actors sit
	// right where the dagger spawns, in the hand, and would stop it instantly).
	if (AActor* Thrower = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(Thrower, true);

		TArray<AActor*> Attached;
		Thrower->GetAttachedActors(Attached, true, true);
		for (AActor* Actor : Attached)
		{
			Collision->IgnoreActorWhenMoving(Actor, true);
		}
	}
}

void ASkillProjectile::Launch(const FVector& Direction, float Speed, float MaxDistance)
{
	const FVector Dir = Direction.GetSafeNormal();
	const float SafeSpeed = FMath::Max(1.f, Speed);

	SetActorRotation(Dir.Rotation());
	Movement->Velocity = Dir * SafeSpeed;
	Movement->Activate(true);

	// Despawn once it has travelled its range.
	SetLifeSpan(FMath::Max(0.05f, MaxDistance / SafeSpeed));
}

void ASkillProjectile::SetHomingTarget(AActor* Target, float HomingAcceleration)
{
	USceneComponent* TargetComponent = Target ? Target->GetRootComponent() : nullptr;
	Movement->bIsHomingProjectile = TargetComponent != nullptr;
	Movement->HomingTargetComponent = TargetComponent;
	Movement->HomingAccelerationMagnitude = FMath::Max(0.f, HomingAcceleration);
}

void ASkillProjectile::SetHitRadius(float Radius)
{
	Collision->SetSphereRadius(FMath::Max(1.f, Radius));
}

bool ASkillProjectile::IsValidTarget(const AActor* Actor) const
{
	if (!Actor || Actor == GetOwner() || Actor == GetInstigator())
	{
		return false;
	}
	if (const APawn* Pawn = Cast<APawn>(Actor); Pawn && Pawn->IsPlayerControlled())
	{
		return false;
	}
	return UHealthComponent::FindHealthComponent(Actor) != nullptr;
}

void ASkillProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHasHit || !IsValidTarget(OtherActor))
	{
		return;
	}

	if (bPierceTargets)
	{
		// One hit per enemy (an enemy can overlap with several components).
		if (PiercedActors.Contains(OtherActor))
		{
			return;
		}
		PiercedActors.Add(OtherActor);
		OnProjectileHitTarget.Broadcast(OtherActor);
		OnImpact(GetActorLocation(), OtherActor);
		return;
	}

	bHasHit = true;

	OnProjectileHitTarget.Broadcast(OtherActor);
	OnImpact(GetActorLocation(), OtherActor);
	Destroy();
}

void ASkillProjectile::HandleStop(const FHitResult& ImpactResult)
{
	// Hit a wall / prop.
	if (bHasHit)
	{
		return;
	}
	bHasHit = true;

	OnImpact(ImpactResult.ImpactPoint, nullptr);
	Destroy();
}
