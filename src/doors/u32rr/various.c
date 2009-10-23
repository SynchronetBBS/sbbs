/*
 * Various functions
 */

#include "IO.h"
#include "Config.h"
#include "files.h"
#include "macros.h"

#include "todo.h"

void Display_Menu(bool force, bool terse, bool *refresh, const char *name, const char *expert_prompt, void (*Meny)(void *), void *cbdata)
{
	if(terse) {
		if(!player->expert) {
			if(*refresh && player->auto_meny) {
				*refresh=false;
				Meny(cbdata);
			}
			nl();
			D(config.textcolor, name, " (", config.hotkeycolor, "?", config.textcolor, " for menu) :");
		}
		else {
			nl();
			TEXT(name, " ", expert_prompt, " :");
		}
	}
	else {
		if((!player->expert) || force) {
			Meny(cbdata);
		}
	}
}
