// CantripTypes.cpp
//
// Only the spellbook palette needs code: the rest of CantripTypes.h is plain data.
//
// The colours are the stained-glass scheme the Art roundels are drawn in. A leaded
// window is three materials and the book uses them the same way -- came (the dark
// structure, which carries no meaning), glass (saturated colour, spent only where
// meaning lives), and light (warm gold, reserved for what is actually lit). Which is
// why "known" is gold rather than a system highlight: she is not selecting the
// cantrip, she has it.

#include "CantripTypes.h"

namespace
{
	/**
	 * An sRGB hex string, as the Designer's colour picker shows it, with an explicit
	 * alpha.
	 *
	 * Authoring in hex rather than float triples so these can be compared by eye
	 * against the palette they came from. FLinearColor's FColor constructor does the
	 * sRGB-to-linear conversion; alpha is linear either way, so it is set after.
	 */
	FLinearColor Glass(const TCHAR* Hex, float Alpha = 1.0f)
	{
		FLinearColor Colour(FColor::FromHex(Hex));
		Colour.A = Alpha;
		return Colour;
	}
}

FCantripPalette::FCantripPalette()
	// Row backing. Deliberately faint: these sit over the page ground and should
	// read as a pane catching light, not as a selected item in a list.
	: RowKnown          (Glass(TEXT("E3B34C"), 0.07f))
	, RowNextToLearn    (Glass(TEXT("A87BD4"), 0.09f))
	, RowLocked         (FLinearColor::Transparent)

	, NameKnown         (Glass(TEXT("EDE6D6")))
	, NameNextToLearn   (Glass(TEXT("C9AEE4")))
	, NameLocked        (Glass(TEXT("5E6474")))

	// White, so the roundel's own colours come through unaltered. Locked is dimmed
	// rather than tinted -- unlit glass, not grey glass. 0.28 rather than the 0.18
	// this started at, because on the near-black ground 0.18 disappears entirely.
	, IconLit           (FLinearColor::White)
	, IconLocked        (Glass(TEXT("FFFFFF"), 0.28f))

	// Bound to a hotbar key sits brightest of the three lit states: it is the only
	// one that says something about what she has done rather than what she can do.
	, ChipBound         (Glass(TEXT("E3B34C"), 0.20f))
	, ChipKnown         (Glass(TEXT("E3B34C"), 0.16f))
	, ChipNextToLearn   (Glass(TEXT("A87BD4"), 0.18f))
	, ChipLocked        (Glass(TEXT("FFFFFF"), 0.04f))

	, ChipTextLit       (Glass(TEXT("C4CBD6")))
	, ChipTextLocked    (Glass(TEXT("5E6474")))
{
}

FCantripGlass::FCantripGlass()
	// Each step toward the reader is a shade lighter, so the three columns separate
	// on value alone even before the came goes in.
	//
	// Not quite opaque: a trace of the garden behind the glass, which is the point of
	// glass. The panes nest, so the rail and detail columns end up around 0.99 while
	// the page's margins stay at 0.92 -- tune the alpha in class defaults, no rebuild.
	: Rail        (Glass(TEXT("070A11"), 0.92f))
	, Page        (Glass(TEXT("0B0E16"), 0.92f))
	, Detail      (Glass(TEXT("10141F"), 0.92f))

	// One line, one width, everywhere. Two pixels would read as a border; one reads
	// as leading, which is the difference between a panel and a window. Lifted from
	// 2A2F3E, where at 1.4:1 against the page the panes barely separated at all -- this
	// is still leading rather than border, just leading you can actually see.
	, Came        (Glass(TEXT("3C4356")))
	, CameWidth   (1.0f)
	, PaneRadius  (5.0f)

	// Ink, measured against the LIGHTEST ground (Detail, 10141F) because that is the
	// worst case -- every value below clears 4.5:1 there, so nothing in the book is
	// text you have to lean in to read. The hierarchy is unchanged, only the floor:
	// Parchment 14.8 : Pewter 7.1 : Muted 5.0.
	, Parchment   (Glass(TEXT("EDE6D6")))
	, Pewter      (Glass(TEXT("9AA0B4")))
	, Muted       (Glass(TEXT("7E8599")))

	, Gold        (Glass(TEXT("E3B34C")))
	, Amethyst    (Glass(TEXT("BB94E0")))

	// The compass the garden is laid out on: Air east, Fire south, Water west,
	// Earth north, and Wyrd for the centre.
	//
	// Three of these were mixed for a light page and lost on a dark one -- Water was the
	// worst at 3.2:1. Brightened until each clears 4.5, and Water chosen for distance
	// from Air specifically (1.9:1 apart) so the two cool colours stay tellable apart
	// rather than both drifting to the same pale cyan.
	, Air         (Glass(TEXT("7FC4E8")))
	, Fire        (Glass(TEXT("D9583A")))
	, Water       (Glass(TEXT("2E8FAF")))
	, Earth       (Glass(TEXT("66B37C")))
	, Wyrd        (Glass(TEXT("BB94E0")))
{
}

FLinearColor FCantripGlass::ForElement(ECantripElement Element) const
{
	switch (Element)
	{
	case ECantripElement::Air:   return Air;
	case ECantripElement::Fire:  return Fire;
	case ECantripElement::Water: return Water;
	case ECantripElement::Earth: return Earth;
	default:                     return Wyrd;
	}
}
