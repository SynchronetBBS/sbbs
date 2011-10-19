/* EX_CHAT.C - Example of a multi-window full-screen chat program written    */
/*             with OpenDoors. See manual for instructions on how to compile */
/*             a program using OpenDoors.                                    */
/*                                                                           */
/*             This program shows how to do the following:                   */
/*                                                                           */
/*                - Replace the standard OpenDoors chat mode with your own   */
/*                  chat mode implementation. See instructions below.        */
/*                - How to scroll a portion of the screen, leaving the rest  */
/*                  of the screen unaltered.                                 */
/*                - How to determine whether input came from the local or    */
/*                  remote keyboard.                                         */
/*                - How to display a popup window with a message in it when  */
/*                  the sysop shells to DOS. (DOS version only.)             */
/*                - How to save and restore the entire contents of the       */
/*                  screen.                                                  */
/*                                                                           */
/*             Conditional compilation directives allow this program to      */
/*             be compiled as a stand-alone chat door, or as a               */
/*             replacement chat mode to be integrated into any OpenDoors     */
/*             program. If STANDALONE is #defined, ex_chat.c will be         */
/*             compiled as a split-screen chat door that can be run like     */
/*             any door program. If STANDALONE is not #defined, the chat     */
/*             mode function will be compiled as a replacement chat mode     */
/*             for the chat mode built into OpenDoors. In this case,  the    */
/*             demo mainline function simply displays a prompt, and will     */
/*             exit the door as soon as you press the [ENTER] key. While     */
/*             the program is running, if you invoke chat mode (press        */
/*             [ALT]-[C]), the current screen will be saved and the          */
/*             split-screen chat mode will be activated. When you press      */
/*             [ESC] the split-screen chat mode will end, and the            */
/*             original screen will be restored. To integrate this chat      */
/*             mode into your own program, you should simply set             */
/*             od_control.od_cbefore_chat to point to the                    */
/*             fullscreen_chat function, as shown, and remove the mainline   */
/*             (main()/WinMain()) function from this file. The compile this  */
/*             file into your program after removing the #define STANDALONE  */
/*             line.                                                         */

/* Include required header files. */
#include "OpenDoor.h"
#include <string.h>


/* The following #define forces this code to compile as a stand-alone door */
/* program. If you wish to use this code to replace the standard OpenDoors */
/* chat mode in your own program, remove this #define.                     */
#define STANDALONE


/* Full-screen chat function prototypes. */
void fullscreen_chat(void);
void chat_new_line(void);
void display_shell_window(void);
void remove_shell_window(void);



/* The main() or WinMain() function: program execution begins here. */
#ifdef ODPLAT_WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPSTR lpszCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
   /* Handle standard command-line options and perform other needed setup. */
#ifdef ODPLAT_WIN32
   od_control.od_cmd_show = nCmdShow;
   od_parse_cmd_line(lpszCmdLine);
#else
   od_parse_cmd_line(argc, argv);
#endif

#ifdef STANDALONE               /* If compiled as a stand-alone chat program */
   od_init();                                        /* Initialize OpenDoors */

   fullscreen_chat();                    /* Invoke full-screen chat function */

#else                  /* If compiled as replacement for OpenDoors chat mode */
                      /* Setup OpenDoors to use our custom chat mode instead */
   od_control.od_cbefore_chat=fullscreen_chat;

   od_printf("Press [Enter] to exit door, or invoke chat mode.\n\r");
   od_get_answer("\n\r");
#endif

   od_exit(0, FALSE);                                       /* Exit program. */
   return(0);                                                   
}





                                   /* FULL-SCREEN CHAT CUSTOMIZATION OPTIONS */

char window_colour[2]={0x0b,0x0c};       /* Text colour used for each person */
char bar_colour=0x70;                     /* Colour of window seperation bar */
char top_line[2]={13,1};      /* Specifies location of each window on screen */
char bottom_line[2]={23,11};         /* Line number of bottom of each window */
char bar_line=12;                    /* Line number of window seperation bar */
char scroll_distance=2;           /* Distance to scroll window when required */
char shell_window_title=0x1a;  /* Colour of title of DOS shell notice window */
char shell_window_boarder=0x1f;        /* Colour of DOS shell window boarder */
char shell_window_text=0x1b;           /* Colour of text in DOS shell window */



int cursor_window;                   /* FULL-SCREEN CHAT INTERNAL VARIABLES */
char current_word[2][81];
int word_length[2];
int cursor_col[2];
int cursor_line[2];
unsigned char key;
int old_chat_key;
void *shell_window;
char *before_shell_text;
char *after_shell_text;
#ifndef STANDALONE     /* If compiled as replacement for OpenDoors chat mode */
char screen_buffer[4004];
#endif

                                                /* FULL-SCREEN CHAT FUNCTION */
void fullscreen_chat(void)
{
   cursor_window=0;                               /* Reset working variables */
   word_length[0]=word_length[1]=0;
   cursor_col[0]=cursor_col[1]=1;
   cursor_line[0]=top_line[0];
   cursor_line[1]=top_line[1];


                         /* If ANSI or AVATAR graphics mode is not available */
   if(!od_control.user_ansi && !od_control.user_avatar)
   {                           /* Then use OpenDoor's line chat mode instead */
#ifdef STANDALONE               /* If compiled as a stand-alone chat program */
      od_chat();
#endif
      return;
   }

   od_control.od_cbefore_shell=display_shell_window;   /* Set shell settings */
   od_control.od_cafter_shell=remove_shell_window;
   before_shell_text=od_control.od_before_shell;
   after_shell_text=od_control.od_after_shell;
   od_control.od_before_shell=NULL;
   od_control.od_after_shell=NULL;
   od_control.od_chat_active=TRUE;

#ifdef STANDALONE               /* If compiled as a stand-alone chat program */
   old_chat_key=od_control.key_chat;      /* Prevent internal chat mode from */
   od_control.key_chat=0;                                   /* being invoked */

#else                  /* If compiled as replacement for OpenDoors chat mode */
   od_save_screen(screen_buffer);           /* Save current screen contents. */
#endif

                                                     /* DRAW THE CHAT SCREEN */
   od_set_attrib(window_colour[0]);
   od_clr_scr();                                         /* Clear the screen */

   od_set_cursor(bar_line,1);                  /* Draw window separation bar */
   od_set_attrib(bar_colour);
   od_clr_line();
   od_set_cursor(bar_line,67);
   od_printf("Ctrl-A: Clear");
   od_set_cursor(bar_line,1);
   od_printf(" Top : %-.28s    Bottom : %-.28s    ",
      od_control.sysop_name, od_control.user_name);

   od_set_cursor(top_line[0],1);    /* Locate cursor where typing will begin */
   od_set_attrib(window_colour[0]);           /* Set appropriate text colour */

                                                           /* MAIN CHAT LOOP */
   for(;;)                             /* (Repeats for each character typed) */
   {
      do
      {
         key=(char)od_get_key(FALSE);    /* Get next keystroke from keyboard */

                                                    /* CHECK FOR SYSOP ABORT */
         if((key==27 && od_control.od_last_input==1) /* If sysop pressed ESC */
            || !od_control.od_chat_active)
         {
            od_set_attrib(0x07);                     /* Reset display colour */
            od_clr_scr();                                /* Clear the screen */

            od_control.od_cbefore_shell=NULL;  /* Restore DOS shell settings */
            od_control.od_cafter_shell=NULL;
            od_control.od_before_shell=before_shell_text;
            od_control.od_after_shell=after_shell_text;
#ifdef STANDALONE               /* If compiled as a stand-alone chat program */
            od_control.key_chat=old_chat_key;/* Re-enable internal chat mode */

#else                  /* If compiled as replacement for OpenDoors chat mode */
            od_control.od_chat_active=FALSE;           /* Turn off chat mode */
            od_restore_screen(screen_buffer);      /* Restore orignal screen */
#endif
            return;                                 /* Exit full-screen chat */

         }
      } while(key==0);

                                                     /* CHECK FOR NEW TYPIST */
      if(od_control.od_last_input!=cursor_window)/* If new person typing now */
      {                               /* Switch cursor to appropriate window */
         cursor_window=od_control.od_last_input;       /* Set current typist */

                                                /* Move cursor to new window */
         od_set_cursor(cursor_line[cursor_window],cursor_col[cursor_window]);

         od_set_attrib(window_colour[cursor_window]);  /* Change text colour */
      }


      if(key==13 || key==10)           /* IF USER PRESSED [ENTER] / [RETURN] */
      {
         word_length[cursor_window]=0;      /* Enter constitutes end of word */

         chat_new_line();                               /* Move to next line */
      }


      else if(key==8)                           /* IF USER PRESS [BACKSPACE] */
      {
         if(cursor_col[cursor_window] > 1)       /* If not at left of screen */
         {
            --cursor_col[cursor_window];    /* Move cursor back on character */
            if(word_length[cursor_window] > 0) --word_length[cursor_window];
            od_printf("\b \b");          /* Erase last character from screen */
         }
      }


      else if(key==32)                            /* IF USER PRESSED [SPACE] */
      {
         word_length[cursor_window]=0;    /* [Space] constitutes end of word */

         if(cursor_col[cursor_window]==79)              /* If at end of line */
            chat_new_line();                     /* Move cursor to next line */
         else                                       /* If not at end of line */
         {
            ++cursor_col[cursor_window];        /* Increment cursor position */
            od_putch(32);                                 /* Display a space */
         }
      }


      else if(key==1)                    /* IF USER PRESSED CLEAR WINDOW KEY */
      {                                               /* Clear user's window */
         od_scroll(1,top_line[cursor_window],79,bottom_line[cursor_window],
            bottom_line[cursor_window]-top_line[cursor_window]+1,0);

         word_length[cursor_window]=0;  /* We are no longer composing a word */

         cursor_col[cursor_window]=1;               /* Reset cursor position */
         cursor_line[cursor_window]=top_line[cursor_window];
         od_set_cursor(cursor_line[cursor_window],cursor_col[cursor_window]);
      }


      else if(key>32)                 /* IF USER TYPED A PRINTABLE CHARACTER */
      {                                    /* PERFORM WORD WRAP IF NECESSARY */
         if(cursor_col[cursor_window]==79)    /* If cursor is at end of line */
         {
                                               /* If there is a word to wrap */
            if(word_length[cursor_window]>0 && word_length[cursor_window]<78)
            {
                                         /* Move cursor to beginning of word */
               od_set_cursor(cursor_line[cursor_window],
                          cursor_col[cursor_window]-word_length[cursor_window]);

               od_clr_line();                /* Erase word from current line */

               chat_new_line();                  /* Move cursor to next line */

                                                           /* Redisplay word */
               od_disp(current_word[cursor_window],word_length[cursor_window],
                                                                          TRUE);
               cursor_col[cursor_window]+=word_length[cursor_window];
            }

            else                            /* If there is no word to "wrap" */
            {
               chat_new_line();                  /* Move cursor to next line */
               word_length[cursor_window]=0;             /* Start a new word */
            }
         }

                                            /* ADD CHARACTER TO CURRENT WORD */
                              /* If there is room for more character in word */
         if(strlen(current_word[cursor_window])<79)     /* Add new character */
            current_word[cursor_window][word_length[cursor_window]++]=key;

                                            /* DISPLAY NEWLY TYPED CHARACTER */
         ++cursor_col[cursor_window];
         od_putch(key);
      }
   }
}



              /* FUNCTION USED BY FULL-SCREEN CHAT TO START A NEW INPUT LINE */
void chat_new_line(void)
{                                        /* If cursor is at bottom of window */
   if(cursor_line[cursor_window]==bottom_line[cursor_window])
   {                                  /* Scroll window up one line on screen */
      od_scroll(1,top_line[cursor_window],79, bottom_line[cursor_window],
                scroll_distance, 0);
      cursor_line[cursor_window]-=(scroll_distance - 1);
   }

   else                              /* If cursor is not at bottom of window */
   {
      ++cursor_line[cursor_window];             /* Move cursor down one line */
   }

                                         /* Move cursor's position on screen */
   od_set_cursor(cursor_line[cursor_window],cursor_col[cursor_window]=1);

   od_set_attrib(window_colour[cursor_window]);        /* Change text colour */
}


void display_shell_window(void)
{
   if((shell_window=od_window_create(17,9,63,15,"DOS Shell",
                                     shell_window_boarder, shell_window_title, 
                                     shell_window_text, 0))==NULL) return;

   od_set_attrib(shell_window_text);
   od_set_cursor(11,26);
   od_printf("The Sysop has shelled to DOS");
   od_set_cursor(13,21);
   od_printf("He/She will return in a few moments...");
}


void remove_shell_window(void)
{
   od_window_remove(shell_window);
   od_set_cursor(cursor_line[cursor_window],cursor_col[cursor_window]);
   od_set_attrib(window_colour[cursor_window]);
}
