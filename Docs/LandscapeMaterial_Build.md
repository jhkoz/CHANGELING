# Landscape Material — Granular Build (node by node)

Builds the whole stack: 5 paintable biomes (2 textures each) over a height‑aware **riverbed** base. This is the step‑by‑step companion to `LandscapeMaterial.md`.

**Build order** (each depends on the ones above):
1. Textures & folders
2. `MF_AutoFadeZ`      — height mask (no deps)
3. `MF_DistanceBlend`  — near/far lerp (no deps)
4. `MF_TextureBombing` — anti‑tiling sampler (no deps)
5. `MF_BiomeLayer`     — one layer → BaseColor/Normal/Roughness/AO (uses 3 & 4)
6. `MF_BiomeHub`       — riverbed composite (optional wrapper)
7. `M_MasterLandscape` — assembles 6 layers
8. `MI_MasterLandscape`— per‑texture instance
9. Layer Info + paint

Conventions: *Add a node* = right‑click the graph and search. **Param** = a Parameter node (shows up in the instance). `→` = wire. Every **Texture Sample**'s **Sampler Source = Shared: Wrap** (keeps you under the 16‑sampler limit).

---

## 1. Textures & folders
- Folder: `Content/_Custom/Landscape/{Functions, Materials, Textures}`.
- Per layer, two textures:
  - **BaseColor** — sRGB **on**. (Optional: pack **Roughness in the Alpha**.)
  - **Normal** — Compression **Normalmap**, sRGB **off**. (Optional: pack **AO in the Alpha**.)
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

## 4. `MF_TextureBombing` — kill the tiling (heaviest function)
Samples the texture across a cell grid with per‑cell random offset + mirror, 4 taps, derivative‑correct mips, bilinear‑blended. (Inigo Quilez "texture III".)

**Create:** `MF_TextureBombing`.
**Inputs:** `Tex` (**Texture2D**), `UV` (Vector2).

**4a. Setup**
1. **Floor**(`UV`) = `iuv`;  **Frac**(`UV`) = `fuv`.
2. **DDX**(`UV`) = `ddx`;  **DDY**(`UV`) = `ddy`.

**4b. Per‑cell hash** — build it once, reuse for 4 corners. For a cell `C` (Vector2):
- `d1 = Dot(C, (127.1, 311.7))`
- `d2 = Dot(C, (269.5, 183.3))`
- `d3 = Dot(C, (113.5, 271.9))`
- `d4 = Dot(C, (246.1, 124.6))`
- `V4 = Append(Append(d1,d2), Append(d3,d4))`  *(AppendVector twice → Vector4)*
- `H = Frac( Sine(V4) × 43758.5453 )`  → a stable Vector4 in [0,1)
  - `Offset = H.xy` (ComponentMask RG)
  - `Flip   = Sign( H.zw − 0.5 )` (ComponentMask BA → Subtract 0.5 → **Sign**) = ±1 mirror.

> Tip: make the hash its own tiny `MF_Hash44` (input `C`, output `H`) and call it 4×, or just duplicate the node cluster for the 4 corners below.

**4c. One tap** — for corner `k` ∈ {(0,0),(1,0),(0,1),(1,1)}:
1. `C_k = iuv + k`.
2. Hash `C_k` → `Offset_k`, `Flip_k`.
3. `uv_k = UV × Flip_k + Offset_k`.
4. `ddx_k = ddx × Flip_k`;  `ddy_k = ddy × Flip_k`.
5. **Texture Sample**: Texture = the `Tex` input pin; **Sampler Source = Shared: Wrap**; **MipValueMode = Derivative (DDX/DDY)** (this exposes DDX/DDY pins); UVs = `uv_k`, DDX = `ddx_k`, DDY = `ddy_k`. Output `S_k` (RGB).

Do this for all four corners → `S00, S10, S01, S11`.

**4d. Blend**
1. `w = SmoothStep(0.25, 0.75, fuv)` (Vector2) — or `Saturate((fuv−0.25)/0.5)` then the cubic.
2. `rowA = Lerp(S00, S10, w.x)`;  `rowB = Lerp(S01, S11, w.x)`.
3. `Result = Lerp(rowA, rowB, w.y)` → **Output `Result`**.

> **Cost:** 4 samples per call. Bomb **BaseColor only** (see §5). For a *Lite* build, skip this function and instead multiply BaseColor by a large‑scale (×0.01 UV) grayscale **Noise** to break macro repetition — one sample, much cheaper.

---

## 5. `MF_BiomeLayer` — one layer → 4 attributes
**Create:** `MF_BiomeLayer`.
**Inputs:**
- `BaseColorTex` (Texture2D), `NormalTex` (Texture2D)
- `TileScale` (Scalar, default 1)
- `RoughnessConst` (Scalar, default 0.85)

**Graph:**
1. **LandscapeLayerCoords** (Mapping Scale = 1) → **Multiply** `TileScale` = `UV`. *(World‑consistent terrain UVs.)*
2. **BaseColor:** `MF_TextureBombing`(Tex = `BaseColorTex`, UV = `UV`) → `BC` (RGB).
   - *(optional)* feed `BC` as **Near** of an `MF_DistanceBlend`, and a plain Texture Sample of `BaseColorTex` (no bombing) as **Far** → cheaper at distance.
3. **Roughness:** if packed, take `BaseColorTex`'s **Alpha** (sample once more, or reuse a sample's A) → `Rough`; else use `RoughnessConst`.
4. **Normal:** plain **Texture Sample** of `NormalTex` (Sampler Type **Normalmap**, Shared: Wrap), UVs = `UV` → `N` (RGB). *(Don't bomb normals — the per‑cell mirror flips tangent X/Y and breaks lighting.)*
5. **AO:** `NormalTex`.Alpha if packed, else **Constant 1**.
6. **Outputs** (4 FunctionOutputs): `BaseColor` = `BC`, `Normal` = `N`, `Roughness` = `Rough`, `AO` = `AO`.

Reused six times (5 biomes + riverbed).

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
