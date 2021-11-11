/**
* @file spec_procs.h
* Header file for special procedure modules. This file groups a lot of the
* legacy special procedures found in spec_procs.c and castle.c.
* 
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*                                                                        
* All rights reserved.  See license for complete information.                                                                
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University 
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
* 
*/
#ifndef _SPEC_PROCS_H_
#define _SPEC_PROCS_H_

/*****************************************************************************
 * Begin Functions and defines for castle.c 
 ****************************************************************************/
void assign_kings_castle(void);

/*****************************************************************************
 * Begin Functions and defines for spec_assign.c 
 ****************************************************************************/
void assign_mobiles(void);
void assign_objects(void);
void assign_rooms(void);

#include "structs.h"
const char *get_spec_func_name(SPECIAL(*func));

/*****************************************************************************
 * Begin Functions and defines for spec_procs.c 
 ****************************************************************************/
/* Utility functions */
void sort_spells(void);
void list_skills(struct char_data *ch);
void list_meta(struct char_data *ch);
void list_train(struct char_data *ch);

/* Special functions */
SPECIAL(guild);
SPECIAL(dump);
SPECIAL(mayor);
SPECIAL(snake);
SPECIAL(thief);
SPECIAL(magic_user);
SPECIAL(guild_guard);
SPECIAL(puff);
SPECIAL(fido);
SPECIAL(janitor);
SPECIAL(cityguard);
SPECIAL(pet_shops);
SPECIAL(bank);


/*
Here we change from a static practice maximum to a skill-based maximum

Salty
05 JAN 2019
*/

#define LEARNED_LEVEL 0       /* % known which is considered "learned" */
#define MAX_PER_PRAC  1       /* max percent gain in skill per practice */
#define MIN_PER_PRAC  2       /* min percent gain in skill per practice */
#define PRAC_TYPE     3       /* should it say 'spell' or 'skill'?     */

//#define LEARNED(ch) (main_stat(ch) * 4)
#define MINGAIN(ch) (prac_params[MIN_PER_PRAC][(int)GET_CLASS(ch)])
#define MAXGAIN(ch) (prac_params[MAX_PER_PRAC][(int)GET_CLASS(ch)])
#define SPLSKL(ch) (prac_types[prac_params[PRAC_TYPE][(int)GET_CLASS(ch)]])
#define LEARNED(ch) (prac_params[LEARNED_LEVEL][(int)GET_CLASS(ch)])
#define MAX_SKILL(ch) (LEARNED(ch) > int_app[GET_INT(ch)].learn ? LEARNED(ch) : int_app[GET_INT(ch)].learn)

#endif /* _SPEC_PROCS_H_ */

ACMD(do_train);
