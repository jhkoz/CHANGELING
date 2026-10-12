// CantripHotbarComponent.h
//
// The ten slots along the bottom of the screen, and what is in them.
//
// Lives on the PLAYER CONTROLLER rather than the character. A changeling who dies has
// not forgotten her Arts, and a hotbar rebuilt from nothing on every respawn would be
// asking the player to lay their spells out again each time they fall off a bridge.
// The controller outlives the pawn; the loadout belongs to the player, not the body.
//
// The source of truth for what CAN be slotted is the ability system itself. Cantrips in
// this game are learned, not picked up -- there is no pickup actor carrying an icon and
// a class around the level -- so the only honest answer to "what can she cast" is to ask
// what her ASC has actually been granted. Everything the UI shows is derived from that
// and from the cantrip's own DataTable row, which means a hotbar cannot drift out of
// step with what the character really knows.

#pragma once

#include "CoreMinimal.h"
#include "CantripTypes.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "CantripHotbarComponent.generated.h"

class ACHANGELINGCharacter;
class UAbilitySystemComponent;
class UDataTable;
class UGameplayAbility;
class UTexture2D;

/**
 * How readily a cantrip comes to this changeling.
 *
 * Deliberately qualitative. The pool and the difficulty are real numbers the resolver
 * uses, but this game never shows the player dice -- so the book says how a working FEELS
 * to reach for, and the arithmetic stays behind the curtain where it belongs.
 */
UENUM(BlueprintType)
enum class ECantripEase : uint8
{
	/** No primary Realm she knows can aim it. Not a matter of difficulty. */
	BeyondReach UMETA(DisplayName = "Beyond her reach"),

	/** She can cast it, but it will cost her. */
	Demanding   UMETA(DisplayName = "Demanding"),

	Comfortable UMETA(DisplayName = "Comfortable"),

	/** Well within her. */
	Effortless  UMETA(DisplayName = "Effortless")
};

/**
 * Everything a widget needs to draw one slot, or one row in a spellbook.
 *
 * A flattened copy rather than a pointer into the DataTable. UMG bindings run every
 * frame they are visible, and handing a widget a raw row pointer invites it to be read
 * after a table reload; this is cheap to copy and cannot dangle.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripSlotDisplay
{
	GENERATED_BODY()

	/** False for an empty slot. Everything below is then meaningless. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	bool bFilled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	FName CantripRow;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	FText Description;

	/** Soft, exactly as the row stores it. A spellbook of fifty cantrips should not
	 *  drag fifty textures into memory to list their names. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * The Art's roundel bearing this cantrip's level in dots -- the hotbar's intended face.
	 *
	 * Unambiguous as an identifier, which is not obvious until you count: eighteen Arts
	 * of five levels is exactly the ninety cantrips, so Art-plus-dots names one and only
	 * one working. The bar can be read at a glance by anyone who has looked at the book,
	 * because it is the same mark in the same place.
	 *
	 * Soft, like Icon, and for the same reason -- Set Brush From Soft Texture takes it
	 * directly, so ten slots cost no synchronous loads. The book's FCantripBookEntry
	 * additionally resolves a hard LevelSymbolTexture because its rows bind to an
	 * Image; nothing on the bar needs that.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	TSoftObjectPtr<UTexture2D> LevelSymbol;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	ECantripElement Element = ECantripElement::None;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	int32 ArtLevel = 0;

	/**
	 * The Art this cantrip belongs to, and its name as the player reads it.
	 *
	 * Carried on the display rather than looked up, because a detail pane or a tooltip
	 * that has one cantrip in hand should not need the whole book to say "Pyretics" --
	 * and GetArtPage rebuilds all ninety rows to answer that question.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	FGameplayTag ArtTag;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	FText ArtDisplayName;

	/**
	 * The Realms this casting is currently aimed through -- her choice, not the
	 * cantrip's permission.
	 *
	 * Her LOWEST primary Realm among these sets the pool, so a strong Art reached
	 * through a Realm she barely knows is a weak casting. That is why the Ease below
	 * changes as she rearranges the chips: it is a verdict on the aim, not on the
	 * cantrip.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	TArray<ECantripRealm> ChosenRealms;

	/** Chimerical costs nothing and is unseen by the mundane; Wyrd costs Glamour. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	ECantripManifestation Manifestation = ECantripManifestation::Wyrd;

	/**
	 * Seconds of holding that buy each step of difficulty reduction.
	 *
	 * The signature mechanic of the whole system, and nothing in the game surfaced it
	 * until now. A ladder of five marks along a line reads it far better than a number.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	TArray<float> CastTierThresholds;

	/** How readily it comes to her. Qualitative on purpose -- no dice are ever shown. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	ECantripEase Ease = ECantripEase::Comfortable;

	/** The book's full passage. Empty unless a Lore Table is set. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	FText FullDescription;

	/** How it resolves. Empty unless a Lore Table is set. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	FText SystemText;

	/**
	 * The cantrip is being held right now.
	 *
	 * Drives the slot's lit style. Read live rather than stored, because a cast can end
	 * for reasons the hotbar never hears about -- an interrupt, a fizzle, a death.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Hotbar")
	bool bChannelling = false;
};

/**
 * One cantrip as the spellbook shows it -- known or not.
 *
 * The book lists all five levels of every Art, including the ones she cannot cast. A
 * spellbook that shows only what you already have is an inventory; what makes it a
 * spellbook is seeing the rung above the one you are standing on.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripBookEntry
{
	GENERATED_BODY()

	/** Name, description, icon, level. AbilityClass is set only when she knows it. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FCantripSlotDisplay Display;

	/**
	 * Her rating in the Art reaches this level -- which is what knowing it MEANS.
	 *
	 * There is no separate learned-list to consult: Pyretics 3 is exactly the first three
	 * Pyretics cantrips. Read from the rating rather than from the granted abilities so
	 * the book states the truth even if the two have drifted apart.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	bool bKnown = false;

	/**
	 * The next rung: one dot in the Art away.
	 *
	 * Exactly one entry per Art is ever true, and it is the only thing on the page she
	 * can act on -- which is worth the book saying outright rather than leaving the
	 * player to infer it from the rating.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	bool bNextToLearn = false;

	/** Hotbar slot holding this cantrip, or -1. Lets the book mark what is already laid out. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	int32 SlotIndex = -1;

	/** The Art's roundel bearing this level's dots. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TSoftObjectPtr<UTexture2D> LevelSymbol;

	/**
	 * The same roundel, already loaded -- what a widget should actually bind to.
	 *
	 * Resolved here so no widget has to Load-then-Cast. That pattern put a blocking
	 * load in every row's Refresh, and worse, its failure branch went nowhere: one
	 * missing texture silently abandoned the rest of the function and the row drew
	 * as empty. Null here simply means no icon, and everything after it still runs.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TObjectPtr<UTexture2D> LevelSymbolTexture;
};

/** One Art's page: its five cantrips, and how far into them she has come. */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripArtPage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FGameplayTag ArtTag;

	/** Leaf of the tag -- "Pyretics". Also the token in the symbol texture's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FName ArtName;

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FText ArtDisplayName;

	/** What this Art is for. Empty until an Art table is set. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FText ArtDescription;

	/** Her dots in this Art, 0-5. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	int32 ArtRating = 0;

	/** How many she knows -- the same number as the rating, kept for convenience. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	int32 KnownCount = 0;

	/** The undotted roundel, for the Art tab. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TSoftObjectPtr<UTexture2D> ArtSymbol;

	/** The same roundel, already loaded. Bind tabs to this, not to the soft pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TObjectPtr<UTexture2D> ArtSymbolTexture;

	/** Ascending by level. Normally five, but only what the table actually holds. */
	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TArray<FCantripBookEntry> Entries;
};

/**
 * The Realms one cantrip is currently aimed through.
 *
 * A struct rather than a bare array because a TMap cannot hold one directly, and the
 * choice needs to be keyed per cantrip -- Hopscotch aimed through Actor and Hopscotch
 * aimed through Nature are the same cantrip pointed at different things, and the player
 * should not have to re-decide every time she opens the page.
 */
USTRUCT(BlueprintType)
struct CHANGELING_API FCantripRealmChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TArray<ECantripRealm> Realms;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCantripHotbarChanged);

/** One cantrip's Realm selection changed. Carries the row so a page can ignore others. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCantripRealmsChanged, FName, CantripRow);

UCLASS(ClassGroup = (Cantrip), meta = (BlueprintSpawnableComponent))
class CHANGELING_API UCantripHotbarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCantripHotbarComponent();

	virtual void BeginPlay() override;

	// ── What the widgets read ───────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "Cantrip|Hotbar")
	int32 GetSlotCount() const { return Slots.Num(); }

	/** One slot's contents. An out-of-range index returns an empty display rather than
	 *  failing, so a widget built for twelve slots against a ten-slot bar still draws. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Hotbar")
	FCantripSlotDisplay GetSlot(int32 Index) const;

	/**
	 * Every cantrip the changeling actually knows, slotted or not.
	 *
	 * This is the spellbook side of the inventory: the list the player drags FROM.
	 * Built from the granted abilities each time it is asked for, so learning one
	 * mid-session needs no cache to be invalidated.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Hotbar")
	TArray<FCantripSlotDisplay> GetKnownCantrips() const;

	/** Which slot holds this cantrip, or -1. Lets the spellbook grey out what is
	 *  already laid out. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Hotbar")
	int32 FindSlotFor(TSubclassOf<UGameplayAbility> AbilityClass) const;

	/** The lowest empty slot, or INDEX_NONE when the bar is full. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Hotbar")
	int32 FindEmptySlot() const;

	/** Whether the bar has room. Never a reason to refuse a cantrip -- see GrantCantrip. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Hotbar")
	bool HasFreeSlot() const { return FindEmptySlot() != INDEX_NONE; }

	/**
	 * Teach her a cantrip: grant it on the ability system, then relay the bar.
	 *
	 * Knowing a cantrip and having it on the bar are separate facts here -- IsKnown reads
	 * the ASC, and the bar is only a set of ten shortcuts into what she knows. So a full
	 * bar is NOT a refusal: she learns it either way and it waits in the spellbook until
	 * the player makes room. Returns false only when she already knew it, or when there
	 * is no ability system to grant it on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Hotbar")
	bool GrantCantrip(TSubclassOf<UGameplayAbility> AbilityClass);

	// ── Arranging ───────────────────────────────────────────────────────────

	/**
	 * Put a cantrip in a slot.
	 *
	 * If it is already in another slot, that one is cleared -- dragging a spell to a new
	 * key MOVES it. Leaving a copy behind would let one cantrip occupy the whole bar,
	 * and no player who dragged something ever meant to duplicate it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Hotbar")
	bool AssignSlot(int32 Index, TSubclassOf<UGameplayAbility> AbilityClass);

	UFUNCTION(BlueprintCallable, Category = "Cantrip|Hotbar")
	bool ClearSlot(int32 Index);

	/** Exchange two slots. The drag-and-drop case where the target is already full. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Hotbar")
	bool SwapSlots(int32 A, int32 B);

	// ── Casting ─────────────────────────────────────────────────────────────

	/**
	 * Begin the cantrip in this slot. Bind to the key's PRESS.
	 *
	 * Press and release are separate because casting in this game is a hold -- the
	 * length of it is the difficulty reduction. A hotbar that only fired on press would
	 * throw away the entire cast-tier mechanic.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Hotbar")
	void PressSlot(int32 Index);

	/**
	 * How long a refused press waits for the working in progress to finish.
	 *
	 * Without this, one cantrip at a time means every key pressed a fraction early is
	 * simply eaten, and the bar feels broken rather than strict. A quarter second is
	 * about the width of a human mistake; much longer and she starts casting things the
	 * player has changed their mind about.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Hotbar")
	float InputBufferSeconds = 0.25f;

	/**
	 * A press that could not be taken and will not be.
	 *
	 * Only fires once the press is genuinely dead -- an empty slot immediately, a blocked
	 * one after its buffered retry has run out of time. A press that is merely early is
	 * held instead, so the bar never flashes a refusal at something it is about to do.
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCantripSlotRefused, int32, SlotIndex);

	UPROPERTY(BlueprintAssignable, Category = "Cantrip|Hotbar")
	FCantripSlotRefused OnSlotRefused;

	/** Let the held cantrip go, rolling at whatever tier the hold reached. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Hotbar")
	void ReleaseSlot(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Cantrip|Hotbar")
	bool IsSlotChannelling(int32 Index) const;

	// ── Keeping up with what she knows ──────────────────────────────────────

	/**
	 * Reconcile the bar with the abilities actually granted.
	 *
	 * Drops slots holding cantrips she no longer has, and -- with auto-fill on -- lays
	 * newly learned ones into the first free slot. Learning a cantrip and then having to
	 * open a menu to place it before it can be cast is a worse first experience than
	 * finding it already on the bar.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Hotbar")
	void RefreshFromKnown();

	/** Fires whenever anything about the bar changes. Widgets rebuild from this rather
	 *  than polling every frame. */
	UPROPERTY(BlueprintAssignable, Category = "Cantrip|Hotbar")
	FCantripHotbarChanged OnHotbarChanged;

	// ── Spellbook ───────────────────────────────────────────────────────────

	/**
	 * Every Art, with its five cantrips, known or not.
	 *
	 * Built from the cantrip DataTable rather than from her granted abilities, because
	 * the whole point of the book is to show what she has NOT learned. The table is
	 * already the complete catalogue -- name, description, icon, Art and level for all
	 * ninety -- so nothing else has to be maintained alongside it.
	 *
	 * Pages come back sorted by Art name; entries within a page ascend by level.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	TArray<FCantripArtPage> GetSpellbook() const;

	/** One Art's page. Empty ArtName if the tag has no rows in the table. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	FCantripArtPage GetArtPage(FGameplayTag ArtTag) const;

	/**
	 * The page the book should fall open on.
	 *
	 * The Art she has gone furthest in, ties broken alphabetically, and the first
	 * page if she has learned nothing at all. A book that opens on whatever happens
	 * to sort first is a book that opens on nothing she is doing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	FCantripArtPage GetOpeningPage() const;

	/**
	 * The cantrip a page should show before she has clicked anything.
	 *
	 * The next rung if there is one, because that is the only row on the page she can
	 * act on; otherwise the first, so the pane is never blank. False only for a page
	 * with no entries at all -- clear the pane on that.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	static bool GetOpeningEntry(const FCantripArtPage& Page, FCantripBookEntry& OutEntry);

	/**
	 * The cantrip's Realms as one readable line -- "Actor, Nature, Prop".
	 *
	 * Here rather than as a Blueprint loop because a widget that has to iterate an
	 * enum array to print a sentence will grow a different sentence in every widget
	 * that does it. Realms she has no dots in are marked, since the LOWEST primary
	 * Realm is what sets the pool: that is the difference between an Art she can lean
	 * on and one she can only just reach, and it is invisible without saying so.
	 *
	 * Empty text for a cantrip with no Realms, which the caller can simply set.
	 *
	 * Takes the array rather than the whole display, because the callers have their
	 * display SPLIT -- a split struct pin hides its parent, so asking for the struct
	 * would mean recombining it and losing every sub-pin already wired. Pair this with
	 * the "Allowed Realms" pin.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	FText DescribeRealms(const TArray<ECantripRealm>& Realms) const;

	/**
	 * The Realm's roundel. Composed, exactly as the Art symbols are.
	 *
	 * Six of them rather than ninety, so the argument for composing over storing is
	 * weaker -- but they are the same kind of asset, named the same way, and a second
	 * convention for the same job is a second convention to remember.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	TSoftObjectPtr<UTexture2D> RealmSymbol(ECantripRealm Realm, int32 Ranks) const;

	// ── Aiming ──────────────────────────────────────────────────────────────
	//
	// Which Realms a cantrip is pointed through is the player's standing decision, not
	// a property of the cantrip. It lives on the component -- which is on the CONTROLLER
	// -- for the same reason the bar does: she has not changed her mind about how she
	// aims Hopscotch because she fell off a bridge.

	/**
	 * The Realms this cantrip is currently aimed through.
	 *
	 * Falls back to a sensible default the first time a page is opened rather than
	 * returning nothing: the single allowed Realm she rates HIGHEST. The pool is the Art
	 * plus her LOWEST Realm among those used, and every Realm past the first costs an
	 * extra Glamour -- so one strong Realm is the cheapest and most effective opening
	 * position, and anything else should be a choice she makes deliberately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	TArray<ECantripRealm> GetChosenRealms(FName CantripRow) const;

	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	bool IsRealmChosen(FName CantripRow, ECantripRealm Realm) const;

	/**
	 * Add or remove one Realm from a cantrip's aim, and remember it.
	 *
	 * Refuses to leave a cantrip aimed through nothing -- unaiming the last Realm would
	 * make it uncastable, and a click that silently breaks the spell is not a choice.
	 * Returns true if the selection actually changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	bool ToggleRealm(FName CantripRow, ECantripRealm Realm);

	/** Forget a cantrip's aim, so the next open falls back to the default again. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip|Spellbook")
	void ClearChosenRealms(FName CantripRow);

	UPROPERTY(BlueprintAssignable, Category = "Cantrip|Spellbook")
	FCantripRealmsChanged OnRealmsChanged;

	/**
	 * Every Realm the player can put into a casting -- all six, on one row.
	 *
	 * The four primary Realms first, then Time and Scene. Those last two never add a
	 * die: they extend what a working can reach, across a duration or a whole place.
	 * They are listed with the others because they are part of the same decision -- the
	 * player is composing one casting, and making her look in two places to do it would
	 * hide that Chronos-through-Scene is a thing she can choose.
	 *
	 * Everything downstream has to keep the distinction even though the UI does not
	 * separate them: the pool ignores modifiers, the default aim never picks one, and a
	 * cantrip cannot be left aimed through modifiers alone.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TArray<ECantripRealm> AimableRealms = {
		ECantripRealm::Fae,  ECantripRealm::Actor,
		ECantripRealm::Nature, ECantripRealm::Prop,
		ECantripRealm::Time, ECantripRealm::Scene };

	/** Her dots in an Art, 0-5. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	int32 GetArtRating(FGameplayTag ArtTag) const;

	/** Her dots in a Realm, 0-5. The book marks which of a cantrip's Realms she knows. */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	int32 GetRealmRating(ECantripRealm Realm) const;

	// ── How a row looks ─────────────────────────────────────────────────────

	/**
	 * Which of the three states this entry is in.
	 *
	 * Reads the two bools the book already computed rather than recomputing from
	 * the rating, so the standing and the entry can never disagree.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	static ECantripStanding StandingFor(const FCantripBookEntry& Entry);

	/**
	 * One row's finished appearance -- every colour and the chip's wording.
	 *
	 * The reason this is here and not in the widget: a row has three looks and five
	 * widgets to paint, and expressing that as Blueprint branches meant fifteen
	 * colour pins that each silently kept a default when left unwired. Resolving it
	 * once means the row's graph is a straight line of setters with nothing to get
	 * wrong, and the palette lives in one place instead of fifteen.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip|Spellbook")
	FCantripRowStyle StyleFor(const FCantripBookEntry& Entry) const;

protected:
	/**
	 * The spellbook's colours. Defaults are the stained-glass scheme; override per
	 * project in the owning Blueprint's class defaults.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FCantripPalette Palette;

	/**
	 * The catalogue. Point this at the same DT_Cantrips the abilities use.
	 *
	 * Required for the spellbook; the hotbar itself works without it, since a granted
	 * ability can describe itself from its own class defaults.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TObjectPtr<UDataTable> CantripTable;

	/**
	 * The eighteen Arts, named and described. Rows of FCantripArtInfo keyed by tag leaf.
	 *
	 * Optional: without it the book still works, falling back to the tag leaf for a name
	 * and showing no description. With it, "DragonsIre" reads as "Dragon's Ire".
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TObjectPtr<UDataTable> ArtTable;

	/**
	 * The long text, rows of FCantripLore keyed as DT_Cantrips is.
	 *
	 * Optional. Without it the book shows the one-line description and nothing more,
	 * which is all the hotbar ever needed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	TObjectPtr<UDataTable> LoreTable;

	/**
	 * Where the Art roundels live, and how they are named.
	 *
	 * Composed rather than stored per row: the textures were generated with one naming
	 * convention for all ninety, and ninety soft references entered by hand is ninety
	 * chances to point a cantrip at the wrong Art's symbol.
	 *
	 * {Art} becomes the tag's leaf, {Level} the cantrip's level.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FString ArtSymbolPath = TEXT("/Game/_Custom/Art/Symbols/Arts");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FString ArtSymbolPattern = TEXT("T_ArtSymbol_{Art}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FString LevelSymbolPattern = TEXT("T_ArtSymbol_{Art}_L{Level}");

	/** Where the six Realm roundels live, and how they are named. {Realm} is the leaf. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FString RealmSymbolPath = TEXT("/Game/_Custom/Art/Symbols/Realms");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FString RealmSymbolPattern = TEXT("T_RealmSymbol_{Realm}");

	/**
	 * The same roundel wearing her rating in dots, exactly as the Art symbols do.
	 *
	 * Rank 0 has no variant and falls back to the plain roundel -- an undotted Realm
	 * reads as "none", which is what a rating of nothing means.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Spellbook")
	FString RealmLevelSymbolPattern = TEXT("T_RealmSymbol_{Realm}_L{Rank}");

	/** Slots on the bar. Ten, to match the number row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Hotbar",
		meta = (ClampMin = "1", ClampMax = "12"))
	int32 SlotCount = 10;

	/** Drop a newly learned cantrip into the first free slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cantrip|Hotbar")
	bool bAutoFillOnLearn = true;

private:
	UFUNCTION() void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void HandleAbilitiesChanged();

	/**
	 * An ability started or stopped. Tells the bar only if it was one of ours.
	 *
	 * bChannelling is read live off the ASC, so the slot displays were always CORRECT --
	 * they were simply never asked again. OnHotbarChanged fired for the four things that
	 * change what is IN the slots and for nothing that changes what they are DOING, so a
	 * cast lit its pane only if some unrelated edit to the bar happened to land mid-hold.
	 */
	void HandleAbilityActiveChanged(UGameplayAbility* Ability);

	/** Start/stop listening for casts on an ASC. Null unbinds and binds nothing. */
	void BindToASC(UAbilitySystemComponent* ASC);

	/**
	 * Broadcast on the next tick rather than right now.
	 *
	 * Required, not tidiness. GameplayAbility::PreActivate calls NotifyAbilityActivated
	 * BEFORE incrementing the spec's ActiveCount -- deliberately, so an ability cannot
	 * cancel itself part-way through coming up -- which means anything reading
	 * IsSlotChannelling from inside the activation callback is told the cast has not
	 * started. A tick later it has. Ending has the opposite order and would be safe
	 * immediately, but both go through here so an activate and an end landing in the
	 * same frame cost one refresh instead of two.
	 */
	void RequestRefresh();
	void FlushRefresh();

	/** Press a slot without buffering on failure. Returns whether it was taken. */
	bool TryPressSlot(int32 Index);

	/** Retry the held press, or give up on it and say so. */
	void FlushBufferedInput();

	/** Fills a display from the ability class's own DataTable row. */
	bool BuildDisplay(TSubclassOf<UGameplayAbility> AbilityClass,
		FCantripSlotDisplay& Out) const;

	ACHANGELINGCharacter* GetChangeling() const;
	UAbilitySystemComponent* GetASC() const;

	bool IsKnown(TSubclassOf<UGameplayAbility> AbilityClass) const;

	/**
	 * How readily a cantrip comes to her, from her Art and the aim she has chosen.
	 *
	 * Takes the ROW rather than the spec now: with every cantrip castable through every
	 * Realm, the difficulty is a fact about her arrangement, not about the cantrip. It
	 * follows that toggling a chip changes the verdict on the page, which is the whole
	 * point of letting her toggle it.
	 */
	ECantripEase EaseFor(FName CantripRow, int32 ArtRating) const;

	/** An Art's display name from the Art table, falling back to the tag's leaf. */
	FText ArtNameFor(FGameplayTag ArtTag) const;

	/** Ability class whose CantripRow matches, or null if she does not know it. */
	TSubclassOf<UGameplayAbility> KnownClassForRow(FName RowName) const;

	/** Builds a soft texture reference from one of the naming patterns above. */
	TSoftObjectPtr<UTexture2D> SymbolFor(const FString& Pattern, FName ArtName,
		int32 Level) const;

	/** Null entries are empty slots. Sized to SlotCount on BeginPlay. */
	UPROPERTY() TArray<TSubclassOf<UGameplayAbility>> Slots;

	/**
	 * Per-cantrip Realm aim, keyed by DataTable row.
	 *
	 * Keyed by ROW rather than by ability class so the choice survives the class being
	 * regranted, and so a cantrip she has not learned yet can still be arranged in the
	 * book before she can cast it. Absent means "never chosen" -- which is different
	 * from an empty array, and is why GetChosenRealms fills in a default instead.
	 */
	UPROPERTY() TMap<FName, FCantripRealmChoice> RealmChoices;

	/** The character we are currently listening to, so the binding can be undone when
	 *  the controller possesses a different body. */
	UPROPERTY() TWeakObjectPtr<ACHANGELINGCharacter> BoundCharacter;

	/** The ability system the two cast delegates below are attached to. Tracked
	 *  separately from the character: by the time a pawn is swapped out we still need
	 *  the OLD ASC to unbind from, and GetASC() has already moved on. */
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	FDelegateHandle AbilityActivatedHandle;
	FDelegateHandle AbilityEndedHandle;

	/** A next-tick refresh is already queued; further requests this frame fold into it. */
	bool bRefreshQueued = false;

	/** The press being held for the current working to end. INDEX_NONE when there is none. */
	int32 BufferedSlot = INDEX_NONE;

	/** When it was pressed, so the hold expires on its own even if nothing ever ends. */
	double BufferedAt = 0.0;

	FTimerHandle BufferTimer;
};
