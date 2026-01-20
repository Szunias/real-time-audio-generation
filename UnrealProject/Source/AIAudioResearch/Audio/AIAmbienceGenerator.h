// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIAmbienceGenerator.generated.h"

class UAudioManager;
class UAudioComponent;
class USoundWaveProcedural;

/**
 * Generates AI ambient sound at level start
 * Place in level and configure the ambient prompt
 */
UCLASS(Blueprintable, BlueprintType)
class AIAUDIORESEARCH_API AAIAmbienceGenerator : public AActor
{
	GENERATED_BODY()

public:
	AAIAmbienceGenerator();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** Server URL for AI audio generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	FString ServerURL = TEXT("ws://localhost:8766"); // ElevenLabs port

	/** Ambient sound description prompt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	FString AmbiencePrompt = TEXT("forest ambience with birds chirping, wind through trees, peaceful daytime atmosphere, graveyard");

	/** Duration of ambient sound in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	float AmbienceDuration = 10.0f;

	/** Should the ambient sound loop? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	bool bLoopAmbience = true;

	/** Volume of ambient sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	float Volume = 0.7f;

	/** Generate on BeginPlay automatically? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	bool bGenerateOnStart = true;

	/** Manually trigger ambient generation */
	UFUNCTION(BlueprintCallable, Category = "AI Ambience")
	void GenerateAmbience();

	/** Stop ambient playback */
	UFUNCTION(BlueprintCallable, Category = "AI Ambience")
	void StopAmbience();

private:
	UFUNCTION()
	void OnAmbienceReady(const FString& SoundId, USoundWaveProcedural* Sound);

	/** Called by timer to replay ambient for looping */
	void ReplayAmbience();

	UPROPERTY()
	UAudioManager* AudioManager;

	UPROPERTY()
	UAudioComponent* AudioComponent;

	bool bIsGenerating = false;
	
	/** Timer for looping ambient sound */
	FTimerHandle LoopTimerHandle;
	
	/** Cached duration for loop timing */
	float CachedDuration = 0.0f;
};
