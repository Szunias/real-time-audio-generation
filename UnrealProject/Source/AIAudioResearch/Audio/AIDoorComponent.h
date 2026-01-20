// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIDoorComponent.generated.h"

// Forward declarations
class UAudioManager;
class UAudioComponent;
class USoundWaveProcedural;

/**
 * Component for doors with AI-generated open/close sounds.
 * Supports pre-caching for zero-latency playback.
 */
UCLASS(ClassGroup=(AIAudio), meta=(BlueprintSpawnableComponent))
class AIAUDIORESEARCH_API UAIDoorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAIDoorComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// ============ SETTINGS ============
	
	/** Server URL for the unified audio server */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString ServerURL = TEXT("ws://localhost:8770");
	
	/** AI model to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString AIModel = TEXT("elevenlabs");
	
	/** Sound prompt for OPENING the door */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString OpenPrompt = TEXT("wooden door creaking open");
	
	/** Sound prompt for CLOSING the door */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	FString ClosePrompt = TEXT("wooden door closing shut");
	
	/** Duration of the sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	float SoundDuration = 2.0f;
	
	/** Volume multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Audio|Settings")
	float Volume = 1.0f;

	// ============ FUNCTIONS ============
	
	/** Play the OPEN door sound */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void PlayOpenSound();
	
	/** Play the CLOSE door sound */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void PlayCloseSound();
	
	/** Pre-cache BOTH open and close sounds */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void PreCacheSounds();
	
	/** Check if open sound is cached */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	bool IsOpenSoundCached() const;
	
	/** Check if close sound is cached */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	bool IsCloseSoundCached() const;
	
	/** Get sound IDs */
	FString GetOpenSoundId() const;
	FString GetCloseSoundId() const;

private:
	UFUNCTION()
	void OnSoundReady(const FString& SoundId, USoundWaveProcedural* Sound);
	
	void PlaySoundInternal(const FString& Prompt, const FString& SoundId);
	void PreCacheSoundInternal(const FString& Prompt, const FString& SoundId);

	UPROPERTY()
	UAudioManager* AudioManager;
	
	UPROPERTY()
	UAudioComponent* AudioComponent;
	
	TSet<FString> CachedSounds;
	TSet<FString> GeneratingSounds;
	
	/** Track which sound user requested to play (via E press) */
	FString PendingPlaySoundId;
};
