// StarCatalog.cpp

#include "StarCatalog.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/FileHelper.h"

namespace
{
	// ── B-V colour index → linear RGB (blue → white → amber) ────────────────────
	FLinearColor BVtoRGB(float BV)
	{
		BV = FMath::Clamp(BV, -0.4f, 2.0f);
		const FLinearColor Blue (0.70f, 0.80f, 1.00f);   // hot
		const FLinearColor White(1.00f, 0.97f, 0.92f);   // sun-like
		const FLinearColor Amber(1.00f, 0.78f, 0.55f);   // cool red giant
		if (BV < 0.5f)
		{
			return FMath::Lerp(Blue, White, FMath::Clamp((BV + 0.4f) / 0.9f, 0.0f, 1.0f));
		}
		return FMath::Lerp(White, Amber, FMath::Clamp((BV - 0.5f) / 1.5f, 0.0f, 1.0f));
	}

	// ── Soft round dot brush (white core, gaussian falloff baked into RGB) ───────
	UTexture2D* MakeSoftDot(int32 S)
	{
		UTexture2D* Tex = UTexture2D::CreateTransient(S, S, PF_B8G8R8A8);
		if (!Tex) return nullptr;
		Tex->SRGB   = false;
		Tex->Filter = TF_Bilinear;

		FTexturePlatformData* PD = Tex->GetPlatformData();
		FColor* Px = static_cast<FColor*>(PD->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		const float C     = (S - 1) * 0.5f;
		const float Sigma = S * 0.22f;
		for (int32 y = 0; y < S; ++y)
		{
			for (int32 x = 0; x < S; ++x)
			{
				const float dx = (x - C) / Sigma;
				const float dy = (y - C) / Sigma;
				const float g  = FMath::Exp(-(dx * dx + dy * dy));
				const uint8 v  = static_cast<uint8>(FMath::Clamp(g * 255.0f, 0.0f, 255.0f));
				Px[y * S + x]  = FColor(v, v, v, v);
			}
		}
		PD->Mips[0].BulkData.Unlock();
		Tex->UpdateResource();
		return Tex;
	}

	// cheap integer hash → [0,1)
	float Hash11(uint32 n)
	{
		n = (n << 13) ^ n;
		n = n * (n * n * 15731u + 789221u) + 1376312589u;
		return (n & 0x7fffffffu) / static_cast<float>(0x7fffffff);
	}
}

void StarCatalog::AppendBrightStars(TArray<FCatalogStar>& Out)
{
	// RA(deg), Dec(deg), magnitude, B-V — the brightest naked-eye stars (real positions).
	static const FCatalogStar K[] =
	{
		{101.29f, -16.72f, -1.46f,  0.00f}, // Sirius
		{ 95.99f, -52.70f, -0.74f,  0.15f}, // Canopus
		{219.90f, -60.83f, -0.27f,  0.71f}, // Rigil Kentaurus
		{213.92f,  19.18f, -0.05f,  1.23f}, // Arcturus
		{279.23f,  38.78f,  0.03f,  0.00f}, // Vega
		{ 79.17f,  46.00f,  0.08f,  0.80f}, // Capella
		{ 78.63f,  -8.20f,  0.13f, -0.03f}, // Rigel
		{114.83f,   5.22f,  0.34f,  0.42f}, // Procyon
		{ 24.43f, -57.24f,  0.46f, -0.16f}, // Achernar
		{ 88.79f,   7.41f,  0.50f,  1.85f}, // Betelgeuse
		{210.96f, -60.37f,  0.61f, -0.23f}, // Hadar
		{297.69f,   8.87f,  0.77f,  0.22f}, // Altair
		{ 68.98f,  16.51f,  0.85f,  1.54f}, // Aldebaran
		{247.35f, -26.43f,  0.96f,  1.83f}, // Antares
		{201.30f, -11.16f,  0.97f, -0.23f}, // Spica
		{116.33f,  28.03f,  1.14f,  1.00f}, // Pollux
		{344.41f, -29.62f,  1.16f,  0.09f}, // Fomalhaut
		{310.36f,  45.28f,  1.25f,  0.09f}, // Deneb
		{152.09f,  11.97f,  1.35f, -0.11f}, // Regulus
		{104.66f, -28.97f,  1.50f, -0.21f}, // Adhara
		{113.65f,  31.89f,  1.57f,  0.03f}, // Castor
		{263.40f, -37.10f,  1.62f, -0.23f}, // Shaula
		{ 81.28f,   6.35f,  1.64f, -0.22f}, // Bellatrix
		{ 81.57f,  28.61f,  1.65f, -0.13f}, // Elnath
		{ 84.05f,  -1.20f,  1.69f, -0.18f}, // Alnilam (Orion belt)
		{ 85.19f,  -1.94f,  1.74f, -0.20f}, // Alnitak (Orion belt)
		{ 83.00f,  -0.30f,  2.25f, -0.18f}, // Mintaka (Orion belt)
		{165.93f,  61.75f,  1.79f,  1.07f}, // Dubhe (Big Dipper)
		{206.89f,  49.31f,  1.86f, -0.10f}, // Alkaid (Big Dipper)
		{193.51f,  55.96f,  1.77f, -0.02f}, // Alioth (Big Dipper)
		{200.98f,  54.93f,  2.04f,  0.06f}, // Mizar (Big Dipper)
		{ 37.95f,  89.26f,  1.98f,  0.60f}, // Polaris (North Star)
	};
	for (const FCatalogStar& S : K)
	{
		Out.Add(S);
	}
}

int32 StarCatalog::AppendFromHYGCsv(const FString& FullPath, TArray<FCatalogStar>& Out)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FullPath))
	{
		return 0;
	}

	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	if (Lines.Num() < 2)
	{
		return 0;
	}

	TArray<FString> Header;
	Lines[0].ParseIntoArray(Header, TEXT(","), false);
	auto Col = [&Header](const TCHAR* Name) -> int32
	{
		return Header.IndexOfByPredicate(
			[Name](const FString& S){ return S.Equals(Name, ESearchCase::IgnoreCase); });
	};
	const int32 iRA  = Col(TEXT("ra"));
	const int32 iDec = Col(TEXT("dec"));
	const int32 iMag = Col(TEXT("mag"));
	const int32 iCI  = Col(TEXT("ci"));
	if (iRA == INDEX_NONE || iDec == INDEX_NONE || iMag == INDEX_NONE)
	{
		return 0;
	}

	int32 Count = 0;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		TArray<FString> F;
		Lines[i].ParseIntoArray(F, TEXT(","), false);
		if (F.Num() <= iMag || F.Num() <= iDec || F.Num() <= iRA)
		{
			continue;
		}
		FCatalogStar S;
		S.RADeg     = FCString::Atof(*F[iRA]) * 15.0f;   // HYG 'ra' is in hours
		S.DecDeg    = FCString::Atof(*F[iDec]);
		S.Magnitude = FCString::Atof(*F[iMag]);
		S.ColorBV   = (iCI != INDEX_NONE && F.Num() > iCI && !F[iCI].IsEmpty())
			? FCString::Atof(*F[iCI]) : 0.6f;
		Out.Add(S);
		++Count;
	}
	return Count;
}

UTextureRenderTarget2D* StarCatalog::Bake(
	UObject* Outer, const TArray<FCatalogStar>& Stars,
	int32 Width, int32 Height, float MagnitudeLimit, int32 ProceduralFill)
{
	if (!Outer)
	{
		return nullptr;
	}

	UTextureRenderTarget2D* RT = UKismetRenderingLibrary::CreateRenderTarget2D(
		Outer, Width, Height, RTF_RGBA16f, FLinearColor::Black, false, false);
	if (!RT)
	{
		return nullptr;
	}

	UTexture2D* Dot = MakeSoftDot(32);

	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize(0.0f, 0.0f);
	FDrawToRenderTargetContext Ctx;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(Outer, RT, Canvas, CanvasSize, Ctx);

	if (Canvas && Dot)
	{
		auto DrawStar = [&](float RADeg, float DecDeg, float Mag, float BV)
		{
			if (Mag > MagnitudeLimit)
			{
				return;
			}
			const float U = FMath::Frac(RADeg / 360.0f + 1.0f);
			const float V = FMath::Clamp((90.0f - DecDeg) / 180.0f, 0.0f, 1.0f);
			const float Px = U * Width;
			const float Py = V * Height;

			const float Flux   = FMath::Pow(2.512f, -Mag);                  // relative flux
			const float Bright = FMath::Clamp(0.4f + Flux, 0.4f, 8.0f);     // emissive peak
			const float Size   = FMath::Clamp(5.0f - Mag, 1.5f, 7.0f);      // texels

			const FLinearColor Col = BVtoRGB(BV) * Bright;
			Canvas->K2_DrawTexture(Dot,
				FVector2D(Px - Size * 0.5f, Py - Size * 0.5f),
				FVector2D(Size, Size),
				FVector2D::ZeroVector, FVector2D(1.0f, 1.0f),
				Col, BLEND_Additive, 0.0f, FVector2D::ZeroVector);
		};

		for (const FCatalogStar& S : Stars)
		{
			DrawStar(S.RADeg, S.DecDeg, S.Magnitude, S.ColorBV);
		}

		// Faint procedural fill, uniform on the sphere, for background density.
		for (int32 i = 0; i < ProceduralFill; ++i)
		{
			const float RA  = Hash11(i * 2u + 1u) * 360.0f;
			const float Dec = FMath::RadiansToDegrees(
				FMath::Asin(Hash11(i * 2u + 7u) * 2.0f - 1.0f));
			const float Mag = 4.8f + Hash11(i * 3u + 11u) * 1.6f;   // faint background
			DrawStar(RA, Dec, Mag, 0.4f + Hash11(i * 5u + 3u) * 0.7f);
		}
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(Outer, Ctx);
	return RT;
}
