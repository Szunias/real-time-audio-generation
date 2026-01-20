// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIPreGenVolume.generated.h"

class UBoxComponent;

/**
 * Volume that triggers pre-generation (pre-caching) of an ambience prompt when player enters.
 * Place this BEFORE the actual AmbienceVolume so the sound is ready when needed.
 */
UCLASS()
class AIAUDIORESEARCH_API AAIPreGenVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	AAIPreGenVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Ambience", meta = (MakeEditWidget = true))
	UBoxComponent* Bounds;

	/** Prompt to pre-generate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Ambience")
	FString PromptToPreGen = TEXT("dark castle ambience");
	
protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
};
