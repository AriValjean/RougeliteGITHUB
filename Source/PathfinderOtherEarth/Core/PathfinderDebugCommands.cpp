// PathfinderDebugCommands.cpp
// Debug-only console commands that don't belong to one subsystem.
// Not compiled into Shipping builds.
//
//   Run.ReturnToCamp   Ends the current run exactly like a death (death animation, Save Run State,
//                      Reset Run Progress, End Run -> camp) by killing the player instantly.
//                      Ignores Death Defiance / invincibility. Does nothing outside a run.
//
// Drop into: Source/PathfinderOtherEarth/Core/PathfinderDebugCommands.cpp

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Combat/HealthComponent.h"
#include "RoomFlow/RoomFlowSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPathfinderDebug, Log, All);

static FAutoConsoleCommandWithWorld GPathfinderReturnToCampCommand(
	TEXT("Run.ReturnToCamp"),
	TEXT("Debug: ends the current run like a death and travels to camp."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		const URoomFlowSubsystem* RoomFlow = GI ? GI->GetSubsystem<URoomFlowSubsystem>() : nullptr;
		if (!RoomFlow || !RoomFlow->IsRunActive())
		{
			UE_LOG(LogPathfinderDebug, Warning, TEXT("Run.ReturnToCamp: no run is active (already at camp?)."));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UHealthComponent* Health = UHealthComponent::FindHealthComponent(Pawn);
		if (!Health)
		{
			UE_LOG(LogPathfinderDebug, Warning, TEXT("Run.ReturnToCamp: player has no Health Component."));
			return;
		}

		// Uses the normal death flow (Player Death event on BP_PlayerCharacter), so the run ends exactly like a real death.
		UE_LOG(LogPathfinderDebug, Log, TEXT("Run.ReturnToCamp: ending the run."));
		Health->Kill(nullptr);
	}),
	ECVF_Cheat);

#endif // !UE_BUILD_SHIPPING
