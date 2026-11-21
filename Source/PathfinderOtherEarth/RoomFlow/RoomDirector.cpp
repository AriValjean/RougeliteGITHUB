// RoomDirector.cpp

#include "RoomFlow/RoomDirector.h"
#include "RoomFlow/RoomFlowSubsystem.h"
#include "Waves/WaveDirectorComponent.h"
#include "Combat/HealthComponent.h"
#include "Meta/PathfinderMetaSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoomDirector, Log, All);

ARoomDirector::ARoomDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	WaveDirector = CreateDefaultSubobject<UWaveDirectorComponent>(TEXT("WaveDirector"));
}

void ARoomDirector::BeginPlay()
{
	Super::BeginPlay();

	// Tell the run we've arrived (starts a debug run when playing straight from this level, prints the room readout).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (URoomFlowSubsystem* RoomFlow = GI->GetSubsystem<URoomFlowSubsystem>())
		{
			RoomFlow->NotifyRoomEntered();
		}
	}

	// Wait one tick so every placed enemy has finished its own BeginPlay (and set up its Health Component).
	GetWorldTimerManager().SetTimerForNextTick(this, &ARoomDirector::GatherPlacedEnemies);
}

void ARoomDirector::GatherPlacedEnemies()
{
	if (EnemyClass)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(this, EnemyClass, Found);
		for (AActor* Enemy : Found)
		{
			RegisterEnemy(Enemy);
		}
	}
	else
	{
		UE_LOG(LogRoomDirector, Warning, TEXT("%s: Enemy Class is not set, so placed enemies are not tracked."), *GetName());
	}

	UE_LOG(LogRoomDirector, Log, TEXT("%s: tracking %d placed enemies."), *GetName(), GetAliveEnemyCount());

	// Generated Waves (Combat rooms only). While waves are running the room can't clear.
	if (WaveDirector)
	{
		WaveDirector->TryStartEncounter();
	}

	bGathered = true;

	// No enemies and no waves (Start, Shop, NPC room) -> cleared right away.
	CheckCleared();
}

void ARoomDirector::RegisterEnemy(AActor* Enemy)
{
	if (bRoomCleared || !IsValid(Enemy))
	{
		return;
	}

	UHealthComponent* Health = UHealthComponent::FindHealthComponent(Enemy);
	if (!Health || Health->IsDead())
	{
		UE_LOG(LogRoomDirector, Warning, TEXT("%s: %s has no Health Component or is already dead; not tracked."), *GetName(), *Enemy->GetName());
		return;
	}

	// A new enemy arriving cancels a pending clear (e.g. a wave spawning during the delay).
	if (bClearPending)
	{
		GetWorldTimerManager().ClearTimer(ClearTimer);
		bClearPending = false;
	}

	Enemies.AddUnique(Health);
	bHadEnemies = true;
	Health->OnDeath.AddUniqueDynamic(this, &ARoomDirector::HandleEnemyDeath);
}

void ARoomDirector::HandleEnemyDeath(AActor* Killer)
{
	// Let the waves react first (may start the next wave), then see if the room is done.
	if (WaveDirector)
	{
		WaveDirector->NotifyEnemyDied();
	}

	// On Death only tells us the killer, not who died, so just recount who is still alive.
	CheckCleared();
}

void ARoomDirector::NotifyWavesFinished()
{
	CheckCleared();
}

int32 ARoomDirector::GetAliveEnemyCount() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<UHealthComponent>& Health : Enemies)
	{
		if (Health.IsValid() && !Health->IsDead())
		{
			++Alive;
		}
	}
	return Alive;
}

void ARoomDirector::CheckCleared()
{
	if (!bGathered || bRoomCleared || bClearPending)
	{
		return;
	}

	// Drop enemies that died or were destroyed.
	Enemies.RemoveAll([](const TWeakObjectPtr<UHealthComponent>& Health)
	{
		return !Health.IsValid() || Health->IsDead();
	});

	// Waves still to come: not cleared yet, even if nobody is alive right now (gap between waves).
	if (WaveDirector && WaveDirector->IsEncounterActive())
	{
		return;
	}

	if (Enemies.Num() > 0)
	{
		return;
	}

	if (ClearDelay > 0.f)
	{
		bClearPending = true;
		GetWorldTimerManager().SetTimer(ClearTimer, this, &ARoomDirector::ClearRoom, ClearDelay, false);
	}
	else
	{
		ClearRoom();
	}
}

void ARoomDirector::ClearRoom()
{
	bClearPending = false;
	if (bRoomCleared)
	{
		return;
	}
	bRoomCleared = true;

	UE_LOG(LogRoomDirector, Log, TEXT("%s: room cleared."), *GetName());
	OnRoomCleared.Broadcast();

	GrantClearRewards();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (URoomFlowSubsystem* RoomFlow = GI->GetSubsystem<URoomFlowSubsystem>())
		{
			RoomFlow->NotifyRoomCleared();
		}
	}
}

void ARoomDirector::GrantClearRewards()
{
	// Start, Shop and NPC rooms never had enemies, so they give nothing.
	if (!bHadEnemies)
	{
		return;
	}

	const bool bGiveMemoria = bGuaranteeMemoria || FMath::FRand() < MemoriaChance;
	if (!bGiveMemoria)
	{
		UE_LOG(LogRoomDirector, Log, TEXT("%s: no Memoria this clear (%.0f%% chance)."), *GetName(), MemoriaChance * 100.f);
		return;
	}

	const int32 Amount = FMath::RandRange(FMath::Min(MemoriaMin, MemoriaMax), FMath::Max(MemoriaMin, MemoriaMax));

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPathfinderMetaSubsystem* Meta = GI->GetSubsystem<UPathfinderMetaSubsystem>())
		{
			Meta->AddCurrency(MemoriaCurrencyId, Amount);
		}
	}
}
