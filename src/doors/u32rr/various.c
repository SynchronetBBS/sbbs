/*
 * Various functions
 */

#include "files.h"
#include "macros.h"
#include "todo.h"

void Display_Menu(bool force, bool terse, bool *refresh, const char *name, const char *expert_prompt, void (*Meny)(void))
{
	if(terse) {
		if(!player->expert) {
			if(*refresh && player->auto_meny) {
				*refresh=false;
				Meny();
			}
			pbreak();
			dc(config.textcolor, name, config.textcolor, " (", config.hotkeycolor, "?", config.textcolor, " for menu) :", D_DONE);
		}
		else {
			pbreak();
			TEXT(name, " ", expert_prompt, " :");
		}
	}
	else {
		if((!player->expert) || force) {
			Meny();
		}
	}
}
