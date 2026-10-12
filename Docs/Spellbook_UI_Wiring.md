# Spellbook UI — Wiring

Eighteen Art roundels in a 2×9 rail, five cantrips on the page, a card on hover.
Mockup: the published **CHANGELING Spellbook** canvas.

## The model, in one line

**Her rating in an Art is the set of cantrips she knows.** Pyretics 3 *is* the first three
Pyretics workings. There is no separate learned-list, so the page has one boundary and one
rung above it — nothing else to express.

| Field on `FCantripBookEntry` | Meaning | Treatment |
|---|---|---|
| `bKnown` | `ArtLevel <= ArtRating` | full colour |
| `bNextToLearn` | `ArtLevel == ArtRating + 1` | lit, cyan chip — the only actionable row |
| neither | beyond that | 18% opacity, locked chip naming the rating |
| `SlotIndex` ≥ 0 | on the hotbar | key badge |
| `Display.AbilityClass` valid | actually granted — castable, draggable | gates the drag |

`bKnown` and a valid `AbilityClass` normally agree. They part only if the granted abilities
have drifted from the ratings, and then the book should read as the character sheet does —
so states come from `bKnown`, and dragging comes from `AbilityClass`.

## C++ you call

| Function | On | Returns |
|---|---|---|
| `Get Spellbook` | Cantrip Hotbar Component | `FCantripArtPage[]`, 18, sorted by name |
| `Get Art Page (Tag)` | " | one page |
| `Get Art Rating (Tag)` | " (also on the character) | 0–5 |
| `Assign Slot (Index, Class)` | " | drop target |
| `Raise Art (Tag, Delta)` | Character | new rating, then re-grants |

**`FCantripArtPage`**: `ArtTag`, `ArtName`, `ArtDisplayName`, `ArtRating`, `KnownCount`,
`ArtSymbol`, `Entries[]`.

---

## 1. Component setup

Player Controller BP → **CantripHotbar** → Details:

| Property | Value |
|---|---|
| Cantrip Table | `DT_Cantrips` |
| Art Symbol Path | `/Game/_Custom/Art/Symbols/Arts` |
| Art Symbol Pattern | `T_ArtSymbol_{Art}` |
| Level Symbol Pattern | `T_ArtSymbol_{Art}_L{Level}` |

Also on `BP_ThirdPersonCharacter`: fill **Cantrip Catalogue** with all 90 cantrip ability
classes. `SyncCantripsToRatings` reads each class's own row for its Art and level, so the
list needs to be complete but not ordered or grouped.

---

## 2. `WBP_ArtTab`

### Hierarchy

```
SizeBox (50 × 50)
└── Button  Btn_Tab
    └── Image  Img_Symbol        (40 × 40, Slot padding 5)
```

No card in here — see §3 for why.

### Details

**Btn_Tab → Style**, all three states Draw As **Rounded Box**, radii `6`, Fixed Radius:

| State | Tint |
|---|---|
| Normal | `1,1,1` α `0.04` |
| Hovered | `0.47,0.84,0.88` α `0.18` |
| Pressed | `0.47,0.84,0.88` α `0.30` |

### Variables

| Name | Type | Flags |
|---|---|---|
| `Page` | Cantrip Art Page | Instance Editable, Expose on Spawn |
| `TabIndex` | Integer | Instance Editable, Expose on Spawn |
| `bSelected` | Boolean | — |

### Dispatchers

Three, on this widget:

- `OnArtClicked` — input `ArtTag` (Gameplay Tag)
- `OnArtHovered` — inputs `Page` (Cantrip Art Page), `Tab` (User Widget, *this*)
- `OnArtUnhovered` — no inputs

### Event Construct

```
Break Cantrip Art Page (Page)
 → Art Symbol → Load Asset Blocking → Cast to Texture2D
   → Make Brush from Texture → Img_Symbol : Set Brush
 → Img_Symbol : Set Color and Opacity
      Art Rating > 0  →  (1,1,1,1)
      else            →  (1,1,1,0.34)
```

An Art she has no dots in still shows — dimmed. It is the thing she might spend on next.

### Events on Btn_Tab

- **On Clicked** → `Call OnArtClicked (Page.ArtTag)`
- **On Hovered** → `Call OnArtHovered (Page, Self)`
- **On Unhovered** → `Call OnArtUnhovered`

Selected state: add a `Set Selected (bool)` function that swaps the button style to a
cyan-bordered variant. The spellbook calls it when the page changes.

---

## 3. `WBP_ArtCard` — the hover card

**One card, owned by the spellbook — not one per tab.** A card living inside a tab is a
sibling of the other tabs, and UMG draws siblings in order, so the card on tab 3 renders
*underneath* tabs 4–18. One card on a canvas above the rail has no z-order problem and no
eighteen-fold duplication.

### Hierarchy

```
SizeBox (Width Override 244)
└── Border  Bdr_Card
    └── VerticalBox
        ├── TextBlock  Txt_Art          (Cormorant Garamond 21)
        ├── TextBlock  Txt_Blurb        (12, Auto Wrap ✅)
        └── HorizontalBox               (top border, padding-top 3)
            ├── HorizontalBox  Box_Dots
            └── TextBlock  Txt_Progress ("3 of 5 cantrips")
```

**Bdr_Card**: Draw As Rounded Box, radii `6`, Brush Color `#0d1220`, Padding `13,15`.

### Variables

`Page` (Cantrip Art Page), `Blurbs` (**Map**: Name → Text, Instance Editable).

Fill `Blurbs` with one line per Art, keyed by `ArtName` (`Pyretics`, `DragonsIre`, …). The
DataTable describes cantrips, not Arts, so this is the one piece of copy with nowhere else
to live. Drafts are in the mockup.

### `Refresh`

```
Break Cantrip Art Page
 → Txt_Art      : Set Text = Art Display Name
 → Blurbs : Find (Art Name) → Txt_Blurb : Set Text
 → Txt_Progress : Set Text = Format Text "{0} of 5 cantrips" ← Art Rating
 → Box_Dots : Clear Children
   For Loop 0..4
     → Create Widget (WBP_RatingDot), Filled = (Index < Art Rating)
     → Box_Dots : Add Child
```

`WBP_RatingDot` is a 9×9 SizeBox holding an Image with a circular brush — filled cyan, or
outline only. Ten lines of work and it gets reused on the character sheet.

---

## 4. `WBP_CantripEntry`

### Hierarchy

```
Border  Bdr_Row
└── HorizontalBox
    ├── SizeBox (56×56) → Image  Img_Level
    ├── VerticalBox                         (Size: Fill)
    │   ├── TextBlock  Txt_Name             (Cormorant Garamond 20)
    │   └── TextBlock  Txt_Desc             (12.5, Auto Wrap ✅)
    └── NamedSlot / Overlay  Slot_Right     (badge or chip)
```

**Bdr_Row**: Draw As Rounded Box, radii `5`, Padding `12,9`.

### `Refresh`

```
Break Cantrip Book Entry → Break Cantrip Slot Display

Img_Level : Set Brush ← Level Symbol → Load Asset Blocking → Cast to Texture2D
                        → Make Brush from Texture
Txt_Name  : Set Text  ← Display Name
Txt_Desc  : Set Text  ← Description
```

Then the three states:

| | Row brush | Row border | Icon opacity | Name | Right |
|---|---|---|---|---|---|
| `bKnown` | `1,1,1` α .05 | cyan α .16 | 1.0 | `#e9e6dd` | key badge, or "Known" chip |
| `bNextToLearn` | cyan α .07 | cyan α .42 | 0.72 | `#e9e6dd` | cyan chip `Art {level}` |
| else | transparent | none | 0.18 | `#4d5361` | grey chip `Art {level}` + lock |

The locked chip names the **rating** that opens it, not the level — same number the player
spends a dot on, so the row tells them the price rather than restating its own position.

### Drag to the hotbar

**On Mouse Button Down** (override):
```
Is Mouse Button Down (Left)  AND  Display.AbilityClass  Is Valid
 → Detect Drag if Pressed (Left)
```

**On Drag Detected** (override):
```
Create Widget (WBP_CantripEntry) with the same Entry   → drag visual
Create Drag & Drop Operation
   Default Drag Visual = that widget
   Payload             = Display.AbilityClass
   Pivot               = Mouse Down
```

Gate on **AbilityClass valid**, not `bKnown` — that is the flag that says she can actually
cast it. Dragging anything else only earns
`HOTBAR: refused … not granted to this character` in the log.

On `WBP_HotbarSlot`, override **On Drop**:
```
Operation → Payload → Cast to Class → Hotbar : Assign Slot (SlotIndex, Class)
```

`Assign Slot` moves rather than copies and broadcasts, so bar and book both redraw.

---

## 5. `WBP_Spellbook`

### Hierarchy

```
Overlay
├── Border  Bdr_Book                         (900 × 560, Rounded Box, radii 8)
│   └── HorizontalBox
│       ├── SizeBox (Width 130)              ← the rail
│       │   └── VerticalBox
│       │       ├── TextBlock "ARTS"
│       │       └── UniformGridPanel  Grid_Arts
│       └── VerticalBox                      ← the page
│           ├── HorizontalBox  (title / rating)
│           └── ScrollBox  Scr_Entries
└── CanvasPanel  Canvas_Overlay              ← the hover card, above everything
    └── WBP_ArtCard  Card                    (Visibility: Collapsed)
```

The **Overlay** is what puts `Canvas_Overlay` on top of the panel. Set the canvas's
**Visibility = Self Hit Test Invisible** so it never eats clicks meant for the rail.

### Variables

`Hotbar` (Cantrip Hotbar Component), `CurrentArt` (Gameplay Tag), `Tabs` (array of
WBP_ArtTab).

### Event Construct

```
Get Owning Player → Cast To <YourPlayerController>
 → drag off the "As" pin → CantripHotbar        → SET Hotbar
 → Bind Event to On Hotbar Changed → custom event "Rebuild"
 → Rebuild
```

### `Rebuild`

```
Grid_Arts : Clear Children ; Tabs : Clear
Hotbar → Get Spellbook → For Each Loop (with index)
   → Create Widget (WBP_ArtTab): Page = Element, TabIndex = Index
   → Bind Event to OnArtClicked    → ShowArt
   → Bind Event to OnArtHovered    → ShowCard
   → Bind Event to OnArtUnhovered  → HideCard
   → Grid_Arts : Add Child as Uniform Grid Slot
        Row    = Index / 2      (integer divide)
        Column = Index % 2
   → Tabs : Add
Branch (CurrentArt valid) — false → CurrentArt = first page's ArtTag
 → ShowArt (CurrentArt)
```

**Grid_Arts**: Slot Padding `2.5`. Eighteen tabs at 50px in nine rows is 495px plus
padding — it fits 560 without scrolling, which is the whole reason for two columns.

### `ShowArt (ArtTag)`

```
SET CurrentArt
Hotbar → Get Art Page (ArtTag) → Break
 → Txt_ArtTitle  : Set Text = Art Display Name
 → rating dots + "{Art Rating} of 5 cantrips"
 → Scr_Entries : Clear Children
   For Each (Entries) → Create Widget (WBP_CantripEntry) → Add Child
 → For Each (Tabs) → Set Selected (its Page.ArtTag == ArtTag)
```

### `ShowCard (Page, Tab)` — positioning

```
Card : SET Page = Page → Card : Refresh
Card : Set Visibility (Hit Test Invisible)

Get Cached Geometry (Tab)            → Local To Absolute (Local = (56, -6))
Get Cached Geometry (Canvas_Overlay) → Absolute To Local  (the result above)
 → Card : Slot as Canvas Slot : Set Position
```

Three nodes, and correct wherever the panel sits on screen — no hardcoded offsets to
re-tune when the rail moves. `(56, -6)` places the card just right of the 50px tab and
nudged up so its first line aligns with the symbol.

### `HideCard`

```
Card : Set Visibility (Collapsed)
```

---

## 6. Opening it

Add `WBP_Spellbook` to `WBP_HUD`'s Canvas Panel, anchored centre, **Visibility =
Collapsed**. Reuse the **I** flip-flop that already toggles the cursor:

```
A: Spellbook : Set Visibility (Visible)
   Set Input Mode Game And UI (focus = HUDReference) ; Show Mouse Cursor = true
B: Spellbook : Set Visibility (Collapsed)
   Set Input Mode Game Only ; Show Mouse Cursor = false
```

**Collapsed**, not Hidden — a Hidden widget still takes layout space and still costs a
prepass every frame.

To kill the hotbar keys while the book is open, push a second Input Mapping Context
(`IMC_Menu`, higher priority, no hotbar keys) on open and remove it on close.

---

## Checks

| Symptom | Cause |
|---|---|
| Book empty, `SPELLBOOK: no CantripTable set` in the log | Cantrip Table unset on the component |
| Book empty, no log line | Rebuild never bound, or `Get Spellbook` not called |
| Every row locked | Art ratings read 0 — `GE_StartingTraits` not applied |
| Rows show as known but won't drag | `Cantrip Catalogue` unfilled, so nothing was granted — `AbilityClass` is null |
| Tabs draw, no symbols | Art Symbol Path wrong, or the roundels aren't imported |
| Symbols never show dots | Level Symbol Pattern must be `T_ArtSymbol_{Art}_L{Level}` |
| Card appears behind tabs | Card is inside the tab widget — move it to the overlay canvas (§3) |
| Card blocks clicks on the rail | Canvas_Overlay needs **Self Hit Test Invisible** |
| Card sticks after the mouse leaves | `On Unhovered` not bound, or a gap between tab and card breaks the hover |
