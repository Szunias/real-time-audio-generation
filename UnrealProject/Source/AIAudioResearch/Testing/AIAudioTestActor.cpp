// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AudioTestActor.h"
#include "AIAudioManager.h"
#include "LatencyLogger.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"

AAudioTestActor::AAudioTestActor()
	: LastLatency(0.0f)
	, TotalRequests(0)
	, PlaybackPulseAlpha(0.0f)
	, TimeSinceLastSound(0.0f)
	, bIsPlaying(false)
{
	PrimaryActorTick.bCanEverTick = true;
	
	// Create audio component
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	RootComponent = AudioComponent;
}

void AAudioTestActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Set default CSV path if not specified
	if (CSVExportPath.IsEmpty())
	{
		CSVExportPath = FPaths::ProjectSavedDir() / TEXT("LatencyLogs") / 
			FString::Printf(TEXT("latency_%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	}
	
	// Create managers
	AudioManager = NewObject<UAudioManager>(this);
	LatencyLogger = NewObject<ULatencyLogger>(this);
	
	// Bind events
	AudioManager->OnSoundReady.AddDynamic(this, &AAudioTestActor::OnSoundReady);
	AudioManager->OnLatencyMeasured.AddDynamic(this, &AAudioTestActor::OnLatencyMeasured);
	
	// Connect to server
	AudioManager->Initialize(ServerURL);
	
	UE_LOG(LogTemp, Log, TEXT("AudioTestActor: Initialized. Press 1-4 to test sounds, L to export logs."));
	
	// Enable input
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		EnableInput(PC);
	}
}

void AAudioTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Auto-export logs on exit
	if (LatencyLogger && LatencyLogger->GetLogCount() > 0)
	{
		ExportLogs();
	}
	
	if (AudioManager)
	{
		AudioManager->Shutdown();
	}
	
	Super::EndPlay(EndPlayReason);
}

void AAudioTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Update visual effects
	TimeSinceLastSound += DeltaTime;
	
	// Fade out pulse effect
	if (PlaybackPulseAlpha > 0.0f)
	{
		PlaybackPulseAlpha = FMath::Max(0.0f, PlaybackPulseAlpha - DeltaTime * 3.0f);
	}
	
	// Check for key presses
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		if (PC->WasInputKeyJustPressed(EKeys::One))
		{
			RequestTestSound(TestPrompt1);
		}
		else if (PC->WasInputKeyJustPressed(EKeys::Two))
		{
			RequestTestSound(TestPrompt2);
		}
		else if (PC->WasInputKeyJustPressed(EKeys::Three))
		{
			RequestTestSound(TestPrompt3);
		}
		else if (PC->WasInputKeyJustPressed(EKeys::Four))
		{
			RequestTestSound(TestPrompt4);
		}
		else if (PC->WasInputKeyJustPressed(EKeys::L))
		{
			ExportLogs();
		}
	}
	
	// Draw visual UI
	if (bShowDebugInfo)
	{
		DrawVisualUI();
	}
}

void AAudioTestActor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Input is handled in Tick for simplicity
}

void AAudioTestActor::RequestTestSound(const FString& Prompt)
{
	if (!AudioManager)
	{
		return;
	}
	
	TotalRequests++;
	LastSoundId = Prompt;
	TimeSinceLastSound = 0.0f;
	
	UE_LOG(LogTemp, Log, TEXT("AudioTestActor: Requesting '%s'"), *Prompt);
	AudioManager->RequestSound(Prompt);
}

void AAudioTestActor::OnSoundReady(const FString& SoundId, USoundWaveProcedural* Sound)
{
	if (AudioComponent && Sound)
	{
		AudioComponent->SetSound(Sound);
		AudioComponent->Play();
		
		// Trigger visual pulse
		PlaybackPulseAlpha = 1.0f;
		bIsPlaying = true;
		
		UE_LOG(LogTemp, Log, TEXT("AudioTestActor: Playing '%s'"), *SoundId);
	}
}

void AAudioTestActor::OnLatencyMeasured(const FString& SoundId, float LatencyMs)
{
	LastLatency = LatencyMs;
	
	// Add to history (keep last 20)
	LatencyHistory.Add(LatencyMs);
	if (LatencyHistory.Num() > 20)
	{
		LatencyHistory.RemoveAt(0);
	}
	
	// Log to logger
	if (LatencyLogger)
	{
		bool bFromCache = (LatencyMs < 1.0f);
		LatencyLogger->LogLatency(SoundId, CurrentModelName, LatencyMs, bFromCache);
	}
}

FColor AAudioTestActor::GetLatencyColor(float LatencyMs)
{
	if (LatencyMs < 1.0f) return FColor::Cyan;      // Cached
	if (LatencyMs < 50.0f) return FColor::Green;    // Excellent
	if (LatencyMs < 100.0f) return FColor::Yellow;  // Good
	if (LatencyMs < 200.0f) return FColor::Orange;  // Acceptable
	return FColor::Red;                              // Too slow
}

void AAudioTestActor::DrawVisualUI()
{
	if (!GEngine) return;
	
	// ===== HEADER =====
	FString HeaderText = TEXT("╔══════════════════════════════════════════╗");
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, HeaderText);
	
	FString TitleText = TEXT("║     🔊 AI AUDIO RESEARCH - DEMO 🔊       ║");
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, TitleText);
	
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, TEXT("╠══════════════════════════════════════════╣"));
	
	// ===== CONNECTION STATUS =====
	bool bConnected = AudioManager && AudioManager->IsConnected();
	FString ConnStatus = FString::Printf(TEXT("║ Status: %s                   ║"), 
		bConnected ? TEXT("🟢 CONNECTED") : TEXT("🔴 DISCONNECTED"));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, bConnected ? FColor::Green : FColor::Red, ConnStatus);
	
	// ===== MODEL INFO =====
	FString ModelText = FString::Printf(TEXT("║ Model: %-20s           ║"), *CurrentModelName);
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, ModelText);
	
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, TEXT("╠══════════════════════════════════════════╣"));
	
	// ===== LAST SOUND =====
	FString SoundText = FString::Printf(TEXT("║ Last Sound: %-20s       ║"), *LastSoundId);
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, SoundText);
	
	// ===== LATENCY WITH COLOR BAR =====
	FColor LatencyColor = GetLatencyColor(LastLatency);
	FString LatencyText;
	if (LastLatency < 1.0f && LastLatency >= 0.0f && TotalRequests > 0)
	{
		LatencyText = TEXT("║ Latency: [CACHED] ⚡ 0.00ms             ║");
	}
	else
	{
		// Create visual bar
		int32 BarLength = FMath::Clamp((int32)(LastLatency / 10.0f), 0, 20);
		FString Bar = TEXT("");
		for (int32 i = 0; i < BarLength; i++) Bar += TEXT("█");
		for (int32 i = BarLength; i < 20; i++) Bar += TEXT("░");
		
		LatencyText = FString::Printf(TEXT("║ Latency: [%s] %.1fms   ║"), *Bar, LastLatency);
	}
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, LatencyColor, LatencyText);
	
	// ===== STATS =====
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, TEXT("╠══════════════════════════════════════════╣"));
	
	FString StatsText = FString::Printf(TEXT("║ Requests: %-4d    Cache: %-4d            ║"), 
		TotalRequests, AudioManager ? AudioManager->GetCacheSize() : 0);
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, StatsText);
	
	// ===== LATENCY HISTORY GRAPH =====
	if (LatencyHistory.Num() > 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, TEXT("╠══════════════════════════════════════════╣"));
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, TEXT("║ Latency History:                         ║"));
		
		// Simple text-based graph
		FString GraphLine = TEXT("║ ");
		for (int32 i = 0; i < LatencyHistory.Num(); i++)
		{
			float Val = LatencyHistory[i];
			if (Val < 1.0f) GraphLine += TEXT("_"); // Cached
			else if (Val < 50.0f) GraphLine += TEXT("▁");
			else if (Val < 100.0f) GraphLine += TEXT("▄");
			else if (Val < 150.0f) GraphLine += TEXT("▆");
			else GraphLine += TEXT("█");
		}
		// Pad to width
		while (GraphLine.Len() < 44) GraphLine += TEXT(" ");
		GraphLine += TEXT("║");
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Emerald, GraphLine);
	}
	
	// ===== PLAYBACK PULSE =====
	if (PlaybackPulseAlpha > 0.1f)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, TEXT("╠══════════════════════════════════════════╣"));
		FColor PulseColor = FColor(0, 255, 128, (uint8)(PlaybackPulseAlpha * 255));
		FString PulseText = TEXT("║       ▶▶▶  NOW PLAYING  ◀◀◀              ║");
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, PulseColor, PulseText);
	}
	
	// ===== CONTROLS =====
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, TEXT("╠══════════════════════════════════════════╣"));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Silver, TEXT("║ [1] Footstep Wood   [2] Footstep Concrete║"));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Silver, TEXT("║ [3] Gunshot         [4] Impact Metal     ║"));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Silver, TEXT("║ [L] Export Logs                          ║"));
	
	// ===== FOOTER =====
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, TEXT("╚══════════════════════════════════════════╝"));
}

void AAudioTestActor::ExportLogs()
{
	if (LatencyLogger)
	{
		// Ensure directory exists
		FString Directory = FPaths::GetPath(CSVExportPath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*Directory);
		
		if (LatencyLogger->ExportToCSV(CSVExportPath))
		{
			UE_LOG(LogTemp, Log, TEXT("AudioTestActor: Exported logs to %s"), *CSVExportPath);
			
			// Get statistics
			float Min, Max, Mean, StdDev;
			LatencyLogger->GetStatistics(CurrentModelName, Min, Max, Mean, StdDev);
			
			UE_LOG(LogTemp, Log, TEXT("Statistics for %s: Min=%.2f, Max=%.2f, Mean=%.2f, StdDev=%.2f"),
				*CurrentModelName, Min, Max, Mean, StdDev);
		}
	}
}
