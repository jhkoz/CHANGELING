// ChangelingCrowdVariation.h
//
// Makes one assembled MetaHuman look like a different person from the next one.
//
// Four Boggan heads have to populate a garden. Building more heads is the obvious answer
// and the expensive one; this is the cheap half, and it does most of the work, because a
// crowd is read at fifteen metres and in motion. What gives clones away at that distance
// is not their faces -- it is that they are all the same height and all stepping on the
// same frame.
//
// So the order here is deliberate: phase and scale first, because they cost nothing and
// fix the loudest tell, then hair colour, which is the only per-instance appearance knob
// MetaHuman skin genuinely offers. Skin tone is baked into each character's textures, so
// varying it at runtime is not available in any real sense -- more variety there means
// more assembled heads.
//
// Everything is driven from a seeded stream. A given NPC therefore looks the same on
// every run, which matters more than it sounds: a placed gardener who is a different
// person each time you press play cannot be art-directed, and a bug report about "the
// one with the red hair" has to stay reproducible.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "ChangelingCrowdVariation.generated.h"

class UGroomComponent;

/**
 * One kith's variation bands, as described in C20's Appearance sections.
 *
 * These are NOT absolute statures. The assembled MetaHuman body already carries the kith's
 * build -- a Boggan mesh is already short and plump -- so every number here is a multiplier
 * around that mesh, describing how much members of the kith differ FROM EACH OTHER.
 *
 * That distinction is what makes the canon usable as data. "Ranging from seven to nine feet
 * tall" is not a height to set; it is a spread of roughly a quarter around the middle, and a
 * spread is exactly what this table holds. Kiths the book describes as uniform get narrow
 * bands; the ones it describes as varied -- redcaps in all shapes and sizes, Autumn sidhe of
 * all types, piskeys who change with their travels -- get wide ones.
 */
USTRUCT(BlueprintType)
struct FChangelingKithAppearance : public FTableRowBase
{
	GENERATED_BODY()

	/** Vertical scale band. Wide where the book says the kith varies in stature. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silhouette")
	float MinHeightScale = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silhouette")
	float MaxHeightScale = 1.05f;

	/** Horizontal scale band. Skewed low for the slender kiths, high for the thick ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silhouette")
	float MinWeightScale = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silhouette")
	float MaxWeightScale = 1.04f;

	/** Hair melanin band -- 0 is white, 1 is black. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
	float MinMelanin = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
	float MaxMelanin = 0.95f;

	/** Hair redness band. High only where the book actually says red. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
	float MinRedness = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
	float MaxRedness = 0.35f;

	/**
	 * Greying band.
	 *
	 * Kith-wide by default, but this is really a SEEMING trait -- a Grump greys where a
	 * Wilder does not. Since Kith is only a row name, the way to express that is a row per
	 * seeming ("Boggan_Grump" beside "Boggan") rather than anything in code.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
	float MinWhiteness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
	float MaxWhiteness = 0.15f;

	/** Iris hue band across the eye material's own gradient. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float MinIrisHue = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float MaxIrisHue = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float MinIrisValue = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float MaxIrisValue = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float MinIrisSaturation = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float MaxIrisSaturation = 3.4f;

	/**
	 * Bloodshot sclera, 0-1. Redcaps only, per the book's "blood-shot eyes".
	 *
	 * Worth having as its own field because it is the one eye parameter that survives the
	 * pipeline's material baking -- Sclera Irritation Veins Opacity stays live on the baked
	 * instance, where the iris hue parameters may not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float ScleraVeins = 0.0f;
};

/**
 * Per-instance appearance variation for an assembled MetaHuman NPC.
 *
 * Add to a character; it applies itself at BeginPlay and then does nothing. Not ticking,
 * not replicated -- variation is cosmetic and deterministic from the seed, so every client
 * arrives at the same answer without anything being sent.
 */
UCLASS(ClassGroup = "Changeling", meta = (BlueprintSpawnableComponent))
class CHANGELING_API UChangelingCrowdVariation : public UActorComponent
{
	GENERATED_BODY()

public:
	UChangelingCrowdVariation();

	/**
	 * Re-roll and re-apply. Exposed so a spawner can vary an actor it pooled and reused.
	 *
	 * Passing a seed of 0 keeps whatever seed the component already has, so calling this
	 * after an edit re-applies the same appearance rather than silently producing a new one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void ApplyVariation(int32 NewSeed = 0);

protected:
	virtual void BeginPlay() override;

	/**
	 * The seed this instance's appearance is derived from; 0 means derive one.
	 *
	 * Derived from the actor's name rather than left to chance, so a placed NPC keeps her
	 * appearance across runs without anyone having to pick numbers. Set it by hand when a
	 * particular gardener needs to look a particular way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	int32 Seed = 0;

	/**
	 * Which kith's bands to use. Must match a row name in KithTable.
	 *
	 * Left as None the component keeps whatever ranges are set below, which is what a
	 * one-off NPC wants. Set it on each kith's Blueprint and the table becomes the single
	 * place the canon lives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	FName Kith = NAME_None;

	/**
	 * The kith appearance table, defaulted to DT_KithAppearance.
	 *
	 * A table rather than per-Blueprint numbers because the bands only make sense next to
	 * each other: whether a troll varies more than a sluagh is a question about two rows,
	 * and answering it by opening two Blueprints is how they drift apart.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TObjectPtr<UDataTable> KithTable;

	// ── Silhouette ──────────────────────────────────────────────────────────

	/**
	 * Height: vertical scale on the mesh, with the capsule grown to match.
	 *
	 * A CHEAT, and worth knowing as one. Real height lives in the parametric body's Height
	 * parameter and is baked into the assembled mesh at build time, so genuine variety
	 * means more assembled bodies. This stretches the finished mesh instead, which also
	 * stretches the head -- invisible at three percent, a bobblehead at ten.
	 *
	 * The capsule is resized alongside it. Skip that and a taller NPC's head leaves her
	 * collision behind, and she walks through doorways her body does not fit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Silhouette",
		meta = (ClampMin = "0.8", ClampMax = "1.2"))
	float MinHeightScale = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Silhouette",
		meta = (ClampMin = "0.8", ClampMax = "1.2"))
	float MaxHeightScale = 1.05f;

	/**
	 * Weight: horizontal scale on the mesh, with the capsule radius to match.
	 *
	 * Same caveat as height, and the ranges want to be tighter still -- widening a body
	 * without rebuilding it thickens the limbs and the skull along with the belly, and a
	 * face reads as wrong several percent before a torso does.
	 *
	 * Rolled independently of height on purpose: tall-and-slight and short-and-broad are
	 * the combinations that make a crowd look like people rather than one person resized.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Silhouette",
		meta = (ClampMin = "0.8", ClampMax = "1.2"))
	float MinWeightScale = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Silhouette",
		meta = (ClampMin = "0.8", ClampMax = "1.2"))
	float MaxWeightScale = 1.04f;

	// ── Motion ──────────────────────────────────────────────────────────────

	/**
	 * Play-rate jitter on the mesh's animation.
	 *
	 * This is the single most effective thing here. Two characters walking the same cycle
	 * on the same frame read as one asset twice; the same two a fifth of a second apart
	 * read as two people. Kept narrow because a walk much off its authored rate starts to
	 * foot-slide, there being no IK to catch it on a MetaHuman skeleton.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Motion",
		meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MinPlayRate = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Motion",
		meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MaxPlayRate = 1.05f;

	// ── Hair ────────────────────────────────────────────────────────────────

	/** Whether to build per-instance material instances for the grooms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair")
	bool bVaryHairColour = true;

	/**
	 * Melanin range -- the black-to-blond axis, and the one worth varying widely.
	 *
	 * Parameter names verified against M_hair_v4: HairMelanin, HairRedness, HairRoughness.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinMelanin = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxMelanin = 0.95f;

	/**
	 * Redness range. Deliberately narrow.
	 *
	 * Widen this and the crowd becomes gingers and clowns -- red is the parameter that
	 * reads as a decision rather than as a person.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinRedness = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxRedness = 0.35f;

	/**
	 * Greying, 0 to 1. The single most effective age cue a crowd has.
	 *
	 * Separate from melanin rather than folded into it, because grey is not "less pigment"
	 * -- it is white strands mixed through pigmented ones, which is why the material carries
	 * a salt-and-pepper function of its own. Driving melanin toward zero gives blond hair,
	 * not grey hair, and a crowd of ash blonds is not a crowd of elders.
	 *
	 * Rolled with the same stream as the rest, and applied to every groom on the character
	 * at once so hair, brows and beard grey together.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinWhiteness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxWhiteness = 0.15f;

	/** Roughness jitter, purely to break up identical sheen across a group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinRoughness = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Hair",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxRoughness = 0.70f;

	// ── Eyes ────────────────────────────────────────────────────────────────

	/**
	 * Whether to vary iris colour on the face mesh's eye materials.
	 *
	 * WHETHER THIS DOES ANYTHING DEPENDS ON THE ASSEMBLY. The pipeline's "Bake Materials"
	 * option flattens the procedural iris into T_EyeIris*_BC, and a baked iris may ignore
	 * the hue and saturation parameters entirely because the colour is already painted into
	 * the texture. Both the tint and the procedural parameters are written below, so an
	 * unbaked assembly is varied by the latter and a baked one -- if its multiply is applied
	 * after the texture sample -- by the former. If neither moves, re-assemble with Bake
	 * Materials off and the procedural path becomes live.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Eyes")
	bool bVaryEyeColour = true;

	/**
	 * Iris hue range, 0-1 around the material's own gradient.
	 *
	 * The mapping from this number to a colour is the material's, not something documented
	 * anywhere I could check, so these defaults are a starting point rather than a
	 * considered palette -- widen them, watch what comes out, and narrow to taste. Kept
	 * tight initially because the failure mode is a garden of violet-eyed Boggans.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Eyes",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinIrisHue = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Eyes",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxIrisHue = 0.25f;

	/** How light or dark the iris reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Eyes",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinIrisValue = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Eyes",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxIrisValue = 0.75f;

	/**
	 * Saturation range. Clamps to 4.0 in the Eyes tool, and 2.0 is its default.
	 *
	 * Low values give the muddy brown a MetaHuman starts with; high values are what made
	 * the Boggans' eyes read as bright rather than flat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Eyes",
		meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float MinIrisSaturation = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Eyes",
		meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float MaxIrisSaturation = 3.4f;

	/**
	 * Bloodshot sclera. Zero for everyone but redcaps.
	 *
	 * Kept separate from the iris settings because it reaches a different parameter, and
	 * one that survives material baking when the iris hue may not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Eyes",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScleraVeins = 0.0f;

private:
	/** Copies the kith's row over the inline ranges. Silent no-op if Kith is unset. */
	void AdoptKithBands();

	void ApplySilhouette(FRandomStream& Stream);
	void ApplyEyeColour(FRandomStream& Stream);
	void ApplyMotion(FRandomStream& Stream);
	void ApplyHairColour(FRandomStream& Stream);

	/** Every groom on the owner, whatever the kith happens to have. */
	void GatherGrooms(TArray<UGroomComponent*>& OutGrooms) const;

	/** Stable per-actor seed, so appearance survives a restart. */
	int32 ResolveSeed() const;
};
