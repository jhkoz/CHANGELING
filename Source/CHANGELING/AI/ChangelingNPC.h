// ChangelingNPC.h
//
// Base for anything in the garden that walks around without a player driving it.
//
// Thin on purpose. It exists for two things the base does not do by itself: possess itself
// with the right controller, and register as a navigation invoker.
//
// Derives from AChangelingCharacterBase rather than ACharacter, so an NPC has the ability
// system, the attribute sets and the cantrip casting path -- a sentry that can throw a
// working is the same machinery as a player who can, not a parallel one.
//
// The invoker is the one that matters here. L_ChangelingGarden_New is a monolithic level
// roughly a hundred thousand units across, and a static navmesh over that is minutes of
// build time and hundreds of megabytes for a garden whose NPCs occupy a few plazas of it.
// An invoker inverts the problem: navmesh is generated only in a bubble around actors that
// ask for it, so the cost follows the NPCs instead of the landscape.
//
// That does mean navigation must be set to Dynamic. Static generation ignores invokers
// entirely, and the symptom is an NPC that never moves and never says why.

#pragma once

#include "CoreMinimal.h"
#include "ChangelingCharacterBase.h"
#include "ChangelingNPC.generated.h"

class UFootstepAudioComponent;
class ULODSyncComponent;
class USkeletalMeshComponent;
class UChangelingCrowdVariation;
class UNavigationInvokerComponent;

UCLASS(Blueprintable)
class CHANGELING_API AChangelingNPC : public AChangelingCharacterBase
{
	GENERATED_BODY()

public:
	AChangelingNPC();

protected:
	virtual void BeginPlay() override;

	/**
	 * Also binds the grooms, so a placed actor has hair in the level viewport.
	 *
	 * BeginPlay never runs for an actor sitting in the editor, which is why they were bald
	 * there while PIE at least got as far as dropping the hair on the floor.
	 */
	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * Asks the navigation system to build navmesh near her.
	 *
	 * Present even when unused -- the component is cheap, and a project that later turns
	 * navigation Dynamic gets working NPCs without touching every Blueprint.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UNavigationInvokerComponent> NavigationInvoker;

	/**
	 * Footsteps, quieter than the player's and audible only nearby.
	 *
	 * Native rather than added per Blueprint because there will be dozens of these across
	 * eleven kiths, and a Blueprint that forgot the component is a character who walks in
	 * silence -- which reads as an audio bug rather than a missing component.
	 *
	 * Driven by UChangelingAnimInstance's foot-plant notifies, not by the BI_Footstep
	 * Blueprint interface the player uses; see PlayNativeFootstep for why the two paths
	 * cannot both run on the same character.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UFootstepAudioComponent> FootstepAudio;

	/**
	 * Per-instance appearance variation, so two NPCs off the same Blueprint are two people.
	 *
	 * Present on every NPC rather than added where remembered. Its ranges are exposed for
	 * editing, and that is where each kith's build belongs -- a Troll and a Piskey want
	 * different height bands, and the C20 descriptions are the source for them.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UChangelingCrowdVariation> CrowdVariation;

	/**
	 * How far around her navmesh is generated, in world units.
	 *
	 * Wants to be comfortably larger than the controller's WanderRadius, or she paths to
	 * the edge of her own bubble and stops: the destination was reachable when it was
	 * chosen and the tile beyond it never existed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI",
		meta = (ClampMin = "100.0"))
	float NavigationGenerationRadius = 3000.0f;

	/**
	 * How far away navmesh is thrown away again.
	 *
	 * Must exceed the generation radius. Equal values make tiles at the boundary build and
	 * unbuild on alternate frames as she crosses back and forth, which costs more than
	 * simply keeping them.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI",
		meta = (ClampMin = "100.0"))
	float NavigationRemovalRadius = 4500.0f;

	/**
	 * Keeps the face, body and every groom on the same LOD.
	 *
	 * Without one, each component picks its own LOD from its own screen size and they cross
	 * their thresholds at different moments -- so hair, brows and lashes appear and vanish
	 * independently of the head wearing them. UGroomComponent implements ILODSyncInterface
	 * for exactly this, and every MetaHuman actor Epic ships carries a LODSync; ours did
	 * not, because the Blueprints were assembled by hand rather than by their pipeline.
	 *
	 * Populated in BeginPlay rather than the constructor: which grooms exist differs per
	 * character -- the Boggan females have no beard, the Grump female has no peachfuzz --
	 * and the components are added per Blueprint, so there is nothing to list until the
	 * actor is real.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rendering")
	TObjectPtr<ULODSyncComponent> LODSync;

	/**
	 * The garment slot.
	 *
	 * A MetaHuman build does NOT merge clothing into the body, whatever a previous
	 * revision of this class assumed. It bakes the outfit into its own skeletal mesh,
	 * <Character>_Outfits, and its generated Blueprint shows it with a plain
	 * SkeletalMeshComponent hung off the body -- verified against
	 * BP_MHC_Boggan_Wilder_Male, which carries:
	 *
	 *     Body          SkeletalMeshComponent -> SKM_..._BodyMesh
	 *     Face          SkeletalMeshComponent -> SKM_..._FaceMesh
	 *     SkeletalMesh  SkeletalMeshComponent -> MHC_..._Outfits
	 *
	 * So a component is required here too. Removing it left three characters with a
	 * built outfit mesh and nothing to display it.
	 *
	 * Empty is the normal state: most kith wear nothing, and an empty slot costs one
	 * null check. Set the mesh and BindOutfits makes the body drive its pose.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<USkeletalMeshComponent> Outfit;

public:
	/**
	 * Marks a component as clothing rather than anatomy.
	 *
	 * ConfigureLODSync identifies the face as the skeletal mesh child of the body, so
	 * without a way to tell them apart a garment is taken for a second face and one of
	 * the two is dropped from the sync. The tag is the discriminator, which is why it
	 * is applied in the constructor and not left to each Blueprint to remember.
	 */
	static const FName OutfitTag;

protected:

private:
	/** Lists the body, face, grooms and garments with the LOD sync so they move together. */
	void ConfigureLODSync();

	/**
	 * Makes the body the single source of pose for every tagged garment.
	 *
	 * A garment is skinned to the body's skeleton, so it must not evaluate its own
	 * animation -- two independent evaluations of the same skeleton drift apart and the
	 * coat swims over the character wearing it. SetLeaderPoseComponent hands it the
	 * body's transforms instead.
	 *
	 * Cheap to run unconditionally: re-pointing a leader that is already correct is a
	 * no-op, and doing it always covers a Blueprint dressed by any route.
	 */
	void BindOutfits();


	/**
	 * Re-applies each groom's binding so the strands actually skin to the head.
	 *
	 * A binding carried as serialised Blueprint data is not a binding that has been
	 * performed. Unbound hair renders at the component origin -- the capsule bottom -- and
	 * ends up lying on the floor behind her.
	 */
	void RebindGrooms();

public:
	/**
	 * Force every groom to card geometry rather than strands.
	 *
	 * On for NPCs, and set here rather than per Blueprint so it cannot be forgotten on the
	 * forty-odd heads still to come -- it applies to any Blueprint parented to this class,
	 * whoever generated it.
	 *
	 * UGroomComponent::bUseCards defaults to FALSE, and Epic's own MetaHuman actor turns it
	 * on explicitly via SetUseCardsOnGroomComponents. A MetaHuman groom assembles with cards
	 * for its lower LODs (Hair_*_CardsMesh_Group0_LOD0..4 and Helmet_LOD5..7); with cards
	 * off, those LODs have no geometry to draw, so the hair vanishes as the camera pulls
	 * back and returns as it closes.
	 *
	 * Turn it off for a hero character who is looked at closely and can afford strands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	bool bUseHairCards = true;

	/**
	 * The coarsest groom LOD the sync is ever allowed to ask for.
	 *
	 * A MetaHuman groom's LOD table is strands at 0-1, cards at 2-4, meshes at 5-7. At 0 the
	 * hair is LOD0 strands at every distance: always correct, and the most expensive option
	 * far from camera.
	 *
	 * Exposed rather than hardcoded because the right value depends on whether a given
	 * groom baked usable card geometry, and that varies per asset -- some of these carry
	 * three strand groups against two card entries, so a group silently draws nothing once
	 * the sync asks for cards. Raise this per Blueprint for background characters once the
	 * grooms are rebaked; leave it at 0 for anyone the camera gets close to.
	 *
	 * While this is below FirstCardLOD, bUseHairCards is forced OFF regardless of how it is
	 * set. The two are not independent: asking for cards inside the strands band asks for
	 * geometry that does not exist at that LOD, and the groom then draws nothing at all.
	 * An earlier revision of this comment claimed the cards flag was merely "moot" here --
	 * it is not, it actively suppresses the hair, which is exactly how these rendered bald
	 * at every distance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (ClampMin = "0", UIMin = "0", UIMax = "7"))
	int32 MaxGroomLOD = 0;

	/**
	 * First LOD in a MetaHuman groom's table that carries card geometry.
	 *
	 * The table is strands 0-1, cards 2-4, meshes 5-7. Named rather than written as a bare 2
	 * at the comparison site, because the number only means anything alongside that layout.
	 */
	static constexpr int32 FirstCardLOD = 2;

protected:
};
