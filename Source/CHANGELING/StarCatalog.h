// StarCatalog.h
//
// Real-star catalogue → equirectangular render-target baker.
// Produces a masked-dome-friendly star texture (bright dots on black) from real star
// positions (RA / Dec / magnitude / colour). Ships with the brightest naked-eye stars
// embedded; optionally appends the full HYG catalogue from a CSV for the complete sky.

#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

/** A single catalogue star in equatorial coordinates. */
struct FCatalogStar
{
	float RADeg     = 0.0f;   // right ascension, degrees [0,360)
	float DecDeg    = 0.0f;   // declination, degrees [-90,90]
	float Magnitude = 6.0f;   // apparent visual magnitude (lower = brighter)
	float ColorBV   = 0.6f;   // B-V colour index (blue <0 … white ~0.6 … red >1.5)
};

namespace StarCatalog
{
	/** Append the embedded set of brightest naked-eye stars (real positions). */
	void AppendBrightStars(TArray<FCatalogStar>& Out);

	/** Append stars from an HYG-format CSV (columns ra[hours], dec, mag, ci). Returns count added. */
	int32 AppendFromHYGCsv(const FString& FullPath, TArray<FCatalogStar>& Out);

	/**
	 * Bake the catalogue into an equirectangular render target (bright dots on black).
	 * @param Outer           world-context object (the manager actor)
	 * @param MagnitudeLimit  faintest star to draw
	 * @param ProceduralFill  faint background stars to scatter for density (0 = none)
	 */
	UTextureRenderTarget2D* Bake(
		UObject* Outer,
		const TArray<FCatalogStar>& Stars,
		int32 Width, int32 Height,
		float MagnitudeLimit,
		int32 ProceduralFill);
}
