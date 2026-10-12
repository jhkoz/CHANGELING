// ChangelingMetaHumanCompositor.cpp

#include "ChangelingMetaHumanCompositor.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "CHANGELING.h"
#include "ChangelingCharacterBase.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
// UMetaHumanCollectionPipeline, for EnsurePipeline. Lives in MetaHumanCharacterPalette,
// which is already a module dependency for IMetaHumanCharacterActorInterface.
#include "MetaHumanCollectionPipeline.h"
// UMetaHumanCollection::SetPipelineFromClass -- the collection's pipeline is the one that
// builds the Specification, and so the one whose absence breaks slot resolution.
#include "MetaHumanCollection.h"
#include "Editor.h"

namespace
{
	/**
	 * Both characters have to be registered with the subsystem before it will touch them.
	 *
	 * Learned the hard way on RequestTextureSources, which logs "not added for editing" and
	 * returns -- no exception, no return value, just nothing happening. Adding them here
	 * rather than requiring the caller to means a script does not have to know the rule.
	 */
	bool PrepareForEditing(UMetaHumanCharacterEditorSubsystem* Subsystem,
		UMetaHumanCharacter* Character)
	{
		if (!Subsystem || !Character)
		{
			return false;
		}

		if (Subsystem->IsObjectAddedForEditing(Character))
		{
			return true;
		}

		return Subsystem->TryAddObjectToEdit(Character);
	}
}

bool UChangelingMetaHumanCompositor::CopyFaceFrom(
	UMetaHumanCharacter* Source, UMetaHumanCharacter* Target)
{
	if (!Source || !Target)
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("CopyFaceFrom: source or target is null"));
		return false;
	}

	if (Source == Target)
	{
		// Harmless but certainly a mistake, and silently succeeding would hide it.
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("CopyFaceFrom: source and target are the same character (%s)"),
			*Source->GetName());
		return false;
	}

	UMetaHumanCharacterEditorSubsystem* Subsystem =
		GEditor ? GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>() : nullptr;

	if (!PrepareForEditing(Subsystem, Source) || !PrepareForEditing(Subsystem, Target))
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("CopyFaceFrom: could not open %s and %s for editing"),
			*Source->GetName(), *Target->GetName());
		return false;
	}

	Target->Modify();

	// A head is never body-independent. The subsystem solves the face against the body it
	// sits on -- SetBodyJointsAndBodyFaceVertices takes the body's bind pose and its own
	// face-region vertices, and the face mesh's bind pose is copied from the body mesh. So
	// the STATE is safe to move between characters: it is re-evaluated against whatever
	// body it lands on, every time the character is initialised.
	Target->SetFaceStateData(Source->GetFaceStateData());

	// The DNA is NOT. It is a rig baked by the cloud auto-rigger against the head as it
	// stood on the donor's body, and nothing recomputes it -- the Full body update path
	// even feeds it back in with bInUpdateFromStateDna. Carry it onto a different body and
	// it describes a neck that no longer exists, which is a seam at the join.
	//
	// So it moves only when the two bodies are the same. An earlier version of this copied
	// it unconditionally and called that a saved cloud round trip; it was a saved round
	// trip and a broken neck.
	const bool bBodiesMatch = Source->HasBodyDNA() && Target->HasBodyDNA()
		&& Source->GetBodyDNABuffer() == Target->GetBodyDNABuffer();

	if (Source->HasFaceDNA() && bBodiesMatch)
	{
		Target->SetFaceDNABuffer(Source->GetFaceDNABuffer(),
			Source->HasFaceDNABlendshapes());

		UE_LOG(LogTemplateCharacter, Log,
			TEXT("CopyFaceFrom: bodies match, carried the face rig across"));
	}
	else
	{
		// Cleared rather than left stale. A character with no rig reports honestly that it
		// needs one; a character wearing the wrong rig looks merely slightly wrong, and
		// that is the kind of defect that ships.
		Target->SetFaceDNABuffer(TConstArrayView<uint8>(), /*bInHasFaceDNABlendshapes*/ false);

		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("CopyFaceFrom: %s needs auto-rigging -- %s"),
			*Target->GetName(),
			Source->HasFaceDNA()
				? TEXT("the donor's rig was built on a different body and was discarded")
				: TEXT("the donor had no rig to copy"));
	}

	// Rebuilds the mesh from the state we just wrote. Without this the asset holds the new
	// head and shows the old one, which reads as the copy having failed.
	Subsystem->CommitFaceState(Target);
	Target->MarkPackageDirty();

	UE_LOG(LogTemplateCharacter, Log, TEXT("Copied head from %s onto %s"),
		*Source->GetName(), *Target->GetName());

	return true;
}

bool UChangelingMetaHumanCompositor::CopyBodyFrom(
	UMetaHumanCharacter* Source, UMetaHumanCharacter* Target)
{
	if (!Source || !Target || Source == Target)
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("CopyBodyFrom: source and target must be two different valid characters"));
		return false;
	}

	UMetaHumanCharacterEditorSubsystem* Subsystem =
		GEditor ? GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>() : nullptr;

	if (!PrepareForEditing(Subsystem, Source) || !PrepareForEditing(Subsystem, Target))
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("CopyBodyFrom: could not open %s and %s for editing"),
			*Source->GetName(), *Target->GetName());
		return false;
	}

	Target->Modify();

	Target->SetBodyStateData(Source->GetBodyStateData());

	if (Source->HasBodyDNA())
	{
		Target->SetBodyDNABuffer(Source->GetBodyDNABuffer());
	}

	Subsystem->CommitBodyState(Target);

	// The head that was already on this character was fitted to the body just replaced.
	// Committing the face writes the re-solved state, and dropping the rig marks it as
	// needing one -- changing the body underneath a head invalidates its rig exactly as
	// swapping the head does.
	Subsystem->CommitFaceState(Target);
	Target->SetFaceDNABuffer(TConstArrayView<uint8>(), /*bInHasFaceDNABlendshapes*/ false);

	UE_LOG(LogTemplateCharacter, Warning,
		TEXT("CopyBodyFrom: %s needs auto-rigging -- its face rig was built on the old body"),
		*Target->GetName());

	Target->MarkPackageDirty();

	UE_LOG(LogTemplateCharacter, Log, TEXT("Copied body from %s onto %s"),
		*Source->GetName(), *Target->GetName());

	return true;
}

bool UChangelingMetaHumanCompositor::SetHeadScale(
	UMetaHumanCharacter* Character, float HeadScale)
{
	if (!Character)
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("SetHeadScale: character is null"));
		return false;
	}

	UMetaHumanCharacterEditorSubsystem* Subsystem =
		GEditor ? GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>() : nullptr;

	if (!PrepareForEditing(Subsystem, Character))
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("SetHeadScale: could not open %s for editing"), *Character->GetName());
		return false;
	}

	// The engine's own clamp. Asking for a bobblehead gets you 1.3 and a log line, rather
	// than an argument silently doing nothing.
	const float Clamped = FMath::Clamp(HeadScale, 0.8f, 1.3f);
	if (!FMath::IsNearlyEqual(Clamped, HeadScale))
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("SetHeadScale: %.2f is outside the supported 0.8-1.3 range; using %.2f"),
			HeadScale, Clamped);
	}

	Character->Modify();

	// Read-modify-write rather than constructing a fresh struct, so Global Delta and the
	// texture offset keep whatever the character was authored with.
	FMetaHumanCharacterFaceEvaluationSettings Settings = Character->FaceEvaluationSettings;
	Settings.HeadScale = Clamped;

	// Commit, not just assign. Writing the property alone changes a number nobody reads --
	// the face state has to be rescaled and re-evaluated, which is what this does.
	Subsystem->CommitFaceEvaluationSettings(Character, Settings);
	Character->MarkPackageDirty();

	UE_LOG(LogTemplateCharacter, Log, TEXT("%s: head scale set to %.2f"),
		*Character->GetName(), Clamped);

	return true;
}

void UChangelingMetaHumanCompositor::ClearPipelineCache(UMetaHumanCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	const int32 Count = Character->PipelinesPerClass.Num();
	if (Count == 0)
	{
		return;
	}

	Character->Modify();
	Character->PipelinesPerClass.Empty();
	Character->MarkPackageDirty();

	UE_LOG(LogTemplateCharacter, Log,
		TEXT("%s: dropped %d cached pipeline instance(s); one can be restored with "
			"EnsurePipeline"),
		*Character->GetName(), Count);
}

bool UChangelingMetaHumanCompositor::EnsurePipeline(
	UMetaHumanCharacter* Character,
	TSubclassOf<UMetaHumanCollectionPipeline> PipelineClass)
{
	if (!Character)
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("EnsurePipeline: character is null"));
		return false;
	}

	if (!PipelineClass)
	{
		// Refused rather than defaulted. Which pipeline a character carries decides what its
		// assembly produces -- the legacy path gives Body/Face/Grooms, the cinematic one adds
		// control rigs and ARKit mappings -- so guessing here would quietly change the output
		// of every character this is run against.
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("EnsurePipeline: no pipeline class given for %s"), *Character->GetName());
		return false;
	}

	// TWO PIPELINES, and only one of them matters for slot resolution.
	//
	// PipelinesPerClass is a cache ON THE CHARACTER, keyed by class. The COLLECTION holds a
	// separate instance in its own private Pipeline property, and it is the collection's
	// that builds the Specification -- so the slot failures name
	//
	//     <Character>:InternalCollection.<Pipeline>.Specification
	//
	// An earlier version of this function filled only the character's cache. The count went
	// from 0 to 1, the log said so, and every virtual slot went on failing, because the
	// broken object was never touched. Fixing a cache is not fixing the thing it caches.
	bool bChanged = false;

	if (UMetaHumanCollection* Collection = Character->GetMutableInternalCollection())
	{
		// Compared by CLASS, not by pointer: a collection carrying an instance of the wrong
		// class -- a non-legacy pipeline where the legacy one is wanted -- is exactly the
		// state that needs correcting, and it is not null, so a null check would miss it.
		const UMetaHumanCollectionPipeline* Current = Collection->GetPipeline();
		if (!Current || Current->GetClass() != PipelineClass)
		{
			Collection->Modify();
			// SetPipelineFromClass builds the instance itself, outered to the collection.
			// Constructing one here and passing it to SetPipeline would work too, but this
			// is the engine's own path and it stays correct if that construction changes.
			Collection->SetPipelineFromClass(PipelineClass);
			bChanged = true;

			UE_LOG(LogTemplateCharacter, Log,
				TEXT("%s: collection pipeline %s -> %s"),
				*Character->GetName(),
				Current ? *Current->GetClass()->GetName() : TEXT("none"),
				*PipelineClass->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("EnsurePipeline: %s has no internal collection"), *Character->GetName());
		return false;
	}

	// The character's own cache, separately. Harmless to keep populated and it is what the
	// Creator's pipeline tool reads, but on its own it fixes nothing.
	const TObjectPtr<UMetaHumanCollectionPipeline>* Cached =
		Character->PipelinesPerClass.Find(PipelineClass);
	if (!Cached || *Cached == nullptr)
	{
		// Outered to THIS character. A pipeline is a private subobject of its owner's
		// package, so pointing two characters at one instance is what produces "Illegal
		// reference to private object" on save -- the failure ClearPipelineCache undoes.
		if (UMetaHumanCollectionPipeline* Instance =
				NewObject<UMetaHumanCollectionPipeline>(Character, PipelineClass))
		{
			Character->Modify();
			Character->PipelinesPerClass.Add(PipelineClass, Instance);
			bChanged = true;
		}
		else
		{
			UE_LOG(LogTemplateCharacter, Error,
				TEXT("EnsurePipeline: could not create a %s for %s"),
				*PipelineClass->GetName(), *Character->GetName());
		}
	}

	if (bChanged)
	{
		Character->MarkPackageDirty();
	}

	return bChanged;
}

UMetaHumanCharacter* UChangelingMetaHumanCompositor::Compose(
	UMetaHumanCharacter* HeadDonor,
	UMetaHumanCharacter* BodyDonor,
	const FString& PackagePath,
	const FString& AssetName)
{
	if (!HeadDonor || !BodyDonor)
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("Compose: a donor is null"));
		return nullptr;
	}

	if (AssetName.IsEmpty() || PackagePath.IsEmpty())
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("Compose: package path and name required"));
		return nullptr;
	}

	IAssetTools& AssetTools =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Duplicating the BODY donor rather than building an empty character: the new asset then
	// inherits skin, eyes and every accent region already tuned on it. Skin in particular has
	// to follow the body -- tone covers the whole figure, and taking it from the head donor
	// would leave a face that does not match its own arms.
	UObject* Duplicated = AssetTools.DuplicateAsset(AssetName, PackagePath, BodyDonor);
	UMetaHumanCharacter* Composed = Cast<UMetaHumanCharacter>(Duplicated);

	if (!Composed)
	{
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("Compose: could not duplicate %s to %s/%s"),
			*BodyDonor->GetName(), *PackagePath, *AssetName);
		return nullptr;
	}

	// The duplicate still points at the BODY DONOR's pipeline instance, which is private to
	// that character's package -- saving would fail with "Illegal reference to private
	// object". Dropped here so every composed character builds its own.
	ClearPipelineCache(Composed);

	if (!CopyFaceFrom(HeadDonor, Composed))
	{
		// The duplicate is left on disk deliberately rather than deleted. It is a valid
		// character -- a copy of the body donor -- and silently removing an asset because a
		// later step failed is worse than leaving one to be looked at.
		UE_LOG(LogTemplateCharacter, Error,
			TEXT("Compose: head copy failed; %s remains as a copy of %s"),
			*Composed->GetName(), *BodyDonor->GetName());
		return nullptr;
	}

	UEditorLoadingAndSavingUtils::SavePackages({ Composed->GetOutermost() }, /*bOnlyDirty*/ false);

	UE_LOG(LogTemplateCharacter, Log,
		TEXT("Composed %s: head from %s, body and skin from %s"),
		*Composed->GetName(), *HeadDonor->GetName(), *BodyDonor->GetName());

	return Composed;
}

#endif // WITH_EDITOR
