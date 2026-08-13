// ChangelingAnimInstance.h
//
// The animation layer's view of a changeling.
//
// Everything the AnimBP needs to know is DERIVED here, once per frame, from the
// character and its ability system. Nothing sets these from outside.
//
// That is the whole point. The obvious way to animate an ability is for the ability
// to reach into the AnimBP and set a boolean, and it works right up until two
// abilities overlap, or one is interrupted, or a cast is cancelled between the set
// and the clear -- and then the flag says "casting" forever and the character is
// frozen with its arms up. Reading the state instead means there is no flag to get
// stuck: the ASC either has the tag or it does not, and when the ability dies for
// ANY reason the tag goes with it.
//
// Traffic in the other direction -- the animation telling the gameplay when the
// gesture has landed -- goes through the two AnimNotify functions at the bottom.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CantripTypes.h"
#include "ChangelingAnimInstance.generated.h"

class ACHANGELINGCharacter;
class UAbilitySystemComponent;
class UCharacterMovementComponent;
class UGA_Cantrip;

UCLASS()
class CHANGELING_API UChangelingAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Deliberately no locomotion here. ABP_Unarmed already computes its own ground
	// speed, direction and fall state, and a parent class publishing a second copy
	// would both block the reparent on the name clash and leave two sources of truth
	// for the same fact. This class covers only what the template has no idea about.

	// ── Cantrip state ───────────────────────────────────────────────────────

	/**
	 * A cantrip is being cast: the spellcasting state machine owns the body.
	 *
	 * Covers the whole span -- wind-up, hold, resolution, recovery -- so it stays true
	 * across the End animation and only drops once the ability is completely done.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bCantripCasting = false;

	/**
	 * The working is open and being held. Drives the loop.
	 *
	 * Falls before bCantripCasting does, and that gap IS the recovery: the loop exits,
	 * the End animation plays, and the cast state clears when the ability ends.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bCantripChannelling = false;

	/** Between the channel closing and the ability ending -- the End animation's
	 *  window. Convenience for a transition rule that would otherwise read
	 *  "casting AND NOT channelling" in four places. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bCantripRecovering = false;

	/** Attitude forced by whatever cantrip is running. Drives the Modify Bone nodes. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	ECantripPose CantripPose = ECantripPose::None;

	/** The raised hand is against something. Gates the Two Bone IK that pulls it back. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bCantripHandBlocked = false;

	/** Elemental quarter of the running cantrip, for picking additive flavour. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	ECantripElement CantripElement = ECantripElement::None;

	/**
	 * The running cantrip's three clips, for the sequence players to read.
	 *
	 * Bind the Begin/Loop/End state's Sequence pin to the matching member and one set
	 * of states serves every cantrip in the game.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	FCantripAnimSet CantripAnims;

	/**
	 * The EffectIntensity curve's current value, republished as a variable.
	 *
	 * Curves are readable from Blueprint already; this exists so the value survives
	 * being blended between states, where reading the curve directly gives whatever
	 * the currently dominant clip happens to say.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	float ChannelIntensity = 0.0f;

	// ── Derived gates ───────────────────────────────────────────────────────

	/**
	 * Whether foot IK should run at all.
	 *
	 * False while casting, because the casting clips were authored for a stationary
	 * body and have no ground contact to solve against -- IK against them lifts the
	 * feet clear of the floor. False while falling for the same reason: there is no
	 * ground to reach for.
	 *
	 * Derived rather than exposed as something to set, because the one thing that must
	 * never happen is IK left switched off after a cast that ended badly.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bAllowFootIK = true;

	// ── Aiming ──────────────────────────────────────────────────────────────
	//
	// The caster turns to look where the camera looks, by twisting the spine rather
	// than by spinning the whole capsule. Every part of that is solved here so the
	// AnimGraph does no arithmetic: it plugs ONE rotator into five Modify Bone nodes.

	/** True while the aiming stance is held. Drives the Modify Bone nodes' alpha. */
	UPROPERTY(BlueprintReadOnly, Category = "Aiming")
	bool bAimingActive = false;

	/**
	 * The share of the aim each spine bone takes: the total divided by the bone count.
	 *
	 * Plug this same rotator into ALL FIVE Modify Bone nodes. Splitting the turn evenly
	 * down the spine is what makes it read as a body twisting rather than a head being
	 * wrenched round -- one bone taking the whole 75 degrees looks broken.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Aiming")
	FRotator SpineBoneAim = FRotator::ZeroRotator;

	/** The whole clamped, smoothed aim offset, before dividing. For anything that
	 *  wants the total rather than a bone's share. */
	UPROPERTY(BlueprintReadOnly, Category = "Aiming")
	FRotator SpineAim = FRotator::ZeroRotator;

	/** How many bones the turn is divided between. Five spine bones on Manny. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "1"))
	int32 SpineBoneCount = 5;

	/**
	 * Limits on the whole turn, in degrees either side.
	 *
	 * A spine has a range; past it the mesh shears and the silhouette breaks. Clamping
	 * means looking hard over your shoulder simply stops at the limit rather than
	 * folding the character in half.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	float MaxAimYaw = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	float MaxAimPitch = 60.0f;

	/** How fast the twist chases the camera. Interpolated rather than snapped: the
	 *  camera can flick instantly and a body cannot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.1"))
	float AimInterpSpeed = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "References")
	TObjectPtr<ACHANGELINGCharacter> ChangelingCharacter;

	// ── Animation to gameplay ───────────────────────────────────────────────

	/**
	 * The gesture has landed; the working may become visible.
	 *
	 * Reachable two ways, and both are wanted. As a TRANSITION EVENT (type the name
	 * into the transition's Start/End Transition Event field) it fires on the blend
	 * between states, which is where a wind-up actually finishes. As an ordinary
	 * notify on a sequence's timeline it fires at a chosen frame, which is better when
	 * one clip does the whole gesture.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void AnimNotify_CantripEffectStart();

	/**
	 * Recovery is complete; the body is the player's again.
	 *
	 * Put this on the transition LEAVING the spellcasting state machine, not on the
	 * End clip's timeline. The transition is taken on every exit -- released,
	 * interrupted, cancelled, killed mid-gesture -- whereas a notify inside a clip is
	 * only reached if that clip is played to the frame it sits on. The one bug this
	 * avoids is a character that never gets its movement back.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void AnimNotify_CantripRecovered();

private:
	void RefreshCantripState();
	void RefreshAim(float DeltaSeconds);

	/** The cantrip currently running, or null. Cached because it is wanted every frame
	 *  while casting and finding it means walking the ASC's activatable list. */
	TWeakObjectPtr<const UGA_Cantrip> ActiveCantrip;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UCharacterMovementComponent> Movement;

	/** Kept private rather than published: ABP_Unarmed already has a variable of this
	 *  name, and only bAllowFootIK needs the answer. */
	bool bIsFalling = false;

	/** Whether this cast ever got as far as opening a channel. Cleared when the cast
	 *  ends, so it describes the cast in progress and not the one before it. */
	bool bHasChannelledThisCast = false;
};
