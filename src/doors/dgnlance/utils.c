#include <ctype.h>

#include "OpenDoor.h"

#include "dgnlnce.h"
#include "xp64.h"
#include "xpdev.h"

double
supplant(void)
{
    return ((double)(xp_random(40) + 80)) / 100;
}

void
statshow(void)
{
    od_clr_scr();
    od_set_color(D_MAGENTA, D_BLACK);
    od_printf("Name: %s   Level: %d\r\n", user.pseudo, user.r);
    od_set_color(L_CYAN, D_BLACK);
    od_printf("W/L: %" QWORDFORMAT "/%" QWORDFORMAT "   Exp: %" QWORDFORMAT "\r\n", user.wins, user.loses, user.experience);
    nl();
    od_set_color(L_YELLOW, D_BLACK);
    od_printf("Steel  (in hand): %" QWORDFORMAT "\r\n", user.gold);
    od_printf("Steel  (in bank): %" QWORDFORMAT "\r\n", user.bank);
    nl();
    od_set_color(L_BLUE, D_BLACK);
    od_printf("Battles: %u   Retreats: %u    Fights: %u   Hps: %u(%u)\r\n", user.battles, user.flights, user.fights, user.hps - user.damage, user.hps);
    nl();
    od_set_color(L_CYAN, D_BLACK);
    od_printf("Weapon: %s     Armor: %s\r\n", weapon[user.weapon].name, armour[user.armour].name);
}

void
pausescr(void)
{
    od_set_color(L_CYAN, D_BLACK);
    od_disp_str("[Pause]");
    od_set_color(D_GREY, D_BLACK);
    od_get_key(TRUE);
    od_disp_str("\010 \010\010 \010\010 \010\010 \010\010 \010\010 \010\010 \010");
}

char           *
sexstrings(struct playertype * plr, int type, BOOL cap)
{
    static char     retbuf[16];
    char            sex;
    sex = plr->sex;
    if (plr->charisma < 8)
	sex = 'I';
    switch (sex) {
	case 'M':
	    switch (type) {
		case HIS:
		    SAFECOPY(retbuf, "his");
		    break;
		case HE:
		    SAFECOPY(retbuf, "he");
		    break;
		case HIM:
		    SAFECOPY(retbuf, "him");
		    break;
	    }
	case 'F':
	    switch (type) {
		case HIS:
		    SAFECOPY(retbuf, "her");
		    break;
		case HE:
		    SAFECOPY(retbuf, "she");
		    break;
		case HIM:
		    SAFECOPY(retbuf, "her");
		    break;
	    }
	case 'I':
	    switch (type) {
		case HIS:
		    SAFECOPY(retbuf, "its");
		    break;
		case HE:
		    SAFECOPY(retbuf, "it");
		    break;
		case HIM:
		    SAFECOPY(retbuf, "it");
		    break;
	    }
    }
    if (cap)
	retbuf[0] = toupper(retbuf[0]);
    return (retbuf);
}
