// GA_SustainedCantrip.cpp

#include "GA_SustainedCantrip.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "CHANGELINGCharacter.h"
#include "CantripInternal.h"
#include "ChangelingAttributeSet.h"
#include "ChangelingGameplayTags.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSustainedCantrip, Log, All);

using CantripInternal::ResolveASC;

UGA_SustainedCantrip::UGA_SustainedCantrip()
{
	// Re-pressing must not stack a second copy on top of the running one; the toggle
	// below decides whether a press starts or stops it.
	bRetriggerInstancedAbility = false;
}

bool UGA_SustainedCantrip::ToggleSustainedCantrip(AActor* Avatar,
	TSubclassOf<UGA_SustainedCantrip> CantripClass)
{
	if (!CantripClass)
	{
		return false;
	}

	// Running? Then this press is the "off" half. Asking the ability itself means
	// there is no separate bool to fall out of step with it.
	if (DouseSustainedCantrips(Avatar, CantripClass) > 0)
	{
		UE_LOG(LogSustainedCantrip, Log, TEXT("TOGGLE %s: was sustaining -> doused"),
			*CantripClass->GetName());
		return false;
	}

	UAbilitySystemComponent* ASC = ResolveASC(Avatar);
	const bool bActivated = ASC ? ASC->TryActivateAbilityByClass(CantripClass) : false;

	UE_LOG(LogSustainedCantrip, Log, TEXT("TOGGLE %s: not sustaining -> TryActivate=%d (asc=%d)"),
		*CantripClass->GetName(), bActivated ? 1 : 0, ASC ? 1 : 0);

	return bActivated;
}

int32 UGA_SustainedCantrip::DouseSustainedCantrips(AActor* Avatar,
	TSubclassOf<UGA_SustainedCantrip> CantripClass)
{
	UAbilitySystemComponent* ASC = ResolveASC(Avatar);
	if (!ASC)
	{
		return 0;
	}

	// Gather BEFORE ending any of them. Ending an ability mutates the activatable
	// list, and cancelling mid-iteration would walk a container being rewritten.
	// Weak, because cancelling one can cascade -- an OnSustainEnded handler is free to
	// douse the rest -- and a raw pointer gathered a moment ago may already be gone.
	TArray<TWeakObjectPtr<UGA_SustainedCantrip>> Running;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability)
		{
			continue;
		}

		if (CantripClass && !Spec.Ability->IsA(CantripClass))
		{
			continue;
		}

		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			UGA_SustainedCantrip* Cantrip = Cast<UGA_SustainedCantrip>(Instance);
			if (Cantrip && Cantrip->IsSustaining())
			{
				Running.Add(Cantrip);
			}
		}
	}

	int32 Doused = 0;
	for (const TWeakObjectPtr<UGA_SustainedCantrip>& Cantrip : Running)
	{
		// Re-check IsSustaining as well as validity: a previous cancel's
		// OnSustainEnded may already have put this one out, and counting it twice
		// would misreport how much was interrupted.
		if (Cantrip.IsValid() && Cantrip->IsSustaining())
		{
			Cantrip->CancelSustain();
			++Doused;
		}
	}

	return Doused;
}

void UGA_SustainedCantrip::CancelSustain()
{
	if (!bSustaining)
	{
		return;
	}

	// bRanOut false: this was cut short, not spent. The distinction reaches Blueprint
	// so a light snuffed by a blow can look different from one that simply burned out.
	StopSustain(/*bRanOut*/ false);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SustainedCantrip::PostResolve(const FCantripOutcome& Outcome, const FCantripSpec& Spec)
{
	// A failed working never opens. Fall through to the base, which ends the ability,
	// so a botched torch costs the Glamour and gives nothing -- which is the point of
	// spending before the roll.
	if (!Outcome.bSucceeded)
	{
		Super::PostResolve(Outcome, Spec);
		return;
	}

	BeginSustain(Outcome.Successes);
}

float UGA_SustainedCantrip::DurationForSuccesses(int32 Successes) const
{
	if (DurationBySuccesses.Num() == 0)
	{
		return 0.0f;
	}

	// Clamp rather than index blindly: a roll can beat the table's length, and the
	// top entry is the intended ceiling, not an invitation to read past it.
	const int32 Index = FMath::Clamp(Successes - 1, 0, DurationBySuccesses.Num() - 1);
	return DurationBySuccesses[Index];
}

float UGA_SustainedCantrip::GetRemainingSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World || !bSustaining)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(DurationTimer));
}

void UGA_SustainedCantrip::HandleExpired()
{
	StopSustain(/*bRanOut*/ true);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SustainedCantrip::BeginSustain(int32 Successes)
{
	bSustaining = true;
	SustainSuccesses = FMath::Max(1, Successes);

	UE_LOG(LogSustainedCantrip, Log, TEXT("LIT: successes=%d duration=%.1fs pose=%d"),
		Successes, DurationForSuccesses(Successes), static_cast<int32>(SustainedPose));

	if (ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->SetCantripPose(SustainedPose);
	}

	UWorld* World = GetWorld();

	ScheduleEffectSpawn();

	if (SustainedGameplayEffect)
	{
		FGameplayEffectSpecHandle EffectSpec =
			MakeOutgoingGameplayEffectSpec(SustainedGameplayEffect, GetAbilityLevel());
		if (EffectSpec.IsValid())
		{
			SustainedEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle,
				CurrentActorInfo, CurrentActivationInfo, EffectSpec);
		}
	}

	const float Duration = DurationForSuccesses(Successes);
	if (World && Duration > 0.0f)
	{
		World->GetTimerManager().SetTimer(DurationTimer,
			FTimerDelegate::CreateUObject(this, &UGA_SustainedCantrip::HandleExpired),
			Duration, false);
	}

	OnSustainBegan();
}

void UGA_SustainedCantrip::ScheduleEffectSpawn()
{
	UWorld* World = GetWorld();

	// The hand has to arrive before the fire does. Lighting the flame on the same
	// frame the arm starts rising reads as the flame dragging the arm up after it.
	if (World && EffectSpawnDelay > 0.0f)
	{
		World->GetTimerManager().SetTimer(SpawnDelayTimer,
			FTimerDelegate::CreateUObject(this, &UGA_SustainedCantrip::SpawnSustainedEffect),
			EffectSpawnDelay, false);
	}
	else
	{
		SpawnSustainedEffect();
	}
}

void UGA_SustainedCantrip::SpawnSustainedEffect()
{
	if (!bSustaining)
	{
		return;
	}

	// Before the early-out below: the light is not conditional on there being a Niagara
	// system. A working can legitimately light a room without any visible flame, and a
	// cantrip whose effect asset is still missing should at least still do its job.
	SpawnSustainedLight();

	if (!SustainedEffect)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	if (!Mesh->DoesSocketExist(AttachSocket))
	{
		// Loud, because the symptom is simply no fire, and a socket name typed one
		// character wrong is invisible from the viewport.
		UE_LOG(LogSustainedCantrip, Warning,
			TEXT("Socket '%s' not found on %s -- sustained effect will not appear."),
			*AttachSocket.ToString(), *GetNameSafe(Mesh->GetSkeletalMeshAsset()));
		return;
	}

	SustainedComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		SustainedEffect, Mesh, AttachSocket, AttachOffset, FRotator::ZeroRotator,
		EAttachLocation::SnapToTargetIncludingScale, /*bAutoDestroy*/ false,
		/*bAutoActivate*/ true);

	// Full strength on spawn. The fade IN is the emitter's own curve on age -- it
	// knows when it started; only the ending needs telling.
	if (SustainedComponent && !FadeParameter.IsNone())
	{
		SustainedComponent->SetVariableFloat(FadeParameter, 1.0f);
	}
}

void UGA_SustainedCantrip::SpawnSustainedLight()
{
	if (LightRadius <= 0.0f)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	// Successes past the first extend the reach. A bare success still lights something,
	// so a marginal casting is dim rather than useless.
	const int32 Extra = FMath::Max(0, SustainSuccesses - 1);
	const float Radius = LightRadius + (LightRadiusPerSuccess * Extra);

	// Brightness grows more slowly than radius, on the square root, because doubling a
	// light's reach does not make it look twice as bright -- scaling both linearly gives
	// a strong casting that blows out everything within arm's reach.
	LightBaseIntensity = LightIntensity * FMath::Sqrt(static_cast<float>(SustainSuccesses));

	SustainedLight = NewObject<UPointLightComponent>(Mesh->GetOwner());
	if (!SustainedLight)
	{
		return;
	}

	// Mobility has to be set before the component is registered; a light that registers
	// as Static cannot be moved afterwards and would stay where the hand was.
	SustainedLight->SetMobility(EComponentMobility::Movable);
	SustainedLight->SetAttenuationRadius(Radius);
	SustainedLight->SetIntensity(LightBaseIntensity);
	SustainedLight->SetLightColor(LightColour);
	SustainedLight->SetCastShadows(bLightCastsShadows);

	SustainedLight->RegisterComponent();
	SustainedLight->AttachToComponent(Mesh,
		FAttachmentTransformRules::SnapToTargetIncludingScale, AttachSocket);
	SustainedLight->SetRelativeLocation(AttachOffset + LightOffset);

	if (LightFlickerAmount > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FlickerTimer,
				FTimerDelegate::CreateUObject(this, &UGA_SustainedCantrip::UpdateLightFlicker),
				1.0f / 30.0f, true);
		}
	}
}

void UGA_SustainedCantrip::UpdateLightFlicker()
{
	const UWorld* World = GetWorld();
	if (!SustainedLight || !World)
	{
		return;
	}

	const float T = World->GetTimeSeconds() * LightFlickerSpeed;

	// Three sines at incommensurate frequencies. One sine reads as a pulse and the eye
	// finds the period within a second or two; layering ratios that never line up gives
	// something that stays unpredictable without needing a noise texture.
	const float Wander =
		  0.50f * FMath::Sin(T)
		+ 0.30f * FMath::Sin(T * 2.37f)
		+ 0.20f * FMath::Sin(T * 5.13f);

	// Floored well above zero: a flame gutters, it does not switch off and back on.
	const float Scale = FMath::Max(0.35f, 1.0f + (Wander * LightFlickerAmount));

	SustainedLight->SetIntensity(LightBaseIntensity * Scale);
}

void UGA_SustainedCantrip::StopSustain(bool bRanOut)
{
	if (!bSustaining)
	{
		return;
	}
	bSustaining = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DurationTimer);
		World->GetTimerManager().ClearTimer(SpawnDelayTimer);
		World->GetTimerManager().ClearTimer(FlickerTimer);
	}

	if (SustainedLight)
	{
		// Handed off and forgotten here, exactly as the effect is: the dim-down outlives
		// this ability, and anything bound to `this` would be torn down mid-ramp and
		// leave the light snapping to black.
		UPointLightComponent* Dimming = SustainedLight;
		SustainedLight = nullptr;

		UWorld* World = GetWorld();
		if (World && FadeOutSeconds > 0.0f)
		{
			const float Start = LightBaseIntensity;
			const float Duration = FadeOutSeconds;
			const float Step = 1.0f / 30.0f;

			TSharedRef<float> Elapsed = MakeShared<float>(0.0f);
			TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();

			World->GetTimerManager().SetTimer(*Handle,
				FTimerDelegate::CreateWeakLambda(Dimming,
					[Dimming, Start, Duration, Step, Elapsed, Handle]()
					{
						*Elapsed += Step;
						const float Alpha = FMath::Clamp(1.0f - (*Elapsed / Duration), 0.0f, 1.0f);
						Dimming->SetIntensity(Start * Alpha);

						if (Alpha <= 0.0f)
						{
							if (UWorld* TimerWorld = Dimming->GetWorld())
							{
								TimerWorld->GetTimerManager().ClearTimer(*Handle);
							}
							Dimming->DestroyComponent();
						}
					}),
				Step, true);
		}
		else
		{
			Dimming->DestroyComponent();
		}
	}

	if (SustainedComponent)
	{
		// Hand the component off and forget it here. The fade outlives this ability --
		// StopSustain is followed by EndAbility, so anything bound to `this` would be
		// torn down mid-ramp and the flame would vanish instead of dying down.
		UNiagaraComponent* Fading = SustainedComponent;
		SustainedComponent = nullptr;

		UWorld* World = GetWorld();
		if (World && FadeOutSeconds > 0.0f && !FadeParameter.IsNone())
		{
			const FName Parameter = FadeParameter;
			const float Duration = FadeOutSeconds;
			const float Step = 1.0f / 30.0f;

			TSharedRef<float> Elapsed = MakeShared<float>(0.0f);
			TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();

			// Weak lambda bound to the COMPONENT: if it is destroyed for any other
			// reason mid-fade, the timer stops itself rather than touching freed memory.
			World->GetTimerManager().SetTimer(*Handle,
				FTimerDelegate::CreateWeakLambda(Fading,
					[Fading, Parameter, Duration, Step, Elapsed, Handle]()
					{
						*Elapsed += Step;
						const float Alpha = FMath::Clamp(1.0f - (*Elapsed / Duration), 0.0f, 1.0f);
						Fading->SetVariableFloat(Parameter, Alpha);

						if (Alpha <= 0.0f)
						{
							if (UWorld* TimerWorld = Fading->GetWorld())
							{
								TimerWorld->GetTimerManager().ClearTimer(*Handle);
							}
							// Deactivate rather than destroy: particles already in the
							// air finish their lives instead of blinking out mid-flight.
							Fading->Deactivate();
							Fading->SetAutoDestroy(true);
						}
					}),
				Step, true);
		}
		else
		{
			Fading->Deactivate();
			Fading->SetAutoDestroy(true);
		}
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (SustainedEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(SustainedEffectHandle);
			SustainedEffectHandle.Invalidate();
		}
	}

	if (ACHANGELINGCharacter* Character = Cast<ACHANGELINGCharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->SetCantripPose(ECantripPose::None);
	}

	OnSustainEnded(bRanOut);
}

void UGA_SustainedCantrip::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Covers every exit -- cancelled, interrupted, avatar destroyed -- so the arm
	// cannot be left raised holding a fire that is no longer there.
	StopSustain(/*bRanOut*/ false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
