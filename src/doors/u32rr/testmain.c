#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include <ciolib.h>

#include "doorIO.h"
#include "IO.h"
#include "Config.h"
#include "macros.h"
#include "files.h"
#include "structs.h"

#include "todo.h"

struct dropfile_info dropfile;
struct door_info door;

void get_string(char *str, size_t size)
{
	DL(lred, "NO GETSTRING!");
	strncpy(str, "NO GETSTRING!", size);
	str[size-1]=0;
}

const char *location_desc(enum onloc loc)
{
	return("NO LOCATION DESCRIPTION");
}

void player_vs_monsters(enum pl_vs_type type, struct monster **mosnters, struct player **players)
{
	DL("A battle occurs!");
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
	D(lred, Asprintf("RANDOM(%d):", limit));
	return(get_number(0, limit-1));
}

void objekt_affect(int i, uint16_t index, enum objtype type, struct player *pl, bool loud)
{
	DL("OBJEKT AFFECT");
}

void user_save(struct player *pl)
{
	DL(lred, "SAVING USER");
}

void inventory_display(struct player *pl)
{
	DL(lred,"INVENTORY DISPLAY");
}

void spell_list(struct player *pl)
{
	DL(lred, "SPELL LIST");
}

void Display_Member(struct player *pl, bool doit)
{
	DL(lred,"DISPLAY MEMBER");
}

void Display_Members(const char *team, bool topbar)
{
	DL(lred, "DISPLAY MEMBERS");
}

void inventory_sort(struct player *pl)
{
	DL(lred,"INVENTORY SORT");
}

void item_select(struct player *pl)
{
	DL(lred,"ITEM SELECT");
}

void use_item(int item)
{
	DL(lred,"USE ITEM");
}

void drop_item(struct player *pl)
{
	DL(lred,"DROP ITEM");
}

void Remove_Item(void)
{
	DL(lred,"REMOVE ITEM");
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
	gotoxy(1,1);
}

char *uplc="PLAYER_COLOUR";	// Colour string for player name in messages
char *uitemc="ITEM_COLOUR";	// Colour string for items in messages
struct onliner onliner_str;
struct onliner *onliner=&onliner_str;
bool global_begged=false;
bool global_nobeg=false;

void Drinking_Competition(void)
{
	BAD("DRINKING COMPETITION not implemented!");
}

void Brawl(void)
{
	BAD("BRAWL not implemented!");
}

void Post(enum mailaction action, const char *to, enum aitype toai, bool togod, enum mailspecial special, const char *from, ...)
{
	BAD("POST not implemented!");
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
	player->deleted=false;
	//player->hps=200;
	//player->gold=50000;
	player->class=Alchemist;
	onliner=onliners;
	DefaultConfig();

	// Open the shops...
	king->shop_weapon=true;
        king->shop_armor=true;
        king->shop_magic=true;
        king->shop_alabat=true;
        king->shop_plmarket=true;
        king->shop_healing=true;
        king->shop_drugs=true;
        king->shop_steroids=true;
        king->shop_orbs=true;
        king->shop_evilmagic=true;
        king->shop_bobs=true;
        king->shop_gigolos=true;

	Shady_Shops();
	return(0);
}
