# Episode 31 — Explosion on Impact: Editor Wiring

All C++ is written and building. This covers only the asset and editor work.

## What the video does in Blueprint that we do NOT

The video builds the whole detonation on the `BP_Fireball` event graph. Ours is already in
C++ on `ACantripProjectile`, so several steps have no editor equivalent:

| Video step | Ours |
|---|---|
| `On Component Hit` event | `Movement->OnProjectileStop` bound in `BeginPlay` |
| "Simulation Generates Hit Events" checkbox | Not needed — we are query-only, no simulated physics. What matters is that the capsule **blocks** the surface. |
| Timeline with sound + light curves | C++ envelopes; optional `CurveFloat` assets override the defaults |
| `Multi Sphere Trace` → `For Loop` → `Spawn Actor Fire Decal` | `UCantripBurnSubsystem::PaintBurst` |
| Moving flammable/burnability onto the decal actor's construction script | Already consolidated one level higher, in the burn subsystem |
| `IntensityAtExplosion` variable | `IntensityAtImpact`, captured in `Detonate` before the ageing timer can touch it |
| Destroy Actor at end of timeline | `SetLifeSpan(max(Linger, Volume, Flash))` |

So the editor work is four assets and one properties pass.

---

## 1. The explosion Niagara system

### 1a. Enable the converter

1. **Edit → Plugins**, search `Cascade`.
2. Tick **Cascade To Niagara Converter**.
3. Restart the editor.

### 1b. Convert

1. Content Browser → `StarterContent/Particles`.
   *(No StarterContent folder? Make a throwaway project with starter content and copy the
   folder in.)*
2. Right-click **P_Explosion** → **Convert To Niagara System**.
3. Move the result next to `NS_TorchFire` and rename it **NS_Explosion**.

### 1c. Clear the conversion errors

Open it. There is a stack of yellow "acknowledge and clear this issue" banners — on the
system **and separately on every emitter**. Clear all of them. Tedious, no shortcut.

### 1d. Strip and gate

1. Right-click the **Sparks** emitter → **Deactivate**. Same for **Smoke**.
   *(Keep Smoke if you want smoke — for Pyretics I'd leave it off; it reads as ordnance
   rather than as faerie fire.)*
2. Select the **System** node → **System State** → **Loop Behavior** = **Once**.
3. On **each** remaining emitter → **Emitter State** → **Life Cycle Mode** = **System**.
   Without this the emitters keep their own loop and ignore the system's "once".

### 1e. Shape the fireball emitter

Select the **Fireball** emitter:

| Module | Setting | Value |
|---|---|---|
| Spawn Burst Instantaneous | Spawn Count | `12` |
| Initialize Particle | Lifetime | `0.3` – `0.4` |
| Initialize Particle | Sprite Size Min / Max | `80` / `150` |
| Shape Location | Sphere Radius | `10` |
| Scale Sprite Size | curve key @ 0s | `0.125` |
| Scale Sprite Size | curve key @ end | `3.5` |

The sphere radius is the important one — at its converted default the puffs start scattered
and it reads as several small fires rather than one burst.

### 1f. Expose intensity

1. **Parameters** panel → **User Exposed** → **+** → **Float** → name it exactly
   **`Intensity`**.
   Must match `Explosion Intensity Parameter` on the projectile (case-sensitive).
2. **Fireball → Spawn Burst Instantaneous → Spawn Count**: click the dropdown → **Divide
   Int**. Drag `User.Intensity` into A, set B = `5`.
   At intensity 100 that is 20 puffs; at 20 it is 4.
3. **Fireball → Scale Sprite Size**: dropdown on the scale factor → **Multiply Float**.
   Leave the curve in A; in B put `User.Intensity` divided by `100`.
   A hundred-intensity blast is then the size you just tuned, and everything else scales
   from it.

Shock Wave and Fire Light need no intensity wiring — they read fine at a fixed size.

### 1g. Test

Temporarily set **Loop Behavior** back to **Infinite**, drag the system into the level,
`stat fps`, watch for a drop, then **set it back to Once** before saving. (Expect ~2 fps.)

---

## 2. The blast sound

1. `Sound/GameplayAbilities/Fire` → duplicate your fireball cue → **SC_Fireball_Explosion**.
2. Duplicate the fire attenuation asset → **ATT_Explosion**. Open it and set **Falloff
   Distance** = `5000` (up from 2000).
3. In `SC_Fireball_Explosion`, set **Attenuation Settings** = `ATT_Explosion`, and the
   **Volume Multiplier** to `0.9`.

Do **not** bake a fade into the cue — the C++ drives the envelope, peaking at 5× and
collapsing cubically over a second. The cue should be the raw sound.

The video's late bug — the explosion audio playing at constant volume regardless of
distance — cannot happen here: `SpawnSoundAttached` parents it to the capsule.

---

## 3. Decal opacity

Open the **flammable** child MI of `M_MS_Decal_Material_VT` (the one from episode 29) and
set **Opacity Intensity** to `0.1`, matching the non-flammable one. At 0.25 a single hit
already looks half-burnt and repeat hits have nowhere to go.

---

## 4. Properties on `BP_Engulf_Projectile`

**Cantrip | Projectile | Impact**

| Property | Value |
|---|---|
| Explode On Impact | ✅ |
| Explosion Effect | `NS_Explosion` |
| Explosion Sound | `SC_Fireball_Explosion` |
| Base Blast Radius | `150` |
| Blast Radius Per Intensity | `2.5` |
| Burst Rays Per Intensity | `0.2` |
| Burst Mark Strength | `0.7` |
| Explosion Volume Scale | `5.0` |
| Explosion Volume Seconds | `1.0` |
| Explosion Volume Curve | *(empty)* |
| Blast Effect | *(empty — damage comes later)* |
| Blast Magnitude Per Intensity | `1.0` |
| Explosion Intensity Parameter | `Intensity` |
| Flight Spawn Parameter | *(see §5)* |

**Cantrip | Projectile | Flash**

| Property | Value |
|---|---|
| Flash Intensity Per Intensity | `50` |
| Flash Falloff Seconds | `0.4` |
| Flash Curve | *(empty)* |

### Note on the blast radius

The video uses `intensity ÷ 2 + 50` — 100cm at intensity 100. My defaults give 400cm at the
same intensity, because our Engulf runs at intensity 20–100 rather than a flat 100 and a
60cm scorch at a weak casting is invisible. To match the video exactly: **Base Blast
Radius = 50**, **Blast Radius Per Intensity = 0.5**.

Ray count already matches: `0.2 × intensity` is the video's `intensity ÷ 5`.

---

## 5. Stopping the flight trail

On impact the C++ pushes `Intensity = 0` into the flight system and then tries to stop it
spawning. Two options:

**Simple (do this first):** clear **Flight Spawn Parameter** to `None`. The C++ deactivates
the component instead — particles already in the air finish their lives, no new ones appear.

**Better, once the system is yours:** in `NS_Engulf_Projectile` add a **User Exposed bool**
named exactly **`SpawnParticles`**, default `true`. On each emitter's spawn module, wrap the
rate in a **Select (bool)** returning `0` when false. Then set **Flight Spawn Parameter** =
`SpawnParticles`. This lets the trail age out under its own curves rather than being cut.

Optional refinement from the video: in the flight system's **Particle Update**, drive
particle **Age** faster when `User.Intensity` is 0, so the trail collapses in a few frames
instead of drifting.

---

## 6. Collision sanity check

The detonation fires from `OnProjectileStop`, which only fires on a **blocking** hit.

- The capsule blocks all channels except `Pawn`, set in C++.
- Landscape, static meshes and BSP must therefore **Block** `WorldDynamic`.
- Anything the fireball should pass through (foliage, chimerical scenery) should be set to
  **Overlap** or **Ignore** for `WorldDynamic`.

If `DETONATE` never appears in the log, this is the first thing to check — the fireball is
flying through the world and timing out instead of hitting it.

---

## Log lines to watch

```
LAUNCH BP_Engulf_Projectile_C_0: ... velocity X=... 
DETONATE BP_Engulf_Projectile_C_0: intensity 60.0, radius 300, 12 scorch rays at X=...
BURST: 12 rays over 300cm at intensity 0.7 -> 9 marks (live 34).
```

- No `DETONATE` → no blocking hit (§6).
- `DETONATE` but no `BURST` → the burn subsystem was never configured; the character's
  `CantripBurnComponent` has to run once (any flamethrower cast) to hand over its decal
  class and surface table.
- `BURST ... -> 0 marks` → the rays found no geometry inside the radius, or every surface
  they hit rolled against its `Mark Chance`.
