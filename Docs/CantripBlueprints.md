# Cantrip Blueprints by parent class

All ninety Blueprints exist and all ninety are referenced by `BP_ThirdPersonCharacter`.
Names below are the ones actually on disk.

`Delivery` and the row's `Kind` are properties on the Blueprint, not part of the choice of
parent. A **payload** is a second thing the cantrip does that its role class does not
cover — added under `Cantrip | Payloads` on the defaults.


## `UGA_ControlCantrip` — 14

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Chicanery | 3 | `GA_Chicanery_DreamLogic` | Sustained | *Debuff* |
| Chicanery | 5 | `GA_Chicanery_LostintheMists` | Sustained | — |
| Chronos | 2 | `GA_Chronos_EffectandCause` | Sustained | *Debuff* |
| Chronos | 3 | `GA_Chronos_SetinStone` | Sustained | — |
| Chronos | 4 | `GA_Chronos_DejaVu` | Sustained | — |
| Chronos | 5 | `GA_Chronos_TimeDilation` | Sustained | — |
| Legerdemain | 1 | `GA_Legerdemain_Ensnare` | Sustained | — |
| Oneiromancy | 5 | `GA_Oneiromancy_DreamPuppet` | Instant | — |
| Sovereign | 1 | `GA_Sovereign_Protocol` | Sustained | — |
| Sovereign | 4 | `GA_Sovereign_Dictum` | Instant | — |
| Summer | 2 | `GA_Summer_Enkindle` | Sustained | *Debuff* |
| Summer | 3 | `GA_Summer_Aphrodisia` | Sustained | — |
| Winter | 3 | `GA_Winter_TerroroftheLongNight` | Sustained | — |
| Winter | 5 | `GA_Winter_Stasis` | Sustained | — |

## `UGA_DivinationCantrip` — 10

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Autumn | 2 | `GA_Autumn_AutumnEyes` | Sustained | — |
| Chronos | 1 | `GA_Chronos_BackwardGlance` | Sustained | — |
| Contract | 2 | `GA_Contract_LiarsBell` | Instant | *Binding* |
| Naming | 1 | `GA_Naming_BetweentheLines` | Instant | — |
| Naming | 3 | `GA_Naming_Saining` | Instant | — |
| Primal | 1 | `GA_Primal_WillowWhisper` | Sustained | *Utility* |
| Soothsay | 1 | `GA_Soothsay_Omen` | Instant | — |
| Soothsay | 2 | `GA_Soothsay_SeersWisp` | Summon | *Creation* |
| Soothsay | 3 | `GA_Soothsay_Tattletale` | Sustained | — |
| Soothsay | 4 | `GA_Soothsay_Augury` | Instant | — |

## `UGA_DamageCantrip` — 9

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Autumn | 3 | `GA_Autumn_ThePoisonedApple` | Instant | *Debuff* |
| Contract | 3 | `GA_Contract_Castigate` | Instant | *Binding* |
| DragonsIre | 4 | `GA_DragonsIre_HollyStrike` | Instant | — |
| DragonsIre | 5 | `GA_DragonsIre_TrippingtheIre` | Sustained | — |
| Pyretics | 1 | `GA_Pyretics_Kindle` | Sustained | *Utility* |
| Pyretics | 4 | `GA_Pyretics_Engulf` | Sustained | — |
| Pyretics | 5 | `GA_Pyretics_PhoenixSong` | Sustained | *Healing* |
| Skycraft | 5 | `GA_Skycraft_LordofLevin` | Instant | — |
| Winter | 1 | `GA_Winter_Chill` | Sustained | *Debuff* |

## `UGA_UtilityCantrip` — 8

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Legerdemain | 2 | `GA_Legerdemain_Mooch` | Instant | — |
| Legerdemain | 4 | `GA_Legerdemain_Gimmix` | Sustained | — |
| Oneiromancy | 2 | `GA_Oneiromancy_DreamCraft` | Sustained | *Illusion* |
| Primal | 5 | `GA_Primal_DanceoftheFiveKings` | Instant | `UCantripPayload_Damage` |
| Pyretics | 2 | `GA_Pyretics_Illuminate` | Sustained | — |
| Skycraft | 1 | `GA_Skycraft_HowlingGale` | Sustained | *Control* |
| Spring | 1 | `GA_Spring_Awaken` | Instant | — |
| Spring | 2 | `GA_Spring_VerdantReclamation` | Sustained | *Control* |

## `UGA_MobilityCantrip` — 8

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Oneiromancy | 1 | `GA_Oneiromancy_DreamWalk` | Instant | *Divination* |
| Oneiromancy | 3 | `GA_Oneiromancy_DreamPortal` | Sustained | — |
| Skycraft | 3 | `GA_Skycraft_HurricaneSpeed` | Sustained | — |
| Wayfare | 1 | `GA_Wayfare_Hopscotch` | Instant | — |
| Wayfare | 2 | `GA_Wayfare_Quicksilver` | Sustained | — |
| Wayfare | 3 | `GA_Wayfare_PortalPassage` | Sustained | — |
| Wayfare | 4 | `GA_Wayfare_WindRunner` | Sustained | — |
| Wayfare | 5 | `GA_Wayfare_FlickerFlash` | Instant | — |

## `UGA_ProtectionCantrip` — 7

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| DragonsIre | 2 | `GA_DragonsIre_ConfoundingCoils` | Sustained | — |
| DragonsIre | 3 | `GA_DragonsIre_DragonScales` | Sustained | — |
| Primal | 3 | `GA_Primal_Oakenshield` | Sustained | — |
| Skycraft | 4 | `GA_Skycraft_StormShroud` | Sustained | `UCantripPayload_Damage` |
| Sovereign | 3 | `GA_Sovereign_GuestList` | Sustained | — |
| Spring | 4 | `GA_Spring_FaerieRing` | Instant | *Mobility* |
| Summer | 4 | `GA_Summer_VestasBlessing` | Instant | — |

## `UGA_TransformationCantrip` — 7

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Metamorphosis | 1 | `GA_Metamorphosis_SparrowsandNightengales` | Sustained | — |
| Metamorphosis | 2 | `GA_Metamorphosis_WormsandGiants` | Sustained | — |
| Metamorphosis | 3 | `GA_Metamorphosis_Thousandskins` | Sustained | — |
| Metamorphosis | 4 | `GA_Metamorphosis_Beastskin` | Sustained | — |
| Metamorphosis | 5 | `GA_Metamorphosis_ChimericExultation` | Sustained | — |
| Naming | 5 | `GA_Naming_Reweaving` | Instant | *Binding* |
| Primal | 4 | `GA_Primal_ElderForm` | Sustained | *Buff* |

## `UGA_BuffCantrip` — 6

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Contract | 5 | `GA_Contract_SanctifiedWords` | Instant | *Binding* |
| DragonsIre | 1 | `GA_DragonsIre_BurningThew` | Sustained | — |
| Naming | 4 | `GA_Naming_RunicImprint` | Sustained | *Binding* |
| Soothsay | 5 | `GA_Soothsay_FateFire` | Sustained | *Debuff* |
| Sovereign | 2 | `GA_Sovereign_Grandeur` | Sustained | *Control* |
| Winter | 2 | `GA_Winter_HardenedHeart` | Sustained | *Debuff* |

## `UGA_CreationCantrip` — 5

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Legerdemain | 3 | `GA_Legerdemain_Effigy` | Summon | *Illusion* |
| Oneiromancy | 4 | `GA_Oneiromancy_Manifest` | Summon | — |
| Primal | 2 | `GA_Primal_EldritchPrime` | Summon | `UCantripPayload_Damage` |
| Summer | 1 | `GA_Summer_FlickerFlies` | Summon | *Utility* |
| Winter | 4 | `GA_Winter_Sculpt` | Sustained | — |

## `UGA_IllusionCantrip` — 4

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Autumn | 1 | `GA_Autumn_CreepingShadows` | Sustained | *Control* |
| Chicanery | 1 | `GA_Chicanery_TrickoftheLight` | Sustained | — |
| Chicanery | 2 | `GA_Chicanery_VeiledEyes` | Sustained | — |
| Legerdemain | 5 | `GA_Legerdemain_SmokeandMirrors` | Sustained | *Creation* |

## `UGA_DebuffCantrip` — 4

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Autumn | 4 | `GA_Autumn_TheWithering` | Sustained | `UCantripPayload_Damage` |
| Autumn | 5 | `GA_Autumn_Shivers` | Sustained | *Control* |
| Chicanery | 4 | `GA_Chicanery_VeiledMind` | Sustained | *Control* |
| Skycraft | 2 | `GA_Skycraft_ElectricGremlins` | Summon | *Utility* |

## `UGA_BindingCantrip` — 4

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Contract | 1 | `GA_Contract_DoneDeal` | Instant | — |
| Contract | 4 | `GA_Contract_CasualContract` | Instant | — |
| Naming | 2 | `GA_Naming_Nickname` | Sustained | — |
| Sovereign | 5 | `GA_Sovereign_Geas` | Sustained | *Control* |

## `UGA_HealingCantrip` — 4

| Art | Lv | Blueprint | Kind | Payload |
|---|---|---|---|---|
| Pyretics | 3 | `GA_Pyretics_Purify` | Instant | *Utility* |
| Spring | 3 | `GA_Spring_WellofLife` | Sustained | — |
| Spring | 5 | `GA_Spring_Renewal` | Instant | — |
| Summer | 5 | `GA_Summer_TheBeltaneBlade` | Sustained | `UCantripPayload_Damage` |

## Payloads to add — five Blueprints

| Blueprint | Parent | Payload |
|---|---|---|
| `GA_Autumn_TheWithering` | `UGA_DebuffCantrip` | `UCantripPayload_Damage` |
| `GA_Primal_EldritchPrime` | `UGA_CreationCantrip` | `UCantripPayload_Damage`, **Only On Contact** |
| `GA_Primal_DanceoftheFiveKings` | `UGA_UtilityCantrip` | `UCantripPayload_Damage` |
| `GA_Skycraft_StormShroud` | `UGA_ProtectionCantrip` | `UCantripPayload_Damage` |
| `GA_Summer_TheBeltaneBlade` | `UGA_HealingCantrip` | `UCantripPayload_Damage` |

`GA_Primal_EldritchPrime` is the flamethrower: `UGA_CreationCantrip` for the conjured
stream, plus a Damage payload with **Only On Contact** ticked, so the burn component calls
`Damage Actor` per particle hit rather than the ability guessing who stood in front of the
caster at the moment of the cast.

## Which array holds them

They belong in **`Cantrip Catalogue`**, not `Default Abilities`.

`SyncCantripsToRatings` reads the catalogue and grants only the cantrips at or below each
Art rating. `DefaultAbilities` grants everything listed at possession regardless of rating
— ninety cantrips there means she can cast all ninety from the first frame, while the
spellbook, which reads the rating, still shows most of them locked.

`DefaultAbilities` should hold only abilities that are not cantrips.
