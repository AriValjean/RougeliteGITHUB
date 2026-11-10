// RoomPortal.h
// Exit portal placed in each room level (usually two: Portal Index 0 = A, 1 = B).
// Hidden and non-interactable until the room is cleared. Then it takes its option from the
// Room Flow Subsystem (Options[Portal Index]) and shows itself. If there is no option for its
// index, it stays hidden.
//
// The Blueprint child (BP_BasePortal) implements InteractableInterface and handles visuals,
// sound and Save Run State, then calls Use Portal to travel.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomFlow/RoomFlowSubsystem.h"
#include "RoomPortal.generated.h"

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API ARoomPortal : public AActor
{
	GENERATED_BODY()

public:
	ARoomPortal();

	/** Travels to this portal's room. Returns false if the portal isn't active (or was already used).
	 *  Save run state BEFORE calling this. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool UsePortal();

	/** True once the room is cleared and this portal has an option. Use for Is Available. */
	UFUNCTION(BlueprintPure, Category = "Portal")
	bool IsPortalActive() const { return bPortalActive; }

	/** The option this portal leads to (room, type, advances stage, ends run). */
	UFUNCTION(BlueprintPure, Category = "Portal")
	const FPortalOption& GetPortalOption() const { return PortalOption; }

	/** Prompt text for the interaction widget, e.g. "Combat", "Shop", "Boss", "Next Stage", "Return to Camp". */
	UFUNCTION(BlueprintPure, Category = "Portal")
	FText GetPromptText() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Camp portal: visible from the start (no Room Director needed), prompt "Start Run",
	 *  and using it starts a new run at the Starting World's first Start room. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Portal")
	bool bStartsRun = false;

	/** 0 = Portal A (main path), 1 = Portal B (shop / alternative). Set per placed portal. Ignored when Starts Run is ticked. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Portal", meta = (ClampMin = "0"))
	int32 PortalIndex = 0;

	/** Called when this portal appears. Use it for the label, icon, VFX and sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void OnPortalActivated(const FPortalOption& Option);

private:
	UFUNCTION()
	void HandlePortalsReady(const TArray<FPortalOption>& Options);

	void SetPortalVisible(bool bVisible);
	URoomFlowSubsystem* GetRoomFlow() const;

	UPROPERTY(VisibleInstanceOnly, Category = "Portal")
	FPortalOption PortalOption;

	UPROPERTY(VisibleInstanceOnly, Category = "Portal")
	bool bPortalActive = false;
};
