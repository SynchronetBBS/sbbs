/* Include standard C header files required by FreeVote. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>


/* Include the OpenDoors header file. This line must be done in any program */
/* using OpenDoors.                                                         */
#include "OpenDoor.h"
#include <genwrap.h>
#include <dirwrap.h>
#include <filewrap.h>
#include <xpendian.h>

/* Manifest constants used by EZVote */
#define NO_QUESTION              -1
#define NEW_ANSWER               -1
#define SET_UN_ANSWERED          -2

#define QUESTIONS_VOTED_ON        0x0001
#define QUESTIONS_NOT_VOTED_ON    0x0002
#define CURRENT_USER_ONLY         0x0004
#define QUESTIONS_FORCED          0x0008

#define CAN_ADD_ANSWERS           0x0001
#define MULTIPLE_ANSWERS          0x0002
#define QUESTION_DELETED          0x0004
#define ANONYMOUS_QUESTION        0x0008
#define BF_15_BY_1_MODE           0x0010
#define NEVER_DELETE_QUESTION     0x0020
#define MULTIPLE_ADDS_P_U         0x0040
#define FORCED_QUESTION           0x0080

#define MAX_QUESTIONS             400
#define MAX_USERS                 32766
#define MAX_ANSWERS               15
#define MAX_ANSWERS2              7
#define QUESTION_STR_SIZE         71
#define ANSWER_STR_SIZE           31

#define USER_FILENAME             "freevote.usr"
#define QUESTION_FILENAME         "freevote.qst"

#define FILE_ACCESS_MAX_WAIT      20

#define QUESTION_PAGE_SIZE        17

#define COLOR_DEF 0
#define COLOR_TNW 1

struct time {
  unsigned char ti_min;
  unsigned char ti_hour;
  unsigned char ti_hund;
  unsigned char ti_sec;
};

/* Structure of records stored in the EZVOTE.USR file */
typedef struct
{
   char szUserName[36];
   WORD bVotedOnQuestion[MAX_QUESTIONS];
} tUserRecord;

tUserRecord CurrentUserRecord;
int user_security=-1;
int b_us;
int AddSecurity=0;
int nCurrentUserNumber;
time_t maxtime=0;
int sysop_sec=32767;
int Forcevote=0;
struct time tt;
char allow_page=TRUE;
int AllowIn=0;
char view_own_results=TRUE;
char multiple_answers=TRUE;
char anonymous_posting=TRUE;
char answer_adding=1; /*0 = no adding
			1 = regular creator chooses
			2 = forced*/
char more_adds_p_u=1; /*0 = never
			1 = creator chooses
			2 = forced*/
char no_same_answers=TRUE;
char bAllowDelete=TRUE;
char bAllowChange=TRUE;
char bAllowUnvote=TRUE;
char ColoredResults=TRUE;
//int bAllowGoodbye=TRUE;
char bHandles=FALSE;
char maint_run=FALSE;
char autodetect=TRUE;

int answers[16]={0x00000001,
		 0x00000002,
		 0x00000004,
		 0x00000008,
		 0x00000010,
		 0x00000020,
		 0x00000040,
		 0x00000080,
		 0x00000100,
		 0x00000200,
		 0x00000400,
		 0x00000800,
		 0x00001000,
		 0x00002000,
		 0x00004000,
		 0x00008000};

/* Structure of records stored in the EZVOTE.QST file */
typedef struct
{
   char szQuestion[QUESTION_STR_SIZE];
   char aszAnswer[MAX_ANSWERS][ANSWER_STR_SIZE];
   char nTotalAnswers;
   DWORD auVotesForAnswer[MAX_ANSWERS];
   DWORD uTotalVotes;
/*   char bCanAddAnswers;
   char bMultipleAnswers;
   char b
   char bDeleted;*/
   char bitflags;  /*1st - Can Add Answers
		     2nd - Multiple Answers
		     3rd - Deleted
		     4th - Anonymous
		     5th - 15 x 1 line / 7 x 2 lines mode
		     6th - Never Delete this question if this is set
		     7th - Allow multiple adds.
		     8th - Forced Question.
   */
   char szCreatorName[36];
   time_t lCreationTime;
} tQuestionRecord;


/* Global variables. */
unsigned int nViewResultsFrom = QUESTIONS_VOTED_ON;
char bAllowAdd = TRUE;
int nQuestionsVotedOn = 0;
char sz_user_name[36];
int colorsch=COLOR_DEF;


/* Prototypes for functions that form EZVote */
void CustomConfigFunction(char *pszKeyword, char *pszOptions);
void BeforeExitFunction(void);
void CheckForBadUsers(char name[]);
void VoteOnQuestion(int all);
void ViewResults(int all);
int FirstQuestion(unsigned int nFromWhichQuestions, int nFileQuestion);
int GetQuestion(int nQuestion, tQuestionRecord *pQuestionRecord);
void AddQuestion(void);
void DeleteQuestion(void);
void ChangeQuestion(void);
void DeleteYourQuestion(void);
int ChooseQuestion(unsigned int nFromWhichQuestions, const char *pszTitle, int *nLocation);
int DisplayQuestionResult(tQuestionRecord *pQuestionRecord,int all);
int ReadOrAddCurrentUser(void);
void WriteCurrentUser(void);
FILE *ExculsiveFileOpen(const char *pszFileName, const char *pszMode);
void WaitForEnter(void);
int CountQuestions(void);
int CountQuestionsF(void);
void dump(void);
void maint(void);
void warp_line(char line1[], char line2[],const char intro[]);
void my_clr_scr();
void trim(char *numstr);
void QuestionEditor(void);
FILE *QRead(tQuestionRecord *QuestionRecord,int nQuestion);
int QWrite(tQuestionRecord *QuestionRecord, FILE *fpFile,int nQuestion);

#define USERRECORD_SIZE (sizeof(WORD)*MAX_QUESTIONS+36)
#define QUESTIONRECORD_SIZE (QUESTION_STR_SIZE+(MAX_ANSWERS*ANSWER_STR_SIZE)+1+(sizeof(DWORD)*MAX_ANSWERS)+sizeof(DWORD)+1+36+8)

int
freadUserRecord(tUserRecord *urec, FILE *f)
{
	int i;

	if(fread(urec->szUserName, sizeof(urec->szUserName), 1, f)!=1)
		return(0);
	for(i=0; i<MAX_QUESTIONS; i++) {
		if(fread(&urec->bVotedOnQuestion[i], sizeof(WORD), 1, f)!=1)
			return(0);
		urec->bVotedOnQuestion[i]=LE_SHORT(urec->bVotedOnQuestion[i]);
	}
	return(1);
}

int
fwriteUserRecord(tUserRecord *urec, FILE *f)
{
	int i;

	if(fwrite(urec->szUserName, sizeof(urec->szUserName), 1, f)!=1)
		return(0);
	for(i=0; i<MAX_QUESTIONS; i++) {
		urec->bVotedOnQuestion[i]=LE_SHORT(urec->bVotedOnQuestion[i]);
		if(fwrite(&urec->bVotedOnQuestion[i], sizeof(WORD), 1, f)!=1)
			return(0);
		urec->bVotedOnQuestion[i]=LE_SHORT(urec->bVotedOnQuestion[i]);
	}
	return(1);
}

int
freadQuestionRecord(tQuestionRecord *q, FILE *f)
{
	INT32 i;

	if(fread(q->szQuestion, sizeof(q->szQuestion), 1, f)!=1)
		return(0);
	for(i=0; i<MAX_ANSWERS; i++) {
		if(fread(q->aszAnswer[i], sizeof(q->aszAnswer[i]), 1, f)!=1)
			return(0);
	}
	if(fread(&q->nTotalAnswers, sizeof(q->nTotalAnswers), 1, f)!=1)
		return(0);
	for(i=0; i<MAX_ANSWERS; i++) {
		if(fread(&q->auVotesForAnswer[i], sizeof(q->auVotesForAnswer[i]), 1, f)!=1)
			return(0);
		q->auVotesForAnswer[i]=LE_SHORT(q->auVotesForAnswer[i]);
	}
	if(fread(&q->uTotalVotes, sizeof(q->uTotalVotes), 1, f)!=1)
		return(0);
	q->uTotalVotes=LE_SHORT(q->uTotalVotes);
	if(fread(&q->bitflags, sizeof(q->bitflags), 1, f)!=1)
		return(0);
	if(fread(q->szCreatorName, sizeof(q->szCreatorName), 1, f)!=1)
		return(0);
	switch(sizeof(time_t)) {
		case 4:
			if(fread(&q->lCreationTime, sizeof(q->lCreationTime), 1, f)!=1)
				return(0);
			q->lCreationTime=LE_LONG(q->lCreationTime);
			if(fread(&i, sizeof(i), 1, f)!=1)
				return(0);
			break;
		default:
			fprintf(stderr, "Unhandled time_t size (%d)\n", sizeof(time_t));
			exit(1);
	}
	return(1);
}

int
fwriteQuestionRecord(tQuestionRecord *q, FILE *f)
{
	INT32 i;

	if(fwrite(q->szQuestion, sizeof(q->szQuestion), 1, f)!=1)
		return(0);
	for(i=0; i<MAX_ANSWERS; i++) {
		if(fwrite(q->aszAnswer[i], sizeof(q->aszAnswer[i]), 1, f)!=1)
			return(0);
	}
	if(fwrite(&q->nTotalAnswers, sizeof(q->nTotalAnswers), 1, f)!=1)
		return(0);
	for(i=0; i<MAX_ANSWERS; i++) {
		q->auVotesForAnswer[i]=LE_SHORT(q->auVotesForAnswer[i]);
		if(fwrite(&q->auVotesForAnswer[i], sizeof(q->auVotesForAnswer[i]), 1, f)!=1) {
			q->auVotesForAnswer[i]=LE_SHORT(q->auVotesForAnswer[i]);
			return(0);
		}
		q->auVotesForAnswer[i]=LE_SHORT(q->auVotesForAnswer[i]);
	}
	q->uTotalVotes=LE_SHORT(q->uTotalVotes);
	if(fwrite(&q->uTotalVotes, sizeof(q->uTotalVotes), 1, f)!=1) {
		q->uTotalVotes=LE_SHORT(q->uTotalVotes);
		return(0);
	}
	q->uTotalVotes=LE_SHORT(q->uTotalVotes);
	if(fwrite(&q->bitflags, sizeof(q->bitflags), 1, f)!=1)
		return(0);
	if(fwrite(q->szCreatorName, sizeof(q->szCreatorName), 1, f)!=1)
		return(0);
	switch(sizeof(time_t)) {
		case 4:
			q->lCreationTime=LE_LONG(q->lCreationTime);
			if(fwrite(&q->lCreationTime, sizeof(q->lCreationTime), 1, f)!=1) {
				q->lCreationTime=LE_LONG(q->lCreationTime);
				return(0);
			}
			q->lCreationTime=LE_LONG(q->lCreationTime);
			if(fwrite(&i, sizeof(i), 1, f)!=1)
				return(0);
			break;
		default:
			fprintf(stderr, "Unhandled time_t size (%d)\n", sizeof(time_t));
			exit(1);
	}
	return(1);
}

void gettime(struct time *tim)  {
  struct timeval ti;
  struct tm *t;
  
  gettimeofday(&ti,NULL);
  t=localtime((time_t *)&ti.tv_sec);
  tim->ti_min=t->tm_min;
  tim->ti_hour=t->tm_hour;
  tim->ti_hund=ti.tv_usec/10000;
  tim->ti_sec=t->tm_sec;
}

void
CheckForBadUsers(char name[])
{
  FILE *badfile;
  char numstr[82];
  char *key;

  badfile=ExculsiveFileOpen("badusers.txt","rt");

  if(badfile==NULL) return;

  while(fgets(numstr,81,badfile)!=NULL) {
    key=strchr(numstr,'\n');
    *key=0;
    trim(numstr);
    if(stricmp(numstr,name)==0) {
      od_printf("\n\r\n`bright`You have been locked out of FrEevOtE!\n\r\n");
      WaitForEnter();
      fclose(badfile);
      od_exit(10,TRUE);
    }
  }
  fclose(badfile);
}

void
trim(char *numstr)
{
  int x;
  for(x=strlen(numstr)-1;numstr[x]==' ' && x>=0;x--) od_kernal;
  numstr[x+1]=0;
  strrev(numstr);
  for(x=strlen(numstr)-1;numstr[x]==' ' && x>=0;x--) od_kernal;
  numstr[x+1]=0;
  strrev(numstr);
}

void
my_clr_scr()
{
  if(od_control.user_rip==TRUE) {
    /*od_control.user_rip=FALSE;
    od_disp_str("\n\r!|*|#|#|#\n\n\r");
    od_clr_scr();
    od_control.user_rip=TRUE;*/
    od_clr_scr();
    od_disp_str("|#|#|#\n\r");
    od_control.user_rip=FALSE;
    od_clr_scr();
    od_control.user_rip=TRUE;
  } else {
    od_printf("\n\n\r");
    od_clr_scr();
  }
}

void
cap_names(char name[])
{
  int cnt=0;
  int cap=TRUE;

  while (name[cnt]!=0) {
    if (cap==TRUE) {
      if (name[cnt]>='a' && name[cnt]<='z')
	name[cnt]-=32;
    } else {
      if (name[cnt]>='A' && name[cnt]<='Z')
	name[cnt]+=32;
    }

    cnt++;

    if (name[cnt-1]==' ')
      cap=TRUE;
    else
      cap=FALSE;

  }
}



/*copy end chars  beginning from beg to dest*/
/*podobny strncpy*/
void
strzcpy(char dest[],const char src[], int beg,int end)
{
  int cnt=0;
  do {
    dest[cnt]=src[beg];
    beg++;
    cnt++;
  } while (cnt<=end && src[cnt]!=0);
  dest[cnt]=0;
}

void
NoDropFile(void)
{
  printf("\nFrEevOtE v3.2 - No dropfile found!\n");
  printf("To start in Local mode type:\nFREEVOTE -L\n");
  exit(10);
}



/* main() function - Program execution begins here. */
main(int argc, char *argv[])
{
   /* Variable to store user's choice from the menu */
   char chMenuChoice;
   char chYesOrNo;
   int cnt,intval;
   char menufile[13];

   /* Enable use of OpenDoors configuration file system. */
   od_control.od_config_file = INCLUDE_CONFIG_FILE;
   od_control.od_config_filename = "freevote.cfg";
   if(fexist("freevote.cfg"))
     od_control.od_config_file = INCLUDE_CONFIG_FILE;
   else
     od_control.od_config_file = NO_CONFIG_FILE;

   od_control.od_mps=INCLUDE_MPS;
   od_control.od_nocopyright=TRUE;

   /* Set function to process custom configuration file lines. */
   od_control.od_config_function = CustomConfigFunction;

   //your reg number here
   strcpy(od_registered_to,"Your Name");
   od_registration_key=0L;

   char key;
   char numstr[81];

   cnt=1;

   od_control.od_logfile_disable=TRUE;

   od_control.od_no_file_func=NoDropFile;
   od_control.od_disable |= DIS_BPS_SETTING;
   od_control.od_disable |= DIS_NAME_PROMPT;


  if(argc>1) {
    do {
      if (strnicmp(argv[cnt],"-LOG",4)==0) {
	od_control.od_logfile = INCLUDE_LOGFILE;
	strcpy(od_control.od_logfile_name, "freevote.log");
	od_control.od_logfile_disable=FALSE;
      } else if (strnicmp(argv[cnt],"-NAD",4)==0) {
	autodetect=FALSE;
      } else if (strnicmp(argv[cnt],"-RDBPS",6)==0) {
	od_control.od_disable &=~ DIS_BPS_SETTING;
      } else if (strnicmp(argv[cnt],"-L",2)==0) {
	od_control.od_force_local=TRUE;
#ifndef ODPLAT_NIX
	clrscr();
	textbackground(LIGHTCYAN);
	textcolor(BLUE);
	gotoxy(1,7);
	cprintf("ÉÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ»");
	gotoxy(1,8);
	cprintf("º FrEevOtE v3.2                                                               º");
	gotoxy(1,9);
	cprintf("º Starting in local mode input your name: °°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°° º");
	gotoxy(1,10);
	cprintf("ÈÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ¼");
	gotoxy(43,9);
//      for(x=0;x<35;x++)
//        putch('°');

//      cprintf(" º\b\b");

//      for(x=0;x<35;x++)
//        putch('\b');


	int cntv=0;
	intval=TRUE;
	do {
	  if(cntv>=35) {
	    cntv=34;
	    putch('\b');
	    key=getch();
	    if(key=='\n' || key=='\r')
	      cntv=35;
	  } else {
	    key=getch();
	  }

	  if(key==27) {
	    gotoxy(1,9);
	    cprintf("º Starting in local mode input your name: ° Canceled ... °°°°°°°°°°°°°°°°°°°° º");
	    gotoxy(1,12);
	    exit(10);
	  }

	  if(intval==TRUE) {
	    if (key>='a' && key<='z')
	      key-=32;
	    intval=FALSE;
	  } else if(intval==FALSE) {
	    if (key>='A' && key<='Z')
	      key+=32;
	  }
	  if(key==' ') {
	    putch('°');
	    key=0;
	    intval=TRUE;
	  } else if(key=='\b') {
	    if(cntv==0) {
	      intval=TRUE;
	      key=0;
	      cntv--;
	    } else {
	      cntv-=2;
	      if(cntv>=0 && od_control.user_name[cntv]==' ')
		intval=TRUE;
	      if(cntv==32)
		cprintf("\b°°\b\b");
	      else
		cprintf("\b°\b");
	      key=0;
	    }
	  }

	  if(key!=0) {
	    od_control.user_name[cntv]=key;
	    putch(key);
	  }
	  cntv++;
	} while (key!='\n' && key!='\r');
	od_control.user_name[cntv-1]=0;

	trim(od_control.user_name);

/*      strlwr(od_control.user_name);
	od_contro.user_name[0]-=32;*/
#endif
      } else if (strnicmp(argv[cnt],"-P",2)==0) {
	strzcpy(od_control.info_path,argv[cnt],2,59);
      } else if (strnicmp(argv[cnt],"-N",2)==0) {
	strzcpy(numstr,argv[cnt],2,59);
	sscanf(numstr,"%d",&od_control.od_node);
      } else if (strnicmp(argv[cnt],"-S",2)==0) {
	strzcpy(numstr,argv[cnt],2,59);
	sscanf(numstr,"%d",&user_security);
      } else if (strnicmp(argv[cnt],"-M",2)==0) {
	od_control.od_force_local=TRUE;
	maint_run=TRUE;
      } else if (strnicmp(argv[cnt],"-C",2)==0) {
	od_control.od_config_file = INCLUDE_CONFIG_FILE;
	od_control.od_config_filename=argv[cnt]+2;
      } else if (strnicmp(argv[cnt],"-FC",3)==0) {
	Forcevote=5;
      } else if (strnicmp(argv[cnt],"-FNQ",4)==0) {
	Forcevote=2;
      } else if (strnicmp(argv[cnt],"-FA",3)==0) {
	Forcevote=3;
      } else if (strnicmp(argv[cnt],"-FQ",3)==0) {
	Forcevote=4;
      } else if (strnicmp(argv[cnt],"-F",2)==0) {
	Forcevote=1;
      }
    } while ((++cnt)<argc);
  }


   /* Include the OpenDoors multiple personality system, which allows    */
   /* the system operator to set the sysop statusline / function key set */
   /* to mimic the BBS software of their choice.                         */
   od_control.od_mps = INCLUDE_MPS;

   /* Set program's name, to be written to the OpenDoors log file       */
   strcpy(od_control.od_prog_name, "FrEevOtE");

   if(maint_run==TRUE) {
     od_init();
     maint();
     od_exit(10,FALSE);
   }


   /* Set function to be called before program exits. */

//   od_control.od_default_rip_win=TRUE;

   /* Initialize OpenDoors. This function call is optional, and can be used */
   /* to force OpenDoors to read the door informtion file and begin door    */
   /* operations. If a call to od_init() is not included in your program,   */
   /* OpenDoors initialization will be performed at the time of your first  */
   /* call to any OpenDoors function. */
   od_init();

   b_us=od_control.user_security;

   if(user_security!=-1)
     od_control.user_security=user_security;

   od_control.od_update_status_now=TRUE;
   od_kernel();

//   od_control.od_disable |= DIS_INFOFILE;

   if(autodetect==TRUE)
     od_autodetect(DETECT_NORMAL);

   od_control.od_help_text2=(char *)"  FrEevOtE v3.2 by Franz ... (c)1995 George Lebl ... FREEWARE!!!!!            ";

   if(od_control.user_rip==TRUE) od_control.user_ansi=TRUE;

   if(bHandles==TRUE)
     strcpy(sz_user_name,od_control.user_handle);
   else
     strcpy(sz_user_name,od_control.user_name);

   trim(sz_user_name);

   CheckForBadUsers(sz_user_name);

   if(AllowIn>od_control.user_security) {
     od_printf("\n\r\n`bright`Your security level is not high enough to let you into FrEevOtE!\n\r\n");
     WaitForEnter();
     od_exit(10,TRUE);
   }

   gettime(&tt);

   if((tt.ti_min +(tt.ti_hour * 60)) < od_control.od_pagestartmin ||
    (tt.ti_min +(tt.ti_hour * 60)) >= od_control.od_pageendmin) {
    allow_page=FALSE;
   }

   if((sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0)) {
     answer_adding=1;
     more_adds_p_u=1;
     no_same_answers=FALSE;
     anonymous_posting=TRUE;
     multiple_answers=TRUE;
   } else {
     if(AddSecurity>od_control.user_security)
       bAllowAdd=FALSE;
   }



   /*check and then pack questionfile and delete old questions*/
/*   maint();*/

   /* Call the EZVote function ReadOrAddCurrentUser() to read the current */
   /* user's record from the EZVote user file, or to add the user to the  */
   /* file if this is the first time that they have used EZVote.          */
   if(!ReadOrAddCurrentUser())
   {
      /* If unable to obtain a user record for the current user, then exit */
      /* the door after displaying an error message.                       */
      od_printf("Unable to access user file. File may be locked or full.\n\r");
      WaitForEnter();
      od_exit(1, FALSE);
   }

   od_control.od_before_exit = BeforeExitFunction;
   if(sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0) {
     nViewResultsFrom = QUESTIONS_VOTED_ON | QUESTIONS_NOT_VOTED_ON;
   } else {
     if(CountQuestionsF() > 0) {
//       od_printf("\n\n\r");
       my_clr_scr();
       if(colorsch==COLOR_DEF)
	 od_printf("`bright green`");
       else
	 od_printf("`bright`");
       od_printf("FrEevOtE v3.2\n\r\n");
       if(colorsch==COLOR_DEF)
	 od_printf("`bright red`");
       else
	 od_printf("`bright black`");
       od_printf("The Sysop has posted forced questions");
       if(colorsch!=COLOR_DEF)
	 od_printf("`white`");
       od_printf(".\n\r");
       if(colorsch!=COLOR_DEF)
	 od_printf("`bright black`");
       od_printf("You will have to answer them now");
       if(colorsch!=COLOR_DEF)
	 od_printf("`white`");
       od_printf("!\n\r\n");
       WaitForEnter();
       Forcevote += 100;
       VoteOnQuestion(TRUE);
       Forcevote -= 100;
     }
   }

   /* Loop until the user choses to exit the door. For each iteration of  */
   /* this loop, we display the main menu, get the user's choice from the */
   /* menu, and perform the appropriate action for their choice.          */

   if(Forcevote==1) {
//     od_printf("\n\n\r");
     my_clr_scr();
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`");
     else
       od_printf("`bright`");
     od_printf("FrEevOtE v3.2\n\r\n");
     WaitForEnter();
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`");
     else
       od_printf("`bright`");
     VoteOnQuestion(TRUE);
     if(colorsch==COLOR_DEF)
       od_printf("`bright red`\n\rEnter FrEevOtE? (Y/[N]):");
     else
       od_printf("`bright black`\n\rEnter FrEevOtE`white`? (`bright black`Y`white`/[`bright black`N`white`]):");
     chMenuChoice=od_get_answer("YN\n\r");
     if(chMenuChoice!='Y') {
       od_printf(" No\n\r");
       od_exit(10,FALSE);
     }
     od_printf(" Yes\n\r");
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`");
     else
       od_printf("`bright`");
     Forcevote=0;
   }

   if(Forcevote==2) {
//     od_printf("\n\n\r");
     my_clr_scr();
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`");
     else
       od_printf("`bright`");

     od_printf("FrEevOtE v3.2\n\r\n");
     WaitForEnter();
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`");
     else
       od_printf("`bright`");
     VoteOnQuestion(TRUE);
     od_exit(10,FALSE);
   }

   if(Forcevote==4) {
//     od_printf("\n\n\r");
     my_clr_scr();
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`");
     else
       od_printf("`bright`");
     od_printf("`bright green`");
     VoteOnQuestion(TRUE);
     od_exit(10,FALSE);
   }


   if(Forcevote==3) {
//     od_printf("\n\n\r");
     my_clr_scr();
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`");
     else
       od_printf("`bright`");
     od_printf("FrEevOtE v3.2\n\r");
     if(CountQuestions()>0) {
       if(colorsch==COLOR_DEF)
	 od_printf("`bright red`\n\rGo Vote? ([Y]/N):");
       else
	 od_printf("`bright black`\n\rGo Vote`white`? ([`bright black`Y`white`]/`bright black`N`white`):");

       chMenuChoice=od_get_answer("YN\n\r");
       if(chMenuChoice=='N') {
	 od_printf(" No\n\r");
	 od_exit(10,FALSE);
       }
       od_printf(" Yes\n\r");
       if(colorsch==COLOR_DEF)
	 od_printf("`bright green`");
       else
	 od_printf("`bright`");

       VoteOnQuestion(TRUE);
       if(colorsch==COLOR_DEF)
	 od_printf("`bright red`\n\rEnter FrEevOtE? (Y/[N]):");
       else
	 od_printf("`bright black`\n\rEnter FrEevOtE`white`? (`bright black`Y`white`/[`bright black`N`white`]):");
       chMenuChoice=od_get_answer("YN\n\r");
       if(chMenuChoice!='Y') {
	 od_printf(" No\n\r");
	 od_exit(10,FALSE);
       }
       Forcevote=0;
       od_printf(" Yes\n\r");
       if(colorsch==COLOR_DEF)
	 od_printf("`bright green`");
       else
	 od_printf("`bright`");

     } else {
       if(colorsch==COLOR_DEF)
	 od_printf("`bright red`");
       else
	 od_printf("`bright black`");

       if(colorsch==COLOR_DEF)
	 od_printf("`bright red`\n\rEnter FrEevOtE Anywayz? (Y/[N]):");
       else
	 od_printf("`bright black`\n\rEnter FrEevOtE Anywayz`white`? (`bright black`Y`white`/[`bright black`N`white`]):");

       chMenuChoice=od_get_answer("YN\n\r");
       if(chMenuChoice!='Y') {
	 od_printf(" No\n\r");
	 od_exit(10,FALSE);
       }
       Forcevote=0;
       od_printf(" Yes\n\r");
       if(colorsch==COLOR_DEF)
	 od_printf("`bright green`");
       else
	 od_printf("`bright`");
     }
   }

   if(Forcevote==5) {
//     od_printf("\n\n\r");
     my_clr_scr();
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`");
     else
       od_printf("`bright`");

     od_printf("FrEevOtE v3.2\n\r");
     CountQuestions();
     WaitForEnter();
     od_exit(10,FALSE);
   }


   for(;;)
   {
      /* Clear the screen */
//      od_printf("\n\n\r");
      my_clr_scr();

      /* Display main menu. */
      sprintf(menufile,"MM%d",od_control.user_security);
      if(od_send_file(menufile)!=TRUE && od_send_file("MMENU")!=TRUE) {
	if(od_control.user_ansi || od_control.user_avatar) {
	  if(colorsch==COLOR_DEF) {
	    od_printf("`bright`                        ßßß     ßßß ÜÜÜ     ßßß  Ü  ßßß   `bright green`v3.2\n\r");
	    od_printf("`bright red`ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ`bright blue`Ûß`bright red`ÄÄ`bright blue`ÛßÜ`bright red`Ä`bright blue`Ûß`bright red`ÄÄ`bright blue`ß`bright red`Ä`bright blue`ß`bright red`Ä`bright blue`Û`bright red`Ä`bright blue`Û`bright red`Ä`bright blue`Û`bright red`Ä`bright blue`Û`bright red`Ä`bright blue`ßßß`bright red`Ä`bright blue`Ûß`bright red`ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n\r");
	    od_printf("`blue`                        ß   ß   ßßß ßßß  ß  ßßß  ßß ßßß   `green`by `bright green`Franz\n\r");
	  } else {
	    od_printf("`bright`                        ßßß     ßßß ÜÜÜ     ßßß  Ü  ßßß   `white`v`bright`3`white`.`bright`2\n\r");
	    od_printf("`bright black`ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ`bright blue`Ûß`bright black`ÄÄ`bright blue`ÛßÜ`bright black`Ä`bright blue`Ûß`bright black`ÄÄ`bright blue`ß`bright black`Ä`bright blue`ß`bright black`Ä`bright blue`Û`bright black`Ä`bright blue`Û`bright black`Ä`bright blue`Û`bright black`Ä`bright blue`Û`bright black`Ä`bright blue`ßßß`bright black`Ä`bright blue`Ûß`bright black`ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n\r");
	    od_printf("`blue`                        ß   ß   ßßß ßßß  ß  ßßß  ßß ßßß   `bright black`by `white`Franz\n\r");
	  }
	} else {
	  od_printf("------------------------< FREEVOTE v3.2 >---< by Franz >---------------------\n\r");
	}

/*      od_printf("`bright red`                                     v3.2\n\r");

//       od_printf("`bright red`                                     FrEevOTe\n\r");
	 od_printf("`green`                                 Made by `bright green`Franz\n\r");
	 od_printf("`yellow`                                  From EZVote\n\r");
	 od_printf("`red`");*/
/*       if(od_control.user_ansi || od_control.user_avatar)
	 {
	    od_repeat((unsigned char)196, 79);
	 }
	 else
	 {
	    od_repeat('-', 79);
	 }*/
	 od_printf("\n\r");
	 if(od_control.user_ansi || od_control.user_avatar) {
	   if(colorsch==COLOR_DEF)
	     od_printf("`bright red`");
	   else
	     od_printf("`bright black`");
	   od_printf("                      ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n\r");
	   if(colorsch==COLOR_DEF)
	     od_printf("`red`");
	   else
	     od_printf("`bright black`");

	 } else {
	   od_printf("                      -------------------------------------\n\r");
	 }
	 if(colorsch==COLOR_DEF) {
	   od_printf("                      (`bright green`V`red`)`green` Vote on all new questions\n\r`red`");
	   od_printf("                      (`bright green`I`red`)`green` Vote on an individual question\n\r`red`");
	   od_printf("                      (`bright green`R`red`)`green` View the results of all questions\n\r`red`");
	   od_printf("                      (`bright green`O`red`)`green` View the results of one question\n\r`red`");
	 } else {
	   od_printf("                      (`bright`V`bright black`) Vote on all new questions\n\r");
	   od_printf("                      (`bright`I`bright black`) Vote on an individual question\n\r");
	   od_printf("                      (`bright`R`bright black`) View the results of all questions\n\r");
	   od_printf("                      (`bright`O`bright black`) View the results of one question\n\r");
	 }

	 /* Display Add New Question option if adding questions is enabled, */
	 /* or if the system operator is using the door.                    */
	 if(bAllowAdd || sysop_sec<=od_control.user_security ||
	   strcmp(od_control.sysop_name, od_control.user_name) == 0)
	 {
	   if(colorsch==COLOR_DEF)
	     od_printf("                      (`bright green`A`red`)`green` Add a new question\n\r`red`");
	   else
	     od_printf("                      (`bright`A`bright black`) Add a new question\n\r");
	 }

	 /* If current user is the system operator, add a D function to permit */
	 /* deletion of unwanted questions. and if users can delete their own! */
	 if(sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0  || bAllowDelete)
	 {
	    if(colorsch==COLOR_DEF)
	      od_printf("                      (`bright green`D`red`)`green` Delete questions\n\r`red`");
	    else
	      od_printf("                      (`bright`D`bright black`) Delete questions\n\r");
	 }
	 if(sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0)
	 {
	    if(colorsch==COLOR_DEF)
	      od_printf("                      (`bright green`E`red`)`green` Edit questions\n\r`red`");
	    else
	      od_printf("                      (`bright`E`bright black`) Edit questions\n\r");

	 }
	 if(sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0  || bAllowChange)
	 {
	    if(colorsch==COLOR_DEF)
	      od_printf("                      (`bright green`C`red`)`green` Change your answers\n\r`red`");
	    else
	      od_printf("                      (`bright`C`bright black`) Change your answers\n\r");
	 }
	 if(allow_page==TRUE) {
	   if(colorsch==COLOR_DEF)
	     od_printf("                      (`bright green`P`red`)`green` Page system operator for chat\n\r`red`");
	   else
	     od_printf("                      (`bright`P`bright black`) Page system operator for chat\n\r");

	 }
	 if(colorsch==COLOR_DEF)
	   od_printf("                      (`bright green`Q`red`)`green` Exit door and return to the BBS\n\r`red`");
	 else
	   od_printf("                      (`bright`Q`bright black`) Exit door and return to the BBS\n\r");

	 if(od_control.user_ansi || od_control.user_avatar) {
	   if(colorsch==COLOR_DEF)
	     od_printf("`bright red`");
	   od_printf("                      ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n\r");
	 } else {
	   od_printf("                      -------------------------------------\n\r");
	 }
	 if(colorsch==COLOR_DEF)
	   od_printf("                      `bright yellow`Time Left: %d mins\n\r",od_control.user_timelimit);
	 else
	   od_printf("                      `bright black`Time Left`white`: `bright`%d `bright black`mins\n\r",od_control.user_timelimit);

	 if(od_control.user_ansi || od_control.user_avatar) {
	   if(colorsch==COLOR_DEF)
	     od_printf("`bright red`");
	   od_printf("                      ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n\r");
	 } else {
	   od_printf("                      -------------------------------------\n\r");
	 }
	 if(colorsch==COLOR_DEF)
	   od_printf("                      `bright`Enter yer choice >`green`");
	 else
	   od_printf("                      `bright black`Enter yer choice `white`>`bright`");

      }
      /* Get the user's choice from the main menu. This choice may only be */
      /* V, R, A, D, P, E or H.                                            */
      chMenuChoice = od_get_answer("VIROADECPQ");
      od_printf("%c\n\r",chMenuChoice);

      /* Perform the appropriate action based on the user's choice */
      switch(chMenuChoice)
      {
	 case 'V':
	    /* Call EZVote's function to vote on new questions */
	    VoteOnQuestion(TRUE);
	    break;

	 case 'I':
	    /* Call EZVote's function to vote on question */
	    VoteOnQuestion(FALSE);
	    break;

	 case 'R':
	    /* Call EZVote's function to view the results of all voting */
	    ViewResults(TRUE);
	    break;

	 case 'O':
	    /* Call EZVote's function to view the results of voting */
	    ViewResults(FALSE);
	    break;

	 case 'A':
	    /* Call EZVote's function to add a new question if door is */
	    /* configured to allow the addition of question.           */
	    if(bAllowAdd || sysop_sec<=od_control.user_security ||
	       strcmp(od_control.sysop_name, od_control.user_name) == 0)
	    {
	       AddQuestion();
	    }
	    break;

	 case 'E':
	    if(sysop_sec<=od_control.user_security ||
	       strcmp(od_control.sysop_name, od_control.user_name) == 0)
	    {
	       QuestionEditor();
	    }
	    break;

	 case 'D':
	    /* Call EZVote's funciton to delete an existing question */
	    DeleteQuestion();
	    break;

	 case 'C':
	    /* Call EZVote's function to add a new question if door is */
	    /* configured to allow the addition of question.           */
	    if(bAllowChange || sysop_sec<=od_control.user_security ||
	       strcmp(od_control.sysop_name, od_control.user_name) == 0)
	    {
	       ChangeQuestion();
	    }
	    break;

	 case 'P':
	    /* If the user pressed P, allow them page the system operator. */
	    if(allow_page==TRUE)
	      od_page();
	    break;

	 case 'Q':
	    /* If the user pressed Q, exit door and return to BBS. */
	    od_exit(0, FALSE);
	    break;

      }
   }

//   return(0);
}


void
dump(void)
{
  char key;
  do {
    scanf("%c",&key);
  } while (key!='\n');
}




/* CustomConfigFunction() is called by OpenDoors to process custom */
/* configuration file keywords that EZVote uses.                   */
void CustomConfigFunction(char *pszKeyword, char *pszOptions)
{
   int intval;

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

   if(stricmp(pszKeyword, "ColorScheme") == 0)
   {
      /* If keyword is ViewUnanswered, set local variable based on contents */
      /* of options string.                                                 */
      if(stricmp(pszOptions, "DEF") == 0)
      {
	colorsch=COLOR_DEF;
      }
      else if(stricmp(pszOptions, "TNW") == 0)
      {
	colorsch=COLOR_TNW;
      }
   }


   if(stricmp(pszKeyword, "AnswerAdding") == 0)
   {
      if(stricmp(pszOptions, "Never") == 0)
      {
	answer_adding=0;
      }
      else if(stricmp(pszOptions, "Optional") == 0)
      {
	answer_adding=1;
      }
      else if(stricmp(pszOptions, "Forced") == 0)
      {
	answer_adding=2;
      }
   }

   if(stricmp(pszKeyword, "MultipleAdds") == 0)
   {
      if(stricmp(pszOptions, "Never") == 0)
      {
	more_adds_p_u=0;
      }
      else if(stricmp(pszOptions, "Optional") == 0)
      {
	more_adds_p_u=1;
      }
      else if(stricmp(pszOptions, "Forced") == 0)
      {
	more_adds_p_u=2;
      }

   }


   if(stricmp(pszKeyword, "NoSameAnswer") == 0)
   {
      /* If keyword is ViewUnanswered, set local variable based on contents */
      /* of options string.                                                 */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	no_same_answers=TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	no_same_answers=FALSE;
      }
   }

   if(stricmp(pszKeyword, "AllowAnonymous") == 0)
   {
      /* If keyword is ViewUnanswered, set local variable based on contents */
      /* of options string.                                                 */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	anonymous_posting=TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	anonymous_posting=FALSE;
      }
   }

   if(stricmp(pszKeyword, "MultipleAnswers") == 0)
   {
      /* If keyword is ViewUnanswered, set local variable based on contents */
      /* of options string.                                                 */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	multiple_answers=TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	multiple_answers=FALSE;
      }
   }

   if(stricmp(pszKeyword, "ViewOwnResults") == 0)
   {
      /* If keyword is ViewUnanswered, set local variable based on contents */
      /* of options string.                                                 */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	view_own_results=TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	view_own_results=FALSE;
      }
   }

   if(stricmp(pszKeyword, "AllowPage") == 0)
   {
      /* If keyword is ViewUnanswered, set local variable based on contents */
      /* of options string.                                                 */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	allow_page=TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	allow_page=FALSE;
      }
   }


   else if(stricmp(pszKeyword, "AllowAdd") == 0)
   {
      /* If keyword is ViewUnvoted, set local variable based on contents */
      /* of options string.                                              */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	 bAllowAdd = TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	 bAllowAdd = FALSE;
      }
   }
   else if(stricmp(pszKeyword, "AddSecurity") == 0)
   {
      sscanf(pszOptions,"%d",&AddSecurity);
      if(AddSecurity<0) AddSecurity=0;
   }
   else if(stricmp(pszKeyword, "MinSecurity") == 0)
   {
      sscanf(pszOptions,"%d",&AllowIn);
      if(AllowIn<0) AllowIn=0;
   }

   else if(stricmp(pszKeyword, "ColoredResults") == 0)
   {
      /* If keyword is ViewUnvoted, set local variable based on contents */
      /* of options string.                                              */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	 ColoredResults = TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	 ColoredResults = FALSE;
      }
   }
   else if(stricmp(pszKeyword, "AllowUnvote") == 0)
   {
      /* If keyword is ViewUnvoted, set local variable based on contents */
      /* of options string.                                              */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	 bAllowUnvote = TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	 bAllowUnvote = FALSE;
      }
   }

   else if(stricmp(pszKeyword, "AllowChange") == 0)
   {
      /* If keyword is ViewUnvoted, set local variable based on contents */
      /* of options string.                                              */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	 bAllowChange = TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	 bAllowChange = FALSE;
      }
   }

   else if(stricmp(pszKeyword, "AllowDelete") == 0)
   {
      /* If keyword is ViewUnvoted, set local variable based on contents */
      /* of options string.                                              */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	 bAllowDelete = TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	 bAllowDelete = FALSE;
      }
   }
   else if(stricmp(pszKeyword, "DeleteAfter") == 0)
   {
     sscanf(pszOptions,"%d",&intval);
     maxtime= intval*24*60*60;
   }
   else if(stricmp(pszKeyword, "SysopSecurity") == 0)
   {
     sscanf(pszOptions,"%d",&sysop_sec);
   }
   else if(stricmp(pszKeyword, "UseHandles") == 0)
   {
      /* If keyword is ViewUnvoted, set local variable based on contents */
      /* of options string.                                              */
      if(stricmp(pszOptions, "Yes") == 0)
      {
	 bHandles = TRUE;
      }
      else if(stricmp(pszOptions, "No") == 0)
      {
	 bHandles = FALSE;
      }
   }



}

void
maint(void)
{
  FILE *fpQuestionFile;
  FILE *fpPackFile;
  FILE *fpUserFile;
  time_t nowtime;
  tQuestionRecord QuestionRecord;
  tUserRecord UserRecord;
  int nFileQuestion=0;
  int deleted_n=0;
  char bQuestionDeleted[MAX_QUESTIONS];
  int cnt,cnt2;
  int nFileUser=0;


   nowtime=time(NULL);

   /* Attempt to open question file. */
   fpQuestionFile = ExculsiveFileOpen(QUESTION_FILENAME, "r+b");

   /* If unable to open question file, assume that no questions have been */
   /* created.                                                            */
   if(fpQuestionFile == NULL)
   {
      return;
   }

   /* Loop for every question record in the file. */
   while(freadQuestionRecord(&QuestionRecord, fpQuestionFile) == 1)
   {
      if(maxtime!=0 && (nowtime-QuestionRecord.lCreationTime)>maxtime
       && (QuestionRecord.bitflags & NEVER_DELETE_QUESTION)==FALSE) {
	fseek(fpQuestionFile,(long)nFileQuestion*QUESTIONRECORD_SIZE,SEEK_SET);
	QuestionRecord.bitflags|=QUESTION_DELETED;
	fwriteQuestionRecord(&QuestionRecord, fpQuestionFile);
	fseek(fpQuestionFile,(long)(1+nFileQuestion)*QUESTIONRECORD_SIZE,SEEK_SET);
      }

      if(QuestionRecord.bitflags & QUESTION_DELETED) {
	deleted_n++;
	bQuestionDeleted[nFileQuestion]=TRUE;
      } else {
	bQuestionDeleted[nFileQuestion]=FALSE;
      }

      /* Move to next question in file. */
      ++nFileQuestion;
   }

   if(deleted_n>0) {
     fseek(fpQuestionFile,0,SEEK_SET);
     nFileQuestion=0;
     od_printf("\n\n\rThere are deleted questions!\n\n\rI will pack the question file NOW!\n\n\r");
     fpPackFile=ExculsiveFileOpen("freevote.tmp","wb");
     while(freadQuestionRecord(&QuestionRecord, fpQuestionFile) == 1)
     {
       if((QuestionRecord.bitflags & QUESTION_DELETED)==0)
	 fwriteQuestionRecord(&QuestionRecord,fpPackFile);

       /* Move to next question in file. */
       ++nFileQuestion;
     }
     fclose(fpQuestionFile);
     fclose(fpPackFile);
     remove("freevote.qst");
     rename("freevote.tmp","freevote.qst");
     od_printf("\n\rUserFile Packing");
     fpUserFile=ExculsiveFileOpen(USER_FILENAME,"rb");
     fpPackFile=ExculsiveFileOpen("freevote.tmp","wb");
     nFileUser=0;
     while(freadUserRecord(&UserRecord, fpUserFile) == 1)
     {
       deleted_n=0;
       for(cnt=0;cnt<MAX_QUESTIONS;cnt++) {
	 if(bQuestionDeleted[cnt]==TRUE) {
	   for(cnt2=(cnt-deleted_n);cnt2<(MAX_QUESTIONS-1);cnt2++) {
	     UserRecord.bVotedOnQuestion[cnt2]=UserRecord.bVotedOnQuestion[cnt2+1];
	   }
	   deleted_n++;
	 }
       }

       cnt2=0;
       for(cnt=0;cnt<nFileQuestion;cnt++) {
	 if(UserRecord.bVotedOnQuestion[cnt]>0)
	   cnt2++;
       }

//       od_printf("%s\n\r%d\n\r",UserRecord.szUserName,(int)UserRecord.szUserName[0]);

       if(UserRecord.szUserName[0]==0)
	 cnt2=0;

       if(cnt2>0) {
  //     od_printf("DEBUG:Wrote!\n\n\r");
	 fwriteUserRecord(&UserRecord,fpPackFile);
       }

    //   WaitForEnter();

       /* Move to next user in file. */
       ++nFileUser;
     }
     fclose(fpUserFile);
     fclose(fpPackFile);
     remove("freevote.usr");
     rename("freevote.tmp","freevote.usr");
     od_printf("Done!!\n\r\n");
   } else {
     fclose(fpQuestionFile);
   }
   /* Close question file to allow other nodes to access the file. */
}

FILE
*QRead(tQuestionRecord *QuestionRecord, int nQuestion)
{
  FILE *fpFile;

  /* Open question file for exclusive access by this node. */
  fpFile = ExculsiveFileOpen(QUESTION_FILENAME, "r+b");
  if(fpFile == NULL) {
  /* If unable to access file, display error and return. */
    od_printf("Unable to access the question file.\n\r");
    WaitForEnter();
    return(NULL);
  }

  /* Read the answer record from disk, because it may have been changed. */
  /* by another node. */
  fseek(fpFile, (long)nQuestion * QUESTIONRECORD_SIZE, SEEK_SET);
  if(freadQuestionRecord(QuestionRecord, fpFile) != 1) {
    /* If unable to access file, display error and return. */
    fclose(fpFile);
    od_printf("\n\rUnable to read from question file.\n\r");
    WaitForEnter();
    return(NULL);
  }
  return(fpFile);
}

int
QWrite(tQuestionRecord *QuestionRecord, FILE *fpFile, int nQuestion)
{
  /* Write the question record back to the file. */
  fseek(fpFile, (long)nQuestion * QUESTIONRECORD_SIZE, SEEK_SET);
  if(fwriteQuestionRecord(QuestionRecord, fpFile) != 1) {
    /* If unable to access file, display error and return. */
    fclose(fpFile);
    od_printf("\n\rUnable to write question to file.\n\r");
    WaitForEnter();
    return(0);
  }

  /* Close the question file to allow access by other nodes. */
  fclose(fpFile);

  return(1);
}

void
QuestionEditor(void)
{
   int nQuestion=-1;
   int nAnswer;
   unsigned int votedon;
   tQuestionRecord QuestionRecord;
   char szNewAnswer[ANSWER_STR_SIZE];
   char szNewAnswer2[ANSWER_STR_SIZE];
   char szQuestion[QUESTION_STR_SIZE];
   char szUserInput[3];
   char key,skey;
   FILE *fpUserFile;
   int nFileUser;
   tUserRecord UserRecord;
   FILE *fpFile;
   int nPageLocation = 0;
   int cnt,cnt2,x,alad;
/*   int aladd;*/
//   char addcnt=TRUE;


   /* Loop until the user chooses to return to the main menu, or until */
   /* there are no more questions to vote on.                          */
   for(;;) {
      nextques:

/*      aladd=TRUE;*/
  //    addcnt=TRUE;

      nQuestion = ChooseQuestion(QUESTIONS_NOT_VOTED_ON | QUESTIONS_VOTED_ON,
	  "                               Edit A Question\n\r",
	   &nPageLocation);

      /* If the user did not choose a question, return to main menu. */
      if(nQuestion == NO_QUESTION) {
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
      alad=TRUE;
      if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	if(QuestionRecord.nTotalAnswers >= MAX_ANSWERS)
	{
	   alad=FALSE;
	}
      } else {
	if(QuestionRecord.nTotalAnswers >= MAX_ANSWERS2)
	{
	   alad=FALSE;
	}
      }

      /* Loop until user makes a valid respose. */
      for(;;)
      {
	 /* Display question to user. */

	 /* Clear the screen. */
//       od_printf("\n\n\r");
	 my_clr_scr();

	 /* Display question itself. */
	 od_printf("`bright`Q) `bright red`%s\n\r", QuestionRecord.szQuestion);

	 od_printf("`bright`N) `red`By: `bright red`%s",QuestionRecord.szCreatorName);
	 od_printf("     `red`People that voted: `bright red`%u\n\n\r",QuestionRecord.uTotalVotes);
	 od_printf("`bright`!) `red`Never delete: `bright red`");
	 if(QuestionRecord.bitflags & NEVER_DELETE_QUESTION)
	   od_printf("ON  ");
	 else
	   od_printf("OFF ");

	 od_printf("`bright`@) `red`Can Add Answers: `bright red`");
	 if(QuestionRecord.bitflags & CAN_ADD_ANSWERS)
	   od_printf("ON  ");
	 else
	   od_printf("OFF ");

	 od_printf("`bright`#) `red`Multiple Answer Question: `bright red`");
	 if(QuestionRecord.bitflags & MULTIPLE_ANSWERS)
	   od_printf("ON\n\r");
	 else
	   od_printf("OFF\n\r");

	 od_printf("`bright`$) `red`Anonymous: `bright red`");
	 if(QuestionRecord.bitflags & ANONYMOUS_QUESTION)
	   od_printf("ON  ");
	 else
	   od_printf("OFF ");

	 od_printf("   `bright`*) `red`Forced Question: `bright red`");
	 if(QuestionRecord.bitflags & FORCED_QUESTION)
	   od_printf("ON  ");
	 else
	   od_printf("OFF ");

	 od_printf("`bright`&) `red`Multiple Adds Per User: `bright red`");
	 if(QuestionRecord.bitflags & MULTIPLE_ADDS_P_U)
	   od_printf("ON\n\n\r");
	 else
	   od_printf("OFF\n\n\r");

	 /* Loop for each answer to the question. */
	 for(nAnswer = cnt = cnt2 = 0; nAnswer < QuestionRecord.nTotalAnswers; ++nAnswer)
	 {
	    /* Display answer number and answer. */
	    cnt2++;
	    od_printf("`bright`%2d) `green`%s  `red`Votes: `bright red`%u",
	      nAnswer + 1,
	      QuestionRecord.aszAnswer[cnt],
	      QuestionRecord.auVotesForAnswer[nAnswer]);
	      od_printf("\n\r");
	    if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	      cnt++;
	    } else {
	      if(strlen(QuestionRecord.aszAnswer[cnt+1])>0)
		od_printf("    `green`%s\n\r",QuestionRecord.aszAnswer[cnt+1]);
	      cnt+=2;
	    }
/*          } else {
	      if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
		cnt++;
	      } else {
		cnt+=2;
	      }
	    }*/
	 }

	 /* Display prompt to user. */
	 od_printf("\n\r`bright`Enter what to change or [E]rase answer, ");
	 if(alad)
	   od_printf("[A]dd answer, ");
	 od_printf("[R]eset votes, [D]one: `green`");

	 /* Get response from user. */
	 od_input_str(szUserInput, 2, ' ', 255);
	 /* Add a blank line. */
	 od_printf("\n\r");

	 /* If user entered Q, return to main menu. */
	 if (szUserInput[0]=='D' || szUserInput[0]=='d')
	 {
	     goto nextques;
	 }

	 nAnswer = atoi(szUserInput) - 1;

	 /* If user enetered A, and adding answers is premitted ... */
	 if ((szUserInput[0]=='A' || szUserInput[0]=='a')
	    && alad==TRUE)
	 {
	    /* ... Prompt for answer from user. */
	    od_printf("`bright green`Please enter the new answer:\n\r");
	    od_printf("`green`[------------------------------]\n\r ");


	    if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	      /* Get string from user. */
	      od_input_str(szNewAnswer,
		ANSWER_STR_SIZE - 1, ' ', 255);
	    } else {
	      warp_line(szNewAnswer,szNewAnswer2," ");
	    }


	    /* If user entered a valid answer, then exit loop. */
	    if(strlen(szNewAnswer) > 0 || ((QuestionRecord.bitflags & BF_15_BY_1_MODE)&&strlen(szNewAnswer2) > 0))
	    {
	       /* Check that there is still room for another answer. */
	       if(QuestionRecord.bitflags & BF_15_BY_1_MODE)
		 cnt=MAX_ANSWERS;
	       else
		 cnt=MAX_ANSWERS2;

	       if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
		 return;

	       if(QuestionRecord.nTotalAnswers >= cnt) {
		 fclose(fpFile);
		 od_printf("Sorry, this question already has the maximum number of answers.\n\r");
		 WaitForEnter();
		 return;
	       }

	       nAnswer=QuestionRecord.nTotalAnswers;

	       ++QuestionRecord.nTotalAnswers;

	       /* Initialize new answer string and count. */
	       if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
		 strcpy(QuestionRecord.aszAnswer[nAnswer], szNewAnswer);
	       } else {
		 strcpy(QuestionRecord.aszAnswer[nAnswer*2], szNewAnswer);
		 strcpy(QuestionRecord.aszAnswer[(nAnswer*2)+1], szNewAnswer2);
	       }
	       QuestionRecord.auVotesForAnswer[nAnswer] = 0;

	       if(!QWrite(&QuestionRecord,fpFile,nQuestion))
		 return;
	    }

	 /* If user enetered E*/
	 } else if (szUserInput[0]=='E' || szUserInput[0]=='e') {
	    /* ... Prompt for answer from user. */
	    od_printf("`bright`Which answer to erase or [Q]uit: `green`");

	    od_input_str(szUserInput, 2, ' ', 255);

	    if(szUserInput[0]!='Q' && szUserInput[0]!='q') {
	      nAnswer = atoi(szUserInput) - 1;

	      if(nAnswer >= 0 && nAnswer < QuestionRecord.nTotalAnswers) {

		od_printf("\n\r`bright`Are you sure? (Y/N) `green`");
		if(od_get_answer("YN")=='Y') {
		  od_printf("Yes\n\r\n`red`Working ...");
		  if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
		    return;

		  if(nAnswer<(QuestionRecord.nTotalAnswers-1)) {
		    for(x=nAnswer;x<(QuestionRecord.nTotalAnswers-1);x++) {
		      if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
			strcpy(QuestionRecord.aszAnswer[x], QuestionRecord.aszAnswer[x+1]);
		      } else {
			strcpy(QuestionRecord.aszAnswer[x*2], QuestionRecord.aszAnswer[((x+1)*2)]);
			strcpy(QuestionRecord.aszAnswer[(x*2)+1], QuestionRecord.aszAnswer[((x+1)*2)+1]);
		      }
		      QuestionRecord.auVotesForAnswer[x] = QuestionRecord.auVotesForAnswer[x+1];
		    }
		    fpUserFile=ExculsiveFileOpen(USER_FILENAME,"r+b");
		    nFileUser=0;
		    while(freadUserRecord(&UserRecord, fpUserFile) == 1) {
		      votedon=UserRecord.bVotedOnQuestion[nQuestion];

		      UserRecord.bVotedOnQuestion[nQuestion]=0;
		      if(votedon & answers[15])
			UserRecord.bVotedOnQuestion[nQuestion] |= answers[15];

		      for(x=0;x<nAnswer;x++) {
			if(votedon & answers[x])
			  UserRecord.bVotedOnQuestion[nQuestion] |= answers[x];
		      }

		      for(x=nAnswer;x<(QuestionRecord.nTotalAnswers-1);x++) {
			if(votedon & answers[x+1])
			  UserRecord.bVotedOnQuestion[nQuestion] |= answers[x];
		      }

		      fseek(fpUserFile,(long)nFileUser * (long)USERRECORD_SIZE,SEEK_SET);
		      fwriteUserRecord(&UserRecord, fpUserFile);
		      ++nFileUser;
		      fseek(fpUserFile,(long)nFileUser * (long)USERRECORD_SIZE,SEEK_SET);
		    }
		    fclose(fpUserFile);
		    votedon=CurrentUserRecord.bVotedOnQuestion[nQuestion];
		    CurrentUserRecord.bVotedOnQuestion[nQuestion]=0;
		    if(votedon & answers[15])
		      CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[15];

		    for(x=0;x<nAnswer;x++) {
		      if(votedon & answers[x])
			CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[x];
		    }

		    for(x=nAnswer;x<(QuestionRecord.nTotalAnswers-1);x++) {
		      if(votedon & answers[x+1])
			CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[x];
		    }

		    WriteCurrentUser();
		  } else {
		    fpUserFile=ExculsiveFileOpen(USER_FILENAME,"r+b");
		    nFileUser=0;
		    while(freadUserRecord(&UserRecord, fpUserFile) == 1) {
		      UserRecord.bVotedOnQuestion[nQuestion] &=~ answers[nAnswer];
		      fseek(fpUserFile,(long)nFileUser * (long)USERRECORD_SIZE,SEEK_SET);
		      fwriteUserRecord(&UserRecord, fpUserFile);
		      ++nFileUser;
		      fseek(fpUserFile,(long)nFileUser * (long)USERRECORD_SIZE,SEEK_SET);
		    }
		    fclose(fpUserFile);
		    CurrentUserRecord.bVotedOnQuestion[nQuestion] &=~ answers[nAnswer];
		    WriteCurrentUser();
		  }
		  QuestionRecord.nTotalAnswers--;

		  if(!QWrite(&QuestionRecord,fpFile,nQuestion))
		    return;

		} else {
		  od_printf("No");
		}
	      }
	    }
	 } else if ((szUserInput[0]=='Q' || szUserInput[0]=='q')) {

	   od_printf("`bright green`Enter The Question (blank line cancels)\n\r");
	   od_printf("`green`[----------------------------------------------------------------------]\n\r ");
	   od_input_str(szQuestion, QUESTION_STR_SIZE - 1, ' ', 255);

	   if(szQuestion[0]!=0) {
	     if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	       return;

	     strcpy(QuestionRecord.szQuestion,szQuestion);

	     if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	       return;
	   }

	 } else if ((szUserInput[0]=='N' || szUserInput[0]=='n')) {

	   od_printf("`bright green`Enter The Creator's Name\n\r");
	   od_printf("`green`[----------------------------------]\n\r ");
	   od_input_str(szQuestion, 35, ' ', 255);

	   if(szQuestion[0]!=0) {
	     if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	       return;

	     strcpy(QuestionRecord.szCreatorName,szQuestion);

	     if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	       return;
	    }
	 } else if (szUserInput[0]=='R' || szUserInput[0]=='r') {
	   od_printf("`bright red`Reset what?\n\n\r");
	   od_printf("`bright`1 - `bright red`Vote counts\n\r");
	   od_printf("`bright`2 - `bright red`Make All users vote on this question again\n\r");
	   od_printf("`bright`3 - `bright red`Do both ... recommended\n\r");
	   od_printf("`bright`Q - `bright red`Quit\n\n\r");
	   od_printf("`bright red`So what is it:");
	   key=od_get_answer("123Q");
	   if(key!='Q') {
	     od_printf(" `bright green`%c\n\n\r",key);
	     od_printf("`bright`Ya sure ya wanna do that? (Y/N)");
	     if(od_get_answer("YN")=='Y') {
	       od_printf(" `bright green`Yes\n\n\r`red`Working...");
	       if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
		 return;

	       if(key=='1' || key=='3') {
		 for(x=0;x<15;x++) {
		   QuestionRecord.auVotesForAnswer[x] = 0;
		 }
		 QuestionRecord.uTotalVotes=0;
	       }
	       if(key=='2' || key=='3') {
		 fpUserFile=ExculsiveFileOpen(USER_FILENAME,"r+b");
		 nFileUser=0;
		 while(freadUserRecord(&UserRecord, fpUserFile) == 1) {
		   UserRecord.bVotedOnQuestion[nQuestion]=0;
		   fseek(fpUserFile,(long)nFileUser * (long)USERRECORD_SIZE,SEEK_SET);
		   fwriteUserRecord(&UserRecord, fpUserFile);
		   ++nFileUser;
		   fseek(fpUserFile,(long)nFileUser * (long)USERRECORD_SIZE,SEEK_SET);
		 }
		 fclose(fpUserFile);
		 CurrentUserRecord.bVotedOnQuestion[nQuestion]=0;
		 WriteCurrentUser();
	       }

	       if(!QWrite(&QuestionRecord,fpFile,nQuestion))
		 return;
	     } else {
	       od_printf(" `bright green`No");
	     }
	   } else {
	     od_printf(" `bright green`Quit");
	   }
	 } else if (szUserInput[0]=='!') {
	   if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	     return;

	   QuestionRecord.bitflags ^= NEVER_DELETE_QUESTION;

	   if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	     return;
	 } else if (szUserInput[0]=='@') {
	   if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	     return;

	   QuestionRecord.bitflags ^= CAN_ADD_ANSWERS;

	   if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	     return;
	 } else if (szUserInput[0]=='#') {
	   if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	     return;

	   QuestionRecord.bitflags ^= MULTIPLE_ANSWERS;

	   if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	     return;
	 } else if (szUserInput[0]=='$') {
	   if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	     return;

	   QuestionRecord.bitflags ^= ANONYMOUS_QUESTION;

	   if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	     return;
	 } else if (szUserInput[0]=='&') {
	   if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	     return;

	   QuestionRecord.bitflags ^= MULTIPLE_ADDS_P_U;

	   if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	     return;
	 } else if (szUserInput[0]=='*') {
	   if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	     return;

	   QuestionRecord.bitflags ^= FORCED_QUESTION;

	   if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	     return;
	 } else if(nAnswer < 0 || nAnswer >= QuestionRecord.nTotalAnswers) {
	    /* Display message. */
	    od_printf("That is not a valid response.\n\r\n");
	    WaitForEnter();
	 } else {
	    od_printf("`bright green`Please enter the new answer:\n\r");
	    od_printf("`green`[------------------------------]\n\r ");
	    if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	      /* Get string from user. */
	      od_input_str(szNewAnswer,
		ANSWER_STR_SIZE - 1, ' ', 255);
	    } else {
	      warp_line(szNewAnswer,szNewAnswer2," ");
	    }
	    if(strlen(szNewAnswer) > 0 || ((QuestionRecord.bitflags & BF_15_BY_1_MODE)&&strlen(szNewAnswer2) > 0))
	    {
	       if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
		 return;

	       /* Initialize new answer string and count. */
	       if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
		 strcpy(QuestionRecord.aszAnswer[nAnswer], szNewAnswer);
	       } else {
		 strcpy(QuestionRecord.aszAnswer[nAnswer*2], szNewAnswer);
		 strcpy(QuestionRecord.aszAnswer[(nAnswer*2)+1], szNewAnswer2);
	       }

	       if(!QWrite(&QuestionRecord,fpFile,nQuestion))
		 return;
	    }
	 }
      }
   }
}


/* EZVote configures OpenDoors to call the BeforeExitFunction() before    */
/* the door exists for any reason. You can use this function to close any */
/* files or perform any other operations that you wish to have peformed   */
/* before OpenDoors exists for any reason. The od_control.od_before_exit  */
/* variable sets the function to be called before program exit.           */
void BeforeExitFunction(void)
{
   char szLogMessage[80];

   WriteCurrentUser();

   /* Write number of messages voted on to log file. */
   sprintf(szLogMessage, "User has voted on %d question(s)",
     nQuestionsVotedOn);

   od_log_write(szLogMessage);

   if(Forcevote!=4) {
     if(colorsch==COLOR_DEF)
       od_printf("\n\r\n`bright red`You have voted on %d question(s)\n\r\n", nQuestionsVotedOn);
     else
       od_printf("\n\r\n`bright black`You have voted on `bright`%d`bright black` question(s)\n\r\n", nQuestionsVotedOn);

     if(colorsch==COLOR_DEF)
       od_printf("`bright green`Returning ...");
     else
       od_printf("`bright black`Returning ...");
   }
   od_control.user_security=b_us;
}


/* EZVote calls the VoteOnQuestion() function when the user chooses the   */
/* vote command from the main menu. This function displays a list of      */
/* available topics, asks for the user's answer to the topic they select, */
/* and display's the results of voting on that topic.                     */
void VoteOnQuestion(int all)
{
   int nQuestion=-1;
   int nAnswer;
   tQuestionRecord QuestionRecord;
   char szNewAnswer[ANSWER_STR_SIZE];
   char szNewAnswer2[ANSWER_STR_SIZE];
   char szUserInput[3];
   FILE *fpFile;
   int nPageLocation = 0;
   int cnt,cnt2,x;
/*   int aladd;*/
//   char addcnt=TRUE;


   /* Loop until the user chooses to return to the main menu, or until */
   /* there are no more questions to vote on.                          */
   for(;;)
   {
      skipques:

/*      aladd=TRUE;*/
  //    addcnt=TRUE;

      if (all==FALSE) {
	/* Allow the user to choose a question from the list of questions */
	/* that they have not voted on.                                   */
	nQuestion = ChooseQuestion(QUESTIONS_NOT_VOTED_ON,
	  "                              Vote On A Question\n\r",
	   &nPageLocation);
      } else {
	if(Forcevote<100) {
	  nQuestion = FirstQuestion(QUESTIONS_NOT_VOTED_ON,nQuestion+1);
	} else {
	  nQuestion = FirstQuestion(QUESTIONS_NOT_VOTED_ON | QUESTIONS_FORCED,nQuestion+1);
	}
      }


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

      moreanswers:

      /* Don't allow addition of new answers if maximum number of answers */
      /* have already been added.                                         */
      if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	if(QuestionRecord.nTotalAnswers >= MAX_ANSWERS)
	{
	   QuestionRecord.bitflags &=~ CAN_ADD_ANSWERS;
	}
      } else {
	if(QuestionRecord.nTotalAnswers >= MAX_ANSWERS2)
	{
	   QuestionRecord.bitflags &=~ CAN_ADD_ANSWERS;
	}
      }

      /* Loop until user makes a valid respose. */
      for(;;)
      {
	 /* Display question to user. */

	 /* Clear the screen. */
//       od_printf("\n\n\r");
	 my_clr_scr();

	 /* Display question itself. */
	 if(colorsch==COLOR_DEF)
	   od_printf("`bright red`");
	 else
	   od_printf("`bright`");
	 od_printf("%s\n\r", QuestionRecord.szQuestion);

	 if(sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0) {
	   od_printf("`red`(%s)\n\r",QuestionRecord.szCreatorName);
	 } else {
	   od_printf("\n\r");
	 }

	 /* Loop for each answer to the question. */
	 for(nAnswer = cnt = cnt2 = 0; nAnswer < QuestionRecord.nTotalAnswers; ++nAnswer)
	 {
	    /* Display answer number and answer. */
	    cnt2++;
	    if(colorsch==COLOR_DEF)
	      od_printf("`bright green`%2d. `green`%s",
		 nAnswer + 1,
		 QuestionRecord.aszAnswer[cnt]);
	    else
	      od_printf("`bright`%2d. `bright black`%s",
		 nAnswer + 1,
		 QuestionRecord.aszAnswer[cnt]);

	    if((CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[nAnswer])==FALSE)
	      od_printf("\n\r");
	    else {
	      if(colorsch==COLOR_DEF)
		od_printf("`bright red`");
	      else
		od_printf("`bright`");
	      od_printf(" [Voted On]\n\r");
	    }
	    if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	      cnt++;
	    } else {
	      if(strlen(QuestionRecord.aszAnswer[cnt+1])>0) {
		if(colorsch==COLOR_DEF)
		  od_printf("`green`");
		else
		  od_printf("`bright black`");
		od_printf("    %s\n\r",QuestionRecord.aszAnswer[cnt+1]);
	      }
	      cnt+=2;
	    }
/*          } else {
	      if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
		cnt++;
	      } else {
		cnt+=2;
	      }
	    }*/
	 }

	 if(cnt2==0)
	   goto donewith;

	 if(QuestionRecord.bitflags & MULTIPLE_ANSWERS) {
	   if(colorsch==COLOR_DEF)
	     od_printf("`bright`\n\rThis question allows multiple answers, select all you want and press [D]one");
	   else
	     od_printf("`bright black`\n\rThis question allows multiple answers`white`,`bright black` select all you want and press `white`[`bright black`D`white`]`bright black`one");
	 }

	 /* Display prompt to user. */
	 if(colorsch==COLOR_DEF)
	   od_printf("`bright`");
	 else
	   od_printf("`bright black`");
	 od_printf("\n\rEnter answer number");
	 if((QuestionRecord.bitflags & CAN_ADD_ANSWERS) && ((CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[15])==FALSE || QuestionRecord.bitflags & MULTIPLE_ADDS_P_U)) {
	   if(colorsch==COLOR_DEF)
	     od_printf(", [A]dd your own response");
	   else
	     od_printf("`white`, [`bright black`A`white`]`bright black`dd your own response");
	 }
	 if(Forcevote!=2 && Forcevote!=4 && Forcevote<100) {
	   if(all==TRUE) {
	     if(colorsch==COLOR_DEF)
	       od_printf(", [S]kip");
	     else
	       od_printf("`white`, [`bright black`S`white`]`bright black`kip");

	   }
	   if(colorsch==COLOR_DEF)
	     od_printf(", [Q]uit");
	   else
	     od_printf("`white`, [`bright black`Q`white`]`bright black`uit");

	 }
	 if(colorsch==COLOR_DEF)
	   od_printf(": `green`");
	 else
	   od_printf("`white`: `bright`");

	 /* Get response from user. */
	 od_input_str(szUserInput, 2, ' ', 255);
	 /* Add a blank line. */
	 od_printf("\n\r");

	 if(Forcevote!=2 && Forcevote!=4 && Forcevote<100) {
	   /* If user entered Q, return to main menu. */
	   if (szUserInput[0]=='Q' || szUserInput[0]=='q')
	   {
	     return;
	   }

	   if (szUserInput[0]=='S' || szUserInput[0]=='s')
	   {
	     goto skipques;
	   }
	 }

	 if((QuestionRecord.bitflags & MULTIPLE_ANSWERS) && (CurrentUserRecord.bVotedOnQuestion[nQuestion]!=answers[15] && CurrentUserRecord.bVotedOnQuestion[nQuestion]!=0)) {
	   if (szUserInput[0]=='D' || szUserInput[0]=='d')
	   {
	     goto donewith;
	   }
	 }


	 /* If user enetered A, and adding answers is premitted ... */
	 if ((szUserInput[0]=='A' || szUserInput[0]=='a')
	    && (QuestionRecord.bitflags & CAN_ADD_ANSWERS)
	    && ((CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[15])==FALSE || QuestionRecord.bitflags & MULTIPLE_ADDS_P_U))
	 {
/*          aladd=FALSE;*/
	    /* ... Prompt for answer from user. */
	    if(colorsch==COLOR_DEF) {
	      od_printf("`bright green`Please enter your new answer:\n\r");
	      od_printf("`green`[------------------------------]\n\r ");
	    } else {
	      od_printf("`bright black`Please enter your new answer`white`:\n\r");
	      od_printf("`white`[------------------------------]\n\r ");
	    }



	    if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	      /* Get string from user. */
	      od_input_str(szNewAnswer,
		ANSWER_STR_SIZE - 1, ' ', 255);
	    } else {
	      warp_line(szNewAnswer,szNewAnswer2," ");
	    }


	    /* Get string from user. */
/*          od_input_str(szNewAnswer, ANSWER_STR_SIZE - 1, ' ', 255);

	    if(!(QuestionRecord.bitflags & BF_15_BY_1_MODE)) {
	      od_printf(" ");
	      od_input_str(szNewAnswer2, ANSWER_STR_SIZE - 1, ' ', 255);
	    }*/

	    /* Record that user entered a new answer answer. */
	    nAnswer = NEW_ANSWER;

	    /* If user entered a valid answer, then exit loop. */
	    if(strlen(szNewAnswer) > 0 || ((QuestionRecord.bitflags & BF_15_BY_1_MODE)&&strlen(szNewAnswer2) > 0))
	    {
	       break;
	    }
	 }

	 /* Otherwise, attempt to get answer number from user. */
	 nAnswer = atoi(szUserInput) - 1;

	 if((QuestionRecord.bitflags & MULTIPLE_ANSWERS)
	  && (CurrentUserRecord.bVotedOnQuestion[nQuestion]==0 || CurrentUserRecord.bVotedOnQuestion[nQuestion]==answers[15])
	  && (szUserInput[0]=='D' || szUserInput[0]=='d')) {
	   if(colorsch!=COLOR_DEF)
	     od_printf("`bright black`");
	   od_printf("You need to have at least one answer checked");
	   if(colorsch!=COLOR_DEF)
	     od_printf("`white`");
	   od_printf("!\n\r\n");
	   WaitForEnter();
	 }
	 /* If user input is not a valid answer. */
	 else if(nAnswer < 0 || nAnswer >= QuestionRecord.nTotalAnswers) // || (CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[nAnswer]))
	 {
	    /* Display message. */
	    if(colorsch!=COLOR_DEF)
	      od_printf("`bright black`");
	    od_printf("That is not a valid response");
	    if(colorsch!=COLOR_DEF)
	      od_printf("`white`");
	    od_printf(".\n\r\n");
	    WaitForEnter();
	 }
	 else
	 {
	    /* Otherwise, exit loop. */
	    break;
	 }
      }

      /* Add user's vote to question. */

      /* If user entered their own answer, try to add it to the question. */
      if(nAnswer == NEW_ANSWER)
      {
	 /* Check that there is still room for another answer. */
	 if(QuestionRecord.bitflags & BF_15_BY_1_MODE)
	   cnt=MAX_ANSWERS;
	 else
	   cnt=MAX_ANSWERS2;

	 if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	   return;

	 if(QuestionRecord.nTotalAnswers >= cnt)
	 {
	    fclose(fpFile);
	    if(colorsch!=COLOR_DEF)
	      od_printf("`bright black`");
	    od_printf("Sorry, this question already has the maximum number of answers");
	    if(colorsch!=COLOR_DEF)
	      od_printf("`white`");
	    od_printf(".\n\r");
	    WaitForEnter();
	    return;
	 }

	 /* Set answer number to number of new answer. */
	 nAnswer = QuestionRecord.nTotalAnswers;

	 /* Add 1 to total number of answers. */
	 ++QuestionRecord.nTotalAnswers;

	 /* Initialize new answer string and count. */
	 if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	   strcpy(QuestionRecord.aszAnswer[nAnswer], szNewAnswer);
	 } else {
	   strcpy(QuestionRecord.aszAnswer[nAnswer*2], szNewAnswer);
	   strcpy(QuestionRecord.aszAnswer[(nAnswer*2)+1], szNewAnswer2);
	 }
	 QuestionRecord.auVotesForAnswer[nAnswer] = 0;

	 if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	   return;
	 CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[15];
      }

      /* Add user's vote to question. */
      /*else
	--QuestionRecord.auVotesForAnswer[nAnswer];*/
//      if(addcnt)

     // addcnt=FALSE;

      /* Record that user has voted on this question. */
      if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[nAnswer])
	CurrentUserRecord.bVotedOnQuestion[nQuestion] &=~ answers[nAnswer];
      else
	CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[nAnswer];

      if(QuestionRecord.bitflags & MULTIPLE_ANSWERS)
	goto moreanswers;

      donewith:

      if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	return;

      ++QuestionRecord.uTotalVotes;

      for(x=0;x<15;x++) {
	if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[x])
	  ++QuestionRecord.auVotesForAnswer[x];
      }


      if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	return;

      WriteCurrentUser();

      /* Open user file for exclusive access by this node. */
//      fpFile = ExculsiveFileOpen(USER_FILENAME, "r+b");
//      if(fpFile == NULL)
//      {
	 /* If unable to access file, display error and return. */
//       od_printf("Unable to access the user file.\n\r");
//       WaitForEnter();
//       return;
//      }

      /* Update the user's record in the user file. */
//      fseek(fpFile, (long)nCurrentUserNumber * USERRECORD_SIZE, SEEK_SET);
//      if(fwrite(&CurrentUserRecord, sizeof(tUserRecord), 1, fpFile) != 1)
//      {
	 /* If unable to access file, display error and return. */
//       fclose(fpFile);
//       od_printf("Unable to write to user file.\n\r");
//       WaitForEnter();
//       return;
//      }

      /* Close the user file to allow access by other nodes. */
//      fclose(fpFile);



      /* Display the result of voting on this question to the user. */
      DisplayQuestionResult(&QuestionRecord,FALSE);

      /* Add 1 to count of questions that the user has voted on. */
      nQuestionsVotedOn++;
   }
}


void ChangeQuestion(void)
{
   int nQuestion=-1;
   int nAnswer;
   tQuestionRecord QuestionRecord;
   char szNewAnswer[ANSWER_STR_SIZE];
   char szNewAnswer2[ANSWER_STR_SIZE];
   char szUserInput[3];
   FILE *fpFile;
   unsigned int original_answers;
   int nPageLocation = 0;
   int cnt,cnt2,decvotes=FALSE,x;

   /* Loop until the user chooses to return to the main menu, or until */
   /* there are no more questions to vote on.                          */
  for(;;)
  {
      /* Allow the user to choose a question from the list of questions */
      /* that they have not voted on.                                   */
    nQuestion = ChooseQuestion(QUESTIONS_VOTED_ON,
	  "                                Change A Vote\n\r",
	   &nPageLocation);


    /* If the user did not choose a question, return to main menu. */
    if(nQuestion == NO_QUESTION)
    {
      return;
    }

/*    moreanswers:*/

  /* Read the question chosen by the user. */
      if(!GetQuestion(nQuestion, &QuestionRecord))
      {
	 /* If unable to access file, return to main menu. */
	 return;
      }

      original_answers=CurrentUserRecord.bVotedOnQuestion[nQuestion];

      decvotes=FALSE;

      moreanswers:

      /* Don't allow addition of new answers if maximum number of answers */
      /* have already been added.                                         */
      if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	if(QuestionRecord.nTotalAnswers >= MAX_ANSWERS)
	{
	   QuestionRecord.bitflags &=~ CAN_ADD_ANSWERS;
	}
      } else {
	if(QuestionRecord.nTotalAnswers >= MAX_ANSWERS2)
	{
	   QuestionRecord.bitflags &=~ CAN_ADD_ANSWERS;
	}
      }

      /* Loop until user makes a valid respose. */
      for(;;)
      {
	 /* Display question to user. */

	 /* Clear the screen. */
//       od_printf("\n\n\r");
	 my_clr_scr();

	 /* Display question itself. */
	 if(colorsch==COLOR_DEF)
	   od_printf("`bright red`");
	 else
	   od_printf("`bright`");
	 od_printf("%s\n\r\n\r", QuestionRecord.szQuestion);

	 /* Loop for each answer to the question. */
	 for(nAnswer = cnt = cnt2 = 0; nAnswer < QuestionRecord.nTotalAnswers; ++nAnswer)
	 {
/*          if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[nAnswer]) {
	      od_printf("`bright red`->");
	    } else {
	      od_printf("  ");
	    }*/
	    /* Display answer number and answer. */
	    cnt2++;
	    if(colorsch==COLOR_DEF)
	      od_printf("`bright green`%2d. `green`%s",
		nAnswer + 1,
		QuestionRecord.aszAnswer[cnt]);
	    else
	      od_printf("`bright`%2d. `bright black`%s",
		nAnswer + 1,
		QuestionRecord.aszAnswer[cnt]);
	    if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[nAnswer]) {
	      if(colorsch==COLOR_DEF)
		od_printf("`bright red`");
	      else
		od_printf("`bright`");
	      od_printf(" [Voted On]\n\r");
	    } else {
	      od_printf("\n\r");
	    }

	    if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	      cnt++;
	    } else {
	      if(strlen(QuestionRecord.aszAnswer[cnt+1])>0)
		if(colorsch==COLOR_DEF)
		  od_printf("`green`");
		else
		  od_printf("`bright black`");
		od_printf("    %s\n\r",QuestionRecord.aszAnswer[cnt+1]);
	      cnt+=2;
	    }
	 }

	 if(cnt2==0)
	   goto donewith;

	 if(QuestionRecord.bitflags & MULTIPLE_ANSWERS) {
	   if(colorsch==COLOR_DEF)
	     od_printf("`bright`\n\rThis question allows multiple answers, Toggle your answers and press [D]one");
	   else
	     od_printf("`bright black`\n\rThis question allows multiple answers`white`, `bright black`Toggle your answers and press `white`[`bright black`D`white`]`bright black`one");
	 }

	 /* Display prompt to user. */
	 if(colorsch==COLOR_DEF)
	   od_printf("`bright`");
	 else
	   od_printf("`bright black");

	 od_printf("\n\rEnter answer number");
	 if((QuestionRecord.bitflags & CAN_ADD_ANSWERS) && ((CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[15])==FALSE || QuestionRecord.bitflags & MULTIPLE_ADDS_P_U)) {
	   if(colorsch==COLOR_DEF)
	     od_printf(", [A]dd your own response");
	   else
	     od_printf("`white`, [`bright black`A`white`]`bright black`dd your own response");
	 }
	 if(bAllowUnvote==TRUE) {
	   if(colorsch==COLOR_DEF)
	     od_printf(", [M]ark as unanswered");
	   else
	     od_printf("`white`, [`bright black`M`white`]`bright black`ark as unanswered");
	 }
	 if(colorsch==COLOR_DEF)
	   od_printf(", [Q]uit: `green`");
	 else
	   od_printf("`white`, [`bright black`Q`white`]`bright black`uit`white`: `bright`");

	 /* Get response from user. */
	 od_input_str(szUserInput, 2, ' ', 255);
	 /* Add a blank line. */
	 od_printf("\n\r");

	 /* If user entered Q, return to main menu. */
	 if (szUserInput[0]=='Q' || szUserInput[0]=='q')
	 {
	   return;
	 }

	 /* If user enetered A, and adding answers is premitted ... */
	 if ((szUserInput[0]=='A' || szUserInput[0]=='a')
	    && (QuestionRecord.bitflags & CAN_ADD_ANSWERS)
	    && ((CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[15])==FALSE || QuestionRecord.bitflags & MULTIPLE_ADDS_P_U))
	 {
	    /* ... Prompt for answer from user. */
	    if(colorsch==COLOR_DEF) {
	      od_printf("`bright green`Please enter your new answer:\n\r");
	      od_printf("`green`[------------------------------]\n\r ");
	    } else {
	      od_printf("`bright black`Please enter your new answer`white`:\n\r");
	      od_printf("[------------------------------]\n\r ");
	    }


	    if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	      /* Get string from user. */
	      od_input_str(szNewAnswer,
		ANSWER_STR_SIZE - 1, ' ', 255);
	    } else {
	      warp_line(szNewAnswer,szNewAnswer2," ");
	    }

	    /* Get string from user. */
/*          od_input_str(szNewAnswer, ANSWER_STR_SIZE - 1, ' ', 255);

	    if(!(QuestionRecord.bitflags & BF_15_BY_1_MODE)) {
	      od_printf(" ");
	      od_input_str(szNewAnswer2, ANSWER_STR_SIZE - 1, ' ', 255);
	    }*/

	    /* Record that user entered a new answer answer. */
	    nAnswer = NEW_ANSWER;

	    /* If user entered a valid answer, then exit loop. */
	    if(strlen(szNewAnswer) > 0)
	    {
	       break;
	    }
	 }

	 if((QuestionRecord.bitflags & MULTIPLE_ANSWERS) && (CurrentUserRecord.bVotedOnQuestion[nQuestion]!=answers[15] && CurrentUserRecord.bVotedOnQuestion[nQuestion]!=0)) {
	   if (szUserInput[0]=='D' || szUserInput[0]=='d')
	   {
	     goto donewith;
	   }
	 }

	 /* Otherwise, attempt to get answer number from user. */
	 nAnswer = atoi(szUserInput) - 1;

	 if ((szUserInput[0]=='M' || szUserInput[0]=='m') && bAllowUnvote==TRUE)
	 {
	   if(colorsch==COLOR_DEF)
	     od_printf("`bright green`Clear your answer to this question and set it as unanswered? [Y] or [N] `green`");
	   else
	     od_printf("`bright black`Clear your answer to this question and set it as unanswered`white`? [`bright black`Y`white`] `bright black`or `white`[`bright black`N`white`] `bright`");
	   if(od_get_answer("YN")=='Y') {
	     od_printf("Yes\n\r\n");
	     nAnswer=SET_UN_ANSWERED;
	     break;
	   } else {
	     od_printf("No\n\r\n");
	   }
	 }
	 else if((QuestionRecord.bitflags & MULTIPLE_ANSWERS)
	  && (CurrentUserRecord.bVotedOnQuestion[nQuestion]==0 || CurrentUserRecord.bVotedOnQuestion[nQuestion]==answers[15])
	  && (szUserInput[0]=='D' || szUserInput[0]=='d')) {
/*       if(answers[nAnswer]==CurrentUserRecord.bVotedOnQuestion[nQuestion] || CurrentUserRecord.bVotedOnQuestion[nQuestion]==answers[15]+answers[nAnswer]) {*/
	   if(colorsch!=COLOR_DEF)
	     od_printf("`bright black`");
	   od_printf("You need to have at least one answer checked");
	   if(colorsch!=COLOR_DEF)
	     od_printf("`white`");
	   od_printf("!\n\r\n");
	   WaitForEnter();
	 /* If user input is not a valid answer. */
	 } else if(nAnswer < 0 || nAnswer >= QuestionRecord.nTotalAnswers) {
	   /* Display message. */
	   if(colorsch!=COLOR_DEF)
	     od_printf("`bright black`");
	   od_printf("That is not a valid response");
	   if(colorsch!=COLOR_DEF)
	     od_printf("`white`");
	   od_printf(".\n\r\n");

	   WaitForEnter();
	 }
	 else
	 {
	    /* Otherwise, exit loop. */
	    break;
	 }
      }

    /* Add user's vote to question. */

      /* If user entered their own answer, try to add it to the question. */
      if(nAnswer == NEW_ANSWER)
      {
	 /* Check that there is still room for another answer. */
	 if(QuestionRecord.bitflags & BF_15_BY_1_MODE)
	   cnt=MAX_ANSWERS;
	 else
	   cnt=MAX_ANSWERS2;

	 if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
	   return;

	 if(QuestionRecord.nTotalAnswers >= cnt)
	 {
	    fclose(fpFile);
	    if(colorsch!=COLOR_DEF)
	      od_printf("`bright black`");
	    od_printf("Sorry, this question already has the maximum number of answers");
	    if(colorsch!=COLOR_DEF)
	      od_printf("`white`");
	    od_printf(".\n\r");

	    WaitForEnter();
	    goto moreanswers;
	 }

	 /* Set answer number to number of new answer. */
	 nAnswer = QuestionRecord.nTotalAnswers;

	 /* Add 1 to total number of answers. */
	 ++QuestionRecord.nTotalAnswers;

	 /* Initialize new answer string and count. */
	 if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	   strcpy(QuestionRecord.aszAnswer[nAnswer], szNewAnswer);
	 } else {
	   strcpy(QuestionRecord.aszAnswer[nAnswer*2], szNewAnswer);
	   strcpy(QuestionRecord.aszAnswer[(nAnswer*2)+1], szNewAnswer2);
	 }
	 QuestionRecord.auVotesForAnswer[nAnswer] = 0;

	 if(!QWrite(&QuestionRecord,fpFile,nQuestion))
	   return;

	 CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[15];
      }

      if(nAnswer == SET_UN_ANSWERED)
      {
/*      for(cnt=0;cnt<QuestionRecord.nTotalAnswers;cnt++) {
	  if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[cnt]) {
	    --QuestionRecord.auVotesForAnswer[cnt];
	    CurrentUserRecord.bVotedOnQuestion[nQuestion] &=~ answers[cnt];
	  }
	}*/

	if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[15]) {
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] = 0;
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[15];
	} else {
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] = 0;
	}
	decvotes=TRUE;
//      --QuestionRecord.uTotalVotes;
	goto donewith;
      }// else {

      if(QuestionRecord.bitflags & MULTIPLE_ANSWERS) {
	/* Record that user has voted on this question. */
	if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[nAnswer])
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] &=~ answers[nAnswer];
	else
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[nAnswer];
      } else {
	if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[15]) {
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] = answers[nAnswer];
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[15];
	} else {
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] = answers[nAnswer];
	}
      }



      /*if(QuestionRecord.bitflags & MULTIPLE_ANSWERS) {*/
	/* Add user's vote to question. */
/*      if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[nAnswer]) {
	  --QuestionRecord.auVotesForAnswer[nAnswer];
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] &=~ answers[nAnswer];
	} else {
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[nAnswer];
	  ++QuestionRecord.auVotesForAnswer[nAnswer];
	}
      } else {
	if(!(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[nAnswer])) {
	  for(cnt=0;cnt<QuestionRecord.nTotalAnswers;cnt++) {
	    if(CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[cnt]) {
	      --QuestionRecord.auVotesForAnswer[cnt];
	      CurrentUserRecord.bVotedOnQuestion[nQuestion] &=~ answers[cnt];
	      break;
	    }
	  }
	  ++QuestionRecord.auVotesForAnswer[nAnswer];
	  CurrentUserRecord.bVotedOnQuestion[nQuestion] |= answers[nAnswer];
	}
      }*/

 //   }

    if((QuestionRecord.bitflags & MULTIPLE_ANSWERS) && nAnswer != SET_UN_ANSWERED)
      goto moreanswers;

    donewith:

    if((fpFile=QRead(&QuestionRecord,nQuestion))==NULL)
      return;

    if(decvotes==TRUE) {
      --QuestionRecord.uTotalVotes;
      decvotes=3;
    }

      for(x=0;x<15;x++) {
	if((CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[x]) && ((original_answers & answers[x])==FALSE))
	  ++QuestionRecord.auVotesForAnswer[x];
	else if(((CurrentUserRecord.bVotedOnQuestion[nQuestion] & answers[x])==FALSE) && (original_answers & answers[x]))
	  --QuestionRecord.auVotesForAnswer[x];
      }

    if(!QWrite(&QuestionRecord,fpFile,nQuestion))
      return;

    WriteCurrentUser();

    if(decvotes!=3)
    /* Display the result of voting on this question to the user. */
      DisplayQuestionResult(&QuestionRecord,FALSE);

    decvotes=FALSE;

    /* Add 1 to count of questions that the user has voted on. */
    nQuestionsVotedOn++;
  }
}





/* The ViewResults function is called when the user chooses the "view    */
/* results" command from the main menu. This function alows the user to  */
/* choose a question from the list of questions, and then displays the   */
/* results of voting on that question.                                   */
void ViewResults(int all)
{
   int nChoice=-1;
   tQuestionRecord QuestionRecord;
   int nPageLocation = 0;


   /* Loop until user chooses to return to main menu. */
   for(;;)
   {
      /* Allow the user to choose a question from the list of questions that */
      /* they have already voted on.                                         */
      if (all==FALSE) {
	if(view_own_results==FALSE) {
	  nChoice = ChooseQuestion(nViewResultsFrom,
	   "                                 View Results\n\r", &nPageLocation);
	} else {
	  nChoice = ChooseQuestion(nViewResultsFrom | CURRENT_USER_ONLY,
	   "                                 View Results\n\r", &nPageLocation);
	}
      } else {
	if(view_own_results==FALSE) {
	  nChoice = FirstQuestion(nViewResultsFrom,nChoice+1);
	} else {
	  nChoice = FirstQuestion(nViewResultsFrom | CURRENT_USER_ONLY,nChoice+1);
	}
      }


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
      if (DisplayQuestionResult(&QuestionRecord,all)==FALSE) return;

   }
}


/* The GetQuestion function read the record for the specified question */
/* number from the question file.                                      */
int GetQuestion(int nQuestion, tQuestionRecord *pQuestionRecord)
{
   FILE *fpQuestionFile;


   if((fpQuestionFile=QRead(pQuestionRecord,nQuestion))==NULL)
     return(FALSE);

   /* Close the question file to allow access by other nodes. */
   fclose(fpQuestionFile);

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
   char szLogMessage[100];
   int cnt,cnt2;

   QuestionRecord.bitflags=0;

   /* Clear the screen. */
//   od_printf("\n\n\r");
   my_clr_scr();

   /* Display screen header. */
   if(colorsch==COLOR_DEF)
     od_printf("`bright red`");
   else
     od_printf("`bright`");
   od_printf("                                Add A Question\n\r");
   if(colorsch==COLOR_DEF)
     od_printf("`red`");
   else
     od_printf("`bright black`");

   if(od_control.user_ansi || od_control.user_avatar)
   {
      od_repeat((unsigned char)196, 79);
   }
   else
   {
      od_repeat('-', 79);
   }

   od_printf("\n\r\n\r");

   if(multiple_answers==TRUE) {
     if(colorsch==COLOR_DEF) {
       od_printf("`bright green`Can people choose more than one answer? If not they will be allowed to only\r\n");
       od_printf("choose one of the answers. (Y/N)`green`");
     } else {
       od_printf("`bright black`Can people choose more than one answer`white`? `bright black`If not they will be allowed to only\r\n");
       od_printf("choose one of the answers`white`. (`bright black`Y`white`/`bright black`N`white`)");
     }

     if(od_get_answer("YN")=='Y'){
       od_printf(" Yes\n\r\n\r");
       QuestionRecord.bitflags |= MULTIPLE_ANSWERS;
     } else {
       od_printf(" No\n\r\n\r");
       QuestionRecord.bitflags &=~ MULTIPLE_ANSWERS;
     }
   }
   if(colorsch==COLOR_DEF) {
     od_printf("`bright green`Should the answers to the question be:\n\r");
     od_printf("                       1) 15 answers, 1 line per answer or,\n\r");
     od_printf("                       2) 7 answers, 2 lines per answer\n\r");
     od_printf("Well:`green`");
   } else {
     od_printf("`bright black`Should the answers to the question be`white`:\n\r");
     od_printf("                       `bright`1`white`) `bright`15 `bright black`answers`white`, `bright`1 `bright black`line per answer or`white`,\n\r");
     od_printf("                       `bright`2`white`) `bright`7 `bright black`answers`white`, `bright`2 `bright black`lines per answer\n\r");
     od_printf("`bright black`Well`white`:");
   }

   if(od_get_answer("12")=='1'){
     od_printf(" 15 by 1\n\r\n\r");
     QuestionRecord.bitflags |= BF_15_BY_1_MODE;
   } else {
     od_printf(" 7 by 2\n\r\n\r");
     QuestionRecord.bitflags &=~ BF_15_BY_1_MODE;
   }

   if(anonymous_posting==TRUE) {
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`Do You want to post the question as Anonymous? (Y/N)`green`");
     else
       od_printf("`bright black`Do You want to post the question as Anonymous`white`? (`bright black`Y`white`/`bright black`N`white`)");

     if(od_get_answer("YN")=='Y'){
       od_printf(" Yes\n\r\n\r");
       QuestionRecord.bitflags |= ANONYMOUS_QUESTION;
     } else {
       od_printf(" No\n\r\n\r");
       QuestionRecord.bitflags &=~ ANONYMOUS_QUESTION;
     }
   }


   if(sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0) {
     if(colorsch==COLOR_DEF)
       od_printf("`bright green`Do you want your question to never be deleted (by timed deletion)? (Y/N)`green`");
     else
       od_printf("`bright black`Do you want your question to never be deleted `white`(`bright black`by timed deletion`white`)? (`bright black`Y`white`/`bright black`N`white`)");

     if(od_get_answer("YN")=='Y'){
       od_printf(" Yes\n\r\n\r");
       QuestionRecord.bitflags |= NEVER_DELETE_QUESTION;
     } else {
       od_printf(" No\n\r\n\r");
       QuestionRecord.bitflags &=~ NEVER_DELETE_QUESTION;
     }
     if(colorsch==COLOR_DEF) {
       od_printf("`bright green`Do you want your question to be forced? So that everybody who enters the door\n\r");
       od_printf("will have to vote on it? (Y/N)`green`");
     } else {
       od_printf("`bright black`Do you want your question to be forced`white`? `bright black`So that everybody who enters the door\n\r");
       od_printf("will have to vote on it`white`? (`bright black`Y`white`/`bright black`N`white`)");
     }

     if(od_get_answer("YN")=='Y'){
       od_printf(" Yes\n\r\n\r");
       QuestionRecord.bitflags |= FORCED_QUESTION;
     } else {
       od_printf(" No\n\r\n\r");
       QuestionRecord.bitflags &=~ FORCED_QUESTION;
     }

   }

   /* Clear the screen. */
//   od_printf("\n\n\r");
   my_clr_scr();

   /* Display screen header. */
   if(colorsch==COLOR_DEF)
     od_printf("`bright red`");
   else
     od_printf("`bright`");
   od_printf("                                Add A Question\n\r");
   if(colorsch==COLOR_DEF)
     od_printf("`red`");
   else
     od_printf("`bright black`");

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
   if(colorsch==COLOR_DEF) {
     od_printf("`bright green`Enter Your Question (blank line cancels)\n\r");
     od_printf("`green`[----------------------------------------------------------------------]\n\r ");
   } else {
     od_printf("`bright black`Enter Your Question `white`(`bright black`blank line cancels`white`)\n\r");
     od_printf("[----------------------------------------------------------------------]\n\r ");
   }
   od_input_str(QuestionRecord.szQuestion, QUESTION_STR_SIZE - 1, ' ', 255);

   /* If question was empty, then return to main menu. */
   if(strlen(QuestionRecord.szQuestion) == 0)
   {
      return;
   }

   /* Display prompt for answers. */
   if(colorsch==COLOR_DEF) {
     od_printf("\n\r`bright green`Enter Possible Answers (blank line when done)\n\r");
     od_printf("`green`   [------------------------------]\n\r");
   } else {
     od_printf("\n\r`bright black`Enter Possible Answers `white`(`bright black`blank line when done`white`)\n\r");
     od_printf("   [------------------------------]\n\r");
   }

   /* Loop, getting answers from user. */
   if(QuestionRecord.bitflags & BF_15_BY_1_MODE)
     cnt2=MAX_ANSWERS;
   else
     cnt2=MAX_ANSWERS2;

   for(QuestionRecord.nTotalAnswers = cnt = 0;
       QuestionRecord.nTotalAnswers < cnt2;
       QuestionRecord.nTotalAnswers++)
   {
      askansweragain:

      /* Display prompt with answer number. */
      if(colorsch==COLOR_DEF)
	od_printf("`bright green`%2d: `green`", QuestionRecord.nTotalAnswers + 1);
      else
	od_printf("`bright`%2d`white`: `white`", QuestionRecord.nTotalAnswers + 1);

      if(QuestionRecord.bitflags & BF_15_BY_1_MODE) {
	/* Get string from user. */
	od_input_str(QuestionRecord.aszAnswer[cnt],
	  ANSWER_STR_SIZE - 1, ' ', 255);
      } else {
	if(colorsch==COLOR_DEF)
	  warp_line(QuestionRecord.aszAnswer[cnt],QuestionRecord.aszAnswer[cnt+1],"`bright green`  : `green`");
	else
	  warp_line(QuestionRecord.aszAnswer[cnt],QuestionRecord.aszAnswer[cnt+1],"`white`  : ");
      }

     /* if(!(QuestionRecord.bitflags & BF_15_BY_1_MODE) && strlen(QuestionRecord.aszAnswer[cnt])>0) {
	od_printf("`bright green`  : `green`");*/

	/* Get string from user. */
      /*        od_input_str(QuestionRecord.aszAnswer[cnt+1], ANSWER_STR_SIZE - 1, ' ', 255);
      } */


      /* If string was empty, but only one answer added then */
      if(strlen(QuestionRecord.aszAnswer[cnt]) == 0 && QuestionRecord.nTotalAnswers <2) {
	if(colorsch==COLOR_DEF) {
	  od_printf("\n\r`bright red`Question has to have at least 2 answers!\n\r");
	  od_printf("`green`   [------------------------------]\n\r");
	} else {
	  od_printf("\n\r`bright black`Question has to have at least `bright`2 `bright black`answers`white`!\n\r");
	  od_printf("   [------------------------------]\n\r");
	}
	goto askansweragain;
      }

      /* If string was empty, then exit loop. */
      if(strlen(QuestionRecord.aszAnswer[cnt]) == 0)
      {
	 break;
      }

      /* Reset count of votes for this answer to zero. */
      QuestionRecord.auVotesForAnswer[QuestionRecord.nTotalAnswers] = 0;

      if(!(QuestionRecord.bitflags & BF_15_BY_1_MODE))
	cnt+=2;
      else
	cnt++;
   }

   /* If no answers were supplied, then cancel, returning to main menu. */
/*   if(QuestionRecord.nTotalAnswers == 0)
   {
      return;
   }*/

   QuestionRecord.bitflags &=~ CAN_ADD_ANSWERS;
   QuestionRecord.bitflags &=~ MULTIPLE_ADDS_P_U;


   if(answer_adding==1 && QuestionRecord.nTotalAnswers < cnt2) {

     /* Ask whether users should be able to add their own answers. */
     if(colorsch==COLOR_DEF)
       od_printf("\n\r`bright green`Should voters be able to add their own options? (Y/N) `green`");
     else
       od_printf("\n\r`bright black`Should voters be able to add their own options`white`? (`bright black`Y`white`/`bright black`N`white`) ");

     /* Get answer from user. */
     if(od_get_answer("YN") == 'Y') {
       /* If user pressed the 'Y' key. */
       od_printf("Yes\n\r");

       /* Record user's response. */
       QuestionRecord.bitflags |= CAN_ADD_ANSWERS;
       if((sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0) || more_adds_p_u==TRUE) {
	 if(colorsch==COLOR_DEF)
	   od_printf("\n\r`bright green`Should one voter be able to add more than one answer? (Y/N) `green`");
	 else
	   od_printf("\n\r`bright black`Should one voter be able to add more than one answer`white`? (`bright black`Y`white`/`bright black`N`white`) ");

	 if(od_get_answer("YN") == 'Y') {
	   /* If user pressed the 'Y' key. */
	   od_printf("Yes\n\r");

	   /* Record user's response. */
	   QuestionRecord.bitflags |= MULTIPLE_ADDS_P_U;

	 } else {
	   /* If user pressed the 'N' key. */
	   od_printf("No\n\r");

	   /* Record user's response. */
	   QuestionRecord.bitflags &=~ MULTIPLE_ADDS_P_U;
	 }
       } else {
	 QuestionRecord.bitflags &=~ MULTIPLE_ADDS_P_U;
       }
     } else {
       /* If user pressed the 'N' key. */
       od_printf("No\n\r");

       /* Record user's response. */
       QuestionRecord.bitflags &=~ CAN_ADD_ANSWERS;
       QuestionRecord.bitflags &=~ MULTIPLE_ADDS_P_U;
     }
   } else {
     QuestionRecord.bitflags &=~ CAN_ADD_ANSWERS;
     QuestionRecord.bitflags &=~ MULTIPLE_ADDS_P_U;
   }

   if(answer_adding==2 && QuestionRecord.nTotalAnswers < cnt2)
     QuestionRecord.bitflags |= CAN_ADD_ANSWERS;

   if(more_adds_p_u==2)
     QuestionRecord.bitflags |= MULTIPLE_ADDS_P_U;

   /* Confirm save of new question. */
   if(colorsch==COLOR_DEF)
     od_printf("\n\r`bright green`Do you wish to save this new question? (Y/N) `green`");
   else
     od_printf("\n\r`bright black`Do you wish to save this new question`white`? (`bright black`Y`white`/`bright black`N`white`) ");



   /* If user does not want to save the question, return to main menu now. */
   if(od_get_answer("YN") == 'N')
   {
      return;
   }

   /* Set total number of votes for this question to 0. */
   QuestionRecord.uTotalVotes = 0;

   /* Set creator name and creation time for this question. */
   strcpy(QuestionRecord.szCreatorName, sz_user_name);
   QuestionRecord.lCreationTime = time(NULL);
   QuestionRecord.bitflags &=~ QUESTION_DELETED;

   /* Open question file for exclusive access by this node. */
   fpQuestionFile = ExculsiveFileOpen(QUESTION_FILENAME, "a+b");
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
   if(ftell(fpQuestionFile) / QUESTIONRECORD_SIZE >= MAX_QUESTIONS)
   {
      fclose(fpQuestionFile);
      od_printf("`bright`Cannot add another question, FrEevOtE is limited to %d questions.\n\r", MAX_QUESTIONS);
      WaitForEnter();
      return;
   }

   /* Add new question to file. */
   if(fwriteQuestionRecord(&QuestionRecord, fpQuestionFile) != 1)
   {
      fclose(fpQuestionFile);
      od_printf("Unable to write to question file.\n\r");
      WaitForEnter();
      return;
   }

   /* Close question file, allowing other nodes to access file. */
   fclose(fpQuestionFile);

   /* Record in the logfile that user has added a new question. */
   sprintf(szLogMessage, "User adding questions: %s",
      QuestionRecord.szQuestion);
   od_log_write(szLogMessage);
}


/* The DeleteQuestion() function is called when the sysop chooses the   */
/* "delete question" option from the main menu. This function displays  */
/* a list of all questions, allowing the sysop to choose a question for */
/* deletion.                                                            */
void DeleteQuestion(void)
{
   int nQuestion;
   tQuestionRecord QuestionRecord;
   FILE *fpFile;
   int nPageLocation = 0;

   deletemore:

   /* Check that user is system operator. */
   if(sysop_sec>od_control.user_security && strcmp(od_control.user_name, od_control.sysop_name) != 0)
   {
      if(bAllowDelete==TRUE) {
	DeleteYourQuestion();
      }
      return;
   }

   /* Allow the user to choose a question from the list of all questions. */
   nQuestion = ChooseQuestion(QUESTIONS_NOT_VOTED_ON | QUESTIONS_VOTED_ON,
      "                              Delete A Question\n\r", &nPageLocation);

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

   /* Confirm deletion of this question. */
   if(colorsch==COLOR_DEF) {
     od_printf("\n\r\n`bright green`Are you sure you want to delete the question:\n\r   `green`%s\n\r",
      QuestionRecord.szQuestion);
     od_printf("`bright green`[Y]es or [N]o? `green`");
   } else {
     od_printf("\n\r\n`bright black`Are you sure you want to delete the question`white`:\n\r   `bright`%s\n\r",
      QuestionRecord.szQuestion);
     od_printf("`white`[`bright black`Y`white`]`bright black`es or `white`[`bright black`N`white`]`bright black`o`white`? ");
   }

   /* If user canceled deletion, return now. */
   if(od_get_answer("YN") == 'N')
   {
      goto deletemore;
   }

   od_printf("Y\n\r");

/*   if(((int)((nQuestion+1)/QUESTION_PAGE_SIZE)*QUESTION_PAGE_SIZE)==(nQuestion+1))
     nPageLocation-=QUESTION_PAGE_SIZE;*/


   /* Mark the question as being deleted. */
   QuestionRecord.bitflags |= QUESTION_DELETED;

   /* Open question file for exclusive access by this node. */
   fpFile = ExculsiveFileOpen(QUESTION_FILENAME, "r+b");
   if(fpFile == NULL)
   {
      /* If unable to access file, display error and return. */
      od_printf("\n\n\rUnable to access the question file.\n\n\r");
      WaitForEnter();
      return;
   }

   /* Write the question record back to the file. */
   fseek(fpFile, (long)nQuestion * QUESTIONRECORD_SIZE, SEEK_SET);
   if(fwriteQuestionRecord(&QuestionRecord, fpFile) != 1)
   {
      /* If unable to access file, display error and return. */
      fclose(fpFile);
      od_printf("\n\rUnable to write question to file.\n\r");
      WaitForEnter();
      return;
   }

   /* Close the question file to allow access by other nodes. */
   fclose(fpFile);


   goto deletemore;

}


void DeleteYourQuestion(void)
{
   int nQuestion;
   tQuestionRecord QuestionRecord;
   FILE *fpFile;
   int nPageLocation = 0;

   deletemore:

   /* Allow the user to choose a question from the list of all questions. */
   nQuestion = ChooseQuestion(CURRENT_USER_ONLY,
      "                              Delete A Question\n\r", &nPageLocation);

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

   /* Confirm deletion of this question. */
   if(colorsch==COLOR_DEF) {
     od_printf("\n\r\n`bright green`Are you sure you want to delete the question:\n\r   `green`%s\n\r",
      QuestionRecord.szQuestion);
     od_printf("`bright green`[Y]es or [N]o? `green`");
   } else {
     od_printf("\n\r\n`bright black`Are you sure you want to delete the question`white`:\n\r   `bright`%s\n\r",
      QuestionRecord.szQuestion);
     od_printf("`white`[`bright black`Y`white`]`bright black`es or `white`[`bright black`N`white`]`bright black`o`white`? ");
   }

   /* If user canceled deletion, return now. */
   if(od_get_answer("YN") == 'N')
   {
      goto deletemore;
   }

   od_printf("Yes\n\r");

/*   if(((int)((nQuestion+1)/QUESTION_PAGE_SIZE)*QUESTION_PAGE_SIZE)==(nQuestion+1))
     nPageLocation-=QUESTION_PAGE_SIZE;*/


   /* Mark the question as being deleted. */
   QuestionRecord.bitflags |= QUESTION_DELETED;

   /* Open question file for exclusive access by this node. */
   fpFile = ExculsiveFileOpen(QUESTION_FILENAME, "r+b");
   if(fpFile == NULL)
   {
      /* If unable to access file, display error and return. */
      od_printf("\n\n\rUnable to access the question file.\n\n\r");
      WaitForEnter();
      return;
   }

   /* Write the question record back to the file. */
   fseek(fpFile, (long)nQuestion * QUESTIONRECORD_SIZE, SEEK_SET);
   if(fwriteQuestionRecord(&QuestionRecord, fpFile) != 1)
   {
      /* If unable to access file, display error and return. */
      fclose(fpFile);
      od_printf("\n\rUnable to write question to file.\n\r");
      WaitForEnter();
      return;
   }

   /* Close the question file to allow access by other nodes. */
   fclose(fpFile);


   goto deletemore;

}

int CountQuestions()
{
   int QuestionCount,nFileQuestion;
   unsigned int bVotedOnQuestion;
   tQuestionRecord QuestionRecord;
   FILE *fpQuestionFile;

   /* Attempt to open question file. */
   fpQuestionFile = ExculsiveFileOpen(QUESTION_FILENAME, "r+b");

   /* If unable to open question file, assume that no questions have been */
   /* created.                                                            */
   if(fpQuestionFile == NULL)
   {
      /* Display "no questions yet" message. */
      if(colorsch==COLOR_DEF)
	od_printf("\n\r`bright cyan`No questions have been created so far.\n\r");
      else
	od_printf("\n\r`bright black`No questions have been created so far`white`.\n\r");

      return(0);
   }

   QuestionCount=0;
   nFileQuestion=0;
   /* Loop for every question record in the file. */
   while(freadQuestionRecord(&QuestionRecord, fpQuestionFile) == 1)
   {
      /* Determine whether or not the user has voted on this question. */
      bVotedOnQuestion = CurrentUserRecord.bVotedOnQuestion[nFileQuestion];

      /* If this is the kind of question that the user is choosing from */
      /* right now.                                                     */
      if(bVotedOnQuestion==answers[15] || bVotedOnQuestion==0)
      {
	 /* If question is not deleted. */
	 if(!(QuestionRecord.bitflags & QUESTION_DELETED))
	 {
	    QuestionCount++;

	 }
      }
      nFileQuestion++;
   }

   /* Close question file to allow other nodes to access the file. */
   fclose(fpQuestionFile);
   if(colorsch==COLOR_DEF) {
     if(QuestionCount==1)
       od_printf("\n\r`bright cyan`There is 1 new question!\n\r");
     else if(QuestionCount>1)
       od_printf("\n\r`bright cyan`There are %d new questions!\n\r",QuestionCount);
     else
       od_printf("\n\r`bright cyan`There are no new questions!\n\r",QuestionCount);
   } else {
     if(QuestionCount==1)
       od_printf("\n\r`bright black`There is `bright`1 `bright black`new question`white`!\n\r");
     else if(QuestionCount>1)
       od_printf("\n\r`bright black`There are `bright`%d `bright black`new questions`white`!\n\r",QuestionCount);
     else
       od_printf("\n\r`bright black`There are no new questions`white`!\n\r",QuestionCount);
   }

   return(QuestionCount);
}

int CountQuestionsF()
{
   int QuestionCount,nFileQuestion;
   unsigned int bVotedOnQuestion;
   tQuestionRecord QuestionRecord;
   FILE *fpQuestionFile;

   /* Attempt to open question file. */
   fpQuestionFile = ExculsiveFileOpen(QUESTION_FILENAME, "r+b");

   /* If unable to open question file, assume that no questions have been */
   /* created.                                                            */
   if(fpQuestionFile == NULL)
   {
      /* Display "no questions yet" message. */
   //   od_printf("\n\r`bright cyan`No questions have been created so far.\n\r");

      return(0);
   }

   QuestionCount=0;
   nFileQuestion=0;
   /* Loop for every question record in the file. */
   while(freadQuestionRecord(&QuestionRecord, fpQuestionFile) == 1)
   {
      /* Determine whether or not the user has voted on this question. */
      bVotedOnQuestion = CurrentUserRecord.bVotedOnQuestion[nFileQuestion];

      /* If this is the kind of question that the user is choosing from */
      /* right now.                                                     */
      if(bVotedOnQuestion==answers[15] || bVotedOnQuestion==0)
      {
	 /* If question is not deleted. */
	 if(!(QuestionRecord.bitflags & QUESTION_DELETED) && (QuestionRecord.bitflags & FORCED_QUESTION))
	 {
	    QuestionCount++;
	 }
      }
      nFileQuestion++;
   }

   /* Close question file to allow other nodes to access the file. */
   fclose(fpQuestionFile);

/*   if(QuestionCount==1)
     od_printf("\n\r`bright cyan`There is 1 new question!\n\r");
   else if(QuestionCount>1)
     od_printf("\n\r`bright cyan`There are %d new questions!\n\r",QuestionCount);
   else
     od_printf("\n\r`bright cyan`There are no new questions!\n\r",QuestionCount);*/

   return(QuestionCount);
}



/* The ChooseQuestion() function provides a list of questions and allows   */
/* the user to choose a particular question, cancel back to the main menu, */
/* and page up and down in the list of questions. Depending upon the value */
/* of the nFromWhichQuestions parameter, this function will present a list */
/* of questions that the user has voted on, a list of questions that the   */
/* user has not voted on, or a list of all questions.                      */
int ChooseQuestion(unsigned int nFromWhichQuestions, const char *pszTitle, int *nLocation)
{
   int nCurrent;
   int nFileQuestion;
   int nPagedToQuestion = *nLocation;
   int nDisplayedQuestion;
   unsigned int bVotedOnQuestion;
   char chCurrent;
   tQuestionRecord QuestionRecord;
   FILE *fpQuestionFile;
   static char szQuestionName[MAX_QUESTIONS][QUESTION_STR_SIZE];
   static int nQuestionNumber[MAX_QUESTIONS];

   nFileQuestion = 0;
   nDisplayedQuestion = 0;

   /* Attempt to open question file. */
   fpQuestionFile = ExculsiveFileOpen(QUESTION_FILENAME, "r+b");

   /* If unable to open question file, assume that no questions have been */
   /* created.                                                            */
   if(fpQuestionFile == NULL)
   {
      /* Display "no questions yet" message. */
      if(colorsch!=COLOR_DEF)
	od_printf("`bright black`");
      od_printf("\n\rNo questions have been created so far");
      if(colorsch!=COLOR_DEF)
	od_printf("`white`");
      od_printf(".\n\n\r");


      /* Wait for user to press enter. */
      WaitForEnter();

      /* Indicate that no question has been chosen. */
      return(NO_QUESTION);
   }

   /* Loop for every question record in the file. */
   while(freadQuestionRecord(&QuestionRecord, fpQuestionFile) == 1)
   {
      /* Determine whether or not the user has voted on this question. */
      bVotedOnQuestion = CurrentUserRecord.bVotedOnQuestion[nFileQuestion];

      /* If this is the kind of question that the user is choosing from */
      /* right now.                                                     */
      if(((bVotedOnQuestion!=answers[15] && bVotedOnQuestion!=0) && (nFromWhichQuestions & QUESTIONS_VOTED_ON)) ||
	 ((bVotedOnQuestion==answers[15] || bVotedOnQuestion==0) && (nFromWhichQuestions & QUESTIONS_NOT_VOTED_ON)) ||
	 (strcmp(sz_user_name,QuestionRecord.szCreatorName)==0 &&
	 (nFromWhichQuestions & CURRENT_USER_ONLY)))
      {
	 /* If question is not deleted. */
	 if(!(QuestionRecord.bitflags & QUESTION_DELETED))
	 {
	    /* Add this question to list to be displayed. */
	    strcpy(szQuestionName[nDisplayedQuestion],
	       QuestionRecord.szQuestion);
	    nQuestionNumber[nDisplayedQuestion] = nFileQuestion;

	    /* Add one to number of questions to be displayed in list. */
	    nDisplayedQuestion++;
	 }
      }

      /* Move to next question in file. */
      ++nFileQuestion;
   }

   /* Close question file to allow other nodes to access the file. */
   fclose(fpQuestionFile);

   /* If there are no questions for the user to choose, display an */
   /* appropriate message and return. */
   if(nDisplayedQuestion == 0)
   {
      /* If we were to list all questions. */
      if((nFromWhichQuestions & QUESTIONS_VOTED_ON)
	 && (nFromWhichQuestions & QUESTIONS_NOT_VOTED_ON))
      {
      if(colorsch!=COLOR_DEF)
	od_printf("`bright black`");
      od_printf("\n\rThere are no questions");
      if(colorsch!=COLOR_DEF)
	od_printf("`white`");
      od_printf(".\n\n\r");
      }
      /* If we were to list questions that the user has voted on. */
      else if(nFromWhichQuestions & QUESTIONS_VOTED_ON)
      {
	if(colorsch!=COLOR_DEF)
	  od_printf("`bright black`");
	od_printf("\n\rThere are no questions that you have voted on");
	if(colorsch!=COLOR_DEF)
	  od_printf("`white`");
	od_printf(".\n\n\r");
      }
      else if(nFromWhichQuestions & CURRENT_USER_ONLY)
      {
	 if(colorsch!=COLOR_DEF)
	   od_printf("`bright black`");
	 od_printf("\n\rYou have not created any questions");
	 if(colorsch!=COLOR_DEF)
	   od_printf("`white`");
	 od_printf(".\n\n\r");

      }

      /* Otherwise, we were to list questions that use has not voted on. */
      else
      {
	 if(colorsch!=COLOR_DEF)
	   od_printf("`bright black`");
	 od_printf("\n\rYou have voted on all the questions");
	 if(colorsch!=COLOR_DEF)
	   od_printf("`white`");
	 od_printf(".\n\n\r");
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
//      od_printf("\n\n\r");
      my_clr_scr();

      /* Display header. */
      if(colorsch==COLOR_DEF)
	od_printf("`bright red`");
      else
	od_printf("`bright`");
      od_printf(pszTitle);
      if(colorsch==COLOR_DEF)
	od_printf("`red`");
      else
	od_printf("`white`");

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
	 if(colorsch==COLOR_DEF)
	   od_printf("`bright green`");
	 else
	   od_printf("`bright`");

	 od_printf("%c.", chCurrent);
	 if(colorsch==COLOR_DEF)
	   od_printf("`green`");
	 else
	   od_printf("`bright black`");
	 od_printf(" %s\n\r", szQuestionName[nCurrent + nPagedToQuestion]);
      }

      /* Display prompt for input. */
      if(colorsch==COLOR_DEF)
	od_printf("\n\r`bright`[Page %d]  Choose a question or",
	 (nPagedToQuestion / QUESTION_PAGE_SIZE) + 1);
      else
	od_printf("\n\r`white`[`bright black`Page `bright`%d`white`]  `bright black`Choose a question or",
	 (nPagedToQuestion / QUESTION_PAGE_SIZE) + 1);

      if(nPagedToQuestion < nDisplayedQuestion - QUESTION_PAGE_SIZE) {
	if(colorsch==COLOR_DEF)
	  od_printf(" [N]ext page,");
	else
	  od_printf(" `white`[`bright black`N`white`]`bright black`ext page`white`,");
      }
      if(nPagedToQuestion > 0) {
	if(colorsch==COLOR_DEF)
	  od_printf(" [P]revious page,");
	else
	  od_printf(" `white`[`bright black`P`white`]`bright black`revious page`white`,");
      }
      if(colorsch==COLOR_DEF)
	od_printf(" [Q]uit:");
      else
	od_printf(" `white`[`bright black`Q`white`]`bright black`uit`white`:");

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



/* The ChooseQuestion() function provides a list of questions and allows   */
/* the user to choose a particular question, cancel back to the main menu, */
/* and page up and down in the list of questions. Depending upon the value */
/* of the nFromWhichQuestions parameter, this function will present a list */
/* of questions that the user has voted on, a list of questions that the   */
/* user has not voted on, or a list of all questions.                      */
int FirstQuestion(unsigned int nFromWhichQuestions, int nFileQuestion)
{
   int nCurrent;
//   int nFileQuestion = 0;
//   int nPagedToQuestion = *nLocation;
//   int nDisplayedQuestion = 0;
   unsigned int bVotedOnQuestion;
//   char chCurrent;
   tQuestionRecord QuestionRecord;
   FILE *fpQuestionFile;
//   static char szQuestionName[MAX_QUESTIONS][QUESTION_STR_SIZE];
//   static int nQuestionNumber[MAX_QUESTIONS];

   /* Attempt to open question file. */
   fpQuestionFile = ExculsiveFileOpen(QUESTION_FILENAME, "r+b");

   /* If unable to open question file, assume that no questions have been */
   /* created.                                                            */
   if(fpQuestionFile == NULL)
   {
      /* Display "no questions yet" message. */
      if(Forcevote!=4) {
	if(colorsch!=COLOR_DEF)
	  od_printf("`bright black`");
	od_printf("\n\rNo questions have been created so far");
	if(colorsch!=COLOR_DEF)
	  od_printf("`white`");
	od_printf(".\n\n\r");
      }

      /* Wait for user to press enter. */
      if(Forcevote!=4)
	WaitForEnter();

      /* Indicate that no question has been chosen. */
      return(NO_QUESTION);
   }

   /* Loop for every question record in the file. */
   fseek(fpQuestionFile,QUESTIONRECORD_SIZE*(long)nFileQuestion,SEEK_SET);
   while(freadQuestionRecord(&QuestionRecord, fpQuestionFile) == 1)
   {
      /* Determine whether or not the user has voted on this question. */
      bVotedOnQuestion = CurrentUserRecord.bVotedOnQuestion[nFileQuestion];

      /* If this is the kind of question that the user is choosing from */
      /* right now.                                                     */
      if(((bVotedOnQuestion!=answers[15] && bVotedOnQuestion!=0) && (nFromWhichQuestions & QUESTIONS_VOTED_ON)) ||
	 ((bVotedOnQuestion==answers[15] || bVotedOnQuestion==0) && (nFromWhichQuestions & QUESTIONS_NOT_VOTED_ON)) ||
	 (strcmp(sz_user_name,QuestionRecord.szCreatorName)==0 && (nFromWhichQuestions & CURRENT_USER_ONLY))) {
	 /* If question is not deleted. */
	 if(!(QuestionRecord.bitflags & QUESTION_DELETED)) {
	   if(nFromWhichQuestions & QUESTIONS_FORCED)  {
	     if(QuestionRecord.bitflags & FORCED_QUESTION) {
	       fclose(fpQuestionFile);
	       return(nFileQuestion);
	     }
	   } else {
	     fclose(fpQuestionFile);
	     return(nFileQuestion);
	   }
	 }
      }

      /* Move to next question in file. */
      ++nFileQuestion;
   }

   /* Close question file to allow other nodes to access the file. */
   fclose(fpQuestionFile);

      /* If we were to list all questions. */
      if((nFromWhichQuestions & QUESTIONS_VOTED_ON)
	 && (nFromWhichQuestions & QUESTIONS_NOT_VOTED_ON))
      {
	if(Forcevote!=4) {
	  if(colorsch!=COLOR_DEF)
	    od_printf("`bright black`");
	  od_printf("\n\r\nThere are no more questions");
	  if(colorsch!=COLOR_DEF)
	    od_printf("`white`");
	  od_printf(".\n\n\r");
	}
      }
      /* If we were to list questions that the user has voted on. */
      else if(nFromWhichQuestions & QUESTIONS_VOTED_ON)
      {
	if(colorsch!=COLOR_DEF)
	  od_printf("`bright black`");
	od_printf("\n\n\rThere are no more questions that you have voted on");
	if(colorsch!=COLOR_DEF)
	  od_printf("`white`");
	od_printf(".\n\n\r");
      }
      /* Otherwise, we were to list questions that use has not voted on. */
      else
      {
	if(Forcevote!=4 && Forcevote<100) {
	  if(colorsch!=COLOR_DEF)
	    od_printf("`bright black`");
	  od_printf("\n\rYou have voted on all the questions");
	  if(colorsch!=COLOR_DEF)
	    od_printf("`white`");
	  od_printf(".\n\n\r");
	}
      }

      /* Wait for user to press enter key. */
      if(Forcevote!=4 && Forcevote<100)
	WaitForEnter();

      /* Return, indicating that no question was chosen. */
      return(NO_QUESTION);

}




/* The DisplayQuestionResult() function is called to display the results */
/* of voting on a paricular question, and is passed the question record  */
/* of the question. This function is called when the user selects a      */
/* question using the "view results" option, and is also called after    */
/* the user has voted on a question, to display the results of voting on */
/* that question.                                                        */
int DisplayQuestionResult(tQuestionRecord *pQuestionRecord,int all)
{
   int nAnswer;
   int uPercent;
   int maxpercent=0;
   int cnt;

   /* Clear the screen. */
  // od_printf("\n\n\r");
   my_clr_scr();

   /* Check that there have been votes on this question. */
   if(pQuestionRecord->uTotalVotes == 0)
   {
      /* If there have been no votes for this question, display a message */
      /* and return.*/
      if(colorsch==COLOR_DEF)
	od_printf("`bright green`");
      else
	od_printf("`bright`");
      od_printf("%s",pQuestionRecord->szQuestion);
      if(colorsch==COLOR_DEF)
	od_printf("\n\n\r`green`Nobody has voted on this question yet.\n\n\r");
      else
	od_printf("\n\n\r`bright black`Nobody has voted on this question yet`white`.\n\n\r");
      WaitForEnter();
      return(TRUE);
   }

   /* Display question itself. */
   if(colorsch==COLOR_DEF)
     od_printf("`bright red`");
   else
     od_printf("`bright`");
   od_printf("%s\n\r", pQuestionRecord->szQuestion);


   if(sysop_sec<=od_control.user_security || strcmp(od_control.sysop_name, od_control.user_name) == 0) {
    if((pQuestionRecord->bitflags & ANONYMOUS_QUESTION)==FALSE) {
       /* Display author's name. */
       if(colorsch==COLOR_DEF)
	 od_printf("`red`Question created by %s on %s\n\r",
	   pQuestionRecord->szCreatorName,
	   ctime(&pQuestionRecord->lCreationTime));
       else
	 od_printf("`bright black`Question created by `white`%s`bright black` on `bright`%s\n\r",
	   pQuestionRecord->szCreatorName,
	   ctime(&pQuestionRecord->lCreationTime));

     } else {
       if(colorsch==COLOR_DEF)
	 od_printf("`red`Question created `bright`Anonymously`red` by %s on %s\n\r",
	   pQuestionRecord->szCreatorName,
	   ctime(&pQuestionRecord->lCreationTime));
       else
	 od_printf("`bright black`Question created `bright`Anonymously`bright black` by `white`%s on `bright`%s\n\r",
	   pQuestionRecord->szCreatorName,
	   ctime(&pQuestionRecord->lCreationTime));

     }
   } else {
     if((pQuestionRecord->bitflags & ANONYMOUS_QUESTION)==FALSE) {
       /* Display author's name. */
       if(colorsch==COLOR_DEF)
	 od_printf("`red`Question created by %s on %s\n\r",
	   pQuestionRecord->szCreatorName,
	   ctime(&pQuestionRecord->lCreationTime));
       else
	 od_printf("`bright black`Question created by `white`%s`bright black` on `bright`%s\n\r",
	   pQuestionRecord->szCreatorName,
	   ctime(&pQuestionRecord->lCreationTime));
     } else {
       if(colorsch==COLOR_DEF)
	 od_printf("`red`Question created by Anonymous on %s\n\r",
	   ctime(&pQuestionRecord->lCreationTime));
       else
	 od_printf("`bright black`Question created by `white`Anonymous`bright black` on `bright`%s\n\r",
	   ctime(&pQuestionRecord->lCreationTime));
     }
   }

   /* Display heading for responses. */
   if(colorsch==COLOR_DEF)
     od_printf("`bright green`");
   else
     od_printf("`bright black`");
   od_printf("Response                        Votes  Percent  Graph\n\r");
   if(colorsch==COLOR_DEF)
     od_printf("`green`");
   else
     od_printf("`white`");

   if(od_control.user_ansi || od_control.user_avatar)
   {
      od_repeat((unsigned char)196, 79);
   }
   else
   {
      od_repeat('-', 79);
   }
   od_printf("\n\r");

   for(nAnswer = 0; nAnswer < pQuestionRecord->nTotalAnswers; ++nAnswer)
   {
      /* Determine percent of users who voted for this answer. */
      uPercent = (pQuestionRecord->auVotesForAnswer[nAnswer] * 100)
	 / pQuestionRecord->uTotalVotes;

      if(uPercent>maxpercent)
	maxpercent=uPercent;
   }


   /* Loop for each answer to the question. */
   for(nAnswer = cnt = 0; nAnswer < pQuestionRecord->nTotalAnswers; ++nAnswer)
   {
      /* Determine percent of users who voted for this answer. */
      uPercent = (pQuestionRecord->auVotesForAnswer[nAnswer] * 100)
	 / pQuestionRecord->uTotalVotes;

      /* Display answer, total votes and percentage of votes. */
      if(ColoredResults==FALSE) {
	if(colorsch==COLOR_DEF)
	  od_printf("`green`");
	else
	  od_printf("`bright black`");
      } else if(nAnswer == int(nAnswer/2)*2)
	od_printf("`bright yellow`");
      else
	od_printf("`bright cyan`");

      od_printf("%-30.30s  %-5u  %3u%%     ",
	pQuestionRecord->aszAnswer[cnt],
	pQuestionRecord->auVotesForAnswer[nAnswer],
	uPercent);

      if(colorsch==COLOR_DEF)
	od_printf("`bright`");
      else
	od_printf("`white`");


      /* Display a bar graph corresponding to percent of users who voted */
      /* for this answer.                                                */
      if(uPercent==maxpercent) {
	if(colorsch==COLOR_DEF)
	  od_printf("`bright red`");
	else
	  od_printf("`bright`");
      }
      if(od_control.user_ansi || od_control.user_avatar)
      {
	 od_repeat((unsigned char)220, (unsigned char)((uPercent * 31) / maxpercent));
      }
      else
      {
	 od_repeat('=', (unsigned char)((uPercent * 31) / maxpercent));
      }

      /* Move to next line. */
      od_printf("\n\r");

      if(!(pQuestionRecord->bitflags & BF_15_BY_1_MODE)) {
	/* Display answer, total votes and percentage of votes. */
	if(ColoredResults==FALSE) {
	  if(colorsch==COLOR_DEF)
	    od_printf("`green`");
	  else
	    od_printf("`bright black`");
	} else if(nAnswer == int(nAnswer/2)*2)
	  od_printf("`bright yellow`");
	else
	  od_printf("`bright cyan`");


	od_printf("%-30.30s                  ",
	   pQuestionRecord->aszAnswer[cnt+1]);

	if(colorsch==COLOR_DEF)
	  od_printf("`bright`");
	else
	  od_printf("`white`");

	/* Display a bar graph corresponding to percent of users who voted */
	/* for this answer.                                                */
	if(uPercent==maxpercent) {
	  if(colorsch==COLOR_DEF)
	    od_printf("`bright red`");
	  else
	    od_printf("`bright`");
	}

	if(od_control.user_ansi || od_control.user_avatar)
	{
	  od_repeat((unsigned char)223, (unsigned char)((uPercent * 31) / maxpercent));
	}
	/* Move to next line. */
	od_printf("\n\r");

	cnt+=2;
      } else {
	cnt++;
      }
   }

   /* Display footer. */
   if(colorsch==COLOR_DEF)
     od_printf("`green`");
   else
     od_printf("`white`");
   if(od_control.user_ansi || od_control.user_avatar)
   {
      od_repeat((unsigned char)196, 79);
   }
   else
   {
      od_repeat('-', 79);
   }
   od_printf("\n\r");
   if(colorsch==COLOR_DEF)
     od_printf("`green`              PEOPLE WHO VOTED: %u\n\r",
       pQuestionRecord->uTotalVotes);
   else
     od_printf("`bright black`              PEOPLE WHO VOTED`white`: `bright`%u\n\r",
       pQuestionRecord->uTotalVotes);

   /* Wait for user to press enter. */

   if (all==TRUE) {
     if(colorsch==COLOR_DEF)
       od_printf("`bright`Press [ENTER] to continue or [S] to stop :");
     else
       od_printf("`bright black`Press `white`[`bright black`ENTER`white`] `bright black`to continue or `white`[`bright black`S`white`] `bright black`to stop `white`:");
     if (od_get_answer("S\n\r")=='S') return(FALSE);
   } else {
     WaitForEnter();
   }
   return(TRUE);
}


/* The ReadOrAddCurrentUser() function is used by EZVote to search the    */
/* EZVote user file for the record containing information on the user who */
/* is currently using the door. If this is the first time that the user   */
/* has used this door, then their record will not exist in the user file. */
/* In this case, this function will add a new record for the current      */
/* user. This function returns TRUE on success, or FALSE on failure.      */
int ReadOrAddCurrentUser(void)
{
   FILE *fpUserFile;
   int bGotUser;
   int nQuestion;

   bGotUser = FALSE;

   /* Attempt to open the user file for exclusize access by this node.     */
   /* This function will wait up to the pre-set amount of time (as defined */
   /* near the beginning of this file) for access to the user file.        */
   fpUserFile = ExculsiveFileOpen(USER_FILENAME, "rb");

   /* If unable to open user file, return with failure. */
   if(fpUserFile != NULL)
   {

   /* Begin with the current user record number set to 0. */
   nCurrentUserNumber = 0;

   /* Loop for each record in the file */
//   fseek(fpUserFile, (long)nCurrentUserNumber * USERRECORD_SIZE, SEEK_SET);
   while(freadUserRecord(&CurrentUserRecord, fpUserFile) == 1)
   {

      /* If name in record matches the current user name ... */
      if(stricmp(CurrentUserRecord.szUserName, sz_user_name) == 0)
      {
	 /* ... then record that we have found the user's record, */
	 bGotUser = TRUE;

	 /* and exit the loop. */
	 break;
      }

      /* Move user record number to next user record. */
      nCurrentUserNumber++;
  //    fseek(fpUserFile, (long)nCurrentUserNumber * USERRECORD_SIZE, SEEK_SET);
   }
   fclose(fpUserFile);
   } else {
     nCurrentUserNumber=0;
     bGotUser=FALSE;
   }



   /* If the user was not found in the file, attempt to add them as a */
   /* new user if the user file is not already full.                  */

   if(!bGotUser && nCurrentUserNumber < MAX_USERS)
   {
      /* Place the user's name in the current user record. */
      strcpy(CurrentUserRecord.szUserName, sz_user_name);

      /* Record that user hasn't voted on any of the questions. */
      for(nQuestion = 0; nQuestion < MAX_QUESTIONS; ++nQuestion)
      {
	 CurrentUserRecord.bVotedOnQuestion[nQuestion] = 0;
      }

      fpUserFile = ExculsiveFileOpen(USER_FILENAME, "a+b");
      /* Write the new record to the file. */
      if(fwriteUserRecord(&CurrentUserRecord, fpUserFile) == 1)
      {
	 /* If write succeeded, record that we now have a valid user record. */
	 bGotUser = TRUE;
      }
      fclose(fpUserFile);
   }

   /* Close the user file to allow other nodes to access it. */
//   fclose(fpUserFile);

   /* Return, indciating whether or not a valid user record now exists for */
   /* the user that is currently online.                                   */
   return(bGotUser);
}


/* The WriteCurrentUser() function is called to save the information on the */
/* user who is currently using the door, to the EZVOTE.USR file.            */
void WriteCurrentUser(void)
{
   FILE *fpUserFile;

   /* Attempt to open the user file for exclusize access by this node.     */
   /* This function will wait up to the pre-set amount of time (as defined */
   /* near the beginning of this file) for access to the user file.        */
   fpUserFile = ExculsiveFileOpen(USER_FILENAME, "r+b");

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

   fseek(fpUserFile, (long)nCurrentUserNumber * USERRECORD_SIZE, SEEK_SET);

   /* Write the new record to the file. */
   if(fwriteUserRecord(&CurrentUserRecord, fpUserFile) < 1)
   {
      /* If unable to write the record, display an error message. */
      fclose(fpUserFile);
      od_printf("Unable to update your user record file.\n\r");
      WaitForEnter();
      return;
   }

   /* Close the user file to allow other nodes to access it again. */
   fclose(fpUserFile);
}


#ifdef ODPLAT_NIX
FILE *fsopen(const char *pszFilename, const char *pszMode, int shmode)
{
    int file;
    int Mode=0;
    char pszNewMode[3];
    const char *p;
    
    for(p=pszMode;*p;p++)  {
        switch (*p)  {
	    case 'r':
	        Mode |= 1;
		break;
	    case 'w':
	        Mode |= 2;
		break;
	    case 'a':
	        Mode |= 4;
                break;
	    case '+':
	        Mode |= 8;
		break;
	    case 'b':
		case 't':
	        break;
	    default:
	        errno=EINVAL;
		return(NULL);
	}
    }
    switch(Mode)  {
        case 1:
	    Mode=O_RDONLY;
	    break;
	case 2:
	    Mode=O_WRONLY|O_CREAT;
	    break;
	case 4:
	    Mode=O_APPEND|O_WRONLY|O_CREAT;
	    break;
	case 9:
	    Mode=O_RDWR;
	    break;
	case 10:
	    Mode=O_RDWR|O_CREAT;
	    break;
	case 12:
	    Mode=O_RDWR|O_APPEND|O_CREAT;
	    break;
	default:
	    errno=EINVAL;
	    return(NULL);
    }
    if(Mode&O_CREAT)
        file=sopen(pszFilename,Mode,shmode,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    else
        file=sopen(pszFilename,Mode,shmode);
    if(file==-1)
        return(NULL);
    return(fdopen(file,pszMode));
}
#endif

/* This function opens the specified file in the specified mode for         */
/* exculsive access by this node; while the file is open, other nodes will  */
/* be unable to open the file. This function will wait for up to the number */
/* of seconds set by FILE_ACCESS_MAX_WAIT, which is defined near the        */
/* beginning of this file.                                                  */
FILE *ExculsiveFileOpen(const char *pszFileName, const char *pszMode)
{
   FILE *fpFile = NULL;
   time_t StartTime = time(NULL);

   /* Attempt to open the file while there is still time remaining. */
   while((fpFile = fsopen(pszFileName, pszMode, SH_DENYRW)) == NULL
      && errno == EACCES
      && difftime(time(NULL), StartTime) < 60)
   {
      /* If we were unable to open the file, call od_kernal, so that    */
      /* OpenDoors can continue to respond to sysop function keys, loss */
      /* of connection, etc.                                            */
      od_kernal();
   }

   /* Return FILE pointer for opened file, if any. */
   return(fpFile);
}

void
warp_line(char line1[], char line2[],const char intro[])
{
  int cnt,last_space=ANSWER_STR_SIZE-2;
  char key,warp=TRUE;
/*  od_printf(" ");*/
  line1[ANSWER_STR_SIZE-1]=0;
  line2[ANSWER_STR_SIZE-1]=0;

  for(cnt=0;cnt<(ANSWER_STR_SIZE);cnt++) {
    key=od_get_key(TRUE);
    if((key==' ' || key=='-' || key==','
       || key=='.' || key==':' || key==';'
       || key=='?' || key=='!') && cnt<(ANSWER_STR_SIZE-1))
      last_space=cnt;

    if(key=='\n' || key=='\r') {
      line1[cnt]=0;
      warp=FALSE;
      break;
    } else if(key=='\b') {
      if(cnt>0) {
	od_printf("\b \b");
	line1[cnt-1]=0;
	cnt-=2;
      }
    } else {
      od_putch(key);
      line1[cnt]=key;
    }
  }
  if(warp==TRUE) {
    for(cnt=last_space;cnt<(ANSWER_STR_SIZE-1);cnt++)
      od_putch('\b');
    for(cnt=last_space;cnt<(ANSWER_STR_SIZE-1);cnt++)
      od_putch(' ');

    od_printf("\n\r");
    od_printf(intro);

    for(cnt=last_space;cnt<(ANSWER_STR_SIZE-1);cnt++) {
      line2[cnt-last_space]=line1[1+cnt];
      od_putch(line1[cnt+1]);
    }

  } else {

    last_space=ANSWER_STR_SIZE-1;
    od_printf("\n\r");
    od_printf(intro);

  }

  line1[last_space+1]=0;
/*  printf("\n\n%s\n\n",line1);*/
  if(line1[0]==0)
    return;



  cnt=ANSWER_STR_SIZE-last_space-1;

/*  printf("\r\n\n%d",cnt);*/
  for(;;) {
    key=od_get_key(TRUE);
    if(key=='\n' || key=='\r') {
      line2[cnt]=0;
      break;
    } else if(key=='\b') {
      if(cnt>0) {
	od_printf("\b \b");
	line2[cnt-1]=0;
	cnt-=2;
      }
    } else {
      od_putch(key);
      line2[cnt]=key;
    }
    if(cnt<(ANSWER_STR_SIZE-1))
      cnt++;
    else
      od_printf("\b \b");
  }
  line2[cnt]=0;
/*  printf("\n\n%s\n\n",line2);*/
  od_printf("\n\r");
}



/* The WaitForEnter() function is used by EZVote to create its custom */
/* "Press [ENTER] to continue." prompt.                               */
void WaitForEnter(void)
{
   /* Display prompt. */
   if(colorsch==COLOR_DEF)
     od_printf("`bright`Press [ENTER] to continue.");
   else
     od_printf("`bright black`Press `white`[`bright black`ENTER`white`]`bright black` to continue`white`.`bright black`");

   /* Wait for a Carriage Return or Line Feed character from the user. */
   od_get_answer("\n\r");
   od_printf("\n\r");
}
