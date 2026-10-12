// ChangelingMetaHumanCompositor.h
//
// Copies a MetaHuman's head onto another MetaHuman's body.
//
// UMetaHumanCharacter already stores the two independently -- FaceStateBulkData and
// FaceDNABulkData on one side, BodyStateBulkData and BodyDNABulkData on the other -- so
// composing a character from someone else's head is not a trick played on the format, it
// is the format. What is missing is only a way to reach it from script: the accessors take
// FSharedBuffer and TConstArrayView<uint8>, neither of which Blueprint or Python can hold,
// so unlike CommitSkinSettings there is no scripted path to a head.
//
// This is that path, and nothing more. It moves buffers and asks the editor subsystem to
// rebuild; it makes no decisions about which head belongs on which body.
//
// WHY IT MATTERS FOR ELEVEN KITHS: composition is combinatorial where authoring is linear.
// Three heads and three bodies is nine people from six authored pieces, and at the distance
// a crowd is read nobody resolves that they share bone structure. C20 makes this canon for
// the kiths it describes as variable -- piskeys, whose defining trait is having no fixed
// look, and Autumn sidhe, "bodies of all types and sizes" -- and merely useful for the rest.
//
// Editor only. The subsystem that rebuilds the meshes lives in MetaHumanCharacterEditor,
// which is an Editor-type module; a packaged build has no way to conform a face to a body.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChangelingMetaHumanCompositor.generated.h"

class UMetaHumanCharacter;
class UMetaHumanCollectionPipeline;

UCLASS()
class CHANGELING_API UChangelingMetaHumanCompositor : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR

	/**
	 * Replace Target's head with Source's, leaving Target's body alone.
	 *
	 * Copies the face state and, when present, the rigged face DNA. A head that has been
	 * auto-rigged carries both; one that has not carries only state, and the rig has to be
	 * requested again afterwards.
	 *
	 * Both characters must be open for editing -- the subsystem refuses to work on a
	 * character it has not been given, the same way RequestTextureSources does.
	 *
	 * @return whether the head was copied and committed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|MetaHuman",
		meta = (DisplayName = "Copy MetaHuman Head"))
	static bool CopyFaceFrom(UMetaHumanCharacter* Source, UMetaHumanCharacter* Target);

	/**
	 * Replace Target's body with Source's, leaving Target's head alone.
	 *
	 * The mirror of the above, and the half more likely to be reused: one head on several
	 * builds reads as one person at different weights, whereas several heads on one build
	 * reads as siblings, which is usually what a crowd wants.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|MetaHuman",
		meta = (DisplayName = "Copy MetaHuman Body"))
	static bool CopyBodyFrom(UMetaHumanCharacter* Source, UMetaHumanCharacter* Target);

	/**
	 * Compose a new character asset from one character's head and another's body.
	 *
	 * Duplicates BodyDonor -- so the new asset inherits its body, its skin and its eyes --
	 * then copies HeadDonor's head over the top. Skin comes from the BODY donor deliberately:
	 * tone covers the whole figure, and taking it from the head would leave a face that does
	 * not match the arms.
	 *
	 * @return the new character, or null if anything failed. Nothing is written on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|MetaHuman",
		meta = (DisplayName = "Compose MetaHuman"))
	static UMetaHumanCharacter* Compose(
		UMetaHumanCharacter* HeadDonor,
		UMetaHumanCharacter* BodyDonor,
		const FString& PackagePath,
		const FString& AssetName);

	/**
	 * Set a character's Head Scale -- the childling lever.
	 *
	 * MetaHuman cannot build a child: the body-type enum is eighteen adult bodies and the
	 * Height constraint only scales an adult skeleton. Shortening the body and enlarging the
	 * head approximates a child's proportions, which are roughly a fifth head to total
	 * height against an adult's seventh.
	 *
	 * Exists because CommitFaceEvaluationSettings carries no UFUNCTION, so Head Scale is
	 * unreachable from Python -- setting the property alone does nothing, since the face
	 * state has to be told to rescale and then committed.
	 *
	 * Clamped by the engine to 0.8-1.3.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|MetaHuman",
		meta = (DisplayName = "Set MetaHuman Head Scale"))
	static bool SetHeadScale(UMetaHumanCharacter* Character, float HeadScale);

	/**
	 * Drop a character's cached pipeline instances.
	 *
	 * A duplicated character keeps a pointer to the ORIGINAL's pipeline, which is a private
	 * subobject of that character's package. Saving then fails with "Illegal reference to
	 * private object", naming a pipeline belonging to a character you thought you had left
	 * behind. Clearing the map makes the copy build its own on next use.
	 *
	 * "On next use" turned out to mean "when the Creator's pipeline tool is touched", not
	 * "automatically" -- see EnsurePipeline, which exists because of that gap.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|MetaHuman",
		meta = (DisplayName = "Clear MetaHuman Pipeline Cache"))
	static void ClearPipelineCache(UMetaHumanCharacter* Character);

	/**
	 * Give a character a pipeline instance of PipelineClass if it has none.
	 *
	 * ClearPipelineCache empties PipelinesPerClass on the assumption the character rebuilds
	 * its own. It does not. The only thing that repopulates the map is the Creator's own
	 * pipeline tool, which does a FindOrAdd when the pipeline selection is touched -- so a
	 * generated character that has never been opened and poked in that panel carries no
	 * pipeline at all.
	 *
	 * A character in that state looks fine until something asks its collection to resolve a
	 * slot, at which point EVERY virtual groom slot fails -- Hair, Eyebrows, Eyelashes,
	 * Beard, Mustache, Peachfuzz -- and grooms cannot be added or edited. Nothing warns; the
	 * Grooms panel simply has nothing to attach to.
	 *
	 * The instance is created fresh and outered to InCharacter. It is never copied from
	 * another character: a pipeline is a private subobject of its owner's package, and
	 * sharing one is exactly what ClearPipelineCache exists to undo.
	 *
	 * Returns true if a pipeline was created, false if one was already present or the
	 * arguments were unusable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|MetaHuman",
		meta = (DisplayName = "Ensure MetaHuman Pipeline"))
	static bool EnsurePipeline(UMetaHumanCharacter* Character,
		TSubclassOf<UMetaHumanCollectionPipeline> PipelineClass);

#endif // WITH_EDITOR
};
