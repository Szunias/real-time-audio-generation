// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Sound/SoundWaveProcedural.h"
#include "AIAudioManager.generated.h"

class FAudioWebSocketClient;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSoundReady, const FString&, SoundId, USoundWaveProcedural*, Sound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLatencyMeasured, const FString&, SoundId, float, LatencyMs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPreGenerationProgress, int32, Completed, int32, Total);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPreGenerationComplete, float, TotalTimeSeconds);

/**
 * Manages AI audio generation, caching, and playback
 * Uses USoundWaveProcedural for runtime PCM audio
 */
UCLASS(Blueprintable, BlueprintType)
class AIAUDIORESEARCH_API UAudioManager : public UObject
{
	GENERATED_BODY()

public:
	UAIAudioManager();
	virtual ~UAIAudioManager();

	/** Initialize and connect to Python backend */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void Initialize(const FString& ServerURL = TEXT("ws://localhost:8765"));
	
	/** Shutdown and disconnect */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void Shutdown();
	
	/** Request a sound to be generated */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void RequestSound(const FString& Prompt, const FString& SoundId = TEXT(""), float Duration = 2.0f, const FString& Model = TEXT("procedural"));
	
	/** Play a sound immediately (from cache if available) */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void PlaySound(const FString& SoundId, UAudioComponent* AudioComponent);
	
	/** Pre-warm cache with multiple sounds */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void PreWarmCache(const TArray<FString>& Prompts);
	
	/** Pre-generate all sounds for a level (with progress tracking) */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void PreGenerateForLevel(const TArray<FString>& SoundPrompts);
	
	/** Check if pre-generation is in progress */
	UFUNCTION(BlueprintPure, Category = "AI Audio")
	bool IsPreGenerating() const { return bIsPreGenerating; }
	
	/** Get pre-generation progress (0.0 - 1.0) */
	UFUNCTION(BlueprintPure, Category = "AI Audio")
	float GetPreGenerationProgress() const;
	
	/** Check if sound is in cache */
	UFUNCTION(BlueprintPure, Category = "AI Audio")
	bool IsSoundCached(const FString& SoundId) const;
	
	/** Get cache statistics */
	UFUNCTION(BlueprintPure, Category = "AI Audio")
	int32 GetCacheSize() const;
	
	/** Clear all cached sounds */
	UFUNCTION(BlueprintCallable, Category = "AI Audio")
	void ClearCache();
	
	/** Check connection status */
	UFUNCTION(BlueprintPure, Category = "AI Audio")
	bool IsConnected() const;

	/** Events - Blueprint compatible */
	UPROPERTY(BlueprintAssignable, Category = "AI Audio")
	FOnSoundReady OnSoundReady;
	
	UPROPERTY(BlueprintAssignable, Category = "AI Audio")
	FOnLatencyMeasured OnLatencyMeasured;
	
	/** Pre-generation progress event */
	UPROPERTY(BlueprintAssignable, Category = "AI Audio")
	FOnPreGenerationProgress OnPreGenerationProgress;
	
	/** Pre-generation completion event */
	UPROPERTY(BlueprintAssignable, Category = "AI Audio")
	FOnPreGenerationComplete OnPreGenerationComplete;

private:
	void HandleAudioDataReceived(const TArray<uint8>& AudioData);
	void HandleWebSocketConnected();
	void HandleWebSocketError(const FString& Error);
	
	USoundWaveProcedural* CreateProceduralSound(const TArray<uint8>& PCMData, int32 SampleRate = 44100, int32 NumChannels = 1);

	TSharedPtr<FAudioWebSocketClient> WebSocketClient;
	
	/** Cache of generated sounds (SoundId -> SoundWaveProcedural) */
	UPROPERTY()
	TMap<FString, USoundWaveProcedural*> SoundCache;
	
	/** Cache of raw PCM data for replay */
	TMap<FString, TArray<uint8>> PCMDataCache;
	
	/** Queue of pending request IDs (FIFO order) */
	TArray<FString> PendingRequestQueue;
	
	/** Timestamps for pending requests (parallel to queue) */
	TArray<double> RequestTimestamps;
	
	/** Pre-generation state */
	bool bIsPreGenerating = false;
	TArray<FString> PreGenQueue;
	int32 PreGenCompleted = 0;
	int32 PreGenTotal = 0;
	double PreGenStartTime = 0.0;
};
