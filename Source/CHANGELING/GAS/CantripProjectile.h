// CantripProjectile.h
//
// A thrown cantrip in flight -- the fireball and anything later that behaves like one.
//
// A Pawn rather than an Actor, so it can be possessed. A changeling steering their own
// fireball is squarely the kind of thing this game should allow, and retrofitting a
// controller onto an AActor later is more work than starting from APawn now.
//
// The effect is NOT spawned here. The ability spawns its Niagara system on the caster's
// hand when the throw begins and hands it over at the apex, so the flame gathers in the
// palm and then leaves with the projectile. Building it here instead would pop a fully
// formed fireball into existence mid-swing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "CantripProjectile.generated.h"

class UAudioComponent;
class UCapsuleComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UProjectileMovementComponent;
class UCurveFloat;
class UGameplayEffect;
class USoundBase;

UCLASS()
class CHANGELING_API ACantripProjectile : public APawn
{
	GENERATED_BODY()

public:
	ACantripProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Adopt the effect the caster has been holding, and launch.
	 *
	 * Intensity carries the successes through: it drives how hard the thing is thrown,
	 * how fiercely it burns, how loud it is and how long it survives. One number so a
	 * good roll is visible in every part of the result rather than only in a damage
	 * figure nobody sees.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Projectile")
	void Launch(UNiagaraComponent* AdoptedEffect, const FVector& Direction, float InIntensity,
		float ExtraSpeed = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Cantrip|Projectile")
	float GetIntensity() const { return Intensity; }

protected:
	/** Collision body. Kept aligned to the direction of travel every tick. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TObjectPtr<UCapsuleComponent> Capsule;

	/**
	 * Velocity-driven movement rather than a simulated rigid body.
	 *
	 * A physics fireball spawns inside the caster's collision and depenetration shoves
	 * whichever body can move -- which is the player, launched forward by their own
	 * spell. Sweeping under its own velocity avoids the whole class of problem, and
	 * steering a possessed fireball later is far easier on a velocity than on an
	 * accumulated impulse.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** Light travels with the flame. Cheaper and better placed than a light renderer
	 *  inside the Niagara system, which lights from each particle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TObjectPtr<UAudioComponent> Audio;

	/** Looping sound for the flight. Volume and pitch follow intensity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TObjectPtr<USoundBase> FlightSound;

	/**
	 * The flame in flight, spawned on launch when nothing is handed over.
	 *
	 * The ability's wind-up effect stays behind on the hand and dies back, so the
	 * projectile brings its own rather than inheriting one. Launch still accepts an
	 * adopted component for the case where a caller does want to transfer.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TObjectPtr<UNiagaraSystem> FlightEffect;

	// ── Flight ──────────────────────────────────────────────────────────────

	/**
	 * Speed at launch regardless of the roll, so a marginal success still travels
	 * rather than dribbling out of the hand. Centimetres per second.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Flight")
	float BaseSpeed = 6000.0f;

	/** Added to the base, per point of intensity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Flight")
	float SpeedPerIntensity = 45.0f;

	// ── Size ────────────────────────────────────────────────────────────────

	/** Scale at zero intensity -- the size a barely-successful cast produces. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Size",
		meta = (ClampMin = "0.01"))
	float BaseEffectScale = 0.6f;

	/**
	 * Added to the scale per point of intensity.
	 *
	 * The roll should be legible at a glance. Speed and brightness already follow it,
	 * but size is what reads across a room -- a five-success Engulf ought to look like
	 * a different spell from a one-success one, not the same spell moving faster.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Size",
		meta = (ClampMin = "0.0"))
	float ScalePerIntensity = 0.012f;

	/**
	 * Keep resizing as it burns down, rather than only at launch.
	 *
	 * A fireball that shrinks as it dies is doing visibly what its intensity is doing
	 * numerically. Off, it stays the size it launched at and simply winks out.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Size")
	bool bShrinkWhileBurning = true;

	/**
	 * How fast the capsule swings to face where it is going.
	 *
	 * Fast, but interpolated rather than snapped: a possessed fireball being steered
	 * should lean into its turns, and an instant realignment reads as a stutter.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Flight")
	float AlignSpeed = 12.0f;

	/**
	 * Which of the projectile's own axes is pointed along the direction of travel.
	 *
	 * The capsule and the flight effect share the actor's orientation, so this has to
	 * agree with whichever axis the Niagara system emits along. A system authored to
	 * fire down its Y -- the same convention the hand socket throws on -- lands
	 * sideways to its own flight path if this is left at X.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Flight")
	TEnumAsByte<EAxis::Type> ForwardAxis = EAxis::X;

	// ── Detonation ──────────────────────────────────────────────────────────

	/**
	 * Blow up on impact rather than simply stopping.
	 *
	 * On the base class rather than a subclass so every thrown cantrip can be given a
	 * detonation by ticking a box. Most will not want one -- a hurled illusion should
	 * land, not explode -- but the ones that do should not each need their own class.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact")
	bool bExplodeOnImpact = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact")
	TObjectPtr<USoundBase> ExplosionSound;

	/** Blast radius at zero intensity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact")
	float BaseBlastRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact")
	float BlastRadiusPerIntensity = 2.5f;

	/**
	 * Scorch rays fired outward, per point of intensity.
	 *
	 * Scaled rather than fixed so a small detonation does not pay for a large one's
	 * traces. Rounded up to at least one, because a blast that marks nothing at all
	 * looks like it did not happen.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact",
		meta = (ClampMin = "0.0"))
	float BurstRaysPerIntensity = 0.2f;

	/** Multiplier on how hard each blast mark scorches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BurstMarkStrength = 0.7f;

	/**
	 * Niagara user float on the EXPLOSION system that the roll is passed on.
	 *
	 * The same name the flight effect uses, so one convention covers both: whatever the
	 * cantrip rolled drives how many puffs the burst spawns and how large they grow,
	 * rather than every explosion looking identical regardless of the casting.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile|Impact")
	FName ExplosionIntensityParameter = TEXT("Intensity");

	/**
	 * Niagara user BOOL on the flight effect that stops it spawning.
	 *
	 * Dropping the flight intensity to zero is the signal the system ages its particles
	 * out on, but on its own it does not necessarily stop new ones appearing -- so the
	 * trail keeps drawing itself out of a fireball that has already burst. Left as None,
	 * the component is deactivated instead, which is blunter but always works.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile|Impact")
	FName FlightSpawnParameter = TEXT("SpawnParticles");

	// ── Flash ───────────────────────────────────────────────────────────────

	/**
	 * Light intensity at the instant of detonation, per point of the roll.
	 *
	 * The flight light is dim enough to sit inside a fireball; an explosion is the one
	 * moment the spell lights the room. Scaled from intensity rather than fixed, so the
	 * brightness of the flash says how good the casting was.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Flash")
	float FlashIntensityPerIntensity = 50.0f;

	/**
	 * Seconds the flash takes to fall to nothing.
	 *
	 * Much shorter than the sound. A blast is heard for a good deal longer than it is
	 * seen, and a light that hangs about for as long as the boom reads as a lamp being
	 * switched off rather than as an explosion.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Flash",
		meta = (ClampMin = "0.0"))
	float FlashFalloffSeconds = 0.4f;

	/** Shapes the flash falloff, X and Y both 0..1. Empty falls away steeply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Flash")
	TObjectPtr<UCurveFloat> FlashCurve;

	// ── Blast sound ─────────────────────────────────────────────────────────

	/**
	 * Peak volume multiplier at the moment of detonation.
	 *
	 * Well above 1 on purpose. This is the same cue the flame and the flight already use
	 * -- purpose-made explosion samples are all far too theatrical for a cantrip -- and
	 * what makes it read as a detonation is the shape of the envelope, not the sample.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact",
		meta = (ClampMin = "0.0"))
	float ExplosionVolumeScale = 5.0f;

	/** Seconds the blast sound takes to fall to silence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact",
		meta = (ClampMin = "0.0"))
	float ExplosionVolumeSeconds = 1.0f;

	/**
	 * Shapes the blast's volume envelope, X and Y both 0..1. Empty drops steeply and
	 * then tails off -- boom and done rather than a long even fade.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact")
	TObjectPtr<UCurveFloat> ExplosionVolumeCurve;

	/**
	 * Effect applied to anything caught in the blast.
	 *
	 * Wired now and deliberately allowed to be empty. The plumbing is the part that is
	 * awkward to retrofit -- finding targets, building a context, passing the roll's
	 * magnitude through -- and doing it later would mean revisiting every one of these
	 * decisions. What the effect DOES is a separate question, answered in the asset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact")
	TSubclassOf<UGameplayEffect> BlastEffect;

	/**
	 * SetByCaller tag the blast magnitude is passed on.
	 *
	 * Defaulted in the constructor to Cantrip.Data.Magnitude -- the same tag every other
	 * role's effects read, and the one GE_CantripDamage looks up. It was previously an
	 * empty tag, and the code that sets the magnitude is guarded on IsValid(): so a
	 * fireball with a damage effect correctly assigned passed no magnitude at all, the
	 * effect's SetByCaller lookup found nothing, and every detonation dealt exactly zero
	 * while looking entirely healthy.
	 *
	 * Override it only for an effect that deliberately reads a different tag.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile|Impact")
	FGameplayTag BlastMagnitudeTag;

	/** Blast magnitude per point of intensity, handed to the effect by that tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Impact")
	float BlastMagnitudePerIntensity = 1.0f;

	// ── Burning down ────────────────────────────────────────────────────────

	/**
	 * How much intensity is lost per ageing step.
	 *
	 * This is the range dial, not the speed. Intensity is the fuel, so a projectile
	 * travels speed x (intensity / burn rate) -- and at one point per step it burns
	 * twenty a second, which gives a modest casting about a second in the air however
	 * fast it is thrown.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Life",
		meta = (ClampMin = "0.001"))
	float IntensityLossPerStep = 0.3f;

	/**
	 * Hard ceiling on time in the air, whatever the roll.
	 *
	 * Without it, range scales with intensity twice over -- a strong casting is both
	 * faster and longer-lived -- and a five-success Engulf crosses a kilometre before
	 * burning out. The cap keeps a good roll dramatic rather than absurd.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Life",
		meta = (ClampMin = "0.1"))
	float MaxLifeSeconds = 6.0f;

	/** Seconds between ageing steps. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Life",
		meta = (ClampMin = "0.01"))
	float AgeInterval = 0.05f;

	/** Grace period after burning out before the actor goes, so the last particles get
	 *  to finish rather than vanishing mid-air. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Life")
	float LingerSeconds = 1.0f;

	// ── Parameter names ─────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile")
	FName IntensityParameter = TEXT("Intensity");

	/** Volume floor, so a dying fireball is quiet rather than silent. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile")
	float MinVolume = 0.8f;

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile")
	float VolumeIntensityScale = 500.0f;

private:
	void AgeProjectile();
	void PushIntensity();

	UFUNCTION()
	void OnProjectileStopped(const FHitResult& Hit);

	/** Blow up here: effect, sound, flash, scorch, and whatever the blast applies. */
	void Detonate(const FHitResult& Hit);

	void ApplyBlastEffect(const FVector& Origin, float Radius);

	/** Drives the flash and the blast envelope down after a detonation. */
	void TickDetonation();

	/** Orientation that points ForwardAxis along Dir. */
	FRotator OrientationFor(const FVector& Dir) const;

	UPROPERTY() TObjectPtr<UNiagaraComponent> Effect;

	FTimerHandle AgeTimer;

	FVector PreviousLocation = FVector::ZeroVector;

	/** The velocity Launch asked for, restored after a spawn overlap is shrugged off. */
	FVector LaunchVelocity = FVector::ZeroVector;

	/** Where it was thrown from, so the flight probe can say how far it actually got. */
	FVector LaunchLocation = FVector::ZeroVector;

	FTimerHandle FlightProbeTimer;

	/** Logs distance travelled half a second after launch. */
	void ReportFlightProgress();

	/** How many times this projectile has been pushed clear of geometry it was born
	 *  inside. Bounded so one spawned deep in a wall cannot be nudged forever. */
	int32 SpawnOverlapNudges = 0;

	static constexpr int32 MaxSpawnOverlapNudges = 3;
	float Intensity = 0.0f;
	bool bBurnedOut = false;
	bool bDetonated = false;

	/** Intensity captured at impact, before it is spent stopping the flight. */
	float IntensityAtImpact = 0.0f;

	float DetonationTime = 0.0f;
	float FlashPeakIntensity = 0.0f;

	FTimerHandle DetonationTimer;

	/** The blast's own audio, attached so it attenuates from where the fireball burst
	 *  rather than playing flat wherever the listener happens to be. */
	UPROPERTY() TObjectPtr<UAudioComponent> ExplosionAudio;

	/** Set once Launch has run, so the pre-launch push still sizes the projectile
	 *  even when continuous resizing is switched off. */
	bool bLaunched = false;

	/** When Launch ran, for the hard life cap. */
	float LaunchTime = 0.0f;
};
