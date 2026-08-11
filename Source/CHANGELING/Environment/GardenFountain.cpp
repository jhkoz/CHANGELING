// GardenFountain.cpp

#include "GardenFountain.h"

#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFountain, Log, All);

AGardenFountain::AGardenFountain()
{
	// Nothing polls. Particle counts are applied on construction, audio on overlap.
	PrimaryActorTick.bCanEverTick = false;

	FountainMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FountainMesh"));
	SetRootComponent(FountainMesh);

	StreamEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("StreamEffect"));
	StreamEffect->SetupAttachment(FountainMesh);

	SplashEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SplashEffect"));
	SplashEffect->SetupAttachment(FountainMesh);

	AudioTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("AudioTrigger"));
	AudioTrigger->SetupAttachment(FountainMesh);
	AudioTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AudioTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	AudioTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AudioTrigger->SetGenerateOverlapEvents(true);

	// bAutoActivate off: the loops start on overlap, otherwise every fountain in the
	// level is playing from the moment it streams in.
	WaterLoopNear = CreateDefaultSubobject<UAudioComponent>(TEXT("WaterLoopNear"));
	WaterLoopNear->SetupAttachment(FountainMesh);
	WaterLoopNear->bAutoActivate = false;

	WaterLoopFar = CreateDefaultSubobject<UAudioComponent>(TEXT("WaterLoopFar"));
	WaterLoopFar->SetupAttachment(FountainMesh);
	WaterLoopFar->bAutoActivate = false;
}

void AGardenFountain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyParticleCount();
	ApplyTriggerRadius();
	ApplyAudioResponse();
}

void AGardenFountain::BeginPlay()
{
	Super::BeginPlay();

	if (AudioTrigger)
	{
		AudioTrigger->OnComponentBeginOverlap.AddDynamic(this, &AGardenFountain::HandleTriggerBegin);
		AudioTrigger->OnComponentEndOverlap.AddDynamic(this, &AGardenFountain::HandleTriggerEnd);
	}

	// Re-apply at runtime too: a fountain spawned rather than placed never ran
	// OnConstruction with its final ParticlesPerSecond if that was set on spawn.
	ApplyParticleCount();
	ApplyAudioResponse();
}

void AGardenFountain::SetParticlesPerSecond(int32 NewCount)
{
	ParticlesPerSecond = FMath::Max(0, NewCount);
	ApplyParticleCount();
	ApplyAudioResponse();
}

void AGardenFountain::ApplyParticleCount()
{
	// Written to BOTH systems from one value. Authoring the stream and the splashes
	// with separate counts is how a fountain ends up with a heavy jet and a token
	// spatter, or the reverse, with no obvious cause.
	if (ParticleCountParameter.IsNone())
	{
		return;
	}

	if (StreamEffect)
	{
		StreamEffect->SetVariableInt(ParticleCountParameter, ParticlesPerSecond);
	}
	if (SplashEffect)
	{
		SplashEffect->SetVariableInt(ParticleCountParameter, ParticlesPerSecond);
	}
}

void AGardenFountain::ApplyTriggerRadius()
{
	if (!AudioTrigger || !FountainMesh)
	{
		return;
	}

	const UStaticMesh* Mesh = FountainMesh->GetStaticMesh();
	if (!Mesh)
	{
		// No mesh yet (a freshly placed actor before assignment). Leave whatever
		// radius is there rather than collapsing it to zero and silencing the prop.
		return;
	}

	// Derived from the mesh, not typed in: rescaling the fountain would otherwise
	// leave a hand-authored radius behind, and the mismatch is invisible until you
	// notice the water is audible from the wrong distance.
	const float Extent = Mesh->GetBounds().BoxExtent.Size();
	AudioTrigger->SetSphereRadius(FMath::Max(1.0f, Extent * TriggerRadiusScale));
}

void AGardenFountain::ApplyAudioResponse()
{
	// A fountain running below baseline is thinner and reads higher and quieter; one
	// running above is broader, louder, and no higher in pitch. Normalised against
	// the baseline so the response is proportional rather than absolute.
	const float Baseline = static_cast<float>(FMath::Max(1, BaselineParticles));
	const float Shift =
		((static_cast<float>(ParticlesPerSecond) - Baseline) / Baseline) * AudioResponseStrength;

	// Pitch moves opposite the count and only upward — see PitchRange's comment.
	const float Pitch = FMath::Clamp(1.0f - Shift,
		static_cast<float>(PitchRange.X), static_cast<float>(PitchRange.Y));

	// Volume follows the count, at twice the rate, in both directions.
	const float Volume = FMath::Clamp(1.0f + Shift * 2.0f,
		static_cast<float>(VolumeRange.X), static_cast<float>(VolumeRange.Y));

	for (UAudioComponent* Loop : { WaterLoopNear.Get(), WaterLoopFar.Get() })
	{
		if (Loop)
		{
			Loop->SetPitchMultiplier(Pitch);
			Loop->SetVolumeMultiplier(Volume);
		}
	}
}

bool AGardenFountain::IsLocalPlayerPawn(const AActor* Other) const
{
	// Deliberately a pawn/controller test rather than a cast to one character class:
	// a cast breaks the moment the player character is swapped or subclassed, and it
	// fails silently — the fountain simply goes mute with nothing in the log.
	const APawn* Pawn = Cast<APawn>(Other);
	return Pawn && Pawn->IsPlayerControlled();
}

void AGardenFountain::HandleTriggerBegin(UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!IsLocalPlayerPawn(OtherActor))
	{
		return;
	}

	// Random start offsets keep two fountains within earshot from phase-locking into
	// one obviously looping sound.
	for (UAudioComponent* Loop : { WaterLoopNear.Get(), WaterLoopFar.Get() })
	{
		if (Loop && !Loop->IsPlaying())
		{
			Loop->Play(FMath::FRandRange(
				static_cast<float>(StartOffsetRange.X), static_cast<float>(StartOffsetRange.Y)));
		}
	}
}

void AGardenFountain::HandleTriggerEnd(UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!bStopAudioOnExit || !IsLocalPlayerPawn(OtherActor))
	{
		return;
	}

	for (UAudioComponent* Loop : { WaterLoopNear.Get(), WaterLoopFar.Get() })
	{
		if (Loop && Loop->IsPlaying())
		{
			Loop->Stop();
		}
	}
}
