// StorytellerRoll.cpp

#include "StorytellerRoll.h"

DEFINE_LOG_CATEGORY_STATIC(LogStoryteller, Log, All);

namespace
{
	/** Difficulty has a hard floor of 2 and ceiling of 10 however it is modified. */
	int32 ResolveDifficulty(EStorytellerDifficulty Base, int32 Modifier)
	{
		// Unset is the zero entry UHT insists on, not a real difficulty. Treat it as
		// Standard so a default-constructed request rolls against 6 rather than 2.
		const int32 Target = (Base == EStorytellerDifficulty::Unset)
			? static_cast<int32>(EStorytellerDifficulty::Standard)
			: static_cast<int32>(Base);

		return FMath::Clamp(Target + Modifier, 2, 10);
	}
}

FStorytellerRollResult UStorytellerLibrary::Roll(const FStorytellerRollRequest& Request)
{
	FStorytellerRollResult Out;

	int32 Pool = FMath::Max(0, Request.DicePool);
	if (Request.bHumanLimit)
	{
		Pool = FMath::Min(Pool, 10);
	}

	const int32 Difficulty = ResolveDifficulty(Request.Difficulty, Request.DifficultyModifier);
	Out.PoolRolled = Pool;
	Out.DifficultyUsed = Difficulty;

	// A seeded stream when asked, so a balance pass or a test can replay a roll.
	// FRandomStream rather than FMath::Rand: the latter cannot be made reproducible.
	FRandomStream Stream(Request.Seed != 0 ? Request.Seed : FMath::Rand());

	Out.Dice.Reserve(Pool);
	for (int32 i = 0; i < Pool; ++i)
	{
		const int32 Face = Stream.RandRange(1, 10);
		Out.Dice.Add(Face);

		if (Face >= Difficulty)
		{
			++Out.RawSuccesses;

			// A specialty makes tens count twice. Applied only when the ten also
			// cleared the difficulty, which at difficulty 10 is the same thing.
			if (Face == 10 && Request.bSpecialty)
			{
				++Out.RawSuccesses;
			}
		}
		else if (Face == 1)
		{
			// Only a 1 cancels, and only when it did not somehow meet the
			// difficulty — which cannot happen, but the ordering makes it explicit.
			++Out.Ones;
		}
	}

	Out.Successes = Out.RawSuccesses - Out.Ones;

	// A botch is not merely a bad failure: it is rolling ones with nothing to offset
	// them. Successes that are then cancelled still count as HAVING succeeded, so a
	// roll of two successes and three ones is a plain failure, not a catastrophe.
	Out.bBotched = (Out.RawSuccesses == 0 && Out.Ones > 0);
	Out.bSucceeded = (Out.Successes > 0);

	if (!Out.bSucceeded)
	{
		// Nothing below zero is meaningful to callers; clamp so a botch reads as 0.
		Out.Successes = FMath::Max(0, Out.Successes);
	}

	Out.Degree = GradeSuccesses(Out.Successes, Out.bBotched);
	return Out;
}

FStorytellerRollResult UStorytellerLibrary::RollTraits(int32 TraitA, int32 TraitB,
	EStorytellerDifficulty Difficulty, int32 DifficultyModifier,
	bool bSpecialty, bool bHumanLimit)
{
	FStorytellerRollRequest Request;
	Request.DicePool = BuildPool(TraitA, TraitB);
	Request.Difficulty = Difficulty;
	Request.DifficultyModifier = DifficultyModifier;
	Request.bSpecialty = bSpecialty;
	Request.bHumanLimit = bHumanLimit;
	return Roll(Request);
}

FStorytellerRollResult UStorytellerLibrary::RollResisted(
	const FStorytellerRollRequest& Challenger, const FStorytellerRollRequest& Opponent,
	FStorytellerRollResult& OutOpponentResult)
{
	FStorytellerRollResult Mine = Roll(Challenger);
	OutOpponentResult = Roll(Opponent);

	// Cancelling one-for-one is why outstanding results are rare in a contest: both
	// sides have to roll well AND the winner has to out-roll the loser's successes.
	const int32 Net = Mine.Successes - OutOpponentResult.Successes;

	Mine.Successes = FMath::Max(0, Net);
	Mine.bSucceeded = (Net > 0);
	Mine.Degree = GradeSuccesses(Mine.Successes, Mine.bBotched);
	return Mine;
}

int32 UStorytellerLibrary::RollExtended(const FStorytellerRollRequest& Request,
	int32 TargetSuccesses, int32 MaxAttempts, FStorytellerRollResult& OutFinal)
{
	OutFinal = FStorytellerRollResult();
	OutFinal.DifficultyUsed = ResolveDifficulty(Request.Difficulty, Request.DifficultyModifier);

	const int32 Attempts = FMath::Max(1, MaxAttempts);
	int32 Used = 0;

	for (int32 i = 0; i < Attempts; ++i)
	{
		// Reseed per attempt, or a seeded extended action rolls the same dice every
		// time and never converges.
		FStorytellerRollRequest Step = Request;
		if (Step.Seed != 0)
		{
			Step.Seed += i;
		}

		const FStorytellerRollResult StepResult = Roll(Step);
		++Used;

		OutFinal.RawSuccesses += StepResult.RawSuccesses;
		OutFinal.Ones += StepResult.Ones;
		OutFinal.PoolRolled = StepResult.PoolRolled;
		OutFinal.Dice.Append(StepResult.Dice);

		if (StepResult.bBotched)
		{
			// A botch ends an extended action outright — the accumulated progress is
			// lost with it. That risk is the whole tension of taking the slow route.
			OutFinal.Successes = 0;
			OutFinal.bBotched = true;
			OutFinal.bSucceeded = false;
			OutFinal.Degree = EStorytellerDegree::Botch;
			return Used;
		}

		OutFinal.Successes += StepResult.Successes;
		if (OutFinal.Successes >= TargetSuccesses)
		{
			break;
		}
	}

	OutFinal.bSucceeded = (OutFinal.Successes >= TargetSuccesses);
	OutFinal.Degree = GradeSuccesses(OutFinal.Successes, false);
	return Used;
}

int32 UStorytellerLibrary::BuildPool(int32 TraitA, int32 TraitB,
	bool bTraitAIsTenScale, bool bTraitBIsTenScale)
{
	TraitA = FMath::Max(0, TraitA);
	TraitB = FMath::Max(0, TraitB);

	// Glamour, Banality and Willpower run 0-10 and stand alone. Adding an Ability to
	// one of them would produce pools no other roll can reach, so the rule is
	// enforced here rather than trusted to every caller.
	if (bTraitAIsTenScale || bTraitBIsTenScale)
	{
		if (bTraitAIsTenScale && bTraitBIsTenScale)
		{
			UE_LOG(LogStoryteller, Warning,
				TEXT("BuildPool: two 0-10 Traits combined; using the larger alone."));
			return FMath::Max(TraitA, TraitB);
		}
		return bTraitAIsTenScale ? TraitA : TraitB;
	}

	return TraitA + TraitB;
}

EStorytellerDegree UStorytellerLibrary::GradeSuccesses(int32 NetSuccesses, bool bBotched)
{
	if (bBotched)
	{
		return EStorytellerDegree::Botch;
	}

	switch (FMath::Max(0, NetSuccesses))
	{
	case 0:  return EStorytellerDegree::Failure;
	case 1:  return EStorytellerDegree::Marginal;
	case 2:  return EStorytellerDegree::Moderate;
	case 3:  return EStorytellerDegree::Complete;
	case 4:  return EStorytellerDegree::Exceptional;
	default: return EStorytellerDegree::Phenomenal;
	}
}
