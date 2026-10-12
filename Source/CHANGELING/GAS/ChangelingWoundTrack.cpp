// ChangelingWoundTrack.cpp

#include "ChangelingWoundTrack.h"

namespace
{
	/**
	 * The book's seven levels, as fractions of a full health bar.
	 *
	 * Thresholds sit at sevenths because seven is what the track has: at the default
	 * HealthMax of 7 each point of damage steps exactly one level, so a continuous bar
	 * and a ticked row of boxes agree exactly. Raise HealthMax and the bands simply get
	 * wider -- she takes more damage per level, which is what a tougher character should
	 * mean.
	 *
	 * The penalties are the book's, including the two places it repeats itself: Hurt and
	 * Injured both cost one die, Wounded and Mauled both cost two. That looks like an
	 * error and is not -- the track spends two levels at each step so that the slide from
	 * healthy to helpless is felt as a slope rather than a staircase. Crippled then jumps
	 * to five, which is the point where fighting on stops being a choice.
	 *
	 * Ordered HIGHEST first; the lookup takes the first row it is at or above.
	 */
	const TArray<FChangelingWoundLevel>& WoundTrack()
	{
		static const TArray<FChangelingWoundLevel> Track = []()
		{
			auto Level = [](const FText& Name, int32 Penalty, float MinFraction,
				bool bDown = false)
			{
				FChangelingWoundLevel Out;
				Out.Name = Name;
				Out.DicePenalty = Penalty;
				Out.MinFraction = MinFraction;
				Out.bIncapacitated = bDown;
				return Out;
			};

			const float Seventh = 1.0f / 7.0f;

			return TArray<FChangelingWoundLevel>{
				Level(NSLOCTEXT("Changeling", "Healthy",       "Healthy"),       0, 1.0f),
				Level(NSLOCTEXT("Changeling", "Bruised",       "Bruised"),       0, 6.0f * Seventh),
				Level(NSLOCTEXT("Changeling", "Hurt",          "Hurt"),          1, 5.0f * Seventh),
				Level(NSLOCTEXT("Changeling", "Injured",       "Injured"),       1, 4.0f * Seventh),
				Level(NSLOCTEXT("Changeling", "Wounded",       "Wounded"),       2, 3.0f * Seventh),
				Level(NSLOCTEXT("Changeling", "Mauled",        "Mauled"),        2, 2.0f * Seventh),
				Level(NSLOCTEXT("Changeling", "Crippled",      "Crippled"),      5, 1.0f * Seventh),
				Level(NSLOCTEXT("Changeling", "Incapacitated", "Incapacitated"), 5, 0.0f, true),
			};
		}();

		return Track;
	}
}

FChangelingWoundLevel UChangelingWoundLibrary::GetWoundLevel(float HealthFraction)
{
	const TArray<FChangelingWoundLevel>& Track = WoundTrack();

	// KINDA_SMALL_NUMBER rather than exactly zero: a fluid scale lands on 0.0001 far more
	// often than on 0, and a changeling left standing with a thousandth of a health level
	// is a bug the player experiences as an enemy that will not die.
	if (HealthFraction <= KINDA_SMALL_NUMBER)
	{
		return Track.Last();
	}

	const float Clamped = FMath::Clamp(HealthFraction, 0.0f, 1.0f);

	for (const FChangelingWoundLevel& Level : Track)
	{
		if (Clamped >= Level.MinFraction)
		{
			return Level;
		}
	}

	// Unreachable while the table ends at zero, but a table is a thing someone edits.
	return Track.Last();
}

int32 UChangelingWoundLibrary::GetWoundPenalty(float HealthFraction)
{
	return GetWoundLevel(HealthFraction).DicePenalty;
}

FChangelingWoundLevel UChangelingWoundLibrary::GetWoundLevelFromHealth(float Health,
	float HealthMax)
{
	// No maximum means no track. Answering "Healthy" would be the friendlier guess and
	// the wrong one: an uninitialised attribute set would then silently hand out a
	// penalty-free pool, and the bug would show up as a character who is unaccountably
	// good at everything.
	if (HealthMax <= KINDA_SMALL_NUMBER)
	{
		return GetWoundLevel(0.0f);
	}

	return GetWoundLevel(Health / HealthMax);
}
