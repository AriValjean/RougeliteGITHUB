// EncounterSubsystem.h
// One per level. Tracks the enemy that owns the screen-wide health bar (the current boss or elite).
// W_BossBar binds On Screen Bar Enemy Changed; None means "hide the bar".

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EncounterSubsystem.generated.h"

class UEnemyInfoComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScreenBarEnemyChanged, UEnemyInfoComponent*, Enemy);

UCLASS()
class PATHFINDEROTHEREARTH_API UEncounterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UEncounterSubsystem* Get(const UObject* WorldContext);

	/** The enemy currently shown on the screen health bar, or None. Read this when the HUD is created. */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	UEnemyInfoComponent* GetScreenBarEnemy() const { return ScreenBarEnemy.Get(); }

	/** Fires when a boss/elite appears (Enemy valid) or dies/leaves (Enemy = None). */
	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FOnScreenBarEnemyChanged OnScreenBarEnemyChanged;

	void RegisterScreenBarEnemy(UEnemyInfoComponent* Enemy);
	void UnregisterScreenBarEnemy(UEnemyInfoComponent* Enemy);

protected:
	/** Only real game worlds (not editor previews). */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TWeakObjectPtr<UEnemyInfoComponent> ScreenBarEnemy;
};
