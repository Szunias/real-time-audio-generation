// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LatencyLogger.generated.h"

/**
 * Single latency measurement entry
 */
USTRUCT(BlueprintType)
struct FLatencyEntry
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	FString SoundId;
	
	UPROPERTY(BlueprintReadOnly)
	FString ModelName;
	
	UPROPERTY(BlueprintReadOnly)
	float LatencyMs;
	
	UPROPERTY(BlueprintReadOnly)
	FDateTime Timestamp;
	
	UPROPERTY(BlueprintReadOnly)
	bool bFromCache;
	
	FLatencyEntry()
		: LatencyMs(0.0f)
		, bFromCache(false)
	{}
};

/**
 * Logs latency measurements to CSV for analysis
 */
UCLASS(Blueprintable, BlueprintType)
class AIAUDIORESEARCH_API ULatencyLogger : public UObject
{
	GENERATED_BODY()

public:
	ULatencyLogger();
	
	/** Log a latency measurement */
	UFUNCTION(BlueprintCallable, Category = "AI Audio|Logging")
	void LogLatency(const FString& SoundId, const FString& ModelName, float LatencyMs, bool bFromCache = false);
	
	/** Export all logs to CSV file */
	UFUNCTION(BlueprintCallable, Category = "AI Audio|Logging")
	bool ExportToCSV(const FString& FilePath);
	
	/** Clear all logged entries */
	UFUNCTION(BlueprintCallable, Category = "AI Audio|Logging")
	void ClearLogs();
	
	/** Get number of logged entries */
	UFUNCTION(BlueprintPure, Category = "AI Audio|Logging")
	int32 GetLogCount() const;
	
	/** Get average latency for a model */
	UFUNCTION(BlueprintPure, Category = "AI Audio|Logging")
	float GetAverageLatency(const FString& ModelName) const;
	
	/** Get all entries for analysis */
	UFUNCTION(BlueprintCallable, Category = "AI Audio|Logging")
	TArray<FLatencyEntry> GetAllEntries() const;
	
	/** Get statistical summary */
	UFUNCTION(BlueprintCallable, Category = "AI Audio|Logging")
	void GetStatistics(const FString& ModelName, float& OutMin, float& OutMax, float& OutMean, float& OutStdDev) const;

private:
	UPROPERTY()
	TArray<FLatencyEntry> Entries;
};
