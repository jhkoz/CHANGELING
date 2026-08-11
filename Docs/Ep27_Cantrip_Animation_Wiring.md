# Cantrip animation wiring — complete Blueprint instructions

Everything the tutorial does with booleans set from the character Blueprint is now
derived in C++. You never set any of that state. What you do build:

| Part | What | Rough time |
|---|---|---|
| 0 | Split the Mixamo animation into three clips | 20 min |
| 1 | Reparent `ABP_Unarmed` | 5 min |
| 2 | The spellcasting state machine | 40 min |
| 3 | The two transition events | 2 min |
| 4 | The `EffectIntensity` curve | 15 min |
| 5 | The flamethrower Niagara system | 45 min |
| 6 | The looping sound cue | 20 min |
| 7 | The ability Blueprint + table row | 10 min |
| 8 | Input | 5 min |
| 9 | Foot IK | 2 min |
| 10 | Test and troubleshoot | — |

Part 3 is two text fields and is the part where a typo actually costs you something.
Part 1 has one gotcha that will stop the compile until you handle it.

---

# Part 0 — Split the animation into three clips

You already have `Standing2HMagicAttack04_UE` retargeted in
`Content/Characters/Mannequins/Anims/Unarmed/Mixamo/`. It is one full-length clip. The
state machine needs three, because the middle has to repeat indefinitely while the two
ends play exactly once.

## 0.1 Download three trimmed copies from Mixamo

1. Go to mixamo.com, sign in, and make sure `SK_Mannequin` from the Mixamo Converter is
   still your uploaded character.
2. Search for **Standing 2H Magic Attack 04** and select it.
3. Under the preview there is a **trim** control with two handles and a frame counter.

Download it three times, adjusting the trim each time. For every download use:
- Format: **FBX Binary (.fbx)**
- Skin: **Without Skin**
- Frames per Second: **30**
- Keyframe Reduction: **Uniform**

| Download | Trim from | Trim to | Save as |
|---|---|---|---|
| 1 | 0 | 30 | `Cantrip_Channel_Begin.fbx` |
| 2 | 33 | 77 | `Cantrip_Channel_Loop.fbx` |
| 3 | 78 | 94 | `Cantrip_Channel_End.fbx` |

On download 2, scrub the preview before downloading — it should read as a continuous
churn with no obvious start or stop. You will see it jump when it cycles; ignore that,
Part 2 fixes it in the graph.

## 0.2 Through the Mixamo Converter

4. Drop all three FBX files into the converter's `incoming fbx` folder.
5. Run the converter, click through to step 2, hit **Convert**.
6. Collect the results from `outgoing fbx`.

## 0.3 Import to the UE4 skeleton

7. In the Content Browser, go to `Content/Characters/Mannequins/` — wherever your
   UE4 mannequin lives (the folder you used in episode 25).
8. Drag all three converted FBX files in.
9. In the import dialog:
   - **Skeleton**: the UE4 mannequin skeleton
   - **Import Translation**: `0, 0, -3`
   - Animation only (mesh import off)
10. Click **Import All**.
11. Double-click one to confirm it plays correctly.

## 0.4 Retarget to Manny

12. Navigate to the `RTG_UE4_Manny_to_UE5_Manny` retargeter asset (in `Mannequins/Rigs/`
    or wherever episode 25 left it) and open it.
13. Select all three imported animations (Ctrl-click).
14. Click **Export Selected Animations**.
15. Send the output to a new folder: `Content/Characters/Mannequins/Anims/Cantrips/`.
16. Rename the three results to:
    - `A_Cantrip_Channel_Begin`
    - `A_Cantrip_Channel_Loop`
    - `A_Cantrip_Channel_End`
17. Open each one and confirm it is mapped to Manny and plays cleanly.

---

# Part 1 — Reparent ABP_Unarmed

`ABP_Unarmed` at `Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed` is the
AnimBP your character actually uses.

1. Open it.
2. **File → Reparent Blueprint**.
3. In the class picker, type `ChangelingAnimInstance` and select it.
4. Click **Compile**.

## 1.1 The compile error you should expect

You will get an error naming a variable that now exists on both the parent and the
child. There is one you should specifically expect:

**`CantripPose`** — you added this to the AnimBP by hand during the torch work.

To fix it:

5. In the **My Blueprint** panel on the left, find your local `CantripPose` variable.
6. Right-click it → **Delete**.
7. Compile again.

Every node that *reads* `CantripPose` will re-link itself to the parent's version
automatically, because the name and type match. You should not have to rewire anything.

8. Now open `BP_ThirdPersonCharacter` and find the **`Set Cantrip Pose`** node you were
   using to push the pose into the AnimBP. **Delete it.** The parent class reads the
   pose off the character every frame now, so that set is not only redundant — the
   parent's copy is read-only and the old node would be writing to a variable nothing
   reads any more.
9. Compile the character Blueprint.

If any *other* variable name collides, the same fix applies: delete the child's copy.

## 1.2 What you should not expect

I deliberately did **not** put `GroundSpeed`, `bIsFalling`, `MovementDirection` or any
other locomotion value on the parent class. `ABP_Unarmed` already computes its own,
duplicate names hard-block a reparent, and two sources for the same fact is how they
drift. Your existing locomotion graph is completely untouched by this.

## 1.3 What you gain

After compiling, these appear in **My Blueprint** under the inherited category
**Cantrip**. All are read-only.

| Variable | Type | Meaning |
|---|---|---|
| `bCantripCasting` | bool | A cantrip owns the body — wind-up, hold, resolve and recovery, the whole span |
| `bCantripChannelling` | bool | The working is open and burning. Drives the loop |
| `bCantripRecovering` | bool | Channelled, and now recovering. False during the wind-up |
| `bAllowFootIK` | bool | False while casting or falling |
| `CantripPose` | enum | Which arms are up |
| `bCantripHandBlocked` | bool | The raised hand is against something |
| `CantripElement` | enum | Air / Fire / Water / Earth / Quintessence |
| `CantripAnims` | struct | The running cantrip's Begin, Loop and End clips |
| `ChannelIntensity` | float | The `EffectIntensity` curve, 0–1 |
| `ChangelingCharacter` | object | Typed reference to the character |

---

# Part 2 — The spellcasting state machine

## 2.1 Create the state machine and its cached pose

1. Open `ABP_Unarmed` and go to the **AnimGraph**.
2. Right-click on empty canvas → **State Machines → Add New State Machine**.
3. In My Blueprint (or by right-clicking the node), rename it to `Cantrip Casting`.
4. Right-click on empty canvas → search **New Save Cached Pose**, add it.
5. Rename the cached pose node to `CantripCastingPose`.
6. Drag from the `Cantrip Casting` state machine's output pin into the
   `CantripCastingPose` input pin.

Nothing is connected to the final output pose yet — that is correct. The next step
connects it through the main state machine.

## 2.2 Hook it into the main state machine

7. Open the top-level state machine — the one that owns Locomotion.
8. Right-click on empty canvas → **Add State**. Name it `Cantrip Casting`.
9. Double-click into that new state.
10. Right-click → search **Use cached pose 'CantripCastingPose'** and add it.
11. Connect it to the state's **Output Animation Pose**.
12. Go back up to the main state machine (breadcrumb at the top).

Now the two transitions:

13. Drag from the edge of the **Locomotion** state to the **Cantrip Casting** state.
    An arrow with a circle appears.
14. Double-click that arrow's circle to open the transition rule.
15. Drag in `bCantripCasting` from My Blueprint and connect it to **Can Enter
    Transition**. Compile.
16. Go back, select the arrow itself (single click the circle), and in the **Details**
    panel set **Duration** to `0.2`.

17. Drag from **Cantrip Casting** back to **Locomotion**.
18. Double-click that arrow. Drag in `bCantripCasting`, then drag off its output pin and
    search **NOT Boolean**. Connect `NOT` → **Can Enter Transition**. Compile.
19. Select that arrow and set **Duration** to `0.4`.

The asymmetry is deliberate: 0.2 in, 0.4 out. The slower blend out is what stops the
character snapping upright the instant the cantrip ends.

## 2.3 Build the five states

20. Double-click into the `Cantrip Casting` state machine.
21. Right-click → **Add State** five times, naming them:
    - `Cantrip Idle`
    - `Channel Begin`
    - `Channel Loop A`
    - `Channel Loop B`
    - `Channel End`
22. Drag from **Entry** to `Cantrip Idle`.

Now fill each state in. Double-click into each, drag the animation from the Content
Browser onto the canvas, connect it to **Output Animation Pose**, then select the
sequence player node and set its Details:

| State | Animation | Loop Animation | Play Rate | Start Position |
|---|---|---|---|---|
| `Cantrip Idle` | `StandingIdle03_UE` (or your preferred idle) | **✔ on** | 1.0 | 0.0 |
| `Channel Begin` | `A_Cantrip_Channel_Begin` | **✘ off** | 1.0 | 0.0 |
| `Channel Loop A` | `A_Cantrip_Channel_Loop` | **✘ off** | 1.0 | 0.0 |
| `Channel Loop B` | `A_Cantrip_Channel_Loop` | **✘ off** | **−1.0** | **1.0** |
| `Channel End` | `A_Cantrip_Channel_End` | **✘ off** | 1.0 | 0.0 |

`Channel Loop B` is the same clip played backwards from its end. Alternating forward and
backward passes is what hides the seam that otherwise makes the character twitch every
time the loop restarts, and it doubles the apparent length of the animation for free.

Loop is **off** on both loop states — the ping-pong between them replaces looping.

## 2.4 Draw the transitions

Drag between states to create each arrow, then double-click the circle to author the
rule.

| # | From → To | Rule to build |
|---|---|---|
| 1 | Cantrip Idle → Channel Begin | `bCantripChannelling` → Can Enter Transition |
| 2 | Channel Begin → Channel Loop A | time-remaining rule (below) |
| 3 | Channel Loop A → Channel Loop B | time-remaining rule |
| 4 | Channel Loop B → Channel Loop A | time-remaining rule |
| 5 | Channel Loop A → Channel End | `bCantripChannelling` → **NOT Boolean** → Can Enter |
| 6 | Channel Loop B → Channel End | `bCantripChannelling` → **NOT Boolean** → Can Enter |
| 7 | Channel End → Cantrip Idle | time-remaining rule |

**The time-remaining rule**, built identically each time:

- Right-click in the rule graph → search **Get Relevant Anim Time Remaining**. Add it.
- Drag off its output → search **less than (float)**, add it (`<` node).
- Set the second input of the `<` node to `0.1`.
- Connect the `<` output to **Can Enter Transition**.
- Compile.

A tenth of a second only fails to catch the end of the animation if your frame rate is
below 10, at which point you have larger problems.

Leave every transition at its default **0.2** duration.

## 2.5 Drive the clips from the variable

Rather than hard-coding three sequences, bind each Sequence Player to the running
cantrip's clips. Ninety cantrips cannot each have their own states in this graph.

For each of the four sequence player nodes:

23. Select the Sequence Player node.
24. In the **Details** panel, find the **Sequence** property.
25. Click the small **binding dropdown** to its right (a chain or pin icon).
26. Choose **Cantrip Anims → Begin** / **Loop** / **Loop** / **End** to match the state.

**If the binding is awkward or unavailable, just leave the three assets assigned
directly.** Only one cantrip has animations right now, so hard-coding costs nothing and
you can come back to the binding when a second cantrip needs different clips. Nothing
else in this document depends on the binding working.

---

# Part 3 — The two transition events

Two text fields. Type them **exactly** — they are matched by string, character for
character, and they are case-sensitive.

## 3.1 Effect start

1. Inside the `Cantrip Casting` state machine, click the circle on the transition
   **Channel Begin → Channel Loop A** (transition #2 above) to select it.
2. In the **Details** panel, find the **Events** section.
3. In **Start Transition Event**, set the **Custom Blueprint Event** name to:

```
CantripEffectStart
```

That is what makes the flame appear. It fires when the gesture actually finishes, rather
than after a fixed delay that stops being correct the moment you retime the clip.

## 3.2 Recovered

4. Navigate back up to the **main** state machine (the one with Locomotion).
5. Click the circle on the transition **Cantrip Casting → Locomotion**.
6. In **Details → Events → End Transition Event**, set the name to:

```
CantripRecovered
```

That is what gives the character its feet back.

**Put it on this transition, not on a notify inside the End clip.** This transition is
taken on every possible exit — released, interrupted, cancelled, killed mid-gesture — so
the movement lock cannot survive a cast that ended in an unusual way. A notify buried
inside a clip only fires if that clip plays as far as the frame it sits on, which is
exactly the failure that leaves you unable to walk.

There is a 1.5-second safety timeout behind this in C++, so a typo here costs you a
short pause before you can move, not a permanently frozen character. **If you see that
pause in testing, check this spelling first.**

7. Compile and save.

You do **not** need to create event nodes in the Event Graph for either of these. The
functions already exist on the parent class and are called automatically by name.

---

# Part 4 — The EffectIntensity curve

This one curve drives the Niagara spawn rate, the sound volume and the sound pitch, all
read from the same place — so they cannot drift apart.

The name must be exactly `EffectIntensity`.

## 4.1 Begin clip

1. Open `A_Cantrip_Channel_Begin`.
2. In the curves panel (bottom left, under the notifies track), click **+ Curve →
   Add Curve** and name it `EffectIntensity`.
3. Double-click the curve name to open its graph editor.
4. Right-click on the curve line at roughly **55%** along → **Add Key**. Select that key
   and set its **Value** to `0`.
5. Right-click at the very **end** of the timeline → **Add Key**. Set its **Time** to
   `1.0` (or the clip's end) and **Value** to `1.0`.
6. If you cannot see the top of the graph, scroll out with the mouse wheel and pan with
   the right mouse button.
7. Right-click each key → **Auto** so the ramp curves instead of kinking.
8. Save.

## 4.2 Loop clip

9. Open `A_Cantrip_Channel_Loop`.
10. **+ Curve → Add Curve**, and pick the existing `EffectIntensity` from the list.
11. Right-click anywhere on the line → **Add Key**. Set its **Value** to `1.0`.

A single key sets the whole line to that value. Confirm by zooming out that the line is
flat at 1.0 across the entire clip.

12. Save.

## 4.3 End clip

13. Open `A_Cantrip_Channel_End`.
14. **+ Curve → Add Curve → `EffectIntensity`**.
15. Add a key at **time 0**, value `1.0`.
16. Add a key at roughly **60%** along, value `0`.
17. Right-click both keys → **Auto**.
18. Save.

---

# Part 5 — The flamethrower Niagara system

The best fire here is the old Cascade `P_Fire` from the starter content, converted. The
Niagara fluids plugin produces either a flat 2D effect that does not project into space,
or a grainy 3D one.

## 5.1 Enable the converter plugin

1. **Edit → Plugins**.
2. Search `Cascade to Niagara`.
3. Tick **Cascade To Niagara Converter**.
4. Restart the editor when prompted.

If you have no `Content/StarterContent/` folder at all, create a scratch project with
starter content enabled and copy the `Particles` and `Textures` folders across first.

## 5.2 Convert

5. Navigate to `Content/StarterContent/Particles/`.
6. Right-click **P_Fire** → **Convert To Niagara System**. Wait a few seconds.
7. Create the folder `Content/_Custom/Niagara/Cantrips/Pyretics/`.
8. Move the converted asset there and rename it:

```
NS_Cantrip_Flamethrower
```

## 5.3 Clear the conversion warnings

9. Open it. There will be a stack of yellow warnings on the system and on every emitter.
10. Click **Acknowledge and Clear Issue** on each one. Expand every module in every
    emitter and clear those too. This is tedious and takes a few minutes; the system
    will not behave until it is done.

## 5.4 Disable the emitters you do not want

11. Deactivate **Smoke**, **Embers** and **Sparks** (right-click the emitter header →
    Disable, or untick its enabled checkbox).
12. Keep **Flames**, **Flames001** and **Distortion**.

Distortion is the heat-shimmer above the flame — the wobble you see over a hot road. It
is worth keeping.

## 5.5 Add the user parameters

13. In the left panel, find **User Exposed Parameters**. Click **+**.
14. **Make New → Common → float**. Name it exactly:

```
Intensity
```

15. Click **+** again, **Make New → Common → float**. Name it exactly:

```
FadeAlpha
```

The C++ writes both every frame. `Intensity` follows the anim curve (0–1); `FadeAlpha`
is 1 while the working is open and ramps smoothly to 0 over half a second when it
closes.

**Do not build the video's `SpawnParticles` bool.** `FadeAlpha` does that job and does
it better — the boolean cuts spawning dead, while the ramp thins the flame out. Either
way the particles already in the air finish their own lifetimes, so the flame dies down
rather than being switched off.

## 5.6 Emitter 1 — Flames

Select the **Flames** emitter and set:

**Emitter Update → Spawn Rate**
- Click the dropdown next to Spawn Rate → **Multiply Float by Float**.
- Set the first input to `20`.
- On the second input, click its dropdown → pick the user parameter **Intensity**.
- Add a second **Multiply Float by Float** on the result, and set its other input to the
  user parameter **FadeAlpha**.

The result is `20 × Intensity × FadeAlpha`. Repeat this same three-part chain for the
other two emitters with their own base numbers.

**Particle Spawn → Shape Location**
- Sphere **Radius**: `10`

**Particle Spawn → Add Velocity** (the plain one)
- **Disable** this module.

**Particle Spawn → Add Velocity** (the cone/random-range one)
- Minimum: X `350`, Y `0`, Z `50`
- Maximum: X `600`, Y `0`, Z `100`
- **Rotation Coordinate Space: Local** — this is what orients the flame to wherever the
  hand is pointing. Getting it wrong sends the fire off in a world-space direction.

**Particle Spawn → Initialize Particle**
- Untick **Dynamic Material Parameters**

**Particle Update → Scale Sprite Size**
- Change the curve's start value from `0` to `0.5` so the fire starts at half size
  rather than at nothing.
- Untick **Dynamic Material Parameters** here too if present.

**Particle Update → add a Collision module**
- Click **+** under Particle Update → search **Collision** → add it.
- Drag it to sit under **Acceleration Force**.
- **CPU Collision Trace Channel: `Pawn`** ← easy to miss and causes strange behaviour
  when wrong
- **Restitution**: `0.05`
- **Randomize Collision Normal Vector**: `0.2`
- **Friction**: `0.9`
- **Friction During Bounce**: `0.9`

**Render → Light Renderer**
- **Untick it.** Rendering particles as lights is very expensive — it costs roughly
  20 fps at high intensity, against about 2 fps without. The fire still reads as
  self-lit.

## 5.7 Emitter 2 — Flames001

**Emitter Update → Spawn Rate**: `100 × Intensity × FadeAlpha` (same chain as above)

**Particle Spawn → Initialize Particle**
- **Sprite Size Mode**: `Random Non-Uniform`
- Minimum: `40, 60`
- Maximum: `70, 90`
- **Sprite Rotation Mode**: `Unset`
- Untick **Dynamic Material Parameters**

**Particle Spawn → Shape Location**: Sphere **Radius** `10`

**Particle Spawn → Add Velocity** (plain): **disable**

**Particle Spawn → Add Velocity** (cone/random range):
- Minimum X `350`, Z `50`; Maximum X `600`, Z `100`
- **Rotation Coordinate Space: Local**

**Particle Update → Scale Sprite Size**: start value `0.5`

**Particle Update → Collision** (add it, under Acceleration Force):
- **CPU Collision Trace Channel: `Pawn`**
- **Restitution**: `0.7` — this emitter bounces a lot, where emitter 1 barely does
- **Randomize Collision Normal Vector**: `0.2`
- **Friction**: `0.9`
- **Friction During Bounce**: `0.3`
- **Advanced → Advance Aging Rate**: `5` (particles age faster once they have hit
  something, so the fire does not pile up against a wall)

**Render → Light Renderer**: **untick**

## 5.8 Emitter 3 — Distortion

**Emitter Update → Spawn Rate**: `125 × Intensity × FadeAlpha`

**Emitter Update → Spawn Burst Instantaneous**: untick (it spawns nothing anyway)

**Particle Spawn → Initialize Particle**
- **Lifetime**: `0.7` to `1.0`
- **Sprite Size Mode**: `Random Uniform`, from `10` to `15`
- **Sprite Rotation Mode**: `Unset`

**Particle Spawn → Add Velocity**
- Minimum X `350`, Z `60`; Maximum X `600`, Z `100`
- **Rotation Coordinate Space: Local**

Distortion rises slightly faster than the fire — heat shimmer sits above the flame.

**Particle Update → Scale Sprite Size**: start value `0.5`

**Particle Update → Collision** (add under Acceleration Force):
- **CPU Collision Trace Channel: `Pawn`**
- **Restitution**: `0.7`
- **Randomize Collision Normal Vector**: `0.2`
- **Friction**: `0.9`
- **Friction During Bounce**: `0.5`
- **Advanced → Advance Aging Rate**: `5`

## 5.9 Finish

16. **Compile** and **Save**.
17. Leave **Auto Destroy** alone — the C++ deactivates the component and lets it clean
    itself up once the last particle expires.

While editing, the preview will show **no particles at all**, because `Intensity` and
`FadeAlpha` both default to 0 and anything times zero is zero. That is expected. To
preview, temporarily set both user parameters to `1` in the parameters panel — just
remember to set them back to 0 before saving, or the effect will burst on spawn.

---

# Part 6 — The sound

## 6.1 Get and cut the source

1. From zapsplat.com, download **medium soft fire whooshes** as MP3.
2. Open it in Audacity. There are two distinct whooshes in the one file.
3. Trim off the silence at the head and the tail.
4. Select the first whoosh, **Edit → Copy**, then **Tracks → Add New → Stereo Track**
   and paste it there.
5. On the original track, delete the first whoosh so only the second remains.
6. **Make the two clips deliberately different lengths.** They will loop independently,
   and equal lengths mean they re-sync into an audible pulse.
7. Solo each in turn to check them.
8. **File → Export → Export Multiple**, WAV format, to your Downloads folder.
9. Name them `Flamethrower_Channel_1` and `Flamethrower_Channel_2`.

## 6.2 Import and build the cue

10. In the Content Browser create `Content/_Custom/Audio/Cantrips/Pyretics/`.
11. Drag both WAVs in.
12. Select both → right-click → **Create Single Cue**.
13. Rename the cue to `SC_Cantrip_Channel`.
14. Open it. On **each** of the two wave player nodes, tick **Looping** in Details.
15. Select the **Output** node. Set **Attenuation Settings** to your footstep
    hard-surface attenuation asset (the non-spatialised one from the footstep work,
    ~50cm radius with a 10m falloff).
16. Save.

**Do not build the volume and pitch tick logic from the video.** The ability already
sets volume from the curve and pitch from a configured range every frame in C++. There
is nothing to wire.

---

# Part 7 — The ability Blueprint and the table row

## 7.1 The ability

1. In `Content/_Custom/GAS/Abilities/` right-click → **Blueprint Class**.
2. Expand **All Classes**, search `GA_ChannelledCantrip`, select it, **Create**.
3. Name it `GA_Pyretics`.
4. Open it, click **Class Defaults**, and set:

| Property | Value |
|---|---|
| Cantrip Table | `DT_Cantrips` |
| Cantrip Row | `Pyretics_1` |
| Realms Used | one element: `Actor` |
| Glamour Cost Effect | your Glamour cost GE |
| Sustained Effect | `NS_Cantrip_Flamethrower` |
| Attach Socket | `hand_rSocket` |
| Attach Offset | `0, 0, -20` |
| Sustained Pose | `Both Hands Raised` |
| Channel Sound | `SC_Cantrip_Channel` |
| Channel Pitch Range | `0.9, 1.1` |
| Lock Movement While Channelling | ✔ |
| Forward Clearance | `120` |
| Clearance Probe Height | `60` |
| Recheck Clearance While Channelling | ✘ |
| Intensity Curve Name | `EffectIntensity` (default) |
| Intensity Parameter | `Intensity` (default) |
| Fade Parameter | `FadeAlpha` (default) |

5. Compile and save.

## 7.2 The table row

6. Open `DT_Cantrips` and find row `Pyretics_1`.
7. Set **Kind** to `Channelled (held)`.
8. Set **Element** to `Fire`.
9. Expand **Anims** and set:
   - **Begin** → `A_Cantrip_Channel_Begin`
   - **Loop** → `A_Cantrip_Channel_Loop`
   - **End** → `A_Cantrip_Channel_End`
10. Save.

---

# Part 8 — Input

1. Open `BP_ThirdPersonCharacter` → **Class Defaults** → find **Default Abilities** and
   add an entry: `GA_Pyretics`.
2. Create an Input Action `IA_Cantrip_Fire`, Value Type **Boolean**.
3. Add it to your Input Mapping Context on whatever key you want.
4. In the character's Event Graph, add the `IA_Cantrip_Fire` event node and wire:

| Pin | Node |
|---|---|
| **Started** | `Try Activate Ability by Class` — Target: self (its ASC), Ability Class: `GA_Pyretics` |
| **Completed** | `Stop Channelling` — Target: self |
| **Canceled** | `Stop Channelling` — Target: self |

Three nodes. `Stop Channelling` handles both meanings of letting go on its own — no
branch, no stored reference, no flip-flop, no retriggerable delay.

## How it plays

Hold the key. The wind-up runs, and at the end of the cast ladder the working opens by
itself and the flame appears — **you do not release to fire it**. Keep holding and it
keeps burning. Let go and it goes out.

Letting go **during** the wind-up, before the flame ever appears, abandons the cast
entirely: no roll, no Glamour, nothing happens. A channel has no weaker version to fire,
so an abandoned wind-up reads as changing your mind rather than as a fizzle you paid for.

One consequence worth knowing: because the channel always resolves at the top of the
cast ladder, the cast-time tiers do not vary a channelled cantrip's difficulty the way
they vary an ordinary one's. The sustained gesture *is* the full performance.

---

# Part 9 — Foot IK

1. In `ABP_Unarmed`'s AnimGraph, find the foot-IK **Control Rig** node
   (it references `CR_Mannequin_FootIK`).
2. Select it. In Details, set **Alpha Input Type** to **Bool**.
3. Drag in `bAllowFootIK` and connect it to the **Alpha Bool Value** pin.
4. Compile and save.

The casting clips were authored for a stationary body and have no ground contact to
solve against, so IK against them lifts the feet clear of the floor. `bAllowFootIK` is
derived in C++ and never set, so it cannot get stuck off after a cast that ended badly.

---

# Part 10 — Test

Work down this list in order. Each step isolates one part.

1. **Press and hold the key.** The wind-up animation plays. Feet stay on the ground.
2. **Keep holding.** The flame appears as the arms finish coming forward, pointing
   forward from the hands, with the sound fading in underneath it.
3. **Try to walk while holding.** You should not move at all.
4. **Release.** The recovery animation plays, the flame thins out over about half a
   second rather than vanishing, and you get movement back.
5. **Release during the wind-up**, before the flame appears. Nothing should happen at
   all — no flame, no sound, no cost.
6. **Spam the key.** No stuck poses, no permanent flame, nothing in the output log.
7. **Stand a foot from a wall and cast.** The channel should refuse rather than firing
   through the wall.

## Troubleshooting

| Symptom | Cause |
|---|---|
| Short pause before you can move after every cast | `CantripRecovered` misspelled, or on the wrong transition. The 1.5s safety timeout is doing the work instead. Check Part 3.2 |
| Never able to move after a cast | The safety timeout is not the issue — check that the character Blueprint derives from `ACHANGELINGCharacter` |
| No flame at all, ever | `Intensity`/`FadeAlpha` misspelled in Niagara, or the spawn-rate multiply chain is not wired |
| Flame appears but never at the right moment | `CantripEffectStart` misspelled. The 1-second fallback is firing it instead |
| Flame fires off sideways or in a fixed world direction | A **Rotation Coordinate Space** left on World instead of Local |
| Fire passes through the character or through walls | A **CPU Collision Trace Channel** left on World Dynamic instead of `Pawn` |
| Character twitches every couple of seconds while channelling | Loop A/B ping-pong not set up, or Loop B missing Play Rate −1 / Start Position 1.0 |
| Feet float during the cast | Part 9 not done |
| Cast does nothing and the log mentions a row | `Cantrip Row` does not match a row name in `DT_Cantrips` |

---

# Appendix — Illuminate's light

Sustained cantrips can now cast real light. It lives on `GA_SustainedCantrip`, so any
working that should glow can, but it is **off by default** (`Light Radius` of 0) because
most sustained cantrips are not torches.

There is nothing to build — no Blueprint, no light actor, no Niagara light renderer.
Open `GA_Illuminate`, go to **Class Defaults → Cantrip | Sustained | Light**, and set:

| Property | Suggested | What it does |
|---|---|---|
| **Light Radius** | `450` | Reach in cm at a bare single success. **Zero means no light at all — this is the switch.** |
| Light Radius Per Success | `150` | Extra reach per success beyond the first |
| Light Intensity | `8.0` | Brightness at one success, in the point light's own units |
| Light Colour | `1.0, 0.72, 0.36` | Warm firelight rather than a torch bulb |
| Light Flicker Amount | `0.18` | How far the brightness wanders, 0–1 |
| Light Flicker Speed | `6.5` | How fast it wanders |
| Light Casts Shadows | ✘ | See below |
| Light Offset | `0, 0, 5` | Nudge above the socket, on top of the effect's own offset |

A 3-success casting therefore lights about 7.5 metres; a bare success about 4.5.

## What it does on its own

- **Scales with the roll.** Radius grows linearly with successes, brightness on the
  square root — doubling a light's reach does not make it look twice as bright, and
  scaling both linearly gives a strong casting that blows out everything within arm's
  reach.
- **Flickers.** Three sine waves at frequencies that never line up, so it stays
  unpredictable without a noise texture and the eye cannot find the period. Floored well
  above zero, because a flame gutters rather than switching off and back on.
- **Dims out** over `Fade Out Seconds` alongside the flame, on the same ramp, then
  destroys itself.
- **Spawns even with no Niagara system assigned**, so a working can light a room without
  a visible flame — and so a cantrip whose effect asset is still missing at least still
  does its job.

## Leave shadows off

A shadow-casting point light held in the hand re-shadows everything around the caster
every frame as they walk. It is both the most expensive thing on this class and the most
likely to shimmer. If you do turn it on, expect to spend time on it.

## If the light does not appear

- `Light Radius` is still 0.
- `Attach Socket` does not exist on the mesh — the log warns about this by name.
- Post-process or auto-exposure is compensating. Test at night or indoors first.

---

# What the C++ is doing, for reference

- **`ChangelingAnimInstance`** derives every Cantrip variable from the character and its
  ability system once a frame. Nothing sets them, so nothing can leave one stuck.
- **`AnimNotify_CantripEffectStart` / `AnimNotify_CantripRecovered`** are the two entry
  points your transition events call. They work as timeline notifies too, if you ever
  want one at a specific frame instead.
- **`GA_ChannelledCantrip`** roots the caster, refuses to start facing a wall, opens the
  working when the animation says the gesture landed (with a 1s fallback), samples the
  curve every 33ms into both the Niagara and the audio, and hands the movement lock to
  the character on its way out so it survives the ability's own death.
- **`ACHANGELINGCharacter`** owns the movement lock and releases it on the recovery
  event or after 1.5 seconds, whichever comes first.
- **`FCantripAnimSet`** on the table row is what lets ninety cantrips share one set of
  states.
