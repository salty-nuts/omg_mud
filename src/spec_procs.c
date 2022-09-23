/**************************************************************************
*  File: spec_procs.c                                      Part of tbaMUD *
*  Usage: Implementation of special procedures for mobiles/objects/rooms. *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

/* For more examples: 
 * ftp://ftp.circlemud.org/pub/CircleMUD/contrib/snippets/specials */

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
#include "spec_procs.h"
#include "class.h"
#include "fight.h"
#include "modify.h"
#include "salty.h"

/* locally defined functions of local (file) scope */
static int compare_spells(const void *x, const void *y);
//static const char *how_good(int percent);
static void npc_steal(struct char_data *ch, struct char_data *victim);

/* Special procedures for mobiles. */
static int spell_sort_info[MAX_SKILLS + 1];



static int compare_spells(const void *x, const void *y)
{
  int	a = *(const int *)x,
	b = *(const int *)y;

  return strcmp(spell_info[a].name, spell_info[b].name);
}

void sort_spells(void)
{
  int a;

  /* initialize array, avoiding reserved. */
  for (a = 1; a <= MAX_SKILLS; a++)
    spell_sort_info[a] = a;

  qsort(&spell_sort_info[1], MAX_SKILLS, sizeof(int), compare_spells);
}

static const char *prac_types[] = {
  "spell",
  "skill"
};


void list_train(struct char_data *ch)
{
  char buf[MAX_STRING_LENGTH];

  send_to_char(ch, "You have %d training session%s remaining.\n\r",  GET_TRAINS(ch), GET_TRAINS(ch) == 1 ? "" : "s");

  strcpy(buf, "\n\rPermanent increase of:\n\r");
  if (GET_REAL_STR(ch) < 24 ) strcat(buf, "str");
  if (GET_REAL_ADD(ch) < 100 ) strcat(buf, " str%");
  if (GET_REAL_INT(ch) < 24 ) strcat(buf, " int");
  if (GET_REAL_WIS(ch) < 24 ) strcat(buf, " wis");
  if (GET_REAL_DEX(ch) < 24 ) strcat(buf, " dex");
  if (GET_REAL_CON(ch) < 24 ) strcat(buf, " con");
  if (GET_REAL_LUCK(ch) < 24 ) strcat(buf, " luck");

  strcat(buf,"\n\r\n\r");

  strcat(buf, "Regeneration speed of:\n\r");
  if (GET_HITP_REGEN(ch) < 100 ) strcat(buf, "hp");
  if (GET_MANA_REGEN(ch) < 100 ) strcat(buf, " mana");
  if (GET_MOVE_REGEN(ch) < 100 ) strcat(buf, " move");

  strcat(buf,"\n\r\n\r");

  strcat(buf, "Melee damage resistances:\n\r");

  if (GET_RESISTS(ch, RESIST_UNARMED) < 100) strcat(buf, "unarmed");
  if (GET_RESISTS(ch, RESIST_EXOTIC) < 100) strcat(buf, " exotic");
  if (GET_RESISTS(ch, RESIST_BLUNT) < 100) strcat(buf, " blunt");
  if (GET_RESISTS(ch, RESIST_SLASH) < 100) strcat(buf, " slash");
  if (GET_RESISTS(ch, RESIST_PIERCE) < 100) strcat(buf, " pierce");

  strcat(buf, "Magical damage resistances:\n\r");

  if (GET_RESISTS(ch, RESIST_RED) < 100) strcat(buf, "red");
  if (GET_RESISTS(ch, RESIST_BLUE) < 100) strcat(buf, " blue");
  if (GET_RESISTS(ch, RESIST_GREEN) < 100) strcat(buf, " green");
  if (GET_RESISTS(ch, RESIST_BLACK) < 100) strcat(buf, " black");
  if (GET_RESISTS(ch, RESIST_WHITE) < 100) strcat(buf, " white");

  strcat(buf,"\n\r\n\r");

  strcat(buf,"Magical power increases:\n\r");

  if (GET_SPELLS_HEALING(ch) < 100 ) strcat(buf, "healing");
  if (GET_SPELLS_DAMAGE(ch) < 100 ) strcat(buf, " damage");
  if (GET_SPELLS_AFFECTS(ch) < 100 ) strcat(buf, " affects");



  if (buf[strlen(buf) - 1] != ':')
  {
    strcat(buf, "\n\r");
    send_to_char(ch, "%s",buf);
  }
  else
    send_to_char(ch,"You have nothing left to train.\n\r");
}

/*
Redone to be percentage based.
Salty
03 JAN 2019
Revisions for Multiclass:  03 FEB 2020
*/
void list_skills(struct char_data *ch)
{
	int i, num = 0;

	send_to_char(ch, "You have %d practice session%s remaining.\r\n", GET_PRACTICES(ch), GET_PRACTICES(ch) == 1 ? "" : "s");
	send_to_char(ch, "\n\rYou know of the following spells:\r\n");
	send_to_char(ch, "---------------------------------------------------------------------------\n\r");

	/* Begin counting Spells */
	for (i = 1; i <= NUM_SPELLS; i++)
	{
    if ( (GET_LEVEL(ch, GET_CLASS(ch)) >= spell_info[i].min_level[(int)GET_CLASS(ch)]) ||
         (GET_LEVEL(ch, GET_MULTI_CLASS(ch)) >= spell_info[i].min_level[(int)GET_MULTI_CLASS(ch)]) || GET_REAL_LEVEL(ch) > LVL_IMMORT)
   	{
     	send_to_char(ch, "%-17s %3d %%   ", spell_info[i].name, GET_SKILL(ch, i));
     	num += 1;
			if ((num % 3) == 0)
     		send_to_char(ch, "\n\r");
		}
	}
  num = 0;
	send_to_char(ch, "\n\r\n\rYou know of the following songs:\r\n");
	send_to_char(ch, "---------------------------------------------------------------------------\n\r");
	for (i = ZERO_SONGS+1; i <= ZERO_SONGS + NUM_SONGS; i++)
	{
    if ( (GET_LEVEL(ch, GET_CLASS(ch)) >= spell_info[i].min_level[(int)GET_CLASS(ch)]) ||
         (GET_LEVEL(ch, GET_MULTI_CLASS(ch)) >= spell_info[i].min_level[(int)GET_MULTI_CLASS(ch)]) || GET_REAL_LEVEL(ch) > LVL_IMMORT)
   	{
     	send_to_char(ch, "%-17s %3d %%   ", spell_info[i].name, GET_SKILL(ch, i));
     	num += 1;
			if ((num % 3) == 0)
     		send_to_char(ch, "\n\r");
		}
	}

	send_to_char(ch, "\n\r\n\rYou know of the following skills:\r\n");
	send_to_char(ch, "---------------------------------------------------------------------------\n\r");

  num = 0;
	/*  Begin counting Skills */
	for (i = ZERO_SKILLS+1; i <= ZERO_SKILLS + NUM_SKILLS; i++)
	{
    if ( (GET_LEVEL(ch, GET_CLASS(ch)) >= spell_info[i].min_level[(int)GET_CLASS(ch)]) ||
         (GET_LEVEL(ch, GET_MULTI_CLASS(ch)) >= spell_info[i].min_level[(int)GET_MULTI_CLASS(ch)]) || GET_REAL_LEVEL(ch) > LVL_IMMORT)
		{
			send_to_char(ch, "%-17s %3d %%   ", spell_info[i].name, GET_SKILL(ch, i));
			num += 1;
			if ((num % 3) == 0)
				send_to_char(ch, "\n\r");
		}
	}
	send_to_char(ch, "\n\r");
}

SPECIAL(guild)
{
  int gain, percent, skill_num;
  char *pOut = NULL;

  if (IS_NPC(ch))
    return (FALSE);

  if (CMD_IS("practice"))
  {
    skip_spaces(&argument);
    if (!*argument)
    {
      list_skills(ch);
      return (TRUE);
    }
    if (GET_PRACTICES(ch) <= 0)
    {
      send_to_char(ch, "You do not seem to be able to practice now.\r\n");
      return (TRUE);
    }

    skill_num = find_skill_num(argument);

    if (skill_num < 1)
    {
      send_to_char(ch, "%s is not a valid option.\r\n",SPLSKL(ch));
      return (TRUE);
    }
    if ( (GET_LEVEL(ch, GET_CLASS(ch)) < spell_info[skill_num].min_level[(int)GET_CLASS(ch)]) &&
         (GET_LEVEL(ch, GET_MULTI_CLASS(ch)) < spell_info[skill_num].min_level[(int)GET_MULTI_CLASS(ch)]) )
    {
      send_to_char(ch, "You do not know of that %s.\r\n", SPLSKL(ch));
      return (TRUE);
    }


    if (GET_SKILL(ch, skill_num) >= LEARNED(ch))
    {
      send_to_char(ch, "You are already learned in that area.\r\n");
      return (TRUE);
    }
    send_to_char(ch, "You practice for a while...\r\n");
    GET_PRACTICES(ch)--;

    percent = GET_SKILL(ch, skill_num);
    gain = ((MAXGAIN(ch) *  int_app[GET_INT(ch)].learn) / 100);
    percent += gain;
     SET_SKILL(ch, skill_num, MIN(LEARNED(ch), percent));

    if (GET_SKILL(ch, skill_num) >= LEARNED(ch))
      send_to_char(ch, "You are now learned in that area.\r\n");
  return (TRUE);
  }
  else if (CMD_IS("train"))
  {
    bool gain = FALSE;
    skip_spaces(&argument);
    if (!*argument)
    {
      list_train(ch);
      return (TRUE);
    }
    if (GET_TRAINS(ch) <= 0)
    {
      send_to_char(ch, "You do not seem to be able to train now.\r\n");
      return (TRUE);
    }
    if (!str_cmp(argument, "hp"))
    {
      pOut = "hitpoit regen";
      if (GET_HITP_REGEN(ch) < MAX_TRAIN)
      {
	gain = TRUE;
	GET_HITP_REGEN(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "mana"))
    {
      pOut = "mana regen";
      if (GET_MANA_REGEN(ch) < MAX_TRAIN)
      {
	gain = TRUE;
	GET_MANA_REGEN(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "move"))
    {
      pOut = "movememt regen";
      if (GET_MOVE_REGEN(ch) < MAX_TRAIN)
      {
        gain = TRUE;
        GET_MOVE_REGEN(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "unarmed"))
    {
      pOut = "unarmed resistance";
      if (GET_RESISTS(ch, RESIST_UNARMED) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_UNARMED) += 1;
      }
    }
    else if (!str_cmp(argument, "exotic"))
    {
      pOut = "exotic resistance";
      if (GET_RESISTS(ch, RESIST_EXOTIC) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_EXOTIC) += 1;
      }
    }
    else if (!str_cmp(argument, "blunt"))
    {
      pOut = "blunt resistance";
      if (GET_RESISTS(ch, RESIST_BLUNT) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_BLUNT) += 1;
      }
    }
    else if (!str_cmp(argument, "slash"))
    {
      pOut = "slash resistance";
      if (GET_RESISTS(ch, RESIST_SLASH) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_SLASH) += 1;
      }
    }
    else if (!str_cmp(argument, "pierce"))
    {
      pOut = "pierce resistance";
      if (GET_RESISTS(ch, RESIST_PIERCE) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_PIERCE) += 1;
      }
    }
    else if (!str_cmp(argument, "red"))
    {
      pOut = "red magic resist";
      if (GET_RESISTS(ch, RESIST_RED) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_RED) += 1;
      }
    }    
    else if (!str_cmp(argument, "blue"))
    {
      pOut = "blue magic resist";
      if (GET_RESISTS(ch, RESIST_BLUE) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_BLUE) += 1;
      }
    }    
    else if (!str_cmp(argument, "green"))
    {
      pOut = "green magic resist";
      if (GET_RESISTS(ch, RESIST_GREEN) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_GREEN) += 1;
      }
    }    
    else if (!str_cmp(argument, "black"))
    {
      pOut = "black magic resist";
      if (GET_RESISTS(ch, RESIST_BLACK) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_BLACK) += 1;
      }
    }    
    else if (!str_cmp(argument, "white"))
    {
      pOut = "white magic resist";
      if (GET_RESISTS(ch, RESIST_WHITE) < MAX_RESIST)
      {
        gain = TRUE;
        GET_RESISTS(ch, RESIST_WHITE) += 1;
      }
    }    
    else if (!str_cmp(argument, "healing"))
    {
      pOut = "healing spells";
      if (GET_SPELLS_HEALING(ch) < MAX_TRAIN)
      {
        gain = TRUE;
        GET_SPELLS_HEALING(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "damage"))
    {
      pOut = "damage spells";
      if (GET_SPELLS_DAMAGE(ch) < MAX_TRAIN)
      {
        gain = TRUE;
        GET_SPELLS_DAMAGE(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "affects"))
    {
      pOut = "affects spells";
      if (GET_SPELLS_AFFECTS(ch) < MAX_TRAIN)
      {
        gain = TRUE;
        GET_SPELLS_AFFECTS(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "melee"))
    {
      pOut = "melee damage";
      if (GET_SPELLS_AFFECTS(ch) < MAX_TRAIN)
      {
        gain = TRUE;
        GET_SPELLS_AFFECTS(ch) += 1;
      }
    }
     if (!str_cmp(argument, "str"))
    {
      pOut = "strength";
      if (GET_REAL_STR(ch) < MAX_STAT)
      {
        gain = TRUE;
        GET_REAL_STR(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "str%"))
    {
      pOut = "strength percent";
      if (GET_REAL_ADD(ch) < 100)
      {
        gain = TRUE;
        GET_REAL_ADD(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "int"))
    {
      pOut = "intelligence";
      if (GET_REAL_INT(ch) < MAX_STAT)
      {
        gain = TRUE;
        GET_REAL_INT(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "wis"))
    {
      pOut = "wisdom";
      if (GET_REAL_WIS(ch) < MAX_STAT)
      {
        gain = TRUE;
        GET_REAL_WIS(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "dex"))
    {
      pOut = "dexterity";
      if (GET_REAL_DEX(ch) < MAX_STAT)
      {
        gain = TRUE;
        GET_REAL_DEX(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "con"))
    {
      pOut = "constitution";
      if (GET_REAL_CON(ch) < MAX_STAT)
      {
        gain = TRUE;
        GET_REAL_CON(ch) += 1;
      }
    }
    else if (!str_cmp(argument, "luck"))
    {
      pOut = "luck";
      if (GET_REAL_LUCK(ch) < MAX_STAT)
      {
        gain = TRUE;
        GET_REAL_LUCK(ch) += 1;
      }
    }

    if (gain)
    {
      send_to_char(ch,"You feel more adept as your %s increases!\r\n",pOut);
      GET_TRAINS(ch) -= 1;
      save_char(ch);
      return TRUE;
    }
    else
    {
      send_to_char(ch,"Your %s is already at maximum.\n\r",pOut);
      return TRUE;
    }
  return TRUE;
  }
  else
  return FALSE;
}
SPECIAL(dump)
{
  struct obj_data *k;
  int value = 0;

  for (k = world[IN_ROOM(ch)].contents; k; k = world[IN_ROOM(ch)].contents) {
    act("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
    extract_obj(k);
  }

  if (!CMD_IS("drop"))
    return (FALSE);

  do_drop(ch, argument, cmd, SCMD_DROP);

  for (k = world[IN_ROOM(ch)].contents; k; k = world[IN_ROOM(ch)].contents) {
    act("$p vanishes in a puff of smoke!", FALSE, 0, k, 0, TO_ROOM);
    value += MAX(1, MIN(50, GET_OBJ_COST(k) / 10));
    extract_obj(k);
  }

  if (value) {
    send_to_char(ch, "You are awarded for outstanding performance.\r\n");
    act("$n has been awarded for being a good citizen.", TRUE, ch, 0, 0, TO_ROOM);

    if (GET_REAL_LEVEL(ch) < 3)
      gain_exp(ch, value);
    else
      increase_gold(ch, value);
  }
  return (TRUE);
}

SPECIAL(mayor)
{
  char actbuf[MAX_INPUT_LENGTH];

  const char open_path[] =
	"W3a3003b33000c111d0d111Oe333333Oe22c222112212111a1S.";
  const char close_path[] =
	"W3a3003b33000c111d0d111CE333333CE22c222112212111a1S.";

  static const char *path = NULL;
  static int path_index;
  static bool move = FALSE;

  if (!move) {
    if (time_info.hours == 6) {
      move = TRUE;
      path = open_path;
      path_index = 0;
    } else if (time_info.hours == 20) {
      move = TRUE;
      path = close_path;
      path_index = 0;
    }
  }
  if (cmd || !move || (GET_POS(ch) < POS_SLEEPING) ||
      (GET_POS(ch) == POS_FIGHTING))
    return (FALSE);

  switch (path[path_index]) {
  case '0':
  case '1':
  case '2':
  case '3':
    perform_move(ch, path[path_index] - '0', 1);
    break;

  case 'W':
    GET_POS(ch) = POS_STANDING;
    act("$n awakens and groans loudly.", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'S':
    GET_POS(ch) = POS_SLEEPING;
    act("$n lies down and instantly falls asleep.", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'a':
    act("$n says 'Hello Honey!'", FALSE, ch, 0, 0, TO_ROOM);
    act("$n smirks.", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'b':
    act("$n says 'What a view!  I must get something done about that dump!'",
	FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'c':
    act("$n says 'Vandals!  Youngsters nowadays have no respect for anything!'",
	FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'd':
    act("$n says 'Good day, citizens!'", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'e':
    act("$n says 'I hereby declare the bazaar open!'", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'E':
    act("$n says 'I hereby declare Midgaard closed!'", FALSE, ch, 0, 0, TO_ROOM);
    break;

  case 'O':
    do_gen_door(ch, strcpy(actbuf, "gate"), 0, SCMD_UNLOCK);	/* strcpy: OK */
    do_gen_door(ch, strcpy(actbuf, "gate"), 0, SCMD_OPEN);	/* strcpy: OK */
    break;

  case 'C':
    do_gen_door(ch, strcpy(actbuf, "gate"), 0, SCMD_CLOSE);	/* strcpy: OK */
    do_gen_door(ch, strcpy(actbuf, "gate"), 0, SCMD_LOCK);	/* strcpy: OK */
    break;

  case '.':
    move = FALSE;
    break;

  }

  path_index++;
  return (FALSE);
}

/* General special procedures for mobiles. */

static void npc_steal(struct char_data *ch, struct char_data *victim)
{
  int gold;

  if (IS_NPC(victim))
    return;
  if (GET_REAL_LEVEL(victim) >= LVL_IMMORT)
    return;
  if (!CAN_SEE(ch, victim))
    return;

  if (AWAKE(victim) && (rand_number(0, GET_REAL_LEVEL(ch)) == 0)) {
    act("You discover that $n has $s hands in your wallet.", FALSE, ch, 0, victim, TO_VICT);
    act("$n tries to steal gold from $N.", TRUE, ch, 0, victim, TO_NOTVICT);
  } else {
    /* Steal some gold coins */
    gold = (GET_GOLD(victim) * rand_number(1, 10)) / 100;
    if (gold > 0) {
      increase_gold(ch, gold);
	  decrease_gold(victim, gold);
    }
  }
}

/* Quite lethal to low-level characters. */
SPECIAL(snake)
{
  if (cmd || GET_POS(ch) != POS_FIGHTING || !FIGHTING(ch))
    return (FALSE);

  if (IN_ROOM(FIGHTING(ch)) != IN_ROOM(ch) || rand_number(0, GET_REAL_LEVEL(ch)) != 0)
    return (FALSE);

  act("$n bites $N!", 1, ch, 0, FIGHTING(ch), TO_NOTVICT);
  act("$n bites you!", 1, ch, 0, FIGHTING(ch), TO_VICT);
  call_magic(ch, FIGHTING(ch), 0, SPELL_POISON, GET_REAL_LEVEL(ch), CAST_SPELL);
  return (TRUE);
}

SPECIAL(thief)
{
  struct char_data *cons;

  if (cmd || GET_POS(ch) != POS_STANDING)
    return (FALSE);

  for (cons = world[IN_ROOM(ch)].people; cons; cons = cons->next_in_room)
    if (!IS_NPC(cons) && GET_REAL_LEVEL(cons) < LVL_IMMORT && !rand_number(0, 4)) {
      npc_steal(ch, cons);
      return (TRUE);
    }

  return (FALSE);
}

SPECIAL(magic_user)
{
  struct char_data *vict;

  if (cmd || GET_POS(ch) != POS_FIGHTING)
    return (FALSE);

  /* pseudo-randomly choose someone in the room who is fighting me */
  for (vict = world[IN_ROOM(ch)].people; vict; vict = vict->next_in_room)
    if (FIGHTING(vict) == ch && !rand_number(0, 4))
      break;

  /* if I didn't pick any of those, then just slam the guy I'm fighting */
  if (vict == NULL && IN_ROOM(FIGHTING(ch)) == IN_ROOM(ch))
    vict = FIGHTING(ch);

  /* Hm...didn't pick anyone...I'll wait a round. */
  if (vict == NULL)
    return (TRUE);

  if (GET_REAL_LEVEL(ch) > 13 && rand_number(0, 10) == 0)
    cast_spell(ch, vict, NULL, SPELL_POISON);

  if (GET_REAL_LEVEL(ch) > 7 && rand_number(0, 8) == 0)
    cast_spell(ch, vict, NULL, SPELL_BLINDNESS);

  if (GET_REAL_LEVEL(ch) > 12 && rand_number(0, 12) == 0) {
      cast_spell(ch, vict, NULL, SPELL_ENERGY_DRAIN);
  }

  if (rand_number(0, 4))
    return (TRUE);

  switch (GET_REAL_LEVEL(ch)) {
    case 4:
    case 5:
      cast_spell(ch, vict, NULL, SPELL_MAGIC_MISSILE);
      break;
    case 6:
    case 7:
      cast_spell(ch, vict, NULL, SPELL_CHILL_TOUCH);
      break;
    case 8:
    case 9:
      cast_spell(ch, vict, NULL, SPELL_BURNING_HANDS);
      break;
    case 10:
    case 11:
      cast_spell(ch, vict, NULL, SPELL_SHOCKING_GRASP);
      break;
    case 12:
    case 13:
      cast_spell(ch, vict, NULL, SPELL_LIGHTNING_BOLT);
      break;
    case 14:
    case 15:
    case 16:
    case 17:
      cast_spell(ch, vict, NULL, SPELL_COLOR_SPRAY);
      break;
    default:
      cast_spell(ch, vict, NULL, SPELL_FIREBALL);
      break;
  }
  return (TRUE);
}

/* Special procedures for mobiles. */
SPECIAL(guild_guard) 
{ 
  int i, direction; 
  struct char_data *guard = (struct char_data *)me; 
  const char *buf = "The guard humiliates you, and blocks your way.\r\n"; 
  const char *buf2 = "The guard humiliates $n, and blocks $s way."; 

  if (!IS_MOVE(cmd) || AFF_FLAGGED(guard, AFF_BLIND)) 
    return (FALSE); 
     
  if (GET_REAL_LEVEL(ch) >= LVL_IMMORT) 
    return (FALSE); 
   
  /* find out what direction they are trying to go */ 
  for (direction = 0; direction < NUM_OF_DIRS; direction++)
    if (!strcmp(cmd_info[cmd].command, dirs[direction]))
      for (direction = 0; direction < DIR_COUNT; direction++)
		if (!strcmp(cmd_info[cmd].command, dirs[direction]) ||
			!strcmp(cmd_info[cmd].command, autoexits[direction]))
	      break; 

  for (i = 0; guild_info[i].guild_room != NOWHERE; i++) { 
    /* Wrong guild. */ 
    if (GET_ROOM_VNUM(IN_ROOM(ch)) != guild_info[i].guild_room) 
      continue; 

    /* Wrong direction. */ 
    if (direction != guild_info[i].direction) 
      continue; 

    /* Allow the people of the guild through. */ 
    if (!IS_NPC(ch) && GET_CLASS(ch) == guild_info[i].pc_class) 
      continue; 

    send_to_char(ch, "%s", buf); 
    act(buf2, FALSE, ch, 0, 0, TO_ROOM); 
    return (TRUE); 
  } 
  return (FALSE); 
} 

SPECIAL(puff)
{
  char actbuf[MAX_INPUT_LENGTH];

  if (cmd)
    return (FALSE);

  switch (rand_number(0, 60)) {
    case 0:
      do_say(ch, strcpy(actbuf, "My god!  It's full of stars!"), 0, 0);	/* strcpy: OK */
      return (TRUE);
    case 1:
      do_say(ch, strcpy(actbuf, "How'd all those fish get up here?"), 0, 0);	/* strcpy: OK */
      return (TRUE);
    case 2:
      do_say(ch, strcpy(actbuf, "I'm a very female dragon."), 0, 0);	/* strcpy: OK */
      return (TRUE);
    case 3:
      do_say(ch, strcpy(actbuf, "I've got a peaceful, easy feeling."), 0, 0);	/* strcpy: OK */
      return (TRUE);
    default:
      return (FALSE);
  }
}

SPECIAL(fido)
{
  struct obj_data *i, *temp, *next_obj;

  if (cmd || !AWAKE(ch))
    return (FALSE);

  for (i = world[IN_ROOM(ch)].contents; i; i = i->next_content) {
    if (!IS_CORPSE(i))
      continue;

    act("$n savagely devours a corpse.", FALSE, ch, 0, 0, TO_ROOM);
    for (temp = i->contains; temp; temp = next_obj) {
      next_obj = temp->next_content;
      obj_from_obj(temp);
      obj_to_room(temp, IN_ROOM(ch));
    }
    extract_obj(i);
    return (TRUE);
  }
  return (FALSE);
}

SPECIAL(janitor)
{
  struct obj_data *i;

  if (cmd || !AWAKE(ch))
    return (FALSE);

  for (i = world[IN_ROOM(ch)].contents; i; i = i->next_content) {
    if (!CAN_WEAR(i, ITEM_WEAR_TAKE))
      continue;
    if (GET_OBJ_TYPE(i) != ITEM_DRINKCON && GET_OBJ_COST(i) >= 15)
      continue;
    act("$n picks up some trash.", FALSE, ch, 0, 0, TO_ROOM);
    obj_from_room(i);
    obj_to_char(i, ch);
    return (TRUE);
  }
  return (FALSE);
}

SPECIAL(cityguard)
{
  struct char_data *tch, *evil, *spittle;
  int max_evil, min_str;

  if (cmd || !AWAKE(ch) || FIGHTING(ch))
    return (FALSE);

  max_evil = 1000;
  min_str = 9;
  spittle = evil = NULL;

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room) {
    if (!CAN_SEE(ch, tch))
      continue;
    if (!IS_NPC(tch) && PLR_FLAGGED(tch, PLR_KILLER)) {
      act("$n screams 'HEY!!!  You're one of those PLAYER KILLERS!!!!!!'", FALSE, ch, 0, 0, TO_ROOM);
      hit(ch, tch, TYPE_UNDEFINED);
      return (TRUE);
    }

    if (!IS_NPC(tch) && PLR_FLAGGED(tch, PLR_THIEF)) {
      act("$n screams 'HEY!!!  You're one of those PLAYER THIEVES!!!!!!'", FALSE, ch, 0, 0, TO_ROOM);
      hit(ch, tch, TYPE_UNDEFINED);
      return (TRUE);
    }

    if (FIGHTING(tch) && GET_ALIGNMENT(tch) < max_evil && (IS_NPC(tch) || IS_NPC(FIGHTING(tch)))) {
      max_evil = GET_ALIGNMENT(tch);
      evil = tch;
    }

    if (GET_STR(tch) < min_str) {
      spittle = tch;
      min_str = GET_STR(tch);
    }
  }

  if (evil && GET_ALIGNMENT(FIGHTING(evil)) >= 0) {
    act("$n screams 'PROTECT THE INNOCENT!  BANZAI!  CHARGE!  ARARARAGGGHH!'", FALSE, ch, 0, 0, TO_ROOM);
    hit(ch, evil, TYPE_UNDEFINED);
    return (TRUE);
  }

  /* Reward the socially inept. */
  if (spittle && !rand_number(0, 9)) {
    static int spit_social;

    if (!spit_social)
      spit_social = find_command("spit");

    if (spit_social > 0) {
      char spitbuf[MAX_NAME_LENGTH + 1];
      strncpy(spitbuf, GET_NAME(spittle), sizeof(spitbuf));	/* strncpy: OK */
      spitbuf[sizeof(spitbuf) - 1] = '\0';
      do_action(ch, spitbuf, spit_social, 0);
      return (TRUE);
    }
  }
  return (FALSE);
}

#define PET_PRICE(pet) (GET_REAL_LEVEL(pet) * 300)
SPECIAL(pet_shops)
{
  char buf[MAX_STRING_LENGTH], pet_name[256];
  room_rnum pet_room;
  struct char_data *pet;

  /* Gross. */
  pet_room = IN_ROOM(ch) + 1;

  if (CMD_IS("list")) {
    send_to_char(ch, "Available pets are:\r\n");
    for (pet = world[pet_room].people; pet; pet = pet->next_in_room) {
      /* No, you can't have the Implementor as a pet if he's in there. */
      if (!IS_NPC(pet))
        continue;
      send_to_char(ch, "%8d - %s\r\n", PET_PRICE(pet), GET_NAME(pet));
    }
    return (TRUE);
  } else if (CMD_IS("buy")) {

    two_arguments(argument, buf, pet_name);

    if (!(pet = get_char_room(buf, NULL, pet_room)) || !IS_NPC(pet)) {
      send_to_char(ch, "There is no such pet!\r\n");
      return (TRUE);
    }
    if (GET_GOLD(ch) < PET_PRICE(pet)) {
      send_to_char(ch, "You don't have enough gold!\r\n");
      return (TRUE);
    }
    decrease_gold(ch, PET_PRICE(pet));

    pet = read_mobile(GET_MOB_RNUM(pet), REAL);
    GET_EXP(pet) = 0;
    SET_BIT_AR(AFF_FLAGS(pet), AFF_CHARM);

    if (*pet_name) {
      snprintf(buf, sizeof(buf), "%s %s", pet->player.name, pet_name);
      /* free(pet->player.name); don't free the prototype! */
      pet->player.name = strdup(buf);

      snprintf(buf, sizeof(buf), "%sA small sign on a chain around the neck says 'My name is %s'\r\n",
	      pet->player.description, pet_name);
      /* free(pet->player.description); don't free the prototype! */
      pet->player.description = strdup(buf);
    }
    char_to_room(pet, IN_ROOM(ch));
    add_follower(pet, ch);

    /* Be certain that pets can't get/carry/use/wield/wear items */
    IS_CARRYING_W(pet) = 1000;
    IS_CARRYING_N(pet) = 100;

    send_to_char(ch, "May you enjoy your pet.\r\n");
    act("$n buys $N as a pet.", FALSE, ch, 0, pet, TO_ROOM);

    return (TRUE);
  }

  /* All commands except list and buy */
  return (FALSE);
}

/* Special procedures for objects. */
SPECIAL(bank)
{
  long amount;

  if (CMD_IS("balance")) {
    if (GET_BANK_GOLD(ch) > 0)
      send_to_char(ch, "Your current balance is %s coins.\r\n", add_commas(GET_BANK_GOLD(ch)));
    else
      send_to_char(ch, "You currently have no money deposited.\r\n");
    return (TRUE);
  } else if (CMD_IS("deposit")) {
    if ((amount = atol(argument)) <= 0) {
      send_to_char(ch, "How much do you want to deposit?\r\n");
      return (TRUE);
    }
    if (GET_GOLD(ch) < amount) {
      send_to_char(ch, "You don't have that many coins!\r\n");
      return (TRUE);
    }
    decrease_gold(ch, amount);
	increase_bank(ch, amount);
    send_to_char(ch, "You deposit %ld coins.\r\n", amount);
    act("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
    return (TRUE);
  } else if (CMD_IS("withdraw")) {
    if ((amount = atol(argument)) <= 0) {
      send_to_char(ch, "How much do you want to withdraw?\r\n");
      return (TRUE);
    }
    if (GET_BANK_GOLD(ch) < amount) {
      send_to_char(ch, "You don't have that many coins deposited!\r\n");
      return (TRUE);
    }
    increase_gold(ch, amount);
	decrease_bank(ch, amount);
    send_to_char(ch, "You withdraw %ld coins.\r\n", amount);
    act("$n makes a bank transaction.", TRUE, ch, 0, FALSE, TO_ROOM);
    return (TRUE);
  } else
    return (FALSE);
}

