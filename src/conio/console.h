/*
 * Copyright (c) 1992, 1993, 1996
 *      Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Berkeley Software
 *      Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>

#include <gen_defs.h>
#include <semwrap.h>

extern sem_t	console_mode_changed;

/* Entry type for the DAC table. Each value is actually 6 bits wide. */
struct dac_colors {
    BYTE red;
    BYTE green;
    BYTE blue;
};

extern int CurrMode;

extern int InitCS;
extern int InitCE;

extern WORD *vmem;

extern BYTE CursRow;
extern BYTE CursCol;
extern BYTE CursStart;
extern BYTE CursEnd;

extern WORD DpyCols;
extern BYTE DpyRows;

extern int FH,FW;

extern int x_nextchar;

extern int console_new_mode;

int init_window();
int video_init();
int init_mode(int mode);
int tty_read(int flag);
int tty_peek(int flag);
int tty_kbhit(void);
void tty_beep(void);
void x_win_title(const char *title);

#define	TTYF_BLOCK	0x00000008
#define	TTYF_POLL	0x00000010
#define NO_NEW_MODE -999

#endif
