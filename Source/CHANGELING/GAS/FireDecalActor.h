// FireDecalActor.h
//
// A scorch mark that remembers how much fire has hit it.
//
// The naive version of this spawns a decal per particle collision, which at a few
// hundred particles a second buries the frame rate and stacks a hundred identical
// marks in the same square foot. This one is found by a trace and UPDATED instead:
// the first hit leaves something barely visible, and every hit after deepens it.
//
// Two channels of state, deliberately separate. OPACITY is the scorch, and it only
// ever grows -- burnt is not a thing a surface stops being. FIRE INTENSITY is the
// ember glow, and it decays, because embers cool when the flame moves on. A surface
// that keeps darkening while its embers die is what fire actually looks like.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DecalActor.h"
#include "FireDecalActor.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UCLASS()
class CHANGELING_API AFireDecalActor : public ADecalActor
{
	GENERATED_BODY()

public:
	AFireDecalActor();

	virtual void BeginPlay() override;

	/**
	 * Register a hit. Intensity 0..1 is how fierce the flame was where it landed.
	 *
	 * Deepens the scorch always; feeds the embers only if the surface can smoulder.
	 */
	UFUNCTION(BlueprintCallable, Category = "Fire Decal")
	void ApplyFireHit(float Intensity);

	/** Set once at spawn, from the physical material that was struck. */
	UFUNCTION(BlueprintCallable, Category = "Fire Decal")
	void ConfigureForSurface(bool bInFlammable);

	/**
	 * Point a recycled mark at a fresh surface, clearing everything it accumulated.
	 *
	 * Without the clear, a reused decal arrives already at its opacity ceiling and
	 * fully lit -- so recycling reads as marks appearing out of nowhere at full
	 * strength. The material is rebuilt only when flammability changed, or a mark
	 * recycled from grass onto stone would keep the ember material and glow on rock.
	 */
	UFUNCTION(BlueprintCallable, Category = "Fire Decal")
	void ReuseForSurface(bool bInFlammable);

	UFUNCTION(BlueprintPure, Category = "Fire Decal")
	bool IsFlammable() const { return bFlammable; }

	/** When this mark was last struck. The painter recycles the coldest one when it
	 *  runs out of budget, which is nearly always the right one to lose. */
	float GetLastHitTime() const { return LastHitTime; }

protected:
	/**
	 * What the painter's traces actually hit when looking for an existing mark.
	 *
	 * A decal component is a scene component and has no collision of any kind, so
	 * without this a mark is invisible to a trace and can never be found and deepened
	 * -- every particle would lay a fresh one on top of the last.
	 *
	 * Blocks the decal channel and ignores everything else, so it costs nothing to the
	 * rest of the game: it is not walkable, not shootable, and not in the way.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire Decal")
	TObjectPtr<UBoxComponent> BurnBounds;

	/** Must match the painter's DecalTraceChannel, or marks are never found.
	 *  GameTraceChannel2 is the project's "FireDecal" channel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal")
	TEnumAsByte<ECollisionChannel> DecalTraceChannel = ECC_GameTraceChannel2;

	/** Scorch only. Used for stone, gravel, anything that marks but will not glow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal")
	TObjectPtr<UMaterialInterface> NonFlammableMaterial;

	/** Scorch plus embers. Used for grass, wood, dirt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal")
	TObjectPtr<UMaterialInterface> FlammableMaterial;

	// ── Material parameters ─────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Fire Decal")
	// Must match the material's parameter exactly. SetScalarParameterValue fails
	// silently on a name that does not exist, so a mismatch does not error -- the decal
	// simply sits at its default opacity forever and looks like a material problem.
	FName OpacityParameter = TEXT("OpacityIntensity");

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Fire Decal")
	FName FireIntensityParameter = TEXT("FireIntensity");

	// ── Scorch ──────────────────────────────────────────────────────────────

	/** How much darker each hit makes it, scaled by that hit's intensity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal|Scorch",
		meta = (ClampMin = "0.0"))
	float OpacityPerHit = 0.02f;

	/**
	 * How dark a mark is allowed to get, by surface.
	 *
	 * Flammable surfaces go further because they are actually burning rather than
	 * merely being scorched, and the difference should be visible at a glance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal|Scorch",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacityNonFlammable = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal|Scorch",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacityFlammable = 0.7f;

	// ── Embers ──────────────────────────────────────────────────────────────

	/**
	 * How much glow each hit adds, scaled by intensity.
	 *
	 * Very small on purpose. Embers should take a sustained blast to raise, so that a
	 * surface catching light reads as something you DID rather than something that
	 * happened as you swept past.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal|Embers",
		meta = (ClampMin = "0.0"))
	float EmberPerHit = 0.02f;

	/**
	 * Seconds a mark stays at full glow after the last hit, before it starts cooling.
	 *
	 * Without this the decay begins the instant the flame moves on, and because the
	 * decay is proportional it takes its biggest bite while the ember is brightest --
	 * so the glow collapses off its peak and then lingers dimly, which is backwards.
	 * Holding first gives the shape fire actually has: flares up, sits hot, fades out.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal|Embers",
		meta = (ClampMin = "0.0"))
	float EmberHoldSeconds = 3.0f;

	/** Multiplier applied to the glow on every fade step. Just under 1, so embers die
	 *  slowly enough to watch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal|Embers",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmberDecayPerStep = 0.995f;

	/** Seconds between fade steps. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal|Embers",
		meta = (ClampMin = "0.01"))
	float EmberFadeInterval = 0.1f;

	/**
	 * How much the scorch deepens per fade step WHILE embers are alive.
	 *
	 * Something still glowing is still burning, so it should keep darkening after the
	 * flame has moved on. This is what makes a mark look like it went on cooking.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Decal|Embers",
		meta = (ClampMin = "0.0"))
	float OpacityGrowthWhileBurning = 0.0004f;

	/** Below this the glow is not worth a timer, so it is zeroed and the fade stops. */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "Fire Decal|Embers",
		meta = (ClampMin = "0.0"))
	float EmberExtinguishThreshold = 0.001f;

private:
	void FadeEmbers();
	void PushMaterialParameters();
	float MaxOpacity() const;

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DecalMaterial;

	FTimerHandle EmberFadeTimer;

	float Opacity = 0.0f;
	float FireIntensity = 0.0f;
	float LastHitTime = 0.0f;
	bool bFlammable = false;
};
