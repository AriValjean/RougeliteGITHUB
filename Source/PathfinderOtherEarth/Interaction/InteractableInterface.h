// InteractableInterface.h
// C++ replacement for BPI_Interaction. Blueprints implement it via
// Class Settings > Interfaces > Implemented Interfaces > Add > InteractableInterface.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractionTypes.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class PATHFINDEROTHEREARTH_API IInteractableInterface
{
	GENERATED_BODY()

public:
	/** Prompt text, hold time and availability. Interactor = the player asking (lets a chest show a cost, etc.). */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	FInteractionOption GetInteractionOption(AActor* Interactor);

	/** Do the thing. Shows up in Blueprint as "Event Interact". */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	void Interact(AActor* Interactor);

	/** Where the prompt appears. Return (0,0,0) to use the actor's location. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	FVector GetInteractionLocation();
};
