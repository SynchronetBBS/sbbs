#include "ny2008.h"

//void ny_clr_scr(void);
void chat_new_line(void);
extern INT16 rip;
extern unsigned _stklen;
/* FULL-SCREEN CHAT CUSTOMIZATION OPTIONS */

char window_colour[2]={0x0b,0x0c};        /* Text colour used for each person */
char bar_colour=0x70;                      /* Colour of window seperation bar */
char top_line[2]={1,13};       /* Specifies location of each window on screen */
char bottom_line[2]={11,23};          /* Line number of bottom of each window */
char bar_line=12;                     /* Line number of window seperation bar */
char scroll_distance=9;            /* Distance to scroll window when required */
char cursor_window;                     /* FULL-SCREEN CHAT INTERNAL VARIABLES */
char current_word[2][81];
INT16 word_length[2];
INT16 cursor_col[2];
INT16 cursor_line[2];
unsigned char key;
INT16 old_chat_key;
INT16 command[2]={FALSE,FALSE}; /*no commands being used*/

/* FULL-SCREEN CHAT FUNCTION */
void fullscreen_chat(void) {
#if !defined(__WATCOMC__)
        cursor_window=0;                                /* Reset working variables */
        word_length[0]=word_length[1]=0;
        cursor_col[0]=cursor_col[1]=1;
        cursor_line[0]=top_line[0];
        cursor_line[1]=top_line[1];




        /* If ANSI or AVATAR graphics mode is not available */
        if(!od_control.user_ansi && !od_control.user_avatar) {
                od_chat();                /* Then use OpenDoor's line chat mode instead */
                return;
        }

        old_chat_key=od_control.key_chat;       /* Prevent internal chat mode from */
        od_control.key_chat=0;                                    /* being invoked */
        /* DRAW THE CHAT SCREEN */
        od_set_attrib(window_colour[0]);
        ny_clr_scr();                                          /* Clear the screen */

        //   char numstr[100];

        if(rip==FALSE) {
                od_set_cursor(bar_line,1);                   /* Draw window seperation bar */
                od_set_attrib(bar_colour);
                od_clr_line();
                od_printf("  `black white`Top : `blue white`%s  `black white`Bottom : `blue white`%s  `red white`'/' = Command",
                          od_control.user_name,
                          od_control.sysop_name);
        } else {
                od_printf("`black black`");

                od_disp_str("\n\r!|*|1K|w0000270N12|Y00000100|1B00000202ZK02080F0F080700000807000000\n\r");
                od_disp_str("!|1UDK4CHK4M0000<>Command<>^M/|1B000002027402080F0F080700000807000000\n\r");
                od_disp_str("!|1U044CD44M0000|W0|c09|Y00000100|@0D4E");
                od_printf("Top : %s  Bottom : %s|#|#|#\n\r  \b\b",od_control.user_name,od_control.sysop_name);
                gotoxy(1,bar_line);                   /* Draw window seperation bar */
                textbackground(WHITE);
                clreol();
                textcolor(BLACK);
                cprintf("  Top : ");
                textcolor(BLUE);
                cprintf("%s  ",od_control.user_name);
                textcolor(BLACK);
                cprintf("Bottom : ");
                textcolor(BLUE);
                cprintf("%s  ",od_control.sysop_name);
                textcolor(RED);
                cprintf("'/' = Command");
        }

        od_set_cursor(top_line[0],1);     /* Locate cursor where typing will begin */
        od_set_attrib(window_colour[0]);            /* Set appropriate text colour */

        /* MAIN CHAT LOOP */
        for(;;)                              /* (Repeats for each character typed) */
        {
                key=(char)od_get_key(TRUE);         /* Get next keystroke from keyboard */

                /* CHECK FOR SYSOP ABORT */
                if(key==27 && od_control.od_last_input==1)    /* If sysop pressed [ESC] */
                {
                        od_set_attrib(0x07);                         /* Reset display colour */
                        ny_clr_scr();                                    /* Clear the screen */
                        od_control.key_chat=old_chat_key;    /* Re-enable internal chat mode */

                        return;                                     /* Exit full-screen chat */
                }

                /* CHECK FOR NEW TYPIST */
                if(od_control.od_last_input!=cursor_window) /* If new person typing now */
                {                             /* Switch cursor to appropriate window */
                        cursor_window=od_control.od_last_input;        /* Set current typist */

                        /* Move cursor to new window */
                        od_set_cursor(cursor_line[cursor_window],cursor_col[cursor_window]);

                        od_set_attrib(window_colour[cursor_window]);   /* Change text colour */
                }


                if(command[od_control.od_last_input]==TRUE)                      /* if user enabled the command mode */
                {
                        if (key=='L' || key=='l') {
                                if (od_control.od_last_input==1) {
                                        printf("\r");
                                        printf("                                                           \r");
                                        od_printf("`bright green`%s `dark green`laughs...",od_control.sysop_name);
                                        chat_new_line();                                /* Move to next line */
                                        command[1]=FALSE;
                                } else {
                                        od_disp("\r",1,FALSE);
                                        od_disp("                                                           \r",60,FALSE);
                                        od_printf("`bright green`%s `dark green`laughs...",od_control.user_name);
                                        chat_new_line();                                /* Move to next line */
                                        command[0]=FALSE;
                                }
                                key=0;
                        } else if (key=='S' || key=='s') {
                                if (od_control.od_last_input==1) {
                                        printf("\r");
                                        printf("                                                           \r");
                                        od_printf("`bright green`%s `dark green`sighs...",od_control.sysop_name);
                                        chat_new_line();                                /* Move to next line */
                                        command[1]=FALSE;
                                } else {
                                        od_disp("\r",1,FALSE);
                                        od_disp("                                                           \r",60,FALSE);
                                        od_printf("`bright green`%s `dark green`sighs...",od_control.user_name);
                                        chat_new_line();                                /* Move to next line */
                                        command[0]=FALSE;
                                }
                                key=0;
                        } else if (key=='W' || key=='w') {
                                if (od_control.od_last_input==1) {
                                        printf("\r");
                                        printf("                                                           \r");
                                        od_printf("`bright green`%s `dark green`winks at `bright green`%s`dark green`...",od_control.sysop_name,od_control.user_name);
                                        chat_new_line();                                /* Move to next line */
                                        command[1]=FALSE;
                                } else {
                                        od_disp("\r",1,FALSE);
                                        od_disp("                                                           \r",60,FALSE);
                                        od_printf("`bright green`%s `dark green`winks at `bright green`%s`dark green`...",od_control.user_name,od_control.sysop_name);
                                        chat_new_line();                                /* Move to next line */
                                        command[0]=FALSE;
                                }
                                key=0;
                        } else if (key=='C' || key=='c') {
                                if (od_control.od_last_input==1) {
                                        printf("\r");
                                        printf("                                                           \r");
                                        od_printf("`bright green`%s `dark green`is crying...",od_control.sysop_name);
                                        chat_new_line();                                /* Move to next line */
                                        command[1]=FALSE;
                                } else {
                                        od_disp("\r",1,FALSE);
                                        od_disp("                                                           \r",60,FALSE);
                                        od_printf("`bright green`%s `dark green`is crying...",od_control.user_name);
                                        chat_new_line();                                /* Move to next line */
                                        command[0]=FALSE;
                                }
                                key=0;
                        } else {
                                if (od_control.od_last_input==1) {
                                        cprintf("\r");
                                        cprintf("                                                           \r");
                                        command[1]=FALSE;
                                } else {
                                        od_disp("\r",1,FALSE);
                                        od_disp("                                                           \r",60,FALSE);
                                        command[0]=FALSE;
                                }
                                key=0;
                        }
                        od_set_attrib(window_colour[cursor_window]);   /* Change text colour */
                }
                else if (key=='/')                       /* if user enabled the command mode */
                {
                        if(cursor_col[cursor_window] == 1)       /* only if at left of screen */
                        {
                                if (od_control.od_last_input==1) {
                                        textcolor(WHITE);
                                        textbackground(BLACK);
                                        cprintf("Do what: (L=Laugh, S=Sigh, W=Wink, C=Cry, Other=Continue) >");
                                        command[1]=TRUE;
                                } else {
                                        if(rip==FALSE)
                                                od_disp("Do what: (L=Laugh, S=Sigh, W=Wink, C=Cry, Other=Continue) >",59,FALSE);
                                        else
                                                od_disp("\r!|10000((*Command::L@Laugh,S@Sigh,W@Wink,C@Cry,^M@Continue))|#|#|#\n\r",69,FALSE);
                                        command[0]=TRUE;
                                }
                                key=0;
                        }
                }

                if(key==13 || key==10)            /* IF USER PRESSED [ENTER] / [RETURN] */
                {
                        word_length[cursor_window]=0;       /* Enter constitutes end of word */

                        chat_new_line();                                /* Move to next line */
                } else if(key==8)                            /* IF USER PRESS [BACKSPACE] */
                {
                        if(cursor_col[cursor_window] > 1)        /* If not at left of screen */
                        {
                                --cursor_col[cursor_window];     /* Move cursor back on character */
                                if(word_length[cursor_window] > 0)
                                        --word_length[cursor_window];
                                od_printf("\b \b");           /* Erase last character from screen */
                        }
                }


                else if(key==32)                             /* IF USER PRESSED [SPACE] */
                {
                        word_length[cursor_window]=0;     /* [Space] constitutes end of word */

                        if(cursor_col[cursor_window]==79)               /* If at end of line */
                                chat_new_line();                      /* Move cursor to next line */
                        else                                        /* If not at end of line */
                        {
                                ++cursor_col[cursor_window];         /* Increment cursor position */
                                od_putch(32);                                  /* Display a space */
                        }
                }


                else if(key>32)                  /* IF USER TYPED A PRINTABLE CHARACTER */
                {                                  /* PERFORM WORD WRAP IF NECESSARY */
                        if(cursor_col[cursor_window]==79)     /* If cursor is at end of line */
                        {
                                /* If there is a word to wrap */
                                if(word_length[cursor_window]>0 && word_length[cursor_window]<78) {
                                        /* Move cursor to beginning of word */
                                        od_set_cursor(cursor_line[cursor_window],
                                                      cursor_col[cursor_window]-word_length[cursor_window]);

                                        od_clr_line();                 /* Erase word from current line */

                                        chat_new_line();                   /* Move cursor to next line */

                                        /* Redisplay word */
                                        od_disp(current_word[cursor_window],word_length[cursor_window],
                                                TRUE);
                                        cursor_col[cursor_window]+=word_length[cursor_window];
                                } else                             /* If there is no word to "wrap" */
                                {
                                        chat_new_line();                   /* Move cursor to next line */
                                        word_length[cursor_window]=0;              /* Start a new word */
                                }
                        }

                        /* ADD CHARACTER TO CURRENT WORD */
                        /* If there is room for more character in word */
                        if(strlen(current_word[cursor_window])<79)      /* Add new character */
                                current_word[cursor_window][word_length[cursor_window]++]=key;

                        /* DISPLAY NEWLY TYPED CHARACTER */
                        ++cursor_col[cursor_window];
                        od_putch(key);
                }
        }
#endif
}



/* FUNCTION USED BY FULL-SCREEN CHAT TO START A NEW INPUT LINE */
void chat_new_line(void) {                                      /* If cursor is at bottom of window */
        if(cursor_line[cursor_window]==bottom_line[cursor_window]) {                                /* Scroll window up one line on screen */
                od_scroll(1,top_line[cursor_window],79, bottom_line[cursor_window],
                          scroll_distance, 0);
                cursor_line[cursor_window]-=(scroll_distance - 1);
        } else                               /* If cursor is not at bottom of window */
        {
                ++cursor_line[cursor_window];              /* Move cursor down one line */
        }

        /* Move cursor's position on screen */
        od_set_cursor(cursor_line[cursor_window],cursor_col[cursor_window]=1);

        od_set_attrib(window_colour[cursor_window]);         /* Change text colour */
}
