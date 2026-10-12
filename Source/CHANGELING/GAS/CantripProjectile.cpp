// CantripProjectile.cpp

#include "CantripProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "CantripBurnSubsystem.h"
#include "ChangelingGameplayTags.h"
#include "Curves/CurveFloat.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripProjectile, Log, All);

ACantripProjectile::ACantripProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// The convention the whole cantrip system already uses: every role hands its
	// magnitude to an effect on this tag, and every effect asset reads it from there.
	// Defaulted rather than left empty so a blast effect works the moment it is assigned.
	BlastMagnitudeTag = ChangelingTags::Cantrip_Data_Magnitude;

	// Being a Pawn is what will let a changeling ride their own fireball one day, but
	// until someone deliberately possesses it, it is scenery that moves. Left at the
	// defaults a spawned Pawn can pick up a controller and take the camera with it --
	// the player watching their spell fly away from behind it instead of watching
	// themselves throw it.
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;
	bFindCameraComponentWhenViewTarget = false;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);

	Capsule->InitCapsuleSize(22.0f, 44.0f);

	// Query only, and no simulated physics. The projectile is spawned inside the
	// caster's own collision, and a rigid body resolving that overlap throws the player
	// forward with their own fireball.
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionObjectType(ECC_WorldDynamic);
	Capsule->SetCollisionResponseToAllChannels(ECR_Block);

	// The caster is standing inside the throw. Colliding with them would stop the
	// fireball dead in the hand that threw it.
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// And the camera has to pass through it as well. A third-person spring arm probes for
	// geometry between the camera and the character and pulls in when it finds any -- so
	// a fireball leaving the hand is a solid object sweeping straight through that probe,
	// and the view snaps into the character's own capsule as it goes past.
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Capsule;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bShouldBounce = false;
	Movement->bRotationFollowsVelocity = false;
	Movement->bAutoActivate = false;

	// Defaults to TRUE, which would take the world-space velocity handed to Launch and
	// rotate it by the actor's orientation a second time -- a throw already pointing the
	// right way sent somewhere else entirely.
	Movement->bInitialVelocityInLocalSpace = false;

	// Zero means "no limit" in this component. Left at its default of 0 alongside an
	// explicit Velocity, the projectile is capped at nothing and never moves.
	Movement->InitialSpeed = 0.0f;
	Movement->MaxSpeed = 0.0f;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Capsule);
	Light->SetIntensity(4000.0f);
	Light->SetAttenuationRadius(600.0f);
	Light->SetLightColor(FLinearColor(1.0f, 0.6f, 0.25f));
	Light->SetCastShadows(false);

	Audio = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	Audio->SetupAttachment(Capsule);
	Audio->bAutoActivate = false;

	// Defaulted so a blast effect authored against the standard tag works the moment it
	// is assigned. Left blank, the magnitude silently never arrives and the effect
	// applies at zero -- which looks exactly like the damage not being wired at all.
	BlastMagnitudeTag = ChangelingTags::Cantrip_Data_BlastMagnitude;
}

void ACantripProjectile::BeginPlay()
{
	Super::BeginPlay();

	PreviousLocation = GetActorLocation();

	// Bound here rather than in Launch, because a projectile placed in the level by hand
	// -- which is how the detonation gets tuned without casting anything -- never goes
	// through Launch at all and would otherwise never explode.
	if (Movement)
	{
		Movement->OnProjectileStop.AddDynamic(this, &ACantripProjectile::OnProjectileStopped);
	}

	if (Audio && FlightSound)
	{
		Audio->SetSound(FlightSound);
		Audio->Play();
	}
}

void ACantripProjectile::Launch(UNiagaraComponent* AdoptedEffect, const FVector& Direction,
	float InIntensity, float ExtraSpeed)
{
	Intensity = FMath::Max(0.0f, InIntensity);

	if (AdoptedEffect)
	{
		// Detached from the hand and re-parented rather than destroyed and respawned.
		// The particles already in the air belong to the gathering flame, and killing
		// the system to make a new one throws them away -- the fireball would blink
		// rather than leave.
		Effect = AdoptedEffect;
		Effect->DetachFromComponent(
			FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
		Effect->AttachToComponent(Capsule,
			FAttachmentTransformRules::SnapToTargetIncludingScale);
	}
	else if (FlightEffect)
	{
		// Nothing handed over, so the projectile brings its own flame. This is the
		// normal path now: the ability's wind-up stays on the hand and dies back.
		Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			FlightEffect, Capsule, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTargetIncludingScale, /*bAutoDestroy*/ false,
			/*bAutoActivate*/ true);
	}

	// Told to ignore the caster outright, not just their collision channel. The mesh and
	// the capsule can sit on different channels, and a fireball that clips the thrower's
	// shoulder on the way past stops dead a foot from their face.
	if (Capsule)
	{
		if (AActor* Thrower = GetInstigator() ? Cast<AActor>(GetInstigator()) : GetOwner())
		{
			Capsule->IgnoreActorWhenMoving(Thrower, true);
		}
	}

	// Oriented before it moves, so the first frame already looks right. Waiting for Tick
	// to align it means one frame of the effect pointing wherever the spawn rotation
	// happened to leave it, which at 2000 units a second is a visible flick.
	SetActorRotation(OrientationFor(Direction));

	if (Movement)
	{
		// The arm's own speed is added rather than replacing the roll's contribution: a
		// hard-flung weak cantrip should still be a weak cantrip travelling fast.
		const float Speed =
			BaseSpeed + (SpeedPerIntensity * Intensity) + FMath::Max(0.0f, ExtraSpeed);

		Movement->Velocity = Direction.GetSafeNormal() * Speed;

		// Kept so a spawn overlap can be shrugged off and the throw resumed at the speed
		// it was thrown with, rather than at whatever the stop left behind (zero).
		LaunchVelocity = Movement->Velocity;

		// Raised only if the throw needs more than the current ceiling, never lowered to
		// it. Setting MaxSpeed = Speed on every launch means a fireball can never be
		// sped up afterwards, which a steered one would need.
		Movement->MaxSpeed = FMath::Max(Movement->MaxSpeed, Speed);
		Movement->Activate();

		/**
		 * Reports where it actually got to, half a second on.
		 *
		 * "It launched" and "it moved" are different claims, and the log has only ever
		 * been able to make the first. A projectile that never stops looks identical in
		 * the log to one that is flying perfectly -- both simply have no DETONATE line --
		 * so this closes the gap by asking the actor itself, once, whether it travelled.
		 */
		LaunchLocation = GetActorLocation();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FlightProbeTimer, this,
				&ACantripProjectile::ReportFlightProgress, 0.5f, false);
		}

		UE_LOG(LogCantripProjectile, Log,
			TEXT("LAUNCH %s: base=%.0f + intensity %.1f x %.1f + hand %.0f = %.0f, "
				 "velocity %s"),
			*GetName(), BaseSpeed, Intensity, SpeedPerIntensity,
			FMath::Max(0.0f, ExtraSpeed), Speed, *Movement->Velocity.ToCompactString());
	}

	PushIntensity();
	bLaunched = true;

	// Started here rather than in BeginPlay: an unlaunched projectile has no intensity
	// to lose, and ageing one that is still sitting in a hand would burn it out before
	// it was thrown.
	if (UWorld* World = GetWorld())
	{
		LaunchTime = World->GetTimeSeconds();

		World->GetTimerManager().SetTimer(AgeTimer,
			FTimerDelegate::CreateUObject(this, &ACantripProjectile::AgeProjectile),
			AgeInterval, true);
	}
}

void ACantripProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector Current = GetActorLocation();
	const FVector Travelled = Current - PreviousLocation;

	// Below this the direction is noise rather than movement, and feeding noise into a
	// look-at rotation makes the capsule twitch while the fireball hangs still.
	if (Travelled.SizeSquared() > 1.0f)
	{
		const FRotator Target = OrientationFor(Travelled);
		const FRotator Aligned =
			FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, AlignSpeed);

		SetActorRotation(Aligned);
	}

	PreviousLocation = Current;
}

FRotator ACantripProjectile::OrientationFor(const FVector& Dir) const
{
	const FVector Unit = Dir.GetSafeNormal();
	if (Unit.IsNearlyZero())
	{
		return GetActorRotation();
	}

	// MakeFrom* rather than Rotation(), which only ever builds an X-forward basis.
	// Each of these handles the degenerate case -- a direction parallel to the chosen
	// up reference -- by picking a different reference, so straight up and straight
	// down are safe.
	switch (ForwardAxis)
	{
	case EAxis::Y: return FRotationMatrix::MakeFromY(Unit).Rotator();
	case EAxis::Z: return FRotationMatrix::MakeFromZ(Unit).Rotator();
	default:       return Unit.Rotation();
	}
}

void ACantripProjectile::ReportFlightProgress()
{
	const float Travelled = FVector::Dist(LaunchLocation, GetActorLocation());
	const FVector Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;

	/**
	 * Everything that has to be true for a projectile to move, printed together.
	 *
	 * "It has velocity and a live component and does not move" is not a diagnosis, it is
	 * the absence of one -- and it has now been reached twice by reasoning about the
	 * movement component from the outside. So each precondition is asked directly:
	 * whether the component is active, whether its tick is even enabled, what it is
	 * actually driving, and whether the capsule is still stuck in something.
	 */
	const bool bActive = Movement && Movement->IsActive();
	const bool bTicking = Movement && Movement->IsComponentTickEnabled();
	const bool bSimPhys = Capsule && Capsule->IsSimulatingPhysics();

	TArray<AActor*> Ignored;
	if (Capsule)
	{
		Ignored = Capsule->CopyArrayOfMoveIgnoreActors();
	}

	UE_LOG(LogCantripProjectile, Log,
		TEXT("FLIGHT %s: %.0f units in 0.5s, now at %s, velocity %s | updated=%s active=%s "
			 "tick=%s simphys=%s ignoring=%d"),
		*GetName(), Travelled, *GetActorLocation().ToCompactString(),
		*Velocity.ToCompactString(),
		(Movement && Movement->UpdatedComponent)
			? *GetNameSafe(Movement->UpdatedComponent) : TEXT("NULL"),
		bActive ? TEXT("yes") : TEXT("NO"),
		bTicking ? TEXT("yes") : TEXT("NO"),
		bSimPhys ? TEXT("yes") : TEXT("no"),
		Ignored.Num());
}

void ACantripProjectile::OnProjectileStopped(const FHitResult& Hit)
{
	if (!bExplodeOnImpact || bDetonated)
	{
		return;
	}

	/**
	 * A stop with nothing travelled is a SPAWN overlap, not an impact.
	 *
	 * The fireball leaves a fist that is standing on a floor, beside a wall, under a
	 * ceiling -- it is always born inside something. When it is, the movement component's
	 * first sweep reports a blocking hit at zero distance with bStartPenetrating set, and
	 * taking that at face value detonates the cantrip in her hand every single time. The
	 * symptom is a fireball that appears to have no projectile at all: five casts, five
	 * explosions, each exactly where she was standing.
	 *
	 * So it is pushed clear along the hit normal and sent on its way instead. Nothing is
	 * added to the ignore list, because the floor it started inside is very often the
	 * floor it should land on a moment later -- ignoring it would trade a fireball that
	 * cannot leave for one that can never arrive.
	 *
	 * Bounded, because a projectile spawned deep inside geometry could otherwise be
	 * nudged around forever. After the allowance is spent the hit is taken as real, which
	 * at least detonates rather than leaving something inert stuck in a wall.
	 */
	if (Hit.bStartPenetrating && SpawnOverlapNudges < MaxSpawnOverlapNudges)
	{
		++SpawnOverlapNudges;

		// At Log rather than Verbose: this fires on a path that is meant to be rare, and
		// when it is NOT rare it is the whole explanation for a fireball that will not
		// leave. Something that only shows up under a console command is no use for a
		// symptom that presents as "nothing happens".
		UE_LOG(LogCantripProjectile, Log,
			TEXT("ENCLOSED %s (%d of %d): born inside '%s' (component '%s'), penetration %.1f -- "
				 "ignoring it for this throw and continuing."),
			*GetName(), SpawnOverlapNudges, MaxSpawnOverlapNudges,
			*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
			Hit.PenetrationDepth);

		/**
		 * Ignored, not shoved.
		 *
		 * Teleporting out along the normal assumes the thing it woke up inside is thin --
		 * a floor it is resting on, a wall it is touching. When that assumption fails the
		 * arithmetic is brutal: a penetration of 358 units against a +Z normal threw the
		 * fireball 358 units straight up, where it hung above the caster's head having
		 * never moved horizontally at all.
		 *
		 * If the caster is standing INSIDE a volume, that volume is not something their
		 * fireball can meaningfully hit from in there anyway -- so it is excluded for this
		 * projectile's lifetime and the throw simply carries on through it. Whatever is
		 * downrange still stops it.
		 */
		if (AActor* Enclosing = Hit.GetActor())
		{
			Capsule->IgnoreActorWhenMoving(Enclosing, true);
		}

		if (Movement)
		{
			/**
			 * The updated component has to be handed back BEFORE the velocity.
			 *
			 * StopSimulating nulls UpdatedComponent and only then broadcasts OnProjectileStop
			 * (ProjectileMovementComponent.cpp:546), so by the time this runs the movement
			 * component has nothing left to move. Restoring the velocity alone wrote a number
			 * into a component pointed at nothing: every fireball was pushed clear of the
			 * floor and then hung motionless in the air exactly where it was born, piling up
			 * one per cast.
			 */
			Movement->SetUpdatedComponent(Capsule);

			/**
			 * The tick has to be switched back on by hand, and Activate() will not do it.
			 *
			 * StopSimulating nulls UpdatedComponent, and clearing that disables the
			 * component's tick -- but it leaves bIsActive TRUE. So Activate() takes its
			 * early-out (ShouldActivate() is !IsActive(), which is false) and returns
			 * having done nothing, tick included. The result was a projectile that passed
			 * every check worth logging -- component live, flagged active, not in physics,
			 * ignoring the right actors, velocity set -- and never moved a unit, because
			 * nothing was updating it.
			 *
			 * Activate(true) forces past that early-out; the explicit enable makes the
			 * intent legible rather than relying on it as a side effect.
			 */
			Movement->SetComponentTickEnabled(true);
			Movement->Activate(/*bReset*/ true);

			// OnProjectileStop has already zeroed the velocity, so it is restored from
			// what the launch asked for rather than from what is left. Last, so nothing
			// above can overwrite it.
			Movement->Velocity = LaunchVelocity;
			Movement->UpdateComponentVelocity();
		}

		return;
	}

	Detonate(Hit);
}

void ACantripProjectile::Detonate(const FHitResult& Hit)
{
	bDetonated = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Captured before anything else touches Intensity. The ageing timer is still running
	// at this instant, and everything the blast is sized from -- radius, ray count, the
	// magnitude handed to the effect -- has to come from the same number, or a detonation
	// that straddles a tick scorches at one strength and damages at another.
	IntensityAtImpact = Intensity;

	// The flame's own position, not the contact point.
	//
	// The player has been watching the fireball travel, and the explosion has to start
	// exactly where that fireball was -- one thing vanishing as the other appears. The
	// impact point is a few centimetres away on the surface itself, which is close enough
	// to look correct and far enough to put the burst rays' origin flush against a wall,
	// where half of them start co-planar with it and find nothing.
	const FVector Origin = Effect
		? Effect->GetComponentLocation()
		: (Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint));

	const float Radius = BaseBlastRadius + (BlastRadiusPerIntensity * IntensityAtImpact);

	// Stopped before the blast is spawned. The projectile is about to become a fireball
	// standing still at the point of impact, and a live capsule there keeps registering
	// against the surface it already hit.
	if (Movement)
	{
		Movement->StopMovementImmediately();
		Movement->Deactivate();
	}

	if (Capsule)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (Effect)
	{
		// Zeroing the intensity is what the flight system reads as "this is over" -- it
		// is the same parameter the whole flight has been driven from, and dropping it
		// ages the trail out fast instead of leaving it drawing itself out of a fireball
		// that has already burst.
		if (!IntensityParameter.IsNone())
		{
			Effect->SetVariableFloat(IntensityParameter, 0.0f);
		}

		// Ageing what exists is not the same as refusing to make more. Without this the
		// trail keeps spawning for as long as the actor survives its own explosion.
		if (!FlightSpawnParameter.IsNone())
		{
			Effect->SetVariableBool(FlightSpawnParameter, false);
		}
		else
		{
			// No agreed parameter, so the blunt instrument: stop the emitters outright.
			// Particles already in the air still finish their own lifetimes.
			Effect->Deactivate();
		}
	}

	if (ExplosionEffect)
	{
		// Unattached and world-placed: the projectile is about to be destroyed, and an
		// explosion parented to it dies with it halfway through its own animation.
		UNiagaraComponent* Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, ExplosionEffect, Origin,
			FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator(),
			FVector(FMath::Max(0.1f, BaseEffectScale + (ScalePerIntensity * IntensityAtImpact))),
			/*bAutoDestroy*/ true);

		// The roll follows the fire all the way to the end. Scale alone would make a weak
		// casting a small copy of a strong one; handing the number over lets the system
		// spawn fewer puffs as well as smaller ones, which is the difference between a
		// scaled-down explosion and a feebler one.
		if (Burst && !ExplosionIntensityParameter.IsNone())
		{
			Burst->SetVariableFloat(ExplosionIntensityParameter, IntensityAtImpact);
		}
	}

	if (Audio)
	{
		Audio->Stop();
	}

	if (ExplosionSound)
	{
		// Attached rather than played at a location, so it keeps the projectile's own
		// attenuation as the volume envelope runs. Played flat, a distant blast is as
		// loud as one at the player's feet.
		ExplosionAudio = UGameplayStatics::SpawnSoundAttached(ExplosionSound, Capsule);

		if (ExplosionAudio)
		{
			ExplosionAudio->SetVolumeMultiplier(ExplosionVolumeScale);
		}
	}

	if (Light)
	{
		FlashPeakIntensity = FlashIntensityPerIntensity * IntensityAtImpact;

		Light->SetIntensity(FlashPeakIntensity);
		Light->SetAttenuationRadius(FMath::Max(Light->AttenuationRadius, Radius * 3.0f));
	}

	DetonationTime = World->GetTimeSeconds();

	// One timer drives both envelopes down. They are different lengths on purpose -- the
	// flash is gone long before the boom is -- so it runs until the longer of the two has
	// finished and then stops itself.
	World->GetTimerManager().SetTimer(DetonationTimer,
		FTimerDelegate::CreateUObject(this, &ACantripProjectile::TickDetonation),
		AgeInterval, true);

	const int32 Rays = FMath::Max(1, FMath::CeilToInt(BurstRaysPerIntensity * IntensityAtImpact));

	int32 Marks = 0;
	bool bBurnReady = false;

	if (UCantripBurnSubsystem* Burn = World->GetSubsystem<UCantripBurnSubsystem>())
	{
		// Same population the flamethrower paints into, so a wall the player has been
		// scorching by hand deepens where the blast catches it rather than growing a
		// second, independently-budgeted set of marks on top.
		bBurnReady = Burn->IsConfigured();
		Marks = Burn->PaintBurst(Origin, Radius, Rays, BurstMarkStrength, this);
	}

	// Reported, because the ray COUNT is decided here and the marks are decided over
	// there. Logging only the rays claimed twenty scorch marks on every detonation while
	// the subsystem was painting none of them.
	if (!bBurnReady)
	{
		UE_LOG(LogCantripProjectile, Warning,
			TEXT("DETONATE %s: burn subsystem unconfigured -- no scorch marks."), *GetName());
	}

	ApplyBlastEffect(Origin, Radius);

	UE_LOG(LogCantripProjectile, Log,
		TEXT("DETONATE %s: intensity %.1f, radius %.0f, %d rays -> %d marks at %s "
			 "-- hit '%s' (component '%s') after %.1f units."),
		*GetName(), IntensityAtImpact, Radius, Rays, Marks, *Origin.ToCompactString(),
		*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance);

	// The ageing timer is stopped rather than left to burn the intensity down: the thing
	// it was shrinking no longer exists.
	World->GetTimerManager().ClearTimer(AgeTimer);
	bBurnedOut = true;

	// Long enough to outlast whatever is still running. Destroying on the linger alone
	// would cut the blast off mid-envelope, which is the one sound in the game the player
	// is guaranteed to be listening for.
	SetLifeSpan(FMath::Max3(LingerSeconds, ExplosionVolumeSeconds, FlashFalloffSeconds));
}

void ACantripProjectile::TickDetonation()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Elapsed = World->GetTimeSeconds() - DetonationTime;

	bool bSoundDone = true;
	bool bFlashDone = true;

	if (ExplosionAudio && ExplosionVolumeSeconds > 0.0f)
	{
		const float Alpha = FMath::Clamp(Elapsed / ExplosionVolumeSeconds, 0.0f, 1.0f);

		// Cubed rather than linear when no curve is given. A blast collapses almost at
		// once and then rings out; an even fade sounds like someone turning a dial down.
		const float Shape = ExplosionVolumeCurve
			? FMath::Max(0.0f, ExplosionVolumeCurve->GetFloatValue(Alpha))
			: FMath::Pow(1.0f - Alpha, 3.0f);

		ExplosionAudio->SetVolumeMultiplier(ExplosionVolumeScale * Shape);
		bSoundDone = Alpha >= 1.0f;
	}

	if (Light && FlashFalloffSeconds > 0.0f)
	{
		const float Alpha = FMath::Clamp(Elapsed / FlashFalloffSeconds, 0.0f, 1.0f);

		const float Shape = FlashCurve
			? FMath::Max(0.0f, FlashCurve->GetFloatValue(Alpha))
			: FMath::Pow(1.0f - Alpha, 3.0f);

		Light->SetIntensity(FlashPeakIntensity * Shape);
		bFlashDone = Alpha >= 1.0f;
	}

	if (bSoundDone && bFlashDone)
	{
		GetWorld()->GetTimerManager().ClearTimer(DetonationTimer);
	}
}

void ACantripProjectile::ApplyBlastEffect(const FVector& Origin, float Radius)
{
	UWorld* World = GetWorld();
	if (!World || !BlastEffect)
	{
		// No effect assigned is the expected state for now. The gathering below is what
		// was worth building early -- it is the part that would otherwise have to be
		// threaded back through the detonation later -- and it costs nothing to skip.
		return;
	}

	// An effect that will be handed no magnitude deals whatever its asset says with the
	// SetByCaller missing, which is zero. Said out loud, because the detonation is
	// otherwise indistinguishable from a working one.
	if (!BlastMagnitudeTag.IsValid())
	{
		UE_LOG(LogCantripProjectile, Warning,
			TEXT("DETONATE %s: '%s' assigned but Blast Magnitude Tag is unset -- "
				 "the effect will be applied with no magnitude."),
			*GetName(), *GetNameSafe(BlastEffect));
	}

	// The caster's own component is the source, not the projectile's. A fireball has no
	// attributes and no tags; the changeling who threw it is who the effect is FROM, which
	// is what lets resistances, sources and any later retaliation resolve against a person.
	UAbilitySystemComponent* SourceASC = nullptr;
	if (const IAbilitySystemInterface* Caster =
		Cast<IAbilitySystemInterface>(GetInstigator() ? GetInstigator() : GetOwner()))
	{
		SourceASC = Caster->GetAbilitySystemComponent();
	}

	if (!SourceASC)
	{
		UE_LOG(LogCantripProjectile, Warning,
			TEXT("DETONATE %s: blast effect assigned but the thrower has no ability system."),
			*GetName());
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CantripBlast), /*bTraceComplex*/ false, this);
	Params.AddIgnoredActor(GetInstigator());

	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddInstigator(GetInstigator(), this);

	const FGameplayEffectSpecHandle Spec =
		SourceASC->MakeOutgoingSpec(BlastEffect, /*Level*/ 1.0f, Context);

	if (!Spec.IsValid())
	{
		return;
	}

	if (BlastMagnitudeTag.IsValid())
	{
		// Passed by tag rather than baked into the effect's level, because the effect is
		// authored as an asset and the roll is not known until it happens. One number,
		// one place to read it, and the asset decides what it means.
		Spec.Data->SetSetByCallerMagnitude(BlastMagnitudeTag,
			BlastMagnitudePerIntensity * IntensityAtImpact);
	}

	TSet<UAbilitySystemComponent*> Hit;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == GetInstigator() || Target == GetOwner())
		{
			continue;
		}

		const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Target);
		UAbilitySystemComponent* TargetASC =
			Interface ? Interface->GetAbilitySystemComponent() : nullptr;

		// Deduplicated, because one actor can answer an overlap through several of its
		// own components and a body caught by capsule AND mesh would take the blast twice.
		if (!TargetASC || Hit.Contains(TargetASC))
		{
			continue;
		}

		Hit.Add(TargetASC);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}

	UE_LOG(LogCantripProjectile, Log,
		TEXT("BLAST %s: %s applied to %d target(s) within %.0fcm."),
		*GetName(), *GetNameSafe(BlastEffect), Hit.Num(), Radius);
}

void ACantripProjectile::AgeProjectile()
{
	Intensity = FMath::Max(0.0f, Intensity - IntensityLossPerStep);

	// The cap is expressed by spending the remaining fuel rather than by destroying the
	// actor outright, so a projectile that times out fades exactly like one that burned
	// through -- same shrink, same ember tail, same linger.
	if (const UWorld* World = GetWorld())
	{
		if (World->GetTimeSeconds() - LaunchTime >= MaxLifeSeconds)
		{
			Intensity = 0.0f;
		}
	}

	PushIntensity();

	if (Intensity > 0.0f || bBurnedOut)
	{
		return;
	}

	bBurnedOut = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AgeTimer);

		// Deactivated rather than destroyed immediately, so the particles still in the
		// air finish their own lifetimes instead of being cut off mid-flight.
		if (Effect)
		{
			Effect->Deactivate();
		}

		if (Audio)
		{
			Audio->FadeOut(LingerSeconds, 0.0f);
		}

		SetLifeSpan(LingerSeconds);
	}
}

void ACantripProjectile::PushIntensity()
{
	if (Effect && !IntensityParameter.IsNone())
	{
		Effect->SetVariableFloat(IntensityParameter, Intensity);
	}

	if (Light)
	{
		// Scaled from the launch value rather than held constant: a fireball that dims
		// as it burns down is doing the same thing its particles are.
		Light->SetIntensity(4000.0f * FMath::Clamp(Intensity / 100.0f, 0.0f, 1.0f));
	}

	// Applied to the ACTOR, so the capsule grows with the flame. A fireball that looks
	// twice the size but collides at its old radius is worse than one that never grew.
	if (bShrinkWhileBurning || !bLaunched)
	{
		const float Scale = BaseEffectScale + (ScalePerIntensity * Intensity);
		SetActorScale3D(FVector(FMath::Max(0.01f, Scale)));
	}

	if (Audio)
	{
		// A floor plus a scaled part, because a purely proportional volume makes a
		// weak cast completely inaudible -- and silence reads as a bug, not as a
		// quiet fireball.
		Audio->SetVolumeMultiplier(
			MinVolume + (Intensity / FMath::Max(1.0f, VolumeIntensityScale)));
	}
}
