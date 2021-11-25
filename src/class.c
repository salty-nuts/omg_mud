/**************************************************************************
*  File: class.c                                           Part of tbaMUD *
*  Usage: Source file for class-specific code.                            *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

/* This file attempts to concentrate most of the code which must be changed
 * in order for new classes to be added.  If you're adding a new class, you
 * should go through this entire file from beginning to end and add the
 * appropriate new special cases for your new class. */

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "spells.h"
#include "interpreter.h"
#include "constants.h"
#include "act.h"
#include "class.h"

/* Names first */
const char *class_abbrevs[] = {
  "Wi", // unused
  "Pr", // unused
  "Ro", // unused
  "Fi", // unused
  "Kn", // Salty
  "Ba",
  "\n"
};

const char *pc_class_types[] = {
  "Wizard",
  "Priest",
  "Rogue",
  "Fighter",
  "Knight", // Salty
  "Bard",
  "\n"
};

/* The menu for choosing a class in interpreter.c: */
const char *class_menu =
"\r\n"
"Select a class:\r\n"
"  [\t(P\t)]riest\r\n"
"  [\t(R\t)]ogue\r\n"
"  [\t(F\t)]ighter\r\n"
"  [\t(W\t)]izard\r\n"
"  [\t(K\t)]night\r\n"
"  [\t(B\t)]ard\r\n";

const char *reroll_menu =
"\n\r"
" [\t(K\t)]eep these stats.\n\r"
" [\t(R\t)]eroll these stats.\n\r";

int parse_reroll(char arg)
{
 arg = LOWER(arg);

 switch(arg){
 case 'k': return FALSE;
 case 'r': return TRUE;
 default: return -1;
 }
}
/* The code to interpret a class letter -- used in interpreter.c when a new
 * character is selecting a class and by 'set class' in act.wizard.c. */
int parse_class(char arg)
{
  arg = LOWER(arg);

  switch (arg) {
  case 'w': return CLASS_WIZARD;
  case 'p': return CLASS_PRIEST;
  case 'f': return CLASS_FIGHTER;
  case 'r': return CLASS_ROGUE;
  case 'k': return CLASS_KNIGHT;
  case 'b': return CLASS_BARD;
  default:  return CLASS_UNDEFINED;
  }
}

/* bitvectors (i.e., powers of two) for each class, mainly for use in do_who
 * and do_users.  Add new classes at the end so that all classes use sequential
 * powers of two (1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, etc.) up to
 * the limit of your bitvector_t, typically 0-31. */
bitvector_t find_class_bitvector(const char *arg)
{
  size_t rpos, ret = 0;

  for (rpos = 0; rpos < strlen(arg); rpos++)
    ret |= (1 << parse_class(arg[rpos]));

  return (ret);
}

/* These are definitions which control the guildmasters for each class.
 * The  first field (top line) controls the highest percentage skill level a
 * character of the class is allowed to attain in any skill.  (After this
 * level, attempts to practice will say "You are already learned in this area."
 *
 * The second line controls the maximum percent gain in learnedness a character
 * is allowed per practice -- in other words, if the random die throw comes out
 * higher than this number, the gain will only be this number instead.
 *
 * The third line controls the minimu percent gain in learnedness a character
 * is allowed per practice -- in other words, if the random die throw comes
 * out below this number, the gain will be set up to this number.
 *
 * The fourth line simply sets whether the character knows 'spells' or 'skills'.
 * This does not affect anything except the message given to the character when
 * trying to practice (i.e. "You know of the following spells" vs. "You know of
 * the following skills" */

#define SPELL	0
#define SKILL	1


int prac_params[4][NUM_CLASSES] = {
/* 	Wi    Pr  Ro  Fi  Kn Ba			*/
  { 80,		80,	80,	80,	80,	80		},	/* learned level */
  { 15,		15,	10,	10,	10,	10		},	/* max per practice */
  { 1,		1,	1,	1,	1,	1		},	/* min per practice */
  { SPELL,	SPELL,	SKILL,	SKILL,	SKILL, SKILL		},	/* prac name */
};

/* The appropriate rooms for each guildmaster/guildguard; controls which types
 * of people the various guildguards let through.  i.e., the first line shows
 * that from room 3017, only MAGIC_USERS are allowed to go south. Don't forget
 * to visit spec_assign.c if you create any new mobiles that should be a guild
 * master or guard so they can act appropriately. If you "recycle" the
 * existing mobs that are used in other guilds for your new guild, then you
 * don't have to change that file, only here. Guildguards are now implemented
 * via triggers. This code remains as an example. */
struct guild_info_type guild_info[] = {

/* Midgaard */
 { CLASS_WIZARD,    3017,    SOUTH   },
 { CLASS_PRIEST,        3004,    NORTH   },
 { CLASS_ROGUE,         3027,    EAST   },
 { CLASS_FIGHTER,       3021,    EAST   },

/* Brass Dragon */
  { -999 /* all */ ,	5065,	WEST	},

/* this must go last -- add new guards above! */
  { -1, NOWHERE, -1}
};

/* Saving throws for : MCTW : PARA, ROD, PETRI, BREATH, SPELL. Levels 0-40. Do
 * not forget to change extern declaration in magic.c if you add to this. */
byte saving_throws(int class_num, int type, int level)
{
  int result = 100;
  if (class_num >= 0)
  {
    switch(type)
    {
      case SAVING_PARA:
      case SAVING_ROD:
      case SAVING_PETRI:
      case SAVING_BREATH:
      case SAVING_SPELL:
        return result - level;
      default:
	return result - level;
    }
  }
  else
    return result - level;
}

/* THAC0 for classes and levels.  (To Hit Armor Class 0) */
int thaco(int class_num, int level)
{
	int mod;
	switch (level)
	{
    case  0:
			mod = 100;
			break;
    case  1:
			mod = 20;
			break;
    case  2:
			mod = 20;
			break;
    case  3:
			mod = 20;
			break;
    case  4:
			mod = 19;
			break;
    case  5:
			mod = 19;
			break;
    case  6:
			mod = 19;
			break;
    case  7:
			mod = 18;
			break;
    case  8:
			mod = 18;
			break;
    case  9:
			mod = 18;
			break;
    case 10:
			mod = 17;
			break;
    case 11:
			mod = 17;
			break;
    case 12:
			mod = 17;
			break;
    case 13:
			mod = 16;
			break;
    case 14:
			mod = 16;
			break;
    case 15:
			mod = 16;
			break;
    case 16:
			mod = 15;
			break;
    case 17:
			mod = 15;
			break;
    case 18:
			mod = 15;
			break;
    case 19:
			mod = 14;
			break;
    case 20:
			mod = 14;
			break;
    case 21:
			mod = 14;
			break;
    case 22:
			mod = 13;
			break;
    case 23:
			mod = 13;
			break;
    case 24:
			mod = 13;
			break;
    case 25:
			mod = 12;
			break;
    case 26:
			mod = 12;
			break;
    case 27:
			mod = 12;
			break;
    case 28:
			mod = 11;
			break;
    case 29:
			mod = 11;
			break;
    case 30:
			mod = 11;
			break;
    case 31:
			mod = 10;
			break;
    case 32:
			mod = 10;
			break;
    case 33:
			mod = 10;
			break;
    case 34:
			mod = 9;
			break;
    case 35:
			mod = 9;
			break;
    case 36:
			mod = 9;
			break;
    case 37:
			mod = 8;
			break;
    case 38:
			mod = 8;
			break;
    case 39:
			mod = 8;
			break;
    case 40:
			mod = 7;
			break;
    case 41:
			mod = 7;
			break;
    case 42:
			mod = 7;
			break;
    case 43:
			mod = 7;
			break;
    case 44:
			mod = 6;
			break;
    case 45:
			mod = 6;
			break;
    case 46:
			mod = 6;
			break;
    case 47:
			mod = 5;
			break;
    case 48:
			mod = 5;
			break;
    case 49:
			mod = 5;
			break;
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
			mod = 4;
			break;
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
			mod = 1;
			break;
    default:
			mod = 1;
			log("SYSERR: Missing level in thac0 chart.");
			break;
    }
	switch (class_num)
	{
		case CLASS_WIZARD:
			return mod;
		case CLASS_PRIEST:
			return mod;
		case CLASS_ROGUE:
			return (mod - 2);
		case CLASS_FIGHTER:
			return (mod - 3);
		case CLASS_KNIGHT:
			return (mod - 2);
		case CLASS_BARD:
			return mod;
		default:
	    log("SYSERR: Unknown class in thac0 chart.");
	}
	return mod;
}





// Changed to 3d6+7, 10-25 range
// Salty 04 JAN 2019

/* Roll the 6 stats for a character... each stat is made of the sum of the best
 * 3 out of 4 rolls of a 6-sided die.  Each class then decides which priority
 * will be given for the best to worst stats. */
void roll_real_abils(struct char_data *ch)
{
  int i, j, k, temp;
  ubyte table[6];
  ubyte rolls[4];

  for (i = 0; i < 6; i++)
    table[i] = 0;

  for (i = 0; i < 6; i++)
  {
    for (j = 0;j < 4;j++)
			rolls[j] = dice(1,6);

    temp = rolls[0] + rolls[1] + rolls[2] + rolls[3] - MIN(rolls[0], MIN(rolls[1], MIN(rolls[2], rolls[3])));

    for (k = 0; k < 6; k++)
      if (table[k] < temp)
			{
				temp ^= table[k];
				table[k] ^= temp;
				temp ^= table[k];
      }
  }

  ch->real_abils.str_add = 0;


  switch (GET_CLASS(ch)) {
  case CLASS_WIZARD:
    ch->real_abils.intel = table[0];
    ch->real_abils.wis = table[1];
    ch->real_abils.dex = table[2];
    ch->real_abils.con = table[3];
    ch->real_abils.str = table[4];
    ch->real_abils.luck = table[5];
    break;
  case CLASS_PRIEST:
    ch->real_abils.wis = table[0];
    ch->real_abils.con = table[1];
    ch->real_abils.str = table[2];
    ch->real_abils.intel = table[3];
    ch->real_abils.dex = table[4];
    ch->real_abils.luck = table[5];
    break;
  case CLASS_ROGUE:
    ch->real_abils.dex = table[0];
    ch->real_abils.luck = table[1];
    ch->real_abils.con = table[2];
    ch->real_abils.str = table[3];
    ch->real_abils.intel = table[4];
    ch->real_abils.wis = table[5];
    break;
  case CLASS_FIGHTER:
    ch->real_abils.str = table[0];
    ch->real_abils.con = table[1];
    ch->real_abils.dex = table[2];
    ch->real_abils.luck = table[3];
    ch->real_abils.wis = table[4];
    ch->real_abils.intel = table[5];
    if (ch->real_abils.str == 25)
      ch->real_abils.str_add = rand_number(0, 100);
    break;
  case CLASS_KNIGHT:
    ch->real_abils.con = table[0];
    ch->real_abils.dex = table[1];
    ch->real_abils.str = table[2];
    ch->real_abils.wis = table[3];
    ch->real_abils.intel = table[4];
    ch->real_abils.luck = table[5];
    if (ch->real_abils.str == 25)
      ch->real_abils.str_add = rand_number(0, 100);
    break;
  case CLASS_BARD:
    ch->real_abils.dex = table[0];
    ch->real_abils.luck = table[1];
    ch->real_abils.wis = table[2];
    ch->real_abils.con = table[3];
    ch->real_abils.intel = table[4];
    ch->real_abils.str = table[5];
    if (ch->real_abils.str == 25)
      ch->real_abils.str_add = rand_number(0, 100);
    break;

	default:
	  ch->real_abils.str = table[0];
  	ch->real_abils.dex = table[1];
  	ch->real_abils.con = table[2];
  	ch->real_abils.wis = table[3];
  	ch->real_abils.intel = table[4];
  	ch->real_abils.luck = table[5];
  	if (ch->real_abils.str == 25)
    	ch->real_abils.str_add = rand_number(0, 100);
		break;
  }
  ch->aff_abils = ch->real_abils;
}

/* Some initializations for characters, including initial skills */
void do_start(struct char_data *ch)
{
//  GET_REAL_LEVEL(ch) = 1;
  GET_EXP(ch) = 1;
  GET_TRAINS(ch) = 1;
  GET_PRACTICES(ch) = 5;
  GET_MULTI_CLASS(ch) = GET_CLASS(ch);
  set_title(ch, NULL);

  GET_MAX_HIT(ch)  = 100;
  GET_MAX_MANA(ch) = 100;
  GET_MAX_MOVE(ch) = 0;

  switch (GET_CLASS(ch)) {

  case CLASS_WIZARD:
    GET_LEVEL(ch, CLASS_WIZARD) = 1;
    break;

  case CLASS_PRIEST:
    GET_LEVEL(ch, CLASS_PRIEST) = 1;
    break;

  case CLASS_ROGUE:
    GET_LEVEL(ch, CLASS_ROGUE) = 1;
    break;

  case CLASS_FIGHTER:
    GET_LEVEL(ch, CLASS_FIGHTER) = 1;
    break;

  case CLASS_KNIGHT:
    GET_LEVEL(ch, CLASS_KNIGHT) = 1;
    break;
  case CLASS_BARD:
    GET_LEVEL(ch, CLASS_BARD) = 1;
    break;
  }

  advance_level(ch);
  GET_HIT(ch) = GET_MAX_HIT(ch);
  GET_MANA(ch) = GET_MAX_MANA(ch);
  GET_MOVE(ch) = GET_MAX_MOVE(ch);

/*   GET_COND(ch, THIRST) = -1;
  GET_COND(ch, HUNGER) = -1;
  GET_COND(ch, DRUNK) = -1; */

  if (CONFIG_SITEOK_ALL)
    SET_BIT_AR(PLR_FLAGS(ch), PLR_SITEOK);
}

/* This function controls the change to maxmove, maxmana, and maxhp for each
 * class every time they gain a level. */
void advance_level(struct char_data *ch)
{
  int skill_gain = 80, add_hp = 0, add_mana = 0, add_move = 100;

  switch (GET_CLASS(ch))
  {
    case CLASS_WIZARD:
      add_mana =  dice(4,int_app[GET_INT(ch)].mana_gain);
      add_hp =  dice(1,con_app[GET_CON(ch)].hitp);
      break;

    case CLASS_PRIEST:
      add_mana =  dice(3,wis_app[GET_WIS(ch)].mana_gain);
      add_hp =  dice(2,con_app[GET_CON(ch)].hitp);
      break;

    case CLASS_ROGUE:
      add_mana =  dice(2,int_app[GET_INT(ch)].mana_gain);
      add_hp =  dice(3,con_app[GET_CON(ch)].hitp);
      break;

    case CLASS_FIGHTER:
      add_mana =  dice(2,int_app[GET_INT(ch)].mana_gain);
      add_hp =  dice(3,con_app[GET_CON(ch)].hitp);
      break;

    case CLASS_KNIGHT:
      add_mana =  dice(1,int_app[GET_INT(ch)].mana_gain);
      add_hp =  dice(4,con_app[GET_CON(ch)].hitp);
      break;

    case CLASS_BARD:
      add_mana =  dice(2,wis_app[GET_WIS(ch)].mana_gain);
      add_hp =  dice(3,con_app[GET_CON(ch)].hitp);
      break;

    default:
      add_mana = int_app[GET_INT(ch)].mana_gain;
      add_hp = con_app[GET_CON(ch)].hitp;
      break;
  }

  ch->points.max_hit += MAX(1, add_hp);
  ch->points.max_mana += MAX(1, add_mana);
  ch->points.max_move += MAX(1, add_move);

//  if (GET_REAL_LEVEL(ch) > 1)
//    ch->points.max_mana += add_mana;

  GET_PRACTICES(ch) += 3 + wis_app[GET_WIS(ch)].bonus;


//  if (GET_REAL_LEVEL(ch) % 5 == 0)
		GET_TRAINS(ch) += dice(1,4);

  for (int i = 1;  i <= NUM_SPELLS; i++)
  {
    if ( ( (GET_LEVEL(ch, GET_CLASS(ch)) >= spell_info[i].min_level[(int)GET_CLASS(ch)]) ||
    (GET_LEVEL(ch, GET_MULTI_CLASS(ch)) >= spell_info[i].min_level[(int)GET_MULTI_CLASS(ch)]) ) &&
    (GET_SKILL(ch, i) == 0) )
      SET_SKILL(ch, i, skill_gain);
  }

  for (int i = ZERO_SONGS;  i <= ZERO_SONGS + NUM_SONGS; i++)
  {
    if ( ( (GET_LEVEL(ch, GET_CLASS(ch)) >= spell_info[i].min_level[(int)GET_CLASS(ch)]) ||
    (GET_LEVEL(ch, GET_MULTI_CLASS(ch)) >= spell_info[i].min_level[(int)GET_MULTI_CLASS(ch)]) ) &&
    (GET_SKILL(ch, i) == 0) )
      SET_SKILL(ch, i, skill_gain);
  }
  for (int j = ZERO_SKILLS;  j <= ZERO_SKILLS + NUM_SKILLS +1; j++)
  {
    if ( ( (GET_LEVEL(ch, GET_CLASS(ch)) >= spell_info[j].min_level[(int)GET_CLASS(ch)]) ||
    (GET_LEVEL(ch, GET_MULTI_CLASS(ch)) >= spell_info[j].min_level[(int)GET_MULTI_CLASS(ch)]) ) &&
    (GET_SKILL(ch, j) == 0) )
      SET_SKILL(ch, j, skill_gain);
  }

  if (GET_REAL_LEVEL(ch) >= LVL_IMMORT)
  {
    for (int i = 0; i < 3; i++)
      GET_COND(ch, i) = (char) -1;
    SET_BIT_AR(PRF_FLAGS(ch), PRF_HOLYLIGHT);
  }
	send_to_char(ch, "You gain %d hp, %d mana and %d movement upon leveling up!\n\r", add_hp, add_mana, add_move);
  snoop_check(ch);
  save_char(ch);
}

/* invalid_class is used by handler.c to determine if a piece of equipment is
 * usable by a particular class, based on the ITEM_ANTI_{class} bitvectors. */
int invalid_class(struct char_data *ch, struct obj_data *obj)
{
  if (OBJ_FLAGGED(obj, ITEM_ANTI_WIZARD) && IS_WIZARD(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_PRIEST) && IS_PRIEST(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_FIGHTER) && IS_FIGHTER(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_ROGUE) && IS_ROGUE(ch))
    return TRUE;

  if (OBJ_FLAGGED(obj, ITEM_ANTI_KNIGHT) && IS_KNIGHT(ch))
    return TRUE;

  return FALSE;
}

/* 
 * SPELLS AND SKILLS. 
 * This area defines which spells are assigned to which classes, 
 * and the minimum level the character must be to use the spell or
 * skill. 
 * 
 */
void init_wizard(void)
{
  /* WIZARD UTILITY */
  spell_level(SPELL_WORD_OF_RECALL, CLASS_WIZARD, 1);
  spell_level(SPELL_IDENTIFY, CLASS_WIZARD, 5);
  spell_level(SPELL_LOCATE_OBJECT, CLASS_WIZARD, 6);
  spell_level(SPELL_LOCATE_CHARACTER, CLASS_WIZARD, 6);
  spell_level(SPELL_RELOCATION, CLASS_WIZARD, 10);
  spell_level(SPELL_ENCHANT_WEAPON, CLASS_WIZARD, 21);
  spell_level(SPELL_BLOOD_MANA, CLASS_WIZARD, 30);

  /* WIZARD BUFFS */
  spell_level(SPELL_INVISIBLE, CLASS_WIZARD, 5);
  spell_level(SPELL_ARMOR, CLASS_WIZARD, 5);
  spell_level(SPELL_MAGE_ARMOR, CLASS_WIZARD, 5);
  spell_level(SPELL_STRENGTH, CLASS_WIZARD, 6);
  spell_level(SPELL_TRUESIGHT, CLASS_WIZARD, 10);
  spell_level(SPELL_MIRROR_IMAGE, CLASS_WIZARD, 20);
  spell_level(SPELL_FLY, CLASS_WIZARD, 20);
  spell_level(SPELL_PASS_DOOR, CLASS_WIZARD, 20);
  spell_level(SPELL_HASTE, CLASS_WIZARD, 30);
  spell_level(SPELL_IMPROVED_INVIS, CLASS_WIZARD, 21);
  spell_level(SPELL_IMP_PASS_DOOR, CLASS_WIZARD, 30);
  spell_level(SPELL_QUICKCAST, CLASS_WIZARD, 30);
  spell_level(SPELL_GROUP_HASTE, CLASS_WIZARD, 30);
  spell_level(SPELL_GROUP_STEEL_SKIN, CLASS_WIZARD, 10);
  spell_level(SPELL_GROUP_STONE_SKIN, CLASS_WIZARD, 10);

  /* WIZARD DEBUFFS */
  spell_level(SPELL_PARALYZE, CLASS_WIZARD, 25);
  spell_level(SPELL_BETRAYAL, CLASS_WIZARD, 30);

  /* WIZARD SKILLS */
  spell_level(SKILL_MAGIC_RECOVERY, CLASS_WIZARD, 1);

  /* WIZARD DAMAGE */
  spell_level(SPELL_MAGIC_MISSILE, CLASS_WIZARD, 1);
  spell_level(SPELL_ENERGY_DRAIN, CLASS_WIZARD, 12);
  spell_level(SPELL_FIREBALL, CLASS_WIZARD, 15);
  spell_level(SPELL_FIREBLAST, CLASS_WIZARD, 25);
  spell_level(SPELL_MISSILE_SPRAY, CLASS_WIZARD, 30);  
}
void init_priest(void)
{
  /* PRIEST */
  spell_level(SPELL_WORD_OF_RECALL, CLASS_PRIEST, 1);
  spell_level(SKILL_MAGIC_RECOVERY, CLASS_PRIEST, 1);
  spell_level(SPELL_CURE_BLIND, CLASS_PRIEST, 1);
  spell_level(SPELL_REMOVE_POISON, CLASS_PRIEST, 1);
  spell_level(SPELL_REMOVE_CURSE, CLASS_PRIEST, 1);
  spell_level(SPELL_SATIATE, CLASS_PRIEST, 5);
  spell_level(SPELL_BLESS, CLASS_PRIEST, 1);
  spell_level(SPELL_SUMMON, CLASS_PRIEST, 10);
  spell_level(SPELL_ARMOR, CLASS_PRIEST, 5);
  spell_level(SPELL_SANCTUARY, CLASS_PRIEST, 5);
  spell_level(SPELL_HEAL, CLASS_PRIEST, 5);
  spell_level(SPELL_HARM, CLASS_PRIEST, 5);
  spell_level(SPELL_RELOCATION, CLASS_PRIEST, 10);
  spell_level(SPELL_STONE_SKIN, CLASS_PRIEST, 10);
  spell_level(SPELL_LOCATE_OBJECT, CLASS_PRIEST, 15);
  spell_level(SPELL_LOCATE_CHARACTER, CLASS_PRIEST, 15);
  spell_level(SPELL_CANCELLATION, CLASS_PRIEST, 15);
  spell_level(SPELL_REGENERATION, CLASS_PRIEST, 20);
  spell_level(SPELL_POWERHEAL, CLASS_PRIEST, 20);
  spell_level(SPELL_NIGHTMARE, CLASS_PRIEST, 20);
  spell_level(SPELL_MIRACLE, CLASS_PRIEST, 25);
  spell_level(SPELL_SYMBOL_OF_PAIN, CLASS_PRIEST, 30);
  spell_level(SPELL_HOLY_WARDING, CLASS_PRIEST, 20);
  spell_level(SPELL_EVIL_WARDING, CLASS_PRIEST, 20);
  spell_level(SPELL_GROUP_RECALL, CLASS_PRIEST, 10);
  spell_level(SPELL_GROUP_HEAL, CLASS_PRIEST, 10);
  spell_level(SPELL_GROUP_BLESS, CLASS_PRIEST, 10);
  spell_level(SPELL_GROUP_ARMOR, CLASS_PRIEST, 10);
  spell_level(SPELL_GROUP_SATIATE, CLASS_PRIEST, 10);
  spell_level(SPELL_GROUP_SUMMON, CLASS_PRIEST, 20);
  spell_level(SPELL_GROUP_SANCTUARY, CLASS_PRIEST, 20);
  spell_level(SPELL_GROUP_PASS_DOOR, CLASS_PRIEST, 20);
  spell_level(SPELL_GROUP_MIRACLE, CLASS_PRIEST, 30);
  spell_level(SPELL_GROUP_REGEN, CLASS_PRIEST, 30);
}
void init_rogue(void)
{
  // ROGUE UTILITY 
  spell_level(SKILL_HEALTH_RECOVERY, CLASS_ROGUE, 1);
  spell_level(SKILL_SNEAK, CLASS_ROGUE, 1);
  spell_level(SKILL_HIDE, CLASS_ROGUE, 1);
  spell_level(SKILL_PICK_LOCK, CLASS_ROGUE, 1);
  spell_level(SKILL_TRACK, CLASS_ROGUE, 5);
  spell_level(SKILL_HUNT, CLASS_ROGUE, 21);
  // ROGUE DEFENSE
  spell_level(SKILL_DODGE, CLASS_ROGUE, 1);
  // ROGUE OFFENSE ACTIVE
  spell_level(SKILL_BACKSTAB, CLASS_ROGUE, 1);
  spell_level(SKILL_TUMBLE, CLASS_ROGUE, 20);
  spell_level(SKILL_CIRCLE, CLASS_ROGUE, 21);
  // ROGUE OFFENSE PASSIVE
  spell_level(SKILL_SECOND_ATTACK, CLASS_ROGUE, 5);
  spell_level(SKILL_DUAL_WIELD, CLASS_ROGUE, 5);
  spell_level(SKILL_ENHANCED_DAMAGE,CLASS_ROGUE, 10);
  spell_level(SKILL_CRITICAL_HIT, CLASS_ROGUE, 15);
  spell_level(SKILL_THIRD_ATTACK, CLASS_ROGUE, 15);
  spell_level(SKILL_DOUBLE_BACKSTAB, CLASS_ROGUE, 30);
  spell_level(SKILL_EXECUTE, CLASS_ROGUE, 30);
  spell_level(SKILL_DIRTY_TRICKS, CLASS_ROGUE, 30);
  spell_level(SKILL_IMPALE, CLASS_ROGUE, 30);
  spell_level(SKILL_REND, CLASS_ROGUE, 30);
  spell_level(SKILL_MINCE, CLASS_ROGUE, 30);
  spell_level(SKILL_THRUST, CLASS_ROGUE, 30);
  spell_level(SKILL_ACROBATICS, CLASS_ROGUE, 30);  
}
void init_fighter(void)
{
  /* FIGHTER */
  spell_level(SKILL_KICK, CLASS_FIGHTER, 1);
  spell_level(SKILL_SECOND_ATTACK, CLASS_FIGHTER, 1);
  spell_level(SKILL_HEALTH_RECOVERY, CLASS_FIGHTER, 1);
  spell_level(SKILL_HEADBUTT, CLASS_FIGHTER, 1);
  spell_level(SKILL_CRITICAL_HIT, CLASS_FIGHTER, 10);
  spell_level(SKILL_SWITCH_TARGET, CLASS_FIGHTER, 15);
  spell_level(SKILL_THIRD_ATTACK, CLASS_FIGHTER, 5);
  spell_level(SKILL_ENHANCED_DAMAGE,CLASS_FIGHTER, 20);
  spell_level(SKILL_OFFENSIVE_STANCE, CLASS_FIGHTER, 5);
  spell_level(SKILL_FOURTH_ATTACK, CLASS_FIGHTER, 10);
  spell_level(SKILL_FIFTH_ATTACK, CLASS_FIGHTER, 15);
  spell_level(SKILL_RAGE, CLASS_FIGHTER, 30);
  spell_level(SKILL_UNARMED_COMBAT, CLASS_FIGHTER, 30);
  spell_level(SKILL_L_HOOK, CLASS_FIGHTER, 20);
  spell_level(SKILL_R_HOOK, CLASS_FIGHTER, 20);
  spell_level(SKILL_SUCKER_PUNCH, CLASS_FIGHTER, 20);
  spell_level(SKILL_UPPERCUT, CLASS_FIGHTER, 25);
  spell_level(SKILL_HAYMAKER, CLASS_FIGHTER, 25);
  spell_level(SKILL_CLOTHESLINE, CLASS_FIGHTER, 25);
  spell_level(SKILL_PILEDRVIER, CLASS_FIGHTER, 30);
  spell_level(SKILL_PALM_STRIKE, CLASS_FIGHTER, 30);  
}
void init_knight(void)
{
  /* KNIGHT */
  spell_level(SKILL_RESCUE, CLASS_KNIGHT, 1);
  spell_level(SKILL_SWITCH_TARGET, CLASS_KNIGHT, 1);
  spell_level(SKILL_SECOND_ATTACK, CLASS_KNIGHT, 1);
  spell_level(SKILL_HEALTH_RECOVERY, CLASS_KNIGHT, 1);
  spell_level(SKILL_BASH, CLASS_KNIGHT, 1);
  spell_level(SKILL_PARRY, CLASS_KNIGHT, 1);
  spell_level(SKILL_ASSAULT, CLASS_KNIGHT, 5);
  spell_level(SKILL_DAMAGE_REDUCTION, CLASS_KNIGHT, 5);
  spell_level(SKILL_SHIELD_MASTER, CLASS_KNIGHT, 20);
  spell_level(SKILL_THIRD_ATTACK, CLASS_KNIGHT, 20);
  spell_level(SKILL_TAUNT, CLASS_KNIGHT, 30);
  spell_level(SKILL_MAGIC_RECOVERY, CLASS_KNIGHT, 21);
  spell_level(SKILL_ENHANCED_PARRY, CLASS_KNIGHT, 25);
  spell_level(SKILL_DEFENSIVE_STANCE, CLASS_KNIGHT, 5);
}
void init_bard(void)
{
  /* BARD */

  spell_level(SKILL_MAGIC_RECOVERY, CLASS_BARD, 1);
  spell_level(SKILL_DODGE, CLASS_BARD, 1);
  spell_level(SKILL_SECOND_ATTACK, CLASS_BARD, 1);
  spell_level(SKILL_TRACK, CLASS_BARD, 5);
  spell_level(SKILL_THIRD_ATTACK, CLASS_BARD, 10);
  spell_level(SKILL_TUMBLE, CLASS_BARD, 15);
  spell_level(SKILL_CHANT, CLASS_BARD, 10);
  spell_level(SKILL_BATTLE_RYTHM, CLASS_BARD, 15);
  spell_level(SPELL_BARD_BLESS, CLASS_BARD, 5);
  spell_level(SPELL_BARD_BUFF, CLASS_BARD, 5);
  spell_level(SPELL_BARD_HEAL, CLASS_BARD, 10);
  spell_level(SPELL_BARD_FEAST, CLASS_BARD, 15);
  spell_level(SPELL_BARD_POWERHEAL, CLASS_BARD, 20);
  spell_level(SPELL_BARD_RECALL, CLASS_BARD, 10);
  spell_level(SPELL_BARD_DEBUFF, CLASS_BARD, 30);
  spell_level(SPELL_BARD_REGEN, CLASS_BARD, 20);
  spell_level(SPELL_BARD_SANC, CLASS_BARD, 30);
  spell_level(SPELL_BARD_FURY, CLASS_BARD, 20);
  spell_level(SPELL_BARD_SONIC, CLASS_BARD, 20);
  spell_level(SPELL_BARD_AGILITY, CLASS_BARD, 5);
  spell_level(SPELL_BARD_KNOWLEDGE, CLASS_BARD, 5);
  spell_level(SPELL_BARD_VITALITY, CLASS_BARD, 5);
  spell_level(SKILL_SKALD_SHRIEK, CLASS_BARD, 30);
}
void init_spell_levels(void)
{
  init_wizard();
  init_priest();
  init_rogue();
  init_fighter();
  init_knight();
  init_bard();
}

/* This is the exp given to implementors -- it must always be greater than the
 * exp required for immortality, plus at least 20,000 or so. */
#define EXP_MAX  2000000000000
#define BASE_EXP 1000
/* Function to return the exp required for each class/level */
int level_exp(int chclass, int level)
{
  if (level > LVL_IMPL || level < 0) {
    log("SYSERR: Requesting exp for invalid level %d!", level);
    return 0;
  }

  /* Gods have exp close to EXP_MAX.  This statement should never have to
   * changed, regardless of how many mortal or immortal levels exist. */
   if (level > LVL_IMMORT) {
     return EXP_MAX - ((LVL_IMPL - level) * 1000);
   }

  /* Exp required for normal mortals is below */

  if (chclass >= 0)
  {
		switch(level)
		{
			case 0: return 0;
			case 1: return 1;
			case 2: return 1000;
			case 3: return 3750;
			case 4: return 10000;
			case 5: return 18750;
			case 6: return 30000;
			case 7: return 55000;
			case 8: return 80000;
			case 9: return 160000; 
			case 10: return 200000;
			case 11: return 315000;
			case 12: return 400000;
			case 13: return 550000;
			case 14: return 650000;
			case 15: return 825000;
			case 16: return 1000000;
			case 17: return 1350000;
			case 18: return 1650000;
			case 19: return 2000000;
			case 20: return 2500000;
			case 21: return 3750000;
			case 22: return 5250000;
			case 23: return 7000000;
			case 24: return 9000000;
			case 25: return 11250000;
			case 26: return 13750000;
			case 27: return 16500000;
			case 28: return 20000000;
			case 29: return 24500000;
			case 30: return 30000000;
			case 31: return 34000000;
			case 32: return 38250000;
			case 33: return 42750000;
			case 34: return 47500000;
			case 35: return 52500000;
			case 36: return 60500000;
			case 37: return 69000000;
			case 38: return 78000000;
			case 39: return 87500000;
			case 40: return 97500000;
			case 41: return 108000000;
			case 42: return 119000000;
			case 43: return 130000000;
			case 44: return 142500000;
			case 45: return 155000000;
			case 46: return 168000000;
			case 47: return 187000000;
			case 48: return 207000000;
			case 49: return 228000000;
			case 50: return 250000000;
			case 51: return 300000000;
			case 52: return 350000000;
			case 53: return 400000000;
			case 54: return 450000000;
			case 55: return 500000000;
			case 56: return 550000000;
			case 57: return 600000000;
			case 58: return 700000000;
			case 59: return 800000000;
			case 60: return 900000000;
		}
  }
	if (level == LVL_IMMORT)
		return (2000000000);

  /* This statement should never be reached if the exp tables in this function
   * are set up properly.  If you see exp of 123456 then the tables above are
   * incomplete. */
  log("SYSERR: XP tables not set up correctly in class.c!");
  return 123456;
}

/* Default titles of male characters. */
const char *title_male(int chclass, int level)
{
  if (level <= 0 || level > LVL_IMPL)
    return "the Man";
  if (level == LVL_IMPL)
    return "the Implementor";

  switch (chclass) {

    case CLASS_WIZARD:
    switch (level) {
      case LVL_IMMORT: return "the Immortal Wizard";
      case LVL_GOD: return "the Avatar of Magic";
      case LVL_GRGOD: return "the God of Magic";
      default: return "the Wizard";
    }

    case CLASS_PRIEST:
    switch (level) {

      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Cardinal";
      case LVL_GOD: return "the Inquisitor";
      case LVL_GRGOD: return "the God of Good and Evil";
      default: return "the Priest";
    }

    case CLASS_ROGUE:
    switch (level) {
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Assassin";
      case LVL_GOD: return "the Demi God of Thieves";
      case LVL_GRGOD: return "the God of Thieves and Tradesmen";
      default: return "the Rogue";
    }

    case CLASS_FIGHTER:
    switch(level) {

      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Warlord";
      case LVL_GOD: return "the Extirpator";
      case LVL_GRGOD: return "the God of War";
      default: return "the Fighter";
    }
    case CLASS_KNIGHT:
	switch(level) {
          /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Knight";
      case LVL_GOD: return "the Lord of Thanes";
      case LVL_GRGOD: return "the God of Arms and Armor";
      default: return "The Knight";
    }
    case CLASS_BARD:
	switch(level) {
          /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Bard";
      case LVL_GOD: return "the Demi God of Music";
      case LVL_GRGOD: return "the God of Skaldic Poetry";
      default: return "the Skald";
    }
  }

  /* Default title for classes which do not have titles defined */
  return "the Classless";
}

/* Default titles of female characters. */
const char *title_female(int chclass, int level)
{
  if (level <= 0 || level > LVL_IMPL)
    return "the Woman";
  if (level == LVL_IMPL)
    return "the Implementress";

  switch (chclass) {

    case CLASS_WIZARD:
    switch (level) {
      case  1: return "the Apprentice of Magic";
      case  2: return "the Spell Student";
      case  3: return "the Scholar of Magic";
      case  4: return "the Delveress in Spells";
      case  5: return "the Medium of Magic";
      case  6: return "the Scribess of Magic";
      case  7: return "the Seeress";
      case  8: return "the Sage";
      case  9: return "the Illusionist";
      case 10: return "the Abjuress";
      case 11: return "the Invoker";
      case 12: return "the Enchantress";
      case 13: return "the Conjuress";
      case 14: return "the Witch";
      case 15: return "the Creator";
      case 16: return "the Savant";
      case 17: return "the Craftess";
      case 18: return "the Wizard";
      case 19: return "the War Witch";
      case 20: return "the Sorceress";
      case 21: return "the Necromancress";
      case 22: return "the Thaumaturgess";
      case 23: return "the Student of the Occult";
      case 24: return "the Disciple of the Uncanny";
      case 25: return "the Minor Elementress";
      case 26: return "the Greater Elementress";
      case 27: return "the Crafter of Magics";
      case 28: return "Shaman";
      case 29: return "the Keeper of Talismans";
      case 30: return "Archwitch";
      case LVL_IMMORT: return "the Immortal Enchantress";
      case LVL_GOD: return "the Empress of Magic";
      case LVL_GRGOD: return "the Goddess of Magic";
      default: return "the Witch";
    }

    case CLASS_PRIEST:
    switch (level) {
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Priestess";
      case LVL_GOD: return "the Inquisitress";
      case LVL_GRGOD: return "the Goddess of Good and Evil";
      default: return "the Priest";
    }

    case CLASS_ROGUE:
    switch (level) {
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Assassin";
      case LVL_GOD: return "the Demi Goddess of Thieves";
      case LVL_GRGOD: return "the Goddess of Thieves and Tradesmen";
      default: return "the Rogue";
    }

    case CLASS_FIGHTER:
    switch(level) {
      /* no one ever thought up these titles 21-30 */
      case LVL_IMMORT: return "the Immortal Lady of War";
      case LVL_GOD: return "the Queen of Destruction";
      case LVL_GRGOD: return "the Goddess of War";
      default: return "the Fighter";
    }
    case CLASS_KNIGHT:
	switch(level) {
      default: return "The Knight";
    }
    case CLASS_BARD:
        switch(level) {
      default: return "the Norn";
    }
  }

  /* Default title for classes which do not have titles defined */
  return "the Classless";
}

