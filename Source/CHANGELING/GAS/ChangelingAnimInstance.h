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

/** Declared rather than included: only the foot-plant notifies need it. */
enum class EFootSide : uint8;

/** A channel of the solved aim rotation, or nothing at all. */
UENUM(BlueprintType)
enum class ECantripAimChannel : uint8
{
	/** Contributes nothing. Use for a bone axis that should stay still. */
	None,
	Pitch,
	Yaw,
	Roll
};

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

	// ── Flight ──────────────────────────────────────────────────────────────
	//
	// Only what the template has no idea about, per the rule above. ABP_Unarmed already
	// computes ground speed and direction, and those are exactly the two axes BS_8WayFly
	// wants (Rotation -180..180, Speed 0..400) -- so the flight state can drive that blend
	// space from the variables the graph already has. What the template cannot know is
	// that she is flying at all, and how fast she is climbing.

	/**
	 * Which of the three gaits she has actually chosen: Walk, Jog or Sprint.
	 *
	 * The STATE, not the speed. A state machine that tests velocity is asking the wrong
	 * question -- a Quicksilver speed boost moves her at a sprint's velocity while she is
	 * merely jogging, and a blend space driven off speed alone will show her sprinting for
	 * free. Sprint is the tier that costs stamina, so it should be the tier that reads as
	 * sprinting, and only this says so.
	 *
	 * Use it to GATE the state (enter Sprint only when this is Sprint) and let the graph's own
	 * GroundSpeed drive the blend inside it. The two answer different questions and both are needed.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	EChangelingGait Gait = EChangelingGait::Jog;

	/**
	 * Where she is in the arc of a flight: crouching, airborne, falling, or picking herself up.
	 *
	 * Separate from bFlying because two of the five phases happen with her feet on the ground.
	 * Gate a Windup state and a Landing state on this; bFlying only covers the middle.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	EChangelingFlightPhase FlightPhase = EChangelingFlightPhase::Grounded;

	/** Crouched and gathering. A one-shot pose, held until the launch. */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	bool bFlightWindup = false;

	/** Down and playing the impact. Drives the landing montage or state. */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	bool bFlightRecovering = false;

	/**
	 * How fast she hit the ground on the last flight landing.
	 *
	 * Kept as the raw speed rather than only the heavy/light verdict, so a landing state can
	 * scale a play rate or a lean by it instead of choosing between two fixed poses.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	float LandingImpactSpeed = 0.0f;

	/**
	 * She is in MOVE_Flying. Use this as the entry and exit condition for a Flight state.
	 *
	 * Read off the movement mode rather than off a cantrip, deliberately: the graph should
	 * not care WHAT made her weightless, and one day something other than Wind Runner will.
	 * The character owns the mode and re-derives it every tick from FallSpeedMultiplier,
	 * so this cannot get stuck true after the thing holding her up has gone.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	bool bFlying = false;

	/**
	 * Vertical speed while flying, signed -- positive is climbing. Centimetres per second.
	 *
	 * BS_8WayFly is a two-axis blend space (Rotation and Speed) with nothing vertical in
	 * it, so climbing and diving both come out of it looking like a hover. Drive an
	 * additive lean from this, or use it as the third axis if the blend space ever grows
	 * one.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	float FlightClimbRate = 0.0f;

	// ── Trailing legs ───────────────────────────────────────────────────────
	//
	// BS_8WayFly holds the legs where the animator left them, which reads as hanging
	// rather than flying: a body moving forward through air has its legs pushed BACK by
	// the air it is moving through, and the faster it goes the further back they trail.
	//
	// Solved here and applied as two Modify Bone nodes on thigh_l and thigh_r, exactly as
	// the aim chain is -- the AnimGraph does no arithmetic, it plugs in a rotator.

	/**
	 * Rotation for thigh_l. Plug into a Modify Bone, Add to Existing, Bone Space.
	 *
	 * SEPARATE from the right leg's, and that is not redundancy. The mannequin's thigh
	 * bones are mirrored, so one local rotation applied to both splays them apart instead
	 * of swinging both back -- which of the two needs negating is a fact about how the
	 * skeleton was authored, not something to reason out. See LegTrailRightSign.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	FRotator LegTrailLeft = FRotator::ZeroRotator;

	/** Rotation for thigh_r. */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	FRotator LegTrailRight = FRotator::ZeroRotator;

	/**
	 * Alpha for both Modify Bone nodes, 0 to 1.
	 *
	 * Rises with speed and falls to nothing when she is not flying, so the nodes can be
	 * left permanently enabled -- a bone offset that has to be switched off by a branch
	 * is a bone offset that eventually gets left on.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	float LegTrailAlpha = 0.0f;

	/**
	 * How far back the legs swing at full speed, in degrees.
	 *
	 * Deliberately modest, because it is the SECOND thing swinging them back: the
	 * character leans the whole mesh forward (MaxFlightPitch) and that already lifts the
	 * feet relative to the world. This adds the last of it -- legs trailing behind a body
	 * that is already streamlined, rather than doing the whole job alone.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxLegTrailAngle = 25.0f;

	/**
	 * Horizontal speed at which the trail reaches its maximum.
	 *
	 * 400 to match BS_8WayFly's own Speed axis, so the legs finish trailing at the same
	 * moment the blend space reaches its fastest sample. Tuning them apart is what makes
	 * a flight pose look like two effects fighting.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "1.0"))
	float LegTrailFullSpeed = 400.0f;

	/** How fast the trail eases in and out. Air does not move a leg instantly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.1"))
	float LegTrailInterpSpeed = 3.0f;

	/**
	 * Which channel of the thigh bone the swing is applied to, and which way.
	 *
	 * Same problem the aim chain documents at length: a bone's axes are an authoring
	 * decision, so which channel produces a backward swing is a property of THIS skeleton.
	 * Set it by watching -- fly forward and see which way the legs go.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Axes")
	ECantripAimChannel LegTrailChannel = ECantripAimChannel::Pitch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Axes")
	float LegTrailLeftSign = 1.0f;

	/** Usually the negative of the left. Mirrored bones, mirrored sign. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Axes")
	float LegTrailRightSign = -1.0f;

	// ── Straightening the knees ─────────────────────────────────────────────
	//
	// BS_8WayFly was authored for an UPRIGHT hover, and tucked legs are correct there --
	// a figure hanging in the air folds its knees. Pitched to horizontal that same pose
	// reads as sitting down in mid-air, and the fold at the hip is what makes the whole
	// silhouette look bent even though the spine is straight.
	//
	// Swinging the thighs back does not fix it: the knee is a separate joint and keeps
	// its bend whatever the thigh does. This extends the calf to put the leg back in line.

	/** Rotation for calf_l. Plug into a Modify Bone alongside the thigh nodes. */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	FRotator FlightCalfLeft = FRotator::ZeroRotator;

	/** Rotation for calf_r. */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	FRotator FlightCalfRight = FRotator::ZeroRotator;

	/**
	 * How far the knee is straightened at full speed, in degrees.
	 *
	 * Enough to undo a roughly ninety-degree tuck without going past straight -- a leg
	 * extended beyond its limit hyperextends at the knee, which reads as a broken rig
	 * rather than as a streamlined one.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float MaxKneeStraighten = 60.0f;

	/** Which channel of the CALF bone extends the knee, and which way per side. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Axes")
	ECantripAimChannel KneeChannel = ECantripAimChannel::Pitch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Axes")
	float KneeLeftSign = -1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Axes")
	float KneeRightSign = 1.0f;

	// ── Head lift ───────────────────────────────────────────────────────────
	//
	// The body is pitched toward horizontal, and the head is on the end of it. Left alone
	// she flies looking at her own chest; a person doing this looks WHERE THEY ARE GOING,
	// which means the head cancels most of the body's lean.
	//
	// Not part of the aim chain, deliberately. That chain measures the camera against the
	// ACTOR and the actor never pitches -- it cannot see the lean at all. This reads the
	// lean straight off the character and answers it.

	/** Rotation for the head bone. Plug into a Modify Bone, Add to Existing, Bone Space. */
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	FRotator FlightHeadLift = FRotator::ZeroRotator;

	/**
	 * How much of the body's lean the head cancels, 0 to 1.
	 *
	 * Not 1. Cancelling the lean exactly puts her eyes dead level, which reads as a
	 * mannequin bolted to a moving body -- a person flying head-first still carries some
	 * of the dive in their neck, and looks slightly down along their own line of travel.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlightHeadLiftFraction = 0.75f;

	/** Ceiling on the lift. A neck has a limit and passing it looks broken, not athletic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxFlightHeadLift = 55.0f;

	/** Which channel of the HEAD bone lifts the face, and which way. Set it by watching. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Axes")
	ECantripAimChannel FlightHeadLiftChannel = ECantripAimChannel::Pitch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Axes")
	float FlightHeadLiftSign = -1.0f;

	// ── Cantrip state ───────────────────────────────────────────────────────

	/**
	 * A montage has claimed the WHOLE body -- a kneeling ritual, a summoning.
	 *
	 * Read off the montages actually playing rather than off the ability, because that
	 * is the question every consumer is really asking: is the pose under this character's
	 * feet authored, or is locomotion still driving them? An upper-body gesture leaves
	 * this false, and she keeps walking.
	 *
	 * This used to mean "the cantrip has clips authored", which routed every cantrip
	 * into a spellcasting state machine whether it had anything to play or not. The
	 * state machine is gone; montages replaced it, and the slot the montage names is
	 * now the only thing that decides how much of her a cantrip takes.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bCantripCasting = false;

	/**
	 * A cantrip is being cast at all, whatever it is doing to the body.
	 *
	 * Separate from bCantripCasting on purpose: the aiming stance belongs to the act of
	 * casting, and a thrown cantrip wants the spine turned even though the throw only
	 * claims her arms. Tying the aim to the full-body flag would drop the stance for
	 * exactly the cantrips that most need it.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bCantripActive = false;

	/**
	 * The slot whose montages own the whole body.
	 *
	 * A name rather than a hardcode so a second skeleton with its own naming can be
	 * pointed at its own slot without a rebuild.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip")
	FName FullBodySlotName = TEXT("DefaultSlot");

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

	/** Attitude forced by whatever cantrip is running. Picks which pose to blend in. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	ECantripPose CantripPose = ECantripPose::None;

	/**
	 * How far into that attitude she is, 0 to 1. Drive a Layered Blend Per Bone with it.
	 *
	 * Exists because an authored pose needs a weight and an enum has none. Modify Bone
	 * carried its own Blend In/Out settings, so nothing outside the node ever had to know
	 * the arm was moving; a blend node takes an alpha instead, and easing it in Blueprint
	 * means a timeline per pose that has to be kept in step with this enum.
	 *
	 * Linear rather than eased: the ease belongs in the authored clip, where an animator
	 * can see it. Two curves fighting is how a raise ends up looking soft in the middle
	 * and abrupt at the ends.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	float CantripPoseAlpha = 0.0f;

	/**
	 * Seconds to reach a pose, and to leave it.
	 *
	 * A third of a second is about how long a person takes to raise an arm deliberately.
	 * Faster reads as a flinch; slower reads as reluctance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip",
		meta = (ClampMin = "0.01"))
	float CantripPoseBlendSeconds = 0.3f;

	/** The raised hand is against something. Gates the Two Bone IK that pulls it back. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bCantripHandBlocked = false;

	/** Elemental quarter of the running cantrip, for picking additive flavour. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	ECantripElement CantripElement = ECantripElement::None;

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

	/**
	 * Whether this body's skeleton can support foot IK at all.
	 *
	 * CR_Mannequin_FootIK solves against ik_foot_root, ik_foot_l and ik_foot_r. The UE5
	 * Mannequin has them; metahuman_base_skel does NOT -- it carries every ordinary bone
	 * under the same names, which is why Mannequin animations play on a MetaHuman without
	 * retargeting, but ships no ik_ chain at all. Run the solver anyway and it cannot
	 * resolve, so the legs hold their reference pose while the arms animate normally.
	 *
	 * Resolved once at initialisation rather than per frame: bones do not appear partway
	 * through a life, and this is a name lookup against the reference skeleton.
	 *
	 * Not fixable by adding the bones. The assembly pipeline regenerates both the body mesh
	 * and its skeleton on every run, so anything added by hand is gone the next time the
	 * character is assembled -- across every kith. The real answer is a foot-IK Control Rig
	 * authored against foot_l/foot_r directly, which would run on both skeletons; until that
	 * exists, MetaHumans go without foot IK rather than with frozen legs.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bSkeletonSupportsFootIK = true;

	/**
	 * The same answer as a weight: 1 while foot IK should run, 0 while it should not.
	 *
	 * PLUG THIS INTO THE CONTROL RIG NODE'S ALPHA, not the bool. A Control Rig node takes
	 * a float Alpha, and switching a solver off between one frame and the next snaps the
	 * feet from wherever IK had them to wherever the clip has them -- which reads as a
	 * flinch at exactly the moment she leaves the ground, when the player is looking.
	 *
	 * Eased in both directions, so taking off fades the solver out over a few frames and
	 * landing fades it back in. The bool above is still the honest yes/no for a transition
	 * rule or a branch; this is the number for a blend.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	float FootIKAlpha = 1.0f;

	/**
	 * Seconds for foot IK to fade out and back in.
	 *
	 * Short. This is not a stylistic blend -- it exists only to stop a one-frame snap, and
	 * anything long enough to notice means the solver is still reaching for the ground for
	 * a visible moment after she has left it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip",
		meta = (ClampMin = "0.0"))
	float FootIKBlendSeconds = 0.15f;

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

	/**
	 * The share each NECK bone takes. Plug into neck_01 and neck_02.
	 *
	 * Optional, and it is not extra rotation on top -- the neck's share is taken OUT
	 * of the spine's. Bones compose down the hierarchy, so the head already arrives at
	 * the full aim from its ancestors; adding neck rotation without removing it from
	 * the spine would simply point the head at twice the angle.
	 *
	 * What it buys is where the effort reads. Neck-heavy turns the head while the
	 * chest stays put; spine-heavy swings the whole torso and takes the arms with it.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Aiming")
	FRotator NeckBoneAim = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "1"))
	int32 NeckBoneCount = 2;

	/**
	 * How much of the total the neck carries, 0 to 1.
	 *
	 * Kept low on purpose. Anything projected from the HANDS needs the spine to do the
	 * work, because the arms hang off the clavicles and only follow what the spine
	 * does. A neck-heavy aim points the face at the target and the flame somewhere
	 * else. Raise it for a cantrip aimed by gaze rather than by hand.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NeckAimFraction = 0.2f;

	/** The whole clamped, smoothed aim offset, before dividing. For anything that
	 *  wants the total rather than a bone's share. */
	UPROPERTY(BlueprintReadOnly, Category = "Aiming")
	FRotator SpineAim = FRotator::ZeroRotator;

	/** How many bones the turn is divided between. Five spine bones on Manny. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "1"))
	int32 SpineBoneCount = 5;

	/**
	 * How far the aim may turn left or right in total, in degrees either side.
	 *
	 * This is the number that decides where the caster can point. The per-bone limit
	 * below is a safety net for individual joints, not the reach.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAimYaw = 75.0f;

	/**
	 * Where the caster is ACTUALLY pointing, in world space.
	 *
	 * The body's aim after clamping -- not where the camera is looking. Anything
	 * projected from the hands must use this, or it separates from the character the
	 * moment the camera turns further than a spine can: the flame keeps tracking the
	 * eyeline while the arms stop, and it leaves the hands sideways.
	 */
	UFUNCTION(BlueprintPure, Category = "Aiming")
	FRotator GetAimWorldRotation() const;

	/**
	 * Limit on any ONE bone, in degrees either side.
	 *
	 * Clamped per bone rather than on the total, so no single joint can be asked for
	 * more than a joint can give however far the camera swings. Five spine bones at 15
	 * is the ~75 degree arc; the ceiling is a property of the bone, not of the aim.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MaxPerBoneAngle = 15.0f;

	/**
	 * How far the aim may tilt up and down, in degrees, before dividing.
	 *
	 * Asymmetric on purpose: a spine bends forward far more readily than it arches
	 * back, and an equal range either way reads as a puppet.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxAimPitchUp = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxAimPitchDown = 55.0f;

	/**
	 * Sign flips mapping the aim's axes onto the BONE's axes.
	 *
	 * A rotator measured between two world rotations and a rotator applied to a spine
	 * bone do not agree about which way is positive -- a skeleton's joint axes are an
	 * authoring decision, not a convention. Left unmapped the character bends when it
	 * should twist, or twists the wrong way.
	 *
	 * Exposed as settings rather than baked in because the right combination is a fact
	 * about THIS skeleton, and finding it should cost one PIE session rather than a
	 * recompile. Defaults follow the UE mannequin.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming|Axes")
	float AimPitchSign = -1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming|Axes")
	float AimYawSign = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming|Axes")
	float AimRollSign = -1.0f;

	/**
	 * Send the aim's YAW to the bone's ROLL, and its roll to the bone's yaw.
	 *
	 * Needed when the bone's long axis runs up the spine, which is the usual authoring
	 * for a spine chain: turning about that axis IS the twist, but it arrives named
	 * roll. Without the swap, asking for yaw folds the character sideways instead.
	 *
	 * Leave off for Component Space, on for Bone Space.
	 */
	/**
	 * Which channel of the AIM drives each channel of the BONE.
	 *
	 * Three dropdowns rather than a row of swap booleans, because the mapping that is
	 * actually needed is often a three-way rotation of the channels and no combination
	 * of pairwise swaps can express one. The aim is solved in the actor's frame while
	 * the Transform (Modify) Bone applies it in the mesh's, and a character mesh is
	 * yawed -90 from its actor by default -- so which aim channel ends up producing a
	 * nod rather than a lean is a property of the rig, not something to reason out.
	 *
	 * Set them by watching: move one mouse axis, see which way the character moves.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming|Axes")
	ECantripAimChannel BonePitchFrom = ECantripAimChannel::Pitch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming|Axes")
	ECantripAimChannel BoneYawFrom = ECantripAimChannel::Yaw;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming|Axes")
	ECantripAimChannel BoneRollFrom = ECantripAimChannel::Roll;

	/**
	 * Only aim while a cantrip is being cast.
	 *
	 * On by default: a character who twists at the waist every time the camera moves
	 * looks possessed. The stance belongs to the act of casting.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	bool bAimOnlyWhileCasting = true;

	/**
	 * Turn the head and shoulders toward the eyeline while flying, without any input.
	 *
	 * Independent of bAimOnlyWhileCasting: that gate exists so the spine does not twist
	 * every time the camera moves on the ground, and flight is the case where it should.
	 * The body is already pitched toward horizontal by the character, so this is what
	 * lifts her face out of the dive -- without it she flies looking at her own chest.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	bool bAimWhileFlying = true;

	/** How fast the twist chases the camera. Interpolated rather than snapped: the
	 *  camera can flick instantly and a body cannot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.1"))
	float AimInterpSpeed = 10.0f;

	/**
	 * The owner as a plain character. Set for ANY character, including NPCs.
	 *
	 * Separate from ChangelingCharacter below because most of what this class publishes is
	 * not Changeling-specific at all -- falling, flying, ground speed and the movement mode
	 * come from the movement component, which every character has. Gating those on the
	 * cantrip-aware subclass meant an NPC got no locomotion data of any kind: the cast
	 * failed, initialisation returned early, and every pin sat at its default while she
	 * slid across the garden in a T-pose.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "References")
	TObjectPtr<class AChangelingCharacterBase> OwnerCharacter;

	/**
	 * The owner as a CHANGELING character, or null.
	 *
	 * Null on an NPC, and that is a supported state rather than a failure -- everything
	 * reading it already checks. Cantrips, aiming and the flight lean simply have nothing
	 * to say about a gardener.
	 */
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

	/** Swings the legs back in proportion to how fast she is actually flying. */
	void RefreshLegTrail(float DeltaSeconds);

	/** Works out whether this body's skeleton carries the ik_ chain foot IK solves against. */
	void RefreshFootIKSupport();

	/**
	 * Foot-plant notifies, handled natively so NPCs get footsteps without Blueprint wiring.
	 *
	 * These fire for anything running ABP_Unarmed. The player already routes the same two
	 * notifies through the BI_Footstep Blueprint interface on BP_ThirdPersonCharacter, so
	 * both handlers would run for her and every step would play twice -- hence the guard in
	 * PlayNativeFootstep, which does nothing when the owner is the player.
	 *
	 * Named to match the notifies themselves; UAnimInstance dispatches by
	 * AnimNotify_<NotifyName>, so these names are not free to tidy.
	 */
	//
	// BlueprintNativeEvent rather than a plain UFUNCTION because ABP_Unarmed already
	// implements both of these in its own graph. A Blueprint function that shadows a
	// non-overridable C++ UFUNCTION of the same name will not compile -- "Overriden
	// function is not compatible with the parent function" -- and the notify names are
	// not free to change, since UAnimInstance dispatches by AnimNotify_<NotifyName>.
	//
	// Declaring them overridable makes the existing Blueprint functions legal overrides.
	// The consequence is that the native body no longer runs unless the Blueprint calls
	// its parent, so ABP_Unarmed needs a Parent node added in each of those two functions.
	// With that in place both paths coexist correctly: the player gets the Blueprint's
	// BI_Footstep call and the native body declines to act, while an NPC implements no
	// interface and is served by the native body alone.
	// Protected rather than private: UHT rejects a private BlueprintNativeEvent, and it is
	// right to -- a Blueprint cannot override what it cannot see.
protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Footstep")
	void AnimNotify_l_foot_plant();

	UFUNCTION(BlueprintNativeEvent, Category = "Footstep")
	void AnimNotify_r_foot_plant();

private:
	/** Plays one step on the owner's footstep component, unless the owner is the player. */
	void PlayNativeFootstep(EFootSide Foot);

	/** Lifts the head out of the dive so she looks where she is going. */
	void RefreshFlightHeadLift();

	/** Eased magnitude of the leg swing, 0-1. Kept so the interp has somewhere to live. */
	float LegTrailBlend = 0.0f;

	/**
	 * Whether any montage currently playing writes into the named slot.
	 *
	 * Walks every montage instance rather than asking for the active one, because with
	 * two slots there can genuinely be two -- a torch held on UpperBody while something
	 * else plays full-body -- and GetActiveMontageInstance would answer for whichever
	 * started last.
	 */
	bool IsMontagePlayingOnSlot(FName SlotName) const;

	/** One bone's share of the aim, mapped onto that bone's axes and clamped. */
	FRotator MakeBoneAim(const FRotator& Aim, float Share) const;

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
