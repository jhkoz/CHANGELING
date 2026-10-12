// ChangelingCrowdVariation.cpp

#include "ChangelingCrowdVariation.h"

#include "ChangelingCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GroomComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// Verified against M_hair_v4 in /Game/MetaHumans/Common/Materials. Writing a parameter a
// material does not have fails silently -- no warning, no error, just no change -- so
// these are named once here rather than spelled out at each call site.
namespace
{
	static const FName HairMelaninParam(TEXT("HairMelanin"));
	static const FName HairRednessParam(TEXT("HairRedness"));
	static const FName HairRoughnessParam(TEXT("HairRoughness"));

	// Greying. Verified on MF_Hair_SaltAndPeper and exposed on MI_Hair, which is what makes
	// it settable on a dynamic instance. The function's siblings -- WhiteColorCoverage,
	// WhiteMelaninVariation and the rest -- shape HOW the grey looks; this one is how much.
	static const FName HairWhiteAmountParam(TEXT("WhiteAmount"));

	// Verified on MI_eye_eyeball_unified_MH_preset_left, the parent of the assembled
	// MI_Eye*_Baked instances. The spaces are part of the names.
	//
	// Two paths, because which one is live depends on whether the assembly baked its
	// materials. Writing a parameter a material does not have is silent, so setting both
	// costs nothing and means this works either way.
	static const FName IrisHueParam(TEXT("Iris Primary Color Hue"));
	static const FName IrisValueParam(TEXT("Iris Primary Color Value"));
	static const FName IrisSaturationParam(TEXT("Iris Global Saturation"));
	static const FName IrisMultiplyParam(TEXT("Iris Color Multiply"));

	// Present on MI_EyeL_Baked itself, so unlike the iris parameters this one is known to
	// survive the pipeline's material baking.
	static const FName ScleraVeinsParam(TEXT("Sclera Irritation Veins Opacity"));
}

UChangelingCrowdVariation::UChangelingCrowdVariation()
{
	// Runs once and is done. Nothing here needs a frame.
	PrimaryComponentTick.bCanEverTick = false;
}

void UChangelingCrowdVariation::BeginPlay()
{
	Super::BeginPlay();

	ApplyVariation(Seed);
}

int32 UChangelingCrowdVariation::ResolveSeed() const
{
	if (Seed != 0)
	{
		return Seed;
	}

	// Derived from the actor's name so a placed NPC looks the same on every run without
	// anyone choosing numbers. A gardener who is a different person each time you press
	// play cannot be art-directed, and "the one with the red hair" stops being a thing
	// anyone can point at.
	const AActor* Owner = GetOwner();
	return Owner ? GetTypeHash(Owner->GetFName()) : 0;
}

void UChangelingCrowdVariation::ApplyVariation(int32 NewSeed)
{
	// Zero means keep what we have, so re-applying after an edit reproduces the same
	// appearance instead of quietly rolling a new one.
	if (NewSeed != 0)
	{
		Seed = NewSeed;
	}

	// The table wins over the inline values, and is read before anything is rolled so the
	// bands in force are the kith's rather than a mixture.
	AdoptKithBands();

	FRandomStream Stream(ResolveSeed());

	ApplySilhouette(Stream);
	ApplyMotion(Stream);

	if (bVaryHairColour)
	{
		ApplyHairColour(Stream);
	}

	if (bVaryEyeColour)
	{
		ApplyEyeColour(Stream);
	}
}

void UChangelingCrowdVariation::AdoptKithBands()
{
	// No kith named is the normal case for a one-off NPC, not an error.
	if (Kith.IsNone() || !KithTable)
	{
		return;
	}

	static const FString Context(TEXT("ChangelingCrowdVariation"));
	const FChangelingKithAppearance* Row =
		KithTable->FindRow<FChangelingKithAppearance>(Kith, Context, /*bWarnIfMissing*/ false);

	if (!Row)
	{
		// Warned rather than ignored. A misspelled kith silently falls back to the Boggan
		// defaults, and a Troll built to Boggan bands looks merely wrong rather than broken,
		// which is the kind of thing that survives to ship.
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("%s: kith '%s' has no row in %s; using the component's own ranges"),
			*GetNameSafe(GetOwner()), *Kith.ToString(), *GetNameSafe(KithTable));
		return;
	}

	MinHeightScale = Row->MinHeightScale;
	MaxHeightScale = Row->MaxHeightScale;
	MinWeightScale = Row->MinWeightScale;
	MaxWeightScale = Row->MaxWeightScale;

	MinMelanin = Row->MinMelanin;
	MaxMelanin = Row->MaxMelanin;
	MinRedness = Row->MinRedness;
	MaxRedness = Row->MaxRedness;
	MinWhiteness = Row->MinWhiteness;
	MaxWhiteness = Row->MaxWhiteness;

	MinIrisHue = Row->MinIrisHue;
	MaxIrisHue = Row->MaxIrisHue;
	MinIrisValue = Row->MinIrisValue;
	MaxIrisValue = Row->MaxIrisValue;
	MinIrisSaturation = Row->MinIrisSaturation;
	MaxIrisSaturation = Row->MaxIrisSaturation;

	ScleraVeins = Row->ScleraVeins;
}

void UChangelingCrowdVariation::ApplyEyeColour(FRandomStream& Stream)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Body)
	{
		return;
	}

	// Eyes are material slots on the FACE mesh, which hangs off the body. Found by walking
	// the children rather than by a component name, since the face is added per Blueprint
	// and nothing guarantees what it is called.
	TArray<USceneComponent*> Attached;
	Body->GetChildrenComponents(/*bIncludeAllDescendants*/ true, Attached);

	// One roll for the character, not one per eye. Two independently coloured eyes is
	// heterochromia, which is rare enough in life that in a crowd it reads as a bug.
	const float Hue = Stream.FRandRange(
		FMath::Min(MinIrisHue, MaxIrisHue), FMath::Max(MinIrisHue, MaxIrisHue));
	const float Value = Stream.FRandRange(
		FMath::Min(MinIrisValue, MaxIrisValue), FMath::Max(MinIrisValue, MaxIrisValue));
	const float Saturation = Stream.FRandRange(
		FMath::Min(MinIrisSaturation, MaxIrisSaturation),
		FMath::Max(MinIrisSaturation, MaxIrisSaturation));

	// The tint path for a baked iris. Built from the same roll so the two paths agree
	// rather than fighting: a warm hue tints warm, a cool one cool. Kept near white so it
	// shades the baked texture instead of replacing it -- a strong multiply on a baked
	// brown iris gives a stained-glass eye, not a different person.
	const FLinearColor Tint = FLinearColor::MakeFromHSV8(
		static_cast<uint8>(FMath::Clamp(Hue, 0.0f, 1.0f) * 255.0f),
		static_cast<uint8>(90),
		static_cast<uint8>(255));
	const FLinearColor Multiply = FMath::Lerp(FLinearColor::White, Tint, 0.35f);

	int32 EyeSlots = 0;

	for (USceneComponent* Child : Attached)
	{
		USkeletalMeshComponent* Face = Cast<USkeletalMeshComponent>(Child);
		if (!Face)
		{
			continue;
		}

		const TArray<UMaterialInterface*> Materials = Face->GetMaterials();
		for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
		{
			const UMaterialInterface* Existing = Materials[Slot];
			if (!Existing)
			{
				continue;
			}

			const FString Name = Existing->GetName();

			// The eyeballs only. EyeShell is the clear outer surface, and the lashes and
			// lacrimal fluid sit in neighbouring slots with similar names -- tinting any of
			// them gives a character who appears to be crying coloured tears.
			if (!Name.Contains(TEXT("Eye"))
				|| Name.Contains(TEXT("Shell"))
				|| Name.Contains(TEXT("lash"))
				|| Name.Contains(TEXT("Lash"))
				|| Name.Contains(TEXT("Lacrimal")))
			{
				continue;
			}

			UMaterialInstanceDynamic* Dynamic = Face->CreateDynamicMaterialInstance(Slot);
			if (!Dynamic)
			{
				continue;
			}

			Dynamic->SetScalarParameterValue(IrisHueParam, Hue);
			Dynamic->SetScalarParameterValue(IrisValueParam, Value);
			Dynamic->SetScalarParameterValue(IrisSaturationParam, Saturation);
			Dynamic->SetVectorParameterValue(IrisMultiplyParam, Multiply);

			// Bloodshot. Redcaps only, and the one eye parameter confirmed still live on
			// the BAKED instance -- MI_EyeL_Baked exposes it where it exposes no iris hue
			// at all, so this works regardless of how the character was assembled.
			if (ScleraVeins > 0.0f)
			{
				Dynamic->SetScalarParameterValue(ScleraVeinsParam, ScleraVeins);
			}

			++EyeSlots;
		}
	}

	UE_LOG(LogTemplateCharacter, Verbose,
		TEXT("%s: iris hue %.2f value %.2f saturation %.2f across %d slot(s)"),
		*GetNameSafe(GetOwner()), Hue, Value, Saturation, EyeSlots);
}

void UChangelingCrowdVariation::ApplySilhouette(FRandomStream& Stream)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Mesh || !Capsule)
	{
		return;
	}

	// Ranges are min/max'd rather than trusted, so entering one backwards in a Blueprint
	// is harmless instead of producing FRandRange's undefined answer.
	const float Height = Stream.FRandRange(
		FMath::Min(MinHeightScale, MaxHeightScale),
		FMath::Max(MinHeightScale, MaxHeightScale));

	const float Weight = Stream.FRandRange(
		FMath::Min(MinWeightScale, MaxWeightScale),
		FMath::Max(MinWeightScale, MaxWeightScale));

	// Scaled on the MESH, not the actor. Actor scale would take the capsule with it, and a
	// capsule has no non-uniform form to take -- it derives its radius from the larger of
	// X and Y, so a character made taller would come out fatter as well.
	Mesh->SetRelativeScale3D(FVector(Weight, Weight, Height));

	// The capsule is then resized deliberately. Both dimensions are taken from the values
	// the character class shipped with rather than from whatever the capsule currently
	// holds, so re-applying variation twice does not compound.
	const ACharacter* Defaults = GetDefault<ACharacter>(Character->GetClass());
	const UCapsuleComponent* DefaultCapsule =
		Defaults ? Defaults->GetCapsuleComponent() : nullptr;

	const float BaseHalfHeight = DefaultCapsule
		? DefaultCapsule->GetUnscaledCapsuleHalfHeight()
		: Capsule->GetUnscaledCapsuleHalfHeight();
	const float BaseRadius = DefaultCapsule
		? DefaultCapsule->GetUnscaledCapsuleRadius()
		: Capsule->GetUnscaledCapsuleRadius();

	const float NewHalfHeight = BaseHalfHeight * Height;
	Capsule->SetCapsuleSize(BaseRadius * Weight, NewHalfHeight, /*bUpdateOverlaps*/ true);

	// The mesh hangs from the capsule's bottom, so the offset has to follow the new half
	// height or she sinks into the floor by exactly the amount she grew. CharacterMovement
	// keeps the capsule bottom on the ground, so with both moved together the feet stay
	// planted and the extra height goes upward, which is where height belongs.
	FVector MeshOffset = Mesh->GetRelativeLocation();
	MeshOffset.Z = -NewHalfHeight;
	Mesh->SetRelativeLocation(MeshOffset);
}

void UChangelingCrowdVariation::ApplyMotion(FRandomStream& Stream)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	const float Low = FMath::Min(MinPlayRate, MaxPlayRate);
	const float High = FMath::Max(MinPlayRate, MaxPlayRate);

	// GlobalAnimRateScale rather than a play rate on any one node: it applies to whatever
	// the animation blueprint is doing without this component needing to know its shape,
	// which matters because every kith will run the same ABP and none of them are ours.
	Mesh->GlobalAnimRateScale = Stream.FRandRange(Low, High);
}

void UChangelingCrowdVariation::GatherGrooms(TArray<UGroomComponent*>& OutGrooms) const
{
	OutGrooms.Reset();

	if (const AActor* Owner = GetOwner())
	{
		Owner->GetComponents<UGroomComponent>(OutGrooms);
	}
}

void UChangelingCrowdVariation::ApplyHairColour(FRandomStream& Stream)
{
	TArray<UGroomComponent*> Grooms;
	GatherGrooms(Grooms);

	if (Grooms.IsEmpty())
	{
		return;
	}

	// Rolled ONCE and shared across every groom on this character. Hair, eyebrows, beard
	// and moustache belong to the same person: rolling per groom gives white hair over
	// black brows, which reads as a bug rather than as variety.
	const float Melanin = Stream.FRandRange(
		FMath::Min(MinMelanin, MaxMelanin), FMath::Max(MinMelanin, MaxMelanin));
	const float Redness = Stream.FRandRange(
		FMath::Min(MinRedness, MaxRedness), FMath::Max(MinRedness, MaxRedness));
	const float Roughness = Stream.FRandRange(
		FMath::Min(MinRoughness, MaxRoughness), FMath::Max(MinRoughness, MaxRoughness));
	const float Whiteness = Stream.FRandRange(
		FMath::Min(MinWhiteness, MaxWhiteness), FMath::Max(MinWhiteness, MaxWhiteness));

	int32 SlotsTouched = 0;

	for (UGroomComponent* Groom : Grooms)
	{
		if (!Groom)
		{
			continue;
		}

		// A groom can carry a material per hair group, so every slot needs its own dynamic
		// instance -- doing only slot 0 leaves multi-group hair half-recoloured.
		const int32 SlotCount = Groom->GetNumMaterials();
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			UMaterialInstanceDynamic* Dynamic = Groom->CreateDynamicMaterialInstance(Slot);
			if (!Dynamic)
			{
				continue;
			}

			Dynamic->SetScalarParameterValue(HairMelaninParam, Melanin);
			Dynamic->SetScalarParameterValue(HairRednessParam, Redness);
			Dynamic->SetScalarParameterValue(HairRoughnessParam, Roughness);
			Dynamic->SetScalarParameterValue(HairWhiteAmountParam, Whiteness);
			++SlotsTouched;
		}
	}

	// Per-instance dynamic materials defeat batching. That is an acceptable trade for
	// dozens of NPCs and a bad one for hundreds -- which is roughly where 5.8's MetaHuman
	// Collections stops being optional. Logged so the count is visible when that day comes.
	UE_LOG(LogTemplateCharacter, Verbose,
		TEXT("%s: crowd variation seeded %d -- %d groom material slot(s) instanced"),
		*GetNameSafe(GetOwner()), ResolveSeed(), SlotsTouched);
}
