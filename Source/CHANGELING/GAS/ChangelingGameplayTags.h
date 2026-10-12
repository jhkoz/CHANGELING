// ChangelingGameplayTags.h
//
// Native tags, so C++ and Blueprint reference one source of truth. A tag typed by
// hand into a DataTable row is a tag that silently stops matching the day someone
// renames it; these will not compile if they drift.
//
// Art.*    the verbs of faerie magic
// Realm.*  what a cantrip may be aimed at
// State.Enchanted marks who can perceive chimerical things at all.

#pragma once

#include "NativeGameplayTags.h"

namespace ChangelingTags
{
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Autumn);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Chicanery);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Chronos);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Contract);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_DragonsIre);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Legerdemain);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Metamorphosis);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Naming);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Oneiromancy);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Primal);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Pyretics);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Skycraft);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Soothsay);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Sovereign);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Spring);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Summer);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Wayfare);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Art_Winter);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Realm_Fae);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Realm_Actor);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Realm_Nature);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Realm_Prop);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Realm_Time);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Realm_Scene);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Manifestation_Chimerical);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Manifestation_Wyrd);
	/**
	 * The WIND-UP, and only the wind-up. Blocks a second working.
	 *
	 * Dropped the moment a Sustained cantrip stops being cast and starts merely holding.
	 * The two used to be one tag, which meant a working that had already landed went on
	 * claiming both hands for its whole duration -- so a thirty-second Hopscotch left the
	 * entire spellbook dead while she bounced around unable to do anything else.
	 */
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_State_Casting);

	/**
	 * A working that has landed and is still running. Blocks nothing.
	 *
	 * Owned for the ability's whole lifetime, wind-up included, so anything that wants to
	 * know "is a cantrip in force" has one tag to ask about -- a hotbar showing a
	 * countdown, an AI deciding she looks dangerous, a dispel looking for something to
	 * end.
	 */
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_State_Held);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_State_Channelling);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_State_Tainted);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Event_EffectStart);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Event_Recovered);

	/** The moment a thrown cantrip leaves the hand. Fired by a notify on the montage,
	 *  because only the animation knows where its own apex is. */
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Event_Release);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Data_GlamourCost);

	/** How hard a detonation hit, handed to the blast effect by SetByCaller. The effect
	 *  asset decides what that number means -- damage, chimerical damage, or knockback. */
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Data_BlastMagnitude);

	/** Magnitude a role class passes to its effect: damage dealt, health restored, dice
	 *  added or taken away. One tag for all of them, because every role asks the same
	 *  question of the roll -- how much -- and the effect asset decides what it means. */
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Data_Magnitude);

	/**
	 * A SECOND number, for the handful of effects that genuinely need two.
	 *
	 * Deliberately rare. One magnitude covers every role because every role asks the roll
	 * the same question, and a cantrip reaching for a third number has almost certainly
	 * been given the wrong role. The case this exists for is a bounding cantrip, where the
	 * jump and the fall are two independent feelings that have to be tuned against each
	 * other -- one number cannot say both "leap twice as high" and "drift down slowly".
	 */
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cantrip_Data_Secondary);
	/**
	 * The jump control is being HELD -- not "is airborne", and not "jumped".
	 *
	 * Set and cleared by the character's own input, and read by any effect that should
	 * only be in force while the player is actively asking for it. Hopscotch is the
	 * reason it exists: the cantrip's duration says how long she MAY bound, and this
	 * says when she is actually doing it, so letting go of the key drops her out of the
	 * air the same instant.
	 *
	 * A tag rather than a bool because GAS can gate an effect on it directly -- see the
	 * ongoing requirement on UGE_CantripBound. Nothing has to poll, and nothing has to
	 * remember to switch anything off.
	 */
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_JumpHeld);

	/**
	 * She is running under her own effort -- moving, on the ground, not walking.
	 *
	 * Drives stamina entirely: the drain effect requires this tag and the recovery effect
	 * is blocked by it, so the two can never both run and neither needs to know about the
	 * other. Same shape as State.JumpHeld gating the bounding cantrip.
	 */
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Enchanted);
	CHANGELING_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Banality_Heavy);
}
