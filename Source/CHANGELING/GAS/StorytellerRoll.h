// StorytellerRoll.h
//
// Dice-pool resolution, run entirely behind the scenes.
//
// The player never sees a die, a difficulty, or a success count. What the pool
// buys us is the SHAPE of the odds: the difference between a two-dot trait and a
// four-dot one is a different curve, not a different number, and that is what makes
// an advance feel earned. Callers consume the graded result (see EStorytellerDegree)
// and express it as something that happens in the world — the lock turns, the guard
// looks up, the cantrip lands hard or barely.
//
// Nothing in here should ever be surfaced to UI. FStorytellerRollResult::Dice exists
// for logging and balance passes only.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StorytellerRoll.generated.h"

/**
 * Standard difficulties. The values ARE the target number a d10 must meet or beat,
 * which is why the list starts at 3 rather than 0.
 *
 * Unset exists only because UHT requires a zero entry; it resolves to Standard so a
 * default-constructed request still rolls something sensible rather than difficulty 0.
 */
UENUM(BlueprintType)
enum class EStorytellerDifficulty : uint8
{
	Unset              = 0  UMETA(Hidden),
	Trivial            = 3  UMETA(DisplayName = "Trivial (3)"),
	Easy               = 4  UMETA(DisplayName = "Easy (4)"),
	Straightforward    = 5  UMETA(DisplayName = "Straightforward (5)"),
	Standard           = 6  UMETA(DisplayName = "Standard (6)"),
	Challenging        = 7  UMETA(DisplayName = "Challenging (7)"),
	Difficult          = 8  UMETA(DisplayName = "Difficult (8)"),
	ExtremelyDifficult = 9  UMETA(DisplayName = "Extremely Difficult (9)")
};

/**
 * How well it went. This is the value gameplay should branch on — never the raw
 * success count, so that retuning the curve later does not mean revisiting every
 * caller.
 */
UENUM(BlueprintType)
enum class EStorytellerDegree : uint8
{
	Botch        UMETA(DisplayName = "Botch"),
	Failure      UMETA(DisplayName = "Failure"),
	Marginal     UMETA(DisplayName = "Marginal (1)"),
	Moderate     UMETA(DisplayName = "Moderate (2)"),
	Complete     UMETA(DisplayName = "Complete (3)"),
	Exceptional  UMETA(DisplayName = "Exceptional (4)"),
	Phenomenal   UMETA(DisplayName = "Phenomenal (5+)")
};

USTRUCT(BlueprintType)
struct CHANGELING_API FStorytellerRollRequest
{
	GENERATED_BODY()

	/** Total dice. Built from at most TWO traits — see UStorytellerLibrary::BuildPool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roll", meta = (ClampMin = "0"))
	int32 DicePool = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roll")
	EStorytellerDifficulty Difficulty = EStorytellerDifficulty::Standard;

	/** Extra difficulty from circumstance (wounds, darkness, Banality). Clamped 2..10. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roll")
	int32 DifficultyModifier = 0;

	/** A relevant specialty: 10s count twice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roll")
	bool bSpecialty = false;

	/**
	 * Apply the ten-dice ceiling that binds humans and the Enchanted. Changelings
	 * drawing on potent Treasures and Arts are exempt, as are chimerical horrors —
	 * so this is opt-in per roll rather than a global clamp.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roll")
	bool bHumanLimit = true;

	/** Non-zero makes the roll reproducible. Leave at 0 for live play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roll")
	int32 Seed = 0;
};

USTRUCT(BlueprintType)
struct CHANGELING_API FStorytellerRollResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Roll")
	EStorytellerDegree Degree = EStorytellerDegree::Failure;

	/** Net successes after 1s cancel. Negative only on a botch. */
	UPROPERTY(BlueprintReadOnly, Category = "Roll")
	int32 Successes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Roll")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Roll")
	bool bBotched = false;

	UPROPERTY(BlueprintReadOnly, Category = "Roll")
	int32 RawSuccesses = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Roll")
	int32 Ones = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Roll")
	int32 PoolRolled = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Roll")
	int32 DifficultyUsed = 6;

	/** Individual faces. Diagnostics and balance passes only — never show these. */
	UPROPERTY(BlueprintReadOnly, Category = "Roll|Debug")
	TArray<int32> Dice;
};

UCLASS()
class CHANGELING_API UStorytellerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Rolls the pool and grades it. */
	UFUNCTION(BlueprintCallable, Category = "Changeling|Storyteller")
	static FStorytellerRollResult Roll(const FStorytellerRollRequest& Request);

	/** Convenience form for the common Attribute + Ability case. */
	UFUNCTION(BlueprintCallable, Category = "Changeling|Storyteller",
		meta = (AdvancedDisplay = "3"))
	static FStorytellerRollResult RollTraits(int32 TraitA, int32 TraitB,
		EStorytellerDifficulty Difficulty = EStorytellerDifficulty::Standard,
		int32 DifficultyModifier = 0, bool bSpecialty = false, bool bHumanLimit = true);

	/**
	 * Two parties contest. Successes cancel one-for-one; the returned result belongs
	 * to the challenger, with Successes reduced by the opponent's.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|Storyteller")
	static FStorytellerRollResult RollResisted(const FStorytellerRollRequest& Challenger,
		const FStorytellerRollRequest& Opponent, FStorytellerRollResult& OutOpponentResult);

	/**
	 * Repeats the roll until the target is reached or the attempts run out,
	 * accumulating successes. Returns the number of attempts consumed; a botch
	 * along the way ends it immediately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|Storyteller")
	static int32 RollExtended(const FStorytellerRollRequest& Request, int32 TargetSuccesses,
		int32 MaxAttempts, FStorytellerRollResult& OutFinal);

	/**
	 * Builds a legal pool from up to two traits.
	 *
	 * Enforces the book's two rules that are easy to violate silently: a pool draws
	 * on no more than two Traits, and the 0–10 Traits (Glamour, Banality, Willpower)
	 * stand alone rather than combining with anything.
	 */
	UFUNCTION(BlueprintCallable, Category = "Changeling|Storyteller")
	static int32 BuildPool(int32 TraitA, int32 TraitB, bool bTraitAIsTenScale = false,
		bool bTraitBIsTenScale = false);

	/** Grades a net success count. Exposed so callers can grade a pooled total. */
	UFUNCTION(BlueprintPure, Category = "Changeling|Storyteller")
	static EStorytellerDegree GradeSuccesses(int32 NetSuccesses, bool bBotched);
};
