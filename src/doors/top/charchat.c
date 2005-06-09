#include "top.h"

/* Code to implement windowed chat inside TOP (INCOMPLETE). */

// These all go to header when ready

typedef struct cbc_window_str
    {
    XINT posx, posy;
    XINT fsizex, fsizey;
    XINT tsizex, tsizey;
    } cbc_window_typ;

char cbcechochars;
unsigned char *cbcbuf;
XINT cbcbufpos;
XINT cbcinfil, cbcoutfil;

// cfg.cbcbuflen
// cfg.cbcpolltime
// cfg.cbcforcewrite
// cbc channel stuff
// also window tracking stuff

char cbc_putch(XINT pchar, char tonode)
    {

    if (cbcbufpos < cfg.cbcbuflen)
        {
        cbcbuf[cbcbufpos++] = pchar;
        return 1;
        }
    else
        {
        // Try to realloc() as well as log a warning
        return 0;
        }

    }

XINT cbc_writebuf(XINT hnd)
    {

    rec_locking(REC_LOCK, hnd, 0L, 1L);
    write(hnd, cbcbuf, strlen(cbcbufpos));
    rec_locking(REC_UNLOCK, hnd, 0L, 1L);

    cbcbuf[0] = '\0'
    cbcbufpos = 0;
    // Try to re-realloc() if strlen is longer than the size.

    }

XINT cbc_readfile(void)
    {
    unsigned char *readbuf = NULL;
    long rfsize;

    rfsize = filelength(cbcinfil);

    if (rfsize <= 1)
        {
        return 0;
        }

    readbuf = malloc(rfsize);
    if (readbuf == NULL)
        {
        // err
        return 0;
        }

    rec_locking(REC_LOCK, cbcinfil, 0L, 1L);
    write(cbcinfil, readbuf, rfsize - 1L);
    rec_locking(REC_UNLOCK, cbcinfil, 0L, 1L);

    readbuf[rfsize - 1] = '\0';

    top_output(OUT_SCREEN, readbuf);

    dofree(readbuf);

    return (rfsize - 1L);
    }
    
