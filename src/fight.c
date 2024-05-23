/**************************************************************************
 *  File: fight.c                                           Part of tbaMUD *
 *  Usage: Combat system.                                                  *
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
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "constants.h"
#include "dg_scripts.h"
#include "act.h"
#include "class.h"
#include "fight.h"
#include "shop.h"
#include "quest.h"
#include "mud_event.h"
#include "salty.h"

#define MORGUE 1099
/* locally defined global variables, used externally */
/* head of l-list of fighting chars */
struct char_data *combat_list = NULL;
/* Weapon attack texts */
struct attack_hit_type attack_hit_text[] =
    {
        {"hit", "hits"}, /* 0 */
        {"sting", "stings"},
        {"whip", "whips"},
        {"slash", "slashes"},
        {"bite", "bites"},
        {"bludgeon", "bludgeons"}, /* 5 */
        {"crush", "crushes"},
        {"pound", "pounds"},
        {"claw", "claws"},
        {"maul", "mauls"},
        {"thrash", "thrashes"}, /* 10 */
        {"pierce", "pierces"},
        {"blast", "blasts"},
        {"punch", "punches"},
        {"stab", "stabs"},
        {"shield slam", "shield slams"}};

/* local (file scope only) variables */
static struct char_data *next_combat_list = NULL;

/* local file scope utility functions */
static void perform_group_gain(struct char_data *ch, long base, struct char_data *victim);
static void dam_message(int dam, struct char_data *ch, struct char_data *victim, int w_type);
static void make_corpse(struct char_data *ch);
static void change_alignment(struct char_data *ch, struct char_data *victim);
static void group_gain(struct char_data *ch, struct char_data *victim);
static void solo_gain(struct char_data *ch, struct char_data *victim);
/** @todo refactor this function name */
static char *replace_string(const char *str, const char *weapon_singular, const char *weapon_plural);
static int compute_thaco(struct char_data *ch, struct char_data *vict);
void fight_sanity(struct char_data *ch, struct char_data *victim);

#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))
void fight_sanity(struct char_data *ch, struct char_data *victim)
{
  if (!FIGHTING(victim))
    set_fighting(victim, ch);
  if (!FIGHTING(ch))
    set_fighting(ch, victim);
}

/* The Fight related routines */
void appear(struct char_data *ch)
{
  if (affected_by_spell(ch, SPELL_INVISIBLE))
    affect_from_char(ch, SPELL_INVISIBLE);

  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_INVISIBLE);
  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);

  if (GET_LEVEL(ch) < LVL_IMMORT)
    act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
  else
    act("You feel a strange presence as $n appears, seemingly from nowhere.",
        FALSE, ch, 0, 0, TO_ROOM);
}

int compute_armor_class(struct char_data *ch)
{
  int armorclass = GET_AC(ch);
  ;

  if (AWAKE(ch))
    armorclass += dex_app[GET_DEX(ch)].defensive * 10;

  if (!IS_NPC(ch))
  {
    if (IS_AFFECTED(ch, AFF_DEFENSE))
      armorclass -= (GET_SKILL(ch, SKILL_DEFENSIVE_STANCE) + GET_LEVEL(ch));
  }

  return (MAX(-500, armorclass));
}

void update_pos(struct char_data *victim)
{
  if ((GET_HIT(victim) > 0) && (GET_POS(victim) > POS_STUNNED))
    return;
  else if (GET_HIT(victim) > 0)
    GET_POS(victim) = POS_STANDING;
  else if (GET_HIT(victim) <= -11)
    GET_POS(victim) = POS_DEAD;
  else if (GET_HIT(victim) <= -6)
    GET_POS(victim) = POS_MORTALLYW;
  else if (GET_HIT(victim) <= -3)
    GET_POS(victim) = POS_INCAP;
  else
    GET_POS(victim) = POS_STUNNED;
}

void check_killer(struct char_data *ch, struct char_data *vict)
{
  if (PLR_FLAGGED(vict, PLR_KILLER) || PLR_FLAGGED(vict, PLR_THIEF))
    return;
  if (PLR_FLAGGED(ch, PLR_KILLER) || IS_NPC(ch) || IS_NPC(vict) || ch == vict)
    return;

  SET_BIT_AR(PLR_FLAGS(ch), PLR_KILLER);
  send_to_char(ch, "If you want to be a PLAYER KILLER, so be it...\r\n");
  mudlog(BRF, MAX(LVL_IMMORT, MAX(GET_INVIS_LEV(ch), GET_INVIS_LEV(vict))),
         TRUE, "PC Killer bit set on %s for initiating attack on %s at %s.",
         GET_NAME(ch), GET_NAME(vict), world[IN_ROOM(vict)].name);
}

/* start one char fighting another (yes, it is horrible, I know... )  */
void set_fighting(struct char_data *ch, struct char_data *vict)
{
  if (ch == vict)
    return;

  if (FIGHTING(ch))
  {
    core_dump();
    return;
  }

  ch->next_fighting = combat_list;
  combat_list = ch;

  if (AFF_FLAGGED(ch, AFF_SLEEP))
    affect_from_char(ch, SPELL_SLEEP);

  FIGHTING(ch) = vict;
  GET_POS(ch) = POS_FIGHTING;

  if (!CONFIG_PK_ALLOWED)
    check_killer(ch, vict);
}

/* remove a char from the list of fighting chars */
void stop_fighting(struct char_data *ch)
{

  struct char_data *temp;

  if (ch == next_combat_list)
    next_combat_list = ch->next_fighting;

  REMOVE_FROM_LIST(ch, combat_list, next_fighting);
  ch->next_fighting = NULL;
  FIGHTING(ch) = NULL;
  GET_POS(ch) = POS_STANDING;
  update_pos(ch);
}

static void make_corpse(struct char_data *ch)
{
  char name[MAX_NAME_LENGTH + 64];
  char corp[MAX_NAME_LENGTH + 64];
  char buf2[MAX_NAME_LENGTH + 64];
  struct obj_data *corpse, *o;
  struct obj_data *money;
  int i, x, y;

  corpse = create_obj();

  corpse->item_number = NOTHING;
  IN_ROOM(corpse) = NOWHERE;
  corpse->name = strdup("corpse");
  if (!IS_NPC(ch))
  {
    snprintf(name, sizeof(name), "%s", GET_NAME(ch));
    snprintf(corp, sizeof(corp), " corpse");
    strcat(name, corp);
    corpse->name = strdup(name);
  }
  snprintf(buf2, sizeof(buf2), "The corpse of %s is lying here.", GET_NAME(ch));
  corpse->description = strdup(buf2);

  snprintf(buf2, sizeof(buf2), "the corpse of %s", GET_NAME(ch));
  corpse->short_description = strdup(buf2);

  GET_OBJ_TYPE(corpse) = ITEM_CONTAINER;
  for (x = y = 0; x < EF_ARRAY_MAX || y < TW_ARRAY_MAX; x++, y++)
  {
    if (x < EF_ARRAY_MAX)
      GET_OBJ_EXTRA_AR(corpse, x) = 0;
    if (y < TW_ARRAY_MAX)
      corpse->obj_flags.wear_flags[y] = 0;
  }
  SET_BIT_AR(GET_OBJ_WEAR(corpse), ITEM_WEAR_TAKE);
  SET_BIT_AR(GET_OBJ_EXTRA(corpse), ITEM_NODONATE);
  GET_OBJ_VAL(corpse, 0) = 0; /* You can't store stuff in a corpse */
  GET_OBJ_VAL(corpse, 3) = 1; /* corpse identifier */
  GET_OBJ_WEIGHT(corpse) = GET_WEIGHT(ch) + IS_CARRYING_W(ch);
  GET_OBJ_RENT(corpse) = 100000;
  if (IS_NPC(ch))
    GET_OBJ_TIMER(corpse) = CONFIG_MAX_NPC_CORPSE_TIME;
  else
    GET_OBJ_TIMER(corpse) = CONFIG_MAX_PC_CORPSE_TIME;

  GET_OBJ_LEVEL(corpse) = (GET_LEVEL(ch)) + dice(1, 10);


  /* transfer character's inventory to the corpse */
  corpse->contains = ch->carrying;
  for (o = corpse->contains; o != NULL; o = o->next_content)
    o->in_obj = corpse;
  object_list_new_owner(corpse, NULL);

  /* transfer character's equipment to the corpse */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i))
    {
      remove_otrigger(GET_EQ(ch, i), ch);
      obj_to_obj(unequip_char(ch, i), corpse);
    }

  /* transfer gold */
  if (GET_GOLD(ch) > 0)
  {
    /* following 'if' clause added to fix gold duplication loophole. The above
      * line apparently refers to the old "partially log in, kill the game
      * character, then finish login sequence" duping bug. The duplication has
      * been fixed (knock on wood) but the test below shall live on, for a
      * while. -gg 3/3/2002 */
    //    if (IS_NPC(ch) || ch->desc)
    if (IS_NPC(ch) || !IS_NPC(ch))
    {
      money = create_money(GET_GOLD(ch));
      obj_to_obj(money, corpse);
    }
    GET_GOLD(ch) = 0;
  }
  ch->carrying = NULL;
  IS_CARRYING_N(ch) = 0;
  IS_CARRYING_W(ch) = 0;

  if (IS_NPC(ch))
    obj_to_room(corpse, IN_ROOM(ch));
  else
    obj_to_room(corpse, real_room(MORGUE));
}

/* When ch kills victim */
static void change_alignment(struct char_data *ch, struct char_data *victim)
{
  /* new alignment change algorithm: if you kill a monster with alignment A,
   * you move 1/16th of the way to having alignment -A.  Simple and fast. */
  GET_ALIGNMENT(ch) += (-GET_ALIGNMENT(victim) - GET_ALIGNMENT(ch)) / 16;
}

void death_cry(struct char_data *ch)
{
  int door;

  act("Your blood freezes as you hear $n's death cry.", FALSE, ch, 0, 0, TO_ROOM);

  for (door = 0; door < DIR_COUNT; door++)
    if (CAN_GO(ch, door))
      send_to_room(world[IN_ROOM(ch)].dir_option[door]->to_room, "Your blood freezes as you hear someone's death cry.\r\n");
}

void raw_kill(struct char_data *ch, struct char_data *killer)
{
  struct char_data *i, *k, *temp;

  if (FIGHTING(ch))
    stop_fighting(ch);

  /* luminari */
  for (k = combat_list; k; k = temp)
  {
    temp = k->next_fighting;
    if (FIGHTING(k) == ch)
      stop_fighting(k);
  }
  while (ch->affected)
    affect_remove(ch, ch->affected);

  /* To make ordinary commands work in scripts.  welcor*/
  //  GET_POS(ch) = POS_STANDING;
  /* Hopefully prevents double kills */
  GET_POS(ch) = POS_DEAD;

  if (killer)
  {
    if (death_mtrigger(ch, killer))
      death_cry(ch);
  }
  else
    death_cry(ch);

  if (killer)
  {
    if (killer->group)
    {
      while ((i = (struct char_data *)simple_list(killer->group->members)) != NULL)
      {
        if (IN_ROOM(i) == IN_ROOM(ch) || (world[IN_ROOM(i)].zone == world[IN_ROOM(ch)].zone))
          autoquest_trigger_check(i, ch, NULL, AQ_MOB_KILL);
      }
    }
    else
      autoquest_trigger_check(killer, ch, NULL, AQ_MOB_KILL);
  }

  /* Alert Group if Applicable */
  if (GROUP(ch))
  {
    send_to_group(ch, GROUP(ch), "%s has died.\r\n", GET_NAME(ch));
  }
  update_pos(ch);
  make_corpse(ch);
  Crash_rentsave(ch, 0);
  extract_char(ch);

  if (killer)
  {
    autoquest_trigger_check(killer, NULL, NULL, AQ_MOB_SAVE);
    autoquest_trigger_check(killer, NULL, NULL, AQ_ROOM_CLEAR);
  }
}

void die(struct char_data *ch, struct char_data *killer)
{
  update_pos(ch);
  gain_exp(ch, -(GET_EXP(ch) / 2));
  if (!IS_NPC(ch))
  {
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_KILLER);
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_THIEF);
  }
  raw_kill(ch, killer);
}

static void perform_group_gain(struct char_data *ch, long base, struct char_data *victim)
{
  long share, hap_share;

  share = LMIN(CONFIG_MAX_EXP_GAIN, LMAX(1, base));

  if ((IS_HAPPYHOUR) && (IS_HAPPYEXP))
  {
    /* This only reports the correct amount - the calc is done in gain_exp */
    hap_share = share + (long)((float)share * ((float)HAPPY_EXP / (float)(100)));
    share = LMIN(CONFIG_MAX_EXP_GAIN, LMAX(1, hap_share));
  }

  if (share > 1)
    send_to_char(ch, "You receive your share of experience -- %s points.\r\n", add_commas(share));
  else
    send_to_char(ch, "You receive your share of experience -- one measly little point!\r\n");

  gain_exp(ch, share);
  change_alignment(ch, victim);
}

static void group_gain(struct char_data *ch, struct char_data *victim)
{
  int tot_members = 0;
  long tot_gain = 0, base = 0;
  struct char_data *k, *i;
  int mult;

  while ((k = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
    if (IN_ROOM(ch) == IN_ROOM(k))
      tot_members++;
  /*
   * Salty 15 JAN 2019
   */
  tot_gain = LMIN(CONFIG_MAX_EXP_GAIN, GET_EXP(victim));

  /* prevent illegal xp creation when killing players */
  if (!IS_NPC(victim))
    tot_gain = LMIN(CONFIG_MAX_EXP_LOSS * 2 / 3, tot_gain);

  mult = tot_members;

  if (IS_AFFECTED(victim, AFF_SANCTUARY))
    mult += 1.5;
  if (IS_AFFECTED(victim, AFF_DEFENSE))
    mult += 1.5;
  if (IS_AFFECTED(victim, AFF_FURY))
    mult += 1.5;
  if (IS_AFFECTED(victim, AFF_OFFENSE))
    mult += 1.5;

  if (tot_members >= 1)
    base = LMAX(1, tot_gain * (long)mult);

  while ((k = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
    if (IN_ROOM(k) == IN_ROOM(ch))
      perform_group_gain(k, base, victim);

  while ((i = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
  {
    if (IN_ROOM(i) == IN_ROOM(ch) || (world[IN_ROOM(i)].zone == world[IN_ROOM(ch)].zone))
      autoquest_trigger_check(i, ch, NULL, AQ_MOB_KILL);
    if (i->group->leader && PRF_FLAGGED(i, PRF_AUTOLOOT))
      do_get(i, "all corpse", 0, 0);
    else if (PRF_FLAGGED(i, PRF_AUTOLOOT))
      do_get(i, "all corpse", 0, 0);
  }
}

static void solo_gain(struct char_data *ch, struct char_data *victim)
{
  long exp, happy_exp;
  int mult = 1;

  /*
   * Mob buffs multiply exp gained
   * Salty, 15 JAN 2019
   */

  exp = LMIN(CONFIG_MAX_EXP_GAIN, GET_EXP(victim));
  if (IS_AFFECTED(victim, AFF_SANCTUARY))
    mult += 1;
  if (IS_AFFECTED(victim, AFF_DEFENSE))
    mult += 1;
  if (IS_AFFECTED(victim, AFF_FURY))
    mult += 1;
  if (IS_AFFECTED(victim, AFF_OFFENSE))
    mult += 1;

  exp *= mult;
  exp = LMAX(1, exp);

  if (IS_HAPPYHOUR && IS_HAPPYEXP)
  {
    happy_exp = exp + (long)((float)exp * ((float)HAPPY_EXP / (float)(100)));
    exp = LMAX(happy_exp, 1);
  }

  if (exp > 1)
    send_to_char(ch, "You receive %s experience points.\r\n", add_commas(exp));
  else
    send_to_char(ch, "You receive one lousy experience point.\r\n");

  gain_exp(ch, exp);
  change_alignment(ch, victim);

  if (PRF_FLAGGED(ch, PRF_AUTOLOOT))
    do_get(ch, "all corpse", 0, 0);
}

static char *replace_string(const char *str, const char *weapon_singular, const char *weapon_plural)
{
  static char buf[256];
  char *cp = buf;

  for (; *str; str++)
  {
    if (*str == '#')
    {
      switch (*(++str))
      {
      case 'w':
        for (; *weapon_plural; *(cp++) = *(weapon_plural++))
          ;
        break;
      case 'W':
        for (; *weapon_singular; *(cp++) = *(weapon_singular++))
          ;
        break;
      default:
        *(cp++) = '#';
        break;
      }
    }
    else
      *(cp++) = *str;

    *cp = 0;
  } /* For */

  return (buf);
}

/* message for doing damage with a weapon */
static void dam_message(int dam, struct char_data *ch, struct char_data *victim,
                        int w_type)
{
  char *buf;
  char val[MAX_STRING_LENGTH];
  int msgnum;

  static struct dam_weapon_type
  {
    const char *to_room;
    const char *to_char;
    const char *to_victim;
  } dam_weapons[] = {

      // 0
      {"$n misses $N with $s #W.", /* 0 */
       "You miss $N with your #W.",
       "$n misses you with $s #W."},

      /* 1 */
      {"$n unleashes a puny blow, which $N easily absorbs.",
       "Your #W hits the target, but $N absorbs it with ease.",
       "You absorb $n's feeble #W."},

      /* 2..10 */
      {"$n tickles $N with $s #W.",
       "You tickle $N as you #W $M.",
       "$n tickles you as $e #W you."},

      /* 11..20 */
      {"$n touches $N with $s #W.",
       "You touch $N as you #W $M.",
       "$n touches you as $e #W you."},

      /* 21..30 */
      {"$n barely #W $N.",
       "You barely #W $N.",
       "$n barely #W you."},

      /* #5, 31..40 */
      {"$n bruises $N with a glancing blow.",
       "You deal a decent #W to $N, leaving a good-sized bruise.",
       "$n's #W leaves you with a large bruise, but nothing more."},

      /* 41..60 */
      {"$n inflicts a flesh wound on $N with $s #W.",
       "You #W $N just hard enough to leave a lasting wound.",
       "$n's #W inflicts a wound deep enough to leave a scar."},

      /* 61..80 */
      {"$n injures $N with $s #W.",
       "You injure $N with your #W.",
       "$n injures you with $s #W."},

      /* 81..100*/
      {"$n knocks $N back a few steps with $s #W.",
       "Your #W causes $N to stagger backwards.",
       "You sprawl back a few feet, under the force of $n's hit."},

      /* 101..125 */
      {"$n #w $N hard.",
       "You #W $N hard.",
       "$n #w you hard."},

      /* #10, 126..150 */
      {"$n #w $N very hard.",
       "You #W $N very hard.",
       "$n #w you very hard."},

      /* 151..175 */
      {"$n #w $N incredibly hard.",
       "You #W $N incredibly hard.",
       "$n #w you incredibly hard."},

      /* 175..200 */
      {"$n #w $N extremely hard.",
       "You #W $N extremely hard.",
       "$n #w you extremely hard."},

      /* 201..250 */
      {"$n lacerates $N with $s #W.",
       "You lacerate $N with your #W.",
       "$n lacerates you with $s #W."},

      /* 251..300 */
      {"$n disfigures $N with $s #W.",
       "You disfigure $N with your #W.",
       "$n disfigures you with $s #W."},

      /* #15, 300..400 */
      {"$n maims $N with $s #W!",
       "You maim $N with your #W!",
       "$n maims you with $s #W!"},

      /* 400..500 */
      {"$n wrecks $N with $s #W!",
       "You wreck $N with your #W!",
       "$n wrecks you with $s #W!"},

      /* 501..600 */
      {"$n mangles $N with $s #W!",
       "You mangle $N with your #W!",
       "$n mangles you with $s #W!"},

      /* 601..700 */
      {"$n thrashes $N with $s #W.",
       "You thrash $N with your #W.",
       "$n thrashes you with $s #W."},

      /* 701..800 */
      {"$n multilates $N with $s #W.",
       "You multilate $N with your #W.",
       "$n multilates you with $s #W."},

      /* #20, 801..900 */
      {"$n ravages $N with $s #W.",
       "You ravage $N with your #W.",
       "$n ravages you with $s #W."},

      /* 901..1000 */
      {"$n butchers $N with $s #W.",
       "You butcher $N with your #W.",
       "$n butchers you with $s #W."},

      /* 1001..1250 */
      {"$n decimates $N with $s #W!",
       "You decimate $N with your #W!",
       "$n decimates you with $s #W!"},

      /* 1251..1500 */
      {"$n massacres $N with $s #W.",
       "You massacre $N with your #W.",
       "$n massacres you with $s #W."},

      /* 1501..1750 */
      {"$n devastates $N with $s #W.",
       "You devastate $N with your #W.",
       "$n devastates you with $s #W."},

      /* #25, 1751..2000 */
      {"$n slaughters $N with $s #W.",
       "You slaughter $N with your #W.",
       "$n slaughters you with $s #W."},

      /* 2001..2500 */
      {"$n eradicates $N with $s #W.",
       "You eradicate $N with your #W.",
       "$n eradicates you with $s #W."},

      /* 2501..3000 */
      {"$n exterminates $N with $s #W.",
       "You exterminate $N with your #W.",
       "$n exterminates you with $s #W."},

      /* 3001..3750 */
      {"$n obliterates $N with $s #W.",
       "You obliterate $N with your #W.",
       "$n obliterates you with $s #W."},

      /* 3751..4500 */
      {"$n annihilates $N with $s #W.",
       "You annihilate $N with your #W.",
       "$n annihilates you with $s #W."},

      /* #30, 4501..5500 */
      {"$n vaporizes $N with $s #W.",
       "You vaporize $N with your #W.",
       "$n vaporizes you with $s #W."},

      /* 5501..7000 */
      {"$n atomizes $N with $s #W.",
       "You atomize $N with your #W.",
       "$n atomizes you with $s #W."},

      /* 7001..10000 */
      {"$n extinguishes $N with $s #W.",
       "You extinguish $N with your #W.",
       "$n extinguishes you with $s #W."},

      /* 10001..15000 */
      {"$n disintegrates $N with $s #W.",
       "You disintegrate $N with your #W.",
       "$n disintegrates you with $s #W."},

      /* #34, 15001+ :) */
      {"$n destroys $N.",
       "You destroy $N with your #W.",
       "$n destroys you with $s #W."},
  };

  w_type -= TYPE_HIT; /* Change to base of table with text */

  if (dam == 0)
    msgnum = 0;
  else if (dam <= 1)
    msgnum = 1;
  else if (dam <= 10)
    msgnum = 2;
  else if (dam <= 20)
    msgnum = 3;
  else if (dam <= 30) // was 4
    msgnum = 4;
  else if (dam <= 40) // was 6
    msgnum = 5;
  else if (dam <= 60) // was 9
    msgnum = 6;
  else if (dam <= 80) // was 13
    msgnum = 7;
  else if (dam <= 100) // was 18
    msgnum = 8;
  else if (dam <= 125) // was 24
    msgnum = 9;
  else if (dam <= 150) // was 30
    msgnum = 10;
  else if (dam <= 175) // was 36
    msgnum = 11;
  else if (dam <= 200) // was 44
    msgnum = 12;
  else if (dam <= 250) // was 53
    msgnum = 13;
  else if (dam <= 300) // was 64
    msgnum = 14;
  else if (dam <= 400) // was 75
    msgnum = 15;
  else if (dam <= 500) // was 88
    msgnum = 16;
  else if (dam <= 600) // was 99
    msgnum = 17;
  else if (dam <= 700) // was 110
    msgnum = 18;
  else if (dam <= 800) // was 120
    msgnum = 19;
  else if (dam <= 900) // was 130
    msgnum = 20;
  else if (dam <= 1000) // was 145
    msgnum = 21;
  else if (dam <= 1250) // was 150
    msgnum = 22;
  else if (dam <= 1500) // was 160
    msgnum = 23;
  else if (dam <= 1750) // was 180
    msgnum = 24;
  else if (dam <= 2000) // was 200
    msgnum = 25;
  else if (dam <= 2500) // was 225
    msgnum = 26;
  else if (dam <= 3000) // was 250
    msgnum = 27;
  else if (dam <= 3750) // was 300
    msgnum = 28;
  else if (dam <= 4500) // was 400
    msgnum = 29;
  else if (dam <= 5500) // was 600
    msgnum = 30;
  else if (dam <= 7000) // was 1000
    msgnum = 31;
  else if (dam <= 10000) // was 1500
    msgnum = 32;
  else if (dam <= 15000) // was 2000
    msgnum = 33;
  else if (dam <= 25000) // was 2500
    msgnum = 34;
  else
    msgnum = 34;

  /* damage message to onlookers */
  buf = replace_string(dam_weapons[msgnum].to_room,
                       attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  act(buf, FALSE, ch, NULL, victim, TO_NOTVICT);

  /* damage message to damager */
  buf = replace_string(dam_weapons[msgnum].to_char, attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  sprintf(val, " [%d]", dam);
  strcat(buf, val);
  send_to_char(ch, QGRN);
  act(buf, FALSE, ch, NULL, victim, TO_CHAR);
  send_to_char(ch, QNRM);

  /* damage message to damagee */
  buf = replace_string(dam_weapons[msgnum].to_victim,
                       attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  sprintf(val, " [%d]", dam);
  strcat(buf, val);
  send_to_char(victim, CCYEL(victim, C_CMP));
  act(buf, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
  send_to_char(victim, CCNRM(victim, C_CMP));
}

/*  message for doing damage with a spell or skill. Also used for weapon
 *  damage on miss and death blows.*/
int skill_message(int dam, struct char_data *ch, struct char_data *vict,
                  int attacktype)
{
  int i, j, nr;
  struct message_type *msg;
  char buf[MAX_STRING_LENGTH];
  char ddam[MAX_STRING_LENGTH];
  struct obj_data *weap = GET_EQ(ch, WEAR_WIELD);

  // @todo restructure the messages library to a pointer based system as
  // opposed to the current cyclic location system.
  for (i = 0; i < MAX_MESSAGES; i++)
  {
    if (fight_messages[i].a_type == attacktype)
    {
      nr = dice(1, fight_messages[i].number_of_attacks);
      for (j = 1, msg = fight_messages[i].msg; (j < nr) && msg; j++)
        msg = msg->next;

      if (!IS_NPC(vict) && (GET_LEVEL(vict) >= LVL_IMPL))
      {
        act(msg->god_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
        act(msg->god_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT);
        act(msg->god_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
      }
      else if (dam != 0)
      {
        // Don't send redundant color codes for TYPE_SUFFERING & other types
        // of damage without attacker_msg.
        if (GET_POS(vict) == POS_DEAD)
        {
          if (msg->die_msg.attacker_msg)
          {
            strcpy(buf, msg->die_msg.attacker_msg);
            sprintf(ddam, " [%d]", dam);
            strcat(buf, ddam);
            send_to_char(ch, CCCYN(ch, C_CMP));
            act(buf, FALSE, ch, weap, vict, TO_CHAR);
            send_to_char(ch, CCNRM(ch, C_CMP));
          }

          strcpy(buf, msg->die_msg.victim_msg);
          sprintf(ddam, " [%d]", dam);
          strcat(buf, ddam);
          send_to_char(vict, QWHT);
          act(buf, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
          send_to_char(vict, CCNRM(vict, C_CMP));

          strcpy(buf, msg->die_msg.room_msg);
          act(buf, FALSE, ch, weap, vict, TO_NOTVICT);
        }
        else
        {
          if (msg->hit_msg.attacker_msg)
          {
            strcpy(buf, msg->hit_msg.attacker_msg);
            sprintf(ddam, " [%d]", dam);
            strcat(buf, ddam);
            send_to_char(ch, CCCYN(ch, C_CMP));
            act(buf, FALSE, ch, weap, vict, TO_CHAR);
            send_to_char(ch, CCNRM(ch, C_CMP));
          }

          strcpy(buf, msg->hit_msg.victim_msg);
          sprintf(ddam, " [%d]", dam);
          strcat(buf, ddam);
          send_to_char(vict, QWHT);
          act(buf, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
          send_to_char(vict, CCNRM(vict, C_CMP));

          strcpy(buf, msg->hit_msg.room_msg);
          act(buf, FALSE, ch, weap, vict, TO_NOTVICT);
        }
      }
      else if (ch != vict)
      { // Dam == 0
        if (msg->miss_msg.attacker_msg)
        {
          strcpy(buf, msg->miss_msg.attacker_msg);
          sprintf(ddam, " [%d]", dam);
          strcat(buf, ddam);
          send_to_char(ch, QWHT);
          act(buf, FALSE, ch, weap, vict, TO_CHAR);
          send_to_char(ch, CCNRM(ch, C_CMP));
        }

        strcpy(buf, msg->miss_msg.victim_msg);
        sprintf(ddam, " [%d]", dam);
        strcat(buf, ddam);
        send_to_char(vict, QWHT);
        act(buf, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
        send_to_char(vict, CCNRM(vict, C_CMP));

        strcpy(buf, msg->miss_msg.room_msg);
        act(buf, FALSE, ch, weap, vict, TO_NOTVICT);
      }
      return (1);
    }
  }
  return (0);
}

/* This function returns the following codes:
 *	< 0	Victim died.
 *	= 0	No damage.
 *	> 0	How much damage done. */
int damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype)
{
  long local_gold = 0, happy_gold = 0;
  char local_buf[256];
  /*   struct char_data *tmp_char;
  struct obj_data *corpse_obj; */
  int resist = -1;
  int dam_mod = 1;
  if (GET_POS(victim) <= POS_DEAD)
  {
    /* This is "normal"-ish now with delayed extraction. -gg 3/15/2001 */
    if (PLR_FLAGGED(victim, PLR_NOTDEADYET) || MOB_FLAGGED(victim, MOB_NOTDEADYET))
      return (-1);

    log("SYSERR: Attempt to damage corpse '%s' in room #%d by '%s'.",
        GET_NAME(victim), GET_ROOM_VNUM(IN_ROOM(victim)), GET_NAME(ch));
    die(victim, ch);
    return (-1); /* -je, 7/7/92 */
  }

  /* peaceful rooms */
  if (ch->nr != real_mobile(DG_CASTER_PROXY) && ch != victim && ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL))
  {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return (0);
  }
  /* shopkeeper and MOB_NOKILL protection */
  if (!ok_damage_shopkeeper(ch, victim) || MOB_FLAGGED(victim, MOB_NOKILL))
  {
    send_to_char(ch, "This mob is protected.\r\n");
    return (0);
  }

  /* You can't damage an immortal! */
  if (!IS_NPC(victim) && ((GET_LEVEL(victim) >= LVL_IMMORT) && PRF_FLAGGED(victim, PRF_NOHASSLE)))
    dam = 0;

  if (victim != ch)
  {
    /* Start the attacker fighting the victim */
    if (GET_POS(ch) > POS_STUNNED && (FIGHTING(ch) == NULL))
    {
      set_fighting(ch, victim);
    }

    /* Start the victim fighting the attacker */
    if (GET_POS(victim) > POS_STUNNED && (FIGHTING(victim) == NULL))
    {
      set_fighting(victim, ch);
      if (MOB_FLAGGED(victim, MOB_MEMORY) && !IS_NPC(ch))
        remember(victim, ch);
    }
  }

  /* If you attack a pet, it hates your guts */
  if (victim->master == ch)
    stop_follower(victim);

  /* If the attacker is invisible, he becomes visible */
  if (AFF_FLAGGED(ch, AFF_INVISIBLE) || AFF_FLAGGED(ch, AFF_HIDE))
    appear(ch);

  if (attacktype >= TYPE_HIT && attacktype < TYPE_HIT + NUM_ATTACK_TYPES)
  {
    switch (attacktype)
    {
    case TYPE_HIT:
    case TYPE_THRASH:
    case TYPE_PUNCH:
      resist = RESIST_UNARMED;
      break;

    case TYPE_CRUSH:
    case TYPE_SHIELD_SLAM:
    case TYPE_BLAST:
      resist = RESIST_EXOTIC;
      break;

    case TYPE_BLUDGEON:
    case TYPE_POUND:
    case TYPE_MAUL:
      resist = RESIST_BLUNT;
      break;

    case TYPE_STING:
    case TYPE_BITE:
    case TYPE_PIERCE:
    case TYPE_STAB:
      resist = RESIST_PIERCE;
      break;

    case TYPE_WHIP:
    case TYPE_SLASH:
    case TYPE_CLAW:
      resist = RESIST_SLASH;
      break;

    default:
      resist = -1;
      break;
    }
  }

  if (resist > -1 && dam >= 2)
  {
    for (int r = 0; r < NUM_RESISTS; r++)
    {
      if (resist == r)
      {
        if (GET_RESISTS(victim, r) > 0)
          dam -= dam * MIN(GET_RESISTS(victim, r), 100) / 100;

        if (GET_RESISTS(victim, r) < 0)
          dam += dam * MIN(abs(GET_RESISTS(victim, r)), 100) / 500;
      }
    }
  }
  /*
  here we use dam_mod to sum a total damage reductrion constant based upon
  all skills / spells / affects that may boost damage reduction
  */
  if (IS_NPC(victim) && AFF_FLAGGED(victim, AFF_SANCTUARY) && dam >= 2)
    dam_mod += 1;

  if (IS_NPC(victim) && AFF_FLAGGED(victim, AFF_DEFENSE) && !IS_AFFECTED(victim, AFF_WITHER) && dam >= 2)
    dam_mod += 1;

  if (!IS_NPC(victim) && GET_LEVEL(ch) < LVL_IMPL)
  {
    if (AFF_FLAGGED(victim, AFF_SANCTUARY) && dam >= 2)
      dam_mod += 1;

    /* Prot from Evil spell*/
    if (AFF_FLAGGED(victim, AFF_RESIST_EVIL) && dam >= 2)
    {
      if (GET_ALIGNMENT(ch) < -250)
        dam_mod += 1;
    }
    /* Prot from Evil spell*/
    if (AFF_FLAGGED(victim, AFF_RESIST_GOOD) && dam >= 2)
    {
      if (GET_ALIGNMENT(ch) > 250)
        dam_mod += 1;
    }
    /* stance defense*/
    if (AFF_FLAGGED(victim, AFF_DEFENSE) && !IS_AFFECTED(victim, AFF_WITHER))
    {
      if (GET_SKILL(victim, SKILL_DEFENSIVE_STANCE) >= rand_number(1, 101) && dam >= 2)
      {
        dam_mod += 1;
        check_improve(victim, SKILL_DEFENSIVE_STANCE, TRUE);
      }
      else
        check_improve(victim, SKILL_DEFENSIVE_STANCE, FALSE);
    }
    /* damage reduction skill
       really powerful, consider balance here?
     */
    if (GET_SKILL(victim, SKILL_DAMAGE_REDUCTION) >= rand_number(1, 101) && dam >= 2)
    {
      switch (GET_CON(ch))
      {
      case 19:
      case 20:
      case 21:
      case 22:
      case 23:
      case 24:
        dam_mod += 1;
        break;
      case 25:
        dam_mod += 2;
        break;
      default:
        dam_mod += 0;
        break;
      }
      check_improve(victim, SKILL_DAMAGE_REDUCTION, TRUE);
    }
    else
      check_improve(victim, SKILL_DAMAGE_REDUCTION, FALSE);
  }
  if (dam_mod == 0)
    dam_mod = 1;
  dam /= dam_mod;

  /* Check for PK if this is not a PK MUD */
  if (!CONFIG_PK_ALLOWED && !IS_NPC(victim))
  {
    check_killer(ch, victim);
    if (PLR_FLAGGED(ch, PLR_KILLER) && (ch != victim))
      dam = 0;
  }
  if (attacktype == SKILL_DISEMBOWEL)
    dam = GET_HIT(victim);
    
  if (IS_AFFECTED(ch, AFF_RITUAL) && (ch != victim))
  {
    long hexp = 0;
    long amt = (long)dam * (long)(10);
    if (IS_HAPPYHOUR && IS_HAPPYEXP)
    {
      hexp = amt + (long)((float)amt * ((float)HAPPY_EXP / (float)(100)));
      amt = LMAX(hexp, 1);
    }
    gain_exp(ch, amt);
    increase_gold(ch, amt);
    if (!PRF_FLAGGED(ch, PRF_BRIEF))
      send_to_char(ch, "You learn a lot from the battle ritual!\n\r");
  }
  /* Set the maximum damage per round and subtract the hit points */



  dam = MAX(MIN(dam, 100000000), 0);

  GET_HIT(victim) -= MIN(dam, GET_MAX_HIT(victim) + 1);

  update_pos(victim);

  /* skill_message sends a message from the messages file in lib/misc.
   * dam_message just sends a generic "You hit $n extremely hard.".
   * skill_message is preferable to dam_message because it is more
   * descriptive.
   *
   * If we are _not_ attacking with a weapon (i.e. a spell), always use
   * skill_message. If we are attacking with a weapon: If this is a miss or a
   * death blow, send a skill_message if one exists; if not, default to a
   * dam_message. Otherwise, always send a dam_message. */
  if (!IS_WEAPON(attacktype))
    skill_message(dam, ch, victim, attacktype);
  else
  {
    if (GET_POS(victim) == POS_DEAD || dam == 0)
    {
      if (!skill_message(dam, ch, victim, attacktype))
        dam_message(dam, ch, victim, attacktype);
    }
    else
    {
      dam_message(dam, ch, victim, attacktype);
    }
  }

  /* Use send_to_char -- act() doesn't send message if you are DEAD. */
  switch (GET_POS(victim))
  {
  case POS_MORTALLYW:
    act("$n is mortally wounded, and will die soon, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char(victim, "You are mortally wounded, and will die soon, if not aided.\r\n");
    break;
  case POS_INCAP:
    act("$n is incapacitated and will slowly die, if not aided.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char(victim, "You are incapacitated and will slowly die, if not aided.\r\n");
    break;
  case POS_STUNNED:
    act("$n is stunned, but will probably regain consciousness again.", TRUE, victim, 0, 0, TO_ROOM);
    send_to_char(victim, "You're stunned, but will probably regain consciousness again.\r\n");
    break;
  case POS_DEAD:
    act("$n is dead!  R.I.P.", FALSE, victim, 0, 0, TO_ROOM);
    send_to_char(victim, "You are dead!  Sorry...\r\n");
    break;

  default: /* >= POSITION SLEEPING */
    if (dam > (GET_MAX_HIT(victim) / 4))
      send_to_char(victim, "That really did HURT!\r\n");

    if (!IS_NPC(victim))
    {
      if (GET_HIT(victim) < (GET_MAX_HIT(victim) / 4))
      {
        send_to_char(victim, "%sYou wish that your wounds would stop BLEEDING so much!%s\r\n",
                     CCRED(victim, C_SPR), CCNRM(victim, C_SPR));
      }
      if (GET_WIMP_LEV(victim) && (victim != ch) &&
          GET_HIT(victim) < GET_WIMP_LEV(victim) && GET_HIT(victim) > 0)
      {
        send_to_char(victim, "You wimp out, and attempt to flee!\r\n");
        do_flee(victim, NULL, 0, 0);
      }
    }
    if (ch != victim && IS_NPC(victim) && MOB_FLAGGED(victim, MOB_WIMPY))
    {
      if (GET_HIT(victim) < (GET_MAX_HIT(victim) / 5))
        do_flee(victim, NULL, 0, 0);
    }
    break;
  }

  /* Help out poor linkless people who are attacked */
  if (!IS_NPC(victim) && !(victim->desc) && GET_POS(victim) > POS_STUNNED)
  {
    do_flee(victim, NULL, 0, 0);
    if (!FIGHTING(victim))
    {
      act("$n is rescued by divine forces.", FALSE, victim, 0, 0, TO_ROOM);
      GET_WAS_IN(victim) = IN_ROOM(victim);
      char_from_room(victim);
      char_to_room(victim, 0);
    }
  }

  /* stop someone from fighting if they're stunned or worse */
  if (GET_POS(victim) < POS_STUNNED && FIGHTING(victim) != NULL)
    stop_fighting(victim);

  /* Uh oh.  Victim died. */
  if (GET_POS(victim) == POS_DEAD)
  {
    //    stop_fighting_group(ch);
    if (attacktype == SKILL_BACKSTAB || attacktype == SKILL_ASSAULT || attacktype == SKILL_CIRCLE)
      GET_WAIT_STATE(ch) -= 1;

    if (!IS_NPC(victim))
    {
      mudlog(BRF, LVL_IMMORT,
             TRUE, "%s killed by %s at %s", GET_NAME(victim), GET_NAME(ch), world[IN_ROOM(victim)].name);
      if (MOB_FLAGGED(ch, MOB_MEMORY))
        forget(ch, victim);
      game_info("%s killed by %s at %s.", GET_NAME(victim), GET_NAME(ch), world[IN_ROOM(victim)].name);
      die(victim, ch);
    }
    /* Cant determine GET_GOLD on corpse, so do now and store */
    if (IS_NPC(victim))
    {

      stop_fighting(ch);
      stop_fighting(victim);
      if ((IS_HAPPYHOUR) && (IS_HAPPYGOLD))
      {
        happy_gold = (long)((long)GET_GOLD(victim) * (((float)(HAPPY_GOLD)) / (float)100));
        happy_gold = MAX(0, happy_gold);
        increase_gold(victim, happy_gold);
      }
      die(victim, ch);
      if (ch != victim && IS_NPC(victim))
      {
        if (GROUP(ch))
          group_gain(ch, victim);
        else
          solo_gain(ch, victim);
      }
      local_gold = GET_GOLD(victim);
      sprintf(local_buf, "%ld", local_gold);
    }

    /*     if (!IS_NPC(ch) && (ch != victim) && PRF_FLAGGED(ch, PRF_AUTOGOLD))
        {
          do_get(ch, "all.coin corpse", 0, 0);
        }
        if (!IS_NPC(ch) && (ch != victim) && PRF_FLAGGED(ch, PRF_AUTOLOOT))
        {
          do_get(ch, "all corpse", 0, 0);
        }
        if (IS_NPC(victim) && !IS_NPC(ch) && PRF_FLAGGED(ch, PRF_AUTOSAC))
        {
          do_sac(ch, "corpse", 0, 0);
        } */
    return (-1);
  }
  return (dam);
}

/* Calculate the THAC0 of the attacker. 'victim' currently isn't used but you
 * could use it for special cases like weapons that hit evil creatures easier
 * or a weapon that always misses attacking an animal. */
static int compute_thaco(struct char_data *ch, struct char_data *victim)
{
  int calc_thaco;

  /*
    if (!IS_NPC(ch))
      calc_thaco = thaco(GET_CLASS(ch), GET_LEVEL(ch));
    else
      calc_thaco = 20;
  */
  calc_thaco = 20;
  calc_thaco -= str_app[STRENGTH_APPLY_INDEX(ch)].tohit;
  calc_thaco -= (GET_HITROLL(ch) + GET_COMBAT_POWER(ch) + GET_RANK(ch));
  if (GET_CLASS(ch) == CLASS_WIZARD)
    calc_thaco -= GET_INT(ch); /* Intelligence helps! */
  if (GET_CLASS(ch) == CLASS_PRIEST)
    calc_thaco -= GET_WIS(ch); /* So does wisdom */

  return calc_thaco;
}

void hit(struct char_data *ch, struct char_data *victim, int type)
{
  struct obj_data *wielded;
  struct affected_type *aff;

  int w_type, victim_ac, calc_thaco, dam, diceroll;
  int tof = 0;
  float mult;
  /*   char buf[MSL];
    int tof_check = FALSE;
   */
  /* Check that the attacker and victim exist */
  if (!ch || !victim)
    return;

  /* Do some sanity checking, in case someone flees, etc. */
  if (IN_ROOM(ch) != IN_ROOM(victim))
  {
    if (FIGHTING(ch) && FIGHTING(ch) == victim)
      stop_fighting(ch);
    return;
  }

  /* Find the weapon type (for display purposes only) */

  if (!IS_NPC(ch))
  {
    if (type == SKILL_DUAL_WIELD)
      wielded = GET_EQ(ch, WEAR_OFFHAND);
    else if (type == SKILL_SHIELD_MASTER)
      wielded = GET_EQ(ch, WEAR_SHIELD);
    else
      wielded = GET_EQ(ch, WEAR_WIELD);

    if (wielded == GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_WIELD))
      w_type = GET_OBJ_VAL(wielded, 3) + TYPE_HIT;
    else if (wielded == GET_EQ(ch, WEAR_OFFHAND) && GET_EQ(ch, WEAR_OFFHAND))
      w_type = GET_OBJ_VAL(wielded, 3) + TYPE_HIT;
    else if (wielded == GET_EQ(ch, WEAR_SHIELD) && GET_EQ(ch, WEAR_SHIELD))
      w_type = TYPE_SHIELD_SLAM;
    else
      w_type = TYPE_HIT;

    if (type == TYPE_HIT)
      w_type = TYPE_HIT;
  }
  else if (IS_NPC(ch))
  {
    wielded = GET_EQ(ch, WEAR_WIELD);
    if (ch->mob_specials.attack_type != 0)
      w_type = ch->mob_specials.attack_type + TYPE_HIT;
    else
      w_type = TYPE_HIT;
  }
  /* Calculate chance of hit. Lower THAC0 is better for attacker. */
  calc_thaco = compute_thaco(ch, victim);

  /* Calculate the raw armor including magic armor.  Lower AC is better for defender. */
  victim_ac = compute_armor_class(victim) / 10;

  /* roll the die and take your chances... */
  diceroll = rand_number(1, 20);

  /* report for debugging if necessary */
  if (CONFIG_DEBUG_MODE >= NRM)
    send_to_char(ch, "\t1Debug:\r\n   \t2Thaco: \t3%d\r\n   \t2AC: \t3%d\r\n   \t2Diceroll: \t3%d\tn\r\n",
                 calc_thaco, victim_ac, diceroll);

  /* Decide whether this is a hit or a miss.
   *  Victim asleep = hit, otherwise:
   *     1   = Automatic miss.
   *   2..19 = Checked vs. AC.
   *    20   = Automatic hit. */
  if (diceroll == 20 || !AWAKE(victim))
    dam = TRUE;
  else if (diceroll == 1)
    dam = FALSE;
  else
    dam = (calc_thaco - diceroll <= victim_ac);

  /* Parry, Dodge, Shield Block and Evade
Salty 04 JAN 2019
*/
  if ((ch != victim) && (GET_HIT(victim) > 0))
  {
    int skill;
    int ritual = 0;
    int test, threshold;
    if (!IS_NPC(victim))
    {
      /* Mirror Image Spell */
      if (IS_AFFECTED(victim, AFF_MIRROR_IMAGES))
      {
        for (aff = victim->affected; aff; aff = aff->next)
        {
          if (aff->spell == spell_type(victim, SPELL_MIRROR_IMAGE))
          {
            skill = GET_SKILL(victim, SPELL_MIRROR_IMAGE) + GET_SPELLS_AFFECTS(victim) + GET_INT(victim) + GET_LUCK(victim) + aff->modifier;
            threshold = rand_number(50, 1000);
            if (skill >= threshold)
            {
              if (aff->modifier > 0)
              {

                act("$N is fooled and attacks a mirror image of $n!", false, victim, 0, ch, TO_NOTVICT);
                act("You are fooled by another of $n's mirror images!", false, victim, 0, ch, TO_VICT);
                act("You smile and watch $N attack a mirror image of you!", false, victim, 0, ch, TO_CHAR);
                aff->modifier--;
                check_improve(victim, SPELL_MIRROR_IMAGE, TRUE);
              }
              else
              {
                send_to_char(victim, "%s\n\r", spell_info[aff->spell].wear_off_msg);
                affect_remove(victim, aff);
                REMOVE_BIT_AR(AFF_FLAGS(victim), AFF_MIRROR_IMAGES);
              }
              return;
            }
            else
              check_improve(victim, SPELL_MIRROR_IMAGE, FALSE);
          }
        }
      } /* End of Mirror Image */

      if (GET_SKILL(victim, SKILL_EVADE) > 0)
      {
        skill = GET_SKILL(victim, SKILL_EVADE);
        test = rand_number(50, 1000) - GET_INT(victim) - GET_LUCK(victim);
        if (skill >= test)
        {
          act("$n evades $N's attack!", false, victim, 0, ch, TO_NOTVICT);
          act("$n evades your attack!", false, victim, 0, ch, TO_VICT);
          act("You evade $N's attack!", false, victim, 0, ch, TO_CHAR);
          if (GET_POS(victim) != POS_FIGHTING)
            set_fighting(victim, ch);
          check_improve(victim, SKILL_EVADE, TRUE);
          return;
        }
        else
          check_improve(victim, SKILL_EVADE, FALSE);
      }
      if (GET_SKILL(victim, SKILL_SHIELD_BLOCK) > 0)
      {
        skill = GET_SKILL(victim, SKILL_SHIELD_BLOCK);
        if (AFF_FLAGGED(victim, AFF_DEFENSE))
          skill += GET_SKILL(victim, SKILL_DEFENSIVE_STANCE) + GET_STR(victim) + GET_LUCK(victim) + GET_CON(victim);
        test = rand_number(50, 1000);
        if (skill >= test)
        {
          act("$n blocks $N's attack with $s shield!", false, victim, 0, ch, TO_NOTVICT);
          act("$n blocks your attack with $s shield!", false, victim, 0, ch, TO_VICT);
          act("You block $N's attack with your shield!", false, victim, 0, ch, TO_CHAR);
          if (GET_POS(victim) != POS_FIGHTING)
            set_fighting(victim, ch);
          check_improve(victim, SKILL_SHIELD_BLOCK, TRUE);
          if (IS_AFFECTED(victim, AFF_RITUAL) && AFF_FLAGGED(victim, AFF_DEFENSE))
          {
            long hexp = 0;
            ritual = skill * 275;
            if (IS_HAPPYHOUR && IS_HAPPYEXP)
            {
              hexp = ritual + (long)((float)ritual * ((float)HAPPY_EXP / (float)(100)));
              ritual = LMAX(hexp, 1);
            }
            gain_exp(victim, ritual);
            increase_gold(victim, ritual);
          }
          return;
        }
        else
          check_improve(victim, SKILL_SHIELD_BLOCK, FALSE);
      }

      if ((GET_SKILL(victim, SKILL_PARRY) > 0) && (!AFF_FLAGGED(victim, AFF_OFFENSE)))
      {
        skill = GET_SKILL(victim, SKILL_PARRY);
        if (AFF_FLAGGED(victim, AFF_DEFENSE))
          skill += GET_SKILL(victim, SKILL_DEFENSIVE_STANCE) + GET_STR(victim) + GET_LUCK(victim);
        test = rand_number(50, 1000);
        if (skill >= test)
        {
          act("$n parries $N's attack!", false, victim, 0, ch, TO_NOTVICT);
          act("$n parries your attack!", false, victim, 0, ch, TO_VICT);
          act("You parry $N's attack!", false, victim, 0, ch, TO_CHAR);
          if (GET_POS(victim) != POS_FIGHTING)
            set_fighting(victim, ch);
          check_improve(victim, SKILL_PARRY, TRUE);
          if (IS_AFFECTED(victim, AFF_RITUAL) && AFF_FLAGGED(victim, AFF_DEFENSE))
          {
            long hexp = 0;
            ritual = skill * 275;
            if (IS_HAPPYHOUR && IS_HAPPYEXP)
            {
              hexp = ritual + (long)((float)ritual * ((float)HAPPY_EXP / (float)(100)));
              ritual = LMAX(hexp, 1);
            }
            gain_exp(victim, ritual);
            increase_gold(victim, ritual);
          }
          return;
        }
        else
          check_improve(victim, SKILL_PARRY, FALSE);
      }
      if ((GET_SKILL(victim, SKILL_DODGE) > 0) && (!AFF_FLAGGED(victim, AFF_OFFENSE)))
      {
        if (GET_SKILL(victim, SKILL_TWIST_OF_FATE) > rand_number(1, 101))
        {
          check_improve(victim, SKILL_TWIST_OF_FATE, TRUE);
          tof = GET_SKILL(victim, SKILL_TWIST_OF_FATE);
        }
        else
          check_improve(victim, SKILL_TWIST_OF_FATE, FALSE);
        skill = GET_SKILL(victim, SKILL_DODGE);
        tof = GET_SKILL(victim, SKILL_TWIST_OF_FATE);
        if (AFF_FLAGGED(victim, AFF_DEFENSE))
          skill += GET_SKILL(victim, SKILL_DEFENSIVE_STANCE);
        test = rand_number(50, 1000) - GET_DEX(victim) - GET_LUCK(victim) - tof;
        if (skill >= test)
        {
          act("$n dodges $N's attack!", false, victim, 0, ch, TO_NOTVICT);
          act("$n dodges your attack!", false, victim, 0, ch, TO_VICT);
          act("You dodge $N's attack!", false, victim, 0, ch, TO_CHAR);
          if (GET_POS(victim) != POS_FIGHTING)
            set_fighting(victim, ch);
          check_improve(victim, SKILL_DODGE, TRUE);
          return;
        }
        else
          check_improve(victim, SKILL_DODGE, FALSE);
      }
    }
    if (IS_NPC(victim))
    {
      /*
       * NPC cannot dodge backstab and circle
       *
       */
      if (type != SKILL_BACKSTAB && type != SKILL_CIRCLE &&
          type != SKILL_ASSAULT && type != SKILL_BASH &&
          type != SKILL_TUMBLE && type != SKILL_HEADBUTT &&
          type != SKILL_HEADBUTT2 &&
          type != SKILL_EXECUTE &&
          type != SKILL_CHOP &&
          type != SKILL_R_HOOK &&
          type != SKILL_L_HOOK &&
          type != SKILL_ELBOW &&
          type != SKILL_KNEE &&
          type != SKILL_TRIP &&
          type != SKILL_KICK &&
          type != SKILL_SUCKER_PUNCH &&
          type != SKILL_UPPERCUT &&
          type != SKILL_HAYMAKER &&
          type != SKILL_CLOTHESLINE &&
          type != SKILL_TRIP &&
          type != SKILL_PILEDRVIER &&
          type != SKILL_PALM_STRIKE &&
          type != SKILL_ROUNDHOUSE &&
          type != SKILL_SPIN_KICK)
      {
        if (MOB_FLAGGED(victim, MOB_DODGE))
        {
          skill = (GET_DEX(victim) + GET_LUCK(victim) + GET_WIS(victim)) * 3;
          if (AFF_FLAGGED(victim, AFF_PARA))
            skill -= 100;
          if (AFF_FLAGGED(victim, AFF_CURSE))
            skill -= 25;
          if (AFF_FLAGGED(victim, AFF_BLIND))
            skill -= 50;

          test = rand_number(50, 400);
          if (skill >= test)
          {
            act("$n dodges $N's attack!", false, victim, 0, ch, TO_NOTVICT);
            act("$n dodges your attack!", false, victim, 0, ch, TO_VICT);
            act("You dodge $N's attack!", false, victim, 0, ch, TO_CHAR);
            if (GET_POS(victim) != POS_FIGHTING)
              set_fighting(victim, ch);
            return;
          }
        }
        if (MOB_FLAGGED(victim, MOB_PARRY))
        {
          skill = (GET_STR(victim) + GET_LUCK(victim) + GET_DEX(victim)) * 3;
          if (AFF_FLAGGED(victim, AFF_PARA))
            skill -= 100;
          if (AFF_FLAGGED(victim, AFF_CURSE))
            skill -= 25;
          if (AFF_FLAGGED(victim, AFF_BLIND))
            skill -= 50;
          test = rand_number(50, 400);
          if (skill >= test)
          {
            act("$n parries $N's attack!", false, victim, 0, ch, TO_NOTVICT);
            act("$n parries your attack!", false, victim, 0, ch, TO_VICT);
            act("You parry $N's attack!", false, victim, 0, ch, TO_CHAR);
            if (GET_POS(victim) != POS_FIGHTING)
              set_fighting(victim, ch);
            return;
          }
        }
      }
    }
  }

  if (!dam)
  {
    /* skill misses should use the skill message */
    if (type == SKILL_BACKSTAB)
      damage(ch, victim, 0, SKILL_BACKSTAB);
    else if (type == SKILL_CIRCLE)
      damage(ch, victim, 0, SKILL_CIRCLE);
    else if (type == SKILL_ASSAULT)
      damage(ch, victim, 0, SKILL_ASSAULT);
    else if (type == SKILL_HEADBUTT)
      damage(ch, victim, 0, SKILL_HEADBUTT);
    else if (type == SKILL_HEADBUTT2)
      damage(ch, victim, 0, SKILL_HEADBUTT2);
    else
      damage(ch, victim, 0, w_type);
  }
  else
  {
    /* okay, we know the guy has been hit.  now calculate damage.
     * Start with the damage bonuses: the damroll and strength apply */
    dam = str_app[STRENGTH_APPLY_INDEX(ch)].todam;
    dam += GET_DAMROLL(ch) + GET_COMBAT_POWER(ch) + GET_RANK(ch);
    /*     int wdam = 0;
     */
    /* Maybe holding arrow? */
    if (wielded && (GET_OBJ_TYPE(wielded) == ITEM_WEAPON))
    {
      /* Add weapon-based damage if a weapon is being wielded */
      dam += dice(GET_OBJ_VAL(wielded, 1), GET_OBJ_VAL(wielded, 2));
    }
    else
    {
      /* If no weapon, add bare hand damage instead */
      if (IS_NPC(ch))
        dam += dice(ch->mob_specials.damnodice, ch->mob_specials.damsizedice);
      else
      {
        if (GET_SKILL(ch, SKILL_UNARMED_COMBAT) >= rand_number(1, 101))
          dam += dice(GET_STR(ch), GET_STR(ch) / 5) + GET_ADD(ch);
        else
          dam += dice(1, 2); /* Max 2 bare hand damage for players */
      }
    }
    /*
      send_to_char(ch, "wdam = %d, todam = %d, GET_DAMROLL(ch) = %d, GET_COMBAT_POWER(ch) = %d, GET_RANK(ch) = %d\r\n",
      wdam, str_app[STRENGTH_APPLY_INDEX(ch)].todam, GET_DAMROLL(ch),GET_COMBAT_POWER(ch), GET_RANK(ch));
      send_to_char(ch, "dam = %d\r\n", dam);
    */

    /* Include a damage multiplier if victim isn't ready to fight:
     * Position sitting  1.33 x normal
     * Position resting  1.66 x normal
     * Position sleeping 2.00 x normal
     * Position stunned  2.33 x normal
     * Position incap    2.66 x normal
     * Position mortally 3.00 x normal
     * Note, this is a hack because it depends on the particular
     * values of the POSITION_XXX constants.
     *
     * */

    mult = 1.0;
    if (GET_POS(victim) < POS_FIGHTING)
    {
      if (GET_POS(victim) == POS_SITTING)
        mult = 2.0;
      else if (GET_POS(victim) == POS_RESTING)
        mult = 3.0;
      else if (GET_POS(victim) == POS_SLEEPING)
        mult = 4.0;
      else if (GET_POS(victim) == POS_STUNNED)
        mult = 5.0;
      else if (GET_POS(victim) == POS_INCAP)
        mult = 7.0;
      else if (GET_POS(victim) == POS_MORTALLYW)
        mult = 10.0;
    }
    else
      mult = 1.0;

    if (AFF_FLAGGED(ch, AFF_FURY) && GET_CLASS(ch) == CLASS_PRIEST)
      mult += 1.5;
    else if (AFF_FLAGGED(ch, AFF_FURY) && GET_CLASS(ch) != CLASS_PRIEST)
      mult += 1;

    if (IS_NPC(ch) && AFF_FLAGGED(ch, AFF_OFFENSE) && !IS_AFFECTED(ch, AFF_WITHER))
      mult += 1;

    if (!IS_NPC(ch))
    {
      if (AFF_FLAGGED(ch, AFF_OFFENSE) && !IS_AFFECTED(ch, AFF_WITHER))
      {
        if (GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) >= rand_number(1, 101))
        {
          mult += 0.5;
          check_improve(ch, SKILL_OFFENSIVE_STANCE, TRUE);
        }
        else
          check_improve(ch, SKILL_OFFENSIVE_STANCE, FALSE);
      }
      if (AFF_FLAGGED(ch, AFF_RAGE))
      {
        if (GET_SKILL(ch, SKILL_RAGE) >= rand_number(1, 101))
        {
          mult += 1;
          check_improve(ch, SKILL_RAGE, TRUE);
        }
        else
          check_improve(ch, SKILL_RAGE, FALSE);
      }
      if (GET_SKILL(ch, SKILL_BATTLE_RYTHM) >= rand_number(1, 101))
      {
        mult += 1;
        check_improve(ch, SKILL_BATTLE_RYTHM, TRUE);
      }
      else
        check_improve(ch, SKILL_BATTLE_RYTHM, FALSE);

      if (GET_SKILL(ch, SKILL_ENHANCED_DAMAGE) >= rand_number(1, 101))
      {
        mult += 0.5;
        check_improve(ch, SKILL_ENHANCED_DAMAGE, TRUE);
      }
      else
        check_improve(ch, SKILL_ENHANCED_DAMAGE, FALSE);

      if (GET_SKILL(ch, SKILL_CRITICAL_HIT) &&
          type != SKILL_REND &&
          type != SKILL_MINCE &&
          type != SKILL_THRUST &&
          type != SKILL_IMPALE)
      {
        mult += (float)check_rogue_critical(ch);
      }
    }
    //    send_to_char(ch,"dam is %d\n\r",dam);
    dam = (int)(dam * mult);

    /* at least 1 hp damage min per hit */
    dam = MAX(1, dam);

    //    send_to_char(ch,"mult is %lf\n\r",mult);
    //    send_to_char(ch,"dam is %d\n\r",dam);

    /*  Damage message changes
Salty 05 NOV 2019
*/
    switch (type)
    {
    /*
     *  Here we're using skill_mult() to manage multiplied damage
     */
    case SKILL_RAMPAGE:
    {
      if (IS_AFFECTED(ch, AFF_DEFENSE))
        damage(ch, victim, skill_mult(ch, SKILL_RAMPAGE, dam, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_RAMPAGE) + GET_SKILL(ch, SKILL_DEFENSIVE_STANCE)), SKILL_RAMPAGE);
      else
        damage(ch, victim, skill_mult(ch, SKILL_RAMPAGE, dam, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_RAMPAGE)), SKILL_RAMPAGE);
      break;
    }
    case SKILL_BACKSTAB:
      damage(ch, victim, skill_mult(ch, SKILL_BACKSTAB, dam, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_BACKSTAB)), SKILL_BACKSTAB);
      break;
    case SKILL_CIRCLE:
      damage(ch, victim, skill_mult(ch, SKILL_CIRCLE, dam, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_CIRCLE)), SKILL_CIRCLE);
      break;

    case SKILL_BASH:
      damage(ch, victim, skill_mult(ch, SKILL_BASH, dam, GET_LEVEL(ch)), SKILL_BASH);
      break;
    case SKILL_ASSAULT:
      damage(ch, victim, skill_mult(ch, SKILL_ASSAULT, dam, GET_LEVEL(ch) + GET_SKILL(ch, SKILL_ASSAULT)), SKILL_ASSAULT);
      break;
    case SKILL_IMPALE:
      damage(ch, victim, skill_mult(ch, SKILL_IMPALE, dam, GET_LEVEL(ch)), TYPE_PIERCE);
      break;
    case SKILL_REND:
      damage(ch, victim, skill_mult(ch, SKILL_REND, dam, GET_LEVEL(ch)), TYPE_SLASH);
      break;
    case SKILL_MINCE:
      damage(ch, victim, skill_mult(ch, SKILL_MINCE, dam, GET_LEVEL(ch)), TYPE_SLASH);
      break;
    case SKILL_THRUST:
      damage(ch, victim, skill_mult(ch, SKILL_THRUST, dam, GET_LEVEL(ch)), TYPE_PIERCE);
      break;

    /*
     *  Here we used unarmed_combat_dam() for fighters
     */
    case SKILL_HEADBUTT:
      damage(ch, victim, unarmed_combat_dam(ch, dam, type), SKILL_HEADBUTT);
      break;
    case SKILL_HEADBUTT2:
      damage(ch, victim, unarmed_combat_dam(ch, dam, type), SKILL_HEADBUTT2);
      break;
    case SKILL_SPIN_KICK:
      damage(ch, victim, unarmed_combat_dam(ch, dam, SKILL_SPIN_KICK), SKILL_SPIN_KICK);
      break;
    case SKILL_ROUNDHOUSE:
    case SKILL_ELBOW:
    case SKILL_KNEE:
    case SKILL_CHOP:
    case SKILL_TRIP:
    case SKILL_KICK:
    case SKILL_R_HOOK:
    case SKILL_L_HOOK:
    case SKILL_SUCKER_PUNCH:
    case SKILL_UPPERCUT:
    case SKILL_HAYMAKER:
    case SKILL_CLOTHESLINE:
    case SKILL_PILEDRVIER:
    case SKILL_PALM_STRIKE:
      damage(ch, victim, unarmed_combat_dam(ch, dam, type), w_type);
      break;
    default:
      damage(ch, victim, dam, w_type);
      break;
    }
  }

  /* check if the victim has a hitprcnt trigger */
  hitprcnt_mtrigger(victim);
}

/* control the fights going on.  Called every 2 seconds from comm.c. */
void perform_violence(void)
{
  struct char_data *ch, *tch, *next_tch, *vict, *tank;
  struct char_data *groupie;

  int dw_num_att = 0, num_attack = 0, sh_num_att = 0, unarmed_num_att = 0;
  int off_stance_bonus = 0;
  char buf[MSL];
  for (ch = combat_list; ch; ch = next_combat_list)
  {
    next_combat_list = ch->next_fighting;

    if (FIGHTING(ch) == NULL || IN_ROOM(ch) != IN_ROOM(FIGHTING(ch)))
    {
      stop_fighting(ch);
      continue;
    }
    if (FIGHTING(ch) && IN_ROOM(ch) == IN_ROOM(FIGHTING(ch)))
    {
      vict = FIGHTING(ch);
    }

/*     if (IS_NPC(ch))
    {

      if (GET_MOB_WAIT(ch) > 0)
      {
        GET_MOB_WAIT(ch) -= PULSE_VIOLENCE;
        continue;
      }
      GET_MOB_WAIT(ch) = 0;

      if (GET_POS(ch) == POS_FIGHTING)
        fight_mtrigger(ch);
    } */
    if (GET_POS(ch) < POS_FIGHTING)
    {
      send_to_char(ch, "You can't fight while sitting!!\r\n");
      continue;
    }
    if (IS_NPC(ch))
    {
      if (GET_MOB_WAIT(ch) > 0)
      {
        GET_MOB_WAIT(ch) -= PULSE_VIOLENCE;
        continue;
      }
      else
        GET_MOB_WAIT(ch) = 0;

      if (GET_POS(ch) < POS_FIGHTING)
      {
        GET_POS(ch) = POS_FIGHTING;
        act("$n scrambles to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
      }
      /* check if the character has a fight trigger */
      if (GET_POS(ch) == POS_FIGHTING)
        fight_mtrigger(ch);
      if (MOB_FLAGGED(ch, MOB_SPEC) && GET_MOB_SPEC(ch) && !MOB_FLAGGED(ch, MOB_NOTDEADYET))
      {
        char actbuf[MAX_INPUT_LENGTH] = "";
        (GET_MOB_SPEC(ch))(ch, ch, 0, actbuf);
      }      
      int mobatt = 1;
      /* Default mob attack counter moblev/10 + 1*/
      if (!(ch->mob_specials.attack_num))
      {
        num_attack = (1 + GET_LEVEL(ch) / 25);
        if (IS_AFFECTED(ch, AFF_HASTE))
          num_attack++;
      }
      else /* ESPEC mob attack number */
        num_attack = ch->mob_specials.attack_num;

      if (IS_AFFECTED(ch, AFF_HASTE))
        num_attack++;
      if (IS_AFFECTED(ch, AFF_WITHER))
        num_attack--;
      if (IS_AFFECTED(ch, AFF_SLOW))
        num_attack--;

      mobatt = 1;

      int i, skill, chance, ritual;

      for (i = 0; i < num_attack; i++)
        hit(ch, FIGHTING(ch), TYPE_UNDEFINED);

      if (FIGHTING(ch))
        vict = FIGHTING(ch);

      if (MOB_FLAGGED(ch, MOB_HIT_GROUP))
      {
        chance = rand_number(0, 601);
        if (FIGHTING(ch))
        {
          mobatt = 1 + (num_attack / 5);
          tank = FIGHTING(ch);
          if (GROUP(tank))
          {
            while ((groupie = (struct char_data *)simple_list(GROUP(tank)->members)) != NULL)
            {
              for (i = 0; i < mobatt; i++)
              {
                if (tank != groupie)
                {                
                  if (AFF_FLAGGED(tank, AFF_DEFENSE))
                  {
                    skill = GET_SKILL(tank, SKILL_ENHANCED_PARRY) + GET_STR(tank) + GET_DEX(tank) + GET_LUCK(tank);
                    skill += GET_SKILL(tank, SKILL_DEFENSIVE_STANCE);
                    if (skill >= chance)
                    {
                      sprintf(buf, "%s lunges across the room to parry %s's attack directed at %s!", GET_NAME(tank), GET_NAME(ch), GET_NAME(groupie));
                      act(buf, false, tank, 0, ch, TO_NOTVICT);
                      sprintf(buf, "You lunge across the room and parry %s's attack directed at %s!", GET_NAME(ch), GET_NAME(groupie));
                      act(buf, false, tank, 0, ch, TO_CHAR);
                      check_improve(tank, SKILL_ENHANCED_PARRY, TRUE);
                      if (IS_AFFECTED(tank, AFF_RITUAL))
                      {
                        long hexp = 0;
                        ritual = skill * 275;
                        if (IS_HAPPYHOUR && IS_HAPPYEXP)
                        {
                          hexp = ritual + (long)((float)ritual * ((float)HAPPY_EXP / (float)(100)));
                          ritual = LMAX(hexp, 1);
                        }
                        gain_exp(tank, ritual);
                        increase_gold(tank, ritual);
                      }
                    }
                    else
                    {
                      hit(ch, groupie, TYPE_UNDEFINED);
                      check_improve(tank, SKILL_ENHANCED_PARRY, FALSE);
                    }
                    fight_sanity(ch, tank);
                  }
                  else
                  {
                    if (!IS_NPC(groupie))
                      hit(ch, groupie, TYPE_UNDEFINED);
                  }
                }
                else
                {
                  if (!IS_NPC(groupie))
                    hit(ch, groupie, TYPE_UNDEFINED);
                }
              }
            }
          }
        }
      }

      for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch)
      {
        next_tch = tch->next_in_room;
        if (IS_NPC(tch))
          continue;
        if (GET_LEVEL(tch) > LVL_IMMORT)
          continue;
        if (tch == ch)
          continue;
        if (MOB_FLAGGED(ch, MOB_HIT_ATTACKERS))
        {

          chance = rand_number(0, 601);
          if (FIGHTING(tch) == ch)
          {
            mobatt = 1 + (num_attack / 5);
            tank = FIGHTING(ch);
            for (i = 0; i < mobatt; i++)
            {
              if (tank != tch)
              {
                if (AFF_FLAGGED(tank, AFF_DEFENSE))
                {
                  skill = GET_SKILL(tank, SKILL_ENHANCED_PARRY) + GET_STR(tank) + GET_DEX(tank) + GET_LUCK(tank);
                  skill += GET_SKILL(tank, SKILL_DEFENSIVE_STANCE);
                  //						send_to_char(vict, "Attackers: Skill is %d, chance is %d\n\r",skill, chance);
                  if (skill >= chance)
                  {
                    sprintf(buf, "%s lunges across the room to parry %s's attack directed at %s!", GET_NAME(tank), GET_NAME(ch), GET_NAME(tch));
                    act(buf, false, tank, 0, ch, TO_NOTVICT);
                    act("$n group parries your attack!", false, tank, 0, ch, TO_VICT);
                    sprintf(buf, "You lunge across the room and parry %s's attack directed at %s!", GET_NAME(ch), GET_NAME(tch));
                    act(buf, false, tank, 0, ch, TO_CHAR);
                    check_improve(tank, SKILL_ENHANCED_PARRY, TRUE);
                    if (IS_AFFECTED(tank, AFF_RITUAL))
                    {
                      long hexp = 0;
                      ritual = skill * 275;
                      if (IS_HAPPYHOUR && IS_HAPPYEXP)
                      {
                        hexp = ritual + (long)((float)ritual * ((float)HAPPY_EXP / (float)(100)));
                        ritual = LMAX(hexp, 1);
                      }
                      gain_exp(tank, ritual);
                      increase_gold(tank, ritual);
                    }
                  }
                  else
                  {
                    hit(ch, tch, TYPE_UNDEFINED);
                    check_improve(tank, SKILL_ENHANCED_PARRY, FALSE);
                  }
                  fight_sanity(ch, tank);
                }
              }
            }
          }
        }
        if (MOB_FLAGGED(ch, MOB_HIT_ROOM))
        {
          chance = rand_number(0, 601);
          mobatt = 1 + (num_attack / 5);
          tank = FIGHTING(ch);
          for (i = 0; i < mobatt; i++)
          {
            if (tank != tch)
            {
              if (AFF_FLAGGED(tank, AFF_DEFENSE))
              {
                skill = GET_SKILL(tank, SKILL_ENHANCED_PARRY) + GET_STR(tank) + GET_DEX(tank) + GET_LUCK(tank);
                skill += GET_SKILL(tank, SKILL_DEFENSIVE_STANCE);
                // send_to_char(vict, "Attackers: Skill is %d, chance is %d\n\r",skill, chance);
                if (skill >= chance)
                {
                  sprintf(buf, "%s lunges across the room to parry %s's attack directed at %s!", GET_NAME(tank), GET_NAME(ch), GET_NAME(tch));
                  act(buf, false, tank, 0, ch, TO_NOTVICT);
                  act("$n group parries your attack!", false, tank, 0, ch, TO_VICT);
                  sprintf(buf, "You lunge across the room and parry %s's attack directed at %s!", GET_NAME(ch), GET_NAME(tch));
                  act(buf, false, tank, 0, ch, TO_CHAR);
                  check_improve(tank, SKILL_ENHANCED_PARRY, TRUE);
                  if (IS_AFFECTED(tank, AFF_RITUAL))
                  {
                    long hexp = 0;
                    ritual = skill * 275;
                    if (IS_HAPPYHOUR && IS_HAPPYEXP)
                    {
                      hexp = ritual + (long)((float)ritual * ((float)HAPPY_EXP / (float)(100)));
                      ritual = LMAX(hexp, 1);
                    }
                    gain_exp(tank, ritual);
                    increase_gold(tank, ritual);
                  }
                }
                else
                {
                  if (!IS_NPC(tch))
                    hit(ch, tch, TYPE_UNDEFINED);
                  check_improve(tank, SKILL_ENHANCED_PARRY, FALSE);
                }
                fight_sanity(ch, tank);
              }
              else
              {
                if (!IS_NPC(tch))
                  hit(ch, tch, TYPE_UNDEFINED);
              }
            }
          }
        }
      }
      if (GET_POS(ch) < POS_FIGHTING)
      {
        GET_POS(ch) = POS_FIGHTING;
        act("$n scrambles to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
      }
    }

    if (!IS_NPC(ch))
    {
      num_attack = 1;

      if (IS_AFFECTED(ch, AFF_OFFENSE) && (GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) + GET_STR(ch) + GET_LUCK(ch) >= rand_number(1, 200)) && !IS_AFFECTED(ch, AFF_WITHER))
        off_stance_bonus = GET_SKILL(ch, SKILL_OFFENSIVE_STANCE);

      if (GET_SKILL(ch, SKILL_SECOND_ATTACK) && (GET_SKILL(ch, SKILL_SECOND_ATTACK) + GET_STR(ch) + GET_LUCK(ch) + off_stance_bonus >= rand_number(1, 300)))
      {
        num_attack++;
        check_improve(ch, SKILL_SECOND_ATTACK, TRUE);
      }
      else
        check_improve(ch, SKILL_SECOND_ATTACK, FALSE);
      if (GET_SKILL(ch, SKILL_THIRD_ATTACK) && (GET_SKILL(ch, SKILL_THIRD_ATTACK) + GET_STR(ch) + GET_LUCK(ch) + off_stance_bonus >= rand_number(1, 300)))
      {
        num_attack++;
        check_improve(ch, SKILL_THIRD_ATTACK, TRUE);
      }
      else
        check_improve(ch, SKILL_THIRD_ATTACK, FALSE);
      if (GET_SKILL(ch, SKILL_FOURTH_ATTACK) && (GET_SKILL(ch, SKILL_FOURTH_ATTACK) + GET_STR(ch) + GET_LUCK(ch) + off_stance_bonus >= rand_number(1, 300)))
      {
        num_attack++;
        check_improve(ch, SKILL_FOURTH_ATTACK, TRUE);
      }
      else
        check_improve(ch, SKILL_FOURTH_ATTACK, FALSE);
      if (GET_SKILL(ch, SKILL_FIFTH_ATTACK) && (GET_SKILL(ch, SKILL_FIFTH_ATTACK) + GET_STR(ch) + GET_LUCK(ch) + off_stance_bonus >= rand_number(1, 300)))
      {
        num_attack++;
        check_improve(ch, SKILL_FIFTH_ATTACK, TRUE);
      }
      else
        check_improve(ch, SKILL_FIFTH_ATTACK, FALSE);
      if (GET_SKILL(ch, SKILL_SIXTH_ATTACK) && (GET_SKILL(ch, SKILL_SIXTH_ATTACK) + GET_STR(ch) + GET_LUCK(ch) + off_stance_bonus >= rand_number(1, 300)))
      {
        num_attack++;
        check_improve(ch, SKILL_SIXTH_ATTACK, TRUE);
      }
      else
        check_improve(ch, SKILL_SIXTH_ATTACK, FALSE);
      if (GET_SKILL(ch, SKILL_SEVENTH_ATTACK) && (GET_SKILL(ch, SKILL_SEVENTH_ATTACK) + GET_STR(ch) + GET_LUCK(ch) + off_stance_bonus >= rand_number(1, 300)))
      {
        num_attack++;
        check_improve(ch, SKILL_SEVENTH_ATTACK, TRUE);
      }
      else
        check_improve(ch, SKILL_SEVENTH_ATTACK, FALSE);

      /* Dual Wield */
      if (!GET_EQ(ch, WEAR_OFFHAND))
        dw_num_att = 0;

      else if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_OFFHAND))
      {
        if (IS_AFFECTED(ch, AFF_OFFENSE) && (GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) >= rand_number(1, 200)) && !IS_AFFECTED(ch, AFF_WITHER))
          off_stance_bonus = GET_SKILL(ch, SKILL_OFFENSIVE_STANCE);

        if (!GET_SKILL(ch, SKILL_DUAL_WIELD))
          dw_num_att = 0;

        else if (GET_SKILL(ch, SKILL_DUAL_WIELD) >= rand_number(1, 101))
        {
          dw_num_att = 1;
          check_improve(ch, SKILL_DUAL_WIELD, TRUE);

          if (GET_SKILL(ch, SKILL_SECOND_ATTACK) && ((GET_SKILL(ch, SKILL_SECOND_ATTACK) + GET_LUCK(ch)) >= rand_number(1, 200)))
            dw_num_att = 2;
          if (GET_SKILL(ch, SKILL_THIRD_ATTACK) && ((GET_SKILL(ch, SKILL_THIRD_ATTACK) + GET_LUCK(ch)) >= rand_number(1, 200)))
            dw_num_att = 3;
        }
        else
        {
          dw_num_att = 0;
          check_improve(ch, SKILL_DUAL_WIELD, FALSE);
        }

        if (IS_AFFECTED(ch, AFF_DEFENSE) && GET_SKILL(ch, SKILL_DEFENSIVE_STANCE) > 0 && dw_num_att > 1)
          dw_num_att -= 1;
      }

      /* Shield Mastery */
      if (!GET_EQ(ch, WEAR_SHIELD))
        sh_num_att = 0;

      else if (GET_EQ(ch, WEAR_SHIELD))
      {
        if (!GET_SKILL(ch, SKILL_SHIELD_MASTER) && !IS_AFFECTED(ch, AFF_OFFENSE))
          sh_num_att = 0;
        else if (!GET_SKILL(ch, SKILL_SHIELD_MASTER) && IS_AFFECTED(ch, AFF_OFFENSE) && !IS_AFFECTED(ch, AFF_WITHER))
          sh_num_att = 1;
        if (GET_SKILL(ch, SKILL_SHIELD_MASTER))
        {
          if (GET_SKILL(ch, SKILL_SHIELD_MASTER) >= rand_number(1, 101))
          {
            sh_num_att = 1;
            check_improve(ch, SKILL_SHIELD_MASTER, TRUE);
          }
          else
          {
            sh_num_att = 0;
            check_improve(ch, SKILL_SHIELD_MASTER, FALSE);
          }
          if ((GET_SKILL(ch, SKILL_SECOND_ATTACK) + GET_LUCK(ch)) >= rand_number(1, 200))
            sh_num_att++;
          if ((GET_SKILL(ch, SKILL_THIRD_ATTACK) + GET_LUCK(ch)) >= rand_number(1, 200))
            sh_num_att++;
          if ((GET_SKILL(ch, SKILL_FOURTH_ATTACK) + GET_LUCK(ch)) >= rand_number(1, 200))
            sh_num_att++;
          if ((GET_SKILL(ch, SKILL_FIFTH_ATTACK) + GET_LUCK(ch)) >= rand_number(1, 200))
            sh_num_att++;
          if ((GET_SKILL(ch, SKILL_SIXTH_ATTACK) + GET_LUCK(ch)) >= rand_number(1, 200))
            sh_num_att++;
          if ((GET_SKILL(ch, SKILL_SEVENTH_ATTACK) + GET_LUCK(ch)) >= rand_number(1, 200))
            sh_num_att++;
        }
        if (IS_AFFECTED(ch, AFF_OFFENSE) && (GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) >= rand_number(1, 200)) && !IS_AFFECTED(ch, AFF_WITHER))
          off_stance_bonus = GET_SKILL(ch, SKILL_OFFENSIVE_STANCE);
        /*         if (IS_AFFECTED(ch, AFF_DEFENSE) && GET_SKILL(ch, SKILL_DEFENSIVE_STANCE) > 0 && sh_num_att > 1)
                  sh_num_att -= 1; */
      }

      /* Unarmed Combat */
      if (
          (!GET_EQ(ch, WEAR_WIELD) && !GET_EQ(ch, WEAR_OFFHAND)) ||
          OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), ITEM_FIST_WEAPON))
      {
        unarmed_num_att = 1;

        if (IS_AFFECTED(ch, AFF_OFFENSE) & !IS_AFFECTED(ch, AFF_WITHER) &&
            (GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) + GET_LUCK(ch) + GET_STR(ch) >= rand_number(1, 300)))
          off_stance_bonus = GET_SKILL(ch, SKILL_OFFENSIVE_STANCE);
 
        if (GET_SKILL(ch, SKILL_SECOND_ATTACK) && (GET_SKILL(ch, SKILL_SECOND_ATTACK) + off_stance_bonus + GET_LUCK(ch)) >= rand_number(1, 300))
          unarmed_num_att++;
        if (GET_SKILL(ch, SKILL_THIRD_ATTACK) && (GET_SKILL(ch, SKILL_THIRD_ATTACK) + off_stance_bonus + GET_LUCK(ch)) >= rand_number(1, 300))
          unarmed_num_att++;
        if (GET_SKILL(ch, SKILL_FOURTH_ATTACK) && (GET_SKILL(ch, SKILL_FOURTH_ATTACK) + off_stance_bonus + GET_LUCK(ch)) >= rand_number(1, 300))
          unarmed_num_att++;
        if (GET_SKILL(ch, SKILL_FIFTH_ATTACK) && (GET_SKILL(ch, SKILL_FIFTH_ATTACK) + off_stance_bonus + GET_LUCK(ch)) >= rand_number(1, 300))
          unarmed_num_att++;
        if (GET_SKILL(ch, SKILL_SIXTH_ATTACK) && (GET_SKILL(ch, SKILL_SIXTH_ATTACK) + off_stance_bonus + GET_LUCK(ch)) >= rand_number(1, 300))
          unarmed_num_att++;
        if (GET_SKILL(ch, SKILL_SEVENTH_ATTACK) && (GET_SKILL(ch, SKILL_SEVENTH_ATTACK) + off_stance_bonus + GET_LUCK(ch)) >= rand_number(1, 300))
          unarmed_num_att++;
      }

      // send_to_char(ch, "num_attack: %d, dw_num_att: %d, sh_num_att: %d, unarmed_num_att: %d\n\r",num_attack,dw_num_att,sh_num_att, unarmed_num_att);

      /* Hit with wield, shield or offhand weapon */
      if (GET_SKILL(ch, SKILL_ACROBATICS) >= rand_number(1, 101))
        check_acrobatics(ch, vict);

      do
      {
        if (num_attack > 0)
        {
          hit(ch, FIGHTING(ch), TYPE_UNDEFINED);
          num_attack--;
        }
        if (dw_num_att > 0)
        {
          hit(ch, FIGHTING(ch), SKILL_DUAL_WIELD);
          dw_num_att--;
        }
        if (sh_num_att > 0)
        {
          hit(ch, FIGHTING(ch), SKILL_SHIELD_MASTER);
          sh_num_att--;
        }
        if (unarmed_num_att > 0)
        {
          hit(ch, FIGHTING(ch), TYPE_HIT);
          unarmed_num_att--;
        }
      } while (num_attack > 0 || dw_num_att > 0 || sh_num_att > 0 || unarmed_num_att > 0);

      if (GET_SKILL(ch, SKILL_SHIELD_SLAM) || GET_SKILL(ch, SKILL_WEAPON_PUNCH))
        knight_combat_update(ch, vict);

      if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_OFFHAND))
      {
        if (GET_SKILL(ch, SKILL_DIRTY_TRICKS) >= rand_number(1, 102))
          check_dirty_tricks(ch, vict);
      }

      if (!GET_EQ(ch, WEAR_WIELD) && !GET_EQ(ch, WEAR_OFFHAND))
      {
        if (GET_SKILL(ch, SKILL_UNARMED_COMBAT) >= rand_number(1, 102))
          check_unarmed_combat(ch, vict);
      }
      else if (GET_EQ(ch, WEAR_WIELD) && OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), ITEM_FIST_WEAPON))
      {
        if (GET_SKILL(ch, SKILL_UNARMED_COMBAT) >= rand_number(1, 102))
          check_unarmed_combat(ch, vict);
      }

      if (GROUP(ch))
      {
        while ((tch = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
        {
          if (tch == ch)
            continue;

          if (!IS_NPC(tch) && FIGHTING(tch) && IS_AFFECTED(tch, AFF_WAR_DANCE))
          {
            if (GET_SKILL(tch, SKILL_WAR_DANCE) > 0)
            {
              if (!IS_NPC(ch) && ch != tch)
              {
                mag_affects(GET_LEVEL(tch) + GET_SPELLS_DAMAGE(tch), tch, ch, SPELL_BARD_WAR_DANCE, CAST_SPELL);
                check_improve(tch, SKILL_WAR_DANCE, TRUE);
              }
            }
            else
              check_improve(tch, SKILL_WAR_DANCE, FALSE);
          }

          if (FIGHTING(tch) && IS_AFFECTED(tch, AFF_SLOW_DANCE))
          {
            if (GET_SKILL(tch, SKILL_SLOW_DANCE) > rand_number(1, 100))
            {
              if (!IS_NPC(ch))
              {
                mag_affects(GET_LEVEL(tch) + GET_SPELLS_DAMAGE(tch), tch, ch, SPELL_BARD_SLOW_DANCE, CAST_SPELL);
                //                call_magic(tch, ch, NULL, SPELL_BARD_SLOW_DANCE, GET_LEVEL(tch) + GET_SPELLS_HEALING(tch), CAST_SPELL);
                check_improve(tch, SKILL_SLOW_DANCE, TRUE);
              }
            }
            else
              check_improve(tch, SKILL_SLOW_DANCE, FALSE);
          }

          if (!IS_NPC(tch) && !PRF_FLAGGED(tch, PRF_AUTOASSIST))
            continue;
          if (IN_ROOM(ch) != IN_ROOM(tch))
            continue;
          if (!CAN_SEE(tch, ch))
            continue;
          if (!IS_NPC(tch) && AFF_FLAGGED(tch, AFF_DEFENSE))
          {
            do_silent_rescue(tch, GET_NAME(ch), 0, 0);
            continue;
          }
          if (GET_POS(tch) != POS_STANDING)
            continue;
          if (FIGHTING(tch))
            continue;
          else
          {
            do_assist(tch, GET_NAME(ch), 0, 0);
            continue;
          }
        }
      }
      if (IS_ROGUE(ch) && GET_SKILL(ch, SKILL_DISEMBOWEL))
        check_disembowel(ch, vict);
    }
    // End of !IS_NPC(ch)


  }
}
