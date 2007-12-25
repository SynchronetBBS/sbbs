#ifndef _DGNLNCE_H_
#define _DGNLNCE_H_

#include <stdio.h>

#include "xp64.h"
#include "OpenDoor.h"

#define FIGHTS_PER_DAY	10
#define BATTLES_PER_DAY	3
#define FLIGHTS_PER_DAY	3

#define DEAD	1<<0
#define ONLINE	1<<1

/* Status for opponent */
enum {
    MONSTER
    ,PLAYER
};

/* Used for sextrings() */
enum {
    HE
    ,HIS
    ,HIM
};

struct playertype {
    char            name[31];	       /* Name from BBS drop file */
    char            pseudo[31];	       /* In-game pseudonym */
    char            killer[31];	       /* Person killed by (if status==DEAD) */
    char            bcry[61];	       /* Battle Cry */
    char            winmsg[61];	       /* What you say when you win a battle */
    char            gaspd[61];	       /* Dying curse */
    char            laston[11];	       /* Last date one */
    char            sex;	       /* Characters sex */
    DWORD           status;	       /* Status alive or dead for player
				        * records, PLAYER or MONSTER for
				        * opponent record (record 0) */
    /* ToDo make configurable */
    WORD            flights;	       /* Weird thing... may be number of
				        * times game can be ran per day */
    WORD            plus;	       /* Weapon Bonus - Not currently Used */
    WORD            r;		       /* Level */
    WORD            strength;	       /* Str */
    WORD            intelligence;      /* Int */
    WORD            dexterity;	       /* Dex */
    WORD            constitution;      /* Con */
    WORD            charisma;	       /* Cha */
    QWORD           experience;	       /* Exp */
    WORD            luck;	       /* Luk */
    WORD            weapon;	       /* Weapon Number */
    WORD            armour;	       /* Armour Number */
    WORD            hps;	       /* Hit Points */
    WORD            damage;	       /* Damage taken so far */
    QWORD           gold;	       /* Gold on hand */
    QWORD           bank;	       /* Gold in bank */
    WORD            attack;	       /* Weapon Attack */
    WORD            power;	       /* Weapon Power */
    QWORD           wins;	       /* Wins so far */
    QWORD           loses;	       /* Losses so far */
    WORD            battles;	       /* Battles left today */
    WORD            fights;	       /* Fights left today */
    /* Players variance... applied to various rolls, Generally, how good the
     * player is feeling. Is set at start and not modified */
    double          vary;
};

struct weapon {
    char            name[31];
    WORD            attack;
    WORD            power;
    DWORD           cost;
};

struct armour {
    char            name[31];
    DWORD           cost;
};


extern struct playertype player[3];
/* player[1] is the current user */
#define user	player[1]
/* player[0] is the current enemy */
#define opp	player[0]
/* player[2] is the temporary user */
#define tmpuser player[2]

/* In these arrays, 0 isn't actually used */
extern QWORD           required[29];	       /* Experience required for each level */
extern struct weapon   weapon[26];
extern struct armour   armour[26];
extern const char	dots[];
FILE           *infile;		       /* Current input file */

/* Utility macros */
#define nl()	od_disp_str("\r\n");

#endif
