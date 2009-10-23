#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include <ciolib.h>

#include "IO.h"
#include "Config.h"
#include "macros.h"
#include "files.h"
#include "structs.h"

#include "todo.h"

const char *location_desc(enum onloc loc)
{
	return("NO LOCATION DESCRIPTION");
}

void player_vs_monsters(enum pl_vs_type type, struct monster **mosnters, struct player **players)
{
	puts("A battle occurs!");
}

void newsy(bool trailing_line, ...)
{
	char *str;
	va_list	ap;

	va_start(ap, trailing_line);
	while((str=va_arg(ap, char *))!=NULL)
		printf("NEWS: %s\n", str);
	va_end(ap);
}

int rand_num(int limit)
{
	return(limit-1);
}

void status(struct player *pl)
{
	puts("DISPLAY USER STATUS");
}

void reduce_player_resurrections(struct player *pl, bool doit)
{
	puts("REDUCE PLAYER RESURRECTIONS");
}

void objekt_affect(int i, uint16_t index, enum objtype type, struct player *pl, bool loud)
{
	puts("OBJEKT AFFECT");
}

void decplayermoney(struct player *pl, int amount)
{
	pl->gold -= amount;
}

void user_save(struct player *pl)
{
	puts("SAVING USER");
}

void door_textattr(int attr)
{
	textattr(attr);
}

void door_outstr(const char *str)
{
	cputs(str);
}

void door_nl(void)
{
	cputs("\r\n");
}

int door_readch(void)
{
	return getch();
}

void door_clearscreen(void)
{
	clrscr();
}

char *uplc="PLAYER_COLOUR";	// Colour string for player name in messages
char *uitemc="ITEM_COLOUR";	// Colour string for items in messages
struct onliner onliner_str;
struct onliner *onliner=&onliner_str;
bool global_begged=false;
bool global_nobeg=false;

void Bobs_Inn(void)
{
	BAD("Bob's Inn not implemented!");
}

void Groggos_Magic(void)
{
	BAD("Groggo's Magic not implemented!");
}

void Drug_Store(void)
{
	BAD("Drug Store not implemented!");
}

void Steroid_Store(void)
{
	BAD("Steroid Store not implemented!");
}

void Orb_Center(void)
{
	BAD("Orb Center not implemented!");
}

int CIOLIB_main(int argc, char **argv)
{
	//create_npc_file();
	//create_player_file();
	//create_poison_file();
	open_files();
	player=players;
	strcpy(player->name1, "Deuce");
	strcpy(player->name2, "Deuce");
	player->allowed=true;
	//player->hps=200;
	//player->gold=50000;
	player->class=Alchemist;
	onliner=onliners;
	DefaultConfig();
	Shady_Shops();
}
