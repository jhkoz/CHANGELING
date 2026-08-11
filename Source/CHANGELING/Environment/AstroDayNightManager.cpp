// AstroDayNightManager.cpp
//
// Solar position  — USunPositionFunctionLibrary (Spencer / Iqbal algorithm)
// Lunar position  — Truncated Meeus Chapter 47 theory
//                   30 longitude terms + 20 latitude terms → ~0.3° accuracy
// Coordinates     — Ecliptic → equatorial (true obliquity)
//                   → horizontal (GMST / LMST / hour angle)
//                   → atmospheric refraction (Bennett formula)

#include "AstroDayNightManager.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Paths.h"
#include "StarCatalog.h"
#include "WeatherController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"

// ─────────────────────────────────────────────────────────────────────────────
// Convenience: degrees ↔ radians
// ─────────────────────────────────────────────────────────────────────────────
static constexpr double kD2R = PI / 180.0;
static constexpr double kR2D = 180.0 / PI;

static double DToR(double d) { return d * kD2R; }
static double RToD(double r) { return r * kR2D; }

/** Normalise degrees to [0, 360) */
static double Norm360(double d)
{
	d = FMath::Fmod(d, 360.0);
	return d < 0.0 ? d + 360.0 : d;
}

/** World uniform scale that makes a mesh of half-extent LocalRadius subtend AngularDiameterDeg
 *  (apparent diameter, degrees) when viewed from Distance away. */
static float ScaleForAngularDiameter(float AngularDiameterDeg, float Distance, float LocalRadius)
{
	if (LocalRadius <= KINDA_SMALL_NUMBER) return 1.0f;
	const float WorldRadius = Distance * FMath::Tan(FMath::DegreesToRadians(AngularDiameterDeg * 0.5f));
	return WorldRadius / LocalRadius;
}

/** Cache a static mesh's unscaled bounds: OutRadius = largest box half-extent (the disc radius for
 *  a sphere or a flat billboard); OutCenter = bounds origin in local space (i.e. the pivot offset,
 *  non-zero when the mesh's pivot isn't its centre). */
static void CacheMeshLocalBounds(UStaticMeshComponent* Mesh, float& OutRadius, FVector& OutCenter)
{
	OutRadius = 50.0f;
	OutCenter = FVector::ZeroVector;
	if (Mesh && Mesh->GetStaticMesh())
	{
		const FBoxSphereBounds B = Mesh->GetStaticMesh()->GetBounds();
		OutRadius = static_cast<float>(FMath::Max3(B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z));
		OutCenter = FVector(B.Origin);
	}
}

//──────────────────────────────────────────────────────────────────────────────
// Constructor
//──────────────────────────────────────────────────────────────────────────────

AAstroDayNightManager::AAstroDayNightManager()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(Root);
	SunLight->Intensity               = 10.0f;
	SunLight->bAtmosphereSunLight     = true;
	SunLight->AtmosphereSunLightIndex = 0;
	SunLight->SetCastShadows(true);
	SunLight->bUseTemperature         = true;
	SunLight->Temperature             = 6500.0f;

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
	MoonLight->SetupAttachment(Root);
	MoonLight->Intensity               = 0.05f;
	MoonLight->bAtmosphereSunLight     = true;    // second atmosphere light; see bMoonLightsSky
	MoonLight->AtmosphereSunLightIndex = 1;
	MoonLight->SetCastShadows(false);
	MoonLight->LightColor              = FColor(160, 180, 255);

	NightFillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("NightFillLight"));
	NightFillLight->SetupAttachment(Root);
	NightFillLight->bAtmosphereSunLight = false;   // pure scene fill, not a sky light
	NightFillLight->SetCastShadows(false);
	NightFillLight->SetIntensity(0.0f);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->bRealTimeCapture = true;
	SkyLight->Intensity        = 1.0f;

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);

	SunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SunMesh"));
	SunMesh->SetupAttachment(Root);
	SunMesh->SetCastShadow(false);
	SunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MoonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MoonMesh"));
	MoonMesh->SetupAttachment(Root);
	MoonMesh->SetCastShadow(false);
	MoonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StarDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StarDome"));
	StarDome->SetupAttachment(Root);
	StarDome->SetCastShadow(false);
	StarDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StarDome->bReceivesDecals = false;
	// Backdrop only — keep the emissive stars out of reflections, sky captures and GI
	// so they don't mirror off scene meshes or leak into Lumen lighting.
	StarDome->bVisibleInReflectionCaptures   = false;
	StarDome->bVisibleInRealTimeSkyCaptures   = false;
	StarDome->bVisibleInRayTracing            = false;
	StarDome->bAffectDynamicIndirectLighting  = false;
	StarDome->SetVisibility(false);   // shown only at night

	CurrentDateTime = FDateTime(2020, 3, 21, 12, 0, 0);   // spring equinox 2020
}

//──────────────────────────────────────────────────────────────────────────────
// Lifecycle
//──────────────────────────────────────────────────────────────────────────────

void AAstroDayNightManager::BeginPlay()
{
	Super::BeginPlay();
	PrevDay = CurrentDateTime.GetDay();
	bWasDay = false;

	// Fallback: if the C++ pointer is null (e.g. stale hot-reload), find by name
	if (!MoonMesh)
	{
		MoonMesh = Cast<UStaticMeshComponent>(
			GetDefaultSubobjectByName(TEXT("MoonMesh")));
		UE_LOG(LogTemp, Warning, TEXT("AstroDayNightManager: MoonMesh was null — fallback %s"),
			MoonMesh ? TEXT("succeeded") : TEXT("FAILED — check Blueprint components"));
	}
	if (!SunMesh)
	{
		SunMesh = Cast<UStaticMeshComponent>(
			GetDefaultSubobjectByName(TEXT("SunMesh")));
	}

	// Cache the sun/moon mesh bounds now so UpdateSun/UpdateMoon can size each disc from an angular
	// diameter and re-centre an off-pivot mesh (the assigned sphere's pivot sits at its base).
	CacheMeshLocalBounds(SunMesh,  SunMeshLocalRadius,  SunMeshLocalCenter);
	CacheMeshLocalBounds(MoonMesh, MoonMeshLocalRadius, MoonMeshLocalCenter);

	if (!MoonLight)
	{
		MoonLight = Cast<UDirectionalLightComponent>(
			GetDefaultSubobjectByName(TEXT("MoonLight")));
	}

	// Apply the moon-as-atmosphere-light choice here, not just the constructor: the placed
	// actor predates the flag and saved instances keep their old component state. Without
	// this the night sky capture is pitch black and the SkyLight night floor multiplies zero.
	if (MoonLight)
	{
		MoonLight->bAtmosphereSunLight     = bMoonLightsSky;
		MoonLight->AtmosphereSunLightIndex = 1;
		MoonLight->MarkRenderStateDirty();
	}

	UE_LOG(LogTemp, Warning, TEXT("AstroDayNightManager — SunLight:%s MoonLight:%s MoonMesh:%s"),
		SunLight  ? TEXT("OK") : TEXT("NULL"),
		MoonLight ? TEXT("OK") : TEXT("NULL"),
		MoonMesh  ? TEXT("OK") : TEXT("NULL"));

	// Link to the weather so heavy cloud can dim the sun and moon.
	if (!Weather)
	{
		Weather = Cast<AWeatherController>(
			UGameplayStatics::GetActorOfClass(this, AWeatherController::StaticClass()));
	}

	// Auto-find the height fog for the night glow.
	if (!HeightFog)
	{
		HeightFog = Cast<AExponentialHeightFog>(
			UGameplayStatics::GetActorOfClass(this, AExponentialHeightFog::StaticClass()));
	}

	// Dynamic material instance lets us fade the stars in and out at runtime
	if (StarDome && StarDome->GetMaterial(0))
	{
		StarMID = StarDome->CreateDynamicMaterialInstance(0);
	}

	// Dynamic material instance for the moon so we can feed it the live sun direction
	// for phase / terminator shading
	if (MoonMesh && MoonMesh->GetMaterial(0))
	{
		MoonMID = MoonMesh->CreateDynamicMaterialInstance(0);
	}

	// Bake the real star catalogue into the dome's texture (equirectangular, masked-friendly).
	if (bGenerateRealStars && StarMID)
	{
		TArray<FCatalogStar> Catalog;
		StarCatalog::AppendBrightStars(Catalog);
		if (!StarCatalogCsv.IsEmpty())
		{
			const FString Path = FPaths::IsRelative(StarCatalogCsv)
				? FPaths::ProjectDir() / StarCatalogCsv : StarCatalogCsv;
			const int32 Loaded = StarCatalog::AppendFromHYGCsv(Path, Catalog);
			UE_LOG(LogTemp, Log, TEXT("StarCatalog: +%d stars from %s"), Loaded, *Path);
		}
		const int32 W = FMath::Max(512, StarTextureWidth);
		StarRenderTarget = StarCatalog::Bake(this, Catalog, W, W / 2,
			StarMagnitudeLimit, StarProceduralFill);
		if (StarRenderTarget)
		{
			StarMID->SetTextureParameterValue(StarTextureParam, StarRenderTarget);
			UE_LOG(LogTemp, Log, TEXT("StarCatalog: baked %d stars into %dx%d"),
				Catalog.Num(), W, W / 2);
		}
	}

	UpdateSun();
	UpdateMoon();
	UpdateSkyLight();
	UpdateStars();
}

void AAstroDayNightManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bPaused) AdvanceTime(DeltaTime);
	UpdateSun();
	UpdateMoon();
	UpdateSkyLight();
	UpdateStars();
	UpdateSeason();
}

//──────────────────────────────────────────────────────────────────────────────
// Season
//──────────────────────────────────────────────────────────────────────────────

float AAstroDayNightManager::GetSeasonPhase() const
{
	// Day of year normalised, with the origin moved to the winter solstice so phase
	// 0 is midwinter and 0.5 is midsummer. Taking the real day count rather than a
	// fixed 365 keeps leap years from drifting the seasons against the sun that the
	// rest of this class computes from the very same date.
	const int32 DaysInYear = FDateTime::DaysInYear(CurrentDateTime.GetYear());
	constexpr int32 SolsticeDay = 355;   // ~21 December

	const float FractionOfDay = (CurrentDateTime.GetHour() * 3600.0f
		+ CurrentDateTime.GetMinute() * 60.0f
		+ CurrentDateTime.GetSecond()) / 86400.0f;

	const float Day = (CurrentDateTime.GetDayOfYear() - 1) + FractionOfDay;
	return FMath::Frac((Day - SolsticeDay + DaysInYear) / static_cast<float>(DaysInYear));
}

void AAstroDayNightManager::UpdateSeason()
{
	if (!SeasonCollection)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(
			World, SeasonCollection, SeasonPhaseParameter, GetSeasonPhase());
	}
}

//──────────────────────────────────────────────────────────────────────────────
// Time
//──────────────────────────────────────────────────────────────────────────────

void AAstroDayNightManager::AdvanceTime(float DeltaTime)
{
	CurrentDateTime += FTimespan::FromSeconds(
		static_cast<double>(DeltaTime) * static_cast<double>(TimeScale));

	const int32 Today = CurrentDateTime.GetDay();
	if (Today != PrevDay) { PrevDay = Today; OnDayChanged.Broadcast(Today); }

	OnTimeChanged.Broadcast(GetTimeOfDayHours());
}

void AAstroDayNightManager::SetTimeOfDay(float Hours)
{
	Hours        = FMath::Clamp(Hours, 0.0f, 23.9999f);
	const int32 H  = FMath::FloorToInt(Hours);
	const int32 Mi = FMath::FloorToInt(FMath::Frac(Hours) * 60.0f);
	const int32 S  = FMath::FloorToInt(FMath::Frac(Hours * 60.0f) * 60.0f);
	CurrentDateTime = FDateTime(
		CurrentDateTime.GetYear(), CurrentDateTime.GetMonth(), CurrentDateTime.GetDay(),
		H, Mi, S);
}

float AAstroDayNightManager::GetTimeOfDayHours() const
{
	return static_cast<float>(CurrentDateTime.GetHour())
	     + static_cast<float>(CurrentDateTime.GetMinute()) / 60.0f
	     + static_cast<float>(CurrentDateTime.GetSecond()) / 3600.0f;
}

float AAstroDayNightManager::GetNormalizedTimeOfDay() const
{
	return GetTimeOfDayHours() / 24.0f;
}

//──────────────────────────────────────────────────────────────────────────────
// Sun
//──────────────────────────────────────────────────────────────────────────────

void AAstroDayNightManager::UpdateSun()
{
	if (!SunLight) return;

	USunPositionFunctionLibrary::GetSunPosition(
		Latitude, Longitude, TimeZone, bDaylightSaving,
		CurrentDateTime.GetYear(), CurrentDateTime.GetMonth(), CurrentDateTime.GetDay(),
		CurrentDateTime.GetHour(), CurrentDateTime.GetMinute(), CurrentDateTime.GetSecond(),
		CachedSunData);

	// The SunPosition plugin reports Elevation/CorrectedElevation offset by +180°
	// "to fit UE's coord system" (Engine SunPosition.cpp ~L143). Undo the offset so
	// every threshold below works with a true astronomical elevation in [-90, 90].
	const float TrueElevation          = CachedSunData.Elevation          - 180.0f;
	const float TrueCorrectedElevation = CachedSunData.CorrectedElevation - 180.0f;

	SunElevation = TrueElevation;

	// Pitch uses the plugin's OFFSET value directly — that +180 is exactly the term
	// that orients the light correctly. Negating it inverts day/night (bright sky at
	// midnight), which is the bug we were chasing.
	const FRotator SunRotation(CachedSunData.CorrectedElevation, CachedSunData.Azimuth + CelestialNorthYaw, 0.0f);
	const float Intensity = ComputeSunIntensity() * CloudDimFactor();

	if (TrueCorrectedElevation < -18.0f)
	{
		// Astronomical night — park the light straight down so the Sky Atmosphere
		// doesn't scatter a horizon glow from a sub-horizon light direction
		SunLight->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
		SunLight->SetIntensity(0.0f);
	}
	else
	{
		SunLight->SetWorldRotation(SunRotation);
		SunLight->SetIntensity(Intensity);
		SunLight->SetLightColor(ComputeSunColor() * SunDiscColor);
		SunLight->LightSourceAngle = SunDiscAngle * HorizonSizeFactor(SunElevation);
	}

	const bool bNowDay = CachedSunData.CorrectedElevation > 0.0f;
	bIsDay = bNowDay;
	if ( bNowDay && !bWasDay) OnSunrise.Broadcast(CurrentDateTime);
	if (!bNowDay &&  bWasDay) OnSunset.Broadcast(CurrentDateTime);
	bWasDay = bNowDay;

	// Dawn / Dusk — civil twilight boundary (sun at -6°)
	const bool bIsCivilTwilight = SunElevation > -6.0f;
	if ( bIsCivilTwilight && !bWasCivilTwilight) OnDawn.Broadcast(CurrentDateTime);
	if (!bIsCivilTwilight &&  bWasCivilTwilight) OnDusk.Broadcast(CurrentDateTime);
	bWasCivilTwilight = bIsCivilTwilight;

	// Position & size the sun disc mesh toward the sun (parallels MoonMesh in UpdateMoon).
	if (SunMesh)
	{
		// SunRotation points along the light's travel (toward the ground); negate → toward the sun.
		const FVector  Dir     = -SunRotation.Vector();
		const FRotator MeshRot = (-Dir).Rotation();                      // billboard the disc at the viewer
		const float    Scale   = ScaleForAngularDiameter(SunAngularDiameter, SunMeshDistance, SunMeshLocalRadius)
		                         * HorizonSizeFactor(SunElevation);
		const FVector  Target  = GetObserverLocation() + Dir * SunMeshDistance;
		// Place the mesh so its BOUNDS CENTRE lands on Target — an off-centre pivot would otherwise
		// throw the disc several degrees off the true sun direction at these scales.
		const FVector  CentreOffset = MeshRot.RotateVector(SunMeshLocalCenter * Scale);
		SunMesh->SetWorldScale3D(FVector(Scale));
		SunMesh->SetWorldRotation(MeshRot);
		SunMesh->SetWorldLocation(Target - CentreOffset);
		SunMesh->SetVisibility(TrueCorrectedElevation > -2.0f);          // hide once below the horizon
	}
}

float AAstroDayNightManager::ComputeSunIntensity() const
{
	if (SunElevation >= 0.0f)
	{
		const float T = FMath::Clamp(SunElevation / 15.0f, 0.0f, 1.0f);
		return FMath::Lerp(SunHorizonIntensity, SunMaxIntensity, T);
	}
	// Below the horizon: fade the direct sun smoothly to 0 by civil dusk (-6°) instead of
	// cutting off at the horizon, so sunrise/sunset ease in/out instead of popping.
	const float Tw = FMath::Clamp((SunElevation + 6.0f) / 6.0f, 0.0f, 1.0f);
	return SunHorizonIntensity * Tw;
}

FLinearColor AAstroDayNightManager::ComputeSunColor() const
{
	const float T = FMath::Clamp((SunElevation + 6.0f) / 36.0f, 0.0f, 1.0f);
	const FLinearColor Horizon(1.0f, 0.45f, 0.1f);
	const FLinearColor Zenith (1.0f, 0.97f, 0.92f);
	return FLinearColor::LerpUsingHSV(Horizon, Zenith, T);
}

float AAstroDayNightManager::HorizonSizeFactor(float ElevationDeg) const
{
	// Fakes the Moon Illusion: full boost at the horizon, fading to 1 as the body climbs.
	if (HorizonBoostFadeDeg <= 0.0f) return 1.0f;
	const float T = FMath::Clamp(ElevationDeg / HorizonBoostFadeDeg, 0.0f, 1.0f);
	return FMath::Lerp(HorizonSizeBoost, 1.0f, T);
}

float AAstroDayNightManager::CloudDimFactor() const
{
	// Heavy overcast hides the sun/moon: cut their direct light by the live cloud cover.
	const float Cover = Weather ? Weather->Current.CloudCoverage : 0.0f;
	return FMath::Clamp(1.0f - Cover * CloudDimStrength, 0.0f, 1.0f);
}

//──────────────────────────────────────────────────────────────────────────────
// Moon — Meeus Chapter 47
//──────────────────────────────────────────────────────────────────────────────

void AAstroDayNightManager::UpdateMoon()
{
	if (!MoonLight) return;

	const double JD = ComputeJulianDate();
	const double T  = (JD - 2451545.0) / 36525.0;   // Julian centuries from J2000.0

	// Lunar ecliptic coordinates
	double EclLon, EclLat, DistKm;
	ComputeLunarEcliptic(T, EclLon, EclLat, DistKm);
	MoonDistanceKm = static_cast<float>(DistKm);

	// Moon phase from ecliptic longitude of moon vs sun
	// Sun's approximate ecliptic longitude (degrees)
	const double SunLon = Norm360(280.46646 + 36000.76983*T);
	const double ElongDeg = Norm360(EclLon - SunLon);
	// Phase: 0 = new, 0.5 = full
	MoonPhase = static_cast<float>(ElongDeg / 360.0);

	// Ecliptic → equatorial
	const double Epsilon = ComputeObliquity(T);
	double RA, Dec;
	EclipticToEquatorial(EclLon, EclLat, Epsilon, RA, Dec);

	// Equatorial → horizontal
	const double GMST = ComputeGMST(JD);
	double MoonAz, MoonAlt;
	EquatorialToHorizontal(RA, Dec, GMST,
		static_cast<double>(Latitude),
		static_cast<double>(Longitude),
		MoonAz, MoonAlt);

	// Atmospheric refraction
	const float CorrectedAlt = ApplyAtmosphericRefraction(static_cast<float>(MoonAlt));
	MoonAltitude = CorrectedAlt;

	// Drive the light — match the sun's +180° elevation convention so the moon shares
	// the sun's azimuth frame (rises east / sets west) instead of being mirrored across
	// the sky. The mesh dir below (-Vector) then correctly points at the moon.
	const FRotator MoonRotation(180.0f + CorrectedAlt, static_cast<float>(MoonAz) + CelestialNorthYaw, 0.0f);
	MoonLight->SetWorldRotation(MoonRotation);

	// Intensity & colour from phase + altitude.
	// Illuminated fraction (smooth, geometric): 0 at new moon → 1 at full.
	const float SinHalf = FMath::Sin(PI * MoonPhase);
	const float Illum   = SinHalf * SinHalf;                 // sin²(π·phase) = (1−cos)/2
	// Real lunar photometric curve (Schaefer): a quarter moon is ~2.6 mag fainter than full,
	// so only ~9% as bright (not the ~25% squaring gives). Phase angle a = acos(2*Illum-1);
	// dimmer-than-full magnitude dm = 0.026*a + 4e-9*a^4; factor = 10^(-0.4*dm).
	const float Alpha       = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(2.0f * Illum - 1.0f, -1.0f, 1.0f)));
	const float MagDelta    = 0.026f * Alpha + 4.0e-9f * Alpha * Alpha * Alpha * Alpha;
	const float PhaseFactor = FMath::Pow(10.0f, -0.4f * MagDelta);
	// Illuminance on the ground ∝ sin(altitude); zero at / below the horizon.
	const float ElevationFactor = FMath::Clamp(
		FMath::Sin(FMath::DegreesToRadians(CorrectedAlt)), 0.0f, 1.0f);
	const float CloudDim = CloudDimFactor();
	MoonLight->SetIntensity(MoonMaxIntensity * PhaseFactor * ElevationFactor * CloudDim);
	MoonIllumination = PhaseFactor * ElevationFactor * CloudDim;   // shared with the night ambient

	// Warm the moonlight as it sinks toward the horizon (atmospheric reddening — the
	// "harvest moon"); cool blue-white when high overhead.
	const FLinearColor MoonHigh(0.62f, 0.70f, 1.00f);
	const FLinearColor MoonLow (1.00f, 0.72f, 0.45f);
	const float HorizonT = FMath::Clamp(CorrectedAlt / 18.0f, 0.0f, 1.0f);
	MoonLight->SetLightColor(FMath::Lerp(MoonLow, MoonHigh, HorizonT));

	// Moonrise / Moonset
	const bool bIsMoonUp = MoonAltitude > 0.0f;
	if ( bIsMoonUp && !bWasMoonUp) OnMoonrise.Broadcast(CurrentDateTime);
	if (!bIsMoonUp &&  bWasMoonUp) OnMoonset.Broadcast(CurrentDateTime);
	bWasMoonUp = bIsMoonUp;

	// Full Moon / New Moon — fire once on entering the phase window
	const bool bInFullMoonZone = (MoonPhase >= 0.46f && MoonPhase <= 0.54f);
	const bool bInNewMoonZone  = (MoonPhase <= 0.04f || MoonPhase >= 0.96f);
	if (bInFullMoonZone && !bWasInFullMoonZone) OnFullMoon.Broadcast(CurrentDateTime);
	if (bInNewMoonZone  && !bWasInNewMoonZone)  OnNewMoon.Broadcast(CurrentDateTime);
	bWasInFullMoonZone = bInFullMoonZone;
	bWasInNewMoonZone  = bInNewMoonZone;

	// Position & size the moon mesh relative to camera
	if (MoonMesh)
	{
		// Negate: MoonRotation.Vector() points toward the ground (light travel direction).
		// We want the direction toward the moon — the opposite.
		const FVector  Dir     = -MoonRotation.Vector();
		// Tidal lock: keep a fixed face toward the viewer so surface features don't drift as the
		// moon crosses the sky. Spinning the sphere doesn't affect the phase — its geometric
		// normals stay radial in world space.
		const FRotator MeshRot = (-Dir).Rotation();

		// Subtle size variation: ~7% larger at perigee vs apogee (384400 km mean).
		const float SizeFactor = 384400.0f / FMath::Max(MoonDistanceKm, 356500.0f);
		const float Scale      = ScaleForAngularDiameter(MoonAngularDiameter, MoonMeshDistance, MoonMeshLocalRadius)
		                         * SizeFactor * HorizonSizeFactor(CorrectedAlt);
		const FVector Target   = GetObserverLocation() + Dir * MoonMeshDistance;
		// Centre the bounds on Target so the assigned mesh's base-pivot doesn't offset the disc.
		const FVector CentreOffset = MeshRot.RotateVector(MoonMeshLocalCenter * Scale);
		MoonMesh->SetWorldScale3D(FVector(Scale));
		MoonMesh->SetWorldRotation(MeshRot);
		MoonMesh->SetWorldLocation(Target - CentreOffset);
		MoonMesh->SetVisibility(CorrectedAlt > -5.0f);

		// Feed the sun's world direction to the moon material so it can light the
		// sun-facing hemisphere and draw the terminator — i.e. the visible phase.
		// The sun is ~400× farther than the moon, so observer→sun ≈ moon→sun.
		if (MoonMID)
		{
			const float   SunTrueElev = CachedSunData.Elevation - 180.0f;
			const FVector SunDir = FRotator(SunTrueElev, CachedSunData.Azimuth + CelestialNorthYaw, 0.0f).Vector();
			MoonMID->SetVectorParameterValue(TEXT("SunDirection"),
				FLinearColor(SunDir.X, SunDir.Y, SunDir.Z, 0.0f));
		}
	}
}

FString AAstroDayNightManager::GetMoonPhaseName() const
{
	if      (MoonPhase < 0.0625f || MoonPhase >= 0.9375f) return TEXT("New Moon");
	else if (MoonPhase < 0.1875f) return TEXT("Waxing Crescent");
	else if (MoonPhase < 0.3125f) return TEXT("First Quarter");
	else if (MoonPhase < 0.4375f) return TEXT("Waxing Gibbous");
	else if (MoonPhase < 0.5625f) return TEXT("Full Moon");
	else if (MoonPhase < 0.6875f) return TEXT("Waning Gibbous");
	else if (MoonPhase < 0.8125f) return TEXT("Last Quarter");
	else                           return TEXT("Waning Crescent");
}

//──────────────────────────────────────────────────────────────────────────────
// Meeus Chapter 47 — Lunar Ecliptic Coordinates
//──────────────────────────────────────────────────────────────────────────────

void AAstroDayNightManager::ComputeLunarEcliptic(double T,
	double& OutLon, double& OutLat, double& OutDist) const
{
	// ── Fundamental arguments (Meeus Eq. 47.1, degrees) ──────────────────────
	const double Lp = Norm360(218.3164477  + 481267.88123421*T
	                        - 0.0015786*T*T + T*T*T/538841.0
	                        - T*T*T*T/65194000.0);

	const double D  = Norm360(297.8501921  + 445267.1114034*T
	                        - 0.0018819*T*T + T*T*T/545868.0
	                        - T*T*T*T/113065000.0);

	const double M  = Norm360(357.5291092  + 35999.0502909*T
	                        - 0.0001536*T*T + T*T*T/24490000.0);

	const double Mp = Norm360(134.9633964  + 477198.8675055*T
	                        + 0.0087414*T*T + T*T*T/69699.0
	                        - T*T*T*T/14712000.0);

	const double F  = Norm360(93.2720950   + 483202.0175233*T
	                        - 0.0036539*T*T - T*T*T/3526000.0
	                        + T*T*T*T/863310000.0);

	// Additional argument terms for corrections
	const double A1 = Norm360(119.75 + 131.849*T);
	const double A2 = Norm360( 53.09 + 479264.290*T);
	const double A3 = Norm360(313.45 + 481266.484*T);

	// Convert to radians
	const double Lp_r = DToR(Lp), D_r  = DToR(D),  M_r  = DToR(M);
	const double Mp_r = DToR(Mp), F_r  = DToR(F);
	const double A1_r = DToR(A1), A2_r = DToR(A2), A3_r = DToR(A3);

	// ── Eccentricity correction ───────────────────────────────────────────────
	const double E  = 1.0 - 0.002516*T - 0.0000074*T*T;
	const double E2 = E * E;

	// ── Periodic terms — longitude and distance (Table 47.A) ─────────────────
	// Coefficients: Sl in units of 1e-6 degrees, Sr in units of 1e-3 km
	// Terms with |M|=1 are multiplied by E, |M|=2 by E²
	struct FLonTerm { int D, M, Mp, F; double Sl, Sr; };
	static const FLonTerm LonTerms[] = {
		{ 0,  0,  1,  0,  6288774.0, -20905355.0 },
		{ 2,  0, -1,  0,  1274027.0,  -3699111.0 },
		{ 2,  0,  0,  0,   658314.0,  -2955968.0 },
		{ 0,  0,  2,  0,   213618.0,   -569925.0 },
		{ 0,  1,  0,  0,  -185116.0,    +48888.0 },
		{ 0,  0,  0,  2,  -114332.0,     -3149.0 },
		{ 2,  0, -2,  0,    58793.0,   +246158.0 },
		{ 2, -1, -1,  0,    57066.0,   -152138.0 },
		{ 2,  0,  1,  0,    53322.0,   -170733.0 },
		{ 2, -1,  0,  0,    45758.0,   -204586.0 },
		{ 0,  1, -1,  0,   -40923.0,   -129620.0 },
		{ 1,  0,  0,  0,   -34720.0,   +108743.0 },
		{ 0,  1,  1,  0,   -30383.0,   +104755.0 },
		{ 2,  0,  0, -2,    15327.0,    +10321.0 },
		{ 0,  0,  1,  2,   -12528.0,         0.0 },
		{ 0,  0,  1, -2,    10980.0,    +79661.0 },
		{ 4,  0, -1,  0,    10675.0,    -34782.0 },
		{ 0,  0,  3,  0,    10034.0,    -23210.0 },
		{ 4,  0, -2,  0,     8548.0,    -21636.0 },
		{ 2,  1, -1,  0,    -7888.0,    +24208.0 },
		{ 2,  1,  0,  0,    -6766.0,    +30824.0 },
		{ 1,  0, -1,  0,    -5163.0,     -8379.0 },
		{ 1,  1,  0,  0,     4987.0,    -16675.0 },
		{ 2, -1,  1,  0,     4036.0,    -12831.0 },
		{ 2,  0,  2,  0,     3994.0,    -10445.0 },
		{ 4,  0,  0,  0,     3861.0,    -11650.0 },
		{ 2,  0, -3,  0,     3665.0,    +14403.0 },
		{ 0,  1, -2,  0,    -2689.0,     -7003.0 },
		{ 2,  0, -1,  2,    -2602.0,         0.0 },
		{ 2, -1, -2,  0,     2390.0,    +10056.0 },
		{ 1,  0,  1,  0,    -2348.0,     +6322.0 },
		{ 2, -2,  0,  0,     2236.0,     -9884.0 },
		{ 0,  1,  2,  0,    -2120.0,     +5751.0 },
		{ 0,  2,  0,  0,    -2069.0,         0.0 },
		{ 2, -2, -1,  0,     2048.0,     -4950.0 },
	};

	// ── Periodic terms — latitude (Table 47.B) ────────────────────────────────
	// Coefficients: Sb in units of 1e-6 degrees
	struct FLatTerm { int D, M, Mp, F; double Sb; };
	static const FLatTerm LatTerms[] = {
		{ 0,  0,  0,  1,  5128122.0 },
		{ 0,  0,  1,  1,   280602.0 },
		{ 0,  0,  1, -1,   277693.0 },
		{ 2,  0,  0, -1,   173237.0 },
		{ 2,  0, -1,  1,    55413.0 },
		{ 2,  0, -1, -1,    46271.0 },
		{ 2,  0,  0,  1,    32573.0 },
		{ 0,  0,  2,  1,    17198.0 },
		{ 2,  0,  1, -1,     9266.0 },
		{ 0,  0,  2, -1,     8822.0 },
		{ 2, -1,  0, -1,     8216.0 },
		{ 2,  0, -2, -1,     4324.0 },
		{ 2,  0,  1,  1,     4200.0 },
		{ 2,  1,  0, -1,    -3359.0 },
		{ 2, -1, -1,  1,     2463.0 },
		{ 2, -1,  0,  1,     2211.0 },
		{ 2, -1, -1, -1,     2065.0 },
		{ 0,  1, -1, -1,    -1870.0 },
		{ 4,  0, -1, -1,     1828.0 },
		{ 0,  1,  0,  1,    -1794.0 },
	};

	// ── Sum longitude terms ───────────────────────────────────────────────────
	double SumL = 0.0, SumR = 0.0;
	for (const auto& t : LonTerms)
	{
		const double arg = t.D*D_r + t.M*M_r + t.Mp*Mp_r + t.F*F_r;
		const double Ec  = (FMath::Abs(t.M) == 2) ? E2
		                 : (FMath::Abs(t.M) == 1) ? E : 1.0;
		SumL += Ec * t.Sl * FMath::Sin(arg);
		SumR += Ec * t.Sr * FMath::Cos(arg);
	}

	// Additional longitude corrections (Meeus p. 338)
	SumL += 3958.0 * FMath::Sin(A1_r)
	      + 1962.0 * FMath::Sin(Lp_r - F_r)
	      +  318.0 * FMath::Sin(A2_r);

	// ── Sum latitude terms ────────────────────────────────────────────────────
	double SumB = 0.0;
	for (const auto& t : LatTerms)
	{
		const double arg = t.D*D_r + t.M*M_r + t.Mp*Mp_r + t.F*F_r;
		const double Ec  = (FMath::Abs(t.M) == 2) ? E2
		                 : (FMath::Abs(t.M) == 1) ? E : 1.0;
		SumB += Ec * t.Sb * FMath::Sin(arg);
	}

	// Additional latitude corrections
	SumB += -2235.0 * FMath::Sin(Lp_r)
	        +  382.0 * FMath::Sin(A3_r)
	        +  175.0 * FMath::Sin(A1_r - F_r)
	        +  175.0 * FMath::Sin(A1_r + F_r)
	        +  127.0 * FMath::Sin(Lp_r - Mp_r)
	        -  115.0 * FMath::Sin(Lp_r + Mp_r);

	// ── Results ───────────────────────────────────────────────────────────────
	OutLon  = Norm360(Lp + SumL * 1.0e-6);   // apparent geocentric ecliptic longitude
	OutLat  = SumB * 1.0e-6;                  // geocentric ecliptic latitude
	OutDist = 385000.56 + SumR * 1.0e-3;      // Earth-Moon distance (km)
}

//──────────────────────────────────────────────────────────────────────────────
// Coordinate Conversions
//──────────────────────────────────────────────────────────────────────────────

void AAstroDayNightManager::EclipticToEquatorial(
	double LambdaDeg, double BetaDeg, double EpsilonDeg,
	double& OutRA, double& OutDec) const
{
	const double L   = DToR(LambdaDeg);
	const double B   = DToR(BetaDeg);
	const double Eps = DToR(EpsilonDeg);

	// Meeus Eq. 13.3 / 13.4
	OutRA  = RToD(FMath::Atan2(
		FMath::Sin(L)*FMath::Cos(Eps) - FMath::Tan(B)*FMath::Sin(Eps),
		FMath::Cos(L)));
	OutRA  = Norm360(OutRA);

	OutDec = RToD(FMath::Asin(
		FMath::Sin(B)*FMath::Cos(Eps) +
		FMath::Cos(B)*FMath::Sin(Eps)*FMath::Sin(L)));
}

void AAstroDayNightManager::EquatorialToHorizontal(
	double RA, double Dec, double GMST,
	double Lat, double Lon,
	double& OutAz, double& OutAlt) const
{
	// Local Mean Sidereal Time and hour angle (degrees)
	const double LMST = Norm360(GMST + Lon);
	const double H    = DToR(Norm360(LMST - RA));   // hour angle, positive westward

	const double LatR = DToR(Lat);
	const double DecR = DToR(Dec);

	// Altitude
	const double SinAlt = FMath::Sin(LatR)*FMath::Sin(DecR)
	                    + FMath::Cos(LatR)*FMath::Cos(DecR)*FMath::Cos(H);
	OutAlt = RToD(FMath::Asin(SinAlt));

	// Azimuth from North, clockwise (0=N, 90=E, 180=S, 270=W)
	const double Az = RToD(FMath::Atan2(
		-FMath::Cos(DecR) * FMath::Sin(H),
		FMath::Sin(DecR)*FMath::Cos(LatR) - FMath::Cos(DecR)*FMath::Cos(H)*FMath::Sin(LatR)));
	OutAz = Norm360(Az);
}

float AAstroDayNightManager::ApplyAtmosphericRefraction(float AltDeg) const
{
	// Bennett (1982) formula — accurate to 0.07' for alt > -5°
	// Below -1° the refraction is large and variable; clamp the correction there.
	if (AltDeg < -1.0f) return AltDeg;

	// R in arcminutes
	const double dAlt = static_cast<double>(AltDeg);
	const double R    = 1.02 / FMath::Tan(DToR(dAlt + 10.3 / (dAlt + 5.11)));
	return AltDeg + static_cast<float>(R / 60.0);   // convert arcmin → degrees
}

//──────────────────────────────────────────────────────────────────────────────
// Julian Date and Sidereal Time
//──────────────────────────────────────────────────────────────────────────────

double AAstroDayNightManager::ComputeJulianDate() const
{
	const int32 Y  = CurrentDateTime.GetYear();
	const int32 Mo = CurrentDateTime.GetMonth();
	const int32 D  = CurrentDateTime.GetDay();
	const int32 H  = CurrentDateTime.GetHour();
	const int32 Mi = CurrentDateTime.GetMinute();
	const int32 S  = CurrentDateTime.GetSecond();

	// Meeus Chapter 7 — proleptic Gregorian calendar
	const int32 A  = (14 - Mo) / 12;
	const int32 Yr = Y + 4800 - A;
	const int32 M  = Mo + 12*A - 3;

	double JD = D
	          + (153*M + 2) / 5
	          + 365*Yr
	          + Yr/4 - Yr/100 + Yr/400
	          - 32045;

	// CurrentDateTime is the *local* clock, but the Julian Date must be in UT — subtract
	// the effective UTC offset (time zone + DST) before applying the noon-UT epoch (-12h).
	// Without this, the moon and stars ran (TimeZone + DST) hours off from the sun.
	const double UtcOffsetHours = static_cast<double>(TimeZone) + (bDaylightSaving ? 1.0 : 0.0);
	JD += (H - 12.0 - UtcOffsetHours) / 24.0 + Mi / 1440.0 + S / 86400.0;
	return JD;
}

double AAstroDayNightManager::ComputeGMST(double JD) const
{
	// Meeus Eq. 12.4 — Greenwich Mean Sidereal Time in degrees
	const double T = (JD - 2451545.0) / 36525.0;
	double GMST = 280.46061837
	            + 360.98564736629 * (JD - 2451545.0)
	            + 0.000387933 * T*T
	            - T*T*T / 38710000.0;
	return Norm360(GMST);
}

double AAstroDayNightManager::ComputeObliquity(double T) const
{
	// Meeus Eq. 22.2 — mean obliquity of the ecliptic (degrees)
	// Accurate to ~0.001° over several centuries around J2000
	return 23.439291111
	     - 0.013004167 * T
	     - 0.0001638889 * T*T
	     + 0.0503611111 * T*T*T;
}

//──────────────────────────────────────────────────────────────────────────────
// Sky Light
//──────────────────────────────────────────────────────────────────────────────

void AAstroDayNightManager::UpdateSkyLight()
{
	// T = 0 at night (-10° and below), T = 1 at day (+10° and above)
	const float T = FMath::Clamp((SunElevation + 10.0f) / 20.0f, 0.0f, 1.0f);

	if (SkyLight)
	{
		// Lift the night floor by the moon's illumination — a full, high moon brightens
		// the whole sky; a new moon leaves just the base floor.
		const float NightFloor = NightSkyLightFloor + MoonIllumination * MoonAmbientBoost;
		SkyLight->SetIntensity(FMath::Lerp(NightFloor, 1.0f, T));
	}

	// Always-on dim fill that ramps in at night so the scene never goes pure black,
	// independent of the moon. Aimed along the camera so whatever you look at is lifted.
	if (NightFillLight)
	{
		// Fade the fill out as the moon takes over — full before moonrise / on moonless
		// nights, gone under a bright high moon. It's purely the no-moon visibility backup.
		NightFillLight->SetIntensity(NightFillIntensity * (1.0f - T) * (1.0f - MoonIllumination));
		NightFillLight->SetLightColor(NightFillColor);
		if (const UWorld* W = GetWorld())
		{
			if (const APlayerController* PC = W->GetFirstPlayerController())
			{
				if (PC->PlayerCameraManager)
				{
					NightFillLight->SetWorldRotation(PC->PlayerCameraManager->GetCameraRotation());
				}
			}
		}
	}

	if (SkyAtmosphere)
	{
		// Fade Rayleigh scattering to near-zero at night so the zenith
		// doesn't glow blue independently of the sun position
		SkyAtmosphere->RayleighScatteringScale = FMath::Lerp(0.002f, 0.0331f, T);
		SkyAtmosphere->MarkRenderStateDirty();
	}

	// Night fog glow — a dim emissive in the volumetric fog for ground-level night
	// visibility before the moon is up. Fades out during the day.
	if (HeightFog)
	{
		if (UExponentialHeightFogComponent* FogComp = HeightFog->GetComponent())
		{
			FogComp->SetVolumetricFogEmissive(NightFogGlow * (NightFogGlowScale * (1.0f - T)));
		}
	}
}

//──────────────────────────────────────────────────────────────────────────────
// Stars
//──────────────────────────────────────────────────────────────────────────────

void AAstroDayNightManager::UpdateStars()
{
	if (!StarDome) return;

	// ── Brightness: fade in as the sun sinks below the horizon ──────────────────
	const float FadeRange = StarFadeStartElevation - StarFadeEndElevation;
	const float T = (FMath::Abs(FadeRange) > KINDA_SMALL_NUMBER)
		? FMath::Clamp((SunElevation - StarFadeEndElevation) / FadeRange, 0.0f, 1.0f)
		: (SunElevation <= StarFadeEndElevation ? 0.0f : 1.0f);
	const float Brightness = (1.0f - T) * StarMaxBrightness;

	const bool bVisible = Brightness > KINDA_SMALL_NUMBER;
	StarDome->SetVisibility(bVisible);
	if (!bVisible) return;

	if (StarMID)
	{
		StarMID->SetScalarParameterValue(StarBrightnessParam, Brightness);
	}

	// ── Pin the dome to the actor (NOT the camera). Re-anchoring it to the camera
	//    every frame fed the velocity buffer phantom motion and motion-blurred the
	//    stars; at this radius the parallax from the viewer moving is negligible, so
	//    it sits still and only rotates.
	StarDome->SetWorldLocation(GetActorLocation());
	StarDome->SetWorldScale3D(FVector(StarDomeScale));

	if (!bStarsRotateWithSky) return;

	// ── Orientation: rotate the celestial sphere by sidereal time + latitude ────
	const double JD   = ComputeJulianDate();
	const double GMST = ComputeGMST(JD);

	// Same (Az,Alt) → world-direction convention the moon mesh uses
	auto HorizDir = [](double AzDeg, double AltDeg) -> FVector
	{
		// Sky-position direction in the shared sun/moon azimuth frame
		return FRotator(static_cast<float>(AltDeg),
		                static_cast<float>(AzDeg), 0.0f).Vector();
	};

	// North celestial pole (Dec = +90) and an equator reference point (RA = 0, Dec = 0)
	double AzP, AltP, AzR, AltR;
	EquatorialToHorizontal(0.0, 90.0, GMST,
		static_cast<double>(Latitude), static_cast<double>(Longitude), AzP, AltP);
	EquatorialToHorizontal(0.0,  0.0, GMST,
		static_cast<double>(Latitude), static_cast<double>(Longitude), AzR, AltR);

	FVector PoleDir = HorizDir(AzP + CelestialNorthYaw, AltP).GetSafeNormal();
	FVector RefDir  = HorizDir(AzR + CelestialNorthYaw, AltR).GetSafeNormal();

	// One-time spin about the pole to line the texture's RA = 0 up with reality
	if (!FMath::IsNearlyZero(StarYawOffset))
	{
		RefDir = RefDir.RotateAngleAxis(StarYawOffset, PoleDir);
	}

	// Dome local +Z → celestial pole, local +X → RA 0 on the celestial equator
	const FRotator DomeRot = FRotationMatrix::MakeFromXZ(RefDir, PoleDir).Rotator();
	StarDome->SetWorldRotation(DomeRot);
}

//──────────────────────────────────────────────────────────────────────────────
// Utilities
//──────────────────────────────────────────────────────────────────────────────

FVector AAstroDayNightManager::GetObserverLocation() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
				return PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	return GetActorLocation();
}
