/*
	conwrap.c -- To give DOS's getch() function to Linux for use in Synchronet
    Casey Martin 2000
*/

/* $Id$ */

/* @format.tab-size 4, @format.use-tabs true */

#ifdef __unix__

#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/* Do the correct thing under BSD */
#ifndef __FreeBSD__
#include <sys/kd.h>
#endif
#ifdef __FreeBSD__
#include <sys/kbio.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#include "conwrap.h"	// Verify prototypes

struct termios current;            // our current term settings
struct termios original;           // old termios settings
struct timeval timeout = {0, 0};   // passed in select() call
fd_set inp;                        // ditto
int beensetup = 0;                 // has _termios_setup() been called?

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