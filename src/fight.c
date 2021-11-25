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

#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))
/* The Fight related routines */
void appear(struct char_data *ch)
{
  if (affected_by_spell(ch, SPELL_INVISIBLE))
    affect_from_char(ch, SPELL_INVISIBLE);

  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_INVISIBLE);
  REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);

  if (GET_REAL_LEVEL(ch) < LVL_IMMORT)
    act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
  else
    act("You feel a strange presence as $n appears, seemingly from nowhere.",
        FALSE, ch, 0, 0, TO_ROOM);
}

int compute_armor_class(struct char_data *ch)
{
  int armorclass = GET_AC(ch);

  if (AWAKE(ch))
    armorclass += dex_app[GET_DEX(ch)].defensive * 10;

  if (IS_AFFECTED(ch, AFF_DEFENSE))
    return (MAX(-350, armorclass)); /* -350 is lowest for tanks */
  else
    return (MAX(-200, armorclass)); /* -200 is lowest */
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
  // PVE priority, Salty 05 JAN 2019
  // causes crash
  //  if (!CONFIG_PK_ALLOWED)
  //    if (!IS_NPC(vict) && !IS_NPC(ch))
  //      return;

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
  clear_char_mud_event(ch, eWHIRLWIND);
  REMOVE_FROM_LIST(ch, combat_list, next_fighting);
  ch->next_fighting = NULL;
  FIGHTING(ch) = NULL;
  GET_POS(ch) = POS_STANDING;
  update_pos(ch);
}

static void make_corpse(struct char_data *ch)
{
  char buf2[MAX_NAME_LENGTH + 64];
  struct obj_data *corpse, *o;
  struct obj_data *money;
  int i, x, y;

  corpse = create_obj();

  corpse->item_number = NOTHING;
  IN_ROOM(corpse) = NOWHERE;
  corpse->name = strdup("corpse");

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
  /*
Give corpses levels for better sacrifice
Salty
09 JAN 2019
*/
  GET_OBJ_LEVEL(corpse) = (int)(2 * GET_REAL_LEVEL(ch) / 3) + dice(1, 10);

  if (IS_NPC(ch))
  {
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
  }
  obj_to_room(corpse, IN_ROOM(ch));
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
  struct char_data *i;

  if (FIGHTING(ch))
    stop_fighting(ch);
  while (ch->affected)
    affect_remove(ch, ch->affected);

  /* To make ordinary commands work in scripts.  welcor*/
  GET_POS(ch) = POS_STANDING;

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
        if (IN_ROOM(i) == IN_ROOM(ch) || (world[IN_ROOM(i)].zone == world[IN_ROOM(ch)].zone))
          autoquest_trigger_check(i, ch, NULL, AQ_MOB_KILL);
    }
    else
      autoquest_trigger_check(killer, ch, NULL, AQ_MOB_KILL);
  }

  /* Alert Group if Applicable */
  if (GROUP(ch))
    send_to_group(ch, GROUP(ch), "%s has died.\r\n", GET_NAME(ch));

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
  struct char_data *k;
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

      //0
      {"$n misses $N with $s #W.", /* 0 */
       "You miss $N with your #W.",
       "$n misses you with $s #W."},

      {"$n unleashes a puny blow, which $N easily absorbs.", /* 1 */
       "Your #W hits the target, but $N absorbs it with ease.",
       "You absorb $n's feeble #W."},

      {"$n tickles $N with $s #W.", /* 2 */
       "You tickle $N as you #W $M.",
       "$n tickles you as $e #W you."},

      {"$n barely #W $N.", /* 3 */
       "You barely #W $N.",
       "$n barely #W you."},

      {"$N responds to $n's pathetic #W with a yawn.", /* 4 */
       "$N yawns in boredom as you #W $M.",
       "*Yawn*  Was that $n you just felt hitting you?"},

      {"$n #w $N.", /* 5..6 */
       "You #W $N.",
       "$n #w you."},

      {"$n #w $N hard.", /* 7..9 */
       "You #W $N hard.",
       "$n #w you hard."},

      {"$n #w $N very hard.", /* 10..13 */
       "You #W $N very hard.",
       "$n #w you very hard."},

      {"$n bruises $N with a glancing blow.", /* 14..18 */
       "You deal a decent #W to $N, leaving a good-sized bruise.",
       "$n's #W leaves you with a large bruise, but nothing more."},

      {"$n #w $N extremely hard.", /* 19..24 */
       "You #W $N extremely hard.",
       "$n #w you extremely hard."},

      //10
      {"$n #w $N incredibly hard.", /* 25..30 */
       "You #W $N incredibly hard.",
       "$n #w you incredibly hard."},

      {"$n inflicts a flesh wound on $N with $s #W.", /* 31..36 */
       "You #W $N just hard enough to leave a lasting wound.",
       "$n's #W inflicts a wound deep enough to leave a scar."},

      {"$n massacres $N to small fragments with $s #W.", /* 37..44 */
       "You massacre $N to small fragments with your #W.",
       "$n massacres you to small fragments with $s #W."},

      {"$n obliterates $N with $s #W.", /* 45..53 */
       "You obliterate $N with your #W.",
       "$n obliterates you with $s #W."},

      {"$n decimates $N with $s incredible #W!", /* 54..64 */
       "You decimate $N with your incredible #W!",
       "WHAM! $n decimates you with $s incredible #W!"},

      {"$n knocks $N back a few steps with $s #W.", /* 65..75 */
       "Your #W causes $N to stagger backwards.",
       "You sprawl back a few feet, under the force of $n's hit."},

      {"$n utterly annihilates $N with $s #W.", /* 76..88 */
       "You utterly annihilate $N with your #W.",
       "$n utterly annihilates you with $s #W."},

      {"$n's #W sends $N crying for $S mommy.", /* 89..99 */
       "Your #W sends $N crying for $S mommy.",
       "Waaaaah.....You want your mommy, $n is mean!"},

      {"$n #w $N so darn hard $E forgets $E's $N.", /* 100..110 */
       "You #W $N so darn hard, $E forgets $S name.",
       "Let's see here... what was your name again?  $n?"},

      {"$n's #W leaves $N seeing little birdies!", /* 111..120 */
       "Your last #W left $N seeing little birdies!",
       "Tweet.. Tweet..  Look at all the birdies!  Don't look at $n!"},

      //20
      {"$n's deadly #W forces $N into dark oblivion.", /* 121..130 */
       "Your #W sends $N into a dark moment of oblivion.",
       "You feel a dark lapse of consciousness from $n's #W."},

      {"$n's #W strikes $N so hard that $E starts seeing double!", /* 131..145 */
       "You #W $N so hard $E starts seeing double!",
       "Oh no!  Now there are two $n's killing you!"},

      {"$n's #W evokes a blood-curdling scream from $N.", /* 146..160 */
       "Your #W evokes a hideous scream from $N!",
       "You emit an ear-shattering cry upon the impact of $n's #W."},

      {"$n demonstrates the meaning of true suffering to $N.", /* 161..180 */
       "Your #W teaches $N the true meaning of pain!",
       "My son, $n has taught you what it means to suffer."},

      {"$n dices $N open and rearranges $S innards.", /* 181..200 */
       "Your #W rips $N open so that you may toy with $S innards.",
       "$n rips you open and viciously rearranges your insides."},

      {"$n's #W splatters chunks of $N all over the room!", /* 201..225 */
       "Your #W leaves pieces of $N sprayed all over the ground.",
       "$n's #W sends particles of your body spraying all about!"},

      {"$n paints the walls with the blood of $N.", /* 226..250 */
       "You dice $N open, and with $S fluids write '$n was here'.",
       "$n paints '$n was here' on a wall with your blood."},

      {"$n's mighty #W sends $N to the brink of death.", /* 251..300 */
       "Your mighty #W sends $N to the brink of death.",
       "$n's #W makes you grasp for the last seconds of your life."},

      {"$n hurts $N with $s #W ...REAL BAD!!!!!", /* 301..400 */
       "You #W $N and you KNOW that one had to HURT!",
       "GOOD LORDY!  $n's #W HURTS!!!"},

      {"$n's #W tears through $N like tissue paper.", /* 401..600 */
       "You tear through $N like tissue paper with your #W!",
       "$n shreds your flesh with $s #W!"},

      //30
      {"$n causes $N's life to flash before $S eyes!", /* 601..1000 */
       "You help $N remember $S life before the end.",
       "$n's #W makes your life flash before your eyes!"},

      {"$n wallops the living daylights out of $N with $s #W!", /* 1001..1500 */
       "You wallop the living daylights out of $N with your #W!.",
       "$n wallops the living daylights out of you!"},

      {"$n vaporizes a piece of $N with $s #W.", /* 1501..2000 */
       "You vaporize a piece of $N with your #W.",
       "Uh oh. $n just removed a piece of you."},

      {"$n blasts a shock wave through $N with $s #W.", /* 2001..2500 */
       "You blast a shock wave through $N with your #W.",
       "$n BLASTS a shock wave that reverberates through you."},

      //34
      {"$n destroys $N.", /* 2501+ :) */
       "You destroy $N with your #W.",
       "$n destroys you with $s #W."},
  };

  w_type -= TYPE_HIT; /* Change to base of table with text */

  if (dam == 0)
    msgnum = 0;
  else if (dam <= 1)
    msgnum = 1;
  else if (dam <= 2)
    msgnum = 2;
  else if (dam <= 3)
    msgnum = 3;
  else if (dam <= 4)
    msgnum = 4;
  else if (dam <= 6)
    msgnum = 5;
  else if (dam <= 9)
    msgnum = 6;
  else if (dam <= 13)
    msgnum = 7;
  else if (dam <= 18)
    msgnum = 8;
  else if (dam <= 24)
    msgnum = 9;
  else if (dam <= 30)
    msgnum = 10;
  else if (dam <= 36)
    msgnum = 11;
  else if (dam <= 44)
    msgnum = 12;
  else if (dam <= 53)
    msgnum = 13;
  else if (dam <= 64)
    msgnum = 14;
  else if (dam <= 75)
    msgnum = 15;
  else if (dam <= 88)
    msgnum = 16;
  else if (dam <= 99)
    msgnum = 17;
  else if (dam <= 110)
    msgnum = 18;
  else if (dam <= 120)
    msgnum = 19;
  else if (dam <= 130)
    msgnum = 20;
  else if (dam <= 145)
    msgnum = 21;
  else if (dam <= 150)
    msgnum = 22;
  else if (dam <= 160)
    msgnum = 23;
  else if (dam <= 180)
    msgnum = 24;
  else if (dam <= 200)
    msgnum = 25;
  else if (dam <= 225)
    msgnum = 26;
  else if (dam <= 250)
    msgnum = 27;
  else if (dam <= 300)
    msgnum = 28;
  else if (dam <= 400)
    msgnum = 29;
  else if (dam <= 600)
    msgnum = 30;
  else if (dam <= 1000)
    msgnum = 31;
  else if (dam <= 1500)
    msgnum = 32;
  else if (dam <= 2000)
    msgnum = 33;
  else if (dam <= 2500)
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
  send_to_char(ch, QWHT);
  act(buf, FALSE, ch, NULL, victim, TO_CHAR);
  send_to_char(ch, QNRM);

  /* damage message to damagee */
  buf = replace_string(dam_weapons[msgnum].to_victim,
                       attack_hit_text[w_type].singular, attack_hit_text[w_type].plural);
  sprintf(val, " [%d]", dam);
  strcat(buf, val);
  send_to_char(victim, CCMAG(victim, C_CMP));
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

      if (!IS_NPC(vict) && (GET_REAL_LEVEL(vict) >= LVL_IMPL))
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
  if (!IS_NPC(victim) && ((GET_REAL_LEVEL(victim) >= LVL_IMMORT) && PRF_FLAGGED(victim, PRF_NOHASSLE)))
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
    for (int r = 0; r < NUM_MELEE_RESISTS; r++)
    {
      if (resist == r)
      {
        if (GET_MELEE_RESIST(victim, r) > 0)
        {
          //	  send_to_char(ch,"Positive resist: %d %%, %d, %d\n\r",GET_MELEE_RESIST(victim, r), dam,  dam * MIN(GET_MELEE_RESIST(victim, r),100) / 100);
          dam -= dam * MIN(GET_MELEE_RESIST(victim, r), 1000) / 1000;
        }
        if (GET_MELEE_RESIST(victim, r) < 0)
        {
          //	  send_to_char(ch,"Negative resist: %d %%, %d, %d\n\r",GET_MELEE_RESIST(victim, r), dam, dam + dam * MIN(abs(GET_MELEE_RESIST(victim, r)),100) / 100);
          dam += dam * MIN(abs(GET_MELEE_RESIST(victim, r)), 1000) / 1000;
        }
      }
    }
  }
  /*
  here we use dam_mod to sum a total damage reductrion constant based upon
  all skills / spells / affects that may boost damage reduction
  */
  if (IS_NPC(victim) && AFF_FLAGGED(victim, AFF_SANCTUARY) && dam >= 2)
    dam_mod += 1;

  if (IS_NPC(victim) && AFF_FLAGGED(victim, AFF_DEFENSE) && dam >= 2)
    dam_mod += 1;

  if (!IS_NPC(victim) && GET_REAL_LEVEL(ch) < LVL_IMPL)
  {
    if (AFF_FLAGGED(victim, AFF_SANCTUARY) && dam >= 2)
    {
      dam_mod += 1;
      //      send_to_char(victim, "Sanctuary: dam_mod = %d\n\r", dam_mod);
    }

    /* Prot from Evil spell*/
    if (AFF_FLAGGED(victim, AFF_HOLY_WARDING) && dam >= 2)
    {
      if (GET_ALIGNMENT(ch) < -500)
      {
        dam_mod += 1;
        //	send_to_char(victim, "Holy Warding: dam_mod = %d\n\r", dam_mod);
      }
    }
    /* stance defense*/
    if (AFF_FLAGGED(victim, AFF_DEFENSE))
    {
      if (GET_SKILL(victim, SKILL_DEFENSIVE_STANCE) >= rand_number(1, 101) && dam >= 2)
      {
        dam_mod += 1;
        //	send_to_char(victim, "Skill Defence: dam_mod = %d\n\r", dam_mod);
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
      case 15:
        dam_mod += 1;
        break;
      case 16:
        dam_mod += 1;
        break;
      case 17:
        dam_mod += 1;
        break;
      case 18:
        dam_mod += 1;
        break;
      case 19:
        dam_mod += 1;
        break;
      case 20:
        dam_mod += 2;
        break;
      case 21:
        dam_mod += 2;
        break;
      case 22:
        dam_mod += 2;
        break;
      case 23:
        dam_mod += 2;
        break;
      case 24:
        dam_mod += 2;
        break;
      case 25:
        dam_mod += 3;
        break;
      default:
        dam_mod += 0;
        break;
        //	send_to_char(victim, "Skill Damage Reduction: dam_mod = %d\n\r", dam_mod);
      }
      check_improve(victim, SKILL_DAMAGE_REDUCTION, TRUE);
    }
    else
      check_improve(victim, SKILL_DAMAGE_REDUCTION, FALSE);
    //   send_to_char(victim,"Damage: %d, dam_mod %d.\n\r",dam,dam_mod);
    dam /= dam_mod;
  }
  /* Check for PK if this is not a PK MUD */
  if (!CONFIG_PK_ALLOWED)
  {
    check_killer(ch, victim);
    if (PLR_FLAGGED(ch, PLR_KILLER) && (ch != victim))
      dam = 0;
  }

  /* Set the maximum damage per round and subtract the hit points */
  dam = MAX(MIN(dam, 100000000), 0);

  GET_HIT(victim) -= MIN(dam, GET_MAX_HIT(victim) + 1);

  /*  Gain exp for the hit */
  /*   if (ch != victim)
  {
//    long amt = (long)dam * (long)1000000000;
    long amt = (long)dam * (long)dice(10,10);
    gain_exp(ch, amt);
    increase_gold(ch, amt);
  } */
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

    if (GET_HIT(victim) < (GET_MAX_HIT(victim) / 4))
    {
      send_to_char(victim, "%sYou wish that your wounds would stop BLEEDING so much!%s\r\n",
                   CCRED(victim, C_SPR), CCNRM(victim, C_SPR));
      if (ch != victim && MOB_FLAGGED(victim, MOB_WIMPY))
        do_flee(victim, NULL, 0, 0);
    }
    if (!IS_NPC(victim) && GET_WIMP_LEV(victim) && (victim != ch) &&
        GET_HIT(victim) < GET_WIMP_LEV(victim) && GET_HIT(victim) > 0)
    {
      send_to_char(victim, "You wimp out, and attempt to flee!\r\n");
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
    if (attacktype == SKILL_BACKSTAB || attacktype == SKILL_ASSAULT)
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
      if ((IS_HAPPYHOUR) && (IS_HAPPYGOLD))
      {
        happy_gold = (long)((long)GET_GOLD(victim) * (((float)(HAPPY_GOLD)) / (float)100));
        happy_gold = MAX(0, happy_gold);
        increase_gold(victim, happy_gold);
      }
      die(victim, ch);
      if (ch != victim && !IS_NPC(ch))
      {
        if (GROUP(ch))
          group_gain(ch, victim);
        else
          solo_gain(ch, victim);
      }
      local_gold = GET_GOLD(victim);
      sprintf(local_buf, "%ld", local_gold);
    }

    // if (GROUP(ch) /*&& (local_gold > 0)*/ && PRF_FLAGGED(ch, PRF_AUTOSPLIT))
    // {
    //   generic_find("corpse", FIND_OBJ_ROOM, ch, &tmp_char, &corpse_obj);
    //   if (corpse_obj)
    //   {
    //     send_to_char(ch, "%ld is the local_gold, %s is the local_buf\n\r",local_gold,local_buf);
    //     do_get(ch, "all.coin corpse", 0, 0);
    //     do_split(ch, local_buf, 0, 0);
    //   }
    //   /* need to remove the gold from the corpse */
    // }

    if (!IS_NPC(ch) && (ch != victim) && PRF_FLAGGED(ch, PRF_AUTOGOLD))
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
    }
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

  if (!IS_NPC(ch))
    calc_thaco = thaco(GET_CLASS(ch), GET_REAL_LEVEL(ch));
  else /* THAC0 for monsters is set in the HitRoll */
    calc_thaco = 20;
  calc_thaco -= str_app[STRENGTH_APPLY_INDEX(ch)].tohit;
  calc_thaco -= GET_HITROLL(ch);
  calc_thaco -= (int)((GET_INT(ch) - 13) / 1.5); /* Intelligence helps! */
  calc_thaco -= (int)((GET_WIS(ch) - 13) / 1.5); /* So does wisdom */

  return calc_thaco;
}

void hit(struct char_data *ch, struct char_data *victim, int type)
{
  struct obj_data *wielded;
  struct affected_type *aff;

  int w_type, victim_ac, calc_thaco, dam, diceroll;
  float mult;
  char buf[MSL];
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

  /* Parry, Dodge and Evade
Salty 04 JAN 2019
*/
  if ((ch != victim) && (GET_HIT(victim) > 0))
  {
    int skill;
    int rand, test;
    if (!IS_NPC(victim))
    {
      /* Mirror Image Spell */
      if (IS_AFFECTED(victim, AFF_MIRROR_IMAGES))
      {
        for (aff = victim->affected; aff; aff = aff->next)
        {
          if (aff->spell == spell_type(victim, SPELL_MIRROR_IMAGE))
          {
            skill = GET_SKILL(victim, SPELL_MIRROR_IMAGE);
            rand = rand_number(0, aff->modifier);
            test = rand_number(50, 1000) - GET_INT(victim) - GET_LUCK(victim) - aff->modifier;
            if (skill >= test)
            {
              if (aff->modifier > 1)
              {
                if (rand % 3 == 0)
                {
                  act("$N is fooled and attacks a mirror image of $n!", false, victim, 0, ch, TO_NOTVICT);
                  act("You are fooled by another of $n's mirror images!", false, victim, 0, ch, TO_VICT);
                  act("You smile and watch $N attack a mirror image of you!", false, victim, 0, ch, TO_CHAR);
                  aff->modifier--;
                }
                else
                {
                  act("$n evades $N's attack!", false, victim, 0, ch, TO_NOTVICT);
                  act("$n evades your attack!", false, victim, 0, ch, TO_VICT);
                  act("You evade $N's attack!", false, victim, 0, ch, TO_CHAR);
                }
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
          check_improve(victim, SKILL_EVADE, TRUE);
          return;
        }
        else
          check_improve(victim, SKILL_EVADE, FALSE);
      }

      if ((GET_SKILL(victim, SKILL_PARRY) > 0) && (!AFF_FLAGGED(victim, AFF_OFFENSE)))
      {
        skill = GET_SKILL(victim, SKILL_PARRY);
        if (AFF_FLAGGED(victim, AFF_DEFENSE))
          skill += GET_SKILL(victim, SKILL_DEFENSIVE_STANCE);
        test = rand_number(50, 1000) - GET_STR(victim) - GET_LUCK(victim);
        if (skill >= test)
        {
          act("$n parries $N's attack!", false, victim, 0, ch, TO_NOTVICT);
          act("$n parries your attack!", false, victim, 0, ch, TO_VICT);
          act("You parry $N's attack!", false, victim, 0, ch, TO_CHAR);
          check_improve(victim, SKILL_PARRY, TRUE);
          return;
        }
        else
          check_improve(victim, SKILL_PARRY, FALSE);
      }
      if ((GET_SKILL(victim, SKILL_DODGE) > 0) && (!AFF_FLAGGED(victim, AFF_OFFENSE)))
      {
        skill = GET_SKILL(victim, SKILL_DODGE);
        if (AFF_FLAGGED(victim, AFF_DEFENSE))
          skill += GET_SKILL(victim, SKILL_DEFENSIVE_STANCE);
        test = rand_number(50, 1000) - GET_DEX(victim) - GET_LUCK(victim);
        if (skill >= test)
        {
          act("$n dodges $N's attack!", false, victim, 0, ch, TO_NOTVICT);
          act("$n dodges your attack!", false, victim, 0, ch, TO_VICT);
          act("You dodge $N's attack!", false, victim, 0, ch, TO_CHAR);
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
          type != SKILL_EXECUTE)
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
            return;
          }
        }
      }
    }
  }

  if (!dam)
  {
    /* the attacker missed the victim */
    if (type == SKILL_BACKSTAB)
      damage(ch, victim, 0, SKILL_BACKSTAB);
    else if (type == SKILL_CIRCLE)
      damage(ch, victim, 0, SKILL_CIRCLE);
    else if (type == SKILL_ASSAULT)
      damage(ch, victim, 0, SKILL_ASSAULT);
    else
      damage(ch, victim, 0, w_type);
  }
  else
  {
    /* okay, we know the guy has been hit.  now calculate damage.
* Start with the damage bonuses: the damroll and strength apply */
    dam = str_app[STRENGTH_APPLY_INDEX(ch)].todam;
    dam += GET_DAMROLL(ch);

    /* Maybe holding arrow? */
    if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON)
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
          dam += dice(GET_SKILL(ch, SKILL_UNARMED_COMBAT) / 5, GET_SKILL(ch, SKILL_UNARMED_COMBAT) / 10);
        else
          dam += dice(1, 2); /* Max 2 bare hand damage for players */
      }
    }
    /* Include a damage multiplier if victim isn't ready to fight:
* Position sitting  1.33 x normal
* Position resting  1.66 x normal
* Position sleeping 2.00 x normal
* Position stunned  2.33 x normal
* Position incap    2.66 x normal
* Position mortally 3.00 x normal
* Note, this is a hack because it depends on the particular
* values of the POSITION_XXX constants. */
    if (GET_POS(victim) < POS_FIGHTING)
      mult = 1 + (POS_FIGHTING - GET_POS(victim)) / 3;
    else
      mult = 1.0;

    if (AFF_FLAGGED(ch, AFF_FURY))
      mult += 1.0;

    if (IS_NPC(ch) && AFF_FLAGGED(ch, AFF_OFFENSE))
      mult += 1;

    if (!IS_NPC(ch))
    {
      if (AFF_FLAGGED(ch, AFF_OFFENSE))
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
          mult += 0.5;
          check_improve(ch, SKILL_RAGE, TRUE);
        }
        else
        {
          //            send_to_char(ch, "You come out of your rage and regain your senses!\n\r");
          //            act("$n calms down from raging and $s is aware of everything again!", false, ch, 0, 0, TO_NOTVICT);
          //            REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_RAGE);
          check_improve(ch, SKILL_RAGE, FALSE);
        }
      }

      if (GET_SKILL(ch, SKILL_ENHANCED_DAMAGE) >= rand_number(1, 101))
      {
        mult += 0.5;
        check_improve(ch, SKILL_ENHANCED_DAMAGE, TRUE);
      }
      else
        check_improve(ch, SKILL_ENHANCED_DAMAGE, FALSE);

      if (GET_SKILL(ch, SKILL_CRITICAL_HIT) >= rand_number(1, 101))
      {
        if (diceroll >= (20 - (GET_LUCK(ch) / 5)))
        {

          if (GET_HIT(victim) > 0)
          {
            mult += 0.5;
            check_improve(ch, SKILL_CRITICAL_HIT, TRUE);
            sprintf(buf, "%s%s%s", CCCYN(ch, C_NRM), "You perform a brilliant maneuver and strike $N in a vital area!", CCNRM(ch, C_NRM));
            act(buf, FALSE, ch, 0, victim, TO_CHAR);
            act("$n performs a brilliant maneuver and strikes $N in a vital area!", FALSE, ch, 0, victim, TO_NOTVICT);
            act("$n brilliantly maneuvers past your defenses and strikes you in a vital area!", FALSE, ch, 0, victim, TO_VICT);
          }
        }
        else
          check_improve(ch, SKILL_CRITICAL_HIT, FALSE);
      }
    }
    dam = (int)(dam * mult);

    /* at least 1 hp damage min per hit */
    dam = MAX(1, dam);

    //		send_to_char(ch,"mult is %lf\n\r",mult);

    /*  Damage message changes
Salty 05 NOV 2019
*/
    switch (type)
    {
    case SKILL_BACKSTAB:
      damage(ch, victim, dam * backstab_mult(GET_REAL_LEVEL(ch)), SKILL_BACKSTAB);
      break;
    case SKILL_CIRCLE:
      damage(ch, victim, dam * backstab_mult(GET_REAL_LEVEL(ch)), SKILL_BACKSTAB);
      break;
    case SKILL_WHIRLWIND:
      damage(ch, victim, dam, w_type);
      break;
    case SKILL_HEADBUTT:
      damage(ch, victim, unarmed_combat_dam(ch, type), TYPE_CRUSH);
      break;
    case SKILL_BASH:
      damage(ch, victim, dam * circle_mult(GET_REAL_LEVEL(ch)), SKILL_BASH);
      break;
    case SKILL_ASSAULT:
      damage(ch, victim, dam * backstab_mult(GET_REAL_LEVEL(ch)), SKILL_ASSAULT);
      break;
    case SKILL_TUMBLE:
      damage(ch, victim, dam, SKILL_TUMBLE);
      break;
    case SKILL_EXECUTE:
      damage(ch, victim, GET_MAX_HIT(victim), w_type);
      break;
    case SKILL_IMPALE:
    case SKILL_REND:
    case SKILL_MINCE:
    case SKILL_THRUST:
      damage(ch, victim, dam * circle_mult(GET_REAL_LEVEL(ch)), w_type);
      break;
    case SKILL_R_HOOK:
    case SKILL_L_HOOK:
    case SKILL_SUCKER_PUNCH:
    case SKILL_UPPERCUT:
    case SKILL_HAYMAKER:
    case SKILL_CLOTHESLINE:
    case SKILL_PILEDRVIER:
    case SKILL_PALM_STRIKE:
      damage(ch, victim, unarmed_combat_dam(ch, type), w_type);
      break;
    default:
      damage(ch, victim, dam, w_type);
      break;
    }
  }

  /* check if the victim has a hitprcnt trigger */
  hitprcnt_mtrigger(victim);
}
void fight_sanity(struct char_data *ch, struct char_data *victim)
{
  if (!FIGHTING(victim))
    set_fighting(victim, ch);
  if (!FIGHTING(ch))
    set_fighting(ch, victim);
}

/* control the fights going on.  Called every 2 seconds from comm.c. */
void perform_violence(void)
{
  struct char_data *ch, *tch, *next_tch, *vict;
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
    if (IS_NPC(ch))
    {
      if (IS_AFFECTED(ch, AFF_FEEBLE))
      {
          REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_FEEBLE);
          affect_from_char(ch, SPELL_FEEBLE);
          continue;
      }
      if (GET_MOB_WAIT(ch) > 0)
      {
        GET_MOB_WAIT(ch) -= PULSE_VIOLENCE;
        continue;
      }
      GET_MOB_WAIT(ch) = 0;
      if (GET_POS(ch) < POS_FIGHTING)
      {
        GET_POS(ch) = POS_FIGHTING;
        act("$n scrambles to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
      }
      /* check if the character has a fight trigger */
      fight_mtrigger(ch);
    }
    if (GET_POS(ch) < POS_FIGHTING)
    {
      send_to_char(ch, "You can't fight while sitting!!\r\n");
      continue;
    }

    if (!IS_NPC(ch))
    {
      num_attack = 1;

      if (IS_AFFECTED(ch, AFF_OFFENSE) && GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) >= rand_number(1, 200))
        off_stance_bonus = GET_SKILL(ch, SKILL_OFFENSIVE_STANCE);

      if (GET_SKILL(ch, SKILL_SECOND_ATTACK) >= rand_number(1, 200))
      {
        num_attack = 2;
        check_improve(ch, SKILL_SECOND_ATTACK, TRUE);
      }
      if (GET_SKILL(ch, SKILL_THIRD_ATTACK) >= rand_number(1, 300))
      {
        num_attack = 3;
        check_improve(ch, SKILL_THIRD_ATTACK, TRUE);
      }
      if (GET_SKILL(ch, SKILL_FOURTH_ATTACK) >= rand_number(1, 400))
      {
        num_attack = 4;
        check_improve(ch, SKILL_FOURTH_ATTACK, TRUE);
      }
      if (GET_SKILL(ch, SKILL_FIFTH_ATTACK) >= rand_number(1, 500))
      {
        num_attack = 5;
        check_improve(ch, SKILL_FIFTH_ATTACK, TRUE);
      }

      /* Dual Wield */
      if (!GET_EQ(ch, WEAR_OFFHAND))
        dw_num_att = 0;

      else if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_OFFHAND))
      {
        if (IS_AFFECTED(ch, AFF_OFFENSE) && GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) >= rand_number(1, 200))
          off_stance_bonus = GET_SKILL(ch, SKILL_OFFENSIVE_STANCE);

        if (!GET_SKILL(ch, SKILL_DUAL_WIELD))
          dw_num_att = 0;

        else if (GET_SKILL(ch, SKILL_DUAL_WIELD) >= rand_number(1, 101))
        {
          dw_num_att = 1;
          check_improve(ch, SKILL_DUAL_WIELD, TRUE);

          if (GET_SKILL(ch, SKILL_SECOND_ATTACK) >= rand_number(1, 200))
            dw_num_att = 2;
          else if (GET_SKILL(ch, SKILL_THIRD_ATTACK) >= rand_number(1, 300))
            dw_num_att = 3;
          else if (GET_SKILL(ch, SKILL_FOURTH_ATTACK) >= rand_number(1, 400))
            dw_num_att = 4;
          else if (GET_SKILL(ch, SKILL_FIFTH_ATTACK) >= rand_number(1, 500))
            dw_num_att = 5;
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
        else if (!GET_SKILL(ch, SKILL_SHIELD_MASTER) && IS_AFFECTED(ch, AFF_OFFENSE))
          sh_num_att = 1;

        else if (GET_SKILL(ch, SKILL_SHIELD_MASTER) > 0)
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
          if (GET_SKILL(ch, SKILL_SECOND_ATTACK) >= rand_number(1, 200))
            sh_num_att = 2;
          else if (GET_SKILL(ch, SKILL_THIRD_ATTACK) >= rand_number(1, 300))
            sh_num_att = 3;
          else if (GET_SKILL(ch, SKILL_FOURTH_ATTACK) >= rand_number(1, 400))
            sh_num_att = 4;
          else if (GET_SKILL(ch, SKILL_FIFTH_ATTACK) >= rand_number(1, 500))
            sh_num_att = 5;
        }
        if (IS_AFFECTED(ch, AFF_OFFENSE) && GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) >= rand_number(1, 200))
          off_stance_bonus = GET_SKILL(ch, SKILL_OFFENSIVE_STANCE);
        if (IS_AFFECTED(ch, AFF_DEFENSE) && GET_SKILL(ch, SKILL_DEFENSIVE_STANCE) > 0 && sh_num_att > 1)
          sh_num_att -= 1;
      }

      /* Unarmed Combat */
      if (!GET_EQ(ch, WEAR_WIELD) && !GET_EQ(ch, WEAR_OFFHAND))
      {
        if (IS_AFFECTED(ch, AFF_OFFENSE) && GET_SKILL(ch, SKILL_OFFENSIVE_STANCE) >= rand_number(1, 300))
          off_stance_bonus = GET_SKILL(ch, SKILL_OFFENSIVE_STANCE);

        if (!IS_AFFECTED(ch, AFF_UNARMED_BONUS))
          unarmed_num_att = 0;

        else if (IS_AFFECTED(ch, AFF_UNARMED_BONUS))
        {
          unarmed_num_att = 1;

          if (GET_SKILL(ch, SKILL_SECOND_ATTACK) + off_stance_bonus >= rand_number(1, 200))
            unarmed_num_att = 2;
          else if (GET_SKILL(ch, SKILL_THIRD_ATTACK) + off_stance_bonus >= rand_number(1, 300))
            unarmed_num_att = 3;
          else if (GET_SKILL(ch, SKILL_FOURTH_ATTACK) + off_stance_bonus >= rand_number(1, 400))
            unarmed_num_att = 4;
          else if (GET_SKILL(ch, SKILL_FIFTH_ATTACK) + off_stance_bonus >= rand_number(1, 500))
            unarmed_num_att = 5;
        }
      }

      //send_to_char(ch, "num_attack: %d, dw_num_att: %d, sh_num_att: %d\n\r",num_attack,dw_num_att,sh_num_att);

      /* Hit with wield, shield or offhand weapon */
      if (GET_SKILL(ch, SKILL_ACROBATICS) >= rand_number(1, 101))
        check_acrobatics(ch, vict);

      do
      {

        if (num_attack > 0)
        {
          hit(ch, FIGHTING(ch), TYPE_UNDEFINED);
          if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_OFFHAND))
          {
            if (GET_SKILL(ch, SKILL_DIRTY_TRICKS) >= rand_number(1, 102))
              check_dirty_tricks(ch, vict);
          }
          else if (!GET_EQ(ch, WEAR_WIELD) && !GET_EQ(ch, WEAR_OFFHAND))
          {
            if (GET_SKILL(ch, SKILL_UNARMED_COMBAT) >= rand_number(1, 102))
              check_unarmed_combat(ch, vict);
          }
        }
        if (dw_num_att > 0)
        {
          hit(ch, FIGHTING(ch), SKILL_DUAL_WIELD);
          if (GET_SKILL(ch, SKILL_DIRTY_TRICKS) >= rand_number(1, 102))
            check_dirty_tricks(ch, vict);
        }
        if (sh_num_att > 0)
        {
          hit(ch, FIGHTING(ch), SKILL_SHIELD_MASTER);
        }
        if (unarmed_num_att > 0)
        {
          hit(ch, FIGHTING(ch), TYPE_UNDEFINED);
          if (!GET_EQ(ch, WEAR_WIELD) && !GET_EQ(ch, WEAR_OFFHAND))
          {
            if (GET_SKILL(ch, SKILL_UNARMED_COMBAT) >= rand_number(1, 102))
              check_unarmed_combat(ch, vict);
          }
        }
        num_attack--;
        dw_num_att--;
        sh_num_att--;
        unarmed_num_att--;
      } while (num_attack > 0 || dw_num_att > 0 || sh_num_att > 0 || unarmed_num_att > 0);

      if (GROUP(ch))
      {
        while ((tch = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
        {
          if (tch == ch)
            continue;
          if (!IS_NPC(tch) && !PRF_FLAGGED(tch, PRF_AUTOASSIST))
            continue;
          if (IN_ROOM(ch) != IN_ROOM(tch))
            continue;
          if (!CAN_SEE(tch, ch))
            continue;
          if (GET_POS(tch) != POS_STANDING)
            continue;
          if (FIGHTING(tch))
            continue;

          if (!IS_NPC(tch) && FIGHTING(ch) && (GET_SKILL(tch, SKILL_RESCUE) > rand_number(1, 100)) &&
              GET_SKILL(tch, SKILL_DEFENSIVE_STANCE) > rand_number(1, 100) &&
              AFF_FLAGGED(tch, AFF_DEFENSE))
          {
            do_rescue(tch, GET_NAME(ch), 0, 0);
            continue;
          }
          else
          {
            do_assist(tch, GET_NAME(ch), 0, 0);
            continue;
          }
        }
      }
    }
    // End of !IS_NPC(ch)

    if (IS_NPC(ch))
    {
      /* Default mob attack counter moblev/10 + 1*/
      if (!(ch->mob_specials.attack_num))
      {
        num_attack = (1 + (int)(GET_REAL_LEVEL(ch) / 25));
        if (IS_AFFECTED(ch, AFF_HASTE))
          num_attack++;
      }
      else /* ESPEC mob attack number */
        num_attack = ch->mob_specials.attack_num;

      int i, skill, chance;

      for (i = 0; i < num_attack; i++)
        hit(ch, FIGHTING(ch), TYPE_UNDEFINED);

      for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch)
      {
        next_tch = tch->next_in_room;
        if (IS_NPC(tch))
          continue;
        if (GET_REAL_LEVEL(tch) > LVL_IMMORT)
          continue;
        if (tch == ch)
          continue;
        if (MOB_FLAGGED(ch, MOB_HIT_GROUP))
        {
          vict = FIGHTING(ch);
          if (GROUP(vict) && (GROUP(vict) == GROUP(tch)) && (vict != tch))
          {
            skill = GET_SKILL(vict, SKILL_ENHANCED_PARRY);
            if (AFF_FLAGGED(vict, AFF_DEFENSE))
              skill += GET_SKILL(vict, SKILL_DEFENSIVE_STANCE);
            chance = rand_number(76, 601) - GET_STR(vict) - GET_DEX(vict) - GET_LUCK(vict);
            //						send_to_char(vict, "Group: Skill is %d, chance is %d\n\r",skill, chance);
            if (skill >= chance)
            {
              sprintf(buf, "%s lunges across the room to parry %s's attack directed at %s!", GET_NAME(vict), GET_NAME(ch), GET_NAME(tch));
              act(buf, false, vict, 0, ch, TO_NOTVICT);
              act("$n group parries your attack!", false, vict, 0, ch, TO_VICT);
              sprintf(buf, "You lunge across the room and parry %s's attack directed at %s!", GET_NAME(ch), GET_NAME(tch));
              act(buf, false, vict, 0, ch, TO_CHAR);
            }
            else
            {
              hit(ch, tch, TYPE_UNDEFINED);
              check_improve(vict, SKILL_ENHANCED_PARRY, FALSE);
            }
            check_improve(vict, SKILL_ENHANCED_PARRY, TRUE);
          }
        }
        if (MOB_FLAGGED(ch, MOB_HIT_ATTACKERS))
        {
          if (FIGHTING(tch) == ch)
          {
            vict = FIGHTING(ch);
            if (vict != tch)
            {
              skill = GET_SKILL(vict, SKILL_ENHANCED_PARRY);
              if (AFF_FLAGGED(vict, AFF_DEFENSE))
                skill += GET_SKILL(vict, SKILL_DEFENSIVE_STANCE);
              chance = rand_number(76, 601) - GET_STR(vict) - GET_DEX(vict) - GET_LUCK(vict);
              //						send_to_char(vict, "Attackers: Skill is %d, chance is %d\n\r",skill, chance);
              if (skill >= chance)
              {
                sprintf(buf, "%s lunges across the room to parry %s's attack directed at %s!", GET_NAME(vict), GET_NAME(ch), GET_NAME(tch));
                act(buf, false, vict, 0, ch, TO_NOTVICT);
                act("$n group parries your attack!", false, vict, 0, ch, TO_VICT);
                sprintf(buf, "You lunge across the room and parry %s's attack directed at %s!", GET_NAME(ch), GET_NAME(tch));
                act(buf, false, vict, 0, ch, TO_CHAR);
              }
              else
              {
                hit(ch, tch, TYPE_UNDEFINED);
                check_improve(vict, SKILL_ENHANCED_PARRY, FALSE);
              }
              check_improve(vict, SKILL_ENHANCED_PARRY, TRUE);
            }
          }
        }
        if (MOB_FLAGGED(ch, MOB_HIT_ROOM))
        {
          vict = FIGHTING(ch);
          if (vict != tch)
          {
            skill = GET_SKILL(vict, SKILL_ENHANCED_PARRY);
            if (AFF_FLAGGED(vict, AFF_DEFENSE))
              skill += GET_SKILL(vict, SKILL_DEFENSIVE_STANCE);
            chance = rand_number(76, 601) - GET_STR(vict) - GET_DEX(vict) - GET_LUCK(vict);
            //						send_to_char(vict, "Room: Skill is %d, chance is %d\n\r",skill, chance);
            if (skill >= chance)
            {
              sprintf(buf, "%s lunges across the room to parry %s's attack directed at %s!", GET_NAME(vict), GET_NAME(ch), GET_NAME(tch));
              act(buf, false, vict, 0, ch, TO_NOTVICT);
              act("$n group parries your attack!", false, vict, 0, ch, TO_VICT);
              sprintf(buf, "You lunge across the room and parry %s's attack directed at %s!", GET_NAME(ch), GET_NAME(tch));
              act(buf, false, vict, 0, ch, TO_CHAR);
            }
            else
            {
              hit(ch, tch, TYPE_UNDEFINED);
              check_improve(vict, SKILL_ENHANCED_PARRY, FALSE);
            }
            check_improve(vict, SKILL_ENHANCED_PARRY, TRUE);
          }
        }
      }
    }

    //		if (!IS_NPC(ch) && FIGHTING(ch) && FIGHTING(FIGHTING(ch)))
    //			send_to_char(ch,"fighting: %s, fighting:fighting: %s\n\r", GET_NAME(FIGHTING(ch)), GET_NAME(FIGHTING(FIGHTING(ch))));

    if (MOB_FLAGGED(ch, MOB_SPEC) && GET_MOB_SPEC(ch) && !MOB_FLAGGED(ch, MOB_NOTDEADYET))
    {
      char actbuf[MAX_INPUT_LENGTH] = "";
      (GET_MOB_SPEC(ch))(ch, ch, 0, actbuf);
    }
  }
}
