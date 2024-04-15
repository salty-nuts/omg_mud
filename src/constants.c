/**
* @file constants.c
* Numeric and string contants used by the MUD.
*
* Part of the core tbaMUD source code distribution, which is a derivative
* of, and continuation of, CircleMUD.
*
* All rights reserved.  See license for complete information.
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
*
* @todo Come up with a standard for descriptive arrays. Either all end with
* newlines or all of them don not.
*/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "interpreter.h"	/* alias_data */
#include "constants.h"

/** Current tbaMUD version.
 * @todo cpp_extern isn't needed here (or anywhere) as the extern reserved word
 * works correctly with C compilers (at least in my Experience)
 * Jeremy Osborne 1/28/2008 */
cpp_extern const char *tbamud_version = "tbaMUD 2018.1";

/* strings corresponding to ordinals/bitvectors in structs.h */
/* (Note: strings for class definitions in class.c instead of here) */

/** Description of cardinal directions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *dirs[] =
{
  "north",
  "east",
  "south",
  "west",
  "up",
  "down",
  "northwest", /* Diagonals only used if CONFIG_DIAGONAL_DIRS is set */
  "northeast",
  "southeast",
  "southwest",
  "\n"
};

const char *autoexits[] =
{
  "n",
  "e",
  "s",
  "w",
  "u",
  "d",
  "nw",
  "ne",
  "se",
  "sw",
  "\n"
};

/** Room flag descriptions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *room_bits[] = {
  "DARK",	//0
  "DEATH",
  "NO_MOB",
  "INDOORS",
  "PEACEFUL",
  "SOUNDPROOF",	//5
  "NO_TRACK",
  "NO_MAGIC",
  "TUNNEL",
  "PRIVATE",
  "GODROOM",	//10
  "HOUSE",
  "HCRSH",
  "ATRIUM",
  "OLC",
  "*",		//15		/* The BFS Mark. */
  "WORLDMAP",
  "DARK",
  "NO_RECALL",
  "NO_TELEPORT",
  "NO_SUMMON",	//20
  "CONFUSING",
  "REGEN",
  "CONSECRATE",
  "\n"
};

/** Room flag descriptions. (ZONE_x)
 * @pre Must be in the same order as the defines in structs.h.
 * Must end array with a single newline. */
const char *zone_bits[] = {
  "CLOSED",
  "NO_IMMORT",
  "QUEST",
  "GRID",
  "NOBUILD",
  "!ASTRAL",
  "WORLDMAP",
  "CONFUSING",
  "NEWBIE",
  "\n"
};

/** Exit bits for doors.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *exit_bits[] = {
  "DOOR",
  "CLOSED",
  "LOCKED",
  "PICKPROOF",
  "\n"
};

/** Description of the room sector type.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *sector_types[] = {
  "Inside",
  "City",
  "Field",
  "Forest",
  "Hills",
  "Mountains",
  "Water (Swim)",
  "Water (No Swim)",
  "In Flight",
  "Underwater",
  "Desert",
  "\n"
};

/** PC and NPC sex.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *genders[] =
{
  "neutral",
  "male",
  "female",
  "\n"
};

/** Character positions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *position_types[] = {
  "Dead",
  "Mortally wounded",
  "Incapacitated",
  "Stunned",
  "Sleeping",
  "Resting",
  "Sitting",
  "Fighting",
  "Standing",
  "\n"
};

/** Player flags.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *player_bits[] = {
  "KILLER",
  "THIEF",
  "FROZEN",
  "DONTSET",
  "WRITING",
  "MAILING",
  "CSH",
  "SITEOK",
  "NOSHOUT",
  "NOTITLE",
  "DELETED",
  "LOADRM",
  "NO_WIZL",
  "NO_DEL",
  "INVST",
  "CRYO",
  "DEAD",    /* You should never see this flag on a character in game. */
  "IBT_BUG",
  "IBT_IDEA",
  "IBT_TYPO",
  "MULTICLASS",
  "UNUSED2",
  "UNUSED3",
  "UNUSED4",
  "UNUSED5",
  "\n"
};

/** Mob action flags.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *action_bits[] = {
  "SPEC",
  "SENTINEL",
  "SCAVENGER",
  "ISNPC",
  "AWARE",
  "AGGR",
  "STAY-ZONE",
  "WIMPY",
  "AGGR_EVIL",
  "AGGR_GOOD",
  "AGGR_NEUTRAL",
  "MEMORY",
  "HELPER",
  "NO_CHARM",
  "NO_SUMMN",
  "NO_SLEEP",
  "NO_BASH",
  "NO_BLIND",
  "NO_KILL",
  "DEAD",    /* You should never see this. */
  "HITS_ATTACKERS",
  "HITS_ROOM",
  "HITS_GROUP",
	"DODGE",
	"PARRY",
  "\n"
};

/** PC Preference flags.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *preference_bits[] = {
  "BRIEF",
  "COMPACT",
  "NO_SHOUT",
  "NO_TELL",
  "D_HP",
  "D_MANA",
  "D_MOVE",
  "AUTOEX",
  "NO_HASS",
  "QUEST",
  "SUMN",
  "NO_REP",
  "LIGHT",
  "C1",
  "C2",
  "NO_WIZ",
  "L1",
  "L2",
  "NO_AUC",
  "NO_GOS",
  "NO_GTZ",
  "RMFLG",
  "D_AUTO",
  "CLS",
  "BLDWLK",
  "AFK",
  "AUTOLOOT",
  "AUTOGOLD",
  "AUTOSPLIT",
  "AUTOSAC",
  "AUTOASSIST",
  "AUTOMAP",
  "AUTOKEY",
  "AUTODOOR",
  "ZONERESETS",
  "D_MAXHP",
  "D_MAXMANA",
  "D_MAXMOVE",
  "D_TANK",
  "D_VICT",
  "D_NAME",
  "AUTOEXIT_L",
  "D_SPELLS",
  "\n"
};

/** Affected bits.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *affected_bits[] =
{
  "\0", /* DO NOT REMOVE!! */
  "BLIND",
  "INVIS",
  "DET-ALIGN",
  "DET-INVIS",
  "DET-MAGIC", //5
  "SENSE-LIFE",
  "WATWALK",
  "SANCT",
  "DONT USE",
  "CURSE", //10
  "INFRA",
  "POISON",
  "HOLY-WARD",
  "EVIL-WARD",
  "SLEEP", //15
  "NO_TRACK",
  "FLY",
  "PASS-DOOR",
  "SNEAK",
  "HIDE", //20
  "IMP-PASS-DOOR",
  "CHARM",
  "FURY",
  "OFFENSE", 
  "DEFENSE",//25
  "HASTE",
  "REGEN",
  "QUICKCAST",
  "PARALYZE",
  "MIRROR_IMAGES", //30
  "RAGE",
  "IMPROVED_INVIS",
  "HARMONY",
  "DISSONANCE",
  "SLOW",  //35
  "RITUAL", // 
  "WARDANCE",  // war dance skill
  "SLOWDANCE", // slow dance skill
  "WITHER", // 39
  "\n"
};

/** Connection type descriptions.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *connected_types[] = {
  "Playing",
  "Disconnecting",
  "Get name",
  "Confirm name",
  "Get password",
  "Get new PW",
  "Confirm new PW",
  "Select sex",
  "Select class",
  "Reading MOTD",
  "Main Menu",
  "Get descript.",
  "Changing PW 1",
  "Changing PW 2",
  "Changing PW 3",
  "Self-Delete 1",
  "Self-Delete 2",
  "Disconnecting",
  "Object edit",
  "Room edit",
  "Zone edit",
  "Mobile edit",
  "Shop edit",
  "Text edit",
  "Config edit",
  "Social edit",
  "Trigger edit",
  "Help edit",
  "Quest edit",
  "Preference edit",
  "IBT edit",
  "Message edit",
  "Protocol Detection",
  "\n"
};

/** Describes the position in the equipment listing.
 * @pre Must be in the same order as the defines.
 * Not used in sprinttype() so no \\n. */
const char *wear_where[] = {
  "[Light   ] ",
  "[Finger  ] ",
  "[Finger  ] ",
  "[Neck    ] ",
  "[Neck    ] ",
  "[Body    ] ",
  "[Head    ] ",
  "[Legs    ] ",
  "[Feet    ] ",
  "[Hands   ] ",
  "[Arms    ] ",
  "[Shield  ] ",
  "[Cloak   ] ",
  "[Belt    ] ",
  "[Wrist   ] ",
  "[Wrist   ] ",
  "[Wielded ] ",
  "[Off-hand] ",
  "[Held    ] ",
  "[Floating] ",
  "[Face    ] "
};

/* Describes where an item can be worn.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *equipment_types[] = {
  "Used as light",
  "Worn on right finger",
  "Worn on left finger",
  "First worn around Neck",
  "Second worn around Neck",
  "Worn on body",
  "Worn on head",
  "Worn on legs",
  "Worn on feet",
  "Worn on hands",
  "Worn on arms",
  "Worn as shield",
  "Worn about body",
  "Worn around waist",
  "Worn around right wrist",
  "Worn around left wrist",
  "Wielded",
  "Off-hand",
  "Held",
  "Floating nearby",
  "Worn on Face",
  "\n"
};

/** Describes the type of object.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *item_types[] = {
  "UNDEFINED",
  "LIGHT",
  "SCROLL",
  "WAND",
  "STAFF",
  "WEAPON",
  "FURNITURE",
  "QUEST_TOKEN",
  "TREASURE",
  "ARMOR",
  "POTION",
  "WORN",
  "OTHER",
  "TRASH",
  "BARD",
  "CONTAINER",
  "NOTE",
  "LIQ CONTAINER",
  "KEY",
  "FOOD",
  "MONEY",
  "PEN",
  "BOAT",
  "FOUNTAIN",
  "\n"
};

/** Describes the wear flags set on an item.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *wear_bits[] = {
  "TAKE",
  "FINGER",
  "NECK",
  "BODY",
  "HEAD",
  "LEGS",
  "FEET",
  "HANDS",
  "ARMS",
  "SHIELD",
  "ABOUT",
  "WAIST",
  "WRIST",
  "WIELD",
	"OFF_HAND",
  "HOLD",
  "FLOATING",
  "FACE",
  "\n"
};

/** Describes the extra flags applied to an item.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *extra_bits[] = {
  "GLOW",
  "HUM",
  "NO_RENT",
  "NO_DONATE",
  "NO_INVIS",
  "INVISIBLE",
  "MAGIC",
  "NO_DROP",
  "BLESS",
  "ANTI_GOOD",
  "ANTI_EVIL",
  "ANTI_NEUTRAL",
  "ANTI_WIZARD",
  "ANTI_PRIEST",
  "ANTI_ROGUE",
  "ANTI_FIGHTER",
  "ANTI_KNIGHT",
  "ANTI_BARD",
  "NO_SELL",
  "QUEST_ITEM",
  "IMPROVED",
	"TWO_HANDED",
  "DECAYING",
  "\n"
};

/** Describes the apply types.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *apply_types[] = {
  "NONE",
  "STR",
  "DEX",
  "INT",
  "WIS",
  "CON",
  "LUCK",
  "CLASS",
  "LEVEL",
  "AGE",
  "CHAR_WEIGHT",
  "CHAR_HEIGHT",
  "MAXMANA",
  "MAXHIT",
  "MAXMOVE",
  "GOLD",
  "EXP",
  "ARMOR",
  "HITROLL",
  "DAMROLL",
  "SAVING_PARA",
  "SAVING_ROD",
  "SAVING_PETRI",
  "SAVING_BREATH",
  "SAVING_SPELL",
	"HITP_GAIN",
	"MANA_GAIN",
	"MOVE_GAIN",
	"SPELLS_HEALING",
	"SPELLS_DAMAGE",
	"SPELLS_AFFECTS",
	"MIRROR_IMAGES",
	"RESIST_UNARMED",
	"RESIST_EXOITC",
	"RESIST_BLUNT",
	"RESIST_PIERCE",
	"RESIST_SLASH",
  "\n"
};

/** Describes the closure mechanism for a container.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *container_bits[] = {
  "CLOSEABLE",
  "PICKPROOF",
  "CLOSED",
  "LOCKED",
  "\n",
};

/** Describes the liquid description.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *drinks[] =
{
  "water",
  "beer",
  "wine",
  "ale",
  "dark ale",
  "whisky",
  "lemonade",
  "firebreather",
  "local speciality",
  "slime mold juice",
  "milk",
  "tea",
  "coffee",
  "blood",
  "salt water",
  "clear water",
  "\n"
};

/** Describes a one word alias for each type of liquid.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *drinknames[] =
{
  "water",
  "beer",
  "wine",
  "ale",
  "ale",
  "whisky",
  "lemonade",
  "firebreather",
  "local",
  "juice",
  "milk",
  "tea",
  "coffee",
  "blood",
  "salt",
  "water",
  "\n"
};

/** Define the effect of liquids on hunger, thirst, and drunkenness, in that
 * order. See values.doc for more information.
 * @pre Must be in the same order as the defines. */
int drink_aff[][3] = {
  {0, 1, 10},
  {3, 2, 5},
  {5, 2, 5},
  {2, 2, 5},
  {1, 2, 5},
  {6, 1, 4},
  {0, 1, 8},
  {10, 0, 0},
  {3, 3, 3},
  {0, 4, -8},
  {0, 3, 6},
  {0, 1, 6},
  {0, 1, 6},
  {0, 2, -1},
  {0, 1, -2},
  {0, 0, 13}
};

/** Describes the color of the various drinks.
 * @pre Must be in the same order as the defines.
 * Must end array with a single newline. */
const char *color_liquid[] =
{
  "clear",
  "brown",
  "clear",
  "brown",
  "dark",
  "golden",
  "red",
  "green",
  "clear",
  "light green",
  "white",
  "brown",
  "black",
  "red",
  "clear",
  "crystal clear",
  "\n"
};

/** Used to describe the level of fullness of a drink container. Not used in
 * sprinttype() so no \\n. */
const char *fullness[] =
{
  "less than half ",
  "about half ",
  "more than half ",
  ""
};

/* Strength attribute affects.
   The fields are
   +hitroll,
   +damroll,
   weight carried mod,
   and max weapon weight */
cpp_extern const struct str_app_type str_app[] = {
  {-5, -4, 0, 0},	/* str = 0 */
  {-5, -4, 3, 1},	/* str = 1 */
  {-3, -2, 3, 2},
  {-3, -1, 10, 3},
  {-2, -1, 25, 4},
  {-2, -1, 55, 5},	/* str = 5 */
  {-1, 0, 80, 6},
  {-1, 0, 90, 7},
  {0, 0, 100, 8},
  {0, 0, 100, 9},
  {0, 0, 115, 10},	/* str = 10 */
  {0, 0, 115, 11},
  {0, 0, 140, 12},
  {0, 0, 140, 13},
  {0, 0, 170, 14},
  {0, 0, 170, 15},	/* str = 15 */
  {0, 1, 195, 16},
  {1, 1, 220, 18},
  {2, 2, 255, 20},	/* str = 18 */
  {3, 3, 640, 40},
  {4, 4, 700, 40},	/* str = 20 */
  {5, 5, 810, 42},      // Stength Additions by Salty, 05 JAN 2019
  {6, 6, 970, 44},
  {7, 7, 1130, 46},
  {8, 8, 1440, 48},
  {9, 9, 1750, 50},	/* str = 25 */
  {10, 10, 1850, 55},	/* str = 25/0 - 50 */
  {11, 11, 2000, 60},	/* str = 25/51 - 75 */
  {12, 12, 2250, 70},	/* str = 25/76 - 90 */
  {13, 13, 2500, 80},	/* str = 25/91 - 99 */
  {14, 14, 3000, 99}	/* str = 25/100 */
};

/** Dexterity skill modifiers for thieves.
 * The fields are for pick pockets, pick locks, find traps, sneak and hide. */
cpp_extern const struct dex_skill_type dex_app_skill[] = {
  {-99, -99, -90, -99, -60},	/* dex = 0 */
  {-90, -90, -60, -90, -50},	/* dex = 1 */
  {-80, -80, -40, -80, -45},
  {-70, -70, -30, -70, -40},
  {-60, -60, -30, -60, -35},
  {-50, -50, -20, -50, -30},	/* dex = 5 */
  {-40, -40, -20, -40, -25},
  {-30, -30, -15, -30, -20},
  {-20, -20, -15, -20, -15},
  {-15, -10, -10, -20, -10},
  {-10, -5, -10, -15, -5},	/* dex = 10 */
  {-5, 0, -5, -10, 0},
  {0, 0, 0, -5, 0},
  {0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0},		/* dex = 15 */
  {0, 5, 0, 0, 0},
  {5, 10, 0, 5, 5},
  {10, 15, 5, 10, 10},		/* dex = 18 */
  {15, 20, 10, 15, 15},
  {15, 20, 10, 15, 15},		/* dex = 20 */
  {20, 25, 10, 15, 20},
  {20, 25, 15, 20, 20},
  {25, 25, 15, 20, 20},
  {25, 30, 15, 25, 25},
  {25, 30, 15, 25, 25}		/* dex = 25 */
};

/** Dexterity attribute affects.
 * The fields are reaction, missile attacks, and defensive (armor class). */
cpp_extern const struct dex_app_type dex_app[] = {
  {-7, -7, 6},		/* dex = 0 */
  {-6, -6, 5},		/* dex = 1 */
  {-4, -4, 5},
  {-3, -3, 4},
  {-2, -2, 3},
  {-1, -1, 2},		/* dex = 5 */
  {0, 0, 1},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},		/* dex = 10 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, -1},		/* dex = 15 */
  {1, 1, -1},
  {2, 2, -2},
  {2, 2, -3},		/* dex = 18 */
  {3, 3, -4},
  {3, 3, -5},		/* dex = 20 */
  {4, 4, -6},
  {4, 4, -7},
  {4, 4, -8},
  {5, 5, -9},
  {5, 5, -10}		/* dex = 25 */
};

/** Constitution attribute affects.
 * The first field for hitpoint bonus on level and regen bonus.
 * The second field is for metagame bonus.
 * */
cpp_extern const struct con_app_type con_app[] = {
  {5, 0},		/* con = 0 */
  {5, 0},		/* con = 1 */
  {5, 0},
  {5, 0},
  {5, 0},
  {5, 1},		/* con = 5 */
  {6, 1},
  {7, 1},
  {8, 1},
  {9, 1},
  {10, 2},		/* con = 10 */
  {11, 2},
  {12, 2},
  {13, 3},
  {14, 3},
  {15, 4},		/* con = 15 */
  {16, 4},
  {17, 4},
  {18, 5},		/* con = 18 */
  {19, 6},
  {20, 7},		/* con = 20 */
  {21, 8},
  {22, 9},
  {23, 10},
  {24, 12},
  {25, 15}		/* con = 25 */
};

/** Intelligence attribute affects.
 * The field shows how much practicing affects a skill/spell. 
 * The second field shows how much mana INT casters gain per level and regen bonus
 * The third field is the max mana INT casters get per meta AND max trains per level
 * */

cpp_extern const struct int_app_type int_app[] = {
  {0,	5, 0},		/* int = 0 */
  {4,	5, 0},		/* int = 1 */
  {8,	5, 0},
  {12,	5, 0},
  {16,	5, 0},
  {20,	5, 1},		/* int = 5 */
  {24,	6, 1},
  {28,	7, 1},
  {32,	8, 1},
  {36,	9, 1},
  {40,	10, 2},		/* int = 10 */
  {44,	11, 2},
  {48,	12, 2},
  {52,	13, 3},
  {56,	14, 3},
  {60,	15, 4},		/* int = 15 */
  {64,	16, 4},
  {68,	17, 4},
  {72,	18, 5},		/* int = 18 */
  {76,	19, 6},
  {80,	20, 7},		/* int = 20 */
  {84,	21, 8},
  {88,	22, 9},
  {92,	23, 10},
  {96,	24, 12},
  {100, 25, 15}		/* int = 25 */
};

/** Wisdom attribute affects.
 * The first field represents how many extra practice points are gained per level. 
 * The second field shows how much mana WIS casters gain per level and regen bonus.
 * The third field is the max mana WIS casters get per meta
 * */
cpp_extern const struct wis_app_type wis_app[] = {
  {1,	5, 0},	/* wis = 0 */
  {1, 5, 0},  /* wis = 1 */
  {1, 5, 0},
  {1,	5, 0},
  {1,	5, 0},
  {2,	5, 1},  /* wis = 5 */
  {2,	6, 1},
  {2,	7, 1},
  {2,	8, 1},
  {2,	9, 1},
  {3,	10, 2},  /* wis = 10 */
  {3,	11, 2},
  {3,	12, 2},
  {3,	13, 3},
  {3,	14, 3},
  {4,	15, 4},  /* wis = 15 */
  {4,	16, 4},
  {4,	17, 4},
  {5,	18, 5},	/* wis = 18 */
  {5,	19, 6},
  {6,	20, 7},  /* wis = 20 */
  {6,	21, 8},
  {6,	22, 9},
  {6,	23, 10},
  {6,	24, 12},
  {7,	25, 15}  /* wis = 25 */
};
/** Define a set of opposite directions from the cardinal directions. */
int rev_dir[] =
{
  SOUTH,
  WEST,
  NORTH,
  EAST,
  DOWN,
  UP,
  SOUTHEAST,
  SOUTHWEST,
  NORTHWEST,
  NORTHEAST
};

/** How much movement is lost moving through a particular sector type. */
int movement_loss[] =
{
  1,	/* Inside     */
  1,	/* City       */
  2,	/* Field      */
  3,	/* Forest     */
  4,	/* Hills      */
  6,	/* Mountains  */
  4,	/* Swimming   */
  1,	/* Unswimable */
  1,	/* Flying     */
  5   /* Underwater */
};

/** The names of the days of the mud week. Not used in sprinttype(). */
const char *weekdays[] = {
  "the Day of the Moon",
  "the Day of the Bull",
  "the Day of the Deception",
  "the Day of Thunder",
  "the Day of Freedom",
  "the Day of the Great Gods",
  "the Day of the Sun"
};

/** The names of the mud months. Not used in sprinttype(). */
const char *month_name[] = {
  "Month of Winter",		/* 0 */
  "Month of the Winter Wolf",
  "Month of the Frost Giant",
  "Month of the Old Forces",
  "Month of the Grand Struggle",
  "Month of the Spring",
  "Month of Nature",
  "Month of Futility",
  "Month of the Dragon",
  "Month of the Sun",
  "Month of the Heat",
  "Month of the Battle",
  "Month of the Dark Shades",
  "Month of the Shadows",
  "Month of the Long Shadows",
  "Month of the Ancient Darkness",
  "Month of the Great Evil"
};

/** Names for mob trigger types. */
const char *trig_types[] = {
  "Global",
  "Random",
  "Command",
  "Speech",
  "Act",
  "Death",
  "Greet",
  "Greet-All",
  "Entry",
  "Receive",
  "Fight",
  "HitPrcnt",
  "Bribe",
  "Load",
  "Memory",
  "Cast",
  "Leave",
  "Door",
  "UNUSED",
  "Time",
  "\n"
};

/** Names for object trigger types. */
const char *otrig_types[] = {
  "Global",
  "Random",
  "Command",
  "UNUSED1",
  "UNUSED2",
  "Timer",
  "Get",
  "Drop",
  "Give",
  "Wear",
  "UNUSED3",
  "Remove",
  "UNUSED4",
  "Load",
  "UNUSED5",
  "Cast",
  "Leave",
  "UNUSED6",
  "Consume",
  "Time",
  "\n"
};

/** Names for world (room) trigger types. */
const char *wtrig_types[] = {
  "Global",
  "Random",
  "Command",
  "Speech",
  "UNUSED1",
  "Zone Reset",
  "Enter",
  "Drop",
  "UNUSED2",
  "UNUSED3",
  "UNUSED4",
  "UNUSED5",
  "UNUSED6",
  "UNUSED7",
  "UNUSED8",
  "Cast",
  "Leave",
  "Door",
  "Login",
  "Time",
  "\n"
};

/** The names of the different channels that history is stored for.
 * @todo Only referenced by do_history at the moment. Should be moved local
 * to that function. */
const char *history_types[] = {
  "all",
  "say",
  "gossip",
  "wiznet",
  "tell",
  "shout",
  "grats",
  "holler",
  "auction",
  "\n"
};

/** Flag names for Ideas, Bugs and Typos (defined in ibt.h) */
const char *ibt_bits[] = {
  "Resolved",
  "Important",
  "InProgress",
  "\n"
};
/* --- End of constants arrays. --- */

/* Various arrays we count so we can check the world files.  These
 * must be at the bottom of the file so they're pre-declared. */
  /** Number of defined room bit descriptions. */
  size_t	room_bits_count = sizeof(room_bits) / sizeof(room_bits[0]) - 1,
  /** Number of defined action bit descriptions. */
	action_bits_count = sizeof(action_bits) / sizeof(action_bits[0]) - 1,
	/** Number of defined affected bit descriptions. */
	affected_bits_count = sizeof(affected_bits) / sizeof(affected_bits[0]) - 1,
	/** Number of defined extra bit descriptions. */
	extra_bits_count = sizeof(extra_bits) / sizeof(extra_bits[0]) - 1,
	/** Number of defined wear bit descriptions. */
	wear_bits_count = sizeof(wear_bits) / sizeof(wear_bits[0]) - 1;

