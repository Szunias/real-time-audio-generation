// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIJumpComponent.generated.h"

// Forward declarations
class UAudioManager;
class UAudioComponent;
class USoundWaveProcedural;
class ACharacter;

/**
 * Component that generates physics-based Foley sounds for Jumping and Landing.
 * Detects movement state changes automatically.
 */
UCLASS(ClassGroup=(AIAudio), meta=(BlueprintSpawnableComponent))
class AIAUDIORESEARCH_API UAIJumpComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAIJumpComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server URL for the unified audio server */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString ServerURL = TEXT("ws://localhost:8770");
	
	/** AI model to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString AIModel = TEXT("elevenlabs");

	// ============ PROMPTS (Simple Defaults) ============
	
	/** Sound prompt for jumping (lifting off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString JumpPrompt = TEXT("quick air whoosh movement");

	/** Sound prompt for landing (impact) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString LandPrompt = TEXT("soft foot impact on ground");
	
	/** Volume multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	float Volume = 0.8f;

private:
	// Logic
	void CheckMovementState();
	void OnJump();
	void OnLand();
	
	// Networking
	UFUNCTION()
	void OnSoundReady(const FString& SoundId, USoundWaveProcedural* Sound);
	
	void PreGenerateSounds();
	void TryPreGenerate();

	// State
	UPROPERTY()
	UAudioManager* AudioManager;
	
	UPROPERTY()
	UAudioComponent* AudioComponent;
	
	UPROPERTY()
	ACharacter* CharacterOwner;
	
	bool bWasFalling = false;
	bool bSoundsRequested = false;
};
