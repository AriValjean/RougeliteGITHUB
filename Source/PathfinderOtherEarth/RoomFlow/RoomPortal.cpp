// RoomPortal.cpp

#include "RoomFlow/RoomPortal.h"
#include "Meta/PathfinderMetaSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoomPortal, Log, All);

ARoomPortal::ARoomPortal()
{
	PrimaryActorTick.bCanEverTick = false;
}

URoomFlowSubsystem* ARoomPortal::GetRoomFlow() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<URoomFlowSubsystem>() : nullptr;
}

void ARoomPortal::BeginPlay()
{
	Super::BeginPlay();

	// Camp portal: always available, doesn't wait for a room clear.
	if (bStartsRun)
	{
		bPortalActive = true;
		SetPortalVisible(true);
		return;
	}

	SetPortalVisible(false);

	if (URoomFlowSubsystem* RoomFlow = GetRoomFlow())
	{
		RoomFlow->OnPortalsReady.AddUniqueDynamic(this, &ARoomPortal::HandlePortalsReady);

		// In case the room was already cleared before this portal started (rare, but safe).
		if (RoomFlow->IsCurrentRoomCleared())
		{
			HandlePortalsReady(RoomFlow->GetPortalOptions());
		}
	}
}

void ARoomPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// (RemoveDynamic is safe even if this portal never bound, e.g. the camp portal.)
	// The subsystem outlives this level, so unbind before the portal is destroyed.
	if (URoomFlowSubsystem* RoomFlow = GetRoomFlow())
	{
		RoomFlow->OnPortalsReady.RemoveDynamic(this, &ARoomPortal::HandlePortalsReady);
	}
	Super::EndPlay(EndPlayReason);
}

void ARoomPortal::HandlePortalsReady(const TArray<FPortalOption>& Options)
{
	if (bPortalActive || !Options.IsValidIndex(PortalIndex))
	{
		return; // No option for this portal -> it stays hidden.
	}

	PortalOption = Options[PortalIndex];
	bPortalActive = true;
	SetPortalVisible(true);

	UE_LOG(LogRoomPortal, Log, TEXT("%s (index %d) -> '%s' (%s)."), *GetName(), PortalIndex,
		*PortalOption.RoomId.ToString(), *UEnum::GetValueAsString(PortalOption.RoomType));

	OnPortalActivated(PortalOption);
}

bool ARoomPortal::UsePortal()
{
	if (!bPortalActive)
	{
		return false;
	}

	URoomFlowSubsystem* RoomFlow = GetRoomFlow();
	if (!RoomFlow)
	{
		return false;
	}

	bPortalActive = false; // Prevents a double interact from travelling twice.

	if (bStartsRun)
	{
		if (!RoomFlow->StartRun(NAME_None, 0))
		{
			bPortalActive = true; // Missing table data (see LogRoomFlow): let the player try again.
			return false;
		}

		// New run: reset per-run currencies (Gold), clear "earned this run" (Memoria counter), refill Death Defiance.
		// The level opens next frame, so the new room starts with a clean run state.
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (UPathfinderMetaSubsystem* Meta = GI->GetSubsystem<UPathfinderMetaSubsystem>())
			{
				Meta->BeginRun();
			}
		}

		UE_LOG(LogRoomPortal, Log, TEXT("%s: new run started."), *GetName());
		return true;
	}

	const bool bTravelled = RoomFlow->TravelToPortal(PortalOption);
	if (!bTravelled)
	{
		bPortalActive = true; // Travel failed: keep the portal usable.
	}
	return bTravelled;
}

FText ARoomPortal::GetPromptText() const
{
	if (bStartsRun)
	{
		return NSLOCTEXT("RoomPortal", "StartRun", "Start Run");
	}
	if (PortalOption.bEndsRun)
	{
		return NSLOCTEXT("RoomPortal", "ReturnToCamp", "Return to Camp");
	}
	if (PortalOption.bAdvancesStage)
	{
		return NSLOCTEXT("RoomPortal", "NextStage", "Next Stage");
	}

	switch (PortalOption.RoomType)
	{
	case ERoomType::Combat: return NSLOCTEXT("RoomPortal", "Combat", "Combat");
	case ERoomType::Shop:   return NSLOCTEXT("RoomPortal", "Shop", "Shop");
	case ERoomType::Boss:
		return PortalOption.EncounterName.IsEmpty()
			? NSLOCTEXT("RoomPortal", "Boss", "Boss")
			: FText::Format(NSLOCTEXT("RoomPortal", "BossNamed", "Boss: {0}"), PortalOption.EncounterName);
	case ERoomType::Elite:
		return PortalOption.EncounterName.IsEmpty()
			? NSLOCTEXT("RoomPortal", "Elite", "Elite")
			: FText::Format(NSLOCTEXT("RoomPortal", "EliteNamed", "Elite: {0}"), PortalOption.EncounterName);
	case ERoomType::NPC:    return NSLOCTEXT("RoomPortal", "Encounter", "Encounter");
	default:                return NSLOCTEXT("RoomPortal", "Travel", "Travel");
	}
}

void ARoomPortal::SetPortalVisible(bool bVisible)
{
	// Hidden + no collision = can't be seen, overlapped or interacted with.
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
}
