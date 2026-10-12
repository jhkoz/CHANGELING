# Episode 32 — Ability Hotbar UI: Wiring

## What changes because cantrips are LEARNED

The video's hotbar is a container the player fills from pickups lying in the level. Ours
cannot work that way: there is no pickup actor, and the honest answer to "what can she
cast" is whatever her ability system has actually been granted.

So the source of truth is the ASC, and the C++ derives everything else from it:

| Video | Ours |
|---|---|
| Create a new Player Controller + set it in the Game Mode | Already have `ACHANGELINGPlayerController` — just add a component to its Blueprint |
| Pickup actor carries icon + ability class | `FCantripSpec` already carries `DisplayName`, `Description`, `Icon`, `Element` |
| Per-slot `AbilityImage`, `NumberOnKeyboard`, `HotbarSlot`, `SlotActivated` variables | One `SlotIndex` int. The slot asks the component for the rest |
| Setting `NumberOnKeyboard` by hand on ten slots | Derived from `SlotIndex` — the video's "last button says 10" bug can't happen |
| `OnClicked` fires the ability | `OnPressed` / `OnReleased`, because casting is a **hold** and the hold length is the difficulty reduction |
| Abilities stored on the controller | Abilities on the character's ASC; the **loadout** on the controller, so it survives death |

## New C++ (written, build pending)

**`UCantripHotbarComponent`** — goes on the Player Controller.

| Function | Use |
|---|---|
| `Get Slot (Index)` → `FCantripSlotDisplay` | Everything one slot needs to draw |
| `Get Known Cantrips` | The spellbook list — what the player drags *from* |
| `Find Slot For (Class)` | Grey out what's already laid out |
| `Assign Slot (Index, Class)` | Drop target. Moves rather than copies |
| `Clear Slot` / `Swap Slots` | Drag off, drag onto an occupied slot |
| `Press Slot` / `Release Slot` | Key down / key up |
| `Is Slot Channelling` | Drives the lit style |
| `Refresh From Known` | Reconcile; auto-fills newly learned cantrips |
| `On Hotbar Changed` (event) | Widgets rebuild from this — never poll |

**On `ACHANGELINGCharacter`:** `Learn Cantrip`, `Forget Cantrip`, `Knows Cantrip`, and an
`On Abilities Changed` event the bar listens to. `Learn Cantrip` is the single door a
cantrip comes through — a mentor, a rite, a Realm finally understood. Grant through the
ASC directly and the hotbar never hears about it.

---

## 1. Add the component

1. Open your Player Controller Blueprint (the child of `CHANGELINGPlayerController` your
   Game Mode already points at).
2. **Add Component → Cantrip Hotbar**.
3. Defaults: **Slot Count** `10`, **Auto Fill On Learn** ✅.

No Game Mode change, and no `CharacterReference` variable — the component finds the pawn
itself and re-binds when the controller possesses a new body.

---

## 2. `WBP_HotbarSlot`

Create `UI/WBP_HotbarSlot` (Widget Blueprint → User Widget).

### Hierarchy

```
Border
└── UniformGridPanel
    ├── Button
    │   └── SizeBox
    │       └── Image        ← the cantrip icon
    └── Text                 ← the key number
```

`Text` is a *sibling* of `Button`, not a child — a Button takes exactly one child. The
uniform grid overlaps them so the number still reads as being on the button.

### Details, component by component

**Border** → Brush → **Draw As = None**.

**Text**
- Horizontal Alignment: **Right**
- Font → Typeface **Regular**, Size **10**
- Advanced → Margin: Left `0`, Top `3`, Right `3`, Bottom `0`

**Button** → Interaction → Advanced
- Click Method: **Precise Click**
- Touch Method: **Precise Tap**

**Button → Style**

| State | Tint | Draw As | Corner Radii | Rounding |
|---|---|---|---|---|
| Normal | `0,0,0` α `0.25` | Rounded Box | 4/4/4/4 | Fixed Radius |
| Hovered | `1,1,0` α `0.5` | Rounded Box | 4/4/4/4 | Fixed Radius |
| Pressed | `1,1,1` α `1.0` | Rounded Box | 4/4/4/4 | Fixed Radius |

- Normal Padding: `0.7 / 0.5 / 0.8 / 0.5`  (L/T/R/B)
- Pressed Padding: same
- Color and Opacity: `1,1,1` α `1` — leave it alone, it multiplies everything below

**SizeBox**
- Padding: `2` on all sides
- Width Override `64`, Height Override `64`

Top-right of the designer: **Fill Screen → Desired On Screen**.

### Variables

| Name | Type | Flags |
|---|---|---|
| `SlotIndex` | Integer | Instance Editable, **Expose on Spawn** |
| `EmptyStyle` | Button Style | Instance Editable |
| `AbilityStyle` | Button Style | Instance Editable |
| `ActivatedStyle` | Button Style | Instance Editable |
| `Hotbar` | Cantrip Hotbar Component (object ref) | — |

Style defaults (all Rounded Box, radii 4/4/4/4, Fixed Radius):

| | Normal | Hovered | Pressed |
|---|---|---|---|
| **EmptyStyle** | `0,0,0` α .25 | `1,1,1` α .25 | `1,1,1` α .25 |
| **AbilityStyle** | `1,1,1` α .25 | `0,1,0` α 1 — *green: clicking will cast* | `1,1,1` α 1 |
| **ActivatedStyle** | `1,1,1` α 1 | `1,0,0` α 1 — *red: clicking will cancel* | `1,1,1` α 1 |

### Key number binding

Select **Text** → **Text** property → **Bind → Create Binding**. In the function:

```
Return  =  To Text (int)  of  ( (SlotIndex + 1) % 10 )
```

Slot 0 → "1", slot 8 → "9", slot 9 → "0". Ten slots, no hand-entered numbers.

### Event Construct

```
Event Construct
 → Get Owning Player
 → Cast To <YourPlayerController>
 → Get Component By Class (Cantrip Hotbar Component)   → SET Hotbar
 → Bind Event to On Hotbar Changed  (Hotbar)  → custom event "Refresh"
 → Refresh
```

### `Refresh` (custom event)

```
Hotbar → Get Slot (SlotIndex) → Break FCantripSlotDisplay
 → Branch (bFilled)
   TRUE:
     Icon → Load Asset Blocking → Make Brush from Texture → Image: Set Brush
     Image: Set Color and Opacity  = (1,1,1,1)
     Branch (bChannelling)
       TRUE  → Button: Set Style = ActivatedStyle
       FALSE → Button: Set Style = AbilityStyle
   FALSE:
     Button: Set Style = EmptyStyle
     Image: Set Color and Opacity = (0,0,0,0.25)
```

`Icon` is a **soft** object reference. `Load Asset Blocking` is fine for ten slots; swap
to `Async Load Asset` when the spellbook lists fifty.

### Button events

Select the Button → Details → Events:

- **On Pressed** → `Hotbar → Press Slot (SlotIndex)`
- **On Released** → `Hotbar → Release Slot (SlotIndex)`

**Not `On Clicked`.** Clicked fires once on release with no press edge, and the gather
would never start — the flame appears and is thrown in the same instant.

---

## 3. `WBP_AbilityHotbar`

### Hierarchy

```
Border
└── HorizontalBox
    └── SizeBox
        └── UniformGridPanel
            └── WBP_HotbarSlot ×10
```

1. Drag in one `WBP_HotbarSlot` (Palette → User Created), then Ctrl+C / Ctrl+V nine times.
2. **SizeBox**: Width Override `770`, Height Override `70`.
3. **Border** → Draw As **None**.
4. **Fill Screen → Desired On Screen**.
5. For each slot, set **Slot (Uniform Grid Slot) → Row `0`, Column `0..9`** and the
   exposed **Slot Index `0..9`**. Column and Slot Index are the same number — unlike the
   video, there is no third number to get wrong.

---

## 4. `WBP_HUD`

```
CanvasPanel
└── WBP_AbilityHotbar
```

- Select the hotbar → **Size To Content** ✅, drag to bottom centre, **Anchors → bottom
  centre**.
- Set **Visibility = Visible** on all three: the HUD root, the Canvas Panel, and the
  hotbar. Left at *Not Hit Testable* the mouse passes straight through and nothing is
  clickable.

---

## 5. Character wiring

On `BP_ThirdPersonCharacter` → **Event BeginPlay**:

```
Create Widget (Class = WBP_HUD)  → promote to variable "HUDReference" (category: References)
 → Add to Viewport
```

Toggle the cursor with **I** (keyboard event → **FlipFlop**):

```
A: Get Player Controller → Set Input Mode Game And UI (In Widget to Focus = HUDReference)
   Get Player Controller → Set Show Mouse Cursor = true
B: Get Player Controller → Set Input Mode Game Only
   Get Player Controller → Set Show Mouse Cursor = false
```

---

## 6. Keyboard 1–0

Wire these in the **Player Controller**, not the character. `CantripHotbar` is a component
on the controller, so there is no Get Player Controller, no cast, and no Get Component By
Class — you drag the component straight in. The controller receives Enhanced Input events
exactly as the pawn does.

### 6A. Create the Input Actions

1. Content Browser → `_Custom/GAS/GameplayAbilities/Inputs/` (where the other IA assets are).
2. Right-click → **Input → Input Action**. Name it `IA_Hotbar_01`.
3. Open it. **Value Type** = **Digital (bool)** (the default). Leave **Triggers** and
   **Modifiers** empty — an empty trigger list gives the built-in "Down" behaviour, which
   is what produces one clean press edge and one clean release edge.
4. Close it. Select it in the Content Browser and press **Ctrl+W** nine times.
5. Rename the copies `IA_Hotbar_02` … `IA_Hotbar_10`.

Ten assets is unavoidable: Enhanced Input cannot report *which* key fired a shared action
without per-binding triggers, which is more work than ten assets.

### 6B. Clear the conflicting key first

1. Open `Content/Input/IMC_Default`.
2. Expand **Mappings** and find anything bound to the **3** key — almost certainly
   `IA_Cantrip_Projectile`.
3. Click the **X** at the right of that mapping row to remove it.

Do **not** delete the `IA_Cantrip_Projectile` asset. It is still useful for a dedicated key
that bypasses the bar. It only must not share a key with the hotbar, or key 3 activates
Engulf twice — once directly, once through the slot — and the second activation is refused
by the release cooldown, which looks like the key randomly failing.

### 6C. Map the ten keys

1. Still in `IMC_Default`, click **+** next to **Mappings**.
2. In the new row's action dropdown, choose `IA_Hotbar_01`.
3. Click the key field (it reads *None* and shows a keyboard icon), then **press the 1 key**.
   The field captures whatever you press next.
4. Repeat for all ten:

| Action | Key |
|---|---|
| IA_Hotbar_01 | 1 |
| IA_Hotbar_02 | 2 |
| IA_Hotbar_03 | 3 |
| IA_Hotbar_04 | 4 |
| IA_Hotbar_05 | 5 |
| IA_Hotbar_06 | 6 |
| IA_Hotbar_07 | 7 |
| IA_Hotbar_08 | 8 |
| IA_Hotbar_09 | 9 |
| IA_Hotbar_10 | **0** |

The tenth is the **0** key, not a "10" key. It drives slot index 9.

5. Save.

### 6D. Two helper events

Ten input events × two pins is twenty wires into the same two functions. Two custom events
collapse that to one wire each.

1. Open the Player Controller Blueprint → **Event Graph**.
2. Right-click → **Add Custom Event** → name it `PressHotbarSlot`.
3. Select the node. In **Details → Inputs**, click **+**. Name the input `Index`, set its
   type to **Integer**.
4. Drag `CantripHotbar` from the **Components** panel into the graph.
5. Drag off its blue pin, type `Press Slot`, and place it.
6. Connect the custom event's exec pin → `Press Slot`'s exec pin, and the event's `Index`
   output → `Press Slot`'s **Index** input.
7. Repeat steps 2–6 for `ReleaseHotbarSlot`, calling **Release Slot** instead.

### 6E. The ten input events

1. Right-click in the graph, type `IA_Hotbar_01`, and choose
   **EnhancedInputAction IA_Hotbar_01**.

   The node has five red execution pins: **Triggered, Started, Ongoing, Canceled,
   Completed**.

2. Wire exactly two of them:

   | Pin | Target | Index |
   |---|---|---|
   | **Started** | `PressHotbarSlot` | 0 |
   | **Completed** | `ReleaseHotbarSlot` | 0 |

   **Never Triggered.** With a Down trigger it fires *every frame the key is held* — Press
   Slot would run sixty times a second and the gather would restart continuously instead
   of building toward a tier.

3. Repeat for the other nine. The index is always **one less than the key**:

| Action | Key | Index |
|---|---|---|
| IA_Hotbar_01 | 1 | 0 |
| IA_Hotbar_02 | 2 | 1 |
| IA_Hotbar_03 | 3 | 2 |
| IA_Hotbar_04 | 4 | 3 |
| IA_Hotbar_05 | 5 | 4 |
| IA_Hotbar_06 | 6 | 5 |
| IA_Hotbar_07 | 7 | 6 |
| IA_Hotbar_08 | 8 | 7 |
| IA_Hotbar_09 | 9 | 8 |
| IA_Hotbar_10 | 0 | 9 |

4. Select the block → **C** to comment it "Hotbar keys 1–0".
5. Compile, Save.

### 6F. Verify

PIE, then hold **1**. Expected:

```
LogCantripAbility:     ACTIVATE BP_GA_Engulf_C: row='Pyretics_4' -> channel open
LogProjectileCantrip:  GATHER BP_GA_Engulf_C: flame in hand, full charge at 4.20s
```

and on release:

```
LogCantripAbility:     RELEASE BP_GA_Engulf_C: held 1.80s -> tier 3
```

Two `ACTIVATE` lines for one press means a second mapping is still on that key.

---

## 7. Learning a cantrip

### 7A. The call

```
<trigger>
 → Cast To BP_ThirdPersonCharacter
 → Learn Cantrip (Ability Class = BP_GA_<Whatever>)
```

`Learn Cantrip` returns a **bool** — true if she learned it, false if she already knew it.
Branch on the return rather than checking first.

### 7B. Worked example: a trainer

On the trainer Blueprint's interact event:

```
Interact
 → Cast To BP_ThirdPersonCharacter          (from the interacting actor)
 → Knows Cantrip (BP_GA_Engulf)
 → Branch
     True  → [dialogue] "You already know this working."
     False → Learn Cantrip (BP_GA_Engulf)
           → Branch (on the returned bool)
               True → [dialogue] "The flame answers you now."
```

Nothing else is needed. No hotbar call, no widget refresh: `Learn Cantrip` broadcasts
`On Abilities Changed`, the component reconciles, auto-fill drops the cantrip into the
first free slot, and the bar redraws itself.

### 7C. Authority

`Learn Cantrip` checks `HasAuthority` and **returns false silently on a client**. In
single-player PIE this never shows. Test with 2 clients and a client-side trainer appears
to do nothing at all.

If the trigger is client input, make the trainer's interact a **Run on Server** event and
call `Learn Cantrip` inside that.

### 7D. Starting cantrips

Anything in **DefaultAbilities** on `BP_ThirdPersonCharacter` is granted at possession.
That path broadcasts `On Abilities Changed` once, after the whole list rather than per
ability, so the bar fills from the starting set on BeginPlay with no wiring at all.

The order of `DefaultAbilities` is the auto-fill order, so it decides which cantrip lands
on key 1.

### 7E. Forgetting

`Forget Cantrip` cancels the ability if it is mid-cast, removes it, and empties its slot —
for a Banality crisis or a lost Art. Clearing an ability that is still casting without
cancelling it first strands its tasks, which is why it is one call rather than two.

---

## Log lines

```
LEARNED BP_GA_Engulf_C (input 4).
HOTBAR: BP_GA_Engulf_C -> slot 0 (learned).
HOTBAR: refused BP_GA_Something_C in slot 3 -- not granted to this character.
```

The third means the drop target accepted a cantrip she doesn't know — the drag source is
listing more than `Get Known Cantrips` returns.

## Next

Episode 33 is ability pickups, which don't apply. The useful half of it — a spellbook
panel listing `Get Known Cantrips` and dragging from it onto the bar — is episode 35's
drag-and-drop, and `Assign Slot` / `Swap Slots` are already built for it.
