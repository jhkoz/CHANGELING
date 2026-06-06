// WeatherController.cpp

#include "WeatherController.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "AstroDayNightManager.h"

AWeatherController::AWeatherController()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	// Camera-following precipitation emitters (assign Niagara Systems in the editor).
	RainFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RainFX"));
	RainFX->SetupAttachment(SceneRoot);
	RainFX->bAutoActivate = false;

	SnowFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SnowFX"));
	SnowFX->SetupAttachment(SceneRoot);
	SnowFX->bAutoActivate = false;

	// ── Default presets (Catskills-flavoured). Editable per instance. ────────────
	//                          cloud   fog    wind   precip  wet    snow
	auto Add = [this](EWeatherType T, float Cloud, float Fog, float Wind,
		float Precip, float Wet, float Snow)
	{
		FWeatherPreset P;
		P.CloudCoverage = Cloud;
		P.FogDensity    = Fog;
		P.WindStrength  = Wind;
		P.Precipitation = Precip;
		P.Wetness       = Wet;
		P.Snow          = Snow;
		Presets.Add(T, P);
	};

	Add(EWeatherType::Clear,        0.10f, 0.010f, 0.10f, 0.0f, 0.0f, 0.0f);
	Add(EWeatherType::PartlyCloudy, 0.35f, 0.015f, 0.25f, 0.0f, 0.0f, 0.0f);
	Add(EWeatherType::Overcast,     0.80f, 0.020f, 0.30f, 0.0f, 0.1f, 0.0f);
	Add(EWeatherType::Foggy,        0.50f, 0.120f, 0.05f, 0.0f, 0.3f, 0.0f);
	Add(EWeatherType::Rain,         0.90f, 0.030f, 0.45f, 0.7f, 0.9f, 0.0f);
	Add(EWeatherType::Storm,        1.00f, 0.040f, 0.90f, 1.0f, 1.0f, 0.0f);
	Add(EWeatherType::Snow,         0.95f, 0.050f, 0.40f, 0.8f, 0.2f, 0.9f);

	// Default randomizer weights — Clear common, Storm rare
	WeatherWeights.Add(EWeatherType::Clear,        4.0f);
	WeatherWeights.Add(EWeatherType::PartlyCloudy, 3.0f);
	WeatherWeights.Add(EWeatherType::Overcast,     2.0f);
	WeatherWeights.Add(EWeatherType::Foggy,        1.0f);
	WeatherWeights.Add(EWeatherType::Rain,         1.5f);
	WeatherWeights.Add(EWeatherType::Storm,        0.5f);
	WeatherWeights.Add(EWeatherType::Snow,         1.0f);
}

void AWeatherController::BeginPlay()
{
	Super::BeginPlay();

	// Convenience: grab the level's height fog if none was assigned
	if (!HeightFog)
	{
		HeightFog = Cast<AExponentialHeightFog>(
			UGameplayStatics::GetActorOfClass(this, AExponentialHeightFog::StaticClass()));
	}

	// Grab the level's volumetric cloud and wrap its material in a dynamic instance so
	// we can drive coverage directly each frame.
	if (!VolumetricCloud)
	{
		VolumetricCloud = Cast<AVolumetricCloud>(
			UGameplayStatics::GetActorOfClass(this, AVolumetricCloud::StaticClass()));
	}
	if (VolumetricCloud)
	{
		if (UVolumetricCloudComponent* CloudComp =
			VolumetricCloud->FindComponentByClass<UVolumetricCloudComponent>())
		{
			if (UMaterialInterface* BaseMat = CloudComp->Material.LoadSynchronous())
			{
				CloudMID = UMaterialInstanceDynamic::Create(BaseMat, this);
				CloudComp->SetMaterial(CloudMID);
			}
		}
	}

	// Weather clock source — share the day/night TimeScale so weather tracks game time.
	if (!DayNight)
	{
		DayNight = Cast<AAstroDayNightManager>(
			UGameplayStatics::GetActorOfClass(this, AAstroDayNightManager::StaticClass()));
	}

	CurrentWeather = StartingWeather;
	Current = ResolvePreset(CurrentWeather);
	Target  = Current;
	ApplyToWorld();

	TimeUntilNextRoll = FMath::RandRange(
		FMath::Min(MinWeatherDuration, MaxWeatherDuration),
		FMath::Max(MinWeatherDuration, MaxWeatherDuration));
}

void AWeatherController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Weather clock: scale by the day/night TimeScale so a "front" lasts a sensible number
	// of game hours no matter how fast time is running. (The visual blend below stays on
	// real DeltaTime so transitions don't pop instantly at high TimeScale.)
	const float TimeStep = DeltaTime *
		((bUseGameTime && DayNight) ? FMath::Max(0.0f, DayNight->TimeScale) : 1.0f);

	// Randomizer — roll a new weather when the hold timer runs out
	if (bRandomizeWeather)
	{
		TimeUntilNextRoll -= TimeStep;
		if (TimeUntilNextRoll <= 0.0f)
		{
			RollRandomWeather();
		}
	}

	// Smoothly ease each field toward the target preset
	auto Ease = [&](float& Cur, float Tgt)
	{
		Cur = FMath::FInterpTo(Cur, Tgt, DeltaTime, TransitionSpeed);
	};
	Ease(Current.CloudCoverage, Target.CloudCoverage);
	Ease(Current.FogDensity,    Target.FogDensity);
	Ease(Current.WindStrength,  Target.WindStrength);
	Ease(Current.Precipitation, Target.Precipitation);
	Ease(Current.Wetness,       Target.Wetness);
	Ease(Current.Snow,          Target.Snow);

	ApplyToWorld();
	UpdatePrecipitation();
}

void AWeatherController::SetWeather(EWeatherType NewWeather)
{
	if (NewWeather == CurrentWeather)
	{
		return;
	}
	CurrentWeather = NewWeather;
	Target = ResolvePreset(NewWeather);
	OnWeatherChanged.Broadcast(NewWeather);
}

bool AWeatherController::IsTransitionComplete() const
{
	return FMath::IsNearlyEqual(Current.CloudCoverage, Target.CloudCoverage, 0.01f)
	    && FMath::IsNearlyEqual(Current.FogDensity,    Target.FogDensity,    0.001f)
	    && FMath::IsNearlyEqual(Current.Precipitation, Target.Precipitation, 0.01f);
}

void AWeatherController::RollRandomWeather()
{
	SetWeather(PickWeightedWeather());
	TimeUntilNextRoll = FMath::RandRange(
		FMath::Min(MinWeatherDuration, MaxWeatherDuration),
		FMath::Max(MinWeatherDuration, MaxWeatherDuration));
}

EWeatherType AWeatherController::PickWeightedWeather() const
{
	float Total = 0.0f;
	for (const TPair<EWeatherType, float>& W : WeatherWeights)
	{
		Total += FMath::Max(0.0f, W.Value);
	}
	if (Total <= 0.0f)
	{
		return CurrentWeather;   // no weights configured
	}

	float Roll = FMath::FRandRange(0.0f, Total);
	for (const TPair<EWeatherType, float>& W : WeatherWeights)
	{
		Roll -= FMath::Max(0.0f, W.Value);
		if (Roll <= 0.0f)
		{
			return W.Key;
		}
	}
	return CurrentWeather;
}

//──────────────────────────────────────────────────────────────────────────────
// Internals
//──────────────────────────────────────────────────────────────────────────────

void AWeatherController::ApplyToWorld()
{
	// Height fog — driven directly
	if (HeightFog)
	{
		if (UExponentialHeightFogComponent* FogComp = HeightFog->GetComponent())
		{
			FogComp->SetFogDensity(Current.FogDensity);
		}
	}

	// Volumetric cloud — drive its coverage scalar directly
	if (CloudMID)
	{
		CloudMID->SetScalarParameterValue(CloudCoverageParam, Current.CloudCoverage);
	}

	// Material Parameter Collection — read by the cloud + surface materials
	if (WeatherParams)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParams, TEXT("CloudCoverage"), Current.CloudCoverage);
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParams, TEXT("WindStrength"),  Current.WindStrength);
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParams, TEXT("Precipitation"), Current.Precipitation);
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParams, TEXT("Wetness"),       Current.Wetness);
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParams, TEXT("Snow"),          Current.Snow);
	}
}

FWeatherPreset AWeatherController::ResolvePreset(EWeatherType Type) const
{
	if (const FWeatherPreset* P = Presets.Find(Type))
	{
		return *P;
	}
	return FWeatherPreset();
}

bool AWeatherController::IsSnowWeather(EWeatherType Type) const
{
	return Type == EWeatherType::Snow;
}

void AWeatherController::UpdatePrecipitation()
{
	const bool  bSnow = IsSnowWeather(CurrentWeather);
	const float Rate  = Current.Precipitation;
	const bool  bWet  = Rate > 0.01f;

	// Horizontal wind velocity, scaled by the current blended strength
	const FVector Wind = FRotator(0.0f, WindHeadingDeg, 0.0f).Vector()
		* (MaxWindSpeed * Current.WindStrength);

	// Park the emitters on the camera so weather surrounds the viewer
	FVector FollowLoc = GetActorLocation();
	if (bPrecipFollowCamera)
	{
		if (const UWorld* W = GetWorld())
		{
			if (const APlayerController* PC = W->GetFirstPlayerController())
			{
				if (PC->PlayerCameraManager)
				{
					FollowLoc = PC->PlayerCameraManager->GetCameraLocation()
						+ FVector(0.0f, 0.0f, PrecipCameraHeight);
				}
			}
		}
	}

	auto Drive = [&](UNiagaraComponent* FX, bool bShouldRun)
	{
		if (!FX)
		{
			return;
		}
		if (bPrecipFollowCamera)
		{
			FX->SetWorldLocation(FollowLoc);
		}
		if (bShouldRun && bWet)
		{
			if (!FX->IsActive())
			{
				FX->Activate();
			}
			FX->SetVariableFloat(PrecipRateParam, Rate);
			FX->SetVariableVec3(WindParam, Wind);
		}
		else if (FX->IsActive())
		{
			FX->Deactivate();   // stop spawning; let existing particles fall out
		}
	};

	Drive(RainFX, !bSnow);
	Drive(SnowFX,  bSnow);
}
