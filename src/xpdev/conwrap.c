/* DOS's kbhit and getch functions for Unix - Casey Martin 2000 */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#if defined(__unix__)

#include <stdlib.h>
#include <string.h>	/* memcpy */
#include <unistd.h>
#include <termios.h>

#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#include "conwrap.h"				/* Verify prototypes */

static struct termios current;		/* our current term settings			*/
static struct termios original;     /* old termios settings					*/
static int beensetup = 0;           /* has _termios_setup() been called?	*/
static int istty = 0;				/* is stdin a tty?						*/		

/* Resets the termios to its previous state */
void _termios_reset(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &original);
}

/************************************************
  This pair of functions handles Ctrl-Z presses
************************************************/
#if defined(__BORLANDC__)
        #pragma argsused
#endif
#ifndef __EMSCRIPTEN__
void _sighandler_stop(int sig)
{
    /* clean up the terminal */
    _termios_reset();

    /* ... and stop */
	kill(getpid(), SIGSTOP);
}
#endif
#if defined(__BORLANDC__)
        #pragma argsused
#endif
void _sighandler_cont(int sig)
{
    /* restore terminal */
	tcsetattr(STDIN_FILENO, TCSANOW, &current);
}


/* Prepares termios for non-blocking action */
void _termios_setup(void)
{
	beensetup = 1;
    
	tcgetattr(STDIN_FILENO, &original);
  
	memcpy(&current, &original, sizeof(struct termios));
	current.c_cc[VMIN] = 1;           /* read() will return with one char */
	current.c_cc[VTIME] = 0;          /* read() blocks forever */
	current.c_lflag &= ~ICANON;       /* character mode */
    current.c_lflag &= ~ECHO;         /* turn off echoing */
	tcsetattr(STDIN_FILENO, TCSANOW, &current);

    /* Let's install an exit function, also.  This way, we can reset
     * the termios silently
	 */
    atexit(_termios_reset);

    /* install the Ctrl-Z handler */
#ifndef __EMSCRIPTEN__
    signal(SIGTSTP, _sighandler_stop);
#endif
    signal(SIGCONT, _sighandler_cont);
}

void _echo_on(void)
{
	tcgetattr(STDIN_FILENO, &current);
    current.c_lflag |= ECHO;         /* turn on echoing */
	tcsetattr(STDIN_FILENO, TCSANOW, &current);
}

void _echo_off(void)
{
	tcgetattr(STDIN_FILENO, &current);
    current.c_lflag &= ~ECHO;         /* turn off echoing */
	tcsetattr(STDIN_FILENO, TCSANOW, &current);
}

int kbhit(void)
{
	fd_set inp;
	struct timeval timeout = {0, 0};

	if(!istty) {
		istty = isatty(STDIN_FILENO);
		if(!istty)
			return 0;
	}

	if(!beensetup)
		_termios_setup();

	/* set up select() args */
	FD_ZERO(&inp);
	FD_SET(STDIN_FILENO, &inp);

	if(select(STDIN_FILENO+1, &inp, NULL, NULL, &timeout)<1)
		return 0;
	return 1;
}

int getch(void)
{
	char c;

    if(!beensetup)
    	_termios_setup();

    /* get a char out of stdin */
    if(read(STDIN_FILENO, &c, 1)==-1)
		return 0;

    return c;
}

#endif /* __unix__ */
