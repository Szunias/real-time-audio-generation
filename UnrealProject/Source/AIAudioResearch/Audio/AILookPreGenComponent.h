// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AILookPreGenComponent.generated.h"

class UAIDoorComponent;

/**
 * Component that pre-generates sounds for objects the player is looking at.
 * Uses line trace from camera to detect interactable objects with sound components.
 * Add this to the Player Character.
 */
UCLASS(ClassGroup=(AIAudio), meta=(BlueprintSpawnableComponent))
class AIAUDIORESEARCH_API UAILookPreGenComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAILookPreGenComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
		FActorComponentTickFunction* ThisTickFunction) override;

	// ============ SETTINGS ============
	
	/** Distance to trace for objects (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	float TraceDistance = 2000.0f;  // 20 meters - earlier detection for pre-generation
	
	/** How often to check for objects (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	float CheckInterval = 0.3f;
	
	/** Show debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	bool bShowDebug = false;
	
	/** Collision channel to trace */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
	void DoLineTrace();
	
	float TimeSinceLastCheck = 0.0f;
	
	/** Set of already pre-cached object IDs to avoid duplicate requests */
	TSet<FString> PreCachedObjects;
	
	/** Currently looked-at actor (for debug display) */
	UPROPERTY()
	AActor* CurrentTarget;
};
