#include "WaterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "FootstepAudioComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPoolMethodEnum.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaterMovement, Log, All);

UWaterMovementComponent::UWaterMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Slots 8 and 9 in DefaultEngine.ini: ShallowWater, DeepWater.
	WaterSurfaces = { SurfaceType8, SurfaceType9 };
}

void UWaterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	PrimaryComponentTick.TickInterval = UpdateInterval;

	OwningCharacter = Cast<ACharacter>(GetOwner());
	if (!OwningCharacter)
	{
		UE_LOG(LogWaterMovement, Warning,
			TEXT("%s: owner is not an ACharacter — water slowdown disabled"),
			*GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	Movement = OwningCharacter->GetCharacterMovement();
	if (!Movement)
	{
		SetComponentTickEnabled(false);
		return;
	}

	BaseWalkSpeed = Movement->MaxWalkSpeed;
	BaseSwimSpeed = Movement->MaxSwimSpeed;
	BaseJumpVelocity = Movement->JumpZVelocity;

	if (const USkeletalMeshComponent* Mesh = OwningCharacter->GetMesh())
	{
		BaseMeshRelativeZ = Mesh->GetRelativeLocation().Z;
	}

	// Swimming is the engine's, driven by an APhysicsVolume with bWaterVolume set;
	// forcing MOVE_Swimming from here would be reverted by PhysSwimming on the next
	// movement tick. Without bCanSwim, entering that volume does nothing at all.
	if (!Movement->CanEverSwim())
	{
		UE_LOG(LogWaterMovement, Warning,
			TEXT("%s: NavAgentProps.bCanSwim is false — the character will not swim ")
			TEXT("even inside a water volume"), *GetNameSafe(OwningCharacter));
	}

	SyncTiersFromFootstepAudio();
}

void UWaterMovementComponent::SyncTiersFromFootstepAudio()
{
	if (!bSyncWithFootstepAudio || !OwningCharacter)
	{
		return;
	}

	const UFootstepAudioComponent* Footsteps =
		OwningCharacter->FindComponentByClass<UFootstepAudioComponent>();

	if (!Footsteps)
	{
		// Not an error: the component is legitimately usable on a character with no
		// footstep audio. Say so once rather than silently running unsynced.
		UE_LOG(LogWaterMovement, Log,
			TEXT("%s: no UFootstepAudioComponent to sync tiers from; using authored values"),
			*GetNameSafe(OwningCharacter));
		return;
	}

	// Loud on drift, quiet when already in agreement — the point is to catch the
	// day someone retunes one component and not the other.
	if (WaterDepth != Footsteps->WaterDepth)
	{
		UE_LOG(LogWaterMovement, Warning,
			TEXT("Water tier thresholds differed from footstep audio; adopting the audio values ")
			TEXT("(Puddle %.0f→%.0f, Ankle %.0f→%.0f, Knee %.0f→%.0f, Waist %.0f→%.0f, Chest %.0f→%.0f)"),
			WaterDepth.PuddleMax, Footsteps->WaterDepth.PuddleMax,
			WaterDepth.AnkleMax,  Footsteps->WaterDepth.AnkleMax,
			WaterDepth.KneeMax,   Footsteps->WaterDepth.KneeMax,
			WaterDepth.WaistMax,  Footsteps->WaterDepth.WaistMax,
			WaterDepth.ChestMax,  Footsteps->WaterDepth.ChestMax);
	}

	WaterDepth = Footsteps->WaterDepth;
	WaterSurfaces = Footsteps->WaterSurfaces;
}

void UWaterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwningCharacter || !Movement)
	{
		return;
	}

	bSwimming = Movement->IsSwimming();

	// Measured even while swimming, because the exit threshold needs a real number.
	// Floating at the surface keeps the water line inside the probe's range; only a
	// character deeper than MaxCheckHeight comes back empty.
	const float Measured = MeasureDepth();

	const APhysicsVolume* Volume = Movement->GetPhysicsVolume();
	const bool bInWaterVolume = Volume && Volume->bWaterVolume;

	if (bSwimming)
	{
		// A zero reading means two opposite things, and the volume is what tells them
		// apart. Still inside it: we are simply deeper than the probe can see, and
		// treating that as dry would snap speed back to land values mid-swim. Outside
		// it: we have genuinely left the water, and forcing a large depth here makes
		// the exit threshold unsatisfiable — which strands the character swimming on
		// dry land.
		CurrentDepth = (Measured > KINDA_SMALL_NUMBER)
			? Measured
			: (bInWaterVolume ? FMath::Max(WaterDepth.ChestMax, MaxCheckHeight) : 0.0f);
		bInWater = true;
		CurrentTier = EWaterDepthTier::Submerged;
	}
	else
	{
		CurrentDepth = Measured;
		bInWater = CurrentDepth > KINDA_SMALL_NUMBER;
		CurrentTier = WaterDepth.Classify(CurrentDepth);
	}

	UpdateSwimGate(bInWaterVolume);

	// The gate may have just changed the mode; speed scaling below must act on the
	// mode we actually ended up in, not the one we sampled at the top of the tick.
	bSwimming = Movement->IsSwimming();

	// Cached on the game thread for the AnimBP. Cheap on land: ImmersionDepth()
	// returns 0 without tracing unless we are actually inside a water volume.
	ImmersionRatio = Movement->ImmersionDepth();

	const float TargetScale = bInWater ? ScaleForTier(CurrentTier) : 1.0f;

	// Ease rather than snap: crossing a tier boundary mid-stride otherwise
	// reads as a stutter rather than as resistance.
	BlendedScale = FMath::FInterpTo(BlendedScale, TargetScale, DeltaTime, BlendSpeed);

	if (bApplyToMovement)
	{
		if (bSwimming)
		{
			Movement->MaxSwimSpeed = BaseSwimSpeed * SwimSpeedScale;

			// MaxWalkSpeed and JumpZVelocity are left where wading put them. Neither
			// is read while swimming, and both are re-applied from the blend on the
			// tick we touch bottom again — so climbing out still starts sluggish and
			// recovers, rather than snapping to full speed for a frame.
		}
		else
		{
			Movement->MaxWalkSpeed = BaseWalkSpeed * BlendedScale;
			if (bScaleJump)
			{
				// Jump scales harder than walking — pushing off through water is worse
				// than moving through it. Leaving the water surface is NOT this jump:
				// that is CharacterMovement's OutofWaterZ.
				Movement->JumpZVelocity = BaseJumpVelocity * FMath::Square(BlendedScale);
			}
		}
	}

	UpdateMeshOffset(DeltaTime);
	UpdateWake();

	if (bDrawDebug)
	{
		const FVector Feet = OwningCharacter->GetActorLocation()
			- FVector(0.0f, 0.0f, OwningCharacter->GetSimpleCollisionHalfHeight());
		DrawDebugLine(GetWorld(), Feet, Feet + FVector(0, 0, CurrentDepth),
			bInWater ? FColor::Blue : FColor::Green, false, UpdateInterval * 1.1f, 0, 4.0f);
	}

	if (bVerboseLogging)
	{
		// Horizontal and vertical are logged separately on purpose: GroundSpeed in the
		// AnimBP is VSizeXY, so a character that is bobbing hard but barely
		// travelling shows up as "no animation" with no clue why. Accel is here to
		// distinguish "input is not arriving" from "input arrives but goes nowhere".
		const FVector Vel = OwningCharacter->GetVelocity();
		// Log, not Verbose. Verbose is off unless someone remembers to type
		// "Log LogWaterMovement Verbose" into the console, and that setting does not
		// survive an editor restart -- so this line silently produced nothing for
		// entire sessions while bVerboseLogging sat there looking enabled.
		UE_LOG(LogWaterMovement, Log,
			TEXT("depth=%.1f tier=%d scale=%.2f %s | vel2D=%.1f velZ=%.1f accel=%.1f "
			     "max=%.0f mode=%d volume=%s"),
			CurrentDepth, static_cast<int32>(CurrentTier), BlendedScale,
			bSwimming ? TEXT("SWIM") : TEXT("walk"),
			Vel.Size2D(), Vel.Z,
			Movement->GetCurrentAcceleration().Size2D(),
			bSwimming ? Movement->MaxSwimSpeed : Movement->MaxWalkSpeed,
			static_cast<int32>(Movement->MovementMode.GetValue()),
			(Movement->GetPhysicsVolume() && Movement->GetPhysicsVolume()->bWaterVolume)
				? TEXT("water") : TEXT("none"));
	}
}

void UWaterMovementComponent::SetBaseWalkSpeed(float NewBaseSpeed)
{
	BaseWalkSpeed = FMath::Max(0.0f, NewBaseSpeed);
	if (bApplyToMovement && Movement)
	{
		Movement->MaxWalkSpeed = BaseWalkSpeed * BlendedScale;
	}
}

void UWaterMovementComponent::UpdateSwimGate(bool bInWaterVolume)
{
	if (!bUseSwimHysteresis || !Movement)
	{
		return;
	}

	if (bSwimming)
	{
		// Leaving the volume ends swimming outright, whatever the probe says. Without
		// this the only way out is the depth threshold, and depth is exactly what
		// becomes unreadable once you are clear of the water.
		if (!bInWaterVolume || CurrentDepth < SwimExitDepth)
		{
			// Falling rather than Walking: we may still be floating clear of the bed,
			// and CharacterMovement will find the floor and hand over to walking by
			// itself. Asking for Walking with no floor underneath just bounces us
			// back to Falling anyway.
			Movement->NavAgentProps.bCanSwim = false;
			Movement->SetMovementMode(MOVE_Falling);
		}
		return;
	}

	// Below the entry depth, bCanSwim = false makes PhysicsVolumeChanged ignore the
	// volume entirely (it checks CanEverSwim first), so the raw centre-point test
	// can no longer drag us into swimming at the shoreline.
	const bool bDeepEnough = CurrentDepth > SwimEnterDepth;
	Movement->NavAgentProps.bCanSwim = bDeepEnough;

	if (!bDeepEnough)
	{
		return;
	}

	// Re-entry has to be explicit: PhysicsVolumeChanged only fires on a volume
	// CHANGE, and we are already inside the volume by the time depth qualifies.
	const APhysicsVolume* Volume = Movement->GetPhysicsVolume();
	if (Volume && Volume->bWaterVolume)
	{
		Movement->SetMovementMode(MOVE_Swimming);
	}
}

void UWaterMovementComponent::UpdateMeshOffset(float DeltaTime)
{
	if (FMath::IsNearlyZero(SwimMeshZOffset) || !OwningCharacter)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = OwningCharacter->GetMesh();
	if (!Mesh)
	{
		return;
	}

	// Its own alpha rather than BlendedScale: that one is the wade speed multiplier,
	// which is 1.0 on land and 0.25 while swimming -- the opposite shape to what the
	// offset needs, and it never reaches 0.
	MeshOffsetAlpha = FMath::FInterpTo(MeshOffsetAlpha, bSwimming ? 1.0f : 0.0f,
		DeltaTime, BlendSpeed);

	const float TargetZ = BaseMeshRelativeZ + SwimMeshZOffset * MeshOffsetAlpha;

	FVector Relative = Mesh->GetRelativeLocation();
	if (!FMath::IsNearlyEqual(Relative.Z, TargetZ, 0.05f))
	{
		Relative.Z = TargetZ;
		Mesh->SetRelativeLocation(Relative);
	}
}

bool UWaterMovementComponent::IsWakeCulled(const FVector& Location) const
{
	if (MaxWakeDistance <= 0.0f)
	{
		return false;
	}

	const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}

	return FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), Location)
		> FMath::Square(MaxWakeDistance);
}

void UWaterMovementComponent::UpdateWake()
{
	if (!OwningCharacter || (!BowWaveEffect && !WakeTrailEffect))
	{
		return;
	}

	const float Speed = OwningCharacter->GetVelocity().Size2D();
	const bool bWading = bInWater && !bSwimming
		&& CurrentDepth >= WakeMinDepth
		&& Speed >= WakeMinSpeed;

	// The wake belongs on the water SURFACE, not on the actor origin: the surface
	// sits CurrentDepth above the feet, and the feet a half-height below the origin.
	const float HalfHeight = OwningCharacter->GetSimpleCollisionHalfHeight();
	const FVector Actor = OwningCharacter->GetActorLocation();
	const FVector Surface(Actor.X, Actor.Y, Actor.Z - HalfHeight + CurrentDepth);

	const bool bCulled = IsWakeCulled(Surface);
	const bool bRun = bWading && !bCulled;

	// Logged on TRANSITION only. This runs every UpdateInterval, so logging each
	// evaluation would bury everything else; the useful moment is when the wake
	// turns on or off, and why it did not when you expected it to.
	if (bVerboseLogging && bRun != bWakeRunLast)
	{
		UE_LOG(LogWaterMovement, Log,
			TEXT("wake %s: depth=%.1f (min %.0f) speed=%.1f (min %.0f) "
			     "inWater=%d swim=%d culled=%d bow=%s trail=%s"),
			bRun ? TEXT("ON") : TEXT("OFF"),
			CurrentDepth, WakeMinDepth, Speed, WakeMinSpeed,
			bInWater ? 1 : 0, bSwimming ? 1 : 0, bCulled ? 1 : 0,
			*GetNameSafe(BowWaveEffect), *GetNameSafe(WakeTrailEffect));
		bWakeRunLast = bRun;
	}

	// Faster and deeper both push more water.
	const float SpeedFactor = FMath::GetMappedRangeValueClamped(
		FVector2f(WakeMinSpeed, FMath::Max(BaseWalkSpeed, WakeMinSpeed + 1.0f)),
		FVector2f(0.3f, 1.0f), Speed);
	const float DepthFactor = FMath::GetMappedRangeValueClamped(
		FVector2f(WakeMinDepth, FMath::Max(WaterDepth.WaistMax, WakeMinDepth + 1.0f)),
		FVector2f(0.5f, 1.0f), CurrentDepth);
	const float Intensity = SpeedFactor * DepthFactor;

	// ── A: the attached bow wave ────────────────────────────────────────────
	if (BowWaveEffect)
	{
		if (!BowWave)
		{
			// Spawned inactive and kept for the lifetime of the character. A looping
			// system respawned on every water entry would churn the pool for nothing.
			BowWave = UNiagaraFunctionLibrary::SpawnSystemAttached(
				BowWaveEffect, OwningCharacter->GetRootComponent(), NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ false,
				/*bAutoActivate*/ false);
		}

		if (BowWave)
		{
			BowWave->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentDepth - HalfHeight));

			if (bRun != BowWave->IsActive())
			{
				// Deactivate rather than destroy: existing particles finish their
				// lifetime instead of vanishing the instant you step onto the bank.
				bRun ? BowWave->Activate(false) : BowWave->Deactivate();
			}

			if (bVerboseLogging && bRun != bWakeBowLast)
			{
				UE_LOG(LogWaterMovement, Log,
					TEXT("  bow wave %s -> component=%s active=%d relZ=%.1f"),
					bRun ? TEXT("activate") : TEXT("deactivate"),
					BowWave ? TEXT("OK") : TEXT("NULL"),
					BowWave->IsActive() ? 1 : 0, CurrentDepth - HalfHeight);
				bWakeBowLast = bRun;
			}

			if (bRun)
			{
				BowWave->SetVariableFloat(WakeSpeedParameter, Speed);
				BowWave->SetVariableFloat(WakeDepthParameter, CurrentDepth);
				BowWave->SetVariableFloat(WakeIntensityParameter, Intensity);
			}
		}
	}

	// ── B: the world-space trail ────────────────────────────────────────────
	if (!WakeTrailEffect)
	{
		return;
	}

	if (!bRun)
	{
		// Reset rather than remember: re-entering the water somewhere else should not
		// measure its first drop against wherever we left it.
		bTrailStarted = false;
		return;
	}

	if (!bTrailStarted)
	{
		LastTrailLocation = Surface;
		bTrailStarted = true;
		return;
	}

	if (FVector::DistSquared2D(Surface, LastTrailLocation) < FMath::Square(WakeTrailSpacing))
	{
		return;
	}

	LastTrailLocation = Surface;

	const FRotator Yaw(0.0f, OwningCharacter->GetActorRotation().Yaw, 0.0f);
	UNiagaraComponent* Drop = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), WakeTrailEffect, Surface, Yaw, FVector(1.0f),
		/*bAutoDestroy*/ false, /*bAutoActivate*/ false,
		ENCPoolMethod::AutoRelease, /*bPreCullCheck*/ true);

	if (bVerboseLogging)
	{
		UE_LOG(LogWaterMovement, Log, TEXT("  wake trail drop at %s -> %s"),
			*Surface.ToCompactString(), Drop ? TEXT("OK") : TEXT("NULL (pre-cull)"));
	}

	if (Drop)
	{
		Drop->SetVariableFloat(WakeSpeedParameter, Speed);
		Drop->SetVariableFloat(WakeDepthParameter, CurrentDepth);
		Drop->SetVariableFloat(WakeIntensityParameter, Intensity);
		Drop->Activate(true);
	}
}

float UWaterMovementComponent::MeasureDepth() const
{
	const UWorld* World = GetWorld();
	if (!World || !OwningCharacter)
	{
		return 0.0f;
	}

	const float HalfHeight = OwningCharacter->GetSimpleCollisionHalfHeight();
	const FVector Feet = OwningCharacter->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);

	// Look DOWN from above the head: the first water hit is the surface.
	const FVector Start = Feet + FVector(0.0f, 0.0f, MaxCheckHeight);
	const FVector End = Feet - FVector(0.0f, 0.0f, 10.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaterDepthProbe), false, OwningCharacter);
	Params.bReturnPhysicalMaterial = true;

	TArray<FHitResult> Hits;
	World->LineTraceMultiByProfile(Hits, Start, End, TraceProfile, Params);

	for (const FHitResult& Hit : Hits)
	{
		// Overlap volumes and initial-penetration hits are not surfaces. This
		// level has large OverlapAllDynamic ambient spheres that would otherwise
		// be picked up here.
		if (!Hit.bBlockingHit || Hit.bStartPenetrating)
		{
			continue;
		}
		if (!IsWaterSurface(UGameplayStatics::GetSurfaceType(Hit)))
		{
			continue;
		}
		// Measured to the feet, so standing on a rock in a pool reads as shallow.
		return FMath::Max(0.0f, Hit.ImpactPoint.Z - Feet.Z);
	}

	return 0.0f;
}

float UWaterMovementComponent::ScaleForTier(EWaterDepthTier Tier) const
{
	switch (Tier)
	{
	case EWaterDepthTier::Puddle:    return PuddleScale;
	case EWaterDepthTier::Ankle:     return AnkleScale;
	case EWaterDepthTier::Knee:      return KneeScale;
	case EWaterDepthTier::Waist:     return WaistScale;
	case EWaterDepthTier::Chest:     return ChestScale;
	case EWaterDepthTier::Submerged: return SubmergedScale;
	default:                         return 1.0f;
	}
}

bool UWaterMovementComponent::IsWaterSurface(EPhysicalSurface Surface) const
{
	for (const TEnumAsByte<EPhysicalSurface>& Entry : WaterSurfaces)
	{
		if (Entry.GetValue() == Surface)
		{
			return true;
		}
	}
	return false;
}
