// EncounterSubsystem.cpp

#include "Enemies/EncounterSubsystem.h"
#include "Enemies/EnemyInfoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UEncounterSubsystem* UEncounterSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UEncounterSubsystem>() : nullptr;
}

bool UEncounterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEncounterSubsystem::RegisterScreenBarEnemy(UEnemyInfoComponent* Enemy)
{
	if (!Enemy)
	{
		return;
	}
	// One screen bar at a time (one boss/elite per room). A second registration takes over.
	ScreenBarEnemy = Enemy;
	OnScreenBarEnemyChanged.Broadcast(Enemy);
}

void UEncounterSubsystem::UnregisterScreenBarEnemy(UEnemyInfoComponent* Enemy)
{
	if (ScreenBarEnemy.Get() != Enemy)
	{
		return;
	}
	ScreenBarEnemy.Reset();
	OnScreenBarEnemyChanged.Broadcast(nullptr);
}
