// ChangelingCharacterBase.cpp

#include "ChangelingCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GAS/CantripInternal.h"
#include "GAS/ChangelingAbilityTraitSet.h"
#include "GAS/ChangelingAttributeSet.h"
#include "GAS/ChangelingGameplayTags.h"
#include "GAS/ChangelingMagicSet.h"
#include "GAS/ChangelingMovementSet.h"
#include "GAS/GA_Cantrip.h"
#include "GAS/GE_ChangelingRegen.h"
#include "MetaHumanCharacterInstance.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "CHANGELING.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AChangelingCharacterBase::AChangelingCharacterBase()
{
	// The ability system and its attribute sets. Created here rather than in a
	// Blueprint so every character deriving from this class has a working GAS setup
	// without per-Blueprint wiring -- and so C++ can rely on them existing.
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);

	// Mixed: the owning client predicts its own abilities, while simulated proxies
	// only receive gameplay cues. Right for a player character.
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Attribute sets register with the ASC simply by being constructed as subobjects
	// of the same owner; no explicit AddAttributeSetSubobject call is needed.
	CoreAttributes = CreateDefaultSubobject<UChangelingAttributeSet>(TEXT("CoreAttributes"));
	AbilityTraits  = CreateDefaultSubobject<UChangelingAbilityTraitSet>(TEXT("AbilityTraits"));
	MagicTraits    = CreateDefaultSubobject<UChangelingMagicSet>(TEXT("MagicTraits"));
	MovementTraits = CreateDefaultSubobject<UChangelingMovementSet>(TEXT("MovementTraits"));

	// Defaulted here rather than left to a Blueprint. See PassiveEffects in the header:
	// forgetting these produces a character who plays fine and never recovers, which is
	// invisible until someone thinks to check.
	PassiveEffects.Add(UGE_GlamourRegen::StaticClass());
	PassiveEffects.Add(UGE_HealthRegen::StaticClass());
	PassiveEffects.Add(UGE_StaminaDrain::StaticClass());
	PassiveEffects.Add(UGE_StaminaRegen::StaticClass());
}

UAbilitySystemComponent* AChangelingCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AChangelingCharacterBase::SetCharacterInstance_Implementation(UMetaHumanCharacterInstance* InCharacterInstance)
{
	// Storing it is the whole contract as far as the pipeline is concerned: assembly writes
	// this property on the generated class default object directly, and only calls through
	// here for editor preview and for anything that swaps instances at runtime.
	//
	// Deliberately does NOT assemble components. What the generated Blueprint actually
	// contains -- how the face mesh parents to the body, how the groom components bind, where
	// the face post-process AnimBP is set -- has not been inspected yet, and guessing at it
	// here would produce a second, divergent setup competing with the pipeline's own.
	CharacterInstance = InCharacterInstance;

	UE_LOG(LogTemplateCharacter, Log, TEXT("%s: MetaHuman character instance set to %s"),
		*GetName(),
		InCharacterInstance ? *InCharacterInstance->GetName() : TEXT("none"));
}

void AChangelingCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server side. InitAbilityActorInfo must run before anything is granted, or the
	// abilities have no avatar to act through.
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
	InitialiseAbilitySystem();
}

void AChangelingCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client side. Re-initialising here is what gives the local machine a valid
	// actor info for prediction; without it, client-side ability activation is
	// silently ignored.
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}

void AChangelingCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Covers the standalone case, where a character placed in the level is possessed
	// before BeginPlay and PossessedBy may already have run -- InitialiseAbilitySystem
	// guards against granting twice.
	//
	// It also covers the NPC case, which has no PlayerState and so never sees
	// OnRep_PlayerState at all.
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
	InitialiseAbilitySystem();
}

void AChangelingCharacterBase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Simulated proxies need it too: the raised arm is the whole tell that
	// someone is holding a working open.
	DOREPLIFETIME(AChangelingCharacterBase, CantripPose);
}

void AChangelingCharacterBase::InitialiseAbilitySystem()
{
	if (!AbilitySystem || !HasAuthority())
	{
		return;
	}

	// Granting is idempotent by flag rather than by checking the spec list: a
	// re-possess (respawn, controller swap) would otherwise hand out a second copy of
	// every cantrip, and duplicate specs fail in ways that look like input bugs.
	if (bAbilitiesGranted)
	{
		return;
	}
	bAbilitiesGranted = true;

	if (DefaultAttributesEffect)
	{
		FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec =
			AbilitySystem->MakeOutgoingSpec(DefaultAttributesEffect, 1.0f, Context);

		if (Spec.IsValid())
		{
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

			UE_LOG(LogTemplateCharacter, Log, TEXT("TRAITS: applied %s."),
				*GetNameSafe(DefaultAttributesEffect));
		}
		else
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("TRAITS: could not build a spec from %s -- no dots granted."),
				*GetNameSafe(DefaultAttributesEffect));
		}
	}
	else
	{
		// The abilities below are granted regardless, so a character with this field
		// empty casts perfectly well and fails every roll -- which reads as broken
		// cantrips rather than as a caster with no Arts.
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("TRAITS: no DefaultAttributesEffect set on %s -- every Art and Realm "
				 "will read 0 and every cantrip will fail its roll."),
			*GetName());
	}

	for (int32 Index = 0; Index < DefaultAbilities.Num(); ++Index)
	{
		if (!DefaultAbilities[Index])
		{
			continue;
		}

		// InputID = array index, so binding an input to a cantrip is a matter of
		// where it sits in this list rather than a code change per ability.
		AbilitySystem->GiveAbility(
			FGameplayAbilitySpec(DefaultAbilities[Index], 1, Index, this));
	}

	// After DefaultAbilities, so the ratings the attributes effect just applied decide
	// which cantrips she actually walks around with.
	if (TestingArtRating > 0)
	{
		// Overwrites what the starting effect just set, and says so in the log: a session
		// where every Art reads five is not a bug worth hunting, but it is absolutely a
		// bug worth hunting if nobody remembers switching it on.
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("TRAITS: TestingArtRating is %d -- every Art overridden. "
				 "Set it to 0 on %s for real starting traits."),
			TestingArtRating, *GetName());

		SetAllArts(TestingArtRating);
	}
	else
	{
		SyncCantripsToRatings();
	}

	// Passives last, so recovery starts against the traits the effect above just set --
	// a Glamour regen applied before GlamourMax exists would clamp against a maximum of
	// zero on its first tick.
	for (const TSubclassOf<UGameplayEffect>& Passive : PassiveEffects)
	{
		if (!Passive)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec =
			AbilitySystem->MakeOutgoingSpec(Passive, 1.0f, Context);

		if (Spec.IsValid())
		{
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

			UE_LOG(LogTemplateCharacter, Log, TEXT("PASSIVE: applied %s."),
				*GetNameSafe(Passive));
		}
	}

	// Announced after the whole list, not per ability. The hotbar reconciles its entire
	// contents from one broadcast, and firing per grant would have it auto-fill against
	// a half-populated ability system.
	OnAbilitiesChanged.Broadcast();
}

bool AChangelingCharacterBase::KnowsCantrip(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	return AbilitySystem && AbilityClass
		&& AbilitySystem->FindAbilitySpecFromClass(AbilityClass) != nullptr;
}

int32 AChangelingCharacterBase::GetArtRating(FGameplayTag ArtTag) const
{
	const FName PropertyName = CantripInternal::ArtPropertyNameFromTag(ArtTag);

	if (!AbilitySystem || PropertyName.IsNone())
	{
		return 0;
	}

	FProperty* Property =
		FindFProperty<FProperty>(UChangelingMagicSet::StaticClass(), PropertyName);

	if (!Property)
	{
		return 0;
	}

	bool bFound = false;
	const float Value =
		AbilitySystem->GetGameplayAttributeValue(FGameplayAttribute(Property), bFound);

	return bFound ? FMath::Clamp(FMath::RoundToInt(Value), 0, 5) : 0;
}

int32 AChangelingCharacterBase::RaiseArt(FGameplayTag ArtTag, int32 Delta)
{
	const FName PropertyName = CantripInternal::ArtPropertyNameFromTag(ArtTag);

	if (!AbilitySystem || PropertyName.IsNone() || !HasAuthority())
	{
		return GetArtRating(ArtTag);
	}

	FProperty* Property =
		FindFProperty<FProperty>(UChangelingMagicSet::StaticClass(), PropertyName);

	if (!Property)
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("RAISE: no Art attribute named '%s' -- is the tag right?"),
			*PropertyName.ToString());
		return 0;
	}

	// Five is the book's ceiling for an Art, and nothing below zero is meaningful.
	const int32 Rating = FMath::Clamp(GetArtRating(ArtTag) + Delta, 0, 5);

	// Set on the BASE value rather than applied as an effect: this is a permanent change
	// to who she is, not a modifier that something could later strip.
	AbilitySystem->SetNumericAttributeBase(FGameplayAttribute(Property), Rating);

	UE_LOG(LogTemplateCharacter, Log, TEXT("RAISE %s -> %d."),
		*ArtTag.ToString(), Rating);

	SyncCantripsToRatings();
	return Rating;
}

int32 AChangelingCharacterBase::SetAllArts(int32 Rating)
{
	if (!AbilitySystem)
	{
		return 0;
	}

	Rating = FMath::Clamp(Rating, 0, 5);

	// Gathered from the catalogue, so this covers exactly the Arts the game actually has
	// cantrips for -- no list to keep in step, and an Art nobody has written a cantrip
	// for is correctly left alone.
	TSet<FName> Touched;

	for (const TSubclassOf<UGameplayAbility>& Class : CantripCatalogue)
	{
		const UGA_Cantrip* CDO =
			Class ? Cast<UGA_Cantrip>(Class->GetDefaultObject()) : nullptr;

		const FCantripSpec* Row = CDO ? CDO->GetCantripSpec() : nullptr;
		if (!Row || !Row->ArtTag.IsValid())
		{
			continue;
		}

		const FName PropertyName = CantripInternal::ArtPropertyNameFromTag(Row->ArtTag);

		if (PropertyName.IsNone() || Touched.Contains(PropertyName))
		{
			continue;
		}

		FProperty* Property =
			FindFProperty<FProperty>(UChangelingMagicSet::StaticClass(), PropertyName);

		if (!Property)
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("SetAllArts: no attribute named '%s' -- is the Art tag right?"),
				*PropertyName.ToString());
			continue;
		}

		// The BASE value, not the current one: a modifier applied on top would be
		// recomputed away the next time anything touched the attribute.
		AbilitySystem->SetNumericAttributeBase(FGameplayAttribute(Property),
			static_cast<float>(Rating));

		Touched.Add(PropertyName);
	}

	UE_LOG(LogTemplateCharacter, Log, TEXT("TRAITS: %d Arts set to %d."),
		Touched.Num(), Rating);

	// Grants what the new ratings entitle her to, and takes back what they no longer do
	// -- so SetAllArts 0 really does empty the book rather than leaving it full.
	SyncCantripsToRatings();

	return Touched.Num();
}

void AChangelingCharacterBase::SyncCantripsToRatings()
{
	if (!AbilitySystem || !HasAuthority())
	{
		return;
	}

	int32 Granted = 0;
	int32 Removed = 0;

	for (const TSubclassOf<UGameplayAbility>& Class : CantripCatalogue)
	{
		const UGA_Cantrip* CDO =
			Class ? Cast<UGA_Cantrip>(Class->GetDefaultObject()) : nullptr;

		const FCantripSpec* Row = CDO ? CDO->GetCantripSpec() : nullptr;
		if (!Row || !Row->ArtTag.IsValid())
		{
			continue;
		}

		const bool bShouldKnow = Row->ArtLevel <= GetArtRating(Row->ArtTag);
		FGameplayAbilitySpec* Existing = AbilitySystem->FindAbilitySpecFromClass(Class);

		if (bShouldKnow && !Existing)
		{
			// Input ID continues the current count so a cantrip granted later cannot
			// collide with one already carrying that index.
			AbilitySystem->GiveAbility(FGameplayAbilitySpec(
				Class, 1, AbilitySystem->GetActivatableAbilities().Num(), this));
			++Granted;
		}
		else if (!bShouldKnow && Existing)
		{
			// Cancelled before clearing: an ability removed mid-cast strands its tasks
			// against something that no longer exists.
			const FGameplayAbilitySpecHandle Handle = Existing->Handle;
			AbilitySystem->CancelAbilityHandle(Handle);
			AbilitySystem->ClearAbility(Handle);
			++Removed;
		}
	}

	if (Granted || Removed)
	{
		UE_LOG(LogTemplateCharacter, Log,
			TEXT("CANTRIPS: %d granted, %d removed to match Art ratings."),
			Granted, Removed);

		OnAbilitiesChanged.Broadcast();
	}
}

bool AChangelingCharacterBase::PressCantrip(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilitySystem || !AbilityClass)
	{
		return false;
	}

	FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(AbilityClass);

	if (!Spec)
	{
		/**
		 * Granted here rather than refused.
		 *
		 * Anything sitting in a hotbar pane is already something she is meant to be able
		 * to cast -- the panes are filled from what she knows -- so a missing spec is a
		 * bookkeeping gap, not a decision. It happens whenever a rating changes without
		 * SyncCantripsToRatings running after it, and it used to present as one emblem
		 * out of ten silently doing nothing when clicked, which reads as a broken button
		 * rather than as an ungranted ability.
		 *
		 * Authority only: GiveAbility asserts on a client, and a client pressing a pane
		 * it should not have is a replication problem to solve at the source.
		 */
		if (!AbilitySystem->IsOwnerActorAuthoritative())
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("PressCantrip: %s is not granted and this is not the authority."),
				*GetNameSafe(AbilityClass));
			return false;
		}

		// Input ID continues the current count, as SyncCantripsToRatings does, so a
		// cantrip granted this way cannot collide with one already carrying that index.
		const FGameplayAbilitySpecHandle Handle = AbilitySystem->GiveAbility(
			FGameplayAbilitySpec(AbilityClass, 1,
				AbilitySystem->GetActivatableAbilities().Num(), this));

		Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			UE_LOG(LogTemplateCharacter, Warning,
				TEXT("PressCantrip: %s could not be granted."), *GetNameSafe(AbilityClass));
			return false;
		}

		// Deliberately no OnAbilitiesChanged here. The bar is already showing this
		// cantrip -- that is why it was pressed -- so there is nothing to refresh, and
		// broadcasting mid-press invites a listener to rebuild the ability list out from
		// under the spec pointer this function is about to use.
		UE_LOG(LogTemplateCharacter, Log,
			TEXT("PressCantrip: granted %s on demand."), *GetNameSafe(AbilityClass));
	}

	// The flag matters as much as the activation. An ability that never sees the key go
	// down has nothing to release, and the input-released path below would be talking to
	// a spec that believes the key was never pressed.
	Spec->InputPressed = true;

	if (Spec->IsActive())
	{
		// A second press on a cantrip that has LANDED and is holding ENDS it. The kind is
		// literally called "Sustained (toggle)", and until this existed the toggle only
		// went one way -- a Wind Runner cast committed her to the full duration in the
		// air with no way down, which is a trap rather than a decision.
		//
		// GetRemainingDuration is what tells a held working apart from one still winding
		// up: it reports zero until PostResolve arms the timer. So a second press during
		// the wind-up still reads as a press, and only a press against something already
		// in force cancels it.
		if (CancelHeldCantripsIn(*Spec))
		{
			return true;
		}

		// Already hers and already running: a second press on the same key is a real
		// press, not a refusal, so it must not read as one to the bar.
		AbilitySystem->AbilitySpecInputPressed(*Spec);
		return true;
	}

	return AbilitySystem->TryActivateAbility(Spec->Handle);
}

bool AChangelingCharacterBase::CancelHeldCantripsIn(const FGameplayAbilitySpec& Spec)
{
	// Gathered before any of them is ended, for the reason ReleaseActiveCantrips spells
	// out at length: ending mutates the very instance array being walked. Weak, because
	// ending one can cascade and tear down another before this loop reaches it.
	TArray<TWeakObjectPtr<UGA_Cantrip>> Held;

	for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
	{
		UGA_Cantrip* Cantrip = Cast<UGA_Cantrip>(Instance);

		// Strictly greater than zero: that is the difference between a working in force
		// and one still being cast.
		if (Cantrip && Cantrip->GetRemainingDuration() > 0.0f)
		{
			Held.Add(Cantrip);
		}
	}

	for (const TWeakObjectPtr<UGA_Cantrip>& Cantrip : Held)
	{
		if (Cantrip.IsValid())
		{
			// Not CancelAbility. EndCantrip runs the payload reverts and RevertCantrip
			// first, which is what takes the movement effect back off -- and with it the
			// weightlessness that RefreshLocomotion reads to decide she is still flying.
			Cantrip->EndCantrip();
		}
	}

	return Held.Num() > 0;
}

int32 AChangelingCharacterBase::CancelHeldCantrips()
{
	if (!AbilitySystem)
	{
		return 0;
	}

	// A copy of the handles, not a walk of the live list: ending an ability mutates the
	// activatable array underneath us.
	TArray<FGameplayAbilitySpecHandle> Handles;
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			Handles.Add(Spec.Handle);
		}
	}

	int32 Ended = 0;
	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		if (FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle))
		{
			if (CancelHeldCantripsIn(*Spec))
			{
				++Ended;
			}
		}
	}

	return Ended;
}


bool AChangelingCharacterBase::CancelCantrip(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilitySystem || !AbilityClass)
	{
		return false;
	}

	FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(AbilityClass);
	return Spec ? CancelHeldCantripsIn(*Spec) : false;
}

void AChangelingCharacterBase::ReleaseCantrip(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilitySystem || !AbilityClass)
	{
		return;
	}

	FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(AbilityClass);
	if (!Spec || !Spec->IsActive())
	{
		return;
	}

	Spec->InputPressed = false;
	AbilitySystem->AbilitySpecInputReleased(*Spec);
}
