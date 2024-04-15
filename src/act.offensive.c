/**************************************************************************
 *  File: act.offensive.c                                   Part of tbaMUD *
 *  Usage: Player-level commands of an offensive nature.                   *
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
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "act.h"
#include "fight.h"
#include "mud_event.h"
#include "salty.h"

#define MAX_SPELL_AFFECTS 5
void perform_assist(struct char_data *ch, struct char_data *helpee);

ACMD(do_defense)
{
  if (IS_NPC(ch))
    return;
}

/* main engine for assist mechanic */
void perform_assist(struct char_data *ch, struct char_data *helpee)
{
  struct char_data *opponent = NULL;

  if (!ch)
    return;

  /* hit same opponent as person you are helping */
  if (FIGHTING(helpee))
    opponent = FIGHTING(helpee);
  else
    for (opponent = world[IN_ROOM(ch)].people;
         opponent && (FIGHTING(opponent) != helpee);
         opponent = opponent->next_in_room)
      ;

  if (!opponent)
    act("But nobody is fighting $N!", FALSE, ch, 0, helpee, TO_CHAR);
  else if (!CAN_SEE(ch, opponent))
    act("You can't see who is fighting $N!", FALSE, ch, 0, helpee, TO_CHAR);
  /* prevent accidental pkill */
  else if (!CONFIG_PK_ALLOWED && !IS_NPC(opponent))
    send_to_char(ch, "You cannot kill other players.\r\n");
  else
  {
    send_to_char(ch, "You join the fight!\r\n");
    act("$N assists you!", 0, helpee, 0, ch, TO_CHAR);
    act("$n assists $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
    set_fighting(ch, opponent);

    // hit(ch, opponent, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);
  }
}

ACMD(do_assist)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *helpee;

  if (FIGHTING(ch))
  {
    send_to_char(ch, "You're already fighting!  How can you assist someone else?\r\n");
    return;
  }
  one_argument(argument, arg);

  if (!*arg)
    send_to_char(ch, "Whom do you wish to assist?\r\n");
  else if (!(helpee = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  else if (helpee == ch)
    send_to_char(ch, "You can't help yourself any more than this!\r\n");
  else
    perform_assist(ch, helpee);
}

ACMD(do_hit)
{
  char arg[MAX_INPUT_LENGTH];
  char buf[MSL];
  struct char_data *vict;
  struct obj_data *wpn;

  one_argument(argument, arg);

  if (!*arg)
    send_to_char(ch, "Hit who?\r\n");
  else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
    send_to_char(ch, "That player is not here.\r\n");
  else if (vict == ch)
  {
    send_to_char(ch, "You hit yourself...OUCH!.\r\n");
    act("$n hits $mself, and says OUCH!", FALSE, ch, 0, vict, TO_ROOM);
  }
  else if (AFF_FLAGGED(ch, AFF_CHARM) && (ch->master == vict))
    act("$N is just such a good friend, you simply can't hit $M.", FALSE, ch, 0, vict, TO_CHAR);
  else
  {
    if (!CONFIG_PK_ALLOWED && !IS_NPC(vict) && !IS_NPC(ch))
      return;
    //      check_killer(ch, vict);
    //	PVE Priority, Salty 05 JAN 2019
    if ((GET_POS(ch) == POS_STANDING) && (vict != FIGHTING(ch)))
    {

      if (GET_EQ(ch, WEAR_WIELD))
      {
        wpn = GET_EQ(ch, WEAR_WIELD);
        if (wpn->action_description)
        {
          sprintf(buf, "%s's %s", GET_NAME(ch), wpn->action_description);
          act(buf, false, ch, 0, 0, TO_ROOM);
          sprintf(buf, "Your %s", wpn->action_description);
          act(buf, false, ch, 0, 0, TO_CHAR);
        }
      }

      if (GET_DEX(ch) > GET_DEX(vict) || (GET_DEX(ch) == GET_DEX(vict) && rand_number(1, 2) == 1)) /* if faster */
        hit(ch, vict, TYPE_UNDEFINED);                                                             /* first */
      else
        hit(vict, ch, TYPE_UNDEFINED); /* or the victim is first */
      WAIT_STATE(ch, PULSE_VIOLENCE + 2);
    }

    else if (GET_POS(ch) == POS_FIGHTING && vict != FIGHTING(ch) /* && FIGHTING(vict) == ch*/)
    {
      if (GET_SKILL(ch, SKILL_SWITCH_TARGET) > rand_number(1, 95))
      {
        send_to_char(ch, "You switch your target!\r\n");
        act("$n switches target to $N!", FALSE, ch, 0, vict, TO_ROOM);
        damage(ch, vict, 1, TYPE_UNDEFINED);
        if (FIGHTING(vict) == ch)
        {
          FIGHTING(ch) = vict;
        }
      }
      else
      {
        send_to_char(ch, "You failed to switch your target!\r\n");
        act("$n misses a swing at $N!", FALSE, ch, 0, vict, TO_ROOM);
      }
    }

    else
      send_to_char(ch, "You're fighting the best you can!\r\n");
  }
}

ACMD(do_kill)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

  if (GET_LEVEL(ch) < LVL_GRGOD || IS_NPC(ch) || !PRF_FLAGGED(ch, PRF_NOHASSLE))
  {
    do_hit(ch, argument, cmd, subcmd);
    return;
  }
  one_argument(argument, arg);

  if (!*arg)
  {
    send_to_char(ch, "Kill who?\r\n");
  }
  else
  {
    if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
      send_to_char(ch, "That player is not here.\r\n");
    else if (ch == vict)
      send_to_char(ch, "Your mother would be so sad.. :(\r\n");
    else
    {
      act("You chop $M to pieces!  Ah!  The blood!", FALSE, ch, 0, vict, TO_CHAR);
      act("$N chops you to pieces!", FALSE, vict, 0, ch, TO_CHAR);
      act("$n brutally slays $N!", FALSE, ch, 0, vict, TO_NOTVICT);
      raw_kill(vict, ch);
    }
  }
}

ACMD(do_order)
{
  char name[MAX_INPUT_LENGTH], message[MAX_INPUT_LENGTH];
  bool found = FALSE;
  struct char_data *vict;
  struct follow_type *k;

  half_chop(argument, name, message);

  if (!*name || !*message)
    send_to_char(ch, "Order who to do what?\r\n");
  else if (!(vict = get_char_vis(ch, name, NULL, FIND_CHAR_ROOM)) && !is_abbrev(name, "followers"))
    send_to_char(ch, "That person isn't here.\r\n");
  else if (ch == vict)
    send_to_char(ch, "You obviously suffer from skitzofrenia.\r\n");
  else
  {
    if (AFF_FLAGGED(ch, AFF_CHARM))
    {
      send_to_char(ch, "Your superior would not aprove of you giving orders.\r\n");
      return;
    }
    if (vict)
    {
      char buf[MAX_STRING_LENGTH];

      snprintf(buf, sizeof(buf), "$N orders you to '%s'", message);
      act(buf, FALSE, vict, 0, ch, TO_CHAR);
      act("$n gives $N an order.", FALSE, ch, 0, vict, TO_ROOM);

      if ((vict->master != ch) || !AFF_FLAGGED(vict, AFF_CHARM))
        act("$n has an indifferent look.", FALSE, vict, 0, 0, TO_ROOM);
      else
      {
        send_to_char(ch, "%s", CONFIG_OK);
        command_interpreter(vict, message);
      }
    }
    else
    { /* This is order "followers" */
      char buf[MAX_STRING_LENGTH];

      snprintf(buf, sizeof(buf), "$n issues the order '%s'.", message);
      act(buf, FALSE, ch, 0, 0, TO_ROOM);

      for (k = ch->followers; k; k = k->next)
      {
        if (IN_ROOM(ch) == IN_ROOM(k->follower))
          if (AFF_FLAGGED(k->follower, AFF_CHARM))
          {
            found = TRUE;
            command_interpreter(k->follower, message);
          }
      }
      if (found)
        send_to_char(ch, "%s", CONFIG_OK);
      else
        send_to_char(ch, "Nobody here is a loyal subject of yours!\r\n");
    }
  }
}

ACMD(do_flee)
{
  int i, attempt, loss;
  struct char_data *was_fighting;

  if (GET_POS(ch) < POS_FIGHTING)
  {
    send_to_char(ch, "You are in pretty bad shape, unable to flee!\r\n");
    return;
  }

  for (i = 0; i < 6; i++)
  {
    attempt = rand_number(0, DIR_COUNT - 1); /* Select a random direction */
    if (CAN_GO(ch, attempt) &&
        !ROOM_FLAGGED(EXIT(ch, attempt)->to_room, ROOM_DEATH))
    {
      act("$n panics, and attempts to flee!", TRUE, ch, 0, 0, TO_ROOM);
      was_fighting = FIGHTING(ch);
      if (do_simple_move(ch, attempt, TRUE))
      {
        send_to_char(ch, "You flee head over heels.\r\n");
        if (was_fighting && !IS_NPC(ch))
        {
          loss = GET_MAX_HIT(was_fighting) - GET_HIT(was_fighting);
          loss *= GET_LEVEL(was_fighting);
          gain_exp(ch, -loss);
        }
        if (FIGHTING(ch))
          stop_fighting(ch);
        if (was_fighting && ch == FIGHTING(was_fighting))
          stop_fighting(was_fighting);
      }
      else
      {
        act("$n tries to flee, but can't!", TRUE, ch, 0, 0, TO_ROOM);
      }
      return;
    }
  }
  send_to_char(ch, "PANIC!  You couldn't escape!\r\n");
}


/*
  Recoded to fix bad list management
  Salty
  06 JAN 2016
*/
EVENTFUNC(event_whirlwind)
{
  struct char_data *ch, *tch;
  struct mud_event_data *pMudEvent;

  if (event_obj == NULL)
    return 0;

  /* For the sake of simplicity, we will place the event data in easily
   * referenced pointers */
  pMudEvent = (struct mud_event_data *)event_obj;
  ch = (struct char_data *)pMudEvent->pStruct;

  /*
     Lets hit every mob in the room
     Salty
     06 JAN 2019
  */
  send_to_char(ch, "You deliver a vicious WHIRLWIND!!!\n\r");
  for (tch = world[IN_ROOM(ch)].people; tch != NULL; tch = tch->next_in_room)
  {
    // Don't hit ourselves or another player
    if (ch != tch && IS_NPC(tch))
      hit(ch, tch, SKILL_WHIRLWIND);
  }
  /*
   * Keep whirlwind only if fighting
   */
  if (GET_POS(ch) != POS_FIGHTING)
  {
    send_to_char(ch, "You must be fighting in order to whirlwind!\n\r");
    return 0;
  }

  /* The "return" of the event function is the time until the event is called
   * again. If we return 0, then the event is freed and removed from the list, but
   * any other numerical response will be the delay until the next call */
  if (GET_SKILL(ch, SKILL_WHIRLWIND) < rand_number(1, 101))
  {
    send_to_char(ch, "You stop spinning.\r\n");
    return 0;
  }
  else
    return 1.5 * PASSES_PER_SEC;
}

/* The "Whirlwind" skill is designed to provide a basic understanding of the
 * mud event and list systems. */
ACMD(do_whirlwind)
{
  int prob, percent, atk;
  struct char_data *tch;
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_WHIRLWIND))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  if ROOM_FLAGGED (IN_ROOM(ch), ROOM_PEACEFUL)
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  if (GET_POS(ch) < POS_FIGHTING)
  {
    send_to_char(ch, "You must be on your feet to perform a whirlwind.\r\n");
    return;
  }

  if (!wielded)
  {
    send_to_char(ch, "You need to wield a weapon to make this a success.\n\r");
    return;
  }

  send_to_char(ch, "You begin to whirl rapidly around the room.\r\n");
  act("$n begins to whirl around the room rapidly!!", FALSE, ch, 0, 0, TO_ROOM);

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room)
  {

    /* The skips:
     *            1: the caster
     *            2: immortals
     *            3: if no pk on this mud, skips over all players
     *            4: pets (charmed NPCs)
     */

    if (tch == ch)
      continue;
    if (!IS_NPC(tch) && GET_LEVEL(tch) >= LVL_IMMORT)
      continue;
    if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(tch))
      continue;
    if (!IS_NPC(ch) && IS_NPC(tch) && AFF_FLAGGED(tch, AFF_CHARM))
      continue;
    hit(ch, tch, SKILL_WHIRLWIND);

    prob = GET_SKILL(ch, SKILL_WHIRLWIND);
    atk = (prob / 25) + rand_number(0, 3);
    percent = rand_number(1, 101);

    if (prob >= percent)
    {
      send_to_char(ch, "You whirl about %s, enabling you to strike %d times!\n\r", GET_NAME(tch), atk);
      for (int i = 0; i < atk; i++)
        hit(ch, tch, SKILL_WHIRLWIND);
      check_improve(ch, SKILL_WHIRLWIND, TRUE);
    }
    else
      check_improve(ch, SKILL_WHIRLWIND, FALSE);
  }
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

ACMD(do_bandage)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;
  int percent, prob;

  if (!GET_SKILL(ch, SKILL_BANDAGE))
  {
    send_to_char(ch, "You are unskilled in the art of bandaging.\r\n");
    return;
  }

  if (GET_POS(ch) != POS_STANDING)
  {
    send_to_char(ch, "You are not in a proper position for that!\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    send_to_char(ch, "Who do you want to bandage?\r\n");
    return;
  }

  if (GET_HIT(vict) >= 0)
  {
    send_to_char(ch, "You can only bandage someone who is close to death.\r\n");
    return;
  }

  WAIT_STATE(ch, PULSE_VIOLENCE * 2);

  percent = rand_number(1, 101); /* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_BANDAGE);

  if (percent <= prob)
  {
    act("Your attempt to bandage fails.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n tries to bandage $N, but fails miserably.", TRUE, ch,
        0, vict, TO_NOTVICT);
    damage(vict, vict, 2, TYPE_SUFFERING);
    check_improve(ch, SKILL_BANDAGE, FALSE);
    return;
  }

  act("You successfully bandage $N.", FALSE, ch, 0, vict, TO_CHAR);
  act("$n bandages $N, who looks a bit better now.", TRUE, ch, 0,
      vict, TO_NOTVICT);
  act("Someone bandages you, and you feel a bit better now.",
      FALSE, ch, 0, vict, TO_VICT);
  check_improve(ch, SKILL_BANDAGE, TRUE);
  GET_HIT(vict) = 0;
}
