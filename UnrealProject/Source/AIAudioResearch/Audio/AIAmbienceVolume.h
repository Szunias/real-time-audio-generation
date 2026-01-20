// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIFootstepComponent.h" // For EFootstepSurface enum
#include "AIAmbienceVolume.generated.h"

class UBoxComponent;
class UAIAmbienceComponent;
class UAIFootstepComponent;

/**
 * Volume that triggers ambience change when player enters.
 * Uses tick-based overlap check as fallback for unreliable overlap events.
 * Also pre-generates footsteps for expected surface type.
 */
UCLASS()
class AIAUDIORESEARCH_API AAIAmbienceVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	AAIAmbienceVolume();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Ambience", meta = (MakeEditWidget = true))
	UBoxComponent* Bounds;

	/** Prompt to switch to when entering this volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	FString AmbiencePrompt = TEXT("dark castle ambience");
	
	/** Expected footstep surface type in this zone (for pre-generation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience|Footsteps")
	EFootstepSurface ExpectedSurface = EFootstepSurface::Stone;
	
	/** Enable footstep pre-generation when entering zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience|Footsteps")
	bool bPreGenFootsteps = true;
	
	/** How often to check for player overlap (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TickCheckInterval = 0.2f;
	
protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;
	
private:
	/** Track if player is currently inside */
	bool bPlayerInside = false;
	
	/** Cached player pawn */
	UPROPERTY()
	APawn* CachedPlayer = nullptr;
	
	/** Timer for tick-based check */
	float TimeSinceLastCheck = 0.0f;
	
	/** Helper to find and trigger ambience component */
	void TriggerAmbience(AActor* PlayerActor);
	void StopAmbience(AActor* PlayerActor);
};
