// Slows the character down as it wades into deeper water.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "FootstepTypes.h"
#include "WaterMovementComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Measures how deep the owning character is standing in water and scales its
 * walk speed accordingly.
 *
 * Adopts the footstep audio component's thresholds at BeginPlay (see
 * bSyncWithFootstepAudio), so the tier you HEAR and the tier that slows you down
 * are the same tier by construction — set the thresholds once, on the footsteps.
 *
 * Depth is measured from the water surface down to the capsule base, so standing
 * on a rock in a deep pool correctly reads as shallow.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class CHANGELING_API UWaterMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaterMovementComponent();

	//~ Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

	// ── Detection ───────────────────────────────────────────────────────────

	/**
	 * Adopt the sibling UFootstepAudioComponent's thresholds and water surfaces at
	 * BeginPlay, making the footstep component the single source of truth.
	 *
	 * Sharing the STRUCT is not sharing the VALUES: without this, the tier you hear
	 * and the tier that slows you down are two independently edited copies that
	 * agree only by luck, and drift silently the first time one is retuned.
	 *
	 * Uncheck only to deliberately run different boundaries for movement than for
	 * audio; the values below are then used as authored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement")
	bool bSyncWithFootstepAudio = true;

	/** Tier boundaries. Overwritten at BeginPlay while bSyncWithFootstepAudio is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement")
	FWaterDepthThresholds WaterDepth;

	/**
	 * Surfaces that count as water. Defaults to ShallowWater + DeepWater, and is
	 * likewise adopted from the footstep component when syncing — a surface that
	 * splashes audibly but does not slow you down is the drift this prevents.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement")
	TArray<TEnumAsByte<EPhysicalSurface>> WaterSurfaces;

	/** Trace profile used to find the water surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement")
	FName TraceProfile = FName(TEXT("BlockAll"));

	/** How far above the feet to look for a water surface. Covers full submersion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement",
		meta = (ClampMin = "50.0"))
	float MaxCheckHeight = 300.0f;

	/** Seconds between depth samples. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement",
		meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float UpdateInterval = 0.1f;

	// ── Speed scaling ───────────────────────────────────────────────────────

	/** Multiplier applied to walk speed at each tier. 1.0 = unaffected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float PuddleScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float AnkleScale = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float KneeScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float WaistScale = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ChestScale = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SubmergedScale = 0.25f;

	/**
	 * Multiplier on MaxSwimSpeed while actually swimming.
	 *
	 * Deliberately NOT the tier scale: those are tuned for the resistance of wading
	 * on a bed, and MaxSwimSpeed already describes a swimmer, so reusing
	 * SubmergedScale here would slow swimming to a quarter of a speed that is
	 * already the swimming speed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed",
		meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float SwimSpeedScale = 1.0f;

	/**
	 * How fast the speed change eases in, per second. Without this, stepping over
	 * a tier boundary snaps the speed and reads as a stutter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed",
		meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float BlendSpeed = 4.0f;

	/** Uncheck to compute the multiplier but leave MaxWalkSpeed alone (drive it yourself). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed")
	bool bApplyToMovement = true;

	// ── Swim hysteresis ─────────────────────────────────────────────────────

	/**
	 * Take over WHEN swimming starts and stops, instead of leaving it to the raw
	 * volume test.
	 *
	 * APhysicsVolume::IsOverlapInVolume is a hard point test on the capsule CENTRE
	 * against the brush, with no deadband. On shoreline ground that puts the centre
	 * near the volume's top face, the mode flips every frame -- swim, buoyancy
	 * lifts, centre clears the face, fall, gravity drops, swim again. That reads as
	 * jitter, and it flaps the AnimBP between Swim and Fall at the same time.
	 *
	 * Gating on measured depth with a gap between the two thresholds removes the
	 * deadband problem entirely, and lets swimming start at a depth you choose
	 * rather than wherever the capsule centre happens to sit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Swim")
	bool bUseSwimHysteresis = true;

	/** Start swimming once submersion passes this (cm). Must exceed SwimExitDepth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Swim",
		meta = (ClampMin = "0.0"))
	float SwimEnterDepth = 110.0f;

	/** Stop swimming once submersion drops below this (cm). The gap is the deadband. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Swim",
		meta = (ClampMin = "0.0"))
	float SwimExitDepth = 80.0f;

	/**
	 * Raises the MESH — not the capsule — by this much while swimming.
	 *
	 * A prone swim clip puts the pelvis at the mesh origin, and that origin sits a
	 * capsule half-height below the capsule's centre. Buoyancy floats the CAPSULE,
	 * so the visible body ends up roughly a metre under the surface even though the
	 * capsule is sitting where it should. Raising Buoyancy does not fix it — that
	 * lifts the capsule, which was never the part that was wrong.
	 *
	 * 0 disables this entirely and the mesh is never touched.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Swim")
	float SwimMeshZOffset = 100.0f;

	/** Also scale jump height — wading jumps should be feeble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Speed")
	bool bScaleJump = true;

	// ── Wake VFX ────────────────────────────────────────────────────────────

	/**
	 * Looping wake riding at the waterline. Spawned once and attached to the
	 * character, then activated and deactivated as it enters and leaves water —
	 * respawning a looping system per entry would churn the pool for nothing.
	 *
	 * Attached to the ROOT rather than the mesh, so it inherits the capsule's yaw.
	 * With bOrientRotationToMovement the capsule already faces travel, which is
	 * exactly the heading a bow wave needs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Wake")
	TObjectPtr<UNiagaraSystem> BowWaveEffect;

	/** One-shot ripples dropped in world space and left behind as you move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Wake")
	TObjectPtr<UNiagaraSystem> WakeTrailEffect;

	/** No wake below this submersion (cm). Ankle-deep puddles do not make wakes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Wake",
		meta = (ClampMin = "0.0"))
	float WakeMinDepth = 25.0f;

	/** No wake below this ground speed. A character standing still displaces nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Wake",
		meta = (ClampMin = "0.0"))
	float WakeMinSpeed = 40.0f;

	/**
	 * Distance travelled between trail drops (cm). Spaced by DISTANCE rather than
	 * time so the trail stays evenly spaced whether you are wading or sprinting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Wake",
		meta = (ClampMin = "10.0"))
	float WakeTrailSpacing = 120.0f;

	/** Skip wake VFX beyond this distance from the camera; 0 disables the cull. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Wake",
		meta = (ClampMin = "0.0"))
	float MaxWakeDistance = 6000.0f;

	/** User parameters written on both wake systems. Missing ones are harmless. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Water Movement|Wake")
	FName WakeSpeedParameter = TEXT("Speed");

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Water Movement|Wake")
	FName WakeDepthParameter = TEXT("Depth");

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Water Movement|Wake")
	FName WakeIntensityParameter = TEXT("Intensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Debug")
	bool bDrawDebug = false;

	/** Diagnostic logging, independent of the debug lines above. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Movement|Debug")
	bool bVerboseLogging = false;

	// ── Queries ─────────────────────────────────────────────────────────────
	//
	// All marked BlueprintThreadSafe so the AnimBP can read them from its
	// thread-safe Update Animation. They only return values cached on the game
	// thread during TickComponent — never call anything here that traces.

	/** Current submersion depth in cm. 0 when not in water. */
	UFUNCTION(BlueprintPure, Category = "Water Movement", meta = (BlueprintThreadSafe))
	float GetWaterDepth() const { return CurrentDepth; }

	UFUNCTION(BlueprintPure, Category = "Water Movement", meta = (BlueprintThreadSafe))
	bool IsInWater() const { return bInWater; }

	UFUNCTION(BlueprintPure, Category = "Water Movement", meta = (BlueprintThreadSafe))
	EWaterDepthTier GetWaterTier() const { return CurrentTier; }

	/** True while CharacterMovement is in MOVE_Swimming, i.e. inside a water volume. */
	UFUNCTION(BlueprintPure, Category = "Water Movement", meta = (BlueprintThreadSafe))
	bool IsSwimming() const { return bSwimming; }

	/**
	 * How much of the capsule is inside the water volume, 0 (dry) to 1 (fully under).
	 * Blend surface-swim against underwater-swim with this.
	 *
	 * A cached copy of CharacterMovement's ImmersionDepth(), which is neither a
	 * UFUNCTION nor callable off the game thread — it line-traces the volume brush.
	 */
	UFUNCTION(BlueprintPure, Category = "Water Movement", meta = (BlueprintThreadSafe))
	float GetImmersionRatio() const { return ImmersionRatio; }

	/** The multiplier currently being applied (post-blend). */
	UFUNCTION(BlueprintPure, Category = "Water Movement", meta = (BlueprintThreadSafe))
	float GetSpeedMultiplier() const { return BlendedScale; }

	/**
	 * Base speed the multiplier is applied to. Call this from sprint / walk logic
	 * instead of setting MaxWalkSpeed directly, or the two will fight each other.
	 */
	UFUNCTION(BlueprintCallable, Category = "Water Movement")
	void SetBaseWalkSpeed(float NewBaseSpeed);

	UFUNCTION(BlueprintPure, Category = "Water Movement")
	float GetBaseWalkSpeed() const { return BaseWalkSpeed; }

private:
	/** Adopts the sibling footstep component's tier boundaries and water surfaces. */
	void SyncTiersFromFootstepAudio();

	/** Drives entry/exit into MOVE_Swimming from measured depth, with a deadband. */
	void UpdateSwimGate(bool bInWaterVolume);

	/** Eases the mesh up to SwimMeshZOffset while swimming, and back down on exit. */
	void UpdateMeshOffset(float DeltaTime);

	/** Runs the attached bow wave and drops the world-space trail behind it. */
	void UpdateWake();

	/** True when the camera is far enough away that wake VFX are not worth spawning. */
	bool IsWakeCulled(const FVector& Location) const;

	float ScaleForTier(EWaterDepthTier Tier) const;
	bool IsWaterSurface(EPhysicalSurface Surface) const;

	/** Depth from water surface to capsule base; 0 when dry. */
	float MeasureDepth() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> Movement;

	/** The persistent attached wake. Created lazily on first use, never respawned. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BowWave;

	FVector LastTrailLocation = FVector::ZeroVector;
	bool bTrailStarted = false;

	/** Last logged wake state, so the log fires on transitions rather than per tick. */
	bool bWakeRunLast = false;
	bool bWakeBowLast = false;

	float BaseWalkSpeed = 0.0f;
	float BaseSwimSpeed = 0.0f;
	float BaseJumpVelocity = 0.0f;
	float BaseMeshRelativeZ = 0.0f;
	float MeshOffsetAlpha = 0.0f;
	float CurrentDepth = 0.0f;
	float ImmersionRatio = 0.0f;
	float BlendedScale = 1.0f;
	bool bInWater = false;
	bool bSwimming = false;
	EWaterDepthTier CurrentTier = EWaterDepthTier::Puddle;
};
