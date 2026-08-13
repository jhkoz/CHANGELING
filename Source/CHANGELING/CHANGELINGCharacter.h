// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GAS/CantripTypes.h"
#include "Logging/LogMacros.h"
#include "CHANGELINGCharacter.generated.h"

class UAbilitySystemComponent;
class UChangelingAbilityTraitSet;
class UChangelingAttributeSet;
class UChangelingMagicSet;
class UGameplayAbility;
class UGameplayEffect;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

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
class ACHANGELINGCharacter : public ACharacter, public IAbilitySystemInterface
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

	/** Toggle Walk/Run Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ToggleWalkRunAction;

	/** Camera Zoom Input Action (mouse wheel, Axis1D) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ZoomAction;

	/** Toggle 1st/3rd person view Input Action (middle mouse, Boolean) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ToggleViewAction;


	/** Speed Variables */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	float MaxRunSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MaxWalkSpeed = 200.0f;

	/** Camera control variables */
	float TargetArmLength = 400.0f;
	bool bIsFirstPerson = false;

	/** State Variables */
	bool bIsWalking = false;
public:

	/** Constructor */
	ACHANGELINGCharacter();

	// ── Gameplay Ability System ─────────────────────────────────────────────

	/** IAbilitySystemInterface. Lets any GAS code find this character's ASC. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	/** Attributes, pools and Health. */
	UPROPERTY()
	TObjectPtr<UChangelingAttributeSet> CoreAttributes;

	/** Talents, Skills and Knowledges. */
	UPROPERTY()
	TObjectPtr<UChangelingAbilityTraitSet> AbilityTraits;

	/** Art and Realm ratings. */
	UPROPERTY()
	TObjectPtr<UChangelingMagicSet> MagicTraits;

	/**
	 * Abilities granted on possession — cantrips go here.
	 *
	 * The array index becomes the ability's InputID, so the first entry binds to
	 * input 0, the second to 1, and so on. That keeps input binding a matter of
	 * ordering this list rather than editing code for every new cantrip.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Applied once on possession to set starting Attributes, Arts and Realms. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffect;

public:
	/**
	 * Body attitude forced by whatever sustained cantrip is running.
	 *
	 * The AnimBP reads this to drive its Modify Bone nodes. Replicated so other
	 * clients see the raised arm too -- a torch nobody else can see being held is
	 * worse than no torch.
	 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Cantrip")
	ECantripPose GetCantripPose() const { return CantripPose; }

	/** Set by the sustained cantrip ability; not intended for direct use. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void SetCantripPose(ECantripPose NewPose) { CantripPose = NewPose; }

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Cantrip")
	ECantripPose CantripPose = ECantripPose::None;

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
	UFUNCTION(BlueprintPure, Category = "Cantrip")
	bool IsCantripHandBlocked() const { return bCantripHandBlocked; }

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

	bool bCantripHandBlocked = false;
	float HandProbeAccumulator = 0.0f;

	bool bCantripMovementLocked = false;
	bool bAwaitingCantripRecovery = false;
	FTimerHandle CantripRecoveryTimer;

	/** Grants DefaultAbilities and applies DefaultAttributesEffect. Server only,
	 *  and guarded so a re-possess cannot grant everything twice. */
	void InitialiseAbilitySystem();

	/** Set once granting has happened. Both PossessedBy and BeginPlay call
	 *  InitialiseAbilitySystem, and either may run first depending on whether the
	 *  character was placed or spawned. */
	bool bAbilitiesGranted = false;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called to toggle Running */
	void ToggleWalkRun();

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

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

