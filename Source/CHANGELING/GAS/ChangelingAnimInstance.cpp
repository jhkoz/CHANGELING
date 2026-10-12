// ChangelingAnimInstance.cpp

#include "ChangelingAnimInstance.h"

#include "ChangelingCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Audio/FootstepAudioComponent.h"
#include "Audio/FootstepTypes.h"
#include "CHANGELINGCharacter.h"
#include "ChangelingGameplayTags.h"
#include "GA_Cantrip.h"
#include "GameFramework/CharacterMovementComponent.h"

void UChangelingAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Any CHANGELING first -- player or NPC. The movement component is what drives falling,
	// flying, speed and the ground/air decision, and an NPC has one exactly as the player
	// does. Since the base carries the ability system and the cantrip pose too, an NPC now
	// gets those for free rather than by a second code path.
	OwnerCharacter = Cast<AChangelingCharacterBase>(TryGetPawnOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	Movement = OwnerCharacter->GetCharacterMovement();

	// The ability system lives on the base, so an NPC has one too.
	AbilitySystem = OwnerCharacter->GetAbilitySystemComponent();

	// Then the PLAYER-specific half, which is allowed to be absent. Gait, the flight arc
	// and aiming are all coupled to a camera or a controller; everything reading this
	// already checks for null.
	ChangelingCharacter = Cast<ACHANGELINGCharacter>(OwnerCharacter);

	RefreshFootIKSupport();
}

void UChangelingAnimInstance::AnimNotify_l_foot_plant_Implementation()
{
	PlayNativeFootstep(EFootSide::Left);
}

void UChangelingAnimInstance::AnimNotify_r_foot_plant_Implementation()
{
	PlayNativeFootstep(EFootSide::Right);
}

void UChangelingAnimInstance::PlayNativeFootstep(EFootSide Foot)
{
	if (!OwnerCharacter)
	{
		return;
	}

	// The player is handled already. BP_ThirdPersonCharacter implements BI_Footstep and
	// ABP_Unarmed calls it from these same two notifies, so letting the native path run
	// for her would play every step twice -- two cues, two traces, and a flam on every
	// footfall that is more obvious than silence would have been.
	//
	// Tested against ACHANGELINGCharacter rather than by asking whether the actor
	// implements BI_Footstep, because that would mean loading a Blueprint interface by
	// content path from C++ and the answer is the same one: the player is the only thing
	// with the Blueprint wiring.
	if (ChangelingCharacter)
	{
		return;
	}

	if (UFootstepAudioComponent* Footsteps =
			OwnerCharacter->FindComponentByClass<UFootstepAudioComponent>())
	{
		Footsteps->PlayFootstep(Foot);
	}
}

void UChangelingAnimInstance::RefreshFootIKSupport()
{
	// Default to allowing it: a skeleton we cannot inspect is more likely the Mannequin
	// mid-initialisation than a MetaHuman, and silently disabling IK on the player would be
	// far harder to notice than the frozen legs this exists to prevent.
	bSkeletonSupportsFootIK = true;

	const USkeletalMeshComponent* Component = GetSkelMeshComponent();
	if (!Component)
	{
		return;
	}

	const USkeletalMesh* Mesh = Component->GetSkeletalMeshAsset();
	if (!Mesh)
	{
		return;
	}

	// Only the root is tested. CR_Mannequin_FootIK wants ik_foot_root, ik_foot_l and
	// ik_foot_r, and no skeleton in practice carries a partial set -- either the ik_ chain
	// was authored or it was not.
	static const FName IKFootRoot(TEXT("ik_foot_root"));

	const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
	bSkeletonSupportsFootIK = RefSkeleton.FindBoneIndex(IKFootRoot) != INDEX_NONE;

	// Warned ONCE per skeleton, not once per initialisation.
	//
	// NativeInitializeAnimation runs far more often than its name suggests -- editor
	// preview instances, thumbnail rendering and the reinitialise-on-null-owner retry in
	// NativeUpdateAnimation all reach it -- so an unconditional warning here produced
	// several hundred lines a second across four NPCs and very nearly buried the one
	// "assembly succeeded" line that mattered. A warning that hides other warnings is
	// worse than no warning.
	//
	// Keyed on the skeleton rather than the character: the fact is about the skeleton, and
	// every MetaHuman in the garden shares one. Saying it once is saying it enough.
	if (!bSkeletonSupportsFootIK)
	{
		static TSet<TWeakObjectPtr<const USkeleton>> AlreadyWarned;

		const USkeleton* Skeleton = Mesh->GetSkeleton();
		if (!AlreadyWarned.Contains(Skeleton))
		{
			AlreadyWarned.Add(Skeleton);

			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("%s: skeleton '%s' has no '%s' -- foot IK disabled for every body ")
				TEXT("using it. Expected on MetaHuman skeletons, which ship no ik_ chain."),
				*GetNameSafe(OwnerCharacter),
				*GetNameSafe(Skeleton),
				*IKFootRoot.ToString());
		}
	}
}

void UChangelingAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The pawn arrives after the AnimBP on a spawned character, so initialisation
	// cannot be the only place this is tried.
	//
	// Retried on OwnerCharacter, not on ChangelingCharacter. Null Changeling is the normal
	// state for an NPC, and retrying on it would re-run initialisation every frame forever.
	if (!OwnerCharacter)
	{
		NativeInitializeAnimation();
		if (!OwnerCharacter)
		{
			return;
		}
	}

	if (const UCharacterMovementComponent* MoveComp = Movement.Get())
	{
		bIsFalling = MoveComp->IsFalling();

		// Read from the movement MODE, not from any cantrip. Wind Runner is what puts her
		// here today, but the graph should not care what did -- and the character already
		// derives this mode from FallSpeedMultiplier every tick, so asking the movement
		// component is asking the one thing that is authoritative about it.
		bFlying = MoveComp->MovementMode == MOVE_Flying;

		// Vertical rate, signed: positive climbing. The 8-way fly blend space has no
		// vertical axis at all -- its two axes are Rotation and Speed -- so without this
		// a straight climb and a straight dive both read as a hover.
		FlightClimbRate = MoveComp->Velocity.Z;
	}

	// The gait she CHOSE, straight from the character. Not inferred from her velocity,
	// because the two genuinely differ: a speed cantrip moves her at sprinting pace while
	// she is only jogging, and a state entered on velocity would show a free sprint.
	if (const ACHANGELINGCharacter* Owner = ChangelingCharacter.Get())
	{
		Gait = Owner->GetGait();

		// The phase, plus the two booleans the state machine actually wants. Both derived
		// here rather than compared against the enum in the graph -- an enum switch in an
		// AnimGraph transition is three nodes where a bool pin is one, and every state that
		// needs it would repeat them.
		FlightPhase = Owner->GetFlightPhase();
		bFlightWindup     = FlightPhase == EChangelingFlightPhase::Windup;
		bFlightRecovering = FlightPhase == EChangelingFlightPhase::Recovery;
		LandingImpactSpeed = Owner->GetLastLandingSpeed();
	}

	RefreshCantripState();

	// Eased here rather than in the AnimBP so the weight and the enum that selects the
	// pose cannot disagree: they are written in the same frame, from the same read.
	const float PoseTarget = (CantripPose == ECantripPose::None) ? 0.0f : 1.0f;

	CantripPoseAlpha = FMath::FInterpConstantTo(CantripPoseAlpha, PoseTarget, DeltaSeconds,
		1.0f / FMath::Max(0.01f, CantripPoseBlendSeconds));
	RefreshAim(DeltaSeconds);
	RefreshLegTrail(DeltaSeconds);
	RefreshFlightHeadLift();

	// All three conditions are the same thing: the clip driving the legs has no ground
	// contact worth solving against. Flying belongs here for the plainest of the three
	// reasons -- there is no floor beneath her, and foot IK left running while she hovers
	// reaches down for one and drags the legs toward whatever it finds.
	//
	// It is a SEPARATE test from bIsFalling rather than covered by it, because
	// CharacterMovement does not consider MOVE_Flying to be falling: IsFalling() is false
	// the whole time she is in the air under Wind Runner.
	//
	// bSkeletonSupportsFootIK is ANDed in rather than checked at the Control Rig node,
	// so a MetaHuman reports bAllowFootIK false to every transition rule and branch that
	// reads it -- not merely a solver running at alpha zero.
	bAllowFootIK = !bCantripCasting && !bIsFalling && !bFlying && bSkeletonSupportsFootIK;

	// The same decision as a weight, eased. Constant rather than interpolated-toward:
	// FInterpTo never quite arrives, and a solver left running at an alpha of 0.02 is a
	// solver still running -- it will still drag a foot toward ground that is not there.
	// Constant reaches exactly 0 and exactly 1, and stays there.
	FootIKAlpha = (FootIKBlendSeconds <= 0.0f)
		? (bAllowFootIK ? 1.0f : 0.0f)
		: FMath::FInterpConstantTo(FootIKAlpha, bAllowFootIK ? 1.0f : 0.0f, DeltaSeconds,
			1.0f / FootIKBlendSeconds);
}

void UChangelingAnimInstance::RefreshLegTrail(float DeltaSeconds)
{
	// Horizontal speed only. A vertical climb should not sweep the legs back -- going
	// straight up, the air is coming past her feet rather than her front, and trailing
	// them then reads as leaning into a wind that is not there.
	float Target = 0.0f;

	if (bFlying)
	{
		if (const UCharacterMovementComponent* MoveComp = Movement.Get())
		{
			const float Speed = MoveComp->Velocity.Size2D();
			Target = FMath::Clamp(Speed / FMath::Max(1.0f, LegTrailFullSpeed), 0.0f, 1.0f);
		}
	}

	// Eased rather than taken directly from speed, and in BOTH directions. Landing drops
	// bFlying in a single frame, and legs that snap from fully trailed to underneath her
	// on that frame read as a glitch at the exact moment the player is watching her land.
	LegTrailBlend = FMath::FInterpTo(LegTrailBlend, Target, DeltaSeconds, LegTrailInterpSpeed);

	LegTrailAlpha = LegTrailBlend;

	// Below a thousandth the offset is invisible but the Modify Bone nodes still evaluate;
	// zeroing here lets the AnimGraph's own alpha short-circuit them entirely.
	if (LegTrailBlend <= KINDA_SMALL_NUMBER)
	{
		LegTrailLeft = FRotator::ZeroRotator;
		LegTrailRight = FRotator::ZeroRotator;
		FlightCalfLeft = FRotator::ZeroRotator;
		FlightCalfRight = FRotator::ZeroRotator;
		return;
	}

	const float Swing = MaxLegTrailAngle * LegTrailBlend;

	// One magnitude, two signs. Which channel carries a backward swing is a property of
	// the skeleton, so it is chosen rather than assumed -- see the note on LegTrailChannel.
	auto Apply = [this](float Amount)
	{
		FRotator Out = FRotator::ZeroRotator;
		switch (LegTrailChannel)
		{
		case ECantripAimChannel::Pitch: Out.Pitch = Amount; break;
		case ECantripAimChannel::Yaw:   Out.Yaw   = Amount; break;
		case ECantripAimChannel::Roll:  Out.Roll  = Amount; break;
		case ECantripAimChannel::None:
		default:                        break;
		}
		return Out;
	};

	LegTrailLeft  = Apply(Swing * LegTrailLeftSign);
	LegTrailRight = Apply(Swing * LegTrailRightSign);

	// The knees, on the same blend as the thighs. One movement, so one alpha -- legs that
	// swung back and straightened on separate curves would fold and unfold out of step.
	const float Straighten = MaxKneeStraighten * LegTrailBlend;

	auto ApplyKnee = [this](float Amount)
	{
		FRotator Out = FRotator::ZeroRotator;
		switch (KneeChannel)
		{
		case ECantripAimChannel::Pitch: Out.Pitch = Amount; break;
		case ECantripAimChannel::Yaw:   Out.Yaw   = Amount; break;
		case ECantripAimChannel::Roll:  Out.Roll  = Amount; break;
		case ECantripAimChannel::None:
		default:                        break;
		}
		return Out;
	};

	FlightCalfLeft  = ApplyKnee(Straighten * KneeLeftSign);
	FlightCalfRight = ApplyKnee(Straighten * KneeRightSign);
}

void UChangelingAnimInstance::RefreshFlightHeadLift()
{
	// Straight off the character's current lean rather than off speed. The lean already
	// accounts for direction of travel -- a climb tips her less -- so a head lift derived
	// from it follows all of that for free, and the two can never disagree about how far
	// over she actually is.
	const float Lean = ChangelingCharacter ? ChangelingCharacter->GetFlightLeanDegrees() : 0.0f;

	if (Lean <= KINDA_SMALL_NUMBER)
	{
		FlightHeadLift = FRotator::ZeroRotator;
		return;
	}

	const float Lift = FMath::Min(Lean * FlightHeadLiftFraction, MaxFlightHeadLift)
		* FlightHeadLiftSign;

	FlightHeadLift = FRotator::ZeroRotator;
	switch (FlightHeadLiftChannel)
	{
	case ECantripAimChannel::Pitch: FlightHeadLift.Pitch = Lift; break;
	case ECantripAimChannel::Yaw:   FlightHeadLift.Yaw   = Lift; break;
	case ECantripAimChannel::Roll:  FlightHeadLift.Roll  = Lift; break;
	case ECantripAimChannel::None:
	default:                        break;
	}
}

void UChangelingAnimInstance::RefreshAim(float DeltaSeconds)
{
	// The stance belongs to the act of casting: a character who twists at the waist every
	// time the camera moves looks possessed. Flight is the exception, and does not go
	// through IsAiming at all.
	//
	// The stance exists so a caster can point a working, which is why it normally wants a
	// held input and a cantrip. A flying changeling is looking where she is going by
	// definition -- there is no separate act of aiming to ask for, and the head staying
	// level with a body pitched to horizontal is exactly what makes the pose read as
	// falling rather than flying.
	const bool bAimingByHand = ChangelingCharacter && ChangelingCharacter->IsAiming()
		&& (!bAimOnlyWhileCasting || bCantripActive);

	bAimingActive = bAimingByHand || (bFlying && bAimWhileFlying);

	FRotator Target = FRotator::ZeroRotator;

	if (bAimingActive)
	{
		// Base aim rotation rather than the controller's, so an AI caster aims by the
		// same rule a player does. Normalized delta, because a raw subtraction wraps at
		// 180 and would send the spine the long way round.
		const FRotator Look = ChangelingCharacter->GetBaseAimRotation();
		const FRotator Body = ChangelingCharacter->GetActorRotation();
		Target = (Look - Body).GetNormalized();

		Target.Yaw   = FMath::Clamp(Target.Yaw, -MaxAimYaw, MaxAimYaw);
		Target.Pitch = FMath::Clamp(Target.Pitch, -MaxAimPitchDown, MaxAimPitchUp);
		Target.Roll  = 0.0f;
	}

	// Interpolated in both directions, so releasing the stance unwinds rather than
	// snapping straight. Zero is just another target.
	SpineAim = FMath::RInterpTo(SpineAim, Target, DeltaSeconds, AimInterpSpeed);

	// The two shares SUM to the whole aim, they do not stack. Rotations compose down
	// the hierarchy, so the head arrives carrying everything its ancestors did; giving
	// the neck its own slice on top would point the face at twice the angle asked for.
	const float NeckPart = FMath::Clamp(NeckAimFraction, 0.0f, 1.0f);
	const float SpinePart = 1.0f - NeckPart;

	SpineBoneAim = MakeBoneAim(SpineAim, SpinePart / FMath::Max(1, SpineBoneCount));
	NeckBoneAim  = MakeBoneAim(SpineAim, NeckPart  / FMath::Max(1, NeckBoneCount));
}

FRotator UChangelingAnimInstance::GetAimWorldRotation() const
{
	if (!ChangelingCharacter)
	{
		return FRotator::ZeroRotator;
	}

	// Actor rotation plus the CLAMPED offset -- deliberately not the camera. This is
	// where the body got to, and anything leaving the hands has to agree with it.
	return (ChangelingCharacter->GetActorRotation() + SpineAim).GetNormalized();
}

FRotator UChangelingAnimInstance::MakeBoneAim(const FRotator& Aim, float Share) const
{
	// Each bone channel picks its source, so any mapping is expressible -- including the
	// three-way rotations that a pair of swap booleans cannot reach.
	auto Channel = [&Aim](ECantripAimChannel Source) -> float
	{
		switch (Source)
		{
		case ECantripAimChannel::Pitch: return Aim.Pitch;
		case ECantripAimChannel::Yaw:   return Aim.Yaw;
		case ECantripAimChannel::Roll:  return Aim.Roll;
		default:                        return 0.0f;
		}
	};

	float Pitch = Channel(BonePitchFrom) * Share * AimPitchSign;
	float Yaw   = Channel(BoneYawFrom)   * Share * AimYawSign;
	float Roll  = Channel(BoneRollFrom)  * Share * AimRollSign;

	// Clamped per bone rather than on the total, because the ceiling is a property of
	// the joint. However far the camera swings, no single vertebra is asked for more
	// than one vertebra can give.
	const float Limit = FMath::Abs(MaxPerBoneAngle);

	return FRotator(
		FMath::Clamp(Pitch, -Limit, Limit),
		FMath::Clamp(Yaw,   -Limit, Limit),
		FMath::Clamp(Roll,  -Limit, Limit));
}

bool UChangelingAnimInstance::IsMontagePlayingOnSlot(FName SlotName) const
{
	for (const FAnimMontageInstance* Instance : MontageInstances)
	{
		if (Instance && Instance->IsActive() && Instance->Montage
			&& Instance->Montage->IsValidSlot(SlotName))
		{
			return true;
		}
	}

	return false;
}

void UChangelingAnimInstance::RefreshCantripState()
{
	// Read off the BASE, so an NPC holding a working raises its arm exactly as she does.
	CantripPose = OwnerCharacter->GetCantripPose();
	bCantripHandBlocked = OwnerCharacter->IsCantripHandBlocked();

	/**
	 * Asked of the montages, not of the ability, and asked EVERY frame -- including the
	 * frames where no cantrip is running at all.
	 *
	 * A full-body montage outlives the ability that started it: the lowering plays out
	 * after the cast has ended, and foot IK switching back on halfway through that would
	 * plant her feet mid-gesture. Deriving this from the ability's tag would do exactly
	 * that. Deriving it from what is actually playing cannot.
	 */
	bCantripCasting = IsMontagePlayingOnSlot(FullBodySlotName);

	UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if (!ASC)
	{
		bCantripActive = false;
		bCantripChannelling = false;
		bCantripRecovering = false;
		return;
	}

	// Held, not Casting. This asks "is a cantrip in force", and Cantrip.State.Held is now
	// the tag that answers it for the ability's whole lifetime -- which is exactly what
	// Casting used to mean before it was narrowed to the wind-up alone. Left on Casting,
	// a sustained working would report itself finished the moment it lit, and the aim
	// stance would drop out from under a cantrip that is still running.
	bCantripActive = ASC->HasMatchingGameplayTag(ChangelingTags::Cantrip_State_Held);

	if (!bCantripActive)
	{
		bCantripChannelling = false;
		bCantripRecovering = false;
		bHasChannelledThisCast = false;
		ActiveCantrip.Reset();

		// The element is deliberately left alone -- an effect spawned by the cast may
		// still be burning down and wants its colour. The intensity does go to zero,
		// because nothing should still be sounding by now.
		ChannelIntensity = 0.0f;
		return;
	}

	if (!ActiveCantrip.IsValid())
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.IsActive())
			{
				continue;
			}

			// The spec's Ability is the class default for an instanced ability; the
			// live instance is what carries this cast's state.
			const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
			if (const UGA_Cantrip* Cantrip = Cast<UGA_Cantrip>(Instance ? Instance : Spec.Ability))
			{
				ActiveCantrip = Cantrip;
				break;
			}
		}
	}

	if (const UGA_Cantrip* Cantrip = ActiveCantrip.Get())
	{
		if (const FCantripSpec* Row = Cantrip->GetCantripSpec())
		{
			CantripElement = Row->Element;
		}
	}

	bCantripChannelling = bCantripActive &&
		ASC->HasMatchingGameplayTag(ChangelingTags::Cantrip_State_Channelling);

	// Not simply "casting but not channelling" -- that is equally true of the WIND-UP,
	// which has not recovered from anything yet. Recovery is the far side of a channel,
	// so it needs to remember that one happened.
	bHasChannelledThisCast |= bCantripChannelling;
	bCantripRecovering = bCantripActive && !bCantripChannelling && bHasChannelledThisCast;

	// Republished from the curve rather than read straight off it in Blueprint, so a
	// state blending out still reports the value its own clip is authoring.
	ChannelIntensity = FMath::Clamp(GetCurveValue(TEXT("EffectIntensity")), 0.0f, 1.0f);
}

void UChangelingAnimInstance::AnimNotify_CantripEffectStart()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		FGameplayEventData Payload;
		Payload.EventTag = ChangelingTags::Cantrip_Event_EffectStart;
		Payload.Instigator = ChangelingCharacter;

		ASC->HandleGameplayEvent(ChangelingTags::Cantrip_Event_EffectStart, &Payload);
	}
}

void UChangelingAnimInstance::AnimNotify_CantripRecovered()
{
	// The character first, and unconditionally. Releasing the movement lock is the one
	// thing here that must survive a missing or torn-down ability system -- everything
	// else failing is a cosmetic problem, this failing is a character that cannot walk.
	if (ChangelingCharacter)
	{
		ChangelingCharacter->NotifyCantripRecovered();
	}

	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		FGameplayEventData Payload;
		Payload.EventTag = ChangelingTags::Cantrip_Event_Recovered;
		Payload.Instigator = ChangelingCharacter;

		ASC->HandleGameplayEvent(ChangelingTags::Cantrip_Event_Recovered, &Payload);
	}
}
