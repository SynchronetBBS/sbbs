/* EX_MUSIC.C - Example program plays "Happy Birthday" to the remote user,   */
/*              if possible. See the manual for instructions on how to       */
/*              compile this program.                                        */
/*                                                                           */
/*              This program shows how to do the following:                  */
/*                                                                           */
/*                 - Demonstrates how to play sounds effects or music on a   */
/*                   remote terminal program that supports the so-called     */
/*                   "ANSI music" standard.                                  */
/*                 - Shows how to send text to the remote system without it  */
/*                   being displayed on the local screen.                    */


/* The opendoor.h file must be included by any program using OpenDoors. */
#include "OpenDoor.h"

#include <string.h>

/* Functions for playing "ANSI music" and testing "ANSI music" capabilities. */
void PlayANSISound(char *pszSounds);
char TestSound(void);

/* Variable indicates whether or not sound is on */
char bSoundEnabled = TRUE;


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

   /* Display introductory message. */
   od_printf("This is a simple door program that will play the song Happy Birthday\n\r");
   od_printf("tune on the remote system, if the user's terminal program supports ANSI\n\r");
   od_printf("music. Music is not played on the local speaker, as BBS system operators\n\r");
   od_printf("do not wish to have the BBS computer making sounds at any time of the day\n\r");
   od_printf("or night. However, the program can easily be modified to also echo sound to\n\r");
   od_printf("the local speaker.\n\r\n\r");


   /* Test whether user's terminal supports "ANSI music". */
   TestSound();


   /* Send birthday greetings to the remote user. */

   /* Clear the screen. */
   od_clr_scr();

   /* Display a message. */
   od_printf("\n\rHappy Birthday!\n\r");

   /* If "ANSI music" is available, play "Happy Birthday". */
   PlayANSISound("MBT120L4MFMNO4C8C8DCFE2C8C8DCGF2C8C8O5CO4AFED2T90B-8B-8AFGF2");

   /* Reset sound after finished playing. */
   PlayANSISound("00m");


   /* Wait for user to press a key before returning to the BBS. */
   od_printf("\n\rPress any key to return to BBS...\n\r");
   od_get_key(TRUE);
   od_exit(0, FALSE);
   return(0);
}


/* Function to test whether the user's terminal program supports ANSI music. */
/* You can either do this every time the user uses your program, or only the */
/* first time they use the program, saving the result in a data file.        */
char TestSound(void)
{
   /* Variable to store user's response to question. */
   char chResponse;

   /* Display description of test to user. */
   od_printf("We need to know whether or not your terminal program supports ANSI music.\n\r");
   od_printf("In order to test this, we will send a short ANSI music sequence. We will then\n\r");
   od_printf("ask whether or not you heard any sound.\n\r");
   od_printf("Press any key to begin this test... ");

   /* Wait for user to press a key to begin. */
   od_get_key(TRUE);
   od_printf("\n\r\n\r");

   /* Temporarily enable sound. */
   bSoundEnabled = TRUE;

   /* Send sound test sequence. */
   PlayANSISound("MBT120L4MFMNO4C8C8DC");

   /* Reset sound after finished test. */
   PlayANSISound("00m");

   /* Clear screen and ask whether user heard the sound. */
   od_clr_scr();
   od_printf("Did you just hear sound from your speaker? (Y/n)");
   chResponse = od_get_answer("YN");

   /* Set ANSI music on/off according to user's response. */
   bSoundEnabled = (chResponse == 'Y');
   
   return(bSoundEnabled);
}


/* Function to play "ANSI" music or sound effects. The play_sound() function
 * can be called with a string of 0 to 250 characters. The caracters of the
 * string define what sounds should be played on the remote speaker, as
 * follows:
 *
 *      A - G       Musical Notes
 *      # or +      Following A-G note means sharp
 *      -           Following A-G note means flat
 *      <           Move down one octave
 *      >           Move up one octave
 *      .           Period acts as dotted note (extend note duration by 3/2)
 *      MF          Music Foreground (pause until finished playing music)
 *      MB          Music Background (continue while music plays)
 *      MN          Music note duration Normal (7/8 of interval between notes)
 *      MS          Music note duration Staccato
 *      ML          Music note duration Legato
 *      Ln          Length of note (n=1-64, 1=whole note, 4=quarter note, etc)
 *      Pn          Pause length (same n values as Ln above)
 *      Tn          Tempo, n=notes/minute (n=32-255, default n=120)
 *      On          Octave number (n=0-6, default n=4)
 */

void PlayANSISound(char *pszSounds)
{
   /* Beginning of sound sequence. */
   char szStartSound[255] = {27, '[', '\0'};

   /* Abort if sound is not enabled. */
   if(!bSoundEnabled) return;

   /* Send sequence to start playing sound to remote system only. */
   od_disp(szStartSound, strlen(szStartSound), FALSE);

   /* Send the sounds codes to the remote system only. */
   od_disp(pszSounds, strlen(pszSounds), FALSE);
}
