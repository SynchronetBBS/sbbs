/* SBJ.C */

/* $Id$ */

/************************/
/* Synchronet Blackjack */
/************************/

/*******************************************************/
/* Multiuser Blackjack game for Synchronet BBS systems */
/*******************************************************/

/****************************************************************************/
/* This source code is completely Public Domain and can be modified and 	*/
/* distributed freely (as long as changes are documented).					*/
/* It is meant as an example to programmers of how to use the XSDK			*/
/****************************************************************************/

/***********/
/* History */
/****************************************************************************\

		Many bugs. Especially multiplayer.
v1.0
		Many bugs fixed. Timing problems still exist.
v1.01
		Fixed yet more bugs. No more timing problems. Appears bullet-proof.
v1.02
		Fixed dealer card up always showing card symbol (even when symbols off).
		Added ctrl-e answer detection and user notification.
		Fixed three 7's bug.
		Raised maximum number of decks to 100 for large multinode systems.
		Fixed /<CR> bug.
		Fixed multiple split bug.
		Fixed non-symbols being sent to other nodes bug.
		Changed this node's hands to say "You" instead of the user name.
v1.03
		Changed the warning and timeout times
v1.04
		Fixed symbols being displayed on dealer's hand even when disabled.
		Made different inactivity warning and timeout values for the main
		menu and when in play.
v1.05
		Fixed invalid (usually negative) card bug. THELP random() doc error.
		Card now actually contains all the cards minus one.
		Fixed multinode play join and hang bug.
v1.06
		If player gets blackjack and dealer gets 21, player wins. Used to push.
v1.07
		Fixed split, then double bug.
v1.08
		Replaced bioskey(1) calls with inkey() and used XSDK v2.0 with node
		intercommunication with users on BBS or in other external programs.
v2.00
		Fixed problem with loosing first character of chat lines
		Added DESQview awareness
v2.01
		Replaced all calls to delay() with fdelay()
v2.02
		Listing users now displays what external program they're running.
		Fixed problem with max bet being too small when users have over
		65mb of credit.
v2.02
		XSDK (and consequently SBJ) now supports shrinking Synchronet to run
		(available in v1b r1).
		SBBSNODE environment variable will be used for the current node's dir
		if the node dir is not specified on the command line.
v2.03
		XSDK and SBJ now support the new message services of Synchronet
		(added in v1b r2) for more full-proof internode messaging.
v2.10
		New XSDK that supports new file retrieval node status display.
v2.11
		Changed getnodemsg to eliminate tiny void where messages could fall.


\****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "xsdk.h"

#define MAX_DECKS	100
#define MAX_CARDS	10		/* maximum number of cards per hand */
#define MAX_HANDS	4		/* maximum number of hands per player */

#define DEBUG 0

#define J 11	/* jack */
#define Q 12	/* queen */
#define K 13	/* king */
#define A 14	/* ace */

#define H 0		/* heart */
#define D 1 	/* diamond */
#define C 2		/* club */
#define S 3		/* spade */
									/* bits used in misc variable */
#define INPLAY		(1<<0)			/* hand in play */

enum {								/* values for status bytes */
	 BET							/* betting */
	,WAIT							/* waiting for turn */
	,PLAY							/* playing his hand */
	,SYNC_P 						/* In sync area - player */
	,SYNC_D 						/* In sync area - dealer */
	};

typedef struct { char value, suit; } card_t;

card_t newdeck[52]={
	 2,H, 2,D, 2,C, 2,S,
	 3,H, 3,D, 3,C, 3,S,
	 4,H, 4,D, 4,C, 4,S,
	 5,H, 5,D, 5,C, 5,S,
	 6,H, 6,D, 6,C, 6,S,
	 7,H, 7,D, 7,C, 7,S,
	 8,H, 8,D, 8,C, 8,S,
	 9,H, 9,D, 9,C, 9,S,
	10,H,10,D,10,C,10,S,
	 J,H, J,D, J,C, J,S,
	 Q,H, Q,D, Q,C, Q,S,
	 K,H, K,D, K,C, K,S,
	 A,H, A,D, A,C, A,S };

uchar	misc;
uchar	curplayer;
uchar	total_decks,sys_decks;
uchar	total_nodes;
int 	cur_card;
uchar	dc;
card_t	dealer[MAX_CARDS];
int 	gamedab;					 /* file handle for data file */
card_t	card[MAX_DECKS*52];
card_t	player[MAX_HANDS][MAX_CARDS];
char	hands,pc[MAX_HANDS];
uchar	total_players;
uchar	symbols=1;
char	autoplay=0;
int 	logit=0,tutor=0;
uint	node[MAX_NODES];   /* the usernumber in each node */
char	status[MAX_NODES];
ulong	credits;
uint	bet[MAX_HANDS],ibet,min_bet,max_bet;
char	tmp[81];
char	*UserSays="\1n\1m\1h%s \1n\1msays \"\1c\1h%s\1n\1m\"\r\n";
char	*UserWhispers="\1n\1m\1h%s \1n\1mwhispers \"\1c\1h%s\1n\1m\"\r\n";
char	*ShoeStatus="\r\n\1_\1w\1hShoe: %u/%u\r\n";

void play(void);
char *cardstr(card_t card);
char hand(card_t card[MAX_CARDS], char count);
char soft(card_t card[MAX_CARDS], char count);
char pair(card_t card[MAX_CARDS], char count);
void getgamedat(char lockit);
void putgamedat(void);
void getcarddat(void);
void putcarddat(void);
void shuffle(void);
void waitturn(void);
void nextplayer(void);
char lastplayer(void);
char firstplayer(void);
void getnodemsg(void);
void putnodemsg(char *msg, uint nodenumber);
void putallnodemsg(char *msg);
void syncplayer(void);
void syncdealer(void);
void moduserdat(void);
char *hit(void);
char *stand(void);
char *doubit(void);
char *split(void);
void open_gamedab(void);
void create_gamedab(void);
char *activity(char status_type);
void chat(void);
void listplayers(void);
char *joined(void);
char *left(void);
void strip_symbols(char *str);
void debug(void);

#ifndef SBJCLEAN

int my_random(int n)
{
	float f;

	if(n<2)
		return(0);
	f=(float)rand()/(float)RAND_MAX;

	return((int)(n*f));
}

/****************************************************************************/
/* Entry point																*/
/****************************************************************************/
int main(int argc, char **argv)
{
	char str[81],compiler[32],*p;
	int i,file;
	FILE *stream;

	node_dir[0]=0;
	for(i=1;i<argc;i++)
		if(!stricmp(argv[i],"/L"))
			logit=1;
		else if(!stricmp(argv[i],"/T"))
			tutor=2;
		else if(!stricmp(argv[i],"/S"))
			tutor=1;
		else strcpy(node_dir,argv[i]);

	p=getenv("SBBSNODE");
	if(!node_dir[0] && p)
		strcpy(node_dir,p);

	if(!node_dir[0]) {	  /* node directory not specified */
		printf("usage: sbj <node directory> [/options]\r\n");
		printf("\r\noptions: L = log wins/losses for each day\r\n");
		getch();
		return(1); }

	if(node_dir[strlen(node_dir)-1]!='\\'
		&& node_dir[strlen(node_dir)-1]!='/')  /* make sure node_dir ends in '\' */
		strcat(node_dir,"/");

	initdata(); 								/* read XTRN.DAT and more */
	credits=user_cdt;
	total_nodes=sys_nodes;

	remove("debug.log");

	if((file=nopen("sbj.cfg",O_RDONLY))==-1) {  /* open config file */
		bputs("Error opening sbj.cfg\r\n");
		pause();
		return(1); }
	if((stream=fdopen(file,"rb"))==NULL) {      /* convert to stream */
		bputs("Error converting sbj.cfg handle to stream\r\n");
		pause();
		return(1); }
	fgets(str,81,stream);						/* number of decks in shoe */
	total_decks=sys_decks=atoi(str);
	fgets(str,81,stream);						/* min bet (in k) */
	min_bet=atoi(str);
	fgets(str,81,stream);						/* max bet (in k) */
	max_bet=atoi(str);
	fgets(str,81,stream);						/* default bet (in k) */
	ibet=atoi(str);
	fclose(stream);
	if(!total_decks || total_decks>MAX_DECKS) {
		bputs("Invalid number of decks in sbj.cfg\r\n");
		pause();
		return(1); }
	if(!max_bet) {
		bputs("Invalid max bet in sbj.cfg\r\n");
		pause();
		return(1); }
	if(min_bet>max_bet) {
		bputs("Invalid min bet in sbj.cfg\r\n");
		pause();
		return(1); }
	if(ibet>max_bet || ibet<min_bet) {
		bputs("Invalid default bet in sbj.cfg\r\n");
		pause();
		return(1); }

	if(!fexist("card.dab")) {
		cur_card=0;
		dc=0;
		memset(dealer,0,sizeof(dealer));
		memset(card,0,sizeof(card));
		putcarddat(); }
	else {
		getcarddat();
		if(total_decks!=sys_decks) {
			remove("card.dab");
			total_decks=sys_decks;
			putcarddat(); } }

	if(!fexist("game.dab"))         /* File's not there */
		create_gamedab();

	open_gamedab();

	getgamedat(0);
	if(total_nodes!=sys_nodes) {  /* total nodes changed */
		close(gamedab);
		total_nodes=sys_nodes;
		create_gamedab();
		open_gamedab(); }

	srand((unsigned)time(NULL));

#ifdef __16BIT__
	while(_bios_keybrd(1))	 /* clear input buffer */
		_bios_keybrd(0);
#endif

	putchar(5); /* ctrl-e */
	mswait(500);
	if(keyhit()) {
#ifdef __16BIT__
		while(_bios_keybrd(1))
			_bios_keybrd(0);
#else
		getkey(0);
#endif
		bputs("\r\n\1r\1h\1i*** ATTENTION ***\1n\1h\r\n");
		bputs("\r\nSynchronet Blackjack uses Ctrl-E (ENQ) for the 'club' card "
			"symbol.");
		bputs("\r\nYour terminal responded to this control character with an "
			"answerback string.");
		bputs("\r\nYou will need to disable all Ctrl-E (ENQ) answerback "
			"strings (Including \r\nCompuserve Quick B transfers) if you wish to "
			"toggle card symbols on.\r\n\r\n");
		symbols=0;
		pause(); }

	getgamedat(1);
	node[node_num-1]=0;
	putgamedat();

	/* Override default mnemonic colors */
	mnehigh=RED|HIGH;
	mnelow=CYAN|HIGH;

	/* Override default inactivity timeout values */
	sec_warn=120;	
	sec_timeout=180;

	COMPILER_DESC(compiler);

#define SBJ_INDENT "                                "
	while(1) {
		cls();
		sprintf(str,"\1n\1h\1cSynchronet \1rBlackjack! \1cv3.20 for %s\r\n"
			,PLATFORM_DESC);
		center(str);
		sprintf(str,"\1w(XSDK v%s %s %s)\r\n\r\n"
            ,xsdk_ver,compiler,__DATE__);
		center(str);

		aborted=0;
		mnemonics(SBJ_INDENT"~Instructions\r\n");
		mnemonics(SBJ_INDENT"~Join/Begin Game\r\n");
		mnemonics(SBJ_INDENT"~List Players\r\n");
		mnemonics(SBJ_INDENT"~Rules of the Game\r\n");
		mnemonics(SBJ_INDENT"~Toggle Card Symbols\r\n");
		sprintf(str,SBJ_INDENT"~Quit to %s\r\n",sys_name);
		mnemonics(str);
		nodesync();
		bprintf("\1_\r\n"SBJ_INDENT"\1y\1hWhich: \1n");
		switch(getkeys("IJBLRTQ|!",0)) {
			#if DEBUG
			case '!':
				if(!com_port)
					autoplay=1;
				break;
			case '|':
				debug();
				break;
			#endif
			case 'I':
				cls();
				printfile("sbj.msg");
				break;
			case 'L':
				listplayers();
				bprintf(ShoeStatus,cur_card,total_decks*52);
				break;
			case 'R':
				bprintf("\1n\1c\r\nMinimum bet: \1h%uk",min_bet);
				bprintf("\1n\1c\r\nMaximum bet: \1h%uk\r\n",max_bet);
				bprintf("\1w\1h\r\nCard decks in shoe: \1h%u\r\n",sys_decks);
				break;
			case 'T':
				symbols=!symbols;
				bprintf("\1_\1w\r\nCard symbols now: %s\r\n",symbols ? "ON":"OFF");
				break;
			case 'Q':
				exit(0);
			case 'J':
			case 'B':
				sec_warn=60;	/* Override default inactivity timeout values */
				sec_timeout=90;
				play();
				sec_warn=120;
				sec_timeout=180;
				break; 
		} 
	}
}

#if DEBUG
void debug()
{
	int i;

if(user_level<90)
	return;
getgamedat(0);
getcarddat();

bprintf("\r\nDeck (%d) Current: %d\r\n\r\n",total_decks,cur_card);
for(i=0;i<total_decks*52;i++) {
	if(!(i%11))
		bputs("\r\n");
	bprintf("%3d:%-11s",i,cardstr(card[i])); }

pause();
bprintf("\1n\r\nDealer (%d)\r\n\r\n",dc);
for(i=0;i<dc;i++)
	bprintf("%s ",cardstr(dealer[i]));
bprintf("\1n\r\nNodes (%d) Current: %d\r\n\r\n"
	,total_nodes,curplayer);
for(i=0;i<total_nodes;i++)
	bprintf("%d: node=%d status=%d %s\r\n",i+1,node[i]
		,status[i],activity(status[i]));
}

void debugline(char *line)
{
	char str[256];
	int file;
	time_t now;
	struct dosdate_t date;
	struct dostime_t curtime;

#if 1
now=time(NULL);
unixtodos(now,&date,&curtime);
if((file=nopen("debug.log",O_WRONLY|O_APPEND|O_CREAT))==-1)
	return;
sprintf(str,"%d %02u:%02u:%02u %s\r\n"
	,node_num,curtime.ti_hour,curtime.ti_min,curtime.ti_sec,line);
write(file,str,strlen(str));
close(file);
#endif
}

#endif

void suggest(char action)
{
bputs("Dealer suggests you ");
switch(action) {
	case 'H':
		bputs("hit");
		break;
	case 'S':
		bputs("stand");
		break;
	case 'D':
		bputs("double");
		break;
	case 'P':
		bputs("split");
		break; }
bputs("\r\n");
}

void wrong(char action)
{
#ifdef __16BIT__
sound(100);
mswait(500);
nosound();
#endif
bputs("Dealer says you should have ");
switch(action) {
	case 'H':
		bputs("hit");
		break;
	case 'S':
		bputs("stood");
		break;
	case 'D':
		bputs("doubled");
		break;
	case 'P':
		bputs("split");
		break; }
bputs("\r\n");
}

/****************************************************************************/
/* This function is the actual game playing loop.							*/
/****************************************************************************/
void play()
{
	char str[256],str2[256],log[81],done,doub,dh,split_card,suggestion
		,*YouWereDealt="\1n\1k\0015 You \1n\1m were dealt: %s\r\n"
		,*UserWasDealt="\1n\1m\1h%s\1n\1m was dealt: %s\r\n"
		,*YourHand="\1n\1k\0015 You \1n\1m                     (%2d) %s"
		,*UserHand="\1n\1m\1h%-25s \1n\1m(%2d) %s"
		,*DealerHand="\1n\1hDealer                    \1n\1m(%2d) "
		,*Bust="\1n\1r\1hBust\1n\r\n"
		,*Natural="\1g\1h\1iNatural "
		,*Three7s="\1r\1h\1iThree 7's "
		,*Blackjack="\1n\0011\1k Blackjack! \1n\r\n"
		,*TwentyOne="\1n\0012\1k Twenty-one \1n\r\n";
	int h,i,j,file;
	uint max;
	long val;
	time_t start,now;
	struct tm* tm;

sprintf(str,"MESSAGE.%d",node_num);         /* remove message if waiting */
if(fexist(str))
    remove(str);

getgamedat(0);
if(node[node_num-1]) {
	getgamedat(1);
	node[node_num-1]=0;
	putgamedat();
	getgamedat(0); }

if(total_players && misc&INPLAY) {
	bputs("\r\n\1hWaiting for end of hand (^A to abort)...\1n");
	start=now=time(NULL);
	getgamedat(0);
	while(total_players && misc&INPLAY) {
		if((i=inkey(0))!=0) {	 /* if key was hit */
			if(i==1) {		 /* if ctrl-a */
				bputs("\r\n");
				return; } }  /* return */
		mswait(100);
		getgamedat(0);
		now=time(NULL);
		if(now-start>300) { /* only wait up to 5 minutes */
			bputs("\r\ntimeout\r\n");
			return; } }
	bputs("\r\n"); }

getgamedat(1);
node[node_num-1]=user_number;
putgamedat();

if(!total_players)
    shuffle();
else
	listplayers();

sprintf(str,"\1n\1m\1h%s \1n\1m%s\r\n",user_name,joined());
putallnodemsg(str);

while(1) {
	aborted=0;
	#if DEBUG
	debugline("top of loop");
	#endif
	if(autoplay)
		lncntr=0;
	bprintf(ShoeStatus,cur_card,total_decks*52);
	if(cur_card>(total_decks*52)-(total_players*10)-10 && lastplayer())
		shuffle();
	getgamedat(1);
	misc&=~INPLAY;
	status[node_num-1]=BET;
	node[node_num-1]=user_number;
	putgamedat();

	bprintf("\r\n\1n\1cYou have \1h%s\1n\1ck credits\r\n"
		,ultoac(credits/1024L,str));
	if(credits<min_bet/1024) {
		bprintf("\1n\1cMinimum bet: \1h%uk\r\n",min_bet);
		bputs("\1n\1r\1hCome back when you have more credits.\r\n");
        break; }
	if(credits/1024L>(ulong)max_bet)
		max=max_bet;
	else
		max=credits/1024L;
	sprintf(str,"\r\nBet amount (in kilobytes) or ~Quit [%u]: "
		,ibet<credits/1024L ? ibet : credits/1024L);
	chat();
	mnemonics(str);
	if(autoplay && keyhit())
		autoplay=0;
	if(autoplay)
		i=ibet;
	else
		i=getnum(max);
	if(i==-1)	/* if user hit ^C or 'Q' */
		break;
	bputs("\r\n");
	if(i)		/* if user entered a value */
		bet[0]=i;
	else		/* if user hit enter */
		bet[0]=ibet<credits/1024L ? ibet : credits/1024L;
	if(bet[0]<min_bet) {
		bprintf("\1n\1cMinimum bet: \1h%uk\r\n",min_bet);
		bputs("\1n\1r\1hCome back when you're ready to bet more.\r\n");
		break; }
	ibet=bet[0];
	getgamedat(0);	/* to get all new arrivals */
	sprintf(str,"\1m\1h%s\1n\1m bet \1n\1h%u\1n\1mk\r\n",user_name,bet[0]);
	putallnodemsg(str);

	pc[0]=2;						/* init player's 1st hand to 2 cards */
	for(i=1;i<MAX_HANDS;i++)		/* init player's other hands to 0 cards */
		pc[i]=0;
	hands=1;						/* init total player's hands to 1 */

	getgamedat(1);					/* first come first serve to be the */
	for(i=0;i<total_nodes;i++)		/* dealer in control of sync */
		if(node[i] && status[i]==SYNC_D)
			break;
	if(i==total_nodes) {
		#if DEBUG
		debugline("syncdealer");
		#endif
		syncdealer();  }			/* all players meet here */
	else {							/* first player is current after here */
		#if DEBUG
		debugline("syncplayer");
		#endif
		syncplayer(); } 			/* game is closed (INPLAY) at this point */

	#if DEBUG
	debugline("waitturn 1");
	#endif
    waitturn();
	getnodemsg();
										/* Initial deal card #1 */
	getcarddat();
	player[0][0]=card[cur_card++];
	putcarddat();
	sprintf(str,YouWereDealt,cardstr(card[cur_card-1]));
	if(!symbols)
		strip_symbols(str);
    bputs(str);
	sprintf(str,UserWasDealt,user_name,cardstr(card[cur_card-1]));
    putallnodemsg(str);
	
	if(lastplayer()) {
		getcarddat();
		dealer[0]=card[cur_card++];
		dc=1;
		putcarddat(); }
	nextplayer();
	#if DEBUG
	debugline("waitturn 2");
	#endif
	waitturn();
	getnodemsg();

	getcarddat();					   /* Initial deal card #2 */
	player[0][1]=card[cur_card++];
	putcarddat();
	sprintf(str,YouWereDealt,cardstr(card[cur_card-1]));
	if(!symbols)
		strip_symbols(str);
	bputs(str);
	sprintf(str,UserWasDealt,user_name,cardstr(card[cur_card-1]));
    putallnodemsg(str);
	
	if(lastplayer()) {
		getcarddat();
		dealer[1]=card[cur_card++];
		dc=2;
		putcarddat(); }
	nextplayer();
	#if DEBUG
	debugline("waitturn 3");
	#endif
	waitturn();
	getnodemsg();
	getcarddat();

	for(i=0;i<hands;i++) {
		if(autoplay)
			lncntr=0;
		done=doub=0;
		while(!done && pc[i]<MAX_CARDS && cur_card<total_decks*52) {
			h=hand(player[i],pc[i]);
			str[0]=0;
			for(j=0;j<pc[i];j++) {
				strcat(str,cardstr(player[i][j]));
				strcat(str," "); }
			j=bstrlen(str);
			while(j++<19)
				strcat(str," ");
			if(h>21) {
				strcat(str,Bust);
				sprintf(str2,YourHand,h,str);
				if(!symbols)
					strip_symbols(str2);
				bputs(str2);
				sprintf(str2,UserHand,user_name,h,str);
				putallnodemsg(str2);
				break; }
			if(h==21) {
				if(pc[i]==2) {	/* blackjack */
					if(player[i][0].suit==player[i][1].suit)
						strcat(str,Natural);
					strcat(str,Blackjack); }
				else {
					if(player[i][0].value==7
						&& player[i][1].value==7
						&& player[i][2].value==7)
						strcat(str,Three7s);
					strcat(str,TwentyOne); }
				sprintf(str2,YourHand,h,str);
				if(!symbols)
					strip_symbols(str2);
				bputs(str2);
				sprintf(str2,UserHand,user_name,h,str);
                putallnodemsg(str2);
				// fdelay(500);
				break; }
			strcat(str,"\r\n");
			sprintf(str2,YourHand,h,str);
			if(!symbols)
				strip_symbols(str2);
			bputs(str2);
			sprintf(str2,UserHand,user_name,h,str);
			putallnodemsg(str2);
			if(doub)
				break;
			sprintf(str,"\1n\1hDealer\1n\1m card up: %s\r\n"
				,cardstr(dealer[1]));
			if(!symbols)
				strip_symbols(str);
			bputs(str);

			if(tutor) {
				if(pc[i]==2)
					split_card=pair(player[i],pc[i]);
				else
					split_card=0;
                if(split_card==A
					|| (split_card==9 && (dealer[1].value<7
						|| (dealer[1].value>7 && dealer[1].value<10)))
                    || split_card==8
					|| (split_card==7 && dealer[1].value<9)
					|| (split_card==6 && dealer[1].value<7)
					|| (split_card==4 && dealer[1].value==5)
					|| (split_card && split_card<4 && dealer[1].value<8))
					suggestion='P';
                else if(soft(player[i],pc[i])) {
                    if(h>18)
						suggestion='S';
					else if(pc[i]==2
						&& ((h==18
							&& dealer[1].value>3 && dealer[1].value<7)
                        || (h==17
							&& dealer[1].value>2 && dealer[1].value<7)
                        || (h>13
							&& dealer[1].value>3 && dealer[1].value<7)
                        || (h==12
							&& dealer[1].value>4 && dealer[1].value<7)))
						suggestion='D';
                    else
						suggestion='H'; }
                else { /* hard */
					if(h>16 || (h>13 && dealer[1].value<7)
						|| (h==12 && dealer[1].value>3 && dealer[1].value<7))
						suggestion='S';
					else if(pc[i]==2
						&& (h==11 || (h==10 && dealer[1].value<10)
						|| (h==9 && dealer[1].value<7)))
						suggestion='D';
                    else
						suggestion='H'; } }

			if(tutor==1)
				suggest(suggestion);
			strcpy(str,"\r\n~Hit");
			strcpy(tmp,"H\r");
			if(bet[i]+ibet<=credits/1024L && pc[i]==2) {
				strcat(str,", ~Double");
				strcat(tmp,"D"); }
			if(bet[i]+ibet<=credits/1024L && pc[i]==2 && hands<MAX_HANDS
				&& player[i][0].value==player[i][1].value) {
				strcat(str,", ~Split");
				strcat(tmp,"S"); }
			strcat(str,", or [Stand]: ");
			chat();
			mnemonics(str);
			if(autoplay && keyhit())
				autoplay=0;


			if(autoplay) {
				lncntr=0;
				bputs("\r\n");
				strcpy(str,stand());
				bputs(str);
				putallnodemsg(str);
				done=1; }
			else
			switch(getkeys(tmp,0)) {
				case 'H':     /* hit */
					if(tutor==2 && suggestion!='H')
						wrong(suggestion);
					strcpy(str,hit());
					bputs(str);
					putallnodemsg(str);
					getcarddat();
					player[i][pc[i]++]=card[cur_card++];
					putcarddat();
					break;
				case 'D':   /* double down */
					if(tutor==2 && suggestion!='D')
                        wrong(suggestion);
					strcpy(str,doubit());
					bputs(str);
					putallnodemsg(str);
					getcarddat();
					player[i][pc[i]++]=card[cur_card++];
					putcarddat();
					doub=1;
					bet[i]+=ibet;
					break;
				case 'S':   /* split */
					if(tutor==2 && suggestion!='P')
                        wrong(suggestion);
					strcpy(str,split());
					bputs(str);
					putallnodemsg(str);
					player[hands][0]=player[i][1];
					getcarddat();
					player[i][1]=card[cur_card++];
					player[hands][1]=card[cur_card++];
					putcarddat();
					pc[hands]=2;
					bet[hands]=ibet;
					hands++;
					break;
				case CR:
					if(tutor==2 && suggestion!='S')
                        wrong(suggestion);
					strcpy(str,stand());
					bputs(str);
					putallnodemsg(str);
					done=1;
					break; } } }

	if(lastplayer()) {	/* last player plays the dealer's hand */
		getcarddat();
		while(hand(dealer,dc)<17 && dc<MAX_CARDS && cur_card<total_decks*52)
			dealer[dc++]=card[cur_card++];
		putcarddat(); }

	nextplayer();
	#if DEBUG
	debugline("waitturn 4");
	#endif
	waitturn();
	getnodemsg();

	if(firstplayer()==node_num) {
		strcpy(str,"\1n\0014\1h Final \1n\r\n");
		bputs(str);
		putallnodemsg(str); }
	getcarddat();
	dh=hand(dealer,dc); 					/* display dealer's hand */
	sprintf(str,DealerHand,dh);
	for(i=0;i<dc;i++) {
		strcat(str,cardstr(dealer[i]));
		strcat(str," "); }
	i=bstrlen(str);
	while(i++<50)				/* was 50 */
		strcat(str," ");
	if(dh>21) {
		strcat(str,Bust);
		if(!symbols)
			strip_symbols(str);
		bputs(str); }
	else if(dh==21) {
		if(dc==2) { 	/* blackjack */
			if(dealer[0].suit==dealer[1].suit)
				strcat(str,Natural);
			strcat(str,Blackjack); }
		else {			/* twenty-one */
			if(dc==3 && dealer[0].value==7 && dealer[1].value==7
				&& dealer[2].value==7)
				strcat(str,Three7s);
			strcat(str,TwentyOne); }
		if(!symbols)
            strip_symbols(str);
		bputs(str); }
	else {
		if(!symbols)
            strip_symbols(str);
		bprintf("%s\r\n",str); }

	for(i=0;i<hands;i++) {						/* display player's hand(s) */
		h=hand(player[i],pc[i]);
		str[0]=0;
		for(j=0;j<pc[i];j++) {
			strcat(str,cardstr(player[i][j]));
			strcat(str," "); }
		j=bstrlen(str);
		while(j++<19)
			strcat(str," ");
		if(logit) {
			now=time(NULL);
			tm=localtime(&now);
			sprintf(log,"%02d%02d%02d.log"                  /* log winnings */
				,tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
			if((file=nopen(log,O_RDONLY))!=-1) {
				read(file,tmp,filelength(file));
				tmp[filelength(file)]=0;
				val=atol(tmp);
				close(file); }
			else
				val=0L;
			if((file=nopen(log,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
				bprintf("error opening %s\r\n",log);
				return; } }
		if(h<22 && (h>dh || dh>21	/* player won */
			|| (h==21 && pc[i]==2 && dh==21 && dh>2))) {	/* blackjack */
			j=bet[i];								  /* and dealer got 21 */
			if(h==21 && 	/* natural blackjack or three 7's */
				((player[i][0].value==7 && player[i][1].value==7
				&& player[i][2].value==7)
				|| (pc[i]==2 && player[i][0].suit==player[i][1].suit)))
				j*=2;
			else if(h==21 && pc[i]==2)	/* regular blackjack */
				j*=1.5; /* blackjack pays 1 1/2 to 1 */
			sprintf(tmp,"\1n\1h\1m\1iWon!\1n\1h %u\1n\1mk",j);
			strcat(str,tmp);
			credits+=j*1024L;
			val-=j*1024L;
			moduserdat(); }
		else if(h<22 && h==dh)
			strcat(str,"\1n\1hPush");
		else {
			strcat(str,"\1nLost");
			credits-=bet[i]*1024L;
			val+=bet[i]*1024L;
			moduserdat(); }
		if(logit) {
			sprintf(tmp,"%ld",val);
			write(file,tmp,strlen(tmp));
			close(file); }					/* close winning log */
		strcat(str,"\1n\r\n");
		sprintf(str2,YourHand,h,str);
		if(!symbols)
			strip_symbols(str2);
		bputs(str2);
		sprintf(str2,UserHand,user_name,h,str);
		putallnodemsg(str2); }

	nextplayer();
	if(!lastplayer()) {
		#if DEBUG
		debugline("lastplayer waitturn");
		#endif
		waitturn();
		nextplayer(); }
	#if DEBUG
	debugline("end of loop");
	#endif
	getnodemsg(); }

getgamedat(1);
node[node_num-1]=0;
putgamedat();
sprintf(str,"\1n\1m\1h%s \1n\1m%s\r\n",user_name,left());
putallnodemsg(str);
}

/****************************************************************************/
/* This function returns a static string that describes the status byte 	*/
/****************************************************************************/
char *activity(char status_type)
{
	static char str[50];

switch(status_type) {
	case BET:
		strcpy(str,"betting");
		break;
	case WAIT:
		strcpy(str,"waiting for turn");
		break;
	case PLAY:
		strcpy(str,"playing");
		break;
	case SYNC_P:
		strcpy(str,"synchronizing");
		break;
	case SYNC_D:
		strcpy(str,"synchronizing (dealer)");
        break;
	default:
		strcat(str,"UNKNOWN");
		break; }
return(str);
}

/****************************************************************************/
/* This function returns the string that represents a playing card. 		*/
/****************************************************************************/
char *cardstr(card_t card)
{
	static char str[20];
	char tmp[20];

strcpy(str,"\1n\0017"); /* card color - background always white */
if(card.suit==H || card.suit==D)
	strcat(str,"\1r");  /* hearts and diamonds - foreground red */
else
	strcat(str,"\1k");  /* spades and clubs - foreground black */
if(card.value>10)	/* face card */
	switch(card.value) {
		case J:
			strcat(str,"J");
			break;
		case Q:
			strcat(str,"Q");
			break;
		case K:
			strcat(str,"K");
			break;
		case A:
			strcat(str,"A");
			break; }
else {
	sprintf(tmp,"%d",card.value);
	strcat(str,tmp); }
switch(card.suit) {  /* suit */
	case H:
		strcat(str,"\3");
		break;
	case D:
		strcat(str,"\4");
		break;
	case C:
		strcat(str,"\5");
		break;
	case S:
		strcat(str,"\6");
		break; }
strcat(str,"\1n");
return(str);
}


/****************************************************************************/
/* This function returns the best value of a given hand.					*/
/****************************************************************************/
char hand(card_t card[MAX_CARDS],char count)
{
	char c,total=0,ace=0;

for(c=0;c<count;c++) {
	if(card[c].value==A) {		/* Ace */
		if(total+11>21)
			total++;
		else {
			ace++;
			total+=11; } }
	else if(card[c].value>=J)	/* Jack, Queen, King */
		total+=10;
	else						/* Number cards */
		total+=card[c].value; }
while(total>21 && ace) { /* ace is low if bust */
	total-=10;
	ace--; }
return(total);
}

/****************************************************************************/
/* This function returns number of soft aces in a given hand				*/
/****************************************************************************/
char soft(card_t card[MAX_CARDS],char count)
{
	char c,total=0,ace=0;

for(c=0;c<count;c++) {
	if(card[c].value==A) {		/* Ace */
		if(total+11>21)
			total++;
		else {
			ace++;
			total+=11; } }
	else if(card[c].value>=J)	/* Jack, Queen, King */
		total+=10;
	else						/* Number cards */
		total+=card[c].value; }
while(total>21 && ace) { /* ace is low if bust */
	total-=10;
	ace--; }
return(ace);
}

/****************************************************************************/
/* This function returns card that is paired in the hand					*/
/****************************************************************************/
char pair(card_t card[MAX_CARDS],char count)
{
	char c,d;

for(c=0;c<count;c++)
	for(d=c+1;d<count;d++)
		if(card[c].value==card[d].value)	   /* Ace */
			return(card[c].value);
return(0);
}


/****************************************************************************/
/* This function shuffles the deck. 										*/
/****************************************************************************/
void shuffle()
{
	char str[81];
	uint i,j;
	card_t shufdeck[52*MAX_DECKS];


getcarddat();

sprintf(str,"\1_\1w\1h\r\nShuffling %d Deck Shoe...",total_decks);
bputs(str);
strcat(str,"\r\n");     /* add crlf for other nodes */
putallnodemsg(str);

for(i=0;i<total_decks;i++)
	memcpy(shufdeck+(i*52),newdeck,sizeof(newdeck));	  /* fresh decks */

i=0;
while(i<(uint)(total_decks*52)-1) {
	j=my_random((total_decks*52)-1);
	if(!shufdeck[j].value)	/* card already used */
		continue;
	card[i]=shufdeck[j];
	shufdeck[j].value=0;	/* mark card as used */
	i++; }

cur_card=0;
for(i=0;i<MAX_HANDS;i++)
	pc[i]=0;
hands=0;
dc=0;
putcarddat();
bputs("\r\n");
}

/****************************************************************************/
/* This function reads and displays a message waiting for this node, if 	*/
/* there is one.															*/
/****************************************************************************/
void getnodemsg()
{
	char str[81], *buf;
	int file;
	ulong length;

nodesync();
sprintf(str,"message.%d",node_num);
if(flength(str)<1L) 					/* v1.02 fix */
	return;
if((file=nopen(str,O_RDWR))==-1) {
	bprintf("Couldn't open %s\r\n",str);
	return; }
length=filelength(file);
if((buf=malloc(length+1L))==NULL) {
	close(file);
	bprintf("\7\r\ngetnodemsg: Error allocating %lu bytes of memory for %\r\n"
		,length+1L,str);
	return; }
buf[read(file,buf,length)]=0;
chsize(file,0);
close(file);
if(!symbols)
	strip_symbols(buf);
bputs(buf);
free(buf);
}

/****************************************************************************/
/* This function creates a message for a certain node.						*/
/****************************************************************************/
void putnodemsg(char *msg, uint nodenumber)
{
	char str[81];
	int file;

sprintf(str,"message.%d",nodenumber);
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
	bprintf("\r\n\7putnodemsg: error opening/creating %s\r\n",str);
	return; }
write(file,msg,strlen(msg));
close(file);
}

/****************************************************************************/
/* This function creates a message for all nodes in the game.				*/
/****************************************************************************/
void putallnodemsg(char *msg)
{
	int i;

for(i=0;i<total_nodes;i++)
	if(node[i] && i+1!=node_num)
		putnodemsg(msg,i+1);
}

/****************************************************************************/
/* This function waits until it is the current player.						*/
/****************************************************************************/
void waitturn()
{
	time_t start,now;

start=now=time(NULL);
getgamedat(1);
status[node_num-1]=WAIT;
putgamedat();
while(curplayer!=node_num) {
	chat();
	mswait(100);
	getgamedat(0);
	if(curplayer && !node[curplayer-1] /*  || status[curplayer-1]==BET */ )
		nextplayer();		/* current player is not playing? */

	if(!node[node_num-1]) { /* current node not in game? */
		getgamedat(1);
		node[node_num-1]=user_number;	/* fix it */
		putgamedat(); }

	now=time(NULL);
	if(now-start>300) { /* only wait upto 5 minutes */
		bputs("\r\nwaitturn: timeout\r\n");
		break; } }
getgamedat(1);
status[node_num-1]=PLAY;
putgamedat();
}

/****************************************************************************/
/* This is the function that is called to see if the user has hit a key,	*/
/* and if so, read in a line of chars and send them to the other nodes. 	*/
/****************************************************************************/
void chat()
{
	char str1[150],str2[256],ch;
	int i;

aborted=0;
if((ch=inkey(0))!=0 || wordwrap[0]) {
	if(ch=='/') {
		bputs("\1n\1y\1hCommand: \1n");
		ch=getkeys("?LS|%\r",0);
		switch(ch) {
			case CR:
				return;
			#if DEBUG
			case '|':
				debug();
				return;
			#endif
			case '%':
				if(!com_port)	/* only if local */
					exit(0);
				break;
			case '?':
				mnemonics("\r\n~List Players");
				mnemonics("\r\n~Send Private Message to Player");
				bputs("\r\n");
				return;
			case 'L':
				listplayers();
				bprintf(ShoeStatus,cur_card,total_decks*52);
				return;
			case 'S':
				listplayers();
				bputs("\1n\r\n\1y\1hWhich node: \1n");
				i=getnum(sys_nodes);
				getgamedat(0);
				if(i>0 && i!=node_num && node[i-1]) {
					bputs("\r\n\1n\1y\1hMessage: ");
					if(getstr(str1,50,K_LINE)) {
						sprintf(str2,UserWhispers,user_name
							,str1);
						putnodemsg(str2,i); } }
				else
					bputs("\1n\r\n\1r\1hInvalid node.\1n\r\n");
				return; } }
	ungetkey(ch);
	if(!getstr(str1,50,K_CHAT|K_WRAP))
		return;
	sprintf(str2,UserSays,user_name,str1);
	putallnodemsg(str2); }
getnodemsg();
}

/****************************************************************************/
/* This function returns 1 if the current node is the highest (or last) 	*/
/* node in the game, or 0 if there is another node with a higher number 	*/
/* in the game. Used to determine if this node is to perform the dealer   */
/* function 															  */
/****************************************************************************/
char lastplayer()
{
	int i;

getgamedat(0);
if(total_players==1 && node[node_num-1])	/* if only player, definetly */
	return(1);								/* the last */

for(i=node_num;i<total_nodes;i++)			  /* look for a higher number */
	if(node[i])
		break;
if(i<total_nodes)							  /* if one found, return 0 */
	return(0);
return(1);									/* else return 1 */
}

/****************************************************************************/
/* Returns the node number of the lower player in the game					*/
/****************************************************************************/
char firstplayer()
{
	int i;

for(i=0;i<total_nodes;i++)
	if(node[i])
		break;
if(i==total_nodes)
	return(0);
return(i+1);
}

/****************************************************************************/
/* This function is only run on the highest node number in the game. It 	*/
/* waits until all other nodes are waiting in their sync routines, and then */
/* releases them by changing the status byte from SYNC_P to PLAY			*/
/* it is assumed that getgamedat(1) is called immediately prior.			*/
/****************************************************************************/
void syncdealer()
{
	char *Dealing="\1n\1hDealing...\r\n\1n";
	int i;
	time_t start,now;

status[node_num-1]=SYNC_D;
putgamedat();
start=now=time(NULL);
// fdelay(1000);				 /* wait for stragglers to join game v1.02 */
getgamedat(0);
while(total_players) {
	for(i=0;i<total_nodes;i++)
		if(i!=node_num-1 && node[i] && status[i]!=SYNC_P)
			break;
	if(i==total_nodes)		  /* all player nodes are waiting */
		break;
	chat();
	mswait(100);
	getgamedat(0);
	if(!node[node_num-1]) { /* current node not in game? */
		getgamedat(1);
		node[node_num-1]=user_number;	/* fix it */
		putgamedat(); }
	now=time(NULL);
	if(now-start>300) { /* only wait upto 5 minutes */
		bputs("\r\nsyncdealer: timeout\r\n");
        break; } }

getgamedat(1);
misc|=INPLAY;
curplayer=firstplayer();
putgamedat();

getnodemsg();
bputs(Dealing);
putallnodemsg(Dealing);

getgamedat(1);
for(i=0;i<total_nodes;i++)		  /* release player nodes */
	if(node[i])
		status[i]=PLAY;
putgamedat();
}


/****************************************************************************/
/* This function halts this node until the dealer releases it by changing	*/
/* the status byte from SYNC_P to PLAY										*/
/* it is assumed that getgamedat(1) is called immediately prior.			*/
/****************************************************************************/
void syncplayer()
{
	time_t start,now;

status[node_num-1]=SYNC_P;
putgamedat();
start=now=time(NULL);
while(node[node_num-1] && status[node_num-1]==SYNC_P) {
	chat();
	mswait(100);
	getgamedat(0);
	if(!node[node_num-1]) { /* current node not in game? */
		getgamedat(1);
		node[node_num-1]=user_number;	/* fix it */
		putgamedat(); }
	now=time(NULL);
	if(now-start>300) { /* only wait upto 5 minutes */
		bputs("\r\nsyncplayer: timeout\r\n");
        break; } }
}

/****************************************************************************/
/* Updates the MODUSER.DAT file that SBBS reads to ajust the user's credits */
/* This function is called whenever the user's credits are adjust so that   */
/* the file will be current in any event.									*/
/****************************************************************************/
void moduserdat()
{
	char str[128];
	FILE *stream;

sprintf(str,"%sMODUSER.DAT",node_dir);
if((stream=fopen(str,"wt"))==NULL) {
	bprintf("Error opening %s for write\r\n",str);
	return; }
fprintf(stream,"%ld",credits-user_cdt);
fclose(stream);

}

/****************************************************************************/
/* This function reads the entire shoe of cards and the dealer's hand from  */
/* the card database file (CARD.DAB)										*/
/****************************************************************************/
void getcarddat()
{
	int file;

if((file=nopen("card.dab",O_RDONLY))==-1) {
	bputs("getcarddat: Error opening card.dab\r\n");
	return; }
read(file,&dc,1);
read(file,dealer,sizeof(dealer));
read(file,&total_decks,1);
read(file,&cur_card,2);
read(file,card,total_decks*52*sizeof(card_t));
close(file);
}

/****************************************************************************/
/* This function writes the entire shoe of cards and the dealer's hand to   */
/* the card database file (CARD.DAB)										*/
/****************************************************************************/
void putcarddat()
{
	int file;

if((file=nopen("card.dab",O_WRONLY|O_CREAT))==-1) {
	bputs("putcarddat: Error opening card.dab\r\n");
	return; }
write(file,&dc,1);
write(file,dealer,sizeof(dealer));
write(file,&total_decks,1);
write(file,&cur_card,2);
write(file,card,total_decks*52*sizeof(card_t));
close(file);
}

/****************************************************************************/
/* This function creates random ways to say "hit"                           */
/****************************************************************************/
char *hit()
{
	static char str[81];

strcpy(str,"\1n\1r\1h");
switch(rand()%10) {
	case 1:
		strcat(str,"Hit it.");
		break;
	case 2:
		strcat(str,"Hit me, Baby!");
		break;
	case 3:
		strcat(str,"Give me an ace.");
		break;
	case 4:
		strcat(str,"One more.");
		break;
	case 5:
		strcat(str,"Just one more.");
		break;
	case 6:
		strcat(str,"Give me a baby card.");
		break;
	case 7:
		strcat(str,"Hit it, Dude.");
		break;
	case 8:
		strcat(str,"Hit.");
		break;
	case 9:
		strcat(str,"Um... Hit.");
		break;
	case 10:
		strcat(str,"Thank you Sir, may I have another.");
		break;
	default:
		strcat(str,"Face card, please.");
		break; }
strcat(str,"\1n\r\n");
return(str);
}

/****************************************************************************/
/* This function creates random ways to say "double"                        */
/****************************************************************************/
char *doubit()
{
	static char str[81];

strcpy(str,"\1n\1b\1h");
switch(rand()%10) {
	case 1:
		strcat(str,"Double.");
		break;
	case 2:
		strcat(str,"Double Down, Man.");
		break;
	case 3:
		strcat(str,"Double it, Dude.");
		break;
	case 4:
		strcat(str,"One more card for double the dough.");
		break;
	case 5:
		strcat(str,"Double me.");
		break;
	case 6:
		strcat(str,"Oh yeah... Double!");
		break;
	case 7:
		strcat(str,"I shouldn't do it, but... Double!");
		break;
	case 8:
		strcat(str,"Double my bet and give me one more card.");
		break;
	case 9:
		strcat(str,"Um... Double.");
		break;
	case 10:
		strcat(str,"Thank you Sir, may I Double?");
		break;
	default:
		strcat(str,"Double - face card, please.");
		break; }
strcat(str,"\1n\r\n");
return(str);
}

/****************************************************************************/
/* This function creates random ways to say "stand"                         */
/****************************************************************************/
char *stand()
{
	static char str[81];

strcpy(str,"\1n\1c\1h");
switch(rand()%10) {
	case 1:
		strcat(str,"Stand.");
		break;
	case 2:
		strcat(str,"Stay.");
		break;
	case 3:
		strcat(str,"No more.");
		break;
	case 4:
		strcat(str,"Just right.");
		break;
	case 5:
		strcat(str,"I should hit, but I'm not gonna.");
		break;
	case 6:
		strcat(str,"Whoa!");
		break;
	case 7:
		strcat(str,"Hold it.");
		break;
	case 8:
		strcat(str,"No way, Jose!");
		break;
	case 9:
		strcat(str,"Um... Stand.");
		break;
	case 10:
		strcat(str,"Thanks, but no thanks.");
		break;
	default:
		strcat(str,"No card, no bust.");
		break; }
strcat(str,"\1n\r\n");
return(str);
}

/****************************************************************************/
/* This function creates random ways to say "split"                         */
/****************************************************************************/
char *split()
{
	static char str[81];

strcpy(str,"\1n\1y\1h");
switch(rand()%10) {
	case 1:
		strcat(str,"Split.");
		break;
	case 2:
		strcat(str,"Split 'em.");
		break;
	case 3:
		strcat(str,"Split it.");
		break;
	case 4:
		strcat(str,"Split, please.");
		break;
	case 5:
		strcat(str,"I should hit, but I'm gonna split instead.");
		break;
	case 6:
		strcat(str,"Whoa! Split them puppies...");
		break;
	case 7:
		strcat(str,"Split 'em, Dude.");
		break;
	case 8:
		strcat(str,"Double the cards, for double the money.");
		break;
	case 9:
		strcat(str,"Um... Split.");
		break;
	case 10:
		strcat(str,"Thank you Sir, I think I'll split 'em.");
		break;
	default:
		strcat(str,"Banana Split.");
		break; }
strcat(str,"\1n\r\n");
return(str);
}

/****************************************************************************/
/* This function creates random ways to say "joined"                        */
/****************************************************************************/
char *joined()
{
	static char str[81];

switch(rand()%10) {
	case 1:
		strcpy(str,"joined.");
		break;
	case 2:
		strcpy(str,"sat down to play.");
		break;
	case 3:
		strcpy(str,"plopped on the chair next to you.");
		break;
	case 4:
		strcpy(str,"belched loudly to announce his entrance.");
		break;
	case 5:
		strcpy(str,"dropped in.");
		break;
	case 6:
		strcpy(str,"joined our game.");
		break;
	case 7:
		strcpy(str,"fell on his face entering the casino!");
		break;
	case 8:
		strcpy(str,"slams a roll of credits on the table.");
		break;
	case 9:
		strcpy(str,"rolled in to join the game.");
		break;
	case 10:
		strcpy(str,"smiles widely as he takes your wife's seat.");
		break;
	default:
		strcpy(str,"spills a drink on your pants while sitting down.");
		break; }
return(str);
}

/****************************************************************************/
/* This function creates random ways to say "left"                          */
/****************************************************************************/
char *left()
{
	static char str[81];

switch(rand()%10) {
	case 1:
		strcpy(str,"left abruptly.");
		break;
	case 2:
		strcpy(str,"sneaked away.");
		break;
	case 3:
		strcpy(str,"took the credits and ran.");
		break;
	case 4:
		strcpy(str,"fell out of the chair.");
		break;
	case 5:
		strcpy(str,"left the game.");
		break;
	case 6:
		strcpy(str,"slipped out the door.");
		break;
	case 7:
		strcpy(str,"giggled as he left the table.");
		break;
	case 8:
		strcpy(str,"left clenching empty pockets.");
		break;
	case 9:
		strcpy(str,"went to the pawn shop to hawk a watch.");
		break;
	case 10:
		strcpy(str,"bailed out the back door.");
		break;
	default:
		strcpy(str,"made like a train and left.");
		break; }
return(str);
}

/****************************************************************************/
/* This function creates the file "GAME.DAB" in the current directory.      */
/****************************************************************************/
void create_gamedab()
{

if((gamedab=sopen("game.dab"
	,O_WRONLY|O_CREAT|O_BINARY,SH_DENYNO))==-1) {
	bputs("Error creating game.dab\r\n");
	pause();
	exit(1); }
misc=0;
curplayer=0;
memset(node,0,sizeof(node));
memset(status,0,sizeof(status));
write(gamedab,&misc,1);
write(gamedab,&curplayer,1);
write(gamedab,&total_nodes,1);
write(gamedab,node,total_nodes*2);
write(gamedab,status,total_nodes);
close(gamedab);
}

/****************************************************************************/
/* This function opens the file "GAME.DAB" in the current directory and     */
/* leaves it open with deny none access. This file uses record locking		*/
/* for shared access.														*/
/****************************************************************************/
void open_gamedab()
{
if((gamedab=sopen("game.dab",O_RDWR|O_BINARY,SH_DENYNO))==-1) {
    bputs("Error opening game.dab\r\n");                /* open deny none */
    pause();
	exit(1); }
}

/****************************************************************************/
/* Lists the players currently in the game and the status of the shoe.		*/
/****************************************************************************/
void listplayers()
{
	int i;

getgamedat(0);
bputs("\r\n");
if(!total_players) {
	bputs("\1_\1w\1hNo game in progress\r\n");
	return; }
for(i=0;i<total_nodes;i++)
	if(node[i])
		bprintf("\1-\1mNode %2d: \1h%s \1n\1m%s\r\n"
			,i+1,username(node[i]),activity(status[i]));
getcarddat();
/***
bprintf("\r\nCurrent player=Node %d user #%d\r\n",curplayer,node[curplayer-1]);
***/
}

/****************************************************************************/
/* This function replaces the card symbols in 'str' with letters to         */
/* represent the different suits.											*/
/****************************************************************************/
void strip_symbols(char *str)
{
	int i,j;

j=strlen(str);
for(i=0;i<j;i++)
	if(str[i]>=3 && str[i]<=6)
		switch(str[i]) {
			case 3:
				str[i]='H';
				break;
			case 4:
				str[i]='D';
				break;
			case 5:
				str[i]='C';
				break;
			case 6:
				str[i]='S';
				break; }
}

#endif	/* end of function not needed for SBJCLEAN.C */

/****************************************************************************/
/* Reads information from GAME.DAB file. If 'lockit' is 1, the file is      */
/* and putgamedat must be called to unlock it. If your updating the info	*/
/* in GAME.DAB, you must first call getgamedat(1), then putgamedat().		*/
/****************************************************************************/
void getgamedat(char lockit)
{
    int i=0;

/* retry 100 times taking at least 3 seconds */
while(lock(gamedab,0L,filelength(gamedab))==-1 && i++<100)
	mswait(30);  /* lock the whole thing */
if(i>=100) {
//	  printf("gamedab=%d %04X:%p %04X\r\n",gamedab,_psp,&gamedab,_DS);
	printf("\7getgamedat: error locking game.dab\r\n"); }

lseek(gamedab,0L,SEEK_SET);
read(gamedab,&misc,1);
read(gamedab,&curplayer,1);
read(gamedab,&total_nodes,1);
read(gamedab,node,total_nodes*2);	   /* user number playing for each node */
read(gamedab,status,total_nodes);	   /* the status of the player */
total_players=0;
for(i=0;i<total_nodes;i++)
    if(node[i])
        total_players++;
if(!lockit)
	unlock(gamedab,0L,filelength(gamedab));
}

/****************************************************************************/
/* Writes information to GAME.DAB file. getgamedat(1) MUST be called before  */
/* this function is called.                                                 */
/****************************************************************************/
void putgamedat()
{

lseek(gamedab,0L,SEEK_SET);
write(gamedab,&misc,1);
write(gamedab,&curplayer,1);
write(gamedab,&total_nodes,1);
write(gamedab,node,total_nodes*2);
write(gamedab,status,total_nodes);
unlock(gamedab,0L,filelength(gamedab));
}

/***************************************************************/
/* This function makes the next active node the current player */
/***************************************************************/
void nextplayer()
{
    int i;

getgamedat(1);                      /* get current info and lock */

if((!curplayer						/* if no current player */
    || total_players==1)            /* or only one player in game */
    && node[node_num-1]) {          /* and this node is in the game */
	curplayer=node_num; 			/* make this node current player */
    putgamedat();                   /* write data and unlock */
    return; }                       /* and return */

for(i=curplayer;i<total_nodes;i++)	/* search for the next highest node */
    if(node[i])                     /* that is active */
        break;
if(i>=total_nodes) {				/* if no higher active nodes, */
	for(i=0;i<curplayer-1;i++)		/* start at bottom and go up  */
        if(node[i])
            break;
	if(i==curplayer-1)				/* if no active nodes found */
		curplayer=0;				/* make current player 0 */
    else
		curplayer=i+1; }
else
	curplayer=i+1;					/* else active node current player */

putgamedat();                       /* write info and unlock */
}

/* End of SBJ.C */
