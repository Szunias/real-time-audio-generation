// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIAudioManager.h"
#include "AIWebSocketClient.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"

UAIAudioManager::UAIAudioManager()
{
	// TArray members auto-initialize
}

UAIAudioManager::~UAIAudioManager()
{
	Shutdown();
}

void UAIAudioManager::Initialize(const FString& ServerURL)
{
	if (WebSocketClient.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAudioManager: Already initialized"));
		return;
	}
	
	WebSocketClient = MakeShared<FAIWebSocketClient>();
	
	// Bind native delegates using lambdas (captures this pointer)
	WebSocketClient->OnAudioDataReceived.BindLambda([this](const TArray<uint8>& AudioData)
	{
		HandleAudioDataReceived(AudioData);
	});
	
	WebSocketClient->OnConnected.BindLambda([this]()
	{
		HandleWebSocketConnected();
	});
	
	WebSocketClient->OnError.BindLambda([this](const FString& Error)
	{
		HandleWebSocketError(Error);
	});
	
	WebSocketClient->Connect(ServerURL);
	
	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Initialized, connecting to %s"), *ServerURL);
}

void UAIAudioManager::Shutdown()
{
	if (WebSocketClient.IsValid())
	{
		WebSocketClient->OnAudioDataReceived.Unbind();
		WebSocketClient->OnConnected.Unbind();
		WebSocketClient->OnError.Unbind();
		
		WebSocketClient->Close();
		WebSocketClient.Reset();
	}
	
	ClearCache();
	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Shutdown complete"));
}

void UAIAudioManager::RequestSound(const FString& Prompt, const FString& SoundId, float Duration, const FString& Model)
{
	if (!WebSocketClient.IsValid() || !WebSocketClient->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAudioManager: Cannot request sound - not connected"));
		return;
	}
	
	// Generate ID if not provided
	FString Id = SoundId.IsEmpty() ? Prompt : SoundId;
	
	// Check cache first
	if (SoundCache.Contains(Id))
	{
		UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Sound '%s' already in cache"), *Id);
		
		// Create new procedural sound from cached PCM data for replay
		if (PCMDataCache.Contains(Id))
		{
			USoundWaveProcedural* CachedSound = CreateProceduralSound(PCMDataCache[Id]);
			if (CachedSound)
			{
				OnSoundReady.Broadcast(Id, CachedSound);
			}
		}
		OnLatencyMeasured.Broadcast(Id, 0.0f); // Cached = 0 latency
		return;
	}
	
	// Queue this request ID (FIFO order)
	PendingRequestQueue.Add(Id);
	RequestTimestamps.Add(FPlatformTime::Seconds());

	WebSocketClient->RequestSound(Prompt, Model, Duration);
	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Requested sound '%s' model:'%s' (%.1fs) [Queue size: %d]"), *Prompt, *Model, Duration, PendingRequestQueue.Num());

	// Show what's being generated (bottom-left)
	if (GEngine)
	{
		FString GenMsg = FString::Printf(TEXT("gen: %s"), *Prompt.Left(20));
		GEngine->AddOnScreenDebugMessage(103, 0.0f, FColor(150, 150, 150), *GenMsg);
	}
}

void UAIAudioManager::PlaySound(const FString& SoundId, UAudioComponent* AudioComponent)
{
	if (!AudioComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAudioManager: No audio component provided"));
		return;
	}
	
	if (!PCMDataCache.Contains(SoundId))
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAudioManager: Sound '%s' not in cache"), *SoundId);
		return;
	}
	
	// Create fresh procedural sound for playback
	USoundWaveProcedural* Sound = CreateProceduralSound(PCMDataCache[SoundId]);
	if (Sound)
	{
		AudioComponent->SetSound(Sound);
		AudioComponent->Play();
		UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Playing sound '%s'"), *SoundId);
	}
}

void UAIAudioManager::PreWarmCache(const TArray<FString>& Prompts)
{
	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Pre-warming cache with %d sounds"), Prompts.Num());
	
	for (const FString& Prompt : Prompts)
	{
		RequestSound(Prompt);
	}
}

void UAIAudioManager::PreGenerateForLevel(const TArray<FString>& SoundPrompts)
{
	if (bIsPreGenerating)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAudioManager: Pre-generation already in progress"));
		return;
	}
	
	if (!IsConnected())
	{
		UE_LOG(LogTemp, Error, TEXT("AIAudioManager: Cannot pre-generate - not connected to server"));
		return;
	}
	
	// Filter out already cached sounds
	PreGenQueue.Empty();
	for (const FString& Prompt : SoundPrompts)
	{
		if (!IsSoundCached(Prompt))
		{
			PreGenQueue.Add(Prompt);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("AIAudioManager: '%s' already cached, skipping"), *Prompt);
		}
	}
	
	if (PreGenQueue.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("AIAudioManager: All sounds already cached!"));
		OnPreGenerationComplete.Broadcast(0.0f);
		return;
	}
	
	// Start pre-generation
	bIsPreGenerating = true;
	PreGenTotal = PreGenQueue.Num();
	PreGenCompleted = 0;
	PreGenStartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Starting pre-generation of %d sounds"), PreGenTotal);
	
	// Request first sound (sequential to avoid overloading server)
	if (PreGenQueue.Num() > 0)
	{
		FString NextPrompt = PreGenQueue[0];
		PreGenQueue.RemoveAt(0);
		RequestSound(NextPrompt);
	}
	
	OnPreGenerationProgress.Broadcast(0, PreGenTotal);
}

float UAIAudioManager::GetPreGenerationProgress() const
{
	if (PreGenTotal == 0) return 1.0f;
	return static_cast<float>(PreGenCompleted) / static_cast<float>(PreGenTotal);
}

bool UAIAudioManager::IsSoundCached(const FString& SoundId) const
{
	return SoundCache.Contains(SoundId);
}

int32 UAIAudioManager::GetCacheSize() const
{
	return SoundCache.Num();
}

void UAIAudioManager::ClearCache()
{
	SoundCache.Empty();
	PCMDataCache.Empty();
	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Cache cleared"));
}

bool UAIAudioManager::IsConnected() const
{
	return WebSocketClient.IsValid() && WebSocketClient->IsConnected();
}

void UAIAudioManager::HandleAudioDataReceived(const TArray<uint8>& AudioData)
{
	// Safety check - prevent crash on empty data
	if (AudioData.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("AIAudioManager: Received empty audio data!"));
		return;
	}
	
	// Get the oldest pending request from the FIFO queue
	if (PendingRequestQueue.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("AIAudioManager: Received audio but no pending request in queue!"));
		return;
	}
	
	// Dequeue: get first element and remove it
	FString CurrentRequestId = PendingRequestQueue[0];
	double CurrentRequestTimestamp = RequestTimestamps.Num() > 0 ? RequestTimestamps[0] : FPlatformTime::Seconds();
	PendingRequestQueue.RemoveAt(0);
	if (RequestTimestamps.Num() > 0) RequestTimestamps.RemoveAt(0);
	
	if (AudioData.Num() < 100)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAudioManager: Received unusually small audio data: %d bytes"), AudioData.Num());
	}
	
	double ReceiveTime = FPlatformTime::Seconds();
	float LatencyMs = (ReceiveTime - CurrentRequestTimestamp) * 1000.0f;

	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Received audio data, %d bytes, latency: %.2f ms"),
		AudioData.Num(), LatencyMs);

	// Show latency on screen (top-right) - fades quickly
	if (GEngine)
	{
		FString LatencyMsg = FString::Printf(TEXT("%.0fms"), LatencyMs);
		GEngine->AddOnScreenDebugMessage(102, 2.0f, FColor(0, 200, 255), *LatencyMsg);
	}

	// Store raw PCM data in cache for replay
	PCMDataCache.Add(CurrentRequestId, AudioData);
	
	// Create SoundWaveProcedural from PCM data
	USoundWaveProcedural* SoundWave = CreateProceduralSound(AudioData);
	
	if (SoundWave)
	{
		// Add to cache
		SoundCache.Add(CurrentRequestId, SoundWave);
		
		// Broadcast events
		OnSoundReady.Broadcast(CurrentRequestId, SoundWave);
		OnLatencyMeasured.Broadcast(CurrentRequestId, LatencyMs);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AIAudioManager: Failed to create SoundWave from data"));
	}
	
	// Handle pre-generation queue continuation
	if (bIsPreGenerating)
	{
		PreGenCompleted++;
		OnPreGenerationProgress.Broadcast(PreGenCompleted, PreGenTotal);
		
		UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Pre-generation progress: %d/%d"), PreGenCompleted, PreGenTotal);
		
		if (PreGenQueue.Num() > 0)
		{
			// Request next sound in queue
			FString NextPrompt = PreGenQueue[0];
			PreGenQueue.RemoveAt(0);
			RequestSound(NextPrompt);
		}
		else
		{
			// Pre-generation complete!
			bIsPreGenerating = false;
			float TotalTime = static_cast<float>(FPlatformTime::Seconds() - PreGenStartTime);
			UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Pre-generation COMPLETE! Total time: %.2f seconds"), TotalTime);
			OnPreGenerationComplete.Broadcast(TotalTime);
		}
	}
}

void UAIAudioManager::HandleWebSocketConnected()
{
	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Connected to server"));

	// Show connection status on screen (top-right)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(101, 30.0f, FColor(0, 255, 0), TEXT("[connected]"));
	}
}

void UAIAudioManager::HandleWebSocketError(const FString& Error)
{
	UE_LOG(LogTemp, Error, TEXT("AIAudioManager: WebSocket error - %s"), *Error);

	// Show error on screen (top-right)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(101, 5.0f, FColor(255, 0, 0), TEXT("[server error]"));
	}
}

USoundWaveProcedural* UAIAudioManager::CreateProceduralSound(const TArray<uint8>& PCMData, int32 SampleRate, int32 NumChannels)
{
	// Safety checks
	if (PCMData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAudioManager: Cannot create sound - empty PCM data"));
		return nullptr;
	}
	
	if (PCMData.Num() < 100) // Minimum sensible audio size
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAudioManager: PCM data too small (%d bytes)"), PCMData.Num());
		return nullptr;
	}
	
	// Create procedural sound wave
	USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>(this);
	if (!SoundWave)
	{
		UE_LOG(LogTemp, Error, TEXT("AIAudioManager: Failed to create SoundWaveProcedural object"));
		return nullptr;
	}
	
	// Configure format
	SoundWave->SetSampleRate(SampleRate);
	SoundWave->NumChannels = NumChannels;
	SoundWave->Duration = (float)PCMData.Num() / (SampleRate * NumChannels * sizeof(int16));
	SoundWave->bLooping = false;  // Looping handled externally for procedural audio
	SoundWave->bCanProcessAsync = true;
	
	// Queue the audio data for playback - use try/catch for safety
	SoundWave->QueueAudio(PCMData.GetData(), PCMData.Num());
	
	UE_LOG(LogTemp, Log, TEXT("AIAudioManager: Created procedural sound, duration: %.2f sec, size: %d bytes"), 
		SoundWave->Duration, PCMData.Num());
	
	return SoundWave;
}
