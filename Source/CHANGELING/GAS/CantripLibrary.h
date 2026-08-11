// CantripLibrary.h
//
// Asking a character what cantrips it has, and what they look like.
//
// This is the piece a hotbar, a spellbook or a tooltip needs: enumerate what has been
// granted, get each one's name, icon, element and current state. It reads GAS rather
// than keeping a parallel list, so a cantrip granted, removed or blocked is reflected
// immediately and there is no second registry to fall out of step.

#pragma once

#include "CoreMinimal.h"
#include "CantripTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CantripLibrary.generated.h"

class UGA_Cantrip;
class UGameplayAbility;
class UTexture2D;

/** One granted cantrip, flattened for UI. */
USTRUCT(BlueprintType)
struct FCantripEntry
{
	GENERATED_BODY()

	/** The ability class, for passing back into activation. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** Row data: name, icon, element, kind, Art and level. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	FCantripSpec Spec;

	/** Row name in the table, so a caller can match a UI slot back to its cantrip. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	FName RowName;

	/** Mid-cast or currently sustaining. Drives the "lit" state on a hotbar slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bActive = false;

	/**
	 * The caster's rating in this cantrip's Art.
	 *
	 * Worth surfacing because zero is the difference between a cantrip you own and
	 * one you can actually cast -- and a hotbar that shows both identically is how
	 * a player concludes the magic is broken.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	int32 ArtRating = 0;

	/** The Art rating this cantrip needs — its level in the Art's progression. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	int32 RequiredArtLevel = 1;

	/** Castable now: the Art is rated at least this cantrip's level. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bCastable = false;

	/**
	 * Known of, but out of reach — the next rung up.
	 *
	 * Shown deliberately. A player who can see the next cantrip in an Art has a
	 * reason to raise it; one who sees only what they already have finds out about
	 * progression by accident. Showing the WHOLE Art would spend that in one go, so
	 * only the next level surfaces.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bPreview = false;
};

UCLASS()
class CHANGELING_API UCantripLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Every cantrip currently granted to this actor, with its row data resolved.
	 *
	 * Rows that cannot be found are skipped rather than returned blank -- a hotbar
	 * slot with no name is worse than one that is absent, because it looks like a
	 * cantrip you own and cannot use.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip",
		meta = (DefaultToSelf = "Avatar"))
	static TArray<FCantripEntry> GetGrantedCantrips(AActor* Avatar);

	/**
	 * Every cantrip the caster knows of, for a spellbook: everything castable in the
	 * Arts they have, plus the next rung up in each.
	 *
	 * Scans the table rather than the granted list, because a cantrip you cannot yet
	 * cast has not been granted and would otherwise be invisible -- and the whole
	 * point of the preview is to show what raising an Art would buy.
	 *
	 * An Art rated 0 contributes nothing at all: an Art you have never studied should
	 * not advertise its first cantrip, or every character sees all eighteen.
	 *
	 * PreviewLevels of 1 shows one rung ahead. 0 shows only what is castable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip",
		meta = (DefaultToSelf = "Avatar", AdvancedDisplay = "PreviewLevels"))
	static TArray<FCantripEntry> GetKnownCantrips(AActor* Avatar, UDataTable* CantripTable,
		int32 PreviewLevels = 1);

	/** Granted cantrips of one element, for a per-quarter spellbook page. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip",
		meta = (DefaultToSelf = "Avatar"))
	static TArray<FCantripEntry> GetGrantedCantripsByElement(AActor* Avatar,
		ECantripElement Element);

	/**
	 * Dice this cantrip would roll right now: Art rating plus the lowest primary
	 * Realm it would reach through.
	 *
	 * For a spellbook to show reach without ever showing the roll. Zero means the
	 * cantrip cannot succeed at all, which is worth communicating in some form the
	 * player can read.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip",
		meta = (DefaultToSelf = "Avatar"))
	static int32 GetCantripPoolSize(AActor* Avatar, TSubclassOf<UGA_Cantrip> CantripClass);
};
