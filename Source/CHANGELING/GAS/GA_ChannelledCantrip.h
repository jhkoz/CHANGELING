// GA_ChannelledCantrip.h
//
// A cantrip held open by the input: a gout of flame, a sustained wind, a stream of
// frost. Runs while the key is down and stops when it is released.
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

	/** Plant the caster for the duration. Restored to walking on release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Channelled")
	bool bLockMovementWhileChannelling = true;

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

	/** Niagara user parameter the intensity is written to. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Channelled")
	FName IntensityParameter = TEXT("Intensity");

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

	/** Fires when a channel is refused for want of clearance, so Blueprint can play a
	 *  failed-cast cue rather than leaving the player wondering why nothing happened. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cantrip|Channelled")
	void OnChannelRefused();

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

	/** Seconds to wait for the animation's effect-start cue before giving up and
	 *  spawning the effect regardless. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0"))
	float EffectStartTimeout = 1.0f;

	/**
	 * Seconds the caster stays planted after release, waiting for the recovery cue.
	 *
	 * Long enough to cover the End animation, and no longer: this doubles as the
	 * deadline after which the character is unrooted whether the animation reported in
	 * or not.
	 */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Channelled",
		meta = (ClampMin = "0.0"))
	float RecoveryTimeout = 1.5f;

private:
	void SampleIntensity();
	void SetMovementLocked(bool bLocked);
	void HandleEffectStartEvent(const FGameplayEventData* Payload);
	void StopListeningForEffectStart();

	UPROPERTY() TObjectPtr<UAudioComponent> ChannelAudio;

	FTimerHandle IntensityTimer;
	FTimerHandle ChannelOpenTimer;
	FTimerHandle EffectStartFallbackTimer;
	FDelegateHandle EffectStartDelegate;
	float CurrentIntensity = 1.0f;
	bool bMovementLocked = false;
};
