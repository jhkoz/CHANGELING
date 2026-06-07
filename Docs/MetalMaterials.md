# Metal Materials — Wiring Guide

One **master** material drives every mundane metal through instances; a **magic** variant adds the enchanted layers on top. PBR metal is simple: **Metallic = 1**, **Base Color = the metal's reflectance tint**, **Roughness = polish vs wear**. (For metals the Base Color *is* the specular colour — Specular stays at 0.5 and is ignored.)

---

## 1. Master mundane metal — `M_Metal_Master`

New Material, **Surface · Opaque · Default Lit**.

**Metallic**
- **ScalarParameter `Metallic`** = `1.0` → **Metallic** output.

**Base Color**
- **VectorParameter `BaseTint`** (the metal colour).
- *(Optional dirt)* **TextureSample `WearTex`** (a tiling grunge, R channel) → `Lerp(BaseTint, BaseTint × 0.4, WearR × ScalarParameter DirtAmount)`.
- → **Base Color**.

**Roughness**
- **ScalarParameter `Roughness`** (base polish).
- *(Optional wear)* `Lerp(Roughness, ScalarParameter RoughWorn, WearR × ScalarParameter WearAmount)` — worn areas read rougher.
- → **Roughness**.

**Normal**
- **TextureSample `DetailNormal`** (scratches/brushing, tiling) → scale by **ScalarParameter `NormalStrength`** → **Normal**.

Make **Material Instances** of this for each metal below and just set `BaseTint` + `Roughness`.

---

## 2. Mundane metal instances

Linear Base Color values (these are the physically‑correct metal reflectances):

| Metal            | BaseTint (R,G,B)        | Roughness |
|------------------|-------------------------|-----------|
| Iron / Steel     | 0.56, 0.57, 0.58        | 0.35      |
| Polished steel   | 0.56, 0.57, 0.58        | 0.12      |
| Gold             | 1.00, 0.77, 0.34        | 0.18      |
| Silver           | 0.97, 0.96, 0.92        | 0.15      |
| Copper           | 0.95, 0.64, 0.54        | 0.30      |
| Brass            | 0.91, 0.78, 0.42        | 0.30      |
| Bronze           | 0.80, 0.50, 0.30        | 0.40      |
| Aluminium        | 0.91, 0.92, 0.92        | 0.25      |
| Pewter           | 0.62, 0.62, 0.65        | 0.45      |

**Rust / patina:** rust isn't metal. Add a mask `RustMask` and drive **Metallic → `1 - RustMask`**, blend Base Color toward brown `(0.30,0.18,0.12)`, push Roughness to ~0.8 in the masked areas.

---

## 3. Magic layer — `M_MetalMagic_Master`

Duplicate `M_Metal_Master`; add these into **Emissive Color** (and one into Base Color). Mix and match per metal.

**A. Glowing runes / veins**
- **TextureSample `RuneMask`** (a rune/crack/vein pattern; or a **Voronoi**/Noise node for organic veins).
- **Pulse:** `Time` → ×`ScalarParameter PulseSpeed` → **Sine** → ×0.5 → +0.5 → ×0.6 → +0.4  *(breathes 0.4–1.0)*.
- `RuneMask × VectorParameter GlowColor × ScalarParameter GlowIntensity × Pulse` → **add to Emissive**.

**B. Fresnel rim glow** (ethereal edge light)
- **Fresnel** (ExponentIn ≈ 4) → × `VectorParameter RimColor` × `ScalarParameter RimIntensity` → **add to Emissive**.

**C. Iridescence** (oil‑slick / mythril hue shift)
- **Fresnel** (ExponentIn ≈ 2) → `Lerp(BaseTint, VectorParameter ShiftColor, fresnel)` → feed this into **Base Color** instead of raw `BaseTint`. The metal shifts hue at grazing angles.

**D. Energy flow** (optional, for living metal)
- **TexCoord** → **Panner** (Speed driven by `Time`) → **TextureSample `FlowTex`** → × `GlowColor` → **add to Emissive**.

So: **Emissive = RuneGlow + RimGlow (+ EnergyFlow)**, and Base Color optionally runs through the iridescence Lerp.

---

## 4. Magic metal instances

| Metal           | Base (R,G,B)         | Rough | GlowColor        | RimColor      | Layers used        |
|-----------------|----------------------|-------|------------------|---------------|--------------------|
| **Mythril**     | 0.97, 0.96, 0.92     | 0.12  | —                | 0.3, 0.6, 1.0 | B (faint) + C blue |
| **Fae‑gold**    | 1.00, 0.77, 0.34     | 0.20  | 0.6, 1.0, 0.4    | 0.8, 1.0, 0.5 | A (soft) + C green |
| **Runed iron**  | 0.20, 0.20, 0.22     | 0.45  | 1.0, 0.40, 0.10  | —             | A (strong pulse)   |
| **Voidsteel**   | 0.08, 0.08, 0.10     | 0.30  | 0.6, 0.2, 1.0    | 0.4, 0.1, 0.8 | A + B purple       |
| **Moonsilver**  | 0.90, 0.93, 1.00     | 0.10  | 0.5, 0.7, 1.0    | 0.6, 0.8, 1.0 | B + D slow flow    |
| **Quicksilver** | 0.97, 0.96, 0.92     | 0.05  | —                | iridescent    | C + D (mirror flow)|

Tips: keep `GlowIntensity` modest (≈ 1–4) so bloom does the work; slower `PulseSpeed` (≈ 1–2) reads as "alive," fast reads as "unstable/cursed."

---

## 5. Cold iron — the anti‑fae metal

For a changeling setting, **cold iron** is the one metal that *won't* take enchantment — so it's deliberately the **mundane master at its dullest**, no magic layer:
- BaseTint **0.34, 0.35, 0.36** (dark, desaturated), Roughness **0.60**, heavy `WearAmount`.
- No emissive, no Fresnel, no iridescence — it actively *reads* as a magic dead‑zone next to the glowing fae metals.
- Gameplay hook: if you ever want it to *suppress* nearby magic visually, drive a "near cold iron" scalar into the magic metals' `GlowIntensity` to dim them.

---

### Quick reference
| Want…                  | Wire…                                             |
|------------------------|---------------------------------------------------|
| Any solid metal        | Metallic 1 + BaseTint + Roughness                 |
| Worn/dirty             | WearTex into Roughness (up) + Base Color (down)   |
| Glowing runes          | RuneMask × GlowColor × Pulse → Emissive           |
| Ethereal edges         | Fresnel × RimColor → Emissive                     |
| Mythril/oil sheen      | Fresnel → Lerp(BaseTint, ShiftColor) → Base Color |
| Flowing energy         | Panner → FlowTex × GlowColor → Emissive           |
| Anti‑magic (cold iron) | Mundane master, dull, no emissive                 |
