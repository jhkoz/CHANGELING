// GardenFountain.h
//
// A fountain prop: mesh, two Niagara systems (the stream and the faked splashes),
// and a looping ambience pair that fades in when the player comes near.
//
// This is the C++ half of the "Let's Build the RPG" episode 22 fountain. Everything
// that episode builds in a Construction Script lives here instead:
//   • one particle-count knob driving BOTH Niagara systems, so the stream and its
//     splashes can never disagree about how hard the fountain is running;
//   • the audio trigger sphere sized from the mesh's own bounds rather than typed
//     in by hand, so rescaling the fountain cannot leave the sound radius behind;
//   • pitch and volume derived from that same particle count.
//
// What is deliberately NOT here: the emitter stacks. Niagara collision, GPU sim
// targets, ray-trace providers, fixed bounds and event handlers have no C++ API —
// they are authored in the Niagara asset and referenced from this actor. See the
// build notes accompanying this class.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GardenFountain.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup = (Environment), meta = (DisplayName = "Garden Fountain"))
class CHANGELING_API AGardenFountain : public AActor
{
	GENERATED_BODY()

public:
	AGardenFountain();

	/** Re-applies particle count, trigger radius and audio response. Runs on every
	 *  property edit and on placement, which is what makes the actor tunable in the
	 *  level rather than only at runtime. */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Sets the rate and re-derives the audio response from it. Call this instead of
	 *  writing ParticlesPerSecond directly, or the sound will not follow. */
	UFUNCTION(BlueprintCallable, Category = "Fountain")
	void SetParticlesPerSecond(int32 NewCount);

	UFUNCTION(BlueprintPure, Category = "Fountain")
	int32 GetParticlesPerSecond() const { return ParticlesPerSecond; }

protected:
	virtual void BeginPlay() override;

	// ── Components ──────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fountain")
	TObjectPtr<UStaticMeshComponent> FountainMesh;

	/** The upward jet. Owns the collision — this is the system that ray-traces. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fountain")
	TObjectPtr<UNiagaraComponent> StreamEffect;

	/**
	 * The ring of splashes in the basin.
	 *
	 * These are FAKED, not spawned from real collisions. A GPU sim cannot write
	 * collision events back to the CPU, so a second emitter cannot be told where the
	 * stream actually landed. Since the fountain is a closed system running in a
	 * known basin, a ring emitter in roughly the right place is indistinguishable at
	 * play distance and costs a fraction of CPU collision.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fountain")
	TObjectPtr<UNiagaraComponent> SplashEffect;

	/** Starts and stops the ambience. Sized from the mesh in OnConstruction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fountain")
	TObjectPtr<USphereComponent> AudioTrigger;

	/** Two loops with different attenuation ranges, so the near and far character of
	 *  the water differ instead of one sound simply getting quieter. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fountain")
	TObjectPtr<UAudioComponent> WaterLoopNear;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fountain")
	TObjectPtr<UAudioComponent> WaterLoopFar;

	// ── Tuning ──────────────────────────────────────────────────────────────

	/** Drives Spawn Rate on both systems, and the audio response below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fountain",
		meta = (ClampMin = "0", ExposeOnSpawn = "true"))
	int32 ParticlesPerSecond = 5000;

	/** User parameter to write on both systems. Must match the name in Niagara. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Fountain")
	FName ParticleCountParameter = TEXT("NumberOfParticles");

	/** Trigger radius as a multiple of the mesh's own bounds. Must comfortably
	 *  exceed the falloff of the LONGER-ranged of the two sounds, or the ambience
	 *  will still be audible at the moment the trigger stops it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fountain",
		meta = (ClampMin = "1.0"))
	float TriggerRadiusScale = 4.0f;

	// ── Audio response ──────────────────────────────────────────────────────

	/** Particle count treated as "normal": pitch and volume are unshifted here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fountain|Audio",
		meta = (ClampMin = "1"))
	int32 BaselineParticles = 5000;

	/** How hard the count moves the sound. 0 disables the response entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fountain|Audio",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AudioResponseStrength = 0.2f;

	/**
	 * Pitch clamp. The floor is 1.0 on purpose: a thinner fountain reads as higher
	 * pitched, but pitching a water loop DOWN just sounds broken, so the response is
	 * one-directional. Past about 1.2 the resampling artefacts become audible.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fountain|Audio")
	FVector2D PitchRange = FVector2D(1.0f, 1.2f);

	/** Volume clamp. Volume moves at twice the pitch's rate and in both directions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fountain|Audio")
	FVector2D VolumeRange = FVector2D(0.3f, 1.5f);

	/** Random start offset (s) so two fountains in earshot never phase-lock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fountain|Audio")
	FVector2D StartOffsetRange = FVector2D(1.0f, 10.0f);

	/** Stop the loops on exit. Attenuation already silences them; this reclaims the
	 *  voices, which matters only once a level holds many fountains. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fountain|Audio")
	bool bStopAudioOnExit = true;

	// ── Internals ───────────────────────────────────────────────────────────

	UFUNCTION()
	void HandleTriggerBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** True only for the actor the local player is driving. */
	bool IsLocalPlayerPawn(const AActor* Other) const;

	void ApplyParticleCount();
	void ApplyTriggerRadius();
	void ApplyAudioResponse();
};
