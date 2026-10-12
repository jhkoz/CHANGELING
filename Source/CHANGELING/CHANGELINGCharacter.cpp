// Copyright Epic Games, Inc. All Rights Reserved.

#include "CHANGELINGCharacter.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/CantripInternal.h"
#include "GAS/GA_Cantrip.h"
#include "GAS/ChangelingAbilityTraitSet.h"
#include "GAS/ChangelingAttributeSet.h"
#include "GAS/ChangelingGameplayTags.h"
#include "GAS/ChangelingMagicSet.h"
#include "GAS/ChangelingMovementSet.h"
#include "GAS/GE_ChangelingRegen.h"
#include "Water/WaterMovementComponent.h"
#include "CHANGELING.h"

// Declared in the header by the template but never defined anywhere, so the category
// was unusable until something tried to log to it.
// LogTemplateCharacter is defined in ChangelingCharacterBase.cpp.

ACHANGELINGCharacter::ACHANGELINGCharacter()
{
	// The raised-hand probe runs here; nothing else on this class needs a tick.
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	// Only what she runs at before BeginPlay; RefreshLocomotion overwrites it from
	// BaseRunSpeed on the first frame and every frame after. Change BaseRunSpeed in the
	// header, not this -- editing this number does nothing you can see.
	GetCharacterMovement()->MaxWalkSpeed = BaseRunSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ACHANGELINGCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping. Through our own pair rather than straight at ACharacter::Jump: the
		// character has to publish whether the key is DOWN, which ACharacter does not
		// track usefully -- bPressedJump is cleared after a frame or two by
		// ClearJumpInput, so it answers "did she just jump", not "is she still asking".
		// Hopscotch needs the second question. Canceled as well as Completed, or losing
		// focus mid-leap leaves her gliding on a key nobody is holding.
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACHANGELINGCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACHANGELINGCharacter::DoJumpEnd);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ACHANGELINGCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::Look);

		// Toggle Walk/Run
		EnhancedInputComponent->BindAction(ToggleWalkRunAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::ToggleWalkRun);

		// Started AND Completed, which is what makes it a hold rather than a toggle.
		// Canceled is bound too: an action interrupted mid-press -- by a focus loss, or by a
		// modifier consuming it -- never fires Completed, and without this she would sprint
		// on a key nobody is holding until it was pressed and released again.
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started,
				this, &ACHANGELINGCharacter::SprintPressed);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed,
				this, &ACHANGELINGCharacter::SprintReleased);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled,
				this, &ACHANGELINGCharacter::SprintReleased);
		}

		// Camera Zoom (mouse wheel)
		EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::Zoom);

		// Toggle 1st/3rd person view (middle mouse button)
		EnhancedInputComponent->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ACHANGELINGCharacter::ToggleCameraView);

		// Drop everything she is holding. Null-checked rather than bound blind: the other
		// actions above are all assigned in the Blueprint, and BindAction on a null action
		// is a silent no-op that looks exactly like a key that does not work.
		if (CancelCantripsAction)
		{
			EnhancedInputComponent->BindAction(CancelCantripsAction, ETriggerEvent::Started,
				this, &ACHANGELINGCharacter::CancelHeldCantripsInput);
		}
		else
		{
			UE_LOG(LogTemplateCharacter, Log,
				TEXT("No CancelCantripsAction assigned; the drop-everything key is unbound. "
				     "Pressing a cantrip's own hotbar key still ends it."));
		}
	}
	else
	{
		UE_LOG(LogCHANGELING, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ACHANGELINGCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ACHANGELINGCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ACHANGELINGCharacter::ToggleWalkRun()
{
	// Flips the STATE and lets RefreshLocomotion work out the number. It used to write
	// MaxWalkSpeed here directly, which looked correct and did nothing: the water
	// component re-derived that same field from its own captured base on every tick, so
	// the toggle was overwritten within a frame and walking never actually happened.
	//
	// Nothing outside RefreshLocomotion writes MaxWalkSpeed any more, and nothing should.
	// Walk and Jog only. Sprint is never "selected" -- it is held, and it sits on top of
	// whichever of these two is current, so a player who sprints and lets go returns to the
	// gait they picked rather than to a default.
	ChosenGait = (ChosenGait == EChangelingGait::Walk)
		? EChangelingGait::Jog
		: EChangelingGait::Walk;

	// Immediately rather than on the next tick, so the change lands on the frame the key
	// was pressed instead of one frame later.
	RefreshLocomotion();
}

void ACHANGELINGCharacter::ToggleCameraView()
{
	bIsFirstPerson = !bIsFirstPerson;

	if (bIsFirstPerson)
	{
		// Switch to first person view
		CameraBoom->TargetArmLength = 0.0f;
	}
	else
	{
		// Switch to third person view
		CameraBoom->TargetArmLength = TargetArmLength;
	}
}

void ACHANGELINGCharacter::ZoomCamera(float AxisValue)
{
	if (bIsFirstPerson) return; // Disable zoom in first person

	// Negative step so wheel-up (positive axis) shrinks the arm = zoom IN.
	TargetArmLength = FMath::Clamp(TargetArmLength - AxisValue * 20.0f, 100.0f, 600.0f);
	CameraBoom->TargetArmLength = TargetArmLength;
}

void ACHANGELINGCharacter::Zoom(const FInputActionValue& Value)
{
	// Enhanced Input adapter — forward the Axis1D value into the existing zoom logic.
	ZoomCamera(Value.Get<float>());
}

void ACHANGELINGCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// FLYING takes the whole eyeline; walking takes the yaw alone.
		//
		// On the ground pitch has to be discarded or looking at your feet would drive you
		// into the floor. In the air it is the only way to change altitude at all -- with
		// pitch zeroed she holds whatever height she took off at forever, which is what
		// made Wind Runner read as skimming rather than flying.
		const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
		const bool bFlyingNow = MoveComp && MoveComp->MovementMode == MOVE_Flying;

		const FVector ForwardDirection = bFlyingNow
			? FRotationMatrix(Rotation).GetUnitAxis(EAxis::X)
			: FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// RIGHT stays flat even in flight. Taking it from the full rotation would tilt
		// the strafe axis with the camera, so sidestepping while looking down would carry
		// her into the ground -- a bank she never asked for.
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ACHANGELINGCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);

		// Immediately, on the same input that caused it. Correcting a frame later in
		// Tick meant the camera briefly passed the limit and was pulled back, which
		// reads as a stutter at exactly the moment the player is pushing hardest.
		ClampAimCamera();
	}
}

void ACHANGELINGCharacter::DoJumpStart()
{
	// Order matters, and all three steps are load-bearing.
	//
	// The tag first: GAS re-evaluates an ongoing tag requirement synchronously from the
	// tag change, so UGE_CantripBound switches ON here and the movement attributes are
	// already correct by the time the next line reads them.
	SetJumpHeld(true);

	// Then the write, so JumpZVelocity carries the bonus BEFORE anything jumps on it.
	// CharacterMovement applies the impulse in DoJump during its own tick, and whether
	// that tick runs before or after this actor's is not something to rely on -- doing it
	// here means the boost cannot miss the leap it was cast for.
	RefreshLocomotion();

	// And only now ask to jump.
	Jump();
}

void ACHANGELINGCharacter::DoJumpEnd()
{
	StopJumping();

	// Full gravity returns on this frame. That instant drop is the point of the ability
	// rather than a side effect of it: a bounding changeling who cannot choose to fall is
	// a changeling hanging in the air waiting to be shot at.
	SetJumpHeld(false);
	RefreshLocomotion();
}

void ACHANGELINGCharacter::SetJumpHeld(bool bHeld)
{
	if (bJumpHeld == bHeld)
	{
		return;
	}

	bJumpHeld = bHeld;

	if (!AbilitySystem)
	{
		return;
	}

	// A loose tag, not an effect. It describes what the player's hand is doing right now,
	// which is not something that should be predicted, replicated as a gameplay effect,
	// or outlive the frame it stops being true.
	if (bHeld)
	{
		AbilitySystem->AddLooseGameplayTag(ChangelingTags::State_JumpHeld);
	}
	else
	{
		// To zero rather than one count off. This tag has exactly one owner -- the jump
		// key -- so any count above one is already a bug, and decrementing would leave it
		// stuck on with nothing left that knows to clear it.
		AbilitySystem->SetLooseGameplayTagCount(ChangelingTags::State_JumpHeld, 0);
	}
}

// ── Gameplay Ability System ──────────────────────────────────────────────────

void ACHANGELINGCharacter::BeginPlay()
{
	Super::BeginPlay();

	// The ability system is brought up by AChangelingCharacterBase::BeginPlay above.

	// Everything below is about becoming the single writer of the movement component's
	// live values. See the long note in the header.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// Captured now rather than trusted from the constructor: a Blueprint may have
		// overridden any of these on its own movement component, and BeginPlay is the
		// first moment those overrides are certainly applied.
		BuiltJumpVelocity = Movement->JumpZVelocity;
		BuiltGravityScale = Movement->GravityScale;
		BuiltAirControl   = Movement->AirControl;
		BuiltSwimSpeed    = Movement->MaxSwimSpeed;
	}

	// The mesh's resting rotation, so the flight lean can be applied ON TOP of it rather
	// than replacing it. A character mesh is yawed -90 from its actor by default, and
	// overwriting that instead of composing with it turns her sideways the moment she
	// takes off.
	if (const USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		BuiltMeshRelativeRotation = CharacterMesh->GetRelativeRotation();
	}

	CachedWaterMovement = FindComponentByClass<UWaterMovementComponent>();

	if (CachedWaterMovement && CachedWaterMovement->bApplyToMovement)
	{
		// Deliberately overriding a Blueprint-authored checkbox, and saying so out loud.
		// The water component computes the right multiplier and always did; the problem
		// is only that it also WROTE it, which made it the second writer and meant its
		// tick silently undid the walk/run toggle. It keeps the measurement job and gives
		// up the writing job -- which is exactly the split its own bApplyToMovement flag
		// was added for ("compute the multiplier but leave MaxWalkSpeed alone").
		CachedWaterMovement->bApplyToMovement = false;

		UE_LOG(LogTemplateCharacter, Log,
			TEXT("Water movement now measures only; %s composes MaxWalkSpeed, JumpZVelocity, ")
			TEXT("GravityScale and AirControl in RefreshLocomotion."), *GetName());
	}

	// Once immediately, so the first frame is already correct rather than running at
	// whatever the constructor left behind until the first tick.
	RefreshLocomotion();
}

void ACHANGELINGCharacter::RefreshLocomotion()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// ── The three terms ─────────────────────────────────────────────────────

	// What the character is, before anything acts on her.
	const float BaseSpeed = GetSpeedForGait(GetGait());

	// What the world is doing to her. 1 on dry land, and 1 when there is no water
	// component at all -- which is why this reads as a plain multiply below rather than
	// as a special case.
	const float WaterScale = CachedWaterMovement ? CachedWaterMovement->GetSpeedMultiplier() : 1.0f;

	// What magic is doing to her.
	float SpeedBonus = 1.0f;
	float JumpBonus  = 1.0f;
	float FallBonus  = 1.0f;
	float AirBonus   = 1.0f;

	if (MovementTraits)
	{
		SpeedBonus = MovementTraits->GetMoveSpeedMultiplier();
		JumpBonus  = MovementTraits->GetJumpPowerMultiplier();
		FallBonus  = MovementTraits->GetFallSpeedMultiplier();
		AirBonus   = MovementTraits->GetAirControlMultiplier();
	}

	// ── The writes ──────────────────────────────────────────────────────────

	Movement->MaxWalkSpeed = BaseSpeed * WaterScale * SpeedBonus;

	// Swimming takes the speed bonus but NOT the water multiplier. Those tier scales
	// describe the resistance of wading with your feet on a bed; MaxSwimSpeed already
	// describes a swimmer, so applying them on top would slow swimming to a quarter of
	// the speed that was already the swimming speed. This picked up its own scale from
	// the water component when the component still did the writing.
	if (CachedWaterMovement)
	{
		Movement->MaxSwimSpeed = BuiltSwimSpeed * CachedWaterMovement->SwimSpeedScale * SpeedBonus;
	}
	else
	{
		Movement->MaxSwimSpeed = BuiltSwimSpeed * SpeedBonus;
	}

	// Water hits jumping harder than walking, and the square is where that came from:
	// pushing off THROUGH water is worse than moving through it. Kept here rather than
	// left behind in the water component, because moving the write without moving the
	// curve would have quietly made wading jumps feel wrong.
	//
	// Leaving the water surface is a different thing entirely and is not this -- that is
	// CharacterMovement's OutofWaterZ.
	const float JumpWaterScale = (CachedWaterMovement && CachedWaterMovement->bScaleJump)
		? FMath::Square(WaterScale)
		: 1.0f;

	Movement->JumpZVelocity = BuiltJumpVelocity * JumpWaterScale * JumpBonus;

	// Gravity and air control have no water term. Both describe what happens with
	// nothing underfoot, and a character in the air is out of the water by definition.
	Movement->GravityScale = BuiltGravityScale * FallBonus;

	// Gravity multiplied by nothing IS flight, so the movement mode follows from the
	// same number rather than from anyone remembering to set it. See UpdateFlightMode.
	UpdateFlightMode(Movement, FallBonus <= KINDA_SMALL_NUMBER);

	// After the speed write, so the check reads the speed she is actually allowed.
	UpdateSprint(Movement);

	// Clamped at 1: CharacterMovement treats air control as a fraction and anything above
	// 1 behaves as 1, so an uncapped value here would look like a knob that stops working
	// partway up its range rather than one that is simply full.
	Movement->AirControl = FMath::Min(1.0f, BuiltAirControl * AirBonus);
}

void ACHANGELINGCharacter::UpdateFlightPitch(float DeltaSeconds)
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!CharacterMesh || !Movement)
	{
		return;
	}

	// The MESH leans, never the actor. The capsule has to stay upright: it is what the
	// world collides against, what the movement component sweeps, and what decides where
	// she can fit. Tipping it over would make a flying changeling unable to pass through
	// a doorway she could walk through.
	//
	// Horizontal speed only, for the same reason the legs trail on it -- going straight up
	// there is nothing to streamline against, and leaning into a climb reads as falling.
	float Target = 0.0f;
	float LeanDegrees = 0.0f;

	if (Movement->MovementMode == MOVE_Flying)
	{
		// Speed still decides HOW MUCH she commits: a hover stays upright and only real
		// travel tips her over. Horizontal speed, because a vertical climb has nothing to
		// streamline against.
		const float Speed = Movement->Velocity.Size2D();
		Target = FMath::Clamp(Speed / FMath::Max(1.0f, FlightPitchFullSpeed), 0.0f, 1.0f);

		// But the DIRECTION now comes from where she is actually going, not from a fixed
		// lean. Once the eyeline steers the flight, a constant tip is wrong the moment she
		// climbs -- she would dive forward while travelling upward, which reads as being
		// dragged rather than flying.
		//
		// Upright is 0 and belly-down level flight is 90, so the lean is the angle between
		// her long axis and vertical: ninety minus how steeply she is climbing.
		const FVector Velocity = Movement->Velocity;
		const float Size = Velocity.Size();

		if (Size > KINDA_SMALL_NUMBER)
		{
			const float Elevation = FMath::RadiansToDegrees(
				FMath::Asin(FMath::Clamp(Velocity.Z / Size, -1.0f, 1.0f)));

			// Clamped at the low end too: past vertical she would be flying head-down,
			// which the animation set has nothing to say about.
			LeanDegrees = FMath::Clamp(90.0f - Elevation, 0.0f, MaxFlightPitch);
		}
	}

	// Held for the lean below, which no longer derives it from MaxFlightPitch alone.
	FlightLeanDegrees = LeanDegrees;

	FlightPitchBlend = FMath::FInterpTo(FlightPitchBlend, Target, DeltaSeconds,
		FlightPitchInterpSpeed);

	// Framed from the SAME blend as the lean, not a second interpolation of its own. Two
	// eases toward the same target with different speeds drift apart in the middle, and
	// the middle is the whole of a take-off -- the camera would arrive after the body, or
	// before it, on every launch.
	UpdateFlightCamera();

	// Snap the last sliver to zero and restore the authored rotation exactly. Interpolation
	// approaches without arriving, and a mesh left a hundredth of a degree off its rest
	// pose forever is the kind of thing that shows up as a seam in a cutscene a year later.
	if (FlightPitchBlend <= KINDA_SMALL_NUMBER)
	{
		FlightPitchBlend = 0.0f;
		CharacterMesh->SetRelativeRotation(BuiltMeshRelativeRotation);
		return;
	}

	// Built as a quaternion about the ACTOR'S RIGHT AXIS rather than by adding to the
	// rotator's Pitch field, and that distinction is the whole trick. The mesh is yawed
	// -90 from the actor, so a pitch written into its relative rotator is applied in a
	// frame that is ninety degrees off -- she rolls onto her side instead of leaning
	// forward. Rotating about the parent's right vector is forward-lean whatever the
	// mesh's own yaw happens to be.
	//
	// NOT negated, and the reason is a genuine trap. FQuat(Axis, Angle) follows the
	// right-hand rule, but Unreal's FRotator inverts pitch about Y -- so the familiar
	// "positive pitch is nose-up" is a fact about FRotator and NOT about a quaternion
	// built on the right vector. Reasoning from the rotator convention and negating
	// accordingly tips her the wrong way, and she flies reclined as though in a deckchair.
	//
	// About +Right by the right-hand rule IS nose-down, which is the lean wanted.
	const float Lean = FlightLeanDegrees * FlightPitchBlend;
	const FQuat Tilt(FVector::RightVector, FMath::DegreesToRadians(Lean));

	CharacterMesh->SetRelativeRotation(Tilt * BuiltMeshRelativeRotation.Quaternion());
}

void ACHANGELINGCharacter::UpdateFlightCamera()
{
	if (!CameraBoom)
	{
		return;
	}

	// TargetOffset, not SocketOffset. TargetOffset moves the point the arm ORBITS, in
	// world space -- so raising it lifts what the camera is looking at, which is exactly
	// the problem: a body pitched to horizontal hangs below a pivot that is still at the
	// standing capsule's centre. SocketOffset would move the camera instead and swing
	// with the arm, so looking up would push her out of frame rather than centring her.
	CameraBoom->TargetOffset = FlightCameraOffset * FlightPitchBlend;

	// First person keeps the arm at zero. Adding distance there would push the camera out
	// through the back of her head, which is not a view of anything.
	if (bIsFirstPerson)
	{
		CameraBoom->TargetArmLength = 0.0f;
		return;
	}

	// Composed from the base each frame rather than added to whatever is there, or the
	// bonus accumulates a little further out on every tick until she is a dot.
	CameraBoom->TargetArmLength = TargetArmLength
		+ (FlightCameraDistance * FlightPitchBlend);
}

void ACHANGELINGCharacter::UpdateFlightMode(UCharacterMovementComponent* Movement,
	bool bWeightless)
{
	if (!Movement)
	{
		return;
	}

	// The cantrip root owns MOVE_None while she is planted, and it is the stronger claim:
	// a working that plants her should not be undone by a flight cantrip still winding
	// down. Left out, the two would trade the movement mode back and forth every frame.
	if (bCantripMovementLocked)
	{
		return;
	}

	if (bWeightless)
	{
		switch (FlightPhase)
		{
		case EChangelingFlightPhase::Grounded:
		case EChangelingFlightPhase::Landing:
		case EChangelingFlightPhase::Recovery:
			// A new flight, from wherever the last one left her. Recovery is interruptible
			// on purpose: casting again while picking herself up should launch her, not
			// make her finish the animation first.
			SetFlightPhase(EChangelingFlightPhase::Windup);
			break;

		case EChangelingFlightPhase::Windup:
			// Waiting on the timer. Nothing to do, and specifically NOT setting MOVE_Flying
			// -- the whole point of the crouch is that she is still on the ground.
			break;

		case EChangelingFlightPhase::Airborne:
			// Re-asserted every tick rather than set once, because anything else in the
			// game may have changed the movement mode out from under the cantrip.
			if (Movement->MovementMode != MOVE_Flying)
			{
				Movement->SetMovementMode(MOVE_Flying);
			}
			break;
		}

		return;
	}

	// Weightlessness gone mid-crouch: the cantrip was cancelled, dispelled or ran out
	// before she ever left the ground. Drop the windup rather than launching her into a
	// flight that no longer exists.
	if (FlightPhase == EChangelingFlightPhase::Windup)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlightWindupTimer);
		}

		SetFlightPhase(EChangelingFlightPhase::Grounded);
		return;
	}

	// Coming down. MOVE_Flying ignores gravity ENTIRELY -- it never calls PhysFalling --
	// so restoring GravityScale does nothing on its own and she simply hovers where the
	// cantrip left her. Taking her out of the mode is the part that actually lands her,
	// and this runs every tick from the attribute, so it happens whether the cantrip
	// expired, was cancelled, was dispelled, or was destroyed outright without ever
	// getting the chance to tidy up after itself.
	//
	// MOVE_Falling rather than MOVE_Walking: she is almost certainly in the air, and
	// telling the movement component she is walking on nothing snaps her down to whatever
	// it can find beneath her. Falling is simply true, and CharacterMovement transitions
	// out of it by itself the moment she touches something.
	if (Movement->MovementMode == MOVE_Flying)
	{
		Movement->SetMovementMode(MOVE_Falling);

		// Falling, not landed. Landed() decides when she is actually down, and it may be a
		// long way below her -- flight ends in mid-air far more often than over a floor.
		SetFlightPhase(EChangelingFlightPhase::Landing);
	}
}

void ACHANGELINGCharacter::SetFlightPhase(EChangelingFlightPhase NewPhase)
{
	if (FlightPhase == NewPhase)
	{
		return;
	}

	FlightPhase = NewPhase;

	UWorld* World = GetWorld();

	switch (NewPhase)
	{
	case EChangelingFlightPhase::Windup:
	{
		// The ground under her feet, handed to the effect so a dust ring sits on the floor
		// rather than at her waist.
		const FVector Feet = GetActorLocation()
			- FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		OnFlightWindup(Feet);

		if (World && FlightWindupSeconds > 0.0f)
		{
			World->GetTimerManager().SetTimer(FlightWindupTimer, this,
				&ACHANGELINGCharacter::LaunchIntoFlight, FlightWindupSeconds, false);
		}
		else
		{
			// No windup configured. Launching immediately rather than waiting a frame, so
			// setting the time to zero genuinely means "as it was before".
			LaunchIntoFlight();
		}
		break;
	}

	case EChangelingFlightPhase::Recovery:
		if (World && LandingRecoverySeconds > 0.0f)
		{
			World->GetTimerManager().SetTimer(LandingRecoveryTimer, this,
				&ACHANGELINGCharacter::FinishLandingRecovery, LandingRecoverySeconds, false);
		}
		else
		{
			FinishLandingRecovery();
		}
		break;

	default:
		break;
	}
}

void ACHANGELINGCharacter::LaunchIntoFlight()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const FVector Feet = GetActorLocation()
		- FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	FlightPhase = EChangelingFlightPhase::Airborne;

	Movement->SetMovementMode(MOVE_Flying);

	// Added to her existing velocity, not assigned over it. A launch out of a run should
	// carry the run: overwriting would stop her dead horizontally at the exact moment she
	// is meant to be flung, which reads as being caught rather than as taking off.
	Movement->Velocity.Z += LaunchImpulse;

	OnFlightLaunched(Feet);
}

void ACHANGELINGCharacter::Landed(const FHitResult& Hit)
{
	// Captured BEFORE Super, which zeroes the velocity she arrived with.
	LastLandingSpeed = FMath::Abs(GetVelocity().Z);

	Super::Landed(Hit);

	// Only a landing from a FLIGHT gets the treatment. An ordinary jump landing on the same
	// montage would make every hop look like a hero shot, which is exactly how a dramatic
	// landing stops being dramatic.
	if (FlightPhase != EChangelingFlightPhase::Landing)
	{
		return;
	}

	const bool bHeavy = LastLandingSpeed >= LandingHeavySpeed;

	SetFlightPhase(EChangelingFlightPhase::Recovery);

	OnFlightLanded(Hit.ImpactPoint, LastLandingSpeed, bHeavy);
}

void ACHANGELINGCharacter::FinishLandingRecovery()
{
	// Guarded: a second flight begun during the recovery has already moved her on, and
	// stamping Grounded over it here would strand her mid-windup.
	if (FlightPhase == EChangelingFlightPhase::Recovery)
	{
		SetFlightPhase(EChangelingFlightPhase::Grounded);
	}
}

void ACHANGELINGCharacter::UpdateSprint(UCharacterMovementComponent* Movement)
{
	if (!Movement || !AbilitySystem || !CoreAttributes)
	{
		return;
	}

	const float Pool = CoreAttributes->GetStaminaPool();
	const float PoolMax = FMath::Max(KINDA_SMALL_NUMBER, CoreAttributes->GetStaminaPoolMax());

	// Latch on empty, release only once she has enough back to be worth having. Without
	// the gap she would run for one frame per point of recovery.
	if (Pool <= KINDA_SMALL_NUMBER)
	{
		bWinded = true;
	}
	else if (bWinded && (Pool / PoolMax) >= SprintRecoverFraction)
	{
		bWinded = false;
	}

	// Nothing is written to ChosenGait here. Running out of wind takes the SPRINT away --
	// GetGait refuses it while bWinded -- and leaves her walking or jogging as she chose.
	// The old code set the walk flag instead, so recovering left her walking even though
	// she had picked a jog, and the choice was gone with no way to tell it had been lost.

	// Sprinting, actually moving, and on the ground. Falling is not effort -- and draining
	// wind mid-flight would make Wind Runner cost stamina for no reason anyone could see.
	const bool bSprinting = (GetGait() == EChangelingGait::Sprint)
		&& !Movement->IsFalling()
		&& Movement->Velocity.SizeSquared2D() > FMath::Square(SprintMinSpeed);

	if (bSprinting == bSprintTagHeld)
	{
		return;
	}

	bSprintTagHeld = bSprinting;

	if (bSprinting)
	{
		AbilitySystem->AddLooseGameplayTag(ChangelingTags::State_Sprinting);
	}
	else
	{
		// SetLooseGameplayTagCount rather than Remove: a count left standing at one would
		// leave her permanently unable to recover, and nothing else adds this tag.
		AbilitySystem->SetLooseGameplayTagCount(ChangelingTags::State_Sprinting, 0);
	}
}

EChangelingGait ACHANGELINGCharacter::GetGait() const
{
	// Winded refuses the sprint and returns her to what she chose -- NOT to a walk. Only
	// sprinting spends stamina, so an empty bar has no claim on her jog.
	if (bSprintHeld && !bWinded)
	{
		return EChangelingGait::Sprint;
	}

	return ChosenGait;
}

float ACHANGELINGCharacter::GetSpeedForGait(EChangelingGait Gait) const
{
	switch (Gait)
	{
	case EChangelingGait::Walk:   return BaseWalkSpeed;
	case EChangelingGait::Sprint: return BaseRunSpeed * SprintSpeedMultiplier;
	case EChangelingGait::Jog:
	default:                      return BaseRunSpeed;
	}
}

void ACHANGELINGCharacter::SprintPressed()
{
	bSprintHeld = true;

	// Immediately, so the speed lands on the frame the key went down. Waiting for the next
	// tick is a frame of lag on the most tactile control in the game.
	RefreshLocomotion();
}

void ACHANGELINGCharacter::SprintReleased()
{
	bSprintHeld = false;
	RefreshLocomotion();
}

void ACHANGELINGCharacter::SetBaseSpeeds(float NewWalkSpeed, float NewRunSpeed)
{
	BaseWalkSpeed = FMath::Max(0.0f, NewWalkSpeed);
	BaseRunSpeed  = FMath::Max(0.0f, NewRunSpeed);

	// Applied now rather than waiting for the next tick, so a caller that sets a speed
	// and immediately reads MaxWalkSpeed back sees what it just asked for.
	RefreshLocomotion();
}

float ACHANGELINGCharacter::GetMoveSpeedMultiplier() const
{
	return MovementTraits ? MovementTraits->GetMoveSpeedMultiplier() : 1.0f;
}


void ACHANGELINGCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// CantripPose is replicated by AChangelingCharacterBase.

	// Being aimed at is information the target should have, so it travels.
	DOREPLIFETIME(ACHANGELINGCharacter, bAiming);
}

void ACHANGELINGCharacter::SetAiming(bool bNewAiming)
{
	if (bAiming == bNewAiming)
	{
		return;
	}

	bAiming = bNewAiming;

	// Only Locked hands the yaw straight to the controller. Lazy turns the body itself
	// in UpdateAimFacing, which controller yaw would override every frame.
	if (AimFacingMode == EAimFacingMode::Locked)
	{
		if (bAiming)
		{
			// Remembered rather than assumed, for the same reason as the movement flag
			// below: restoring must not quietly change a setting made elsewhere.
			bYawWasControllerDriven = bUseControllerRotationYaw;
			bUseControllerRotationYaw = true;
		}
		else
		{
			bUseControllerRotationYaw = bYawWasControllerDriven;
		}
	}

	// Orientation-to-movement has to go whatever the mode: it fights a frozen facing,
	// it fights controller yaw, and it fights a lazy turn just as hard.
	if (bFreezeFacingWhileAiming || AimFacingMode != EAimFacingMode::TwistOnly)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			if (bAiming)
			{
				// Remembered rather than assumed, so restoring cannot quietly change a
				// setting somebody deliberately turned off elsewhere.
				bFacingWasOrientedToMovement = Movement->bOrientRotationToMovement;
				Movement->bOrientRotationToMovement = false;
			}
			else
			{
				Movement->bOrientRotationToMovement = bFacingWasOrientedToMovement;
			}
		}
	}

	// Yaw only, and before the clamp below: pitch belongs to the camera and the spine,
	// and a body pitched to match the eyeline would be lying on the floor to look down.
	if (bAiming && bRecentreOnAimStart)
	{
		if (const AController* OwningController = GetController())
		{
			FRotator Facing = GetActorRotation();
			Facing.Yaw = OwningController->GetControlRotation().Yaw;
			SetActorRotation(Facing);
		}
	}

	// Entering the stance with the camera already outside the arc would otherwise sit
	// there uncorrected until the player next moved the mouse, and the flame would
	// point somewhere the body never agreed to. With the recentring above this now
	// finds an offset of zero and does nothing -- it stays for the case where the
	// recentring is switched off, and for leaving the stance.
	ClampAimCamera();
}

void ACHANGELINGCharacter::ClampAimCamera()
{
	AController* OwningController = GetController();

	// Nothing to fence unless the body is genuinely staying put. In the other modes the
	// clamp would push the control rotation back at a body already turning to meet it.
	if (!bAiming || AimFacingMode != EAimFacingMode::TwistOnly
		|| CameraYawLimitWhileAiming <= 0.0f || !OwningController)
	{
		return;
	}

	FRotator Control = OwningController->GetControlRotation();
	const float BodyYaw = GetActorRotation().Yaw;

	// Shortest signed angle. A plain subtraction wraps at 180 and would fence the
	// camera against the wrong side the moment the character faced south.
	const float Offset = FMath::FindDeltaAngleDegrees(BodyYaw, Control.Yaw);
	const float Fenced = FMath::Clamp(Offset,
		-CameraYawLimitWhileAiming, CameraYawLimitWhileAiming);

	if (FMath::IsNearlyEqual(Offset, Fenced))
	{
		return;
	}

	// Written back rather than blocked at the input, so the camera comes to rest
	// against the limit and stays usable. Refusing the input instead would leave the
	// stick fighting a wall that gives no sign it is there.
	Control.Yaw = BodyYaw + Fenced;
	OwningController->SetControlRotation(Control);
}

void ACHANGELINGCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateAimFacing(DeltaSeconds);
	UpdateFlightPitch(DeltaSeconds);
	UpdateHandProbe(DeltaSeconds);

	// Unconditionally, every frame. Four multiplies and four float writes is cheaper than
	// the alternative -- a list of everything that might have changed a term, which
	// someone eventually forgets to add to. The water multiplier eases continuously, so
	// there is no edge to hang a notification on in any case.
	RefreshLocomotion();
}

void ACHANGELINGCharacter::UpdateAimFacing(float DeltaSeconds)
{
	const AController* OwningController = GetController();
	if (!bAiming || AimFacingMode != EAimFacingMode::Lazy || !OwningController)
	{
		return;
	}

	const float BodyYaw = GetActorRotation().Yaw;
	const float AimYaw = OwningController->GetControlRotation().Yaw;

	// Shortest signed angle, for the same reason the fence uses it: a plain subtraction
	// wraps at 180 and would send the body the long way round when facing south.
	const float Offset = FMath::FindDeltaAngleDegrees(BodyYaw, AimYaw);

	if (FMath::Abs(Offset) <= AimFacingDeadzone)
	{
		return;
	}

	// Chase the EDGE of the deadzone, not the aim itself. Turning all the way to the
	// aim would leave the spine straight and the body drifting on every small camera
	// movement; stopping at the edge means the twist is still doing its share, and the
	// body settles as soon as the aim stops running away.
	const float Target = AimYaw - FMath::Sign(Offset) * AimFacingDeadzone;

	FRotator Facing = GetActorRotation();
	Facing.Yaw = FMath::FixedTurn(Facing.Yaw, Target, AimFacingTurnSpeed * DeltaSeconds);
	SetActorRotation(Facing);
}

void ACHANGELINGCharacter::SetCantripMovementLocked(bool bLocked)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || bCantripMovementLocked == bLocked)
	{
		return;
	}

	// Back to Walking rather than to whatever mode was in force before. Restoring the
	// remembered one would drop the character back into Falling or Swimming on a frame
	// where it is demonstrably standing still on solid ground, and the movement
	// component would have to fall out of that state all over again.
	Movement->SetMovementMode(bLocked ? MOVE_None : MOVE_Walking);
	bCantripMovementLocked = bLocked;

	if (bLocked)
	{
		// A fresh lock supersedes any recovery still pending from the last cast.
		bAwaitingCantripRecovery = false;
		GetWorldTimerManager().ClearTimer(CantripRecoveryTimer);
	}
}

void ACHANGELINGCharacter::ReleaseCantripMovementLockOnRecovery(float TimeoutSeconds)
{
	if (!bCantripMovementLocked)
	{
		return;
	}

	bAwaitingCantripRecovery = true;

	if (TimeoutSeconds <= 0.0f)
	{
		NotifyCantripRecovered();
		return;
	}

	GetWorldTimerManager().SetTimer(CantripRecoveryTimer, this,
		&ACHANGELINGCharacter::NotifyCantripRecovered, TimeoutSeconds, false);
}

void ACHANGELINGCharacter::NotifyCantripRecovered()
{
	// Reached both from the animation and from the timeout, and the animation may
	// report in for a cast whose lock was already released. Doing nothing unless
	// something is actually waiting keeps the late notify harmless.
	if (!bAwaitingCantripRecovery)
	{
		return;
	}

	bAwaitingCantripRecovery = false;
	GetWorldTimerManager().ClearTimer(CantripRecoveryTimer);

	SetCantripMovementLocked(false);
}

void ACHANGELINGCharacter::UpdateHandProbe(float DeltaSeconds)
{
	// Nothing raised means nothing to correct. Clearing the flag here matters: the arm
	// lowering must release the IK, or the correction outlives the pose that needed it.
	if (CantripPose == ECantripPose::None)
	{
		bCantripHandBlocked = false;
		HandProbeAccumulator = 0.0f;
		return;
	}

	HandProbeAccumulator += DeltaSeconds;
	if (HandProbeAccumulator < HandProbeInterval)
	{
		return;
	}
	HandProbeAccumulator = 0.0f;

	// Named ProbeMesh, not Mesh: ACharacter already has a member called Mesh, and
	// shadowing it is a warning, which this project compiles as an error.
	const USkeletalMeshComponent* ProbeMesh = GetMesh();
	const UWorld* World = GetWorld();
	if (!ProbeMesh || !World || !ProbeMesh->DoesSocketExist(HandProbeSocket))
	{
		bCantripHandBlocked = false;
		return;
	}

	// Wider radius to release than to engage. A single threshold flickers: the IK pulls
	// the hand clear, the next probe finds nothing, the correction drops, the hand goes
	// back into the wall. Deliberately NOT gated on the character moving -- that hides
	// the flicker rather than fixing it, and leaves the hand inside walls when standing.
	const float Radius = bCantripHandBlocked ? HandProbeReleaseRadius : HandProbeEngageRadius;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CantripHandProbe), /*bTraceComplex*/ false, this);

	// OverlapAny, not a sweep: the question is "is anything close to the hand", and it
	// stops at the first hit instead of gathering every overlap it finds.
	bCantripHandBlocked = World->OverlapAnyTestByChannel(
		ProbeMesh->GetSocketLocation(HandProbeSocket), FQuat::Identity,
		HandProbeChannel, FCollisionShape::MakeSphere(Radius), Params);
}

void ACHANGELINGCharacter::CancelHeldCantripsInput()
{
	const int32 Ended = CancelHeldCantrips();

	UE_LOG(LogTemplateCharacter, Log, TEXT("Cancel key: ended %d held cantrip(s)."), Ended);
}
