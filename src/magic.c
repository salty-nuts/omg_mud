/**************************************************************************
*  File: magic.c                                           Part of tbaMUD *
*  Usage: Low-level functions for magic; spell template code.             *
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
#include "interpreter.h"
#include "constants.h"
#include "dg_scripts.h"
#include "class.h"
#include "fight.h"
#include "mud_event.h"
#include "salty.h"

/* local file scope function prototypes */
static int mag_materials(struct char_data *ch, IDXTYPE item0, IDXTYPE item1, IDXTYPE item2, int extract, int verbose);
static void perform_mag_groups(int level, struct char_data *ch, struct char_data *tch, int spellnum, int savetype);

/* Negative apply_saving_throw[] values make saving throws better! So do
 * negative modifiers.  Though people may be used to the reverse of that.
 * It's due to the code modifying the target saving throw instead of the
 * random number of the character as in some other systems. */
int mag_savingthrow(struct char_data *ch, int type, int modifier)
{
  /* NPCs use warrior tables according to some book */
  int class_sav = CLASS_FIGHTER;
  int save;

  if (!IS_NPC(ch))
    class_sav = GET_CLASS(ch);

  save = saving_throws(class_sav, type, GET_REAL_LEVEL(ch));
  save += GET_SAVE(ch, type);
  save += modifier;

  /* Throwing a 0 is always a failure. */
  if (MAX(1, save) < rand_number(0, 99))
    return (TRUE);

  /* Oops, failed. Sorry. */
  return (FALSE);
}

/* affect_update: called from comm.c (causes spells to wear off) */
void affect_update(void)
{
  struct affected_type *af, *next;
  struct char_data *i;

  for (i = character_list; i; i = i->next)
    for (af = i->affected; af; af = next)
    {
      next = af->next;
      if (af->duration >= 1)
        af->duration--;
      else if (af->duration == -1) /* No action */
        ;
      else
      {
        if ((af->spell > 0) && (af->spell <= MAX_SPELLS))
          if (!af->next || (af->next->spell != af->spell) ||
              (af->next->duration > 0))
            if (spell_info[af->spell].wear_off_msg)
              send_to_char(i, "%s\r\n", spell_info[af->spell].wear_off_msg);
        affect_remove(i, af);
      }
    }
}

/* Checks for up to 3 vnums (spell reagents) in the player's inventory. If
 * multiple vnums are passed in, the function ANDs the items together as
 * requirements (ie. if one or more are missing, the spell will not fail).
 * @param ch The caster of the spell.
 * @param item0 The first required item of the spell, NOTHING if not required.
 * @param item1 The second required item of the spell, NOTHING if not required.
 * @param item2 The third required item of the spell, NOTHING if not required.
 * @param extract TRUE if mag_materials should consume (destroy) the items in
 * the players inventory, FALSE if not. Items will only be removed on a
 * successful cast.
 * @param verbose TRUE to provide some generic failure or success messages,
 * FALSE to send no in game messages from this function.
 * @retval int TRUE if ch has all materials to cast the spell, FALSE if not.
 */
static int mag_materials(struct char_data *ch, IDXTYPE item0,
                         IDXTYPE item1, IDXTYPE item2, int extract, int verbose)
{
  /* Begin Local variable definitions. */
  /*------------------------------------------------------------------------*/
  /* Used for object searches. */
  struct obj_data *tobj = NULL;
  /* Points to found reagents. */
  struct obj_data *obj0 = NULL, *obj1 = NULL, *obj2 = NULL;
  /*------------------------------------------------------------------------*/
  /* End Local variable definitions. */

  /* Begin success checks. Checks must pass to signal a success. */
  /*------------------------------------------------------------------------*/
  /* Check for the objects in the players inventory. */
  for (tobj = ch->carrying; tobj; tobj = tobj->next_content)
  {
    if ((item0 != NOTHING) && (GET_OBJ_VNUM(tobj) == item0))
    {
      obj0 = tobj;
      item0 = NOTHING;
    }
    else if ((item1 != NOTHING) && (GET_OBJ_VNUM(tobj) == item1))
    {
      obj1 = tobj;
      item1 = NOTHING;
    }
    else if ((item2 != NOTHING) && (GET_OBJ_VNUM(tobj) == item2))
    {
      obj2 = tobj;
      item2 = NOTHING;
    }
  }

  /* If we needed items, but didn't find all of them, then the spell is a
   * failure. */
  if ((item0 != NOTHING) || (item1 != NOTHING) || (item2 != NOTHING))
  {
    /* Generic spell failure messages. */
    if (verbose)
    {
      switch (rand_number(0, 2))
      {
      case 0:
        send_to_char(ch, "A wart sprouts on your nose.\r\n");
        break;
      case 1:
        send_to_char(ch, "Your hair falls out in clumps.\r\n");
        break;
      case 2:
        send_to_char(ch, "A huge corn develops on your big toe.\r\n");
        break;
      }
    }
    /* Return fales, the material check has failed. */
    return (FALSE);
  }
  /*------------------------------------------------------------------------*/
  /* End success checks. */

  /* From here on, ch has all required materials in their inventory and the
   * material check will return a success. */

  /* Begin Material Processing. */
  /*------------------------------------------------------------------------*/
  /* Extract (destroy) the materials, if so called for. */
  if (extract)
  {
    if (obj0 != NULL)
      extract_obj(obj0);
    if (obj1 != NULL)
      extract_obj(obj1);
    if (obj2 != NULL)
      extract_obj(obj2);
    /* Generic success messages that signals extracted objects. */
    if (verbose)
    {
      send_to_char(ch, "A puff of smoke rises from your pack.\r\n");
      act("A puff of smoke rises from $n's pack.", TRUE, ch, NULL, NULL, TO_ROOM);
    }
  }

  /* Don't extract the objects, but signal materials successfully found. */
  if (!extract && verbose)
  {
    send_to_char(ch, "Your pack rumbles.\r\n");
    act("Something rumbles in $n's pack.", TRUE, ch, NULL, NULL, TO_ROOM);
  }
  /*------------------------------------------------------------------------*/
  /* End Material Processing. */

  /* Signal to calling function that the materials were successfully found
   * and processed. */
  return (TRUE);
}

/* Every spell that does damage comes through here.  This calculates the amount
 * of damage, adds in any modifiers, determines what the saves are, tests for
 * save and calls damage(). -1 = dead, otherwise the amount of damage done. */
int mag_damage(int level, struct char_data *ch, struct char_data *victim,
               int spellnum, int savetype)
{
  int dam = level;
  if (victim == NULL || ch == NULL)
    return (0);

  if (!IS_NPC(ch))
    dam += GET_RANK(ch);

  switch (spellnum)
  {

  case SPELL_HARM:
  case SPELL_FIREBALL:
  case SPELL_CONCUSSIVE_WAVE:
    dam *= dice(2, 4) + 1;
    break;
  case SPELL_MAGIC_MISSILE:
  case SPELL_NIGHTMARE:

    dam *= dice(2, 4) + 1;
    break;

  case SPELL_FIREBLAST:
  case SPELL_SYMBOL_OF_PAIN:
  case SPELL_MISSILE_SPRAY:
  case SPELL_CHAIN_LIGHTNING:
    dam *= dice(1, 4) + 1;
    break;

  case SPELL_CHILL_TOUCH: /* chill touch also has an affect */
  case SPELL_BURNING_HANDS:
  case SPELL_SHOCKING_GRASP:
  case SPELL_LIGHTNING_BOLT:
  case SPELL_COLOR_SPRAY:
  case SPELL_CALL_LIGHTNING:
  case SPELL_EARTHQUAKE:
    dam *= dice(1, 4) + 1;
    break;

  case SPELL_ENERGY_DRAIN:
    dam *= dice(2, 4);
    if (GET_HIT(ch) < GET_MAX_HIT(ch) && !mag_savingthrow(victim, SAVING_PETRI, 0))
    {
      GET_HIT(ch) += (dam / 10);
      if (GET_HIT(ch) > GET_MAX_HIT(ch))
        GET_HIT(ch) = GET_MAX_HIT(ch);
    }
    break;
  }

  /*  Heavy casting improves damage  */
  if (!IS_NPC(ch) && GET_SKILL(ch, SKILL_BATTLE_RYTHM))
  {
    if (rand_number(1, 101) > GET_SKILL(ch, SKILL_BATTLE_RYTHM))
    {
      check_improve(ch, SKILL_BATTLE_RYTHM, FALSE);
    }
    else
    {
      check_improve(ch, SKILL_BATTLE_RYTHM, TRUE);
      if (GET_SKILL(ch, SKILL_BATTLE_RYTHM) > rand_number(1, 300))
      {
        send_to_char(ch, "The battle rythm makes you feel alive!\n\r");
        dam *= 2.0;
      }
      else
      {
        send_to_char(ch, "The battle rythm hangs in the air.\n\r");
        dam *= 1.5;
      }
    }
  }

  /* Dissonance chant bypasses saves */
  if (!IS_NPC(ch) && IS_AFFECTED(ch, AFF_DISSONANCE))
  {
    if (rand_number(1, 101) > GET_SKILL(ch, SKILL_CHANT))
    {
      check_improve(ch, SKILL_CHANT, FALSE);
    }
    else
    {
      send_to_char(ch, "Your dissonant chant powers your song!\n\r");
      check_improve(ch, SKILL_CHANT, TRUE);
      return (damage(ch, victim, dam, spellnum));
    }
  }

  /* and finally, inflict the damage */
  /*  Magic Missile and Missle Spray bypass saves */
  if (spellnum == SPELL_MAGIC_MISSILE || spellnum == SPELL_MISSILE_SPRAY)
    return (damage(ch, victim, dam, spellnum));
  /* divide damage by two if victim makes his saving throw */
  else
    return (damage(ch, victim, mag_savingthrow(victim, savetype, 0) ? dam / 2 : dam, spellnum));
}

/* Every spell that does an affect comes through here.  This determines the
 * effect, whether it is added or replacement, whether it is legal or not, etc.
 * affect_join(vict, aff, add_dur, avg_dur, add_mod, avg_mod) */
#define MAX_SPELL_AFFECTS 5 /* change if more needed */

void mag_affects(int level, struct char_data *ch, struct char_data *victim,
                 int spellnum, int savetype)
{
  struct affected_type af[MAX_SPELL_AFFECTS];
  bool accum_affect = FALSE, accum_duration = FALSE;
  const char *to_vict = NULL, *to_room = NULL;
  const char *to_char = NULL;
  int i, j;

  if (victim == NULL || ch == NULL)
    return;

  for (i = 0; i < MAX_SPELL_AFFECTS; i++)
  {
    new_affect(&(af[i]));
    af[i].spell = spellnum;
  }

  switch (spellnum)
  {

  case SPELL_FRENZY:
    af[0].duration = 0;
    af[0].location = APPLY_AC;
    af[0].modifier = 50;
    SET_BIT_AR(af[0].bitvector, AFF_FRENZY);
    accum_duration = TRUE;
    to_vict = "The battle ritual drives you into a frenzy!";
    to_room = "$n is driven to frenzy by the battle ritual!";
    break;
  case SPELL_REND:
    af[0].duration = 1;
    SET_BIT_AR(af[0].bitvector, AFF_REND);
    accum_duration = TRUE;
    to_vict = "Rends of flesh tear off your body!";
    to_room = "$n screams as rends of flesh tear off $s body!";
    break;
  case SPELL_FEEBLE:
    af[0].location = APPLY_SAVING_SPELL;
    af[0].modifier = 25;
    af[0].duration = 0;
    SET_BIT_AR(af[0].bitvector, AFF_FEEBLE);
    accum_duration = TRUE;
    to_vict = "Your feeble mind cannot concentrate with all that shrieking!";
    to_room = "$n covers $s ears from the shrieking, $e is feeble!";
    break;
  case SPELL_BARD_REGEN:
    af[0].duration = level;
    af[0].location = APPLY_HITP_REGEN;
    af[0].modifier = 50;
    SET_BIT_AR(af[0].bitvector, AFF_REGEN);

    af[1].duration = level;
    af[1].location = APPLY_MANA_REGEN;
    af[1].modifier = 50;
    SET_BIT_AR(af[0].bitvector, AFF_REGEN);

    af[2].duration = level;
    af[2].location = APPLY_MOVE_REGEN;
    af[2].modifier = 50;
    SET_BIT_AR(af[0].bitvector, AFF_REGEN);

    accum_duration = TRUE;
    to_vict = "Your skin briefly glows blue with wode as you begin to regenerate.";
    to_room = "$n's skin glows blue with wode.";
    break;
    break;

  case SPELL_BARD_SANC:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_SANCTUARY);

    accum_duration = TRUE;
    to_vict = "A white aura momentarily surrounds you.";
    to_room = "$n is surrounded by a white aura.";
    break;

  case SPELL_BARD_DEBUFF:
    if (!IS_NPC(ch) && (ch == victim))
      return;
    else if ( !IS_NPC(victim))
      return;  
    if (mag_savingthrow(victim, savetype, 0))
    {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }
    else
    {
      af[0].location = APPLY_HITROLL;
      af[0].duration = level;
      af[0].modifier = -10;
      SET_BIT_AR(af[0].bitvector, AFF_CURSE);

      af[1].location = APPLY_SAVING_SPELL;
      af[1].duration = level;
      af[1].modifier = 10;
      SET_BIT_AR(af[1].bitvector, AFF_CURSE);

      af[2].location = APPLY_AC;
      af[2].duration = level;
      af[2].modifier = 100;
      SET_BIT_AR(af[1].bitvector, AFF_PARA);

      af[3].location = APPLY_DAMROLL;
      af[3].duration = level;
      af[3].modifier = -10;
      SET_BIT_AR(af[2].bitvector, AFF_PARA);

      accum_duration = TRUE;
      to_room = "$n's limbs stiffens and $e turns red from your cutting words!";
      to_vict = "You feel cursed and paralyzed.";
      break;
    }
    break;

  case SPELL_BARD_BLESS:
    af[0].location = APPLY_HITROLL;
    af[0].modifier = 10;
    af[0].duration = level;

    af[1].location = APPLY_DAMROLL;
    af[1].modifier = 10;
    af[1].duration = level;

    accum_duration = TRUE;
    to_vict = "You feel inspired!";
    to_room = "$n is inspired!";
    break;

  case SPELL_BARD_BUFF:
    af[0].location = APPLY_RESIST_UNARMED;
    af[0].modifier = 20;
    af[0].duration = level;

    af[1].location = APPLY_RESIST_SLASH;
    af[1].modifier = 20;
    af[1].duration = level;

    af[2].location = APPLY_RESIST_PIERCE;
    af[2].modifier = 20;
    af[2].duration = level;

    af[3].location = APPLY_RESIST_EXOTIC;
    af[3].modifier = 20;
    af[3].duration = level;

    af[4].location = APPLY_RESIST_BLUNT;
    af[4].modifier = 20;
    af[4].duration = level;

    accum_duration = TRUE;
    to_vict = "You feel ready for battle!";
    to_room = "$n is ready for battle!";
    break;

  case SPELL_CHILL_TOUCH:
    af[0].location = APPLY_STR;
    if (mag_savingthrow(victim, savetype, 0))
      af[0].duration = 1;
    else
      af[0].duration = 4;
    af[0].modifier = -1;
    accum_duration = TRUE;
    to_vict = "You feel your strength wither!";
    to_room = "$n is chilled!";
    break;

  case SPELL_ARMOR:
    af[0].location = APPLY_AC;
    af[0].modifier = -20;
    af[0].duration = level;
    accum_duration = TRUE;
    to_vict = "You feel someone protecting you.";
    to_room = "$n looks armored.";
    break;

  case SPELL_BLESS:
    af[0].location = APPLY_HITROLL;
    af[0].modifier = 10;
    af[0].duration = level;

    af[1].location = APPLY_SAVING_SPELL;
    af[1].modifier = -10;
    af[1].duration = level;

    accum_duration = TRUE;
    to_vict = "You feel righteous.";
    to_room = "$n looks blessed.";
    break;

  case SPELL_BLINDNESS:
    if (MOB_FLAGGED(victim, MOB_NOBLIND) || (!IS_NPC(victim) && GET_REAL_LEVEL(victim) >= LVL_IMMORT) || mag_savingthrow(victim, savetype, 0))
    {
      send_to_char(ch, "You fail.\r\n");
      return;
    }

    af[0].location = APPLY_HITROLL;
    af[0].modifier = -10;
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_BLIND);

    af[1].location = APPLY_AC;
    af[1].modifier = 40;
    af[1].duration = level;
    SET_BIT_AR(af[1].bitvector, AFF_BLIND);

    to_room = "$n seems to be blinded!";
    to_vict = "You have been blinded!";
    break;

  case SPELL_CURSE:
    if (mag_savingthrow(victim, savetype, 0))
    {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_HITROLL;
    af[0].duration = level;
    af[0].modifier = -10;
    SET_BIT_AR(af[0].bitvector, AFF_CURSE);

    af[1].location = APPLY_SAVING_SPELL;
    af[1].duration = level;
    af[1].modifier = 10;
    SET_BIT_AR(af[1].bitvector, AFF_CURSE);

    accum_duration = TRUE;
    to_room = "$n briefly glows red!";
    to_vict = "You feel very uncomfortable.";
    break;

  case SPELL_DETECT_ALIGN:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_ALIGN);
    accum_duration = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_DETECT_INVIS:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_INVIS);
    accum_duration = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_DETECT_MAGIC:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_DETECT_MAGIC);
    accum_duration = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_TRUESIGHT:

    accum_duration = TRUE;
    to_vict = "Your eyes tingle.";
    break;

  case SPELL_HASTE:
    af[0].duration = level;
    af[0].location = APPLY_AC;
    af[0].modifier = -20;
    SET_BIT_AR(af[0].bitvector, AFF_HASTE);
    accum_duration = TRUE;
    to_vict = "You feel yourself speed up.";
    to_room = "$n's movements begin to blur.";
    break;

  case SPELL_FLY:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_FLYING);
    accum_duration = TRUE;
    to_vict = "You float above the ground.";
    to_room = "$n begins to fly.";
    break;

  case SPELL_FURY:
  case SPELL_BARD_FURY:
    af[0].duration = level;
    af[0].location = APPLY_AC;
    af[0].modifier = 100;    
    SET_BIT_AR(af[0].bitvector, AFF_FURY);
    accum_duration = TRUE;
    to_vict = "You feel furious.";
    to_room = "$n's turns red and looks furious.";
    break;

  case SPELL_BARD_SONIC:
    if (!IS_NPC(ch) && (ch == victim))
      return;
    else if ( !IS_NPC(victim))
      return;
    else if (mag_savingthrow(victim, savetype, 0))
    {
      send_to_char(ch, "Your sonic shanty fails to influence %s.\r\n", GET_SHORT(victim));
      return;
    }
    af[0].duration = 1;
    af[0].location = APPLY_RESIST_BLUNT;
    af[0].modifier = -100;
    SET_BIT_AR(af[0].bitvector, AFF_SONIC);
    af[1].duration = 1;
    af[1].location = APPLY_RESIST_EXOTIC;
    af[1].modifier = -100;
    SET_BIT_AR(af[1].bitvector, AFF_SONIC);
    af[2].duration = 1;
    af[2].location = APPLY_RESIST_PIERCE;
    af[2].modifier = -100;
    SET_BIT_AR(af[2].bitvector, AFF_SONIC);    
    af[3].duration = 1;
    af[3].location = APPLY_RESIST_SLASH;
    af[3].modifier = -100;
    SET_BIT_AR(af[3].bitvector, AFF_SONIC);    
    af[4].duration = 1;
    af[4].location = APPLY_RESIST_UNARMED;
    af[4].modifier = -100;
    SET_BIT_AR(af[4].bitvector, AFF_SONIC);    
    to_char = "$p's ears begin to bleed!";
    to_room = "$n's ears begin to bleed!";
    to_vict = "Your ears begin to bleed!";
    break;

  case SPELL_BARD_AGILITY:
    af[0].duration = level;
    af[0].location = APPLY_DEX;
    af[0].modifier = 5;
    SET_BIT_AR(af[0].bitvector, AFF_AGILITY);
    af[1].duration = level;
    af[1].location = APPLY_LUCK;
    af[1].modifier = 5;
    SET_BIT_AR(af[0].bitvector, AFF_AGILITY);
    accum_duration = TRUE;
    to_char = "$p gains a spring in their step!";
    to_room = "$n gains a spring in their step!";
    to_vict = "You gain a spring in your step!";    
    break;

  case SPELL_BARD_KNOWLEDGE:
    af[0].duration = level;
    af[0].location = APPLY_INT;
    af[0].modifier = 5;
    SET_BIT_AR(af[0].bitvector, AFF_KNOWLEDGE);
    af[1].duration = level;
    af[1].location = APPLY_WIS;
    af[1].modifier = 5;
    SET_BIT_AR(af[0].bitvector, AFF_KNOWLEDGE);
    accum_duration = TRUE;
    to_char = "$p gains an insight!";
    to_room = "$n gains an insight!";
    to_vict = "You gain an insight!";    
    break;

  case SPELL_BARD_VITALITY:
    af[0].duration = level;
    af[0].location = APPLY_STR;
    af[0].modifier = 5;
    SET_BIT_AR(af[0].bitvector, AFF_VITALITY);
    af[1].duration = level;
    af[1].location = APPLY_CON;
    af[1].modifier = 5;
    accum_duration = TRUE;
    SET_BIT_AR(af[0].bitvector, AFF_VITALITY);
    to_char = "$p gains extra vitality!";
    to_room = "$n gains extra vitality!";
    to_vict = "You gain extra vitality!";     
    break;

  case SPELL_GAIN_ADVANTAGE:
    af[0].duration = 0;
    af[0].location = APPLY_DAMROLL;
    af[0].modifier = 100;
    SET_BIT_AR(af[0].bitvector, AFF_ADVANTAGE);
    accum_duration = TRUE;
    to_vict = "Your strength and luck causes you to gain an advantage!";
    to_room = "$n's grins as $e spots an advantage.";
    break;
  case SPELL_UNARMED_BONUS:
    af[0].duration = 0;
    SET_BIT_AR(af[0].bitvector, AFF_UNARMED_BONUS);
    accum_duration = TRUE;
    to_vict = "Your prowess endows you with an unarmed bonus!";
    to_room = "$n's eyes widen as $e is endowed with a bonus!";
    break;

  case SPELL_UNARMED_DEBUFF1:
    af[0].location = APPLY_HITROLL;
    af[0].modifier = -10;
    af[0].duration = 1;
    SET_BIT_AR(af[0].bitvector, AFF_UNARMED_DEBUFF1);
    af[1].location = APPLY_DAMROLL;
    af[1].modifier = -10;
    af[1].duration = 1;
    SET_BIT_AR(af[1].bitvector, AFF_UNARMED_DEBUFF1);
    accum_duration = TRUE;
    to_vict = "You are struck by a powerful haymaker!";
    to_room = "$n seems rattled by the haymaker!";
    break;
  case SPELL_UNARMED_DEBUFF2:
    to_room = "$n needs a minute after being laid out by a clothesline.";
    to_vict = "You look for the referee after taking a clothesline from hell!";
    af[0].location = APPLY_AC;
    af[0].modifier = 100;
    af[0].duration = 1;
    accum_duration = TRUE;
    SET_BIT_AR(af[0].bitvector, AFF_UNARMED_DEBUFF2);
    break;
  case SPELL_PASS_DOOR:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_PASS_DOOR);
    accum_duration = TRUE;
    to_vict = "You can pass through UNLOCKED doors.";
    break;
  case SPELL_IMP_PASS_DOOR:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_IMP_PASS_DOOR);
    accum_duration = TRUE;
    to_vict = "You can pass through LOCKED doors.";
    break;

  case SPELL_A_DEBUFF:
    af[0].location = APPLY_LUCK;
    af[0].modifier = -5;
    af[0].duration = 1;
    SET_BIT_AR(af[0].bitvector, AFF_AGILITY);
    af[1].location = APPLY_DEX;
    af[1].modifier = -5;
    af[1].duration = 1;
    SET_BIT_AR(af[1].bitvector, AFF_AGILITY);
    af[2].location = APPLY_HITROLL;
    af[2].modifier = -10;
    af[2].duration = 1;
    SET_BIT_AR(af[2].bitvector, AFF_AGILITY);    
    accum_duration = TRUE;
    to_vict = "You feel less agile!";
    to_room = "$n seems less agile!";
    to_char = "$p seems less agile!";
    break;
  
  case SPELL_K_DEBUFF:
    af[0].location = APPLY_INT;
    af[0].modifier = -5;
    af[0].duration = 1;
    SET_BIT_AR(af[0].bitvector, AFF_KNOWLEDGE);
    af[1].location = APPLY_WIS;
    af[1].modifier = -5;
    af[1].duration = 1;
    SET_BIT_AR(af[1].bitvector, AFF_KNOWLEDGE);
    af[2].location = APPLY_SAVING_SPELL;
    af[2].modifier = 10;
    af[2].duration = 1;
    SET_BIT_AR(af[2].bitvector, AFF_KNOWLEDGE); 
    accum_duration = TRUE;
    to_vict = "You feel less knowledgeable!";
    to_room = "$n seems less knowledgeable!";
    to_char = "$p seems less knowledgeable!";
    break;

  case SPELL_V_DEBUFF:
    af[0].location = APPLY_STR;
    af[0].modifier = -5;
    af[0].duration = 1;
    SET_BIT_AR(af[0].bitvector, AFF_VITALITY);
    af[1].location = APPLY_CON;
    af[1].modifier = -5;
    af[1].duration = 1;
    SET_BIT_AR(af[1].bitvector, AFF_VITALITY);
    af[2].location = APPLY_DAMROLL;
    af[2].modifier = -10;
    af[2].duration = 1;
    SET_BIT_AR(af[2].bitvector, AFF_VITALITY);     
    accum_duration = TRUE;
    to_vict = "You feel your vitality decrease!";
    to_room = "$n seems less vital!";
    to_char = "$p seems less vital!";
    break;

  

  case SPELL_REGENERATION:
    af[0].duration = level;
    af[0].location = APPLY_HITP_REGEN;
    af[0].modifier = 25;
    SET_BIT_AR(af[0].bitvector, AFF_REGEN);

    af[1].duration = level;
    af[1].location = APPLY_MANA_REGEN;
    af[1].modifier = 25;
    SET_BIT_AR(af[0].bitvector, AFF_REGEN);

    af[2].duration = level;
    af[2].location = APPLY_MOVE_REGEN;
    af[2].modifier = 25;
    SET_BIT_AR(af[0].bitvector, AFF_REGEN);

    accum_duration = TRUE;
    to_vict = "Your skin briefly glows blue as you begin to regenerate.";
    to_room = "$n's skin glows blue briefly.";
    break;
  case SPELL_INFRAVISION:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_INFRAVISION);
    accum_duration = TRUE;
    to_vict = "Your eyes glow red.";
    to_room = "$n's eyes glow red.";
    break;

  case SPELL_INVISIBLE:
    if (!victim)
      victim = ch;

    af[0].duration = level;
    af[0].modifier = -40;
    af[0].location = APPLY_AC;
    SET_BIT_AR(af[0].bitvector, AFF_INVISIBLE);
    accum_duration = TRUE;
    to_vict = "You vanish.";
    to_room = "$n slowly fades out of existence.";
    break;

  case SPELL_IMPROVED_INVIS:
    if (!victim)
      victim = ch;

    af[0].duration = level;
    af[0].modifier = -40;
    af[0].location = APPLY_AC;
    SET_BIT_AR(af[0].bitvector, AFF_IMPROVED_INVIS);
    to_vict = "You vanish without a trace.";
    to_room = "$n slowly fades out of existence.";
    break;

  case SPELL_POISON:
    if (mag_savingthrow(victim, savetype, 0))
    {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_STR;
    af[0].duration = level;
    af[0].modifier = -2;
    SET_BIT_AR(af[0].bitvector, AFF_POISON);

    af[1].location = APPLY_DAMROLL;
    af[1].duration = level;
    af[1].modifier = -10;
    SET_BIT_AR(af[1].bitvector, AFF_POISON);

    to_vict = "You feel very sick.";
    to_room = "$n gets violently ill!";
    break;

  case SPELL_HOLY_WARDING:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_HOLY_WARDING);
    accum_duration = TRUE;
    to_vict = "You are warded with holy magic!";
    to_room = "$n is warded with holy magic.";
    break;
  case SPELL_EVIL_WARDING:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_EVIL_WARDING);
    accum_duration = TRUE;
    to_vict = "You are warded with evil magic!";
    to_room = "$n is warded with evil magic.";
    break;

  case SPELL_SANCTUARY:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_SANCTUARY);

    accum_duration = TRUE;
    to_vict = "A white aura momentarily surrounds you.";
    to_room = "$n is surrounded by a white aura.";
    break;
  case SPELL_BETRAYAL:
    if (!IS_NPC(ch))
    {
      if (mag_savingthrow(victim, savetype, 0))
      {
        send_to_char(ch, "%s", CONFIG_NOEFFECT);
        return;
      }
      if (IS_AFFECTED(victim, AFF_SANCTUARY))
      {
        REMOVE_BIT_AR(AFF_FLAGS(victim), AFF_SANCTUARY);
        to_vict = "A black aura momentarily surrounds you before your sanctuary fades away.";
        to_room = "$n is surrounded by a black aura before $s sanctuary fades away.";
        affect_from_char(victim, SPELL_SANCTUARY);
      }
      break;
    }
    else if (IS_NPC(ch))
    {
      if (IS_AFFECTED(victim, AFF_SANCTUARY))
      {
        REMOVE_BIT_AR(AFF_FLAGS(victim), AFF_SANCTUARY);
        to_vict = "A black aura momentarily surrounds you before your sanctuary fades away.";
        to_room = "$n is surrounded by a black aura before $s sanctuary fades away.";
        affect_from_char(victim, SPELL_SANCTUARY);
        break;
      }
    }
  case SPELL_QUICKCAST:
    af[0].duration = level;
    af[0].location = APPLY_MANA_REGEN;
    af[0].modifier = 50;
    SET_BIT_AR(af[0].bitvector, AFF_QUICKCAST);

    accum_duration = TRUE;
    to_vict = "Your mental alacrity increases.";
    to_room = "$n is quite alert.";
    break;

  case SPELL_SLEEP:
    if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(victim))
      return;
    if (MOB_FLAGGED(victim, MOB_NOSLEEP))
      return;
    if (mag_savingthrow(victim, savetype, 0))
      return;

    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_SLEEP);

    if (GET_POS(victim) > POS_SLEEPING)
    {
      send_to_char(victim, "You feel very sleepy...  Zzzz......\r\n");
      act("$n goes to sleep.", TRUE, victim, 0, 0, TO_ROOM);
      GET_POS(victim) = POS_SLEEPING;
    }
    break;

  case SPELL_STRENGTH:
    if (GET_ADD(victim) == 100)
      return;

    af[0].location = APPLY_STR;
    af[0].duration = level;
    af[0].modifier = 1 + (level > 18);
    accum_duration = TRUE;
    accum_affect = TRUE;
    to_vict = "You feel stronger!";
    break;

  case SPELL_SENSE_LIFE:
    to_vict = "Your feel your awareness improve.";
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_SENSE_LIFE);
    accum_duration = TRUE;
    break;

  case SPELL_WATERWALK:
    af[0].duration = level;
    SET_BIT_AR(af[0].bitvector, AFF_WATERWALK);
    accum_duration = TRUE;
    to_vict = "You feel webbing between your toes.";
    break;

  case SPELL_STONE_SKIN:
    af[0].location = APPLY_RESIST_PIERCE;
    af[0].modifier = 20;
    af[0].duration = level;

    af[1].location = APPLY_RESIST_SLASH;
    af[1].modifier = 20;
    af[1].duration = level;
    accum_duration = TRUE;
    to_vict = "Your skin turns hard as stone.";
    break;

  case SPELL_STEEL_SKIN:
    af[0].location = APPLY_RESIST_BLUNT;
    af[0].modifier = 20;
    af[0].duration = level;

    af[1].location = APPLY_RESIST_UNARMED;
    af[1].modifier = 20;
    af[1].duration = level;

    accum_duration = TRUE;
    to_vict = "Your skin turns hard as steel.";
    break;

  case SPELL_MAGE_ARMOR:
    af[0].location = APPLY_AC;
    af[0].modifier = -30;
    af[0].duration = level;
    accum_duration = TRUE;
    to_vict = "You are armored in robes of magic.";
    break;

  case SPELL_MIRROR_IMAGE:
    af[0].location = APPLY_MIRROR_IMAGE;
    af[0].modifier = level;
    af[0].duration = level;
    SET_BIT_AR(af[3].bitvector, AFF_INFRAVISION);
    af[0].duration = level;
    send_to_char(ch, "You are surrounded by %d mirror images of you!\n\r", level / 2);
    SET_BIT_AR(af[0].bitvector, AFF_MIRROR_IMAGES);
    break;

  case SPELL_PARALYZE:
    if (mag_savingthrow(victim, savetype, 0))
    {
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
      return;
    }

    af[0].location = APPLY_HITROLL;
    af[0].duration = level;
    af[0].modifier = -10;
    SET_BIT_AR(af[0].bitvector, AFF_PARA);

    af[1].location = APPLY_AC;
    af[1].duration = level;
    af[1].modifier = 100;
    SET_BIT_AR(af[1].bitvector, AFF_PARA);

    af[2].location = APPLY_DAMROLL;
    af[2].duration = level;
    af[2].modifier = -10;
    SET_BIT_AR(af[2].bitvector, AFF_PARA);

    accum_duration = TRUE;
    to_room = "$n's limbs stiffens as the paralysis sets in!";
    to_vict = "You feel absolutely paralyzed.";
    break;
  }

  /* If this is a mob that has this affect set in its mob file, do not perform
   * the affect.  This prevents people from un-sancting mobs by sancting them
   * and waiting for it to fade, for example. */
  if (IS_NPC(victim) && !affected_by_spell(victim, spellnum))
  {
    for (i = 0; i < MAX_SPELL_AFFECTS; i++)
    {
      for (j = 0; j < NUM_AFF_FLAGS; j++)
      {
        if (IS_SET_AR(af[i].bitvector, j) && AFF_FLAGGED(victim, j))
        {
          send_to_char(ch, "%s", CONFIG_NOEFFECT);
          return;
        }
      }
    }
  }

  /* If the victim is already affected by this spell, and the spell does not
   * have an accumulative effect, then fail the spell. */
  if (affected_by_spell(victim, spellnum) && !(accum_duration || accum_affect))
  {
    send_to_char(ch, "%s", CONFIG_NOEFFECT);
    return;
  }

  for (i = 0; i < MAX_SPELL_AFFECTS; i++)
    if (af[i].bitvector[0] || af[i].bitvector[1] ||
        af[i].bitvector[2] || af[i].bitvector[3] ||
        (af[i].location != APPLY_NONE))
      affect_join(victim, af + i, accum_duration, FALSE, accum_affect, FALSE);

  if (to_vict != NULL)
    act(to_vict, FALSE, victim, 0, ch, TO_CHAR);
  if (to_room != NULL)
    act(to_room, TRUE, victim, 0, ch, TO_ROOM);
}

/* This function is used to provide services to mag_groups.  This function is
 * the one you should change to add new group spells. */
static void perform_mag_groups(int level, struct char_data *ch,
                               struct char_data *tch, int spellnum, int savetype)
{
  switch (spellnum)
  {
  case SPELL_GROUP_HEAL:
    mag_points(level, ch, tch, SPELL_HEAL, savetype);
    break;
  case SPELL_GROUP_ARMOR:
    mag_affects(level, ch, tch, SPELL_ARMOR, savetype);
    break;
  case SPELL_GROUP_RECALL:
    spell_recall(level, ch, tch, NULL);
    break;
  case SPELL_GROUP_MIRACLE:
    mag_points(level, ch, tch, SPELL_MIRACLE, savetype);
    break;
  case SPELL_GROUP_HASTE:
    mag_affects(level, ch, tch, SPELL_HASTE, savetype);
    break;
  case SPELL_GROUP_SATIATE:
    spell_satiate(level, ch, tch, NULL);
    break;
  case SPELL_GROUP_FLY:
    mag_affects(level, ch, tch, SPELL_FLY, savetype);
    break;
  case SPELL_GROUP_BLESS:
    mag_affects(level, ch, tch, SPELL_BLESS, savetype);
    break;
  case SPELL_GROUP_SUMMON:
    spell_summon(level, ch, tch, NULL);
    break;
  case SPELL_GROUP_POWERHEAL:
    mag_points(level, ch, tch, SPELL_POWERHEAL, savetype);
    break;
  case SPELL_GROUP_STONE_SKIN:
    mag_affects(level, ch, tch, SPELL_STONE_SKIN, savetype);
    break;
  case SPELL_GROUP_STEEL_SKIN:
    mag_affects(level, ch, tch, SPELL_STEEL_SKIN, savetype);
    break;
  case SPELL_GROUP_SANCTUARY:
    mag_affects(level, ch, tch, SPELL_SANCTUARY, savetype);
    break;
  case SPELL_GROUP_REGEN:
    mag_affects(level, ch, tch, SPELL_REGENERATION, savetype);
    break;
  case SPELL_GROUP_PASS_DOOR:
    mag_affects(level, ch, tch, SPELL_PASS_DOOR, savetype);
    break;
  }
}

/* Every spell that affects the group should run through here perform_mag_groups
 * contains the switch statement to send us to the right magic. Group spells
 * affect everyone grouped with the caster who is in the room, caster last. To
 * add new group spells, you shouldn't have to change anything in mag_groups.
 * Just add a new case to perform_mag_groups. */
void mag_groups(int level, struct char_data *ch, int spellnum, int savetype)
{
  struct char_data *tch;

  if (ch == NULL)
    return;

  if (!GROUP(ch))
    return;

  while ((tch = (struct char_data *)simple_list(GROUP(ch)->members)) != NULL)
  {
    if (IN_ROOM(tch) != IN_ROOM(ch) && spellnum != SPELL_GROUP_SUMMON)
      continue;
    if (tch == ch)
      continue;
    perform_mag_groups(level, ch, tch, spellnum, savetype);
  }
  if (spellnum != SPELL_GROUP_SUMMON)
    perform_mag_groups(level, ch, ch, spellnum, savetype);
}

/* Mass spells affect every creature in the room except the caster. */
void mag_masses(int level, struct char_data *ch, int spellnum, int savetype)
{
  struct char_data *tch, *tch_next;

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch_next)
  {
    tch_next = tch->next_in_room;
    if (tch == ch)
      continue;

    switch (spellnum)
    {
    }
  }
}

/* Mass spells affect every creature in the room including the caster. */
void mag_bard(int level, struct char_data *ch, int spellnum, int savetype)
{
  struct char_data *tch, *tch_next;

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch_next)
  {
    tch_next = tch->next_in_room;

    switch (spellnum)
    {
    case SPELL_BARD_BLESS:
      mag_affects(level, ch, tch, SPELL_BARD_BLESS, savetype);
      break;
    case SPELL_BARD_BUFF:
      mag_affects(level, ch, tch, SPELL_BARD_BUFF, savetype);
      break;
    case SPELL_BARD_HEAL:
      mag_points(level, ch, tch, SPELL_BARD_HEAL, savetype);
      break;
    case SPELL_BARD_POWERHEAL:
      mag_points(level, ch, tch, SPELL_BARD_POWERHEAL, savetype);
      break;
    case SPELL_BARD_FEAST:
      spell_satiate(level, ch, tch, NULL);
      break;
    case SPELL_BARD_RECALL:
      spell_recall(level, ch, tch, NULL);
      break;
    case SPELL_BARD_DEBUFF:
      mag_affects(level, ch, tch, SPELL_BARD_DEBUFF, savetype);
      break;
    case SPELL_BARD_REGEN:
      mag_affects(level, ch, tch, SPELL_BARD_REGEN, savetype);
      break;
    case SPELL_BARD_SANC:
      mag_affects(level, ch, tch, SPELL_BARD_SANC, savetype);
      break;
    case SPELL_BARD_SONIC:
      mag_affects(level, ch, tch, SPELL_BARD_SONIC, savetype);
      break;
    case SPELL_BARD_AGILITY:
      mag_affects(level, ch, tch, SPELL_BARD_AGILITY, savetype);
      break;
    case SPELL_BARD_KNOWLEDGE:
      mag_affects(level, ch, tch, SPELL_BARD_KNOWLEDGE, savetype);
      break;
    case SPELL_BARD_VITALITY:
      mag_affects(level, ch, tch, SPELL_BARD_VITALITY, savetype);
      break;    default:
      break;
    }
  }
}

/* Every spell that affects an area (room) runs through here.  These are
 * generally offensive spells.  This calls mag_damage to do the actual damage.
 * All spells listed here must also have a case in mag_damage() in order for
 * them to work. Area spells have limited targets within the room. */
void mag_areas(int level, struct char_data *ch, int spellnum, int savetype)
{
  struct char_data *tch, *next_tch;
  const char *to_char = NULL, *to_room = NULL;

  if (ch == NULL)
    return;

  /* to add spells just add the message here plus an entry in mag_damage for
   * the damaging part of the spell.   */
  switch (spellnum)
  {
  case SPELL_EARTHQUAKE:
    to_char = "You gesture and the earth begins to shake all around you!";
    to_room = "$n gracefully gestures and the earth begins to shake violently!";
    break;
  case SPELL_MISSILE_SPRAY:
    to_char = "You gesture, a flick of the wrist, missiles fire in every direction!";
    to_room = "$n gestures and fires magic missiles across the room!";
    break;
  case SPELL_FIREBLAST:
    if (GET_POS(ch) != POS_FIGHTING)
    {
      to_char = "You gesture and flames gather around your fingertips!";
      to_room = "$n gestures and and flames gather about $s fingertips!";
    }
    else
    {
      to_char = "You gesture and flames erupt from your hands!";
      to_room = "$n gestures and and flames erupt from $s hands!";
    }
    break;
  case SPELL_SYMBOL_OF_PAIN:
    if (GET_POS(ch) == POS_FIGHTING)
    {
      to_char = "Your holy symbol radiates light in all directions!";
      to_room = "$n holds out a radiant magical holy symbol!";
    }
    else
    {
      to_char = "You brandish your Holy Symbol for all to see!";
      to_room = "$n holds out the a magical holy symbol!";
    }
    break;
  case SPELL_CHAIN_LIGHTNING:
    to_char = "Arcing bolts of lightning flare from your fingertips!";
    to_room = "Arcing bolts of lightning fly from the fingers of $n!";
    break;    
  }

  if (to_char != NULL)
    act(to_char, FALSE, ch, 0, 0, TO_CHAR);
  if (to_room != NULL)
    act(to_room, FALSE, ch, 0, 0, TO_NOTVICT);

  for (tch = world[IN_ROOM(ch)].people; tch; tch = next_tch)
  {
    next_tch = tch->next_in_room;

    /* The skips: 1: the caster
     *            2: immortals
     *            3: if no pk on this mud, skips over all players
     *            4: pets (charmed NPCs)
     *            5: other players in the same group (if the spell is 'violent')
     *            6: Flying people if earthquake is the spell                         */
    if (tch == ch)
      continue;
    if (!IS_NPC(tch) && GET_REAL_LEVEL(tch) >= LVL_IMMORT)
      continue;
    if (!CONFIG_PK_ALLOWED && !IS_NPC(ch) && !IS_NPC(tch))
      continue;
    if (!IS_NPC(ch) && IS_NPC(tch) && AFF_FLAGGED(tch, AFF_CHARM))
      continue;
    if (!IS_NPC(tch) && spell_info[spellnum].violent && GROUP(tch) && GROUP(ch) && GROUP(ch) == GROUP(tch))
      continue;
    if ((spellnum == SPELL_EARTHQUAKE) && AFF_FLAGGED(tch, AFF_FLYING))
      continue;
    /* Doesn't matter if they die here so we don't check. -gg 6/24/98 */
    mag_damage(level, ch, tch, spellnum, 1);
  }
}

/*----------------------------------------------------------------------------*/
/* Begin Magic Summoning - Generic Routines and Local Globals */
/*----------------------------------------------------------------------------*/

/* Every spell which summons/gates/conjours a mob comes through here. */
/* These use act(), don't put the \r\n. */
static const char *mag_summon_msgs[] = {
    "\r\n",
    "$n makes a strange magical gesture; you feel a strong breeze!",
    "$n animates a corpse!",
    "$N appears from a cloud of thick blue smoke!",
    "$N appears from a cloud of thick green smoke!",
    "$N appears from a cloud of thick red smoke!",
    "$N disappears in a thick black cloud!"
    "As $n makes a strange magical gesture, you feel a strong breeze.",
    "As $n makes a strange magical gesture, you feel a searing heat.",
    "As $n makes a strange magical gesture, you feel a sudden chill.",
    "As $n makes a strange magical gesture, you feel the dust swirl.",
    "$n magically divides!",
    "$n animates a corpse!"};

/* Keep the \r\n because these use send_to_char. */
static const char *mag_summon_fail_msgs[] = {
    "\r\n",
    "There are no such creatures.\r\n",
    "Uh oh...\r\n",
    "Oh dear.\r\n",
    "Gosh durnit!\r\n",
    "The elements resist!\r\n",
    "You failed.\r\n",
    "There is no corpse!\r\n"};

/* Defines for Mag_Summons */
#define MOB_CLONE 10  /**< vnum for the clone mob. */
#define OBJ_CLONE 161 /**< vnum for clone material. */
#define MOB_ZOMBIE 11 /**< vnum for the zombie mob. */
#define MOB_JUJU_ZOMBIE 1299
void mag_summons(int level, struct char_data *ch, struct obj_data *obj,
                 int spellnum, int savetype)
{
  struct char_data *mob = NULL;
  struct obj_data *tobj, *next_obj;
  int pfail = 0, msg = 0, fmsg = 0, num = 1, handle_corpse = FALSE, i;
  mob_vnum mob_num;

  if (ch == NULL)
    return;

  switch (spellnum)
  {
  case SPELL_CLONE:
    msg = 10;
    fmsg = rand_number(2, 6); /* Random fail message. */
    mob_num = MOB_CLONE;
    /*
     * We have designated the clone spell as the example for how to use the
     * mag_materials function.
     * In stock tbaMUD it checks to see if the character has item with
     * vnum 161 which is a set of sacrificial entrails. If we have the entrails
     * the spell will succeed,  and if not, the spell will fail 102% of the time
     * (prevents random success... see below).
     * The object is extracted and the generic cast messages are displayed.
     */
    if (!mag_materials(ch, OBJ_CLONE, NOTHING, NOTHING, TRUE, TRUE))
      pfail = 102; /* No materials, spell fails. */
    else
      pfail = 0; /* We have the entrails, spell is successfully cast. */
    break;

  case SPELL_ANIMATE_DEAD:
    if (obj == NULL || !IS_CORPSE(obj))
    {
      act(mag_summon_fail_msgs[7], FALSE, ch, 0, 0, TO_CHAR);
      return;
    }
    handle_corpse = TRUE;
    msg = 11;
    fmsg = rand_number(2, 6); /* Random fail message. */
    mob_num = MOB_JUJU_ZOMBIE;
    pfail = 10; /* 10% failure, should vary in the future. */
    break;

  default:
    return;
  }

  if (AFF_FLAGGED(ch, AFF_CHARM))
  {
    send_to_char(ch, "You are too giddy to have any followers!\r\n");
    return;
  }
  if (rand_number(0, 101) < pfail)
  {
    send_to_char(ch, "%s", mag_summon_fail_msgs[fmsg]);
    return;
  }
  for (i = 0; i < num; i++)
  {
    if (!(mob = read_mobile(mob_num, VIRTUAL)))
    {
      send_to_char(ch, "You don't quite remember how to make that creature.\r\n");
      return;
    }
    char_to_room(mob, IN_ROOM(ch));
    IS_CARRYING_W(mob) = 0;
    IS_CARRYING_N(mob) = 0;
    SET_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);
    if (spellnum == SPELL_CLONE)
    {
      char desc[80];
      strcpy(desc, "A Clone of ");
      strcat(desc, GET_NAME(ch));
      strcat(desc, ".\n\r");
      send_to_char(ch, "%s\n\r", desc);
      /* Don't mess up the prototype; use new string copies. */
      GET_LEVEL(mob, CLASS_FIGHTER) = GET_REAL_LEVEL(ch);
      GET_HIT(mob) = GET_HIT(ch);
      GET_MAX_HIT(mob) = GET_MAX_HIT(ch);
      mob->player.name = strdup(GET_NAME(ch));
      mob->player.short_descr = strdup(GET_NAME(ch));
      mob->player.long_descr = strdup(desc);
    }
    act(mag_summon_msgs[msg], FALSE, ch, 0, mob, TO_ROOM);
    load_mtrigger(mob);
    add_follower(mob, ch);

    if (GROUP(ch) && GROUP_LEADER(GROUP(ch)) == ch)
      join_group(mob, GROUP(ch));
  }
  if (handle_corpse)
  {
    for (tobj = obj->contains; tobj; tobj = next_obj)
    {
      next_obj = tobj->next_content;
      obj_from_obj(tobj);
      obj_to_char(tobj, mob);
    }
    extract_obj(obj);
  }
}

/* Clean up the defines used for mag_summons. */
#undef MOB_CLONE
#undef OBJ_CLONE
#undef MOB_ZOMBIE

/*----------------------------------------------------------------------------*/
/* End Magic Summoning - Generic Routines and Local Globals */
/*----------------------------------------------------------------------------*/

void mag_points(int level, struct char_data *ch, struct char_data *victim,
                int spellnum, int savetype)
{
  int healing = 0, move = 0,  skill = 0;
  int modifier = 0;
  skill = GET_SKILL(ch, spellnum);
  
 
  if (victim == NULL)
    return;

  if (AFF_FLAGGED(ch, AFF_HARMONY))
  {
    if (rand_number(1, 101) > GET_SKILL(ch, SKILL_CHANT))
    {
      check_improve(ch, SKILL_CHANT, FALSE);
    }
    else
    {
      send_to_char(ch, "Your harmonic chant warms %s's heart!\n\r", GET_NAME(victim));
      check_improve(ch, SKILL_CHANT, TRUE);
      modifier += 1;
    }
  }
  if (GET_SKILL(ch, SKILL_BEDSIDE_MANNER))
  {
    if (rand_number(1, 101) > GET_SKILL(ch, SKILL_BEDSIDE_MANNER))
    {
      check_improve(ch, SKILL_BEDSIDE_MANNER, FALSE);
    }
    else
    {
      send_to_char(ch, "Your bedside manner cheers %s up!\n\r", GET_NAME(victim));
      check_improve(ch, SKILL_BEDSIDE_MANNER, TRUE);
      modifier += 2;
    }
  }
  switch (spellnum)
  {
  case SPELL_HEAL:
    modifier = 2;
    send_to_char(victim, "You are healed!\n\r");
    break;
  case SPELL_POWERHEAL:
    modifier = 3;
    send_to_char(victim, "You are powerfully healed!\n\r");
    break;
  case SPELL_MIRACLE:
    modifier = 5;
    send_to_char(victim, "You are miraculously healed!\n\r");
    break;
  case SPELL_BARD_HEAL:
    modifier = 2;
    send_to_char(victim, "You wounds are healed!\n\r");
    break;
  case SPELL_BARD_POWERHEAL:
    modifier = 3;
    send_to_char(victim, "You wounds are powerhealed!\n\r");
    break;
  default:
    modifier = 1;
    send_to_char(victim, "Default healing spell used on you!!\n\r");
    break;
  }

  healing = (level + skill) * modifier;

  //   send_to_char(victim, "Spell level: %d\n\r", level + GET_SKILL(ch, spellnum));
  //   send_to_char(victim, "Healing was %d!\n\r", healing);
  GET_HIT(victim) = MIN(GET_MAX_HIT(victim), GET_HIT(victim) + healing);
  GET_MOVE(victim) = MIN(GET_MAX_MOVE(victim), GET_MOVE(victim) + move);
  update_pos(victim);
}

void mag_unaffects(int level, struct char_data *ch, struct char_data *victim,
                   int spellnum, int type)
{
  int spell = 0, msg_not_affected = TRUE;
  const char *to_vict = NULL, *to_room = NULL;

  if (victim == NULL)
    return;

  switch (spellnum)
  {
  case SPELL_POWERHEAL:
  case SPELL_MIRACLE:
  case SPELL_HEAL:
    /* Heal also restores health, so don't give the "no effect" message if the
     * target isn't afflicted by the 'blindness' spell. */
    msg_not_affected = FALSE;
    /* fall-through */
  case SPELL_CURE_BLIND:
    spell = SPELL_BLINDNESS;
    to_vict = "Your vision returns!";
    to_room = "There's a momentary gleam in $n's eyes.";
    break;
  case SPELL_REMOVE_POISON:
    spell = SPELL_POISON;
    to_vict = "A warm feeling runs through your body!";
    to_room = "$n looks better.";
    break;
  case SPELL_REMOVE_CURSE:
    spell = SPELL_CURSE;
    to_vict = "You don't feel so unlucky.";
    break;
  default:
    log("SYSERR: unknown spellnum %d passed to mag_unaffects.", spellnum);
    return;
  }

  if (!affected_by_spell(victim, spell))
  {
    if (msg_not_affected)
      send_to_char(ch, "%s", CONFIG_NOEFFECT);
    return;
  }

  affect_from_char(victim, spell);
  if (to_vict != NULL)
    act(to_vict, FALSE, victim, 0, ch, TO_CHAR);
  if (to_room != NULL)
    act(to_room, TRUE, victim, 0, ch, TO_ROOM);
}

void mag_alter_objs(int level, struct char_data *ch, struct obj_data *obj,
                    int spellnum, int savetype)
{
  const char *to_char = NULL, *to_room = NULL;

  if (obj == NULL)
    return;

  switch (spellnum)
  {
  case SPELL_BLESS:
    if (!OBJ_FLAGGED(obj, ITEM_BLESS) &&
        (GET_OBJ_WEIGHT(obj) <= 5 * GET_REAL_LEVEL(ch)))
    {
      SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_BLESS);
      to_char = "$p glows briefly.";
    }
    break;
  case SPELL_CURSE:
    if (!OBJ_FLAGGED(obj, ITEM_NODROP))
    {
      SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_NODROP);
      if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
        GET_OBJ_VAL(obj, 2)
      --;
      to_char = "$p briefly glows red.";
    }
    break;
  case SPELL_INVISIBLE:
    if (!OBJ_FLAGGED(obj, ITEM_NOINVIS) && !OBJ_FLAGGED(obj, ITEM_INVISIBLE))
    {
      SET_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_INVISIBLE);
      to_char = "$p vanishes.";
    }
    break;
  case SPELL_POISON:
    if (((GET_OBJ_TYPE(obj) == ITEM_DRINKCON) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOUNTAIN) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOOD)) &&
        !GET_OBJ_VAL(obj, 3))
    {
      GET_OBJ_VAL(obj, 3) = 1;
      to_char = "$p steams briefly.";
    }
    break;
  case SPELL_REMOVE_CURSE:
    if (OBJ_FLAGGED(obj, ITEM_NODROP))
    {
      REMOVE_BIT_AR(GET_OBJ_EXTRA(obj), ITEM_NODROP);
      if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
        GET_OBJ_VAL(obj, 2)
      ++;
      to_char = "$p briefly glows blue.";
    }
    break;
  case SPELL_REMOVE_POISON:
    if (((GET_OBJ_TYPE(obj) == ITEM_DRINKCON) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOUNTAIN) ||
         (GET_OBJ_TYPE(obj) == ITEM_FOOD)) &&
        GET_OBJ_VAL(obj, 3))
    {
      GET_OBJ_VAL(obj, 3) = 0;
      to_char = "$p steams briefly.";
    }
    break;
  }

  if (to_char == NULL)
    send_to_char(ch, "%s", CONFIG_NOEFFECT);
  else
    act(to_char, TRUE, ch, obj, 0, TO_CHAR);

  if (to_room != NULL)
    act(to_room, TRUE, ch, obj, 0, TO_ROOM);
  else if (to_char != NULL)
    act(to_char, TRUE, ch, obj, 0, TO_ROOM);
}

void mag_creations(int level, struct char_data *ch, int spellnum)
{
  struct obj_data *tobj;
  obj_vnum z;

  if (ch == NULL)
    return;
  /* level = MAX(MIN(level, LVL_IMPL), 1); - Hm, not used. */

  switch (spellnum)
  {
  case SPELL_CREATE_FOOD:
    z = 10;
    break;
  default:
    send_to_char(ch, "Spell unimplemented, it would seem.\r\n");
    return;
  }

  if (!(tobj = read_object(z, VIRTUAL)))
  {
    send_to_char(ch, "I seem to have goofed.\r\n");
    log("SYSERR: spell_creations, spell %d, obj %d: obj not found",
        spellnum, z);
    return;
  }
  obj_to_char(tobj, ch);
  act("$n creates $p.", FALSE, ch, tobj, 0, TO_ROOM);
  act("You create $p.", FALSE, ch, tobj, 0, TO_CHAR);
  load_otrigger(tobj);
}

void mag_rooms(int level, struct char_data *ch, int spellnum)
{
  room_rnum rnum;
  int duration = 0;
  bool failure = FALSE;
  event_id IdNum = eNULL;
  const char *msg = NULL;
  const char *room = NULL;

  rnum = IN_ROOM(ch);

  if (ROOM_FLAGGED(rnum, ROOM_NOMAGIC))
    failure = TRUE;

  switch (spellnum)
  {
  case SPELL_DARKNESS:
    IdNum = eSPL_DARKNESS;
    if (ROOM_FLAGGED(rnum, ROOM_DARK))
      failure = TRUE;

    duration = 5;
    SET_BIT_AR(ROOM_FLAGS(rnum), ROOM_DARK);

    msg = "You cast a shroud of darkness upon the area.";
    room = "$n casts a shroud of darkness upon this area.";
    break;

  case SPELL_CONSECRATION:
    IdNum = eSPL_CONSECRATE;
    if (ROOM_FLAGGED(rnum, ROOM_CONSECRATE))
      failure = TRUE;

    duration = 60 + level;
    SET_BIT_AR(ROOM_FLAGS(rnum), ROOM_CONSECRATE);

    msg = "You consecrate your area with holy water and a benediction.";
    room = "$n consecrates the the area with some prayers and chanting.";
    break;

  }

  if (failure || IdNum == eNULL)
  {
    send_to_char(ch, "You failed!\r\n");
    return;
  }

  send_to_char(ch, "%s\r\n", msg);
  act(room, FALSE, ch, 0, 0, TO_ROOM);

  NEW_EVENT(IdNum, &world[rnum], NULL, duration * PASSES_PER_SEC);
}
