# MF_BiomeLayer — from-scratch build

Per-biome ground layer: two texture-bombed albedos with paired normals, blended by a
world-space Perlin-Worley variation mask. Outputs BC / N / R / AO for MF_BiomeHub,
which height-blends them against the riverbed set via MF_AutoFadeZ.

Layout coordinates are suggestions matching the original graph; only the wiring matters.

## 0 — Asset

1. Content Browser → `Content/_Custom/Landscape/Functions` → right-click →
   Materials → **Material Function**. Name it `MF_BiomeLayer` (or `_v2` to keep the old
   one until the swap).
2. Open it. Click empty canvas → Details: tick **Expose to Library**; Description:
   `Dual bombed albedo + paired normals, macro variation mask. Outputs BC/N/R/AO.`
3. A new function ships with one FunctionOutput named `Result` — it becomes `BC` later.

## 1 — Function inputs (left column, x ≈ −1500)

Right-click → **Function Input** for each. Set Input Name / Input Type / Sort Priority:

| Input name | Type      | Sort | Notes |
|------------|-----------|------|-------|
| BC_Tex_A   | Texture2D | 0    | primary albedo |
| BC_Tex_B   | Texture2D | 1    | secondary albedo |
| N_Tex_A    | Texture2D | 2    | normal paired with A |
| N_Tex_B    | Texture2D | 3    | normal paired with B |
| Roughness  | Scalar    | 4    | Preview Value X = 0.85, tick **Use Preview Value as Default** |

Texture inputs have no usable default — every call site must wire all four, or the
material that uses the function will not compile.

## 2 — Scalar parameters (internal knobs)

Right-click → **Scalar Parameter** for each. These surface automatically in any MI whose
master calls this function, and are SHARED across all instances of the function:

| Parameter         | Default | Meaning |
|-------------------|---------|---------|
| UV Scale A        | 0.005   | repeats per cm for the A pair (0.005 = one repeat / 2 m) |
| UV Scale B        | 0.003   | repeats per cm for the B pair (~3.3 m) — keep ≠ A |
| VariationScale    | 0.0005  | mask repeats per cm (0.0005 ≈ 20 m patches; smaller = bigger) |
| VariationContrast | 0.5     | mask hardness (1.0 = near-binary patches) |
| NormalStrength    | 1.0     | normal flatten/exaggerate (0 = flat, >1 extrapolates) |
| Ambient Occlusion | 1.0     | KEEP AT 1.0 — this input scales sky/indirect light |

## 3 — UV streams (comment box "UVs", x ≈ −1550..−750)

1. **WorldPosition** node (leave Shader Offsets on *Absolute World Position (Including
   Material Shader Offsets)*).
2. Drag off **XYZ**, double-click the wire to drop a **reroute knot** — every consumer
   taps this knot.
3. **Multiply-A**: A ← knot, B ← `UV Scale A`.
4. **Multiply-B**: A ← knot, B ← `UV Scale B`.
5. **Multiply-V**: A ← knot, B ← `VariationScale`. (Variation is deliberately
   independent of both texture scales.)
6. Select the lot, press **C**, label the comment `UVs`.

The float3 → float2 truncation where these feed V2 pins is intentional (planar XY
mapping); Multiply-V's consumer needs the full float3.

## 4 — Color bombing (x ≈ −950)

1. Drop two **MF_TextureBombing** calls (drag the asset from
   `Content/_Custom/Landscape/Functions` onto the graph).
2. Call A: `Tex` ← BC_Tex_A, `UVs` ← Multiply-A.
3. Call B: `Tex` ← BC_Tex_B, `UVs` ← Multiply-B.

## 5 — Variation mask (x ≈ 600..1300)

1. **TextureSample** node. In Details set Texture =
   `VT_PerlinWorley_Balanced` (`/Engine/EngineSky/VolumetricClouds/` — enable
   **Show Engine Content** in the picker's view options to see it).
   `UVs` ← Multiply-V (volume texture, takes the float3).
2. **CheapContrast** (engine material function): `In (S)` ← TextureSample **RGB**,
   `Contrast (S)` ← `VariationContrast`. (Scalar input truncates RGB to R — intended.)
3. Optional reroute knot on `Result` — it feeds two lerps below.

## 6 — Albedo blend → BC output (x ≈ 1568..1728)

1. **Lerp**: A ← bombing-A `Result`, B ← bombing-B `Result`, Alpha ← CheapContrast
   `Result`.
2. Select the default FunctionOutput: Output Name → `BC`, Sort Priority 0,
   Input ← the lerp.

## 7 — Normal bombing (two Custom nodes, x ≈ −950, y ≈ 500/800)

1. Right-click → **Custom**. In Details:
   - Output Type: **CMOT Float3**
   - Description: `StochasticNormal` (becomes the node title)
   - Inputs array — add THREE entries, names exact and case-sensitive:
     `UV`, `Tex`, `Strength`
   - Code — paste verbatim:

```hlsl
#define HASH4(p) frac(sin(float4(dot(p,float2(127.1,311.7)),dot(p,float2(269.5,183.3)),dot(p,float2(113.5,271.9)),dot(p,float2(246.1,124.6))))*43758.5453)

float2 dX = ddx(UV);
float2 dY = ddy(UV);
float2 iuv = floor(UV);
float2 fuv = frac(UV);
iuv = iuv - floor(iuv / 256.0) * 256.0;

float4 ha = HASH4(iuv + float2(0,0));
float4 hb = HASH4(iuv + float2(1,0));
float4 hc = HASH4(iuv + float2(0,1));
float4 hd = HASH4(iuv + float2(1,1));

float2 fa = sign(ha.zw - 0.5);
float2 fb = sign(hb.zw - 0.5);
float2 fc = sign(hc.zw - 0.5);
float2 fd = sign(hd.zw - 0.5);

float4 sa = Texture2DSampleGrad(Tex, GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings), UV*fa + ha.xy, dX*fa, dY*fa);
float4 sb = Texture2DSampleGrad(Tex, GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings), UV*fb + hb.xy, dX*fb, dY*fb);
float4 sc = Texture2DSampleGrad(Tex, GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings), UV*fc + hc.xy, dX*fc, dY*fc);
float4 sd = Texture2DSampleGrad(Tex, GetMaterialSharedSampler(TexSampler, Material.Wrap_WorldGroupSettings), UV*fd + hd.xy, dX*fd, dY*fd);

float3 na = float3((sa.rg*2-1)*fa, 0); na.z = sqrt(saturate(1 - dot(na.xy,na.xy)));
float3 nb = float3((sb.rg*2-1)*fb, 0); nb.z = sqrt(saturate(1 - dot(nb.xy,nb.xy)));
float3 nc = float3((sc.rg*2-1)*fc, 0); nc.z = sqrt(saturate(1 - dot(nc.xy,nc.xy)));
float3 nd = float3((sd.rg*2-1)*fd, 0); nd.z = sqrt(saturate(1 - dot(nd.xy,nd.xy)));

float2 w = smoothstep(0.25, 0.75, fuv);
#undef HASH4
float3 n = normalize(lerp(lerp(na,nb,w.x), lerp(nc,nd,w.x), w.y));
return normalize(lerp(float3(0,0,1), n, Strength));
```

   `TexSampler` is auto-generated from the `Tex` input — do not declare it.
   The `rg*2-1` unpack expects raw BC5 normal data; Sampler Type *Normal* on the
   texture parameters at the call sites is correct.

2. Wire node A: `UV` ← Multiply-A, `Tex` ← N_Tex_A, `Strength` ← NormalStrength.
3. **Ctrl+C / Ctrl+V** the node (copies code + input declarations). Wire the copy:
   `UV` ← Multiply-B, `Tex` ← N_Tex_B, `Strength` ← NormalStrength (second wire from
   the same parameter).

## 8 — Normal blend → N output

1. **Lerp**: A ← Custom-A output, B ← Custom-B output, Alpha ← the SAME CheapContrast
   `Result` as the albedo blend (this keeps normal and color switching together).
2. **Normalize** ← the lerp. (Lerp+normalize is the correct crossfade for two
   tangent-space normals; BlendAngleCorrectedNormals is for stacking detail, not
   crossfading.)
3. New **FunctionOutput**: Output Name `N`, Sort Priority 1, Input ← Normalize.

## 9 — R and AO outputs (stubs, x ≈ −256, y ≈ 1150/1380)

1. FunctionOutput `R`, Sort Priority 2, Input ← `Roughness` function input.
   (Placeholder until the ORDp G-channel hookup.)
2. FunctionOutput `AO`, Sort Priority 3, Input ← `Ambient Occlusion` parameter.
   (Placeholder until the ORDp R-channel hookup. At any value below 1.0 the landscape
   rejects sky/indirect light — the ground-black-at-night bug.)

## 10 — Validate and save

- Stats bar: no errors.
- Right-click the Normalize → *Start Previewing Node*: lavender normal-ish sphere.
- Right-click the BC lerp → preview: two textures in noise patches.
- Save.

## 11 — Call sites (MF_BiomeHub)

- Rebuilt in place → right-click each BiomeLayer call → **Refresh Node**, rewire any
  orphaned pins.
- New asset → select each old call node → Details → Material Function → pick the new
  function → rewire all five inputs.
- Texture pairing per layer (A = primary, B = secondary; N always matches its BC):
  duplicate the existing texture-object parameter nodes and re-point them — keeps
  Sampler Type Normal etc. consistent.

## 12 — MI starting values

| Param | Value |
|---|---|
| UV Scale A | 0.005 |
| UV Scale B | 0.003 |
| VariationScale | 0.0005 |
| VariationContrast | 0.5 |
| NormalStrength | 1.0 |
| Ambient Occlusion | 1.0 |

Tuning trick: set VariationContrast to 1.0 while dialing the scales (hard patch borders
let you judge A and B independently), then return to ~0.5.

## Footnotes

- Cost: 8 gradient color samples + 8 gradient normal samples + 1 volume sample per
  function instance, ×3 paint layers in the master. Fine for SM6 desktop; the B-side
  normal node is the first thing to cut if budget gets tight.
- Scalar params are shared across every instance of the function. Per-LAYER scales
  (Earth ≠ Water) would require converting them to function inputs fed by per-layer
  parameters.
- The riverbed/height blend is downstream in MF_BiomeHub (MF_AutoFadeZ mask between
  this function's outputs and the RB_* set) — nothing in this function touches it.
