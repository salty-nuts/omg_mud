/**************************************************************************
File: salty.c

Updated: 08 MAR 2020
Updated: 25 JUN 2021
Updated: 21 JUL 2021
Updated: 22 JUL 2021

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
#define HITP_REGEN_EXP(ch) (GET_HITP_REGEN(ch) * 1000000)
#define MANA_REGEN_EXP(ch) (GET_MANA_REGEN(ch) * 1000000)
#define MOVE_REGEN_EXP(ch) (GET_MOVE_REGEN(ch) * 1000000)
#define AFF_EXP(ch) (GET_SPELLS_AFFECTS(ch) * 1000000)
#define DAM_EXP(ch) (GET_SPELLS_DAMAGE(ch) * 1000000)
#define HEAL_EXP(ch) (GET_SPELLS_HEALING(ch) * 1000000)
#define TRAIN_EXP (100000)
#define RANK_ONE (long)500000000

#define STRADD_EXP(ch) ((1 + GET_REAL_ADD(ch)) * 1000000)
#define STR_EXP(ch) (GET_REAL_STR(ch) * 1000000)
#define INT_EXP(ch) (GET_REAL_INT(ch) * 1000000)
#define WIS_EXP(ch) (GET_REAL_WIS(ch) * 1000000)
#define DEX_EXP(ch) (GET_REAL_DEX(ch) * 1000000)
#define CON_EXP(ch) (GET_REAL_CON(ch) * 1000000)
#define LUCK_EXP(ch) (GET_REAL_LUCK(ch) * 1000000)
#define CPOW_EXP(ch) ((GET_COMBAT_POWER(ch)-99) * 1000000)
#define IMPROVE_COST 10000000

ACMD(do_testgroup)
{
  char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
  struct char_data *victim1;
  struct char_data *victim2;

  two_arguments(argument, buf, buf2);

  if (!*buf)
    send_to_char(ch,"Who do you wish to test the group of?\r\n");
  
  if (!(victim1 = get_char_vis(ch, buf, NULL, FIND_CHAR_WORLD))) 
    send_to_char(ch, "%s", CONFIG_NOPERSON);
  if (!(victim2 = get_char_vis(ch, buf2, NULL, FIND_CHAR_WORLD)))
    send_to_char(ch, "You need to specify a second person\r\n");  
  else
  {
    send_to_char(ch, "Victim1 = %s, Victim2 = %s\r\n", GET_NAME(victim1), GET_NAME(victim2));
    if (GROUP(victim1) && GROUP(victim2))
    {
      if (GROUP_LEADER(GROUP(victim1)) == GROUP_LEADER(GROUP(victim2)))
        send_to_char(ch, "victim1 group leader is %s, victim2 group leader is %s.\r\n", 
          GET_NAME(GROUP_LEADER(GROUP(victim1))), GET_NAME(GROUP_LEADER(GROUP(victim2))));
      else
        send_to_char(ch, "victim1 and victim2 are not in the same group.\r\n");
    }
    else
      send_to_char(ch, "victim1 or victim2 are not in a group.\r\n");
    if (!GROUP(victim1) || !GROUP(victim2))
    {
      if (!GROUP(victim1))
        send_to_char(ch, "Victim1 is not in a group.\r\n");
      if (!GROUP(victim2))
        send_to_char(ch, "Victim2 is not in a group.\r\n");      
    }      
  }
}

const char *rank_name(struct char_data *ch)
{
  if (IS_NPC(ch))
    return "";
  if (GET_RANK(ch) >= 10)
    return NAME_RANK_10(ch);
  else if (GET_RANK(ch) == 9)
    return NAME_RANK_9(ch);
  else if (GET_RANK(ch) == 8)
    return NAME_RANK_8(ch);
  else if (GET_RANK(ch) == 7)
    return NAME_RANK_7(ch);
  else if (GET_RANK(ch) == 6)
    return NAME_RANK_6(ch);
  else if (GET_RANK(ch) == 5)
    return NAME_RANK_5(ch);
  else if (GET_RANK(ch) == 4)
    return NAME_RANK_4(ch);
  else if (GET_RANK(ch) == 3)
    return NAME_RANK_3(ch);
  else if (GET_RANK(ch) == 2)
    return NAME_RANK_2(ch);
  else if (GET_RANK(ch) == 1)
    return NAME_RANK_1(ch);
  else
    return "";
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
/*
  Metagame Menu

  Salty
  06 JAN 2019
*/
void list_meta(struct char_data *ch)
{
  long hitp_exp, hitp_gold, mana_exp, mana_gold, move_exp, move_gold;
  mana_gold = hitp_gold = move_gold = 0;

  long s_meta_exp = 0;
  long s_meta_gold = 0;

  switch (GET_CLASS(ch))
  {
  case CLASS_WIZARD:
    hitp_exp = (long)GET_MAX_HIT(ch) * 1250;
    mana_exp = (long)GET_MAX_MANA(ch) * 500;
    break;
  case CLASS_PRIEST:
    hitp_exp = (long)GET_MAX_HIT(ch) * 1250;
    mana_exp = (long)GET_MAX_MANA(ch) * 625;
    break;
  case CLASS_ROGUE:
    hitp_exp = (long)GET_MAX_HIT(ch) * 1000;
    mana_exp = (long)GET_MAX_MANA(ch) * 1000;
    break;
  case CLASS_FIGHTER:
    hitp_exp = (long)GET_MAX_HIT(ch) * 875;
    mana_exp = (long)GET_MAX_MANA(ch) * 1000;
    break;
  case CLASS_KNIGHT:
    hitp_exp = (long)GET_MAX_HIT(ch) * 500;
    mana_exp = (long)GET_MAX_MANA(ch) * 1000;
    break;
  case CLASS_BARD:
    hitp_exp = (long)GET_MAX_HIT(ch) * 1250;
    mana_exp = (long)GET_MAX_MANA(ch) * 750;
    break;
  default:
    hitp_exp = (long)GET_MAX_HIT(ch) * 1000;
    mana_exp = (long)GET_MAX_MANA(ch) * 1000;
    break;
  }
  move_exp = (long)GET_MAX_MOVE(ch) * 100;
  move_gold = 10000;
  hitp_gold = hitp_exp / 50;
  mana_gold = mana_exp / 50;

  s_meta_exp = (hitp_exp + mana_exp) * 5;
  s_meta_gold = (hitp_gold + mana_gold) * 5;

  send_to_char(ch, "\n\r");
  send_to_char(ch, "You have %s experience points.\n\r", add_commas(GET_EXP(ch)));
  send_to_char(ch, "You can metagame the following:\r\n\n\r");
  send_to_char(ch, "[1] Increase hitpoints:   %12s exp, %12s gold.\n\r", add_commas(hitp_exp), add_commas(hitp_gold));
  send_to_char(ch, "[2] Increase mana:        %12s exp, %12s gold.\n\r", add_commas(mana_exp), add_commas(mana_gold));
  send_to_char(ch, "[3] Increase movement:    %12s exp, %12s gold.\n\r", add_commas(move_exp), add_commas(move_gold));
  send_to_char(ch, "\n\r\n");  
  send_to_char(ch, "[4] Super Meta:           %12s exp, %12s gold.\n\r", add_commas(s_meta_exp), add_commas(s_meta_gold));
  send_to_char(ch, "\n\r    Syntax: metagame <number>\n\r");
  send_to_char(ch, "\n\r    You may also 'train' to increase stats.\n\r");

}
ACMD(do_metagame)
{
  long hitp_exp, hitp_gold, mana_exp, mana_gold, move_exp, move_gold;
  int hroll = con_app[GET_CON(ch)].meta_hp;
  int mroll = int_app[GET_INT(ch)].meta_mana;

  int mvroll = 1000;
  int arg = 0;

  long s_meta_exp = 0;
  long s_meta_gold = 0;  
  int s_hroll = 0;
  int s_mroll = 0;

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
    s_hroll = con_app[GET_CON(ch)].meta_hp;
    s_mroll = int_app[GET_INT(ch)].meta_mana;
    hitp_exp = (long)GET_MAX_HIT(ch) * 1250;
    mana_exp = (long)GET_MAX_MANA(ch) * 500;
    s_meta_exp = (hitp_exp + mana_exp) * 5;
    break;
  case CLASS_PRIEST:
    mroll = dice(1, int_app[GET_WIS(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    s_hroll = con_app[GET_CON(ch)].meta_hp;
    s_mroll = int_app[GET_WIS(ch)].meta_mana;
    hitp_exp = (long)GET_MAX_HIT(ch) * 1250;
    mana_exp = (long)GET_MAX_MANA(ch) * 625;
    s_meta_exp = (hitp_exp + mana_exp) * 5;
    break;
  case CLASS_ROGUE:
    mroll = dice(1, int_app[GET_INT(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    s_hroll = con_app[GET_CON(ch)].meta_hp;
    s_mroll = int_app[GET_INT(ch)].meta_mana;
    hitp_exp = (long)GET_MAX_HIT(ch) * 1000;
    mana_exp = (long)GET_MAX_MANA(ch) * 1000;
    s_meta_exp = (hitp_exp + mana_exp) * 5;
    break;
  case CLASS_FIGHTER:
    mroll = dice(1, int_app[GET_INT(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    s_hroll = con_app[GET_CON(ch)].meta_hp;
    s_mroll = int_app[GET_INT(ch)].meta_mana;
    hitp_exp = (long)GET_MAX_HIT(ch) * 875;
    mana_exp = (long)GET_MAX_MANA(ch) * 1000;
    s_meta_exp = (hitp_exp + mana_exp) * 5;
    break;
  case CLASS_KNIGHT:
    mroll = dice(1, int_app[GET_WIS(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    s_hroll = con_app[GET_CON(ch)].meta_hp;
    s_mroll = int_app[GET_WIS(ch)].meta_mana;
    hitp_exp = (long)GET_MAX_HIT(ch) * 500;
    mana_exp = (long)GET_MAX_MANA(ch) * 1000;
    s_meta_exp = (hitp_exp + mana_exp) * 5;
    break;
  case CLASS_BARD:
    mroll = dice(1, int_app[GET_WIS(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    s_hroll = con_app[GET_CON(ch)].meta_hp;
    s_mroll = int_app[GET_WIS(ch)].meta_mana;
    hitp_exp = (long)GET_MAX_HIT(ch) * 1250;
    mana_exp = (long)GET_MAX_MANA(ch) * 750;
    s_meta_exp = (hitp_exp + mana_exp) * 5;
    break;
  default:
    mroll = dice(1, int_app[GET_INT(ch)].meta_mana);
    hroll = dice(1, con_app[GET_CON(ch)].meta_hp);
    s_hroll = con_app[GET_CON(ch)].meta_hp;
    s_mroll = int_app[GET_INT(ch)].meta_mana;
    hitp_exp = (long)GET_MAX_HIT(ch) * 1000;
    mana_exp = (long)GET_MAX_MANA(ch) * 1000;
    s_meta_exp = (hitp_exp + mana_exp) * 5;
    break;
  }

  mana_gold = mana_exp / 50;
  hitp_gold = hitp_exp / 50;
  s_meta_gold = (mana_gold + hitp_gold) * 5;
  move_exp = (long)GET_MAX_MOVE(ch) * 100;
  move_gold = 10000;

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
      if (GET_EXP(ch) < move_exp)
      {
        send_to_char(ch, "You need %s more experience points to metagame your Move.\n\r", add_commas(move_exp - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You manage to metagame and increase your Move by %d!\n\r", mvroll);
        GET_MOVE(ch) += mvroll;
        GET_MAX_MOVE(ch) += mvroll;
        rank_exp(ch, move_exp);
        return;
      }
    }
    case 4:
    {
      if (GET_EXP(ch) < s_meta_exp)
      {
        send_to_char(ch, "You need %s more experience points in order to Super Meta!\n\r", add_commas(s_meta_exp - GET_EXP(ch)));
        return;
      }
      if (GET_GOLD(ch) < s_meta_gold)
      {
        send_to_char(ch, "You need %s more gold coins in order to Super Meta!\n\r", add_commas(s_meta_gold - GET_GOLD(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You manage to metagame and increase your Hitpoints by %d and Mana by %d!\n\r", s_hroll, s_mroll);
        GET_HIT(ch) += s_hroll;
        GET_MAX_HIT(ch) += s_hroll;
        GET_MANA(ch) += s_mroll;
        GET_MAX_MANA(ch) += s_mroll;
        GET_GOLD(ch) -= s_meta_gold;
        rank_exp(ch, s_meta_exp);
        return;
      }
    }
  }
  return;
}
void list_train(struct char_data *ch)
{
  if (GET_LEVEL(ch) < 100)
  {
    send_to_char(ch, "You must be level 100 in order to train\r\n");
    return;
  }
  send_to_char(ch, "\n\r");
  send_to_char(ch, "You have %s experience points.\n\r", add_commas(GET_EXP(ch)));
  send_to_char(ch, "You can train the following:\r\n\n\r");
  if ((GET_REAL_STR(ch) < 25) || (GET_REAL_INT(ch) < 25) || (GET_REAL_WIS(ch) < 25) ||
      (GET_REAL_DEX(ch) < 25) || (GET_REAL_CON(ch) < 25) || (GET_REAL_ADD(ch) < 100) ||
      (GET_REAL_LUCK(ch) < 25))
  {
    send_to_char(ch, "You can train the following abilities to a max of 25.\n\r\n\r");

    if (GET_REAL_STR(ch) < 25)
      send_to_char(ch, "[A] Permanenly increase STR by 1:     %12s exp.\n\r", add_commas(STR_EXP(ch)));
    if (GET_REAL_INT(ch) < 25)
      send_to_char(ch, "[B] Permanenly increase INT by 1:     %12s exp.\n\r", add_commas(INT_EXP(ch)));
    if (GET_REAL_WIS(ch) < 25)
      send_to_char(ch, "[C] Permanenly increase WIS by 1:     %12s exp.\n\r", add_commas(WIS_EXP(ch)));
    if (GET_REAL_DEX(ch) < 25)
      send_to_char(ch, "[D] Permanenly increase DEX by 1:     %12s exp.\n\r", add_commas(DEX_EXP(ch)));
    if (GET_REAL_CON(ch) < 25)
      send_to_char(ch, "[E] Permanenly increase CON by 1:     %12s exp.\n\r", add_commas(CON_EXP(ch)));
    if (GET_REAL_LUCK(ch) < 25)
      send_to_char(ch, "[F] Permanenly increase LUCK by 1:    %12s exp.\n\r", add_commas(CON_EXP(ch)));

    if (GET_REAL_ADD(ch) < 100 && GET_REAL_STR(ch) >= 25)
      send_to_char(ch, "[G] Permanenly increase STR%% by 1:    %12s exp.\n\r", add_commas(STRADD_EXP(ch)));
  }
  if (GET_COMBAT_POWER(ch) < 200)
  {
    send_to_char(ch, "\n\r");
    send_to_char(ch, "[H] Increase Combat Power by 1:       %12s exp.\n\r", add_commas(CPOW_EXP(ch)));
  }
  if (GET_SPELLS_AFFECTS(ch) < 100)
    send_to_char(ch, "[I] Increase Affect Spells by 1:      %12s exp.\n\r", add_commas(AFF_EXP(ch)));    
  if (GET_SPELLS_DAMAGE(ch) < 100)
    send_to_char(ch, "[J] Increase Damage Spells by 1:      %12s exp.\n\r", add_commas(DAM_EXP(ch)));    
  if (GET_SPELLS_HEALING(ch) < 100)
    send_to_char(ch, "[K] Increase Healing Spells by 1:     %12s exp.\n\r", add_commas(HEAL_EXP(ch)));    
  if (GET_HITP_REGEN(ch) < 100)
    send_to_char(ch, "[L] Increase Hitpoint Regen by 1:     %12s exp.\n\r", add_commas(HITP_REGEN_EXP(ch)));    

  if (GET_MANA_REGEN(ch) < 100)
    send_to_char(ch, "[M] Increase Mana Regen by 1:         %12s exp.\n\r", add_commas(MANA_REGEN_EXP(ch)));    
  if (GET_MOVE_REGEN(ch) < 100)
    send_to_char(ch, "[N] Increase Move Regen by 1:         %12s exp.\n\r", add_commas(MOVE_REGEN_EXP(ch)));    

  send_to_char(ch, "\n\r    Syntax: train <number>\n\r");
  send_to_char(ch, "\n\r    You may also 'meta' to increase your hitpoints, mana and movement.\n\r");


}
ACMD(do_train)
{
  char arg[MAX_INPUT_LENGTH];
//  int arg = 0;
  int inc = 1;
  one_argument(argument, arg);

  if (GET_LEVEL(ch) < 100)
  {
    send_to_char(ch, "You must be level 100 in order to train\r\n");
    return;
  }
  if (!*argument)
  {
    list_train(ch);
    rank_exp(ch, 0);
    return;
  }
//  arg = atoi(argument);

  if (!*argument)
  {
    list_train(ch);
    rank_exp(ch, 0);
    return;
  }
  switch (*arg)
  {
    case 'A':
    case 'a':
    {
      if (GET_REAL_STR(ch) >= 25)
      {
        send_to_char(ch, "You have completed the +STR training.\n\r");
        return;
      }
      if (GET_EXP(ch) < STR_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your +STR.\n\r", add_commas(STR_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase your +STR by %d!\n\r", inc);
        GET_REAL_STR(ch) += inc;
        rank_exp(ch, STR_EXP(ch));
        return;
      }
    }
    case 'B':
    case 'b':
    {
      if (GET_REAL_INT(ch) >= 25)
      {
        send_to_char(ch, "You have completed the +INT training.\n\r");
        return;
      }
      if (GET_EXP(ch) < INT_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your +INT.\n\r", add_commas(INT_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase your +INT by %d!\n\r", inc);
        GET_REAL_INT(ch) += inc;
        rank_exp(ch, INT_EXP(ch));
        return;
      }
    }
    case 'C':
    case 'c':
    {
      if (GET_REAL_WIS(ch) >= 25)
      {
        send_to_char(ch, "You have completed the +WIS training.\n\r");
        return;
      }
      if (GET_EXP(ch) < WIS_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your +WIS.\n\r", add_commas(WIS_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase your +WIS by %d!\n\r", inc);
        GET_REAL_WIS(ch) += inc;
        rank_exp(ch, WIS_EXP(ch));
        return;
      }
    }
    case 'D':
    case 'd':
    {
      if (GET_REAL_DEX(ch) >= 25)
      {
        send_to_char(ch, "You have completed the +DEX training.\n\r");
        return;
      }
      if (GET_EXP(ch) < DEX_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your +DEX.\n\r", add_commas(DEX_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase your +DEX by %d!\n\r", inc);
        GET_REAL_DEX(ch) += inc;
        rank_exp(ch, DEX_EXP(ch));
        return;
      }
    }
    case 'E':
    case 'e':
    {
      if (GET_REAL_CON(ch) >= 25)
      {
        send_to_char(ch, "You have completed the +CON training.\n\r");
        return;
      }
      if (GET_EXP(ch) < CON_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your +CON.\n\r", add_commas(CON_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase your +CON by %d!\n\r", inc);
        GET_REAL_CON(ch) += inc;
        rank_exp(ch, CON_EXP(ch));
        return;
      }
    }
    case 'F':
    case 'f':
    {
      if (GET_REAL_LUCK(ch) >= 25)
      {
        send_to_char(ch, "You have completed the +LUCK training.\n\r");
        return;
      }
      if (GET_EXP(ch) < LUCK_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your +LUCK.\n\r", add_commas(LUCK_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase your +LUCK by %d!\n\r", inc);
        GET_REAL_LUCK(ch) += inc;
        rank_exp(ch, LUCK_EXP(ch));
        return;
      }
    }
    case 'G':
    case 'g':
    {
      if (GET_REAL_ADD(ch) >= 100)
      {
        send_to_char(ch, "You have completed the STR_ADD training.\n\r");
        return;
      }
      if (GET_EXP(ch) < STRADD_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your STR_ADD.\n\r", add_commas(STRADD_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase your STR_ADD by %d!\n\r", inc);
        GET_REAL_ADD(ch) += inc;
        rank_exp(ch, STRADD_EXP(ch));
        return;
      }
    }
    case 'H':
    case 'h':
    {
      if (GET_COMBAT_POWER(ch) > 200)
      {
        send_to_char(ch, "You have completed Combat Power training.\n\r");
        return;
      }
      if (GET_EXP(ch) < CPOW_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your Combat Power.\n\r", add_commas(CPOW_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase your Combat Power by %d!\n\r", inc);
        GET_COMBAT_POWER(ch) += inc;
        rank_exp(ch, CPOW_EXP(ch));
        return;
      }      
    }
    case 'I':
    case 'i':
    {
      if (GET_SPELLS_AFFECTS(ch) > 100)
      {
        send_to_char(ch, "You have completed increasing the potency of your Affect Spells!\n\r");
        return;
      }
      if (GET_EXP(ch) < AFF_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your Affect Spells.\n\r", add_commas(AFF_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase the potency of your Affect Spells by %d!\n\r", inc);
        GET_SPELLS_AFFECTS(ch) += inc;
        rank_exp(ch, AFF_EXP(ch));
        return;
      }      
    }
    case 'J':
    case 'j':
    {
      if (GET_SPELLS_DAMAGE(ch) > 100)
      {
        send_to_char(ch, "You have completed increasing the potency of your Damage Spells!\n\r");
        return;
      }
      if (GET_EXP(ch) < DAM_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your Damage Spells.\n\r", add_commas(DAM_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase the potency of your Damage Spells by %d!\n\r", inc);
        GET_SPELLS_DAMAGE(ch) += inc;
        rank_exp(ch, DAM_EXP(ch));
        return;
      }      
    }
    case 'K':
    case 'k':
    {
      if (GET_SPELLS_HEALING(ch) > 100)
      {
        send_to_char(ch, "You have completed increasing the potency of your Healing Spells!\n\r");
        return;
      }
      if (GET_EXP(ch) < HEAL_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your Healing Spells.\n\r", add_commas(HEAL_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase the potency of your Healing Spells by %d!\n\r", inc);
        GET_SPELLS_HEALING(ch) += inc;
        rank_exp(ch, HEAL_EXP(ch));
        return;
      }      
    }
    case 'L':
    case 'l':
    {
      if (GET_HITP_REGEN(ch) > 100)
      {
        send_to_char(ch, "You have completed increasing the potency of your Hitpoint Regeneration!.\n\r");
        return;
      }
      if (GET_EXP(ch) < HITP_REGEN_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your Hitpoint Regeneration.\n\r", add_commas(HITP_REGEN_EXP(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase the potency of your Hitpoint Regeneration by %d!\n\r", inc);
        GET_HITP_REGEN(ch) += inc;
        rank_exp(ch, HITP_REGEN_EXP(ch));
        return;
      }      
    }
    case 'M':
    case 'm':
    {
      if (GET_MANA_REGEN(ch) > 100)
      {
        send_to_char(ch, "You have completed increasing the potency of your Mana Regeneration!.\n\r");
        return;
      }
      if (GET_EXP(ch) < MANA_REGEN_EXP(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your Mana Regeneration!.\n\r", add_commas(GET_MANA_REGEN(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase the potency of your Mana Regeneration by %d!\n\r", inc);
        GET_MANA_REGEN(ch) += inc;
        rank_exp(ch, GET_MANA_REGEN(ch));
        return;
      }      
    }
    case 'N':
    case 'n':
    {
      if (GET_MOVE_REGEN(ch) > 100)
      {
        send_to_char(ch, "You have completed increasing the potency of your Movement Regeneration!.\n\r");
        return;
      }
      if (GET_EXP(ch) < GET_MOVE_REGEN(ch))
      {
        send_to_char(ch, "You need %s more experience points to train your Movement Regeneration!.\n\r", add_commas(GET_MOVE_REGEN(ch) - GET_EXP(ch)));
        return;
      }
      else
      {
        send_to_char(ch, "You train and increase the potency of your Movement Regeneration by %d!\n\r", inc);
        GET_MOVE_REGEN(ch) += inc;
        rank_exp(ch, GET_MOVE_REGEN(ch));
        return;
      }      
    }
    default:
    {
      send_to_char(ch, "There is nothing like that to train!\n\r");
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
    if (GET_LEVEL(victim) > GET_LEVEL(ch))
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
  struct char_data *groupie;
  struct affected_type *af;
  char arg[MAX_INPUT_LENGTH];
  one_argument(argument, arg);

  if (IS_NPC(ch))
    return;

  if (!GET_SKILL(ch, SKILL_CHANT))
  {
    send_to_char(ch, "You don't have the ability to chant.\r\n");
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
    send_to_char(ch, "You begin chanting a harmonic chant!\n\r");
    if (GROUP(ch))
    {
      while ((groupie = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
      {
        for (af = groupie->affected; af; af = af->next)
        {
          if (af->spell == spell_type(groupie, SPELL_BARD_DISSONANCE))
            affect_remove(groupie, af);
        }
        REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_HARMONY);
        REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_DISSONANCE);
        if (IS_NPC(groupie))
          continue;
        if (!IS_NPC(groupie))
        {
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_DISSONANCE);
          act("You take up a harmonic chant.", FALSE, ch, NULL, groupie, TO_VICT);
        }
      }
      cast_spell(ch, groupie, NULL, SPELL_BARD_HARMONY);
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
    send_to_char(ch, "You begin chanting a dissonant chant!\n\r");
    if (GROUP(ch))
    {
      while ((groupie = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
      {
        for (af = groupie->affected; af; af = af->next)
        {
          if (af->spell == spell_type(groupie, SPELL_BARD_HARMONY))
            affect_remove(groupie, af);
        }
        REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_HARMONY);
        REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_DISSONANCE);
        if (IS_NPC(groupie))
          continue;
        if (ch != groupie)
        {
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_HARMONY);
          act("You take up a dissonant chant.", FALSE, ch, NULL, groupie, TO_VICT);
        }
      }
      cast_spell(ch, groupie, NULL, SPELL_BARD_DISSONANCE);
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
    if (GROUP(ch))
    {
      while ((groupie = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
      {
        for (af = groupie->affected; af; af = af->next)
        {
          if (af->spell == spell_type(groupie, SPELL_BARD_HARMONY))
            affect_remove(groupie, af);
          if (af->spell == spell_type(groupie, SPELL_BARD_DISSONANCE))
            affect_remove(groupie, af);
        }
        if (IS_NPC(groupie))
          continue;
        if (!IS_NPC(groupie))
        {
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_HARMONY);
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_DISSONANCE);
        }
      }
    }
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HARMONY);
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_DISSONANCE);
    send_to_char(ch, "You stop chanting.\n\r");
    act("$n is no longert chanting.", true, ch, 0, 0, TO_ROOM);
    save_char(ch);
  }
  else
  {
    send_to_char(ch, "Usage: chant <type>\n\r");
    send_to_char(ch, "Types: harmony / dissonance / none\r\n");
  }
}
ACMD(do_dance)
{
  struct char_data *groupie;
  struct affected_type *af;

  if (IS_NPC(ch))
    return;
  char arg[MAX_INPUT_LENGTH];
  one_argument(argument, arg);

  if (!*arg)
  {
    send_to_char(ch, "Usage: dance <type>\n\r");
    send_to_char(ch, "Types: war / slow / none\r\n");
    return;
  }
  /*   if (!str_cmp("war", arg))
    {
      send_to_char(ch, "You begin the dance of war!\n\r");
      if (GROUP(ch))
      {
        while ((groupie = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
        {
          for (af = groupie->affected; af; af = af->next)
          {
            if (af->spell == spell_type(groupie, SPELL_BARD_SLOW_DANCE))
              affect_remove(groupie, af);
          }
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_WAR_DANCE);
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_SLOW_DANCE);
          if (IS_NPC(groupie) || ch == groupie)
            continue;
          if (!IS_NPC(groupie) && ch != groupie)
          {
            REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_SLOW_DANCE);
            act("Your steps follow the war dance.", FALSE, ch, NULL, groupie, TO_VICT);
          }
        }
        cast_spell(ch, groupie, NULL, SPELL_BARD_WAR_DANCE);
      }
      else if (AFF_FLAGGED(ch, AFF_WAR_DANCE))
      {
        act("You are already dancing the war dance.", true, ch, 0, 0, TO_CHAR);
        return;
      }
      save_char(ch);
    } */
  if (!str_cmp("war", arg))
  {
    if (!GET_SKILL(ch, SKILL_WAR_DANCE))
    {
      send_to_char(ch, "You don't know how to do the war dance.");
      return;
    }
    if (!AFF_FLAGGED(ch, AFF_WAR_DANCE))
    {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SLOW_DANCE);
      SET_BIT_AR(AFF_FLAGS(ch), AFF_WAR_DANCE);
      send_to_char(ch, "You begin the dance of war!\n\r");
    }
    else if (AFF_FLAGGED(ch, AFF_WAR_DANCE))
    {
      act("You are already dancing the war dance.", true, ch, 0, 0, TO_CHAR);
      return;
    }
    save_char(ch);
  }
  /*   else if (!str_cmp("slow", arg))
    {
      send_to_char(ch, "You begin dancing the slow dance!\n\r");
      if (GROUP(ch))
      {
        while ((groupie = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
        {
          for (af = groupie->affected; af; af = af->next)
          {
            if (af->spell == spell_type(groupie, SPELL_BARD_WAR_DANCE))
              affect_remove(groupie, af);
          }
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_WAR_DANCE);
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_SLOW_DANCE);
          if (IS_NPC(groupie))
            continue;
          if (ch != groupie)
          {
            REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_WAR_DANCE);
            act("Your steps follow the slow dance.", FALSE, ch, NULL, groupie, TO_VICT);
          }
        }
        cast_spell(ch, groupie, NULL, SPELL_BARD_SLOW_DANCE);
      }
      else if (AFF_FLAGGED(ch, AFF_SLOW_DANCE))
      {
        act("You are already dancing the slow dance!", true, ch, 0, 0, TO_CHAR);
        return;
      }
      save_char(ch);
    } */
  else if (!str_cmp("slow", arg))
  {
    if (!GET_SKILL(ch, SKILL_SLOW_DANCE))
    {
      send_to_char(ch, "You don't know how to do the slow dance.");
      return;
    }

    if (!AFF_FLAGGED(ch, AFF_SLOW_DANCE))
    {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_WAR_DANCE);
      SET_BIT_AR(AFF_FLAGS(ch), AFF_SLOW_DANCE);
      send_to_char(ch, "You begin dancing the slow dance!\n\r");
    }
    else if (AFF_FLAGGED(ch, AFF_SLOW_DANCE))
    {
      act("You are already dancing the slow dance.", true, ch, 0, 0, TO_CHAR);
      return;
    }
    save_char(ch);
  }
  /*   else if (!str_cmp("none", arg))
    {
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_WAR_DANCE);
      REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SLOW_DANCE);
      send_to_char(ch, "You stop dancing.\n\r");
      act("$n is no longer dancing.", true, ch, 0, 0, TO_ROOM);
      save_char(ch);
    } */
  else if (!str_cmp("none", arg))
  {
    if (GROUP(ch))
    {
      while ((groupie = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
      {
        for (af = groupie->affected; af; af = af->next)
        {
          if (af->spell == spell_type(groupie, SPELL_BARD_SLOW_DANCE))
            affect_remove(groupie, af);
          if (af->spell == spell_type(groupie, SPELL_BARD_WAR_DANCE))
            affect_remove(groupie, af);
        }
        if (IS_NPC(groupie))
          continue;
        if (!IS_NPC(groupie))
        {
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_SLOW_DANCE);
          REMOVE_BIT_AR(AFF_FLAGS(groupie), AFF_WAR_DANCE);
        }
      }
    }
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SLOW_DANCE);
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_WAR_DANCE);
    send_to_char(ch, "You stop dancing.\n\r");
    act("$n is no longer dancing.", true, ch, 0, 0, TO_ROOM);
    save_char(ch);
  }
}

void circle_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101); /* 101% is a complete failure */
  int prob = GET_SKILL(ch, SKILL_CIRCLE);

  if (!FIGHTING(ch) || !FIGHTING(victim) || !prob)
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
  if (!CONFIG_PK_ALLOWED && !IS_NPC(victim) && !IS_NPC(ch))
  {
    send_to_char(ch, "Not on players!\r\n");
    return;
  }
  if (victim == ch)
  {
    send_to_char(ch, "How can you circle behind yourself?\r\n");
    return;
  }
  if (!GET_EQ(ch, WEAR_WIELD))
  {
    send_to_char(ch, "You need to wield a weapon to make it a success.\r\n");
    return;
  }
  if (
      (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT) &&
      (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_STAB - TYPE_HIT) &&
      (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_BITE - TYPE_HIT) &&
      (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_STING - TYPE_HIT))
  {
    send_to_char(ch, "Your weapon must pierce, stab, bite or sting in order to circle.\r\n");
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
    send_to_char(ch, "%s is too alert, you can't sneak up!\n\r", GET_NAME(victim));
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
  if (
      (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT) &&
      (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_STAB - TYPE_HIT) &&
      (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_BITE - TYPE_HIT) &&
      (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_STING - TYPE_HIT))
  {
    send_to_char(ch, "Your weapon must pierce, stab, bite or sting in order to backstab.\r\n");
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
    set_fighting(ch, victim);
    set_fighting(victim, ch);
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
  set_fighting(ch, victim);
  set_fighting(victim, ch);
  hit(ch, victim, SKILL_BACKSTAB);
  act("$n sneaks up behind $N and stabs $S in the back!", false, ch, 0, victim, TO_ROOM);
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

  if (IS_NPC(ch))
    return;

  if (GET_LEVEL(ch) < spell_info[sn].min_level[GET_CLASS(ch)])
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

  if (GET_CLASS(ch) == CLASS_ROGUE)
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
  int prob = GET_SKILL(ch, SKILL_HEADBUTT);

  if (!FIGHTING(ch) || !FIGHTING(victim) || !prob)
    return;

  if (percent > prob)
  {
    act("You try to headbutt $N but miss.  How embarassing!", false, ch, 0, victim, TO_CHAR);
    act("You evade a grab and $n misses $s headbutt.", false, ch, 0, victim, TO_VICT);
    act("$n tries to grab $N for a headbutt, but misses and hits the ground.", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_HEADBUTT, FALSE);
  }
  else
  {
    roll = rand_number(1, 20) + (GET_LUCK(ch) / 5);
    if (roll <= 10)
      hit(ch, victim, SKILL_HEADBUTT);
    else
    {
      hit(ch, victim, SKILL_HEADBUTT);
      if (roll > 15)
      {
        act("You noticed the dazed look on $N's face and grin.", false, ch, 0, victim, TO_CHAR);
        act("$n looks you square in the face and smiles...", false, ch, 0, victim, TO_VICT);
        act("$N looks dazed for a moment as $n studies $S face.", false, ch, 0, victim, TO_NOTVICT);

        act("You look at the spot between $N's eyes and headbutt it again!", false, ch, 0, victim, TO_CHAR);
        act("BAMM!!!! What was the license plate of that truck?!", false, ch, 0, victim, TO_VICT);
        act("$n takes $N and crushes $M with $s head,...Blood Everywhere!!", false, ch, 0, victim, TO_NOTVICT);
        hit(ch, victim, SKILL_HEADBUTT2);
      }
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
  int percent = rand_number(1, 101);
  int dam, chance, damdice;
  int tof = 0;
  bool vict_lag = FALSE;

  dam = chance = 0;
  damdice = dice(GET_DEX(ch), GET_LUCK(ch));
  int prob = GET_SKILL(ch, SKILL_TUMBLE);

  if (!FIGHTING(ch) || !FIGHTING(victim) || !prob)
    return;

  if (percent > prob)
  {
    damage(ch, victim, 0, SKILL_TUMBLE);
    check_improve(ch, SKILL_TUMBLE, FALSE);
  }
  else
  {
    if (GET_SKILL(ch, SKILL_TWIST_OF_FATE) >= rand_number(1, 101))
    {
      tof = (GET_SKILL(ch, SKILL_TWIST_OF_FATE) / 10);
      check_improve(victim, SKILL_TWIST_OF_FATE, TRUE);
    }
    else
      check_improve(victim, SKILL_TWIST_OF_FATE, FALSE);

    chance = rand_number(1, 100) + GET_LUCK(ch) + GET_DEX(ch) + tof;
    if (chance > 75 && FIGHTING(ch))
    {
      dam += dice(GET_DEX(ch), GET_LUCK(ch));
/*       GET_POS(victim) = POS_STUNNED;
 */    }
else if (chance > 85 && FIGHTING(ch))
{
  if (vict_lag)
    WAIT_STATE(victim, PULSE_VIOLENCE);
}
check_improve(ch, SKILL_TUMBLE, TRUE);
dam = damdice + GET_LEVEL(ch) + GET_SKILL(ch, SKILL_TUMBLE) + GET_DAMROLL(ch) + GET_COMBAT_POWER(ch);
damage(ch, victim, dam, SKILL_TUMBLE);
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
  int percent, player_counter = 0;
  int prob = GET_SKILL(ch, SKILL_TAUNT) + GET_LUCK(ch);
  char actbuf[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  if (!prob)
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "Behave yourself here please!\n\r");
    return;
  }

  if (IS_AFFECTED(ch, AFF_DEFENSE))
    prob += GET_SKILL(ch, SKILL_DEFENSIVE_STANCE);

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
      continue;

    if (!IS_NPC(tmp_ch))
    {
      player_counter -= 1;
      continue;
    }
    if (IS_NPC(ch) && IS_NPC(tmp_ch))
      continue;

    if (!IS_NPC(tmp_ch))
      continue;
    if (AFF_FLAGGED(tmp_ch, AFF_CHARM))
    {
      send_to_char(ch, "The charmie ignores your taunt!\r\n");
      continue;
    }

    percent = rand_number(1, 200); /* 101% is a complete failure */

    if (percent > prob)
    {
      send_to_char(ch, "Your taunt goes unnoticed.\r\n");
      act("$n's taunt fails to provoke you", FALSE, ch, 0, 0, TO_VICT);
      check_improve(ch, SKILL_TAUNT, FALSE);
      continue;
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
  int prob = GET_SKILL(ch, SKILL_ACROBATICS);
  bool proc = FALSE;

  if (IS_NPC(ch))
    return;

  if (!FIGHTING(ch) || !FIGHTING(victim) || !prob)
    return;

  if (lrn >= num)
  {
    switch (rand_number(1, 3))
    {
    case 1:
    case 2:
    case 3:
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
  int prob = GET_SKILL(ch, SKILL_DIRTY_TRICKS);
  bool proc = FALSE;

  if (IS_NPC(ch))
    return;
  if (!FIGHTING(ch) || !FIGHTING(victim) || !prob)
    return;

  if (lrn >= num)
  {
    switch (dice(1, 4))
    {
    case 1:
      impale_combat(ch, victim);
      break;
    case 2:
      rend_combat(ch, victim);
      break;
    case 3:
      mince_combat(ch, victim);
      break;
    case 4:
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
  int prob = GET_SKILL(ch, SKILL_UNARMED_COMBAT);
  bool proc = FALSE;

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

  if (IS_AFFECTED(ch, AFF_RAGE))
    lrn += GET_STR(ch);

  if (lrn >= num)
  {
    switch (dice(1, 14))
    {
    case 1:
      kick_combat(ch, victim);
      break;
    case 2:
      trip_combat(ch, victim);
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
      break;
    case 11:
      palm_strike_combat(ch, victim);
      break;
    case 12:
      elbow_combat(ch, victim);
      break;
    case 13:
      roundhouse_combat(ch, victim);
      break;
    case 14:
      knee_combat(ch, victim);
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
void knight_combat_update(struct char_data *ch, struct char_data *victim)
{
  int roll = rand_number(1, 10);

  if (IS_NPC(ch) || !FIGHTING(ch) || !FIGHTING(victim))
    return;

  switch (roll)
  {
  case 1:
    shield_slam_combat(ch, victim);
    break;
  case 2:
    weapon_punch_combat(ch, victim);
    break;
  default:
    break;
  }
}
void shield_slam_combat(struct char_data *ch, struct char_data *victim)
{
  /* 101% is a complete failure */
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_SHIELD_SLAM);

  int str_roll = dice(2 * GET_STR(ch) / 5, 2 * GET_STR(ch) / 10);
  int con_roll = dice(2 * GET_CON(ch) / 5, 2 * GET_CON(ch) / 10);
  int dam = str_roll * con_roll;

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

  if (percent > prob)
  {
    damage(ch, victim, 0, TYPE_SHIELD_SLAM);
    check_improve(ch, SKILL_SHIELD_SLAM, FALSE);
  }
  else
  {
    if (!MOB_FLAGGED(victim, MOB_NOBASH))
    {
      act("$n slams $s shield into $N forcing $M to $S knees.", FALSE, ch, NULL, victim, TO_NOTVICT);
      act("You slam $N so hard with your shield $E takes a knee in pain!", FALSE, ch, NULL, victim, TO_CHAR);
      act("$n slams $s shield into you, forcing you to your knees!", FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
      damage(ch, victim, dam, TYPE_SHIELD_SLAM);
      check_improve(ch, SKILL_SHIELD_SLAM, TRUE);
    }
  }
}
void weapon_punch_combat(struct char_data *ch, struct char_data *victim)
{
  struct obj_data *wielded;

  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_WEAPON_PUNCH);

  int str_roll = dice(2 * GET_STR(ch) / 5, 2 * GET_STR(ch) / 10);
  int luck_roll = dice(2 * GET_LUCK(ch) / 5, 2 * GET_LUCK(ch) / 10);
  int dam;

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

  wielded = GET_EQ(ch, WEAR_WIELD);
  if (wielded == NULL)
    return;

  if (percent > prob)
  {
    damage(ch, victim, 0, TYPE_BLUDGEON);
    check_improve(ch, SKILL_WEAPON_PUNCH, FALSE);
  }
  else
  {
    dam = dice(GET_OBJ_VAL(wielded, 1), GET_OBJ_VAL(wielded, 2));
    dam += str_roll * luck_roll;

    if (!MOB_FLAGGED(victim, MOB_NOBASH))
    {
      act("$n punches $s weapon into $N forcing $M to stagger back", FALSE, ch, NULL, victim, TO_NOTVICT);
      act("You punch $N so hard with your weapon $E staggers back!", FALSE, ch, NULL, victim, TO_CHAR);
      act("$n punches you with $s weapon, you stagger back in pain!", FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
      damage(ch, victim, dam, TYPE_BLUDGEON);
      check_improve(ch, SKILL_WEAPON_PUNCH, TRUE);
    }
  }
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
    skill_lev = 1 + (GET_LEVEL(ch) / 25);
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
    if (!MOB_FLAGGED(victim, MOB_NOBLIND) && mag_savingthrow(victim, ch, SAVING_SPELL, SKILL_KICK, 0))
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
  int level = GET_LEVEL(ch) + GET_SKILL(ch, SKILL_BASH) + GET_SKILL(ch, SKILL_SHIELD_MASTER);
  int damdice = GET_REAL_STR(ch);
  int numdice = (GET_LEVEL(ch) + GET_SKILL(ch, SKILL_BASH) + GET_SKILL(ch, SKILL_SHIELD_MASTER)) / 3;
  int spelldam = dice(numdice, damdice) + level;

  if (percent > prob)
  {
    damage(ch, victim, 0, SKILL_BASH);
    //    GET_POS(ch) = POS_STUNNED;
    check_improve(ch, SKILL_BASH, FALSE);
  }
  else
  {
    hit(ch, victim, SKILL_BASH);
    if (IN_ROOM(ch) == IN_ROOM(victim))
    {
      if (rand_number(1, 20) > 15)
        call_magic(ch, victim, NULL, SPELL_CONCUSSIVE_WAVE, spelldam, CAST_SPELL);
      /*       else
              GET_POS(victim) = POS_STUNNED; */
    }
    if (!mag_savingthrow(victim, ch, SAVING_PARA, SKILL_BASH, 0))
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

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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
    check_improve(ch, SKILL_IMPALE, TRUE);
    hit(ch, victim, SKILL_IMPALE);
    return;
  }
}

void rend_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_REND);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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
    act("You skewer $N on two blades and proceed to hack viciously at $S tattered body!", false, ch, 0, victim, TO_CHAR);
    act("$n cruelly rends you with an unstoppable onslaught of expert attacks.", false, ch, 0, victim, TO_VICT);
    act("$n skewers $N on two blades and proceeds to hack viciously at $S tattered body!", false, ch, 0, victim, TO_NOTVICT);
    check_improve(ch, SKILL_REND, TRUE);
    hit(ch, victim, SKILL_REND);
    return;
  }
}

void mince_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_MINCE);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

    return;
  }
}

void left_hook_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_L_HOOK);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

    return;
  }
}

void sucker_punch_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_SUCKER_PUNCH);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

    return;
  }
}
void uppercut_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_UPPERCUT);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

    return;
  }
}
void haymaker_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_HAYMAKER);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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
      call_magic(ch, victim, NULL, SPELL_UNARMED_DEBUFF1, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_HAYMAKER), CAST_SPELL);

    return;
  }
}
void clothesline_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_CLOTHESLINE);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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
      call_magic(ch, victim, NULL, SPELL_UNARMED_DEBUFF2, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_CLOTHESLINE), CAST_SPELL);

    return;
  }
}
void piledriver_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_PILEDRVIER);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

    /*     if (percent > 90)
          call_magic(ch, ch, NULL, SPELL_UNARMED_BONUS, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_PILEDRVIER), CAST_SPELL); */

    return;
  }
}
void palm_strike_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_PALM_STRIKE);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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
      call_magic(ch, ch, NULL, SPELL_GAIN_ADVANTAGE, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_PALM_STRIKE), CAST_SPELL);

    return;
  }
}
void chop_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_CHOP);

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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

  if (IS_NPC(ch) || !prob || !FIGHTING(ch) || !FIGHTING(victim))
    return;

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
int skill_mult(struct char_data *ch, int skillnum, int dam, int level)
{
  int output;
  switch (skillnum)
  {
  case SKILL_BACKSTAB:
    output = dam * (1 + (level / 4));
    break;
  case SKILL_CIRCLE:
    output = dam * (1 + (level / 6));
    break;
  case SKILL_ASSAULT:
    output = dam * (1 + (level / 4));
    break;
  case SKILL_BASH:
    output = dam * (1 + (level / 10));
    break;
  case SKILL_RAMPAGE:
    output = dam * (1 + (level / 10));
    break;
  case SKILL_IMPALE:
  case SKILL_MINCE:
  case SKILL_THRUST:
  case SKILL_REND:
    output = dam * (1 + (level / 10) + check_rogue_critical(ch));
    break;
  default:
    output = dam * 1;
    break;
  }
  return (output);
}

int whirlwind_mult(int level)
{
  return (1 + (level / 10));
}

int unarmed_combat_dam(struct char_data *ch, int dam, int skill)
{
  int damage_mult = 1;

  if (dam < 1)
    dam = 1;

  switch (skill)
  {
  case SKILL_CHOP:
  case SKILL_R_HOOK:
  case SKILL_L_HOOK:
    damage_mult = 7;
    break;
  case SKILL_ELBOW:
  case SKILL_KNEE:
  case SKILL_TRIP:
  case SKILL_KICK:
    damage_mult = 9;
    break;
  case SKILL_SUCKER_PUNCH:
  case SKILL_UPPERCUT:
  case SKILL_HAYMAKER:
  case SKILL_CLOTHESLINE:
    damage_mult = 11;
    break;
  case SKILL_PILEDRVIER:
  case SKILL_PALM_STRIKE:
  case SKILL_ROUNDHOUSE:
  case SKILL_SPIN_KICK:
    damage_mult = 13;
    break;
  case SKILL_HEADBUTT:
  case SKILL_HEADBUTT2:
    damage_mult = 15;
    break;
  default:
    break;
  }

  //  return ((strength_roll + damage_bonus) * damage_mult);
  return (dam * damage_mult);
}

ACMD(do_testcmd)
{
  char arg[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH];
  struct char_data *vict;

  half_chop(argument, arg, buf);

  if (!*arg)
  {
    send_to_char(ch, "test what on whom?\r\n");
    return;
  }
  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD)))
  {
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
  call_magic(ch, vict, NULL, SPELL_BARD_REGEN, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BARD_BLESS, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_BARD_RESISTS, 1000, CAST_SPELL);
  call_magic(ch, vict, NULL, SPELL_QUICKCAST, 1000, CAST_SPELL);
}

ACMD(do_ritual)
{
  char arg[MAX_INPUT_LENGTH];
  int cost = 1000;
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

  act("You begin the battle ritual!", false, ch, 0, NULL, TO_CHAR);
  act("$n begins an ancient skaldic ritual!", FALSE, ch, 0, 0, TO_ROOM);

  GET_MANA(ch) -= cost;

  /* NEW_EVENT() will add a new mud event to the event list of the character.
   * This function below adds a new event of "eRITUAL", to "ch", and passes "NULL" as
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
  prob = GET_SKILL(ch, SKILL_BARD_RITUAL);
  GET_MANA(ch) -= cost;

  if (GET_POS(ch) != POS_FIGHTING)
  {
    send_to_char(ch, "The battle is over, no need for the ritual!\n\r");
    act("$n stops $s ritual as combat ends.", false, ch, 0, NULL, TO_ROOM);
    return 0;
  }
  if (GET_MANA(ch) < cost)
  {
    send_to_char(ch, "You are exhausted from performing the ritual.\n\r");
    act("$n suddenly stops the ritual due to exhaustion.", false, ch, 0, NULL, TO_ROOM);
    return 0;
  }

  act("You continue performing the ancient skaldic battle ritual!", false, ch, 0, NULL, TO_CHAR);

  for (tch = world[IN_ROOM(ch)].people; tch != NULL; tch = tch->next_in_room)
  {
    if (/* ch != tch &&  */ !IS_NPC(tch))
    {
      call_magic(ch, tch, NULL, SPELL_RITUAL, GET_LEVEL(ch) + GET_SPELLS_AFFECTS(ch), CAST_SPELL);
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
  int percent;
  int prob = GET_SKILL(ch, SKILL_GARROTTE);

  if (IS_NPC(ch) || prob)
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
  prob = GET_SKILL(ch, SKILL_GARROTTE);

  if (percent > prob)
  {
    act("$N moves abruptly and you miss your chance to strangle $M!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n fails to strangle $N with $s garrotte.", TRUE, ch, 0, vict, TO_NOTVICT);
    damage(ch, vict, 0, TYPE_UNDEFINED);
  }
  else
  {
    damage(ch, vict, rand_number(1, GET_LEVEL(ch)), TYPE_UNDEFINED);
    act("Your steel wire bites deeply into $N's throat!", FALSE, ch, 0, vict, TO_CHAR);
    act("A shadow detaches itself from the darkness and wraps a wire around $N's throat!", TRUE, ch, 0, vict, TO_NOTVICT);
    act("Your garrotte cuts off $N's ability to speak!", FALSE, ch, 0, vict, TO_CHAR);
  }

  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}
ACMD(do_rampage)
{
  // dirt skill not learned automatic failure
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RAMPAGE))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  rampage_combat(ch);
  // wait period
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}
void rampage_combat(struct char_data *ch)
{
  struct char_data *victim, *next_victim;
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_RAMPAGE);

  if (IS_NPC(ch))
    return;

  // ch misses victim, using TYPE_UNDEFINED instead of SKILL_SPIN_KICK messages in <message> file.
  if (percent > prob)
  {
    act("Your storm of steel is pathetic.  Do better.", FALSE, ch, 0, NULL, TO_CHAR);
    act("$n tries to storm the room with sword and shield, but fails!", TRUE, ch, 0, NULL, TO_NOTVICT);
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    check_improve(ch, SKILL_RAMPAGE, FALSE);
    return;
  }
  else
  {
    act("You explode into a storm of sword and shield!", FALSE, ch, 0, NULL, TO_CHAR);
    act("$n becomes a whirlwind of sword and steel!", TRUE, ch, 0, NULL, TO_NOTVICT);
    for (victim = world[IN_ROOM(ch)].people; victim; victim = next_victim)
    {
      next_victim = victim->next_in_room;
      if (victim == ch)
        continue;
      if (!IS_NPC(victim) && GET_LEVEL(victim) >= LVL_IMMORT)
        continue;
      if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(victim))
        continue;
      if (!IS_NPC(ch) && IS_NPC(victim) && AFF_FLAGGED(victim, AFF_CHARM))
        continue;
      if (MOB_FLAGGED(victim, MOB_NOKILL))
        continue;
      hit(ch, victim, SKILL_RAMPAGE);
      check_improve(ch, SKILL_RAMPAGE, TRUE);
      WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    }
  }
}

ACMD(do_spin_kick)
{
  // dirt skill not learned automatic failure
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_SPIN_KICK))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  spinkick_combat(ch);
  // wait period
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}
void spinkick_combat(struct char_data *ch)
{
  struct char_data *victim, *next_victim;
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_SPIN_KICK);

  if (IS_NPC(ch))
    return;

  // ch misses victim, using TYPE_UNDEFINED instead of SKILL_SPIN_KICK messages in <message> file.
  if (percent > prob)
  {
    act("You attempt to execute a beautiful spin kick, but slip and fall!", FALSE, ch, 0, NULL, TO_CHAR);
    act("$n tries to do a spin kick but slips and falls to the ground!", TRUE, ch, 0, NULL, TO_NOTVICT);
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    check_improve(ch, SKILL_SPIN_KICK, FALSE);
    return;
  }
  else
  {
    act("You execute a beautiful spin kick!", FALSE, ch, 0, NULL, TO_CHAR);
    act("$n executes a beautiful spin kick!", TRUE, ch, 0, NULL, TO_NOTVICT);
    for (victim = world[IN_ROOM(ch)].people; victim; victim = next_victim)
    {
      next_victim = victim->next_in_room;
      if (victim == ch)
        continue;
      if (!IS_NPC(victim) && GET_LEVEL(victim) >= LVL_IMMORT)
        continue;
      if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(victim))
        continue;
      if (!IS_NPC(ch) && IS_NPC(victim) && AFF_FLAGGED(victim, AFF_CHARM))
        continue;
      if (MOB_FLAGGED(victim, MOB_NOKILL))
        continue;
      hit(ch, victim, SKILL_SPIN_KICK);
      check_improve(ch, SKILL_SPIN_KICK, TRUE);
      WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    }
  }
}

ACMD(do_dirt_kick)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;

  // dirt skill not learned automatic failure
  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_DIRT_KICK))
  {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }
  one_argument(argument, arg);

  if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
      victim = FIGHTING(ch);
    else
    {
      send_to_char(ch, "Dirt Kick who?\r\n");
      return;
    }
  }
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }
  if (MOB_FLAGGED(victim, MOB_NOKILL))
  {
    send_to_char(ch, "This mob is protected.\r\n");
    return;
  }
  // do nothing if trying to dirt kick self
  if (victim == ch)
  {
    send_to_char(ch, "Aren't we funny today...\r\n");
    return;
  }
  dirtkick_combat(ch, victim);
  // wait period
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

void dirtkick_combat(struct char_data *ch, struct char_data *victim)
{
  int percent = rand_number(1, 101);
  int prob = GET_SKILL(ch, SKILL_DIRT_KICK);
  struct affected_type af;

  if (IS_NPC(ch))
    return;

  // ch misses victim, using TYPE_UNDEFINED instead of SKILL_DIRT_KICK messages in <message> file.
  if (percent > prob)
  {
    act("You try to kick dirt in $N's eyes, but slip and fall!", FALSE, ch, 0, victim, TO_CHAR);
    act("$n tries to kick dirt in $N's eyes, but slips and falls to the ground!", TRUE, ch, 0, victim, TO_NOTVICT);
    damage(ch, victim, 0, TYPE_UNDEFINED);
  }
  else
  {
    // we have a chance to hit!
    // check to see if mob can be blinded or not
    if (MOB_FLAGGED(victim, MOB_NOBLIND))
    {
      act("You kick dirt in $N's face, but nothing happens.", FALSE, ch, 0, victim, TO_CHAR);
    }
    else
    {
      // if vict is not affected by blind then apply the debuffs
      if (!AFF_FLAGGED(victim, AFF_BLIND))
      {
        new_affect(&af);
        af.spell = SKILL_DIRT_KICK;
        af.location = APPLY_HITROLL;
        af.modifier = -10;
        af.duration = 1;
        SET_BIT_AR(af.bitvector, AFF_BLIND);
        affect_to_char(victim, &af);

        new_affect(&af);
        af.spell = SKILL_DIRT_KICK;
        af.location = APPLY_AC;
        af.modifier = 40;
        af.duration = 1;
        SET_BIT_AR(af.bitvector, AFF_BLIND);
        affect_to_char(victim, &af);

        damage(ch, victim, rand_number(1, GET_LEVEL(ch)), TYPE_UNDEFINED);
        act("You temporarily blind $N.", FALSE, ch, 0, victim, TO_CHAR);
        act("$N is temporarily blinded.", TRUE, ch, 0, victim, TO_NOTVICT);
        act("You kick dirt in $N's face!", FALSE, ch, 0, victim, TO_CHAR);
      }
      else
      {
        damage(ch, victim, rand_number(1, GET_LEVEL(ch)), TYPE_UNDEFINED);
        act("You kick dirt in $N's eyes!", FALSE, ch, 0, victim, TO_CHAR);
        act("$n kicks dirt in $N's eyes!", TRUE, ch, 0, victim, TO_NOTVICT);
      }
    }
  }
}

ACMD(do_gateway)
{

  struct char_data *victim;
  char arg[MAX_INPUT_LENGTH];
  one_argument(argument, arg);
  int target;

  // Rooms we cannot relocate from
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PRIVATE))
  {
    send_to_char(ch, "Something prevents you from leaving the room.\r\n");
    return;
  }
  if (!*arg)
  {
    send_to_char(ch, "Open a gateway to who?\r\n");
  }
  else
  {
    if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD)))
      send_to_char(ch, "You can't seem to find them.\r\n");
    else if (!IS_NPC(victim))
      send_to_char(ch, "Gateway cannot target players.\r\n");
    else if (ch == victim)
      send_to_char(ch, "Your mother would be so sad.. :(\r\n");
    else if (ROOM_FLAGGED(IN_ROOM(victim), ROOM_PRIVATE) || ROOM_FLAGGED(IN_ROOM(victim), ROOM_TUNNEL) ||
             ROOM_FLAGGED(IN_ROOM(victim), ROOM_GODROOM) || ROOM_FLAGGED(IN_ROOM(victim), ROOM_NOTRACK))
      send_to_char(ch, "Something prevents you from reaching %s!\r\n", GET_NAME(victim));
    else if (GET_LEVEL(ch) < GET_LEVEL(victim))
      send_to_char(ch, "You are not powerful enough.\r\n");
    else
    {
      act("$n vanises abruptly.", true, ch, 0, 0, TO_ROOM);
      target = victim->in_room;
      char_from_room(ch);
      char_to_room(ch, target);
      act("$n exits a gateway.", true, ch, 0, 0, TO_ROOM);
      act("You step into the spacetime continuum and emerge at $N!", false, ch, 0, victim, TO_CHAR);
      do_look(ch, "", 0, 0);
      return;
    }
    return;
  }
}

ACMD(do_rescue)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict, *tmp_ch;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RESCUE))
  {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
  {
    send_to_char(ch, "Whom do you want to rescue?\r\n");
    return;
  }
  if (vict == ch)
  {
    send_to_char(ch, "Rescue yourself?\r\n");
    return;
  }
  if (FIGHTING(ch) == vict)
  {
    send_to_char(ch, "How can you rescue someone you are trying to kill?\r\n");
    return;
  }
  for (tmp_ch = world[IN_ROOM(ch)].people; tmp_ch && (FIGHTING(tmp_ch) != vict); tmp_ch = tmp_ch->next_in_room)
    ;

  if ((FIGHTING(vict) != NULL) && (FIGHTING(ch) == FIGHTING(vict)) && (tmp_ch == NULL))
  {
    tmp_ch = FIGHTING(vict);
    if (FIGHTING(tmp_ch) == ch)
    {
      send_to_char(ch, "You have already rescued %s from %s.\r\n", GET_NAME(vict), GET_NAME(FIGHTING(ch)));
      return;
    }
  }

  if (!tmp_ch)
  {
    act("But nobody is fighting $M!", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }
  percent = rand_number(1, 101); /* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_RESCUE);

  if (percent > prob)
  {
    send_to_char(ch, "You fail the rescue!\r\n");
    check_improve(ch, SKILL_RESCUE, FALSE);
    return;
  }
  send_to_char(ch, "Banzai!  To the rescue...\r\n");
  act("You are rescued by $n, you are confused!", FALSE, ch, 0, vict, TO_VICT);
  act("$n heroically rescues $N!", FALSE, ch, 0, vict, TO_NOTVICT);
  check_improve(ch, SKILL_RESCUE, TRUE);
  if (FIGHTING(vict) == tmp_ch)
    stop_fighting(vict);
  if (FIGHTING(tmp_ch))
    stop_fighting(tmp_ch);
  if (FIGHTING(ch))
    stop_fighting(ch);

  set_fighting(ch, tmp_ch);
  set_fighting(tmp_ch, ch);

  WAIT_STATE(vict, 2 * PULSE_VIOLENCE);
}
ACMD(do_silent_rescue)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict, *tmp_ch;
  int percent, prob;

  if (IS_NPC(ch) || !GET_SKILL(ch, SKILL_RESCUE))
    return;

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
    return;

  if (vict == ch)
    return;

  if (FIGHTING(ch) == vict)
    return;

  for (tmp_ch = world[IN_ROOM(ch)].people; tmp_ch && (FIGHTING(tmp_ch) != vict);
       tmp_ch = tmp_ch->next_in_room)
    ;

  if ((FIGHTING(vict) != NULL) && (FIGHTING(ch) == FIGHTING(vict)) && (tmp_ch == NULL))
  {
    tmp_ch = FIGHTING(vict);
    if (FIGHTING(tmp_ch) == ch)
      return;
  }

  if (!tmp_ch)
    return;

  percent = rand_number(1, 101); /* 101% is a complete failure */
  prob = GET_SKILL(ch, SKILL_RESCUE);

  if (percent > prob)
  {
    send_to_char(ch, "You fail the rescue!\r\n");
    check_improve(ch, SKILL_RESCUE, FALSE);
    return;
  }
  send_to_char(ch, "Banzai!  To the rescue...\r\n");
  act("You are rescued by $n, you are confused!", FALSE, ch, 0, vict, TO_VICT);
  act("$n heroically rescues $N!", FALSE, ch, 0, vict, TO_NOTBRIEF);
  check_improve(ch, SKILL_RESCUE, TRUE);
  if (FIGHTING(vict) == tmp_ch)
    stop_fighting(vict);
  if (FIGHTING(tmp_ch))
    stop_fighting(tmp_ch);
  if (FIGHTING(ch))
    stop_fighting(ch);

  set_fighting(ch, tmp_ch);
  set_fighting(tmp_ch, ch);

  WAIT_STATE(vict, 2 * PULSE_VIOLENCE);
}
int check_rogue_critical(struct char_data *ch)
{
  int tof_check = FALSE;
  int crit_check = FALSE;
  int anat_check = FALSE;
  int mult = 0;
  char buf[MSL];

  if (GET_SKILL(ch, SKILL_CRITICAL_HIT) >= rand_number(1, 101))
  {
    check_improve(ch, SKILL_CRITICAL_HIT, TRUE);
    crit_check = TRUE;

    /* Anatomy Lessons come after critical */
    if (GET_SKILL(ch, SKILL_ANATOMY_LESSONS) &&
    (GET_SKILL(ch, SKILL_ANATOMY_LESSONS) + GET_LUCK(ch) >= rand_number(1, 200))
       )
    {
      check_improve(ch, SKILL_ANATOMY_LESSONS, TRUE);
      anat_check = TRUE;
    }
    else
      check_improve(ch, SKILL_ANATOMY_LESSONS, FALSE);

    /* Check for TOF success */
    if (GET_SKILL(ch, SKILL_TWIST_OF_FATE) && 
    (GET_SKILL(ch, SKILL_TWIST_OF_FATE) + GET_LUCK(ch) >= rand_number(1, 300) ) 
       )
    {
      check_improve(ch, SKILL_TWIST_OF_FATE, TRUE);
      tof_check = TRUE;
    }
    else
      check_improve(ch, SKILL_TWIST_OF_FATE, FALSE);

    /* TOF does the most damage */
    if (tof_check == TRUE)
    {
      mult = 4;
      sprintf(buf, "%s%s%s", CCCYN(ch, C_NRM), "Fate bends to your will! You exploit a vulnerability! ", CCNRM(ch, C_NRM));
      act(buf, FALSE, ch, 0, NULL, TO_CHAR);
      return (mult);
    }
    /* Anatomy Lessons does the middle damage */
    else if (anat_check == TRUE)
    {
      mult = 3;
      sprintf(buf, "%s%s%s", CCCYN(ch, C_NRM), "Your anatomy lessons pay off!  You struck a nerve!", CCNRM(ch, C_NRM));
      act(buf, FALSE, ch, 0, NULL, TO_CHAR);
      return (mult);
    }
    /* Critical Hit does the least damage*/
    else if (crit_check == TRUE)
    {
      mult = 2;
      sprintf(buf, "%s%s%s", CCCYN(ch, C_NRM), "Your training pays off as you strike a critical area!", CCNRM(ch, C_NRM));
      act(buf, FALSE, ch, 0, NULL, TO_CHAR);
      return (mult);
    }
  }
  else
  {
    check_improve(ch, SKILL_CRITICAL_HIT, FALSE);
    return (0);
  }
  return 0;
}

int check_block(struct char_data *ch, struct char_data *victim)
{
  int block_skill = GET_SKILL(ch, SKILL_SHIELD_BLOCK) + GET_SKILL(ch, SKILL_SHIELD_MASTER) +
                  GET_LUCK(ch) + GET_STR(ch) + GET_ADD(ch);
  int block_chance = rand_number(1, 400);
  if (IS_KNIGHT(ch) && GET_SKILL(ch, SKILL_SHIELD_BLOCK))
  {
    if (block_skill > block_chance)
    {
      act("You step in front of $N, blocking $S from fleeing!", false, ch, 0, victim, TO_CHAR);
      act("$n steps in front of $N and blocks $S from fleeing!", TRUE, ch, 0, victim, TO_ROOM);
      GET_MOVE(victim) -= 10;
      return (TRUE);
    }
    else
      return (FALSE);
  }
  return (FALSE);
}
void check_disembowel(struct char_data *ch, struct char_data *victim)
{
  if (!IS_NPC(victim))
    return;
  if (!IS_ROGUE(ch))
    return;

  int disem_skill = GET_SKILL(ch, SKILL_DISEMBOWEL) + GET_SKILL(ch, SKILL_TWIST_OF_FATE) + GET_LUCK(ch) + GET_DEX(ch);
  int disem_chance = rand_number(1,1000);
  long health = ((long)GET_HIT(victim) * (long)100) / (long)GET_MAX_HIT(victim);
  int dam = 0;

/*   
  if (victim->mob_specials.mob_mult > 0)
    disem_chance = rand_number(1,1000) * victim->mob_specials.mob_mult;
  else 
    disem_chance = rand_number(1,1000);
*/

  if (health > 20)
    return;
  else
  {
    if (disem_skill > disem_chance)
    {
      act("You jam your blade into $N's stomach and rip out $S entrails!", false, ch, 0, victim, TO_CHAR);
      act("$n rips out $N's entrails, blood and guts everywhere!", TRUE, ch, 0, victim, TO_ROOM);
      dam = GET_HIT(victim);
      damage(ch, victim, dam, SKILL_DISEMBOWEL);
      check_improve(ch, SKILL_DISEMBOWEL, TRUE);
      return;
    }
    else
    {
      send_to_char(ch, "You miss an opportunity to disembowel %s!\r\n", GET_NAME(victim));
      check_improve(ch, SKILL_DISEMBOWEL, FALSE);
      return;
    }
  }
}