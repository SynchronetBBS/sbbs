/* conwrap.c */

/* -- To give DOS's getch() function to Unix - Casey Martin 2000 */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#if defined(__unix__)

#include <stdlib.h>
#include <string.h>	/* memcpy */
#include <unistd.h>
#include <termios.h>

/* Do the correct thing under BSD */
#if defined(__FreeBSD__)
	#include <sys/kbio.h>
#else /* Linux and others */
	#include <sys/kd.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#include "conwrap.h"	// Verify prototypes

static struct termios current;            // our current term settings
static struct termios original;           // old termios settings
static struct timeval timeout = {0, 0};   // passed in select() call
static fd_set inp;                        // ditto
static int beensetup = 0;                 // has _termios_setup() been called?

/*
	I'm using a variable function here simply for the sake of speed.  The
    termios functions must be called before a kbhit() can be successful, so
    on the first call, we just set up the terminal, point to variable function
    to kbhit_norm(), and then call the new function.  Otherwise, testing would
	be required on every call to determine if termios has already been setup.

    Maybe I'm being way too anal, though.
*/

/* Resets the termios to its previous state */
void _termios_reset(void)
{
	tcsetattr(0, TCSANOW, &original);
}

/************************************************
  This pair of functions handles Ctrl-Z presses
************************************************/

void _sighandler_stop(int sig)
{
    // clean up the terminal
    _termios_reset();

    // ... and stop
	kill(getpid(), SIGSTOP);
}

void _sighandler_cont(int sig)
{
    // restore terminal
	tcsetattr(0, TCSANOW, &current);
}


/* Prepares termios for non-blocking action */
void _termios_setup(void)
{
	beensetup = 1;
    
	tcgetattr(0, &original);
  
	memcpy(&current, &original, sizeof(struct termios));
	current.c_cc[VMIN] = 1;           // read() will return with one char
	current.c_cc[VTIME] = 0;          // read() blocks forever
	current.c_lflag &= ~ICANON;       // character mode
    current.c_lflag &= ~ECHO;         // turn off echoing
	tcsetattr(0, TCSANOW, &current);

    // Let's install an exit function, also.  This way, we can reset
    // the termios silently
    atexit(_termios_reset);

    // install the Ctrl-Z handler
    signal(SIGSTOP, _sighandler_stop);
    signal(SIGCONT, _sighandler_cont);
}


int kbhit(void)
{
	// set up select() args
	FD_ZERO(&inp);
	FD_SET(0, &inp);

	return select(1, &inp, NULL, NULL, &timeout);
}

int getch(void)
{
	char c;

    if (!beensetup)
    	// I hate to test for this every time, but this shouldn't be
        // called that often anyway...
    	_termios_setup();

    // get a char out of stdin
    read(0, &c, 1);

    return c;
}

#endif // __unix__
