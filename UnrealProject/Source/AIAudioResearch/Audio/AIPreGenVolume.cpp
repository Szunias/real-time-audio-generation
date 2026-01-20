// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIPreGenVolume.h"
#include "Components/BoxComponent.h"
#include "AIAmbienceComponent.h"

AAIPreGenVolume::AAIPreGenVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create trigger box
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	RootComponent = Bounds;
	Bounds->SetBoxExtent(FVector(600.0f, 600.0f, 600.0f)); // Slightly larger default
	Bounds->SetCollisionProfileName(TEXT("Trigger"));

	// Visual settings for editor
	Bounds->SetHiddenInGame(false);
	Bounds->SetVisibility(true);
	Bounds->ShapeColor = FColor(255, 255, 0); // Yellow color for pre-gen zones
	Bounds->SetLineThickness(2.0f);
}

void AAIPreGenVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (OtherActor && !PromptToPreGen.IsEmpty())
	{
		UAIAmbienceComponent* AmbienceComp = OtherActor->FindComponentByClass<UAIAmbienceComponent>();
		if (AmbienceComp)
		{
			AmbienceComp->PreCacheAmbience(PromptToPreGen);
		}
	}
}
