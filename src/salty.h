/*
Header file for salty.c

Salty
20 FEB 2024

*/
#include "utils.h" /* for the ACMD macro */

#define ROOM_OBJ_VOID 1299

#define MAX_STAT 25
#define MAX_STR 25
#define MAX_STR_ADD 100
#define MAX_INT 25
#define MAX_WIS 25
#define MAX_DEX 25
#define MAX_CON 25
#define MAX_CHA 25

#define MAX_RESIST 100
#define MAX_TRAIN 100
#define MAX_COMBAT_POWER 200
// Function Prototypes for salty.c

const char* rank_name(struct char_data *ch);
bool rank_display(struct char_data *ch, char *arg);

void list_meta(struct char_data *ch);
void list_trian(struct char_data *ch);
void check_improve(struct char_data *ch, int sn, bool success);
void check_unarmed_combat(struct char_data *ch, struct char_data *victim);
void headbutt_combat(struct char_data *ch, struct char_data *victim);
void kick_combat(struct char_data *ch, struct char_data *victim);
void tumble_combat(struct char_data *ch, struct char_data *victim);
void circle_combat(struct char_data *ch, struct char_data *victim);
void bash_combat(struct char_data *ch, struct char_data *victim);
void check_dirty_tricks(struct char_data *ch, struct char_data *victim);
void impale_combat(struct char_data *ch, struct char_data *victim);
void rend_combat(struct char_data *ch, struct char_data *victim);
void mince_combat(struct char_data *ch, struct char_data *victim);
void thrust_combat(struct char_data *ch, struct char_data *victim);
void check_acrobatics(struct char_data *ch, struct char_data *victim);
void right_hook_combat(struct char_data *ch, struct char_data *victim);
void left_hook_combat(struct char_data *ch, struct char_data *victim);
void sucker_punch_combat(struct char_data *ch, struct char_data *victim);
void uppercut_combat(struct char_data *ch, struct char_data *victim);
void haymaker_combat(struct char_data *ch, struct char_data *victim);
void clothesline_combat(struct char_data *ch, struct char_data *victim);
void piledriver_combat(struct char_data *ch, struct char_data *victim);
void palm_strike_combat(struct char_data *ch, struct char_data *victim);
void chop_combat(struct char_data *ch, struct char_data *victim);
void roundhouse_combat(struct char_data *ch, struct char_data *victim);
void trip_combat(struct char_data *ch, struct char_data *victim);
void knee_combat(struct char_data *ch, struct char_data *victim);
void elbow_combat(struct char_data *ch, struct char_data *victim);
void dirtkick_combat(struct char_data *ch, struct char_data *victim);
void weapon_punch_combat(struct char_data *ch, struct char_data *victim);
void shield_slam_combat(struct char_data *ch, struct char_data *victim);
void knight_combat_update(struct char_data *ch, struct char_data *victim);
void spinkick_combat(struct char_data *ch);
void rampage_combat(struct char_data *ch);
void check_rescue(struct char_data *ch, struct char_data *target);
void check_disembowel(struct char_data *ch, struct char_data *target);
void check_rend(struct char_data *ch, struct char_data *victim);
void check_feeble(struct char_data *ch, struct char_data *victim);

int get_max_train(struct char_data *ch);
int spell_type(struct char_data *ch, int type);
int skill_mult(struct char_data *ch, int skillnum, int dam, int level);
int circle_mult(int level);
int whirlwind_mult(int level);
int bash_mult(int level);
int unarmed_combat_dam(struct char_data *ch, int dam, int skill);
int check_rogue_critical(struct char_data *ch);
int check_block(struct char_data *ch, struct char_data *victim);
int rank_level(long total);
long rank_remaining(long total);

ACMD(do_listrank);
ACMD(do_metagame);
ACMD(do_train);
ACMD(do_stalk);
ACMD(do_circle);
ACMD(do_backstab);
ACMD(do_multiclass);
ACMD(do_stance);
ACMD(do_assault);
ACMD(do_headbutt);
ACMD(do_tumble);
ACMD(do_taunt);
ACMD(do_rage);
ACMD(do_kick);
ACMD(do_bash);
ACMD(do_chant);
ACMD(do_testcmd);
ACMD(do_shriek);
ACMD(do_ritual);
ACMD(do_dirt_kick);
ACMD(do_garrotte);
ACMD(do_recover);
ACMD(do_dance);
ACMD(do_gateway);
ACMD(do_spin_kick);
ACMD(do_rampage);
ACMD(do_rescue);
ACMD(do_silent_rescue);
ACMD(do_teleport);
ACMD(do_testgroup);

#define NAME_RANK_1(ch)	(GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Sir":"Dame") : "Dame")
#define NAME_RANK_2(ch)	(GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Lord":"Lady") :"Lady")
#define NAME_RANK_3(ch)	(GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Baron":"Baroness") :"Baroness")
#define NAME_RANK_4(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Count":"Countess") :"Countess")
#define NAME_RANK_5(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Marquis":"Marquise") :"Marquise")
#define NAME_RANK_6(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Duke":"Duchess") :"Duchess")
#define NAME_RANK_7(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Prince":"Princess") :"Princess")
#define NAME_RANK_8(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Arch Duke":"Arch Duchess") :"Arch Duchess")
#define NAME_RANK_9(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "King":"Queen") :"Queen")
#define NAME_RANK_10(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "Emperor":"Empress") :"Empress")
