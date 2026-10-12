// GA_ChannelledCantrip.h
//
// A cantrip held open by the input: a gout of flame, a sustained wind, a stream of
// frost. Runs while the key is down, and stops when it is released OR when the duration
// the roll bought runs out -- whichever comes first. Successes buy both how fierce it is
// and how long it can be held.
//
// Three things separate it from the toggled kind:
//
//   ROOTED. Movement is locked while channelling, so the caster plants and commits.
//   A channel you can walk out of is a channel with no cost, and the animation --
//   which has no locomotion to blend with -- slides its feet across the floor.
//
//   CLEARANCE. It refuses to start facing a wall. Firing a metre-long flame into
//   stone from a hand's breadth away looks like a bug even when it is not.
//
//   INTENSITY. An animation curve drives the effect and its sound rather than the
//   ability doing it on a timer, so the flame swells as the arms come up and dies as
//   they drop, and the two can never drift apart because they read the same source.

#pragma once

#include "CoreMinimal.h"
#include "GA_SustainedCantrip.h"
#include "GA_ChannelledCantrip.generated.h"

class UAudioComponent;
class USoundBase;

UCLASS(Abstract, Blueprintable)
class CHANGELING_API UGA_ChannelledCantrip : public UGA_SustainedCantrip
{
	GENERATED_BODY()

public:
	UGA_ChannelledCantrip();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/**
	 * Let go. Bind to the input's Completed AND Canceled -- this one node covers both
	 * things releasing can mean.
	 *
	 * Released once the flame is burning, it puts it out. Released DURING the wind-up,
	 * before the working ever opened, it abandons the cast entirely: no roll, no
	 * Glamour, nothing happens.
	 *
	 * That asymmetry is deliberate. An ordinary cantrip released early still fires,
	 * weaker, because there is a weaker version of it to fire. A channel has no weaker
	 * version -- it is either open or it is not -- so an abandoned wind-up is better
	 * read as changing your mind than as a fizzle you paid for.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Channelled",
		meta = (DefaultToSelf = "Avatar"))
	static void StopChannelling(AActor* Avatar);

	/** 0..1 from the animation curve, or 1 when no curve is driving it. Drives the
	 *  Niagara Intensity parameter and the loop's volume and pitch. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Channelled")
	float GetChannelIntensity() const { return CurrentIntensity; }

	/** True when something is close enough in front that a channel would be refused. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Channelled",
		meta = (DefaultToSelf = "Avatar"))
	static bool IsForwardBlocked(AActor* Avatar, float Distance = 120.0f, float Height = 60.0f);

protected:
	virtual void PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec) override;

	/**
	 * Wait for the animation to say the gesture has landed, rather than for a clock.
	 *
	 * The AnimBP raises Cantrip.Event.EffectStart on the transition out of the wind-up.
	 * A fallback timer spawns the effect anyway if that never arrives, so a cantrip
	 * whose animations have not been made yet still works -- it simply fires on time
	 * rather than on cue.
	 */
	virtual void ScheduleEffectSpawn() override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// ── Rooting ─────────────────────────────────────────────────────────────
	//
	// The flag, the timeout and the lock itself all live on UGA_Cantrip. This class sets
	// their defaults in its constructor and decides only WHEN the root is taken.

	/**
	 * Not on activate: on channel open.
	 *
	 * Activation is the start of the WIND-UP, and the roll has not happened yet. Rooting
	 * there plants the caster for a cantrip that may still fail, and leaves her standing
	 * in a gesture that produced nothing. The root is taken in PostResolve instead, on
	 * the frame the working actually opens.
	 */
	virtual bool ShouldTakeRootOnActivate() const override { return false; }

	/**
	 * The montage DOES start on activate, unlike the root.
	 *
	 * For a channel the hold is the wind-up, so the Begin section belongs to it. Waiting
	 * for the roll meant the player pressed the key, watched nothing happen for as long
	 * as they held it, and only then saw her brace -- every part of the cast reading late
	 * by exactly the length of the hold.
	 *
	 * Rooting still waits, because a wind-up that may yet fail should not plant her.
	 */
	virtual bool ShouldStartMontageOnActivate() const override { return true; }

	/**
	 * The flame gathers during the wind-up rather than appearing lit.
	 *
	 * The effect-start cue fires inside the Begin section, and refusing to spawn until
	 * the working had opened threw that cue away and left the whole presentation to
	 * arrive at once, at full strength, after the roll.
	 */
	virtual bool CanSpawnSustainedEffect() const override { return true; }

	// ── Clearance ───────────────────────────────────────────────────────────

	/** Refuse to start when something solid is within this distance ahead. Zero
	 *  disables the check. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0"))
	float ForwardClearance = 120.0f;

	/** Height above the actor's origin to probe from — roughly chest height, so a
	 *  kerb underfoot does not read as a wall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled")
	float ClearanceProbeHeight = 60.0f;

	/** Keep checking while channelling, so walking into a wall mid-cast stops it.
	 *  Off by default: with movement locked the caster cannot usually get there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled")
	bool bRecheckClearanceWhileChannelling = false;

	// ── Intensity ───────────────────────────────────────────────────────────

	/**
	 * Animation curve read each sample, 0..1.
	 *
	 * Author it on the montage or sequences: rising through Begin, flat through Loop,
	 * falling through End. Everything the ability presents follows it, so the effect
	 * and the animation cannot fall out of step.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled")
	FName IntensityCurveName = TEXT("EffectIntensity");

	// IntensityParameter, IntensityAtOneSuccess and IntensityPerSuccess live on
	// UGA_SustainedCantrip: a torch grows with the roll exactly as a stream does, and
	// only the CURVE that shapes it over time is particular to a channel.

	/** Seconds between intensity samples. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0"))
	float IntensitySampleInterval = 0.033f;

	// ── Sound ───────────────────────────────────────────────────────────────

	/** Looping cue attached to the caster; volume and pitch follow the curve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled")
	TObjectPtr<USoundBase> ChannelSound;

	/** Pitch at zero intensity and at full, interpolated between. Narrow on purpose --
	 *  past about 20% either way a looping cue starts sounding resampled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled")
	FVector2D ChannelPitchRange = FVector2D(0.9f, 1.1f);

	/**
	 * How much of the roll's fierceness reaches the VOLUME, as an exponent.
	 *
	 * Not a multiplier, because hearing is logarithmic: roughly ten times the acoustic
	 * power to sound twice as loud. At the default, a stream three times the size plays
	 * about half again as loud rather than three times -- which is both what a fire
	 * actually does and what keeps a cue mastered near full scale from clipping.
	 *
	 * Zero makes loudness ignore the roll entirely; one makes it track the flame
	 * literally, which will be too much.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LoudnessExponent = 0.35f;

	/**
	 * How much the roll's fierceness DROPS the pitch, as an exponent.
	 *
	 * Down, not up. A larger body of flame resonates lower -- a big blaze roars where a
	 * small one hisses -- so a strong casting should deepen rather than sharpen. The
	 * result is clamped afterwards, since this compounds with the curve's own ramp
	 * through ChannelPitchRange and a loop pitched too far stops sounding deeper and
	 * starts sounding resampled.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchDropExponent = 0.12f;

	/** Fires when a channel is refused for want of clearance, so Blueprint can play a
	 *  failed-cast cue rather than leaving the player wondering why nothing happened. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Channelled")
	void OnChannelRefused();

	/**
	 * Keep the caster's hands up until the effect has actually gone out.
	 *
	 * Releasing stops the flame being fed, but it still takes FadeOutSeconds to ramp
	 * down and the particles already in the air outlive even that. Recovering the
	 * instant the key comes up therefore drops the arms through fire that is still
	 * burning, which reads as the animation and the effect belonging to different
	 * events.
	 *
	 * With this on, the loop holds for the length of the fade and only then does the
	 * body recover -- you stop feeding it, it gutters out, you lower your hands.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled")
	bool bHoldPoseUntilFaded = true;

	virtual void CancelSustain() override;

	/**
	 * Seconds of wind-up before the working opens on its own, without waiting for the
	 * input to be released.
	 *
	 * Zero uses the cantrip's own full cast ladder, which is the intended setting: the
	 * flame kindles at the moment the gesture is complete, and keeping the key down
	 * from there simply holds it open. Because the channel therefore always resolves
	 * at the top of the ladder, the cast-time tiers do not vary a channelled cantrip's
	 * difficulty the way they vary an ordinary one's -- the sustained gesture IS the
	 * full performance, so there is no shorter version of it to reward or punish.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0"))
	float ChannelOpensAfterSeconds = 0.0f;

	/**
	 * Seconds after the gesture begins before the flame gathers in her hands.
	 *
	 * Measured from ACTIVATION, so it lands at a fixed point in the raise however long
	 * the cast is held. Set it to the frame the intensity curve starts climbing and the
	 * fire, the sound and the arms all come up together.
	 *
	 * The animation's effect-start cue pre-empts this if it arrives first, which is the
	 * arrangement worth having: the cue moves with the clip when it is retimed, and this
	 * is what happens when there is no cue, or when there is one that never fires.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0"))
	float GatherDelay = 0.6f;

	/** Seconds to wait for the animation's effect-start cue before giving up and
	 *  spawning the effect regardless. Now only a backstop for the post-roll path;
	 *  GatherDelay above is what normally decides when the flame appears. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0"))
	float EffectStartTimeout = 2.0f;

private:
	/** Start the looping cue and the intensity sampling. Called from ActivateAbility so
	 *  the curve has the whole wind-up to ramp across. */
	void StartChannelPresentation();

	void SampleIntensity();
	void HandleEffectStartEvent(const FGameplayEventData* Payload);
	void ListenForEffectStart();
	void StopListeningForEffectStart();

	UPROPERTY() TObjectPtr<UAudioComponent> ChannelAudio;

	FTimerHandle IntensityTimer;
	FTimerHandle ChannelOpenTimer;
	FTimerHandle EffectStartFallbackTimer;
	FTimerHandle RecoveryHoldTimer;
	FDelegateHandle EffectStartDelegate;
	float CurrentIntensity = 1.0f;

	/**
	 * The animation has already said the gesture landed.
	 *
	 * Latched rather than acted on directly, because the cue routinely arrives BEFORE
	 * the working opens: the wind-up animation finishes when it finishes, and the roll
	 * resolves when the cast ladder says so. Whichever happens second does the spawning.
	 */
	bool bEffectStartSeen = false;

	/** Whether the Channelling tag was actually added, so a fizzled cast does not
	 *  remove one it never put there. */
	bool bChannellingTagged = false;
};
