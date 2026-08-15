// FireDecalActor.cpp

#include "FireDecalActor.h"

#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
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

	PushMaterialParameters();
}

void AFireDecalActor::ConfigureForSurface(bool bInFlammable)
{
	// Before BeginPlay, so the right parent material is chosen when the instance is
	// created rather than swapped afterwards.
	bFlammable = bInFlammable;
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
	FireIntensity *= EmberDecayPerStep;

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
}

float AFireDecalActor::MaxOpacity() const
{
	return bFlammable ? MaxOpacityFlammable : MaxOpacityNonFlammable;
}
