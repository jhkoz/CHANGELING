// GA_ProjectileCantrip.cpp

#include "GA_ProjectileCantrip.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "CHANGELINGCharacter.h"
#include "CantripProjectile.h"
#include "ChangelingAnimInstance.h"
#include "ChangelingGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Curves/CurveFloat.h"
#include "NiagaraFunctionLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectileCantrip, Log, All);

void UGA_ProjectileCantrip::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// The base can end the ability inside that call -- a failed commit, a missing row --
	// and putting a flame in the hand of an ability that is already over strands it
	// there with nothing left running to take it away again.
	if (!IsActive())
	{
		return;
	}

	StartGather();
}

void UGA_ProjectileCantrip::StartGather()
{
	if (!bGatherWhileHeld || !CastEffect)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// A flame from the previous throw may still be dying back. Adopting it would hand
	// this cast a component already partway through a fade it cannot cancel.
	if (GatheredEffect)
	{
		ClearWindup();
	}

	GatheredEffect = SpawnGatheredEffect();
	if (!GatheredEffect)
	{
		return;
	}

	Phase = EGatherPhase::Charging;
	bReleased = false;
	SwingHistory.Reset();

	// Pushed once immediately as well as on the timer, or the flame spends its first
	// frames at whatever the system's authored defaults are -- which for a fire effect is
	// full strength, and a charge that starts at full has nothing left to show.
	UpdateWindup();

	World->GetTimerManager().SetTimer(WindupTimer,
		FTimerDelegate::CreateUObject(this, &UGA_ProjectileCantrip::UpdateWindup),
		WindupUpdateInterval, true);

	const FCantripSpec* Spec = GetCantripSpec();
	const float FromTiers = Spec ? Spec->CastTiers.FullHoldSeconds() : 0.0f;

	UE_LOG(LogProjectileCantrip, Log,
		TEXT("GATHER %s: flame in hand, full charge at %.2fs (%s)."), *GetName(),
		FromTiers > 0.0f ? FromTiers : GatherFullSeconds,
		FromTiers > 0.0f ? TEXT("from cast tiers") : TEXT("fallback -- row has no tiers"));
}

UNiagaraComponent* UGA_ProjectileCantrip::SpawnGatheredEffect()
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;

	if (!Mesh || !CastEffect)
	{
		return nullptr;
	}

	// Checked rather than trusted. SpawnSystemAttached does not fail on a socket it
	// cannot find -- it attaches at the component's ORIGIN instead, which on a character
	// mesh is down at the feet. The effect appears, the projectile spawns where the
	// effect is, and the whole thing looks like a physics bug rather than a typo.
	if (!AttachSocket.IsNone() && !Mesh->DoesSocketExist(AttachSocket))
	{
		UE_LOG(LogProjectileCantrip, Warning,
			TEXT("Socket '%s' not found on %s -- effect and projectile will spawn at the "
				 "mesh origin."),
			*AttachSocket.ToString(), *GetNameSafe(Mesh->GetSkeletalMeshAsset()));
	}

	return UNiagaraFunctionLibrary::SpawnSystemAttached(
		CastEffect, Mesh, AttachSocket, AttachOffset, FRotator::ZeroRotator,
		EAttachLocation::SnapToTargetIncludingScale, /*bAutoDestroy*/ false,
		/*bAutoActivate*/ true);
}

float UGA_ProjectileCantrip::GatherAlpha() const
{
	// The cantrip's own tiers first, so the flame reaches full at the same instant the
	// hold stops earning anything. A separate charge time would drift out of step with
	// the mechanic and teach the player the wrong moment to let go.
	float Full = GatherFullSeconds;

	if (const FCantripSpec* Spec = GetCantripSpec())
	{
		const float FromTiers = Spec->CastTiers.FullHoldSeconds();
		if (FromTiers > 0.0f)
		{
			Full = FromTiers;
		}
	}

	if (Full <= 0.0f)
	{
		return 1.0f;
	}

	return FMath::Clamp(GetHeldSeconds() / Full, 0.0f, 1.0f);
}

void UGA_ProjectileCantrip::PostResolve(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec)
{
	// Reset per cast, not per instance.
	//
	// The ability is InstancedPerActor, so this object is the same one for the whole
	// life of the character. Anything left set from the last throw is still set on the
	// next -- and bReleased latching true means the second cast plays its montage, fires
	// its notify, and quietly declines to throw anything.
	bReleased = false;
	Intensity = 0.0f;

	// NOT cleared: GatheredEffect is the flame the player has been holding, and it is the
	// same fire that is about to be thrown. Nulling it here would leave the charged one
	// orphaned on the hand while a second identical system was spawned on top of it.

	// Same reason as the flag above: samples left from the last throw would have the
	// next one leaving along an arc the arm finished swinging seconds ago. The hold's own
	// samples go too -- shifting weight over a long charge is not a throwing arc.
	SwingHistory.Reset();

	// A failed working never leaves the hand. Faded rather than cut, so a botch looks
	// like a flame that guttered out in the palm -- which is the only feedback the player
	// gets, since the Glamour is spent either way and no number is ever shown.
	if (!Outcome.bSucceeded)
	{
		FadeWindup();
		Super::PostResolve(Outcome, Spec);
		return;
	}

	Intensity = BaseIntensity + (IntensityPerSuccess * FMath::Max(0, Outcome.Successes - 1));

	UWorld* World = GetWorld();
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;

	if (!Mesh || !CastMontage)
	{
		UE_LOG(LogProjectileCantrip, Warning,
			TEXT("No mesh or no cast montage on %s -- nothing to throw."), *GetName());

		FadeWindup();
		Super::PostResolve(Outcome, Spec);
		return;
	}

	// Gathered before the montage starts so the flame is already in the palm as the arm
	// comes back, rather than appearing at the moment of release. With the hold enabled
	// it has been there since the press and simply carries on.
	if (CastEffect)
	{
		if (!GatheredEffect)
		{
			GatheredEffect = SpawnGatheredEffect();
		}

		// The roll is in, so the promise the charge was making is replaced by the real
		// number. From here the wind-up curve owns the parameter, not the charge.
		Phase = EGatherPhase::Throwing;

		if (GatheredEffect && !IntensityParameter.IsNone())
		{
			GatheredEffect->SetVariableFloat(IntensityParameter, Intensity);
		}

		// The flame gathering in the palm is sized by the roll too, so the throw is
		// already legible before it leaves -- a strong casting looks strong while it is
		// still being wound up, not only once it is in the air.
		if (GatheredEffect)
		{
			const float Scale = WindupBaseScale + (WindupScalePerIntensity * Intensity);
			GatheredEffect->SetRelativeScale3D(FVector(FMath::Max(0.01f, Scale)));
		}

		if (GatheredEffect)
		{
			WindupStartTime = World ? World->GetTimeSeconds() : 0.0f;

			// Pushed once immediately as well as on the timer, or the flame is at full
			// strength for the first frame before the curve's opening value lands.
			UpdateWindup();

			// Also runs for Arm Swing, which needs the socket sampled over time whether
			// or not there is a curve to drive.
			if (World && (WindupCurve
				|| ThrowDirectionSource == EThrowDirectionSource::SocketMotion))
			{
				World->GetTimerManager().SetTimer(WindupTimer,
					FTimerDelegate::CreateUObject(this, &UGA_ProjectileCantrip::UpdateWindup),
					WindupUpdateInterval, true);
			}
		}
	}

	if (CastSound)
	{
		UGameplayStatics::SpawnSoundAttached(CastSound, Mesh, AttachSocket);
	}

	// Two tasks rather than one: the montage says when the ability is over, the notify
	// says when the projectile leaves. Timing the release off the montage's own length
	// would break the moment anyone retimes the animation.
	if (UAbilityTask_WaitGameplayEvent* WaitRelease =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, ChangelingTags::Cantrip_Event_Release))
	{
		WaitRelease->EventReceived.AddDynamic(this, &UGA_ProjectileCantrip::OnRelease);
		WaitRelease->ReadyForActivation();
	}

	if (UAbilityTask_PlayMontageAndWait* PlayMontage =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, CastMontage, MontagePlayRate))
	{
		// All four outcomes end the ability. An interrupted throw is still over -- the
		// caster was hit, or something cancelled it, and leaving the ability running
		// would strand the gathered effect on their hand.
		PlayMontage->OnCompleted.AddDynamic(this, &UGA_ProjectileCantrip::OnMontageFinished);
		PlayMontage->OnBlendOut.AddDynamic(this, &UGA_ProjectileCantrip::OnMontageFinished);
		PlayMontage->OnInterrupted.AddDynamic(this, &UGA_ProjectileCantrip::OnMontageFinished);
		PlayMontage->OnCancelled.AddDynamic(this, &UGA_ProjectileCantrip::OnMontageFinished);
		PlayMontage->ReadyForActivation();
	}
}

void UGA_ProjectileCantrip::OnRelease(FGameplayEventData Payload)
{
	// A montage can be retriggered, and a notify can be crossed twice if somebody
	// scrubs. Once thrown, thrown.
	if (bReleased)
	{
		return;
	}

	bReleased = true;

	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();

	if (!World || !Avatar)
	{
		return;
	}

	// Stamped here rather than at EndAbility, so the gap is measured from the throw
	// itself and a long recovery animation does not eat into it.
	LastReleaseTime = World->GetTimeSeconds();

	// Loud, because the notify firing and the projectile never appearing look identical
	// from outside -- and an empty class here is the likelier of the two.
	if (!ProjectileClass)
	{
		UE_LOG(LogProjectileCantrip, Warning,
			TEXT("RELEASE on %s but ProjectileClass is empty -- nothing to throw."),
			*GetName());
		return;
	}

	const FVector Direction = ResolveThrowDirection();

	// Belt height off the actor, not wherever the wrist has swung to. The actor origin
	// on a character is the capsule's centre, so a Z near zero is between the hip and
	// the buckle -- where a pitch actually lets go.
	/**
	 * The hand's world position, sampled NOW.
	 *
	 * Not remembered from the wind-up and not inferred from the gathered effect's
	 * component: the arm has been moving throughout the throw, and the only frame whose
	 * hand position means anything is the one the release fires on.
	 */
	FVector SocketAt = FVector::ZeroVector;
	bool bHaveSocket = false;

	if (const ACharacter* Thrower = Cast<ACharacter>(Avatar))
	{
		if (const USkeletalMeshComponent* Mesh = Thrower->GetMesh())
		{
			if (Mesh->DoesSocketExist(AttachSocket))
			{
				SocketAt = Mesh->GetSocketLocation(AttachSocket);
				bHaveSocket = true;
			}
			else
			{
				// Loud, because the symptom is a fireball leaving from the wrong place,
				// which reads as a tuning problem rather than as a name typed wrong.
				UE_LOG(LogProjectileCantrip, Warning,
					TEXT("RELEASE on %s: socket '%s' not found on %s -- falling back."),
					*GetName(), *AttachSocket.ToString(),
					*GetNameSafe(Mesh->GetSkeletalMeshAsset()));
			}
		}
	}

	FVector HandAt;

	if (bReleaseFromBody)
	{
		// Steady in plan: the actor origin does not swing, so the fireball leaves from
		// beside the hip whatever the wrist is doing.
		HandAt = Avatar->GetActorTransform().TransformPosition(ReleaseBodyOffset);

		/**
		 * ...but the height still comes from the hand when asked for.
		 *
		 * ReleaseBodyOffset's Z is measured from the capsule's CENTRE, which is belt
		 * height and a good deal lower than it reads on screen -- low enough that every
		 * throw was spawning inside the ground she stood on, hundreds of units within the
		 * landscape at one site and a floor mesh at another. No forward offset can fix
		 * that, because the error is vertical.
		 */
		if (bReleaseAtSocketHeight && bHaveSocket)
		{
			HandAt.Z = SocketAt.Z;
		}
	}
	else if (bHaveSocket)
	{
		// Straight from the hand, in all three axes. Truest to what is on screen, and
		// right for a throw whose animation actually presents the release; the cost is
		// that a wild arm carries the release point with it.
		HandAt = SocketAt;
	}
	else
	{
		HandAt = GatheredEffect ? GatheredEffect->GetComponentLocation()
								: Avatar->GetActorLocation();
	}

	// Pushed out along the throw so it leaves the fingertips rather than the middle of
	// the fist.
	const FVector SpawnAt = HandAt + (Direction.GetSafeNormal() * ReleaseForwardOffset);

	FActorSpawnParameters Params;
	Params.Owner = Avatar;
	Params.Instigator = Cast<APawn>(Avatar);
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACantripProjectile* Projectile = World->SpawnActor<ACantripProjectile>(
		ProjectileClass, SpawnAt, Direction.Rotation(), Params);

	if (!Projectile)
	{
		return;
	}

	// Handed over rather than respawned. The particles already gathered in the palm are
	// the fireball -- destroying the system to make a fresh one on the projectile would
	// throw them away and the flame would blink instead of leaving.
	UE_LOG(LogProjectileCantrip, Log,
		TEXT("RELEASE: %s thrown from %s along %s, intensity %.1f%s"),
		*GetNameSafe(Projectile), *SpawnAt.ToCompactString(),
		*Direction.ToCompactString(), Intensity,
		GatheredEffect ? TEXT("") : TEXT(" (no gathered effect to hand over)"));

	// Launched with nothing to adopt. The gathering flame stays on the hand and dies
	// back over its fade, while the projectile brings its own effect -- the fire in the
	// palm and the fire in flight are two things, and handing one over empties the hand
	// in the same frame the fireball appears.
	Projectile->Launch(nullptr, Direction, Intensity,
		ResolveSwingSpeed() * FMath::Clamp(HandSpeedContribution, 0.0f, 1.0f));

	FadeWindup();
}

void UGA_ProjectileCantrip::UpdateWindup()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Sampled before the early-out below, because the swing has to be tracked even on a
	// cast with no gathered effect and no curve to push a value into.
	if (ThrowDirectionSource == EThrowDirectionSource::SocketMotion && !bReleased)
	{
		if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
			{
				if (!AttachSocket.IsNone() && Mesh->DoesSocketExist(AttachSocket))
				{
					const float Now = World->GetTimeSeconds();
					SwingHistory.Emplace(Now, Mesh->GetSocketLocation(AttachSocket));

					// Half a second is far more than the sample window needs and keeps
					// the array from growing for the length of a held charge.
					while (SwingHistory.Num() > 1 && Now - SwingHistory[0].Key > 0.5f)
					{
						SwingHistory.RemoveAt(0, 1, EAllowShrinking::No);
					}
				}
			}
		}
	}

	if (!GatheredEffect)
	{
		return;
	}

	// While the key is held nothing has been rolled yet, so the flame is driven by the
	// hold alone -- and it drives size and intensity as well as the fade parameter,
	// because a charge that only brightens does not read as gathering.
	if (Phase == EGatherPhase::Charging)
	{
		const float Alpha = GatherAlpha();
		const float Shaped = GatherCurve
			? FMath::Clamp(GatherCurve->GetFloatValue(Alpha), 0.0f, 1.0f)
			: Alpha;

		if (!WindupParameter.IsNone())
		{
			GatheredEffect->SetVariableFloat(WindupParameter, Shaped);
		}

		if (!IntensityParameter.IsNone())
		{
			GatheredEffect->SetVariableFloat(IntensityParameter,
				GatherIntensityAtFull * Shaped);
		}

		GatheredEffect->SetRelativeScale3D(FVector(FMath::Max(0.01f,
			FMath::Lerp(GatherStartScale, GatherFullScale, Shaped))));

		return;
	}

	if (WindupParameter.IsNone())
	{
		return;
	}

	const float Elapsed = World->GetTimeSeconds() - WindupStartTime;

	// Before release the curve owns the value; after it, the fade does. Both write the
	// same parameter, so whichever is running has to be the only one deciding.
	float Value = 1.0f;

	if (Phase == EGatherPhase::Fading)
	{
		const float FadeElapsed = World->GetTimeSeconds() - FadeStartTime;
		const float Alpha = WindupFadeSeconds > 0.0f
			? FMath::Clamp(FadeElapsed / WindupFadeSeconds, 0.0f, 1.0f)
			: 1.0f;

		Value = FMath::Lerp(FadeFromValue, 0.0f, Alpha);

		if (Alpha >= 1.0f)
		{
			GatheredEffect->SetVariableFloat(WindupParameter, 0.0f);
			ClearWindup();
			return;
		}
	}
	else if (WindupCurve)
	{
		Value = FMath::Clamp(WindupCurve->GetFloatValue(Elapsed), 0.0f, 1.0f);
	}

	GatheredEffect->SetVariableFloat(WindupParameter, Value);
}

void UGA_ProjectileCantrip::FadeWindup()
{
	UWorld* World = GetWorld();
	if (!GatheredEffect || !World)
	{
		ClearWindup();
		return;
	}

	FadeStartTime = World->GetTimeSeconds();

	// Faded from wherever the flame had actually got to, not from full. Snapping to 1 to
	// begin the fade would make it flare at the exact moment it is supposed to leave --
	// and a charge abandoned a quarter of the way in would flare hardest of all.
	if (Phase == EGatherPhase::Charging)
	{
		const float Alpha = GatherAlpha();
		FadeFromValue = GatherCurve
			? FMath::Clamp(GatherCurve->GetFloatValue(Alpha), 0.0f, 1.0f)
			: Alpha;
	}
	else
	{
		FadeFromValue = WindupCurve
			? FMath::Clamp(WindupCurve->GetFloatValue(FadeStartTime - WindupStartTime),
				0.0f, 1.0f)
			: 1.0f;
	}

	Phase = EGatherPhase::Fading;

	// Spawning stops immediately; what is already in the air burns out on its own, which
	// is what makes it read as dissipating rather than being switched off.
	GatheredEffect->Deactivate();

	World->GetTimerManager().SetTimer(WindupTimer,
		FTimerDelegate::CreateUObject(this, &UGA_ProjectileCantrip::UpdateWindup),
		WindupUpdateInterval, true);
}

void UGA_ProjectileCantrip::ClearWindup()
{
	Phase = EGatherPhase::Idle;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindupTimer);
	}

	if (GatheredEffect)
	{
		GatheredEffect->DestroyComponent();
		GatheredEffect = nullptr;
	}
}

bool UGA_ProjectileCantrip::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	if (ReleaseCooldownSeconds <= 0.0f)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	return (World->GetTimeSeconds() - LastReleaseTime) >= ReleaseCooldownSeconds;
}

void UGA_ProjectileCantrip::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

float UGA_ProjectileCantrip::ResolveSwingSpeed() const
{
	if (SwingHistory.Num() < 2)
	{
		return 0.0f;
	}

	const TPair<float, FVector>& Newest = SwingHistory.Last();
	const float Cutoff = Newest.Key - SwingSampleSeconds;

	const TPair<float, FVector>* Oldest = &SwingHistory[0];
	for (const TPair<float, FVector>& Sample : SwingHistory)
	{
		if (Sample.Key <= Cutoff)
		{
			Oldest = &Sample;
		}
		else
		{
			break;
		}
	}

	const float Span = Newest.Key - Oldest->Key;
	if (Span <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// Distance over time across the sampled window. Straight-line rather than arc
	// length: the ball leaves on the tangent, and the tangent is what the hand covered
	// between those two points.
	return FVector::Dist(Newest.Value, Oldest->Value) / Span;
}

FVector UGA_ProjectileCantrip::ResolveThrowDirection() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return FVector::ForwardVector;
	}

	const ACharacter* Character = Cast<ACharacter>(Avatar);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;

	FRotator Throw = Avatar->GetActorRotation();

	if (ThrowDirectionSource == EThrowDirectionSource::SocketMotion
		&& Mesh && !AttachSocket.IsNone() && Mesh->DoesSocketExist(AttachSocket)
		&& SwingHistory.Num() >= 2)
	{
		const FVector Current = Mesh->GetSocketLocation(AttachSocket);
		const float Cutoff = SwingHistory.Last().Key - SwingSampleSeconds;

		// The oldest sample still inside the window, so the tangent spans the requested
		// interval rather than whatever one tick happened to cover.
		FVector From = SwingHistory[0].Value;
		for (const TPair<float, FVector>& Sample : SwingHistory)
		{
			if (Sample.Key <= Cutoff)
			{
				From = Sample.Value;
			}
			else
			{
				break;
			}
		}

		const FVector Swing = Current - From;

		// A hand that has barely moved gives a direction made of noise. Falling through
		// to the socket's own axis is wrong less often than throwing at random.
		if (Swing.SizeSquared() > FMath::Square(2.0f))
		{
			FRotator SwingThrow = Swing.Rotation();
			SwingThrow.Pitch = FMath::Clamp(SwingThrow.Pitch, -MaxThrowPitch, MaxThrowPitch);
			SwingThrow.Roll = 0.0f;

			return BiasForward(SwingThrow.Vector(), Avatar);
		}
	}

	if ((ThrowDirectionSource == EThrowDirectionSource::SocketAxis
			|| ThrowDirectionSource == EThrowDirectionSource::SocketMotion)
		&& Mesh && !AttachSocket.IsNone() && Mesh->DoesSocketExist(AttachSocket))
	{
		// Straight off the socket's own basis on the frame the notify fired. The hand is
		// carried by a body that is already turning to the aim, so this follows the aim
		// without being told to -- and it agrees with the animation, which the aim
		// rotation does not have to.
		FVector Axis =
			Mesh->GetSocketTransform(AttachSocket).GetUnitAxis(SocketThrowAxis);

		if (bInvertSocketAxis)
		{
			Axis = -Axis;
		}

		Throw = Axis.Rotation();
	}
	else if (Mesh)
	{
		if (const UChangelingAnimInstance* Anim =
			Cast<UChangelingAnimInstance>(Mesh->GetAnimInstance()))
		{
			// The pose's aim, not the camera's. It already has the spine limits
			// applied, so the throw follows a line the body can actually support
			// rather than one only the camera can reach.
			Throw = Anim->GetAimWorldRotation();
		}
	}

	// Clamped after the fact rather than trusted: a camera looking near-vertical would
	// otherwise have the caster lob the thing over their own head.
	Throw.Pitch = FMath::Clamp(Throw.Pitch, -MaxThrowPitch, MaxThrowPitch);
	Throw.Roll = 0.0f;

	return BiasForward(Throw.Vector(), Avatar);
}

FVector UGA_ProjectileCantrip::BiasForward(const FVector& Raw, const AActor* Avatar) const
{
	if (!Avatar)
	{
		return Raw;
	}

	const FVector Facing = Avatar->GetActorForwardVector();
	const FVector Unit = Raw.GetSafeNormal();

	FVector Result =
		FMath::Lerp(Unit, Facing, FMath::Clamp(ForwardBias, 0.0f, 1.0f)).GetSafeNormal();

	// A hard floor on top of the blend. Bias alone cannot save a tangent pointing
	// straight backwards -- halfway between behind and ahead is sideways, and the
	// fireball still leaves past the caster's shoulder. Anything still in the rear
	// hemisphere is folded forward rather than allowed through.
	if (FVector::DotProduct(Result, Facing) < 0.0f)
	{
		Result = (Result - (2.0f * FVector::DotProduct(Result, Facing) * Facing))
			.GetSafeNormal();
	}

	if (Result.IsNearlyZero())
	{
		return Facing;
	}

	// Clamped HERE, after the blend, rather than upstream on each source. Clamping the
	// raw tangent and then blending leaves the blend free to reintroduce pitch, so the
	// limit has to be the last thing applied or it is not a limit.
	FRotator Final = Result.Rotation();
	Final.Pitch = FMath::Clamp(Final.Pitch, -MaxThrowPitchDown, MaxThrowPitch);
	Final.Roll = 0.0f;

	return Final.Vector();
}

void UGA_ProjectileCantrip::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Belt and braces, for the flame left burning on a hand that never threw anything:
	// a montage missing its notify, an interrupted throw, or a cast that fizzled before
	// the roll because the caster could not pay.
	//
	// Faded rather than destroyed, because every one of those cases is a working that
	// failed and a fire that goes out is the only way the player is told so. A throw that
	// already released is left alone -- its fade is running and restarting it would make
	// the flame jump back up as the montage ends.
	if (Phase == EGatherPhase::Charging || Phase == EGatherPhase::Throwing)
	{
		FadeWindup();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
