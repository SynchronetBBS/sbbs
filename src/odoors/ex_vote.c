/* EX_VOTE.C - This program demonstrates an online voting program that is    */
/*             written using OpenDoors. The Vote program allows users to     */
/*             create questions or surveys for other users to respond to.    */
/*             Users are also able to view the results of voting on each     */
/*             topic. The program supports up to 200 questions, and can be   */
/*             configured to either allow or disallow viewing of results     */
/*             prior to voting.                                              */
/*                                                                           */
/*             This program shows how to do the following:                   */
/*                                                                           */
/*                - How to display text using od_printf(), using imbedded    */
/*                  strings to change the display color.                     */
/*                - Add support for standard command-line options.           */
/*                - Use the OpenDoors configuration file system, including   */
/*                  adding your own configuration file options.              */
/*                - Activate the OpenDoors log file system and write         */
/*                  information to the log file.                             */
/*                - Display a menu from an external ASCI/ANSI/Avatar/RIP     */
/*                  file.                                                    */
/*                - How to setup a user file and general data file, and how  */
/*                  to access it in a multi-node compatible way (if          */
/*                  MULTINODE_AWARE is defined).                             */
/*                                                                           */
/*             To recompile this program, follow the instructions in the     */
/*             OpenDoors manual. For a DOS compiler, be sure to set your     */
/*             compiler to use the large memory model, and add the           */
/*             ODOORL.LIB file to your project/makefile.                     */

/* Uncomment the following line for multi-node compatible file access. */
/* #define MULTINODE_AWARE */

/* Include standard C header files required by Vote. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

/* Include the OpenDoors header file. This line must be done in any program */
/* using OpenDoors.                                                         */
#include "OpenDoor.h"

#ifdef MULTINODE_AWARE
#include <filewrap.h>
#endif
#include "genwrap.h"

/* Manifest constants used by Vote */
#define NO_QUESTION              -1
#define NEW_ANSWER               -1

#define QUESTIONS_VOTED_ON        0x0001
#define QUESTIONS_NOT_VOTED_ON    0x0002

#define MAX_QUESTIONS             200
#define MAX_USERS                 30000
#define MAX_ANSWERS               15
#define QUESTION_STR_SIZE         71
#define ANSWER_STR_SIZE           31

#define USER_FILENAME             "vote.usr"
#define QUESTION_FILENAME         "vote.qst"

#define FILE_ACCESS_MAX_WAIT      20

#define QUESTION_PAGE_SIZE        17


/* Structure of records stored in the VOTE.USR file */
typedef struct
{
   char szUserName[36];
   BYTE bVotedOnQuestion[MAX_QUESTIONS];
} tUserRecord;
              
tUserRecord CurrentUserRecord;
int nCurrentUserNumber;


/* Structure of records stored in the VOTE.QST file */
typedef struct
{
   char szQuestion[72];
   char aszAnswer[MAX_ANSWERS][32];
   INT32 nTotalAnswers;
   DWORD auVotesForAnswer[MAX_ANSWERS];
   DWORD uTotalVotes;
   DWORD bCanAddAnswers;
   char szCreatorName[36];
   time_t lCreationTime;
} tQuestionRecord;


/* Global variables. */
int nViewResultsFrom = QUESTIONS_VOTED_ON;
int nQuestionsVotedOn = 0;


/* Prototypes for functions that form EX_VOTE */
void CustomConfigFunction(char *pszKeyword, char *pszOptions);
void BeforeExitFunction(void);
void VoteOnQuestion(void);
void ViewResults(void);
int GetQuestion(int nQuestion, tQuestionRecord *pQuestionRecord);
void AddQuestion(void);
int ChooseQuestion(int nFromWhichQuestions, char *pszTitle, int *nLocation);
void DisplayQuestionResult(tQuestionRecord *pQuestionRecord);
int ReadOrAddCurrentUser(void);
void WriteCurrentUser(void);
FILE *ExclusiveFileOpen(char *pszFileName, char *pszMode, int *phHandle);
void ExclusiveFileClose(FILE *pfFile, int hHandle);
void WaitForEnter(void);


/* main() or WinMain() function - Program execution begins here. */
#ifdef ODPLAT_WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPSTR lpszCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
   /* Variable to store user's choice from the menu */
   char chMenuChoice = '\0';
   char chYesOrNo;

#ifdef ODPLAT_WIN32
   /* In Windows, pass in nCmdShow value to OpenDoors. */
   od_control.od_cmd_show = nCmdShow;

   /* Ignore unused parameters. */
   (void)hInstance;
   (void)hPrevInstance;
#endif
   
   /* Set program's name for use by OpenDoors. */
   strcpy(od_control.od_prog_name, "Vote");
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

   /* Enable use of OpenDoors configuration file system. */
   od_control.od_config_file = INCLUDE_CONFIG_FILE;

   /* Set function to process custom configuration file lines. */
   od_control.od_config_function = CustomConfigFunction;

   /* Include the OpenDoors multiple personality system, which allows    */
   /* the system operator to set the sysop statusline / function key set */
   /* to mimic the BBS software of their choice.                         */
   od_control.od_mps = INCLUDE_MPS;
   
   /* Include the OpenDoors log file system, which will record when the */
   /* door runs, and major activites that the user performs.            */ 
   od_control.od_logfile = INCLUDE_LOGFILE;

   /* Set filename for log file. If not set, DOOR.LOG will be used by */
   /* default.                                                        */
   strcpy(od_control.od_logfile_name, "vote.log");

   /* Set function to be called before program exits. */
   od_control.od_before_exit = BeforeExitFunction;

   /* Initialize OpenDoors. This function call is optional, and can be used */
   /* to force OpenDoors to read the door informtion file and begin door    */
   /* operations. If a call to od_init() is not included in your program,   */
   /* OpenDoors initialization will be performed at the time of your first  */
   /* call to any OpenDoors function. */
   od_init();

   /* Call the Vote function ReadOrAddCurrentUser() to read the current   */
   /* user's record from the Vote user file, or to add the user to the    */
   /* file if this is the first time that they have used Vote.            */
   if(!ReadOrAddCurrentUser())
   {
      /* If unable to obtain a user record for the current user, then exit */
      /* the door after displaying an error message.                       */
      od_printf("Unable to access user file. File may be locked or full.\n\r");
      WaitForEnter();
      od_exit(1, FALSE);
   }   

   /* Loop until the user choses to exit the door. For each iteration of  */
   /* this loop, we display the main menu, get the user's choice from the */
   /* menu, and perform the appropriate action for their choice.          */

   while(chMenuChoice != 'E' && chMenuChoice != 'H')
   {
      /* Clear the screen */
      od_clr_scr();

      /* Display main menu. */
      
      /* First, attempt to display menu from an VOTE.ASC/ANS/AVT/RIP file. */
      if((chMenuChoice = od_hotkey_menu("VOTE", "VRADPEH", TRUE)) == 0)
      {
         /* If the VOTE file could not be displayed, display our own menu. */
         od_printf("`bright red`                     Vote - OpenDoors 6.00 example program\n\r");\
         od_printf("`dark red`");
         if(od_control.user_ansi || od_control.user_avatar)
         {
            od_repeat((unsigned char)196, 79);
         }
         else
         {
            od_repeat('-', 79);
         }
         od_printf("\n\r\n\r\n\r`dark green`");
         od_printf("                        [`bright green`V`dark green`] Vote on a question\n\r\n\r");
         od_printf("                        [`bright green`R`dark green`] View the results of question\n\r\n\r");
         od_printf("                        [`bright green`A`dark green`] Add a new question\n\r\n\r");
         od_printf("                        [`bright green`P`dark green`] Page system operator for chat\n\r\n\r");
         od_printf("                        [`bright green`E`dark green`] Exit door and return to the BBS\n\r\n\r");
         od_printf("                        [`bright green`H`dark green`] End call (hangup)\n\r\n\r\n\r");
         od_printf("`bright white`Press the key corresponding to the option of your choice. (%d mins)\n\r`dark green`",
            od_control.user_timelimit);
                                                                                                       \
         /* Get the user's choice from the main menu. This choice may only be */
         /* V, R, A, D, P, E or H.                                            */
         chMenuChoice = od_get_answer("VRADPEH");
      }

      /* Perform the appropriate action based on the user's choice */
      switch(chMenuChoice)
      {
         case 'V':
            /* Call Vote's function to vote on question */
            VoteOnQuestion();
            break;
            
         case 'R':
            /* Call Vote's function to view the results of voting */
            ViewResults();
            break;
            
         case 'A':
            /* Call Vote's function to add a new question. */
            AddQuestion();
            break;
            
         case 'P':
            /* If the user pressed P, allow them page the system operator. */
            od_page();
            break;

         case 'H':
            /* If the user pressed H, ask whether they wish to hangup. */
            od_printf("\n\rAre you sure you wish to hangup? (Y/N) ");

            /* Get user's response */
            chYesOrNo = od_get_answer("YN");

            if(chYesOrNo == 'N')
            {
               /* If user answered no, then reset menu choice, so that */
               /* program will not exit.                               */
               chMenuChoice = '\0';
            }
            break;
      }
   }

   if(chMenuChoice == 'H')
   {
      /* If the user chooses to hangup, then hangup when exiting. */
      od_exit(0, TRUE);
   }
   else
   {
      /* Otherwise, exit normally (without hanging up). */
      od_printf("Returning to BBS, please wait...\n\r");
      od_exit(0, FALSE);
   }

   return(0);
}


/* CustomConfigFunction() is called by OpenDoors to process custom */
/* configuration file keywords that Vote uses.                     */
void CustomConfigFunction(char *pszKeyword, char *pszOptions)
{
   if(stricmp(pszKeyword, "ViewUnanswered") == 0)
   {
      /* If keyword is ViewUnanswered, set local variable based on contents */
      /* of options string.                                                 */
      if(stricmp(pszOptions, "Yes") == 0)
      {
         nViewResultsFrom = QUESTIONS_VOTED_ON | QUESTIONS_NOT_VOTED_ON;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
         nViewResultsFrom = QUESTIONS_VOTED_ON;
      }
   }
}


/* Vote configures OpenDoors to call the BeforeExitFunction() before      */
/* the door exists for any reason. You can use this function to close any */
/* files or perform any other operations that you wish to have peformed   */
/* before OpenDoors exists for any reason. The od_control.od_before_exit  */
/* variable sets the function to be called before program exit.           */
void BeforeExitFunction(void)
{
   char szLogMessage[80];
   
   /* Write number of messages voted on to log file. */
   sprintf(szLogMessage, "User has voted on %d question(s)",
      nQuestionsVotedOn);
   od_log_write(szLogMessage);
}


/* Vote calls the VoteOnQuestion() function when the user chooses the     */
/* vote command from the main menu. This function displays a list of      */
/* available topics, asks for the user's answer to the topic they select, */
/* and display's the results of voting on that topic.                     */
void VoteOnQuestion(void)
{
   int nQuestion;
   int nAnswer;
   tQuestionRecord QuestionRecord;
   char szNewAnswer[ANSWER_STR_SIZE];
   char szUserInput[3];
   FILE *fpFile;
   int hFile;
   int nPageLocation = 0;

   /* Loop until the user chooses to return to the main menu, or until */
   /* there are no more questions to vote on.                          */
   for(;;)   
   {
      /* Allow the user to choose a question from the list of questions */
      /* that they have not voted on.                                   */
      nQuestion = ChooseQuestion(QUESTIONS_NOT_VOTED_ON,
         "                              Vote On A Question\n\r",
         &nPageLocation);
   

      /* If the user did not choose a question, return to main menu. */   
      if(nQuestion == NO_QUESTION)
      {
         return;
      }

      /* Read the question chosen by the user. */
      if(!GetQuestion(nQuestion, &QuestionRecord))
      {
         /* If unable to access file, return to main menu. */
         return;
      }
   
      /* Don't allow addition of new answers if maximum number of answers */
      /* have already been added.                                         */
      if(QuestionRecord.nTotalAnswers >= MAX_ANSWERS)
      {
         QuestionRecord.bCanAddAnswers = FALSE;
      }
   
      /* Loop until user makes a valid respose. */
      for(;;)
      {
         /* Display question to user. */

         /* Clear the screen. */
         od_clr_scr();

         /* Display question itself. */
         od_printf("`bright red`%s\n\r\n\r", QuestionRecord.szQuestion);

         /* Loop for each answer to the question. */   
         for(nAnswer = 0; nAnswer < QuestionRecord.nTotalAnswers; ++nAnswer)
         {
            /* Display answer number and answer. */
            od_printf("`bright green`%d. `dark green`%s\n\r",
               nAnswer + 1,
               QuestionRecord.aszAnswer[nAnswer]);
         }

         /* Display prompt to user. */
         od_printf("\n\r`bright white`Enter answer number, ");
         if(QuestionRecord.bCanAddAnswers)
         {
            od_printf("[A] to add your own response, ");
         }
         od_printf("[Q] to quit: `dark green`");
   
         /* Get response from user. */
         od_input_str(szUserInput, 2, ' ', 255);
         /* Add a blank line. */      
         od_printf("\n\r");
   
         /* If user entered Q, return to main menu. */
         if(stricmp(szUserInput, "Q") == 0)
         {
            return;
         }

         /* If user enetered A, and adding answers is premitted ... */
         else if (stricmp(szUserInput, "A") == 0
            && QuestionRecord.bCanAddAnswers)
         {
            /* ... Prompt for answer from user. */
            od_printf("`bright green`Please enter your new answer:\n\r");
            od_printf("`dark green`[------------------------------]\n\r ");
         
            /* Get string from user. */
            od_input_str(szNewAnswer, ANSWER_STR_SIZE - 1, ' ', 255);
         
            /* Record that user entered a new answer answer. */
            nAnswer = NEW_ANSWER;

            /* If user entered a valid answer, then exit loop. */
            if(strlen(szNewAnswer) > 0)
            {
               break;
            }         
         }

         /* Otherwise, attempt to get answer number from user. */      
         nAnswer = atoi(szUserInput) - 1;

         /* If user input is not a valid answer. */      
         if(nAnswer < 0 || nAnswer >= QuestionRecord.nTotalAnswers)
         {
            /* Display message. */
            od_printf("That is not a valid response.\n\r");
            WaitForEnter();
         }
         else
         {
            /* Otherwise, exit loop. */
            break;
         }
      }

      /* Add user's vote to question. */
   
      /* Open question file for exclusive access by this node. */
      fpFile = ExclusiveFileOpen(QUESTION_FILENAME, "r+b", &hFile);
      if(fpFile == NULL)
      {
         /* If unable to access file, display error and return. */
         od_printf("Unable to access the question file.\n\r");
         WaitForEnter();
         return;
      }
   
      /* Read the answer record from disk, because it may have been changed. */
      /* by another node. */
      fseek(fpFile, (long)nQuestion * sizeof(tQuestionRecord), SEEK_SET);
      if(fread(&QuestionRecord, sizeof(tQuestionRecord), 1, fpFile) != 1)
      {
         /* If unable to access file, display error and return. */
         ExclusiveFileClose(fpFile, hFile);
         od_printf("Unable to read from question file.\n\r");
         WaitForEnter();
         return;
      }
   
      /* If user entered their own answer, try to add it to the question. */
      if(nAnswer == NEW_ANSWER)
      {
         /* Check that there is still room for another answer. */
         if(QuestionRecord.nTotalAnswers >= MAX_ANSWERS)
         {
            ExclusiveFileClose(fpFile, hFile);
            od_printf("Sorry, this question already has the maximum number of answers.\n\r");
            WaitForEnter();
            return;
         }
      
         /* Set answer number to number of new answer. */
         nAnswer = QuestionRecord.nTotalAnswers;
      
         /* Add 1 to total number of answers. */
         ++QuestionRecord.nTotalAnswers;
      
         /* Initialize new answer string and count. */
         strcpy(QuestionRecord.aszAnswer[nAnswer], szNewAnswer);
         QuestionRecord.auVotesForAnswer[nAnswer] = 0;
      }
   
      /* Add user's vote to question. */
      ++QuestionRecord.auVotesForAnswer[nAnswer];
      ++QuestionRecord.uTotalVotes;
   
      /* Write the question record back to the file. */
      fseek(fpFile, (long)nQuestion * sizeof(tQuestionRecord), SEEK_SET);
      if(fwrite(&QuestionRecord, sizeof(tQuestionRecord), 1, fpFile) != 1)
      {
         /* If unable to access file, display error and return. */
         ExclusiveFileClose(fpFile, hFile);
         od_printf("Unable to write question to file.\n\r");
         WaitForEnter();
         return;
      }
   
      /* Close the question file to allow access by other nodes. */
      ExclusiveFileClose(fpFile, hFile);
   
      /* Record that user has voted on this question. */
      CurrentUserRecord.bVotedOnQuestion[nQuestion] = TRUE;
   
      /* Open user file for exclusive access by this node. */
      fpFile = ExclusiveFileOpen(USER_FILENAME, "r+b", &hFile);
      if(fpFile == NULL)
      {
         /* If unable to access file, display error and return. */
         od_printf("Unable to access the user file.\n\r");
         WaitForEnter();
         return;
      }

      /* Update the user's record in the user file. */
      fseek(fpFile, nCurrentUserNumber * sizeof(tUserRecord), SEEK_SET);
      if(fwrite(&CurrentUserRecord, sizeof(tUserRecord), 1, fpFile) != 1)
      {
         /* If unable to access file, display error and return. */
         ExclusiveFileClose(fpFile, hFile);
         od_printf("Unable to write to user file.\n\r");
         WaitForEnter();
         return;
      }
   
      /* Close the user file to allow access by other nodes. */
      ExclusiveFileClose(fpFile, hFile);
   
      /* Display the result of voting on this question to the user. */
      DisplayQuestionResult(&QuestionRecord);

      /* Add 1 to count of questions that the user has voted on. */
      nQuestionsVotedOn++;
   }
}


/* The ViewResults function is called when the user chooses the "view    */
/* results" command from the main menu. This function alows the user to  */
/* choose a question from the list of questions, and then displays the   */
/* results of voting on that question.                                   */
void ViewResults(void)
{
   int nChoice;
   tQuestionRecord QuestionRecord;
   int nPageLocation = 0;

   /* Loop until user chooses to return to main menu. */
   for(;;)
   {   
      /* Allow the user to choose a question from the list of questions that */
      /* they have already voted on.                                         */
      nChoice = ChooseQuestion(nViewResultsFrom,
         "                                 View Results\n\r", &nPageLocation);

      /* If the user did not choose a question, return to main menu. */   
      if(nChoice == NO_QUESTION)
      {
         return;
      }
   
      /* Read the specified question number from the question file. */
      if(!GetQuestion(nChoice, &QuestionRecord))
      {
         return;
      }
   
      /* Display the results for the selected question. */
      DisplayQuestionResult(&QuestionRecord);
   }
}


/* The GetQuestion function read the record for the specified question */
/* number from the question file.                                      */
int GetQuestion(int nQuestion, tQuestionRecord *pQuestionRecord)
{
   FILE *fpQuestionFile;
   int hQuestionFile;

   /* Open the question file for exculsive access by this node. */
   fpQuestionFile = ExclusiveFileOpen(QUESTION_FILENAME, "r+b",
      &hQuestionFile);
   if(fpQuestionFile == NULL)
   {
      /* If unable to access file, display error and return. */
      od_printf("Unable to access the question file.\n\r");
      WaitForEnter();
      return(FALSE);
   }
   
   /* Move to location of question in file. */
   fseek(fpQuestionFile, (long)nQuestion * sizeof(tQuestionRecord), SEEK_SET);
   
   /* Read the question from the file. */
   if(fread(pQuestionRecord, sizeof(tQuestionRecord), 1, fpQuestionFile) != 1)
   {
      /* If unable to access file, display error and return. */
      ExclusiveFileClose(fpQuestionFile, hQuestionFile);
      od_printf("Unable to read from question file.\n\r");
      WaitForEnter();
      return(FALSE);;
   }
   
   /* Close the question file to allow access by other nodes. */
   ExclusiveFileClose(fpQuestionFile, hQuestionFile);

   /* Return with success. */
   return(TRUE);
}
 

/* The AddQuestion() function is called when the user chooses the "add    */
/* question" option from the main menu. This function allows the user     */
/* to enter a new question, possible responses, and save the question for */
/* other users to vote on.                                                */
void AddQuestion(void)
{
   tQuestionRecord QuestionRecord;
   FILE *fpQuestionFile;
   int hQuestionFile;
   char szLogMessage[100];

   /* Clear the screen. */
   od_clr_scr();
   
   /* Display screen header. */
   od_printf("`bright red`                                Add A Question\n\r");
   od_printf("`dark red`");
   if(od_control.user_ansi || od_control.user_avatar)
   {
      od_repeat((unsigned char)196, 79);
   }
   else
   {
      od_repeat('-', 79);
   }
   od_printf("\n\r\n\r");
   
   /* Obtain quesiton text from the user. */
   od_printf("`bright green`Enter Your Question (blank line cancels)\n\r");
   od_printf("`dark green`[----------------------------------------------------------------------]\n\r ");
   od_input_str(QuestionRecord.szQuestion, QUESTION_STR_SIZE - 1, ' ', 255);
   
   /* If question was empty, then return to main menu. */
   if(strlen(QuestionRecord.szQuestion) == 0)
   {
      return;
   }
   
   /* Display prompt for answers. */
   od_printf("\n\r`bright green`Enter Possible Answers (blank line when done)\n\r");
   od_printf("`dark green`   [------------------------------]\n\r");
   
   /* Loop, getting answers from user. */
   for(QuestionRecord.nTotalAnswers = 0;
       QuestionRecord.nTotalAnswers < MAX_ANSWERS;
       QuestionRecord.nTotalAnswers++)
   {
      /* Display prompt with answer number. */
      od_printf("`bright green`%2d: `dark green`", QuestionRecord.nTotalAnswers + 1);
      
      /* Get string from user. */
      od_input_str(QuestionRecord.aszAnswer[QuestionRecord.nTotalAnswers],
         ANSWER_STR_SIZE - 1, ' ', 255);
         
      /* If string was empty, then exit loop. */
      if(strlen(QuestionRecord.aszAnswer[QuestionRecord.nTotalAnswers]) == 0)
      {
         break;
      }
      
      /* Reset count of votes for this answer to zero. */
      QuestionRecord.auVotesForAnswer[QuestionRecord.nTotalAnswers] = 0;
   }
   
   /* If no answers were supplied, then cancel, returning to main menu. */
   if(QuestionRecord.nTotalAnswers == 0)
   {
      return;
   }

   /* Ask whether users should be able to add their own answers. */
   od_printf("\n\r`bright green`Should voters be able to add their own options? (Y/N) `dark green`");
   
   /* Get answer from user. */
   if(od_get_answer("YN") == 'Y')
   {
      /* If user pressed the 'Y' key. */
      od_printf("Yes\n\r\n\r");
      
      /* Record user's response. */
      QuestionRecord.bCanAddAnswers = TRUE;
   }
   else
   {
      /* If user pressed the 'N' key. */
      od_printf("No\n\r\n\r");

      /* Record user's response. */
      QuestionRecord.bCanAddAnswers = FALSE;
   }
   
   /* Confirm save of new question. */
   od_printf("`bright green`Do you wish to save this new question? (Y/N) `dark green`");
   
   /* If user does not want to save the question, return to main menu now. */
   if(od_get_answer("YN") == 'N')
   {
      return;
   }

   /* Set total number of votes for this question to 0. */   
   QuestionRecord.uTotalVotes = 0;
   
   /* Set creator name and creation time for this question. */
   strcpy(QuestionRecord.szCreatorName, od_control.user_name);
   QuestionRecord.lCreationTime = time(NULL);
   
   /* Open question file for exclusive access by this node. */
   fpQuestionFile = ExclusiveFileOpen(QUESTION_FILENAME, "a+b",
      &hQuestionFile);
   if(fpQuestionFile == NULL)
   {
      od_printf("Unable to access the question file.\n\r");
      WaitForEnter();
      return;
   }
   
   /* Determine number of records in question file. */
   fseek(fpQuestionFile, 0, SEEK_END);
   
   /* If question file is full, display message and return to main menu */
   /* after closing file.                                               */
   if(ftell(fpQuestionFile) / sizeof(tQuestionRecord) >= MAX_QUESTIONS)
   {
      ExclusiveFileClose(fpQuestionFile, hQuestionFile);
      od_printf("Cannot add another question, Vote is limited to %d questions.\n\r", MAX_QUESTIONS);
      WaitForEnter();
      return;
   }
   
   /* Add new question to file. */
   if(fwrite(&QuestionRecord, sizeof(QuestionRecord), 1, fpQuestionFile) != 1)
   {
      ExclusiveFileClose(fpQuestionFile, hQuestionFile);
      od_printf("Unable to write to question file.\n\r");
      WaitForEnter();
      return;
   }
   
   /* Close question file, allowing other nodes to access file. */
   ExclusiveFileClose(fpQuestionFile, hQuestionFile);

   /* Record in the logfile that user has added a new question. */
   sprintf(szLogMessage, "User adding questions: %s",
      QuestionRecord.szQuestion);
   od_log_write(szLogMessage);
}


/* The ChooseQuestion() function provides a list of questions and allows   */
/* the user to choose a particular question, cancel back to the main menu, */ 
/* and page up and down in the list of questions. Depending upon the value */
/* of the nFromWhichQuestions parameter, this function will present a list */
/* of questions that the user has voted on, a list of questions that the   */
/* user has not voted on, or a list of all questions.                      */
int ChooseQuestion(int nFromWhichQuestions, char *pszTitle, int *nLocation)
{
   int nCurrent;
   int nFileQuestion = 0;
   int nPagedToQuestion = *nLocation;
   int nDisplayedQuestion = 0;
   char bVotedOnQuestion;
   char chCurrent;
   tQuestionRecord QuestionRecord;
   FILE *fpQuestionFile;
   int hQuestionFile;
   static char szQuestionName[MAX_QUESTIONS][QUESTION_STR_SIZE];
   static int nQuestionNumber[MAX_QUESTIONS];
   
   /* Attempt to open question file. */
   fpQuestionFile = ExclusiveFileOpen(QUESTION_FILENAME, "r+b",
      &hQuestionFile);

   /* If unable to open question file, assume that no questions have been */
   /* created.                                                            */
   if(fpQuestionFile == NULL)
   {
      /* Display "no questions yet" message. */
      od_printf("\n\rNo questions have been created so far.\n\r");
      
      /* Wait for user to press enter. */
      WaitForEnter();
      
      /* Indicate that no question has been chosen. */
      return(NO_QUESTION);
   }
   
   /* Loop for every question record in the file. */
   while(fread(&QuestionRecord, sizeof(QuestionRecord), 1, fpQuestionFile) == 1)
   {
      /* Determine whether or not the user has voted on this question. */
      bVotedOnQuestion = CurrentUserRecord.bVotedOnQuestion[nFileQuestion];
      
      /* If this is the kind of question that the user is choosing from */
      /* right now.                                                     */
      if((bVotedOnQuestion && (nFromWhichQuestions & QUESTIONS_VOTED_ON)) ||
         (!bVotedOnQuestion && (nFromWhichQuestions & QUESTIONS_NOT_VOTED_ON)))
      {
         /* Add this question to list to be displayed. */
         strcpy(szQuestionName[nDisplayedQuestion],
            QuestionRecord.szQuestion);
         nQuestionNumber[nDisplayedQuestion] = nFileQuestion;
         
         /* Add one to number of questions to be displayed in list. */
         nDisplayedQuestion++;
      }
      
      /* Move to next question in file. */
      ++nFileQuestion;
   }   
   
   /* Close question file to allow other nodes to access the file. */
   ExclusiveFileClose(fpQuestionFile, hQuestionFile);

   /* If there are no questions for the user to choose, display an */
   /* appropriate message and return. */
   if(nDisplayedQuestion == 0)
   {
      /* If we were to list all questions. */
      if((nFromWhichQuestions & QUESTIONS_VOTED_ON)
         && (nFromWhichQuestions & QUESTIONS_NOT_VOTED_ON))
      {
         od_printf("\n\rThere are no questions.\n\r");
      }
      /* If we were to list questions that the user has voted on. */
      else if(nFromWhichQuestions & QUESTIONS_VOTED_ON)
      {
         od_printf("\n\rThere are no questions that you have voted on.\n\r");
      }
      /* Otherwise, we were to list questions that use has not voted on. */
      else
      {
         od_printf("\n\rYou have voted on all the questions.\n\r");
      }
      
      /* Wait for user to press enter key. */
      WaitForEnter();
      
      /* Return, indicating that no question was chosen. */
      return(NO_QUESTION);
   }

   /* Ensure that initial paged to location is within range. */
   while(nPagedToQuestion >= nDisplayedQuestion)
   {
      nPagedToQuestion -= QUESTION_PAGE_SIZE;
   }

   /* Loop, displaying current page of questions, until the user makes a */
   /* choice.                                                            */
   for(;;)
   {
      /* Clear the screen. */
      od_clr_scr();

      /* Display header. */
      od_printf("`bright red`");
      od_printf(pszTitle);
      od_printf("`dark red`");
      if(od_control.user_ansi || od_control.user_avatar)
      {
         od_repeat((unsigned char)196, 79);
      }
      else
      {
         od_repeat('-', 79);
      }
      od_printf("\n\r");
   
      /* Display list of questions on this page. */
      for(nCurrent = 0;
         nCurrent < QUESTION_PAGE_SIZE
         && nCurrent < (nDisplayedQuestion - nPagedToQuestion);
         ++nCurrent)
      {
         /* Determine character to display for current line. */
         if(nCurrent < 9)
         {
            chCurrent = (char)('1' + nCurrent);
         }
         else
         {
            chCurrent = (char)('A' + (nCurrent - 9));
         }
      
         /* Display this question's title. */
         od_printf("`bright green`%c.`dark green`", chCurrent);
         od_printf(" %s\n\r", szQuestionName[nCurrent + nPagedToQuestion]);
      }

      /* Display prompt for input. */
      od_printf("\n\r`bright white`[Page %d]  Choose a question or",
         (nPagedToQuestion / QUESTION_PAGE_SIZE) + 1);
      if(nPagedToQuestion < nDisplayedQuestion - QUESTION_PAGE_SIZE)
      {
         od_printf(" [N]ext page,");
      }
      if(nPagedToQuestion > 0)
      {
         od_printf(" [P]revious page,");
      }
      od_printf(" [Q]uit.\n\r");
      
      /* Loop until the user makes a valid choice. */
      for(;;)
      {      
         /* Get input from user */
         chCurrent = (char)od_get_key(TRUE);
         chCurrent = (char)toupper(chCurrent);
      
         /* Respond to user's input. */
      
         /* If user pressed Q key. */
         if(chCurrent == 'Q')
         {
            /* Return without a choosing a question. */
            return(NO_QUESTION);
         }
      
         /* If user pressed P key. */
         else if(chCurrent == 'P')
         {
            /* If we are not at the first page. */
            if(nPagedToQuestion > 0)
            {
               /* Move paged to location up one page. */
               nPagedToQuestion -= QUESTION_PAGE_SIZE;
               
               /* Exit user input loop to display next page. */
               break;
            }
         }
      
         /* If user pressed N key. */
         else if(chCurrent == 'N')
         {
            /* If there is more questions after this page. */
            if(nPagedToQuestion < nDisplayedQuestion - QUESTION_PAGE_SIZE)
            {
               /* Move paged.to location down one page. */
               nPagedToQuestion += QUESTION_PAGE_SIZE;

               /* Exit user input loop to display next page. */
               break;
            }
         }
      
         /* Otherwise, check whether the user chose a valid question. */
         else if ((chCurrent >= '1' && chCurrent <= '9')
            || (chCurrent >= 'A' && chCurrent <= 'H'))
         {
            /* Get question number from key pressed. */
            if(chCurrent >= '1' && chCurrent <= '9')
            {
               nCurrent = chCurrent - '1';
            }
            else
            {
               nCurrent = (chCurrent - 'A') + 9;
            }
         
            /* Add current paged to position to user's choice. */
            nCurrent += nPagedToQuestion;

            /* If this is valid question number. */            
            if(nCurrent < nDisplayedQuestion)
            {
               /* Set caller's current question number. */
               *nLocation = nPagedToQuestion;
            
               /* Return actual question number in file. */
               return(nQuestionNumber[nCurrent]);
            }
         }
      }
   }
}


/* The DisplayQuestionResult() function is called to display the results */
/* of voting on a paricular question, and is passed the question record  */
/* of the question. This function is called when the user selects a      */
/* question using the "view results" option, and is also called after    */
/* the user has voted on a question, to display the results of voting on */
/* that question.                                                        */
void DisplayQuestionResult(tQuestionRecord *pQuestionRecord)
{
   int nAnswer;
   int uPercent;

   /* Clear the screen. */
   od_clr_scr();

   /* Check that there have been votes on this question. */
   if(pQuestionRecord->uTotalVotes == 0)
   {
      /* If there have been no votes for this question, display a message */
      /* and return.                                                      */
      od_printf("Nobody has voted on this question yet.\n\r");
      WaitForEnter();
      return;
   }

   /* Display question itself. */
   od_printf("`bright red`%s\n\r", pQuestionRecord->szQuestion);

   /* Display author's name. */
   od_printf("`dark red`Question created by %s on %s\n\r",
      pQuestionRecord->szCreatorName,
      ctime(&pQuestionRecord->lCreationTime));
   
   /* Display heading for responses. */
   od_printf("`bright green`Response                        Votes  Percent  Graph\n\r`dark green`");
   if(od_control.user_ansi || od_control.user_avatar)
   {
      od_repeat((unsigned char)196, 79);
   }
   else
   {
      od_repeat('-', 79);
   }
   od_printf("\n\r");

   /* Loop for each answer to the question. */   
   for(nAnswer = 0; nAnswer < pQuestionRecord->nTotalAnswers; ++nAnswer)
   {
      /* Determine percent of users who voted for this answer. */
      uPercent = (pQuestionRecord->auVotesForAnswer[nAnswer] * 100)
         / pQuestionRecord->uTotalVotes;
      
      /* Display answer, total votes and percentage of votes. */
      od_printf("`dark green`%-30.30s  %-5u  %3u%%     `bright white`",
         pQuestionRecord->aszAnswer[nAnswer],
         pQuestionRecord->auVotesForAnswer[nAnswer],
         uPercent);

      /* Display a bar graph corresponding to percent of users who voted */
      /* for this answer.                                                */
      if(od_control.user_ansi || od_control.user_avatar)
      {
         od_repeat((unsigned char)220, (unsigned char)((uPercent * 31) / 100));
      }
      else
      {
         od_repeat('=', (unsigned char)((uPercent * 31) / 100));
      }

      /* Move to next line. */
      od_printf("\n\r");
   }
   
   /* Display footer. */
   od_printf("`dark green`");
   if(od_control.user_ansi || od_control.user_avatar)
   {
      od_repeat((unsigned char)196, 79);
   }
   else
   {
      od_repeat('-', 79);
   }
   od_printf("\n\r");
   od_printf("`dark green`                         TOTAL: %u\n\r\n\r",
      pQuestionRecord->uTotalVotes);
   
   /* Wait for user to press enter. */
   WaitForEnter();
}


/* The ReadOrAddCurrentUser() function is used by Vote to search the      */
/* Vote user file for the record containing information on the user who   */
/* is currently using the door. If this is the first time that the user   */
/* has used this door, then their record will not exist in the user file. */
/* In this case, this function will add a new record for the current      */
/* user. This function returns TRUE on success, or FALSE on failure.      */
int ReadOrAddCurrentUser(void)
{
   FILE *fpUserFile;
   int hUserFile;
   int bGotUser = FALSE;
   int nQuestion;

   /* Attempt to open the user file for exclusize access by this node.     */
   /* This function will wait up to the pre-set amount of time (as defined */   
   /* near the beginning of this file) for access to the user file.        */
   fpUserFile = ExclusiveFileOpen(USER_FILENAME, "a+b", &hUserFile);

   /* If unable to open user file, return with failure. */   
   if(fpUserFile == NULL)
   {
      return(FALSE);
   }

   /* Begin with the current user record number set to 0. */
   nCurrentUserNumber = 0;

   /* Loop for each record in the file */
   while(fread(&CurrentUserRecord, sizeof(tUserRecord), 1, fpUserFile) == 1)
   {
      /* If name in record matches the current user name ... */
      if(strcmp(CurrentUserRecord.szUserName, od_control.user_name) == 0)
      {
         /* ... then record that we have found the user's record, */
         bGotUser = TRUE;
         
         /* and exit the loop. */
         break;
      }

      /* Move user record number to next user record. */      
      nCurrentUserNumber++;
   }

   /* If the user was not found in the file, attempt to add them as a */
   /* new user if the user file is not already full.                  */
   if(!bGotUser && nCurrentUserNumber < MAX_USERS)
   {
      /* Place the user's name in the current user record. */
      strcpy(CurrentUserRecord.szUserName, od_control.user_name);
      
      /* Record that user hasn't voted on any of the questions. */
      for(nQuestion = 0; nQuestion < MAX_QUESTIONS; ++nQuestion)
      {
         CurrentUserRecord.bVotedOnQuestion[nQuestion] = FALSE;
      }
      
      /* Write the new record to the file. */
      if(fwrite(&CurrentUserRecord, sizeof(tUserRecord), 1, fpUserFile) == 1)
      {
         /* If write succeeded, record that we now have a valid user record. */
         bGotUser = TRUE;
      }
   }

   /* Close the user file to allow other nodes to access it. */
   ExclusiveFileClose(fpUserFile, hUserFile);

   /* Return, indciating whether or not a valid user record now exists for */
   /* the user that is currently online.                                   */   
   return(bGotUser);
}


/* The WriteCurrentUser() function is called to save the information on the */
/* user who is currently using the door, to the VOTE.USR file.              */
void WriteCurrentUser(void)
{
   FILE *fpUserFile;
   int hUserFile;

   /* Attempt to open the user file for exclusize access by this node.     */
   /* This function will wait up to the pre-set amount of time (as defined */   
   /* near the beginning of this file) for access to the user file.        */
   fpUserFile = ExclusiveFileOpen(USER_FILENAME, "r+b", &hUserFile);

   /* If unable to access the user file, display an error message and */
   /* return.                                                         */
   if(fpUserFile == NULL)
   {
      od_printf("Unable to access the user file.\n\r");
      WaitForEnter();
      return;
   }
   
   /* Move to appropriate location in user file for the current user's */
   /* record. */
   fseek(fpUserFile, (long)nCurrentUserNumber * sizeof(tUserRecord), SEEK_SET);

   /* Write the new record to the file. */
   if(fwrite(&CurrentUserRecord, sizeof(tUserRecord), 1, fpUserFile) == 1)
   {
      /* If unable to write the record, display an error message. */
      ExclusiveFileClose(fpUserFile, hUserFile);
      od_printf("Unable to update your user record file.\n\r");
      WaitForEnter();
      return;
   }
   
   /* Close the user file to allow other nodes to access it again. */
   ExclusiveFileClose(fpUserFile, hUserFile);
}


/* This function is used by Vote to open a file. If Vote has been compiled */
/* with #define MULTINODE_AWARE uncommented (see the beginning of this     */
/* file), file access is performed in a multinode-aware way. This implies  */
/* that the file is opened of exclusive access, using share-aware open     */
/* functions that may not be available using all compilers.                */
FILE *ExclusiveFileOpen(char *pszFileName, char *pszMode, int *phHandle)
{
#ifdef MULTINODE_AWARE
   /* If Vote is being compiled for multinode-aware file access, then   */
   /* attempt to use compiler-specific share-aware file open functions. */
   FILE *fpFile = NULL;
   time_t StartTime = time(NULL);
   int hFile;

   /* Attempt to open the file while there is still time remaining. */    
   while((hFile = sopen(pszFileName, O_BINARY | O_RDWR, SH_DENYRW,
      S_IREAD | S_IWRITE)) == -1)
   {
      /* If we have been unable to open the file for more than the */
      /* maximum wait time, or if open failed for a reason other   */
      /* than file access, then attempt to create a new file and   */
      /* exit the loop.                                            */
      if(errno != EACCES ||
         difftime(time(NULL), StartTime) >= FILE_ACCESS_MAX_WAIT)
      {
         hFile = sopen(pszFileName, O_BINARY | O_CREAT, SH_DENYRW,
            S_IREAD | S_IWRITE);
         break;
      }

      /* If we were unable to open the file, call od_kernel, so that    */
      /* OpenDoors can continue to respond to sysop function keys, loss */
      /* of connection, etc.                                            */
      od_kernel();
   }

   /* Attempt to obtain a FILE * corresponding to the handle. */
   if(hFile != -1)
   {
      fpFile = fdopen(hFile, pszMode);
      if(fpFile == NULL)
      {
         close(hFile);
      }
   }

   /* Pass file handle back to the caller. */
   *phHandle = hFile;

   /* Return FILE pointer for opened file, if any. */   
   return(fpFile);
#else
   /* Ignore unused parameters. */
   (void)phHandle;

   /* If Vote is not being compiled for multinode-aware mode, then just */
   /* use fopen to access the file.                                     */
   return(fopen(pszFileName, pszMode));
#endif
}


/* The ExclusiveFileClose() function closes a file that was opened using */
/* ExclusiveFileOpen().                                                  */
void ExclusiveFileClose(FILE *pfFile, int hHandle)
{
   fclose(pfFile);
#ifdef MULTINODE_AWARE
   close(hHandle);
#else
   /* Ignore unused parameters. */
   (void)hHandle;
#endif
}


/* The WaitForEnter() function is used by Vote to create its custom   */
/* "Press [ENTER] to continue." prompt.                               */
void WaitForEnter(void)
{
   /* Display prompt. */
   od_printf("`bright white`Press [ENTER] to continue.\n\r");
   
   /* Wait for a Carriage Return or Line Feed character from the user. */
   od_get_answer("\n\r");
}
