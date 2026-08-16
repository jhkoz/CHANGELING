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
#include "GAS/ChangelingAbilityTraitSet.h"
#include "GAS/ChangelingAttributeSet.h"
#include "GAS/ChangelingMagicSet.h"
#include "CHANGELING.h"

ACHANGELINGCharacter::ACHANGELINGCharacter()
{
	// The raised-hand probe runs here; nothing else on this class needs a tick.
	PrimaryActorTick.bCanEverTick = true;

	// The ability system and its attribute sets. Created here rather than in a
	// Blueprint so every character deriving from this class has a working GAS setup
	// without per-Blueprint wiring -- and so C++ can rely on them existing.
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);

	// Mixed: the owning client predicts its own abilities, while simulated proxies
	// only receive gameplay cues. Right for a player character.
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Attribute sets register with the ASC simply by being constructed as subobjects
	// of the same owner; no explicit AddAttributeSetSubobject call is needed.
	CoreAttributes = CreateDefaultSubobject<UChangelingAttributeSet>(TEXT("CoreAttributes"));
	AbilityTraits  = CreateDefaultSubobject<UChangelingAbilityTraitSet>(TEXT("AbilityTraits"));
	MagicTraits    = CreateDefaultSubobject<UChangelingMagicSet>(TEXT("MagicTraits"));

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
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
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
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::Look);

		// Toggle Walk/Run
		EnhancedInputComponent->BindAction(ToggleWalkRunAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::ToggleWalkRun);

		// Camera Zoom (mouse wheel)
		EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ACHANGELINGCharacter::Zoom);

		// Toggle 1st/3rd person view (middle mouse button)
		EnhancedInputComponent->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ACHANGELINGCharacter::ToggleCameraView);
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
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return;

	if (bIsWalking)
	{
		//Switch to Running
		MoveComp->MaxWalkSpeed = MaxRunSpeed;
		bIsWalking = false;
	}
	else
	{
		//Switch to Walking
		MoveComp->MaxWalkSpeed = MaxWalkSpeed;
		bIsWalking = true;
	}
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

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
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
	// signal the character to jump
	Jump();
}

void ACHANGELINGCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

// ── Gameplay Ability System ──────────────────────────────────────────────────

UAbilitySystemComponent* ACHANGELINGCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ACHANGELINGCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server side. InitAbilityActorInfo must run before anything is granted, or the
	// abilities have no avatar to act through.
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
	InitialiseAbilitySystem();
}

void ACHANGELINGCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client side. Re-initialising here is what gives the local machine a valid
	// actor info for prediction; without it, client-side ability activation is
	// silently ignored.
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}

void ACHANGELINGCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Covers the standalone case, where a character placed in the level is possessed
	// before BeginPlay and PossessedBy may already have run -- InitialiseAbilitySystem
	// guards against granting twice.
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
	InitialiseAbilitySystem();
}

void ACHANGELINGCharacter::InitialiseAbilitySystem()
{
	if (!AbilitySystem || !HasAuthority())
	{
		return;
	}

	// Granting is idempotent by flag rather than by checking the spec list: a
	// re-possess (respawn, controller swap) would otherwise hand out a second copy of
	// every cantrip, and duplicate specs fail in ways that look like input bugs.
	if (bAbilitiesGranted)
	{
		return;
	}
	bAbilitiesGranted = true;

	if (DefaultAttributesEffect)
	{
		FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec =
			AbilitySystem->MakeOutgoingSpec(DefaultAttributesEffect, 1.0f, Context);

		if (Spec.IsValid())
		{
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	for (int32 Index = 0; Index < DefaultAbilities.Num(); ++Index)
	{
		if (!DefaultAbilities[Index])
		{
			continue;
		}

		// InputID = array index, so binding an input to a cantrip is a matter of
		// where it sits in this list rather than a code change per ability.
		AbilitySystem->GiveAbility(
			FGameplayAbilitySpec(DefaultAbilities[Index], 1, Index, this));
	}
}

void ACHANGELINGCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Simulated proxies need it too: the raised arm is the whole tell that
	// someone is holding a working open.
	DOREPLIFETIME(ACHANGELINGCharacter, CantripPose);

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

	// Entering the stance with the camera already outside the arc would otherwise sit
	// there uncorrected until the player next moved the mouse, and the flame would
	// point somewhere the body never agreed to.
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
	UpdateHandProbe(DeltaSeconds);
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
