// FootstepAudioComponent.h
//
// Footstep / landing audio for a Character. Replaces the logic that lived in the
// ABP_Unarmed EventGraph.
//
// Design notes, each answering a specific defect in the Blueprint version:
//   • ONE trace path, parameterised by socket. The Blueprint had near-identical
//     left / right / jump subgraphs, and the right-foot water branch called the
//     LEFT trace — a copy-paste slip that is invisible in a node graph.
//   • Foot sockets, not a fixed capsule offset. Both feet previously traced the
//     same hard-coded offset (30,10,±120), so the two feet were literally the
//     same point.
//   • Surface TYPE, not physical-material identity. The Blueprint compared
//     PhysMat == DeepWater_PM in three places, so a second water material — or
//     ShallowWater on fountains — silently missed the water path.
//   • Landing is an EVENT (ACharacter::LandedDelegate). The Blueprint polled a
//     line trace every animation update behind a 0.3s debounce flag, which both
//     traced every frame and re-fired the splash while merely standing in water.
//   • Debug drawing is OFF by default. The Blueprint left DrawDebugType on
//     ForDuration with a 5s lifetime on all three traces.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
// EPhysicalSurface. Reaches us through the shared PCH in a normal build, so its
// absence here only shows up when this file is compiled on its own.
#include "Chaos/ChaosEngineInterface.h"
#include "FootstepTypes.h"
#include "FootstepAudioComponent.generated.h"

class ACharacter;
class UDataTable;
class USoundBase;

// ─────────────────────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class CHANGELING_API UFootstepAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFootstepAudioComponent();

	// ── Entry points ────────────────────────────────────────────────────────

	/** Call from the l_foot_plant / r_foot_plant AnimNotify. */
	UFUNCTION(BlueprintCallable, Category = "Footstep")
	void PlayFootstep(EFootSide Foot);

	/** Landing sound. Bound to the character's LandedDelegate automatically; also callable. */
	UFUNCTION(BlueprintCallable, Category = "Footstep")
	void PlayJumpLand();

	// ── Debug helpers ───────────────────────────────────────────────────────

	/**
	 * Friendly surface name for a hit — "Grass", "Stone", "DeepWater" — read from
	 * the PhysicalSurfaces list in DefaultEngine.ini. Returns None when the hit
	 * has no physical material.
	 *
	 * Printing EPhysicalSurface directly only ever gives you "SurfaceType5";
	 * this is the mapping that turns that into the name you actually named it.
	 */
	UFUNCTION(BlueprintPure, Category = "Footstep|Debug",
		meta = (DisplayName = "Get Surface Name (Hit)"))
	static FName GetSurfaceNameFromHit(const FHitResult& Hit);

	/** Same mapping, when you already have the enum rather than a hit. */
	UFUNCTION(BlueprintPure, Category = "Footstep|Debug",
		meta = (DisplayName = "Get Surface Name (Surface Type)"))
	static FName GetSurfaceName(EPhysicalSurface Surface);

	// ── Data ────────────────────────────────────────────────────────────────

	/** Row per surface, named to match DefaultEngine.ini (Grass, Stone, Wood…). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Data")
	TObjectPtr<UDataTable> SurfaceCueTable;

	/** Row per water tier (Puddle, Ankle, Knee, Waist, Chest, Submerged). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Data")
	TObjectPtr<UDataTable> WaterCueTable;

	/** Surfaces routed to the water table instead of the surface table. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Data")
	TArray<TEnumAsByte<EPhysicalSurface>> WaterSurfaces;

	// ── Tuning ──────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Trace")
	FName LeftFootSocket = TEXT("foot_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Trace")
	FName RightFootSocket = TEXT("foot_r");

	/**
	 * Minimum height above the socket to start from, to cope with the foot sinking
	 * into geometry. The trace actually starts at whichever is higher, this or the
	 * top of the capsule — see TraceFromSocket for why the capsule matters.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Trace", meta = (ClampMin = "0.0"))
	float TraceStartHeight = 30.0f;

	/** How far BELOW the socket the trace reaches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Trace", meta = (ClampMin = "1.0"))
	float TraceLength = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Trace")
	FName TraceProfile = TEXT("BlockAll");

	/** Ground speed at or above which a step counts as a run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Tuning", meta = (ClampMin = "0.0"))
	float RunSpeedThreshold = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Tuning")
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Water")
	FWaterDepthThresholds WaterDepth;

	/** Spawn the row's Niagara Effect at the impact point alongside the cue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|VFX")
	bool bSpawnEffects = true;

	/** Skip VFX beyond this distance from the listener; 0 disables the cull. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|VFX",
		meta = (ClampMin = "0.0"))
	float MaxEffectDistance = 6000.0f;

	/**
	 * Downward speed (cm/s) at which a landing splash reaches full intensity.
	 *
	 * Landings are scaled by IMPACT speed, not ground speed. A drop straight down
	 * has almost no horizontal velocity, so reusing the walking measure would make
	 * the hardest landings throw the smallest splash — exactly backwards.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|VFX",
		meta = (ClampMin = "1.0"))
	float LandFullImpactSpeed = 900.0f;

	/**
	 * Draw effects from the Niagara component pool rather than allocating one per
	 * step. Running through the moat fires ~3 a second, so the churn is real.
	 *
	 * Turn OFF to diagnose splashes that stop appearing after a few minutes: that
	 * is pool starvation, caused by a system whose emitters never report complete.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|VFX")
	bool bPoolEffects = true;

	/** Written to the effect's WaterColor user parameter, tinting splash particles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|VFX")
	FLinearColor WaterTint = FLinearColor(0.55f, 0.75f, 0.85f, 1.0f);

	/**
	 * User parameter names written on spawn. A system missing any of them is fine —
	 * the write simply lands on nothing — so one effect asset can ignore all three.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Footstep|VFX")
	FName IntensityParameter = TEXT("Intensity");

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Footstep|VFX")
	FName VelocityParameter = TEXT("OwnerVelocity");

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Footstep|VFX")
	FName WaterColorParameter = TEXT("WaterColor");

	/**
	 * Depth at the step, normalised 0..1 against the Submerged boundary, so an
	 * effect can taper itself with depth rather than only growing with it.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Footstep|VFX")
	FName DepthParameter = TEXT("WaterDepthNorm");

	/** Draw trace lines and log the resolved surface. Off in shipping builds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Footstep|Debug")
	bool bDrawDebug = false;

	/**
	 * Diagnostic logging, independent of the debug lines above. Kept separate on
	 * purpose: the two were one flag, which meant every quiet playtest was also a
	 * blind one, and every diagnostic run came with lines drawn through the shot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|Debug")
	bool bVerboseLogging = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Bound to ACharacter::LandedDelegate. */
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	/** Shared implementation for both feet and for landing. */
	void PlayStepAtSocket(FName Socket, EFootstepGait Gait);

	/**
	 * Trace down from a socket. Returns every hit so the water surface and the
	 * bed beneath it can both be identified in one query.
	 */
	bool TraceFromSocket(FName Socket, TArray<FHitResult>& OutHits, FVector& OutFoot) const;

	/**
	 * Resolves both the cue and the row that produced it. The row comes back so
	 * the caller can spawn its Effect — a step with no cue may still have VFX,
	 * and vice versa, so the two are handled independently.
	 *
	 * OutDepth is the water depth in cm that picked the tier, or 0 on land; the
	 * splash scales with it, so it has to survive past the row lookup.
	 *
	 * Foot is the socket the trace came from. Water depth is measured from the
	 * surface down to it, which is this step's submersion rather than the water
	 * column's height — on uneven ground the two feet legitimately differ.
	 */
	USoundBase* ResolveCue(const TArray<FHitResult>& Hits, const FVector& Foot,
	                       EFootstepGait Gait,
	                       FVector& OutLocation, FVector& OutNormal, float& OutDepth,
	                       const FFootstepCueRow*& OutRow) const;

	/** Spawns the row's Niagara Effect, aligned to the surface and distance-culled. */
	void SpawnStepEffect(const FFootstepCueRow& Row, EFootstepGait Gait,
	                     const FVector& Location, const FVector& Normal,
	                     float Depth) const;

	bool IsWaterSurface(EPhysicalSurface Surface) const;

	/** EPhysicalSurface → the name declared in DefaultEngine.ini. */
	static FName SurfaceToRowName(EPhysicalSurface Surface);

	static FName TierToRowName(EWaterDepthTier Tier);

	EFootstepGait ResolveGait() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	/** Downward speed captured at touchdown, used to size the landing splash. */
	float LandingImpactSpeed = 0.0f;
};
