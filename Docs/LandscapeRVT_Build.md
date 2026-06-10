# Landscape - Runtime Virtual Texture (RVT) Conversion

**Why:** `M_MasterLandscape` is heavy (6 texture-bombed layers). A weight-blend landscape compiles the full render-pass set (base/depth/shadow/velocity/...) per **layer combination per component**, so every first paint stalls on shader compiles. RVT bakes the heavy layered shading into a virtual texture **once**: the on-screen landscape samples that VT (cheap, no per-combo permutations), and only the VT-write pass carries the layers - collapsing the compile thrash.

**Result:** main pass = one cheap RVT sample (never recompiles on paint). VT-write pass = the heavy layers, **one** pass, lazily rendered. Painting gets snappy.

**Tradeoff (read this):** the RVT is a finite-resolution cache, so up close the baked result can look **softer** than direct sampling. You trade some close-up crispness (and a bit of VRAM) for the compile/runtime win. Tune it with the RVT **Size** (Section 6). For a garden viewed at character height this is usually fine; if you need hero-crisp ground at your feet, size it up or keep direct sampling on that area.

---

## 0. Prerequisites
- Project Settings -> Rendering -> **Enable virtual texture support = On** (default in UE5; toggling needs an editor restart).
- Know your landscape's footprint - the RVT volume must cover all of it.

## 1. Create the RVT asset
1. Content Browser -> **Add -> Textures -> Runtime Virtual Texture**. Name it `RVT_Landscape`.
2. Open it and set:
   - **Content** = `Base Color, Normal, Roughness, Specular`.
   - **Size of the Virtual Texture** - start moderate (total ~8192-16384; Tile Size 256; Tile Border 4). Raise later if it looks soft.
   - Leave the rest default.

## 2. Place and fit the RVT Volume
1. Place Actor -> **Runtime Virtual Texture Volume**.
2. Details -> **Virtual Texture** = `RVT_Landscape`.
3. Set **Bounds Align Actor** = your Landscape, then click **Set Bounds**. The box snaps to cover the terrain (you'll see it framing the landscape from above).

## 3. Landscape -> write to the RVT
1. Select the **Landscape** actor. Details -> search "Virtual Texture".
2. **Render to Virtual Textures** (array): add one element = `RVT_Landscape`.
3. Leave **Num Virtual Texture LODs / LOD Bias** default for now.

## 4. Material -> WRITE into the RVT
In `M_MasterLandscape`:
1. Add a **Runtime Virtual Texture Output** node.
2. Wire your final layer outputs into it:
   - LayerBlend **BaseColor** -> RVT Output **Base Color**
   - LayerBlend **Normal** -> **Normal**
   - LayerBlend **Roughness** -> **Roughness**
   - **Constant 0.5** (no specular layer) -> **Specular**
   - (World Height: leave empty.)
3. These inputs are evaluated **only** in the VT pass - that's where the heavy bombing now lives.

## 5. Material -> READ from the RVT (cheap main pass)
1. Add a **Runtime Virtual Texture Sample** node:
   - **Virtual Texture** = `RVT_Landscape`
   - **Content** = `Base Color, Normal, Roughness, Specular` (match the asset exactly).
2. **Untick "Use Material Attributes"** on the material root.
3. Wire RVT Sample outputs -> the main pins:
   - **Base Color** -> Base Color
   - **Specular** -> Specular
   - **Roughness** -> Roughness
   - **Normal** -> Normal
4. The old `MakeMaterialAttributes -> root` is now unused - delete it (root takes the RVT-sample pins instead).

Net graph: heavy layers (bombing + biomes + riverbed + Reveal) feed **only** the RVT Output (VT pass); the main pins come from the RVT Sample (main pass).

## 6. Apply, verify, tune
1. **Apply/Save.** Terrain should look the same after a brief VT page render.
2. **Blurry up close?** Raise the RVT **Size** (resolution) or lower its **LOD Bias**.
3. **On-screen "VT pool" warning?** Increase the pool: console `r.VT.PoolSizeScale 2` (or Project Settings -> Rendering -> Virtual Textures).
4. **Paint test:** first paint of a genuinely new layer combo still compiles the VT-writer (one pass, quick); the **main view and re-paints no longer recompile**.

## Notes and gotchas
- **Substrate:** your function outputs show `SubstrateUnlitBSDF`, so Substrate is on. RVT Output still works - keep the individual BaseColor/Normal/Roughness/Specular wiring above.
- **Reveal / riverbed / bombing / macro variation** all stay - they just bake into the RVT via the RVT Output path. Wire `Reveal` exactly as planned; it bakes into the VT.
- **Volume must cover the whole terrain** or areas outside it render untextured (default grey).
- **Normals:** the `Base Color, Normal, Roughness, Specular` content includes Normal - good.
- **It's runtime, not a static bake** - painting/edits update the VT pages automatically.
- **Reverting:** delete the two RVT nodes, re-tick Use Material Attributes, rewire MakeMaterialAttributes -> root, remove the RVT from the Landscape's Render-to list. Non-destructive.

---

## Quick reference
| Step | Where | Key setting |
|------|-------|-------------|
| RVT asset | Content Browser | Content = Base Color, Normal, Roughness, Specular |
| RVT volume | Level | Bounds Align Actor = Landscape -> Set Bounds |
| Landscape | Details -> Virtual Texture | Render to Virtual Textures += RVT_Landscape |
| Write | M_MasterLandscape | Runtime Virtual Texture Output <- layer BaseColor/Normal/Roughness/Specular |
| Read | M_MasterLandscape | Runtime Virtual Texture Sample -> main BaseColor/Normal/Roughness/Specular |
