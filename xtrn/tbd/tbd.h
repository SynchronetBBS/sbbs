/*
 *  The Beast's Domain door game
 *  Copyright (C) 2005  Domain Entertainment
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "xsdk.h"

void movement(int sx,int sy,int sz,int sgx,int sgy);
int inway(int x,int y,int z,int gx,int gy);
int initial_inway(int x,int y,int z,int gx,int gy);
void write_movement(int pic,int x,int y,int z,int gx,int gy);
void status_message(int line,char *str, ...);
void draw_others(int x,int y,int z);
void drawpos(int x,int y,int z);
void game_commands(int menu,int line);
long read_user(int get_empty);
void write_user(void);
void put_single_object(int level,int room,int num);
void get_room_objects(int level,int room,int display);
void comp_room_objects(int level,int room);
int pickup_object(int x,int y,int z,int room);
int drop_object(int x,int y,int z,int room);
void get_single_object(int level,int room,int num);
void read_map_level(int level);
int player_attack(int player,uchar level,int ac,int symbol);
int attack_monster(uchar level);
void monster_attack(uchar level);
void scatter_items(int z,int room);
int find_empty(void);
int use_item(void);
void center_wargs(char *str,...);
void readline(char *outstr, int maxlen, FILE *stream);
void moduser(void);
void list_users(void);
void monster_check(int gx,int gy);
int warp_to_other(void);
void perform_daily_maintenance(int);
void restore_clock(void);
void send_message(int oldnumnodes);
void read_player_message(void);
void send_player_message(int plr, char *msg);
void game_winner(void);

#define N_DOOR  (1<<0)
#define S_DOOR  (1<<1)
#define E_DOOR  (1<<2)
#define W_DOOR  (1<<3)
#define NW_DOOR (1<<4)
#define NE_DOOR (1<<5)
#define SW_DOOR (1<<6)
#define SE_DOOR (1<<7)

#define LEVELS  10
#define SQUARE  30

#define FACE    2
#define UP      30
#define DOWN    31
#define LEFT    174
#define RIGHT   175

#define WEAPON      1   /* Is the object a weapon? */
#define ARMOR       2   /* Is the object armor? */
#define GOLD        3   /* Is the object gold? */
#define SPECIAL     4   /* Is the object special (wand/spell/potion)? */
#define STAIRS      5   /* Stairway to another level */
#define TRADINGPOST 6   /* Store for Buy/Sell */
#define MONSTER     7   /* A monster (duh) */
#define BLOCKAGE    8   /* Something blocking your way (tree or object) */

#define NUM_WEAPON      1  /* 7  */
#define NUM_ARMOR       8  /* 6  */
#define NUM_GOLD        14 /* 1  */
#define NUM_SPECIAL     15 /* 13 */
#define NUM_STAIRS      28 /* 2  */
#define NUM_TRADINGPOST 30 /* 3  */
#define NUM_MONSTER     33 /* 23 */
#define NUM_BLOCKAGE    56 /* 6  */

typedef struct {
    unsigned char item, val;
} rmobj_t;

typedef struct {
    short status;
    char handle[26];
    unsigned short mapx,mapy,mapz,roomx,roomy,health,max_health;
    ulong exp;
    uchar level;
    unsigned short weapon,armor,gold,item[6];
    long left_game;
    uchar ring,key,compass;
    char space[28];
} user_t;
#define USER_T_SIZE	100

typedef struct {
    char type,color,symbol,name[50];
    short value,misc;
} object_t;

extern rmobj_t  rmobj[55];
extern rmobj_t  rmcmp[55];
extern object_t object[62];
extern user_t user;
extern int in_shop;
extern long cost_per_min,times_per_day,total_cost;

extern char     redraw_screen;
extern long     record_number;
extern int      create_log,chfile,rmfile,weapon_ready,invisible,strong,
                tpic,lasthit,clock_tick,clock_tick2,ateof;
extern uchar    map[LEVELS][SQUARE][SQUARE];

