// CantripBurnComponent.cpp

#include "CantripBurnComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "FireDecalActor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripBurn, Log, All);

// Console rather than only a Blueprint checkbox: this is the kind of thing you want on
// for one cast and off again, and reaching for it should not mean stopping play,
// recompiling a Blueprint and starting over.
static TAutoConsoleVariable<int32> CVarBurnVerbose(
	TEXT("Changeling.Burn.Verbose"),
	0,
	TEXT("Log a once-a-second summary of cantrip burn batches. 0 off, 1 on."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBurnDrawTraces(
	TEXT("Changeling.Burn.DrawTraces"),
	0,
	TEXT("Draw the burn traces from the flame origin to each particle. 0 off, 1 on."),
	ECVF_Default);

UCantripBurnComponent::UCantripBurnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCantripBurnComponent::ReceiveParticleData_Implementation(
	const TArray<FBasicParticleData>& Data, UNiagaraSystem* /*NiagaraSystem*/,
	const FVector& SimulationPositionOffset)
{
	if (!DecalClass)
	{
		// Once, not every frame -- but loudly, because it stops everything downstream
		// and produces exactly the same symptom as a Niagara misconfiguration.
		if (!bWarnedNoDecalClass)
		{
			bWarnedNoDecalClass = true;
			UE_LOG(LogCantripBurn, Warning,
				TEXT("No DecalClass set on %s -- no marks can ever appear."),
				*GetNameSafe(GetOwner()));
		}
		return;
	}

	if (Data.Num() == 0)
	{
		return;
	}

	++Stats.Batches;
	Stats.Received += Data.Num();

	// Strongest first, then take only the top few. A batch is dozens of collisions
	// within a few centimetres of each other, and the weak ones are the tail of a
	// particle's life -- barely a singe, and each one still costs two traces. Sorting
	// means the budget is spent on the hits that would have looked like something.
	// Distance is rejected HERE rather than in HandleHit, and the ordering matters more
	// than it looks. Intensity falls with age, so the strongest particles are the
	// youngest -- which are the ones still at the caster's hand. Sorting first hands
	// every trace slot to particles that the distance guard then throws away, and the
	// budget is spent entirely on hits that were never eligible.
	const float MinDistSq = FMath::Square(MinHitDistance);

	TArray<const FBasicParticleData*> Sorted;
	Sorted.Reserve(Data.Num());
	for (const FBasicParticleData& Particle : Data)
	{
		if (Particle.Size < MinHitIntensity)
		{
			continue;
		}

		const FVector World = Particle.Position + SimulationPositionOffset;
		if ((World - FireOrigin).SizeSquared() < MinDistSq)
		{
			++Stats.TooClose;
			continue;
		}

		Sorted.Add(&Particle);
	}

	Sorted.Sort([](const FBasicParticleData& A, const FBasicParticleData& B)
	{
		return A.Size > B.Size;
	});

	Stats.Kept += Sorted.Num();

	const int32 Count = FMath::Min(Sorted.Num(), FMath::Max(1, MaxHitsPerBatch));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FBasicParticleData Particle = *Sorted[Index];

		if (Index == 0)
		{
			SampleRawPos = Particle.Position;
			SampleOffset = SimulationPositionOffset;
		}

		// Simulations can run in local space with an offset applied afterwards; without
		// this the marks land wherever the system's origin happens to be.
		Particle.Position += SimulationPositionOffset;

		HandleHit(Particle);
	}

	FlushStats();
}

void UCantripBurnComponent::FlushStats()
{
	// Either source turns it on, so the checkbox stays useful for a build you always
	// want instrumented and the console covers everything else.
	if (!bVerboseLogging && CVarBurnVerbose.GetValueOnGameThread() == 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastStatsTime < 1.0f)
	{
		return;
	}

	LastStatsTime = Now;

	// Read left to right: each number is the count surviving one more stage, so the
	// first one that collapses to zero is where the pipeline is broken.
	UE_LOG(LogCantripBurn, Log,
		TEXT("BURN batches=%d received=%d kept=%d | tooclose=%d notrace=%d decalhit=%d ")
		TEXT("surface=%d chancefail=%d spawned=%d | live=%d | nophysmat=%d unmapped=%d ")
		TEXT("hitsurface=%d flammable=%d"),
		Stats.Batches, Stats.Received, Stats.Kept, Stats.TooClose, Stats.NoTrace,
		Stats.DecalHits, Stats.SurfaceHits, Stats.ChanceFail, Stats.Spawned,
		LiveDecals.Num(), Stats.NoPhysMat, Stats.Unmapped,
		static_cast<int32>(SampleSurface.GetValue()), bSampleFlammable ? 1 : 0);

	// The geometry behind those counts. If raw and start are nowhere near each other
	// the exported positions are not in the space this trace assumes; if len is tiny
	// the trace is degenerate and would miss whatever the coordinates were.
	UE_LOG(LogCantripBurn, Log,
		TEXT("  raw=%s off=%s start=%s end=%s len=%.0f"),
		*SampleRawPos.ToCompactString(), *SampleOffset.ToCompactString(),
		*SampleStart.ToCompactString(), *SampleEnd.ToCompactString(),
		(SampleEnd - SampleStart).Size());

	Stats = FBurnStats();
}

void UCantripBurnComponent::HandleHit(const FBasicParticleData& Particle)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Start = FireOrigin;
	const FVector Collision = Particle.Position;

	// Anything this close collided with the caster, not with the world.
	if ((Collision - Start).SizeSquared() < FMath::Square(MinHitDistance))
	{
		++Stats.TooClose;
		return;
	}

	// Run the trace on PAST where the particle stopped. A particle has width and
	// reports colliding from its centre, so it stops short of the surface by its own
	// radius -- a trace ending there ends in mid-air and finds nothing at all.
	const FVector End = Collision + (Collision - Start) * (TraceOvershoot - 1.0f);

	SampleStart = Start;
	SampleEnd = End;

	// SIMPLE collision, not complex. A complex trace only hits per-poly data, which most
	// meshes do not carry -- so it misses every surface in the level while the character
	// walks around on the very same geometry, because movement uses simple collision.
	// Simple is also what Niagara collided against to produce these particles, so this
	// keeps the trace asking about the same world the particle already hit.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CantripBurn), /*bTraceComplex*/ false,
		GetOwner());
	Params.bReturnPhysicalMaterial = true;

	if (bDrawDebugTraces || CVarBurnDrawTraces.GetValueOnGameThread() != 0)
	{
		DrawDebugLine(World, Start, End, FColor::Orange, false, 2.0f);
	}

	// An existing mark first. Finding one and deepening it is the whole reason this is
	// affordable: without it a held flamethrower stacks a fresh decal per collision and
	// the frame rate is decided by how long the key is held.
	FHitResult DecalHit;
	if (World->LineTraceSingleByChannel(DecalHit, Start, End, DecalTraceChannel, Params))
	{
		if (AFireDecalActor* Existing = Cast<AFireDecalActor>(DecalHit.GetActor()))
		{
			++Stats.DecalHits;
			Existing->ApplyFireHit(Particle.Size);
			return;
		}
	}

	// Nothing there yet, so find the surface itself. This trace is also what gives the
	// precise contact point and normal -- the particle's own position is a radius adrift
	// and has no idea which way the wall faces.
	FHitResult SurfaceHit;
	if (!World->LineTraceSingleByChannel(SurfaceHit, Start, End, SurfaceTraceChannel, Params))
	{
		++Stats.NoTrace;
		return;
	}

	++Stats.SurfaceHits;

	// Checked before the chance roll, because deepening an existing mark is not a new
	// mark and should not be gated by whether the surface takes one.
	if (AFireDecalActor* Nearby = FindNearbyDecal(SurfaceHit.ImpactPoint))
	{
		++Stats.DecalHits;
		Nearby->ApplyFireHit(Particle.Size);
		return;
	}

	const FCantripBurnSurface& Surface = SurfaceFor(SurfaceHit);

	// Zero is how water and glass opt out without needing a special case anywhere.
	if (Surface.MarkChance <= 0.0f)
	{
		++Stats.ChanceFail;
		return;
	}

	if (FMath::FRand() > Surface.MarkChance)
	{
		++Stats.ChanceFail;
		return;
	}

	if (AFireDecalActor* Decal = SpawnDecal(SurfaceHit, Surface.bFlammable))
	{
		++Stats.Spawned;
		Decal->ApplyFireHit(Particle.Size);
	}
}

const FCantripBurnSurface& UCantripBurnComponent::SurfaceFor(const FHitResult& Hit)
{
	if (const UPhysicalMaterial* Physical = Hit.PhysMaterial.Get())
	{
		SampleSurface = Physical->SurfaceType;

		if (const FCantripBurnSurface* Found = Surfaces.Find(Physical->SurfaceType))
		{
			bSampleFlammable = Found->bFlammable;
			return *Found;
		}

		// Painted, but with a surface nobody has told the burn component about. Worth
		// separating from having no physical material at all: one is a missing map
		// entry, the other is unpainted geometry, and they are fixed in different
		// places.
		++Stats.Unmapped;
	}
	else
	{
		// The usual reason nothing ever catches fire. A landscape whose layers carry no
		// physical material reports none, every hit falls through to DefaultSurface,
		// and DefaultSurface is not flammable -- so grass burns exactly like stone.
		++Stats.NoPhysMat;
	}

	bSampleFlammable = DefaultSurface.bFlammable;

	// Unpainted geometry is most of a level early on, and silently refusing to mark it
	// looks like the ability is broken rather than like the surface is unusual.
	return DefaultSurface;
}

AFireDecalActor* UCantripBurnComponent::SpawnDecal(const FHitResult& Hit, bool bFlammable)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Spun at random about the surface normal.
	//
	// The projection direction is fixed by the normal, but the roll around it is free --
	// and leaving it deterministic stamps every mark on flat ground at the same
	// orientation. Overlap a dozen of those and the eye reads one texture repeated
	// rather than a burnt patch. Spinning each one costs nothing and is most of what
	// makes a cluster look like fire damage.
	//
	// Built up here because the recycle path below needs it too -- a reused mark landing
	// at its old orientation is the same repetition by another route.
	const FQuat Spin(Hit.ImpactNormal.GetSafeNormal(),
		FMath::FRandRange(0.0f, 2.0f * PI));

	const FRotator Rotation =
		(Spin * FRotationMatrix::MakeFromZ(Hit.ImpactNormal).ToQuat()).Rotator();

	// Prune anything destroyed elsewhere -- a streamed-out level, a demolished wall --
	// before deciding whether the budget is actually full.
	LiveDecals.RemoveAll([](const TObjectPtr<AFireDecalActor>& Decal)
	{
		return !IsValid(Decal);
	});

	if (LiveDecals.Num() >= MaxLiveDecals)
	{
		if (AFireDecalActor* Recycled = RecycleColdestDecal())
		{
			Recycled->SetActorLocation(Hit.ImpactPoint);
			Recycled->SetActorRotation(Rotation);

			// Cleared, or the mark arrives at the opacity ceiling it earned somewhere
			// else and appears fully burnt the instant it is placed.
			Recycled->ReuseForSurface(bFlammable);
			return Recycled;
		}
	}

	// Rotation is built from the normal as the actor's UP, not its forward.
	//
	// ADecalActor already pitches its decal component -90 in the constructor, so an
	// upright actor projects straight down and needs no help. Pointing the actor's
	// forward into the surface as well stacks a second -90 on top, and the projection
	// ends up running PARALLEL to the surface -- which smears the texture across it in
	// stripes inside a hard-edged box, and looks for all the world like a broken
	// material rather than a rotation that was applied twice.

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spawn.Owner = GetOwner();

	AFireDecalActor* Decal = World->SpawnActorDeferred<AFireDecalActor>(
		DecalClass, FTransform(Rotation, Hit.ImpactPoint), GetOwner());

	if (!Decal)
	{
		return nullptr;
	}

	// Before BeginPlay, so the right parent material is chosen when the dynamic
	// instance is created rather than swapped afterwards.
	Decal->ConfigureForSurface(bFlammable);
	Decal->FinishSpawning(FTransform(Rotation, Hit.ImpactPoint));

	// Varied so a swept flame leaves a run of different marks rather than a row of
	// identical stamps, which the eye reads as tiling immediately.
	const float ScaleX = FMath::FRandRange(DecalScaleRange.X, DecalScaleRange.Y);
	const float ScaleY = FMath::FRandRange(DecalScaleRange.X, DecalScaleRange.Y);
	Decal->SetActorScale3D(FVector(1.0f, ScaleX, ScaleY));

	LiveDecals.Add(Decal);
	return Decal;
}

AFireDecalActor* UCantripBurnComponent::FindNearbyDecal(const FVector& Point) const
{
	if (MinDecalSpacing <= 0.0f)
	{
		return nullptr;
	}

	const float LimitSq = FMath::Square(MinDecalSpacing);

	AFireDecalActor* Closest = nullptr;
	float BestSq = LimitSq;

	// Nearest rather than first found, so a hit between two marks deepens the one it is
	// actually inside instead of whichever happens to sit earlier in the array.
	for (const TObjectPtr<AFireDecalActor>& Decal : LiveDecals)
	{
		if (!IsValid(Decal))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Decal->GetActorLocation(), Point);
		if (DistSq < BestSq)
		{
			BestSq = DistSq;
			Closest = Decal;
		}
	}

	return Closest;
}

AFireDecalActor* UCantripBurnComponent::RecycleColdestDecal()
{
	AFireDecalActor* Coldest = nullptr;
	float BestTime = TNumericLimits<float>::Max();

	// Least recently struck, not oldest. A mark the player has been holding the flame
	// on for ten seconds is the one they are looking at; the one they swept past on the
	// way in is the one to lose.
	for (const TObjectPtr<AFireDecalActor>& Decal : LiveDecals)
	{
		if (IsValid(Decal) && Decal->GetLastHitTime() < BestTime)
		{
			BestTime = Decal->GetLastHitTime();
			Coldest = Decal;
		}
	}

	return Coldest;
}
