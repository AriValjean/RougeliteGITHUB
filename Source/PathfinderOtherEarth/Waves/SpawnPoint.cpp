// SpawnPoint.cpp

#include "Waves/SpawnPoint.h"
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpawnPoint, Log, All);

ASpawnPoint::ASpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Root);
	}

	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(Root);
		Arrow->ArrowColor = FColor::Red;
	}
#endif
}

bool ASpawnPoint::FindSpawnLocation(FRandomStream& Stream, FVector& OutLocation) const
{
	const FVector Origin = GetActorLocation();

	// Random point inside a circle (sqrt keeps it evenly spread instead of bunched in the middle).
	auto RandomOffset = [this, &Stream]()
	{
		const float Angle = Stream.FRandRange(0.f, 2.f * PI);
		const float Dist = ScatterRadius * FMath::Sqrt(Stream.FRand());
		return FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
	};

	// Air points don't need the navmesh.
	if (SpawnPointType == ESpawnPointType::Air)
	{
		OutLocation = Origin + RandomOffset();
		return true;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogSpawnPoint, Warning, TEXT("%s: no navigation system in this level, spawning without navmesh snapping."), *GetName());
		OutLocation = Origin;
		return true;
	}

	// Try a few random spots; snap each onto the navmesh (searches 500cm up/down for uneven ground).
	const FVector QueryExtent(100.f, 100.f, 500.f);
	for (int32 Try = 0; Try < 5; ++Try)
	{
		FNavLocation NavLocation;
		if (NavSys->ProjectPointToNavigation(Origin + RandomOffset(), NavLocation, QueryExtent))
		{
			OutLocation = NavLocation.Location;
			return true;
		}
	}

	UE_LOG(LogSpawnPoint, Warning, TEXT("%s: couldn't find navmesh near this point. Move it onto the navmesh (press P in the viewport to see it)."), *GetName());
	return false;
}
