#include "RiverAudioFollowComponent.h"

#include "Components/AudioComponent.h"
#include "Components/SplineComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogRiverAudio, Log, All);

URiverAudioFollowComponent::URiverAudioFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = true;
}

void URiverAudioFollowComponent::BeginPlay()
{
	Super::BeginPlay();

	PrimaryComponentTick.TickInterval = UpdateInterval;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// UWaterSplineComponent derives from USplineComponent, so this finds the
	// river's spline without linking against the Water module.
	Spline = Owner->FindComponentByClass<USplineComponent>();
	if (!Spline)
	{
		UE_LOG(LogRiverAudio, Warning,
			TEXT("%s: no USplineComponent on owner — river audio will not follow"),
			*Owner->GetName());
		return;
	}

	// The plugin's own emitter sits at the actor origin; leaving it running
	// would double the sound from a fixed point.
	if (bSilencePluginRiverAudio)
	{
		TArray<UAudioComponent*> Existing;
		Owner->GetComponents<UAudioComponent>(Existing);
		for (UAudioComponent* Comp : Existing)
		{
			if (Comp && Comp != AudioComp)
			{
				Comp->Stop();
				Comp->SetAutoActivate(false);
				UE_LOG(LogRiverAudio, Verbose, TEXT("%s: silenced '%s'"),
					*Owner->GetName(), *Comp->GetName());
			}
		}
	}

	if (!RiverSound)
	{
		UE_LOG(LogRiverAudio, Warning, TEXT("%s: no RiverSound assigned"),
			*Owner->GetName());
		return;
	}

	AudioComp = NewObject<UAudioComponent>(Owner);
	if (!AudioComp)
	{
		return;
	}
	AudioComp->bAutoActivate = false;
	AudioComp->bAllowSpatialization = true;
	AudioComp->bStopWhenOwnerDestroyed = true;
	AudioComp->SetSound(RiverSound);
	if (Attenuation)
	{
		AudioComp->AttenuationSettings = Attenuation;
		AudioComp->bOverrideAttenuation = false;
	}
	AudioComp->RegisterComponent();

	// Start it where the listener is now, so it does not fade in from the origin.
	FVector Listener;
	const FVector Start = (GetListenerLocation(Listener) && Spline)
		? Spline->FindLocationClosestToWorldLocation(Listener, ESplineCoordinateSpace::World)
		: Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	AudioComp->SetWorldLocation(Start);
	AudioComp->Play();
}

void URiverAudioFollowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AudioComp)
	{
		AudioComp->Stop();
		AudioComp->DestroyComponent();
		AudioComp = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void URiverAudioFollowComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!AudioComp || !Spline)
	{
		return;
	}

	FVector Listener;
	if (!GetListenerLocation(Listener))
	{
		return;
	}

	const FVector Closest =
		Spline->FindLocationClosestToWorldLocation(Listener, ESplineCoordinateSpace::World);

	// Far away the exact emitter position cannot matter — it is already silent.
	if (MaxUpdateDistance > 0.0f &&
		FVector::DistSquared(Listener, Closest) > FMath::Square(MaxUpdateDistance))
	{
		return;
	}

	AudioComp->SetWorldLocation(Closest);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Closest, 120.0f, 12, FColor::Cyan, false,
			UpdateInterval * 1.1f);
		DrawDebugLine(GetWorld(), Closest, Listener, FColor::Cyan, false,
			UpdateInterval * 1.1f);
	}
}

FVector URiverAudioFollowComponent::GetEmitterLocation() const
{
	return AudioComp ? AudioComp->GetComponentLocation() : GetComponentLocation();
}

bool URiverAudioFollowComponent::GetListenerLocation(FVector& OutLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (PC->PlayerCameraManager)
		{
			OutLocation = PC->PlayerCameraManager->GetCameraLocation();
			return true;
		}
		if (const APawn* Pawn = PC->GetPawn())
		{
			OutLocation = Pawn->GetActorLocation();
			return true;
		}
	}
	return false;
}
