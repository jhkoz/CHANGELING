// CantripBurnComponent.h
//
// Turns Niagara particle collisions into marks on the world.
//
// Niagara's "Export Particle Data to Blueprint" module hands us a batch of collided
// particles once a tick. For each one we find the surface it actually struck, decide
// from its physical material whether that surface marks at all and whether it can
// smoulder, and then either deepen an existing scorch or lay a new one.
//
// It lives on the CHARACTER rather than on the ability that spawns the flame, and that
// is deliberate. Niagara holds the callback handler as a plain object pointer, and a
// gameplay ability is a transient thing that can end while its last particles are still
// in the air -- pointing Niagara at one is pointing it at something that may not be
// there when the batch arrives. The character outlives every cantrip it casts.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Components/ActorComponent.h"
#include "NiagaraDataInterfaceExport.h"
#include "CantripBurnComponent.generated.h"

class AFireDecalActor;

/**
 * What fire does to one kind of surface.
 *
 * Two separate questions, because they are genuinely separate. Whether a surface MARKS
 * is about how readily it takes a scorch; whether it SMOULDERS is about whether it can
 * catch. Stone marks and never glows. Water does neither.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripBurnSurface
{
	GENERATED_BODY()

	/** Can it hold embers. Drives which decal material is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burn")
	bool bFlammable = false;

	/**
	 * How readily it marks at all, 0 to 1, as the chance any given hit registers.
	 *
	 * A probability rather than a rate, because it thins the work as well as the look:
	 * a surface at 0.2 does a fifth of the traces. Zero makes a surface immune, which
	 * is how water and glass avoid needing a special case.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Burn",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MarkChance = 0.5f;
};

UCLASS(ClassGroup = (Cantrip), meta = (BlueprintSpawnableComponent))
class CHANGELING_API UCantripBurnComponent : public UActorComponent,
	public INiagaraParticleCallbackHandler
{
	GENERATED_BODY()

public:
	UCantripBurnComponent();

	/** Niagara calls this once a tick with every particle that collided. */
	virtual void ReceiveParticleData_Implementation(const TArray<FBasicParticleData>& Data,
		UNiagaraSystem* NiagaraSystem, const FVector& SimulationPositionOffset) override;

	/**
	 * Where the flame is coming from, for the traces.
	 *
	 * Set by the ability when it spawns the effect. A collided particle knows where it
	 * stopped but not what it hit, so the surface has to be found by tracing along the
	 * flame's own path -- which needs to know where that path began.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Burn")
	void SetFireOrigin(FVector WorldOrigin) { FireOrigin = WorldOrigin; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn")
	TSubclassOf<AFireDecalActor> DecalClass;

	/** Per physical surface. Anything not listed uses DefaultSurface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn")
	TMap<TEnumAsByte<EPhysicalSurface>, FCantripBurnSurface> Surfaces;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn")
	FCantripBurnSurface DefaultSurface;

	/**
	 * Trace channel that hits existing decals and nothing else.
	 *
	 * A separate channel because the alternative is tracing on Visibility and then
	 * filtering, which means every particle pays for a trace that mostly hits the floor
	 * it is already standing on. Set this to the custom channel; its default response
	 * should be Ignore so only decals answer.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn")
	TEnumAsByte<ECollisionChannel> DecalTraceChannel = ECC_GameTraceChannel2;

	/** Channel used to find the surface itself once no existing decal was found. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn")
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_Visibility;

	/**
	 * How far past the collision point to keep tracing, as a multiple of the distance
	 * travelled.
	 *
	 * A Niagara particle has width, and it reports colliding from its CENTRE -- which
	 * is still short of the surface by its own radius. A trace that stops there stops
	 * in mid-air and finds nothing, so it has to be run on past.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn",
		meta = (ClampMin = "1.0"))
	float TraceOvershoot = 2.0f;

	/**
	 * Ceiling on live scorch marks. The coldest is recycled once it is reached.
	 *
	 * A flamethrower held down produces collisions indefinitely, and without a ceiling
	 * the only thing deciding how many decals exist is how long the player holds the
	 * key. Recycling the least recently struck keeps the ones being looked at.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn",
		meta = (ClampMin = "1"))
	int32 MaxLiveDecals = 64;

	/**
	 * Most collisions in a batch land within a few centimetres of each other. Handling
	 * every one costs two traces and buys nothing the first already bought.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn",
		meta = (ClampMin = "1"))
	int32 MaxHitsPerBatch = 8;

	/** Hits weaker than this are ignored. The tail of a particle's life barely
	 *  singes anything and is not worth a trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Burn",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinHitIntensity = 0.05f;

	/** Random scale spread, so marks are not all identical circles. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Burn")
	FVector2D DecalScaleRange = FVector2D(0.8f, 1.25f);

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Cantrip|Burn")
	bool bDrawDebugTraces = false;

private:
	void HandleHit(const FBasicParticleData& Particle);
	const FCantripBurnSurface& SurfaceFor(const FHitResult& Hit) const;
	AFireDecalActor* SpawnDecal(const FHitResult& Hit, bool bFlammable);
	AFireDecalActor* RecycleColdestDecal();

	UPROPERTY() TArray<TObjectPtr<AFireDecalActor>> LiveDecals;

	FVector FireOrigin = FVector::ZeroVector;
};
