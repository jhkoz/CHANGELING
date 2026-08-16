// FireDecalActor.cpp

#include "FireDecalActor.h"

#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFireDecal, Log, All);
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

AFireDecalActor::AFireDecalActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Shallow and small. The default decal box is a metre-plus cube, which projects a
	// scorch onto every surface within it -- including the far side of a wall and the
	// underside of anything overhead.
	if (UDecalComponent* DecalComp = GetDecal())
	{
		DecalComp->DecalSize = FVector(32.0f, 128.0f, 128.0f);
	}

	// Matches the decal's own extents, so "the trace found this mark" means the same
	// thing as "the mark covers that spot".
	BurnBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("BurnBounds"));
	BurnBounds->SetupAttachment(GetRootComponent());
	BurnBounds->SetBoxExtent(FVector(32.0f, 128.0f, 128.0f));
	BurnBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BurnBounds->SetCollisionObjectType(ECC_WorldStatic);
	BurnBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	BurnBounds->SetCollisionResponseToChannel(DecalTraceChannel, ECR_Block);
	BurnBounds->SetGenerateOverlapEvents(false);
	BurnBounds->CanCharacterStepUpOn = ECB_No;
}

void AFireDecalActor::BeginPlay()
{
	Super::BeginPlay();

	// Re-applied because the channel is editable per Blueprint, and a value changed
	// there arrives after the constructor has already set the response.
	if (BurnBounds)
	{
		BurnBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
		BurnBounds->SetCollisionResponseToChannel(DecalTraceChannel, ECR_Block);
	}

	UDecalComponent* DecalComp = GetDecal();
	UMaterialInterface* Source = bFlammable ? FlammableMaterial : NonFlammableMaterial;

	if (!DecalComp || !Source)
	{
		return;
	}

	// One dynamic instance per mark. Without it every scorch in the level shares one
	// material and they all darken together, which is worse than having none at all.
	DecalMaterial = UMaterialInstanceDynamic::Create(Source, this);
	DecalComp->SetDecalMaterial(DecalMaterial);

	// SetScalarParameterValue cannot report a missing name, so the only way a typo or a
	// renamed parameter shows up is as a decal that never changes -- which looks exactly
	// like a broken material. Ask the instance directly, once, and say so.
	static bool bProbed = false;
	if (!bProbed)
	{
		bProbed = true;

		float Probe = 0.0f;
		const bool bHasOpacity =
			DecalMaterial->GetScalarParameterValue(OpacityParameter, Probe);
		const bool bHasFire =
			DecalMaterial->GetScalarParameterValue(FireIntensityParameter, Probe);

		UE_LOG(LogFireDecal, Warning,
			TEXT("FIREDECAL material='%s' flammable=%d | '%s' found=%d | '%s' found=%d"),
			*GetNameSafe(Source), bFlammable ? 1 : 0,
			*OpacityParameter.ToString(), bHasOpacity ? 1 : 0,
			*FireIntensityParameter.ToString(), bHasFire ? 1 : 0);
	}

	PushMaterialParameters();
}

void AFireDecalActor::ConfigureForSurface(bool bInFlammable)
{
	// Before BeginPlay, so the right parent material is chosen when the instance is
	// created rather than swapped afterwards.
	bFlammable = bInFlammable;
}

void AFireDecalActor::ReuseForSurface(bool bInFlammable)
{
	if (bInFlammable != bFlammable || !DecalMaterial)
	{
		bFlammable = bInFlammable;

		if (UDecalComponent* DecalComp = GetDecal())
		{
			if (UMaterialInterface* Source =
				bFlammable ? FlammableMaterial : NonFlammableMaterial)
			{
				DecalMaterial = UMaterialInstanceDynamic::Create(Source, this);
				DecalComp->SetDecalMaterial(DecalMaterial);
			}
		}
	}

	Opacity = 0.0f;
	FireIntensity = 0.0f;

	// Stopped rather than left running: the mark it was fading belongs to the old
	// location, and the timer restarts itself on the next hit anyway.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EmberFadeTimer);
		LastHitTime = World->GetTimeSeconds();
	}

	PushMaterialParameters();
}

void AFireDecalActor::ApplyFireHit(float Intensity)
{
	const float Strength = FMath::Clamp(Intensity, 0.0f, 1.0f);

	if (const UWorld* World = GetWorld())
	{
		LastHitTime = World->GetTimeSeconds();
	}

	// Scorch always, and it never comes back. Burnt is not a thing a surface stops
	// being, so this only ever climbs.
	Opacity = FMath::Clamp(Opacity + OpacityPerHit * Strength, 0.0f, MaxOpacity());

	if (bFlammable)
	{
		FireIntensity = FMath::Clamp(FireIntensity + EmberPerHit * Strength, 0.0f, 1.0f);

		// Started once and left running, rather than re-armed per hit. A timer set on
		// every collision would be re-set hundreds of times a second while the flame
		// is on the surface, and the fade would never advance a step.
		if (UWorld* World = GetWorld())
		{
			if (!World->GetTimerManager().IsTimerActive(EmberFadeTimer))
			{
				World->GetTimerManager().SetTimer(EmberFadeTimer,
					FTimerDelegate::CreateUObject(this, &AFireDecalActor::FadeEmbers),
					EmberFadeInterval, true);
			}
		}
	}

	PushMaterialParameters();
}

void AFireDecalActor::FadeEmbers()
{
	// Measured from the last hit rather than from spawn, so sweeping the flame back over
	// a mark restarts its hold instead of letting it cool while still being burnt.
	const UWorld* HoldWorld = GetWorld();
	const float SinceLastHit = HoldWorld
		? HoldWorld->GetTimeSeconds() - LastHitTime
		: TNumericLimits<float>::Max();

	if (SinceLastHit >= EmberHoldSeconds)
	{
		FireIntensity *= EmberDecayPerStep;
	}

	// Still glowing means still burning, so the mark goes on deepening after the flame
	// has moved on. This is most of what sells it as fire rather than paint.
	Opacity = FMath::Clamp(Opacity + OpacityGrowthWhileBurning, 0.0f, MaxOpacity());

	if (FireIntensity <= EmberExtinguishThreshold)
	{
		FireIntensity = 0.0f;

		// Nothing left to animate. Left running, every scorch mark ever made would
		// keep a timer alive for the rest of the session.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(EmberFadeTimer);
		}
	}

	PushMaterialParameters();
}

void AFireDecalActor::PushMaterialParameters()
{
	if (!DecalMaterial)
	{
		return;
	}

	DecalMaterial->SetScalarParameterValue(OpacityParameter, Opacity);
	DecalMaterial->SetScalarParameterValue(FireIntensityParameter, FireIntensity);

	// The last unobserved link in the chain. Everything upstream has been verified by
	// inspection -- the parameters exist, the graph is wired, the hits arrive -- so what
	// is left is whether these two numbers actually move. Throttled to one line a second
	// across all decals, because there are dozens of them being written every frame.
	if (const UWorld* World = GetWorld())
	{
		static double NextLogTime = 0.0;
		const double Now = World->GetTimeSeconds();

		// World time restarts at zero every PIE session, so a deadline left over from
		// the last one sits in the future and silences the log until the new session has
		// run just as long. Time moving backwards means a new world, not a long wait.
		if (Now + 1.0 < NextLogTime)
		{
			NextLogTime = 0.0;
		}

		if (Now >= NextLogTime)
		{
			NextLogTime = Now + 1.0;

			UE_LOG(LogFireDecal, Warning,
				TEXT("FIREDECAL flammable=%d opacity=%.4f (max %.2f) fire=%.4f"),
				bFlammable ? 1 : 0, Opacity, MaxOpacity(), FireIntensity);
		}
	}
}

float AFireDecalActor::MaxOpacity() const
{
	return bFlammable ? MaxOpacityFlammable : MaxOpacityNonFlammable;
}
