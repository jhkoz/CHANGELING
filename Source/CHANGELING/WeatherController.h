// WeatherController.h
//
// Data-driven weather controller for UE 5.7.
// Each weather type is a tunable preset (cloud, fog, wind, precipitation, wetness,
// snow). The controller blends smoothly between presets and pushes the result to:
//   • the ExponentialHeightFog        (driven directly)
//   • a Material Parameter Collection (CloudCoverage / WindStrength / Precipitation /
//                                      Wetness / Snow) that your cloud + surface
//                                      materials read
// Precipitation is also exposed live so you can drive a rain/snow Niagara from it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Delegates/DelegateCombinations.h"
#include "WeatherController.generated.h"

class AExponentialHeightFog;
class AVolumetricCloud;
class UMaterialParameterCollection;
class UMaterialInstanceDynamic;

// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EWeatherType : uint8
{
	Clear         UMETA(DisplayName = "Clear"),
	PartlyCloudy  UMETA(DisplayName = "Partly Cloudy"),
	Overcast      UMETA(DisplayName = "Overcast"),
	Foggy         UMETA(DisplayName = "Foggy"),
	Rain          UMETA(DisplayName = "Rain"),
	Storm         UMETA(DisplayName = "Storm"),
	Snow          UMETA(DisplayName = "Snow")
};

/** Target conditions for a single weather type. All 0–1 except fog density. */
USTRUCT(BlueprintType)
struct FWeatherPreset
{
	GENERATED_BODY()

	/** Cloud cover: 0 = clear, 1 = solid overcast */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CloudCoverage = 0.3f;

	/** Exponential height fog density */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float FogDensity = 0.02f;

	/** Wind strength 0–1 (drive cloud motion + precipitation slant from this) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WindStrength = 0.2f;

	/** Precipitation rate 0 (none) – 1 (downpour). Drive a rain/snow Niagara from this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Precipitation = 0.0f;

	/** Surface wetness 0–1 (read in your materials from the MPC) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Wetness = 0.0f;

	/** Snow coverage 0–1 (read in your materials from the MPC) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Snow = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeatherChanged, EWeatherType, NewWeather);

// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class CHANGELING_API AWeatherController : public AActor
{
	GENERATED_BODY()

public:
	AWeatherController();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	//──────────────────────────────────────────────────────────────
	// References
	//──────────────────────────────────────────────────────────────

	/** The level's ExponentialHeightFog — driven directly. Auto-found if left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|References")
	AExponentialHeightFog* HeightFog;

	/** The level's VolumetricCloud — its material's coverage scalar is driven directly.
	 *  Auto-found if left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|References")
	AVolumetricCloud* VolumetricCloud;

	/** Name of the coverage scalar in your cloud material (e.g. "Coverage"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|References")
	FName CloudCoverageParam = "Coverage";

	/** Material Parameter Collection the controller writes weather scalars into:
	 *  CloudCoverage, WindStrength, Precipitation, Wetness, Snow.
	 *  Read these in your cloud material + surface materials. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|References")
	UMaterialParameterCollection* WeatherParams;

	//──────────────────────────────────────────────────────────────
	// Config
	//──────────────────────────────────────────────────────────────

	/** Per-type target values. Pre-populated with sensible defaults; tune per instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TMap<EWeatherType, FWeatherPreset> Presets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	EWeatherType StartingWeather = EWeatherType::Clear;

	/** How fast conditions blend toward the target (higher = quicker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.01"))
	float TransitionSpeed = 1.0f;

	//──────────────────────────────────────────────────────────────
	// Live state (read-only)
	//──────────────────────────────────────────────────────────────

	/** The weather we're currently heading toward. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|State")
	EWeatherType CurrentWeather = EWeatherType::Clear;

	/** The blended-right-now conditions. Drive Niagara etc. from these. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|State")
	FWeatherPreset Current;

	/** Seconds until the randomizer rolls the next weather (when enabled). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weather|State")
	float TimeUntilNextRoll = 0.0f;

	//──────────────────────────────────────────────────────────────
	// Events
	//──────────────────────────────────────────────────────────────

	/** Fires the instant a new target weather is requested. */
	UPROPERTY(BlueprintAssignable, Category = "Weather|Events")
	FOnWeatherChanged OnWeatherChanged;

	//──────────────────────────────────────────────────────────────
	// API
	//──────────────────────────────────────────────────────────────

	/** Begin transitioning to a new weather type. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetWeather(EWeatherType NewWeather);

	/** True once Current has effectively reached the target weather. */
	UFUNCTION(BlueprintPure, Category = "Weather")
	bool IsTransitionComplete() const;

	//──────────────────────────────────────────────────────────────
	// Randomizer
	//──────────────────────────────────────────────────────────────

	/** Automatically roll a new weather every Min..Max seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Randomizer")
	bool bRandomizeWeather = false;

	/** Shortest a weather holds before the next roll (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Randomizer", meta = (ClampMin = "1.0"))
	float MinWeatherDuration = 60.0f;

	/** Longest a weather holds before the next roll (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Randomizer", meta = (ClampMin = "1.0"))
	float MaxWeatherDuration = 240.0f;

	/** Relative likelihood of each type when rolling (higher = more common). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather|Randomizer")
	TMap<EWeatherType, float> WeatherWeights;

	/** Immediately roll a new weighted-random weather and reset the timer. */
	UFUNCTION(BlueprintCallable, Category = "Weather|Randomizer")
	void RollRandomWeather();

private:
	FWeatherPreset Target;                       // preset we're blending toward
	void ApplyToWorld();                         // push Current to fog + cloud + MPC
	FWeatherPreset ResolvePreset(EWeatherType Type) const;
	EWeatherType   PickWeightedWeather() const;  // weighted random selection

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* CloudMID = nullptr;
};
