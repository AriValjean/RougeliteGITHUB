// SpawnPoint.h
// Marks a spot in a room level where the wave director may spawn enemies.
// Place several around each arena. Only points whose type matches an enemy card's Spawn Point Type are used for it.
// A squad spawned here is scattered inside Scatter Radius, snapped onto the navmesh (except Air points).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Waves/WaveTypes.h"
#include "SpawnPoint.generated.h"

class UBillboardComponent;
class UArrowComponent;

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API ASpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ASpawnPoint();

	/** Which enemies may use this point (matches Spawn Point Type on the enemy card). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Point")
	ESpawnPointType SpawnPointType = ESpawnPointType::Ground;

	/** Squad members are scattered randomly inside this radius (cm) so they don't stack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Point", meta = (ClampMin = "0.0"))
	float ScatterRadius = 300.f;

	/** Untick to switch this point off without deleting it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Point")
	bool bEnabled = true;

	/** Picks a spot inside Scatter Radius. Ground / Ledge spots are snapped onto the navmesh.
	 *  Returns false if no navmesh spot was found (the point is probably off the navmesh). */
	bool FindSpawnLocation(FRandomStream& Stream, FVector& OutLocation) const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Spawn Point")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	/** Editor-only icon and facing arrow so the point is easy to see while placing. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;

	UPROPERTY()
	TObjectPtr<UArrowComponent> Arrow;
#endif
};
