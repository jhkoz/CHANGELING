// FootstepTypes.h
//
// Shared types for the footstep audio system.
//
// Cue selection is data-driven: a DataTable row per surface (and per water depth
// tier), each holding the Walk / Run / Jump cue. Adding a new cue is a row edit,
// not a code or Blueprint change — which is what the old Blueprint switch got
// wrong, since a cue that existed but wasn't in the switch was simply silent.
//
// Row names are matched against the surface names declared in DefaultEngine.ini
// ([/Script/Engine.PhysicsSettings] PhysicalSurfaces), so the .ini stays the one
// place a surface is named.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FootstepTypes.generated.h"

class USoundBase;
class UNiagaraSystem;

// ─────────────────────────────────────────────────────────────────────────────

/** Which foot triggered the step. Passed in from the AnimNotify. */
UENUM(BlueprintType)
enum class EFootSide : uint8
{
	Left   UMETA(DisplayName = "Left"),
	Right  UMETA(DisplayName = "Right")
};

/** Gait bucket used to pick a cue from a row. */
UENUM(BlueprintType)
enum class EFootstepGait : uint8
{
	Walk  UMETA(DisplayName = "Walk"),
	Run   UMETA(DisplayName = "Run"),
	Jump  UMETA(DisplayName = "Jump")
};

/**
 * Water depth tiers, shallow → deep. Row names in the water table must match
 * these names exactly (Puddle, Ankle, Knee, Waist, Chest, Submerged) so they
 * line up with the SC_Foot_Water_<Tier>_<Gait> cue set.
 */
UENUM(BlueprintType)
enum class EWaterDepthTier : uint8
{
	Puddle     UMETA(DisplayName = "Puddle"),
	Ankle      UMETA(DisplayName = "Ankle"),
	Knee       UMETA(DisplayName = "Knee"),
	Waist      UMETA(DisplayName = "Waist"),
	Chest      UMETA(DisplayName = "Chest"),
	Submerged  UMETA(DisplayName = "Submerged")
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * One surface (or one water tier) worth of cues.
 *
 * Hard references on purpose: footstep audio should stay resident rather than
 * hitch on the first step onto a new surface.
 */
USTRUCT(BlueprintType)
struct FFootstepCueRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> Walk = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> Run = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> Jump = nullptr;

	/**
	 * Particle effect spawned at the impact point — a splash for water rows, dust
	 * or debris for land rows. Optional: rows without one simply play audio.
	 *
	 * Spawned aligned to the surface normal, so it stands up off sloped ground.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|VFX")
	TObjectPtr<UNiagaraSystem> Effect = nullptr;

	/** Uniform scale for Effect. Deeper water usually wants a bigger splash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|VFX",
		meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float EffectScale = 1.0f;

	/**
	 * Splash for a LANDING rather than a foot plant. Optional: unset falls back to
	 * Effect, so a row only needs this when a landing should look genuinely
	 * different rather than merely bigger — the scale below covers "bigger".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|VFX")
	TObjectPtr<UNiagaraSystem> LandEffect = nullptr;

	/**
	 * Uniform scale used for landings, whichever effect they resolve to. Applies
	 * even when LandEffect is unset, so a row with only an Effect still throws a
	 * bigger splash when you drop into it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|VFX",
		meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float LandEffectScale = 1.8f;

	/** Effect for the gait, preferring LandEffect on a landing when one is set. */
	UNiagaraSystem* GetEffect(EFootstepGait Gait) const
	{
		return (Gait == EFootstepGait::Jump && LandEffect) ? LandEffect : Effect;
	}

	/** Scale for the gait. Landings always use LandEffectScale. */
	float GetEffectScale(EFootstepGait Gait) const
	{
		return (Gait == EFootstepGait::Jump) ? LandEffectScale : EffectScale;
	}

	/** Cue for the requested gait, falling back Run→Walk and Jump→Walk when unset. */
	USoundBase* GetCue(EFootstepGait Gait) const
	{
		switch (Gait)
		{
		case EFootstepGait::Run:  return Run  ? Run  : Walk;
		case EFootstepGait::Jump: return Jump ? Jump : Walk;
		default:                  return Walk;
		}
	}
};

/** Upper bound (cm) of each water tier. Anything deeper than ChestMax is Submerged. */
USTRUCT(BlueprintType)
struct FWaterDepthThresholds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float PuddleMax = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float AnkleMax = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float KneeMax = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float WaistMax = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ChestMax = 140.0f;

	EWaterDepthTier Classify(float Depth) const
	{
		if (Depth <= PuddleMax) { return EWaterDepthTier::Puddle; }
		if (Depth <= AnkleMax)  { return EWaterDepthTier::Ankle; }
		if (Depth <= KneeMax)   { return EWaterDepthTier::Knee; }
		if (Depth <= WaistMax)  { return EWaterDepthTier::Waist; }
		if (Depth <= ChestMax)  { return EWaterDepthTier::Chest; }
		return EWaterDepthTier::Submerged;
	}

	bool operator==(const FWaterDepthThresholds& Other) const
	{
		return PuddleMax == Other.PuddleMax
			&& AnkleMax  == Other.AnkleMax
			&& KneeMax   == Other.KneeMax
			&& WaistMax  == Other.WaistMax
			&& ChestMax  == Other.ChestMax;
	}

	bool operator!=(const FWaterDepthThresholds& Other) const { return !(*this == Other); }
};
