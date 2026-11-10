// RoomDirector.h
// One per room level. Tracks the room's enemies and decides when the room is cleared.
// When the last enemy dies it fires On Room Cleared and tells the Room Flow Subsystem,
// which rolls the portal options.
//
// Enemies already placed in the level are found automatically (by Enemy Class) one tick after BeginPlay.
// Enemies spawned later (waves) call Register Enemy.
// A room with no enemies (Start, Shop, NPC) clears right away.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomDirector.generated.h"

class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomClearedSignature);

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API ARoomDirector : public AActor
{
	GENERATED_BODY()

public:
	ARoomDirector();

	/** Adds an enemy to this room. Its death is tracked through its Health Component's On Death.
	 *  Ignored if it has no Health Component, is already dead, or the room is already cleared. */
	UFUNCTION(BlueprintCallable, Category = "Room")
	void RegisterEnemy(AActor* Enemy);

	/** Living enemies still registered to this room. */
	UFUNCTION(BlueprintPure, Category = "Room")
	int32 GetAliveEnemyCount() const;

	UFUNCTION(BlueprintPure, Category = "Room")
	bool IsRoomCleared() const { return bRoomCleared; }

	/** Fired once, when the last enemy dies (after Clear Delay). Portals and rewards listen to this. */
	UPROPERTY(BlueprintAssignable, Category = "Room|Events")
	FOnRoomClearedSignature OnRoomCleared;

protected:
	virtual void BeginPlay() override;

	/** Enemies of this class already in the level are registered automatically. Set to BP_EnemyBase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	TSubclassOf<AActor> EnemyClass;

	/** Seconds between the last death and the room counting as cleared (lets the death play out). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room", meta = (ClampMin = "0.0"))
	float ClearDelay = 1.0f;

private:
	UFUNCTION()
	void HandleEnemyDeath(AActor* Killer);

	void GatherPlacedEnemies();
	void CheckCleared();
	void ClearRoom();

	/** Weak so a destroyed enemy simply counts as gone. */
	TArray<TWeakObjectPtr<UHealthComponent>> Enemies;

	bool bGathered = false;
	bool bRoomCleared = false;
	bool bClearPending = false;

	FTimerHandle ClearTimer;
};
