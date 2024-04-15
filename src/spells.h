/**
* @file spells.h
* Constants and function prototypes for the spell system.
*
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*
* All rights reserved.  See license for complete information.
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
*/
#ifndef _SPELLS_H_
#define _SPELLS_H_

#define DEFAULT_STAFF_LVL	12
#define DEFAULT_WAND_LVL	12

#define CAST_UNDEFINED	(-1)
#define CAST_SPELL	0
#define CAST_POTION	1
#define CAST_WAND	2
#define CAST_STAFF	3
#define CAST_SCROLL	4

#define MAG_DAMAGE	(1 << 0)
#define MAG_AFFECTS	(1 << 1)
#define MAG_UNAFFECTS	(1 << 2)
#define MAG_POINTS	(1 << 3)
#define MAG_ALTER_OBJS	(1 << 4)
#define MAG_GROUPS	(1 << 5)
#define MAG_MASSES	(1 << 6)
#define MAG_AREAS	(1 << 7)
#define MAG_SUMMONS	(1 << 8)
#define MAG_CREATIONS	(1 << 9)
#define MAG_MANUAL	(1 << 10)
#define MAG_ROOMS   (1 << 11)
#define MAG_BARD    (1 << 12)


#define TYPE_UNDEFINED               (-1)
#define SPELL_RESERVED_DBC            0  /* SKILL NUMBER ZERO -- RESERVED */

/* PLAYER SPELLS -- Numbered from 1 to MAX_SPELLS */
#define SPELL_ARMOR                   1 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_TELEPORT                2 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_BLESS                   3 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_BLINDNESS               4 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_BURNING_HANDS           5 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CALL_LIGHTNING          6 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CHARM                   7 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CHILL_TOUCH             8 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CLONE                   9 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_COLOR_SPRAY            10 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CONTROL_WEATHER        11 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CREATE_FOOD            12 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CREATE_WATER           13 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CURE_BLIND             14 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CURE_CRITIC            15 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CURE_LIGHT             16 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_CURSE                  17 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DETECT_ALIGN           18 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DETECT_INVIS           19 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DETECT_MAGIC           20 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DETECT_POISON          21 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DISPEL_EVIL            22 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_EARTHQUAKE             23 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_ENCHANT_WEAPON         24 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_ENERGY_DRAIN           25 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_FIREBALL               26 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_HARM                   27 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_HEAL                   28 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_INVISIBLE              29 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_LIGHTNING_BOLT         30 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_LOCATE_OBJECT          31 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_MAGIC_MISSILE          32 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_POISON                 33 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_HOLY_WARDING           34 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_REMOVE_CURSE           35 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SANCTUARY              36 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SHOCKING_GRASP         37 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SLEEP                  38 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_STRENGTH               39 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SUMMON                 40 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_BLOOD_MANA             41 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_WORD_OF_RECALL         42 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_REMOVE_POISON          43 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_SENSE_LIFE             44 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_ANIMATE_DEAD           45 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DISPEL_GOOD            46 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_GROUP_ARMOR            47 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_GROUP_HEAL             48 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_GROUP_RECALL           49 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_INFRAVISION            50 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_WATERWALK              51 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_IDENTIFY               52 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_FLY                    53 /* Reserved Skill[] DO NOT CHANGE */
#define SPELL_DARKNESS               54
#define SPELL_RELOCATION	     			 55 // Salty
#define SPELL_FURY                   56 // Salty
#define SPELL_MIRACLE                57 // Salty
#define SPELL_GROUP_MIRACLE	     		 58 // Salty
#define SPELL_GROUP_FLY              59 // Salty
#define SPELL_GROUP_BLESS            60 // Salty
#define SPELL_GROUP_SUMMON           61 // Salty
#define SPELL_POWERHEAL 	     			 62 // Salty
#define SPELL_GROUP_POWERHEAL        63 // Salty
#define SPELL_HASTE		     					 64 // Salty
#define SPELL_GROUP_HASTE	     			 65 // Salty
#define SPELL_TRUESIGHT		     			 66 // Salty
#define SPELL_REGENERATION	     		 67 // Salty
#define SPELL_STONE_SKIN             68 // Salty
#define SPELL_STEEL_SKIN	     			 69 // Salty
#define SPELL_GAIN_ADVANTAGE				 70 // Salty
#define SPELL_QUICKCAST              71 // Salty
#define SPELL_CANCELLATION	     		 72 // Salty
#define SPELL_MISSILE_SPRAY	     		 73 // Salty
#define SPELL_MAGE_ARMOR						 74 // Salty
#define SPELL_ASTRAL_WALK 					 75 // Salty
#define SPELL_MIRROR_IMAGE					 76 // Salty
#define SPELL_SATIATE								 77 // Salty
#define SPELL_GROUP_SATIATE					 78 // Salty
#define SPELL_GROUP_SANCTUARY				 79 // Salty
#define SPELL_GROUP_STONE_SKIN			 80 // Salty
#define SPELL_GROUP_STEEL_SKIN			 81 // Salty
#define SPELL_FIREBLAST							 82
#define SPELL_CONCUSSIVE_WAVE				 83 // Salty
#define SPELL_PARALYZE							 84 // Salty
#define SPELL_BETRAYAL							 85 // Salty
#define SPELL_IMPROVED_INVIS				 86 // Salty
#define SPELL_SYMBOL_OF_PAIN  	     87
#define SPELL_NIGHTMARE 	           88
#define SPELL_GROUP_REGEN            89
#define SPELL_LOCATE_CHARACTER       90
#define SPELL_PASS_DOOR 	           91
#define SPELL_IMP_PASS_DOOR	         92
#define SPELL_GROUP_PASS_DOOR        93
#define SPELL_EVIL_WARDING           94
#define SPELL_UNARMED_BONUS          95
#define SPELL_UNARMED_DEBUFF1        96
#define SPELL_UNARMED_DEBUFF2        97
#define SPELL_SLOW                   98
#define SPELL_FIRESHIELD             99 
#define SPELL_DIVINE_INTERVENTION    100  
#define SPELL_REND                   101
#define SPELL_FEEBLE                 102
#define SPELL_RITUAL                 103
#define SPELL_CHAIN_LIGHTNING        104
#define SPELL_CONSECRATION           105
#define SPELL_ENERGIZE               106
#define SPELL_MANA_TRANSFER          107
#define SPELL_REAPPEAR               108
#define SPELL_WITHER                 109
#define SPELL_ELDRITCH_BLAST         110
#define SPELL_CALM                   111
#define SPELL_NOVA                   112

/** Total Number of defined spells */
#define NUM_SPELLS                   112
/* Insert new spells here, up to MAX_SPELLS */
#define MAX_SPELLS		    120


/**
 * Chromatic Spells
 * Salty
 * 24 SEP  2022
 *
 * red = fire /  / burn
 * blue = ice / freeze / electric
 * green = earth / nature / poison
 * black = acid / necrotic / unholy
 * white = holy / force / light
 *
 */
#define ZERO_SPELLS_CHROMA    200
#define SPELL_RED_1           201
#define SPELL_RED_2           202
#define SPELL_RED_3           203
#define SPELL_RED_4           204
#define SPELL_RED_5           205
#define SPELL_BLUE_1           206
#define SPELL_BLUE_2           207
#define SPELL_BLUE_3           208
#define SPELL_BLUE_4           208
#define SPELL_BLUE_5           209
#define SPELL_GREEN_1           210
#define SPELL_GREEN_2           211
#define SPELL_GREEN_3           212
#define SPELL_GREEN_4           213
#define SPELL_GREEN_5           214
#define SPELL_BLACK_1           215
#define SPELL_BLACK_2           216
#define SPELL_BLACK_3           217
#define SPELL_BLACK_4           218
#define SPELL_BLACK_5           219
#define SPELL_WHITE_1           220
#define SPELL_WHITE_2           221
#define SPELL_WHITE_3           222
#define SPELL_WHITE_4           223
#define SPELL_WHITE_5           224

#define NUM_SPELLS_CHROMA       25

/* PLAYER SONGS -- Numbered from MAX_SPELLS+1 to MAX_SKILLS*/
#define ZERO_SONGS            300
#define SPELL_BARD_BLESS      301
#define SPELL_BARD_RESISTS    302
#define SPELL_BARD_HEAL       303
#define SPELL_BARD_POWERHEAL  304
#define SPELL_BARD_FEAST      305
#define SPELL_BARD_UNUSED     306
#define SPELL_BARD_RECALL     307
#define SPELL_BARD_CANTICLE   308
#define SPELL_BARD_REGEN      309
#define SPELL_BARD_SANC       310
#define SPELL_BARD_FURY       311
#define SPELL_BARD_FLY        312
#define SPELL_BARD_AGILITY    313
#define SPELL_BARD_KNOWLEDGE  314
#define SPELL_BARD_VITALITY   315
#define SPELL_BARD_STORM      316
#define SPELL_BARD_HARMONY    317
#define SPELL_BARD_DISSONANCE 318
#define SPELL_BARD_WAR_DANCE  319
#define SPELL_BARD_SLOW_DANCE 320
/*  Total Number of defined songs  */
#define NUM_SONGS              20
/*  Insert new songs here, up to 400*/

/* PLAYER SKILLS - Numbered from MAX_SPELLS+1 to MAX_SKILLS */
#define ZERO_SKILLS                 400
#define SKILL_BACKSTAB              401 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_BASH                  402 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_HIDE                  403 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_KICK                  404 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_PICK_LOCK             405 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_WHIRLWIND             406 
#define SKILL_RESCUE                407 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_SNEAK                 408 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_STEAL                 409 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_TRACK                 410 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_BANDAGE               411 /* Reserved Skill[] DO NOT CHANGE */
#define SKILL_SECOND_ATTACK	    		412 // Salty
#define SKILL_THIRD_ATTACK          413 // Salty
#define SKILL_FOURTH_ATTACK         414 // Salty
#define SKILL_FIFTH_ATTACK          415 // Salty
#define SKILL_PARRY                 416 // Salty
#define SKILL_DODGE                 417 // Salty
#define SKILL_EVADE                 418 // Salty
#define SKILL_IMPROVE		    				419 // Salty
#define SKILL_CIRCLE		    				420 // Salty
#define SKILL_DOUBLE_BACKSTAB       421 // Salty
#define SKILL_ENHANCED_DAMAGE	    	422 // Salty
#define SKILL_SWITCH_TARGET					423 // Salty
#define SKILL_CRITICAL_HIT					424 // Salty
#define SKILL_DEFENSIVE_STANCE			425 // Salty
#define SKILL_OFFENSIVE_STANCE			426 // Salty
#define SKILL_DUAL_WIELD						427 // Salty
#define SKILL_SHIELD_MASTER					428 // Salty
#define SKILL_HUNT									429 // Salty
#define SKILL_STALK									430 // Salty
#define SKILL_HEALTH_RECOVERY				431 // Salty
#define SKILL_DAMAGE_REDUCTION			432 // Salty
#define SKILL_ENHANCED_PARRY				433 // Salty
#define SKILL_ASSAULT								434 // Salty
#define SKILL_HEADBUTT							435 // Salty
#define SKILL_MAGIC_RECOVERY				436
#define SKILL_TUMBLE								437
#define SKILL_TAUNT									438
#define SKILL_EXECUTE	              439
#define SKILL_RAGE                  440
#define SKILL_UNARMED_COMBAT        441
#define SKILL_DIRTY_TRICKS          442
#define SKILL_CHANT                 443
#define SKILL_BATTLE_RYTHM          444
#define SKILL_IMPALE                445
#define SKILL_REND                  446 
#define SKILL_MINCE                 447
#define SKILL_THRUST                448
#define SKILL_ACROBATICS            449
#define SKILL_TWO_HAND              450
#define SKILL_R_HOOK                451
#define SKILL_L_HOOK                452
#define SKILL_SUCKER_PUNCH          453
#define SKILL_UPPERCUT              454
#define SKILL_HAYMAKER              455
#define SKILL_CLOTHESLINE           456
#define SKILL_PILEDRVIER            457
#define SKILL_PALM_STRIKE           458
#define SKILL_BARD_SHRIEK           459
#define SKILL_BARD_RITUAL           460
#define SKILL_BARD_SCORN            461
#define SKILL_BLOODBATH             462
#define SKILL_BLOOD_FRENZY          463
#define SKILL_BEDSIDE_MANNER        464
#define SKILL_DIRT_KICK             465
#define SKILL_ADRENALINE_RUSH       466
#define SKILL_KNEE                  467
#define SKILL_CHOP                  468
#define SKILL_TRIP                  469 
#define SKILL_ROUNDHOUSE            470
#define SKILL_ELBOW                 471
#define SKILL_GARROTTE              472
#define SKILL_SPELL_TWINNING        473
#define SKILL_SPELL_TRIPLING        474
#define SKILL_SPELL_CRITICAL        475
#define SKILL_ANATOMY_LESSONS       476
#define SKILL_WEAPON_PUNCH          477
#define SKILL_SHIELD_SLAM           478
#define SKILL_TWIST_OF_FATE         479
#define SKILL_WAR_DANCE             480
#define SKILL_SLOW_DANCE            481
#define SKILL_GATEWAY               482
#define SKILL_SHIELD_BLOCK          483
#define SKILL_SPIN_KICK             484
#define SKILL_STORM_OF_STEEL        485
/*
#define SKILL_TRANCE                476  // bard
*/
#define NUM_SKILLS 		               85

/* New skills may be added here up to MAX_SKILLS (600) */

/* NON-PLAYER AND OBJECT SPELLS AND SKILLS: The practice levels for the spells
 * and skills below are _not_ recorded in the players file; therefore, the
 * intended use is for spells and skills associated with objects (such as
 * SPELL_IDENTIFY used with scrolls of identify) or non-players (such as NPC
 * only spells). */

/* To make an affect induced by dg_affect look correct on 'stat' we need to
 * define it with a 'spellname'. */
#define SPELL_DG_AFFECT              698

#define TOP_SPELL_DEFINE	     699
/* NEW NPC/OBJECT SPELLS can be inserted here up to 299 */

/* WEAPON ATTACK TYPES */
#define TYPE_HIT        700
#define TYPE_STING      701
#define TYPE_WHIP       702
#define TYPE_SLASH      703
#define TYPE_BITE       704
#define TYPE_BLUDGEON   705
#define TYPE_CRUSH      706
#define TYPE_POUND      707
#define TYPE_CLAW       708
#define TYPE_MAUL       709
#define TYPE_THRASH     710
#define TYPE_PIERCE     711
#define TYPE_BLAST		  712
#define TYPE_PUNCH		  713
#define TYPE_STAB		    714
#define TYPE_SHIELD_SLAM 715
/** The total number of attack types */
#define NUM_ATTACK_TYPES  16

/* new attack types can be added here - up to TYPE_SUFFERING */
#define TYPE_SUFFERING		     799

#define SAVING_PARA   0
#define SAVING_ROD    1
#define SAVING_PETRI  2
#define SAVING_BREATH 3
#define SAVING_SPELL  4

/***
 **Possible Targets:
 **  TAR_IGNORE    : IGNORE TARGET.
 **  TAR_CHAR_ROOM : PC/NPC in room.
 **  TAR_CHAR_WORLD: PC/NPC in world.
 **  TAR_FIGHT_SELF: If fighting, and no argument, select tar_char as self.
 **  TAR_FIGHT_VICT: If fighting, and no argument, select tar_char as victim (fighting).
 **  TAR_SELF_ONLY : If no argument, select self, if argument check that it IS self.
 **  TAR_NOT_SELF  : Target is anyone else besides self.
 **  TAR_OBJ_INV   : Object in inventory.
 **  TAR_OBJ_ROOM  : Object in room.
 **  TAR_OBJ_WORLD : Object in world.
 **  TAR_OBJ_EQUIP : Object held.
 ***/
#define TAR_IGNORE      (1 << 0)
#define TAR_CHAR_ROOM   (1 << 1)
#define TAR_CHAR_WORLD  (1 << 2)
#define TAR_FIGHT_SELF  (1 << 3)
#define TAR_FIGHT_VICT  (1 << 4)
#define TAR_SELF_ONLY   (1 << 5) /* Only a check, use with i.e. TAR_CHAR_ROOM */
#define TAR_NOT_SELF   	(1 << 6) /* Only a check, use with i.e. TAR_CHAR_ROOM */
#define TAR_OBJ_INV     (1 << 7)
#define TAR_OBJ_ROOM    (1 << 8)
#define TAR_OBJ_WORLD   (1 << 9)
#define TAR_OBJ_EQUIP	  (1 << 10)

struct spell_info_type {
   byte min_position;	/* Position for caster	 */
   int mana_min;	/* Min amount of mana used by a spell (highest lev) */
   int mana_max;	/* Max amount of mana used by a spell (lowest lev) */
   int mana_change;	/* Change in mana used by spell from lev to lev */

   int min_level[NUM_CLASSES];
   int routines;
   byte violent;
   int targets;         /* See below for use with TAR_XXX  */
   const char *name;	/* Input size not limited. Originates from string constants. */
   const char *wear_off_msg;	/* Input size not limited. Originates from string constants. */
};

/* Possible Targets:
   bit 0 : IGNORE TARGET
   bit 1 : PC/NPC in room
   bit 2 : PC/NPC in world
   bit 3 : Object held
   bit 4 : Object in inventory
   bit 5 : Object in room
   bit 6 : Object in world
   bit 7 : If fighting, and no argument, select tar_char as self
   bit 8 : If fighting, and no argument, select tar_char as victim (fighting)
   bit 9 : If no argument, select self, if argument check that it IS self. */
#define SPELL_TYPE_SPELL   0
#define SPELL_TYPE_POTION  1
#define SPELL_TYPE_WAND    2
#define SPELL_TYPE_STAFF   3
#define SPELL_TYPE_SCROLL  4

#define ASPELL(spellname) \
void	spellname(int level, struct char_data *ch, \
		  struct char_data *victim, struct obj_data *obj)

#define MANUAL_SPELL(spellname)	spellname(level, caster, cvict, ovict);

ASPELL(spell_create_water);
ASPELL(spell_recall);
ASPELL(spell_teleport);
ASPELL(spell_summon);
ASPELL(spell_locate_object);
ASPELL(spell_charm);
ASPELL(spell_information);
ASPELL(spell_identify);
ASPELL(spell_enchant_weapon);
ASPELL(spell_detect_poison);
ASPELL(spell_relocate);
ASPELL(spell_cancellation);
ASPELL(spell_astral_walk);
ASPELL(spell_satiate);
ASPELL(spell_blood_mana);
ASPELL(spell_locate_char);
ASPELL(spell_mana_transfer);
ASPELL(spell_energize);
ASPELL(spell_astral_walk);
/* basic magic calling functions */



int find_skill_num(char *name);

int mag_damage(int level, struct char_data *ch, struct char_data *victim,
  int spellnum, int savetype);

void mag_affects(int level, struct char_data *ch, struct char_data *victim,
  int spellnum, int savetype);

void mag_groups(int level, struct char_data *ch, int spellnum, int savetype);

void mag_masses(int level, struct char_data *ch, int spellnum, int savetype);
void mag_bard(int level, struct char_data *ch, int spellnum, int savetype);

void mag_areas(int level, struct char_data *ch, int spellnum, int savetype);

void mag_rooms(int level, struct char_data *ch, int spellnum);

void mag_summons(int level, struct char_data *ch, struct obj_data *obj,
 int spellnum, int savetype);

void mag_points(int level, struct char_data *ch, struct char_data *victim,
 int spellnum, int savetype);

void mag_unaffects(int level, struct char_data *ch, struct char_data *victim,
  int spellnum, int type);

void mag_alter_objs(int level, struct char_data *ch, struct obj_data *obj,
  int spellnum, int type);

void mag_creations(int level, struct char_data *ch, int spellnum);

int	call_magic(struct char_data *caster, struct char_data *cvict,
  struct obj_data *ovict, int spellnum, int level, int casttype);

void	mag_objectmagic(struct char_data *ch, struct obj_data *obj,
			char *argument);

int	cast_spell(struct char_data *ch, struct char_data *tch,
  struct obj_data *tobj, int spellnum);

/* other prototypes */
void spell_level(int spell, int chclass, int level);
void init_spell_levels(void);
void init_spell_ranks(void);
void spell_ranks(int spell, int rank);

/* Class init function prototypes*/
void init_wizard(void);
void init_priest(void);
void init_rogue(void);
void init_fighter(void);
void init_knight(void);
void init_bard(void);

const char *skill_name(int num);

/* From magic.c */
int mag_savingthrow(struct char_data *victim, struct char_data *caster, int type, int spellnum, int modifier);
void affect_update(void);

/* from spell_parser.c */
ACMD(do_cast);
ACMD(do_sing);
void unused_spell(int spl);
void mag_assign_spells(void);




/* Global variables */
extern struct spell_info_type spell_info[];
extern char cast_arg2[];
extern const char *unused_spellname;

#endif /* _SPELLS_H_ */

