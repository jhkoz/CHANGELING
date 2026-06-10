# Landscape Material — Granular Build (node by node)

Builds the whole stack: 5 paintable biomes (2 textures each) over a height‑aware **riverbed** base. This is the step‑by‑step companion to `LandscapeMaterial.md`.

**Build order** (each depends on the ones above):
1. Textures & folders
2. `MF_AutoFadeZ`      — height mask (no deps)
3. `MF_DistanceBlend`  — near/far lerp (no deps)
4. `MF_TextureBombing` — anti‑tiling sampler (no deps)
5. `MF_BiomeLayer`     — two albedos blended → one layer's attributes (uses 3 & 4)
6. `MF_BiomeHub`       — riverbed composite (optional wrapper)
7. `M_MasterLandscape` — assembles 6 layers
8. `MI_MasterLandscape`— per‑texture instance
9. Layer Info + paint

Conventions: *Add a node* = right‑click the graph and search. **Param** = a Parameter node (shows up in the instance). `→` = wire. Every **Texture Sample**'s **Sampler Source = Shared: Wrap** (keeps you under the 16‑sampler limit).

---

## 1. Textures & folders
- Folder: `Content/_Custom/Landscape/{Functions, Materials, Textures}`.
- Per layer, **two base‑color (albedo) textures** that blend for variation — sRGB **on**. *(Optional: pack Roughness in an Alpha.)*
- *(Optional, shared across layers)* a **detail Normal** (Compression **Normalmap**, sRGB **off**) + a small grayscale **noise** texture for the macro blend.
- Six layers total: `Riverbed, Meadow, Lawn, Grove, Gravel, Sand` (rename to taste).

---

## 2. `MF_AutoFadeZ` — world‑height mask
**Create:** Add → Materials → **Material Function**, name `MF_AutoFadeZ`.
**Inputs** (right‑click → Input → set type):
- `TopZ` (Scalar) — world Z of the waterline.
- `BankHeight` (Scalar) — transition thickness (≈150–400).
- `EdgeJitter` (Scalar, default 0) — waterline wiggle in cm.
- `NoiseScale` (Scalar, default 0.002).

**Graph:**
1. **AbsoluteWorldPosition** → **ComponentMask (B)** = `WorldZ`.
2. *(optional jitter)* **Noise** node (Function = Simplex, Output = Scalar) fed **AbsoluteWorldPosition × `NoiseScale`** → **× `EdgeJitter`** → **Add** to `WorldZ`.
3. **Subtract**: `WorldZ − TopZ`.
4. **Divide** by `BankHeight` → **Saturate** = `t`.
5. S‑curve: `t × t × (3 − 2t)` (Multiply, Multiply, and a `3 − 2t` via Subtract). *(Or use the engine **SmoothStep** node.)*
6. **OneMinus** → **Output `Reveal`** (1 below `TopZ` = low ground / riverbed).

> Reusable: this is also your snow‑line / altitude mask later.

---

## 3. `MF_DistanceBlend` — near/far by camera distance
**Create:** `MF_DistanceBlend`.
**Inputs:** `Near` (Vector3), `Far` (Vector3), `StartDist` (Scalar, e.g. 2000), `Range` (Scalar, e.g. 8000).
**Graph:**
1. **PixelDepth** → `Dist` (distance from camera, cheap).
2. **Subtract** `StartDist` → **Divide** `Range` → **Saturate** = `t`.
3. **Lerp**(A = `Near`, B = `Far`, Alpha = `t`) → **Output `Result`**.

Used in §5 to fade expensive detail into a cheap macro sample at distance.

---

## 4. `MF_TextureBombing` — kill the tiling (one Custom node)
Anti-tiling sampler: per-cell random offset + mirror, 4 derivative-correct taps, bilinear-blended (Inigo Quilez "texture III"). Built as a **single Custom HLSL node** — the pure-node version kept collapsing to one tap (Stats showed 3 lookups), so this is the reliable build.

**Create:** `MF_TextureBombing`.
**Inputs:** `Tex` (**Texture2D**), `UV` (Vector2 — already tiled: `LandscapeLayerCoords × TileScale`).

**Add one `Custom` node** and set:
- **Output Type:** `CMOT Float4`
- **Inputs** (case-sensitive — must match the code exactly): `Tex` ← the `Tex` input pin; `UV` ← the `UV` input pin.

Paste into its **Code** box:

```hlsl
// IQ stochastic non-tiling texture (4-tap). UV must already be tiled (LandscapeCoords * TileScale).
#define HASH4(p) frac(sin(float4(dot(p,float2(127.1,311.7)),dot(p,float2(269.5,183.3)),dot(p,float2(113.5,271.9)),dot(p,float2(246.1,124.6))))*43758.5453)

float2 dX = ddx(UV);
float2 dY = ddy(UV);
float2 iuv = floor(UV);
float2 fuv = frac(UV);

float4 ha = HASH4(iuv + float2(0,0));
float4 hb = HASH4(iuv + float2(1,0));
float4 hc = HASH4(iuv + float2(0,1));
float4 hd = HASH4(iuv + float2(1,1));

float2 fa = sign(ha.zw - 0.5);
float2 fb = sign(hb.zw - 0.5);
float2 fc = sign(hc.zw - 0.5);
float2 fd = sign(hd.zw - 0.5);

float4 ca = Texture2DSampleGrad(Tex, GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings), UV*fa + ha.xy, dX*fa, dY*fa);
float4 cb = Texture2DSampleGrad(Tex, GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings), UV*fb + hb.xy, dX*fb, dY*fb);
float4 cc = Texture2DSampleGrad(Tex, GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings), UV*fc + hc.xy, dX*fc, dY*fc);
float4 cd = Texture2DSampleGrad(Tex, GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings), UV*fd + hd.xy, dX*fd, dY*fd);

float2 w = smoothstep(0.25, 0.75, fuv);
#undef HASH4
return lerp(lerp(ca,cb,w.x), lerp(cc,cd,w.x), w.y);
```

Wire the Custom node output → function **Output `Result`** (`.rgb` for color; `.a` carries packed roughness if you pack it).

**Why a Custom node, not the wired graph:** it bakes in the three bugs that broke the node version — UV uses **Flip** not Offset (`UV*fa`), the derivatives mirror with it (`dX*fa`, `dY*fa`), and the corner order is x-first `(0,0),(1,0),(0,1),(1,1)` to match the blend. All four taps share **one** wrap sampler via `GetMaterialSharedSampler`, so the whole 6-layer material stays at ~1-3 samplers instead of ~18.

**Two caveats:**
- If `Material.Wrap_WorldGroupSettings` errors as undeclared, replace both `GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings)` with just `TexSampler` — compiles identically, but costs one sampler slot per texture.
- The random flip mirrors normal maps on ~half the tiles. Fine for organic ground; if it bothers you, sample the normal flip-free (`UV + ha.xy`, drop the `*fa`/`sign`).

> **Cost:** 4 samples / **1 sampler** per call. Bomb the **albedos**, not the normal (§5). **Verify:** after compile, Stats `Texture Lookups (PS)` should jump from 3 to ~12 per active layer and the visible grid repetition should vanish — if it stays at 3, the Custom node isn't feeding the output (check the wire).

---

## 5. `MF_BiomeLayer` — two albedos → one layer
Each biome is **two base‑color textures blended together** (macro variation that kills repetition), then the usual attributes. Outputs `BaseColor / Normal / Roughness / AO`.

**Create:** `MF_BiomeLayer`.
**Inputs:**
- `BaseColorA`, `BaseColorB` (Texture2D) — the two albedos
- `TileScale` (Scalar, default 1)
- `VariationScale` (Scalar, default 0.02) — macro patch size (*smaller = bigger patches*)
- `VariationContrast` (Scalar, default 1) — A↔B edge hardness
- `RoughnessConst` (Scalar, default 0.85)
- *(optional)* `NormalTex` (Texture2D)

**Graph:**
1. `UV` = **LandscapeLayerCoords** → **Multiply** `TileScale`. *(World‑consistent terrain UVs.)*
2. **De‑tile each albedo:** `BC_A = MF_TextureBombing(BaseColorA, UV)`; `BC_B = MF_TextureBombing(BaseColorB, UV)`.
3. **Macro blend mask** — world‑aligned so it doesn't ride the tiling:
   - **AbsoluteWorldPosition → ComponentMask (R,G)** → **Multiply** `VariationScale` → `MUV`.
   - Sample a tiling grayscale **noise texture** at `MUV` (or a **Noise** node) → `M` (0–1).
   - *(optional sharpen)* `M = CheapContrast(M, VariationContrast)`.
4. **BaseColor** = **Lerp**(`BC_A`, `BC_B`, `M`).
5. **Roughness:** `BaseColorA`'s **Alpha** if packed, else `RoughnessConst`. *(Optionally Lerp A.A ↔ B.A by `M` to vary it too.)*
6. **Normal:** if you have one, plain **Texture Sample** of `NormalTex` (Sampler Type **Normalmap**, Shared: Wrap, UVs = `UV`) → `N`; else **Constant3Vector (0,0,1)** (flat). *(Don't bomb normals — the mirror flips tangent X/Y. One shared detail normal at the master is the cheap way to add bump.)*
7. **AO:** **Constant 1** (or a packed alpha).
8. **Outputs** (4 FunctionOutputs): `BaseColor`, `Normal`, `Roughness`, `AO`.

> **Cost:** two bombed albedos = **8 taps** per biome, but still only **2 samplers** (Shared: Wrap) + 1 for the noise. If that's heavy: wrap `BaseColorB`'s bombing in `MF_DistanceBlend` (de‑tile B only up close), or just plain‑sample B — the macro Lerp already hides its tiling.

Reused six times (5 biomes + riverbed). *(For the riverbed, feed its two albedos too — or pass one texture into both A and B for a plain layer.)*

---

## 6. `MF_BiomeHub` — riverbed composite *(optional wrapper)*
Folds the final "riverbed underneath" lerp into one node. (Skip it and inline the four Lerps in the master if you prefer.)
**Inputs:** `BiomeBC`,`BiomeN` (V3), `BiomeR`,`BiomeAO` (S); `RbBC`,`RbN` (V3), `RbR`,`RbAO` (S); `Reveal` (S).
**Graph:**
1. `Show = OneMinus(Reveal)`.
2. `OutBC  = Lerp(RbBC,  BiomeBC,  Show)`
3. `OutN   = Normalize( Lerp(RbN, BiomeN, Show) )`
4. `OutR   = Lerp(RbR,   BiomeR,   Show)`
5. `OutAO  = Lerp(RbAO,  BiomeAO,  Show)`
6. Outputs: `BaseColor, Normal, Roughness, AO`.

Riverbed is the **A** side of every Lerp → it's the base; biomes ride on top wherever `Reveal` = 0 (high ground).

---

## 7. `M_MasterLandscape`
**Create** material `M_MasterLandscape`. Details: **Usage → Used with Landscape ✓**, Shading **Default Lit**, Two Sided off. *(Leave "Use Material Attributes" OFF — we wire attributes individually.)*

**7a. Build the six layers**
For each layer create **two Texture Object Parameter** nodes (e.g. `Meadow_BaseColor`, `Meadow_Normal`) and one **ScalarParameter** `Meadow_TileScale` (default 1). Wire them into an `MF_BiomeLayer` call:
- `BaseColorTex ← Meadow_BaseColor`, `NormalTex ← Meadow_Normal`, `TileScale ← Meadow_TileScale`.
Repeat for `Lawn, Grove, Gravel, Sand`, and `Riverbed`. You now have 6 layer blocks, each exposing BaseColor/Normal/Roughness/AO.

**7b. Blend the five biomes** (per attribute)
Add **four** `Landscape Layer Blend` nodes — one each for **BaseColor, Normal, Roughness, AO**. In every one, add **5 elements** named exactly `Meadow, Lawn, Grove, Gravel, Sand`, **Blend Type = LB Weight Blend**, a sensible **Preview Weight** (1 on Meadow, 0 elsewhere). Wire each biome's matching output into its element's **Layer Input**.
Results: `BiomeBC, BiomeN, BiomeR, BiomeAO`.

**7c. Height reveal**
Add `MF_AutoFadeZ`. Promote its inputs to params: `RiverbedTopZ`, `BankHeight`, `EdgeJitter`, `NoiseScale`. Output = `Reveal`.

**7d. Composite riverbed under biomes**
Feed `MF_BiomeHub`:
- Biome side ← `BiomeBC/N/R/AO`
- Riverbed side ← the **Riverbed** `MF_BiomeLayer` outputs
- `Reveal` ← step 7c
Wire `MF_BiomeHub`'s outputs to the material's **Base Color, Normal, Roughness, Ambient Occlusion**. Set **Metallic = 0**, **Specular = 0.5** (constants). **Save.**

---

## 8. `MI_MasterLandscape`
Right‑click `M_MasterLandscape` → **Create Material Instance** → `MI_MasterLandscape`.
- Assign every `*_BaseColor` / `*_Normal` texture parameter to real textures.
- Set each `*_TileScale` (start ~ landscape size / 8).
- Set **`RiverbedTopZ`** to your waterline's world Z, **`BankHeight`** ≈ 250, **`EdgeJitter`** ≈ 100.

---

## 9. Layer Info + paint
1. Assign `MI_MasterLandscape` to the landscape (Landscape Mode → selected actor → Material).
2. **Paint** tab → for each of the **5 biomes** click the **+** beside the layer → **Weight‑Blended Layer (normal)** → save the **Layer Info** asset. *(No Layer Info for Riverbed — it's height‑driven.)*
3. Paint **Meadow** across everything first (so unpainted ≠ black), then detail the others.
4. The **riverbed appears by itself** in low ground. Nudge `RiverbedTopZ` until it fills the channels + central moat to match your water plane.

---

## Test & tune
- **Riverbed too high/low:** adjust `RiverbedTopZ`. Banks too sharp: raise `BankHeight`. Waterline too straight: raise `EdgeJitter`.
- **Tiling still visible:** raise per‑layer `TileScale`, confirm bombing is wired on BaseColor.
- **Sampler limit error:** make sure every Texture Sample is **Shared: Wrap**; drop optional packed‑alpha extra samples; or cut bombing to fewer layers.
- **Perf:** bombing is 4×; if heavy, wrap BaseColor in `MF_DistanceBlend` (bombed near, plain far) or use the Lite macro‑noise detiling.

## Quick reference
| Function | Inputs | Outputs |
|---|---|---|
| `MF_AutoFadeZ` | TopZ, BankHeight, EdgeJitter, NoiseScale | Reveal (1 = low) |
| `MF_DistanceBlend` | Near, Far, StartDist, Range | Result |
| `MF_TextureBombing` | Tex, UV | Result (detiled RGB) |
| `MF_BiomeLayer` | 2 textures, TileScale, RoughnessConst | BaseColor, Normal, Roughness, AO |
| `MF_BiomeHub` | biome×4, riverbed×4, Reveal | BaseColor, Normal, Roughness, AO |
| `M_MasterLandscape` | (params) | the landscape shader |
