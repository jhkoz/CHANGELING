// FootstepAudioComponent.cpp

#include "FootstepAudioComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPoolMethodEnum.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Sound/SoundBase.h"

// Included unguarded: ENABLE_DRAW_DEBUG is defined BY this header, so testing it
// first only works when something else has already pulled the header in.
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFootstep, Log, All);

// ─────────────────────────────────────────────────────────────────────────────

UFootstepAudioComponent::UFootstepAudioComponent()
{
	// Entirely event-driven: notifies for steps, LandedDelegate for landings.
	// Nothing here should ever tick.
	PrimaryComponentTick.bCanEverTick = false;

	WaterSurfaces = { SurfaceType8 /* ShallowWater */, SurfaceType9 /* DeepWater */ };
}

void UFootstepAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningCharacter = Cast<ACharacter>(GetOwner());
	if (!OwningCharacter)
	{
		UE_LOG(LogFootstep, Warning,
			TEXT("%s is not on a Character; footstep audio disabled."), *GetNameSafe(GetOwner()));
		return;
	}

	OwningCharacter->LandedDelegate.AddDynamic(this, &UFootstepAudioComponent::HandleLanded);
}

void UFootstepAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OwningCharacter)
	{
		OwningCharacter->LandedDelegate.RemoveDynamic(this, &UFootstepAudioComponent::HandleLanded);
	}

	Super::EndPlay(EndPlayReason);
}

void UFootstepAudioComponent::HandleLanded(const FHitResult& /*Hit*/)
{
	// Grab the impact speed BEFORE anything else runs: CharacterMovement zeroes the
	// downward velocity as part of landing, so by the time the effect spawns there
	// is nothing left to measure.
	LandingImpactSpeed = OwningCharacter
		? FMath::Abs(OwningCharacter->GetVelocity().Z)
		: 0.0f;

	PlayJumpLand();
}

// ── Entry points ─────────────────────────────────────────────────────────────

void UFootstepAudioComponent::PlayFootstep(EFootSide Foot)
{
	const FName Socket = (Foot == EFootSide::Left) ? LeftFootSocket : RightFootSocket;
	PlayStepAtSocket(Socket, ResolveGait());
}

void UFootstepAudioComponent::PlayJumpLand()
{
	// Landings read from the left socket only because both feet arrive together;
	// the cue is a single impact, not a per-foot step.
	PlayStepAtSocket(LeftFootSocket, EFootstepGait::Jump);
}

// ── Core ─────────────────────────────────────────────────────────────────────

void UFootstepAudioComponent::PlayStepAtSocket(FName Socket, EFootstepGait Gait)
{
	if (!OwningCharacter)
	{
		return;
	}

	// A swimming character has no foot contact. Guarded here rather than in the
	// AnimNotify because the locomotion state machine will happily keep playing a
	// walk cycle — notifies and all — until a swim state exists to replace it.
	if (const UCharacterMovementComponent* CharacterMovement = OwningCharacter->GetCharacterMovement())
	{
		if (CharacterMovement->IsSwimming())
		{
			return;
		}
	}

	TArray<FHitResult> Hits;
	FVector Foot = FVector::ZeroVector;
	if (!TraceFromSocket(Socket, Hits, Foot))
	{
		return;
	}

	FVector PlayLocation = FVector::ZeroVector;
	FVector SurfaceNormal = FVector::UpVector;
	float WaterDepthAtStep = 0.0f;
	const FFootstepCueRow* Row = nullptr;

	USoundBase* Cue = ResolveCue(Hits, Foot, Gait, PlayLocation, SurfaceNormal, WaterDepthAtStep, Row);

	if (Cue)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Cue, PlayLocation, VolumeMultiplier);
	}

	// Independent of the cue: a row may carry a splash but no sound, or the other
	// way round, and one missing should not suppress the other.
	if (bSpawnEffects && Row && Row->GetEffect(Gait))
	{
		SpawnStepEffect(*Row, Gait, PlayLocation, SurfaceNormal, WaterDepthAtStep);
	}
}

void UFootstepAudioComponent::SpawnStepEffect(const FFootstepCueRow& Row,
                                              EFootstepGait Gait,
                                              const FVector& Location,
                                              const FVector& Normal,
                                              float Depth) const
{
	UWorld* World = GetWorld();
	UNiagaraSystem* System = Row.GetEffect(Gait);
	if (!World || !System)
	{
		return;
	}

	// Footstep VFX is a detail effect — not worth spawning across the garden.
	if (MaxEffectDistance > 0.0f)
	{
		if (const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (PC->PlayerCameraManager)
			{
				const float DistSq =
					FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), Location);
				if (DistSq > FMath::Square(MaxEffectDistance))
				{
					return;
				}
			}
		}
	}

	// Stand the effect up off the surface rather than always world-up, so splashes
	// on a sloped bank lean with the bank.
	const FRotator Rotation = FRotationMatrix::MakeFromZ(Normal).Rotator();

	// Spawn INACTIVE. The user parameters below are read by the emitters' spawn
	// scripts, and a one-shot burst runs those scripts the instant it activates —
	// so anything set after activation misses the only burst there will ever be.
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, System, Location, Rotation, FVector(Row.GetEffectScale(Gait)),
		/*bAutoDestroy*/ !bPoolEffects, /*bAutoActivate*/ false,
		bPoolEffects ? ENCPoolMethod::AutoRelease : ENCPoolMethod::None,
		/*bPreCullCheck*/ true);

	if (bVerboseLogging)
	{
		UE_LOG(LogFootstep, Log,
			TEXT("SpawnStepEffect: system=%s loc=%s scale=%.2f gait=%d -> component=%s"),
			*GetNameSafe(System), *Location.ToCompactString(),
			Row.GetEffectScale(Gait), static_cast<int32>(Gait),
			Effect ? TEXT("OK") : TEXT("NULL (pre-cull or scalability refused it)"));
	}

	if (!Effect)
	{
		// Scalability or the pre-cull check refused it; not an error.
		return;
	}

	// Faster steps throw more water; deeper water gives it more to throw. Depth is
	// 0 on land, where it must not scale a dust puff down to nothing.
	//
	// A landing is measured differently: by how hard it hit, not how fast it was
	// travelling sideways. Dropping straight in has almost no horizontal speed, so
	// the walking measure would give the biggest impacts the smallest splash.
	float SpeedScale;
	if (Gait == EFootstepGait::Jump)
	{
		SpeedScale = FMath::GetMappedRangeValueClamped(
			FVector2f(0.0f, FMath::Max(LandFullImpactSpeed, 1.0f)), FVector2f(0.5f, 1.5f),
			LandingImpactSpeed);
	}
	else
	{
		const float Speed = OwningCharacter ? OwningCharacter->GetVelocity().Size2D() : 0.0f;
		SpeedScale = FMath::GetMappedRangeValueClamped(
			FVector2f(0.0f, FMath::Max(RunSpeedThreshold, 1.0f) * 2.0f), FVector2f(0.35f, 1.0f), Speed);
	}

	const float DepthScale = (Depth > 0.0f)
		? FMath::GetMappedRangeValueClamped(
			FVector2f(0.0f, FMath::Max(WaterDepth.KneeMax, 1.0f)), FVector2f(0.4f, 1.2f), Depth)
		: 1.0f;

	Effect->SetVariableFloat(IntensityParameter, SpeedScale * DepthScale);
	Effect->SetVariableVec3(VelocityParameter,
		OwningCharacter ? OwningCharacter->GetVelocity() : FVector::ZeroVector);
	Effect->SetVariableLinearColor(WaterColorParameter, WaterTint);

	// Not every part of a splash grows with depth. Droplets and the crown do, but a
	// surface ripple does the opposite: once the foot is far enough under, it stops
	// disturbing the surface at all and the bow wave carries it instead. Hand the
	// system a normalised depth so an emitter can taper on it.
	const float DepthNorm =
		FMath::Clamp(Depth / FMath::Max(WaterDepth.ChestMax, 1.0f), 0.0f, 1.0f);
	Effect->SetVariableFloat(DepthParameter, DepthNorm);

	Effect->Activate(true);

	if (bVerboseLogging)
	{
		UE_LOG(LogFootstep, Log,
			TEXT("  activated: active=%d worldLoc=%s compScale=%s intensity=%.2f depthNorm=%.2f"),
			Effect->IsActive() ? 1 : 0,
			*Effect->GetComponentLocation().ToCompactString(),
			*Effect->GetComponentScale().ToCompactString(),
			SpeedScale * DepthScale, DepthNorm);
	}
}

bool UFootstepAudioComponent::TraceFromSocket(FName Socket, TArray<FHitResult>& OutHits,
                                              FVector& OutFoot) const
{
	const USkeletalMeshComponent* Mesh = OwningCharacter ? OwningCharacter->GetMesh() : nullptr;
	if (!Mesh || !GetWorld())
	{
		return false;
	}

	const FVector Foot = Mesh->GetSocketLocation(Socket);
	OutFoot = Foot;

	// Start above the CAPSULE, not merely above the foot. Water bodies overlap the
	// Pawn rather than blocking it, so a wading character stands on the lakebed with
	// the surface somewhere up the leg. A ray started at foot+30 is already UNDER
	// that surface, and firing downward it never crosses it -- so every tier past
	// Ankle resolved as the bed instead of as water, taking its splash with it.
	float StartZ = Foot.Z + TraceStartHeight;
	if (const UCapsuleComponent* Capsule = OwningCharacter->GetCapsuleComponent())
	{
		StartZ = FMath::Max(StartZ,
			OwningCharacter->GetActorLocation().Z + Capsule->GetScaledCapsuleHalfHeight() + 10.0f);
	}

	// End is measured from the socket, so raising the start never shortens the reach
	// below the foot.
	const FVector Start = FVector(Foot.X, Foot.Y, StartZ);
	const FVector End   = FVector(Foot.X, Foot.Y, Foot.Z - TraceLength);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(Footstep), /*bTraceComplex*/ false, GetOwner());
	Params.bReturnPhysicalMaterial = true;

	// Multi so a water plane and the bed underneath come back from one query,
	// which is what makes a depth reading possible without a second trace.
	TArray<FHitResult> Raw;
	GetWorld()->LineTraceMultiByProfile(Raw, Start, End, TraceProfile, Params);

	// A multi-trace also returns OVERLAPS, and initial-penetration hits when the
	// trace starts inside geometry. Neither is a surface you can stand on --
	// this level has five radius-5000 OverlapAllDynamic ambient-audio spheres,
	// and without this filter one of those becomes Hits[0] and decides the cue.
	OutHits.Reset();
	for (const FHitResult& Hit : Raw)
	{
		if (Hit.bBlockingHit && !Hit.bStartPenetrating)
		{
			OutHits.Add(Hit);
		}
	}

	const bool bHit = OutHits.Num() > 0;

	if (bVerboseLogging && Raw.Num() != OutHits.Num())
	{
		UE_LOG(LogFootstep, Verbose, TEXT("discarded %d non-blocking hit(s)"),
			Raw.Num() - OutHits.Num());
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 2.0f);
	}
#endif

	return bHit && OutHits.Num() > 0;
}

USoundBase* UFootstepAudioComponent::ResolveCue(const TArray<FHitResult>& Hits,
                                                const FVector& Foot,
                                                EFootstepGait Gait,
                                                FVector& OutLocation,
                                                FVector& OutNormal,
                                                float& OutDepth,
                                                const FFootstepCueRow*& OutRow) const
{
	OutRow = nullptr;
	OutNormal = FVector::UpVector;
	OutDepth = 0.0f;

	if (Hits.Num() == 0)
	{
		return nullptr;
	}

	// Hits come back sorted by distance, so Hits[0] is what the foot is ON.
	// Only that decides water vs solid — a water plane BELOW the surface you
	// are standing on (this level has water bodies running under terrain and
	// paving) must not hijack the step.
	const FHitResult& FirstHit = Hits[0];
	const bool bStandingInWater = IsWaterSurface(UGameplayStatics::GetSurfaceType(FirstHit));

	const FHitResult* WaterHit = bStandingInWater ? &FirstHit : nullptr;

	// ── Water: depth decides the tier, and the tier names the row ────────────
	if (WaterHit && WaterCueTable)
	{
		// Surface to foot, and nothing else. This used to hunt down the riverbed as a
		// second hit and measure the water column between the two -- but the bed never
		// came back from the query (274 logged steps, every one reporting a single
		// hit), so it always took a ChestMax + 1 fallback that pinned the whole level
		// to Submerged and made five of the six tiers unreachable.
		//
		// The foot is the better reference anyway: it is how deep THIS step is, not
		// how deep the water is. On uneven ground one foot can be on a rock while the
		// other is in a hollow, and the splash should follow the foot that landed.
		const float Depth = FMath::Max(0.0f, WaterHit->ImpactPoint.Z - Foot.Z);

		const EWaterDepthTier Tier = WaterDepth.Classify(Depth);
		const FName RowName = TierToRowName(Tier);

		if (const FFootstepCueRow* Row =
			WaterCueTable->FindRow<FFootstepCueRow>(RowName, TEXT("Footstep water")))
		{
			OutLocation = WaterHit->ImpactPoint;
			OutNormal = WaterHit->ImpactNormal;
			OutDepth = Depth;
			OutRow = Row;

			if (bVerboseLogging)
			{
				UE_LOG(LogFootstep, Log, TEXT("Water %s depth %.1f -> %s"),
					*RowName.ToString(), Depth, *GetNameSafe(Row->GetCue(Gait)));
			}
			return Row->GetCue(Gait);
		}

		UE_LOG(LogFootstep, Warning, TEXT("No water row '%s' in %s"),
			*RowName.ToString(), *GetNameSafe(WaterCueTable));
		return nullptr;
	}

	// ── Solid surface ───────────────────────────────────────────────────────
	// Only reachable when the foot is NOT in water: the water branch above either
	// returns a cue or bails. Hits[0] is therefore the thing being stood on.
	if (bStandingInWater || !SurfaceCueTable)
	{
		return nullptr;
	}

	const EPhysicalSurface Surface = UGameplayStatics::GetSurfaceType(FirstHit);
	const FName RowName = SurfaceToRowName(Surface);
	OutLocation = FirstHit.ImpactPoint;
	OutNormal = FirstHit.ImpactNormal;

	if (RowName.IsNone())
	{
		// Default surface: no physical material assigned to whatever was hit.
		if (bVerboseLogging)
		{
			UE_LOG(LogFootstep, Warning, TEXT("No physical material on %s"),
				*GetNameSafe(FirstHit.GetActor()));
		}
		return nullptr;
	}

	if (const FFootstepCueRow* Row =
		SurfaceCueTable->FindRow<FFootstepCueRow>(RowName, TEXT("Footstep surface")))
	{
		OutRow = Row;

		if (bVerboseLogging)
		{
			UE_LOG(LogFootstep, Log, TEXT("Surface %s -> %s"),
				*RowName.ToString(), *GetNameSafe(Row->GetCue(Gait)));
		}
		return Row->GetCue(Gait);
	}

	UE_LOG(LogFootstep, Warning, TEXT("No surface row '%s' in %s"),
		*RowName.ToString(), *GetNameSafe(SurfaceCueTable));
	return nullptr;
}

// ── Debug helpers ────────────────────────────────────────────────────────────

FName UFootstepAudioComponent::GetSurfaceNameFromHit(const FHitResult& Hit)
{
	return SurfaceToRowName(UGameplayStatics::GetSurfaceType(Hit));
}

FName UFootstepAudioComponent::GetSurfaceName(EPhysicalSurface Surface)
{
	return SurfaceToRowName(Surface);
}

// ── Helpers ──────────────────────────────────────────────────────────────────

bool UFootstepAudioComponent::IsWaterSurface(EPhysicalSurface Surface) const
{
	return WaterSurfaces.Contains(Surface);
}

FName UFootstepAudioComponent::SurfaceToRowName(EPhysicalSurface Surface)
{
	if (Surface == SurfaceType_Default)
	{
		return NAME_None;
	}

	// DefaultEngine.ini is the single source of truth for surface names, so a
	// new surface needs no code change here.
	if (const UPhysicsSettings* Settings = UPhysicsSettings::Get())
	{
		for (const FPhysicalSurfaceName& Entry : Settings->PhysicalSurfaces)
		{
			if (Entry.Type == Surface)
			{
				return Entry.Name;
			}
		}
	}
	return NAME_None;
}

FName UFootstepAudioComponent::TierToRowName(EWaterDepthTier Tier)
{
	switch (Tier)
	{
	case EWaterDepthTier::Puddle:    return TEXT("Puddle");
	case EWaterDepthTier::Ankle:     return TEXT("Ankle");
	case EWaterDepthTier::Knee:      return TEXT("Knee");
	case EWaterDepthTier::Waist:     return TEXT("Waist");
	case EWaterDepthTier::Chest:     return TEXT("Chest");
	case EWaterDepthTier::Submerged: return TEXT("Submerged");
	}
	return NAME_None;
}

EFootstepGait UFootstepAudioComponent::ResolveGait() const
{
	if (!OwningCharacter)
	{
		return EFootstepGait::Walk;
	}

	const float GroundSpeed = OwningCharacter->GetVelocity().Size2D();
	return (GroundSpeed >= RunSpeedThreshold) ? EFootstepGait::Run : EFootstepGait::Walk;
}
