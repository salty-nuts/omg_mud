/**************************************************************************
*  File: spells.c                                          Part of tbaMUD *
*  Usage: Implementation of "manual spells."                              *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "spells.h"
#include "handler.h"
#include "db.h"
#include "constants.h"
#include "interpreter.h"
#include "dg_scripts.h"
#include "act.h"
#include "fight.h"
#include "house.h"
#include "screen.h"

/* this function takes a real number for a room and returns:
   FALSE - mortals shouldn't be able to teleport to this destination
   TRUE - mortals CAN teleport to this destination
 * accepts NULL ch data
 */
int valid_mortal_tele_dest(struct char_data *ch, room_rnum dest, bool dim_lock) {

  if (dest == NOWHERE)
    return FALSE;

/*
  if (IS_AFFECTED(ch, AFF_DIM_LOCK) && dim_lock)
    return FALSE;
*/

  /* this function needs a vnum, not rnum */
  if (ch && !House_can_enter(ch, GET_ROOM_VNUM(dest)))
    return FALSE;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(dest), ZONE_NOASTRAL))
    return FALSE;

  if (ROOM_FLAGGED(dest, ROOM_PRIVATE))
    return FALSE;

  if (ROOM_FLAGGED(dest, ROOM_DEATH))
    return FALSE;

  if (ROOM_FLAGGED(dest, ROOM_GODROOM))
    return FALSE;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(dest), ZONE_CLOSED))
    return FALSE;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(dest), ZONE_NOASTRAL))
    return FALSE;

  //passed all tests!
  return TRUE;
}
ASPELL(spell_locate_char)
{
  struct char_data *i;
  int found = 0, num = 0;

  if (ch == NULL)
    return;
  if (victim == NULL)
    return;
  if (victim == ch)
  {
    send_to_char(ch, "You were once lost, but now you are found!\r\n");
    return;
  }

  send_to_char(ch, "%s\r\n", QNRM);
  for (i = character_list; i; i = i->next)
  {
    if (is_abbrev(GET_NAME(victim), GET_NAME(i)) && CAN_SEE(ch, i) && IN_ROOM(i) != NOWHERE)
    {
      found = 1;
      send_to_char(ch, "%3d. %-25s%s - %-25s%s", ++num, GET_NAME(i), QNRM,
                   world[IN_ROOM(i)].name, QNRM);
      send_to_char(ch, "%s\r\n", QNRM);
    }
  }

  if (!found)
    send_to_char(ch, "Couldn't find any such creature.\r\n");
}
ASPELL(spell_blood_mana)
{
  if (IS_NPC(ch))
    return;
  int dam, mana;
  dam = GET_HIT(ch) / 5;
  if (GET_HIT(ch) < 500)
  {
    send_to_char(ch, "You need at least 500 HP to cast Blood Mana.\n\r");
    return;
  }
  if (dam < 100)
    dam = 100;
  else
  {
    mana = dam / 2;
    act("$n makes a blood offering unto a mortar and pestle.", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char(ch,"You sacrifice some blood in your dark mana ritual.\n\r");
    GET_HIT(ch) -= dam;
    GET_MANA(ch) += mana;
    update_pos(ch);
  }

}

/* Special spells appear below. */
ASPELL(spell_cancellation)
{
  struct affected_type *af;
  if (IS_NPC(ch))
    return;
  if (IS_NPC(victim))
    return;

  if (victim->affected)
  {
    for (af = victim->affected; af; af = af->next)
//      send_to_char(ch,"Trying to dispel %s\n\r",spell_info[af->spell].name);
      affect_remove(victim, af);
  }
  else
  {
    send_to_char(ch,"You don't seem to be able to cancel any spells.\n\r");
    return;
  }
}
ASPELL(spell_satiate)
{
  if (IS_NPC(victim))
    return;

  act("$n is glowing with a bright light.", true, victim, 0, 0, TO_ROOM);
  act("You are full.", true, victim, 0,0,TO_CHAR);

	if (GET_COND(victim, HUNGER) > - 1)
		GET_COND(victim, HUNGER) = 24;
	if (GET_COND(victim, THIRST) > -1)
		GET_COND(victim, THIRST) = 24;
	if (GET_COND(victim, DRUNK) > -1)
		GET_COND(victim, DRUNK) = 0;

	update_pos(victim);
}

ASPELL(spell_create_water)
{
  int water;

  if (ch == NULL || obj == NULL)
    return;
  /* level = MAX(MIN(level, LVL_IMPL), 1);	 - not used */

  if (GET_OBJ_TYPE(obj) == ITEM_DRINKCON) {
    if ((GET_OBJ_VAL(obj, 2) != LIQ_WATER) && (GET_OBJ_VAL(obj, 1) != 0)) {
      name_from_drinkcon(obj);
      GET_OBJ_VAL(obj, 2) = LIQ_SLIME;
      name_to_drinkcon(obj, LIQ_SLIME);
    } else {
      water = MAX(GET_OBJ_VAL(obj, 0) - GET_OBJ_VAL(obj, 1), 0);
      if (water > 0) {
	if (GET_OBJ_VAL(obj, 1) >= 0)
	  name_from_drinkcon(obj);
	GET_OBJ_VAL(obj, 2) = LIQ_WATER;
	GET_OBJ_VAL(obj, 1) += water;
	name_to_drinkcon(obj, LIQ_WATER);
	weight_change_object(obj, water);
	act("$p is filled.", FALSE, ch, obj, 0, TO_CHAR);
      }
    }
  }
}

ASPELL(spell_recall)
{
  if (victim == NULL || IS_NPC(victim))
    return;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_NORECALL))
  {
    send_to_char(ch, "You cannot recall from this room!");
    return;
  }

  act("$n disappears.", TRUE, victim, 0, 0, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, r_mortal_start_room);
  act("$n appears in the middle of the room.", TRUE, victim, 0, 0, TO_ROOM);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
}

ASPELL(spell_teleport)
{
  room_rnum to_room;

  if (victim == NULL || IS_NPC(victim))
    return;

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_NOTELEPORT))
  {
    send_to_char(ch, "Your teleport spell fails in this room!");
    return;
  }

  do {
    to_room = rand_number(0, top_of_world);
  } while (ROOM_FLAGGED(to_room, ROOM_PRIVATE) || ROOM_FLAGGED(to_room, ROOM_DEATH) ||
           ROOM_FLAGGED(to_room, ROOM_GODROOM) || ZONE_FLAGGED(GET_ROOM_ZONE(to_room), ZONE_CLOSED) ||
           ROOM_FLAGGED(to_room, ROOM_NOTELEPORT) ||
           ZONE_FLAGGED(GET_ROOM_ZONE(to_room), ZONE_NOASTRAL));

  act("$n slowly fades out of existence and is gone.",
      FALSE, victim, 0, 0, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, to_room);
  act("$n slowly fades into existence.", FALSE, victim, 0, 0, TO_ROOM);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
}

#define SUMMON_FAIL "You failed.\r\n"
ASPELL(spell_summon)
{
  if (ch == NULL || victim == NULL)
    return;

  if (GET_REAL_LEVEL(victim) > MIN(LVL_IMMORT - 1, level + 3)) {
    send_to_char(ch, "%s", SUMMON_FAIL);
    return;
  }

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(victim)), ZONE_NOASTRAL) ||
      ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(ch)), ZONE_NOASTRAL)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_NOSUMMON))
  {
    send_to_char(ch, "Summoning is forbidden in this room!");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(victim), ROOM_NOTELEPORT))
  {
    send_to_char(ch, "Your summon cannot seem to reach into %s room.", HSHR(victim));
    return;
  }

  if (!CONFIG_PK_ALLOWED) {
    if (MOB_FLAGGED(victim, MOB_AGGRESSIVE)) {
      act("As the words escape your lips and $N travels\r\n"
	  "through time and space towards you, you realize that $E is\r\n"
	  "aggressive and might harm you, so you wisely send $M back.",
	  FALSE, ch, 0, victim, TO_CHAR);
      return;
    }
    if (!IS_NPC(victim) && !PRF_FLAGGED(victim, PRF_SUMMONABLE) &&
	!PLR_FLAGGED(victim, PLR_KILLER)) {
      send_to_char(victim, "%s just tried to summon you to: %s.\r\n"
	      "This failed because you have summon protection on.\r\n"
	      "Type NOSUMMON to allow other players to summon you.\r\n",
	      GET_NAME(ch), world[IN_ROOM(ch)].name);

      send_to_char(ch, "You failed because %s has summon protection on.\r\n", GET_NAME(victim));
      mudlog(BRF, MAX(LVL_IMMORT, MAX(GET_INVIS_LEV(ch), GET_INVIS_LEV(victim))), TRUE, 
        "%s failed summoning %s to %s.", GET_NAME(ch), GET_NAME(victim), world[IN_ROOM(ch)].name);
      return;
    }
  }

  if (MOB_FLAGGED(victim, MOB_NOSUMMON) ||
      (IS_NPC(victim) && mag_savingthrow(victim, SAVING_SPELL, 0))) {
    send_to_char(ch, "%s", SUMMON_FAIL);
    return;
  }

  act("$n disappears suddenly.", TRUE, victim, 0, 0, TO_ROOM);

  char_from_room(victim);
  char_to_room(victim, IN_ROOM(ch));

  act("$n arrives suddenly.", TRUE, victim, 0, 0, TO_ROOM);
  act("$n has summoned you!", FALSE, ch, 0, victim, TO_VICT);
  look_at_room(victim, 0);
  entry_memory_mtrigger(victim);
  greet_mtrigger(victim, -1);
  greet_memory_mtrigger(victim);
}

/* Used by the locate object spell to check the alias list on objects */
static int isname_obj(char *search, char *list)
{
  char *found_in_list; /* But could be something like 'ring' in 'shimmering.' */
  char searchname[512];
  char namelist[MAX_STRING_LENGTH];
  int found_pos = -1;
  int found_name=0; /* found the name we're looking for */
  int match = 1;
  int i;

  /* Force to lowercase for string comparisons */
  sprintf(searchname, "%s", search);
  for (i = 0; searchname[i]; i++)
    searchname[i] = LOWER(searchname[i]);

  sprintf(namelist, "%s", list);
  for (i = 0; namelist[i]; i++)
    namelist[i] = LOWER(namelist[i]);

  /* see if searchname exists any place within namelist */
  found_in_list = strstr(namelist, searchname);
  if (!found_in_list) {
    return 0;
  }

  /* Found the name in the list, now see if it's a valid hit. The following
   * avoids substrings (like ring in shimmering) is it at beginning of
   * namelist? */
  for (i = 0; searchname[i]; i++)
    if (searchname[i] != namelist[i])
      match = 0;

  if (match) /* It was found at the start of the namelist string. */
    found_name = 1;
  else { /* It is embedded inside namelist. Is it preceded by a space? */
    found_pos = found_in_list - namelist;
    if (namelist[found_pos-1] == ' ')
      found_name = 1;
  }

  if (found_name)
    return 1;
  else
    return 0;
}

ASPELL(spell_locate_object)
{
  struct obj_data *i;
  char name[MAX_INPUT_LENGTH];
  char obj_name[MAX_STRING_LENGTH];
  int j;

  if (!obj) {
    send_to_char(ch, "You sense nothing.\r\n");
    return;
  }

  /*  added a global var to catch 2nd arg. */
  sprintf(name, "%s", cast_arg2);

  j = GET_INT(ch) * 4;  /* # items to show = character's int*/

  for (i = object_list; i && (j > 0); i = i->next) {
//		if (IN_ROOM(i) == NOWHERE)
//      continue;
    if (!isname_obj(name, i->name))
      continue;

	sprintf(obj_name,"%c%s", UPPER(*i->short_description), i->short_description + 1);

    if (i->carried_by)
      send_to_char(ch, " %s is being carried by %s.\r\n", obj_name, PERS(i->carried_by, ch));
    else if (IN_ROOM(i) != NOWHERE)
      send_to_char(ch, " %s is in %s.\r\n", obj_name, world[IN_ROOM(i)].name);
    else if (i->in_obj)
      send_to_char(ch, " %s is in %s.\r\n", obj_name, i->in_obj->short_description);
    else if (i->worn_by)
      send_to_char(ch, " %s is being worn by %s.\r\n", obj_name, PERS(i->worn_by, ch));
//    else
//	  continue;
  //    send_to_char(ch, "'s location is uncertain.\r\n");

    j--;
  }
}

ASPELL(spell_charm)
{
  struct affected_type af;

  if (victim == NULL || ch == NULL)
    return;

  if (victim == ch)
    send_to_char(ch, "You like yourself even better!\r\n");
  else if (!IS_NPC(victim) && !PRF_FLAGGED(victim, PRF_SUMMONABLE))
    send_to_char(ch, "You fail because SUMMON protection is on!\r\n");
  else if (AFF_FLAGGED(victim, AFF_SANCTUARY))
    send_to_char(ch, "Your victim is protected by sanctuary!\r\n");
  else if (MOB_FLAGGED(victim, MOB_NOCHARM))
    send_to_char(ch, "Your victim resists!\r\n");
  else if (AFF_FLAGGED(ch, AFF_CHARM))
    send_to_char(ch, "You can't have any followers of your own!\r\n");
  else if (AFF_FLAGGED(victim, AFF_CHARM) || level < GET_REAL_LEVEL(victim))
    send_to_char(ch, "You fail.\r\n");
  /* player charming another player - no legal reason for this */
  else if (!CONFIG_PK_ALLOWED && !IS_NPC(victim))
    send_to_char(ch, "You fail - shouldn't be doing it anyway.\r\n");
  else if (circle_follow(victim, ch))
    send_to_char(ch, "Sorry, following in circles is not allowed.\r\n");
  else if (mag_savingthrow(victim, SAVING_PARA, 0))
    send_to_char(ch, "Your victim resists!\r\n");
  else {
    if (victim->master)
      stop_follower(victim);

    add_follower(victim, ch);

    new_affect(&af);
    af.spell = SPELL_CHARM;
    af.duration = 24 * 2;
    if (GET_INT(ch))
      af.duration *= GET_INT(ch);
    if (GET_INT(victim))
      af.duration /= GET_INT(victim);
    SET_BIT_AR(af.bitvector, AFF_CHARM);
    affect_to_char(victim, &af);

    act("Isn't $n just such a nice fellow?", FALSE, ch, 0, victim, TO_VICT);
    if (IS_NPC(victim))
      REMOVE_BIT_AR(MOB_FLAGS(victim), MOB_SPEC);
  }
}

ASPELL(spell_group_summon) {
  struct char_data *tch = NULL;

  if (ch == NULL)
    return;

  if (!GROUP(ch))
    return;

  if (!valid_mortal_tele_dest(ch, IN_ROOM(ch), TRUE)) {
    send_to_char(ch, "A bright flash prevents your spell from working!");
    return;
  }

  while ((tch = (struct char_data *) simple_list(GROUP(ch)->members)) !=
          NULL) {

    if (ch == tch)
      continue;

    if (MOB_FLAGGED(tch, MOB_NOSUMMON))
      continue;

    if (IN_ROOM(tch) == IN_ROOM(ch))
      continue;

    if (!valid_mortal_tele_dest(tch, IN_ROOM(tch), TRUE))
      continue;

    act("$n disappears suddenly.", TRUE, tch, 0, 0, TO_ROOM);

    char_from_room(tch);
    char_to_room(tch, IN_ROOM(ch));

    act("$n arrives suddenly.", TRUE, tch, 0, 0, TO_ROOM);
    act("$n has summoned you!", FALSE, ch, 0, tch, TO_VICT);
    look_at_room(tch, 0);
    entry_memory_mtrigger(tch);
    greet_mtrigger(tch, -1);
    greet_memory_mtrigger(tch);
  }
}


ASPELL(spell_identify)
{
  int i, found;
  size_t len;
  char buf[MAX_STRING_LENGTH];

  if (obj) {
    char bitbuf[MAX_STRING_LENGTH];

    sprinttype(GET_OBJ_TYPE(obj), item_types, bitbuf, sizeof(bitbuf));
    send_to_char(ch, "You feel informed:\r\nObject '%s', Item type: %s, Item owner: %s\r\n", obj->short_description, bitbuf, obj->owner);
    if (GET_OBJ_AFFECT(obj)) {
      sprintbitarray(GET_OBJ_AFFECT(obj), affected_bits, AF_ARRAY_MAX, bitbuf);
      send_to_char(ch, "Item will give you following abilities:  %s\r\n", bitbuf);
    }

    sprintbitarray(GET_OBJ_EXTRA(obj), extra_bits, EF_ARRAY_MAX, bitbuf);
    send_to_char(ch, "Item is: %s\r\n", bitbuf);

    send_to_char(ch, "Weight: %d, Value: %d, Rent: %d, Min. level: %d\r\n",
                     GET_OBJ_WEIGHT(obj), GET_OBJ_COST(obj), GET_OBJ_RENT(obj), GET_OBJ_LEVEL(obj));

    switch (GET_OBJ_TYPE(obj)) {
    case ITEM_SCROLL:
    case ITEM_POTION:
      len = i = 0;

      if (GET_OBJ_VAL(obj, 1) >= 1) {
	i = snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 1)));
        if (i >= 0)
          len += i;
      }

      if (GET_OBJ_VAL(obj, 2) >= 1 && len < sizeof(bitbuf)) {
	i = snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 2)));
        if (i >= 0)
          len += i;
      }

      if (GET_OBJ_VAL(obj, 3) >= 1 && len < sizeof(bitbuf)) {
	snprintf(bitbuf + len, sizeof(bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(obj, 3)));
      }

      send_to_char(ch, "This %s casts: %s\r\n", item_types[(int) GET_OBJ_TYPE(obj)], bitbuf);
      break;
    case ITEM_WAND:
    case ITEM_STAFF:
      send_to_char(ch, "This %s casts: %s\r\nIt has %d maximum charge%s and %d remaining.\r\n",
		item_types[(int) GET_OBJ_TYPE(obj)], skill_name(GET_OBJ_VAL(obj, 3)),
		GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 1) == 1 ? "" : "s", GET_OBJ_VAL(obj, 2));
      break;
    case ITEM_WEAPON:
      send_to_char(ch, "Damage Dice is '%dD%d' for an average per-round damage of %.1f.\r\n",
		GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 2), ((GET_OBJ_VAL(obj, 2) + 1) / 2.0) * GET_OBJ_VAL(obj, 1));
      break;
    case ITEM_ARMOR:
      send_to_char(ch, "AC-apply is %d\r\n", GET_OBJ_VAL(obj, 0));
      sprintbitarray(GET_OBJ_WEAR(obj), wear_bits, TW_ARRAY_MAX, buf);
      send_to_char(ch, "Can be worn on: %s\r\n", buf);
      break;
    case ITEM_TOKEN:
	send_to_char(ch,"Questpoints: %d\n\r",GET_OBJ_VAL(obj, 0));
	break;
    }
    found = FALSE;
    for (i = 0; i < MAX_OBJ_AFFECT; i++) {
      if ((obj->affected[i].location != APPLY_NONE) &&
	  (obj->affected[i].modifier != 0)) {
	if (!found) {
	  send_to_char(ch, "Can affect you as :\r\n");
	  found = TRUE;
	}
	sprinttype(obj->affected[i].location, apply_types, bitbuf, sizeof(bitbuf));
	send_to_char(ch, "   Affects: %s By %d\r\n", bitbuf, obj->affected[i].modifier);
      }
    }
  } else if (victim) {		/* victim */
    send_to_char(ch, "Name: %s\r\n", GET_NAME(victim));
    if (!IS_NPC(victim))
      send_to_char(ch, "%s is %d years, %d months, %d days and %d hours old.\r\n",
	      GET_NAME(victim), age(victim)->year, age(victim)->month,
	      age(victim)->day, age(victim)->hours);
    send_to_char(ch, "Height %d cm, Weight %d pounds\r\n", GET_HEIGHT(victim), GET_WEIGHT(victim));
    send_to_char(ch, "Level: %d, Hits: %d, Mana: %d\r\n", GET_REAL_LEVEL(victim), GET_HIT(victim), GET_MANA(victim));
    send_to_char(ch, "AC: %d, Hitroll: %d, Damroll: %d\r\n", compute_armor_class(victim), GET_HITROLL(victim), GET_DAMROLL(victim));
    send_to_char(ch, "Str: %d/%d, Int: %d, Wis: %d, Dex: %d, Con: %d, Luck: %d\r\n",
	GET_STR(victim), GET_ADD(victim), GET_INT(victim),
	GET_WIS(victim), GET_DEX(victim), GET_CON(victim), GET_LUCK(victim));
  }
}

/* Cannot use this spell on an equipped object or it will mess up the wielding
 * character's hit/dam totals. */
ASPELL(spell_enchant_weapon)
{
  int i;

  if (ch == NULL || obj == NULL)
    return;

  /* Either already enchanted or not a weapon. */
  if (GET_OBJ_TYPE(obj) != ITEM_WEAPON/* || OBJ_FLAGGED(obj, ITEM_MAGIC)*/)
    return;

  /* Make sure no other affections.*/
  for (i = 0; i < MAX_OBJ_AFFECT; i++)
    if (obj->affected[i].location != APPLY_NONE)
      obj->affected[i].modifier = level/10;


//  if (GET_SKILL(ch, SPELL_ENCHANT_WEAPON) < rand_number(1,101))
    SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_MAGIC);

  GET_OBJ_VAL(obj, 1) = level/10;
  GET_OBJ_VAL(obj, 2) = level/20;

  if (IS_GOOD(ch))
  {
//    SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_ANTI_EVIL);
    act("$p glows blue.", FALSE, ch, obj, 0, TO_CHAR);
  }
  else if (IS_EVIL(ch))
  {
//    SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_ANTI_GOOD);
    act("$p glows red.", FALSE, ch, obj, 0, TO_CHAR);
  }
  else
    act("$p glows yellow.", FALSE, ch, obj, 0, TO_CHAR);
}

ASPELL(spell_detect_poison)
{
  if (victim) {
    if (victim == ch) {
      if (AFF_FLAGGED(victim, AFF_POISON))
        send_to_char(ch, "You can sense poison in your blood.\r\n");
      else
        send_to_char(ch, "You feel healthy.\r\n");
    } else {
      if (AFF_FLAGGED(victim, AFF_POISON))
        act("You sense that $E is poisoned.", FALSE, ch, 0, victim, TO_CHAR);
      else
        act("You sense that $E is healthy.", FALSE, ch, 0, victim, TO_CHAR);
    }
  }

  if (obj) {
    switch (GET_OBJ_TYPE(obj)) {
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
    case ITEM_FOOD:
      if (GET_OBJ_VAL(obj, 3))
	act("You sense that $p has been contaminated.",FALSE,ch,obj,0,TO_CHAR);
      else
	act("You sense that $p is safe for consumption.", FALSE, ch, obj, 0,
	    TO_CHAR);
      break;
    default:
      send_to_char(ch, "You sense that it should not be consumed.\r\n");
    }
  }
}


/*
  Relocation

  Salty, 05 JAN 2019
*/
ASPELL(spell_relocate)
{
  int target;
  if (IS_NPC(victim))
  {
    send_to_char(ch,"You failed.\r\n");
    return;
  }
  // Rooms we cannot relocate from
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PRIVATE))
  {
    send_to_char(ch,"Something about this room prevents you from leaving!\r\n");
    return;
  }
  // Rooms we cannot relocate to
  if (ROOM_FLAGGED(IN_ROOM(victim), ROOM_PRIVATE) || ROOM_FLAGGED(IN_ROOM(victim), ROOM_TUNNEL) || 
      ROOM_FLAGGED(IN_ROOM(victim), ROOM_GODROOM) || ROOM_FLAGGED(IN_ROOM(victim), ROOM_NOTRACK))
  {
    send_to_char(ch, "Something prevents you from reaching that player!\r\n");
    return;
  }

  if ((GET_REAL_LEVEL(ch) + 5) < GET_REAL_LEVEL(victim))
  {
    send_to_char(ch,"You failed.\r\n");
    return;
  }

  act("$n disappears suddenly.",true,ch,0,0,TO_ROOM);

  target = victim->in_room;
  char_from_room(ch);
  char_to_room(ch, target);

  act("$n relocates to here.",true,ch,0,0,TO_ROOM);
  act("You relocate to $N!",false,ch,0,victim,TO_CHAR);
  do_look(ch,"",0,0);
}

ASPELL(spell_gateway)
{
  int target;
  if (IS_NPC(victim))
  {
    send_to_char(ch,"You failed.\r\n");
    return;
  }
  // Rooms we cannot relocate from
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PRIVATE))
  {
    send_to_char(ch,"Something prevents you from leaving the room.\r\n");
    return;
  }
  // Rooms we cannot relocate to
  if (ROOM_FLAGGED(IN_ROOM(victim), ROOM_PRIVATE) || ROOM_FLAGGED(IN_ROOM(victim), ROOM_TUNNEL) ||
      ROOM_FLAGGED(IN_ROOM(victim), ROOM_GODROOM) || ROOM_FLAGGED(IN_ROOM(victim), ROOM_NOTRACK))
  {
    send_to_char(ch, "Something prevents you from reaching that player!\r\n");
    return;
  }

  if ((GET_REAL_LEVEL(ch) + 5) < GET_REAL_LEVEL(victim))
  {
    send_to_char(ch,"You failed.\r\n");
    return;
  }

  act("$n vanises abruptly.",true,ch,0,0,TO_ROOM);

  target = victim->in_room;
  char_from_room(ch);
  char_to_room(ch, target);

  act("$n exits a gateway.",true,ch,0,0,TO_ROOM);
  act("You step into the spacetime continuum and emerge at $N!",false,ch,0,victim,TO_CHAR);
  do_look(ch,"",0,0);
}

