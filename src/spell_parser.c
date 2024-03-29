/**************************************************************************
*  File: spell_parser.c                                    Part of tbaMUD *
*  Usage: Top-level magic routines; outside points of entry to magic sys. *
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
#include "interpreter.h"
#include "spells.h"
#include "handler.h"
#include "comm.h"
#include "db.h"
#include "dg_scripts.h"
#include "fight.h" /* for hit() */
#include "salty.h"

#define SINFO spell_info[spellnum]

/* Global Variables definitions, used elsewhere */
struct spell_info_type spell_info[TOP_SPELL_DEFINE + 1];
char cast_arg2[MAX_INPUT_LENGTH];
const char *unused_spellname = "!UNUSED!"; /* So we can get &unused_spellname */

/* Local (File Scope) Function Prototypes */
static void say_spell(struct char_data *ch, int spellnum, struct char_data *tch, struct obj_data *tobj);
static void spello(int spl, const char *name, int max_mana, int min_mana, int mana_change, int minpos, int targets, int violent, int routines, const char *wearoff);
static int mag_manacost(struct char_data *ch, int spellnum);

/* Local (File Scope) Variables */
struct syllable
{
  const char *org;
  const char *news;
};
static struct syllable syls[] = {
    {" ", " "},
    {"ar", "abra"},
    {"ate", "i"},
    {"cau", "kada"},
    {"blind", "nose"},
    {"bur", "mosa"},
    {"cu", "judi"},
    {"de", "oculo"},
    {"dis", "mar"},
    {"ect", "kamina"},
    {"en", "uns"},
    {"gro", "cra"},
    {"light", "dies"},
    {"lo", "hi"},
    {"magi", "kari"},
    {"mon", "bar"},
    {"mor", "zak"},
    {"move", "sido"},
    {"ness", "lacri"},
    {"ning", "illa"},
    {"per", "duda"},
    {"ra", "gru"},
    {"re", "candus"},
    {"son", "sabru"},
    {"tect", "infra"},
    {"tri", "cula"},
    {"ven", "nofo"},
    {"word of", "inset"},
    {"a", "i"},
    {"b", "v"},
    {"c", "q"},
    {"d", "m"},
    {"e", "o"},
    {"f", "y"},
    {"g", "t"},
    {"h", "p"},
    {"i", "u"},
    {"j", "y"},
    {"k", "t"},
    {"l", "r"},
    {"m", "w"},
    {"n", "b"},
    {"o", "a"},
    {"p", "s"},
    {"q", "d"},
    {"r", "f"},
    {"s", "g"},
    {"t", "h"},
    {"u", "e"},
    {"v", "z"},
    {"w", "x"},
    {"x", "n"},
    {"y", "l"},
    {"z", "k"},
    {"", ""}};

static int mag_manacost(struct char_data *ch, int spellnum)
{
  if (SINFO.min_level[(int)GET_CLASS(ch)] == LVL_IMMORT)
    return MAX(SINFO.mana_max - (SINFO.mana_change * (GET_REAL_LEVEL(ch) - SINFO.min_level[(int)GET_MULTI_CLASS(ch)])), SINFO.mana_min);
  else
    return MAX(SINFO.mana_max - (SINFO.mana_change * (GET_REAL_LEVEL(ch) - SINFO.min_level[(int)GET_CLASS(ch)])), SINFO.mana_min);
}

static void say_spell(struct char_data *ch, int spellnum, struct char_data *tch,
                      struct obj_data *tobj)
{
  char lbuf[256], buf[256], buf1[256], buf2[256]; /* FIXME */
  const char *format;

  struct char_data *i;
  int j, ofs = 0;

  *buf = '\0';
  strlcpy(lbuf, skill_name(spellnum), sizeof(lbuf));

  while (lbuf[ofs])
  {
    for (j = 0; *(syls[j].org); j++)
    {
      if (!strncmp(syls[j].org, lbuf + ofs, strlen(syls[j].org)))
      {
        strcat(buf, syls[j].news); /* strcat: BAD */
        ofs += strlen(syls[j].org);
        break;
      }
    }
    /* i.e., we didn't find a match in syls[] */
    if (!*syls[j].org)
    {
      log("No entry in syllable table for substring of '%s'", lbuf);
      ofs++;
    }
  }

  if (tch != NULL && IN_ROOM(tch) == IN_ROOM(ch))
  {
    if (tch == ch)
      format = "$n closes $s eyes and utters the words, '%s'.";
    else
      format = "$n stares at $N and utters the words, '%s'.";
  }
  else if (tobj != NULL &&
           ((IN_ROOM(tobj) == IN_ROOM(ch)) || (tobj->carried_by == ch)))
    format = "$n stares at $p and utters the words, '%s'.";
  else
    format = "$n utters the words, '%s'.";

  snprintf(buf1, sizeof(buf1), format, skill_name(spellnum));
  snprintf(buf2, sizeof(buf2), format, buf);

  for (i = world[IN_ROOM(ch)].people; i; i = i->next_in_room)
  {
    if (i == ch || i == tch || !i->desc || !AWAKE(i))
      continue;
    if (GET_CLASS(ch) == GET_CLASS(i))
      perform_act(buf1, ch, tobj, tch, i);
    else
      perform_act(buf2, ch, tobj, tch, i);
  }

  if (tch != NULL && tch != ch && IN_ROOM(tch) == IN_ROOM(ch))
  {
    snprintf(buf1, sizeof(buf1), "$n stares at you and utters the words, '%s'.",
             GET_CLASS(ch) == GET_CLASS(tch) ? skill_name(spellnum) : buf);
    act(buf1, FALSE, ch, NULL, tch, TO_VICT);
  }
}

/* This function should be used anytime you are not 100% sure that you have
 * a valid spell/skill number.  A typical for() loop would not need to use
 * this because you can guarantee > 0 and <= TOP_SPELL_DEFINE. */
const char *skill_name(int num)
{
  if (num > 0 && num <= TOP_SPELL_DEFINE)
    return (spell_info[num].name);
  else if (num == -1)
    return ("UNUSED");
  else
    return ("UNDEFINED");
}

int find_skill_num(char *name)
{
  int skindex, ok;
  char *temp, *temp2;
  char first[256], first2[256], tempbuf[256];

  for (skindex = 1; skindex <= TOP_SPELL_DEFINE; skindex++)
  {
    if (is_abbrev(name, spell_info[skindex].name))
      return (skindex);

    ok = TRUE;
    strlcpy(tempbuf,
            spell_info[skindex].name, sizeof(tempbuf)); /* strlcpy: OK */
    temp = any_one_arg(tempbuf, first);
    temp2 = any_one_arg(name, first2);
    while (*first && *first2 && ok)
    {
      if (!is_abbrev(first2, first))
        ok = FALSE;
      temp = any_one_arg(temp, first);
      temp2 = any_one_arg(temp2, first2);
    }

    if (ok && !*first2)
      return (skindex);
  }

  return (-1);
}

/* This function is the very heart of the entire magic system.  All invocations
 * of all types of magic -- objects, spoken and unspoken PC and NPC spells, the
 * works -- all come through this function eventually. This is also the entry
 * point for non-spoken or unrestricted spells. Spellnum 0 is legal but silently
 * ignored here, to make callers simpler. */
int call_magic(struct char_data *caster, struct char_data *cvict,
               struct obj_data *ovict, int spellnum, int level, int casttype)
{
  int savetype;

  if (spellnum < 1 || spellnum > TOP_SPELL_DEFINE)
    return (0);

  if (!cast_wtrigger(caster, cvict, ovict, spellnum))
    return 0;
  if (!cast_otrigger(caster, ovict, spellnum))
    return 0;
  if (!cast_mtrigger(caster, cvict, spellnum))
    return 0;

  if (ROOM_FLAGGED(IN_ROOM(caster), ROOM_NOMAGIC))
  {
    send_to_char(caster, "Your magic fizzles out and dies.\r\n");
    act("$n's magic fizzles out and dies.", FALSE, caster, 0, 0, TO_ROOM);
    return (0);
  }
  if (ROOM_FLAGGED(IN_ROOM(caster), ROOM_PEACEFUL) &&
      (SINFO.violent || IS_SET(SINFO.routines, MAG_DAMAGE)))
  {
    send_to_char(caster, "A flash of white light fills the room, dispelling your violent magic!\r\n");
    act("White light from no particular source suddenly fills the room, then vanishes.", FALSE, caster, 0, 0, TO_ROOM);
    return (0);
  }
  if (cvict && MOB_FLAGGED(cvict, MOB_NOKILL))
  {
    send_to_char(caster, "This mob is protected.\r\n");
    return (0);
  }
  /* determine the type of saving throw */
  switch (casttype)
  {
  case CAST_STAFF:
  case CAST_SCROLL:
  case CAST_POTION:
  case CAST_WAND:
    savetype = SAVING_ROD;
    break;
  case CAST_SPELL:
    savetype = SAVING_SPELL;
    break;
  default:
    savetype = SAVING_BREATH;
    break;
  }
  if (spellnum == SPELL_PARALYZE)
    savetype = SAVING_PARA;

  if (IS_SET(SINFO.routines, MAG_DAMAGE))
    if (mag_damage(level, caster, cvict, spellnum, savetype) == -1)
      return (-1); /* Successful and target died, don't cast again. */

  if (IS_SET(SINFO.routines, MAG_AFFECTS))
    mag_affects(level, caster, cvict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_UNAFFECTS))
    mag_unaffects(level, caster, cvict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_POINTS))
    mag_points(level, caster, cvict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_ALTER_OBJS))
    mag_alter_objs(level, caster, ovict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_GROUPS))
    mag_groups(level, caster, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_MASSES))
    mag_masses(level, caster, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_BARD))
    mag_bard(level, caster, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_AREAS))
    mag_areas(level, caster, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_SUMMONS))
    mag_summons(level, caster, ovict, spellnum, savetype);

  if (IS_SET(SINFO.routines, MAG_CREATIONS))
    mag_creations(level, caster, spellnum);

  if (IS_SET(SINFO.routines, MAG_ROOMS))
    mag_rooms(level, caster, spellnum);

  if (IS_SET(SINFO.routines, MAG_MANUAL))
    switch (spellnum)
    {
    case SPELL_CHARM:
      MANUAL_SPELL(spell_charm);
      break;
    case SPELL_CREATE_WATER:
      MANUAL_SPELL(spell_create_water);
      break;
    case SPELL_DETECT_POISON:
      MANUAL_SPELL(spell_detect_poison);
      break;
    case SPELL_ENCHANT_WEAPON:
      MANUAL_SPELL(spell_enchant_weapon);
      break;
    case SPELL_IDENTIFY:
      MANUAL_SPELL(spell_identify);
      break;
    case SPELL_LOCATE_OBJECT:
      MANUAL_SPELL(spell_locate_object);
      break;
    case SPELL_SUMMON:
      MANUAL_SPELL(spell_summon);
      break;
    case SPELL_WORD_OF_RECALL:
      MANUAL_SPELL(spell_recall);
      break;
    case SPELL_TELEPORT:
      MANUAL_SPELL(spell_teleport);
      break;
    case SPELL_RELOCATION:
      MANUAL_SPELL(spell_relocate);
      break;
    case SPELL_CANCELLATION:
      MANUAL_SPELL(spell_cancellation);
      break;
    case SPELL_BARD_GATEWAY:
      MANUAL_SPELL(spell_gateway);
      break;
    case SPELL_BLOOD_MANA:
      MANUAL_SPELL(spell_blood_mana);
      break;
    case SPELL_SATIATE:
      MANUAL_SPELL(spell_satiate);
      break;
    case SPELL_LOCATE_CHARACTER:
      MANUAL_SPELL(spell_locate_char);
      break;
    case SPELL_ENERGIZE:
      MANUAL_SPELL(spell_energize);
      break;
    }

  return (1);
}

/* mag_objectmagic: This is the entry-point for all magic items.  This should
 * only be called by the 'quaff', 'use', 'recite', etc. routines.
 * For reference, object values 0-3:
 * staff  - [0]	level	[1] max charges	[2] num charges	[3] spell num
 * wand   - [0]	level	[1] max charges	[2] num charges	[3] spell num
 * scroll - [0]	level	[1] spell num	[2] spell num	[3] spell num
 * potion - [0] level	[1] spell num	[2] spell num	[3] spell num
 * Staves and wands will default to level 14 if the level is not specified; the
 * DikuMUD format did not specify staff and wand levels in the world files */
void mag_objectmagic(struct char_data *ch, struct obj_data *obj,
                     char *argument)
{
  char arg[MAX_INPUT_LENGTH];
  int i, k;
  struct char_data *tch = NULL, *next_tch;
  struct obj_data *tobj = NULL;

  one_argument(argument, arg);

  k = generic_find(arg, FIND_CHAR_ROOM | FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tch, &tobj);

  switch (GET_OBJ_TYPE(obj))
  {
  case ITEM_STAFF:
    act("You tap $p three times on the ground.", FALSE, ch, obj, 0, TO_CHAR);
    if (obj->action_description)
      act(obj->action_description, FALSE, ch, obj, 0, TO_ROOM);
    else
      act("$n taps $p three times on the ground.", FALSE, ch, obj, 0, TO_ROOM);

    if (GET_OBJ_VAL(obj, 2) <= 0)
    {
      send_to_char(ch, "It seems powerless.\r\n");
      act("Nothing seems to happen.", FALSE, ch, obj, 0, TO_ROOM);
    }
    else
    {
      GET_OBJ_VAL(obj, 2)
      --;
      WAIT_STATE(ch, PULSE_VIOLENCE);
      /* Level to cast spell at. */
      k = GET_OBJ_VAL(obj, 0) ? GET_OBJ_VAL(obj, 0) : DEFAULT_STAFF_LVL;

      /* Area/mass spells on staves can cause crashes. So we use special cases
       * for those spells spells here. */
      if (HAS_SPELL_ROUTINE(GET_OBJ_VAL(obj, 3), MAG_MASSES | MAG_AREAS))
      {
        for (i = 0, tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room)
          i++;
        while (i-- > 0)
          call_magic(ch, NULL, NULL, GET_OBJ_VAL(obj, 3), k, CAST_STAFF);
      }
      else
      {
        for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch)
        {
          next_tch = tch->next_in_room;
          if (ch != tch)
            call_magic(ch, tch, NULL, GET_OBJ_VAL(obj, 3), k, CAST_STAFF);
        }
      }
    }
    break;
  case ITEM_WAND:
    if (k == FIND_CHAR_ROOM)
    {
      if (tch == ch)
      {
        act("You point $p at yourself.", FALSE, ch, obj, 0, TO_CHAR);
        act("$n points $p at $mself.", FALSE, ch, obj, 0, TO_ROOM);
      }
      else
      {
        act("You point $p at $N.", FALSE, ch, obj, tch, TO_CHAR);
        if (obj->action_description)
          act(obj->action_description, FALSE, ch, obj, tch, TO_ROOM);
        else
          act("$n points $p at $N.", TRUE, ch, obj, tch, TO_ROOM);
      }
    }
    else if (tobj != NULL)
    {
      act("You point $p at $P.", FALSE, ch, obj, tobj, TO_CHAR);
      if (obj->action_description)
        act(obj->action_description, FALSE, ch, obj, tobj, TO_ROOM);
      else
        act("$n points $p at $P.", TRUE, ch, obj, tobj, TO_ROOM);
    }
    else if (IS_SET(spell_info[GET_OBJ_VAL(obj, 3)].routines, MAG_AREAS | MAG_MASSES))
    {
      /* Wands with area spells don't need to be pointed. */
      act("You point $p outward.", FALSE, ch, obj, NULL, TO_CHAR);
      act("$n points $p outward.", TRUE, ch, obj, NULL, TO_ROOM);
    }
    else
    {
      act("At what should $p be pointed?", FALSE, ch, obj, NULL, TO_CHAR);
      return;
    }

    if (GET_OBJ_VAL(obj, 2) <= 0)
    {
      send_to_char(ch, "It seems powerless.\r\n");
      act("Nothing seems to happen.", FALSE, ch, obj, 0, TO_ROOM);
      return;
    }
    GET_OBJ_VAL(obj, 2)
    --;
    WAIT_STATE(ch, PULSE_VIOLENCE);
    if (GET_OBJ_VAL(obj, 0))
      call_magic(ch, tch, tobj, GET_OBJ_VAL(obj, 3),
                 GET_OBJ_VAL(obj, 0), CAST_WAND);
    else
      call_magic(ch, tch, tobj, GET_OBJ_VAL(obj, 3),
                 DEFAULT_WAND_LVL, CAST_WAND);
    break;
  case ITEM_SCROLL:
    if (*arg)
    {
      if (!k)
      {
        act("There is nothing to here to affect with $p.", FALSE,
            ch, obj, NULL, TO_CHAR);
        return;
      }
    }
    else
      tch = ch;

    act("You recite $p which dissolves.", TRUE, ch, obj, 0, TO_CHAR);
    if (obj->action_description)
      act(obj->action_description, FALSE, ch, obj, tch, TO_ROOM);
    else
      act("$n recites $p.", FALSE, ch, obj, NULL, TO_ROOM);

    WAIT_STATE(ch, PULSE_VIOLENCE);
    for (i = 1; i <= 3; i++)
      if (call_magic(ch, tch, tobj, GET_OBJ_VAL(obj, i),
                     GET_OBJ_VAL(obj, 0), CAST_SCROLL) <= 0)
        break;

    if (obj != NULL)
      extract_obj(obj);
    break;
  case ITEM_POTION:
    tch = ch;

    if (!consume_otrigger(obj, ch, OCMD_QUAFF)) /* check trigger */
      return;

    act("You quaff $p.", FALSE, ch, obj, NULL, TO_CHAR);
    if (obj->action_description)
      act(obj->action_description, FALSE, ch, obj, NULL, TO_ROOM);
    else
      act("$n quaffs $p.", TRUE, ch, obj, NULL, TO_ROOM);

    WAIT_STATE(ch, PULSE_VIOLENCE);
    for (i = 1; i <= 3; i++)
      if (call_magic(ch, ch, NULL, GET_OBJ_VAL(obj, i),
                     GET_OBJ_VAL(obj, 0), CAST_POTION) <= 0)
        break;

    if (obj != NULL)
      extract_obj(obj);
    break;
  default:
    log("SYSERR: Unknown object_type %d in mag_objectmagic.",
        GET_OBJ_TYPE(obj));
    break;
  }
}

/* cast_spell is used generically to cast any spoken spell, assuming we already
 * have the target char/obj and spell number.  It checks all restrictions,
 * prints the words, etc. Entry point for NPC casts.  Recommended entry point
 * for spells cast by NPCs via specprocs. */
int cast_spell(struct char_data *ch, struct char_data *tch,
               struct obj_data *tobj, int spellnum)
{
  int level = GET_REAL_LEVEL(ch);
  if (spellnum < 0 || spellnum > TOP_SPELL_DEFINE)
  {
    log("SYSERR: cast_spell trying to call spellnum %d/%d.", spellnum,
        TOP_SPELL_DEFINE);
    return (0);
  }

  if (GET_POS(ch) < SINFO.min_position)
  {
    switch (GET_POS(ch))
    {
    case POS_SLEEPING:
      send_to_char(ch, "You dream about great magical powers.\r\n");
      break;
    case POS_RESTING:
      send_to_char(ch, "You cannot concentrate while resting.\r\n");
      break;
    case POS_SITTING:
      send_to_char(ch, "You can't do this sitting!\r\n");
      break;
    case POS_FIGHTING:
      send_to_char(ch, "Impossible!  You can't concentrate enough!\r\n");
      break;
    default:
      send_to_char(ch, "You can't do much of anything like this!\r\n");
      break;
    }
    return (0);
  }
  if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master == tch))
  {
    send_to_char(ch, "You are afraid you might hurt your master!\r\n");
    return (0);
  }
  if ((tch != ch) && IS_SET(SINFO.targets, TAR_SELF_ONLY))
  {
    send_to_char(ch, "You can only target this spell upon yourself!\r\n");
    return (0);
  }
  if ((tch == ch) && IS_SET(SINFO.targets, TAR_NOT_SELF))
  {
    send_to_char(ch, "You cannot cast this spell upon yourself!\r\n");
    return (0);
  }
  if (IS_SET(SINFO.routines, MAG_GROUPS) && !GROUP(ch))
  {
    send_to_char(ch, "You can't cast this spell if you're not in a group!\r\n");
    return (0);
  }

  //send_to_char(ch, "spell_parser unmodified level is %d\n\r", level);

  if (IS_SET(SINFO.routines, MAG_DAMAGE))
  {
    level += GET_SPELLS_DAMAGE(ch);
    //send_to_char(ch, "spell_parser, %d, MAG_DAMAGE\n\r", level);
  }
  else if (IS_SET(SINFO.routines, MAG_AFFECTS))
  {
    level += GET_SPELLS_AFFECTS(ch);
    //send_to_char(ch, "spell_parser %d, MAG_AFFECTS\n\r", level);
  }
  else if (IS_SET(SINFO.routines, MAG_POINTS))
  {
    level += GET_SPELLS_HEALING(ch);
    //send_to_char(ch, "spell_parser %d, MAG_POINTS\n\r", level);
  }
  else if (IS_SET(SINFO.routines, MAG_UNAFFECTS))
  {
    level += GET_SPELLS_AFFECTS(ch);
    //send_to_char(ch, "spell_parser %d, MAG_UNAFFECTS\n\r", level);
  }
  else if (IS_SET(SINFO.routines, MAG_GROUPS))
  {
    level += (int)((GET_SPELLS_HEALING(ch) + GET_SPELLS_AFFECTS(ch)) / 2);
    //send_to_char(ch, "spell_parser %d, level + average\n\r", level);
  }
  else if (IS_SET(SINFO.routines, MAG_ALTER_OBJS))
  {
    level += GET_SPELLS_AFFECTS(ch);
    //send_to_char(ch, "spell_parser %d, MAG_ALTRER_OBJS\n\r", level);
  }
  else if (IS_SET(SINFO.routines, MAG_AREAS))
  {
    level += GET_SPELLS_DAMAGE(ch);
    //send_to_char(ch, "spell_parser %d, MAG_AREAS\n\r", level);
  }

  //	send_to_char(ch, "spell_parser Level is %d, spells_healing is %d,\n\r",level,GET_SPELLS_HEALING(ch));
  send_to_char(ch, "%s", CONFIG_OK);
  say_spell(ch, spellnum, tch, tobj);

  return (call_magic(ch, tch, tobj, spellnum, level, CAST_SPELL));
}

/* do_cast is the entry point for PC-casted spells.  It parses the arguments,
 * determines the spell number and finds a target, throws the die to see if
 * the spell can be cast, checks for sufficient mana and subtracts it, and
 * passes control to cast_spell(). */
ACMD(do_cast)
{
  struct char_data *tch = NULL;
  struct obj_data *tobj = NULL;
  char *s, *t;
  int number, mana, spellnum, i, target = 0;

  if (IS_NPC(ch))
    return;

  /* get: blank, spell name, target name */
  s = strtok(argument, "'");

  if (GET_CLASS(ch) != CLASS_PRIEST && GET_MULTI_CLASS(ch) != CLASS_PRIEST && GET_CLASS(ch) != CLASS_WIZARD && GET_MULTI_CLASS(ch) != CLASS_WIZARD)
  {
    send_to_char(ch, "You don't know how to use magic!\n\r");
    return;
  }
  if (s == NULL)
  {
    send_to_char(ch, "Cast what where?\r\n");
    return;
  }
  s = strtok(NULL, "'");
  if (s == NULL)
  {
    send_to_char(ch, "Spell names must be enclosed in the Holy Magic Symbols: '\r\n");
    return;
  }
  t = strtok(NULL, "\0");

  skip_spaces(&s);

  /* spellnum = search_block(s, spells, 0); */
  spellnum = find_skill_num(s);

  if ((spellnum < 1) || (spellnum > MAX_SPELLS) || !*s)
  {
    send_to_char(ch, "Cast what?!?\r\n");
    return;
  }
  if (GET_SKILL(ch, spellnum) == 0)
  {
    send_to_char(ch, "You are unfamiliar with that spell.\r\n");
    return;
  }

  /* Find the target */
  if (t != NULL)
  {
    char arg[MAX_INPUT_LENGTH];

    strlcpy(arg, t, sizeof(arg));
    one_argument(arg, t);
    skip_spaces(&t);

    /* Copy target to global cast_arg2, for use in spells like locate object */
    strcpy(cast_arg2, t);
  }
  if (IS_SET(SINFO.targets, TAR_IGNORE))
  {
    target = TRUE;
  }
  else if (t != NULL && *t)
  {
    number = get_number(&t);
    if (!target && (IS_SET(SINFO.targets, TAR_CHAR_ROOM)))
    {
      if ((tch = get_char_vis(ch, t, &number, FIND_CHAR_ROOM)) != NULL)
        target = TRUE;
    }
    if (!target && IS_SET(SINFO.targets, TAR_CHAR_WORLD))
      if ((tch = get_char_vis(ch, t, &number, FIND_CHAR_WORLD)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_INV))
      if ((tobj = get_obj_in_list_vis(ch, t, &number, ch->carrying)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_EQUIP))
    {
      for (i = 0; !target && i < NUM_WEARS; i++)
        if (GET_EQ(ch, i) && isname(t, GET_EQ(ch, i)->name))
        {
          tobj = GET_EQ(ch, i);
          target = TRUE;
        }
    }
    if (!target && IS_SET(SINFO.targets, TAR_OBJ_ROOM))
      if ((tobj = get_obj_in_list_vis(ch, t, &number, world[IN_ROOM(ch)].contents)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_WORLD))
      if ((tobj = get_obj_vis(ch, t, &number)) != NULL)
        target = TRUE;
  }
  else
  { /* if target string is empty */
    if (!target && IS_SET(SINFO.targets, TAR_FIGHT_SELF))
      if (FIGHTING(ch) != NULL)
      {
        tch = ch;
        target = TRUE;
      }
    if (!target && IS_SET(SINFO.targets, TAR_FIGHT_VICT))
      if (FIGHTING(ch) != NULL)
      {
        tch = FIGHTING(ch);
        target = TRUE;
      }
    /* if no target specified, and the spell isn't violent, default to self */
    if (!target && IS_SET(SINFO.targets, TAR_CHAR_ROOM) && !SINFO.violent)
    {
      tch = ch;
      target = TRUE;
    }
    if (!target)
    {
      send_to_char(ch, "Upon %s should the spell be cast?\r\n",
                   IS_SET(SINFO.targets, TAR_OBJ_ROOM | TAR_OBJ_INV | TAR_OBJ_WORLD | TAR_OBJ_EQUIP) ? "what" : "who");
      return;
    }
  }

  if (target && (tch == ch) && SINFO.violent)
  {
    send_to_char(ch, "You shouldn't cast that on yourself -- could be bad for your health!\r\n");
    return;
  }
  if (!target)
  {
    send_to_char(ch, "Cannot find the target of your spell!\r\n");
    return;
  }
  mana = mag_manacost(ch, spellnum);
  if ((mana > 0) && (GET_MANA(ch) < mana) && (GET_REAL_LEVEL(ch) < LVL_IMMORT))
  {
    send_to_char(ch, "You haven't the energy to cast that spell!\r\n");
    return;
  }

  /* You throws the dice and you takes your chances.. 101% is total failure */
  if (rand_number(0, 101) > GET_SKILL(ch, spellnum))
  {
    int wait = PULSE_VIOLENCE;

    if (IS_AFFECTED(ch, AFF_QUICKCAST))
      wait /= 2;

    WAIT_STATE(ch, wait);

    if (!tch || !skill_message(0, ch, tch, spellnum))
      send_to_char(ch, "You lost your concentration!\r\n");
    check_improve(ch, spellnum, FALSE);
    if (mana > 0)
      GET_MANA(ch) = MAX(0, MIN(GET_MAX_MANA(ch), GET_MANA(ch) - (mana / 2)));
    if (SINFO.violent && tch && IS_NPC(tch))
      hit(tch, ch, TYPE_UNDEFINED);
  }
  else
  { /* cast spell returns 1 on success; subtract mana & set waitstate */
    if (cast_spell(ch, tch, tobj, spellnum))
    {
      int wait = PULSE_VIOLENCE;

      if (IS_AFFECTED(ch, AFF_QUICKCAST))
        wait /= 2;

      WAIT_STATE(ch, wait);
      check_improve(ch, spellnum, TRUE);

      if (mana > 0)
        GET_MANA(ch) = MAX(0, MIN(GET_MAX_MANA(ch), GET_MANA(ch) - mana));
    }
  }
}

ACMD(do_sing)
{
  struct char_data *tch = NULL;
  struct obj_data *tobj = NULL;
  {
      /* data */
  };

  char *s, *t;
  int number, mana, spellnum, i, target = 0;

  if (IS_NPC(ch))
    return;

  /* get: blank, spell name, target name */
  s = strtok(argument, "'");

  if (GET_CLASS(ch) != CLASS_BARD && GET_MULTI_CLASS(ch) != CLASS_BARD)
  {
    send_to_char(ch, "You don't know how to carry a tune.\n\r");
    return;
  }
  if (!IS_NPC(ch) && !GET_EQ(ch, WEAR_HOLD))
  {
    send_to_char(ch, "You must be holding a bardic instrument.\n\r");
    return;
  }
  if (GET_EQ(ch, WEAR_HOLD))
  {
    if (GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) != ITEM_BARD)
    {
      send_to_char(ch, "You must be holding a bardic instrument.\n\r");
      return;
    }
  }
  if (s == NULL)
  {
    send_to_char(ch, "Sing what song?\r\n");
    return;
  }
  s = strtok(NULL, "'");
  if (s == NULL)
  {
    send_to_char(ch, "Song names must be enclosed in the Holy Title Symbols: '\r\n");
    return;
  }
  t = strtok(NULL, "\0");

  skip_spaces(&s);

  /* spellnum = search_block(s, spells, 0); */
  spellnum = find_skill_num(s);

  if ((spellnum < ZERO_SONGS) || (spellnum > ZERO_SONGS + NUM_SONGS) || !*s)
  {
    send_to_char(ch, "Sing what?!?\r\n");
    return;
  }
  if (GET_SKILL(ch, spellnum) == 0)
  {
    send_to_char(ch, "You don't know that song.\r\n");
    return;
  }

  /* Find the target */
  if (t != NULL)
  {
    char arg[MAX_INPUT_LENGTH];

    strlcpy(arg, t, sizeof(arg));
    one_argument(arg, t);
    skip_spaces(&t);

    /* Copy target to global cast_arg2, for use in spells like locate object */
    strcpy(cast_arg2, t);
  }
  if (IS_SET(SINFO.targets, TAR_IGNORE))
  {
    target = TRUE;
  }
  else if (t != NULL && *t)
  {
    number = get_number(&t);
    if (!target && (IS_SET(SINFO.targets, TAR_CHAR_ROOM)))
    {
      if ((tch = get_char_vis(ch, t, &number, FIND_CHAR_ROOM)) != NULL)
        target = TRUE;
    }
    if (!target && IS_SET(SINFO.targets, TAR_CHAR_WORLD))
      if ((tch = get_char_vis(ch, t, &number, FIND_CHAR_WORLD)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_INV))
      if ((tobj = get_obj_in_list_vis(ch, t, &number, ch->carrying)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_EQUIP))
    {
      for (i = 0; !target && i < NUM_WEARS; i++)
        if (GET_EQ(ch, i) && isname(t, GET_EQ(ch, i)->name))
        {
          tobj = GET_EQ(ch, i);
          target = TRUE;
        }
    }
    if (!target && IS_SET(SINFO.targets, TAR_OBJ_ROOM))
      if ((tobj = get_obj_in_list_vis(ch, t, &number, world[IN_ROOM(ch)].contents)) != NULL)
        target = TRUE;

    if (!target && IS_SET(SINFO.targets, TAR_OBJ_WORLD))
      if ((tobj = get_obj_vis(ch, t, &number)) != NULL)
        target = TRUE;
  }
  else
  { /* if target string is empty */
    if (!target && IS_SET(SINFO.targets, TAR_FIGHT_SELF))
      if (FIGHTING(ch) != NULL)
      {
        tch = ch;
        target = TRUE;
      }
    if (!target && IS_SET(SINFO.targets, TAR_FIGHT_VICT))
      if (FIGHTING(ch) != NULL)
      {
        tch = FIGHTING(ch);
        target = TRUE;
      }
    /* if no target specified, and the spell isn't violent, default to self */
    if (!target && IS_SET(SINFO.targets, TAR_CHAR_ROOM) && !SINFO.violent)
    {
      tch = ch;
      target = TRUE;
    }
    if (!target)
    {
      send_to_char(ch, "%s are you singing to?\r\n",
                   IS_SET(SINFO.targets, TAR_OBJ_ROOM | TAR_OBJ_INV | TAR_OBJ_WORLD | TAR_OBJ_EQUIP) ? "What" : "Who");
      return;
    }
  }

  if (target && (tch == ch) && SINFO.violent)
  {
    send_to_char(ch, "You shouldn't sing that to yourself -- could be bad for your mental health!\r\n");
    return;
  }
  if (!target)
  {
    send_to_char(ch, "Cannot find who you are singing to!\r\n");
    return;
  }
  mana = mag_manacost(ch, spellnum);
  if ((mana > 0) && (GET_MANA(ch) < mana) && (GET_REAL_LEVEL(ch) < LVL_IMMORT))
  {
    send_to_char(ch, "You haven't the energy to sing thast song!\r\n");
    return;
  }

  /* You throws the dice and you takes your chances.. 101% is total failure */
  if (rand_number(0, 101) > GET_SKILL(ch, spellnum))
  {
    int wait = PULSE_VIOLENCE;

    if (IS_AFFECTED(ch, AFF_QUICKCAST))
      wait /= 2;

    WAIT_STATE(ch, wait);

    if (!tch || !skill_message(0, ch, tch, spellnum))
      send_to_char(ch, "You lost the tune, the song failed!\r\n");
    check_improve(ch, spellnum, FALSE);
    if (mana > 0)
      GET_MANA(ch) = MAX(0, MIN(GET_MAX_MANA(ch), GET_MANA(ch) - (mana / 2)));
    if (SINFO.violent && tch && IS_NPC(tch))
      hit(tch, ch, TYPE_UNDEFINED);
  }
  else
  { /* cast spell returns 1 on success; subtract mana & set waitstate */
    if (cast_spell(ch, tch, tobj, spellnum))
    {
      int wait = PULSE_VIOLENCE;

      WAIT_STATE(ch, wait);
      check_improve(ch, spellnum, TRUE);

      if (mana > 0)
        GET_MANA(ch) = MAX(0, MIN(GET_MAX_MANA(ch), GET_MANA(ch) - mana));
    }
  }
}

void spell_level(int spell, int chclass, int level)
{
  //  int bad = 0;
  bool bad = FALSE;
  if (spell < 0 || spell > TOP_SPELL_DEFINE)
  {
    log("SYSERR: attempting assign to illegal spellnum %d/%d", spell, TOP_SPELL_DEFINE);
    return;
  }

  if (chclass < 0 || chclass >= NUM_CLASSES)
  {
    log("SYSERR: assigning '%s' to illegal class %d/%d.", skill_name(spell),
        chclass, NUM_CLASSES - 1);
    //    bad = 1;
    bad = TRUE;
  }

  if (level < 1 || level > LVL_IMPL)
  {
    log("SYSERR: assigning '%s' to illegal level %d/%d.", skill_name(spell),
        level, LVL_IMPL);
    //    bad = 1;
    bad = TRUE;
  }

  if (bad == FALSE)
    spell_info[spell].min_level[chclass] = level;
}

/* Assign the spells on boot up */
static void spello(int spl, const char *name, int max_mana, int min_mana,
                   int mana_change, int minpos, int targets, int violent, int routines, const char *wearoff)
{
  int i;

  for (i = 0; i < NUM_CLASSES; i++)
    spell_info[spl].min_level[i] = LVL_IMMORT;
  spell_info[spl].mana_max = max_mana;
  spell_info[spl].mana_min = min_mana;
  spell_info[spl].mana_change = mana_change;
  spell_info[spl].min_position = minpos;
  spell_info[spl].targets = targets;
  spell_info[spl].violent = violent;
  spell_info[spl].routines = routines;
  spell_info[spl].name = name;
  spell_info[spl].wear_off_msg = wearoff;
}

void unused_spell(int spl)
{
  int i;

  for (i = 0; i < NUM_CLASSES; i++)
    spell_info[spl].min_level[i] = LVL_IMPL + 1;
  spell_info[spl].mana_max = 0;
  spell_info[spl].mana_min = 0;
  spell_info[spl].mana_change = 0;
  spell_info[spl].min_position = 0;
  spell_info[spl].targets = 0;
  spell_info[spl].violent = 0;
  spell_info[spl].routines = 0;
  spell_info[spl].name = unused_spellname;
}

#define skillo(skill, name) spello(skill, name, 0, 0, 0, 0, 0, 0, 0, NULL);
/* Arguments for spello calls:
 * spellnum, maxmana, minmana, manachng, minpos, targets, violent?, routines.
 * spellnum:  Number of the spell.  Usually the symbolic name as defined in
 *  spells.h (such as SPELL_HEAL).
 * spellname: The name of the spell.
 * maxmana :  The maximum mana this spell will take (i.e., the mana it
 *  will take when the player first gets the spell).
 * minmana :  The minimum mana this spell will take, no matter how high
 *  level the caster is.
 * manachng:  The change in mana for the spell from level to level.  This
 *  number should be positive, but represents the reduction in mana cost as
 *  the caster's level increases.
 * minpos  :  Minimum position the caster must be in for the spell to work
 *  (usually fighting or standing).
 * targets :  A "list" of the valid targets for the spell, joined with bitwise OR ('|').
 * violent :  TRUE or FALSE, depending on if this is considered a violent
 *  spell and should not be cast in PEACEFUL rooms or on yourself.  Should be
 *  set on any spell that inflicts damage, is considered aggressive (i.e.
 *  charm, curse), or is otherwise nasty.
 * routines:  A list of magic routines which are associated with this spell
 *  if the spell uses spell templates.  Also joined with bitwise OR ('|').
 * See the documentation for a more detailed description of these fields. You
 * only need a spello() call to define a new spell; to decide who gets to use
 * a spell or skill, look in class.c.  -JE */
void mag_assign_spells(void)
{
  int i;

  /* Do not change the loop below. */
  for (i = 0; i <= TOP_SPELL_DEFINE; i++)
    unused_spell(i);
  /* Do not change the loop above. */
  spello(SPELL_FRENZY, "frenzy", 100, 50, 10, POS_FIGHTING, TAR_IGNORE, FALSE, MAG_AFFECTS, NULL);

  spello(SPELL_ENERGIZE, "energize", 250 , 100, 20, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_MANUAL,
         NULL);
  spello(SPELL_CONSECRATION, "consecration", 250, 100, 20, POS_STANDING, TAR_IGNORE, FALSE,
         MAG_ROOMS | MAG_AFFECTS, NULL);

  spello(SPELL_REND, "rend", 100, 50, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_FIGHT_VICT,
         TRUE, MAG_AFFECTS, NULL);

  spello(SPELL_FEEBLE, "feeble", 100, 50, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_FIGHT_VICT,
         TRUE, MAG_AFFECTS, NULL);

   spello(SPELL_CHAIN_LIGHTNING, "chain lightning", 250, 100, 5, POS_FIGHTING, TAR_IGNORE,
         TRUE, MAG_AREAS, NULL);

/*   spello(SPELL_FIRESHIELD, "fireshield", 100, 50, 5, POS_FIGHTING, TAR_SELF_ONLY,
         TRUE, MAG_AFFECTS, NULL);
 */
  spello(SPELL_ANIMATE_DEAD, "animate dead", 35, 10, 3, POS_STANDING,
         TAR_OBJ_ROOM, FALSE, MAG_SUMMONS,
         NULL);

  spello(SPELL_BARD_SANC, "cromlech chorus", 500, 250, 50, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS, NULL);

  spello(SPELL_BARD_REGEN, "rebirth refrain", 200, 100, 20, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS, NULL);

  spello(SPELL_BARD_DEBUFF, "chaotic canticle", 200, 100, 20, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS, NULL);

  spello(SPELL_BARD_RECALL, "recall refrain", 100, 50, 5, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD,
         NULL);

  spello(SPELL_BARD_FEAST, "banquet ballad", 50, 25, 5, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS,
         NULL);

  spello(SPELL_BARD_BLESS, "campaign carol", 50, 25, 5, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS,
         "You feel less inspired.");

  spello(SPELL_BARD_BUFF, "resist rock", 50, 25, 5, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS,
         "You feel less ready for battle");

  spello(SPELL_BARD_HEAL, "healing hymnal", 200, 100, 10, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_POINTS,
         NULL);

  spello(SPELL_BARD_POWERHEAL, "powerheal psalm", 400, 200, 25, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_POINTS,
         NULL);

  spello(SPELL_BARD_SONIC, "sonic shanty", 500, 250, 20, POS_FIGHTING,
         TAR_IGNORE, TRUE, MAG_BARD | MAG_AFFECTS,
         NULL);

  spello(SPELL_BARD_KNOWLEDGE, "insight aria", 200, 50, 5, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS,
         "Your knowledge has decreased.");

  spello(SPELL_BARD_AGILITY, "agility anthem", 200, 50, 5, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS,
         "Your agility has decreased.");

  spello(SPELL_BARD_VITALITY, "vitality verse", 200, 50, 5, POS_FIGHTING,
         TAR_IGNORE, FALSE, MAG_BARD | MAG_AFFECTS,
         "Your vitality has decreased.");

  spello(SPELL_ARMOR, "armor", 30, 15, 3, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
         "You feel less protected.");

  spello(SPELL_BLESS, "bless", 35, 5, 3, POS_STANDING,
         TAR_CHAR_ROOM | TAR_OBJ_INV, FALSE, MAG_AFFECTS | MAG_ALTER_OBJS,
         "You feel less righteous.");

  spello(SPELL_BLINDNESS, "blindness", 35, 25, 1, POS_STANDING,
         TAR_CHAR_ROOM | TAR_NOT_SELF, FALSE, MAG_AFFECTS,
         "You feel a cloak of blindness dissolve.");

  spello(SPELL_BURNING_HANDS, "burning hands", 30, 10, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_CALL_LIGHTNING, "call lightning", 40, 25, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_CHARM, "charm person", 75, 50, 2, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_NOT_SELF, TRUE, MAG_MANUAL,
         "You feel more self-confident.");

  spello(SPELL_CHILL_TOUCH, "chill touch", 30, 10, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE | MAG_AFFECTS,
         "You feel your strength return.");

  spello(SPELL_CLONE, "clone", 80, 65, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_SUMMONS,
         NULL);

  spello(SPELL_COLOR_SPRAY, "color spray", 30, 15, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_CONTROL_WEATHER, "control weather", 75, 25, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_CREATE_FOOD, "create food", 30, 5, 4, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_CREATIONS,
         NULL);

  spello(SPELL_CREATE_WATER, "create water", 30, 5, 4, POS_STANDING,
         TAR_OBJ_INV | TAR_OBJ_EQUIP, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_CURE_BLIND, "cure blind", 30, 5, 2, POS_STANDING,
         TAR_CHAR_ROOM, FALSE, MAG_UNAFFECTS,
         NULL);

  spello(SPELL_CURE_CRITIC, "cure critic", 30, 10, 2, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_POINTS,
         NULL);

  spello(SPELL_CURE_LIGHT, "cure light", 30, 10, 2, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_POINTS,
         NULL);

  spello(SPELL_CURSE, "curse", 80, 50, 5, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_OBJ_INV, TRUE, MAG_AFFECTS | MAG_ALTER_OBJS,
         "You feel more optimistic.");

  spello(SPELL_PARALYZE, "paralyze", 150, 75, 5, POS_FIGHTING,
         TAR_CHAR_ROOM, TRUE, MAG_AFFECTS,
         "You regain control of your limbs.");

  spello(SPELL_DARKNESS, "darkness", 30, 5, 4, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_ROOMS,
         NULL);

  spello(SPELL_DETECT_ALIGN, "detect alignment", 20, 10, 2, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "You feel less aware.");

  spello(SPELL_DETECT_INVIS, "detect invis", 20, 10, 2, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "Your eyes stop tingling.");

  spello(SPELL_DETECT_MAGIC, "detect magic", 20, 10, 2, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "The detect magic wears off.");

  spello(SPELL_DETECT_POISON, "detect poison", 15, 5, 1, POS_STANDING,
         TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE, MAG_MANUAL,
         "The detect poison wears off.");

  spello(SPELL_DISPEL_EVIL, "dispel evil", 40, 25, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_DISPEL_GOOD, "dispel good", 40, 25, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_EARTHQUAKE, "earthquake", 40, 25, 3, POS_FIGHTING,
         TAR_IGNORE, TRUE, MAG_AREAS,
         NULL);

  spello(SPELL_ENCHANT_WEAPON, "enchant weapon", 150, 100, 10, POS_STANDING,
         TAR_OBJ_INV, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_ENERGY_DRAIN, "energy drain", 40, 25, 1, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE | MAG_MANUAL,
         NULL);

  spello(SPELL_GROUP_ARMOR, "group armor", 50, 30, 2, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS,
         NULL);

  spello(SPELL_GROUP_RECALL, "group recall", 100, 50, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS,
         NULL);

  spello(SPELL_GROUP_SUMMON, "group summon", 200, 150, 10, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS,
         NULL);

  spello(SPELL_GROUP_SATIATE, "group satiate", 100, 50, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS,
         NULL);

  spello(SPELL_FIREBALL, "fireball", 40, 30, 2, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_FLY, "fly", 40, 20, 2, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
         "You drift slowly to the ground.");

  spello(SPELL_FURY, "fury", 100, 50, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "You calm down.");
  spello(SPELL_BARD_FURY, "bardic fury", 100, 50, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "You calm down.");
  spello(SPELL_GAIN_ADVANTAGE, "gain advantage", 100, 50, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "You no longer identify the advantage.");
  spello(SPELL_MAGE_ARMOR, "mage armor", 50, 25, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "Your magic armor fades.");
  spello(SPELL_HASTE, "haste", 100, 50, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "You slow down.");
  spello(SPELL_REGENERATION, "regeneration", 50, 25, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "Your regeneration fades.");
  spello(SPELL_STONE_SKIN, "stone skin", 50, 25, 5, POS_FIGHTING, TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "Your skin no longer feels like stone.");
  spello(SPELL_STEEL_SKIN, "steel skin", 50, 25, 5, POS_FIGHTING, TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "Your skin no longer feels like steel.");
  spello(SPELL_QUICKCAST, "quickcast", 100, 50, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "Your mental alacrity recedes.");
  spello(SPELL_MIRROR_IMAGE, "mirror image", 500, 250, 10, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "No mirrors images remain.");
  spello(SPELL_TRUESIGHT, "truesight", 50, 25, 5, POS_FIGHTING, TAR_CHAR_ROOM, FALSE, MAG_AFFECTS, "Your truesight fades.");

  spello(SPELL_PASS_DOOR, "pass door", 50, 10, 2, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "You are unable to through locked doors any longer.");
  spello(SPELL_IMP_PASS_DOOR, "imp pass door", 200, 10, 2, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "You are unable to through unlocked doors any longer.");

  spello(SPELL_GROUP_HASTE, "group haste", 500, 250, 10, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS, NULL);

  spello(SPELL_GROUP_STEEL_SKIN, "group steel skin", 100, 50, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS, NULL);
  spello(SPELL_GROUP_STONE_SKIN, "group stone skin", 100, 50, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS, NULL);
  spello(SPELL_GROUP_PASS_DOOR, "group pass dooor", 200, 50, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS, NULL);
  spello(SPELL_GROUP_SANCTUARY, "group sanctuary", 200, 150, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS, NULL);
  spello(SPELL_GROUP_BLESS, "group bless", 100, 25, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS, NULL);
  spello(SPELL_GROUP_FLY, "group fly", 100, 50, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS, NULL);

  spello(SPELL_GROUP_MIRACLE, "group miracle", 500, 250, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_POINTS,
         NULL);

  spello(SPELL_GROUP_HEAL, "group heal", 100, 50, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_POINTS,
         NULL);

  spello(SPELL_GROUP_POWERHEAL, "group powerheal", 250, 100, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_POINTS,
         NULL);

  spello(SPELL_HARM, "harm", 75, 45, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_CONCUSSIVE_WAVE, "concussive wave", 100, 50, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_HEAL, "heal", 60, 40, 3, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_POINTS | MAG_UNAFFECTS,
         NULL);

  spello(SPELL_POWERHEAL, "powerheal", 100, 75, 5, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_POINTS | MAG_UNAFFECTS,
         NULL);

  spello(SPELL_INFRAVISION, "infravision", 25, 10, 1, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "Your night vision seems to fade.");

  spello(SPELL_INVISIBLE, "invisibility", 35, 25, 1, POS_STANDING,
         TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE, MAG_AFFECTS | MAG_ALTER_OBJS,
         "You feel yourself exposed.");

  spello(SPELL_IMPROVED_INVIS, "improved invis", 500, 250, 10, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "Your improved invisibility fades.");

  spello(SPELL_LIGHTNING_BOLT, "lightning bolt", 30, 15, 1, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_LOCATE_OBJECT, "locate object", 25, 20, 1, POS_STANDING,
         TAR_OBJ_WORLD, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_MAGIC_MISSILE, "magic missile", 25, 10, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_POISON, "poison", 50, 20, 3, POS_STANDING,
         TAR_CHAR_ROOM | TAR_NOT_SELF | TAR_OBJ_INV, TRUE,
         MAG_AFFECTS | MAG_ALTER_OBJS,
         "You feel less sick.");

  spello(SPELL_HOLY_WARDING, "holy warding", 40, 10, 3, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "You feel less protected.");

  spello(SPELL_EVIL_WARDING, "evil warding", 40, 10, 3, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "You feel less protected.");

  spello(SPELL_REMOVE_CURSE, "remove curse", 45, 25, 5, POS_STANDING,
         TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_EQUIP, FALSE,
         MAG_UNAFFECTS | MAG_ALTER_OBJS,
         NULL);

  spello(SPELL_REMOVE_POISON, "remove poison", 40, 8, 4, POS_STANDING,
         TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE, MAG_UNAFFECTS | MAG_ALTER_OBJS,
         NULL);

  spello(SPELL_SANCTUARY, "sanctuary", 110, 85, 5, POS_STANDING,
         TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
         "The white aura around your body fades.");

  spello(SPELL_BETRAYAL, "betrayal", 500, 250, 10, POS_STANDING,
         TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
         NULL);

  spello(SPELL_SATIATE, "satiate", 50, 25, 5, POS_STANDING,
         TAR_CHAR_ROOM, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_SENSE_LIFE, "sense life", 20, 10, 2, POS_STANDING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS,
         "You feel less aware of your surroundings.");

  spello(SPELL_SHOCKING_GRASP, "shocking grasp", 30, 15, 3, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_SLEEP, "sleep", 40, 25, 5, POS_STANDING,
         TAR_CHAR_ROOM, TRUE, MAG_AFFECTS,
         "You feel less tired.");

  spello(SPELL_STRENGTH, "strength", 35, 30, 1, POS_STANDING,
         TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
         "You feel weaker.");

  spello(SPELL_SUMMON, "summon", 75, 50, 3, POS_STANDING,
         TAR_CHAR_WORLD | TAR_NOT_SELF, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_TELEPORT, "teleport", 75, 50, 3, POS_STANDING,
         TAR_CHAR_ROOM, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_WATERWALK, "waterwalk", 40, 20, 2, POS_STANDING,
         TAR_CHAR_ROOM, FALSE, MAG_AFFECTS,
         "Your feet seem less buoyant.");

  spello(SPELL_WORD_OF_RECALL, "word of recall", 20, 10, 2, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_IDENTIFY, "identify", 20, 10, 5, POS_STANDING,
         TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_RELOCATION, "relocate", 75, 50, 5, POS_STANDING,
         TAR_CHAR_WORLD | TAR_NOT_SELF, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_BARD_GATEWAY, "gateway", 75, 50, 5, POS_STANDING,
         TAR_CHAR_WORLD | TAR_NOT_SELF, FALSE, MAG_MANUAL, NULL);

  /*
	Spell Miracle as demo to Rebekah.
	Salty
	08 JAN 2019
*/
  spello(SPELL_MIRACLE, "miracle", 150, 100, 5, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_POINTS | MAG_UNAFFECTS,
         NULL);

  spello(SPELL_CANCELLATION, "cancellation", 20, 10, 2, POS_FIGHTING,
         TAR_CHAR_ROOM, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_MISSILE_SPRAY, "missile spray", 500, 250, 25, POS_FIGHTING,
         TAR_IGNORE, TRUE, MAG_AREAS,
         NULL);

  spello(SPELL_FIREBLAST, "fireblast", 400, 200, 25, POS_FIGHTING,
         TAR_IGNORE, TRUE, MAG_AREAS,
         NULL);

  spello(SPELL_BLOOD_MANA, "blood mana", 100, 50, 10, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_MANUAL, NULL);

  /* NON-castable spells should appear below here. */
  spello(SPELL_IDENTIFY, "identify", 0, 0, 0, POS_STANDING,
         TAR_CHAR_ROOM | TAR_OBJ_INV | TAR_OBJ_ROOM, FALSE, MAG_MANUAL,
         NULL);

  spello(SPELL_SYMBOL_OF_PAIN, "symbol of pain", 500, 250, 30, POS_FIGHTING,
         TAR_IGNORE, TRUE, MAG_AREAS,
         NULL);

  spello(SPELL_NIGHTMARE, "nightmare", 100, 50, 5, POS_FIGHTING,
         TAR_CHAR_ROOM | TAR_FIGHT_VICT, TRUE, MAG_DAMAGE,
         NULL);

  spello(SPELL_GROUP_REGEN, "group regen", 500, 250, 5, POS_STANDING,
         TAR_IGNORE, FALSE, MAG_GROUPS | MAG_AFFECTS, NULL);

  spello(SPELL_LOCATE_CHARACTER, "locate char", 50, 25, 5, POS_STANDING, TAR_CHAR_WORLD, FALSE, MAG_MANUAL, NULL);

  spello(SPELL_UNARMED_BONUS, "unarmed bonus", 100, 50, 5, POS_FIGHTING, TAR_CHAR_ROOM | TAR_SELF_ONLY, FALSE, MAG_AFFECTS, "Your unarmed bonus recedes.");

  spello(SPELL_UNARMED_DEBUFF1, "unarmed debuff1", 100, 75, 5, POS_FIGHTING,
         TAR_CHAR_ROOM, TRUE, MAG_AFFECTS,
         "You recover from the haymaker.");

  spello(SPELL_UNARMED_DEBUFF2, "unarmed debuff2", 100, 75, 5, POS_FIGHTING,
         TAR_CHAR_ROOM, TRUE, MAG_AFFECTS,
         "You can feel your feet again.");

  spello(SPELL_V_DEBUFF, "vitality debuff", 100, 75, 5, POS_FIGHTING,
         TAR_CHAR_ROOM, TRUE, MAG_AFFECTS,
         "You feel more vital.");

  spello(SPELL_A_DEBUFF, "agility debuff", 100, 75, 5, POS_FIGHTING,
         TAR_CHAR_ROOM, TRUE, MAG_AFFECTS,
         "You feel more agile.");

  spello(SPELL_K_DEBUFF, "knowledge debuff", 100, 75, 5, POS_FIGHTING,
         TAR_CHAR_ROOM, TRUE, MAG_AFFECTS,
         "You feel smarter.");

  /* you might want to name this one something more fitting to your theme -Welcor*/
  spello(SPELL_DG_AFFECT, "Script-inflicted", 0, 0, 0, POS_SITTING,
         TAR_IGNORE, TRUE, 0,
         NULL);

  /* Declaration of skills - this actually doesn't do anything except set it up
   * so that immortals can use these skills by default.  The min level to use
   * the skill for other classes is set up in class.c. */
  skillo(SKILL_BACKSTAB, "backstab");
  skillo(SKILL_BASH, "bash");
  skillo(SKILL_HIDE, "hide");
  skillo(SKILL_KICK, "kick");
  skillo(SKILL_PICK_LOCK, "pick lock");
  skillo(SKILL_RESCUE, "rescue");
  skillo(SKILL_SNEAK, "sneak");
  skillo(SKILL_STEAL, "steal");
  skillo(SKILL_TRACK, "track");
  skillo(SKILL_WHIRLWIND, "whirlwind");
  skillo(SKILL_BANDAGE, "bandage");
  skillo(SKILL_SECOND_ATTACK, "2nd attack");
  skillo(SKILL_THIRD_ATTACK, "3rd attack");
  skillo(SKILL_FOURTH_ATTACK, "4th attack");
  skillo(SKILL_FIFTH_ATTACK, "5th attack");
  skillo(SKILL_PARRY, "parry");
  skillo(SKILL_DODGE, "dodge");
  skillo(SKILL_EVADE, "evade");
  skillo(SKILL_IMPROVE, "improve");
  skillo(SKILL_CIRCLE, "circle");
  skillo(SKILL_DOUBLE_BACKSTAB, "double backstab");
  skillo(SKILL_ENHANCED_DAMAGE, "enhanced damage");
  skillo(SKILL_SWITCH_TARGET, "switch target");
  skillo(SKILL_CRITICAL_HIT, "critical hit");
  skillo(SKILL_DEFENSIVE_STANCE, "defensive stance");
  skillo(SKILL_OFFENSIVE_STANCE, "offensive stance");
  skillo(SKILL_DUAL_WIELD, "dual wield");
  skillo(SKILL_SHIELD_MASTER, "shield master");
  skillo(SKILL_HUNT, "advanced track");
  skillo(SKILL_STALK, "stalk");
  skillo(SKILL_HEALTH_RECOVERY, "health recovery");
  skillo(SKILL_MAGIC_RECOVERY, "magic recovery");
  skillo(SKILL_DAMAGE_REDUCTION, "damage reduction");
  skillo(SKILL_ENHANCED_PARRY, "enhanced parry");
  skillo(SKILL_ASSAULT, "assault");
  skillo(SKILL_HEADBUTT, "headbutt");
  skillo(SKILL_TUMBLE, "tumble");
  skillo(SKILL_TAUNT, "taunt");
  skillo(SKILL_EXECUTE, "execute");
  skillo(SKILL_RAGE, "rage");
  skillo(SKILL_UNARMED_COMBAT, "unarmed combat");
  skillo(SKILL_DIRTY_TRICKS, "dirty tricks");
  skillo(SKILL_CHANT, "chant");
  skillo(SKILL_BATTLE_RYTHM, "battle rythm");
  skillo(SKILL_IMPALE, "impale");
  skillo(SKILL_REND, "rend");
  skillo(SKILL_MINCE, "mince");
  skillo(SKILL_THRUST, "thrust");
  skillo(SKILL_ACROBATICS, "acrobatics");
  skillo(SKILL_TWO_HAND, "2H Weapons");
  skillo(SKILL_L_HOOK, "left hook");
  skillo(SKILL_R_HOOK, "right hook");
  skillo(SKILL_SUCKER_PUNCH, "sucker punch");
  skillo(SKILL_UPPERCUT, "uppercut");
  skillo(SKILL_HAYMAKER, "haymaker");
  skillo(SKILL_CLOTHESLINE, "clothesline");
  skillo(SKILL_PILEDRVIER, "piledriver");
  skillo(SKILL_PALM_STRIKE, "palm strike");
  skillo(SKILL_BARD_SHRIEK, "shriek");
  skillo(SKILL_BARD_RITUAL, "ritual");
  skillo(SKILL_BARD_SCORN, "scorn");
  skillo(SKILL_BLOODBATH, "bloodbath");
  skillo(SKILL_FRENZY, "frenzy");
  skillo(SKILL_BEDSIDE_MANNER, "bedside manner");
  skillo(SKILL_GARROTTE, "garrotte");
  skillo(SKILL_DIRT_KICK, "dirt kick");
  skillo(SKILL_ELBOW, "elbow");
  skillo(SKILL_CHOP, "chop");
  skillo(SKILL_TRIP, "trip");
  skillo(SKILL_KNEE, "knee");
  skillo(SKILL_ROUNDHOUSE, "roundhouse");
  skillo(SKILL_ADRENALINE_RUSH, "adrenaline rush");
  skillo(SKILL_ARMOR_MASTER, "armor master");
  }
