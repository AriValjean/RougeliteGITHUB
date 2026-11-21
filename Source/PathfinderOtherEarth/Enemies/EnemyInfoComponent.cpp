// EnemyInfoComponent.cpp

#include "Enemies/EnemyInfoComponent.h"
#include "Enemies/EncounterSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Currency/CurrencyTypes.h"
#include "Inventory/InventoryManagerComponent.h"
#include "Meta/PathfinderMetaSubsystem.h"
#include "RoomFlow/RoomFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyInfo, Log, All);

UEnemyInfoComponent::UEnemyInfoComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FName UEnemyInfoComponent::GetEnemyId() const
{
	if (!EnemyId.IsNone())
	{
		return EnemyId;
	}
	return GetOwner() ? GetOwner()->GetClass()->GetFName() : NAME_None;
}

UHealthComponent* UEnemyInfoComponent::GetHealthComponent() const
{
	return UHealthComponent::FindHealthComponent(GetOwner());
}

UEnemyInfoComponent* UEnemyInfoComponent::FindEnemyInfo(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UEnemyInfoComponent>() : nullptr;
}

void UEnemyInfoComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UHealthComponent* Health = GetHealthComponent())
	{
		Health->OnDeath.AddUniqueDynamic(this, &UEnemyInfoComponent::HandleDeath);
	}
	else
	{
		UE_LOG(LogEnemyInfo, Warning, TEXT("%s has no Health Component; drops and the boss bar won't work."), *GetNameSafe(GetOwner()));
	}

	if (bShowScreenHealthBar)
	{
		if (UEncounterSubsystem* Encounter = UEncounterSubsystem::Get(this))
		{
			Encounter->RegisterScreenBarEnemy(this);
		}
	}
}

void UEnemyInfoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHealthComponent* Health = GetHealthComponent())
	{
		Health->OnDeath.RemoveDynamic(this, &UEnemyInfoComponent::HandleDeath);
	}
	if (UEncounterSubsystem* Encounter = UEncounterSubsystem::Get(this))
	{
		Encounter->UnregisterScreenBarEnemy(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UEnemyInfoComponent::HandleDeath(AActor* Killer)
{
	GrantMaterialDrops();

	// Elites and bosses are remembered in the save file (for dialogue / unlocks later).
	if (Tier != EEnemyTier::Standard)
	{
		const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (UPathfinderMetaSubsystem* Meta = GI ? GI->GetSubsystem<UPathfinderMetaSubsystem>() : nullptr)
		{
			Meta->RecordEnemyDefeated(GetEnemyId());
		}
	}

	if (UEncounterSubsystem* Encounter = UEncounterSubsystem::Get(this))
	{
		Encounter->UnregisterScreenBarEnemy(this);
	}
}

void UEnemyInfoComponent::GrantMaterialDrops()
{
	const FEnemyRewardRow* Row = RewardsRow.IsNull() ? nullptr : RewardsRow.GetRow<FEnemyRewardRow>(TEXT("EnemyInfo Drops"));
	if (!Row || Row->MaterialDrops.Num() == 0)
	{
		return;
	}

	// Drops go straight to the player (single-player): no pickups in the world.
	UInventoryManagerComponent* Inventory =
		UInventoryManagerComponent::FindInventoryManager(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Inventory)
	{
		return;
	}

	// Roll from the run's seeded stream when a run is active, so a seed reproduces the run.
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	URoomFlowSubsystem* RoomFlow = GI ? GI->GetSubsystem<URoomFlowSubsystem>() : nullptr;
	FRandomStream Fallback(FMath::Rand());
	FRandomStream& Rng = (RoomFlow && RoomFlow->IsRunActive()) ? RoomFlow->GetRandomStream() : Fallback;

	for (const FMaterialDrop& Drop : Row->MaterialDrops)
	{
		if (Drop.MaterialId.IsNone() || Rng.FRand() >= Drop.Chance)
		{
			continue;
		}

		const int32 Count = Rng.RandRange(FMath::Min(Drop.MinCount, Drop.MaxCount), FMath::Max(Drop.MinCount, Drop.MaxCount));
		if (!Inventory->AddMaterial(Drop.MaterialId, Count))
		{
			UE_LOG(LogEnemyInfo, Warning, TEXT("%s: material '%s' is not in DT_CraftingMaterialsCPP."), *GetNameSafe(GetOwner()), *Drop.MaterialId.ToString());
		}
	}
}
