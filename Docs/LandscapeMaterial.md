# Landscape Material — 5 Biomes over a Height-Aware Riverbed Base

Goal: `M_MasterLandscape` with **6 layers** — five paintable biomes (2 textures each) sitting on top of a **riverbed** (2 textures) that auto-reveals in low ground by world height. Built to slot into your existing `MF_BiomeLayer / MF_BiomeHub / MF_TextureBombing / MF_AutoFadeZ / MF_DistanceBlend`.

## Texture budget — 2 per layer
Each layer is two samplers, packed for full PBR:
- **Tex A — BaseColor**: RGB = albedo, **A = Roughness** (optional; else a scalar).
- **Tex B — Normal**: RGB = normal, **A = AO or Height** (optional).

6 layers × 2 = **12 samplers**. Set every sampler's **Sampler Source = Shared: Wrap** so you stay under the 16-sampler limit — `MF_TextureBombing` re-taps the *same* texture (more instructions, **no** extra samplers), so shared samplers keep it legal.

## Signal flow
```
Biome1..5 ── MF_BiomeLayer (Tex A + Tex B) ──►─┐
                                                ├─ Landscape Layer Blend (Weight) ─► BiomeAttr ─┐
Riverbed  ── MF_BiomeLayer (Tex A + Tex B) ────────────────────────────────────► RiverbedAttr ─┤
                                                                                                │
World Z ── MF_AutoFadeZ ─► RiverbedReveal (1 = low) ── OneMinus ─► BiomeShowMask ───────────────┘
                                                                                                ▼
        BlendMaterialAttributes( A = RiverbedAttr, B = BiomeAttr, Alpha = BiomeShowMask ) ─► Result
```
The riverbed is the **A** (base) input — literally underneath. Biomes ride on top *except* where the ground drops below the waterline, where height pulls the riverbed through.

> Turn on **Use Material Attributes** on `M_MasterLandscape` so every layer carries a full attribute set (BaseColor/Normal/Roughness/AO) through the blends.

---

## 0. Layer Info objects
Create **6 Landscape Layer Info** assets, all **Weight-Blended**, e.g. `Riverbed, Meadow, Lawn, Grove, Gravel, Sand`. (Names are yours; "Riverbed" is special only in the graph — see §4 — not in painting.)

## 1. One biome layer — `MF_BiomeLayer` (fed 2 textures)
Per biome, inside / driven by `MF_BiomeLayer`:
1. **Tex A (BaseColor)** and **Tex B (Normal)** as **Texture Parameters** (so instances swap them), Sampler Source **Shared: Wrap**.
2. Sample both through **`MF_TextureBombing`** at a `TileScale` parameter — kills visible repetition.
3. *(Optional)* run albedo/normal through **`MF_DistanceBlend`** — crisp near, calm far.
4. Roughness = `Tex A.A` (if packed) or a `Roughness` scalar; AO = `Tex B.A` or 1.
5. **MakeMaterialAttributes**: BaseColor, Normal, Roughness, AO, Specular 0.5, Metallic 0.
   → the biome's **MaterialAttributes** output.

Do this five times — one **Material Instance** per biome, swapping the 2 textures + `TileScale`.

## 2. Blend the five biomes — `MF_BiomeHub` / Landscape Layer Blend
- **Landscape Layer Blend** node, **LB Weight Blend**, 5 entries (`Meadow…Sand`); each **Layer Input** = that biome's MaterialAttributes; set a **Preview Weight** per layer for editor preview.
- Output = **`BiomeAttr`**. *(If `MF_BiomeHub` already wraps this blend, just feed it the five attribute sets.)*

## 3. Riverbed layer (2 textures)
Same as a biome (§1) with riverbed textures — wet gravel / mud / cobble:
- Lower **Roughness** (it's wet), slightly **darker** BaseColor.
- **MakeMaterialAttributes → `RiverbedAttr`**.

## 4. Height-aware reveal — `MF_AutoFadeZ`
Decides where the riverbed wins.
- Feed **AbsoluteWorldPosition → ComponentMask (B)** = world **Z** in.
- Parameters: **`RiverbedTopZ`** (world Z of the waterline) and **`BankHeight`** (transition thickness, ~150–400).
- Set it so **below `RiverbedTopZ` → 1, above → 0**. If you need the explicit math:
  ```
  Reveal = 1 - Saturate( (WorldZ - RiverbedTopZ) / BankHeight )
  ```
  Soft banks (optional S-curve): `Reveal = Reveal*Reveal*(3 - 2*Reveal)`.
- **Wiggle the waterline** so it isn't a dead contour: add `Noise × EdgeJitter` to `WorldZ` before the compare (world-aligned **Noise**, scale ~0.002, `EdgeJitter` ~100).
- *(Optional)* allow hand-painting riverbed too: `Reveal = max(Reveal, RiverbedPaintWeight)` from a 6th paint layer.
- Output = **`RiverbedReveal`** (1 = riverbed / low ground).

## 5. Final blend — riverbed underneath the five
- **OneMinus** `RiverbedReveal` → **`BiomeShowMask`**.
- **BlendMaterialAttributes**: **A = `RiverbedAttr`** (base), **B = `BiomeAttr`** (the five), **Alpha = `BiomeShowMask`**.
- Wire its output into the material's **Material Attributes** result pin.

Done: the riverbed is the height-aware base; the five biomes paint on top everywhere the ground sits above the waterline.

## 6. (Nice) make the riverbed actually wet — weather tie-in
Reuse the weather MPC you already built:
- **CollectionParameter `Wetness`** (`MPC_Weather`) → `Lerp(dryRough, 0.1, Wetness)` on the riverbed roughness, and darken its BaseColor as it rains.
- Nudge **`RiverbedTopZ`** up slightly with `Precipitation` so channels swell during storms.

## 7. Landscape setup & paint
1. Assign **`MI_MasterLandscape`**; in **Paint**, bind each Layer Info.
2. Paint one base biome everywhere first (so unpainted ground isn't black), then detail the rest.
3. **The riverbed needs no painting** — it appears wherever terrain drops below `RiverbedTopZ`. Tune `RiverbedTopZ` until it fills your channels + central moat exactly, matching the water plane.

---

## Quick reference
| Piece              | Node / function                                   | Output           |
|--------------------|---------------------------------------------------|------------------|
| 2-texture biome    | `MF_BiomeLayer` + `MF_TextureBombing`             | MaterialAttributes |
| Blend 5 biomes     | Landscape Layer Blend (Weight) / `MF_BiomeHub`    | `BiomeAttr`      |
| Riverbed (wet)     | `MF_BiomeLayer` (2 tex)                            | `RiverbedAttr`   |
| Height reveal      | `MF_AutoFadeZ` (World Z, `RiverbedTopZ`, `BankHeight`) | `RiverbedReveal` |
| Compose            | `BlendMaterialAttributes(A=Riverbed, B=Biomes, Alpha=1−Reveal)` | Result |

Set `RiverbedTopZ` to your garden's waterline and the riverbed fills every channel automatically — a height-aware base beneath all five biomes.
