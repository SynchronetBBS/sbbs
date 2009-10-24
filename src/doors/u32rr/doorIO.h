#ifndef _DOORIO_H_
#define _DOORIO_H_

#include <stdbool.h>

/*
 * Sets the current DOS text attribute to attr
 */
void door_textattr(int attr);

/*
 * Sends the string str to the remote system
 */
void door_outstr(const char *str);

/*
 * Sends a newline sequence to the remote
 */
void door_nl(void);

/*
 * Reads a single **UNSIGNED** character from the remote, returns -1 on error
 */
int door_readch(void);

/*
 * Clears the screen (either to black OR to corrent attr... doesn't matter)
 * and moves to position 1;1
 */
void door_clearscreen(void);

struct dropfile_info {
	bool	ansi;
};

struct door_info {
	int		time_left;	// Minutes
};

extern struct dropfile_info dropfile;
extern struct door_info door;

#endif
