// InteractionManagerComponent.h
// C++ replacement for BPC_InteractionManager's logic.
// Detects interactables overlapping the owner's capsule, focuses the one the
// camera is looking at, shows the prompt, and runs press/hold interactions.
// Input lives on the character: IA_Interact Started -> BeginInteract,
// Completed/Canceled -> EndInteract.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionTypes.h"
#include "InteractionManagerComponent.generated.h"

class UPrimitiveComponent;
class UWidgetComponent;
class UInteractionPromptWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionFocusChanged, AActor*, NewFocus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionHoldProgress, float, Percent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteracted, AActor*, Target);

UCLASS(Blueprintable, ClassGroup = (Pathfinder), meta = (BlueprintSpawnableComponent))
class PATHFINDEROTHEREARTH_API UInteractionManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionManagerComponent();

	/** Interact key pressed. Instant interactables fire now; hold interactables start timing. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void BeginInteract();

	/** Interact key released (or cancelled). Cancels an unfinished hold. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void EndInteract();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedInteractable() const { return FocusedActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsHolding() const { return bIsHolding; }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionFocusChanged OnFocusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionHoldProgress OnHoldProgress;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteracted OnInteracted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Set to W_InteractionPromt (after it is reparented to InteractionPromptWidget). */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|UI")
	TSubclassOf<UInteractionPromptWidget> PromptWidgetClass;

	/** How often (seconds) to re-pick the focused interactable while any are in range. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.02", Units = "s"))
	float FocusRefreshInterval = 0.1f;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void CreatePromptWidget();
	void ScanInitialOverlaps();
	void AddCandidate(AActor* Actor);
	void RemoveCandidate(AActor* Actor);
	void RefreshFocus();
	void SetFocus(AActor* NewFocus);
	void UpdatePrompt(bool bForce);
	void PerformInteract();
	void CancelHold();
	void SetHoldProgress(float Percent);

	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	FVector GetInteractionLocationFor(AActor* Actor) const;
	static bool IsInteractable(const AActor* Actor);

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> DetectionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent;

	TArray<TWeakObjectPtr<AActor>> Candidates;
	TWeakObjectPtr<AActor> FocusedActor;
	FInteractionOption FocusedOption;
	FTimerHandle FocusRefreshTimer;

	bool bIsHolding = false;
	float HoldElapsed = 0.f;
};
