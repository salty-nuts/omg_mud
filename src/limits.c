/**************************************************************************
*  File: limits.c                                          Part of tbaMUD *
*  Usage: Limits & gain funcs for HMV, exp, hunger/thirst, idle time.     *
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
#include "spells.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "interpreter.h"
#include "dg_scripts.h"
#include "class.h"
#include "fight.h"
#include "screen.h"
#include "mud_event.h"
#include "string.h"
#include "constants.h"
#include "salty.h"
/* local file scope function prototypes */
static void check_idling(struct char_data *ch);

int mana_gain(struct char_data *ch)
{
  int gain = 0;
  int mult = 1;

  if (IS_NPC(ch))
  {
    /* Neat and fast */
    gain = GET_REAL_LEVEL(ch);
  }
  else
  {
    if (GET_MANA(ch) >= GET_MAX_MANA(ch))
      return (0);

    gain = GET_REAL_LEVEL(ch);
    gain += GET_MANA_REGEN(ch);

    /* Class calculations */
    switch (GET_CLASS(ch))
    {
    case CLASS_WIZARD:
      gain += int_app[GET_INT(ch)].mana_gain;
      break;

    case CLASS_PRIEST:
      gain += wis_app[GET_WIS(ch)].mana_gain;
      break;

    case CLASS_ROGUE:
      gain += dice(2, 6);
      break;

    case CLASS_FIGHTER:
      gain += dice(2, 6);
      break;

    case CLASS_KNIGHT:
      gain += int_app[GET_INT(ch)].mana_gain;
      break;
    case CLASS_BARD:
      gain += wis_app[GET_WIS(ch)].mana_gain;
      break;
    }

    /* Skill/Spell calculations */
    if (GET_SKILL(ch, SKILL_MAGIC_RECOVERY) > 0)
    {
      gain += 2 * GET_SKILL(ch, SKILL_MAGIC_RECOVERY);
    }

    // Houses and clubs give double regen
    if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_REGEN) || ROOM_FLAGGED(IN_ROOM(ch), ROOM_HOUSE) || ROOM_FLAGGED(IN_ROOM(ch), ROOM_CONSECRATE))
      gain *= 2;

    /* Position calculations    */
    switch (GET_POS(ch))
    {
    case POS_FIGHTING: /* No regen while fighting */
      gain = 0;
      break;
    case POS_SLEEPING:
      gain += (gain / 2); /* Divide by 2 */
      break;
    case POS_RESTING:
      gain += (gain / 4); /* Divide by 2 */
      break;
    case POS_SITTING:
      gain += (gain / 8); /* Divide by 4 */
      break;
    }

    if ((GET_COND(ch, HUNGER) == 0) || (GET_COND(ch, THIRST) == 0))
      gain /= 4;
  }
  if (AFF_FLAGGED(ch, AFF_POISON))
    gain /= 4;

  return (gain);
}

/* Hitpoint gain pr. game hour */
int hit_gain(struct char_data *ch)
{
  int gain;

  if (IS_NPC(ch))
  {
    /* Neat and fast */
    gain = GET_REAL_LEVEL(ch);
  }
  else
  {
    if (GET_HIT(ch) >= GET_MAX_HIT(ch))
      return (0);

    /* Class/Level calculations */
    gain = GET_REAL_LEVEL(ch);
    gain += con_app[GET_CON(ch)].hitp;
    gain += GET_HITP_REGEN(ch);

    switch (GET_CLASS(ch))
    {
    case CLASS_WIZARD:
      gain += dice(2, 6);
      break;

    case CLASS_PRIEST:
    case CLASS_ROGUE:
      gain += dice(2, 8);
      break;

    case CLASS_FIGHTER:
      gain += con_app[GET_CON(ch)].hitp;
      break;

    case CLASS_KNIGHT:
      gain += con_app[GET_CON(ch)].hitp;
      break;
    case CLASS_BARD:
      gain += con_app[GET_CON(ch)].hitp;
      break;
    }

    /* Skill/Spell calculations */
    if (GET_SKILL(ch, SKILL_HEALTH_RECOVERY) > 0)
    {
      gain += 2 * GET_SKILL(ch, SKILL_HEALTH_RECOVERY);
    }

    // Houses and clubs give double regen
    if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_REGEN) || ROOM_FLAGGED(IN_ROOM(ch), ROOM_HOUSE) || ROOM_FLAGGED(IN_ROOM(ch), ROOM_CONSECRATE))
      gain *= 2;

    /* Position calculations    */
    switch (GET_POS(ch))
    {
    case POS_FIGHTING: /* No regen while fighting */
      gain = 0;
      break;
    case POS_SLEEPING:
      gain += (gain / 2); /* Divide by 2 */
      break;
    case POS_RESTING:
      gain += (gain / 4); /* Divide by 4 */
      break;
    case POS_SITTING:
      gain += (gain / 8); /* Divide by 8 */
      break;
    }

    if ((GET_COND(ch, HUNGER) == 0) || (GET_COND(ch, THIRST) == 0))
      gain /= 4;
  }
  if (AFF_FLAGGED(ch, AFF_POISON))
    gain /= 4;

  return (gain);
}

/* move gain pr. game hour */
int move_gain(struct char_data *ch)
{
  int gain;

  if (IS_NPC(ch))
  {
    /* Neat and fast */
    gain = GET_REAL_LEVEL(ch);
  }
  else
  {
    if (GET_MOVE(ch) >= GET_MAX_MOVE(ch))
      return (0);

    gain = GET_REAL_LEVEL(ch) + dice(1, 6);
    gain += GET_MOVE_REGEN(ch);

    /* Class/Level calculations */
    /* Skill/Spell calculations */

    // Houses and clubs give double regen
    if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_REGEN) || ROOM_FLAGGED(IN_ROOM(ch), ROOM_HOUSE) || ROOM_FLAGGED(IN_ROOM(ch), ROOM_CONSECRATE))
      gain *= 2;

    /* Position calculations    */
    switch (GET_POS(ch))
    {
    case POS_FIGHTING: /* No regen while fighting */
      gain = 0;
      break;
    case POS_SLEEPING:
      gain += (gain / 2); /* Divide by 2 */
      break;
    case POS_RESTING:
      gain += (gain / 4); /* Divide by 4 */
      break;
    case POS_SITTING:
      gain += (gain / 8); /* Divide by 8 */
      break;
    }

    if ((GET_COND(ch, HUNGER) == 0) || (GET_COND(ch, THIRST) == 0))
      gain /= 4;
  }
  if (AFF_FLAGGED(ch, AFF_POISON))
    gain /= 4;

  return (gain);
}

void set_title(struct char_data *ch, char *title)
{
  if (GET_TITLE(ch) != NULL)
    free(GET_TITLE(ch));

  if (title == NULL)
  {
    GET_TITLE(ch) = strdup(GET_SEX(ch) == SEX_FEMALE ? title_female(GET_CLASS(ch), GET_REAL_LEVEL(ch)) : title_male(GET_CLASS(ch), GET_REAL_LEVEL(ch)));
  }
  else
  {
    if (strlen(title) > MAX_TITLE_LENGTH)
      title[MAX_TITLE_LENGTH] = '\0';

    GET_TITLE(ch) = strdup(title);
  }
}

void run_autowiz(void)
{
#if defined(CIRCLE_UNIX) || defined(CIRCLE_WINDOWS)
  if (CONFIG_USE_AUTOWIZ)
  {
    size_t res;
    char buf[256];

#if defined(CIRCLE_UNIX)
    res = snprintf(buf, sizeof(buf), "nice ../bin/autowiz %d %s %d %s %d &",
                   CONFIG_MIN_WIZLIST_LEV, WIZLIST_FILE, LVL_IMMORT, IMMLIST_FILE, (int)getpid());
#elif defined(CIRCLE_WINDOWS)
    res = snprintf(buf, sizeof(buf), "autowiz %d %s %d %s",
                   CONFIG_MIN_WIZLIST_LEV, WIZLIST_FILE, LVL_IMMORT, IMMLIST_FILE);
#endif /* CIRCLE_WINDOWS */

    /* Abusing signed -> unsigned conversion to avoid '-1' check. */
    if (res < sizeof(buf))
    {
      mudlog(CMP, LVL_IMMORT, FALSE, "Initiating autowiz.");
      reboot_wizlists();
      int rval = system(buf);
      if (rval != 0)
        mudlog(BRF, LVL_IMMORT, TRUE, "Warning: autowiz failed with return value %d", rval);
    }
    else
      log("Cannot run autowiz: command-line doesn't fit in buffer.");
  }
#endif /* CIRCLE_UNIX || CIRCLE_WINDOWS */
}

void gain_exp(struct char_data *ch, long gain)
{
  int is_altered = FALSE;
  int num_levels = 0;

  if (!IS_NPC(ch) && ((GET_REAL_LEVEL(ch) < 1 || GET_REAL_LEVEL(ch) >= LVL_IMMORT)))
    return;

  if (IS_NPC(ch))
  {
    GET_EXP(ch) += gain;
    return;
  }
  if (gain > 0)
  {
    //    if ((IS_HAPPYHOUR) && (IS_HAPPYEXP))
    //      gain += (long)((float)gain * ((float)HAPPY_EXP / (float)(100)));

    gain = LMIN(CONFIG_MAX_EXP_GAIN, gain); /* put a cap on the max gain per kill */
    GET_EXP(ch) += gain;
    //    send_to_char(ch,"The actual gain is %ld!\n\r",gain);

    if (!PLR_FLAGGED(ch, PLR_MULTICLASS))
      while (GET_REAL_LEVEL(ch) < LVL_MULTICLASS && GET_EXP(ch) >= level_exp(GET_CLASS(ch), GET_REAL_LEVEL(ch) + 1))
      {
        GET_LEVEL(ch, GET_CLASS(ch)) += 1;
        num_levels++;
        advance_level(ch);
        GET_EXP(ch) -= level_exp(GET_CLASS(ch), GET_REAL_LEVEL(ch));
        is_altered = TRUE;
      }

    if (PLR_FLAGGED(ch, PLR_MULTICLASS))
      while (GET_REAL_LEVEL(ch) < LVL_MAX_MORTAL && GET_EXP(ch) >= level_exp(GET_CLASS(ch), GET_REAL_LEVEL(ch) + 1))
      {
        GET_LEVEL(ch, GET_MULTI_CLASS(ch)) += 1;
        num_levels++;
        advance_level(ch);
        GET_EXP(ch) -= level_exp(GET_CLASS(ch), GET_REAL_LEVEL(ch));
        is_altered = TRUE;
      }

    if (is_altered)
    {
      mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s advanced %d level%s to level %d.",
             GET_NAME(ch), num_levels, num_levels == 1 ? "" : "s", GET_REAL_LEVEL(ch));
      game_info("%s gained %d level%s, %s is now level %d!",
                GET_NAME(ch), num_levels, num_levels == 1 ? "" : "s", HSSH(ch), GET_REAL_LEVEL(ch));
      if (num_levels == 1)
        send_to_char(ch, "You rise a level!\r\n");
      else
        send_to_char(ch, "You rise %d levels!\r\n", num_levels);
      set_title(ch, NULL);
      if (GET_REAL_LEVEL(ch) >= LVL_IMMORT && !PLR_FLAGGED(ch, PLR_NOWIZLIST))
        run_autowiz();
    }
  }
  else if (gain < 0)
  {
    gain = MAX(-CONFIG_MAX_EXP_LOSS, gain); /* Cap max exp lost per death */
    GET_EXP(ch) += gain;
    if (GET_EXP(ch) < 0)
      GET_EXP(ch) = 0;
  }
  if (GET_REAL_LEVEL(ch) >= LVL_IMMORT && !PLR_FLAGGED(ch, PLR_NOWIZLIST))
    run_autowiz();
}

void gain_exp_regardless(struct char_data *ch, long gain)
{
  int is_altered = FALSE;
  int num_levels = 0;

  if ((IS_HAPPYHOUR) && (IS_HAPPYEXP))
    gain += (long)((float)gain * ((float)HAPPY_EXP / (float)(100)));

  GET_EXP(ch) += gain;
  if (GET_EXP(ch) < 0)
    GET_EXP(ch) = 0;

  if (!IS_NPC(ch))
  {
    if (!PLR_FLAGGED(ch, PLR_MULTICLASS))
      while (GET_REAL_LEVEL(ch) < LVL_MULTICLASS && GET_EXP(ch) >= level_exp(GET_CLASS(ch), GET_REAL_LEVEL(ch) + 1))
      {
        GET_LEVEL(ch, GET_CLASS(ch)) += 1;
        num_levels++;
        advance_level(ch);
        GET_EXP(ch) -= level_exp(GET_CLASS(ch), GET_REAL_LEVEL(ch));
        is_altered = TRUE;
      }

    if (PLR_FLAGGED(ch, PLR_MULTICLASS))
      while (GET_REAL_LEVEL(ch) < LVL_MAX_MORTAL && GET_EXP(ch) >= level_exp(GET_CLASS(ch), GET_REAL_LEVEL(ch) + 1))
      {
        GET_LEVEL(ch, GET_MULTI_CLASS(ch)) += 1;
        num_levels++;
        advance_level(ch);
        GET_EXP(ch) -= level_exp(GET_CLASS(ch), GET_REAL_LEVEL(ch));
        is_altered = TRUE;
      }

    if (is_altered)
    {
      mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s advanced %d level%s to level %d.",
             GET_NAME(ch), num_levels, num_levels == 1 ? "" : "s", GET_REAL_LEVEL(ch));
      game_info("%s gained %d level%s, %s is now level %d!",
                GET_NAME(ch), num_levels, num_levels == 1 ? "" : "s", HSSH(ch), GET_REAL_LEVEL(ch));
      if (num_levels == 1)
        send_to_char(ch, "You rise a level!\r\n");
      else
        send_to_char(ch, "You rise %d levels!\r\n", num_levels);
      set_title(ch, NULL);
    }
  }

  if (GET_REAL_LEVEL(ch) >= LVL_IMMORT && !PLR_FLAGGED(ch, PLR_NOWIZLIST))
    run_autowiz();
}

void gain_condition(struct char_data *ch, int condition, int value)
{
  bool intoxicated;

  if (IS_NPC(ch) || GET_COND(ch, condition) == -1) /* No change */
    return;

  intoxicated = (GET_COND(ch, DRUNK) > 0);

  GET_COND(ch, condition) += value;

  GET_COND(ch, condition) = MAX(0, GET_COND(ch, condition));
  GET_COND(ch, condition) = MIN(24, GET_COND(ch, condition));

  if (GET_COND(ch, condition) || PLR_FLAGGED(ch, PLR_WRITING))
    return;

  switch (condition)
  {
  case HUNGER:
    send_to_char(ch, "You are hungry.\r\n");
    break;
  case THIRST:
    send_to_char(ch, "You are thirsty.\r\n");
    break;
  case DRUNK:
    if (intoxicated)
      send_to_char(ch, "You are now sober.\r\n");
    break;
  default:
    break;
  }
}

static void check_idling(struct char_data *ch)
{
  if (ch->char_specials.timer > CONFIG_IDLE_VOID)
  {
    if (GET_WAS_IN(ch) == NOWHERE && IN_ROOM(ch) != NOWHERE)
    {
      GET_WAS_IN(ch) = IN_ROOM(ch);
      if (FIGHTING(ch))
      {
        stop_fighting(FIGHTING(ch));
        stop_fighting(ch);
      }
      act("$n disappears into the void.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char(ch, "You have been idle, and are pulled into a void.\r\n");
      save_char(ch);
      Crash_crashsave(ch);
      char_from_room(ch);
      char_to_room(ch, 1);
    }
    else if (ch->char_specials.timer > CONFIG_IDLE_RENT_TIME)
    {
      if (IN_ROOM(ch) != NOWHERE)
        char_from_room(ch);
      char_to_room(ch, 3);
      if (ch->desc)
      {
        STATE(ch->desc) = CON_DISCONNECT;
        /*
	 * For the 'if (d->character)' test in close_socket().
	 * -gg 3/1/98 (Happy anniversary.)
	 */
        ch->desc->character = NULL;
        ch->desc = NULL;
      }
      if (CONFIG_FREE_RENT)
        Crash_rentsave(ch, 0);
      else
        Crash_idlesave(ch);
      mudlog(CMP, MAX(LVL_GOD, GET_INVIS_LEV(ch)), TRUE, "%s force-rented and extracted (idle).", GET_NAME(ch));
      add_llog_entry(ch, LAST_IDLEOUT);
      extract_char(ch);
    }
  }
}

/* Update PCs, NPCs, and objects */
void point_update(void)
{
  struct char_data *i, *next_char;
  struct obj_data *j, *next_thing, *jj, *next_thing2;
  int skill;

  /* characters */
  for (i = character_list; i; i = next_char)
  {
    next_char = i->next;

    /*     gain_condition(i, HUNGER, -1);
    gain_condition(i, DRUNK, -1);
    gain_condition(i, THIRST, -1); */

    if (!IS_NPC(i))
    {
      /*
       * Salty Prompt
       * 01 FEB 2020
       */
      if (hit_gain(i) || mana_gain(i) || move_gain(i))
        send_to_char(i, "\r");
    }
    if (GET_POS(i) >= POS_STUNNED)
    {
      if (!IS_NPC(i) && GET_SKILL(i, SKILL_HEALTH_RECOVERY) > 0)
      {
        skill = SKILL_HEALTH_RECOVERY;
        if (rand_number(0, 101) > GET_SKILL(i, SKILL_HEALTH_RECOVERY))
          check_improve(i, skill, FALSE);
        else
          check_improve(i, skill, TRUE);
      }
      if (!IS_NPC(i) && GET_SKILL(i, SKILL_MAGIC_RECOVERY) > 0)
      {
        skill = SKILL_MAGIC_RECOVERY;
        if (rand_number(0, 101) > GET_SKILL(i, SKILL_MAGIC_RECOVERY))
          check_improve(i, skill, FALSE);
        else
          check_improve(i, skill, TRUE);
      }

      GET_HIT(i) = MIN(GET_HIT(i) + hit_gain(i), GET_MAX_HIT(i));
      GET_MANA(i) = MIN(GET_MANA(i) + mana_gain(i), GET_MAX_MANA(i));
      GET_MOVE(i) = MIN(GET_MOVE(i) + move_gain(i), GET_MAX_MOVE(i));
      if (AFF_FLAGGED(i, AFF_POISON))
        if (damage(i, i, 2, SPELL_POISON) == -1)
          continue; /* Oops, they died. -gg 6/24/98 */
      if (GET_POS(i) <= POS_STUNNED)
      {
        update_pos(i);
      }
    }
    else if (GET_POS(i) == POS_INCAP)
    {
      if (damage(i, i, 1, TYPE_SUFFERING) == -1)
        continue;
    }
    else if (GET_POS(i) == POS_MORTALLYW)
    {
      if (damage(i, i, 2, TYPE_SUFFERING) == -1)
        continue;
    }

    if (!IS_NPC(i))
    {
      update_char_objects(i);
      (i->char_specials.timer)++;
      if (GET_REAL_LEVEL(i) < CONFIG_IDLE_MAX_LEVEL)
        check_idling(i);
    }
  }

  /* objects */
  for (j = object_list; j; j = next_thing)
  {
    next_thing = j->next; /* Next in object list */

    /* If this is a corpse */
    if (IS_CORPSE(j))
    {
      /* timer count down */
      if (GET_OBJ_TIMER(j) > 0)
        GET_OBJ_TIMER(j)
      --;

      if (!GET_OBJ_TIMER(j))
      {

        if (j->carried_by)
          act("$p decays in your hands.", FALSE, j->carried_by, j, 0, TO_CHAR);
        else if ((IN_ROOM(j) != NOWHERE) && (world[IN_ROOM(j)].people))
        {
          act("A quivering horde of maggots consumes $p.",
              TRUE, world[IN_ROOM(j)].people, j, 0, TO_ROOM);
          act("A quivering horde of maggots consumes $p.",
              TRUE, world[IN_ROOM(j)].people, j, 0, TO_CHAR);
        }
        for (jj = j->contains; jj; jj = next_thing2)
        {
          next_thing2 = jj->next_content; /* Next in inventory */
          obj_from_obj(jj);

          if (j->in_obj)
            obj_to_obj(jj, j->in_obj);
          else if (j->carried_by)
            obj_to_room(jj, IN_ROOM(j->carried_by));
          else if (IN_ROOM(j) != NOWHERE)
            obj_to_room(jj, IN_ROOM(j));
          else
            core_dump();
        }
        extract_obj(j);
      }
    }
    /* If the timer is set, count it down and at 0, try the trigger
     * note to .rej hand-patchers: make this last in your point-update() */
    else if (GET_OBJ_TIMER(j) > 0)
    {
      GET_OBJ_TIMER(j)
      --;
      if (!GET_OBJ_TIMER(j))
        timer_otrigger(j);
    }
  }

  /* Take 1 from the happy-hour tick counter, and end happy-hour if zero */
  if (HAPPY_TIME > 1)
    HAPPY_TIME--;
  else if (HAPPY_TIME == 1) /* Last tick - set everything back to zero */
  {
    HAPPY_QP = 1000;
    HAPPY_EXP = 1000;
    HAPPY_GOLD = 1000;
    HAPPY_TIME = 1000;
    game_info("Happy hour has ended!");
  }
}

/* Note: amt may be negative */
long increase_gold(struct char_data *ch, long amt)
{
  long curr_gold;

  curr_gold = GET_GOLD(ch);

  if (amt < 0)
  {
    GET_GOLD(ch) = LMAX(0, curr_gold + amt);
    /* Validate to prevent overflow */
    if (GET_GOLD(ch) > curr_gold)
      GET_GOLD(ch) = 0;
  }
  else
  {
    GET_GOLD(ch) = LMAX(curr_gold, curr_gold + amt);
    /* Validate to prevent overflow */
    //    if (GET_GOLD(ch) < curr_gold)
    //    GET_GOLD(ch) = MAX_GOLD;
  }
  // if (GET_GOLD(ch) == MAX_GOLD)
  // send_to_char(ch, "%sYou have reached the maximum gold!\r\n%sYou must spend it or bank it before you can gain any more.\r\n", QBRED, QNRM);

  return (GET_GOLD(ch));
}

long decrease_gold(struct char_data *ch, long deduction)
{
  long amt;
  amt = (deduction * -1);
  increase_gold(ch, amt);
  return (GET_GOLD(ch));
}

long increase_bank(struct char_data *ch, long amt)
{
  long curr_bank;

  if (IS_NPC(ch))
    return 0;

  curr_bank = GET_BANK_GOLD(ch);

  if (amt < 0)
  {
    GET_BANK_GOLD(ch) = LMAX(0, curr_bank + amt);
    /* Validate to prevent overflow */
    if (GET_BANK_GOLD(ch) > curr_bank)
      GET_BANK_GOLD(ch) = 0;
  }
  else
  {
    GET_BANK_GOLD(ch) = LMAX(curr_bank, curr_bank + amt);
    /* Validate to prevent overflow */
    // if (GET_BANK_GOLD(ch) < curr_bank) GET_BANK_GOLD(ch) = MAX_BANK;
  }
  //  if (GET_BANK_GOLD(ch) == MAX_BANK)
  //  send_to_char(ch, "%sYou have reached the maximum bank balance!\r\n%sYou cannot put more into your account unless you withdraw some first.\r\n", QBRED, QNRM);
  return (GET_BANK_GOLD(ch));
}

long decrease_bank(struct char_data *ch, long deduction)
{
  long amt;
  amt = (deduction * -1);
  increase_bank(ch, amt);
  return (GET_BANK_GOLD(ch));
}
