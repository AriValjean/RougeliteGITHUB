// SkillTargetIndicator.cpp
// Drop into: Source/PathfinderOtherEarth/Skills/SkillTargetIndicator.cpp

#include "Skills/SkillTargetIndicator.h"
#include "Components/SceneComponent.h"

ASkillTargetIndicator::ASkillTargetIndicator()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ASkillTargetIndicator::SetIndicatorRadius_Implementation(float Radius)
{
	const float Scale = FMath::Max(0.01f, Radius / 100.f);
	SetActorScale3D(FVector(Scale, Scale, 1.f));
}

void ASkillTargetIndicator::SetIndicatorLine_Implementation(float Length, float Width)
{
	SetActorScale3D(FVector(FMath::Max(0.01f, Length / 100.f), FMath::Max(0.01f, Width / 100.f), 1.f));
}
