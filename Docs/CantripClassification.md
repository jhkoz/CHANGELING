# Cantrip Classification — all 90

Read out of C20's Arts chapter (pp. 192–240) and reconciled against `DT_Cantrips_AllArts`.
Source of truth for the `Kind` column of the DataTable, and for which C++ class each
cantrip's Blueprint derives from.

## The class tree

```
UGA_Cantrip                      abstract — the family base: pool, difficulty, hold-to-cast,
│                                Glamour, the roll, the row's effect
├── UGA_InstantCantrip           happens and is over          25 cantrips
│   └── UGA_SummonCantrip        leaves an actor behind        6 cantrips
├── UGA_SustainedCantrip         stays up for a duration      58 cantrips
│   └── UGA_ChannelledCantrip    held open by the input        0 cantrips
└── UGA_ProjectileCantrip        thrown                        1 (Engulf)
```

Every one is `Abstract, Blueprintable` — each actual cantrip is a Blueprint child, and the
`Blueprint parent` column below names which.

**Summon derives from Instant, not Sustained**, and the distinction decides the parent.
A sustained cantrip *is* the effect: end the ability and the flame goes out. A summon is
over the moment it finishes casting — what persists is an **actor**, with its own lifetime
and every right to outlive the caster's attention. So the ability ends immediately and
hands the duration to the thing it made.

`UGA_ProjectileCantrip` is a **presentation** choice, not a book category. Nothing in C20
decides it; any cantrip could be re-parented to it if you want it hurled.

## What came out

| | Count |
|---|---|
| Sustained | 59 |
| Instant | 25 |
| Summon | 6 |
| Channelled | **0** |

**No cantrip in C20 is concentration-held.** Every one is either instantaneous or runs for
a stated duration, so `UGA_ChannelledCantrip` has no user in the core book. Worth knowing
before you build anything against it.

Sustained dominating is not a quirk of the reading — C20 gives most cantrips a
successes-to-duration table, which is precisely a sustained effect whose length the roll
decides. That maps onto `GA_SustainedCantrip` directly: the roll's degree picks the
duration row.

## Confidence

- **62 of 90** classify from explicit language in the book — a duration table whose cells
  are time spans, or a sentence saying what the cantrip lasts for.
- **28** carry no duration language at all. Those are marked ⚠ and hold **my read**, not
  the book's word. Most are Instant for a good reason: an effect that happens and is over
  needs no duration. Check the ⚠ rows before committing them.

Twelve more were hand-adjudicated where the wording defeats a pattern match — Aphrodisia's
duration is split across a page header, Reweaving's `1 success…` table lists changes rather
than times, and the six Summons never say "summon" in their System line.

## The table

⚠ marks my read rather than the book's.

| Art | Lv | Cantrip | Kind | Blueprint parent | Basis |
|---|---|---|---|---|---|
| Autumn | 1 | Creeping Shadows | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Autumn | 2 | Autumn Eyes | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Autumn | 3 | The Poisoned Apple | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Autumn | 4 | The Withering | **Sustained** | `UGA_SustainedCantrip` | duration table 6/6 |
| Autumn | 5 | Shivers | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Chicanery | 1 | Trick of the Light | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Chicanery | 2 | Veiled Eyes | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Chicanery | 3 | Dream Logic | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Chicanery | 4 | Veiled Mind | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Chicanery | 5 | Lost in the Mists | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Chronos | 1 | Backward Glance | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Chronos | 2 | Effect and Cause | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Chronos | 3 | Set in Stone | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Chronos | 4 | Déjà Vu | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Chronos | 5 | Time Dilation | **Sustained** | `UGA_SustainedCantrip` | duration table 4/5 |
| Contract | 1 | Done Deal | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Contract | 2 | Liar’s Bell | **Instant** | `UGA_InstantCantrip` | permanent |
| Contract | 3 | Castigate | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Contract | 4 | Casual Contract | **Instant** | `UGA_InstantCantrip` | permanent |
| Contract | 5 | Sanctified Words | **Instant** ⚠ | `UGA_InstantCantrip` | effect table 0/3 |
| DragonsIre | 1 | Burning Thew | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| DragonsIre | 2 | Confounding Coils | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| DragonsIre | 3 | Dragonscales | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| DragonsIre | 4 | Holly-Strike | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| DragonsIre | 5 | Tripping the Ire | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Legerdemain | 1 | Ensnare | **Sustained** | `UGA_SustainedCantrip` | turns hindered |
| Legerdemain | 2 | Mooch | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Legerdemain | 3 | Effigy | **Summon** | `UGA_SummonCantrip` | creates a duplicate |
| Legerdemain | 4 | Gimmix | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Legerdemain | 5 | Smoke and Mirrors | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Metamorphosis | 1 | Sparrows and Nightingales | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Metamorphosis | 2 | Worms and Giants | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Metamorphosis | 3 | Thousandskins | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Metamorphosis | 4 | Beastskin | **Sustained** | `UGA_SustainedCantrip` | duration table 6/6 |
| Metamorphosis | 5 | Chimeric Exultation | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Naming | 1 | Between the Lines | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Naming | 2 | Nickname | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Naming | 3 | Saining | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Naming | 4 | Runic Imprint | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Naming | 5 | Reweaving | **Instant** | `UGA_InstantCantrip` | permanent rewrite, effect table not duration |
| Oneiromancy | 1 | Dream Walk | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Oneiromancy | 2 | Dream Craft | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Oneiromancy | 3 | Dream Portal | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Oneiromancy | 4 | Manifest | **Summon** | `UGA_SummonCantrip` | brings a dream-thing into the waking world |
| Oneiromancy | 5 | Dream Puppet | **Instant** ⚠ | `UGA_InstantCantrip` | effect table 0/5 |
| Primal | 1 | Willow Whisper | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Primal | 2 | Eldritch Prime | **Summon** | `UGA_SummonCantrip` | conjures elemental matter |
| Primal | 3 | Oakenshield | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Primal | 4 | Elder-Form | **Sustained** | `UGA_SustainedCantrip` | duration table 4/4 |
| Primal | 5 | Dance of the Five Kings | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Pyretics | 1 | Kindle | **Sustained** | `UGA_SustainedCantrip` | turns overheated |
| Pyretics | 2 | Illuminate | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Pyretics | 3 | Purify | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Pyretics | 4 | Engulf | **Sustained** | `UGA_ProjectileCantrip` | burns for a duration |
| Pyretics | 5 | Phoenix Song | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Skycraft | 1 | Howling Gale | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Skycraft | 2 | Electric Gremlins | **Summon** | `UGA_SummonCantrip` | creates gremlins that persist |
| Skycraft | 3 | Hurricane Speed | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Skycraft | 4 | Storm Shroud | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Skycraft | 5 | Lord of Levin | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Soothsay | 1 | Omen | **Instant** ⚠ | `UGA_InstantCantrip` | effect table 0/5 |
| Soothsay | 2 | Seer’s Wisp | **Summon** | `UGA_SummonCantrip` | creates a wisp |
| Soothsay | 3 | Tattletale | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Soothsay | 4 | Augury | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Soothsay | 5 | Fate Fire | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Sovereign | 1 | Protocol | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Sovereign | 2 | Grandeur | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Sovereign | 3 | Guest List | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Sovereign | 4 | Dictum | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Sovereign | 5 | Geas | **Sustained** | `UGA_SustainedCantrip` | remains in place for the duration |
| Spring | 1 | Awaken | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Spring | 2 | Verdant Reclamation | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Spring | 3 | Well of Life | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Spring | 4 | Faerie Ring | **Instant** ⚠ | `UGA_InstantCantrip` | effect table 0/5 |
| Spring | 5 | Renewal | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Summer | 1 | Flicker-Flies | **Summon** | `UGA_SummonCantrip` | conjures motes of light |
| Summer | 2 | Enkindle | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Summer | 3 | Aphrodisia | **Sustained** | `UGA_SustainedCantrip` | one scene (text split by page header) |
| Summer | 4 | Vesta’s Blessing | **Instant** ⚠ | `UGA_InstantCantrip` | effect table 0/5 |
| Summer | 5 | The Beltane Blade | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Wayfare | 1 | Hopscotch | **Instant** ⚠ | `UGA_InstantCantrip` | effect table 0/4 |
| Wayfare | 2 | Quicksilver | **Sustained** ⚠ | `UGA_SustainedCantrip` | no duration in C20 -- my read |
| Wayfare | 3 | Portal Passage | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Wayfare | 4 | Wind Runner | **Sustained** | `UGA_SustainedCantrip` | duration table 4/4 |
| Wayfare | 5 | Flicker Flash | **Instant** ⚠ | `UGA_InstantCantrip` | no duration in C20 -- my read |
| Winter | 1 | Chill | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Winter | 2 | Hardened Heart | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Winter | 3 | Terror of the Long Night | **Sustained** | `UGA_SustainedCantrip` | explicit duration |
| Winter | 4 | Sculpt | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |
| Winter | 5 | Stasis | **Sustained** | `UGA_SustainedCantrip` | duration table 5/5 |

## Using it

The CSV alongside this file (`CantripClassification.csv`) has the same data plus
`RowName`, which matches the DataTable keys — so `Kind` can be reconciled against
`DT_Cantrips` directly rather than retyped.

Currently the table has 89 Instant and 1 Sustained, so the `Kind` column is effectively
unset. Fixing that is worth doing before building the Blueprints: `GA_SustainedCantrip`
reads it, and a Sustained cantrip parented to `UGA_Cantrip` will resolve and vanish
instead of staying up.
