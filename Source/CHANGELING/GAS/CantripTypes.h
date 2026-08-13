// CantripTypes.h
//
// Data shapes for casting a cantrip.
//
// The book asks the player to invent a "bunk" -- a performative, often humiliating
// act that makes the magic easier -- and rates it 1 to 5. We have replaced that with
// CASTING TIME. Holding the cast IS the performance: a tap is a snatched, unreliable
// working; a long hold is the changeling committing to the theatre of it. Same
// mechanic, same risk/reward, no authoring surface and no menu.
//
// Release fires at whatever tier has been reached, so an interruption does not waste
// the input -- it just casts weaker. The tension is "dare I hold one more beat".

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "StorytellerRoll.h"
#include "CantripTypes.generated.h"

class UAnimSequenceBase;
class UGameplayEffect;

/** Chimerical costs nothing but exists only for the enchanted; Wyrd costs Glamour
 *  and is real in both worlds. Some cantrips let the caster choose. */
UENUM(BlueprintType)
enum class ECantripManifestation : uint8
{
	Chimerical  UMETA(DisplayName = "Chimerical (free, unseen by the mundane)"),
	Wyrd        UMETA(DisplayName = "Wyrd (1 Glamour, real)"),
	Either      UMETA(DisplayName = "Caster's choice")
};

/** The four Realms that contribute dice, plus the two that only extend reach. */
/**
 * Body attitude a sustained cantrip forces while it runs.
 *
 * Read by the AnimBP to drive Modify Bone nodes, rather than the AnimBP casting to
 * the character and reading a pile of loose bools. One enum means a second sustained
 * cantrip needing the same raised hand costs nothing, and one needing a different
 * attitude adds a value here instead of another variable everywhere.
 */
UENUM(BlueprintType)
enum class ECantripPose : uint8
{
	None            UMETA(DisplayName = "None"),
	RightHandRaised UMETA(DisplayName = "Right Hand Raised"),
	LeftHandRaised  UMETA(DisplayName = "Left Hand Raised"),
	BothHandsRaised UMETA(DisplayName = "Both Hands Raised")
};

/**
 * Elemental quarter a cantrip belongs to.
 *
 * Not a rules concept -- the book's Arts are not elemental. This is the WORLD's
 * scheme: the garden's four quarters and their directional metals, with the centre
 * as the axle that turns them. Kept as metadata so a hotbar can group by it and a
 * place can favour its own element, without the resolver ever caring.
 */
UENUM(BlueprintType)
enum class ECantripElement : uint8
{
	None         UMETA(DisplayName = "None"),
	Air          UMETA(DisplayName = "Air (East / Spring)"),
	Fire         UMETA(DisplayName = "Fire (South / Summer)"),
	Water        UMETA(DisplayName = "Water (West / Autumn)"),
	Earth        UMETA(DisplayName = "Earth (North / Winter)"),
	Quintessence UMETA(DisplayName = "Quintessence (Centre)")
};

/**
 * How a cantrip behaves once cast. Declares which ability class a row expects, so a
 * row and its ability cannot silently disagree about their own shape.
 */
UENUM(BlueprintType)
enum class ECantripKind : uint8
{
	Instant   UMETA(DisplayName = "Instant"),
	Sustained UMETA(DisplayName = "Sustained (toggle)"),
	Channelled UMETA(DisplayName = "Channelled (held)"),
	Summon    UMETA(DisplayName = "Summon")
};

/**
 * What decides which way a projected working points.
 *
 * Separated from the socket that decides WHERE it starts, because those are genuinely
 * different questions and the hand answers only the first one well. A wrist bone's axes
 * are an accident of how the skeleton was authored, and an arm's animation swings them
 * around while the caster stands still.
 */
UENUM(BlueprintType)
enum class ECantripAimSource : uint8
{
	/** The socket's own rotation. Right for a torch, which should follow the hand. */
	Socket        UMETA(DisplayName = "Socket (follows the hand)"),

	/** The way the body faces. Steady, and ignores what the arms are doing. */
	ActorForward  UMETA(DisplayName = "Body facing"),

	/**
	 * Where the caster is LOOKING -- camera for a player, focus for AI.
	 *
	 * Read from the pawn's base aim rotation rather than the controller directly, so
	 * an AI caster aims by the same rule a player does instead of needing its own path.
	 */
	ViewDirection UMETA(DisplayName = "Eyeline (aims where you look)")
};

UENUM(BlueprintType)
enum class ECantripRealm : uint8
{
	None    = 0,
	Fae     = 1,
	Actor   = 2,
	Nature  = 3,
	Prop    = 4,
	// Modifier Realms below this line never contribute dice.
	Time    = 5,
	Scene   = 6
};

/**
 * The two moments in a casting animation that the ability needs to hear about.
 *
 * Both are placed on the animation rather than guessed at with a timer. A timer that
 * says "the arms are up by now" is a timer that lies the moment the animation is
 * retimed, played at a different rate, or blended out of early -- and the failure is
 * silent, because a flame that starts a fifth of a second wrong still starts.
 */
UENUM(BlueprintType)
enum class ECantripAnimEvent : uint8
{
	/** The gesture has completed and the working should become visible. */
	EffectStart UMETA(DisplayName = "Effect Start (gesture complete)"),

	/** The recovery is finished; the body is its own again. Releases the movement lock. */
	Recovered   UMETA(DisplayName = "Recovered (body released)")
};

/**
 * The three clips a cantrip animates through, held as data.
 *
 * Ninety cantrips cannot each have their own state in the graph. One set of states
 * plays whichever sequences the running cantrip names, so adding a cantrip's animation
 * is filling in three slots on a row rather than reopening the AnimBP.
 *
 * All three are optional. A cantrip with none set casts on the idle pose -- correct
 * behaviour while the animations are still being made, rather than a T-pose or a
 * silent refusal to cast.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripAnimSet
{
	GENERATED_BODY()

	/** The wind-up. Plays once; the loop takes over as it ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Animation")
	TObjectPtr<UAnimSequenceBase> Begin;

	/** Held for as long as the cantrip runs. Needs to loop cleanly, or to be short
	 *  enough that the graph's ping-pong between two copies hides the seam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Animation")
	TObjectPtr<UAnimSequenceBase> Loop;

	/** The recovery. Plays once on release, and its end releases the movement lock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Animation")
	TObjectPtr<UAnimSequenceBase> End;

	bool IsSet() const { return Begin || Loop || End; }
};

/**
 * How the cast-time hold converts to a difficulty reduction.
 *
 * Thresholds are the seconds held to REACH each tier. Releasing between thresholds
 * casts at the lower tier -- there is no partial credit, so the player learns the
 * rhythm of the beats rather than shaving milliseconds.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripCastTiers
{
	GENERATED_BODY()

	/** Seconds to reach −1, −2, −3, −4, −5. Below the first, the cast is a tap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
	TArray<float> TierThresholds = { 0.5f, 1.1f, 1.9f, 2.9f, 4.2f };

	/** Difficulty reduction earned per tier reached. Caps at the book's −5. */
	int32 TierForHold(float HeldSeconds) const
	{
		int32 Tier = 0;
		for (const float T : TierThresholds)
		{
			if (HeldSeconds >= T) { ++Tier; } else { break; }
		}
		return FMath::Min(Tier, 5);
	}

	float FullHoldSeconds() const
	{
		return TierThresholds.Num() ? TierThresholds.Last() : 0.0f;
	}
};

/** One cantrip: an Art at a level. Rows are keyed "<Art>_<Level>", e.g. Wayfare_3. */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripSpec : public FTableRowBase
{
	GENERATED_BODY()

	/** Identifies the Art. Drives which Art attribute supplies the dice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	FGameplayTag ArtTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip",
		meta = (ClampMin = "1", ClampMax = "5"))
	int32 ArtLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	ECantripManifestation Manifestation = ECantripManifestation::Wyrd;

	/** Realms this cantrip can legitimately be aimed through. The caster's own
	 *  ratings decide which of these are actually available to her. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	TArray<ECantripRealm> AllowedRealms = { ECantripRealm::Actor };

	/**
	 * Effect applied on success. Magnitude should read the graded outcome rather
	 * than the raw success count, so retuning the curve never means reopening every
	 * effect asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	TSubclassOf<UGameplayEffect> EffectClass;

	/** Applied instead when nightmare dice carried the roll. Optional: without one,
	 *  a tainted success simply applies EffectClass and tags the result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	TSubclassOf<UGameplayEffect> TaintedEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	FCantripCastTiers CastTiers;

	/** Begin/Loop/End clips this cantrip animates through. Optional while they are
	 *  still being made; the cast simply plays on the idle pose until they exist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Animation")
	FCantripAnimSet Anims;

	/** Cues for the escalating cast and the release. */
	/**
	 * Presentation metadata. None of this reaches the resolver -- it exists so a
	 * hotbar, a spellbook or a tooltip can describe a cantrip without a second
	 * table that would drift out of step with this one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Presentation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Presentation",
		meta = (MultiLine = "true"))
	FText Description;

	/** Soft, so a spellbook of fifty cantrips does not drag fifty textures into
	 *  memory just to know their names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Presentation")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Presentation")
	ECantripElement Element = ECantripElement::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	ECantripKind Kind = ECantripKind::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Presentation")
	FGameplayTag ChannelCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip|Presentation")
	FGameplayTag ReleaseCue;
};

/** Everything the resolver needs, assembled by the ability before rolling. */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripAttempt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	int32 ArtRating = 0;

	/** Rating of the LOWEST primary Realm being used. Modifier Realms excluded. */
	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	int32 LowestPrimaryRealmRating = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	int32 NightmareRating = 0;

	/** Personal Banality plus whatever the place imposes. Raises difficulty. */
	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	int32 BanalityModifier = 0;

	/** Tier reached by holding the cast, 0-5. Subtracted from the base difficulty. */
	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	int32 CastTier = 0;

	/** Aiming through a Realm the caster lacks: legal, but it costs extra Glamour. */
	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	bool bCheatedRealm = false;

	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	int32 PrimaryRealmsUsed = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	ECantripManifestation Manifestation = ECantripManifestation::Wyrd;
};

/** Graded outcome. Nothing here should reach the player as a number. */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripOutcome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	EStorytellerDegree Degree = EStorytellerDegree::Failure;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bBotched = false;

	/** At least one success came from a nightmare die. The Dreaming answered, but
	 *  not kindly -- swap in the tainted effect and hand the caster a Nightmare. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	bool bTainted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	int32 Successes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	int32 GlamourCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	int32 DifficultyUsed = 8;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	int32 PoolUsed = 0;
};
