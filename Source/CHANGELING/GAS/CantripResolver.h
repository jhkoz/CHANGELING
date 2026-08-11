// CantripResolver.h
//
// Turns a cantrip attempt into a graded outcome. Pure logic, no actors, no assets --
// so it can be unit-tested and reasoned about without spinning up a world.
//
// The player never sees any of the numbers this produces. They see a lock turn, a
// leap carry further than it should, or a working curdle in their hands.

#pragma once

#include "CoreMinimal.h"
#include "CantripTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CantripResolver.generated.h"

UCLASS()
class CHANGELING_API UCantripResolver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Base difficulty for every cantrip before the cast tier and Banality move it. */
	static constexpr int32 BaseCantripDifficulty = 8;

	/** Rolls the attempt and grades it. */
	UFUNCTION(BlueprintCallable, Category = "Changeling|Cantrip")
	static FCantripOutcome Resolve(const FCantripAttempt& Attempt);

	/**
	 * Dice before Nightmare eats into them: the Art rating plus the LOWEST primary
	 * Realm used. Modifier Realms (Time, Scene) never contribute.
	 */
	UFUNCTION(BlueprintPure, Category = "Changeling|Cantrip")
	static int32 ComputePool(const FCantripAttempt& Attempt);

	/** Base 8, minus the cast tier, plus Banality. Clamped to the legal 2..10. */
	UFUNCTION(BlueprintPure, Category = "Changeling|Cantrip")
	static int32 ComputeDifficulty(const FCantripAttempt& Attempt);

	/** Chimerical is free; Wyrd costs one, plus one for extra or cheated Realms. */
	UFUNCTION(BlueprintPure, Category = "Changeling|Cantrip")
	static int32 ComputeGlamourCost(const FCantripAttempt& Attempt);

	/** True for the four Realms that supply dice. */
	UFUNCTION(BlueprintPure, Category = "Changeling|Cantrip")
	static bool IsPrimaryRealm(ECantripRealm Realm);
};
