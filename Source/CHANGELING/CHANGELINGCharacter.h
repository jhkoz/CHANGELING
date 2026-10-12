// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ChangelingCharacterBase.h"
#include "GAS/CantripTypes.h"
#include "Logging/LogMacros.h"
#include "CHANGELINGCharacter.generated.h"

class UAbilitySystemComponent;

// FChangelingAbilitiesChanged is declared by AChangelingCharacterBase, which this includes.
class UChangelingAbilityTraitSet;
class UChangelingAttributeSet;
class UChangelingMagicSet;
class UChangelingMovementSet;
class UWaterMovementComponent;
class UGameplayAbility;
class UGameplayEffect;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

// LogTemplateCharacter is declared by AChangelingCharacterBase.

/** How the body answers the aim while a sustained cantrip is being pointed. */
UENUM(BlueprintType)
enum class EAimFacingMode : uint8
{
	/** Body holds still; the spine carries the whole offset, fenced by the yaw limit. */
	TwistOnly UMETA(DisplayName = "Twist Only"),

	/**
	 * Spine carries small offsets; the body swings round once the aim leaves a
	 * deadzone. What a person actually does -- glance without moving, turn when the
	 * glance stops being comfortable.
	 */
	Lazy UMETA(DisplayName = "Lazy Turn"),

	/** Body takes the camera's yaw directly. No twist, no lag. */
	Locked UMETA(DisplayName = "Locked To Aim")
};

// EChangelingGait now lives in GAS/CantripTypes.h, which this file already includes -- the
// animation layer needs it as well, and both include that header already.

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 *
 *  Also the GAS host. The AbilitySystemComponent and the three attribute sets live
 *  here rather than on the PlayerState: this is a single-player game, the character
 *  IS the persistent thing, and putting them here means every Blueprint deriving from
 *  this class gets a working ability system with no wiring of its own.
 */
UCLASS(abstract)
class ACHANGELINGCharacter : public AChangelingCharacterBase
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/**
	 * Held to sprint. Bind to Left Shift.
	 *
	 * A HOLD, not a toggle, and deliberately unlike the walk key beside it. Sprinting is the
	 * only gait that spends anything, so it should require the player to keep asking for it:
	 * a toggled sprint drains the bar while its owner is reading the map.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SprintAction;

	/** Toggle Walk/Run Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ToggleWalkRunAction;

	/** Camera Zoom Input Action (mouse wheel, Axis1D) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ZoomAction;

	/** Toggle 1st/3rd person view Input Action (middle mouse, Boolean) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ToggleViewAction;

	/**
	 * Drop every cantrip currently being held. Optional -- leave unset and nothing binds.
	 *
	 * One key for all of them rather than one per cantrip, because the moment this is
	 * wanted is a moment when something has gone wrong: she is flying somewhere she did
	 * not mean to be, or a working is running that she wants off NOW. Hunting for the
	 * right hotbar slot is not what that moment needs.
	 *
	 * Pressing a cantrip's own hotbar key still toggles just that one.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* CancelCantripsAction;


	/**
	 * Speeds before anything acts on them.
	 *
	 * Renamed from MaxRunSpeed/MaxWalkSpeed, and the rename is the point: the old name
	 * was character for character the same as UCharacterMovementComponent::MaxWalkSpeed,
	 * which is the LIVE value the engine actually moves her at. Two things called
	 * MaxWalkSpeed, one of them an input and the other the output, is how you end up
	 * writing the answer into the question. BaseWalkSpeed cannot be mistaken for it.
	 *
	 * Old Blueprint-authored values are carried over by PropertyRedirects in
	 * DefaultEngine.ini; without those, every Blueprint that set these would have
	 * silently reverted to the defaults here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	float BaseRunSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseWalkSpeed = 200.0f;

	/**
	 * Sprint speed as a multiple of the jog, rather than a speed of its own.
	 *
	 * Relative so that anything changing how fast she runs -- a mount, a difficulty setting,
	 * SetBaseSpeeds -- carries the sprint with it. An absolute third number would have to be
	 * kept in step with the second by hand, and the first time it was not, sprinting would
	 * be slower than jogging.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
		meta = (ClampMin = "1.0"))
	float SprintSpeedMultiplier = 2.0f;

	/** Camera control variables */
	float TargetArmLength = 400.0f;
	bool bIsFirstPerson = false;

	/**
	 * The gait she has CHOSEN -- Walk or Jog only. Caps Lock flips between them.
	 *
	 * Never written by anything else. Being winded does not change it, casting does not
	 * change it, and neither does sprinting: those are conditions on top of a choice, not
	 * replacements for it. That is precisely what makes coming out of a cantrip return her
	 * to the speed she was using when she started it -- there is nothing to restore, because
	 * nothing was overwritten.
	 *
	 * The old bIsWalking did the opposite: being winded set it true, so recovering left her
	 * walking even though she had chosen to jog, and the choice was silently gone.
	 */
	EChangelingGait ChosenGait = EChangelingGait::Jog;

	/** Mirrors the sprint key. A hold, so it is simply true while the key is down. */
	bool bSprintHeld = false;

	/** Mirrors State.JumpHeld, so the tag is only touched on an actual transition. */
	bool bJumpHeld = false;

	// ── Locomotion ──────────────────────────────────────────────────────────
	//
	// ONE writer for MaxWalkSpeed, JumpZVelocity, GravityScale and AirControl, and it is
	// RefreshLocomotion below. This is not tidiness; it is the only arrangement that
	// works. Every one of those fields is the LIVE value the engine moves her by, so a
	// second place that writes one does not layer onto the first -- it erases it, on
	// whichever tick happens to run last.
	//
	// Two writers had already collided here before any cantrip existed: ToggleWalkRun set
	// MaxWalkSpeed directly while UWaterMovementComponent re-derived it from its own
	// captured base every tick, which quietly meant the walk/run toggle did nothing at
	// all after the first frame. Adding a speed cantrip to that would have made three.

public:
	/**
	 * Compose every influence on how this body moves and write the result. Runs on tick.
	 *
	 * base × water × attribute, in that order, for each of four values. Cheap enough to
	 * do unconditionally -- four multiplies and four float writes -- and doing it
	 * unconditionally is what makes it correct: there is no "who needs to remember to
	 * call this when X changes" list to fall out of date.
	 */
	void RefreshLocomotion();

	// ── Ending a working early ──────────────────────────────────────────────

	/**
	 * End one held cantrip of this class, if one is in force. Returns whether anything
	 * was ended.
	 *
	 * A held working is one that has LANDED and is running its duration -- not one still
	 * winding up, which a press should not cancel. Pressing a cantrip's own hotbar key a
	 * second time already does this; bind this to a key of its own if you want a general
	 * "drop what I am holding".
	 */

	/** End every held cantrip at once. Returns how many. A panic key, or a cutscene. */

private:
	/** Input wrapper for the above. Enhanced Input needs a void(); the count goes to the log. */
	void CancelHeldCantripsInput();

	virtual void BeginPlay() override;

public:

	/**
	 * Base run and walk speeds, for anything that wants to change how fast she is
	 * WITHOUT a gameplay effect -- a difficulty setting, a mount, a story beat.
	 *
	 * Set these, never MaxWalkSpeed. Writing MaxWalkSpeed from outside works for exactly
	 * one tick and is then overwritten by RefreshLocomotion, which is a bug that looks
	 * like a physics problem.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetBaseSpeeds(float NewWalkSpeed, float NewRunSpeed);

	/** The multiplier gameplay effects are currently asking for, or 1. For UI. */
	UFUNCTION(BlueprintPure, Category = "Movement")
	float GetMoveSpeedMultiplier() const;

protected:
	/**
	 * What the character was built with, captured once at BeginPlay.
	 *
	 * Captured rather than read from the constructor's literals because a Blueprint is
	 * free to override any of them on its own movement component, and BeginPlay is the
	 * first moment those overrides are certainly in place.
	 */
	float BuiltJumpVelocity = 500.0f;
	float BuiltGravityScale = 1.0f;
	float BuiltAirControl = 0.35f;
	float BuiltSwimSpeed = 300.0f;

	/**
	 * Found once at BeginPlay. Null on any character without one, which is fine --
	 * RefreshLocomotion then simply has no water term.
	 *
	 * "Cached" is not decoration. BP_ThirdPersonCharacter carries its water component
	 * under the name WaterMovement, and a C++ property of that name collides with it
	 * outright -- the Blueprint fails to compile with "tried to create a property
	 * WaterMovement in scope BP_ThirdPersonCharacter_C, but another object already exists
	 * there". The component keeps the good name; this pointer to it takes the longer one.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UWaterMovementComponent> CachedWaterMovement;

	/**
	 * Hold her in MOVE_Flying while weightless, and drop her out of it when she is not.
	 *
	 * Driven from the attribute every tick rather than set once by whatever cast the
	 * flight, because MOVE_Flying ignores gravity outright: an ability that ends without
	 * clearing the mode leaves her hovering forever no matter what happens to
	 * GravityScale, and there is no way back down from there.
	 */
	void UpdateFlightMode(class UCharacterMovementComponent* Movement, bool bWeightless);

	/** Windup elapsed: leave the ground. */
	void LaunchIntoFlight();

	/** Recovery elapsed: she has her feet back. */
	void FinishLandingRecovery();

	/** Moves to a phase, publishes it, and fires the matching event. One writer. */
	void SetFlightPhase(EChangelingFlightPhase NewPhase);

public:
	/** Where she is in a flight, for the AnimGraph and for anything that needs to wait. */
	UFUNCTION(BlueprintPure, Category = "Movement|Flight")
	EChangelingFlightPhase GetFlightPhase() const { return FlightPhase; }

	/** Speed she hit the ground at on the last flight landing. For the animation. */
	UFUNCTION(BlueprintPure, Category = "Movement|Flight")
	float GetLastLandingSpeed() const { return LastLandingSpeed; }

	/** True while crouched and gathering, or while picking herself up. Not hers to steer. */
	UFUNCTION(BlueprintPure, Category = "Movement|Flight")
	bool IsFlightLocked() const
	{
		return FlightPhase == EChangelingFlightPhase::Windup
			|| FlightPhase == EChangelingFlightPhase::Recovery;
	}

protected:
	/**
	 * She has crouched; the launch is one beat away. Dust at her feet belongs here.
	 *
	 * Fires at the START of the windup, not the end -- an effect triggered on the launch
	 * itself is already too late to read as anticipation, which is the entire point of
	 * making her crouch first.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Movement|Flight")
	void OnFlightWindup(FVector GroundPoint);

	/** Feet have left the ground. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Movement|Flight")
	void OnFlightLaunched(FVector GroundPoint);

	/**
	 * Down. ImpactSpeed is how fast she was falling, so the effect can match the arrival.
	 *
	 * bHeavy is that same number already judged against LandingHeavySpeed, so a Blueprint
	 * picking between two montages does not have to keep its own copy of the threshold and
	 * cannot drift out of step with the one the animation flag uses.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Movement|Flight")
	void OnFlightLanded(FVector ImpactPoint, float ImpactSpeed, bool bHeavy);

	/**
	 * How long she crouches before leaving the ground.
	 *
	 * Short. This is anticipation, not a cast time -- the casting already happened, and a
	 * long pause here reads as the input having been missed rather than as a windup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Flight",
		meta = (ClampMin = "0.0"))
	float FlightWindupSeconds = 0.35f;

	/**
	 * Upward kick at the moment of launch, on top of whatever flight does afterwards.
	 *
	 * Without it she rises at flight speed from a standing start, which is smooth and
	 * completely undramatic. The crouch promises a shove; this is the shove.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Flight",
		meta = (ClampMin = "0.0"))
	float LaunchImpulse = 900.0f;

	/** How long she is planted after a landing, playing the impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Flight",
		meta = (ClampMin = "0.0"))
	float LandingRecoverySeconds = 0.45f;

	/** Downward speed at or above which the landing counts as heavy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Flight",
		meta = (ClampMin = "0.0"))
	float LandingHeavySpeed = 900.0f;

private:
	EChangelingFlightPhase FlightPhase = EChangelingFlightPhase::Grounded;

	FTimerHandle FlightWindupTimer;
	FTimerHandle LandingRecoveryTimer;

	/** Speed she was falling at on the frame she touched down, for the landing event and
	 *  for the animation to choose how hard the impact was. */
	float LastLandingSpeed = 0.0f;

protected:

	/**
	 * Leans the MESH forward as she picks up speed, for streamlined flight.
	 *
	 * The mesh and not the actor: the capsule is what the world collides against and what
	 * decides where she fits, so tipping it would make a flying changeling unable to pass
	 * through a doorway she could walk through.
	 */
	void UpdateFlightPitch(float DeltaSeconds);

	/**
	 * Reframes the camera for a body that is no longer standing up.
	 *
	 * Called from UpdateFlightPitch and driven by the same blend, so the camera and the
	 * lean can never be out of step -- which they would be within a frame or two if this
	 * eased toward the target on its own.
	 */
	void UpdateFlightCamera();

	/**
	 * How far the camera's pivot rises at full lean.
	 *
	 * Up, because a body pitched to horizontal hangs BELOW a pivot that is still at the
	 * standing capsule's centre -- she sinks toward the bottom of frame exactly as she
	 * starts moving fastest. Slightly back as well, so the outstretched length of her
	 * fits without the arm having to grow as much.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Camera")
	FVector FlightCameraOffset = FVector(-30.0f, 0.0f, 55.0f);

	/**
	 * Extra spring-arm length at full lean.
	 *
	 * A horizontal body is far longer on screen than an upright one, and at the standing
	 * distance the head and feet reach the edges of frame. Ignored in first person.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight|Camera",
		meta = (ClampMin = "0.0"))
	float FlightCameraDistance = 120.0f;

	/** How far forward she leans at full flight speed, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxFlightPitch = 70.0f;

	/**
	 * Degrees the mesh is leaned over RIGHT NOW, after the speed blend. 0 when upright.
	 *
	 * Published because the animation layer cannot work it out for itself: the aim chain
	 * measures the camera against the ACTOR, and the actor never pitches -- only the mesh
	 * does. Without this number the head has no idea the body is lying down, and stays
	 * pointed wherever the torso points, which in level flight is at her own chest.
	 */
public:
	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetFlightLeanDegrees() const { return FlightLeanDegrees * FlightPitchBlend; }

protected:

	/**
	 * Horizontal speed at which the lean reaches its maximum.
	 *
	 * Matched to BS_8WayFly's Speed axis and to the animation's LegTrailFullSpeed, so the
	 * body finishes levelling out at the same moment the blend space reaches its fastest
	 * sample and the legs finish trailing. Three systems describing one movement; tuning
	 * them to different speeds is what makes a flight pose look like an argument.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "1.0"))
	float FlightPitchFullSpeed = 400.0f;

	/** How fast the lean eases in and out. Slower than the legs: a whole body has more
	 *  to move than a limb, and matching them makes the legs look weightless. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.1"))
	float FlightPitchInterpSpeed = 2.5f;

private:
	/** Eased 0-1 lean amount, and the mesh's authored rest rotation to compose it onto. */
	float FlightPitchBlend = 0.0f;
	FRotator BuiltMeshRelativeRotation = FRotator::ZeroRotator;

	/**
	 * Degrees of lean the CURRENT direction of travel calls for, before the speed blend.
	 *
	 * Ninety while flying level, less while climbing, capped by MaxFlightPitch. Recomputed
	 * every frame rather than derived from MaxFlightPitch alone, because that constant now
	 * only says how far she is ALLOWED to tip -- the direction decides how far she does.
	 */
	float FlightLeanDegrees = 0.0f;

public:

	/**
	 * The gait actually in force this frame: the chosen one, or Sprint over the top of it.
	 *
	 * Sprint is refused while winded, and the refusal drops her to whatever she chose rather
	 * than to a walk. Only sprinting costs anything, so running out of wind has no business
	 * taking her jog away too.
	 */
	UFUNCTION(BlueprintPure, Category = "Movement")
	EChangelingGait GetGait() const;

	/** What Caps Lock last selected -- Walk or Jog, never Sprint. */
	UFUNCTION(BlueprintPure, Category = "Movement")
	EChangelingGait GetChosenGait() const { return ChosenGait; }

	/** Ground speed for a gait, before water and gameplay effects act on it. */
	UFUNCTION(BlueprintPure, Category = "Movement")
	float GetSpeedForGait(EChangelingGait Gait) const;

	/**
	 * Decides whether she is sprinting and publishes State.Sprinting.
	 *
	 * The TAG is the whole interface: the drain effect requires it, the recovery effect is
	 * blocked by it, and neither knows this function exists. Nothing here subtracts a
	 * number from an attribute -- doing it in a tick would bypass prediction, replication
	 * and every clamp in the attribute set.
	 */
	void UpdateSprint(class UCharacterMovementComponent* Movement);

	/** True while State.Sprinting is applied, so it is added and removed exactly once. */
	bool bSprintTagHeld = false;

	/**
	 * She has run herself out, and cannot sprint until she gets some back.
	 *
	 * Blocks SPRINT only. It used to force a walk, which made sense when jogging was what
	 * cost stamina; now that only sprinting does, taking her jog away as well would punish
	 * her twice for the same empty bar.
	 *
	 * Latched rather than recomputed from an empty pool, because recovery starts the same
	 * instant sprinting stops: without the latch she would flick back to a sprint one frame
	 * later, drain it again, and stutter between the two forever. Cleared at
	 * SprintRecoverFraction, which is what makes it hysteresis rather than a threshold.
	 */
	bool bWinded = false;

public:
	/** Ground speed below which running does not count as sprinting. Standing still in a
	 *  run stance is not effort. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement",
		meta = (ClampMin = "0.0"))
	float SprintMinSpeed = 150.0f;

	/** Fraction of the wind she must recover before she may run again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SprintRecoverFraction = 0.2f;

	/** True while she is spending wind. Drives the animation, and the HUD if wanted. */
	UFUNCTION(BlueprintPure, Category = "Movement")
	bool IsSprinting() const { return bSprintTagHeld; }

	/** True while she is too winded to run. */
	UFUNCTION(BlueprintPure, Category = "Movement")
	bool IsWinded() const { return bWinded; }

protected:

	/** Ends every LANDED cantrip in one spec. Returns whether anything was ended. */

public:

	/** Constructor */
	ACHANGELINGCharacter();

public:
	/**
	 * Aiming: the body turns to follow where the camera looks.
	 *
	 * A toggle rather than a hold, because aiming is a stance you are in, not a button
	 * you fight. Replicated so other clients see the caster turn to face what they are
	 * about to do -- being aimed at is information the target should have.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aiming")
	void SetAiming(bool bNewAiming);

	UFUNCTION(BlueprintCallable, Category = "Aiming")
	void ToggleAiming() { SetAiming(!bAiming); }

	UFUNCTION(BlueprintPure, Category = "Aiming")
	bool IsAiming() const { return bAiming; }

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aiming")
	bool bAiming = false;

	/**
	 * How far the camera may swing off the body's facing while aiming, in degrees.
	 *
	 * Without this the camera orbits freely and the body simply stops turning at its
	 * limit, so the player keeps looking while the caster keeps pointing -- and the two
	 * silently disagree about where the ability is going. Fencing the camera in makes
	 * the limit something you can feel rather than something you discover.
	 *
	 * Set to match the spine's own reach. Zero disables the fence.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float CameraYawLimitWhileAiming = 75.0f;

	/**
	 * Hold the body's facing while aiming, so movement strafes instead of turning.
	 *
	 * Without this the fence cannot hold. It is measured from the body, and a body that
	 * turns to follow movement carries the fence around with it -- so the player simply
	 * walks in a circle and ends up looking behind themselves with the arc apparently
	 * still intact. Freezing the facing is what makes the limit mean anything.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	bool bFreezeFacingWhileAiming = true;

	/**
	 * Turn the body to the camera's heading as the stance is taken, so aiming always
	 * begins centred.
	 *
	 * The twist itself already eases from zero -- SpineAim decays while the stance is
	 * down and interpolates back up. What it eases TOWARDS is the problem: the camera has
	 * usually wandered off the body's heading by the time the button is pressed, so the
	 * first thing the stance does is wrench the spine out to meet an offset that
	 * accumulated while nobody was aiming.
	 *
	 * Bringing the body to the camera instead makes that offset zero at the moment it
	 * starts to matter. She turns to face what you are looking at, and every degree of
	 * twist after that is one you asked for.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	bool bRecentreOnAimStart = true;

	/**
	 * What the body does about the aim. See EAimFacingMode.
	 *
	 * Twist Only: the feet stay planted and the spine carries the offset, which is what
	 * a person bracing a sustained effect actually does -- they turn from the waist
	 * rather than shuffling their stance round. The flame still follows the aim because
	 * its direction comes from the anim instance's aim rotation, which includes that
	 * twist; it is the legs, not the effect, that stay put.
	 *
	 * This is the mode the yaw fence exists for: with the body fixed, the camera has to
	 * be held inside the arc the spine can actually reach.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming")
	EAimFacingMode AimFacingMode = EAimFacingMode::TwistOnly;

	/**
	 * How far the aim may stray from the body's facing before the body follows, in
	 * degrees. Lazy mode only.
	 *
	 * Keep it inside what the spine can actually reach -- past roughly 60 the twist
	 * stops looking like a person looking sideways and starts looking dislocated.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float AimFacingDeadzone = 45.0f;

	/**
	 * Degrees per second the body turns while catching up. Lazy mode only.
	 *
	 * Fast enough not to feel like wading, slow enough that the turn is visible --
	 * instant catch-up is just Locked mode with extra steps.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aiming",
		meta = (ClampMin = "1.0"))
	float AimFacingTurnSpeed = 360.0f;

private:
	/** Keeps the camera inside the arc the body can actually follow. */
	void ClampAimCamera();

	/** Swings the body round once the aim has left the deadzone. Lazy mode only. */
	void UpdateAimFacing(float DeltaSeconds);

	/** What bOrientRotationToMovement was before aiming took it away. */
	bool bFacingWasOrientedToMovement = true;

	/** What bUseControllerRotationYaw was before the aim lock took it. */
	bool bYawWasControllerDriven = false;

public:
	/**
	 * True when something solid is close enough to the raised hand that the arm needs
	 * pulling back. The AnimBP reads this to gate its Two Bone IK.
	 *
	 * Computed locally on every machine rather than replicated: it is derived purely
	 * from world geometry, so each client reaches the same answer from what it can
	 * already see, and a replicated bool would only add latency to an animation
	 * correction that needs to be immediate.
	 */

	// ── Rooting during a cast ───────────────────────────────────────────────
	//
	// The lock lives on the character rather than on the ability that asks for it,
	// because it has to OUTLIVE that ability. The body is still recovering when the
	// cantrip ends, and a character that regains its feet mid-recovery slides across
	// the floor; one whose unlock died with the ability never regains them at all.

	/** Plant the character. Movement mode goes to none and back to walking. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void SetCantripMovementLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Cantrip")
	bool IsCantripMovementLocked() const { return bCantripMovementLocked; }

	/**
	 * Keep the lock until the recovery animation reports in, or TimeoutSeconds elapse.
	 *
	 * The timeout is not a fallback for tidiness, it is the whole safety net: a
	 * missing transition event, an AnimBP that was never reparented, an animation
	 * blended out before its notify -- any of them would otherwise leave the character
	 * permanently unable to move, and the player has no way to recover from that.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void ReleaseCantripMovementLockOnRecovery(float TimeoutSeconds = 1.5f);

	/** Called by the AnimBP when the recovery animation finishes. Safe to call when
	 *  nothing is waiting on it. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void NotifyCantripRecovered();

protected:
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Ground contact. The only reliable place to catch the end of a fall.
	 *
	 * Polling MovementMode for the change would miss the impact SPEED, which is gone by the
	 * time the mode reads Walking -- CharacterMovement has already zeroed the velocity it
	 * landed with. Hit and the pre-landing velocity are both only available here.
	 */
	virtual void Landed(const FHitResult& Hit) override;

	/** Socket probed for obstructions. Matches the sustained cantrip's attach socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Hand IK")
	FName HandProbeSocket = TEXT("hand_rSocket");

	/**
	 * Radius that ENGAGES the correction, and the larger one that RELEASES it.
	 *
	 * Two radii on purpose. With a single threshold the IK pulls the hand clear, the
	 * next probe finds nothing, the correction releases, the hand goes back into the
	 * wall -- a visible flicker at a few hertz. Requiring a wider margin to release
	 * than to engage means it cannot chatter across the boundary.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Hand IK",
		meta = (ClampMin = "1.0"))
	float HandProbeEngageRadius = 22.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Hand IK",
		meta = (ClampMin = "1.0"))
	float HandProbeReleaseRadius = 34.0f;

	/** Seconds between probes. Animation-rate is plenty; this is not a physics query. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Hand IK",
		meta = (ClampMin = "0.0"))
	float HandProbeInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Hand IK")
	TEnumAsByte<ECollisionChannel> HandProbeChannel = ECC_Visibility;

private:
	void UpdateHandProbe(float DeltaSeconds);

	float HandProbeAccumulator = 0.0f;

	bool bCantripMovementLocked = false;
	bool bAwaitingCantripRecovery = false;
	FTimerHandle CantripRecoveryTimer;

	/** Grants DefaultAbilities and applies DefaultAttributesEffect. Server only,
	 *  and guarded so a re-possess cannot grant everything twice. */

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called to toggle Running */
	/** Caps Lock. Flips between Walk and Jog; never selects Sprint. */
	void ToggleWalkRun();

	/** Left Shift down and up. */
	void SprintPressed();
	void SprintReleased();

protected:

	void ToggleCameraView();
	void ZoomCamera(float AxisValue);

	/** Enhanced Input handler for camera zoom; forwards the Axis1D value to ZoomCamera */
	void Zoom(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/**
	 * Publish whether the jump control is DOWN, as State.JumpHeld on this character's ASC.
	 *
	 * Not the same question as ACharacter::bPressedJump, which ClearJumpInput drops after
	 * a frame or two and therefore answers "did she just jump" rather than "is she still
	 * asking". Hopscotch needs the second one: its effect is applied for the cantrip's
	 * whole duration but only IN FORCE while this tag is set, so releasing the key gives
	 * her full gravity back on that frame.
	 */
	void SetJumpHeld(bool bHeld);

public:
	/** Whether the jump control is currently down. */
	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsJumpHeld() const { return bJumpHeld; }

protected:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

