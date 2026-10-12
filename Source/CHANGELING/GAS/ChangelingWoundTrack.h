// ChangelingWoundTrack.h
//
// The book's health track, read off a continuous bar.
//
// C20 counts injury in seven named levels, and the NAME is not the point -- the dice
// penalty attached to it is. Bruised costs nothing, Wounded costs two dice, Crippled
// costs five. That penalty is what makes a hurt changeling play differently, and it
// survives perfectly well without discrete levels underneath it: the track becomes a
// function of how much health is left rather than a row of boxes to tick.
//
// So this is one table, and it answers both questions at once. The resolver asks it for
// a dice penalty; the HUD asks it for a name. Two lookups of the same row means the
// number on the bar and the number in the roll can never disagree -- which they would
// within a week if the penalty lived in the ability and the label lived in the widget.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChangelingWoundTrack.generated.h"

/** One band of the health track. */
USTRUCT(BlueprintType)
struct CHANGELING_API FChangelingWoundLevel
{
	GENERATED_BODY()

	/** "Wounded". Shown beside the health bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Wounds")
	FText Name;

	/**
	 * Dice removed from every pool while she is here. Zero or positive; it is SUBTRACTED.
	 *
	 * Stored positive rather than as a negative bonus because every caller wants to
	 * subtract it, and a negative number that is then negated is where sign errors live.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Wounds")
	int32 DicePenalty = 0;

	/** Lowest health fraction, 0-1, that still counts as this level. */
	UPROPERTY(BlueprintReadOnly, Category = "Wounds")
	float MinFraction = 0.0f;

	/** She cannot act at all. Only the bottom of the track. */
	UPROPERTY(BlueprintReadOnly, Category = "Wounds")
	bool bIncapacitated = false;
};

UCLASS()
class CHANGELING_API UChangelingWoundLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The band a given health fraction falls in. Clamped, so any input is answerable.
	 *
	 * Fraction rather than absolute health on purpose: HealthMax is an attribute and can
	 * be raised, and a track keyed to absolute points would quietly get gentler every
	 * time it was. Seven health of ten is the same wound as seven of seven only if you
	 * are counting boxes rather than measuring damage.
	 */
	UFUNCTION(BlueprintPure, Category = "Wounds")
	static FChangelingWoundLevel GetWoundLevel(float HealthFraction);

	/** Just the penalty. The resolver's question. */
	UFUNCTION(BlueprintPure, Category = "Wounds")
	static int32 GetWoundPenalty(float HealthFraction);

	/** Convenience for the two callers that hold Health and HealthMax rather than a
	 *  fraction. A HealthMax of zero answers Incapacitated rather than dividing by it. */
	UFUNCTION(BlueprintPure, Category = "Wounds")
	static FChangelingWoundLevel GetWoundLevelFromHealth(float Health, float HealthMax);
};
