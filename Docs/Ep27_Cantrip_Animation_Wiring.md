# Cantrip animation wiring — Blueprint side

Everything the video does with booleans set from the character Blueprint is now derived
in C++ instead. You do not set any of it. Your job is:

1. Reparent the AnimBP so it inherits the state.
2. Build the state machine that reads it.
3. Place two named events so the animation can talk back.
4. Author the Niagara system and the sound.
5. Fill in an ability Blueprint and a table row.

Read Part 1 carefully — the reparent is the one step that can go wrong in a way that
takes a while to untangle.

---

## Part 0 — Get the animations

You already have `Standing2HMagicAttack04_UE` retargeted and sitting in
`Content/Characters/Mannequins/Anims/Unarmed/Mixamo/` — but as one full-length clip. The
state machine needs it as three, because the middle has to be able to repeat
indefinitely while the two ends play exactly once.

Easiest reliable route is to go back to Mixamo and download the same animation three
times, trimming the frame range each time (no skin, uniform keyframe reduction):

| Download | Frames | Rename to |
|---|---|---|
| 1 | 0 – 30 | `A_Cantrip_Channel_Begin` |
| 2 | 33 – 77 | `A_Cantrip_Channel_Loop` |
| 3 | 78 – 94 | `A_Cantrip_Channel_End` |

Through the Mixamo converter and the `RTG_UE4_Manny_to_UE5_Manny` rig as in episode 25,
then put the retargeted results in `Content/Characters/Mannequins/Anims/Cantrips/`.

---

## Part 1 — Reparent ABP_Unarmed

`ABP_Unarmed` is the AnimBP the character actually uses
(`Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`).

1. Open it. **File → Reparent Blueprint → `ChangelingAnimInstance`.**
2. Compile.

### If it will not compile after reparenting

You will get an error naming a variable that now exists in both the parent and the
child. There is one you should expect:

**`CantripPose`** — you added this to the AnimBP by hand during the torch work. Delete
the child's copy. Every node that reads it will re-link itself to the parent's version
automatically, because the name and type match.

Then find and **delete the `Set Cantrip Pose` node in the character Blueprint** that was
feeding it. The parent now reads the pose from the character every frame, so setting it
by hand is not just redundant — the parent's copy is read-only and the old set would be
writing to a variable nothing looks at any more.

I deliberately did **not** put `GroundSpeed`, `bIsFalling` or any other locomotion value
on the parent class, precisely because `ABP_Unarmed` already computes its own and a
second copy would both block this reparent and give you two answers to the same
question. Your existing locomotion graph is untouched.

### What you get

New inherited variables, all read-only, under category **Cantrip**:

| Variable | Meaning |
|---|---|
| `bCantripCasting` | A cantrip owns the body — wind-up, hold, resolve and recovery, the whole span. |
| `bCantripChannelling` | The working is open and burning. Drives the loop. |
| `bCantripRecovering` | Casting but no longer channelling — the recovery window. |
| `bAllowFootIK` | False while casting or falling. |
| `CantripPose` | Which arms are up. |
| `bCantripHandBlocked` | The raised hand is against something. |
| `CantripElement` | Air / Fire / Water / Earth / Quintessence. |
| `CantripAnims` | The running cantrip's Begin, Loop and End clips. |
| `ChannelIntensity` | The `EffectIntensity` curve, 0–1. |

---

## Part 2 — The spellcasting state machine

### 2a. In the AnimGraph

1. Right-click → **Add New State Machine**. Rename it `Cantrip Casting`.
2. Right-click → **New Save Cached Pose**. Rename it `CantripCastingPose`. Plug the
   state machine into it.

### 2b. In the main state machine

3. Open the top-level state machine (the one holding Locomotion).
4. Add a state, name it `Cantrip Casting`. Inside it, drop a **Use cached pose
   `CantripCastingPose`** and connect it to the output.
5. Draw a transition **Locomotion → Cantrip Casting**. Rule: `bCantripCasting`.
   Set its **Duration to 0.2**.
6. Draw a transition **Cantrip Casting → Locomotion**. Rule: `NOT bCantripCasting`.
   Set its **Duration to 0.4** — the slower blend out is what stops the character
   snapping upright the instant the cantrip ends.

### 2c. Inside the Cantrip Casting state machine

Five states:

```
Entry → [Cantrip Idle] → [Channel Begin] → [Channel Loop A] ⇄ [Channel Loop B]
                                                   ↓                ↓
                                              [Channel End] → back to Cantrip Idle
```

- **Cantrip Idle** — the retargeted standing idle, looping. Rarely seen; it is the
  in-between while a cast starts and finishes.
- **Channel Begin** — Sequence Player, **loop OFF**.
- **Channel Loop A** — Sequence Player, **loop OFF** (the ping-pong replaces looping).
- **Channel Loop B** — same clip, **loop OFF**, **Play Rate −1**, **Start Position 1.0**.
  Playing the loop backwards on alternate passes is what hides the seam that otherwise
  makes the character twitch every time the clip restarts.
- **Channel End** — Sequence Player, **loop OFF**.

Transition rules:

| From → To | Rule |
|---|---|
| Cantrip Idle → Channel Begin | `bCantripChannelling` |
| Channel Begin → Channel Loop A | `Get Relevant Anim Time Remaining` < `0.1` |
| Channel Loop A → Channel Loop B | `Get Relevant Anim Time Remaining` < `0.1` |
| Channel Loop B → Channel Loop A | `Get Relevant Anim Time Remaining` < `0.1` |
| Channel Loop A → Channel End | `NOT bCantripChannelling` |
| Channel Loop B → Channel End | `NOT bCantripChannelling` |
| Channel End → Cantrip Idle | `Get Relevant Anim Time Remaining` < `0.1` |

Leave every transition at the default 0.2s duration except where noted above.

### 2d. Driving the clips from the variable

Rather than hard-coding the three sequences, bind each Sequence Player to the running
cantrip's clips. Ninety cantrips cannot each have their own states in this graph.

For each of Begin / Loop A / Loop B / End:

1. Select the Sequence Player node.
2. In the Details panel, find the **Sequence** property.
3. Click the **binding dropdown** to its right (the small chain/pin icon).
4. Choose **Cantrip Anims → Begin** (or `Loop`, `Loop`, `End` respectively).

**If the binding proves awkward, just assign the three assets directly for now.** Only
one cantrip has animations at this point, so hard-coding costs nothing and you can come
back to the binding when a second one needs different clips.

---

## Part 3 — The two named events

This is the part that matters most, and it is only two text fields. Type the names
**exactly** — they are matched by string.

### 3a. Effect start

Select the transition **Channel Begin → Channel Loop A**.
In Details → **Events** → **Start Transition Event** → set the name to:

```
CantripEffectStart
```

That is what makes the flame appear. It fires when the gesture actually completes,
rather than after a fixed delay that stops being right the moment you retime the clip.

### 3b. Recovered

Go back to the **main** state machine. Select the transition
**Cantrip Casting → Locomotion**. In Details → **Events** → **End Transition Event**:

```
CantripRecovered
```

That is what gives the character its feet back.

**Put it on this transition, not on a notify inside the End clip.** This transition is
taken on every possible exit — released, interrupted, cancelled, killed mid-gesture — so
the movement lock cannot survive a cast that ended in an unusual way. A notify buried in
a clip only fires if that clip plays as far as the frame it sits on, which is exactly
the bug that leaves you unable to walk.

There is a 1.5 second safety timeout in C++ behind this, so if you mistype the name the
symptom is a short pause before you can move again, not a permanently frozen character.
If you see that pause, check the spelling here first.

You do **not** need to create event nodes in the Event Graph for either of these. The
functions already exist on the parent class.

---

## Part 4 — The anim curve

Add a curve named exactly `EffectIntensity` to all three clips. Open each animation,
then in the curve panel: **+ Curve → Create Curve → `EffectIntensity`**.

| Clip | Shape |
|---|---|
| Begin | 0 until roughly 55% through, then rising to **1.0** at the end |
| Loop | flat **1.0** throughout (one key is enough) |
| End | **1.0** at time 0, falling to **0** by roughly 60% through |

Right-click each key → **Auto** so the ramps curve rather than kink.

This one curve drives the Niagara spawn rate, the sound volume and the sound pitch, all
read from the same place, so they cannot drift apart.

---

## Part 5 — The Niagara system

Build the flamethrower exactly as the video does — convert `P_Fire` from the starter
content with the Cascade-to-Niagara converter, disable smoke/embers/sparks, keep
distortion, clear the conversion issues on every module, and set every **CPU Collision
Trace Channel to `Pawn`**. All of that is asset work and the video covers it well.

Two deltas where our setup differs:

### 5a. Two user parameters, both floats

Under **User Exposed Parameters**, add:

- `Intensity` — float
- `FadeAlpha` — float

The C++ writes both. `Intensity` follows the anim curve every frame; `FadeAlpha` is 1
while the working is open and ramps smoothly to 0 over half a second when it closes.

### 5b. Spawn rate

For each emitter, set Spawn Rate to `BaseRate × Intensity × FadeAlpha` (chain two
**Multiply Float by Float** nodes). Use base rates of roughly **20 / 100 / 125** for the
three emitters, matching the video's ratios.

**Skip the video's `SpawnParticles` bool entirely.** `FadeAlpha` already does that job
and does it better: the boolean cuts spawning dead, while the ramp thins the flame out
over half a second. Particles already in the air finish their own lifetimes either way,
so the flame dies down instead of being switched off.

Leave **Auto Destroy** alone — the C++ deactivates the component and lets it clean
itself up once the last particle expires.

---

## Part 6 — The sound

1. Get the ZapSplat `medium soft fire whooshes`, cut two differently-lengthed loops from
   it in Audacity, import both.
2. Select both → right-click → **Create Single Cue**. Name it `SC_Cantrip_Channel`.
3. In the cue, set both wave players to **Loop**.
4. On the Output node, set **Attenuation** to the footstep hard-surface attenuation
   asset.

That is all. **Do not build the volume and pitch tick logic from the video** — the
ability already sets volume from the curve and pitch from the configured range every
frame in C++.

---

## Part 7 — The ability Blueprint

Create a Blueprint deriving from **`GA_ChannelledCantrip`**, named `GA_Pyretics`. Set:

| Property | Value |
|---|---|
| Cantrip Table | `DT_Cantrips` |
| Cantrip Row | `Pyretics_1` |
| Realms Used | `Actor` |
| Sustained Effect | your flamethrower Niagara system |
| Attach Socket | `hand_rSocket` |
| Attach Offset | `0, 0, -20` |
| Sustained Pose | `Both Hands Raised` |
| Channel Sound | `SC_Cantrip_Channel` |
| Channel Pitch Range | `0.9, 1.1` |
| Lock Movement While Channelling | ✔ |
| Forward Clearance | `120` |
| Clearance Probe Height | `60` |

Then open `DT_Cantrips`, find row `Pyretics_1`, and fill in **Anims → Begin / Loop /
End** with the three retargeted clips.

---

## Part 8 — Input

1. Add `GA_Pyretics` to **Default Abilities** on `BP_ThirdPersonCharacter`.
2. Make an Input Action `IA_Cantrip_Fire` (Boolean) and add it to the mapping context.
3. In the character's Event Graph:
   - **Started** → `Try Activate Ability by Class` (Class = `GA_Pyretics`)
   - **Completed** → `Stop Channelling` (Target = self)
   - **Canceled** → `Stop Channelling` (Target = self)

Three nodes total. `Stop Channelling` handles both meanings of letting go on its own.

### How it plays

Hold the key. The wind-up runs; at the end of the cast ladder the working opens by
itself and the flame appears — you do not release to fire it. Keep holding and it keeps
burning. Let go and it goes out.

Letting go **during** the wind-up, before the flame ever appears, abandons the cast
entirely: no roll, no Glamour, nothing happens. A channel has no weaker version to fire,
so an abandoned wind-up reads as changing your mind rather than as a fizzle you paid for.

---

## Part 9 — Foot IK

Find the foot-IK Control Rig node in `ABP_Unarmed`. Set its **Alpha Input Type** to
**Bool** and plug in `bAllowFootIK`.

This is the video's floating-feet fix. The casting clips have no ground contact to solve
against, so IK against them lifts the feet clear of the floor. The variable is derived,
never set, so it cannot get stuck off after a cast that ended badly.

---

## What C++ is doing, for reference

- `ChangelingAnimInstance` derives every Cantrip variable from the character and its
  ability system once a frame. Nothing sets them, so nothing can leave one stuck.
- `AnimNotify_CantripEffectStart` / `AnimNotify_CantripRecovered` are the two entry
  points your transition events call.
- `GA_ChannelledCantrip` roots the caster, refuses to start facing a wall, opens the
  working when the animation says the gesture landed, samples the curve every 33ms into
  the Niagara and audio, and hands the movement lock to the character on the way out so
  it survives the ability's own death.
- `ACHANGELINGCharacter` owns the movement lock and releases it on the recovery event or
  after 1.5 seconds, whichever comes first.
