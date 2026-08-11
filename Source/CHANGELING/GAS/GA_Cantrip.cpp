// GA_Cantrip.cpp

#include "GA_Cantrip.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "CantripResolver.h"
#include "ChangelingAttributeSet.h"
#include "ChangelingGameplayTags.h"
#include "ChangelingMagicSet.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripAbility, Log, All);

namespace
{
	/** Property name on UChangelingMagicSet backing a Realm. */
	FName RealmPropertyName(ECantripRealm Realm)
	{
		switch (Realm)
		{
		case ECantripRealm::Fae:    return TEXT("Fae");
		case ECantripRealm::Actor:  return TEXT("Actor");
		case ECantripRealm::Nature: return TEXT("Nature");
		case ECantripRealm::Prop:   return TEXT("Prop");
		case ECantripRealm::Time:   return TEXT("Time");
		case ECantripRealm::Scene:  return TEXT("Scene");
		default:                    return NAME_None;
		}
	}

	/**
	 * "Art.Wayfare" -> "Wayfare".
	 *
	 * Derived from the tag rather than kept in a parallel lookup table: a table is
	 * one more thing to forget when the nineteenth Art arrives, and it fails
	 * silently when it drifts.
	 */
	FName ArtPropertyNameFromTag(const FGameplayTag& ArtTag)
	{
		if (!ArtTag.IsValid())
		{
			return NAME_None;
		}

		FString Path = ArtTag.GetTagName().ToString();
		FString Leaf;
		return Path.Split(TEXT("."), nullptr, &Leaf, ESearchCase::CaseSensitive,
			ESearchDir::FromEnd) ? FName(*Leaf) : FName(*Path);
	}
}

UGA_Cantrip::UGA_Cantrip()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// The channel is a state others can see and react to, so it wants a tag.
	ActivationOwnedTags.AddTag(ChangelingTags::Cantrip_State_Casting);
}

void UGA_Cantrip::ReleaseActiveCantrips(AActor* Avatar)
{
	if (!Avatar)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Avatar->FindComponentByClass<UAbilitySystemComponent>();
	if (!ASC)
	{
		// The ASC often lives on the PlayerState rather than the pawn, so fall back
		// to the interface rather than assuming it is a component of this actor.
		if (const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Avatar))
		{
			ASC = Interface->GetAbilitySystemComponent();
		}
	}

	if (!ASC)
	{
		return;
	}

	// Gather BEFORE releasing any of them.
	//
	// Releasing ends the ability, and ending mutates both the ASC's activatable list
	// and the spec's own instance array -- the two containers being iterated here. One
	// channelling cantrip hides it, because the loop exits before the damage shows;
	// two makes it a crash. Weak pointers because an earlier release can cascade and
	// tear down a later one before we reach it.
	TArray<TWeakObjectPtr<UGA_Cantrip>> Channelling;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			if (UGA_Cantrip* Cantrip = Cast<UGA_Cantrip>(Instance))
			{
				Channelling.Add(Cantrip);
			}
		}
	}

	for (const TWeakObjectPtr<UGA_Cantrip>& Cantrip : Channelling)
	{
		if (Cantrip.IsValid())
		{
			Cantrip->ReleaseCantrip();
		}
	}
}

const FCantripSpec* UGA_Cantrip::FindSpec() const
{
	if (!CantripTable || CantripRow.IsNone())
	{
		return nullptr;
	}
	return CantripTable->FindRow<FCantripSpec>(CantripRow, TEXT("Cantrip"), false);
}

float UGA_Cantrip::ReadMagicAttribute(FName PropertyName) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || PropertyName.IsNone())
	{
		return 0.0f;
	}

	FProperty* Property = FindFProperty<FProperty>(UChangelingMagicSet::StaticClass(), PropertyName);
	if (!Property)
	{
		UE_LOG(LogCantripAbility, Warning,
			TEXT("No Art/Realm attribute named '%s' on UChangelingMagicSet."),
			*PropertyName.ToString());
		return 0.0f;
	}

	bool bFound = false;
	const float Value = ASC->GetGameplayAttributeValue(FGameplayAttribute(Property), bFound);
	return bFound ? Value : 0.0f;
}

void UGA_Cantrip::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FCantripSpec* Spec = FindSpec();
	if (!Spec)
	{
		UE_LOG(LogCantripAbility, Warning, TEXT("Cantrip row '%s' not found."),
			*CantripRow.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bReleaseHandled = false;
	CastStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	UE_LOG(LogCantripAbility, Log, TEXT("ACTIVATE %s: row='%s' -> channel open"),
		*GetName(), *CantripRow.ToString());

	OnChannelStarted(*Spec);

	// Cap the channel so a dropped input cannot leave the caster rooted forever.
	// Firing at max hold rather than cancelling: the player held it, they earn it.
	if (GetWorld() && MaxHoldSeconds > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(AutoReleaseTimer,
			FTimerDelegate::CreateUObject(this, &UGA_Cantrip::ReleaseCantrip),
			MaxHoldSeconds, false);
	}
}

void UGA_Cantrip::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	ReleaseCantrip();
}

float UGA_Cantrip::GetHeldSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0f, World->GetTimeSeconds() - CastStartTime) : 0.0f;
}

int32 UGA_Cantrip::GetCurrentCastTier() const
{
	const FCantripSpec* Spec = FindSpec();
	return Spec ? Spec->CastTiers.TierForHold(GetHeldSeconds()) : 0;
}

float UGA_Cantrip::GetTierProgress() const
{
	const FCantripSpec* Spec = FindSpec();
	if (!Spec || Spec->CastTiers.TierThresholds.Num() == 0)
	{
		return 0.0f;
	}

	const TArray<float>& Thresholds = Spec->CastTiers.TierThresholds;
	const float Held = GetHeldSeconds();
	const int32 Tier = Spec->CastTiers.TierForHold(Held);

	if (Tier >= Thresholds.Num())
	{
		return 1.0f;
	}

	const float Lower = (Tier == 0) ? 0.0f : Thresholds[Tier - 1];
	const float Upper = Thresholds[Tier];
	return (Upper > Lower) ? FMath::Clamp((Held - Lower) / (Upper - Lower), 0.0f, 1.0f) : 0.0f;
}

bool UGA_Cantrip::BuildAttempt(const FCantripSpec& Spec, FCantripAttempt& OutAttempt) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return false;
	}

	OutAttempt.ArtRating = FMath::RoundToInt(ReadMagicAttribute(ArtPropertyNameFromTag(Spec.ArtTag)));

	// The pool takes the LOWEST primary Realm in play, so a strong Art reaching
	// through a Realm you barely know is dragged down to that Realm's level.
	// Modifier Realms are skipped entirely -- they extend reach, never power.
	int32 Lowest = TNumericLimits<int32>::Max();
	int32 PrimaryCount = 0;

	for (const ECantripRealm Realm : RealmsUsed)
	{
		if (!UCantripResolver::IsPrimaryRealm(Realm))
		{
			continue;
		}

		++PrimaryCount;
		const int32 Rating = FMath::RoundToInt(ReadMagicAttribute(RealmPropertyName(Realm)));
		Lowest = FMath::Min(Lowest, Rating);
	}

	if (PrimaryCount == 0)
	{
		// No primary Realm means nothing to aim through. Time and Scene alone cannot
		// carry a cantrip.
		return false;
	}

	OutAttempt.LowestPrimaryRealmRating = (Lowest == TNumericLimits<int32>::Max()) ? 0 : Lowest;
	OutAttempt.PrimaryRealmsUsed = PrimaryCount;

	// A Realm rated zero is being "cheated" -- reaching for something you have not
	// learned to hold. Permitted, but the Dreaming charges for it.
	OutAttempt.bCheatedRealm = (OutAttempt.LowestPrimaryRealmRating <= 0);

	bool bFound = false;
	OutAttempt.NightmareRating = FMath::RoundToInt(
		ASC->GetGameplayAttributeValue(UChangelingAttributeSet::GetNightmareAttribute(), bFound));

	const int32 OwnBanality = FMath::RoundToInt(
		ASC->GetGameplayAttributeValue(UChangelingAttributeSet::GetBanalityAttribute(), bFound));

	OutAttempt.BanalityModifier = OwnBanality + GetEnvironmentalBanality();
	OutAttempt.CastTier = GetCurrentCastTier();
	OutAttempt.Manifestation = Spec.Manifestation;

	// "Either" resolves to Wyrd here: a cantrip that could be real usually should be.
	// Expose the choice on the ability later if a stealth build wants the free,
	// invisible version.
	if (OutAttempt.Manifestation == ECantripManifestation::Either)
	{
		OutAttempt.Manifestation = ECantripManifestation::Wyrd;
	}

	return true;
}

void UGA_Cantrip::ReleaseCantrip()
{
	if (bReleaseHandled)
	{
		UE_LOG(LogCantripAbility, Log, TEXT("RELEASE %s: ignored, already handled"), *GetName());
		return;
	}
	bReleaseHandled = true;

	UE_LOG(LogCantripAbility, Log, TEXT("RELEASE %s: held %.2fs -> tier %d"),
		*GetName(), GetHeldSeconds(), GetCurrentCastTier());

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoReleaseTimer);
	}

	const FCantripSpec* Spec = FindSpec();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Spec || !ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	FCantripAttempt Attempt;
	if (!BuildAttempt(*Spec, Attempt))
	{
		UE_LOG(LogCantripAbility, Warning,
			TEXT("  FIZZLE: no usable primary Realm. RealmsUsed must contain at least one "
			     "of Fae/Actor/Nature/Prop."));
		OnCantripFizzled(*Spec);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const int32 Cost = UCantripResolver::ComputeGlamourCost(Attempt);

	bool bFound = false;
	const float Glamour = ASC->GetGameplayAttributeValue(
		UChangelingAttributeSet::GetGlamourAttribute(), bFound);

	if (Glamour < Cost)
	{
		UE_LOG(LogCantripAbility, Warning,
			TEXT("  FIZZLE: Glamour %.0f < cost %d (cheatedRealm=%d realms=%d)"),
			Glamour, Cost, Attempt.bCheatedRealm ? 1 : 0, Attempt.PrimaryRealmsUsed);
		// Not enough essence to make it real. No roll happens -- the book spends
		// Glamour before the dice, so a caster who cannot pay never gets to try.
		OnCantripFizzled(*Spec);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Spent on release regardless of how the roll goes, for the same reason: the
	// Glamour leaves her whether or not the working takes.
	if (Cost > 0 && GlamourCostEffect)
	{
		FGameplayEffectSpecHandle CostSpec =
			MakeOutgoingGameplayEffectSpec(GlamourCostEffect, GetAbilityLevel());
		if (CostSpec.IsValid())
		{
			CostSpec.Data->SetSetByCallerMagnitude(
				ChangelingTags::Cantrip_Data_GlamourCost, static_cast<float>(-Cost));
			ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo,
				CurrentActivationInfo, CostSpec);
		}
	}

	const FCantripOutcome Outcome = UCantripResolver::Resolve(Attempt);

	UE_LOG(LogCantripAbility, Log,
		TEXT("  ROLL art=%d realm=%d pool=%d diff=%d nightmare=%d banality=%d "
		     "-> successes=%d success=%d botch=%d"),
		Attempt.ArtRating, Attempt.LowestPrimaryRealmRating, Outcome.PoolUsed,
		Outcome.DifficultyUsed, Attempt.NightmareRating, Attempt.BanalityModifier,
		Outcome.Successes, Outcome.bSucceeded ? 1 : 0, Outcome.bBotched ? 1 : 0);

	if (Outcome.bSucceeded)
	{
		const TSubclassOf<UGameplayEffect> EffectClass =
			(Outcome.bTainted && Spec->TaintedEffectClass)
				? Spec->TaintedEffectClass
				: Spec->EffectClass;

		if (EffectClass)
		{
			FGameplayEffectSpecHandle EffectSpec =
				MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());

			if (EffectSpec.IsValid())
			{
				// Magnitude comes off the graded degree, not the raw count, so
				// retuning the curve never means reopening every effect asset.
				EffectSpec.Data->SetSetByCallerMagnitude(
					ChangelingTags::Cantrip_State_Tainted,
					static_cast<float>(Outcome.Degree));

				if (AActor* Target = CantripTarget)
				{
					if (UAbilitySystemComponent* TargetASC =
						Target->FindComponentByClass<UAbilitySystemComponent>())
					{
						ASC->ApplyGameplayEffectSpecToTarget(*EffectSpec.Data.Get(), TargetASC);
					}
				}
				else
				{
					ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo,
						CurrentActivationInfo, EffectSpec);
				}
			}
		}
	}

	OnCantripResolved(Outcome, *Spec, CantripTarget);

	PostResolve(Outcome, *Spec);
}

void UGA_Cantrip::PostResolve(const FCantripOutcome& /*Outcome*/, const FCantripSpec& /*Spec*/)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
