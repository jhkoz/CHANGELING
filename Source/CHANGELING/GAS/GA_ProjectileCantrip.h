// GA_ProjectileCantrip.h
//
// A cantrip that is thrown: Engulf, and anything later that leaves the hand.
//
// The shape is a montage with three moments in it. The working opens and the effect
// begins gathering in the palm; the animation reaches its apex and the effect is handed
// to a projectile; the montage ends and the ability is over. Only the middle one is
// interesting, and only the animation knows when it happens -- hence the notify.
//
// Unlike a sustained cantrip this holds nothing open. Once the projectile has the
// effect, the ability's remaining job is to get out of the way.

#pragma once

#include "CoreMinimal.h"
#include "GA_Cantrip.h"
#include "GA_ProjectileCantrip.generated.h"

class ACantripProjectile;
class UAnimMontage;
class UCurveFloat;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/** Where a thrown cantrip gets its direction. */
UENUM(BlueprintType)
enum class EThrowDirectionSource : uint8
{
	/**
	 * The hand socket's own orientation at the moment of release.
	 *
	 * Follows the aim for free, because the hand is carried by a body that is already
	 * turning to it -- and unlike the aim rotation it matches what the animation is
	 * actually doing on the frame the notify fires.
	 */
	SocketAxis UMETA(DisplayName = "Socket Axis"),

	/**
	 * The direction the hand is actually travelling as the arm swings through.
	 *
	 * A static axis samples one instant and takes whatever the socket happened to be
	 * facing. This takes the tangent of the arc instead, so a throw that crosses the
	 * body leaves along the sweep rather than along wherever the wrist was pointed --
	 * which is what makes the release read as thrown rather than emitted.
	 */
	SocketMotion UMETA(DisplayName = "Arm Swing"),

	/** The anim instance's aim rotation, ignoring where the hand happens to point. */
	Aim UMETA(DisplayName = "Aim Rotation")
};

UCLASS()
class CHANGELING_API UGA_ProjectileCantrip : public UGA_Cantrip
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec) override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * CastMontage is the throw, and it must carry a Cantrip Event notify at the apex or
	 * the projectile is never released and the effect burns out in the caster's hand.
	 *
	 * Started by this class rather than by the base, because the ability ends on the
	 * montage finishing -- it needs the task's completion delegates, not just playback.
	 */
	virtual bool ShouldPlayCastMontageOnActivate() const override { return false; }

	/** Gathered on the hand while the montage winds up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TObjectPtr<UNiagaraSystem> CastEffect;

	// ── Gathering ───────────────────────────────────────────────────────────

	/**
	 * Show the flame gathering in the palm for as long as the cast is held.
	 *
	 * Casting is already a hold -- the whole tier system is built on it -- but until now
	 * nothing was visible until AFTER the key came up, so the most important decision the
	 * player makes was the one they had no feedback on. This puts the fire in the hand at
	 * the moment of the press and lets it build.
	 *
	 * The flame is spawned before anything has been rolled or paid for, which is correct:
	 * Glamour is spent on release, and a working that fizzles is one that gathered and
	 * then guttered out. That is a better read than nothing happening at all.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Gather")
	bool bGatherWhileHeld = true;

	/** Scale of the gathering flame the instant the key goes down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Gather",
		meta = (ClampMin = "0.01"))
	float GatherStartScale = 0.2f;

	/** Scale once the hold has reached the top tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Gather",
		meta = (ClampMin = "0.01"))
	float GatherFullScale = 1.0f;

	/**
	 * Value pushed into IntensityParameter at full charge.
	 *
	 * The roll has not happened yet, so this is a promise rather than a measurement --
	 * what the flame LOOKS worth while it is being gathered. The resolved intensity
	 * replaces it the moment the throw begins.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Gather",
		meta = (ClampMin = "0.0"))
	float GatherIntensityAtFull = 60.0f;

	/**
	 * Shapes the gather, X and Y both 0..1. Empty is linear.
	 *
	 * Keyed on the charge itself rather than on seconds, unlike WindupCurve -- the length
	 * of a full hold is set by the cantrip's own cast tiers and differs per row, so a
	 * curve authored in seconds would mean something different for every cantrip.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Gather")
	TObjectPtr<UCurveFloat> GatherCurve;

	/**
	 * How long a full charge takes when the cantrip's row defines no cast tiers.
	 *
	 * Normally the tiers ARE the answer: the flame reaches full exactly as the hold
	 * reaches the last difficulty reduction, so what the player sees and what they are
	 * earning are the same thing. This is only the fallback for a row left unfilled.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile|Gather",
		meta = (ClampMin = "0.1"))
	float GatherFullSeconds = 3.0f;

	// ── Wind-up ─────────────────────────────────────────────────────────────

	/**
	 * Strength of the gathering flame over time, in SECONDS from the start of the
	 * montage. X is elapsed time, Y is 0..1.
	 *
	 * Seconds rather than a normalised 0..1, because the release point is wherever the
	 * notify sits and nothing here knows that in advance. Authoring against the montage
	 * timeline means the curve and the animation can be read side by side.
	 *
	 * Leave empty and the flame simply appears at full strength.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Windup")
	TObjectPtr<UCurveFloat> WindupCurve;

	/** Niagara user parameter the curve drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Windup")
	FName WindupParameter = TEXT("FadeAlpha");

	/** Size of the gathering flame at zero intensity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Windup",
		meta = (ClampMin = "0.01"))
	float WindupBaseScale = 0.6f;

	/** Added to the wind-up's scale per point of intensity, so the roll is readable
	 *  before the throw rather than only after it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Windup",
		meta = (ClampMin = "0.0"))
	float WindupScalePerIntensity = 0.010f;

	/** How often the curve is sampled. 30 a second is smoother than the eye needs and
	 *  costs one float write. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile|Windup",
		meta = (ClampMin = "0.008"))
	float WindupUpdateInterval = 0.033f;

	/**
	 * Seconds the gathering flame takes to die back once the projectile has gone.
	 *
	 * The flame in the palm and the flame in flight are different things. Handing the
	 * gathered system to the projectile would leave the hand empty in the same frame
	 * the fireball appears, which reads as a cut; letting it fall away over a moment
	 * reads as the fire leaving.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Windup")
	float WindupFadeSeconds = 0.35f;

	// ── Rate of fire ────────────────────────────────────────────────────────

	/**
	 * Seconds after a projectile leaves the hand before another can be thrown.
	 *
	 * Measured from RELEASE rather than from activation, so a long wind-up does not
	 * count toward it -- holding a charge should never earn a shorter gap between
	 * throws than snapping one off.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile",
		meta = (ClampMin = "0.0"))
	float ReleaseCooldownSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TSubclassOf<ACantripProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	FName AttachSocket = TEXT("hand_rSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	FVector AttachOffset = FVector::ZeroVector;

	/** Played as the throw begins. The flight sound lives on the projectile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	TObjectPtr<USoundBase> CastSound;

	// ── Intensity ───────────────────────────────────────────────────────────

	/**
	 * What a bare success is worth, before successes are counted.
	 *
	 * Intensity is the only number the projectile gets, and everything it does is
	 * scaled from it -- speed, brightness, volume, how long it lives. A roll that
	 * barely landed should produce something visibly feebler than a good one, and this
	 * is the floor that keeps it from producing nothing at all.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	float BaseIntensity = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile")
	float IntensityPerSuccess = 20.0f;

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Projectile")
	FName IntensityParameter = TEXT("Intensity");

	// ── Aim ─────────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Aim")
	EThrowDirectionSource ThrowDirectionSource = EThrowDirectionSource::SocketAxis;

	/**
	 * Which of the socket's own axes the throw follows. Green is Y.
	 *
	 * Hand sockets are authored for grips, not for throwing, so their forward is
	 * whatever the rig needed rather than the direction anything leaves along. Reading
	 * the axis off the socket in the viewport beats assuming X.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Aim",
		meta = (EditCondition = "ThrowDirectionSource == EThrowDirectionSource::SocketAxis"))
	TEnumAsByte<EAxis::Type> SocketThrowAxis = EAxis::Y;

	/** Flip it, for when the right axis points backwards. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Aim",
		meta = (EditCondition = "ThrowDirectionSource == EThrowDirectionSource::SocketAxis"))
	bool bInvertSocketAxis = false;

	/**
	 * How far back along the swing to measure from, in seconds. Arm Swing only.
	 *
	 * Too short and the direction is frame noise -- a hand barely moves in a sixtieth
	 * of a second, and the leftover jitter points anywhere. Too long and it averages
	 * across the whole wind-up, including the part where the arm was travelling
	 * backwards.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Aim",
		meta = (ClampMin = "0.016", ClampMax = "0.5",
			EditCondition = "ThrowDirectionSource == EThrowDirectionSource::SocketMotion"))
	float SwingSampleSeconds = 0.08f;

	/**
	 * How far the throw is pulled toward the body's facing, 0 to 1.
	 *
	 * A side-throw's hand is travelling ACROSS the body at the moment of release, so its
	 * raw tangent points sideways and sometimes behind -- which is why an unbiased swing
	 * curls round and lands beside the caster. Blending toward the facing keeps the
	 * animation's character while making the result a throw rather than a discard.
	 *
	 * 0 is the pure tangent, 1 ignores the arm entirely.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Aim",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ForwardBias = 0.6f;

	// ── Release ─────────────────────────────────────────────────────────────

	/**
	 * Where the projectile leaves from, in the ACTOR's space.
	 *
	 * A character's actor origin sits at the capsule's centre -- roughly belt height --
	 * so a Z near zero releases between the hip and the buckle, which is where a pitch
	 * actually lets go. Tracking the hand socket instead follows the wrist through its
	 * whole arc, and a fireball released at the top of a windmill leaves above the head.
	 *
	 * X is forward, Y is right, Z is up.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Release")
	FVector ReleaseBodyOffset = FVector(45.0f, 20.0f, 0.0f);

	/** Release from ReleaseBodyOffset rather than from the hand socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Release")
	bool bReleaseFromBody = true;

	/**
	 * Take the release HEIGHT from the hand socket while keeping the body offset's plan
	 * position. Ignored unless bReleaseFromBody is set.
	 *
	 * On by default. ReleaseBodyOffset's Z is measured from the actor origin, which on a
	 * character is the capsule's centre -- belt height, and a good deal lower than it
	 * reads on screen. Low enough, in practice, to put the fireball inside the ground:
	 * every throw was spawning hundreds of units within the landscape or the floor mesh
	 * beneath her, which is a problem no amount of forward offset can solve because the
	 * error is vertical.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Release",
		meta = (EditCondition = "bReleaseFromBody"))
	bool bReleaseAtSocketHeight = true;

	/**
	 * How far ahead of the socket the projectile leaves, along the throw direction.
	 *
	 * A ball rolls off the fingers, not out of the middle of the fist. Spawning at the
	 * socket puts the fireball inside the hand for its first frames -- and with the
	 * palm still closing through the follow-through, it reads as squeezed out rather
	 * than released.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Release")
	float ReleaseForwardOffset = 15.0f;

	/**
	 * Fraction of the hand's own speed added to the throw, 0 to 1.
	 *
	 * What separates a pitch from an emitter. The arm is already moving when the ball
	 * leaves it, and a throw that ignores that travels at exactly the same speed whether
	 * it was whipped or lobbed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Release",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HandSpeedContribution = 0.5f;

	/** Upward pitch limit for the throw. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Aim",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxThrowPitch = 45.0f;

	/**
	 * Downward pitch limit, kept separate and much tighter.
	 *
	 * A throwing arm is swinging DOWN at the moment it releases, so the tangent of that
	 * arc points at the floor -- and a fast projectile aimed 40 degrees down is a fast
	 * projectile hitting the ground three metres away. Up and down are not symmetrical
	 * for a throw: lobbing high is a choice, pitching into your own feet is not.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Projectile|Aim",
		meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxThrowPitchDown = 8.0f;

private:
	/**
	 * What the flame in the hand is currently doing.
	 *
	 * One component serves all three stages, so something has to say which of them owns
	 * its parameters at any moment -- the gather, the wind-up curve and the fade all
	 * write the same values, and two of them running at once is a flicker.
	 */
	enum class EGatherPhase : uint8
	{
		Idle,
		Charging,   // key held, nothing rolled yet
		Throwing,   // resolved, montage running
		Fading      // thrown or abandoned, dying back
	};

	UFUNCTION() void OnRelease(FGameplayEventData Payload);
	UFUNCTION() void OnMontageFinished();

	/** Put the flame in the palm and start it building. Called on the press. */
	void StartGather();

	/** 0..1 across a full hold, from the cantrip's own cast tiers. */
	float GatherAlpha() const;

	void UpdateWindup();
	void FadeWindup();
	void ClearWindup();

	/** Spawns the gathering flame on the hand socket, warning if the socket is missing. */
	UNiagaraComponent* SpawnGatheredEffect();

	FVector ResolveThrowDirection() const;

	/** Hand speed over the swing window, in centimetres per second. */
	float ResolveSwingSpeed() const;

	/** Pulls a direction toward the body's facing, and never lets one point behind. */
	FVector BiasForward(const FVector& Raw, const AActor* Avatar) const;

	UPROPERTY() TObjectPtr<UNiagaraComponent> GatheredEffect;

	FTimerHandle WindupTimer;

	/** Recent socket positions with their timestamps, for the Arm Swing tangent. */
	TArray<TPair<float, FVector>> SwingHistory;

	EGatherPhase Phase = EGatherPhase::Idle;

	float Intensity = 0.0f;
	float WindupStartTime = 0.0f;
	float FadeStartTime = 0.0f;
	float FadeFromValue = 1.0f;
	bool bReleased = false;

	/** Survives across activations because the ability is InstancedPerActor -- which is
	 *  exactly what makes a release-based cooldown possible without a gameplay effect. */
	float LastReleaseTime = -BIG_NUMBER;
};
