// CantripBurnSubsystem.h
//
// The world's burn marks, owned in one place.
//
// Every source of fire in the game paints into the same population: the flamethrower's
// particle collisions, an Engulf detonation, and whatever comes later. Sharing matters
// for more than tidiness -- the budget, the spacing rule and the recycling only mean
// anything if they are global. Two systems each keeping "at most 64 marks" is a level
// with 128 marks and two of everything on the same square metre.
//
// The component still owns HOW it decides where to paint. This owns WHAT gets painted
// and how many of them exist.

#pragma once

#include "CoreMinimal.h"
#include "CantripBurnComponent.h"
#include "FireDecalActor.h"
#include "Subsystems/WorldSubsystem.h"
#include "CantripBurnSubsystem.generated.h"

UCLASS()
class CHANGELING_API UCantripBurnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Take configuration from the first burn component to start up.
	 *
	 * The alternative was a settings object in Project Settings, which is tidier but
	 * would mean re-entering the surface table that is already filled in on the
	 * character. The component keeps being the place those values are authored; this
	 * just borrows them so an explosion fifty metres away paints by the same rules.
	 */
	void AdoptConfig(TSubclassOf<AFireDecalActor> InDecalClass,
		const TMap<TEnumAsByte<EPhysicalSurface>, FCantripBurnSurface>& InSurfaces,
		const FCantripBurnSurface& InDefaultSurface,
		int32 InMaxLiveDecals, float InMinDecalSpacing, FVector2D InScaleRange);

	/** True once a component has handed over its settings. */
	bool IsConfigured() const { return DecalClass != nullptr; }

	/** Deepen the mark nearest this point, or lay a new one. Returns what it touched. */
	AFireDecalActor* PaintAt(const FHitResult& Hit, float Intensity);

	/** The nearest existing mark within the spacing radius, if any. */
	AFireDecalActor* FindNearby(const FVector& Point) const;

	/**
	 * Scorch everything around a point -- a detonation rather than a stream.
	 *
	 * Traces outward along random directions rather than sweeping a sphere: a sphere
	 * trace returns the wrong normal for a decal, which is the whole reason the marks
	 * need a surface to lie against. Count scales with the blast so a small one does
	 * not pay for a large one's traces.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Burn")
	int32 PaintBurst(const FVector& Origin, float Radius, int32 Count, float Intensity,
		AActor* IgnoreActor);

	/** How many marks exist in the level right now, across every source. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Burn")
	int32 GetLiveCount() const { return LiveDecals.Num(); }

	/** Surface rules for whatever was struck. */
	const FCantripBurnSurface& SurfaceFor(const FHitResult& Hit) const;

private:
	AFireDecalActor* SpawnMark(const FHitResult& Hit, bool bFlammable);
	AFireDecalActor* RecycleColdest();

	UPROPERTY() TArray<TObjectPtr<AFireDecalActor>> LiveDecals;

	UPROPERTY() TSubclassOf<AFireDecalActor> DecalClass;

	TMap<TEnumAsByte<EPhysicalSurface>, FCantripBurnSurface> Surfaces;
	FCantripBurnSurface DefaultSurface;

	int32 MaxLiveDecals = 64;
	float MinDecalSpacing = 12.0f;
	FVector2D DecalScaleRange = FVector2D(0.5f, 1.6f);

	/** Channel the burst traces on. Visibility, same as the component's surface pass. */
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_Visibility;
};
