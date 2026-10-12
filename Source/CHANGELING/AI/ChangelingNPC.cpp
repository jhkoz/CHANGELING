// ChangelingNPC.cpp

#include "ChangelingNPC.h"

#include "Audio/FootstepAudioComponent.h"
#include "ChangelingAIController.h"
#include "ChangelingCrowdVariation.h"
#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "NavigationInvokerComponent.h"
#include "UObject/ConstructorHelpers.h"


const FName AChangelingNPC::OutfitTag(TEXT("Outfit"));


AChangelingNPC::AChangelingNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	// Both halves are needed and forgetting either produces the same symptom -- a pawn that
	// stands there. Without the class she gets the default AAIController, which has no
	// behaviour at all; without PlacedInWorldOrSpawned she gets no controller whatsoever.
	AIControllerClass = AChangelingAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	NavigationInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(
		TEXT("NavigationInvoker"));

	// Every NPC varies by default rather than only the ones someone remembered to tick.
	// Four assembled Boggan heads have to populate a garden, and the tell at crowd distance
	// is not repeated faces -- it is identical heights stepping on identical frames.
	//
	// The component seeds itself from the actor's name, so a placed gardener keeps her
	// appearance across runs and can still be art-directed. Its ranges are EditAnywhere,
	// which is where per-kith shape belongs: a Troll and a Piskey want very different
	// height bands, and those live on each kith's Blueprint rather than in this constructor.
	CrowdVariation = CreateDefaultSubobject<UChangelingCrowdVariation>(
		TEXT("CrowdVariation"));

	FootstepAudio = CreateDefaultSubobject<UFootstepAudioComponent>(TEXT("FootstepAudio"));
	if (FootstepAudio)
	{
		// A third of the player's. Her own steps sit under the camera and are part of how
		// moving feels; theirs are scenery, and a plaza of gardeners at full volume is a
		// crowd rather than a garden.
		FootstepAudio->VolumeMultiplier = 0.33f;

		// 50 m. Past that an NPC is a figure in the distance, and a footfall arriving from
		// something that small reads as coming from nowhere. Also the cheap end of the
		// bargain: the cull skips two ground traces per stride as well as the cue.
		FootstepAudio->MaxAudibleDistance = 5000.0f;

		// Effects cull in well inside the audio range, so a nearby NPC is both heard and
		// seen, a middle-distance one only heard, and a far one neither.
		FootstepAudio->MaxEffectDistance = 2500.0f;

		// The cue tables are defaulted here for the same reason PassiveEffects are defaulted
		// on the base class: an empty table is not an error, it is silence, and silence is
		// indistinguishable from a system nobody has wired up yet. These are the same two
		// tables BP_ThirdPersonCharacter uses.
		static ConstructorHelpers::FObjectFinder<UDataTable> SurfaceTable(
			TEXT("/Game/_Custom/Audio/Data/DT_FootstepSurfaces.DT_FootstepSurfaces"));
		if (SurfaceTable.Succeeded())
		{
			FootstepAudio->SurfaceCueTable = SurfaceTable.Object;
		}

		static ConstructorHelpers::FObjectFinder<UDataTable> WaterTable(
			TEXT("/Game/_Custom/Audio/Data/DT_FootstepWater.DT_FootstepWater"));
		if (WaterTable.Succeeded())
		{
			FootstepAudio->WaterCueTable = WaterTable.Object;
		}
	}

	// Turn where she is going rather than where she is looking. The player character uses
	// controller yaw so her strafe blend space works; an NPC has no controller rotation
	// worth following, and orienting to movement is what makes a walk read as walking.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 360.0f, 0.0f);

		// Slower than the player's jog by default. An NPC crossing the plaza at her pace
		// reads as urgent, and most of them are not.
		Movement->MaxWalkSpeed = 250.0f;

		// Follow paths by ACCELERATING, not by being assigned a velocity.
		//
		// Engine default is false (FNavMovementProperties::bUseAccelerationForPaths), and
		// with it false PathFollowingComponent calls RequestDirectMove -- the pawn is given
		// a velocity outright and GetCurrentAcceleration() stays at zero forever.
		//
		// That is invisible until something asks. ABP_Unarmed computes
		//     ShouldMove = GroundSpeed > 0.01 AND GetCurrentAcceleration() != 0
		// so an NPC walking a path had speed but no acceleration, ShouldMove stayed false,
		// the locomotion state machine never left Idle, and she slid across the garden in
		// an idle pose. Nothing errored: every part was behaving as documented.
		//
		// It also just moves better -- accelerating into a walk and braking out of one,
		// rather than starting and stopping at full speed on the same frame.
		//
		// Reached through the accessor: the struct itself is protected on
		// UNavMovementComponent, and the deprecated flat bUseAccelerationForPaths that used
		// to sit beside it is not the one PathFollowingComponent reads any more.
		if (FNavMovementProperties* NavProperties = Movement->GetNavMovementProperties())
		{
			NavProperties->bUseAccelerationForPaths = true;
		}
	}


	LODSync = CreateDefaultSubobject<ULODSyncComponent>(TEXT("LODSync"));

	// The garment slot, attached to the body because that is what it is skinned to and
	// what ConfigureLODSync walks to find it.
	Outfit = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Outfit"));
	if (Outfit)
	{
		Outfit->SetupAttachment(GetMesh());
		// Tagged here rather than per Blueprint: this is the only thing separating a
		// coat from a face as far as the LOD sync is concerned, and a Blueprint that
		// forgot the tag would silently lose one or the other.
		Outfit->ComponentTags.Add(OutfitTag);
		// It never evaluates its own animation -- BindOutfits gives it the body's pose.
		Outfit->SetAnimationMode(EAnimationMode::AnimationCustomMode);
		Outfit->SetCastShadow(true);
	}

	bUseControllerRotationYaw = false;
}

void AChangelingNPC::BeginPlay()
{
	Super::BeginPlay();

	// Set here rather than in the constructor so a Blueprint's own values are respected --
	// the constructor runs before the Blueprint's defaults are loaded, and anything written
	// there is simply overwritten by them a moment later.
	if (NavigationInvoker)
	{
		NavigationInvoker->SetGenerationRadii(
			NavigationGenerationRadius,
			FMath::Max(NavigationRemovalRadius, NavigationGenerationRadius + 500.0f));
	}

	RebindGrooms();
	BindOutfits();
	ConfigureLODSync();
}

void AChangelingNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Placed actors never reach BeginPlay in the editor, so without this they sit in the
	// level bald while the same Blueprint looks correct in its own viewport -- which is
	// exactly the discrepancy that made this so slow to pin down.
	RebindGrooms();

	// Garments need the same treatment for the same reason: a placed actor never
	// reaches BeginPlay in the editor, so without this the coat sits in T-pose over a
	// character who is not.
	BindOutfits();

	// And the LOD sync for the same reason. When OnConstruction was added for the grooms
	// this was left behind in BeginPlay, so a placed actor had its hair bound but every
	// groom still choosing its own LOD off its own screen size -- hair present up close,
	// gone as the camera pulled back, which is a different fault wearing the same symptom.
	//
	// Epic's own generated Blueprints avoid this by carrying a configured LODSyncComponent
	// as serialised data; ours builds the equivalent at construction instead.
	ConfigureLODSync();
}

void AChangelingNPC::RebindGrooms()
{
	// Re-apply every groom's binding through SetBindingAsset.
	//
	// The generated Blueprints carry the binding as a serialised property, which is not the
	// same thing as having bound: SetBindingAsset is what performs the bind against the
	// skeletal mesh the groom is actually attached to. A groom that never binds renders its
	// authored geometry at the component's own origin instead of following the head -- and
	// since the mesh sits at the capsule bottom, that puts the hair on the floor at her
	// feet. It reads as hair popping in and out as the camera moves past it.
	//
	// Cheap enough to do unconditionally: reassigning a binding that is already correct is
	// a no-op, and doing it always means a Blueprint generated by any route is covered.

	USkeletalMeshComponent* BodyComponent = GetMesh();
	if (!BodyComponent)
	{
		return;
	}

	TArray<USceneComponent*> Attached;
	BodyComponent->GetChildrenComponents(/*bIncludeAllDescendants*/ true, Attached);

	int32 Rebound = 0;

	for (USceneComponent* Child : Attached)
	{
		UGroomComponent* Groom = Cast<UGroomComponent>(Child);
		if (!Groom)
		{
			continue;
		}

		UGroomBindingAsset* Binding = Groom->BindingAsset;
		if (!Binding)
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("%s: groom '%s' has no binding asset -- it will render unbound."),
				*GetName(), *Groom->GetName());
			continue;
		}

		// A binding is built against ONE head. If it targets a different mesh than the one
		// this groom hangs off, no amount of re-applying will help -- the binding itself
		// has to be rebuilt -- so that case is reported rather than silently retried.
		const USkeletalMeshComponent* AttachedTo =
			Cast<USkeletalMeshComponent>(Groom->GetAttachParent());
		const USkeletalMesh* AttachedMesh =
			AttachedTo ? AttachedTo->GetSkeletalMeshAsset() : nullptr;
		const USkeletalMesh* BindingTarget = Binding->GetTargetSkeletalMesh();

		if (AttachedMesh && BindingTarget && AttachedMesh != BindingTarget)
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("%s: groom '%s' is bound to '%s' but attached to '%s'. ")
				TEXT("The binding must be rebuilt against the attached head."),
				*GetName(), *Groom->GetName(),
				*GetNameSafe(BindingTarget), *GetNameSafe(AttachedMesh));
			continue;
		}

		// Cards BEFORE the residency check, and before the bind.
		//
		// Before the residency check because this is a flag on the component rather than
		// something needing the asset loaded -- a groom still streaming should still come
		// back with cards enabled. Before the bind because the bind re-initialises the
		// groom's resources, and it should do that already knowing which geometry to build.
		//
		// DERIVED from the LOD ceiling, not taken straight from bUseHairCards, because the
		// two settings otherwise contradict each other silently.
		//
		// A MetaHuman groom's LOD table is strands at 0-1, cards at 2-4, meshes at 5-7.
		// Asking for cards while the sync is pinned inside the strands band asks for
		// geometry that does not exist at that LOD, and the groom draws NOTHING -- bald at
		// every distance, which is not the old flicker and does not look like a LOD problem
		// at all. Cards are only meaningful once the ceiling actually reaches their band.
		const bool bCardsReachable = (FMath::Max(0, MaxGroomLOD) >= FirstCardLOD);
		Groom->SetUseCards(bUseHairCards && bCardsReachable);

		// The groom asset has to be RESIDENT before this will do anything.
		//
		// SetBindingAsset routes into SetGroomAsset, which begins:
		//
		//     if (Asset && Asset->IsValid()) { ...bind... }
		//     else if (Asset) { GroomAssetBeingLoaded = Asset; ... }   // defers, silently
		//
		// so a rebind issued while the groom is still streaming is quietly stashed rather
		// than performed. That is why firing these across every NPC at once bound exactly
		// one character: the hair alone is tens of megabytes, and only the asset that
		// happened to be resident took. The engine re-applies the stashed one when loading
		// finishes, so the right move is to skip rather than to fight it.
		UGroomAsset* GroomAsset = Groom->GroomAsset;
		if (!GroomAsset || !GroomAsset->IsValid())
		{
			UE_LOG(LogTemplateCharacter, Verbose,
				TEXT("%s: groom '%s' not resident yet; the engine will bind it on load."),
				*GetName(), *Groom->GetName());
			continue;
		}

		// A binding still compiling would be invalidated and re-fetched from the DDC by the
		// call below, which is wasted work -- it is already on its way.
		if (Binding->IsCompiling())
		{
			UE_LOG(LogTemplateCharacter, Verbose,
				TEXT("%s: binding for '%s' is still compiling; leaving it alone."),
				*GetName(), *Groom->GetName());
			continue;
		}

		// One call is enough: SetGroomAsset releases and re-initialises resources
		// unconditionally, so passing the same binding back still performs the bind. An
		// earlier version cleared it first on the assumption that an equal value would
		// early-out; the guard it was avoiding only gates the pointer assignment.
		Groom->SetBindingAsset(Binding);
		++Rebound;
	}

	if (Rebound > 0)
	{
		// The cards decision is logged because it is invisible otherwise: a groom asked for
		// cards inside the strands band renders nothing, and neither the LOD sync line nor a
		// bound-groom count would show anything wrong.
		UE_LOG(LogTemplateCharacter, Log,
			TEXT("%s: rebound %d groom(s) to the head. cards=%s (bUseHairCards=%s, ")
			TEXT("MaxGroomLOD=%d, cards start at LOD%d)."),
			*GetName(), Rebound,
			(bUseHairCards && FMath::Max(0, MaxGroomLOD) >= FirstCardLOD) ? TEXT("ON") : TEXT("OFF"),
			bUseHairCards ? TEXT("true") : TEXT("false"),
			MaxGroomLOD, FirstCardLOD);
	}
}

void AChangelingNPC::BindOutfits()
{
	// Make the body the single source of pose for every tagged garment.
	//
	// A garment is skinned to the body's skeleton. Left to itself it evaluates that
	// skeleton independently, and two evaluations of the same skeleton drift a frame
	// apart -- the coat swims over the character wearing it. SetLeaderPoseComponent
	// replaces its evaluation with the body's transforms outright.
	//
	// Every tagged descendant, not just the native Outfit component: a Blueprint may
	// carry several slots, and the MetaHuman build's own generated Blueprint hangs its
	// clothing off the body the same way.

	USkeletalMeshComponent* BodyComponent = GetMesh();
	if (!BodyComponent)
	{
		return;
	}

	TArray<USceneComponent*> Attached;
	BodyComponent->GetChildrenComponents(/*bIncludeAllDescendants*/ true, Attached);

	int32 Bound = 0;
	for (USceneComponent* Child : Attached)
	{
		USkeletalMeshComponent* Garment = Cast<USkeletalMeshComponent>(Child);
		if (!Garment || !Garment->ComponentHasTag(OutfitTag))
		{
			continue;
		}

		if (!Garment->GetSkeletalMeshAsset())
		{
			// An empty slot, which is how a character wears a coat but no hat. Binding it
			// would be harmless and pointless; skipping keeps the count honest.
			continue;
		}

		Garment->SetLeaderPoseComponent(BodyComponent);
		++Bound;
	}

	if (Bound > 0)
	{
		UE_LOG(LogTemplateCharacter, Verbose,
			TEXT("%s: %d garment(s) following the body."), *GetName(), Bound);
	}
}

void AChangelingNPC::ConfigureLODSync()
{
	if (!LODSync)
	{
		return;
	}

	USkeletalMeshComponent* BodyComponent = GetMesh();
	if (!BodyComponent || !BodyComponent->GetSkeletalMeshAsset())
	{
		// No body means nothing to drive from, and a sync component with only passive
		// entries would leave every groom exactly as unmanaged as it is now.
		return;
	}

	LODSync->ComponentsToSync.Reset();
	LODSync->CustomLODMapping.Reset();

	// The body drives: it is the component with a real screen-size LOD decision behind it,
	// and everything else is attached to it.
	LODSync->ComponentsToSync.Add(
		FComponentSync(BodyComponent->GetFName(), ESyncOption::Drive));

	const int32 BodyLODs = BodyComponent->GetNumLODs();
	int32 FaceLODs = 0;
	FName FaceComponentName = NAME_None;

	// Everything hanging off the body follows it. ONE driver, deliberately.
	//
	// Epic's MetaHuman actor marks both body and face as Drive, but it also feeds
	// CustomLODMapping for each, which reconciles the two into agreement. Without that
	// mapping -- and the pipeline computes it during assembly, so it is not available here
	// -- two drivers simply disagree, because a face mesh and a body mesh reach their LOD
	// thresholds at different screen sizes. ULODSyncComponent then takes the coarsest of
	// them, and the result oscillates frame to frame:
	//
	//     FaceMesh       Source LOD [1]
	//     CharacterMesh0 Source LOD [3]   -> Current LOD (3)
	//     FaceMesh       Source LOD [2]
	//     CharacterMesh0 Source LOD [1]   -> Current LOD (2)
	//
	// The grooms follow that number, and their LOD table is strands at 0-1, cards at 2-4,
	// meshes at 5-7. Bouncing across those boundaries several times a second is the hair
	// flickering in and out. One driver cannot disagree with itself.
	//
	// Not named Children: AActor already has a member by that name.
	TArray<USceneComponent*> Attached;
	BodyComponent->GetChildrenComponents(/*bIncludeAllDescendants*/ true, Attached);

	int32 GroomCount = 0;
	TArray<FName> GroomNames;

	// Garments are mapped like the face: their own LOD count rarely matches the
	// body's, so each needs its own mapping to reach the range it defines.
	TArray<TPair<FName, int32>> Garments;

	for (USceneComponent* Child : Attached)
	{
		if (USkeletalMeshComponent* SkeletalChild = Cast<USkeletalMeshComponent>(Child))
		{
			if (SkeletalChild->ComponentHasTag(OutfitTag))
			{
				// Passive for the same reason as the face: clothing wears whatever LOD
				// the body it is skinned to is wearing. Collected separately so it is
				// never mistaken for the face -- garments are skeletal children too, and
				// before the tag existed this loop handed FaceComponentName to whichever
				// skeletal child happened to come last, running the whole sync off a coat.
				const FName GarmentName = SkeletalChild->GetFName();
				LODSync->ComponentsToSync.Add(
					FComponentSync(GarmentName, ESyncOption::Passive));
				Garments.Emplace(GarmentName, SkeletalChild->GetNumLODs());
				continue;
			}

			// Anything untagged reaching here is meant to be the face, and there should
			// be exactly one. A second is reported rather than allowed to take the
			// face's place: before the tag existed this loop handed FaceComponentName
			// to whichever skeletal child came last, and the whole sync then ran off a
			// hoodie's LOD table.
			//
			// If this fires on a garment, that garment is missing OutfitTag.
			if (FaceComponentName != NAME_None)
			{
				UE_LOG(LogTemplateCharacter, Warning,
					TEXT("%s: extra skeletal mesh component %s under the body. The face is "
						 "%s; this one is ignored by the LOD sync. If it is clothing, tag "
						 "it '%s'."),
					*GetName(), *SkeletalChild->GetName(), *FaceComponentName.ToString(),
					*OutfitTag.ToString());
				continue;
			}

			// Passive, not Drive. The face is the second opinion that caused the fight.
			FaceComponentName = SkeletalChild->GetFName();
			LODSync->ComponentsToSync.Add(FComponentSync(FaceComponentName, ESyncOption::Passive));
			FaceLODs = SkeletalChild->GetNumLODs();
		}
		else if (Cast<UGroomComponent>(Child))
		{
			// Passive: a groom has no opinion worth acting on -- it should wear whatever LOD
			// the head it is bound to is wearing, which is the entire point of the exercise.
			LODSync->ComponentsToSync.Add(FComponentSync(Child->GetFName(), ESyncOption::Passive));
			GroomNames.Add(Child->GetFName());
			++GroomCount;
		}
	}

	// Drive at the FINER of the two granularities, not the body's.
	//
	// An assembled MetaHuman face commonly carries eight LODs against the body's four, and
	// running the sync at four silently asks the face for LODs 0-3 out of its own eight.
	// The grooms are bound to the face, so they inherit that wrong index and pop in and out
	// as it changes -- which is precisely the symptom this whole component was added to fix.
	//
	// An earlier version set NumLODs to the body's count and only WARNED about the
	// mismatch, on the reasoning that a wrong mapping is worse than none. That was the wrong
	// call: no mapping is not neutral, it is the identity mapping, and the identity mapping
	// across differing counts is itself wrong.
	// The DRIVER defines the sync range. Taking max(body, face) instead made the sync run
	// 0..7 on an 8-LOD face while the body -- the only driver -- had 4, so the body needed
	// a mapping to reach a range it was supposed to be defining. ULODSyncComponent then
	// read that table backwards to convert the driver's LOD into a sync LOD:
	//
	//     CharacterMesh0 - Source LOD [1] RemappedLOD[3]   -> Current LOD (3)
	//
	// The grooms follow the sync LOD, and 3 is in their cards band, so hair vanished on any
	// character whose body and face LOD counts differed -- while one standing right in
	// front of the camera at body LOD 1.
	const int32 SyncLODs = BodyLODs;
	LODSync->NumLODs = SyncLODs;

	// Epic computes these arrays during assembly and we have no access to them, so this is
	// a proportional fit instead: sync LOD i maps to the component's own range, ends pinned.
	// With a 4-LOD body driving and an 8-LOD face, the face gets 0,2,5,7 -- it walks its
	// whole range across the body's four steps instead of stopping a third of the way down
	// and holding detail nobody is looking at.
	//
	// Only PASSIVE components are mapped. The driver sets the range, so it needs no table.
	auto BuildMapping = [SyncLODs](int32 ComponentLODs) -> FLODMappingData
	{
		FLODMappingData Data;
		if (ComponentLODs <= 0 || SyncLODs <= 0)
		{
			return Data;
		}

		Data.Mapping.Reserve(SyncLODs);
		for (int32 Index = 0; Index < SyncLODs; ++Index)
		{
			const int32 Mapped = (SyncLODs == 1)
				? 0
				: FMath::RoundToInt(
					(static_cast<float>(Index) * (ComponentLODs - 1)) / (SyncLODs - 1));

			Data.Mapping.Add(FMath::Clamp(Mapped, 0, ComponentLODs - 1));
		}
		return Data;
	};

	// No mapping for the body. It is the driver, so its own LOD index IS the sync index --
	// identity by definition. Giving it a table is what produced the remap above.

	if (FaceComponentName != NAME_None && FaceLODs > 0 && FaceLODs != SyncLODs)
	{
		LODSync->CustomLODMapping.Add(FaceComponentName, BuildMapping(FaceLODs));
	}

	for (const TPair<FName, int32>& Garment : Garments)
	{
		if (Garment.Value > 0 && Garment.Value != SyncLODs)
		{
			LODSync->CustomLODMapping.Add(Garment.Key, BuildMapping(Garment.Value));
		}
	}

	// Pin every groom to its STRAND LODs.
	//
	// A MetaHuman groom's LOD table is strands at 0-1, cards at 2-4, meshes at 5-7. Left to
	// follow the body, a groom reaches cards at body LOD 2 -- and whether it still draws
	// anything there depends on whether that particular groom baked usable card geometry.
	// Across these characters it plainly did not:
	//
	//     Hair_S_UpdoBuns   groups:3  cards:2     three groups, two card entries
	//     Hair_S_Casual     groups:1  cards:2
	//     Hair_S_Clean      groups:1  cards:5
	//
	// A group with no card data renders nothing, which is why identical LOD-sync setups
	// behaved differently per character -- Male Grump kept its hair and Male Wilder lost
	// it on the same configuration.
	//
	// So the sync never asks for anything past MaxGroomLOD, which defaults to 0: the hair
	// stays LOD0 strands at every distance. Always correct, and more expensive far away than
	// cards would be. That is a deliberate trade against unreliable card data, not a fix for
	// it -- the real repair is rebaking the grooms' cards, after which this can be raised
	// per Blueprint rather than edited here.
	FLODMappingData StrandsOnly;
	// Clamped rather than trusted: ClampMin guards the details panel, not a value set from
	// Blueprint or a serialised negative from an older asset.
	const int32 GroomLODCeiling = FMath::Max(0, MaxGroomLOD);

	StrandsOnly.Mapping.Reserve(SyncLODs);
	for (int32 Index = 0; Index < SyncLODs; ++Index)
	{
		StrandsOnly.Mapping.Add(FMath::Min(Index, GroomLODCeiling));
	}

	for (const FName& GroomName : GroomNames)
	{
		LODSync->CustomLODMapping.Add(GroomName, StrandsOnly);
	}

	LODSync->RefreshSyncComponents();

	UE_LOG(LogTemplateCharacter, Log,
		TEXT("%s: LOD sync at %d LODs -- body drives, face (%d LODs) follows, ")
		TEXT("%d groom(s) pinned to LOD<=%d (%s)."),
		*GetName(), SyncLODs, FaceLODs, GroomCount, GroomLODCeiling,
		GroomLODCeiling == 0 ? TEXT("LOD0 strands") : TEXT("strands"));
}
