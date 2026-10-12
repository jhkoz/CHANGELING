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
#include "Styling/SlateColor.h"
#include "CantripTypes.generated.h"

class UAnimSequenceBase;
class UGameplayEffect;
class UTexture2D;

/**
 * How fast she has chosen to move.
 *
 * Three tiers rather than two, and the third is not merely "faster": Sprint is the only one
 * that costs anything. That distinction is the design -- walking and jogging are free and
 * can be held indefinitely, so the stamina bar only ever moves when the player has asked
 * it to.
 *
 * Lives here rather than on the character because the animation layer needs it too, and
 * both already include this header. Putting it on the character would mean dragging the
 * whole character header into the AnimInstance for the sake of one enum.
 */
UENUM(BlueprintType)
enum class EChangelingGait : uint8
{
	Walk   UMETA(DisplayName = "Walk"),
	Jog    UMETA(DisplayName = "Jog"),
	Sprint UMETA(DisplayName = "Sprint")
};

/**
 * Where she is in the arc of a flight.
 *
 * Flight used to be a boolean -- weightless or not -- and a boolean cannot express a windup.
 * The crouch before a launch and the recovery after a landing are both states in which she
 * is neither flying nor simply standing: input is hers again but her feet are not yet under
 * her, and an animation is mid-way through saying so.
 *
 * Here rather than on the character for the same reason the gait is: the AnimInstance needs
 * it, only forward-declares the character, and an enum reached by accident through a unity
 * build compiles until the day the unity grouping changes.
 */
UENUM(BlueprintType)
enum class EChangelingFlightPhase : uint8
{
	/** Feet on the ground, nothing pending. */
	Grounded,

	/** Crouched and gathering. The cantrip has fired but she has not left the ground. */
	Windup,

	/** MOVE_Flying. */
	Airborne,

	/** Out of flight and falling, waiting for ground. */
	Landing,

	/** Down, and playing the impact. Briefly not hers to steer. */
	Recovery
};

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
 * What a cantrip DOES, as opposed to how long it lasts.
 *
 * Orthogonal to ECantripKind: that decides the ability's shape and lifetime -- which C++
 * class the Blueprint derives from -- while this decides what the player reaches for it
 * FOR. A spellbook of ninety workings is unusable without it; sorted by Art alone, the
 * question "what do I have that stops someone running" has no answer short of reading all
 * ninety descriptions.
 *
 * Derived from C20 rather than borrowed from a class-based RPG, which is why Binding and
 * Divination are here and a "tank/healer/dps" triad is not. Ten of the ninety return
 * information and change nothing; four bind a promise the Dreaming itself enforces.
 * Neither has anywhere to sit in the usual taxonomy.
 */
UENUM(BlueprintType)
enum class ECantripRole : uint8
{
	/** Harms directly. The only role that reduces health by itself. */
	Damage         UMETA(DisplayName = "Damage"),

	/** Takes away the target's choices: held, frozen, compelled, terrified. */
	Control        UMETA(DisplayName = "Control"),

	/** Leaves the target able to act, but worse at it. */
	Debuff         UMETA(DisplayName = "Debuff"),

	/** Makes the caster or an ally better at something. */
	Buff           UMETA(DisplayName = "Buff"),

	/** Turns harm aside. Separate from Buff because it answers an attack rather than
	 *  improving a capability -- and the player reaches for it at a different moment. */
	Protection     UMETA(DisplayName = "Protection"),

	/** Restores what was lost: health, life, purity. */
	Healing        UMETA(DisplayName = "Healing"),

	/** Moves a body through space that would otherwise stop it. */
	Mobility       UMETA(DisplayName = "Mobility"),

	/** Returns information and changes nothing. */
	Divination     UMETA(DisplayName = "Divination"),

	/** Lies to the senses. Not Control: it does not stop you, it misleads you. */
	Illusion       UMETA(DisplayName = "Illusion"),

	/** Rewrites the form of a creature or thing. */
	Transformation UMETA(DisplayName = "Transformation"),

	/** Brings matter, light or a creature into being. */
	Creation       UMETA(DisplayName = "Creation"),

	/** Oaths, true names and geasa -- the Dreaming enforcing a promise. */
	Binding        UMETA(DisplayName = "Binding"),

	/** Acts on the world rather than on a character: light, weather, growth, objects. */
	Utility        UMETA(DisplayName = "Utility")
};

/** How the wound is taken, in the book's three grades. */
UENUM(BlueprintType)
enum class ECantripDamageType : uint8
{
	/** Bruises. Soaked by almost anything, heals in hours. */
	Bashing      UMETA(DisplayName = "Bashing"),

	/** Cuts and burns. The ordinary currency of a fight. */
	Lethal       UMETA(DisplayName = "Lethal"),

	/** Wounds that do not simply mend -- cold iron, and fire to a fae thing. */
	Aggravated   UMETA(DisplayName = "Aggravated")
};

/**
 * How a cantrip reaches what it acts on.
 *
 * Separated from the role because they vary independently: a Damage cantrip may be a
 * touch, a look or a thrown fireball, and so may a Healing one. Making delivery a
 * property rather than a class is what keeps thirteen role classes from becoming fifty.
 */
UENUM(BlueprintType)
enum class ECantripDelivery : uint8
{
	/** On the caster. */
	SelfCast       UMETA(DisplayName = "Self"),

	/** Whatever she is looking at, traced from the eyes rather than the camera so a
	 *  cantrip cannot reach round a corner the body is standing behind. */
	AimTrace       UMETA(DisplayName = "Aim Trace"),

	/** Whatever SetCantripTarget was handed -- a dialogue partner, a selected oathmate. */
	ExplicitTarget UMETA(DisplayName = "Explicit Target"),

	/** Thrown. Spawns ACantripProjectile and acts wherever it lands. */
	Projectile     UMETA(DisplayName = "Projectile"),

	/** Everything within a radius of the caster. */
	AreaBurst      UMETA(DisplayName = "Area Burst")
};

/**
 * One Art, described.
 *
 * The cantrip table says what each of the ninety workings does; nothing said what an ART
 * is. That text belongs in data rather than in a widget's variable, because the spellbook,
 * the character sheet and a tooltip all want the same sentence, and three copies of it
 * drift apart the first time one is edited.
 *
 * Keyed by the tag's leaf -- "Pyretics", "DragonsIre" -- so a row and its Art cannot be
 * matched up wrongly.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripArtInfo : public FTableRowBase
{
	GENERATED_BODY()

	/** Properly spelled and punctuated: "Dragon's Ire", not the tag leaf. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	FText DisplayName;

	/** A sentence or two. What this Art is FOR, in the world's terms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art",
		meta = (MultiLine = "true"))
	FText Description;

	/**
	 * Overrides the roundel composed from the naming pattern.
	 *
	 * Left empty for all eighteen at present -- the generated symbols follow the pattern
	 * exactly. It exists for the Art that one day wants a hand-made icon.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UTexture2D> Symbol;
};

/**
 * The book's own words for a cantrip, kept apart from the row that drives play.
 *
 * A SEPARATE table on purpose. DT_Cantrips carries the one-line descriptions the hotbar
 * and the list use, plus Kind, Role and the cast tiers -- all hand-tuned. Adding a long
 * text column there would mean reimporting that table to fill it, and a reimport replaces
 * every row, taking the tuning with it.
 *
 * Keyed by the same row names, so the two are joined by lookup and neither can disturb
 * the other.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripLore : public FTableRowBase
{
	GENERATED_BODY()

	/** The full passage: what the working is, and what it looks like in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lore",
		meta = (MultiLine = "true"))
	FText FullDescription;

	/** How it resolves at the table -- Realms, successes, what each degree buys. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lore",
		meta = (MultiLine = "true"))
	FText SystemText;
};

/** What a resolved casting is acting upon. */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripTargets
{
	GENERATED_BODY()

	/** Everyone caught. Empty for a cantrip that acts on a place rather than a person. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	TArray<TObjectPtr<AActor>> Actors;

	/** Where it landed. Always set, even when Actors is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	FVector Point = FVector::ZeroVector;

	/** Surface normal at that point, for orienting effects. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip")
	FVector Normal = FVector::UpVector;

	/**
	 * The first actor, or null. Convenience for the many cantrips that act on one thing.
	 *
	 * Plain C++ rather than a UFUNCTION: a USTRUCT cannot carry one. Blueprint reads
	 * Actors directly and takes index 0 itself.
	 */
	AActor* Primary() const { return Actors.Num() ? Actors[0] : nullptr; }
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

	// No AllowedRealms here, deliberately (removed 2026-08-29). Every cantrip can be
	// aimed through every Realm -- what changes is the EFFECT, not the permission.
	// Pyretics through Actor sets a person alight; through Prop it lights a torch. A
	// per-cantrip whitelist could only ever have said "all of them", and the column
	// spent ninety rows saying "Actor" instead, which was worse than saying nothing.
	// Which Realms a given casting uses is the player's standing choice, kept per
	// cantrip on UCantripHotbarComponent.

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

	/**
	 * What the player reaches for this cantrip FOR. Drives sorting and filtering.
	 *
	 * Utility is the default because it is the honest answer for an unclassified row --
	 * "it does something to the world" -- rather than a claim the cantrip damages or
	 * heals when nobody has said so.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	ECantripRole Role = ECantripRole::Utility;

	/**
	 * A second role, where one genuinely applies. Thirty-six of the ninety carry one.
	 *
	 * Optional on purpose: Phoenix Song destroys AND resurrects, and flattening that to
	 * either alone loses the cantrip. Set it equal to Role to mean "no secondary" -- a
	 * separate bool would be one more thing to keep in step.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cantrip")
	ECantripRole SecondaryRole = ECantripRole::Utility;

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

	/**
	 * Dice removed by injury. Positive; see FChangelingWoundLevel.
	 *
	 * Takes dice OUT of the pool rather than raising the difficulty, and the two are not
	 * interchangeable. Difficulty is how hard the working is -- a property of the cantrip
	 * and the place, which Banality already speaks to. A wound does not make magic harder;
	 * it makes HER worse at it, and fewer dice is what that means. It also stacks
	 * differently: a bleeding changeling in a shopping centre is short of dice AND facing
	 * a wall, which is exactly as bad as it sounds.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Cantrip")
	int32 WoundPenalty = 0;

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

// ── How the spellbook looks ─────────────────────────────────────────────────────
//
// The book's appearance is decided here rather than in the widget graphs. A row has
// three possible looks and five widgets to paint; wiring that as branches meant
// fifteen colour pins, each of which silently kept its default when left alone. One
// resolved struct means the graph has no branches at all -- and a colour that has
// drifted is a value you can read, not a pin you have to go and find.

/**
 * Where a cantrip stands relative to her rating in its Art.
 *
 * Three states rather than the two bools the entry carries, because exactly one of
 * them decides the row's whole appearance. A pair of bools admits a fourth
 * combination that cannot happen, and every reader then has to decide what to do
 * about it. This does not.
 */
UENUM(BlueprintType)
enum class ECantripStanding : uint8
{
	/** Her rating reaches it: lit glass. */
	Known,
	/** One dot away -- the only row on the page she can act on. */
	NextToLearn,
	/** Past her reach for now: glass with no light behind it. */
	Locked
};

/**
 * Every colour the spellbook paints, in one place.
 *
 * A plain struct on the component rather than a DataAsset, so it cannot be null. A
 * palette that must be assigned is a palette that will one day ship as white boxes,
 * and this project has already paid that price once with the lore table. Defaults
 * compile in and work with no setup; the Blueprint's class defaults can still tune
 * them without a rebuild.
 *
 * Authored as sRGB hex to match what the Designer's colour picker shows, and
 * converted to linear once in the constructor.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripPalette
{
	GENERATED_BODY()

	FCantripPalette();

	// Row backing. Alpha is doing the work here -- these sit over the page ground
	// and are meant to read as a pane catching light, not as a filled bar.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Row")
	FLinearColor RowKnown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Row")
	FLinearColor RowNextToLearn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Row")
	FLinearColor RowLocked;

	// Cantrip name.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Name")
	FLinearColor NameKnown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Name")
	FLinearColor NameNextToLearn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Name")
	FLinearColor NameLocked;

	// The level roundel. Lit is plain white so the artwork's own colour comes
	// through; locked is dimmed rather than tinted, so it reads as unlit glass.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Icon")
	FLinearColor IconLit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Icon")
	FLinearColor IconLocked;

	// The chip at the right of the row.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Chip")
	FLinearColor ChipBound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Chip")
	FLinearColor ChipKnown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Chip")
	FLinearColor ChipNextToLearn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Chip")
	FLinearColor ChipLocked;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Chip")
	FLinearColor ChipTextLit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Chip")
	FLinearColor ChipTextLocked;
};

/**
 * The book's chrome: grounds, leading and ink.
 *
 * Separate from FCantripPalette because the two are read at different times by
 * different things. FCantripPalette answers "what standing is this cantrip in", which
 * only the hotbar can know, at runtime. This answers "what is a spellbook made of",
 * which is fixed -- so the widgets carry it themselves and can paint in the Designer,
 * with no component and no PIE session.
 *
 * A leaded window is three materials, and the book spends them the same way. Came is
 * the dark structure and carries no meaning. Glass is saturated colour, spent only in
 * the Art roundels, which are literally stained glass. Ink is what is written on the
 * page. Gold is light coming through, and is reserved: the Art's progress, a cantrip's
 * reachability, and the ring around the Art she is reading. Colour that appears
 * everywhere stops meaning anything.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripGlass
{
	GENERATED_BODY()

	FCantripGlass();

	// Grounds, darkest first. The rail sits behind the page, the page behind the
	// detail, so each step toward the reader is a shade lighter.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Ground")
	FLinearColor Rail;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Ground")
	FLinearColor Page;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Ground")
	FLinearColor Detail;

	// The leading. One colour and one width everywhere, which is the whole trick --
	// panes read as panes because the line between them never varies.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Came")
	FLinearColor Came;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Came")
	float CameWidth;

	/** Corner radius for a pane. Roundels use a half-height radius instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Came")
	float PaneRadius;

	// Ink, brightest first.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Ink")
	FLinearColor Parchment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Ink")
	FLinearColor Pewter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Ink")
	FLinearColor Muted;

	// Light through the window. Spent sparingly or it stops reading as light.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Light")
	FLinearColor Gold;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Light")
	FLinearColor Amethyst;

	// The five elements, for anything that wants to speak in a cantrip's own colour.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Element")
	FLinearColor Air;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Element")
	FLinearColor Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Element")
	FLinearColor Water;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Element")
	FLinearColor Earth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Glass|Element")
	FLinearColor Wyrd;

	/** The elemental colour for an element, or Wyrd for anything unrecognised. */
	FLinearColor ForElement(ECantripElement Element) const;
};

/**
 * One row's finished appearance: nothing left to decide, only values to apply.
 *
 * The widget's Refresh reads this and calls six setters in a straight line. No
 * branch, no comparison, no colour typed into a pin -- which is the point. The row
 * cannot disagree with the page about what "known" looks like, because neither of
 * them is deciding.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripRowStyle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	ECantripStanding Standing = ECantripStanding::Locked;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FLinearColor RowTint = FLinearColor::Transparent;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FSlateColor NameColor;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FLinearColor IconTint = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FLinearColor ChipTint = FLinearColor::Transparent;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FSlateColor ChipTextColor;

	/**
	 * What the chip says: the hotbar key when she has laid it out, "KNOWN" when she
	 * can cast it but has not, and the rung otherwise.
	 *
	 * Composed here because the alternative -- a Format Text node per state in the
	 * widget -- is three more places for the wording to drift.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FText ChipText;
};
