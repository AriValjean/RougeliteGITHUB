// WaveDirectorComponent.cpp

#include "Waves/WaveDirectorComponent.h"
#include "Waves/SpawnPoint.h"
#include "RoomFlow/RoomDirector.h"
#include "RoomFlow/RoomFlowSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaveDirector, Log, All);

namespace WaveDirector
{
	/** Weighted random pick. Returns INDEX_NONE if nothing has any weight. */
	template <typename T, typename FWeightFn>
	int32 PickWeighted(const TArray<T>& Items, FRandomStream& Stream, FWeightFn GetWeight)
	{
		float Total = 0.f;
		for (const T& Item : Items)
		{
			Total += FMath::Max(0.f, GetWeight(Item));
		}
		if (Total <= 0.f)
		{
			return INDEX_NONE;
		}

		float Roll = Stream.FRandRange(0.f, Total);
		int32 LastValid = INDEX_NONE;
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const float W = FMath::Max(0.f, GetWeight(Items[i]));
			if (W <= 0.f)
			{
				continue;
			}
			LastValid = i;
			Roll -= W;
			if (Roll <= 0.f)
			{
				return i;
			}
		}
		return LastValid;
	}
}

UWaveDirectorComponent::UWaveDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

URoomFlowSubsystem* UWaveDirectorComponent::GetRoomFlow() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<URoomFlowSubsystem>() : nullptr;
}

FRandomStream& UWaveDirectorComponent::GetStream()
{
	if (URoomFlowSubsystem* RoomFlow = GetRoomFlow())
	{
		return RoomFlow->GetRandomStream();
	}
	return FallbackStream;
}

// ======================================================================
// Start
// ======================================================================

void UWaveDirectorComponent::TryStartEncounter()
{
	URoomFlowSubsystem* RoomFlow = GetRoomFlow();
	if (!RoomFlow)
	{
		UE_LOG(LogWaveDirector, Warning, TEXT("%s: no Room Flow Subsystem, waves disabled."), *GetNameSafe(GetOwner()));
		return;
	}

	FRoomRow Room;
	if (!RoomFlow->GetRoomData(RoomFlow->CurrentRoomId, Room))
	{
		UE_LOG(LogWaveDirector, Log, TEXT("%s: room '%s' isn't in DT_RoomsCPP, using placed enemies only."), *GetNameSafe(GetOwner()), *RoomFlow->CurrentRoomId.ToString());
		return;
	}

	// One mode per room: only Combat rooms set to Generated Waves run the director.
	if (Room.RoomType != ERoomType::Combat || Room.EncounterMode != EEncounterMode::Generated)
	{
		return;
	}

	if (!EnemyCardTable)
	{
		UE_LOG(LogWaveDirector, Error, TEXT("%s: Enemy Card Table isn't set on the Wave Director (BP_RoomDirector). No waves."), *GetNameSafe(GetOwner()));
		return;
	}

	FallbackStream.GenerateNewSeed();

	// ---- Budget ----
	WaveCount = FMath::Max(1, Room.WaveCount);
	const int32 OverallStage = FMath::Max(1, RoomFlow->GetOverallStageNumber());
	const int32 RoomInStage = FMath::Max(0, RoomFlow->RoomsCompletedThisStage);
	WaveBudget = BaseCreditsPerWave
		* (1.f + StageScaling * (OverallStage - 1))
		* (1.f + RoomScaling * RoomInStage)
		* Room.BudgetMultiplier;

	// ---- Enemy pools for this stage ----
	const FName StageId = RoomFlow->GetCurrentStageId();
	BuildPools(StageId, RoomInStage);
	if (SingleCards.Num() == 0 && Squads.Num() == 0)
	{
		UE_LOG(LogWaveDirector, Warning, TEXT("%s: no enemy cards or squads list stage '%s'. No waves."), *GetNameSafe(GetOwner()), *StageId.ToString());
		return;
	}

	// ---- Spawn points ----
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, ASpawnPoint::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		if (ASpawnPoint* Point = Cast<ASpawnPoint>(Actor))
		{
			if (Point->bEnabled)
			{
				SpawnPoints.Add(Point);
			}
		}
	}
	if (SpawnPoints.Num() == 0)
	{
		UE_LOG(LogWaveDirector, Error, TEXT("%s: no enabled Spawn Points in this level. Place some BP_SpawnPoint actors. No waves."), *GetNameSafe(GetOwner()));
		return;
	}

	if (const ARoomDirector* Director = Cast<ARoomDirector>(GetOwner()))
	{
		if (Director->GetAliveEnemyCount() > 0)
		{
			UE_LOG(LogWaveDirector, Warning, TEXT("%s: this room is set to Generated Waves but also has placed enemies. They still have to die for the room to clear; set the room to Placed if that's intended."), *GetNameSafe(GetOwner()));
		}
	}

	// From here on the room can't clear until the waves are done.
	bEncounterActive = true;

	// ---- Load the enemy classes (async, so a big pool doesn't hitch) ----
	TArray<FSoftObjectPath> ToLoad;
	for (const TPair<FName, FEnemySpawnCard>& Pair : Cards)
	{
		if (!Pair.Value.EnemyClass.IsNull())
		{
			ToLoad.AddUnique(Pair.Value.EnemyClass.ToSoftObjectPath());
		}
	}

	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(ToLoad, FStreamableDelegate::CreateUObject(this, &UWaveDirectorComponent::OnClassesLoaded));
	if (!LoadHandle.IsValid())
	{
		// Nothing needed loading (or the request failed); carry on with whatever is already in memory.
		OnClassesLoaded();
	}
}

void UWaveDirectorComponent::BuildPools(FName StageId, int32 RoomInStage)
{
	Cards.Reset();
	SingleCards.Reset();
	Squads.Reset();

	const FString Context(TEXT("WaveDirector"));

	// Single cards valid in this stage / room.
	EnemyCardTable->ForeachRow<FEnemySpawnCard>(Context, [&](const FName& Key, const FEnemySpawnCard& Row)
	{
		if (Row.EnemyClass.IsNull() || Row.Weight <= 0.f || !Row.Stages.Contains(StageId) || Row.MinRoomInStage > RoomInStage)
		{
			return;
		}
		Cards.Add(Key, Row);
		SingleCards.Add(Key);
	});

	if (!SquadTable)
	{
		return;
	}

	// Squads valid in this stage / room. Their member cards are used even if those cards don't list the stage themselves.
	SquadTable->ForeachRow<FSquadCard>(Context, [&](const FName& Key, const FSquadCard& Row)
	{
		if (Row.Weight <= 0.f || !Row.Stages.Contains(StageId) || Row.MinRoomInStage > RoomInStage)
		{
			return;
		}

		FWaveSquadEntry Entry;
		Entry.Id = Key;
		Entry.Weight = Row.Weight;

		for (const FSquadMember& Member : Row.Members)
		{
			const FEnemySpawnCard* Card = Member.Card.GetRow<FEnemySpawnCard>(Context);
			if (!Card || Card->EnemyClass.IsNull())
			{
				UE_LOG(LogWaveDirector, Warning, TEXT("Squad '%s': member card '%s' is missing or has no Enemy Class. Squad skipped."), *Key.ToString(), *Member.Card.RowName.ToString());
				return;
			}
			Cards.Add(Member.Card.RowName, *Card);
			Entry.Members.Add(TPair<FName, int32>(Member.Card.RowName, FMath::Max(1, Member.Count)));
			Entry.Cost += Card->Cost * FMath::Max(1, Member.Count);
		}

		if (Entry.Members.Num() > 0 && Entry.Cost > 0)
		{
			Squads.Add(Entry);
		}
	});
}

void UWaveDirectorComponent::OnClassesLoaded()
{
	if (bClassesLoaded || !bEncounterActive)
	{
		return;
	}
	bClassesLoaded = true;

	// Drop any card whose Blueprint failed to load, and anything that used it.
	for (auto It = Cards.CreateIterator(); It; ++It)
	{
		if (!It->Value.EnemyClass.Get())
		{
			UE_LOG(LogWaveDirector, Warning, TEXT("Enemy card '%s': Enemy Class failed to load. Card skipped."), *It->Key.ToString());
			It.RemoveCurrent();
		}
	}
	SingleCards.RemoveAll([this](const FName& Id) { return !Cards.Contains(Id); });
	Squads.RemoveAll([this](const FWaveSquadEntry& Squad)
	{
		for (const TPair<FName, int32>& Member : Squad.Members)
		{
			if (!Cards.Contains(Member.Key))
			{
				return true;
			}
		}
		return false;
	});

	if (SingleCards.Num() == 0 && Squads.Num() == 0)
	{
		UE_LOG(LogWaveDirector, Error, TEXT("%s: no enemies could be loaded. No waves."), *GetNameSafe(GetOwner()));
		FinishEncounter();
		return;
	}

	UE_LOG(LogWaveDirector, Log, TEXT("%s: %d waves, %.0f credits per wave, %d single cards, %d squads, %d spawn points."),
		*GetNameSafe(GetOwner()), WaveCount, WaveBudget, SingleCards.Num(), Squads.Num(), SpawnPoints.Num());

	StartNextWave();
}

// ======================================================================
// Waves
// ======================================================================

void UWaveDirectorComponent::StartNextWave()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(NextWaveTimer);

	++CurrentWave;
	GroupPoints.Reset();
	CurrentWaveSpent = BuyWave(CurrentWave);
	LastWaveStartTime = World->GetTimeSeconds();

	UE_LOG(LogWaveDirector, Log, TEXT("%s: wave %d/%d bought %d enemies for %d credits (%d carried over)."),
		*GetNameSafe(GetOwner()), CurrentWave, WaveCount, Queue.Num(), CurrentWaveSpent, CarriedCredits);

	OnWaveStarted.Broadcast(CurrentWave, WaveCount);

	if (Queue.Num() == 0)
	{
		UE_LOG(LogWaveDirector, Warning, TEXT("%s: wave %d couldn't afford any enemy (budget too low for the cheapest card)."), *GetNameSafe(GetOwner()), CurrentWave);
		CheckProgress();
		return;
	}

	// Spawn the first one now, then one every Spawn Interval.
	ProcessQueue();
	if (Queue.Num() > 0 && !World->GetTimerManager().IsTimerActive(QueueTimer))
	{
		World->GetTimerManager().SetTimer(QueueTimer, this, &UWaveDirectorComponent::ProcessQueue, SpawnInterval, true);
	}
}

int32 UWaveDirectorComponent::BuyWave(int32 WaveNumber)
{
	FRandomStream& Stream = GetStream();
	int32 Credits = FMath::FloorToInt(WaveBudget) + CarriedCredits;
	int32 Spent = 0;

	// Squads first (designed groups), then single cards to use up what's left.
	for (int32 Safety = 0; Safety < 200; ++Safety)
	{
		TArray<const FWaveSquadEntry*> AffordableSquads;
		for (const FWaveSquadEntry& Squad : Squads)
		{
			if (Squad.Cost <= Credits)
			{
				AffordableSquads.Add(&Squad);
			}
		}

		const int32 SquadPick = WaveDirector::PickWeighted(AffordableSquads, Stream, [](const FWaveSquadEntry* S) { return S->Weight; });
		if (SquadPick != INDEX_NONE)
		{
			const FWaveSquadEntry& Squad = *AffordableSquads[SquadPick];
			const int32 GroupId = NextGroupId++;
			for (const TPair<FName, int32>& Member : Squad.Members)
			{
				for (int32 i = 0; i < Member.Value; ++i)
				{
					Queue.Add({ Member.Key, GroupId, WaveNumber });
				}
			}
			Credits -= Squad.Cost;
			Spent += Squad.Cost;
			continue;
		}

		TArray<FName> AffordableCards;
		for (const FName& Id : SingleCards)
		{
			const FEnemySpawnCard* Card = Cards.Find(Id);
			if (Card && Card->Cost <= Credits)
			{
				AffordableCards.Add(Id);
			}
		}

		const int32 CardPick = WaveDirector::PickWeighted(AffordableCards, Stream, [this](const FName& Id) { return Cards.FindChecked(Id).Weight; });
		if (CardPick == INDEX_NONE)
		{
			break; // can't afford anything else
		}

		const FName Id = AffordableCards[CardPick];
		const int32 Cost = Cards.FindChecked(Id).Cost;
		Queue.Add({ Id, NextGroupId++, WaveNumber });
		Credits -= Cost;
		Spent += Cost;
	}

	CarriedCredits = Credits;
	return Spent;
}

// ======================================================================
// Spawning
// ======================================================================

void UWaveDirectorComponent::ProcessQueue()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (Queue.Num() == 0)
	{
		World->GetTimerManager().ClearTimer(QueueTimer);
		return;
	}

	// Cap: wait until something dies.
	if (GetAliveCount() + Telegraphing >= MaxAliveEnemies)
	{
		return;
	}

	const FWavePendingSpawn Entry = Queue[0];
	Queue.RemoveAt(0);

	const FEnemySpawnCard* Card = Cards.Find(Entry.CardId);

	// Squad members share their group's spawn point.
	ASpawnPoint* Point = nullptr;
	if (const TWeakObjectPtr<ASpawnPoint>* Existing = GroupPoints.Find(Entry.GroupId))
	{
		Point = Existing->Get();
	}
	if (!Point && Card)
	{
		Point = ChooseSpawnPoint(Card->SpawnPointType);
		GroupPoints.Add(Entry.GroupId, Point);
	}

	FVector Location;
	if (!Card || !Point || !Point->FindSpawnLocation(GetStream(), Location))
	{
		UE_LOG(LogWaveDirector, Warning, TEXT("%s: couldn't place '%s', skipped."), *GetNameSafe(GetOwner()), *Entry.CardId.ToString());
		CheckProgress();
		return;
	}

	++Telegraphing;

	if (TelegraphEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, TelegraphEffect, Location);
	}
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTelegraph)
	{
		DrawDebugSphere(World, Location, 60.f, 12, FColor::Red, false, FMath::Max(TelegraphTime, 0.1f));
	}
#endif
	OnSpawnTelegraph.Broadcast(Location, TelegraphTime);

	if (TelegraphTime > 0.f)
	{
		FTimerHandle Handle;
		const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UWaveDirectorComponent::FinishSpawn, Entry.CardId, Entry.WaveNumber, Location);
		World->GetTimerManager().SetTimer(Handle, Delegate, TelegraphTime, false);
	}
	else
	{
		FinishSpawn(Entry.CardId, Entry.WaveNumber, Location);
	}
}

void UWaveDirectorComponent::FinishSpawn(FName CardId, int32 WaveNumber, FVector Location)
{
	Telegraphing = FMath::Max(0, Telegraphing - 1);

	UWorld* World = GetWorld();
	const FEnemySpawnCard* Card = Cards.Find(CardId);
	UClass* EnemyClass = Card ? Card->EnemyClass.Get() : nullptr;
	if (!World || !EnemyClass)
	{
		CheckProgress();
		return;
	}

	// The location is on the floor; lift characters by their capsule so they don't spawn half in the ground.
	FVector SpawnLocation = Location;
	if (Card->SpawnPointType != ESpawnPointType::Air)
	{
		if (const ACharacter* CharacterCDO = Cast<ACharacter>(EnemyClass->GetDefaultObject()))
		{
			if (const UCapsuleComponent* Capsule = CharacterCDO->GetCapsuleComponent())
			{
				SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
			}
		}
	}

	// Face the player.
	FRotator SpawnRotation = FRotator::ZeroRotator;
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		FVector ToPlayer = Player->GetActorLocation() - SpawnLocation;
		ToPlayer.Z = 0.f;
		if (!ToPlayer.IsNearlyZero())
		{
			SpawnRotation = ToPlayer.Rotation();
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* Enemy = World->SpawnActor<APawn>(EnemyClass, SpawnLocation, SpawnRotation, Params);
	if (Enemy)
	{
		// In case the Blueprint's Auto Possess AI isn't set to "Placed in World or Spawned".
		if (!Enemy->GetController())
		{
			Enemy->SpawnDefaultController();
		}

		if (!UHealthComponent::FindHealthComponent(Enemy))
		{
			UE_LOG(LogWaveDirector, Warning, TEXT("%s has no Health Component, so it can't die or count toward the room."), *Enemy->GetName());
		}

		Spawned.Add({ Enemy, WaveNumber, Card->Cost });

		if (ARoomDirector* Director = Cast<ARoomDirector>(GetOwner()))
		{
			Director->RegisterEnemy(Enemy);
		}
	}

	CheckProgress();
}

ASpawnPoint* UWaveDirectorComponent::ChooseSpawnPoint(ESpawnPointType Type)
{
	// Version 1: a random enabled point of the right type, preferring ones at least Min Player Distance away.
	// The EQS upgrade replaces only this function.
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	TArray<ASpawnPoint*> Matching;
	TArray<ASpawnPoint*> FarEnough;
	for (const TWeakObjectPtr<ASpawnPoint>& Weak : SpawnPoints)
	{
		ASpawnPoint* Point = Weak.Get();
		if (!Point || !Point->bEnabled || Point->SpawnPointType != Type)
		{
			continue;
		}
		Matching.Add(Point);
		if (!Player || FVector::Dist(Point->GetActorLocation(), Player->GetActorLocation()) >= MinPlayerDistance)
		{
			FarEnough.Add(Point);
		}
	}

	const TArray<ASpawnPoint*>& Pool = FarEnough.Num() > 0 ? FarEnough : Matching;
	if (Pool.Num() == 0)
	{
		UE_LOG(LogWaveDirector, Warning, TEXT("%s: no enabled Spawn Point of type %s."), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Type));
		return nullptr;
	}
	return Pool[GetStream().RandHelper(Pool.Num())];
}

// ======================================================================
// Progress
// ======================================================================

void UWaveDirectorComponent::NotifyEnemyDied()
{
	CheckProgress();
}

void UWaveDirectorComponent::CheckProgress()
{
	if (!bEncounterActive || !bClassesLoaded)
	{
		return;
	}

	// Last wave: finished once everything has spawned.
	if (CurrentWave >= WaveCount)
	{
		if (Queue.Num() == 0 && Telegraphing == 0)
		{
			FinishEncounter();
		}
		return;
	}

	EvaluateNextWave();
}

void UWaveDirectorComponent::EvaluateNextWave()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The current wave has to be fully on the field first.
	if (Queue.Num() > 0 || Telegraphing > 0)
	{
		return;
	}

	// Overlap: start the next wave once what's left of this one is worth little enough.
	const int32 AliveCredits = GetAliveCreditsInWave(CurrentWave);
	if (AliveCredits > CurrentWaveSpent * NextWaveThreshold)
	{
		return;
	}

	const float Wait = MinTimeBetweenWaves - (World->GetTimeSeconds() - LastWaveStartTime);
	if (Wait > KINDA_SMALL_NUMBER)
	{
		if (!World->GetTimerManager().IsTimerActive(NextWaveTimer))
		{
			World->GetTimerManager().SetTimer(NextWaveTimer, this, &UWaveDirectorComponent::CheckProgress, Wait, false);
		}
		return;
	}

	StartNextWave();
}

void UWaveDirectorComponent::FinishEncounter()
{
	if (!bEncounterActive)
	{
		return;
	}
	bEncounterActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QueueTimer);
		World->GetTimerManager().ClearTimer(NextWaveTimer);
	}

	UE_LOG(LogWaveDirector, Log, TEXT("%s: all waves spawned."), *GetNameSafe(GetOwner()));
	OnAllWavesSpawned.Broadcast();

	if (ARoomDirector* Director = Cast<ARoomDirector>(GetOwner()))
	{
		Director->NotifyWavesFinished();
	}
}

// ======================================================================
// Counting
// ======================================================================

bool UWaveDirectorComponent::IsEnemyAlive(const AActor* Enemy)
{
	if (!IsValid(Enemy))
	{
		return false;
	}
	const UHealthComponent* Health = UHealthComponent::FindHealthComponent(const_cast<AActor*>(Enemy));
	return Health && !Health->IsDead();
}

int32 UWaveDirectorComponent::GetAliveCount()
{
	Spawned.RemoveAll([](const FWaveSpawnedEnemy& E) { return !IsEnemyAlive(E.Actor.Get()); });
	return Spawned.Num();
}

int32 UWaveDirectorComponent::GetAliveCreditsInWave(int32 WaveNumber)
{
	GetAliveCount(); // prunes the dead
	int32 Credits = 0;
	for (const FWaveSpawnedEnemy& E : Spawned)
	{
		if (E.WaveNumber == WaveNumber)
		{
			Credits += E.Cost;
		}
	}
	return Credits;
}

void UWaveDirectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
	bEncounterActive = false;

	Super::EndPlay(EndPlayReason);
}
