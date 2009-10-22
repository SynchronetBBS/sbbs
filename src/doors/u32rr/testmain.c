#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "macros.h"
#include "files.h"
#include "structs.h"

#include "todo.h"

void sethotkeys_on(int type, char *chars)
{
	puts("!!! sethotkeys_on() not implemented");
}

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

/*
 * Multiple strings, all the same colour, followed by nl
 */
void dl(int color, const char *str, ...)
{
	va_list ap;

	va_start(ap, str);
	do {
		printf("%s", str);
		str=va_arg(ap, char *);
	} while(str != NULL);
	va_end(ap);
	puts("");
}

/*
 * Multiple strings, all different colours, followed by nl
 */
void dlc(int color, const char *str, ...)
{
	va_list ap;

	va_start(ap, str);
	do {
		printf("%s", str);
		color=va_arg(ap, int);
		if(color==D_DONE)
			break;
		str=va_arg(ap, char *);
	} while(color != D_DONE && str != NULL);
	va_end(ap);
	puts("");
}

/*
 * Multiple strings, all the same colour, no trailing newline
 */
void d(int color, const char *str, ...)
{
	va_list ap;

	va_start(ap, str);
	do {
		printf("%s", str);
		str=va_arg(ap, char *);
	} while(str != NULL);
	va_end(ap);
}

/*
 * Multiple strings, all different colours, no trailing newline
 */
void dc(int color, const char *str, ...)
{
	va_list ap;

	va_start(ap, str);
	do {
		printf("%s", str);
		color=va_arg(ap, int);
		if(color==D_DONE)
			break;
		str=va_arg(ap, char *);
	} while(color != D_DONE && str != NULL);
	va_end(ap);
}

void upause(void)
{
	char	str[10];

	d(GREY, "PAUSE: Press enter", NULL);
	fgets(str, sizeof(str), stdin);
}

void halt(void)
{
	exit(0);
}

void newscreen(void)
{
	puts("====");
	puts("NewScreen");
	puts("====");
	puts("");
}

void menu(const char *str)
{
	printf("MENU: %s\n", str);
}

void menu2(const char *str)
{
	printf("MENU2: %s", str);
}

char confirm(const char *prompt, char dflt)
{
	char	str[5];

	printf("CONFIRM: %s", str);
	fgets(str, sizeof(str), stdin);
	return toupper(str[0])==toupper(dflt);
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

int get_number(int min, int max)
{
	int	ret=min-1;

	while(ret < min || ret > max) {
		scanf("%d", &ret);
	}
	return(ret);
}

void user_save(struct player *pl)
{
	puts("SAVING USER");
}

char *uplc="PLAYER_COLOUR";	// Colour string for player name in messages
char *uitemc="ITEM_COLOUR";	// Colour string for items in messages
struct onliner onliner_str;
struct onliner *onliner=&onliner_str;
bool global_begged=false;
bool global_nobeg=false;
struct config config={
	GREEN,
	WHITE,
	MAGENTA,
	BRIGHT_GREEN,
	BRIGHT_MAGENTA,
	BRIGHT_RED,
	WHITE,
	MAGENTA,
	CYAN,
	MAGENTA,
	MAGENTA,
	BRIGHT_GREEN,
	BRIGHT_BLUE,

	"|G",
	"|M",

	"gold",
	"coin",
	"coins",
	"Reese",
	"Groggo"
};

int main(int argc, char **argv)
{
	open_files();
	player=players;
	Alchemisty();
}
