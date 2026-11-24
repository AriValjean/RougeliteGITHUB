// SkillTargetIndicator.h
// Marker shown while holding a Hold To Aim skill.
//
// Circle (Rain of Arrows, Blink): make BP_TargetIndicator with a ring / disc mesh that is 100 cm in
//   radius at scale 1. Set Indicator Radius scales the actor to the skill's Radius.
// Line (Arcane Bolt): make BP_LineIndicator with a flat 100 x 100 cm plane moved forward 50 cm (X = 50)
//   so its back edge sits on the actor's origin. Set Indicator Line scales X to the length and Y to the width.
// Override either function in the Blueprint to size a decal or material differently.
//
// Drop into: Source/PathfinderOtherEarth/Skills/SkillTargetIndicator.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillTargetIndicator.generated.h"

UCLASS(Blueprintable)
class PATHFINDEROTHEREARTH_API ASkillTargetIndicator : public AActor
{
	GENERATED_BODY()

public:
	ASkillTargetIndicator();

	/** Circle. Default: scales the actor in X/Y so a 100 cm-radius mesh matches Radius. */
	UFUNCTION(BlueprintNativeEvent, Category = "Indicator")
	void SetIndicatorRadius(float Radius);

	/** Line. Called every frame while aiming. Default: X scale = Length / 100, Y scale = Width / 100. */
	UFUNCTION(BlueprintNativeEvent, Category = "Indicator")
	void SetIndicatorLine(float Length, float Width);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Indicator")
	TObjectPtr<USceneComponent> Root;
};
