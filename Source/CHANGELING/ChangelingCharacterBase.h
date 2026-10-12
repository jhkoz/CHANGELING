// ChangelingCharacterBase.h
//
// Everything a changeling IS, without anything about how one is played.
//
// The ability system, the four attribute sets, the cantrip catalogue and the casting path
// live here. Camera, Enhanced Input, gait, sprinting, flight and aiming stay on
// ACHANGELINGCharacter, because every one of them is coupled to a player controller or a
// camera boom that an NPC has no use for.
//
// The split exists because NPCs need to cast. Every piece of cantrip machinery -- UGA_Cantrip,
// the resolver, the payloads, GetCantripPose -- was written against ACHANGELINGCharacter, so a
// gardener derived from plain ACharacter could not cast anything, ever. Rather than give every
// sentry a spring arm and eight input actions it will never bind, the half that is about being
// a changeling moved down here and both halves inherit it.
//
// Moving members between classes preserves Blueprint serialisation as long as their names and
// types do not change, so BP_ThirdPersonCharacter needed no edit and kept every override.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "MetaHumanCharacterActorInterface.h"
#include "GAS/CantripTypes.h"
#include "ChangelingCharacterBase.generated.h"

class UAbilitySystemComponent;
class UMetaHumanCharacterInstance;
class UChangelingAbilityTraitSet;
class UChangelingAttributeSet;
class UChangelingMagicSet;
class UChangelingMovementSet;
class UGameplayAbility;
class UGameplayEffect;
class UGA_Cantrip;
struct FGameplayAbilitySpec;

/** Shared by the player and every NPC, so it is declared with the class they share. */
DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FChangelingAbilitiesChanged);

UCLASS(abstract)
class CHANGELING_API AChangelingCharacterBase : public ACharacter, public IAbilitySystemInterface, public IMetaHumanCharacterActorInterface
{
	GENERATED_BODY()

public:
	AChangelingCharacterBase();

	// ── MetaHuman assembly ──────────────────────────────────────────────────
	//
	// The assembly pipeline writes its Blueprint on top of whatever class is named in
	// UMetaHumanDefaultPipeline::ActorClass, and that property carries
	// meta = (MustImplement = "MetaHumanCharacterActorInterface") -- so a class that does
	// not implement this interface simply never appears in the picker.
	//
	// Implementing it here rather than on a MetaHuman-only subclass means the player and
	// every NPC are both eligible, and assembly produces a Blueprint that already has the
	// ability system, the cantrip machinery and CharacterMovement on it. The alternative --
	// assembling into Epic's actor and reparenting afterwards -- loses to the pipeline on
	// every re-assembly, because it decides whether it may reuse an existing Blueprint with
	// InBlueprint->ParentClass->IsChildOf(ActorClass).
	//
	// A changeling who never came from MetaHuman just never has an instance set. That
	// matters for Childlings, whose proportions MetaHuman cannot currently produce.

	/**
	 * The assembled MetaHuman this body was built from, if any.
	 *
	 * The NAME AND TYPE ARE LOAD-BEARING. UMetaHumanDefaultEditorPipeline::UpdateActorBlueprint
	 * finds this by FindPropertyByName(TEXT("CharacterInstance")) and casts to FObjectProperty,
	 * with an Epic comment noting it only works for TObjectPtr and not TSoftObjectPtr. Rename
	 * it or soften the pointer and the pipeline stops binding the instance -- silently.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MetaHuman")
	TObjectPtr<UMetaHumanCharacterInstance> CharacterInstance;

	/** IMetaHumanCharacterActorInterface. Called by the pipeline and by editor preview. */
	virtual void SetCharacterInstance_Implementation(UMetaHumanCharacterInstance* InCharacterInstance) override;

	// -- Cancelling ----------------------------------------------------------

	/** End one cantrip by class, whether held or landed. Returns whether anything ended. */
	UFUNCTION(BlueprintCallable, Category = "Cantrips")
	bool CancelCantrip(TSubclassOf<UGameplayAbility> AbilityClass);

	/** End every held cantrip at once. Returns how many. A panic key, or a cutscene. */
	UFUNCTION(BlueprintCallable, Category = "Cantrips")
	int32 CancelHeldCantrips();

	/**
	 * Whether the casting hand is up against something and the reach IK has given up.
	 *
	 * Written by the player's hand probe; an NPC simply never sets it. Declared here because
	 * the AnimBP reads it off whatever character it is driving, and that is now the base.
	 */
	UFUNCTION(BlueprintPure, Category = "Cantrip")
	bool IsCantripHandBlocked() const { return bCantripHandBlocked; }

protected:
	/** Ends every LANDED cantrip in one spec. Returns whether anything was ended. */
	bool CancelHeldCantripsIn(const struct FGameplayAbilitySpec& Spec);

	/** Grants the default abilities and applies the passive effects. Guards against
	 *  running twice, which possession and BeginPlay between them will try to do. */
	void InitialiseAbilitySystem();

	bool bCantripHandBlocked = false;

	/** Whether the grant has already happened. Possession and BeginPlay will both try. */
	bool bAbilitiesGranted = false;

public:
	// ── Gameplay Ability System ─────────────────────────────────────────────

	/** IAbilitySystemInterface. Lets any GAS code find this character's ASC. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	/** Attributes, pools and Health. */
	UPROPERTY()
	TObjectPtr<UChangelingAttributeSet> CoreAttributes;

	/** Talents, Skills and Knowledges. */
	UPROPERTY()
	TObjectPtr<UChangelingAbilityTraitSet> AbilityTraits;

	/** Art and Realm ratings. */
	UPROPERTY()
	TObjectPtr<UChangelingMagicSet> MagicTraits;

	/** Speed, jump, gravity and air-control multipliers. See RefreshLocomotion. */
	UPROPERTY()
	TObjectPtr<UChangelingMovementSet> MovementTraits;

	/**
	 * Abilities granted on possession — cantrips go here.
	 *
	 * The array index becomes the ability's InputID, so the first entry binds to
	 * input 0, the second to 1, and so on. That keeps input binding a matter of
	 * ordering this list rather than editing code for every new cantrip.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/**
	 * Infinite effects applied once at startup. Recovery lives here.
	 *
	 * Defaulted in C++ to the Glamour and health regens rather than left for a Blueprint
	 * to remember, because the failure mode is silent: a character with this empty plays
	 * perfectly well and simply never recovers, which reads as a balance problem for as
	 * long as it takes someone to think of checking.
	 *
	 * Clear it on a character that genuinely should not heal -- a chimera, a corpse.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;

	/** Applied once on possession to set starting Attributes, Arts and Realms. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffect;

	/**
	 * Every cantrip ability class in the game, in any order.
	 *
	 * The catalogue SyncCantripsToRatings works from: each class names its Art and level
	 * in its own DataTable row, so this list only has to be complete, never ordered or
	 * grouped. Anything missing here is a cantrip her rating entitles her to that she
	 * will never actually be granted.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> CantripCatalogue;

	/**
	 * Testing only: raise every Art to this on possession. Zero leaves the effect alone.
	 *
	 * Here rather than in GE_StartingTraits because that effect describes a character,
	 * and eighteen Arts at five is not a character -- it is a way to reach all ninety
	 * cantrips at once. Kept apart so turning it off is one number rather than unpicking
	 * eighteen modifiers from the thing that says who she started as, and so a build that
	 * shipped with it on is a one-line diff rather than an archaeology problem.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Testing",
		meta = (ClampMin = "0", ClampMax = "5"))
	int32 TestingArtRating = 0;

public:
	/**
	 * Begin a cantrip and hold it. Bind to the key's PRESS.
	 *
	 * Activating with TryActivateAbilityByClass alone leaves the ability with no idea
	 * the key is down, so it never hears the release and every cast runs to
	 * MaxHoldSeconds before rolling. The wind-up is the whole tier mechanic; without a
	 * release it is an eight-second delay pretending to be one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	/** Returns whether the press was taken -- false means blocked, uncastable, unknown. */
	bool PressCantrip(TSubclassOf<UGameplayAbility> AbilityClass);

	/** Let a held cantrip go, rolling at whatever tier the hold reached. Bind to the
	 *  key's RELEASE. */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void ReleaseCantrip(TSubclassOf<UGameplayAbility> AbilityClass);

	/**
	 * Her rating in an Art, 0-5 -- which IS the set of cantrips she knows.
	 *
	 * A changeling with Pyretics 3 knows the first three Pyretics cantrips. There is no
	 * separate list of learned workings: the rating is the whole answer, and the cantrips
	 * below it are what having that rating MEANS.
	 */
	UFUNCTION(BlueprintPure, Category = "Abilities")
	int32 GetArtRating(FGameplayTag ArtTag) const;

	UFUNCTION(BlueprintPure, Category = "Abilities")
	bool KnowsCantrip(TSubclassOf<UGameplayAbility> AbilityClass) const;

	/**
	 * Raise an Art and grant what the new rating brings with it.
	 *
	 * This is how a cantrip is learned: not one working at a time, but a dot in the Art,
	 * which opens the next rung by definition. Returns the new rating.
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	int32 RaiseArt(FGameplayTag ArtTag, int32 Delta = 1);

	/**
	 * Set every Art she has cantrips for to one rating, and re-sync. Returns Arts touched.
	 *
	 * Exec as well as BlueprintCallable, so it can be typed at the console mid-session:
	 * `SetAllArts 5` to see the whole book, `SetAllArts 0` to put it back.
	 *
	 * The list of Arts is read out of the catalogue rather than written down here. Any
	 * hardcoded eighteen would be one more place to edit when the nineteenth arrives, and
	 * it would go wrong silently -- the missing Art would simply never be granted, which
	 * looks exactly like a cantrip that was never authored.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Abilities|Testing")
	int32 SetAllArts(int32 Rating);

	/**
	 * Make the granted cantrips agree with her Art ratings.
	 *
	 * Grants everything at or below each rating and clears anything above it, so a rating
	 * changed by any route -- advancement, a Banality crisis, a Gameplay Effect nobody
	 * here knows about -- still leaves her able to cast exactly what she should.
	 *
	 * Called on possession and after RaiseArt. Call it yourself after anything else that
	 * moves an Art attribute.
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void SyncCantripsToRatings();

		UPROPERTY(BlueprintAssignable, Category = "Abilities")
	FChangelingAbilitiesChanged OnAbilitiesChanged;

protected:

public:
	/**
	 * Body attitude forced by whatever sustained cantrip is running.
	 *
	 * The AnimBP reads this to drive its Modify Bone nodes. Replicated so other
	 * clients see the raised arm too -- a torch nobody else can see being held is
	 * worse than no torch.
	 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Cantrip")
	ECantripPose GetCantripPose() const { return CantripPose; }

	/** Set by the sustained cantrip ability; not intended for direct use. */
	UFUNCTION(BlueprintCallable, Category = "Cantrip")
	void SetCantripPose(ECantripPose NewPose) { CantripPose = NewPose; }

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Cantrip")
	ECantripPose CantripPose = ECantripPose::None;

};
