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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy|Sky")
	UStaticMeshComponent* StarDome;

	//──────────────────────────────────────────────────────────────
	// Time
	//──────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
	FDateTime CurrentDateTime;

	/** Real-to-game time multiplier. 60 = 1 real second → 1 game minute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time", meta = (ClampMin = "0.0"))
	float TimeScale = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
	bool bPaused = false;

	//──────────────────────────────────────────────────────────────
	// Location
	//──────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location",
		meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float Latitude = 42.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location",
		meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float Longitude = -74.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location",
		meta = (ClampMin = "-12.0", ClampMax = "14.0"))
	float TimeZone = -5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	bool bDaylightSaving = true;   // default date is June (EDT); turn off for winter

	//──────────────────────────────────────────────────────────────
	// Sun
	//──────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	float SunMaxIntensity = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	float SunHorizonIntensity = 0.5f;

	/** Angular diameter of the sun disc in degrees. Real sun = 0.53°. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float SunDiscAngle = 2.0f;

	/** Warm colour tint applied to the sun disc by the Sky Atmosphere */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	FLinearColor SunDiscColor = FLinearColor(1.3f, 1.05f, 0.75f);


	//──────────────────────────────────────────────────────────────
	// Moon
	//──────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	float MoonMaxIntensity = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	float MoonMeshDistance = 600000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	float MoonMeshScale = 1500.0f;

	//──────────────────────────────────────────────────────────────
	// Sky / night ambient
	//──────────────────────────────────────────────────────────────

	/** SkyLight ambient intensity at full night (day = 1.0). Raise to see the world;
	 *  too high washes out the stars. Moonlight (MoonMaxIntensity) is the nicer source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky", meta = (ClampMin = "0.0"))
	float NightSkyLightFloor = 0.5f;

	/** Peak intensity of the always-on night fill (guarantees visibility even on moonless
	 *  nights). Aimed along the view; 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky", meta = (ClampMin = "0.0"))
	float NightFillIntensity = 0.5f;

	/** Tint of the night fill light */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	FLinearColor NightFillColor = FLinearColor(0.5f, 0.6f, 0.9f);

	//──────────────────────────────────────────────────────────────
	// Stars
	//──────────────────────────────────────────────────────────────

	/** Uniform scale of the star dome. Keep it beyond MoonMeshDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stars")
	float StarDomeScale = 160000.0f;

	/** Emissive multiplier handed to the star material at full night */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stars")
	float StarMaxBrightness = 1.0f;

	/** Sun elevation (deg) at/above which the stars are fully hidden */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stars")
	float StarFadeStartElevation = -3.0f;

	/** Sun elevation (deg) at/below which the stars reach full brightness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stars")
	float StarFadeEndElevation = -16.0f;

	/** Wheel the stars with sidereal time + latitude (astronomically accurate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stars")
	bool bStarsRotateWithSky = true;

	/** One-time yaw (deg) about the celestial pole to align the texture's RA */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stars")
	float StarYawOffset = 0.0f;

	/** Scalar parameter in the star material the manager drives for brightness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stars")
	FName StarBrightnessParam = "StarBrightness";

	//──────────────────────────────────────────────────────────────
	// Read-only state
	//──────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float SunElevation = 0.0f;

	/** 0 = new moon  0.25 = first quarter  0.5 = full  0.75 = last quarter */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MoonPhase = 0.0f;

	/** Computed moon altitude above the horizon in degrees */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MoonAltitude = 0.0f;

	/** Earth-Moon distance in kilometres (varies ~356,500 – 406,700 km) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MoonDistanceKm = 384400.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsDay = false;

	//──────────────────────────────────────────────────────────────
	// Events
	//──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTimeChanged OnTimeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnDayChanged OnDayChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnSunriseEvent OnSunrise;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnSunsetEvent OnSunset;

	/** Fires when the sun crosses -6° rising — civil twilight begins */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnDawnEvent OnDawn;

	/** Fires when the sun crosses -6° descending — civil twilight ends */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnDuskEvent OnDusk;

	/** Fires when the moon crosses the horizon rising */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnMoonriseEvent OnMoonrise;

	/** Fires when the moon crosses the horizon setting */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnMoonsetEvent OnMoonset;

	/** Fires once when the moon enters the full moon window (phase 0.46 – 0.54) */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnFullMoonEvent OnFullMoon;

	/** Fires once when the moon enters the new moon window (phase < 0.04 or > 0.96) */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnNewMoonEvent OnNewMoon;

	//──────────────────────────────────────────────────────────────
	// Utilities
	//──────────────────────────────────────────────────────────────

	/** Set the time of day by decimal hour. 6.5 = 06:30, 23.75 = 23:45 */
	UFUNCTION(BlueprintCallable, Category = "Time")
	void SetTimeOfDay(float Hours);

	UFUNCTION(BlueprintPure, Category = "Time")
	float GetTimeOfDayHours() const;

	UFUNCTION(BlueprintPure, Category = "Time")
	float GetNormalizedTimeOfDay() const;

	UFUNCTION(BlueprintPure, Category = "Moon")
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
	FVector      GetObserverLocation()  const;

	//──────────────────────────────────────────────────────────────
	// State
	//──────────────────────────────────────────────────────────────
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* StarMID = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MoonMID = nullptr;

	FSunPositionData CachedSunData;
	bool  bWasDay            = false;
	bool  bWasCivilTwilight  = false;  // sun was above -6°
	bool  bWasMoonUp         = false;  // moon was above horizon
	bool  bWasInFullMoonZone = false;  // phase was in [0.46, 0.54]
	bool  bWasInNewMoonZone  = false;  // phase was in [0.0, 0.04] or [0.96, 1.0]
	int32 PrevDay            = -1;
};
