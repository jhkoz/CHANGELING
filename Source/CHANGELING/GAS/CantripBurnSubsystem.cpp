// CantripBurnSubsystem.cpp

#include "CantripBurnSubsystem.h"

#include "Engine/World.h"
#include "FireDecalActor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripBurnShared, Log, All);

void UCantripBurnSubsystem::AdoptConfig(TSubclassOf<AFireDecalActor> InDecalClass,
	const TMap<TEnumAsByte<EPhysicalSurface>, FCantripBurnSurface>& InSurfaces,
	const FCantripBurnSurface& InDefaultSurface,
	int32 InMaxLiveDecals, float InMinDecalSpacing, FVector2D InScaleRange)
{
	// First writer wins. A second character or a respawned one handing over the same
	// values is harmless, but letting a later one replace the class mid-session would
	// orphan every mark already painted from the previous one.
	if (IsConfigured() || !InDecalClass)
	{
		return;
	}

	DecalClass = InDecalClass;
	Surfaces = InSurfaces;
	DefaultSurface = InDefaultSurface;
	MaxLiveDecals = FMath::Max(1, InMaxLiveDecals);
	MinDecalSpacing = FMath::Max(0.0f, InMinDecalSpacing);
	DecalScaleRange = InScaleRange;

	UE_LOG(LogCantripBurnShared, Log,
		TEXT("BURN SHARED: configured from %s -- %d surfaces, max %d marks, spacing %.0f."),
		*GetNameSafe(InDecalClass), Surfaces.Num(), MaxLiveDecals, MinDecalSpacing);
}

const FCantripBurnSurface& UCantripBurnSubsystem::SurfaceFor(const FHitResult& Hit) const
{
	if (const UPhysicalMaterial* Physical = Hit.PhysMaterial.Get())
	{
		if (const FCantripBurnSurface* Found = Surfaces.Find(Physical->SurfaceType))
		{
			return *Found;
		}
	}

	return DefaultSurface;
}

AFireDecalActor* UCantripBurnSubsystem::FindNearby(const FVector& Point) const
{
	if (MinDecalSpacing <= 0.0f)
	{
		return nullptr;
	}

	AFireDecalActor* Closest = nullptr;
	float BestSq = FMath::Square(MinDecalSpacing);

	// Nearest rather than first found, so a hit between two marks deepens the one it is
	// actually inside instead of whichever sits earlier in the array.
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

AFireDecalActor* UCantripBurnSubsystem::PaintAt(const FHitResult& Hit, float Intensity)
{
	if (!DecalClass)
	{
		return nullptr;
	}

	// Deepening comes before the chance roll: adding to an existing mark is not the
	// same decision as making a new one, and should not be refused by a surface that
	// merely marks reluctantly.
	if (AFireDecalActor* Nearby = FindNearby(Hit.ImpactPoint))
	{
		Nearby->ApplyFireHit(Intensity);
		return Nearby;
	}

	const FCantripBurnSurface& Surface = SurfaceFor(Hit);

	// Zero is how water and glass opt out without a special case anywhere.
	if (Surface.MarkChance <= 0.0f || FMath::FRand() > Surface.MarkChance)
	{
		return nullptr;
	}

	if (AFireDecalActor* Mark = SpawnMark(Hit, Surface.bFlammable))
	{
		Mark->ApplyFireHit(Intensity);
		return Mark;
	}

	return nullptr;
}

int32 UCantripBurnSubsystem::PaintBurst(const FVector& Origin, float Radius, int32 Count,
	float Intensity, AActor* IgnoreActor)
{
	UWorld* World = GetWorld();
	if (!World || !DecalClass || Radius <= 0.0f)
	{
		return 0;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CantripBurst), /*bTraceComplex*/ true,
		IgnoreActor);
	Params.bReturnPhysicalMaterial = true;

	int32 Painted = 0;
	const int32 Rays = FMath::Max(1, Count);

	for (int32 Index = 0; Index < Rays; ++Index)
	{
		// A fresh random direction per ray rather than a fixed lattice. A lattice makes
		// every explosion leave the same rosette, and a burn pattern that repeats reads
		// as a decal system rather than as fire.
		const FVector End = Origin + (FMath::VRand() * Radius);

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Origin, End, SurfaceTraceChannel, Params))
		{
			continue;
		}

		if (PaintAt(Hit, Intensity))
		{
			++Painted;
		}
	}

	UE_LOG(LogCantripBurnShared, Log,
		TEXT("BURST: %d rays over %.0fcm at intensity %.1f -> %d marks (live %d)."),
		Rays, Radius, Intensity, Painted, LiveDecals.Num());

	return Painted;
}

AFireDecalActor* UCantripBurnSubsystem::SpawnMark(const FHitResult& Hit, bool bFlammable)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	LiveDecals.RemoveAll([](const TObjectPtr<AFireDecalActor>& Decal)
	{
		return !IsValid(Decal);
	});

	// Spun at random about the normal. The projection direction is fixed by the surface
	// but the roll around it is free, and leaving it deterministic stamps every mark at
	// the same orientation -- a cluster then reads as one texture repeated.
	const FQuat Spin(Hit.ImpactNormal.GetSafeNormal(), FMath::FRandRange(0.0f, 2.0f * PI));
	const FRotator Rotation =
		(Spin * FRotationMatrix::MakeFromZ(Hit.ImpactNormal).ToQuat()).Rotator();

	const float ScaleY = FMath::FRandRange(DecalScaleRange.X, DecalScaleRange.Y);
	const float ScaleZ = FMath::FRandRange(DecalScaleRange.X, DecalScaleRange.Y);

	if (LiveDecals.Num() >= MaxLiveDecals)
	{
		if (AFireDecalActor* Recycled = RecycleColdest())
		{
			Recycled->SetActorLocation(Hit.ImpactPoint);
			Recycled->SetActorRotation(Rotation);
			Recycled->SetActorScale3D(FVector(1.0f, ScaleY, ScaleZ));
			Recycled->ReuseForSurface(bFlammable);
			return Recycled;
		}
	}

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AFireDecalActor* Decal = World->SpawnActorDeferred<AFireDecalActor>(
		DecalClass, FTransform(Rotation, Hit.ImpactPoint));

	if (!Decal)
	{
		return nullptr;
	}

	// Before BeginPlay, so the right parent material is chosen when the dynamic instance
	// is created rather than swapped afterwards.
	Decal->ConfigureForSurface(bFlammable);
	Decal->FinishSpawning(FTransform(Rotation, Hit.ImpactPoint));
	Decal->SetActorScale3D(FVector(1.0f, ScaleY, ScaleZ));

	LiveDecals.Add(Decal);
	return Decal;
}

AFireDecalActor* UCantripBurnSubsystem::RecycleColdest()
{
	AFireDecalActor* Coldest = nullptr;
	float BestTime = TNumericLimits<float>::Max();

	// Least recently struck, not oldest. A mark the player has been holding a flame on
	// is the one they are looking at; the one they swept past on the way in is the one
	// to lose.
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
