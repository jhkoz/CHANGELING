// CantripBurnComponent.cpp

#include "CantripBurnComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CantripBurnSubsystem.h"
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

void UCantripBurnComponent::BeginPlay()
{
	Super::BeginPlay();

	// Configure the shared subsystem now, so a detonation is never the thing that
	// discovers it was never set up. GetSharedBurn does the adopting.
	if (!DecalClass)
	{
		// The same warning ReceiveParticleData raises, but at the moment it becomes
		// true rather than the first time a flame happens to touch something.
		UE_LOG(LogCantripBurn, Warning,
			TEXT("No DecalClass set on %s -- no burn marks can appear, from any source."),
			*GetNameSafe(GetOwner()));

		bWarnedNoDecalClass = true;
		return;
	}

	GetSharedBurn();
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

int32 UCantripBurnComponent::SharedLiveCount() const
{
	// Read from the shared population rather than a local list. Reporting this
	// component's own count would now always say zero, which is worse than saying
	// nothing -- it reads as "no marks exist" while the level is covered in them.
	if (const UWorld* World = GetWorld())
	{
		if (const UCantripBurnSubsystem* Shared = World->GetSubsystem<UCantripBurnSubsystem>())
		{
			return Shared->GetLiveCount();
		}
	}

	return 0;
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
		SharedLiveCount(), Stats.NoPhysMat, Stats.Unmapped,
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

	// Handed to the shared population rather than kept here. The component still decides
	// WHERE to paint -- that is the flamethrower's business, and an explosion answers it
	// differently -- but every mark in the level belongs to one budget, one spacing rule
	// and one recycler. Two systems each capping themselves at 64 is a level with 128.
	UCantripBurnSubsystem* Shared = GetSharedBurn();
	if (!Shared)
	{
		return;
	}

	// Counted before the call so the summary still distinguishes deepening from
	// spawning, which is most of what makes that line worth reading.
	const bool bHadNearby = Shared->FindNearby(SurfaceHit.ImpactPoint) != nullptr;

	if (AFireDecalActor* Touched = Shared->PaintAt(SurfaceHit, Particle.Size))
	{
		bHadNearby ? ++Stats.DecalHits : ++Stats.Spawned;
	}
	else
	{
		++Stats.ChanceFail;
	}
}

UCantripBurnSubsystem* UCantripBurnComponent::GetSharedBurn()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UCantripBurnSubsystem* Shared = World->GetSubsystem<UCantripBurnSubsystem>();
	if (!Shared)
	{
		return nullptr;
	}

	// Configured from the first component to need it. The surface table is already
	// authored on the character; making the subsystem borrow it beats asking anyone to
	// type nine physical surfaces into Project Settings a second time.
	if (!Shared->IsConfigured())
	{
		Shared->AdoptConfig(DecalClass, Surfaces, DefaultSurface, MaxLiveDecals,
			MinDecalSpacing, DecalScaleRange);
	}

	return Shared;
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
