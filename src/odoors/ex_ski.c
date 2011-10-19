/* EX_SKI.C - EX_SKI is a simple but addictive door game that is written     */
/*            using OpenDoors. In this action game, the player must control  */
/*            a skier through a downhill slalom course. The user may turn    */
/*            the skier left or right, and the game ends as soon as the      */
/*            player skis outside the marked course. The game begins at      */
/*            an easy level, but quickly becomes more and more difficult     */
/*            as the course to be navigated becomes more and more narrow.    */
/*            The game maintains a list of players with high scores, and     */
/*            this list may be viewed from the main menu.                    */
/*                                                                           */
/*            This program shows how to do the following:                    */
/*                                                                           */
/*               - Maintain a high-score file in a game, in a multi-node     */
/*                 compatible manner.                                        */
/*               - How to use your own terminal control sequences.           */
/*               - How to perform reasonably percise timing under both DOS   */
/*                 and Windows.                                              */


/* Header file for the OpenDoors API */
#include "OpenDoor.h"

/* Other required C header files */
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#include "genwrap.h"


/* Hard-coded configurable constants - change these values to alter game */
#define HIGH_SCORES           15       /* Number of high scores in list */
#define INITIAL_COURSE_WIDTH  30       /* Initial width of ski course */
#define MINIMUM_COURSE_WIDTH  4        /* Minimum width of course */
#define DECREASE_WIDTH_AFTER  100      /* # of ticks before course narrows */
#define CHANGE_DIRECTION      10       /* % of ticks course changes direction */
#define MAX_NAME_SIZE         35       /* Maximum characters in player name */
#define WAIT_FOR_FILE         10       /* Time to wait for access to file */
#define SCORE_FILENAME   "skigame.dat" /* Name of high score file */


/* High-score file format structure */
typedef struct
{
   char szPlayerName[MAX_NAME_SIZE + 1];
   DWORD lnHighScore;
   time_t lnPlayDate;
} tHighScoreRecord;

typedef struct
{
   tHighScoreRecord aRecord[HIGH_SCORES];
} tHighScoreFile;


/* Prototypes for functions defined and used in this file */
FILE *OpenAndReadHighScores(tHighScoreFile *pFileContents);
void CloseHighScores(FILE *pfHighScoreFile);
void WriteHighScores(FILE *pfHighScoreFile, tHighScoreFile *pFileContents);
FILE *OpenExclusiveFile(char *pszFileName, char *pszAccess, time_t Wait);
int FileExists(char *pszFileName);
void ShowHighScores(void);
void PlayGame(void);
void SpaceRight(int nColumns);
void MoveLeft(int nColumns);
int AddHighScore(tHighScoreFile *pHighScores, tHighScoreRecord *pScoreRecord);


/* The main() or WinMain() function: program execution begins here. */
#ifdef ODPLAT_WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPSTR lpszCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
   char chMenuChoice;

#ifdef ODPLAT_WIN32
   /* In Windows, pass in nCmdShow value to OpenDoors. */
   od_control.od_cmd_show = nCmdShow;
#endif
   
   /* Set program's name for use by OpenDoors. */
   strcpy(od_control.od_prog_name, "Grand Slalom");
   strcpy(od_control.od_prog_version, "Version 6.00");
   strcpy(od_control.od_prog_copyright, "Copyright 1991-1996 by Brian Pirie");

   /* Call the standard command-line parsing function. You will probably     */
   /* want to do this in most programs that you write using OpenDoors, as it */
   /* automatically provides support for many standard command-line options  */
   /* that will make the use and setup of your program easer. For details,   */
   /* run the vote program with the /help command line option.               */
#ifdef ODPLAT_WIN32
   od_parse_cmd_line(lpszCmdLine);
#else
   od_parse_cmd_line(argc, argv);
#endif

   /* Loop until the user chooses to exit the door */
   do
   {
      /* Clear the screen */
      od_clr_scr();

      /* Display program title */
      od_printf("`bright white`                Ûßß ÛßÜ ÛßÛ ÛÜ Û ÛßÜ   Ûßß Û   ÛßÛ Û   ÛßÛ ÛßÛßÛ\n\r");
      od_printf("`bright red`ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ`bright white`ÛßÛ`bright red`Ä`bright white`ÛßÜ`bright red`Ä`bright white`ÛßÛ`bright red`Ä`bright white`Û`bright red`Ä`bright white`ßÛ`bright red`Ä`bright white`Û");
      od_printf("`bright red`Ä`bright white`Û`bright red`ÄÄÄ`bright white`ßßÛ`bright red`Ä`bright white`Û`bright red`ÄÄÄ`bright white`ÛßÛ`bright red`Ä`bright white`Û`bright red`ÄÄÄ`bright white`Û");
      od_printf("`bright red`Ä`bright white`Û`bright red`Ä`bright white`Û`bright red`Ä`bright white`Û`bright red`Ä`bright white`Û`bright red`ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n\r");
      od_printf("`bright white`                ßßß ß ß ß ß ß  ß ßß    ßßß ßßß ß ß ßßß ßßß ß ß ß\n\r\n\r");

      /* Display instructions */
      od_printf("`dark green`Prepare yourself for the challenge of Grand Slalom downhill skiing!\n\r\n\r");
      od_printf("When `flashing dark green`playing`dark green` the game, press:\n\r");
      od_printf("`dark green`          [`bright green`Q`dark green`] key to ski left\n\r");
      od_printf("          [`bright green`W`dark green`] key to ski right\n\r\n\r");
      od_printf("All that you have to do is ski within the slalom course.\n\r");
      od_printf("It may sound easy - but be warned - it gets harder as you go!\n\r");
      od_printf("(Each time you hear the beep, the course becomes a bit narrower.)\n\r\n\r");

      /* Get menu choice from user. */
      od_printf("`bright white`Now, press [ENTER] to begin game, [H] to view High Scores, [E] to Exit: ");
      chMenuChoice = od_get_answer("HE\n\r");

      /* Perform appropriate action based on user's choice */
      switch(chMenuChoice)
      {
         case '\n':
         case '\r':
            /* If user chooses to play the game */
            PlayGame();
            break;

         case 'H':
            /* If user chose to view high scores */
            ShowHighScores();
            break;

         case 'E':
            /* If user chose to return to BBS */
            od_printf("\n\rGoodbye from SKIGAME!\n\r");
            break;
      }
   } while(chMenuChoice != 'E');

   /* Exit door at errorlevel 10, and do not hang up */
   od_exit(10, FALSE);
   return(1);
}


/* OpenAndReadHighScores() - Opens high score file and reads contents. If */
/*                           file does not exist, it is created. File is  */
/*                           locked to serialize access by other nodes on */
/*                           this system.                                 */
FILE *OpenAndReadHighScores(tHighScoreFile *pFileContents)
{
   FILE *pfFile;
   int iHighScore;

   /* If high score file does not exist */
   if(!FileExists(SCORE_FILENAME))
   {
      /* Open and create it */
      pfFile = OpenExclusiveFile(SCORE_FILENAME, "wb", WAIT_FOR_FILE);

      /* If open was successful */
      if(pfFile != NULL)
      {
         /* Initialize new high score list */
         for(iHighScore = 0; iHighScore < HIGH_SCORES; ++iHighScore)
         {
            pFileContents->aRecord[iHighScore].lnHighScore = 0L;
         }

         /* Write high score list to the file */
         WriteHighScores(pfFile, pFileContents);
      }
   }

   /* If high score file does exit */
   else
   {
      /* Open the existing file */
      pfFile = OpenExclusiveFile(SCORE_FILENAME, "r+b",
                                WAIT_FOR_FILE);

      /* Read the contents of the file */
      if(fread(pFileContents, sizeof(tHighScoreFile), 1, pfFile) != 1)
      {
         /* If unable to read file, then return with an error */
         fclose(pfFile);
         pfFile = NULL;
      }
   }

   /* Return pointer to high score file, if avilable */
   return(pfFile);
}


/* FileExists() - Returns TRUE if file exists, otherwise returns FALSE */
int FileExists(char *pszFileName)
{
   /* Attempt to open the specified file for reading. */
   FILE *pfFile = OpenExclusiveFile(pszFileName, "rb", WAIT_FOR_FILE);

   if(pfFile != NULL)
   {
      /* If we are able to open the file, then close it and return */
      /* indicating that it exists.                                */
      fclose(pfFile);
      return(TRUE);
   }
   else
   {
      /* If we are unable to open the file, we proceed as if the file        */
      /* doesn't exist (note that this may not always be a valid assumption) */
      return(FALSE);
   }
}


/* OpenExclusiveFile() - Opens a file for exclusive access, waiting if the */
/*                       file is not currently available.                  */
FILE *OpenExclusiveFile(char *pszFileName, char *pszAccess, time_t Wait)
{
   FILE *pfFile;
   time_t StartTime = time(NULL);

   for(;;)
   {
      /* Attempt to open file */
      pfFile = fopen(pszFileName, pszAccess);

      /* If file was opened successfuly, then exit */
      if(pfFile != NULL) break;

      /* If open failed, but not due to access failure, then exit */
      if(errno != EACCES) break;

      /* If maximum time has elapsed, then exit */
      if(StartTime + Wait < time(NULL)) break;

      /* Give the OpenDoors kernel a chance to execute before trying again */
      od_kernel();
   }

   /* Return pointer to file, if opened */
   return(pfFile);
}


/* CloseHighScores() - Closes the high score file, allowing other nodes on */
/*                     system to access it.                                */
void CloseHighScores(FILE *pfHighScoreFile)
{
   if(pfHighScoreFile != NULL)
   {
      fclose(pfHighScoreFile);
   }
}


/* WriteHighScores() - Writes the information from pFileContents to the */
/*                     high score file.                                 */
void WriteHighScores(FILE *pfHighScoreFile, tHighScoreFile *pFileContents)
{
   if(pfHighScoreFile != NULL)
   {
      fseek(pfHighScoreFile, 0L, SEEK_SET);
      fwrite(pFileContents, sizeof(tHighScoreFile), 1, pfHighScoreFile);
   }
}


/* ShowHighScores() - Called From DoDoor() to display list of high scores */
void ShowHighScores(void)
{
   FILE *pfFile;
   tHighScoreFile HighScores;
   int iHighScore;
   struct tm *pTimeBlock;
   char szTimeString[34];

   /* Clear the screen */
   od_clr_scr();

   /* Attempt to read high scores from file */
   pfFile = OpenAndReadHighScores(&HighScores);
   CloseHighScores(pfFile);

   if(pfFile == NULL)
   {
      /* If unable to open high score file, display an error message */
      od_printf("`bright red`Unable to access high score file!\n\r");
   }
   else
   {
      /* Display header line */
      od_printf("`bright green`Player                            Score     "
                "Record Date`dark green`\n\r");
      od_printf("ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n\r");

      /* Display high scores */
      for(iHighScore = 0; iHighScore < HIGH_SCORES; ++iHighScore)
      {
         /* Exit loop when we have reached the end of the high scores */
         if(HighScores.aRecord[iHighScore].lnHighScore == 0L) break;

         /* Get local time when player set the high score */
         pTimeBlock = localtime(&HighScores.aRecord[iHighScore].lnPlayDate);
         strftime(szTimeString, sizeof(szTimeString),
            "%B %d, %Y at %I:%M%p", pTimeBlock);

         /* Display next high score */
         od_printf("%-32.32s  %-8ld  %s\n\r",
                   HighScores.aRecord[iHighScore].szPlayerName,
                   HighScores.aRecord[iHighScore].lnHighScore,
                   szTimeString);
      }
   }

   /* Display footer line */
   od_printf("ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n\r\n\r");

   /* Wait for user to press a key */
   od_printf("`bright white`Press [ENTER]/[RETURN] to continue: ");
   od_get_answer("\n\r");
}


/* PlayGame() - Called from DoDoor() when user chooses to play a game. */
void PlayGame(void)
{
   int nLeftEdge = 1;
   int nRightEdge = nLeftEdge + 1 + INITIAL_COURSE_WIDTH;
   int nPlayerPos = nLeftEdge + 1 + (INITIAL_COURSE_WIDTH / 2);
   long lnScore = 0;
   int nDistanceSinceShrink = 0;
   int bMovingRight = TRUE;
   char cKeyPress;
   tHighScoreRecord ScoreRecord;
   FILE *pfFile;
   tHighScoreFile HighScores;
   int nBackup=0;
   clock_t StartClock;

   /* Clear the Screen */
   od_set_color(L_WHITE, B_BLACK);
   od_clr_scr();

   /* Set current display colour to white */
   od_set_attrib(L_WHITE);

   /* Re-seed random number generator */
   srand((unsigned int)time(NULL));

   /* Loop until game is over */
   for(;;)
   {
      StartClock = msclock();

      /* Display current line */
      if(od_control.user_ansi || od_control.user_avatar)
      {
         SpaceRight(nLeftEdge - 1);
         od_set_color(L_WHITE, D_RED);
         od_putch((char)223);
         od_repeat((unsigned char)219, 
            (unsigned char)(nPlayerPos - nLeftEdge - 1));
         od_putch((char)254);
         od_repeat((unsigned char)219,
            (unsigned char)(nRightEdge - nPlayerPos - 1));
         od_putch((char)223);
         nBackup = nRightEdge - nPlayerPos + 1;
      }
      else
      {
         /* If neither ANSI nor AVATAR modes are active, then display */
         /* course using plain-ASCII.                                 */
         SpaceRight(nLeftEdge - 1);
         od_putch((char)(bMovingRight ? '\\' : '/'));
         SpaceRight(nPlayerPos - nLeftEdge - 1);
         od_putch('o');
         SpaceRight(nRightEdge - nPlayerPos - 1);
         od_putch((char)(bMovingRight ? '\\' : '/'));
      }

      /* Loop for each key pressed by user */
      while((cKeyPress = (char)od_get_key(FALSE)) != '\0')
      {
         if(cKeyPress == 'q' || cKeyPress == 'Q')
         {
            /* Move left */
            --nPlayerPos;
         }
         else if(cKeyPress == 'w' || cKeyPress == 'W')
         {
            /* Move right */
            ++nPlayerPos;
         }
      }

      /* Check whether course should turn */
      if((rand() % 100) < CHANGE_DIRECTION)
      {
         bMovingRight = !bMovingRight;
      }
      else
      {
         /* If no change in direction, then position moves */
         /* Adjust course position appropriately */
         if(bMovingRight)
         {
            ++nLeftEdge;
            ++nRightEdge;
         }
         else
         {
            --nLeftEdge;
            --nRightEdge;
         }
      }

      /* Check whether course size should shink */
      if(++nDistanceSinceShrink >= DECREASE_WIDTH_AFTER)
      {
         /* Reset distance */
         nDistanceSinceShrink = 0;

         /* Randomly choose a side to shrink */
         if((rand() % 100) < 50)
         {
            ++nLeftEdge;
         }
         else
         {
            --nRightEdge;
         }

         /* Beep when we shrink the size. */
         od_printf("\a");
      }

      /* Change course direction if it collides with edge of screen */
      if(nLeftEdge < 1)
      {
         bMovingRight = TRUE;
         ++nLeftEdge;
         ++nRightEdge;
      }
      else if(nRightEdge > 79)
      {
         bMovingRight = FALSE;
         --nLeftEdge;
         --nRightEdge;
      }

      /* Check that player is still within the course */
      if(nPlayerPos <= nLeftEdge || nPlayerPos >= nRightEdge)
      {
         /* Player has left course - game over! */
         od_set_color(D_GREY, D_BLACK);
         od_clr_scr();
         od_printf("`bright red`       !!! Game Over !!!\n\r\n\r");
         od_printf("`dark green`You have veered off the course!\n\r\n\r");
         od_printf("Your Score is: %ld\n\r", lnScore);

         /* Create a score record */
         ScoreRecord.lnHighScore = lnScore;
         strncpy(ScoreRecord.szPlayerName, od_control.user_name, MAX_NAME_SIZE);
         ScoreRecord.szPlayerName[MAX_NAME_SIZE] = '\0';
         ScoreRecord.lnPlayDate = time(NULL);

         /* Attempt to read high scores from file */
         pfFile = OpenAndReadHighScores(&HighScores);

         if(pfFile == NULL)
         {
            /* If unable to open high score file, display an error message */
            od_printf("`bright red`Unable to access high score file!\n\r");
         }
         else
         {
            /* Check whether user made it to high score list */
            if(AddHighScore(&HighScores, &ScoreRecord))
            {
               od_printf("Congratulations! You have made it to the high score list!\n\r");
               /* If so, write the new high score list */
               WriteHighScores(pfFile, &HighScores);
            }

            /* Close and unlock file */
            CloseHighScores(pfFile);
         }

         /* Wait for user to press enter */
         od_printf("`bright white`\n\rPress [ENTER]/[RETURN] to return to menu: ");
         od_get_answer("\n\r");

         return;
      }

      /* Delay for about 1/10th of a second, to add a constant delay after */
      /* each line is displayed that does not depend on the connect speed. */
      while(msclock() < StartClock + (((clock_t)MSCLOCKS_PER_SEC) / 10))
         od_sleep(0);

      /* Increase score */
      ++lnScore;

      /* Replace skiier character with track character */
      if(od_control.user_ansi)
      {
         MoveLeft(nBackup);
         od_set_color(L_WHITE, D_GREY);
         od_putch((char)178);
         od_set_color(L_WHITE, B_BLACK);
      }

      /* Move to next line */
      od_printf("\r\n");
   }
}


/* SpaceRight() - Moves right the specified number of columns. In ANSI mode, */
/*                uses the move cursor right control sequence. Otherwise,    */
/*                uses od_repeat(), which is optimized for ASCII and AVATAR  */
/*                modes.                                                     */
void SpaceRight(int nColumns)
{
   char szSequence[6];

   /* If we don't have a positive column count, then return immediately */
   if(nColumns <= 0) return;

   /* If operating in ANSI mode */
   if(od_control.user_ansi)
   {
      /* Move cursor right using ESC[nC control sequence */
      sprintf(szSequence, "\x1b[%02dC", nColumns);
      od_disp_emu(szSequence, TRUE);
   }

   /* If not operating in ANSI mode */
   else
   {
      od_repeat(' ', (unsigned char)nColumns);
   }
}


/* MoveLeft() - Moves the cursor right the specified number of columns. */
/*              Intended for use in ANSI mode only.                     */
void MoveLeft(int nColumns)
{
   /* Move cursor left using ESC[nD control sequence */
   char szSequence[6];
   sprintf(szSequence, "\x1b[%02dD", nColumns);
   od_disp_emu(szSequence, TRUE);
}


/* AddHighScore() - Adds a new score to the high score list, if it is high   */
/*                  enough. Returns TRUE if score is added, FALSE otherwise. */
int AddHighScore(tHighScoreFile *pHighScores, tHighScoreRecord *pScoreRecord)
{
   int iHighScore;
   int iExistingScore;

   /* Loop through each existing high score */
   for(iHighScore = 0; iHighScore < HIGH_SCORES; ++iHighScore)
   {
      /* If new score is greater than or equal to this one, then its */
      /* position has been found.                                    */
      if(pHighScores->aRecord[iHighScore].lnHighScore <=
         pScoreRecord->lnHighScore)
      {
         /* Move remaining scores down one in list */
         for(iExistingScore = HIGH_SCORES - 1; iExistingScore >= iHighScore + 1;
          --iExistingScore)
         {
            pHighScores->aRecord[iExistingScore] =
               pHighScores->aRecord[iExistingScore - 1];
         }

         /* Add new score to list */
         pHighScores->aRecord[iHighScore] = *pScoreRecord;

         /* Return with success */
         return(TRUE);
      }
   }

   /* Score did not make it to list */
   return(FALSE);
}
