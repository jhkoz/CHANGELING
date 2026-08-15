// CantripBurnComponent.cpp

#include "CantripBurnComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "FireDecalActor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripBurn, Log, All);

UCantripBurnComponent::UCantripBurnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCantripBurnComponent::ReceiveParticleData_Implementation(
	const TArray<FBasicParticleData>& Data, UNiagaraSystem* /*NiagaraSystem*/,
	const FVector& SimulationPositionOffset)
{
	if (!DecalClass || Data.Num() == 0)
	{
		return;
	}

	// Strongest first, then take only the top few. A batch is dozens of collisions
	// within a few centimetres of each other, and the weak ones are the tail of a
	// particle's life -- barely a singe, and each one still costs two traces. Sorting
	// means the budget is spent on the hits that would have looked like something.
	TArray<const FBasicParticleData*> Sorted;
	Sorted.Reserve(Data.Num());
	for (const FBasicParticleData& Particle : Data)
	{
		if (Particle.Size >= MinHitIntensity)
		{
			Sorted.Add(&Particle);
		}
	}

	Sorted.Sort([](const FBasicParticleData& A, const FBasicParticleData& B)
	{
		return A.Size > B.Size;
	});

	const int32 Count = FMath::Min(Sorted.Num(), FMath::Max(1, MaxHitsPerBatch));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FBasicParticleData Particle = *Sorted[Index];

		// Simulations can run in local space with an offset applied afterwards; without
		// this the marks land wherever the system's origin happens to be.
		Particle.Position += SimulationPositionOffset;

		HandleHit(Particle);
	}
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

	// Run the trace on PAST where the particle stopped. A particle has width and
	// reports colliding from its centre, so it stops short of the surface by its own
	// radius -- a trace ending there ends in mid-air and finds nothing at all.
	const FVector End = Collision + (Collision - Start) * (TraceOvershoot - 1.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CantripBurn), /*bTraceComplex*/ true,
		GetOwner());
	Params.bReturnPhysicalMaterial = true;

	if (bDrawDebugTraces)
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
		return;
	}

	const FCantripBurnSurface& Surface = SurfaceFor(SurfaceHit);

	// Zero is how water and glass opt out without needing a special case anywhere.
	if (Surface.MarkChance <= 0.0f)
	{
		return;
	}

	if (FMath::FRand() > Surface.MarkChance)
	{
		return;
	}

	if (AFireDecalActor* Decal = SpawnDecal(SurfaceHit, Surface.bFlammable))
	{
		Decal->ApplyFireHit(Particle.Size);
	}
}

const FCantripBurnSurface& UCantripBurnComponent::SurfaceFor(const FHitResult& Hit) const
{
	if (const UPhysicalMaterial* Physical = Hit.PhysMaterial.Get())
	{
		if (const FCantripBurnSurface* Found = Surfaces.Find(Physical->SurfaceType))
		{
			return *Found;
		}
	}

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
			Recycled->SetActorRotation(FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator());
			return Recycled;
		}
	}

	// Decals project along -X, so the actor faces INTO the surface. Built from the
	// normal rather than assigned, or a mark on a wall lies flat as though the wall
	// were a floor.
	const FRotator Rotation = FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator();

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
