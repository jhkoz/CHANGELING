// CantripResolver.cpp

#include "CantripResolver.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantrip, Log, All);

bool UCantripResolver::IsPrimaryRealm(ECantripRealm Realm)
{
	// Time and Scene extend what a cantrip can reach -- across a duration, across a
	// whole place -- but they are not things you aim AT, so they add no dice.
	return Realm == ECantripRealm::Fae
		|| Realm == ECantripRealm::Actor
		|| Realm == ECantripRealm::Nature
		|| Realm == ECantripRealm::Prop;
}

int32 UCantripResolver::ComputePool(const FCantripAttempt& Attempt)
{
	// LOWEST, not highest, and not a sum: knowing Actor 5 does not help you when the
	// cantrip also has to reach through a Prop you barely understand. Breadth of
	// Realms is free; it is the weakest link that sets the ceiling.
	const int32 Art = FMath::Max(0, Attempt.ArtRating);
	const int32 Realm = FMath::Max(0, Attempt.LowestPrimaryRealmRating);
	return Art + Realm;
}

int32 UCantripResolver::ComputeDifficulty(const FCantripAttempt& Attempt)
{
	// Casting time is the bunk. Every tier held shaves a point off, to the book's
	// maximum of five -- the difference between a snatched working and a committed
	// piece of theatre.
	const int32 Tier = FMath::Clamp(Attempt.CastTier, 0, 5);
	const int32 Banality = FMath::Max(0, Attempt.BanalityModifier);

	return FMath::Clamp(BaseCantripDifficulty - Tier + Banality, 2, 10);
}

int32 UCantripResolver::ComputeGlamourCost(const FCantripAttempt& Attempt)
{
	if (Attempt.Manifestation == ECantripManifestation::Chimerical)
	{
		// Costs nothing because it changes nothing the mundane world can see. The
		// price of a free cantrip is that it is not real.
		return 0;
	}

	int32 Cost = 1;

	if (Attempt.PrimaryRealmsUsed > 1)
	{
		++Cost;
	}
	if (Attempt.bCheatedRealm)
	{
		++Cost;
	}

	return Cost;
}

FCantripOutcome UCantripResolver::Resolve(const FCantripAttempt& Attempt)
{
	FCantripOutcome Out;

	const int32 Pool = ComputePool(Attempt);
	const int32 Difficulty = ComputeDifficulty(Attempt);

	Out.PoolUsed = Pool;
	Out.DifficultyUsed = Difficulty;
	Out.GlamourCost = ComputeGlamourCost(Attempt);

	// Nightmare REPLACES dice rather than removing them. The pool stays the size it
	// always was, which is exactly why a corrupted changeling does not feel weaker --
	// she feels unpredictable. Those dice still roll, and can still succeed; a
	// success carried by them is a success the Dreaming granted on its own terms.
	const int32 Nightmare = FMath::Clamp(Attempt.NightmareRating, 0, Pool);
	const int32 CleanDice = Pool - Nightmare;

	FStorytellerRollRequest CleanRequest;
	CleanRequest.DicePool = CleanDice;
	CleanRequest.Difficulty = EStorytellerDifficulty::Unset;
	CleanRequest.DifficultyModifier = Difficulty - static_cast<int32>(EStorytellerDifficulty::Standard);
	CleanRequest.bHumanLimit = false;   // Changelings are not bound by the ten-dice cap.

	FStorytellerRollRequest NightmareRequest = CleanRequest;
	NightmareRequest.DicePool = Nightmare;

	const FStorytellerRollResult CleanRoll = UStorytellerLibrary::Roll(CleanRequest);
	const FStorytellerRollResult NightmareRoll = UStorytellerLibrary::Roll(NightmareRequest);

	// Rolled as two pools purely so we can tell which dice carried the result; the
	// arithmetic is identical to rolling them together.
	const int32 RawSuccesses = CleanRoll.RawSuccesses + NightmareRoll.RawSuccesses;
	const int32 Ones = CleanRoll.Ones + NightmareRoll.Ones;
	const int32 Net = RawSuccesses - Ones;

	Out.bBotched = (RawSuccesses == 0 && Ones > 0);
	Out.Successes = FMath::Max(0, Net);
	Out.bSucceeded = (Out.Successes > 0);
	Out.Degree = UStorytellerLibrary::GradeSuccesses(Out.Successes, Out.bBotched);

	// Tainted only when the working actually LANDED on nightmare dice. A failed
	// cantrip is just a failure; it does not need the Dreaming's fingerprints on it.
	Out.bTainted = Out.bSucceeded && (NightmareRoll.RawSuccesses > 0);

	UE_LOG(LogCantrip, Verbose,
		TEXT("cantrip: pool=%d (clean %d + nightmare %d) diff=%d -> successes=%d degree=%d "
		     "tainted=%d botch=%d cost=%d"),
		Pool, CleanDice, Nightmare, Difficulty, Out.Successes,
		static_cast<int32>(Out.Degree), Out.bTainted ? 1 : 0, Out.bBotched ? 1 : 0,
		Out.GlamourCost);

	return Out;
}
