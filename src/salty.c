/**************************************************************************
File: salty.c

Updated: 08 MAR 2020
Updated: 25 JUN 2021
Updated: 21 JUL 2021
Updated: 22 JUL 2021

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

**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "constants.h"
#include "act.h"
#include "screen.h"
#include "spec_procs.h"
#include "class.h"
#include "fight.h"
#include "modify.h"
#include "mud_event.h"
#include "salty.h"
#include "shop.h"
/*
  Metagame cost

  Salty
  06 JAN 2019
*/
#define MOVE_EXP (100000)
#define TRAIN_EXP (1000000)
#define RANK_ONE (long)500000000

#define STRADD_EXP(ch) ((1 + GET_REAL_ADD(ch)) * 100000)
#define STR_EXP(ch) (GET_REAL_STR(ch) * 100000)
#define INT_EXP(ch) (GET_REAL_INT(ch) * 100000)
#define WIS_EXP(ch) (GET_REAL_WIS(ch) * 100000)
#define DEX_EXP(ch) (GET_REAL_DEX(ch) * 100000)
#define CON_EXP(ch) (GET_REAL_CON(ch) * 100000)
#define LUCK_EXP(ch) (GET_REAL_LUCK(ch) * 100000)

#define IMPROVE_COST 10000000

/*
  Metagame Menu

  Salty
  06 JAN 2019
*/
void list_meta(struct char_data *ch)
{
  long hitp_exp, hitp_gold, mana_exp, mana_gold, train_exp, train_gold;
  mana_gold = hitp_gold = train_gold = 0;

  switch (GET_CLASS(ch))
  {
  case CLASS_WIZARD:
    hitp_exp = (long)GET_MAX_HIT(ch) * 15000;
    mana_exp = (long)GET_MAX_MANA(ch) * 5000;
    break;
  case CLASS_PRIEST:
    hitp_exp = (long)GET_MAX_HIT(ch) * 12500;
    mana_exp = (long)GET_MAX_MANA(ch) * 6250;
    break;
  case CLASS_ROGUE:
    hitp_exp = (long)GET_MAX_HIT(ch) * 10000;
    mana_exp = (long)GET_MAX_MANA(ch) * 10000;
    break;
  case CLASS_FIGHTER:
    hitp_exp = (long)GET_MAX_HIT(ch) * 8750;
    mana_exp = (long)GET_MAX_MANA(ch) * 100000;
    break;
  case CLASS_KNIGHT:
    hitp_exp = (long)GET_MAX_HIT(ch) * 5000;
    mana_exp = (long)GET_MAX_MANA(ch) * 15000;
    break;
  case CLASS_BARD:
    hitp_exp = (long)GET_MAX_HIT(ch) * 10000;
    mana_exp = (long)GET_MAX_MANA(ch) * 7500;
    break;
  default:
    hitp_exp = (long)GET_MAX_HIT(ch) * 10000;
    mana_exp = (long)GET_MAX_MANA(ch) * 10000;
    break;
  }

  hitp_gold = hitp_exp / 50;
  mana_gold = mana_exp / 50;
  train_exp = (GET_TRAIN_META(ch) + 1) * TRAIN_EXP;
  train_gold = train_exp / 10;

  send_to_char(ch, "\n\r");
  send_to_char(ch, "You have %s experience points.\n\r", add_commas(GET_EXP(ch)));
  send_to_char(ch, "You can metagame the following stats:\r\n\n\r");
  send_to_char(ch, "[1] Increase hitpoints:   %12s exp, %12s gold.\n\r", add_commas(hitp_exp), add_commas(hitp_gold));
  send_to_char(ch, "[2] Increase mana:        %12s exp, %12s gold.\n\r", add_commas(mana_exp), add_commas(mana_gold));
  send_to_char(ch, "[3] Increase movement:    %12s exp\n\r", add_commas(MOVE_EXP));
  send_to_char(ch, "[4] Increase trains:      %12s exp, %12s gold.\n\r", add_commas(train_exp), add_commas(train_gold));

  send_to_char(ch, "\n\r    Syntax: metagame <number>\n\r");
}

bool rank_display(struct char_data *ch, char *arg)
{
  char buf[MAX_STRING_LENGTH];
  GET_RANK(ch) = rank_level(GET_TOTAL_EXP(ch));
  if (GET_RANK(ch) > 0)
  {
    sprintf(buf, "[R: %3d]", GET_RANK(ch));
    strcat(buf, " ");
    strcpy(arg, buf);
    return TRUE;
  }
  else
    strcpy(arg, "");
  return FALSE;
}
/*
 * Rewrite of rank_exp()
   Salty
   08 JAN 2019
 */
void rank_exp(struct char_data *ch, long exp)
{
  if (IS_NPC(ch))
    return;
  if (ch->points.exp < exp)
    return;

  ch->points.exp -= exp;

  GET_TOTAL_EXP(ch) += exp;

  if (rank_level(GET_TOTAL_EXP(ch)) > GET_RANK(ch))
  {
    GET_RANK(ch) = rank_level(GET_TOTAL_EXP(ch));
    GET_TRAINS(ch) += dice(1, MIN(10, GET_RANK(ch)));
  }
  if (GET_RANK(ch) >= 10)
  {
    GET_COND(ch, THIRST) = -1;
    GET_COND(ch, HUNGER) = -1;
    GET_COND(ch, DRUNK) = -1;
  }
  save_char(ch);
}

/*
 * Here we figure out the ranks
 * Salty
 * 21 NOV 2019
 *
*/
int rank_level(long total)
{
  int rank = 0;
  int i = 1;
  long rank_cost = RANK_ONE * i;

  while (total > rank_cost)
  {
    total -= rank_cost;
    rank++;
    i++;
    rank_cost = RANK_ONE * i;
  }
  return rank;
}

long rank_remaining(long total)
{
  int i = 1;
  long rank_cost = RANK_ONE * i;

  while (total > rank_cost)
  {
    total -= rank_cost;
    i++;
    rank_cost = RANK_ONE * i;
  }
  return (rank_cost - total);
}

ACMD(do_listrank)
{
  long rank_cost = RANK_ONE;
  int i = 1;
  long total = 0;

  while (i < 100)
  {
    rank_cost = RANK_ONE * i;
    total += rank_cost;
    send_to_char(ch, "Rank:[%3d]  Rank Cost:[%15s]  Total Exp:[%15s]\n\r", i, add_commas(rank_cost), add_commas(total));
    i++;
  }
}

ACMD(do_metagame)
{
  long hitp_exp, hitp_gold, mana_exp, mana_gold, train_exp, train_gold;
  int hroll = con_app[GET_CON(ch)].meta_hp;
  int mroll = int_app[GET_INT(ch)].meta_mana;
  int vroll = 100;
  int arg = 0;

  mana_gold = hitp_gold = 0;
  skip_spaces(&argument);

  if (!*argument)
  {
    list_meta(ch);
    rank_exp(ch, 0);
    return;
  }
  arg = atoi(argument);

  switch (GET_CLASS(ch))
  {
  case CLASS_WIZARD:
    mroll = dice(1, int_app[GET_INT(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    hitp_exp = (long)GET_MAX_HIT(ch) * 15000;
    mana_exp = (long)GET_MAX_MANA(ch) * 5000;
    break;
  case CLASS_PRIEST:
    mroll = dice(1, int_app[GET_WIS(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    hitp_exp = (long)GET_MAX_HIT(ch) * 12500;
    mana_exp = (long)GET_MAX_MANA(ch) * 6250;
    break;
  case CLASS_ROGUE:
    mroll = dice(1, int_app[GET_INT(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    hitp_exp = (long)GET_MAX_HIT(ch) * 10000;
    mana_exp = (long)GET_MAX_MANA(ch) * 10000;
    break;
  case CLASS_FIGHTER:
    mroll = dice(1, int_app[GET_INT(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    hitp_exp = (long)GET_MAX_HIT(ch) * 8750;
    mana_exp = (long)GET_MAX_MANA(ch) * 100000;
    break;
  case CLASS_KNIGHT:
    mroll = dice(1, int_app[GET_WIS(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    hitp_exp = (long)GET_MAX_HIT(ch) * 5000;
    mana_exp = (long)GET_MAX_MANA(ch) * 15000;
    break;
  case CLASS_BARD:
    mroll = dice(1, int_app[GET_WIS(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    hitp_exp = (long)GET_MAX_HIT(ch) * 10000;
    mana_exp = (long)GET_MAX_MANA(ch) * 7500;
    break;
  default:
    mroll = dice(1, int_app[GET_INT(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    hitp_exp = (long)GET_MAX_HIT(ch) * 10000;
    mana_exp = (long)GET_MAX_MANA(ch) * 10000;
    break;
  }

  mana_gold = mana_exp / 50;
  hitp_gold = hitp_exp / 50;
  train_exp = (GET_TRAIN_META(ch) + 1) * TRAIN_EXP;
  train_gold = train_exp / 10;

  switch (arg)
  {
  case 1:
  {
    if (GET_EXP(ch) < hitp_exp)
    {
      send_to_char(ch, "You need %s more experience points to metagame your HP.\n\r", add_commas(hitp_exp - GET_EXP(ch)));
      return;
    }
    else if (GET_GOLD(ch) < hitp_gold)
    {
      send_to_char(ch, "You need %s more gold coins to metagame your HP.\n\r", add_commas(hitp_gold - GET_GOLD(ch)));
      return;
    }
    else
    {
      send_to_char(ch, "You manage to metagame and increase your HP by %d!\n\r", hroll);
      GET_HIT(ch) += hroll;
      GET_MAX_HIT(ch) += hroll;
      GET_GOLD(ch) -= hitp_gold;
      rank_exp(ch, hitp_exp);
      return;
    }
  }
  case 2:
  {
    if (GET_EXP(ch) < mana_exp)
    {
      send_to_char(ch, "You need %s more experience points to metagame your Mana.\n\r", add_commas(mana_exp - GET_EXP(ch)));
      return;
    }
    else if (GET_GOLD(ch) < mana_gold)
    {
      send_to_char(ch, "You need %s more gold coins to metagame your mana.\n\r", add_commas(mana_gold - GET_GOLD(ch)));
      return;
    }
    else
    {
      send_to_char(ch, "You manage to metagame and increase your Mana by %d!\n\r", mroll);
      GET_MANA(ch) += mroll;
      GET_MAX_MANA(ch) += mroll;
      GET_GOLD(ch) -= mana_gold;
      rank_exp(ch, mana_exp);
      return;
    }
  }
  case 3:
  {
    if (GET_EXP(ch) < MOVE_EXP)
    {
      send_to_char(ch, "You need %s more experience points to metagame your Move.\n\r", add_commas(MOVE_EXP - GET_EXP(ch)));
      return;
    }
    else
    {
      send_to_char(ch, "You manage to metagame and increase your Move by %d!\n\r", vroll);
      GET_MOVE(ch) += vroll;
      GET_MAX_MOVE(ch) += vroll;
      rank_exp(ch, MOVE_EXP);
      return;
    }
  }
  case 4:
  {
    int trns = dice(1, 4);
    if (GET_EXP(ch) < train_exp)
    {
      send_to_char(ch, "You need %s more experience points to metagame your trains.\n\r", add_commas(train_exp - GET_EXP(ch)));
      return;
    }
    else if (GET_GOLD(ch) < train_gold)
    {
      send_to_char(ch, "You need %s more gold coins to metagame your trains.\n\r", add_commas(train_gold - GET_GOLD(ch)));
      return;
    }
    else
    {
      send_to_char(ch, "You manage to metagame and increase your trains by %d!\n\r", trns);
      GET_TRAINS(ch) += trns;
      GET_TRAIN_META(ch) += trns;
      rank_exp(ch, train_exp);
      return;
    }
  }
  default:
  {
    send_to_char(ch, "There is nothing like that to metagame!\n\r");
    return;
  }
  }
  return;
}
/*
  Salty
  06 JAN 2019
*/
ACMD(do_stalk)
{
  char buf[MAX_STRING_LENGTH];
  struct char_data *victim;
  int location;

  one_argument(argument, buf);

  if (rand_number(0, 101) >= GET_SKILL(ch, SKILL_STALK))
  {
    return;
    check_improve(ch, SKILL_STALK, FALSE);
  }
  if (!*buf)
    send_to_char(ch, "Who do you wish to stalk?\n\r");
  else if (!(victim = get_char_vis(ch, buf, NULL, FIND_CHAR_WORLD)))
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  else
  {
    location = IN_ROOM(victim);
    if (GET_REAL_LEVEL(victim) > GET_REAL_LEVEL(ch))
    {
      send_to_char(ch, "Don't even think about it.\n\r");
      return;
    }
    if (IS_NPC(victim))
    {
      send_to_char(ch, "You cannot stalk NPC, try track instead.\n\r");
      return;
    }
    if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PRIVATE))
    {

      send_to_char(ch, "Something about this room prevents you from leaving!\r\n");
      return;
    }
    // Rooms we cannot relocate to
    if (ROOM_FLAGGED(location, ROOM_PRIVATE) || ROOM_FLAGGED(location, ROOM_TUNNEL) ||
        ROOM_FLAGGED(location, ROOM_GODROOM) || ROOM_FLAGGED(location, ROOM_NOTRACK))
    {
      send_to_char(ch, "Something prevents you from reaching that player!\r\n");
      return;
    }
    act("$n hurries off..", true, ch, 0, 0, TO_ROOM);

    char_from_room(ch);
    char_to_room(ch, location);

    act("$n enters the room..", true, ch, 0, 0, TO_ROOM);
    send_to_char(ch, "You slide up next to %s!\n\r", GET_NAME(victim));
    look_at_room(ch, 0);
  }
}

ACMD(do_stance)
{
  if (IS_NPC(ch))
    return;
  char arg[MAX_INPUT_LENGTH];
  one_argument(argument, arg);

  if (!*arg)
  {
    send_to_char(ch, "Usage: stance <type>\n\r");
    send_to_char(ch, "Types: offense / defense / none\r\n");
    return;
  }
  if (!str_cmp("offense", arg) || !str_cmp("off", arg))
  {
    if (!GET_SKILL(ch, SKILL_OFFENSIVE_STANCE))
    {
      send_to_char(ch, "You don't have any skill in offensive stance.");
      return;
    }
    if (!AFF_FLAGGED(ch, AFF_OFFENSE))
    {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_DEFENSE);
      SET_BIT_AR(AFF_FLAGS(ch), AFF_OFFENSE);
      send_to_char(ch, "You shift into an offensive fighting stance!\n\r");
    }
    else if (AFF_FLAGGED(ch, AFF_OFFENSE))
    {
      act("You are already in an offensive fighting stance.", true, ch, 0, 0, TO_CHAR);
      return;
    }
    save_char(ch);
  }
  else if (!str_cmp("defense", arg) || !str_cmp("def", arg))
  {
    if (!GET_SKILL(ch, SKILL_DEFENSIVE_STANCE))
    {
      send_to_char(ch, "You don't have any skill in defensive stance.");
      return;
    }

    if (!AFF_FLAGGED(ch, AFF_DEFENSE))
    {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_OFFENSE);
      SET_BIT_AR(AFF_FLAGS(ch), AFF_DEFENSE);
      send_to_char(ch, "You shift into an defensive fighting stance!\n\r");
    }
    else if (AFF_FLAGGED(ch, AFF_DEFENSE))
    {
      act("You are already in a defensive fighting stance.", true, ch, 0, 0, TO_CHAR);
      return;
    }
    save_char(ch);
  }
  else if (!str_cmp("none", arg))
  {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_OFFENSE);
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_DEFENSE);
    send_to_char(ch, "You relax from all fighting stances.\n\r");
    act("$n is relaxed and no longer on guard.", true, ch, 0, 0, TO_ROOM);
    save_char(ch);
  }
}


ACMD(do_chant)
{
  if (IS_NPC(ch))
    return;
  char arg[MAX_INPUT_LENGTH];
  one_argument(argument, arg);

  if (!GET_SKILL(ch, SKILL_CHANT))
  {
    send_to_char(ch, "You don't have the ability to chant.");
    return;
  }

  if (!*arg)
  {
    send_to_char(ch, "Usage: chant <type>\n\r");
    send_to_char(ch, "Types: harmony / dissonance / none\r\n");
    return;
  }
  if (!str_cmp("harmony", arg) || !str_cmp("har", arg))
  {
    if (!AFF_FLAGGED(ch, AFF_HARMONY))
    {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_DISSONANCE);
      SET_BIT_AR(AFF_FLAGS(ch), AFF_HARMONY);
      send_to_char(ch, "You begin chanting a harmonic chant!\n\r");
    }
    else if (AFF_FLAGGED(ch, AFF_HARMONY))
    {
      act("You are already chanting a harmonic chant!", true, ch, 0, 0, TO_CHAR);
      return;
    }
    save_char(ch);
  }
  else if (!str_cmp("dissonance", arg) || !str_cmp("dis", arg))
  {
    if (!AFF_FLAGGED(ch, AFF_DISSONANCE))
    {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HARMONY);
      SET_BIT_AR(AFF_FLAGS(ch), AFF_DISSONANCE);
      send_to_char(ch, "You begin chanting a dissonant chant!\n\r");
    }
    else if (AFF_FLAGGED(ch, AFF_DISSONANCE))
    {
      act("You are already chanting a dissonant chant!", true, ch, 0, 0, TO_CHAR);
      return;
    }
    save_char(ch);
  }
  else if (!str_cmp("none", arg))
  {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HARMONY);
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_DISSONANCE);
    send_to_char(ch, "You stop chanting.\n\r");
    act("$n is no longert chanting.", true, ch, 0, 0, TO_ROOM);
    save_char(ch);
  }
}

ACMD(do_improve)
{
  char arg[MAX_INPUT_LENGTH];
  struct obj_data *obj;
  int sum, exp;

  one_argument(argument, arg);

  if (!*arg)
  {
    send_to_char(ch, "Usage: improve <weapon>\r\n");
    send_to_char(ch, "Improve the weapon dice XdY.\r\n");
    send_to_char(ch, "Cost: 100,000,000 experience points\r\n");
    return;
  }
  else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying)))
  {
    send_to_char(ch, "You aren't carrying %s.\n\r", AN(arg));
    return;
  }
  else
  {
    sum = GET_OBJ_VAL(obj, 1) + GET_OBJ_VAL(obj, 2);
    exp = sum * IMPROVE_COST;

    if (!GET_SKILL(ch, SKILL_IMPROVE))
    {
      send_to_char(ch, "You have no idea how to sharpen things.\n\r");
      return;
    }
    if (GET_OBJ_TYPE(obj) != ITEM_WEAPON)
    {
      send_to_char(ch, "That is not a weapon!\n\r");
      return;
    }
    if (GET_EXP(ch) < exp)
    {
      send_to_char(ch, "You need %s more experience points before you can improve your weapon!\n\r", add_commas(exp - GET_EXP(ch)));
      return;
    }
    if (OBJ_FLAGGED(obj, ITEM_IMPROVED))
    {
      send_to_char(ch, "That weapon is sharpened already!\n\r");
      return;
    }
    send_to_char(ch, "%s has has Damage Dice '%dD%d'\n\r", obj->short_description, GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 2));
    send_to_char(ch, "You spend %s experience points improving %s!\n\r", add_commas(exp), obj->short_description);

    WAIT_STATE(ch, PULSE_VIOLENCE);

    ch->points.exp -= exp;
    sum = GET_OBJ_VAL(obj, 1) + GET_OBJ_VAL(obj, 2);

    if (GET_SKILL(ch, SKILL_IMPROVE) > rand_number(1, 101))
    {
      GET_OBJ_OWNER(obj) = GET_NAME(ch);
      GET_OBJ_VAL(obj, 1) += 1;
      GET_OBJ_VAL(obj, 2) += 1;
      send_to_char(ch, "You improved %s, it now has Damage Dice '%dD%d'\n\r", obj->short_description, GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 2));
      act("$p can still be improved!.", FALSE, ch, obj, 0, TO_ROOM);
      act("$p can still be improved!.", FALSE, ch, obj, 0, TO_CHAR);
    }
    else
    {
      SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_IMPROVED);
      act("$p cannot be improved.", FALSE, ch, obj, 0, TO_ROOM);
      act("$p cannot be improved.", FALSE, ch, obj, 0, TO_CHAR);
    }
  }
}

void circle_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101); /* 101% is a complete failure */
  int prob = GET_SKILL(ch, SKILL_CIRCLE);

  if (!FIGHTING(ch) || !FIGHTING(victim))
    return;

  // failed
  if (AWAKE(victim) && (percent > prob))
  {
    damage(ch, victim, 0, SKILL_CIRCLE);
    check_improve(ch, SKILL_CIRCLE, FALSE);
  }
  else // success
  {
    hit(ch, victim, SKILL_CIRCLE);
    check_improve(ch, SKILL_CIRCLE, TRUE);

    if ((victim && GET_HIT(victim) > 0) && (GET_SKILL(ch, SKILL_DOUBLE_BACKSTAB) > 0))
    {
      percent = rand_number(1, 101); // 101 is complete failure
      prob = GET_SKILL(ch, SKILL_DOUBLE_BACKSTAB);
      if (percent > prob)
      {
        send_to_char(ch, "You failed to execute a double circle!\n\r");
        check_improve(ch, SKILL_DOUBLE_BACKSTAB, FALSE);
      }
      else
      {
        act("$N spasms in pain as and is STUNNED you slam your weapon in $S back.", false, ch, 0, victim, TO_CHAR);
        act("Suddenly $n stabs you in the back, AGAIN!", false, ch, 0, victim, TO_VICT);
        act("$n plunges $s weapon into the back of $N, AGAIN!", false, ch, 0, victim, TO_NOTVICT);
        send_to_char(ch, "You manage to execute a perfect double cicle!\n\r");
        GET_POS(FIGHTING(ch)) = POS_STUNNED;
        hit(ch, victim, SKILL_CIRCLE);
        check_improve(ch, SKILL_CIRCLE, TRUE);
        check_improve(ch, SKILL_DOUBLE_BACKSTAB, TRUE);
        /*
        if ( ( victim && GET_HIT(victim) > 0 )  &&  ( GET_SKILL(ch, SKILL_EXECUTE) > rand_number(1,500) ) )
        {
          act("$N stiffens, $s eyes widen before they die at your feet.  EXECUTION!!", false, ch, 0, victim, TO_CHAR);
          act("$n executes you like a prisoner!", false, ch, 0, victim, TO_VICT);
          act("$n sneaks up behind $N and executes $M!", false, ch, 0, victim, TO_ROOM);
          hit(ch,victim,SKILL_EXECUTE);
          check_improve(ch,SKILL_EXECUTE,TRUE);
          return;
        }
        else
          check_improve(ch,SKILL_EXECUTE, FALSE);
*/
      }
    }
  }
}
ACMD(do_circle)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_CIRCLE))
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (!FIGHTING(ch))
  {
    send_to_char(ch, "You must be fighting to do this!\n\r");
    return;
  }

  one_argument(argument, arg);

  if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
    {
      victim = FIGHTING(ch);
    }
    else
    {
      send_to_char(ch, "Circle who?\r\n");
      return;
    }
  }
  if (victim == ch)
  {
    send_to_char(ch, "How can you circle behind yourself?\r\n");
    return;
  }
  if (!CONFIG_PK_ALLOWED && !IS_NPC(victim) && !IS_NPC(ch))
    return;

  if (MOB_FLAGGED(victim, MOB_AWARE) && AWAKE(victim))
  {
    send_to_char(ch, "%s is too alert, you can't sneak up!\n\r", GET_NAME(victim));
    return;
  }
  if (!GET_EQ(ch, WEAR_WIELD))
  {
    send_to_char(ch, "You need to wield a weapon to make it a success.\r\n");
    return;
  }
  /* peaceful rooms */
  if (ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  /* shopkeeper and MOB_NOKILL protection */
  if (!ok_damage_shopkeeper(ch, victim) || MOB_FLAGGED(victim, MOB_NOKILL))
  {
    send_to_char(ch, "This mob is protected.\r\n");
    return;
  }

  circle_combat(ch, victim);

  WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
}

ACMD(do_backstab)
{
  char buf[MAX_INPUT_LENGTH];
  struct char_data *victim;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BACKSTAB))
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  one_argument(argument, buf);

  if (!(victim = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM)))
  {
    send_to_char(ch, "Backstab who?\r\n");
    return;
  }
  if (!CONFIG_PK_ALLOWED && !IS_NPC(victim) && !IS_NPC(ch))
    return;

  if (victim == ch)
  {
    send_to_char(ch, "How can you sneak up on yourself?\r\n");
    return;
  }
  if (!GET_EQ(ch, WEAR_WIELD))
  {
    send_to_char(ch, "You need to wield a weapon to make it a success.\r\n");
    return;
  }
  /* peaceful rooms */
  if (ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  /* shopkeeper and MOB_NOKILL protection */
  if (!ok_damage_shopkeeper(ch, victim) || MOB_FLAGGED(victim, MOB_NOKILL))
  {
    send_to_char(ch, "This mob is protected.\r\n");
    return;
  }

  if (FIGHTING(victim))
  {
    send_to_char(ch, "You can't backstab a fighting person -- they're too alert!\r\n");
    return;
  }

  if (MOB_FLAGGED(victim, MOB_AWARE) && AWAKE(victim))
  {
    act("You notice $N lunging at you!", FALSE, victim, 0, ch, TO_CHAR);
    act("$e notices you lunging at $m!", FALSE, victim, 0, ch, TO_VICT);
    act("$n notices $N lunging at $m!", FALSE, victim, 0, ch, TO_NOTVICT);
    hit(victim, ch, TYPE_UNDEFINED);
    return;
  }

  percent = rand_number(1, 101); /* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_BACKSTAB);

  if (AWAKE(victim) && (percent > prob))
  {
    damage(ch, victim, 0, SKILL_BACKSTAB);
    check_improve(ch, SKILL_BACKSTAB, FALSE);
    WAIT_STATE(ch, PULSE_VIOLENCE + 2);
    return;
  }
  if ((victim && GET_HIT(victim) > 0) && (GET_SKILL(ch, SKILL_EXECUTE) > rand_number(1, 200)))
  {
    act("$N stiffens, $s eyes widen before they die at your feet.  EXECUTION!!", false, ch, 0, victim, TO_CHAR);
    act("$n executes you like a prisoner!", false, ch, 0, victim, TO_VICT);
    act("$n sneaks up behind $N and executes $M!", false, ch, 0, victim, TO_ROOM);
    hit(ch, victim, SKILL_EXECUTE);
    check_improve(ch, SKILL_EXECUTE, TRUE);
    return;
  }
  check_improve(ch, SKILL_EXECUTE, FALSE);

  hit(ch, victim, SKILL_BACKSTAB);
  check_improve(ch, SKILL_BACKSTAB, TRUE);

  if ((victim && GET_HIT(victim) > 0) && (GET_SKILL(ch, SKILL_DOUBLE_BACKSTAB) > 0))
  {
    percent = rand_number(1, 101); // 101 is complete failure
    prob = GET_SKILL(ch, SKILL_DOUBLE_BACKSTAB);
    if (percent > prob)
    {
      send_to_char(ch, "You failed to execute a double backstab!\n\r");
      check_improve(ch, SKILL_DOUBLE_BACKSTAB, FALSE);
    }
    else
    {
      send_to_char(ch, "You execute a perfect double backstab!\n\r");
      act("$N spasms in pain as and is STUNNED you slam your weapon in $S back.", false, ch, 0, victim, TO_CHAR);
      act("Suddenly $n stabs you in the back, AGAIN!", false, ch, 0, victim, TO_VICT);
      act("$n plunges $s weapon into the back of $N, AGAIN!", false, ch, 0, victim, TO_NOTVICT);
      GET_POS(FIGHTING(ch)) = POS_STUNNED;
      hit(ch, victim, SKILL_BACKSTAB);
      check_improve(ch, SKILL_BACKSTAB, TRUE);
      check_improve(ch, SKILL_DOUBLE_BACKSTAB, TRUE);
    }
  }
  WAIT_STATE(ch, PULSE_VIOLENCE + 2);
}
ACMD(do_assault)
{
  char buf[MAX_INPUT_LENGTH];
  struct char_data *victim;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_ASSAULT))
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  one_argument(argument, buf);

  if (!(victim = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM)))
  {
    send_to_char(ch, "Assault whom?\r\n");
    return;
  }
  if (victim == ch)
  {
    send_to_char(ch, "How can you sneak up on yourself?\r\n");
    return;
  }
  if (!CONFIG_PK_ALLOWED && !IS_NPC(victim) && !IS_NPC(ch))
    return;

  if (!GET_EQ(ch, WEAR_WIELD))
  {
    send_to_char(ch, "You need to wield a weapon.\r\n");
    return;
  }
  /* peaceful rooms */
  if (ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  /* shopkeeper and MOB_NOKILL protection */
  if (!ok_damage_shopkeeper(ch, victim) || MOB_FLAGGED(victim, MOB_NOKILL))
  {

    send_to_char(ch, "This mob is protected.\r\n");
    return;
  }

  if (MOB_FLAGGED(victim, MOB_AWARE) && AWAKE(victim))
  {
    act("You notice $N lunging at you!", FALSE, victim, 0, ch, TO_CHAR);
    act("$e notices you lunging at $m!", FALSE, victim, 0, ch, TO_VICT);
    act("$n notices $N lunging at $m!", FALSE, victim, 0, ch, TO_NOTVICT);
    hit(victim, ch, TYPE_UNDEFINED);
    return;
  }

  percent = rand_number(1, 101); /* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_ASSAULT);

  if (AWAKE(victim) && (percent > prob))
  {
    damage(ch, victim, 0, SKILL_ASSAULT);
    check_improve(ch, SKILL_ASSAULT, FALSE);
  }
  else
  {
    hit(ch, victim, SKILL_ASSAULT);
    check_improve(ch, SKILL_ASSAULT, TRUE);
  }
  WAIT_STATE(ch, PULSE_VIOLENCE + 2);
}

/*
 * Hasty rewrite of check_improve
 * Salty
 * 11 NOV 2019
 */
void check_improve(struct char_data *ch, int sn, bool success)
{
  int need, gain, roll_s, roll_f, base;
  int class_sanity = 0;

  if (IS_NPC(ch))
    return;

  /* Salty's Multiclass Addition
	 * 04 FEB 2020
	 * Skill is not known by class or multiclass.
	 */
  for (int i = 0; i < NUM_CLASSES; i++)
  {
    if (GET_LEVEL(ch, i) < spell_info[sn].min_level[i])
    {
      class_sanity++;
    }
  }
  /* Skill is not known by class */
  if (class_sanity > (NUM_CLASSES - 1))
    return;

  /* Skill is not practiced or skill is maxed */
  if (GET_SKILL(ch, sn) == 0 || GET_SKILL(ch, sn) == MAX_SKILL(ch))
    return;

  /* Base for spells */
  if (sn > 0 && sn <= NUM_SPELLS)
  {
    if (spell_info[sn].violent)
      base = 5 * MAX_SKILL(ch);
    else
      base = 3 * MAX_SKILL(ch);
  }
  /* Base for skills */
  else if (sn > MAX_SPELLS && sn <= MAX_SPELLS + NUM_SKILLS)
    base = 5 * MAX_SKILL(ch);
  else
    base = 5 * MAX_SKILL(ch);

  if (GET_CLASS(ch) == CLASS_ROGUE || GET_MULTI_CLASS(ch) == CLASS_ROGUE)
    need = MAX_SKILL(ch) - GET_SKILL(ch, sn) + GET_LUCK(ch) * 2;
  else
    need = MAX_SKILL(ch) - GET_SKILL(ch, sn);

  /* We need a random number size for successful roll */
  roll_s = MAX(0, base - (2 * GET_TOTAL(ch)));

  /* We need a random number size for failure roll */
  roll_f = MAX(0, base - (3 * GET_TOTAL(ch)));

  //	send_to_char(ch, "base = %d, need = %d, roll_s = %d, roll_f = %d\n\r",base, need, roll_s,roll_f);
  /* Was the skill check a success? */
  if (success)
  {
    if (need > rand_number(0, roll_s))
    {
      gain = GET_SKILL(ch, sn);
      gain++;
      SET_SKILL(ch, sn, MIN(MAX_SKILL(ch), gain));
      send_to_char(ch, "You have become better at %s! (%d%%)\n\r", spell_info[sn].name, GET_SKILL(ch, sn));

      if (GET_SKILL(ch, sn) >= MAX_SKILL(ch))
        send_to_char(ch, "You are now learned in at %s.\r\n", spell_info[sn].name);
    }
  }
  else /* Whatever skill, we failed it.  Lets try to learn from our mistakes. */
  {
    if (need > rand_number(0, roll_f))
    {
      gain = GET_SKILL(ch, sn);
      gain += dice(1, 4) + 1;

      SET_SKILL(ch, sn, MIN(MAX_SKILL(ch), gain));
      send_to_char(ch, "You learn from your mistakes, and your %s skill improves. (%d%%)\n\r", spell_info[sn].name, GET_SKILL(ch, sn));

      if (GET_SKILL(ch, sn) >= MAX_SKILL(ch))
        send_to_char(ch, "You are now learned in at %s.\r\n", spell_info[sn].name);
    }
  }
}

ACMD(do_multiclass)
{
  char arg[MAX_INPUT_LENGTH];
  int class_num;

  one_argument(argument, arg);

  if (!*arg)
  {
    send_to_char(ch, "Syntax: multiclass <class name>\n\r");
    return;
  }
  if ((class_num = parse_class(*arg)) == CLASS_UNDEFINED)
  {
    send_to_char(ch, "That is not a class.\r\n");
    return;
  }
  if (class_num == GET_CLASS(ch))
  {
    send_to_char(ch, "You are a %s, you cannot multiclass into the same class!\n\r", pc_class_types[GET_CLASS(ch)]);
    return;
  }
  if (PLR_FLAGGED(ch, PLR_MULTICLASS))
  {
    send_to_char(ch, "You have already selected %s as your multiclass.\n\r", pc_class_types[GET_MULTI_CLASS(ch)]);
    return;
  }

  GET_MULTI_CLASS(ch) = class_num;
  SET_BIT_AR(PLR_FLAGS(ch), PLR_MULTICLASS);
  send_to_char(ch, "Your multiclass is now %s!\n\r", pc_class_types[GET_MULTI_CLASS(ch)]);
  mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s has selected %s as %s multiclass.",
         GET_NAME(ch), pc_class_types[GET_MULTI_CLASS(ch)], HSHR(ch));
  game_info("%s has selected %s as %s multiclass.", GET_NAME(ch), pc_class_types[GET_MULTI_CLASS(ch)], HSHR(ch));
  return;
}

/* Return spell number if a char is affected by a spell (SPELL_XXX), FALSE indicates
 * not affected. */
int spell_type(struct char_data *ch, int type)
{
  struct affected_type *hjp;

  for (hjp = ch->affected; hjp; hjp = hjp->next)
    if (hjp->spell == type)
      return type;

  return (FALSE);
}

void headbutt_combat(struct char_data *ch, struct char_data *victim)
{
  byte percent = rand_number(1, 101);
  int roll = 0;

  if (!FIGHTING(ch) || !FIGHTING(victim))
    return;

  if (percent > GET_SKILL(ch, SKILL_HEADBUTT))
  {
    act("You try to headbutt $N but miss.  How embarassing!", false, ch, 0, victim, TO_CHAR);
    act("You evade a grab and $n misses $s headbutt.", false, ch, 0, victim, TO_VICT);
    act("$n tries to grab $N for a headbutt, but misses and hits the ground.", false, ch, 0, victim, TO_NOTVICT);
    damage(ch, victim, 0, TYPE_CRUSH);
    check_improve(ch, SKILL_HEADBUTT, FALSE);
  }
  else
  {
    roll = rand_number(1, 20);
    if (roll <= 15)
    {
      act("You headbutt $N, slamming your head against $M.", false, ch, 0, victim, TO_CHAR);
      act("$n grabs you and.....AHHH!!!! BLOOD EVERYWHERE!", false, ch, 0, victim, TO_VICT);
      act("$n smashes $s head against $N.", false, ch, 0, victim, TO_NOTVICT);
      hit(ch, victim, SKILL_HEADBUTT);
    }
    else
    {
      act("You headbutt $N, slamming your head against $M.", false, ch, 0, victim, TO_CHAR);
      act("$n grabs you and.....AHHH!!!! BLOOD EVERYWHERE!", false, ch, 0, victim, TO_VICT);
      act("$n smashes $s head against $N.", false, ch, 0, victim, TO_NOTVICT);
      hit(ch, victim, SKILL_HEADBUTT);

      act("You noticed the dazed look on $N's face and grin.", false, ch, 0, victim, TO_CHAR);
      act("$n looks you square in the face and smiles...", false, ch, 0, victim, TO_VICT);
      act("$N looks dazed for a moment as $n studies $S face.", false, ch, 0, victim, TO_NOTVICT);

      act("You look at the spot between $N's eyes and headbutt it again!", false, ch, 0, victim, TO_CHAR);
      act("BAMM!!!! What was the license plate of that truck?!", false, ch, 0, victim, TO_VICT);
      act("$n takes $N and crushes $M with $s head,...Blood Everywhere!!", false, ch, 0, victim, TO_NOTVICT);
      hit(ch, victim, SKILL_HEADBUTT);
    }
    check_improve(ch, SKILL_HEADBUTT, TRUE);
  }
}
ACMD(do_headbutt)
{
  struct char_data *victim;
  char name[256];

  one_argument(argument, name);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_HEADBUTT))
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (!FIGHTING(ch))
  {
    send_to_char(ch, "You must be fighting to do this!\n\r");
    return;
  }

  if (!(victim = get_char_vis(ch, name, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch))
      victim = FIGHTING(ch);
    else
    {
      send_to_char(ch, "Headbutt whom?\n\r");
      return;
    }
  }
  if (victim == ch)
  {
    send_to_char(ch, "Surely you intend to headbang?\n\r");
    return;
  }
  if (!IS_NPC(victim))
  {
    send_to_char(ch, "Why would you do a thing like that?\n\r");
    return;
  }
  if (ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "Behave yourself here please!\n\r");
    return;
  }
  headbutt_combat(ch, victim);
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

void tumble_combat(struct char_data *ch, struct char_data *victim)
{
  struct affected_type af;
  int percent = rand_number(1, 101);
  int dam, chance;
  bool vict_lag = FALSE;

  dam = chance = 0;

  if (!FIGHTING(ch) || !FIGHTING(victim))
    return;

  if (percent > GET_SKILL(ch, SKILL_TUMBLE))
  {
    act("You try to tumble into $N, but miss horribly.", false, ch, 0, victim, TO_CHAR);
    act("$n tries to tumble into you, but you step aside quickly.", false, ch, 0, victim, TO_VICT);
    act("$n tries to tumble into $N, but misses horribly.", false, ch, 0, victim, TO_NOTVICT);
    damage(ch, victim, 0, SKILL_TUMBLE);
    GET_POS(ch) = POS_SITTING;
    check_improve(ch, SKILL_TUMBLE, FALSE);
  }
  else
  {
    chance = rand_number(1, 25) + GET_LUCK(ch) + GET_DEX(ch);
    dam = GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_TUMBLE);
    act("You tumble into $N, knocking $M across the room!", false, ch, 0, victim, TO_CHAR);
    act("$n tumbles into you, knocking you across the room!", false, ch, 0, victim, TO_VICT);
    act("$n tumbles into $N, knocking $M across the room!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_TUMBLE, TRUE);

    if (chance > 35 && FIGHTING(ch))
      dam += GET_DEX(ch);
    if (chance > 45 && FIGHTING(ch))
      dam += GET_LUCK(ch);
    if (chance > 55 && FIGHTING(ch))
      dam += GET_DAMROLL(ch);
    if (chance > 65 && FIGHTING(ch))
    {
      dam += dice(GET_DEX(ch), GET_LUCK(ch));
      if (!mag_savingthrow(victim, SAVING_PARA, 0))
      {
        vict_lag = TRUE;
        new_affect(&af);
        af.spell = SPELL_PARALYZE;
        SET_BIT_AR(af.bitvector, AFF_PARA);
        af.location = APPLY_AC;
        af.modifier = 100;
        af.duration = 0;
        affect_join(victim, &af, TRUE, FALSE, FALSE, FALSE);
        act("$n leaves $N PARALYZED from the strike!", false, ch, 0, victim, TO_NOTVICT);
        act("You leave $N PARALYZED from the strike!", false, ch, 0, victim, TO_CHAR);
      }
    }
    damage(ch, victim, dam, SKILL_TUMBLE);
    GET_POS(victim) = POS_STUNNED;
    if (vict_lag)
      WAIT_STATE(victim, PULSE_VIOLENCE);
  }
}
ACMD(do_tumble)
{
  struct char_data *victim;
  char name[256];

  one_argument(argument, name);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_TUMBLE))
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (!FIGHTING(ch))
  {
    send_to_char(ch, "You must be fighting to do this!\n\r");
    return;
  }

  if (!(victim = get_char_vis(ch, name, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch))
      victim = FIGHTING(ch);
    else
    {
      send_to_char(ch, "Tumble into whom?\n\r");
      return;
    }
  }
  if (victim == ch)
  {
    send_to_char(ch, "Are you trying to breakdance?\n\r");
    return;
  }
  if (!IS_NPC(victim))
  {
    send_to_char(ch, "Why would you do a thing like that?\n\r");
    return;
  }
  if (ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "Behave yourself here please!\n\r");
    return;
  }
  tumble_combat(ch, victim);
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

ACMD(do_taunt)
{
  struct char_data *tmp_ch, *next_tmp_ch;
  int percent, prob, player_counter = 0;
  char actbuf[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  if (!GET_SKILL(ch, SKILL_TAUNT))
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "Behave yourself here please!\n\r");
    return;
  }

  do_say(ch, strcpy(actbuf, "ARE YOU NOT ENTERTAINED!?"), 0, 0);

  for (tmp_ch = world[IN_ROOM(ch)].people; tmp_ch; tmp_ch = next_tmp_ch)
  {
    (next_tmp_ch = tmp_ch->next_in_room);
    player_counter++;
    /*
     * The skips: 1: the caster
     *            2: immortals
     *            3: if no pk on this mud, skips over all players
     *            4: pets (charmed NPCs)
     */

    if (tmp_ch == ch)
    {
      continue;
    }
    if (!IS_NPC(tmp_ch))
    {
      player_counter -= 1;
      continue;
    }
    if (IS_NPC(ch) && IS_NPC(tmp_ch))
    {
      continue;
    }

    if (!IS_NPC(tmp_ch))
    {
      continue;
    }
    if (AFF_FLAGGED(tmp_ch, AFF_CHARM))
    {
      send_to_char(ch, "The charmie ignores your taunt!\r\n");
      continue;
    }

    percent = rand_number(1, 101); /* 101% is a complete failure */
    prob = GET_SKILL(ch, SKILL_TAUNT);

    if (percent > prob)
    {
      send_to_char(ch, "Your taunt goes unnoticed.\r\n");
      act("$n's taunt fails to provoke you", FALSE, ch, 0, 0, TO_VICT);
      WAIT_STATE(ch, PULSE_VIOLENCE * 3);
      check_improve(ch, SKILL_TAUNT, FALSE);
      return;
    }

    send_to_char(ch, "Your taunt is successful...\r\n");

    /* act("$n roars ferociously!", FALSE, ch, 0, 0, TO_NOTVICT); */
    act("$n's taunt provokes you to anger!", FALSE, ch, 0, 0, TO_VICT);

    if (FIGHTING(tmp_ch))
    {
      stop_fighting(tmp_ch);
    }

    hit(tmp_ch, ch, TYPE_UNDEFINED);
    check_improve(ch, SKILL_TAUNT, TRUE);
    WAIT_STATE(ch, PULSE_VIOLENCE * 3);
  }
  if (player_counter <= 1)
  {
    send_to_char(ch, "There is no one here except for you...\r\n");
  }
}

ACMD(do_rage)
{
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RAGE))
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (!IS_AFFECTED(ch, AFF_RAGE))
  {
    send_to_char(ch, "You go into a rage! YOU ARE RAGING MAD!\n\r");
    act("$n goes into a rage!", false, ch, 0, 0, TO_NOTVICT);
    SET_BIT_AR(AFF_FLAGS(ch), AFF_RAGE);
    return;
  }
  else if (IS_AFFECTED(ch, AFF_RAGE))
  {
    if (FIGHTING(ch) != NULL)
    {
      act("You cannot think right now, you are RAGING!", false, ch, 0, 0, TO_CHAR);
      return;
    }
    else
    {
      act("You calm down and look exhausted!", false, ch, 0, 0, TO_CHAR);
      act("$n calms down from raging and $s is totally exhausted!", false, ch, 0, 0, TO_NOTVICT);
      GET_MOVE(ch) = 1;
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_RAGE);
      return;
    }
  }
}


void check_acrobatics(struct char_data *ch, struct char_data *victim)
{
  int num = rand_number(1, 201);
  int lrn = GET_SKILL(ch, SKILL_ACROBATICS) + GET_LUCK(ch) + GET_DEX(ch);
  bool proc = FALSE;
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_ACROBATICS))
  {
    return;
  }
  if (lrn >= num)
  {
    switch (dice(1, 4))
    {
    case 1:
    case 3:
      break;
    case 2:
    case 4:
      tumble_combat(ch, victim);
      break;
    default:
      break;
    }
    proc = TRUE;
  }
  if (proc)
    check_improve(ch, SKILL_ACROBATICS, TRUE);
  else
    check_improve(ch, SKILL_ACROBATICS, FALSE);
}
void check_dirty_tricks(struct char_data *ch, struct char_data *victim)
{
  int num = rand_number(1, 201);
  int lrn = GET_SKILL(ch, SKILL_DIRTY_TRICKS) + GET_LUCK(ch) + GET_DEX(ch);
  bool proc = FALSE;
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_DIRTY_TRICKS))
  {
    return;
  }
  if (lrn >= num)
  {
    switch (dice(1, 8))
    {
    case 1:
    case 5:
      impale_combat(ch, victim);
      break;
    case 2:
    case 6:
      rend_combat(ch, victim);
      break;
    case 3:
    case 7:
      mince_combat(ch, victim);
      break;
    case 4:
    case 8:
      thrust_combat(ch, victim);
      break;
    default:
      break;
    }
    proc = TRUE;
  }
  if (proc)
    check_improve(ch, SKILL_DIRTY_TRICKS, TRUE);
  else
    check_improve(ch, SKILL_DIRTY_TRICKS, FALSE);
}

void check_unarmed_combat(struct char_data *ch, struct char_data *victim)
{
  int num = rand_number(1, 201);
  int lrn = GET_SKILL(ch, SKILL_UNARMED_COMBAT) + GET_LUCK(ch);
  bool proc = FALSE;
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_UNARMED_COMBAT))
  {
    return;
  }
  if (IS_AFFECTED(ch, AFF_RAGE))
    lrn += GET_STR(ch);

  if (lrn >= num)
  {
    switch (dice(1, 10))
    {
    case 1:
      kick_combat(ch, victim);
      break;
    case 2:
      headbutt_combat(ch, victim);
      break;
    case 3:
      left_hook_combat(ch, victim);
      break;
    case 4:
      right_hook_combat(ch, victim);
      break;
    case 5:
      sucker_punch_combat(ch, victim);
      break;
    case 6:
      uppercut_combat(ch, victim);
      break;
    case 7:
      haymaker_combat(ch, victim);
      break;
    case 8:
      clothesline_combat(ch, victim);
      break;
    case 9:
      piledriver_combat(ch, victim);
      break;
    case 10:
      chop_combat(ch, victim);
      palm_strike_combat(ch, victim);
      break;
    default:
      break;
    }
    proc = TRUE;
  }
  if (proc)
    check_improve(ch, SKILL_UNARMED_COMBAT, TRUE);
  else
    check_improve(ch, SKILL_UNARMED_COMBAT, FALSE);
}

void kick_combat(struct char_data *ch, struct char_data *victim)
{
  struct affected_type af;
  /* 101% is a complete failure */
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_KICK);

  /* Max of dice(50/5,50/10) or dice(10,5)*/
  int dex_roll = dice(2 * GET_DEX(ch) / 5, 2 * GET_DEX(ch) / 10);

  /* Max of dice(10,5) */
  int luck_roll = dice(2 * GET_LUCK(ch) / 5, 2 * GET_LUCK(ch) / 10);
  int dam, skill_lev;

  if (!FIGHTING(ch) || !FIGHTING(victim))
    return;

  if (AFF_FLAGGED(ch, AFF_OFFENSE)) /* Max of 3*/
    skill_lev = 1 + (GET_REAL_LEVEL(ch) / 30);
  else /* Max of 1 */
    skill_lev = 1;

  dam = dex_roll * luck_roll * skill_lev;

  if (percent > prob)
  {
    damage(ch, victim, 0, SKILL_KICK);
    check_improve(ch, SKILL_KICK, FALSE);
  }
  else
  {
    damage(ch, victim, dam, SKILL_KICK);
    check_improve(ch, SKILL_KICK, TRUE);
    if (!MOB_FLAGGED(victim, MOB_NOBLIND) && mag_savingthrow(victim, SAVING_SPELL, 0))
    {
      new_affect(&af);
      af.spell = SPELL_BLINDNESS;
      SET_BIT_AR(af.bitvector, AFF_BLIND);
      af.modifier = 100;
      af.duration = 1;
      af.location = APPLY_AC;
      affect_join(victim, &af, TRUE, FALSE, FALSE, FALSE);
      act("$n kicks $N so hard, causing $N to be blinded in pain!!", FALSE, ch, NULL, victim, TO_NOTVICT);
      act("You kick $N so hard $E is blinded by pain!", FALSE, ch, NULL, victim, TO_CHAR);
      act("$n kicks you so hard, you can't see anything but the stars!", FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
    }
  }
}
ACMD(do_kick)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_KICK))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if (!FIGHTING(ch))
  {
    send_to_char(ch, "You must be fighting to do this!\n\r");
    return;
  }
  one_argument(argument, arg);

  if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
    {
      victim = FIGHTING(ch);
    }
    else
    {
      send_to_char(ch, "Kick who?\r\n");
      return;
    }
  }
  if (victim == ch)
  {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  if (!CONFIG_PK_ALLOWED && !IS_NPC(victim) && !IS_NPC(ch))
    return;

  kick_combat(ch, victim);

  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

void bash_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_BASH);
  int level = 0;

  if (percent > prob)
  {
    damage(ch, victim, 0, SKILL_BASH);
    GET_POS(ch) = POS_SITTING;
    check_improve(ch, SKILL_BASH, FALSE);
  }
  else
  {
    level = GET_REAL_LEVEL(ch) + (GET_SKILL(ch, SKILL_BASH) + GET_SKILL(ch, SKILL_SHIELD_MASTER)) / 2;
    hit(ch, victim, SKILL_BASH);
    if (IN_ROOM(ch) == IN_ROOM(victim))
    {
      if (rand_number(1, 20) > 15)
        call_magic(ch, victim, NULL, SPELL_CONCUSSIVE_WAVE, level, CAST_SPELL);
      else
        GET_POS(victim) = POS_STUNNED;
    }
    if (!mag_savingthrow(victim, SAVING_PARA, 0))
      WAIT_STATE(victim, PULSE_VIOLENCE);
    check_improve(ch, SKILL_BASH, TRUE);
  }
}

ACMD(do_bash)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BASH))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  if (!GET_EQ(ch, WEAR_SHIELD))
  {
    send_to_char(ch, "You need to wear a shield to make it a success.\r\n");
    return;
  }
  if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
    {
      victim = FIGHTING(ch);
    }
    else
    {
      send_to_char(ch, "Bash who?\r\n");
      return;
    }
  }
  if (victim == ch)
  {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  if (!CONFIG_PK_ALLOWED && !IS_NPC(victim) && !IS_NPC(ch))
    return;

  if (MOB_FLAGGED(victim, MOB_NOKILL))
  {
    send_to_char(ch, "This mob is protected.\r\n");
    return;
  }

  if (MOB_FLAGGED(victim, MOB_NOBASH))
  {
    send_to_char(ch, "You cannot bash this target.\r\n");
    return;
  }
  bash_combat(ch, victim);
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

void impale_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_IMPALE);

  if (percent > prob)
  {
    act("You try, but can't pin $N down to impale $M.", false, ch, 0, victim, TO_CHAR);
    act("$n tries to pin you down, but you escape.", false, ch, 0, victim, TO_VICT);
    act("Weapons drawn, $n tries to pin $N down, but fails.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_IMPALE, FALSE);
    return;
  }
  else
  {
    act("Holding $N at bay with one weapon, you nimbly pin $M down.\r\nCrazed with bloodlust, "
        "you run your other weapon straight through $S insides!",
        false, ch, 0, victim, TO_CHAR);
    act("$n pins you down and impales you with $s weapon!", false, ch, 0, victim, TO_VICT);
    act("$n pins $N down and impales $M with $s weapon!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_REND, TRUE);
    hit(ch, victim, SKILL_IMPALE);

    return;
  }
}

void rend_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_REND);

  if (percent > prob)
  {
    act("You try to skewer $N on your blades, but fail the difficult maneuver.", false, ch, 0, victim, TO_CHAR);
    act("$n draws $s blades and fails some strange maneuver.", false, ch, 0, victim, TO_VICT);
    act("Blades drawn, $n tries to rend $N, but fails.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_REND, FALSE);
    return;
  }
  else
  {
    act("You skewer $N on two blades and, taking advantage of\r\n$S prone situation, proceed "
        "to hack viciously at $S tattered body!\r\nBlood fountains everywhere!",
        false, ch, 0, victim, TO_CHAR);
    act("$n cruelly rends you with an unstoppable onslaught of expert attacks.", false, ch, 0, victim, TO_VICT);
    act("$n skewers $N on two blades and, taking advantage of\r\n$S prone situation, proceeds "
        "to hack viciously at $S tattered body!\r\nBlood fountains everywhere!",
        false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_REND, TRUE);
    hit(ch, victim, SKILL_REND);
    check_rend(ch, victim);
    return;
  }
}

void mince_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_MINCE);

  if (percent > prob)
  {
    act("You fumble your blades and fail the skill.", false, ch, 0, victim, TO_CHAR);
    act("$n approaches you, blades drawn, but fumbles $s weapons.", false, ch, 0, victim, TO_VICT);
    act("$n approaches $N, blades drawn, but fumbles $s weapons.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_MINCE, FALSE);
    return;
  }
  else
  {
    act("You spin your blades about, lashing incisions into $N.", false, ch, 0, victim, TO_CHAR);
    act("$n whirls $s blades furiously, lacerating you.", false, ch, 0, victim, TO_VICT);
    act("$n whirls $s blades furiously, digging them into $N.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_MINCE, TRUE);
    hit(ch, victim, SKILL_MINCE);
    return;
  }
}

void thrust_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_THRUST);

  if (percent > prob)
  {
    act("$N easily dodges your turgid maneuver.", false, ch, 0, victim, TO_CHAR);
    act("You easily evade $n's turgid thrust.", false, ch, 0, victim, TO_VICT);
    act("$N easily dodges $n's lazy thrust.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_THRUST, FALSE);
    return;
  }
  else
  {
    act("You spryly spring forth and thrust your weapon into $N!", false, ch, 0, victim, TO_CHAR);
    act("$n suddenly leaps forth and thrusts $s weapon into you!", false, ch, 0, victim, TO_VICT);
    act("$n springs forth and thrusts his weapon into $N!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_THRUST, TRUE);
    hit(ch, victim, SKILL_THRUST);
    return;
  }
}

void right_hook_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_R_HOOK);
  if (percent > prob)
  {
    act("Your right hook runs far afoul of $N.", false, ch, 0, victim, TO_CHAR);
    act("$n's right hook misses you by a comfortable margin.", false, ch, 0, victim, TO_VICT);
    act("$n throws a wildly errant right hook at $N.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_R_HOOK, FALSE);
    return;
  }
  else
  {
    act("Your right hook connects squarely with $N's jaw.", false, ch, 0, victim, TO_CHAR);
    act("$n's right hook connects squarely with your jaw.", false, ch, 0, victim, TO_VICT);
    act("$n's right hook connects squarely with $N's jaw.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_R_HOOK, TRUE);
    hit(ch, victim, SKILL_R_HOOK);

    if (percent > 80)
      call_magic(ch, victim, NULL, SPELL_A_DEBUFF, GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_R_HOOK), CAST_SPELL);
    return;
  }
}

void left_hook_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_L_HOOK);
  if (percent > prob)
  {
    act("Your left hook runs far afoul of $N.", false, ch, 0, victim, TO_CHAR);
    act("$n's left hook misses you by a comfortable margin.", false, ch, 0, victim, TO_VICT);
    act("$n throws a wildly errant left hook at $N.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_L_HOOK, FALSE);
    return;
  }
  else
  {
    act("Your left hook connects squarely with $N's ribs.", false, ch, 0, victim, TO_CHAR);
    act("$n's left hook connects squarely with your ribs.", false, ch, 0, victim, TO_VICT);
    act("$n's left hook connects squarely with $N's ribs.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_L_HOOK, TRUE);
    hit(ch, victim, SKILL_L_HOOK);

    if (percent > 80)
      call_magic(ch, victim, NULL, SPELL_A_DEBUFF, GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_L_HOOK), CAST_SPELL);

    return;
  }
}

void sucker_punch_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_SUCKER_PUNCH);
  if (percent > prob)
  {
    act("$N reads your fake and sidesteps your sucker punch.", false, ch, 0, victim, TO_CHAR);
    act("You read $n's fake and sidestep $s sucker punch.", false, ch, 0, victim, TO_VICT);
    act("$N reads $n's fake and sidesteps $s sucker punch.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_SUCKER_PUNCH, FALSE);
    return;
  }
  else
  {
    act("You deftly distract $N, then sucker punch $M in the gut.", false, ch, 0, victim, TO_CHAR);
    act("$n's fake catches you unaware, as $e sucker punches you in the gut.", false, ch, 0, victim, TO_VICT);
    act("$n deftly distracts $N, then sucker punches $M in the gut.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_SUCKER_PUNCH, TRUE);
    hit(ch, victim, SKILL_SUCKER_PUNCH);

    if (percent > 80)
      call_magic(ch, victim, NULL, SPELL_V_DEBUFF, GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_SUCKER_PUNCH), CAST_SPELL);
    return;
  }
}
void uppercut_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_UPPERCUT);
  if (percent > prob)
  {
    act("Your uppercut slices through nothing but air.", false, ch, 0, victim, TO_CHAR);
    act("You feel nothing but breeze from $n's bumbling uppercut.", false, ch, 0, victim, TO_VICT);
    act("$n's uppercut slices through air, missing $N completely.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_UPPERCUT, FALSE);
    return;
  }
  else
  {
    act("Your uppercut smashes into $N's chin and knocks $M into the air!", false, ch, 0, victim, TO_CHAR);
    act("The world spins as $n's graceful uppercut lands in your face.", false, ch, 0, victim, TO_VICT);
    act("$n's graceful uppercut arcs into $N's face.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_UPPERCUT, TRUE);
    hit(ch, victim, SKILL_UPPERCUT);

    if (percent > 80)
      call_magic(ch, victim, NULL, SPELL_K_DEBUFF, GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_UPPERCUT), CAST_SPELL);

    return;
  }
}
void haymaker_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_HAYMAKER);
  if (percent > prob)
  {
    act("$N notices your wind-up and easily sidesteps your haymaker.", false, ch, 0, victim, TO_CHAR);
    act("You notice $n's wind-up and easily sidestep $s haymaker.", false, ch, 0, victim, TO_VICT);
    act("$N notices $n's wind-up and easily sidesteps $s haymaker.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_HAYMAKER, FALSE);
    return;
  }
  else
  {
    act("$N's bones snap beneath the impact of your awesome haymaker.", false, ch, 0, victim, TO_CHAR);
    act("You feel your bones shatter beneath the impact of $n's haymaker.", false, ch, 0, victim, TO_VICT);
    act("You hear bones snap as $n levels $N with an awesome haymaker.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_HAYMAKER, TRUE);
    hit(ch, victim, SKILL_HAYMAKER);

    if (percent > 80)
      call_magic(ch, victim, NULL, SPELL_UNARMED_DEBUFF1, GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_HAYMAKER), CAST_SPELL);

    return;
  }
}
void clothesline_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_CLOTHESLINE);
  if (percent > prob)
  {
    act("You barrel toward $N, arms outstretched, but $E ducks the attack.", false, ch, 0, victim, TO_CHAR);
    act("$n barrels toward you, arms outstretched, but you duck the attack.", false, ch, 0, victim, TO_VICT);
    act("$n barrels toward $N, arms outstretched, but fails to land a blow.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_CLOTHESLINE, FALSE);
    return;
  }
  else
  {
    act("The ground rattles as you level $N with a sweeping clothesline.", false, ch, 0, victim, TO_CHAR);
    act("The ground rattles as $n bowls you over with a clothesline.", false, ch, 0, victim, TO_VICT);
    act("The ground rattles as $n levels $N with a sweeping clothesline.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_CLOTHESLINE, TRUE);
    hit(ch, victim, SKILL_CLOTHESLINE);

    if (percent > 80)
      call_magic(ch, victim, NULL, SPELL_UNARMED_DEBUFF2, GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_CLOTHESLINE), CAST_SPELL);

    return;
  }
}
void piledriver_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_PILEDRVIER);
  if (percent > prob)
  {
    act("You try to grab $N for a piledriver, but fail to make contact.", false, ch, 0, victim, TO_CHAR);
    act("$n tries to grab you for a piledriver, but fails to make contact.", false, ch, 0, victim, TO_VICT);
    act("$n tries to grab $N for a piledriver, but fails to make contact.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_PILEDRVIER, FALSE);
    return;
  }
  else
  {
    act("You make the ground shake with $N's head as you piledrive $M.", false, ch, 0, victim, TO_CHAR);
    act("The ground shakes as $n drives your head into it.", false, ch, 0, victim, TO_VICT);
    act("The ground shakes as $n drives $N's head into it.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_PILEDRVIER, TRUE);
    hit(ch, victim, SKILL_PILEDRVIER);

    if (percent > 90)
      call_magic(ch, ch, NULL, SPELL_UNARMED_BONUS, GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_PILEDRVIER), CAST_SPELL);

    return;
  }
}
void palm_strike_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_PALM_STRIKE);
  if (percent > prob)
  {
    act("Your focus frays while preparing the strike, causing you to miss your foe.", false, ch, 0, victim, TO_CHAR);
    act("$n's unfocused palm strike misses you by a wide margin.", false, ch, 0, victim, TO_VICT);
    act("$n's unfocused palm strike misses $N by a wide margin.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_PALM_STRIKE, FALSE);
    return;
  }
  else
  {
    act("Blood gushes from $N's mouth as your palm strike crushes $S ribcage.", false, ch, 0, victim, TO_CHAR);
    act("Your insides hemorrhage as  $n's palm strike crushes your ribcage.", false, ch, 0, victim, TO_VICT);
    act("Blood gushes from $N's mouth as $n's palm strike crushes $S ribcage.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_PALM_STRIKE, TRUE);
    hit(ch, victim, SKILL_PALM_STRIKE);

    if (percent > 90)
      call_magic(ch, ch, NULL, SPELL_GAIN_ADVANTAGE, GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_PALM_STRIKE), CAST_SPELL);

    return;
  }
}
void chop_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_CHOP);
  if (percent > prob)
  {
    act("You try to chop $N, but your hand whizzes past $S face.", false, ch, 0, victim, TO_CHAR);
    act("$n's hand narrowly misses your face.", false, ch, 0, victim, TO_VICT);
    act("$n tries to chop at $N, but $E narrowly misses.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_CHOP, FALSE);
    return;
  }
  else
  {
    act("You tense your hands and whack $N really hard.", false, ch, 0, victim, TO_CHAR);
    act("$n hits you really hard in the ribs with $s chop.", false, ch, 0, victim, TO_VICT);
    act("$n smacks $N in the ribs with $s chop.  Ouch!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_CHOP, TRUE);
    hit(ch, victim, SKILL_CHOP);
    return;
  }  
}
void roundhouse_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_ROUNDHOUSE);
  if (percent > prob)
  {
    act("You flail wildly at $N and miss!", false, ch, 0, victim, TO_CHAR);
    act("$n flails wildly at you and misses!", false, ch, 0, victim, TO_VICT);
    act("$n flails wildly at $N and misses!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_ROUNDHOUSE, FALSE);
    return;
  }
  else
  {
    act("Your roundhouse blow is so powerful that it sends blood spilling out of $N's mouth!", false, ch, 0, victim, TO_CHAR);
    act("Blood spills out from your mouth as you are struck by $n's roundhouse blow!", false, ch, 0, victim, TO_VICT);
    act("Blood spills out from $N's mouth as $E is struck by $n's roundhouse blow!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_ROUNDHOUSE, TRUE);
    hit(ch, victim, SKILL_ROUNDHOUSE);
    return;
  }    
}
void trip_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_TRIP);
  if (percent > prob)
  {
    act("$N manages to avoid your attempt to trip $M.", false, ch, 0, victim, TO_CHAR);
    act("You manage to avoid $n's attempt to trip you.", false, ch, 0, victim, TO_VICT);
    act("$N manages to avoid $n's attempt to trip $M.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_TRIP, FALSE);
    return;
  }
  else
  {
    act("$N is tripped by you and falls badly to the ground.", false, ch, 0, victim, TO_CHAR);
    act("You are tripped by $n and fall badly to the ground.", false, ch, 0, victim, TO_VICT);
    act("$N is tripped by $n and falls badly to the ground.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_TRIP, TRUE);
    hit(ch, victim, SKILL_TRIP);
    return;
  }    
}

void knee_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_KICK);
  if (percent > prob)
  {
    act("You strike too slowly with your knee and miss $N altogether.", false, ch, 0, victim, TO_CHAR);
    act("$n barely misses hitting you in the stomach with $s knee.", false, ch, 0, victim, TO_VICT);
    act("$N jumps out of the way from $n's knee strike.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_KNEE, FALSE);
    return;
  }
  else
  {
    act("You knee $N painfully in the solar-plexus.", false, ch, 0, victim, TO_CHAR);
    act("$n knees you painfully in the solar-plexus.", false, ch, 0, victim, TO_VICT);
    act("$n knees $N in the solar-plexus.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_KNEE, TRUE);
    hit(ch, victim, SKILL_KNEE);
    return;
  }    
}
void elbow_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_ELBOW);
  if (percent > prob)
  {
    act("Your elbow hits thin air!", false, ch, 0, victim, TO_CHAR);
    act("You easily deflect $n's incoming elbow!", false, ch, 0, victim, TO_VICT);
    act("$N easily deflects $n's incoming elbow!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_ELBOW, FALSE);
    return;
  }
  else
  {
    act("Your elbow smashes into $N's ribs, cracking them with a dull sound!", false, ch, 0, victim, TO_CHAR);
    act("Your ribs are smashed apart by the powerful force of $n's elbow!", false, ch, 0, victim, TO_VICT);
    act("$N's ribs are smashed into bits by the powerful force of $n's elbow!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_ELBOW, TRUE);
    hit(ch, victim, SKILL_ELBOW);
    return;
  }    
}
/* This simply calculates the backstab multiplier based on a character's level.
 * This used to be an array, but was changed to be a function so that it would
 * be easier to add more levels to your MUD.  This doesn't really create a big
 * performance hit because it's not used very often. */
int backstab_mult(int level)
{
  return (1 + (level / 5));
}
int circle_mult(int level)
{
  return (1 + (level / 10));
}
int whirlwind_mult(int level)
{
  return (1 + (level / 10));
}

int unarmed_combat_dam(struct char_data *ch, int skill)
{
  /* skill_lev has a min of 1, max of 6*/
  int skill_lev;
  /* Max of dice(10,5) */
  int str_roll = dice((2 * GET_STR(ch)) / 5, (2 * GET_STR(ch)) / 10);
  /* Max of dice(5,5) */
  int luck_roll = dice((2 * GET_LUCK(ch)) / 10, (2 * GET_LUCK(ch)) / 10);

  switch (skill)
  {
  case SKILL_ROUNDHOUSE:
  case SKILL_ELBOW:
  case SKILL_KNEE:
  case SKILL_CHOP:
  case SKILL_TRIP:
  case SKILL_KICK:
  case SKILL_HEADBUTT:
  case SKILL_R_HOOK:
  case SKILL_L_HOOK:
    skill_lev = 1 + (GET_REAL_LEVEL(ch) / 20);
    break;
  case SKILL_SUCKER_PUNCH:
  case SKILL_UPPERCUT:
  case SKILL_HAYMAKER:
  case SKILL_CLOTHESLINE:
    skill_lev = 1 + (GET_REAL_LEVEL(ch) / 15);
    break;
  case SKILL_PILEDRVIER:
  case SKILL_PALM_STRIKE:
    skill_lev = 1 + (GET_REAL_LEVEL(ch) / 12);
    break;
  default:
    skill_lev = 1;
    break;
  }

  return (str_roll * luck_roll * skill_lev);
}

ACMD(do_testcmd)
{
  char arg[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH];
  struct char_data *vict;

  half_chop(argument, arg, buf);

  if (!*arg) {
    send_to_char(ch, "test what on whom?\r\n");
    return;
  }
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD))) {
    send_to_char(ch, "%s", CONFIG_NOPERSON);
    return;
  }
  send_to_char(ch, "This is the test command.\n\r");
  act("You scream \tYcharge\tn!", FALSE, ch, NULL, NULL, TO_CHAR);
  act("You say \tOOLE\tn!", FALSE, ch, NULL, NULL, TO_CHAR);

  call_magic(ch, vict, NULL, SPELL_SANCTUARY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_FURY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BLESS, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_HASTE, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_STONE_SKIN, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_STEEL_SKIN, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_ARMOR, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_STRENGTH, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_REGENERATION, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BARD_VITALITY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BARD_AGILITY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BARD_KNOWLEDGE, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BARD_REGEN, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BARD_BLESS, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BARD_BUFF, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_QUICKCAST, 1000, CAST_SPELL);
/*   call_magic(ch, vict, NULL, SPELL_FURY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_FURY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_FURY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_FURY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_FURY, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_FURY, 1000, CAST_SPELL); */
  
}

void check_rend(struct char_data *ch, struct char_data *victim)
{
  int level = GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_REND);
  int dam = 1000000;

  if (!IS_AFFECTED(victim, AFF_REND))
  {
    if (!mag_savingthrow(victim, SAVING_SPELL, 0))
    {
      call_magic(ch, victim, NULL, SPELL_REND, level, CAST_SPELL);
    }
  }
  else
  {
    if (!mag_savingthrow(victim, SAVING_SPELL, 0))
    {
      act("$n leaves $N bleeding from rends of flesh!", false, ch, 0, victim, TO_NOTVICT);
      act("You leave $N bleeding from rends of flesh!", false, ch, 0, victim, TO_CHAR);
      damage(ch, victim, dam, TYPE_SLASH);
    }
    else
    {
      REMOVE_BIT_AR(AFF_FLAGS(victim), AFF_REND);
      act("$N stops bleeding from the rends in $S flesh.", false, ch, 0, victim, TO_CHAR);
      act("$N stops bleeding from the rends in $S flesh.", false, ch, 0, victim, TO_NOTVICT);
      act("The rends in your flesh stop bleeding!", false, ch, 0, victim, TO_VICT);
      affect_from_char(victim, SPELL_REND);
    }
  }
}

void check_feeble(struct char_data *ch, struct char_data *victim)
{
  int level = GET_REAL_LEVEL(ch) + GET_SKILL(ch, SKILL_BARD_SHRIEK);

  if (!IS_AFFECTED(victim, AFF_FEEBLE))
  {
    if (!mag_savingthrow(victim, SAVING_SPELL, 0))
    {
      call_magic(ch, victim, NULL, SPELL_FEEBLE, level, CAST_SPELL);
    }
  }
  else
  {
    if (!mag_savingthrow(victim, SAVING_SPELL, 0))
    {
      act("$n shrieks causes $N to covers $S feeble ears!", false, ch, 0, victim, TO_NOTVICT);
      act("How pathetic, $N feels feeble from your shriek!", false, ch, 0, victim, TO_CHAR);
    }
    else
    {
      REMOVE_BIT_AR(AFF_FLAGS(victim), AFF_FEEBLE);
      affect_from_char(victim, SPELL_FEEBLE);
      act("$N stops covering $S ears and takes up the fight!", false, ch, 0, victim, TO_CHAR);
      act("$N stops feeling feeble.", false, ch, 0, victim, TO_NOTVICT);
      act("You stop feeling feeble.", false, ch, 0, victim, TO_VICT);
    }
  }
}

ACMD(do_shriek)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;
  int cost = 100;
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_BARD_SHRIEK);

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BARD_SHRIEK))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
    {
      victim = FIGHTING(ch);
    }
    else
    {
      send_to_char(ch, "Shriek at whom?\r\n");
      return;
    }
  }
  if (victim == ch)
  {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  if (!CONFIG_PK_ALLOWED && !IS_NPC(victim) && !IS_NPC(ch))
    return;

  if (MOB_FLAGGED(victim, MOB_NOKILL))
  {
    send_to_char(ch, "This mob is protected.\r\n");
    return;
  }
  if (GET_MANA(ch) < cost)
  {
    send_to_char(ch, "You don't have enough energy to shriek.\r\n");
    return;
  }
  if (percent > prob)
  {
    send_to_char(ch, "Your shriek gets caught in your throat.\r\n");
    WAIT_STATE(ch, PULSE_VIOLENCE);
    return;
  }

  if (char_has_mud_event(ch, eSHRIEK))
  {
    send_to_char(ch, "You are already shrieking as loud as you can!\r\n");
    return;
  }

  if (char_has_mud_event(ch, eRITUAL))
  {
    send_to_char(ch, "You are already performing the battle ritual!\r\n");
    return;
  }  
  act("You emit a piercing shriek!", false, ch, 0, NULL, TO_CHAR);
  act("$n begins to shriek loudly!", FALSE, ch, 0, 0, TO_ROOM);

  /* We need to pay for it */
  GET_MANA(ch) -= cost;

  /* NEW_EVENT() will add a new mud event to the event list of the character.
   * This function below adds a new event of "eSHRIEK", to "ch", and passes "NULL" as
   * additional data. The event will be called in "3 * PASSES_PER_SEC" or 3 seconds */
  NEW_EVENT(eSHRIEK, ch, NULL, 3 * PASSES_PER_SEC);

  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

EVENTFUNC(event_shriek)
{
  struct char_data *ch, *tch;
  struct mud_event_data *pMudEvent;
  int cost = 100;
  int percent = rand_number(1, 101);
  int prob = 0;

  if (event_obj == NULL)
    return 0;

  pMudEvent = (struct mud_event_data *)event_obj;
  ch = (struct char_data *)pMudEvent->pStruct;
  prob = GET_SKILL(ch, SKILL_BARD_SHRIEK);

  act("You continue shrieking like a banshee!", false, ch, 0, NULL, TO_CHAR);
  GET_MANA(ch) -= cost;

  for (tch = world[IN_ROOM(ch)].people; tch != NULL; tch = tch->next_in_room)
  {
    if (GROUP(tch) == GROUP(ch))
      continue;
    if (ch != tch && IS_NPC(tch))
    {
      call_magic(ch, tch, NULL, SPELL_FEEBLE, GET_REAL_LEVEL(ch) + GET_SPELLS_AFFECTS(ch), CAST_SPELL);
    }
  }

  if (GET_POS(ch) != POS_FIGHTING)
  {
    send_to_char(ch, "You must be fighting in order to shriek!\n\r");
    act("$n stops shrieking as the din of combat fades.", false, ch, 0, NULL, TO_ROOM);
    return 0;
  }

  if (GET_MANA(ch) < cost)
  {
    send_to_char(ch, "You are exhausted from combat and all that shrieking.\n\r");
    act("$n suddenly stops shrieking and looks exhausted.", false, ch, 0, NULL, TO_ROOM);
    return 0;
  }
  /* The "return" of the event function is the time until the event is called
   * again. If we return 0, then the event is freed and removed from the list, but
   * any other numerical response will be the delay until the next call */
  if (percent > prob)
  {
    send_to_char(ch, "Your shriek gets caught in your throat.\r\n");
    act("$n tries to shriek but has a frong in $s throat.", false, ch, 0, NULL, TO_ROOM);
    check_improve(ch, SKILL_BARD_SHRIEK, FALSE);
    return 0;
  }
  else
  {
    check_improve(ch, SKILL_BARD_SHRIEK, TRUE);
    return 7 * PASSES_PER_SEC;
  }
}

ACMD(do_ritual)
{
  char arg[MAX_INPUT_LENGTH];
  int cost = 100;
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_BARD_RITUAL);

  one_argument(argument, arg);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_BARD_RITUAL))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (GET_MANA(ch) < cost)
  {
    send_to_char(ch, "You don't have enough energy to perform a battle ritual.\r\n");
    return;
  }
  if (percent > prob)
  {
    send_to_char(ch, "You fumble about and fail the battle ritual.\r\n");
    WAIT_STATE(ch, PULSE_VIOLENCE);
    return;
  }

  if (char_has_mud_event(ch, eRITUAL))
  {
    send_to_char(ch, "You are already performing the battle ritual!\r\n");
    return;
  }
  if (char_has_mud_event(ch, eSHRIEK))
  {
    send_to_char(ch, "You are already shrieking as loud as you can!\r\n");
    return;
  }
  act("You begin the battle ritual!", false, ch, 0, NULL, TO_CHAR);
  act("$n begins an ancient skaldic ritual!", FALSE, ch, 0, 0, TO_ROOM);

  /* We need to pay for it */
  GET_MANA(ch) -= cost;

  /* NEW_EVENT() will add a new mud event to the event list of the character.
   * This function below adds a new event of "eSHRIEK", to "ch", and passes "NULL" as
   * additional data. The event will be called in "3 * PASSES_PER_SEC" or 3 seconds */
  NEW_EVENT(eRITUAL, ch, NULL, 3 * PASSES_PER_SEC);

  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

EVENTFUNC(event_ritual)
{
  struct char_data *ch, *tch;
  struct mud_event_data *pMudEvent;
  int cost = 100;
  int percent = rand_number(1, 101);
  int prob = 0;

  if (event_obj == NULL)
    return 0;

  pMudEvent = (struct mud_event_data *)event_obj;
  ch = (struct char_data *)pMudEvent->pStruct;
  prob = GET_SKILL(ch, SKILL_BARD_SHRIEK);
  GET_MANA(ch) -= cost;

/*   if (GET_POS(ch) != POS_FIGHTING)
  {
    send_to_char(ch, "The battle is over, no need for the ritual!\n\r");
    act("$n stops $s ritual as combat ends.", false, ch, 0, NULL, TO_ROOM);
    return 0;
  }
 */
  if (GET_MANA(ch) < cost)
  {
    send_to_char(ch, "You are exhausted from combat and all that shrieking.\n\r");
    act("$n suddenly stops shrieking and looks exhausted.", false, ch, 0, NULL, TO_ROOM);
    return 0;
  }  
  
  act("You continue performing the ancient skaldic battle ritual!", false, ch, 0, NULL, TO_CHAR);

  for (tch = world[IN_ROOM(ch)].people; tch != NULL; tch = tch->next_in_room)
  {
    if (ch != tch && !IS_NPC(tch))
    {
      call_magic(ch, tch, NULL, SPELL_FRENZY, GET_REAL_LEVEL(ch) + GET_SPELLS_AFFECTS(ch), CAST_SPELL);
    }
  }


  /* The "return" of the event function is the time until the event is called
   * again. If we return 0, then the event is freed and removed from the list, but
   * any other numerical response will be the delay until the next call */
  if (percent > prob)
  {
    send_to_char(ch, "You lost your place, you must restart the ritual.\r\n");
    act("$n seems to have lost $s place in the ritual.", false, ch, 0, NULL, TO_ROOM);
    check_improve(ch, SKILL_BARD_RITUAL, FALSE);
    return 0;
  }
  else
  {
    check_improve(ch, SKILL_BARD_RITUAL, TRUE);
    return 7 * PASSES_PER_SEC;
  }
}

ACMD(do_garrotte)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  struct affected_type af;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_GARROTTE))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
    else
    {
      send_to_char(ch, "Garrotte who?\r\n");
      return;
    }
  }
  if (vict == ch)
  {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  percent = rand_number(1, 101);
  prob = GET_SKILL(ch, SKILL_DIRT_KICK);

  if (percent > prob)
  {
    act("$N moves abruptly and you miss your chance to strangle $M!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n fails to strangle $N with $s garrotte.", TRUE, ch, 0, vict, TO_NOTVICT);
    damage(ch, vict, 0, TYPE_UNDEFINED);
  }
  else
  {
    if (!AFF_FLAGGED(ch, AFF_GARROTTE))
    {
      new_affect(&af);
      af.spell = SKILL_GARROTTE;
      af.location = APPLY_HITROLL;
      af.modifier = -10;
      af.duration = 1;
      SET_BIT_AR(af.bitvector, AFF_GARROTTE);
      affect_to_char(vict, &af);

      new_affect(&af);
      af.spell = SKILL_GARROTTE;
      af.location = APPLY_AC;
      af.modifier = 40;
      af.duration = 1;
      SET_BIT_AR(af.bitvector, AFF_GARROTTE);
      affect_to_char(vict, &af);

      damage(ch, vict, rand_number(1, GET_REAL_LEVEL(ch)), TYPE_UNDEFINED);
      act("Your steel wire bites deeply into $N's throat!", FALSE, ch, 0, vict, TO_CHAR);
      act("A shadow detaches itself from the darkness and wraps a wire around $N's throat!", TRUE, ch, 0, vict, TO_NOTVICT);
      act("Your garrotte cuts off $N's ability to speak!", FALSE, ch, 0, vict, TO_CHAR);
    }
    else
    {
      damage(ch, vict, rand_number(1, GET_REAL_LEVEL(ch)), TYPE_UNDEFINED);
      act("$N moves abruptly and you miss your chance to strangle $M!", FALSE, ch, 0, vict, TO_CHAR);
      act("$n tries to garrotte you but you dodge out of the way in time!", TRUE, ch, 0, vict, TO_NOTVICT);
    }
  }
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

ACMD(do_dirt_kick)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  struct affected_type af;
  int percent, prob;

  // dirt skill not learned automatic failure
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_DIRT_KICK))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      vict = FIGHTING(ch);
    else
    {
      send_to_char(ch, "Dirt Kick who?\r\n");
      return;
    }
  }

  // do nothing if trying to dirt kick self
  if (vict == ch)
  {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }

  // vars to check if we land a kick or not
  percent = rand_number(1, 101);
  prob = GET_SKILL(ch, SKILL_DIRT_KICK);

  // ch misses victim, using TYPE_UNDEFINED instead of SKILL_DIRT_KICK messages in <message> file.
  if (percent > prob)
  {
    act("You try to kick dirt in $N's eyes, but slip and fall!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n tries to kick dirt in $N's eyes, but slips and falls to the ground!", TRUE, ch, 0, vict, TO_NOTVICT);
    damage(ch, vict, 0, TYPE_UNDEFINED);
  }
  else
  {
    // we have a chance to hit!
    // check to see if mob can be blinded or not
    if (MOB_FLAGGED(vict, MOB_NOBLIND))
    {
      act("You kick dirt in $N's face, but nothing happens.", FALSE, ch, 0, vict, TO_CHAR);
    }
    else
    {
      // if vict is not affected by blind then apply the debuffs
      if (!AFF_FLAGGED(vict, AFF_BLIND))
      {
        new_affect(&af);
        af.spell = SKILL_DIRT_KICK;
        af.location = APPLY_HITROLL;
        af.modifier = -10;
        af.duration = 1;
        SET_BIT_AR(af.bitvector, AFF_BLIND);
        affect_to_char(vict, &af);

        new_affect(&af);
        af.spell = SKILL_DIRT_KICK;
        af.location = APPLY_AC;
        af.modifier = 40;
        af.duration = 1;
        SET_BIT_AR(af.bitvector, AFF_BLIND);
        affect_to_char(vict, &af);

        damage(ch, vict, rand_number(1, GET_REAL_LEVEL(ch)), TYPE_UNDEFINED);
        act("You temporarily blind $N.", FALSE, ch, 0, vict, TO_CHAR);
        act("$N is temporarily blinded.", TRUE, ch, 0, vict, TO_NOTVICT);
        act("You kick dirt in $N's face!", FALSE, ch, 0, vict, TO_CHAR);
      }
      else
      {
        damage(ch, vict, rand_number(1, GET_REAL_LEVEL(ch)), TYPE_UNDEFINED);
        act("You kick dirt in $N's eyes!", FALSE, ch, 0, vict, TO_CHAR);
        act("$n kicks dirt in $N's eyes!", TRUE, ch, 0, vict, TO_NOTVICT);
      }
    }
  }

  // wait period
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}
