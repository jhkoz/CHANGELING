// GA_Cantrip.cpp

#include "GA_Cantrip.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimMontage.h"
#include "CantripInternal.h"
#include "CHANGELINGCharacter.h"
#include "AbilitySystemInterface.h"
#include "CantripPayload.h"
#include "CantripResolver.h"
#include "Engine/OverlapResult.h"
#include "ChangelingAttributeSet.h"
#include "ChangelingGameplayTags.h"
#include "ChangelingMagicSet.h"
#include "ChangelingWoundTrack.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCantripAbility, Log, All);

using CantripInternal::RealmPropertyName;
using CantripInternal::ArtPropertyNameFromTag;

UGA_Cantrip::UGA_Cantrip()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// A working in force is a state others can see and react to, so it wants a tag, and
	// GAS keeps this one for the ability's whole lifetime. It blocks nothing.
	ActivationOwnedTags.AddTag(ChangelingTags::Cantrip_State_Held);

	// What blocks is the WIND-UP, and only the wind-up: she cannot throw a second cantrip
	// out of a hand that is still finishing the first. Cantrip.State.Casting is applied
	// and dropped by TakeCastingTag/DropCastingTag rather than owned, because a Sustained
	// working has to stop blocking the moment it lands -- see the note in the header.
	//
	// These were one tag until Hopscotch. A cantrip that holds for thirty seconds kept
	// the casting tag for all thirty, so landing a speed or bounding cantrip switched off
	// the entire rest of the spellbook until it expired -- and the one thing such a
	// cantrip is FOR is doing something else while it runs.
	ActivationBlockedTags.AddTag(ChangelingTags::Cantrip_State_Casting);
}

void UGA_Cantrip::TakeCastingTag()
{
	if (bCastingTagHeld)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(ChangelingTags::Cantrip_State_Casting);
		bCastingTagHeld = true;
	}
}

void UGA_Cantrip::DropCastingTag()
{
	if (!bCastingTagHeld)
	{
		return;
	}

	// Cleared before the removal, not after: if the ASC has gone (avatar destroyed
	// mid-cast) the tag is gone with it, and leaving the flag raised would make the
	// EndAbility drop try again on a null pointer's behalf.
	bCastingTagHeld = false;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(ChangelingTags::Cantrip_State_Casting);
	}
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

	// Each zero says why it is zero. A pool of nothing can mean the caster has no dots,
	// or that the Art tag on the row is empty, or that the attribute never got granted --
	// three different problems that look identical in the roll line.
	if (!ASC)
	{
		UE_LOG(LogCantripAbility, Warning,
			TEXT("ATTR '%s': no ability system component."), *PropertyName.ToString());
		return 0.0f;
	}

	if (PropertyName.IsNone())
	{
		UE_LOG(LogCantripAbility, Warning,
			TEXT("ATTR: empty property name -- the Art tag on the row is unset, or the "
				 "ability lists no primary Realm."));
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

	// bFound false means the attribute set is not on the component at all; a found zero
	// means it is there and the caster simply has no dots. The first is a wiring fault,
	// the second is a character sheet.
	if (!bFound)
	{
		UE_LOG(LogCantripAbility, Warning,
			TEXT("ATTR '%s': attribute set not present on the component."),
			*PropertyName.ToString());
		return 0.0f;
	}

	UE_LOG(LogCantripAbility, Verbose, TEXT("ATTR '%s' = %.1f"),
		*PropertyName.ToString(), Value);

	if (Value <= 0.0f)
	{
		UE_LOG(LogCantripAbility, Warning,
			TEXT("ATTR '%s' reads 0 -- the attribute exists but the caster has no dots. "
				 "Check GE_StartingTraits is applied."),
			*PropertyName.ToString());
	}

	return Value;
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

	// Both hands are busy from here until the working lands. Taken after the row lookup,
	// so a cantrip that cannot find its own data does not block the next one on its way
	// out the door.
	TakeCastingTag();

	UE_LOG(LogCantripAbility, Log, TEXT("ACTIVATE %s: row='%s' -> channel open"),
		*GetName(), *CantripRow.ToString());

	OnChannelStarted(*Spec);

	if (bLockMovementWhileCasting && ShouldTakeRootOnActivate())
	{
		TakeCastMovementLock();
	}

	// From activation, not from the roll: the stance has to be held while she winds up,
	// or the spine only begins turning once the working is already open.
	if (bAimWhileCasting)
	{
		TakeCastAim();
	}

	if (ShouldPlayCastMontageOnActivate())
	{
		PlayCastMontage();
	}

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

void UGA_Cantrip::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Every exit runs through here -- resolved, fizzled, cancelled, interrupted, avatar
	// destroyed mid-gesture -- which is the whole reason the release lives at this end
	// rather than beside the roll. A caster left rooted is unrecoverable without a
	// reload; a caster freed a frame early is not worth noticing.
	ReleaseCastMovementLock();
	ReleaseCastAim();

	// The one guaranteed drop. A Sustained working has usually let go of this already and
	// this is a no-op; everything else -- fizzled, cancelled, interrupted, avatar
	// destroyed mid-gesture -- reaches it only here, and a casting tag left standing is a
	// changeling who can never cast again.
	DropCastingTag();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Cantrip::TakeCastAim()
{
	if (bAimTaken)
	{
		return;
	}

	ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	// Remembered before it is changed, so a player already holding the stance keeps it
	// when the cantrip ends. SetAiming does more than set a flag -- it saves and restores
	// controller yaw and orient-to-movement, and clamps the camera into the arc -- so
	// calling it redundantly would have it remember its own values as the originals.
	bAimWasAlreadyHeld = Character->IsAiming();
	bAimTaken = true;

	if (!bAimWasAlreadyHeld)
	{
		Character->SetAiming(true);
	}
}

void UGA_Cantrip::ReleaseCastAim()
{
	if (!bAimTaken)
	{
		return;
	}

	bAimTaken = false;

	// Only if this casting is what turned it on. Dropping a stance the player is still
	// holding would snap her facing loose mid-aim.
	if (bAimWasAlreadyHeld)
	{
		return;
	}

	if (ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->SetAiming(false);
	}
}

void UGA_Cantrip::TakeCastMovementLock()
{
	if (bMovementLocked)
	{
		return;
	}

	ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	// The character owns the lock. It has to be able to outlive this ability -- the body
	// is still recovering when the cantrip ends -- so the ability only ever asks.
	Character->SetCantripMovementLocked(true);
	bMovementLocked = true;
}

void UGA_Cantrip::ReleaseCastMovementLock()
{
	if (!bMovementLocked)
	{
		return;
	}

	// Cleared first, and unconditionally. Whatever happens to the character from here,
	// this casting has given up its claim -- and a claim that outlives the ability is a
	// caster nothing will ever unroot.
	bMovementLocked = false;

	if (ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo()))
	{
		// Handed over rather than released outright: with a timeout of zero the character
		// frees itself immediately, and with one set it waits for the recovery animation
		// or that deadline, whichever comes first.
		Character->ReleaseCantripMovementLockOnRecovery(CastRecoveryTimeout);
	}
}

float UGA_Cantrip::PlayCastMontage()
{
	if (!CastMontage)
	{
		return 0.0f;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return 0.0f;
	}

	return ASC->PlayMontage(this, CurrentActivationInfo, CastMontage, MontagePlayRate);
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

	// FLOORED, not rounded, and that is a rules decision rather than a numeric one.
	//
	// Both of these are stored on a continuous scale but spent as whole dice: a
	// difficulty cannot rise by four tenths. Rounding would put the step at the HALFWAY
	// mark, so Banality 3.49 and 3.51 play a full point apart with nothing on screen to
	// say why, and the bar's last half would be progress toward a threshold already
	// crossed. Flooring puts the step at the whole number the player can see coming: the
	// bar fills visibly toward the next point, and arriving there is what costs.
	//
	// It also means a changeling is never charged for Banality she has not finished
	// accruing, which is the kinder reading of a trait that only ever goes up.
	bool bFound = false;
	OutAttempt.NightmareRating = FMath::FloorToInt(
		ASC->GetGameplayAttributeValue(UChangelingAttributeSet::GetNightmareAttribute(), bFound));

	const int32 OwnBanality = FMath::FloorToInt(
		ASC->GetGameplayAttributeValue(UChangelingAttributeSet::GetBanalityAttribute(), bFound));

	OutAttempt.BanalityModifier = OwnBanality + GetEnvironmentalBanality();

	// Injury, as a fraction rather than as points. The wound track is the one place that
	// decides what a given fraction costs, so the dice she loses here and the level named
	// on her health bar are two reads of the same row and cannot drift apart.
	const float Health = ASC->GetGameplayAttributeValue(
		UChangelingAttributeSet::GetHealthAttribute(), bFound);
	const float HealthMax = ASC->GetGameplayAttributeValue(
		UChangelingAttributeSet::GetHealthMaxAttribute(), bFound);

	OutAttempt.WoundPenalty =
		UChangelingWoundLibrary::GetWoundLevelFromHealth(Health, HealthMax).DicePenalty;

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

	// Unset means this cantrip is FREE, and nothing else would ever say so: the cost is
	// computed, the fizzle check passes, the log prints a cost, and not a point leaves
	// her. Warned once per cast rather than silently, because a whole Art costing nothing
	// looks like a balance decision until someone checks the asset.
	if (Cost > 0 && !GlamourCostEffect)
	{
		UE_LOG(LogCantripAbility, Warning,
			TEXT("  NO COST APPLIED: %s has no GlamourCostEffect, so its %d Glamour is "
			     "never spent. Assign GE_CantripCost to it."),
			*GetName(), Cost);
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

void UGA_Cantrip::PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec)
{
	// A failed working reaches nothing and holds nothing. The Glamour is still gone,
	// which is the whole point of paying before the roll rather than after it.
	if (!Outcome.bSucceeded)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	ResolvedTargets = FCantripTargets();
	ResolveTargets(ResolvedTargets);

	ApplyCantrip(Outcome, Spec, ResolvedTargets);

	for (const TObjectPtr<UCantripPayload>& Payload : Payloads)
	{
		if (Payload)
		{
			UE_LOG(LogCantripAbility, Log, TEXT("  payload: %s"), *Payload->Describe());
			Payload->Apply(this, Outcome, ResolvedTargets);
		}
	}

	OnCantripApplied(Outcome, ResolvedTargets);

	bCantripRunning = true;

	switch (Spec.Kind)
	{
	case ECantripKind::Channelled:
		// Held open by the input. Nothing to schedule -- the release ends it, and the
		// safety valve on the channel itself catches a dropped key.
		return;

	case ECantripKind::Sustained:
	{
		const float Duration =
			BaseDuration + (DurationPerSuccess * FMath::Max(0, Outcome.Successes - 1));

		if (Duration > 0.0f)
		{
			if (UWorld* World = GetWorld())
			{
				DurationEndTime = World->GetTimeSeconds() + Duration;

				World->GetTimerManager().SetTimer(DurationTimer,
					FTimerDelegate::CreateUObject(this, &UGA_Cantrip::EndCantrip),
					Duration, false);

				// The working is cast. Her hands are free again for the whole of the
				// duration, which is the entire reason to hold one -- a Hopscotch that
				// let her bound across a courtyard but not throw anything while she did
				// would be a worse version of standing still.
				//
				// Channelled deliberately does NOT reach this: a channel is still being
				// cast for as long as it runs, and both hands really are on it.
				DropCastingTag();

				UE_LOG(LogCantripAbility, Log,
					TEXT("  HOLDS for %.1fs -- hands free, other cantrips unblocked"),
					Duration);
				return;
			}
		}

		// A sustained cantrip with no duration would hang forever. Treated as instant
		// rather than left running, because a working nobody can end is worse than a
		// short one.
		break;
	}

	case ECantripKind::Summon:
	case ECantripKind::Instant:
	default:
		break;
	}

	bCantripRunning = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Cantrip::EndCantrip()
{
	if (!bCantripRunning)
	{
		return;
	}

	bCantripRunning = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DurationTimer);
	}

	// Payloads first, in case one of them depends on state the role is about to tear
	// down -- a transformation reverting the body a payload is still holding an effect on.
	for (const TObjectPtr<UCantripPayload>& Payload : Payloads)
	{
		if (Payload)
		{
			Payload->Revert(this);
		}
	}

	RevertCantrip();
	OnCantripReverted();

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

float UGA_Cantrip::GetRemainingDuration() const
{
	const UWorld* World = GetWorld();

	if (!World || !bCantripRunning || DurationEndTime <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, DurationEndTime - World->GetTimeSeconds());
}

void UGA_Cantrip::ResolveTargets(FCantripTargets& Out) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	switch (Delivery)
	{
	case ECantripDelivery::ExplicitTarget:
		if (AActor* Target = GetCantripTarget())
		{
			Out.Actors.Add(Target);
			Out.Point = Target->GetActorLocation();
			return;
		}
		// Nothing handed over falls back to the caster rather than the world origin,
		// which is what an unset target would otherwise mean.
		Out.Point = Avatar->GetActorLocation();
		return;

	case ECantripDelivery::AreaBurst:
		Out.Point = Avatar->GetActorLocation();
		GatherArea(Out.Point, Out);
		return;

	case ECantripDelivery::AimTrace:
	case ECantripDelivery::Projectile:
	{
		// Projectile resolves like an aim trace here so the cantrip knows where it is
		// pointed; the flying actor is spawned by the role that wants one, and reports
		// back when it lands.
		const UWorld* World = GetWorld();
		if (!World)
		{
			Out.Point = Avatar->GetActorLocation();
			return;
		}

		FVector Eye;
		FRotator Look;
		Avatar->GetActorEyesViewPoint(Eye, Look);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(CantripAim), true, Avatar);

		// TWO traces, because in third person the reticle and the caster do not agree.
		//
		// One trace from her eyes along the camera's pitch is the obvious implementation and
		// is wrong here: the camera sits above and behind her looking DOWN, so that ray
		// leaves her face at fifteen or twenty degrees below level and meets the ground a few
		// metres out -- well short of whatever the player was actually pointing at. Portal
		// Passage found this the hard way; six clean castings in a row resolved against the
		// floor and opened nothing.
		//
		// So the camera decides WHAT is being aimed at, and she decides whether she can
		// reach it.
		FVector AimStart = Eye;
		FVector AimDir = Look.Vector();

		const APawn* AsPawn = Cast<APawn>(Avatar);
		const APlayerController* Controller =
			AsPawn ? Cast<APlayerController>(AsPawn->GetController()) : nullptr;

		if (Controller && Controller->PlayerCameraManager)
		{
			AimStart = Controller->PlayerCameraManager->GetCameraLocation();
			AimDir = Controller->PlayerCameraManager->GetCameraRotation().Vector();
		}

		// An AI or a possessed pawn with no camera keeps the old behaviour, which is correct
		// for it -- without a viewpoint there is no reticle to disagree with.
		const FVector AimEnd = AimStart + (AimDir * MaxRange);

		FHitResult AimHit;
		const bool bHitSomething =
			World->LineTraceSingleByChannel(AimHit, AimStart, AimEnd, ECC_Visibility, Params);

		const FVector AimPoint = bHitSomething ? AimHit.ImpactPoint : AimEnd;

		// Second trace: from HER to that point. This is what stops a cantrip being cast
		// through a wall the camera can see over, and it is also what supplies the surface
		// she is actually working on rather than the one the camera happened to see.
		FHitResult ReachHit;
		if (World->LineTraceSingleByChannel(ReachHit, Eye, AimPoint, ECC_Visibility, Params))
		{
			Out.Point = ReachHit.ImpactPoint;
			Out.Normal = ReachHit.ImpactNormal;

			if (AActor* Struck = ReachHit.GetActor())
			{
				Out.Actors.Add(Struck);
			}
		}
		else if (bHitSomething)
		{
			// Clear line to it. The camera's own hit is the better answer here: it carries
			// the surface normal at the point the player was looking at.
			Out.Point = AimHit.ImpactPoint;
			Out.Normal = AimHit.ImpactNormal;

			if (AActor* Struck = AimHit.GetActor())
			{
				Out.Actors.Add(Struck);
			}
		}
		else
		{
			// Nothing struck by either. It still lands, at the end of its reach -- a cantrip
			// fired into open sky should look fired, not swallowed.
			Out.Point = AimPoint;
		}
		return;
	}

	case ECantripDelivery::SelfCast:
	default:
		Out.Actors.Add(const_cast<AActor*>(Avatar));
		Out.Point = Avatar->GetActorLocation();
		return;
	}
}

void UGA_Cantrip::GatherArea(const FVector& Origin, FCantripTargets& Out) const
{
	const UWorld* World = GetWorld();
	AActor* Avatar = const_cast<AActor*>(GetAvatarActorFromActorInfo());

	if (!World || EffectRadius <= 0.0f)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CantripArea), false, Avatar);

	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(EffectRadius), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Found = Overlap.GetActor();

		// Deduplicated: one actor answers an overlap through capsule AND mesh, and a
		// cantrip applied twice to the same body is a bug that only shows up on bosses.
		if (Found && (bAreaHitsCaster || Found != Avatar))
		{
			Out.Actors.AddUnique(Found);
		}
	}

	if (bAreaHitsCaster && Avatar)
	{
		Out.Actors.AddUnique(Avatar);
	}
}

FActiveGameplayEffectHandle UGA_Cantrip::ApplyEffectTo(AActor* Target,
	TSubclassOf<UGameplayEffect> Effect, float Magnitude, TOptional<float> Secondary) const
{
	UAbilitySystemComponent* Source = GetAbilitySystemComponentFromActorInfo();

	if (!Source || !Effect || !Target)
	{
		return FActiveGameplayEffectHandle();
	}

	// The target needs its own ability system. A crate cannot carry a debuff, and a
	// cantrip aimed at one should fail quietly rather than assert.
	const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Target);
	UAbilitySystemComponent* TargetASC =
		Interface ? Interface->GetAbilitySystemComponent() : nullptr;

	if (!TargetASC)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle =
		Source->MakeOutgoingSpec(Effect, GetAbilityLevel(), Context);

	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		ChangelingTags::Cantrip_Data_Magnitude, Magnitude);

	// Set only when a caller actually has a second number. Setting it unconditionally
	// would be harmless for effects that ignore the tag, but it would also make every
	// effect look like it might read two -- and the whole point of one magnitude is that
	// a role author never has to wonder which one their effect is getting.
	if (Secondary.IsSet())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(
			ChangelingTags::Cantrip_Data_Secondary, Secondary.GetValue());
	}

	return Source->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void UGA_Cantrip::ApplyEffectToTargets(const FCantripTargets& Targets,
	TSubclassOf<UGameplayEffect> Effect, float Magnitude,
	TArray<FActiveGameplayEffectHandle>& OutHandles, TOptional<float> Secondary) const
{
	if (!Effect)
	{
		return;
	}

	for (const TObjectPtr<AActor>& Target : Targets.Actors)
	{
		const FActiveGameplayEffectHandle Handle =
			ApplyEffectTo(Target, Effect, Magnitude, Secondary);

		// Only kept when it took. An invalid handle in the list would be removed later
		// as though something had been undone.
		if (Handle.IsValid())
		{
			OutHandles.Add(Handle);
		}
	}
}

void UGA_Cantrip::RemoveAppliedEffects(TArray<FActiveGameplayEffectHandle>& Handles) const
{
	for (const FActiveGameplayEffectHandle& Handle : Handles)
	{
		// Removed through the component that OWNS the effect, not the caster's. On a
		// target's ability system the caster has no authority to remove anything.
		if (Handle.IsValid())
		{
			if (UAbilitySystemComponent* Owner = Handle.GetOwningAbilitySystemComponent())
			{
				Owner->RemoveActiveGameplayEffect(Handle);
			}
		}
	}

	Handles.Reset();
}

float UGA_Cantrip::GetTotalDuration(const FCantripOutcome& Outcome) const
{
	const FCantripSpec* Spec = FindSpec();

	// Only a sustained row has a duration to spread anything across. A channel's length
	// is however long the key is held, which nothing can know in advance.
	if (!Spec || Spec->Kind != ECantripKind::Sustained)
	{
		return 0.0f;
	}

	return BaseDuration + (DurationPerSuccess * FMath::Max(0, Outcome.Successes - 1));
}

UCantripPayload* UGA_Cantrip::FindPayload(TSubclassOf<UCantripPayload> PayloadClass) const
{
	if (!PayloadClass)
	{
		return nullptr;
	}

	for (const TObjectPtr<UCantripPayload>& Payload : Payloads)
	{
		if (Payload && Payload->IsA(PayloadClass))
		{
			return Payload;
		}
	}

	return nullptr;
}
