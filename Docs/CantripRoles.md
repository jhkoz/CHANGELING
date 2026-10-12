# Cantrip Roles — what each of the ninety is FOR

A second axis over the same ninety cantrips. `Kind` (Instant / Sustained / Summon) decides
the ability's **shape** — how long it lasts, which C++ class the Blueprint derives from.
This decides what the player reaches for it **for**.

They are independent, and both are needed. Sorted by Art alone, "what do I have that stops
someone running" has no answer short of reading ninety descriptions.

## The roles

Derived from what is actually in C20, not borrowed from a class-based RPG — which is why
Binding and Divination are here and a tank/healer/dps triad is not. Ten of the ninety
return information and change nothing. Four bind a promise the Dreaming itself enforces.
Neither has anywhere to sit in the usual taxonomy.

| Role | Primary | Also | Meaning |
|---|---|---|---|
| **Control** | 14 | 7 | Takes away the target's choices -- held, frozen, compelled, terrified. |
| **Divination** | 10 | 1 | Returns information and changes nothing. |
| **Damage** | 9 | 4 | Harms directly. The only role that reduces health on its own. |
| **Mobility** | 8 | 1 | Moves a body through space that would otherwise stop it. |
| **Utility** | 8 | 5 | Acts on the world rather than on a character: light, weather, growth, moving objects. |
| **Protection** | 7 | 0 | Turns harm aside. Separate from Buff because it answers an attack rather than improving a capability. |
| **Transformation** | 7 | 0 | Rewrites the form of a creature or thing. |
| **Buff** | 6 | 1 | Makes the caster or an ally better at something. |
| **Creation** | 5 | 2 | Brings matter, light or a creature into being. |
| **Debuff** | 4 | 7 | Leaves the target able to act, but worse at it. |
| **Healing** | 4 | 1 | Restores what was lost -- health, life, purity. |
| **Illusion** | 4 | 2 | Lies to the senses. Distinct from Control: it does not stop you, it misleads you. |
| **Binding** | 4 | 5 | Oaths, true names and geasa -- the Dreaming enforcing a promise. No other game's taxonomy has this, and Changeling cannot do without it. |

Thirty-six carry a secondary role. That is deliberate rather than indecision: Phoenix Song
destroys **and** resurrects, and flattening it to either alone loses the cantrip.

## What the shape of it says

**Control is the largest role at 14, and twelve of those are Sustained.** Changeling's
answer to a fight is mostly to take away the other side's choices for a while, not to
reduce their health — only nine cantrips deal damage as their primary purpose. If combat
is built expecting a damage curve, most of the spell list will have nothing to say.

**Divination is second at 10, and returns nothing a combat system can use.** These want
somewhere to land that is not a health bar — a clue, a marked location, a line of
dialogue. Without that, a fifth of Soothsay, Naming and Chronos is unimplementable.

**Every Creation cantrip except one is a Summon**, which is the cross-check that the two
axes agree where they should.

**Wayfare is five-for-five Mobility and Metamorphosis five-for-five Transformation.** Some
Arts are a single idea at five strengths; Pyretics spans Damage, Utility and Healing. Worth
knowing before designing an Art's progression as if it were uniform.

## The table


### Autumn

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Creeping Shadows | **Illusion** | Control | Sustained |
| 2 | Autumn Eyes | **Divination** | — | Sustained |
| 3 | The Poisoned Apple | **Damage** | Debuff | Instant |
| 4 | The Withering | **Debuff** | Damage | Sustained |
| 5 | Shivers | **Debuff** | Control | Sustained |

### Chicanery

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Trick of the Light | **Illusion** | — | Sustained |
| 2 | Veiled Eyes | **Illusion** | — | Sustained |
| 3 | Dream Logic | **Control** | Debuff | Sustained |
| 4 | Veiled Mind | **Debuff** | Control | Sustained |
| 5 | Lost in the Mists | **Control** | — | Sustained |

### Chronos

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Backward Glance | **Divination** | — | Sustained |
| 2 | Effect and Cause | **Control** | Debuff | Sustained |
| 3 | Set in Stone | **Control** | — | Sustained |
| 4 | Déjà Vu | **Control** | — | Sustained |
| 5 | Time Dilation | **Control** | — | Sustained |

### Contract

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Done Deal | **Binding** | — | Instant |
| 2 | Liar’s Bell | **Divination** | Binding | Instant |
| 3 | Castigate | **Damage** | Binding | Instant |
| 4 | Casual Contract | **Binding** | — | Instant |
| 5 | Sanctified Words | **Buff** | Binding | Instant |

### DragonsIre

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Burning Thew | **Buff** | — | Sustained |
| 2 | Confounding Coils | **Protection** | — | Sustained |
| 3 | Dragonscales | **Protection** | — | Sustained |
| 4 | Holly-Strike | **Damage** | — | Instant |
| 5 | Tripping the Ire | **Damage** | — | Sustained |

### Legerdemain

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Ensnare | **Control** | — | Sustained |
| 2 | Mooch | **Utility** | — | Instant |
| 3 | Effigy | **Creation** | Illusion | Summon |
| 4 | Gimmix | **Utility** | — | Sustained |
| 5 | Smoke and Mirrors | **Illusion** | Creation | Sustained |

### Metamorphosis

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Sparrows and Nightingales | **Transformation** | — | Sustained |
| 2 | Worms and Giants | **Transformation** | — | Sustained |
| 3 | Thousandskins | **Transformation** | — | Sustained |
| 4 | Beastskin | **Transformation** | — | Sustained |
| 5 | Chimeric Exultation | **Transformation** | — | Sustained |

### Naming

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Between the Lines | **Divination** | — | Instant |
| 2 | Nickname | **Binding** | — | Sustained |
| 3 | Saining | **Divination** | — | Instant |
| 4 | Runic Imprint | **Buff** | Binding | Sustained |
| 5 | Reweaving | **Transformation** | Binding | Instant |

### Oneiromancy

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Dream Walk | **Mobility** | Divination | Instant |
| 2 | Dream Craft | **Utility** | Illusion | Sustained |
| 3 | Dream Portal | **Mobility** | — | Sustained |
| 4 | Manifest | **Creation** | — | Summon |
| 5 | Dream Puppet | **Control** | — | Instant |

### Primal

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Willow Whisper | **Divination** | Utility | Sustained |
| 2 | Eldritch Prime | **Creation** | — | Summon |
| 3 | Oakenshield | **Protection** | — | Sustained |
| 4 | Elder-Form | **Transformation** | Buff | Sustained |
| 5 | Dance of the Five Kings | **Utility** | Damage | Instant |

### Pyretics

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Kindle | **Damage** | Utility | Sustained |
| 2 | Illuminate | **Utility** | — | Sustained |
| 3 | Purify | **Healing** | Utility | Instant |
| 4 | Engulf | **Damage** | — | Sustained |
| 5 | Phoenix Song | **Damage** | Healing | Sustained |

### Skycraft

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Howling Gale | **Utility** | Control | Sustained |
| 2 | Electric Gremlins | **Debuff** | Utility | Summon |
| 3 | Hurricane Speed | **Mobility** | — | Sustained |
| 4 | Storm Shroud | **Protection** | Damage | Sustained |
| 5 | Lord of Levin | **Damage** | — | Instant |

### Soothsay

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Omen | **Divination** | — | Instant |
| 2 | Seer’s Wisp | **Divination** | Creation | Summon |
| 3 | Tattletale | **Divination** | — | Sustained |
| 4 | Augury | **Divination** | — | Instant |
| 5 | Fate Fire | **Buff** | Debuff | Sustained |

### Sovereign

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Protocol | **Control** | — | Sustained |
| 2 | Grandeur | **Buff** | Control | Sustained |
| 3 | Guest List | **Protection** | — | Sustained |
| 4 | Dictum | **Control** | — | Instant |
| 5 | Geas | **Binding** | Control | Sustained |

### Spring

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Awaken | **Utility** | — | Instant |
| 2 | Verdant Reclamation | **Utility** | Control | Sustained |
| 3 | Well of Life | **Healing** | — | Sustained |
| 4 | Faerie Ring | **Protection** | Mobility | Instant |
| 5 | Renewal | **Healing** | — | Instant |

### Summer

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Flicker-Flies | **Creation** | Utility | Summon |
| 2 | Enkindle | **Control** | Debuff | Sustained |
| 3 | Aphrodisia | **Control** | — | Sustained |
| 4 | Vesta’s Blessing | **Protection** | — | Instant |
| 5 | The Beltane Blade | **Healing** | Damage | Sustained |

### Wayfare

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Hopscotch | **Mobility** | — | Instant |
| 2 | Quicksilver | **Mobility** | — | Sustained |
| 3 | Portal Passage | **Mobility** | — | Sustained |
| 4 | Wind Runner | **Mobility** | — | Sustained |
| 5 | Flicker Flash | **Mobility** | — | Instant |

### Winter

| Lv | Cantrip | Role | Also | Kind |
|---|---|---|---|---|
| 1 | Chill | **Damage** | Debuff | Sustained |
| 2 | Hardened Heart | **Buff** | Debuff | Sustained |
| 3 | Terror of the Long Night | **Control** | — | Sustained |
| 4 | Sculpt | **Creation** | — | Sustained |
| 5 | Stasis | **Control** | — | Sustained |

## In C++

`ECantripRole` is on `FCantripSpec` as `Role` and `SecondaryRole`. Both default to
`Utility` — the honest answer for an unfilled row, rather than a claim the cantrip damages
or heals when nobody has said so. A secondary equal to the primary means "no secondary".

`CantripRoles.csv` carries the same data keyed by `RowName` for reconciling against
`DT_Cantrips` directly.
