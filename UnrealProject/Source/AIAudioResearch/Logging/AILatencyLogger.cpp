// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "LatencyLogger.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

ULatencyLogger::ULatencyLogger()
{
}

void ULatencyLogger::LogLatency(const FString& SoundId, const FString& ModelName, float LatencyMs, bool bFromCache)
{
	FLatencyEntry Entry;
	Entry.SoundId = SoundId;
	Entry.ModelName = ModelName;
	Entry.LatencyMs = LatencyMs;
	Entry.Timestamp = FDateTime::Now();
	Entry.bFromCache = bFromCache;
	
	Entries.Add(Entry);
	
	UE_LOG(LogTemp, Log, TEXT("LatencyLogger: %s | %s | %.2f ms | Cache: %s"), 
		*SoundId, *ModelName, LatencyMs, bFromCache ? TEXT("Yes") : TEXT("No"));
}

bool ULatencyLogger::ExportToCSV(const FString& FilePath)
{
	FString CSVContent;
	
	// Header
	CSVContent += TEXT("Timestamp,SoundId,ModelName,LatencyMs,FromCache\n");
	
	// Data rows
	for (const FLatencyEntry& Entry : Entries)
	{
		CSVContent += FString::Printf(TEXT("%s,%s,%s,%.4f,%s\n"),
			*Entry.Timestamp.ToString(),
			*Entry.SoundId,
			*Entry.ModelName,
			Entry.LatencyMs,
			Entry.bFromCache ? TEXT("true") : TEXT("false"));
	}
	
	if (FFileHelper::SaveStringToFile(CSVContent, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("LatencyLogger: Exported %d entries to %s"), Entries.Num(), *FilePath);
		return true;
	}
	
	UE_LOG(LogTemp, Error, TEXT("LatencyLogger: Failed to export to %s"), *FilePath);
	return false;
}

void ULatencyLogger::ClearLogs()
{
	Entries.Empty();
	UE_LOG(LogTemp, Log, TEXT("LatencyLogger: Cleared all entries"));
}

int32 ULatencyLogger::GetLogCount() const
{
	return Entries.Num();
}

float ULatencyLogger::GetAverageLatency(const FString& ModelName) const
{
	float Sum = 0.0f;
	int32 Count = 0;
	
	for (const FLatencyEntry& Entry : Entries)
	{
		if (Entry.ModelName == ModelName && !Entry.bFromCache)
		{
			Sum += Entry.LatencyMs;
			Count++;
		}
	}
	
	return Count > 0 ? Sum / Count : 0.0f;
}

TArray<FLatencyEntry> ULatencyLogger::GetAllEntries() const
{
	return Entries;
}

void ULatencyLogger::GetStatistics(const FString& ModelName, float& OutMin, float& OutMax, float& OutMean, float& OutStdDev) const
{
	TArray<float> Values;
	
	for (const FLatencyEntry& Entry : Entries)
	{
		if (Entry.ModelName == ModelName && !Entry.bFromCache)
		{
			Values.Add(Entry.LatencyMs);
		}
	}
	
	if (Values.Num() == 0)
	{
		OutMin = OutMax = OutMean = OutStdDev = 0.0f;
		return;
	}
	
	// Calculate min, max, mean
	OutMin = FLT_MAX;
	OutMax = -FLT_MAX;
	float Sum = 0.0f;
	
	for (float Val : Values)
	{
		OutMin = FMath::Min(OutMin, Val);
		OutMax = FMath::Max(OutMax, Val);
		Sum += Val;
	}
	
	OutMean = Sum / Values.Num();
	
	// Calculate standard deviation
	float SumSquaredDiff = 0.0f;
	for (float Val : Values)
	{
		SumSquaredDiff += FMath::Square(Val - OutMean);
	}
	
	OutStdDev = FMath::Sqrt(SumSquaredDiff / Values.Num());
}
