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
| 5 | The flamethrower Niagara system | 20 min |
| 6 | The looping sound cue | done already |
| 7 | The ability Blueprint + table row | 10 min |
| 8 | Input | 5 min |
| 9 | Foot IK | 2 min |
| 10 | Test and troubleshoot | — |

Part 3 is two text fields and is the part where a typo actually costs you something.
Part 1 has one gotcha that will stop the compile until you handle it.

---

# Part 0 — Split the animation into three clips

We are using **Standing 2H Magic Attack 03**, which is **129 frames / 4.30 seconds** at
30fps. You already have it retargeted as `Standing2HMagicAttack03_UE` in
`Content/Characters/Mannequins/Anims/Unarmed/Mixamo/`, but as one full-length clip. The
state machine needs three, because the middle has to repeat indefinitely while the two
ends play exactly once.

Attack 04, which the tutorial uses, is 100 frames / 3.33s — so **every frame number the
video quotes is about 29% short for our clip.**

## 0.1 Download three trimmed copies from Mixamo

1. Go to mixamo.com, sign in, and make sure `SK_Mannequin` from the Mixamo Converter is
   still your uploaded character.
2. Search for **Standing 2H Magic Attack 03** and select it.
3. Under the preview there is a **trim** control with two handles and a frame counter.

Download it three times, adjusting the trim each time. For every download use:
- Format: **FBX Binary (.fbx)**
- Skin: **Without Skin**
- Frames per Second: **30**
- Keyframe Reduction: **Uniform**

| Download | Trim from | Trim to | Save as |
|---|---|---|---|
| 1 | 0 | ~39 | `Cantrip_Channel_Begin.fbx` |
| 2 | ~43 | ~99 | `Cantrip_Channel_Loop.fbx` |
| 3 | ~101 | ~121 | `Cantrip_Channel_End.fbx` |

**These are scaled from the tutorial's numbers, not read off the clip — I can measure its
length but not watch it, so treat them as a starting point and confirm by scrubbing.**
What you are actually looking for:

- **Begin ends** at the frame where the arms reach the forward extended pose and stop
  travelling. Slightly late is better than slightly early — a wind-up cut short pops.
- **Loop** is the span where the body is only churning in place. Its **first and last
  frames should be as close to the same pose as you can get**; everything else about the
  loop is easier to fix than that. Leave a couple of frames of gap after Begin.
- **End starts** where the arms first begin to withdraw and runs until the body settles.
  Stop before any final idle settle at the very end of the clip.

On download 2, scrub the preview before downloading — it should read as a continuous
churn with no obvious start or stop. You will see it jump when it cycles; ignore that,
Part 2 fixes it in the graph.

Note the length of your Begin clip once it is imported — Part 7 uses it.

## 0.2 Import — SKIP the converter for these files

**Your three clips already carry a full UE5 Manny skeleton, root bone included.** They do
not go through the Mixamo Converter and they do not go through the retargeter. Feeding
them to the converter produces `Can't add root bone: existing root detected!`, which is
the tool correctly refusing to add a root to a skeleton that has one.

How to tell, for any future clip: open it and look for `ik_foot_root`, `ik_hand_gun`,
`interaction` or `center_of_mass`. Those are Unreal-only bones that Mixamo never
produces. If they are present, the file is already converted.

4. Drag your three FBX files into `Content/Characters/Mannequins/Anims/Cantrips/`.
5. In the import dialog set:
   - **Skeleton**: the **UE5 Manny** skeleton — the one `ABP_Unarmed` uses. Not the UE4 one.
   - **Import Translation**: `0, 0, 0` — the `0,0,-3` used elsewhere corrects for the
     Mixamo pipeline and would sink these into the floor.
   - **Import Mesh**: unticked. Animation only.
6. Click **Import All**.
7. Rename to `A_Cantrip_Channel_Begin` / `_Loop` / `_End`.
8. Open each and confirm the feet sit on the ground and the hands are not twisted. A
   wrong skeleton pick shows up immediately as mangled limbs.

Note the length of your Begin clip — Part 7.2 needs it.

## 0.3 If you ever do need the converter

For a genuinely raw Mixamo download (no root bone), the route is: converter's
`IncomingFbx` → run it → `OutgoingFbx` → import against the **UE4** mannequin skeleton
with **Import Translation `0,0,-3`** → open `RTG_UE4_Manny_to_UE5_Manny` → select the
animations → **Export Selected Animations**.

One thing to know if it fails: the converter's actual engine is `MixamoToUE.exe` plus
`libfbxsdk.dll`, and both live *inside* the `IncomingFbx` folder. If they are missing,
every conversion fails with "Failed to recover the error" — that message means the GUI
could not launch its own backend, not that anything is wrong with your files.

---

# Part 1 — Reparent ABP_Unarmed

`ABP_Unarmed` at `Content/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed` is the
AnimBP your character actually uses.

1. Open it.
2. **File → Reparent Blueprint**.
3. In the class picker, type `ChangelingAnimInstance` and select it.
4. Click **Compile**.

## 1.1 You will not see the new variables yet

**The My Blueprint panel hides inherited variables by default.** After reparenting,
nothing appears to have changed — this is the panel's filter, not a failed reparent.

5. In the **My Blueprint** panel, click the **settings / gear icon** in its top-right
   corner.
6. Tick **Show Inherited Variables**.

The ten variables in 1.3 now appear under a `Cantrip` category.

Two ways to confirm the reparent actually took:

- **Class Settings → Parent Class** should read `ChangelingAnimInstance`. If it still
  says `AnimInstance`, the reparent did not happen.
- Right-click in the graph and search `Cantrip Pose`. The context menu finds inherited
  variables regardless of the My Blueprint filter, so a hit there means everything is
  wired up.

## 1.1a If the compile fails on a name clash

Only relevant if you happen to have a local variable with the same name as one of the
parent's. If so, right-click your local copy in My Blueprint → **Delete**, and compile
again. Nodes that *read* it re-link to the parent's version automatically, since the
name and type match — you should not have to rewire anything.

Then check `BP_ThirdPersonCharacter` for a **`Set Cantrip Pose`** node pushing the pose
into the AnimBP. If one exists, delete it: the parent reads the pose off the character
every frame now, and the parent's copy is read-only.

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
| 1 | Cantrip Idle → Channel Begin | `bCantripCasting` → Can Enter Transition |
| 2 | Channel Begin → Channel Loop A | `bCantripChannelling` → Can Enter Transition |
| 3 | Channel Loop A → Channel Loop B | `Get Relevant Anim Time Remaining` < `0.1` |
| 4 | Channel Loop B → Channel Loop A | `Get Relevant Anim **Time**` < `0.1` — NOT time remaining |
| 5 | Channel Loop A → Channel End | `bCantripRecovering` → Can Enter Transition |
| 6 | Channel Loop B → Channel End | `bCantripRecovering` → Can Enter Transition |
| 7 | Channel End → Cantrip Idle | time-remaining rule |
| 8 | Channel Begin → Cantrip Idle | `bCantripCasting` → **NOT Boolean** → Can Enter |

Three of those are worth understanding rather than just copying, because the obvious
versions all race:

- **#1 uses `bCantripCasting`, not `bCantripChannelling`.** The wind-up gesture has to
  play *while* you are building the cast. Channelling does not become true until the roll
  resolves, so keying the wind-up off it would leave the character standing still through
  the entire hold and then playing the wind-up after the flame was already lit.
- **#2 uses `bCantripChannelling` with no time check.** The Begin clip and the cast ladder
  are different lengths and neither is authoritative. With loop off, whichever finishes
  first simply waits: if the clip ends early the character holds the completed gesture
  until the working opens, which is exactly right.
- **#5 and #6 use `bCantripRecovering`, not `NOT bCantripChannelling`.** "Not channelling"
  is equally true *before* the working ever opens, so on any frame where the loop is
  entered a moment before the roll lands, the End animation would fire instantly and the
  cast would collapse. `bCantripRecovering` is false until a channel has actually opened.
### Start with ONE looping state

Build `Channel Loop A` with **Loop Animation ticked** and skip `Channel Loop B` and
transitions #3/#4 entirely. The ping-pong exists only to hide a seam when the loop clip's
first and last frames do not match, and if you chose your own trim points it may cycle
cleanly without it. A possible seam every few seconds beats a guaranteed jitter.

Add the ping-pong only if you actually see a twitch. When you do, note that **the two
directions need different questions**: `Channel Loop B` plays with **Play Rate −1** from
**Start Position 1.0**, and "time remaining" is computed as *length − current position*
regardless of direction. B therefore starts with zero time remaining and transitions out
on its first frame — A and B alternate every frame and the character shakes. A clip
playing backwards finishes near time **zero**, so B→A asks for its current
**Time**, not its Time Remaining.

- **#8 is the abandoned wind-up** — you let go before the flame appeared. Without it the
  inner state machine stays parked in Channel Begin and the next cast starts from the
  wrong state.

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

Two text fields. They are matched by string, character for character, and they are
case-sensitive. This is the shortest part of the document and the one most worth
slowing down for.

You are **not** creating notify assets and **not** adding Event Graph nodes. The two
functions already exist on `ChangelingAnimInstance`; Unreal calls them by name when a
transition starts or ends. Typing the name here is the entire wiring.

## 3.1 Effect start — makes the flame appear

1. Open `ABP_Unarmed`.
2. In **My Blueprint** (left panel), under **Animation Graphs**, double-click
   `Cantrip Casting` to open that state machine.
3. Find the arrow running from **Channel Begin** to **Channel Loop A**. Every transition
   arrow has a small round node partway along it.
4. **Single-click that circle** to select the transition. Do not double-click — that
   opens the rule graph, which is not what you want here.
5. Look at the **Details** panel (right side). With a transition selected it shows
   sections including **Transition**, **Blend Settings**, **Notifies** and **Events**.
6. Expand **Events**.
7. You will see two entries: **Start Transition Event** and **End Transition Event**.
   Each expands into fields including **Custom Blueprint Event**.
8. Expand **Start Transition Event** and type into its **Custom Blueprint Event** field:

```
CantripEffectStart
```

9. Press **Enter** to commit the text. Clicking away without pressing Enter can drop it.
10. **Compile.**

### Verifying it took

Reselect the transition and re-read the field. If it is blank, the text was not
committed — retype it and press Enter.

## 3.2 Recovered — gives the character its feet back

11. Navigate **up** to the state machine that contains **Locomotion**. This is the
    top-level one, not `Cantrip Casting`. Use the breadcrumb bar along the top of the
    graph, or double-click the parent state machine in My Blueprint.
12. Find the arrow from **Cantrip Casting** back to **Locomotion**.
13. Single-click its circle.
14. **Details → Events → End Transition Event → Custom Blueprint Event**:

```
CantripRecovered
```

15. Press **Enter**.
16. **Compile and save.**

Note this one is **End** Transition Event, where 3.1 was **Start**. They are different
fields in the same-looking section, and putting the name in the wrong one produces no
error at all — just the symptom described below.

## 3.3 Why this one is on the transition and not in a clip

The obvious place for "the cast is over" is a notify on the last frame of the End
animation. Do not do that.

A notify inside a clip only fires if that clip plays as far as the frame the notify sits
on. A cast that was interrupted, cancelled, or ended because the character was killed
mid-gesture never reaches that frame, and the movement lock never lifts.

The **Cantrip Casting → Locomotion** transition is taken on *every* exit, however the
cast ended. That is why the name goes here.

## 3.4 The safety net, and what it tells you

There is a **1.5-second timeout** in C++ behind `CantripRecovered`. If the event never
arrives, the character is released anyway.

This means a typo in 3.2 does not freeze your character — it gives you a **consistent
short pause** between the end of the recovery animation and being able to move again.

**If you see that pause in testing, check 3.2's spelling before looking at anything
else.** It is the single most likely mistake in this document.

The equivalent fallback for 3.1 is **1 second**. A typo there shows up as the flame
appearing at a fixed beat that never quite matches the gesture.

---

# Part 4 — The EffectIntensity curve

One curve drives three things: the Niagara spawn rate, the sound volume, and the sound
pitch. They all read the same source, so they cannot drift apart from each other or from
the animation.

The name must be exactly `EffectIntensity` on all three clips — it is how the C++ finds
it, and it is how the three clips' curves join into one continuous signal as the state
machine blends between them.

## 4.0 Finding the curve editor

1. Double-click an animation asset to open it in the Animation Editor.
2. The timeline runs along the bottom. To its left is a track list containing
   **Notifies** and, below it, **Curves**.
3. If you cannot see a **Curves** track, use **Window → Curves** from the menu bar, or
   click the **+ Curve** / **Add Curve** button in the track list header.

## 4.1 Begin clip — the ramp up

4. Open `A_Cantrip_Channel_Begin`.
5. In the track list, click **+ Curve → Add Curve** (some versions: **Add Float Curve**).
6. Name it exactly:

```
EffectIntensity
```

7. **Double-click the curve's name** in the track list to open its graph editor. You
   should now see a horizontal line and a numeric vertical axis.
8. **Right-click on the curve line at roughly 55% along → Add Key.**
9. Click that key to select it. Its **Time** and **Value** appear in fields above the
   graph (or in the Details panel). Set **Value** to `0`.
10. **Right-click at the far right end of the timeline → Add Key.**
11. Select it and set **Value** to `1.0`.
12. If the top of the graph is off-screen, **scroll out with the mouse wheel** and pan
    with the **right mouse button held**.
13. **Right-click each key → Auto** (under a Tangent or Interpolation submenu). This
    curves the ramp instead of leaving a hard kink.
14. **Save.**

The shape you want: flat at zero for the first half of the clip, then a smooth rise to
1.0 by the last frame. The flame is dark while the arms come up, and at full strength
the instant the gesture lands.

## 4.2 Loop clip — flat at full

15. Open `A_Cantrip_Channel_Loop`.
16. **+ Curve → Add Curve.** This time `EffectIntensity` already exists in the project —
    pick it from the list rather than typing it again, so the names cannot diverge.
17. Double-click it to open the graph.
18. **Right-click anywhere on the line → Add Key.** Select it, set **Value** to `1.0`.

A single key sets the entire line to that value.

19. **Zoom out and confirm the line is flat at 1.0 across the whole clip.** If it slopes,
    you have a second key somewhere — delete it.
20. **Save.**

## 4.3 End clip — the fall

21. Open `A_Cantrip_Channel_End`.
22. **+ Curve → Add Curve → `EffectIntensity`.**
23. Add a key at **Time 0**, **Value `1.0`**.
24. Add a key at roughly **60%** along, **Value `0`**.
25. **Right-click both keys → Auto.**
26. **Save.**

The last 40% of the clip sits at zero deliberately: the flame is out well before the arms
finish dropping, so the character is visibly finishing a gesture rather than the fire
being cut off at the same moment.

## 4.4 If you skip this part entirely

The C++ treats a missing curve as full strength throughout. The cantrip works — flame,
sound, everything — it just switches on and off at full intensity instead of swelling
and dying. Worth knowing so you can test Parts 5–8 before coming back to do this.

---

# Part 5 — The flamethrower Niagara system

You already own a purpose-built one: `Content/MixedVFX/Particles/Fires/NS_FlameThrower`.
Use it. It is a **CPU** sprite system, which is what we need — GPU emitters cannot hand
collision data back out, and we will want that later for what the fire actually burns.

**This replaces converting `P_Fire` from Cascade.** You skip enabling the converter
plugin, the conversion itself, clearing dozens of conversion warnings, and disabling
unwanted emitters. What is left is adding two parameters and a collision module.

Roughly 20 minutes instead of 45.

## 5.0 What is in the asset

Read from the asset itself, so expect these but do not be alarmed by small differences:

| | |
|---|---|
| Emitters | `DirectionalBurst`, `Empty`, `Empty001` (the last two are just unnamed) |
| Simulation | CPU |
| Renderer | Sprite, all three using `MI_Fire` |
| Modules | Sphere Location, Add Velocity, Acceleration Force, Apply Initial Forces, Solve Forces And Velocity, Colour + Colour Curve, Scale Colour, Scale Sprite Size, Scale Sprite Size By Speed, Sprite Rotation Rate |
| User parameters | **none — you add two** |
| Collision | **none — you add one per emitter** |

The pack author has already tuned the spawn rates, colour curve and sizes. **Preserve
that work.** Everything below wraps or adds to what is there rather than replacing it.

## 5.1 Duplicate it first

1. Create the folder `Content/_Custom/Niagara/Cantrips/Pyretics/`.
2. Right-click `MixedVFX/Particles/Fires/NS_FlameThrower` → **Duplicate**.
3. Move the copy into that folder and name it:

```
NS_Cantrip_Flamethrower
```

Work only on the copy, so the pack asset stays pristine and you always have something to
compare against.

## 5.2 Add the two user parameters

4. Open `NS_Cantrip_Flamethrower`.
5. In the left-hand panel of the **System** view, find **User Exposed Parameters**.
6. Click **+ → Make New → Common → float**. Name it exactly:

```
Intensity
```

7. Click **+ → Make New → Common → float** again. Name it exactly:

```
FadeAlpha
```

Spelling is load-bearing. The C++ writes these by name; a wrong name lands on nothing and
fails silently — you get a flamethrower that never appears and no error anywhere.

**What they do.** `Intensity` follows the animation curve, 0–1, so the fire swells as the
arms come up. `FadeAlpha` sits at 1 while the working is open and ramps to 0 over half a
second when it closes, so the flame thins out instead of being switched off.

## 5.3 Write down the existing spawn rates — do this before you change anything

8. Select the **DirectionalBurst** emitter. In **Emitter Update → Spawn Rate**, note the
   number.
9. Do the same for **Empty** and **Empty001**.

Write all three down.

The next step replaces that local value with a dynamic input, and **the number you have
now is gone the moment you do**. It is the pack author's tuning and you want it back.

| Emitter | Existing Spawn Rate |
|---|---|
| DirectionalBurst | |
| Empty | |
| Empty001 | |

## 5.4 Wrap each spawn rate as `Existing × Intensity × FadeAlpha`

Do this once per emitter, using that emitter's own noted number.

10. Select the emitter. Go to **Emitter Update → Spawn Rate**.
11. Click the **small dropdown arrow to the right of the Spawn Rate value field.** A menu
    appears with sections including *Set a Local Value*, *Link Inputs* and
    *Dynamic Inputs*.
12. Under **Dynamic Inputs**, choose **Multiply Float by Float**. Two sub-inputs, **A**
    and **B**, appear.
13. Type your noted number into **A**.
14. Click **B's dropdown → Link Inputs → User.Intensity**.
15. Click the **top-level Spawn Rate** dropdown again → **Dynamic Inputs → Multiply Float
    by Float**. This wraps what you just built; the previous multiply moves under **A**.
16. Click the new **B's dropdown → Link Inputs → User.FadeAlpha**.

Result: `(Existing × Intensity) × FadeAlpha`.

If step 15 puts your work in the wrong slot, undo and build outermost-first instead: make
the top multiply, link **B** to `FadeAlpha`, then give **A** its own Multiply Float by
Float holding the number and `Intensity`.

17. Repeat for all three emitters.

## 5.5 Set the velocity to Local space — the one that will bite you

For each of the three emitters:

18. **Particle Spawn → Add Velocity.**
19. Set **Rotation Coordinate Space** to **`Local`**.

This is what makes the flame come out of the hand in the direction the hand is pointing.
Left on World, the fire fires along a fixed world axis no matter which way the character
faces, and it looks broken from the very first test.

While you are in there, check **Particle Spawn → Sphere Location → Radius**. It wants to
be small — around `10` — so the fire emanates from a point in the hand rather than a
cloud around it. The pack may already have it low; only change it if it is large.

## 5.6 Add collision, so fire does not pour through walls

The downloaded system has no Collision module. Without one the flame passes through
geometry, which for a hand-projected gout at close range is very visible.

For each of the three emitters:

20. Click the **+** at the top of the **Particle Update** section.
21. Search `Collision` and add it.
22. **Drag the module so it sits below Acceleration Force.** Order matters in Niagara.
23. Set **CPU Collision Trace Channel** to **`Pawn`** on all three. This is easily missed
    and a wrong value produces odd behaviour rather than obvious failure.

Then per emitter:

| Setting | DirectionalBurst | Empty | Empty001 |
|---|---|---|---|
| Restitution | `0.05` | `0.7` | `0.7` |
| Randomize Collision Normal Vector | `0.2` | `0.2` | `0.2` |
| Friction | `0.9` | `0.9` | `0.9` |
| Friction During Bounce | `0.9` | `0.3` | `0.5` |
| Advanced → Advance Aging Rate | — | `5` | `5` |

The mixed restitution is deliberate: one emitter's particles die roughly where they land
while the others skitter off surfaces, and that difference is what stops the collision
reading as uniform rubber-ball bouncing.

`Advance Aging Rate` makes particles age faster once they have hit something, so fire
does not pile into a bright wad against a wall.

## 5.7 Confirm it loops

24. Select each emitter, open **Emitter State**, and check **Loop Behavior**.
25. It should be **Infinite**. If any emitter is set to `Once`, change it.

A channelled cantrip runs for as long as the key is held. An emitter set to fire once
will burn for its loop duration and then quietly stop while the player is still holding.

## 5.8 Light

26. If any emitter has a **Light Renderer**, untick it.

Particle lights cost roughly **20 fps** at high intensity against about **2 fps** without,
and the fire still reads as self-lit because the sprites are emissive. If you want the
flamethrower to genuinely light the room, do it the way Illuminate does — one point
light, not thousands of particle lights. The Appendix has that machinery already.

## 5.9 Save, and why the preview looks empty

27. **Compile**, then **Save**.

**The preview now shows no particles at all.** Both user parameters default to 0, and
anything multiplied by zero is zero. This is correct, and it is the quickest confirmation
that 5.4 is wired properly.

To look at it: temporarily set `Intensity` and `FadeAlpha` to `1` in the **User Exposed
Parameters** panel. **Set them both back to 0 before saving** — saved at 1, the system
bursts at full strength the instant it spawns, before the ability has said anything.

## 5.10 Keep it on CPU

Do not convert these emitters to GPU. GPU collision does not behave well here, and more
importantly we will later want the collision data passed back out — what is burning, how
fast — which only CPU emitters can provide.

---

# Part 6 — The looping sound (already built)

**You already have this**, from the earlier fire work:

```
Content/_Custom/Audio/Character/Abilities/Fire/FlamethrowerChannel_Cue
```

Read from the asset, it already contains:

- a **Random** node choosing between `Flamethrower_01` and `Flamethrower_02` — the
  two-source variation the tutorial builds by hand in Audacity
- looping set on the wave players
- **Attenuation Settings** → `FootstepsHardSurfaceAttenuation`, which is exactly the
  asset this part would have told you to assign

So there is nothing to build. Two things to confirm:

1. Open the cue and check **Looping** is ticked on **both** Wave Player nodes.
2. In Part 7, set **Channel Sound** to `FlamethrowerChannel_Cue`.

## 6.1 What you are deliberately not building

The tutorial wires an Event Tick that reads the animation curve and drives volume and
pitch. Skip all of it. `GA_ChannelledCantrip` already samples the curve every 33ms and
writes both, from the same source that drives the Niagara — so the fire and its sound
cannot drift apart.

---

# Part 7 — The ability Blueprint, timing, and the table row

## 7.1 The ability already exists — fix one field

`Content/_Custom/GAS/GameplayAbilities/Cantrips/Primal/GA_Primal_EldritchPrime` is
already there, already parented to `GA_ChannelledCantrip`, and already has `DT_Cantrips`
assigned. You do not create anything.

**One field is wrong and it is the only thing blocking the whole cantrip.**

1. Open it, click **Class Defaults**.
2. Change **Cantrip Row** from `Primal_2` to:

```
EldritchPrime
```

Rows in `DT_Cantrips` are keyed by the cantrip's NAME, not by `<Art>_<Level>`. With the
wrong key the ability activates, fails to find its row, and fizzles without ever opening
a channel — which is exactly the "nothing happens" you were seeing, and it logs
`Cantrip row 'Primal_2' not found.` every single time.

Then set the rest:

| Section | Property | Value |
|---|---|---|
| Cantrip | Cantrip Table | `DT_Cantrips` (already set) |
| Cantrip | Cantrip Row | `EldritchPrime` |
| Cantrip | Realms Used | one element: `Actor` |
| Cantrip | Glamour Cost Effect | your Glamour cost GE |
| Sustained | Sustained Effect | `NS_Cantrip_Flamethrower` |
| Sustained | Attach Socket | `hand_rSocket` |
| Sustained | Attach Offset | `0, 0, -20` |
| Sustained | Sustained Pose | `Both Hands Raised` |
| Sustained | Fade Parameter | `FadeAlpha` (default) |
| Sustained | Fade Out Seconds | `0.5` (default) |
| Channelled | Channel Sound | `FlamethrowerChannel_Cue` |
| Channelled | Channel Pitch Range | `0.9, 1.1` |
| Channelled | Lock Movement While Channelling | ✔ |
| Channelled | Forward Clearance | `120` |
| Channelled | Clearance Probe Height | `60` |
| Channelled | Recheck Clearance While Channelling | ✘ |
| Channelled | Intensity Curve Name | `EffectIntensity` (default) |
| Channelled | Intensity Parameter | `Intensity` (default) |

3. **Compile and save.**

### One design note, for later

Eldritch Prime is *"conjure raw manifestations of the elements"* — Primal 2, and its row's
Element is currently **Earth**. Hard-wiring a fire Niagara into it makes the whole cantrip
mean "flamethrower", when the book's version commands any of the four.

Nothing to do now. But when a second element wants the same cantrip, the effect wants to
come off the caster's chosen element rather than off the ability's defaults — the
`CantripElement` variable on the anim instance already exists for exactly that. If you
would rather keep it simple, change the row's Element to `Fire` and treat this cantrip as
the fire one.

## 7.2 Match the cast ladder to the gesture — do not skip this

By default the working opens at the end of the cast ladder, at **4.2 seconds**. Your
Begin clip is roughly **1.3 seconds**. Left alone, the character completes the whole
gesture and then stands holding the finished pose for another three seconds before
anything ignites. It reads as the game having hung.

8. Open `DT_Cantrips`, find row `EldritchPrime`.
9. Expand **Cast Tiers → Tier Thresholds**.
10. Set the five entries to fit inside the Begin clip. For a 1.3s gesture:

```
0.25, 0.5, 0.8, 1.05, 1.3
```

Now the flame kindles on the frame the arms finish coming forward.

**Alternative:** to keep the long ladder and just have the flame appear earlier, set
**Channel Opens After Seconds** on the ability instead. It overrides the ladder for
timing only.

**Either way the ordering is safe.** The C++ latches the animation's effect-start cue, so
whichever of the gesture and the roll finishes second is the one that spawns the flame.
Getting this wrong costs a pause, not a broken cantrip.

## 7.3 The table row

Still in `DT_Cantrips`, row `EldritchPrime`:

11. **Kind** → `Channelled (held)`
12. **Element** → `Fire`
13. Expand **Anims**:
    - **Begin** → `A_Cantrip_Channel_Begin`
    - **Loop** → `A_Cantrip_Channel_Loop`
    - **End** → `A_Cantrip_Channel_End`
14. **Save.**

`Kind` is not decoration — it is how a row and its ability class declare the same shape,
so a mismatch can be caught rather than silently producing a cantrip that behaves unlike
its description.

---

# Part 8 — Input

## 8.1 Grant the ability

1. Open `BP_ThirdPersonCharacter` → **Class Defaults**.
2. Find **Abilities → Default Abilities**.
3. Click **+** and set the new entry to `GA_Primal_EldritchPrime`.

## 8.2 Create the input action

4. In your Input folder, **right-click → Input → Input Action**.
5. Name it `IA_Cantrip_Fire`.
6. Open it, set **Value Type** to `Digital (bool)`.
7. Open your **Input Mapping Context**, click **+** on **Mappings**, choose
   `IA_Cantrip_Fire`, and bind it to a key.

## 8.3 Three nodes

8. In `BP_ThirdPersonCharacter`'s **Event Graph**, right-click → search
   `IA_Cantrip_Fire` → add the **EnhancedInputAction IA_Cantrip_Fire** event node. It has
   output pins: Triggered, Started, Completed, Canceled.

9. From **Started**, add **Try Activate Ability by Class**:
   - **Target**: drag from self → **Get Ability System Component**
   - **Ability Class**: `GA_Primal_EldritchPrime`

10. From **Completed**, add **Stop Channelling** (Target: self).
11. From **Canceled**, add **Stop Channelling** (Target: self).

That is the whole binding. No branch, no stored reference, no flip-flop, no
retriggerable delay — `Stop Channelling` handles both meanings of letting go, and does
nothing harmlessly if nothing is running.

12. **Compile and save.**

## 8.4 How it plays, so you can tell working from broken

Hold the key. The wind-up gesture runs. At the end of the cast ladder the working opens
**by itself** and the flame appears — **you do not release to fire it.** Keep holding and
it keeps burning. Let go and it goes out.

Releasing **during** the wind-up, before the flame appears, abandons the cast entirely:
no roll, no Glamour, nothing happens. A channel has no weaker version to fire, so an
abandoned wind-up reads as changing your mind rather than as a fizzle you paid for.

One consequence worth understanding: because a channel always resolves at the top of the
cast ladder, the cast-time tiers do **not** vary a channelled cantrip's difficulty the way
they vary an ordinary cantrip's. The sustained gesture *is* the full performance. That is
why 7.2 is about timing and not about power.

---

# Part 9 — Foot IK

1. In `ABP_Unarmed`'s **AnimGraph**, find the foot-IK **Control Rig** node. It references
   `CR_Mannequin_FootIK` or similar.
2. Select it.
3. In **Details**, set **Alpha Input Type** to **Bool**.
4. A new **Alpha Bool Value** pin appears on the node.
5. Drag `bAllowFootIK` from My Blueprint into the graph and connect it to that pin.
6. **Compile and save.**

The casting clips were authored for a stationary body and have no ground contact to solve
against, so IK against them lifts the feet clear of the floor.

`bAllowFootIK` is `NOT casting AND NOT falling`, derived in C++ every frame. Because
nothing ever sets it, it cannot get stuck off after a cast that ended badly — which is
exactly the failure mode of doing this with a Blueprint boolean.

---

# Part 10 — Test

Work down this list in order. Each step isolates one part, so the first thing that fails
tells you where to look.

| # | Do this | Expect | If it fails |
|---|---|---|---|
| 1 | Press and hold the key | Wind-up plays, feet stay on the ground | Parts 2, 9 |
| 2 | Keep holding | Flame appears as the arms finish, sound fading in under it | Parts 3.1, 5, 7.2 |
| 3 | Try to walk while holding | No movement at all | Part 7.1 (Lock Movement) |
| 4 | Release | Recovery plays, flame thins over ~½s, movement returns | Parts 3.2, 5.5 |
| 5 | Release during the wind-up | Nothing at all — no flame, no sound, no cost | Part 2, transition #8 |
| 6 | Spam the key | No stuck poses, no permanent flame, clean output log | Part 2 |
| 7 | Stand a foot from a wall and cast | Refuses rather than firing through it | Part 7.1 (Forward Clearance) |

## Troubleshooting

| Symptom | Cause |
|---|---|
| Short consistent pause before you can move after every cast | `CantripRecovered` misspelled, or in **Start** instead of **End** Transition Event — Part 3.2 |
| Never able to move after a cast | Character Blueprint does not derive from `ACHANGELINGCharacter` |
| No flame at all, ever | `Intensity`/`FadeAlpha` misspelled in Niagara, or the multiply chain is not wired — Parts 5.5, 5.6 |
| Flame appears at a fixed beat that never matches the gesture | `CantripEffectStart` misspelled; the 1s fallback is firing it — Part 3.1 |
| Gesture completes, then a long wait before the flame | Cast ladder longer than the Begin clip — Part 7.2 |
| Wind-up never plays; character stands still, then flame | Transition #1 keyed off `bCantripChannelling` instead of `bCantripCasting` |
| End animation fires the instant the loop starts | Transitions #5/#6 keyed off `NOT bCantripChannelling` instead of `bCantripRecovering` |
| Flame fires sideways or in a fixed world direction | A **Rotation Coordinate Space** left on World — Parts 5.7–5.9 |
| Fire passes through the character or through walls | A **CPU Collision Trace Channel** left on World Dynamic — Parts 5.7–5.9 |
| Character twitches every couple of seconds while channelling | Loop B missing **Play Rate −1** / **Start Position 1.0** — Part 2.3 |
| Flame bursts at full strength the instant it spawns | `Intensity`/`FadeAlpha` saved at 1 in the Niagara preview — Part 5.10 |
| Feet float during the cast | Part 9 not done |
| Sound plays but never changes volume | Curve missing or misnamed — Part 4. Not fatal; C++ falls back to full strength |
| Cast does nothing, log mentions a row | `Cantrip Row` does not match a row name in `DT_Cantrips` |

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
