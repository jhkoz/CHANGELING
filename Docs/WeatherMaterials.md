# Weather Materials — Build Guide

How the weather visuals get driven:

```
WeatherController (C++)
   ├─ RainFX / SnowFX Niagara  ← spawn rate (User.Precipitation) + slant (User.Wind)
   └─ MPC_Weather scalars       ← CloudCoverage, WindStrength, Precipitation, Wetness, Snow
                                   (Snow = the accumulate/melt value)
Materials read MPC_Weather to show snow on the ground + wet/puddled surfaces.
```

You build five things: **MPC_Weather**, **M_RainStreak**, **M_SnowFlake**, **MF_SnowCover**, **MF_WetSurface**.

---

## 0. Prerequisite — `MPC_Weather` (do this first)

1. Content Browser → **Add → Materials → Material Parameter Collection**. Name it **`MPC_Weather`** (put it in `_Custom/Weather`).
2. Open it. Under **Scalar Parameters**, click **+** five times and name them exactly:
   `CloudCoverage`, `WindStrength`, `Precipitation`, `Wetness`, `Snow`.
3. Select your **BP_WeatherController** in the level → Details → **Weather|References → WeatherParams** → set it to **`MPC_Weather`**.

That's the bridge: the C++ writes these scalars every frame; the materials below read them.

> Any node that says "Collection Parameter `X`" below = drop a **CollectionParameter** node, set Collection = `MPC_Weather`, Parameter Name = `X`.

---

## 1. Rain sprite — `M_RainStreak`

1. **Add → Materials → Material**. Name **`M_RainStreak`**.
2. Details panel:
   - **Material Domain:** Surface
   - **Blend Mode:** Translucent
   - **Shading Model:** Unlit
   - **Two Sided:** ✓
   - **Usage → Used with Niagara Sprites:** ✓  *(critical, or it won't render on the emitter)*
3. Build the streak shape:
   - **TextureCoordinate** → **ComponentMask (R)** → call it `U`.
   - `U` → **Subtract** (B = 0.5) → **Abs** → **Multiply** (B = 4) → **OneMinus** → **Saturate**. → this is `StreakX` (a thin vertical bar).
   - **TextureCoordinate** → **ComponentMask (G)** → **Subtract** (0.5) → **Abs** → **Multiply** (1.8) → **OneMinus** → **Saturate** → `FadeY`.
   - **Multiply** `StreakX` × `FadeY` → `Shape`.
4. **ParticleColor** node:
   - `Shape` × **ParticleColor.A** × **Constant 0.6** → **Opacity**.
   - **ParticleColor.RGB** × **Constant3Vector (0.6, 0.7, 0.9)** → **Emissive Color**.
5. Save. Assign it on **NS_Rain → Sprite Renderer → Material**.

The velocity‑aligned sprite stretches this sliver into a falling streak.

---

## 2. Snow flake — `M_SnowFlake`

1. **Add → Materials → Material**. Name **`M_SnowFlake`**.
2. Details: **Translucent · Unlit · Two Sided ✓ · Used with Niagara Sprites ✓**.
3. Round soft dot:
   - **TextureCoordinate** → **Distance** (B = Constant2Vector (0.5, 0.5)) → **Multiply** (2) → **OneMinus** → **Saturate** → **Power** (Exp = 1.5) → `Dot`.
4. **ParticleColor**:
   - `Dot` × **ParticleColor.A** → **Opacity**.
   - **ParticleColor.RGB** × **Constant3Vector (0.9, 0.95, 1.0)** → **Emissive Color**.
5. Save. Assign on **NS_Snow → Sprite Renderer → Material**.

---

## 3. Accumulating snow — `MF_SnowCover` (Material Function)

This lays snow on up‑facing surfaces by the live `Snow` level. Build it once, reuse everywhere.

1. **Add → Materials → Material Function**. Name **`MF_SnowCover`**.
2. Add inputs (right‑click → **FunctionInput**):
   - `InBaseColor` (Input Type: **Vector3**, Sort Priority 0)
   - `InRoughness` (Input Type: **Scalar**, Sort Priority 1)
   - `InNormal` (Input Type: **Vector3**, Sort Priority 2)
3. Build the snow mask:
   - **VertexNormalWS** → **ComponentMask (B)** → `Up` (1 = facing straight up).
   - `Up` → **Subtract** (0.3) → **Divide** (0.7) → **Saturate** → `UpMask`.
   - **Collection Parameter `Snow`** → **Multiply** by `UpMask` → **`SnowMask`** (0–1).
4. Snow look (use a `CAA_SnowV4` texture for a real surface, or constants to start):
   - **SnowAlbedo:** a `CAA_SnowV4` snow albedo texture (or Constant3Vector `(0.85, 0.88, 0.95)`).
   - **SnowRoughness:** Constant `0.8`.
   - **SnowNormal:** a `CAA_SnowV4` snow normal (or **Constant3Vector (0,0,1)** flat).
5. Blend (each is a **LinearInterpolate**, Alpha = `SnowMask`):
   - `Lerp(InBaseColor, SnowAlbedo, SnowMask)` → **Output `OutBaseColor`** (FunctionOutput).
   - `Lerp(InRoughness, SnowRoughness, SnowMask)` → **Output `OutRoughness`**.
   - `Lerp(InNormal, SnowNormal, SnowMask)` → **Output `OutNormal`**.
6. Save.

*(Applying it to your surfaces: see Section 5.)*

---

## 4. Wet surfaces & puddles — `MF_WetSurface` (Material Function)

Two effects in one: a **wet sheen** everywhere it's raining, and **puddles** that fill low spots as wetness rises.

1. **Add → Materials → Material Function**. Name **`MF_WetSurface`**.
2. Inputs: `InBaseColor` (Vector3), `InRoughness` (Scalar), `InNormal` (Vector3) — same as before.
3. Up mask (puddles + sheen only land on upward faces):
   - **VertexNormalWS → ComponentMask (B) → Saturate** → `UpMask`.
4. **Collection Parameter `Wetness`** → `Wet` (0–1).
5. **Wet sheen** (applies broadly):
   - `WetMask` = `Wet` × `UpMask`.
   - WetColor = `InBaseColor` × **Constant 0.7** (rain darkens surfaces).
   - WetRough = `Lerp(InRoughness, 0.3, WetMask)` (glossier).
6. **Puddles** (water pooling):
   - Puddle distribution: **AbsoluteWorldPosition → ComponentMask (RG) → Multiply (0.01)** → plug into a tiling **Noise** texture's UV (or a **Noise** node). Output `PuddleHeight` (0–1).
   - `PuddleMask` = `Saturate((Wet - PuddleHeight) * 8)` × `UpMask`.
     *(As `Wet` rises, water covers the lower noise values first — puddles grow.)*
   - Puddle water: **Albedo** `(0.02,0.02,0.03)`, **Roughness** `0.04` (mirror), **Normal** `(0,0,1)` (flat surface).
7. Combine (LinearInterpolate, Alpha = `PuddleMask`):
   - `Lerp(WetColor, PuddleAlbedo, PuddleMask)` → **Output `OutBaseColor`**.
   - `Lerp(WetRough, 0.04, PuddleMask)` → **Output `OutRoughness`**.
   - `Lerp(InNormal, (0,0,1), PuddleMask)` → **Output `OutNormal`**.
8. Save.

> Want fancier puddles later: feed a small ripple normal map (panned by time × `WindStrength`) into the puddle normal so rain stipples the water.

---

## 5. Apply the functions to a surface material

For each material you want weather to affect (start with the ground/terrain):

1. Open the material. Find where **Base Color / Roughness / Normal** currently feed the output.
2. Drag **`MF_SnowCover`** into the graph. Wire your existing Base Color/Roughness/Normal into its `In*` inputs.
3. Drag **`MF_WetSurface`** in **after** it: wire `MF_SnowCover`'s outputs into `MF_WetSurface`'s inputs. *(Wet first then snow, or snow first then wet — snow‑last reads more natural since snow sits on top of a wet ground.)*
4. Wire `MF_WetSurface`'s outputs to the **material's** Base Color / Roughness / Normal.
5. Apply / Save.

Order tip: **Wet → Snow** (snow covers the wet ground). Swap if you prefer slush.

---

## 6. Test & tune

- **Rain/Snow sprites:** set `BP_WeatherController → StartingWeather = Rain` (then `Snow`), Play. Confirm sprites fall and slant with wind.
- **Snow buildup:** set `StartingWeather = Snow`, watch `SnowAccumulation` (Weather|State) climb; the ground whitens over `SnowAccumulateHours`. Switch to `Clear` and it melts over `SnowMeltHours`.
- **Wetness/puddles:** `StartingWeather = Rain`, watch surfaces darken + gloss and puddles fill; switch to `Clear` and they dry as `Wetness` falls.
- If snow/puddles never appear: check `MPC_Weather` is assigned to `WeatherParams`, and that your Collection Parameter nodes point at the right scalar.

---

### Quick reference — which scalar drives what
| MPC scalar     | Driven by (C++)              | Material use                          |
|----------------|------------------------------|---------------------------------------|
| `Snow`         | `SnowAccumulation` (build/melt) | `MF_SnowCover` coverage             |
| `Wetness`      | `Current.Wetness` (blended)  | `MF_WetSurface` sheen + puddle level  |
| `Precipitation`| `Current.Precipitation`      | (Niagara spawn rate, via User param)  |
| `WindStrength` | `Current.WindStrength`       | (Niagara wind / ripple panning)       |
| `CloudCoverage`| `Current.CloudCoverage`      | cloud material coverage               |

---

## 7. Weather VFX — Niagara emitters

The controller drives camera‑following emitters (`RainFX`, `SnowFX`, `DustFX`) and spawns a bolt (`BoltSystem`) on each strike. Rain/snow were covered live; here are the two new ones.

### `NS_Dust` (dust storms) — assign to **DustFX**
Wind‑blown dirt that surrounds the viewer.
1. New Niagara System → empty emitter. Add **User Parameters**: float **`Dust`**, vector **`Wind`** (names must match `DustParam` / `WindParam`).
2. **Emitter Update → Spawn Rate** → Dynamic Input **Multiply Float**: A = `User.Dust`, B ≈ 2000.
3. **Particle Spawn → Initialize Particle**: Lifetime 3–6 s; Color dusty brown `(0.55, 0.45, 0.32)`, Alpha ≈ 0.25; **Sprite Size large** (200–600, random) — billowing puffs.
4. **Box Location**: wide + shallow (≈ 4000 × 4000 × 600) centered on the emitter so dust wraps around you.
5. **Add Velocity** → link to **`User.Wind`** (dust blows horizontally with the storm), plus a small downward bias.
6. **Particle Update → Curl Noise Force** (Strength ≈ 150) → rolling turbulence.
7. **Render → Sprite Renderer**, **Facing Camera**; material = a soft translucent brown puff (a smoke texture, or your star‑dot tinted brown).
8. *(Optional)* a second emitter of small fast streaks for low grit near the ground.

### `NS_Bolt` (arcs crawling the clouds) — assign to **BoltSystem**
A beam drawn from its spawn point to `User.BoltEnd` (the code sets both ends).
1. New Niagara System → empty emitter. Add a vector **User Parameter `BoltEnd`** (must match `BoltEndParam`).
2. **Emitter Update → Spawn Burst Instantaneous**, Count ≈ 40 (particles laid along the beam).
3. **Particle Spawn → Spawn Beam**: **Beam Start** = local origin `(0,0,0)`, **Beam End** = **`User.BoltEnd`** → lays the line from the spawn point to the code's endpoint.
4. **Particle Spawn → Jitter Position** (or a one‑shot Curl Noise offset), strong → the jagged zig‑zag.
5. **Render → Ribbon Renderer**, thin; material = **emissive white‑blue, Additive, Unlit**.
6. **Lifetime ≈ 0.15–0.25 s** — it flashes and dies (the controller auto‑destroys the component).
7. *(Optional, forks)* a second Spawn Beam to 2–3 branch points offset off the main line, shorter‑lived.
8. Assign to **`BoltSystem`**; set **`BoltAltitude`** to your cloud‑deck height, **`BoltSpread`** wide.

> The flash light already lights the sky each strike; `NS_Bolt` adds the visible arc, and `OnLightning` is your hook for the thunder cue (add a little random delay for distance).
