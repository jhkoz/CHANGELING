// AstroDayNightManager.h
// Astronomically accurate day/night cycle for UE 5.7.
// Solar position via USunPositionFunctionLibrary (Spencer/Iqbal).
// Lunar position via truncated Meeus Chapter 47 theory (~0.3° accuracy).
// Coordinate chain: ecliptic → equatorial → horizontal (with atmospheric refraction).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "SunPosition.h"
#include "Delegates/DelegateCombinations.h"
#include "AstroDayNightManager.generated.h"

class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class AWeatherController;
class AExponentialHeightFog;

// ── Delegates ─────────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeChanged,  float,     TimeOfDayHours);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayChanged,   int32,     NewDayOfMonth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSunriseEvent, FDateTime, DateTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSunsetEvent,  FDateTime, DateTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDawnEvent,    FDateTime, DateTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDuskEvent,    FDateTime, DateTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoonriseEvent,FDateTime, DateTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoonsetEvent, FDateTime, DateTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFullMoonEvent,FDateTime, DateTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNewMoonEvent, FDateTime, DateTime);

// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class CHANGELING_API AAstroDayNightManager : public AActor
{
	GENERATED_BODY()

public:
	AAstroDayNightManager();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	//──────────────────────────────────────────────────────────────
	// Components
	//──────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Sun")
	UDirectionalLightComponent* SunLight;

	/** Assign a sphere mesh in the Blueprint to show the sun disc (mirrors MoonMesh).
	 *  A bright translucent material lets the SunLight transmit through it for a real glow. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Sun")
	UStaticMeshComponent* SunMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Moon")
	UDirectionalLightComponent* MoonLight;

	/** Dim shadowless fill, ramps in at night, guarantees the scene is never pitch black */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Sky")
	UDirectionalLightComponent* NightFillLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Sky")
	USkyLightComponent* SkyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Sky")
	USkyAtmosphereComponent* SkyAtmosphere;

	/** Assign a sphere mesh in the Blueprint to show the moon */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Moon")
	UStaticMeshComponent* MoonMesh;

	/** Inverted-normal sphere + Unlit/Additive star material for the night sky */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Stars")
	UStaticMeshComponent* StarDome;

	//──────────────────────────────────────────────────────────────
	// Time
	//──────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Time")
	FDateTime CurrentDateTime;

	/** Real-to-game time multiplier. 60 = 1 real second → 1 game minute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Time", meta = (ClampMin = "0.0"))
	float TimeScale = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Time")
	bool bPaused = false;

	//──────────────────────────────────────────────────────────────
	// Location
	//──────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Location",
		meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float Latitude = 42.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Location",
		meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float Longitude = -74.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Location",
		meta = (ClampMin = "-12.0", ClampMax = "14.0"))
	float TimeZone = -5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Location")
	bool bDaylightSaving = true;   // default date is June (EDT); turn off for winter

	//──────────────────────────────────────────────────────────────
	// Sun
	//──────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sun")
	float SunMaxIntensity = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sun")
	float SunHorizonIntensity = 0.5f;

	/** Angular diameter of the sun disc in degrees. Real sun = 0.53° (tiny!); ~1.5 reads better. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sun", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float SunDiscAngle = 1.5f;

	/** Warm colour tint applied to the sun disc by the Sky Atmosphere */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sun")
	FLinearColor SunDiscColor = FLinearColor(1.3f, 1.05f, 0.75f);

	/** Distance from the camera to the sun disc mesh (matches the moon's by default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sun")
	float SunMeshDistance = 600000.0f;

	/** Apparent angular diameter of the sun disc, in degrees (the real sun is ≈ 0.53°; ~12° reads
	 *  as a bold, stylised sun). The world scale is derived from this and the assigned mesh's actual
	 *  bounds every frame, so it stays correct for whatever mesh you drop on SunMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sun", meta = (ClampMin = "0.1", ClampMax = "45.0"))
	float SunAngularDiameter = 11.04f;   // 1.15 × the moon's 9.6° — nearly equal, sun a touch larger

	/** Fakes the "Moon Illusion": disc/mesh size multiplier at the horizon, fading to 1 as the
	 *  body climbs. Applies to BOTH sun and moon. 1.0 = camera-accurate (off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sun", meta = (ClampMin = "1.0"))
	float HorizonSizeBoost = 1.3f;

	/** Elevation (deg) above which the horizon size boost has faded back to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sun", meta = (ClampMin = "1.0"))
	float HorizonBoostFadeDeg = 25.0f;


	//──────────────────────────────────────────────────────────────
	// Moon
	//──────────────────────────────────────────────────────────────

	/** Peak moonlight illuminance (lux). Stylized: with SunMaxIntensity = 10 this is ~3.5%
	 *  of the sun — a clearly readable full-moon night. Drop toward 0.04 for a near-real,
	 *  barely-there country moon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Moon")
	float MoonMaxIntensity = 0.35f;

	/** Make the moon the second Sky Atmosphere light (index 1). The night sky then scatters
	 *  faint moonlight, the real-time SkyLight capture turns non-black, and the night ambient
	 *  (NightSkyLightFloor / MoonAmbientBoost) actually reaches the ground. Costs a little
	 *  star contrast on bright moonlit nights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Moon")
	bool bMoonLightsSky = true;

	/** Distance from the camera to the moon disc. Kept CLOSER than SunMeshDistance so the moon
	 *  always renders in front of (occludes) the sun during a transit/eclipse. Apparent size is
	 *  unaffected — it's driven by MoonAngularDiameter regardless of distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Moon")
	float MoonMeshDistance = 500000.0f;

	/** Apparent angular diameter of the moon disc, in degrees (the real moon is ≈ 0.52°). Derived
	 *  to world scale from the assigned mesh's bounds every frame, so any mesh sizes correctly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Moon", meta = (ClampMin = "0.1", ClampMax = "45.0"))
	float MoonAngularDiameter = 9.6f;

	//──────────────────────────────────────────────────────────────
	// Sky / night ambient
	//──────────────────────────────────────────────────────────────

	/** SkyLight ambient intensity at full night (day = 1.0). Raise to see the world;
	 *  too high washes out the stars. Moonlight (MoonMaxIntensity) is the nicer source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky", meta = (ClampMin = "0.0"))
	float NightSkyLightFloor = 0.02f;   // dark-sky floor; the moon supplies the rest

	/** Peak intensity of the always-on night fill, aimed along the view. 0 (default) keeps a
	 *  true dark-sky / country night; raise it to guarantee visibility on moonless nights at
	 *  the cost of star contrast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky", meta = (ClampMin = "0.0"))
	float NightFillIntensity = 0.0f;

	/** Tint of the night fill light */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky")
	FLinearColor NightFillColor = FLinearColor(0.5f, 0.6f, 0.9f);

	/** How much a full, high moon lifts the night SkyLight ambient (0 = moon doesn't brighten it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky", meta = (ClampMin = "0.0"))
	float MoonAmbientBoost = 0.3f;

	/** WeatherController whose cloud cover dims the sun/moon. Auto-found if left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky")
	AWeatherController* Weather;

	/** How much full overcast dims the sun and moon (0.85 → overcast cuts them to ~15%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CloudDimStrength = 0.85f;

	/** ExponentialHeightFog whose volumetric fog glows at night for ambient visibility.
	 *  Auto-found if empty. (Enable Volumetric Fog on the fog actor for this to show.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky")
	AExponentialHeightFog* HeightFog;

	/** Colour of the night fog glow — a dim ambient lift before the moon rises. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky")
	FLinearColor NightFogGlow = FLinearColor(0.4f, 0.5f, 0.8f);

	/** Brightness of the night fog glow (0 = off). Driven up at night, off by day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Sky", meta = (ClampMin = "0.0"))
	float NightFogGlowScale = 0.1f;

	//──────────────────────────────────────────────────────────────
	// Stars
	//──────────────────────────────────────────────────────────────

	/** Uniform scale of the star dome. Keep it beyond MoonMeshDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars")
	float StarDomeScale = 160000.0f;

	/** Emissive multiplier handed to the star material at full night */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars")
	float StarMaxBrightness = 1.5f;

	/** Sun elevation (deg) at/above which the stars are fully hidden */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars")
	float StarFadeStartElevation = -3.0f;

	/** Sun elevation (deg) at/below which the stars reach full brightness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars")
	float StarFadeEndElevation = -16.0f;

	/** Wheel the stars with sidereal time + latitude (astronomically accurate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars")
	bool bStarsRotateWithSky = true;

	/** One-time yaw (deg) about the celestial pole to align the texture's RA */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars")
	float StarYawOffset = 0.0f;

	/** Scalar parameter in the star material the manager drives for brightness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars")
	FName StarBrightnessParam = "StarBrightness";

	//──────────────────────────────────────────────────────────────
	// Real star catalogue (baked equirectangular texture)
	//──────────────────────────────────────────────────────────────

	/** Bake real star positions into the dome texture at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars|Catalog")
	bool bGenerateRealStars = true;

	/** Width of the baked star texture (height = width/2). 4096 ≈ 67 MB at RGBA16f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars|Catalog", meta = (ClampMin = "512"))
	int32 StarTextureWidth = 4096;

	/** Faintest apparent magnitude to draw (naked-eye limit ≈ 6.0; 6.5 for a dark country sky). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars|Catalog")
	float StarMagnitudeLimit = 6.5f;

	/** Faint background stars scattered for density (0 = catalogue only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars|Catalog", meta = (ClampMin = "0"))
	int32 StarProceduralFill = 8000;

	/** Optional HYG-format CSV (ra,dec,mag,ci) under the project dir for the full ~9k sky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars|Catalog")
	FString StarCatalogCsv;

	/** Texture parameter in the star material the baked sky is plugged into. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astronomy|Stars|Catalog")
	FName StarTextureParam = "StarTexture";

	//──────────────────────────────────────────────────────────────
	// Read-only state
	//──────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|State")
	float SunElevation = 0.0f;

	/** 0 = new moon  0.25 = first quarter  0.5 = full  0.75 = last quarter */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|State")
	float MoonPhase = 0.0f;

	/** Computed moon altitude above the horizon in degrees */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|State")
	float MoonAltitude = 0.0f;

	/** Earth-Moon distance in kilometres (varies ~356,500 – 406,700 km) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|State")
	float MoonDistanceKm = 384400.0f;

	/** Moon's current light contribution [0,1] = phase × altitude (drives moonlight + ambient) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|State")
	float MoonIllumination = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|State")
	bool bIsDay = false;

	//──────────────────────────────────────────────────────────────
	// Events
	//──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnTimeChanged OnTimeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnDayChanged OnDayChanged;

	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnSunriseEvent OnSunrise;

	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnSunsetEvent OnSunset;

	/** Fires when the sun crosses -6° rising — civil twilight begins */
	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnDawnEvent OnDawn;

	/** Fires when the sun crosses -6° descending — civil twilight ends */
	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnDuskEvent OnDusk;

	/** Fires when the moon crosses the horizon rising */
	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnMoonriseEvent OnMoonrise;

	/** Fires when the moon crosses the horizon setting */
	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnMoonsetEvent OnMoonset;

	/** Fires once when the moon enters the full moon window (phase 0.46 – 0.54) */
	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnFullMoonEvent OnFullMoon;

	/** Fires once when the moon enters the new moon window (phase < 0.04 or > 0.96) */
	UPROPERTY(BlueprintAssignable, Category = "Astronomy|Events")
	FOnNewMoonEvent OnNewMoon;

	//──────────────────────────────────────────────────────────────
	// Utilities
	//──────────────────────────────────────────────────────────────

	/** Set the time of day by decimal hour. 6.5 = 06:30, 23.75 = 23:45 */
	UFUNCTION(BlueprintCallable, Category = "Astronomy|Time")
	void SetTimeOfDay(float Hours);

	UFUNCTION(BlueprintPure, Category = "Astronomy|Time")
	float GetTimeOfDayHours() const;

	UFUNCTION(BlueprintPure, Category = "Astronomy|Time")
	float GetNormalizedTimeOfDay() const;

	UFUNCTION(BlueprintPure, Category = "Astronomy|Moon")
	FString GetMoonPhaseName() const;

private:
	//──────────────────────────────────────────────────────────────
	// Update pipeline
	//──────────────────────────────────────────────────────────────
	void AdvanceTime(float DeltaTime);
	void UpdateSun();
	void UpdateMoon();
	void UpdateSkyLight();
	void UpdateStars();

	//──────────────────────────────────────────────────────────────
	// Astronomical computation
	//──────────────────────────────────────────────────────────────

	/** Julian Date from CurrentDateTime */
	double ComputeJulianDate() const;

	/** Greenwich Mean Sidereal Time in degrees */
	double ComputeGMST(double JD) const;

	/** Mean obliquity of the ecliptic in degrees */
	double ComputeObliquity(double T) const;

	/**
	 * Truncated Meeus Chapter 47 lunar theory.
	 * Accurate to ~0.3° in longitude, ~0.2° in latitude.
	 * @param T        Julian centuries from J2000.0
	 * @param OutLon   Apparent geocentric ecliptic longitude (degrees)
	 * @param OutLat   Geocentric ecliptic latitude (degrees)
	 * @param OutDist  Earth-Moon distance (km)
	 */
	void ComputeLunarEcliptic(double T,
		double& OutLon, double& OutLat, double& OutDist) const;

	/** Ecliptic → equatorial (J2000 frame, no precession needed at game timescales) */
	void EclipticToEquatorial(double LambdaDeg, double BetaDeg, double EpsilonDeg,
		double& OutRA, double& OutDec) const;

	/**
	 * Equatorial → horizontal.
	 * @param RA       Right ascension (degrees)
	 * @param Dec      Declination (degrees)
	 * @param GMST     Greenwich Mean Sidereal Time (degrees)
	 * @param Lat      Observer latitude (degrees)
	 * @param Lon      Observer longitude (degrees, east positive)
	 * @param OutAz    Azimuth from North, clockwise (degrees)
	 * @param OutAlt   Altitude above horizon (degrees, negative = below)
	 */
	void EquatorialToHorizontal(double RA, double Dec, double GMST,
		double Lat, double Lon,
		double& OutAz, double& OutAlt) const;

	/** Bennett atmospheric refraction formula. Input/output in degrees. */
	float ApplyAtmosphericRefraction(float AltitudeDeg) const;

	//──────────────────────────────────────────────────────────────
	// Lighting helpers
	//──────────────────────────────────────────────────────────────
	float        ComputeSunIntensity()  const;
	FLinearColor ComputeSunColor()      const;
	float        HorizonSizeFactor(float ElevationDeg) const;
	float        CloudDimFactor()       const;
	FVector      GetObserverLocation()  const;

	//──────────────────────────────────────────────────────────────
	// State
	//──────────────────────────────────────────────────────────────
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* StarMID = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MoonMID = nullptr;

	UPROPERTY(Transient)
	UTextureRenderTarget2D* StarRenderTarget = nullptr;

	FSunPositionData CachedSunData;
	bool  bWasDay            = false;
	bool  bWasCivilTwilight  = false;  // sun was above -6°
	bool  bWasMoonUp         = false;  // moon was above horizon
	bool  bWasInFullMoonZone = false;  // phase was in [0.46, 0.54]
	bool  bWasInNewMoonZone  = false;  // phase was in [0.0, 0.04] or [0.96, 1.0]
	int32 PrevDay            = -1;

	// Cached unscaled bounds of the assigned sun/moon meshes, captured at BeginPlay. Lets the disc
	// size derive from an angular diameter, and re-centres a mesh whose pivot isn't at its centre.
	float   SunMeshLocalRadius  = 50.0f;
	float   MoonMeshLocalRadius = 50.0f;
	FVector SunMeshLocalCenter  = FVector::ZeroVector;
	FVector MoonMeshLocalCenter = FVector::ZeroVector;
};
