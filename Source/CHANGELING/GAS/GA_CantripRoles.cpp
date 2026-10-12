// GA_CantripRoles.cpp

#include "GA_CantripRoles.h"

#include "CantripPortal.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripRole, Log, All);

// ── Damage ──────────────────────────────────────────────────────────────────

void UGA_DamageCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	float Damage = ScaleByRoll(BaseDamage, DamagePerSuccess, Outcome);

	// Spread across the duration rather than delivered whole. A cantrip that sets
	// someone ablaze should burn them for as long as it burns; the effect asset applies
	// this per period, so the magnitude it wants is the per-tick share.
	if (bDamageOverTime && Spec.Kind == ECantripKind::Sustained)
	{
		const float Duration =
			FMath::Max(1.0f, BaseDuration + (DurationPerSuccess * FMath::Max(0, Outcome.Successes - 1)));
		Damage /= Duration;
	}

	TArray<FActiveGameplayEffectHandle> Handles;
	ApplyEffectToTargets(Targets, DamageEffect, Damage, Handles);

	UE_LOG(LogCantripRole, Log, TEXT("DAMAGE %s: %.1f %s to %d target(s)."),
		*GetName(), Damage,
		DamageType == ECantripDamageType::Aggravated ? TEXT("aggravated")
			: DamageType == ECantripDamageType::Lethal ? TEXT("lethal") : TEXT("bashing"),
		Targets.Actors.Num());
}

// ── Control ─────────────────────────────────────────────────────────────────

void UGA_ControlCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	// The magnitude a control effect wants is how hard it is to shake off, not how much
	// of anything is applied -- so it carries the resist difficulty rather than a number
	// of dice.
	const float Resist = ResistDifficultyPerSuccess * FMath::Max(1, Outcome.Successes);

	ApplyEffectToTargets(Targets, ControlEffect, Resist, Applied);

	UE_LOG(LogCantripRole, Log, TEXT("CONTROL %s: %d held, resist %.0f."),
		*GetName(), Applied.Num(), Resist);
}

void UGA_ControlCantrip::RevertCantrip()
{
	RemoveAppliedEffects(Applied);
}

// ── Debuff and Buff ─────────────────────────────────────────────────────────

void UGA_DebuffCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	// Negative, so an effect asset can add the magnitude without having to know whether
	// it is helping or hurting. One curve, one sign convention.
	const float Penalty = -ScaleByRoll(BasePenalty, PenaltyPerSuccess, Outcome);

	ApplyEffectToTargets(Targets, DebuffEffect, Penalty, Applied);
}

void UGA_DebuffCantrip::RevertCantrip()
{
	RemoveAppliedEffects(Applied);
}

void UGA_BuffCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	ApplyEffectToTargets(Targets, BuffEffect,
		ScaleByRoll(BaseBonus, BonusPerSuccess, Outcome), Applied);
}

void UGA_BuffCantrip::RevertCantrip()
{
	RemoveAppliedEffects(Applied);
}

// ── Protection ──────────────────────────────────────────────────────────────

void UGA_ProtectionCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	ApplyEffectToTargets(Targets, ProtectionEffect,
		ScaleByRoll(BaseMitigation, MitigationPerSuccess, Outcome), Applied);
}

void UGA_ProtectionCantrip::RevertCantrip()
{
	RemoveAppliedEffects(Applied);
}

// ── Healing ─────────────────────────────────────────────────────────────────

void UGA_HealingCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	TArray<FActiveGameplayEffectHandle> Handles;

	ApplyEffectToTargets(Targets, HealEffect,
		ScaleByRoll(BaseHealing, HealingPerSuccess, Outcome), Handles);

	UE_LOG(LogCantripRole, Log, TEXT("HEAL %s: %d target(s)%s%s."), *GetName(),
		Handles.Num(),
		bMendsAggravated ? TEXT(", mends aggravated") : TEXT(""),
		bRestoresLife ? TEXT(", restores life") : TEXT(""));
}

// ── Mobility ────────────────────────────────────────────────────────────────

void UGA_MobilityCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	const float Distance = ScaleByRoll(BaseDistance, DistancePerSuccess, Outcome);

	// Speed, Bound and Flight apply an INFINITE effect and rely on RevertCantrip to take
	// it off again -- and RevertCantrip is only ever reached from EndCantrip, which only
	// a Sustained row's duration timer fires. On an Instant row the effect is applied and
	// then nothing in the game will ever remove it.
	//
	// For Flight that is not a cosmetic bug: gravity never comes back, so the character
	// hovers permanently with no way down and no way to end it. It cost a session to find
	// precisely because both halves looked correct in isolation -- the row said Instant,
	// which is a perfectly ordinary thing for a row to say, and the ability said Infinite,
	// which is right for something whose lifetime the ability owns.
	if (Spec.Kind != ECantripKind::Sustained
		&& (Mode == ECantripMobilityMode::Speed
		 || Mode == ECantripMobilityMode::Bound
		 || Mode == ECantripMobilityMode::Flight))
	{
		UE_LOG(LogCantripRole, Error,
			TEXT("Mobility cantrip '%s' is mode %d but its row Kind is %d, not Sustained. "
			     "This mode applies a lasting effect that only a Sustained row's duration "
			     "can remove. Set Kind=Sustained on that row in DT_Cantrips. Refusing to "
			     "apply, rather than leaving the caster permanently altered."),
			*GetCantripRowName().ToString(), static_cast<int32>(Mode),
			static_cast<int32>(Spec.Kind));

		return;
	}

	switch (Mode)
	{
	case ECantripMobilityMode::Leap:
		if (ACharacter* Character = Cast<ACharacter>(Avatar))
		{
			FVector Eye;
			FRotator Look;
			Avatar->GetActorEyesViewPoint(Eye, Look);

			// Launched with the existing velocity overridden. Added to, a leap cast while
			// already sprinting sends her somewhere neither she nor the level expected.
			Character->LaunchCharacter(Look.Vector() * Distance, true, true);
		}
		break;

	case ECantripMobilityMode::Blink:
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			break;
		}

		// Capped at the reach the roll bought, so a blink cannot outrun its own
		// distance just because the aim trace found something further away.
		const FVector From = Avatar->GetActorLocation();
		FVector Toward = Targets.Point - From;

		// A blink is the one working that is meaningless without a direction, and the
		// delivery modes that do not produce one -- SelfCast, an unset ExplicitTarget --
		// resolve the target to the caster's own feet. That is a zero-length line, and it
		// teleported her precisely nowhere while reporting a clean success every time.
		//
		// Rather than fail, aim her forward. Flattened, because the view pitch in third
		// person points at the ground a few paces ahead, and a blink is not a dive.
		if (Toward.IsNearlyZero())
		{
			FVector Eye;
			FRotator Look;
			Avatar->GetActorEyesViewPoint(Eye, Look);

			FVector Ahead = Look.Vector().GetSafeNormal2D();
			if (Ahead.IsNearlyZero())
			{
				Ahead = Avatar->GetActorForwardVector().GetSafeNormal2D();
			}

			Toward = Ahead * Distance;

			UE_LOG(LogCantripRole, Warning,
				TEXT("BLINK %s: delivery gave no direction (is Delivery set to AimTrace?) ")
				TEXT("-- blinking straight ahead."), *GetName());
		}

		const FVector To = From + Toward.GetClampedToMaxSize(Distance);

		if (!bBlinkThroughObstacles)
		{
			// The conservative reading, kept for a level that needs it: swept, so she
			// stops at the first thing in the way.
			Avatar->SetActorLocation(To, /*bSweep*/ true);
			break;
		}

		FVector Landing;
		if (!FindBlinkLanding(From, To, Avatar, Landing))
		{
			// Every point along the line was solid. The casting succeeded and there was
			// nowhere on that heading to arrive -- she is left standing where she was,
			// which is the honest outcome and reads as the way being refused rather than
			// as the cantrip doing nothing.
			UE_LOG(LogCantripRole, Log,
				TEXT("BLINK %s: no room anywhere along %.0f units -- she stays put."),
				*GetName(), Distance);
			break;
		}

		// BEFORE the move, at the place she is leaving. Spawned at a world location rather
		// than attached to her, or it would travel with her and read as one puff jumping.
		if (BlinkDepartureEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World,
				BlinkDepartureEffect, From + BlinkEffectOffset, FRotator::ZeroRotator,
				FVector(BlinkEffectScale), /*bAutoDestroy*/ true);
		}

		if (BlinkDepartureSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, BlinkDepartureSound, From);
		}

		OnBlinkDeparted(From);

		// NOT swept. The sweep is the whole difference between this cantrip and running:
		// swept, she slides to the nearest wall and has paid Glamour to travel less far
		// than her own legs would. FindBlinkLanding has already proved she fits where she
		// is going, so the move can be direct.
		Avatar->SetActorLocation(Landing, /*bSweep*/ false, nullptr,
			ETeleportType::TeleportPhysics);

		if (BlinkArrivalEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World,
				BlinkArrivalEffect, Landing + BlinkEffectOffset, FRotator::ZeroRotator,
				FVector(BlinkEffectScale), /*bAutoDestroy*/ true);
		}

		if (BlinkArrivalSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, BlinkArrivalSound, Landing);
		}

		OnBlinkArrived(Landing);

		UE_LOG(LogCantripRole, Log,
			TEXT("BLINK %s: %d success(es), %.0f of %.0f units, %s -> %s"),
			*GetName(), Outcome.Successes, FVector::Dist(From, Landing), Distance,
			*From.ToCompactString(), *Landing.ToCompactString());
		break;
	}

	case ECantripMobilityMode::Portal:
		if (!PortalClass)
		{
			// Silent until now, and it cost a session: a clean five-success casting resolved,
			// spent its Glamour, and opened nothing, with not one line in the log to say why.
			// An unset class is a setup mistake rather than a runtime one, so it says so.
			UE_LOG(LogCantripRole, Warning,
				TEXT("PORTAL %s: PortalClass is not set -- nothing to open. ")
				TEXT("Set it in the ability's Class Defaults."), *GetName());
			break;
		}

		{
			UWorld* World = GetWorld();
			if (!World)
			{
				break;
			}

			// How far a surface leans from upright, as an angle. A vertical surface has a
			// horizontal normal, so the normal's Z is the sine of that lean: 0 on a true
			// wall, 1 on a floor or a ceiling.
			auto LeanOf = [](const FVector& N)
			{
				return FMath::RadiansToDegrees(
					FMath::Asin(FMath::Clamp(FMath::Abs(N.Z), 0.0f, 1.0f)));
			};

			// THE PROBE FIRST, and the aim ray only if it finds nothing.
			//
			// This was the other way round, and the log settled it: across eight castings the
			// aim ray hit a horizontal surface eight times, while the probe found a wall four
			// times out of four. That is not bad luck, it is the geometry of a third-person
			// camera -- the player looks slightly down at their own character, so the reticle
			// sits on the ground whatever they believe they are pointing at. Deriving a
			// doorway from that ray and then correcting it was solving the problem backwards.
			//
			// A door is in the wall in front of you. Look there first.
			FVector Facing = (Targets.Point - Avatar->GetActorLocation());
			Facing.Z = 0.0f;

			// Aimed straight up or straight down there is no horizontal direction to take
			// from the aim at all -- fall back to the way she is standing.
			if (!Facing.Normalize())
			{
				Facing = Avatar->GetActorForwardVector().GetSafeNormal2D();
			}

			const ACharacter* AsCharacter = Cast<ACharacter>(Avatar);
			const UCapsuleComponent* Capsule =
				AsCharacter ? AsCharacter->GetCapsuleComponent() : nullptr;

			const float ChestHeight = Capsule
				? (Capsule->GetScaledCapsuleHalfHeight() * 0.5f) : 45.0f;

			const FVector ProbeFrom =
				Avatar->GetActorLocation() + FVector(0.0f, 0.0f, ChestHeight);

			FCollisionQueryParams ProbeParams(
				SCENE_QUERY_STAT(PortalWallProbe), /*bTraceComplex*/ true, Avatar);

			// Level on purpose. The whole point is to ignore the camera's pitch, so a probe
			// that inherited any of it would find the same floor the aim ray does.
			FHitResult WallHit;
			const bool bProbeFoundWall =
				World->LineTraceSingleByChannel(WallHit, ProbeFrom,
					ProbeFrom + (Facing * PortalWallProbeRange), ECC_Visibility, ProbeParams)
				&& LeanOf(WallHit.ImpactNormal) <= PortalMaxSurfaceLean;

			FVector SurfacePoint;
			FVector SurfaceNormal;

			if (bProbeFoundWall)
			{
				SurfacePoint  = WallHit.ImpactPoint;
				SurfaceNormal = WallHit.ImpactNormal;

				UE_LOG(LogCantripRole, Log,
					TEXT("PORTAL %s: wall %.0f units ahead."),
					*GetName(), WallHit.Distance);
			}
			else if (LeanOf(Targets.Normal) <= PortalMaxSurfaceLean)
			{
				// Nothing level in front of her, but she IS pointing at something upright --
				// a wall across a courtyard, a face she has deliberately looked up at. The
				// aim ray keeps its place as the way to reach a wall the probe cannot.
				SurfacePoint  = Targets.Point;
				SurfaceNormal = Targets.Normal;

				UE_LOG(LogCantripRole, Log,
					TEXT("PORTAL %s: no wall within %.0f units; using the aimed surface at %s."),
					*GetName(), PortalWallProbeRange, *SurfacePoint.ToCompactString());
			}
			else
			{
				UE_LOG(LogCantripRole, Log,
					TEXT("PORTAL %s: no wall within %.0f units and the aim leans %.0f deg ")
					TEXT("from vertical (max %.0f) -- nothing opened."),
					*GetName(), PortalWallProbeRange, LeanOf(Targets.Normal),
					PortalMaxSurfaceLean);
				break;
			}

			// Into the surface, not at it. The normal points back out of whatever she is
			// opening, so its opposite is the direction a way through would run.
			const FVector Through = (-SurfaceNormal).GetSafeNormal();
			if (Through.IsNearlyZero())
			{
				break;
			}

			const float Thickness =
				ScaleByRoll(PortalBaseThickness, PortalThicknessPerSuccess, Outcome);

			FVector FarFace;
			if (!FindFarFace(SurfacePoint, Through, Thickness, Avatar, FarFace))
			{
				// Too thick, or solid all the way. The casting succeeded and the working
				// still found nowhere to open -- she reached for a way through and there
				// wasn't one. Left as a log rather than a failure, because the Glamour is
				// already spent and taking the cantrip back would be worse.
				UE_LOG(LogCantripRole, Log,
					TEXT("PORTAL %s: no far face within %.0f units -- nothing opened."),
					*GetName(), Thickness);
				break;
			}

			FActorSpawnParameters Params;
			Params.Owner = Avatar;
			Params.Instigator = Cast<APawn>(Avatar);
			Params.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			// The near door faces HER -- back out along the surface normal. The far door
			// faces the way she is going. That opposition is what the portal's own flip
			// expects, and it is what makes her walk out of the far side still travelling
			// in the direction she walked in.
			//
			// Built from X-and-Z rather than from the normal's own rotator, which would
			// carry the surface's lean into the door: on a wall tilted twenty degrees the
			// doorway would tilt with it, and a leaning door is one she cannot walk through
			// squarely. MakeFromXZ points it along the normal and then stands it up as near
			// to world vertical as that allows.
			const FTransform NearAt(
				FRotationMatrix::MakeFromXZ(SurfaceNormal, FVector::UpVector).Rotator(),
				SurfacePoint + (SurfaceNormal * PortalSurfaceOffset));
			const FTransform FarAt(
				FRotationMatrix::MakeFromXZ(Through, FVector::UpVector).Rotator(),
				FarFace + (Through * PortalSurfaceOffset));

			ACantripPortal* Near = World->SpawnActor<ACantripPortal>(PortalClass, NearAt, Params);
			ACantripPortal* Far  = World->SpawnActor<ACantripPortal>(PortalClass, FarAt, Params);

			if (!Near || !Far)
			{
				// One without the other is a door to nowhere. Tidy up rather than leave it
				// standing -- a half-made portal is more confusing than none.
				if (Near) { Near->Destroy(); }
				if (Far)  { Far->Destroy(); }
				break;
			}

			Near->LinkTo(Far);

			// Sized before either is told to open, so the growth animation runs at the final
			// scale and nothing is seen to resize itself.
			//
			// Zero means measure her. A door "just big enough to pass through" is a statement
			// about the caster, not a number a designer should have to keep in step with the
			// character's capsule by hand.
			float Clearance = PortalBaseClearance;

			if (Clearance <= 0.0f)
			{
				// Capsule is the one the probe above already found -- the same caster, and
				// there is nothing to gain from asking twice.
				//
				// A tenth over her full height. Exactly her height is a door she scrapes
				// through, and the margin is what makes it read as a way rather than a gap.
				Clearance = Capsule
					? (Capsule->GetScaledCapsuleHalfHeight() * 2.2f)
					: 200.0f;
			}

			Clearance += PortalClearancePerSuccess * FMath::Max(0, Outcome.Successes - 1);

			Near->SizeToClearance(Clearance);
			Far->SizeToClearance(Clearance);

			// The numbers the eye cannot check. Whether the pair is the right way round,
			// how thick the wall turned out to be against how thick the roll could pierce,
			// and how far apart the two doors ended up -- all invisible on screen, and all
			// the first thing worth knowing when a portal behaves oddly.
			UE_LOG(LogCantripRole, Log,
				TEXT("PORTAL %s: %d success(es), pierced %.0f of %.0f allowed, ")
				TEXT("near %s -> far %s, offset %.0f"),
				*GetName(), Outcome.Successes,
				FVector::Dist(SurfacePoint, FarFace), Thickness,
				*NearAt.GetLocation().ToCompactString(),
				*FarAt.GetLocation().ToCompactString(),
				PortalSurfaceOffset);

			// Only the near one is remembered. A dispel shuts it, and the far door unlinks
			// and closes on its own timer -- see ACantripPortal::CloseNow.
			SpawnedPortal = Near;

			// Both told the same lifetime, so the pair dies together. The roll buys standing
			// time and thickness; nothing about the door itself gets stronger.
			const float Lifetime =
				ScaleByRoll(PortalBaseLifetime, PortalLifetimePerSuccess, Outcome);

			Near->OpenFor(Lifetime);
			Far->OpenFor(Lifetime);
		}
		break;

	case ECantripMobilityMode::Bound:
		// Two numbers, and only the first came from the roll. The second is handed over
		// as a signed BONUS on an attribute that sits at 1.0, computed here so that a
		// designer editing a cantrip row types "0.2" and never has to think in deltas.
		ApplyEffectToTargets(Targets, MovementEffect, Distance, Applied,
			BoundFallScale - 1.0f);
		break;

	case ECantripMobilityMode::Flight:
		// Just the effect. The movement MODE is not this class's business, and trying to
		// make it so is what left Wind Runner unable to land: the ability set MOVE_Flying
		// on the way in and cleared it on the way out, which meant the one path that
		// mattered -- coming down -- depended on a list of characters surviving inside an
		// ability instance until its duration expired, and on the mode still being
		// exactly MOVE_Flying when it got there.
		//
		// ACHANGELINGCharacter::RefreshLocomotion now derives the mode from
		// FallSpeedMultiplier every tick instead. Taking the effect off is therefore the
		// whole of landing her, and it works identically whether the cantrip expired, was
		// cancelled, was dispelled, or was destroyed outright.
		ApplyEffectToTargets(Targets, MovementEffect, Distance, Applied);
		break;

	case ECantripMobilityMode::Speed:
	default:
		// Changes how the body moves rather than where it is, so it is an effect that
		// lasts as long as the cantrip does.
		ApplyEffectToTargets(Targets, MovementEffect, Distance, Applied);
		break;
	}
}

void UGA_MobilityCantrip::RevertCantrip()
{
	// This is the whole of landing a flier. Removing the effect returns
	// FallSpeedMultiplier to 1, and the character's own RefreshLocomotion takes her out
	// of MOVE_Flying on the next tick because she is no longer weightless. Nothing here
	// touches the movement mode, and nothing here needs to remember who was flying.
	RemoveAppliedEffects(Applied);

	// Asked to shut, not destroyed. A door that blinks out of existence reads as a bug;
	// one that draws itself closed reads as the magic being unmade -- which is what a
	// dispel actually is. Usually a no-op: on an Instant row this runs long after the
	// door has already closed and cleaned itself up, and the weak pointer is null.
	if (ACantripPortal* Portal = SpawnedPortal.Get())
	{
		Portal->CloseNow();
	}
	SpawnedPortal = nullptr;
}

void UGA_MobilityCantrip::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// A net, not the normal path. On a Sustained row RevertCantrip has already run and
	// emptied Applied, so this finds nothing and costs nothing.
	//
	// It exists for every OTHER way this ability can end while still holding an infinite
	// effect: a misconfigured row, a cancellation that bypasses EndCantrip, the avatar
	// dying mid-flight. The consequence of missing one of those is a character who cannot
	// come down, which no amount of correct code elsewhere can recover.
	if (Applied.Num() > 0)
	{
		UE_LOG(LogCantripRole, Warning,
			TEXT("Mobility cantrip '%s' ended still holding %d movement effect(s); "
			     "removing them here. RevertCantrip did not run -- check the row's Kind."),
			*GetCantripRowName().ToString(), Applied.Num());

		RemoveAppliedEffects(Applied);
	}

	// Super LAST, and it is not optional: the base releases the movement lock, drops the
	// aim stance and takes the casting tag off. Skipping it leaves a caster rooted and
	// unable to cast anything ever again.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ── Divination ──────────────────────────────────────────────────────────────

void UGA_DivinationCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	const float Radius = ScaleByRoll(BaseSightRadius, SightRadiusPerSuccess, Outcome);

	FCantripTargets Found = Targets;

	if (bRevealsHidden)
	{
		// Gathered around what the cantrip was aimed at rather than around the caster:
		// scrying looks somewhere, and the somewhere is the target point.
		const float Saved = EffectRadius;
		const_cast<UGA_DivinationCantrip*>(this)->EffectRadius = Radius;
		GatherArea(Targets.Point, Found);
		const_cast<UGA_DivinationCantrip*>(this)->EffectRadius = Saved;
	}

	if (RevealEffect)
	{
		TArray<FActiveGameplayEffectHandle> Handles;
		ApplyEffectToTargets(Found, RevealEffect, static_cast<float>(Outcome.Degree), Handles);
	}

	UE_LOG(LogCantripRole, Log, TEXT("DIVINE %s: %d found within %.0f, degree %d."),
		*GetName(), Found.Actors.Num(), Radius, Outcome.Degree);

	// The answer goes to Blueprint. Nothing here can know whether it becomes a marker,
	// a journal line or a piece of dialogue.
	OnDivined(Found, static_cast<int32>(Outcome.Degree), Radius);
}

// ── Illusion ────────────────────────────────────────────────────────────────

void UGA_IllusionCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	if (Mode == ECantripIllusionMode::Phantasm && PhantasmClass)
	{
		if (UWorld* World = GetWorld())
		{
			FActorSpawnParameters Params;
			Params.Owner = GetAvatarActorFromActorInfo();
			Params.Instigator = Cast<APawn>(Params.Owner);
			Params.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			SpawnedPhantasm = World->SpawnActor<AActor>(PhantasmClass,
				FTransform(FRotator::ZeroRotator, Targets.Point), Params);
		}
		return;
	}

	// Conceal and Disguise both work on a subject rather than making something new, so
	// both are the same shape: an effect carrying the tag perception checks against.
	ApplyEffectToTargets(Targets, IllusionEffect,
		static_cast<float>(FMath::Max(1, Outcome.Successes)), Applied);
}

void UGA_IllusionCantrip::RevertCantrip()
{
	RemoveAppliedEffects(Applied);

	if (AActor* Phantasm = SpawnedPhantasm.Get())
	{
		Phantasm->Destroy();
	}
	SpawnedPhantasm = nullptr;
}

// ── Transformation ──────────────────────────────────────────────────────────

void UGA_TransformationCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	Transformed.Reset();
	OriginalMeshes.Reset();
	OriginalScales.Reset();

	for (const TObjectPtr<AActor>& Target : Targets.Actors)
	{
		if (!Target)
		{
			continue;
		}

		ACharacter* Character = Cast<ACharacter>(Target);
		USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;

		// Captured BEFORE anything changes, and captured per target, because one cantrip
		// may transform several things that were not alike to begin with.
		Transformed.Add(Target);
		OriginalMeshes.Add(Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr);
		OriginalScales.Add(Target->GetActorScale3D());

		if (Mesh && NewMesh)
		{
			Mesh->SetSkeletalMesh(NewMesh);

			if (NewAnimClass)
			{
				Mesh->SetAnimInstanceClass(NewAnimClass);
			}
		}

		if (!FMath::IsNearlyEqual(ScaleMultiplier, 1.0f))
		{
			Target->SetActorScale3D(Target->GetActorScale3D() * ScaleMultiplier);
		}
	}

	ApplyEffectToTargets(Targets, FormEffect,
		static_cast<float>(FMath::Max(1, Outcome.Successes)), Applied);
}

void UGA_TransformationCantrip::RevertCantrip()
{
	RemoveAppliedEffects(Applied);

	// A permanent shape is the whole point of Reweaving. Everything else must go back,
	// or a changeling who blinks out mid-transformation stays a sparrow.
	if (bPermanent)
	{
		Transformed.Reset();
		OriginalMeshes.Reset();
		OriginalScales.Reset();
		return;
	}

	for (int32 Index = 0; Index < Transformed.Num(); ++Index)
	{
		AActor* Target = Transformed[Index].Get();
		if (!Target)
		{
			continue;
		}

		if (OriginalScales.IsValidIndex(Index))
		{
			Target->SetActorScale3D(OriginalScales[Index]);
		}

		if (OriginalMeshes.IsValidIndex(Index) && OriginalMeshes[Index])
		{
			if (ACharacter* Character = Cast<ACharacter>(Target))
			{
				if (USkeletalMeshComponent* Mesh = Character->GetMesh())
				{
					Mesh->SetSkeletalMesh(OriginalMeshes[Index]);
				}
			}
		}
	}

	Transformed.Reset();
	OriginalMeshes.Reset();
	OriginalScales.Reset();
}

// ── Creation ────────────────────────────────────────────────────────────────

UGA_CreationCantrip::UGA_CreationCantrip()
{
	// Rooted by default, unlike every other role.
	//
	// Creation is the one family where standing still is not a cost imposed on the
	// cantrip but the substance of it: she is holding a shape in her hands until it
	// agrees to exist. Eldritch Prime, Effigy, Manifest, Sculpt, Flicker-Flies -- all
	// of them look like nothing is happening if she can stroll away mid-conjuring.
	//
	// A default rather than a rule, so a Creation cantrip that genuinely should be cast
	// on the move can turn it off on its own Blueprint without needing a new role.
	bLockMovementWhileCasting = true;
}

void UGA_CreationCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();

	if (!World || !Avatar || !CreatedClass)
	{
		return;
	}

	const int32 Extra = FMath::Max(0, Outcome.Successes - 1);
	const int32 Count = FMath::Clamp(BaseCount + (CountPerSuccess * Extra), 1, MaxCount);
	const float Lifetime = BaseLifetime + (LifetimePerSuccess * Extra);

	Created.Reset();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FActorSpawnParameters Params;
		Params.Owner = Avatar;
		Params.Instigator = Cast<APawn>(Avatar);

		// Always spawn. One refused for overlapping a rock is a cantrip that silently
		// did nothing, which reads as a broken key rather than as a bad roll.
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Spawn = World->SpawnActor<AActor>(
			CreatedClass, PlacementFor(Index, Count, Targets.Point), Params);

		if (!Spawn)
		{
			continue;
		}

		// Zero means it stays until something else removes it, expressed by never
		// setting a lifespan rather than by a sentinel nobody has to remember.
		if (Lifetime > 0.0f)
		{
			Spawn->SetLifeSpan(Lifetime);
		}

		Created.Add(Spawn);
		OnCreated(Spawn, Index, Outcome);
	}

	UE_LOG(LogCantripRole, Log, TEXT("CREATE %s: %d x %s%s."), *GetName(), Created.Num(),
		*GetNameSafe(CreatedClass),
		Lifetime > 0.0f ? *FString::Printf(TEXT(" for %.0fs"), Lifetime) : TEXT(""));
}

void UGA_CreationCantrip::RevertCantrip()
{
	if (!bBanishOnEnd)
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& Spawn : Created)
	{
		if (AActor* Alive = Spawn.Get())
		{
			Alive->Destroy();
		}
	}

	Created.Reset();
}

FTransform UGA_CreationCantrip::PlacementFor(int32 Index, int32 Count,
	const FVector& Origin) const
{
	FVector Location = Origin;

	switch (Placement)
	{
	case ECantripPlacement::Ring:
	{
		const float Angle = (2.0f * PI * Index) / FMath::Max(1, Count);
		Location += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * PlacementRadius;
		break;
	}
	case ECantripPlacement::Scattered:
	{
		// Square-rooted so points spread evenly over the AREA. A plain random radius
		// crowds them toward the middle, which does not look scattered.
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Dist = PlacementRadius * FMath::Sqrt(FMath::FRand());
		Location += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Dist;
		break;
	}
	case ECantripPlacement::Stacked:
	default:
		break;
	}

	if (bDropToGround)
	{
		if (const UWorld* World = GetWorld())
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(CantripCreate), false,
				GetAvatarActorFromActorInfo());

			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 200.0f),
				Location - FVector(0, 0, 400.0f), ECC_Visibility, Params))
			{
				Location = Hit.ImpactPoint;
			}
		}
	}

	return FTransform(FRotator::ZeroRotator, Location);
}

// ── Binding ─────────────────────────────────────────────────────────────────

void UGA_BindingCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	const float Weight = ScaleByRoll(BaseWeight, WeightPerSuccess, Outcome);

	TArray<FActiveGameplayEffectHandle> Handles;
	ApplyEffectToTargets(Targets, OathEffect, Weight, Handles);

	// BreachEffect is deliberately not applied. An oath is broken later -- possibly days
	// later, possibly nowhere near the caster -- so whatever watches the oath applies it
	// then. This ability is long finished by the time it matters.
	UE_LOG(LogCantripRole, Log, TEXT("OATH %s: %d bound, weight %.1f%s."),
		*GetName(), Handles.Num(), Weight,
		BreachEffect ? TEXT("") : TEXT(" (no breach effect set)"));

	OnOathSworn(Targets, static_cast<int32>(Outcome.Degree));
}

// ── Utility ─────────────────────────────────────────────────────────────────

void UGA_UtilityCantrip::ApplyCantrip(const FCantripOutcome& Outcome,
	const FCantripSpec& Spec, const FCantripTargets& Targets)
{
	const float Magnitude = ScaleByRoll(BaseMagnitude, MagnitudePerSuccess, Outcome);

	ApplyEffectToTargets(Targets, UtilityEffect, Magnitude, Applied);

	if (SpawnedClass)
	{
		if (UWorld* World = GetWorld())
		{
			FActorSpawnParameters Params;
			Params.Owner = GetAvatarActorFromActorInfo();
			Params.Instigator = Cast<APawn>(Params.Owner);
			Params.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			Spawned = World->SpawnActor<AActor>(SpawnedClass,
				FTransform(FRotationMatrix::MakeFromZ(Targets.Normal).Rotator(),
					Targets.Point), Params);
		}
	}
}

void UGA_UtilityCantrip::RevertCantrip()
{
	RemoveAppliedEffects(Applied);

	if (bDespawnOnEnd)
	{
		if (AActor* Alive = Spawned.Get())
		{
			Alive->Destroy();
		}
	}

	Spawned = nullptr;
}

bool UGA_MobilityCantrip::FindBlinkLanding(const FVector& From, const FVector& To,
	const AActor* Caster, FVector& OutLanding) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const ACharacter* AsCharacter = Cast<const ACharacter>(Caster);
	const UCapsuleComponent* Capsule =
		AsCharacter ? AsCharacter->GetCapsuleComponent() : nullptr;

	if (!Capsule)
	{
		// Not a character, so there is no capsule to fit and nothing to test. Anything
		// else that can be blinked is taken at its word.
		OutLanding = To;
		return true;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Body = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	// COMPLEX. Simple collision is exactly what this project's Nanite architecture does not
	// reliably have -- the same reason the stairs and bridges needed Use Complex As Simple
	// to be walkable. A ground trace against simple collision here missed the floor
	// entirely, which left every candidate buried and every blink travelling zero units.
	FCollisionQueryParams Params(
		SCENE_QUERY_STAT(BlinkLanding), /*bTraceComplex*/ true, Caster);

	// Lifted to CAPSULE-CENTRE height before anything else.
	//
	// Targets.Point is a point on a surface -- the floor, usually -- while From is her
	// capsule's centre, a half-height above the floor she is standing on. Interpolating
	// between the two without this puts every intermediate candidate at ankle level, which
	// is a body half sunk into the ground and reads to the overlap test as solid rock.
	const FVector Standing = To + FVector(0.0f, 0.0f, HalfHeight);

	const float Span = FVector::Dist(From, Standing);
	const FVector Backwards = (Span > KINDA_SMALL_NUMBER)
		? ((From - Standing) / Span) : FVector::ZeroVector;

	const int32 Attempts = FMath::Max(1, BlinkLandingAttempts);

	for (int32 Step = 0; Step <= Attempts; ++Step)
	{
		// From the target back toward her, so a blocked blink still travels as far as it
		// can. Forward from her would give up the moment anything was in the way and land
		// her a pace from where she started.
		FVector Candidate = Standing + (Backwards * (Span * Step / Attempts));

		// Dropped to whatever is beneath. An aim ray that struck a vertical face resolves
		// to a point ON that face, which is mid-air; arriving there means arriving falling.
		FHitResult Ground;
		if (World->LineTraceSingleByChannel(Ground,
			Candidate,
			Candidate - FVector(0.0f, 0.0f, BlinkGroundSearch + HalfHeight),
			ECC_Visibility, Params))
		{
			// A hair above the surface, or the capsule rests exactly touching it and the
			// very first overlap test reports her as embedded in the floor.
			Candidate = Ground.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
		}

		// Pawn rather than Visibility: this asks "does a BODY fit here", and the two
		// channels disagree about plenty -- trigger volumes, foliage, anything set to
		// ignore pawns but block sight.
		if (!World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity,
			ECC_Pawn, Body, Params))
		{
			OutLanding = Candidate;
			return true;
		}

		UE_LOG(LogCantripRole, Verbose,
			TEXT("BLINK: candidate %d of %d at %s is solid."),
			Step, Attempts, *Candidate.ToCompactString());
	}

	return false;
}

bool UGA_MobilityCantrip::FindFarFace(const FVector& Entry, const FVector& Direction,
	float Thickness, const AActor* Caster, FVector& OutExit) const
{
	const UWorld* World = GetWorld();
	if (!World || Thickness <= 0.0f)
	{
		return false;
	}

	// Backwards, from beyond the wall to the face she is standing at. This is the only
	// way to find the far side of something: a trace that STARTS inside a collider is
	// already in it and never reports leaving, so tracing forward from the entry point
	// finds nothing at all and a thick wall is indistinguishable from thin air.
	const FVector Beyond = Entry + (Direction * Thickness);

	FCollisionQueryParams Query(SCENE_QUERY_STAT(CantripPortalFarFace), /*bTraceComplex*/ true);
	Query.AddIgnoredActor(Caster);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Beyond, Entry, ECC_WorldStatic, Query))
	{
		// Nothing between the far point and the entry face. The obstacle is thicker than
		// Thickness -- the backwards trace began inside it too, and so found nothing.
		return false;
	}

	// Sanity, not paranoia. A hit essentially ON the entry face means the trace crossed the
	// whole span and came back out where it went in: she is aimed at a surface with no
	// second side within reach, and opening a door there would put the exit on top of the
	// entrance.
	const float Span = FVector::DotProduct(Hit.ImpactPoint - Entry, Direction);
	if (Span <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutExit = Hit.ImpactPoint;
	return true;
}
