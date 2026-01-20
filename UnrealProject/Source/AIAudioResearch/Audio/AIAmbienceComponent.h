// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIAmbienceComponent.generated.h"

// Forward declarations
class UAudioManager;
class UAudioComponent;
class USoundWaveProcedural;

/**
 * Component that generates AI ambient sound (music/sfx) at play start.
 * Uses ElevenLabs or other AI models via Unified Server.
 */
UCLASS(ClassGroup=(AIAudio), meta=(BlueprintSpawnableComponent))
class AIAUDIORESEARCH_API UAIAmbienceComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAIAmbienceComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	/** Server URL for the unified audio server */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	FString ServerURL = TEXT("ws://localhost:8770");

	/** Ambient sound description prompt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	FString AmbiencePrompt = TEXT("forest");

	/** Duration of ambient sound in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience", meta = (ClampMin = "3.0", ClampMax = "20.0"))
	float AmbienceDuration = 10.0f;

	/** Should the ambient sound loop? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	bool bLoopAmbience = true;
	
	/** AI model to use for ambience generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	FString AIModel = TEXT("elevenlabs");

	/** Volume of ambient sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	float Volume = 0.7f;
	
	/** Crossfade duration when switching zones (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float CrossfadeDuration = 1.5f;

	/** Dynamically switch ambience to a new prompt (e.g. entering a zone) */
	UFUNCTION(BlueprintCallable, Category = "AI Ambience")
	void SwitchAmbience(FString NewPrompt);

	/** Pre-generate/Cache an ambience prompt without playing it yet */
	UFUNCTION(BlueprintCallable, Category = "AI Ambience")
	void PreCacheAmbience(FString Prompt);
	
	/** Stop current ambience (called when exiting zone) */
	UFUNCTION(BlueprintCallable, Category = "AI Ambience")
	void StopAmbience();

private:
	UFUNCTION()
	void OnAmbienceReady(const FString& SoundId, USoundWaveProcedural* Sound);

	/** Trigger generation */
	void GenerateAmbience();

	/** Helper to safely retry generation via timer */
	void TryGenerateAmbience();

	/** Called by timer to replay ambient for looping */
	void ReplayAmbience();

	// Helper to generate consistent ID from prompt
	FString GetIDForPrompt(const FString& Prompt);
	
	/** Called by timer after crossfade completes */
	void PlayAfterCrossfade();

	UPROPERTY()
	UAudioManager* AudioManager;
	
	UPROPERTY()
	UAudioComponent* AudioComponent;
	
	UPROPERTY()
	FString CurrentSoundId;
	
	FTimerHandle LoopTimerHandle;
	FTimerHandle CrossfadeTimerHandle;
	float CachedDuration = 0.0f;
	bool bIsGenerating = false;
	bool bPendingCachedPlay = false;
	
	/** Track sound IDs that are currently being generated to prevent duplicates */
	TSet<FString> GeneratingSoundIds;
};
