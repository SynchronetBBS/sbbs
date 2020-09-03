/* $Id: dpoker.c,v 1.17 2019/08/13 20:09:01 rswindell Exp $ */

/******************************************************************************
  DPOKER.EXE: Domain Poker online multi-player poker BBS door game for
  Synchronet (and other) BBS software.
  Copyright (C) 1992-2007 Allen Christiansen DBA Domain Entertainment.

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation; either version 2 of the License, or (at your option) any later
  version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 Temple
  Place - Suite 330, Boston, MA  02111-1307, USA.

  E-Mail      : door.games@domainentertainment.com
  Snail Mail  : Domain Entertainment
                PO BOX 54452
                Irvine, CA 92619-4452
******************************************************************************/

#include "ini_file.h"
#include "xsdk.h"
#define VERSION "2K7"
char	revision[16];
char	compiler[32];

#define J 11    /* jack */
#define Q 12    /* queen */
#define K 13    /* king */
#define A 14    /* ace */

#define H 0     /* heart */
#define D 1     /* diamond */
#define C 2     /* club */
#define S 3     /* spade */

#define MAX_PLAYERS 6
#define MAX_TABLES 25

#define COMPUTER    (1<<0)
#define PASSWORD    (1<<1)

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

card_t deck[52], card[52];
card_t hand[MAX_NODES+1][10];
char comp_name[41],password[10]={0};
int cur_card=0,rank=0,last_bet=100,create_log=0,symbols=1,table=1,comp_stat=0,
    player_dis=0,xraised=0;
    /* player_dis tells the computer how many the player threw out */
short cur_table=0,dupcard1=0,dupcard2=0,hicard=0,total_players,cur_node,
      firstbid=0,stage;
short num_tables=0,skim=0,bet_limit[MAX_TABLES+1]={0},
    ante[MAX_TABLES+1]={0},options[MAX_TABLES+1]={0};
uchar node[MAX_NODES+1]={0};
ulong player_bet=0,newpts,temppts,current_bet,total_pot,comp_bet=0,
    time_played=0,time_allowed=0,max_bet=0,max_total[MAX_TABLES+1]={0};
uint firstbet=1;
unsigned short temp_num,dealer;

long xfer_pts2=0;

/*******************
 Function Prototypes
********************/
int main(int argc, char **argv);
void get_player(int player);
void get_game_status(int tablenum);
void put_game_status(int specnode);
void send_all_message(char *msg,int exception);
void send_player_message(int plr, char *msg);
void quit_out(void);

#ifndef DPCLEAN

void put_player(int player);
int straight_flush(void);
void center_wargs(char *str, ...);
void moduser(void);
void deal(void);
void discard(void);
void computer_discard(void);
void join_game(void);
int select_table(void);
void increase_bet(void);
void computer_bet(void);
void bet_menu(void);
void play_menu(void);
void putdeck(void);
void getdeck(void);
void shuffle(void);
void show_all_hands(int winner);
void player_status(void);
char *handstr(int player);
char *cardstr(card_t card);
char *ranking(int num);
char *activity(int num);
char *activity2(int num);
void single_winner(void);
void dealer_ctrl(void);
int hand_total(int player);
void gethandtype(int player);
void read_player_message(void);
void strip_symbols(char *str);
void readline(char *outstr, int maxlen, FILE *stream);
void show_tables(void);
void show_players(int tablenum);
void next_player(void);
void send_message(void);
void computer_log(long points);
void declare_winner(void);

/* These are the hand types */
enum { NOTHING, ONE_PAIR, TWO_PAIR, THREE_OF_A_KIND, STRAIGHT, FLUSH,
       FULL_HOUSE, FOUR_OF_A_KIND, STRAIGHT_FLUSH, ROYAL_FLUSH };

/* These are for the player status */
enum { INACTIVE, WAITING, BETTING, DISCARDING, FOLDED, DEALING };

/* These or for determining what stage the game is in */
enum { OPEN, DEAL, BET, DISCARD, BET2, GET_WINNER };

#ifdef _WIN32	/* necessary for compatibility with SBBS v2 */
#pragma pack(push)
#pragma pack(1)
#endif

#ifdef __GNUC__ 
	#define _PACK __attribute__ ((packed))
#else
	#define _PACK
#endif

int main(int argc, char **argv)
{
    char	ch,str[256],*buf;
	char*	p;
    int		file,i,x;
    long	won,lost;
    ulong	length,l,lastrun;
	FILE	*inifile;
	int		opts;
	char	key_name[8];
	BOOL	cleanup=FALSE;
	char	*env_node;
    struct _PACK {
        char name[25];
        ulong time;
        long points;
    } player_stuff;

	sscanf("$Revision: 1.17 $", "%*s %s", revision);
	DESCRIBE_COMPILER(compiler);

    memset(node,'\0',MAX_NODES);

	/* ToDo... this should seed better */
    srand(time(NULL));
	env_node=getenv("SBBSNODE");
	if(env_node==NULL) {
		printf("\nDomain Poker v%s-%s/XSDK v%s  Copyright %s Domain "
				"Entertainment\n",VERSION,revision,xsdk_ver,__DATE__+7);
		printf("\nERROR: SBBSNODE Environment variable not set!\n");
		return(1);
	}
    sprintf(node_dir,"%s",env_node);
	backslash(node_dir);

    for (x=1; x<argc; x++) {
		p=argv[x];
		while(*p=='/' || *p=='-')
			p++;
        if(stricmp(p,"L")==0)
            create_log=TRUE;
		else if(stricmp(p,"clean")==0)
			cleanup=TRUE;
		else {
			printf("\nDomain Poker v%s-%s/XSDK v%s  Copyright %s Domain "
					"Entertainment\n",VERSION,revision,xsdk_ver,__DATE__+7);
			printf("Compiler: %s\n", compiler);
			printf("\nUsage: dpoker [-L] [-CLEAN]\n");
			printf("\nOptions:\n");
			printf("\tL = Create daily log of computer vs player "
				  "wins/losses\n");
			printf("\tCLEAN = Clean-up player/node data (replaces 'dpclean')\n");
			return(0); 
		}
    }
    initdata();

	if(cleanup) {
		sprintf(str,"player.%d",node_num);
		if (fexist(str)) {
			get_player(node_num);
			newpts=temppts;
			quit_out();
		}
		return(0);
	}
    newpts=user_cdt;

    if(!(fexist("dpoker.mnt"))) {
        if((file=nopen("dpoker.mnt",O_WRONLY|O_CREAT))!=-1) {
            bprintf("\r\nPLEASE WAIT: Running daily maintenance.\r\n");
            lastrun=time(NULL);
			lastrun-=lastrun%86400;
            write(file,&lastrun,sizeof(lastrun)); close(file);
            delfiles(".","player.*", /* keep: */0);
            delfiles(".","gamestat.*", /* keep: */0);
            delfiles(".","deck.*", /* keep: */0);
            delfiles(".","message.*", /* keep: */0);
            unlink("dpoker.plr");
			close(file);
		}
	}
/* ToDo...
    if((file=nopen("dpoker.mnt",O_RDWR|O_DENYALL))==-1) { */
    if((file=nopen("dpoker.mnt",O_RDWR))==-1) {
        bprintf("\r\nPLEASE WAIT: Daily maintenance is running.\r\n");
        mswait(10000);
    } else {
        read(file,&lastrun,sizeof(lastrun));
        if(time(NULL)-lastrun>=86400) {
            bprintf("\r\nPLEASE WAIT: Running daily maintenance.\r\n");
            lastrun=time(NULL); lseek(file,0L,SEEK_SET);
			lastrun-=lastrun%86400;
            write(file,&lastrun,sizeof(lastrun));
            delfiles(".","player.*", /* keep: */0);
            delfiles(".","gamestat.*", /* keep: */0);
            delfiles(".","deck.*", /* keep: */0);
            delfiles(".","message.*", /* keep: */0);
            unlink("dpoker.plr");
		}
        close(file);
	}

	inifile=fopen("dpoker.ini","r");
	iniReadString(inifile,NULL,"ComputerName","King Drafus",comp_name);
	num_tables=iniReadShortInt(inifile,NULL,"Tables",5);
	if(num_tables>MAX_TABLES)
		num_tables=MAX_TABLES;
	if(num_tables<1)
		num_tables=1;
	time_allowed=iniReadInteger(inifile,NULL,"TimeAllowed",86400);
	if(time_allowed>86400 || time_allowed<0)
		time_allowed=86400;
	skim=iniReadShortInt(inifile,NULL,"Skim",10);
	if(skim<0)
		skim=0;
	if(skim>50)
		skim=50;
	for(i=1;i<=num_tables;i++) {
		sprintf(key_name,"Table%d",i);
		opts=0;
		opts|=(iniReadBool(inifile,key_name,"ComputerPlayer",TRUE)?COMPUTER:0);
		opts|=(iniReadBool(inifile,key_name,"Password",FALSE)?PASSWORD:0);
		options[i]=opts;
		ante[i]=iniReadShortInt(inifile,key_name,"Ante",i*50);
		if(ante[i] < 0)
			ante[i]=0;
		bet_limit[i]=iniReadShortInt(inifile,key_name,"BetLimit",i*100);
		if(bet_limit[i] < 1)
			bet_limit[i]=1;
		max_total[i]=iniReadInteger(inifile,key_name,"TableLimit",i*1000);
		if(max_total[i] < 1)
			max_total[i]=1;
	}
	if(inifile!=NULL)
		fclose(inifile);

    if (comp_name[0]==0)
        strcpy(comp_name,sys_guru);

    if(fexist("dpoker.plr")) {
        if((file=nopen("dpoker.plr",O_RDONLY))==-1) {
            printf("Error opening time file\r\n");
            pause();
            return(1);}
        length=filelength(file);
        if ((buf=(char *)malloc(length))!=NULL) {
            read(file,buf,length);
        }
        close(file);
        for (l=0;l<length;l+=sizeof(player_stuff)) {
            sprintf(player_stuff.name,"%.25s",buf+l);
            player_stuff.time=*(ulong *)(buf+l+25);
            player_stuff.points=*(long *)(buf+l+29);
            truncsp(player_stuff.name);
            if (!stricmp(player_stuff.name,user_name))
                time_played+=(player_stuff.time*60);
        }
        free(buf);
    }


    for (x=1;x<=num_tables;x++) {
        if (bet_limit[x]<=0 || bet_limit[x]>9999) bet_limit[x]=100;
        if (max_total[x]<bet_limit[x] || max_total[x]>99999)
            max_total[x]=bet_limit[x]*2;
        if (ante[x]<=0 || ante[x]>bet_limit[x]) ante[x]=bet_limit[x];
    }

    while(inkey(0));	/* clear input buffer */
    putchar(5); /* ctrl-e */
    mswait(500);
    if(keyhit()) {
        while(inkey(0));
        cls(); bputs("\r\n"); center_wargs("\1r\1h\1i*** ATTENTION ***");
        bputs("\r\n\1n\1hDomain Poker \1nuses Ctrl-E (ENQ) for the 'club' card "
              "symbol.");
        bputs("\r\nYour terminal responded to this control character with an "
              "answerback string.");
        bputs("\r\nIf you have any problems with missing characters or "
              "incomplete strings\r\nwhile playing the game, you will probably "
              "need to either disable all Ctrl-E ");
        bputs("\r\n(ENQ) answerback strings (Including Compuserve Quick B "
              "transfers) or toggle\r\ncard symbols OFF.\r\n\r\n");
        pause();
    }
    cls();
    center_wargs("\1n\1gWelcome to Domain Poker v%s-%s",VERSION,revision);
    center_wargs("\1n\1gCopyright %s Domain Entertainment", __DATE__+7);

    do {
        mnehigh=GREEN|HIGH; mnelow=GREEN;
        mnemonics("\r\n\r\n~How to play Domain Poker\r\n~Instructions\r\n"
                  "~Join a card game\r\n");
        mnemonics("~List players\r\n~Toggle card symbols ON/OFF\r\n");
        mnemonics("~Player log of the day\r\n~Quit back to ");
        bprintf("\1y\1h%s\1n",sys_name);
        bprintf("\r\n"); nodesync();
        bprintf("\r\n\1m\1hCommand:\1n ");
		ch=getkey(K_UPPER);
        bprintf("%c\r\n",ch);

		switch(ch) {
			case 'Q':
				break;
            case 'P':
                if (!fexist("dpoker.plr")) {
                    bprintf("\r\n\1c\1hNo one has played Domain Poker today.");
                    break;
                }
                won=lost=0L;
                bprintf("\r\n\1y\1hList of today's players\1n\r\n");
                if((file=nopen("dpoker.plr",O_RDONLY))!=-1) {
                    length=filelength(file);
                    if ((buf=(char *)malloc(length))!=NULL) {
                        read(file,buf,length);
                    }
                    close(file);
                    for (l=0;l<length;l+=sizeof(player_stuff)) {
                        sprintf(player_stuff.name,"%.25s",buf+l);
                        player_stuff.time=*(ulong *)(buf+l+25);
                        player_stuff.points=*(long *)(buf+l+29);
                        if (player_stuff.points>0) strcpy(str,"\1m\1hWON!");
                        else strcpy(str,"\1y\1hLOST");
                        bprintf("\r\n\1b\1h%-25.25s          %s \1c\1h%8ldK \1b"
                                "\1hof credits.",player_stuff.name,str,
                                labs(player_stuff.points));
                        truncsp(player_stuff.name);
                        if (!stricmp(player_stuff.name,user_name)) {
                            if (player_stuff.points>0)
                                won+=player_stuff.points;
                            else
                                lost+=labs(player_stuff.points);
                        }
                    }
                    free(buf);
                    if (won || lost) {
                        bprintf("\r\n\r\n\1m\1hYou've won \1y%ldK \1mand "
                                "lost \1y%ldK \1mfor a total %s of "
                                "\1c%ldK \1mtoday.",won,lost,(lost>won) ? "loss"
                                : "win",(lost>won) ? (lost-won) : (won-lost));
                    }
                }
                break;
            case 'H':
				cls();
                printfile("dpoker.how");
                break;
			case 'I':
				cls();
                printfile("dpoker.ins");
				break;
			case 'L':
                do {
                bprintf("\r\n\1b\1hWhich table [\1c1-%d\1b][\1cT=Table list\1b]"
                        ": ",num_tables);
                x=getkeys("qt",num_tables);
                if(x&0x8000) {
                    x&=~0x8000; show_players(x); }
                if(x=='T') {
                    show_tables(); bputs("\r\n"); }
                if(x=='Q')
                    break;
                } while (x=='T');
				break;
            case 'T':
                symbols=!symbols;
                bprintf("\1_\1b\1h\r\nCard symbols now: \1c%s\r\n",symbols ?
                        "ON":"OFF");
                break;
			case 'J':
                last_bet=100;
                mnehigh=CYAN|HIGH; mnelow=CYAN;
                if (select_table()) {
                    if (newpts<=(max_total[cur_table]*1024L)) {
                        bprintf("\r\n\1r\1hYou don't have enough credits to "
                                "play that table!\1n\r\n\1b\1hThe minimum required "
                                "is \1c\1h%luK\1n",max_total[cur_table]);
                    } else join_game();
                } else break;
                if (node[node_num-1])
                    play_menu();
                break;
		}
    } while (ch!='Q');

    bprintf("\r\n\r\n\1n\1gReturning you to \1g\1h%s\r\n\r\n\1n",sys_name);
	mswait(500);
	return(0);
}

/*****************************************************************************
 This function allows the player to discard up to three cards, this happens
 after the initial bet round is complete.  This function also draws the number
 of cards from the deck that the player has discarded.
******************************************************************************/
void discard()
{
    char str[256],dishand[256];
    int dc,tdc=1,num,i;
    card_t temp[10];

    getdeck(); player_dis=0;
    get_player(node_num);
    gethandtype(node_num);
    for (num=0;num<5;num++)
        temp[num]=hand[node_num][num];
    get_game_status(cur_table); node[node_num-1]=DISCARDING;
    put_game_status(node_num-1);
    sprintf(str,"\r\n\1n\1gNode %2d: \1h%s \1n\1gis discarding cards.",node_num,
            user_name);
    send_all_message(str,0);
    lncntr=0;
    bprintf("\r\n\r\n\1y\1hDiscard Stage\r\n");
    bprintf("\r\n\r\n\r\n\r\n\r\n\r\n");
    bprintf("\x1b[6A");
    do {
        strcpy(dishand,handstr(node_num)); if(!symbols) strip_symbols(dishand);
        sprintf(str,"\r\n\t\t\t\1b\1h%s - \1b\1h(\1c%s\1b)\1n",dishand,
            ranking(rank));
        bprintf("%s\r\n\t\t\t\1b\1h(\1c1\1b) (\1c2\1b) (\1c3\1b) (\1c4\1b) "
                "(\1c5\1b)\r\n",str);
        bprintf("\r\n\1b\1hDiscard Which (\1c\1hQ when done\1b\1h): ");
        dc=getnum(5);
        if (hand[node_num][dc-1].value>0 && dc>0) {
            if (tdc>3) {
                bprintf("\r\n\1r\1hYou can only discard up to 3 cards!\1n");
                pause(); for (i=0;i<35;i++) bputs("\b \b"); bprintf("\x1b[A");
            } else { hand[node_num][dc-1].value=0; tdc++; }
        } else if (dc>0) {
            hand[node_num][dc-1]=temp[dc-1]; tdc--; }
        if (user_misc&ANSI) bprintf("\x1b[5A");
    } while (dc!=-1);

    for (num=0;num<5;num++)
        if (!hand[node_num][num].value) {
            hand[node_num][num]=deck[cur_card++];
        }

    putdeck(); put_player(node_num); gethandtype(node_num);
    strcpy(dishand,handstr(node_num)); if(!symbols) strip_symbols(dishand);
    if (user_misc&ANSI) bprintf("\x1b[A");
    bprintf("\r\n\r\n\1m\1h          You now have: %s - \1b\1h(\1c%s\1b)\1n"
            "          ",dishand,ranking(rank));
    if (user_misc&ANSI) bprintf("\r\n\t\t\t                                  ");
    player_dis=tdc-1;
    sprintf(str,"\r\n\1m\1h%s discards %d cards",user_name,tdc-1);
    send_all_message(str,node_num);
    node[node_num-1]=WAITING; put_game_status(node_num-1);
    lncntr=0;
    if (dealer!=node_num)
        next_player();
}

/********************************************************************
 This function checks for possible straight or flush of computer hand
 returns 1 if possible, 0 if not
*********************************************************************/
int straight_flush()
{
    int x,i,j,matchcard=0;

    gethandtype(0);

    /* check for a possible flush */
    x=0;
    for (i=0;i<5;i++) {
        matchcard=hand[0][i].suit; x=0;
        for (j=0;j<5;j++) {
            if (hand[0][j].suit==(matchcard)) x++;
        }
        if (x>2) return 1;      /* yes, possible flush */
    }

    /* check for a possible straight */
    x=0;                                    /* Check for a Straight */
    for (i=0;i<5;i++) {
        matchcard=hand[0][i].value; x=0; if (matchcard==14) matchcard=1;
        for (j=0;j<5;j++) {
            if (hand[0][j].value==(matchcard+1)) {
                x++; matchcard++;
            }
        }
        if (x>2) return 1;      /* yes, possible straight */
    }

    return 0;                   /* nothing */
}

/***************************************************
 This function is where the computer player discards
****************************************************/
void computer_discard()
{
    int x,i,j,match,dc=0,temp_hc=0,matchcard=0;

    getdeck(); gethandtype(0);
    bprintf("\r\n\1g\1h%s \1n\1gis discarding cards.",comp_name);

    /*******************************************************/
    /* This makes the computer discard the oddball card(s) */
    /* when the hand has a certain ranking > NOTHING       */
    /*******************************************************/
    if (rank==ONE_PAIR) {
        for (x=0;x<5;x++)
            if (hand[0][x].value!=dupcard1 && hand[0][x].value<=10) {
                hand[0][x]=deck[cur_card++]; dc++;
            }
        if (!dc)
            for (x=0;x<5;x++)
                if (hand[0][x].value!=dupcard1 && hand[0][x].value<=14) {
                    hand[0][x]=deck[cur_card++]; dc++;
                }
        bprintf("\r\n\1m\1h%s discards %d cards",comp_name,dc);
        putdeck();
        next_player(); return;
    }
    if (rank==TWO_PAIR || rank==THREE_OF_A_KIND || rank==FOUR_OF_A_KIND) {
        for (x=0;x<5;x++)
            if (hand[0][x].value!=dupcard1 && hand[0][x].value!=dupcard2) {
                hand[0][x]=deck[cur_card++]; dc++;
            }
        bprintf("\r\n\1m\1h%s discards %d cards",comp_name,dc);
        putdeck();
        next_player(); return;
    }

    /*************************************************************************/
    /* This routine will run 20% of the time if there's the possibility of a */
    /* straight or a flush (because of the low chances of getting either)    */
    /*************************************************************************/
    if (straight_flush() && !rand()%5) {
        /**********************************************************************/
        /* This checks for a possible flush and discards non-conforming cards */
        /**********************************************************************/
        x=0;
        for (i=0;i<5;i++) {
            matchcard=hand[0][i].suit; x=0;
            for (j=0;j<5;j++) {
                if (hand[0][j].suit==(matchcard)) x++;
            }
            if (x>2 && !xp_random(3)) {
                for (i=0;i<5;i++) {
                    if (hand[0][i].suit!=matchcard)
                        hand[0][i]=deck[cur_card++];
                }
                bprintf("\r\n\1m\1h%s discards %d cards.",comp_name,5-x);
                putdeck(); next_player();
                return;
            }
        }


        /*********************************************************************/
        /* This checks for a possible strt and discards non-conforming cards */
        /*********************************************************************/
        x=0;                                    /* Check for a Straight */
        for (i=0;i<5;i++) {
            matchcard=hand[0][i].value; x=0; if (matchcard==14) matchcard=1;
            for (j=0;j<5;j++) {
                if (hand[0][j].value==(matchcard+1)) {
                    x++; matchcard++;
                }
            }
            if (x>2 && !xp_random(3)) {
                for (i=0;i<5;i++) {
                    match=0;
                    for (j=0;j<x;j++)
                        if (hand[0][i].value==matchcard-j) match=1;
                    if (!match)
                        hand[0][i]=deck[cur_card++];
                }
                bprintf("\r\n\1m\1h%s discards %d cards.",comp_name,5-x);
                putdeck(); next_player();
                return;
            }
        }
        if (x<4) { x=0;                         /* Leave cards at their */
            for (i=0;i<5;i++) {                 /* normal values        */
                matchcard=hand[0][i].value; x=0;
                for (j=0;j<5;j++) {
                    if (hand[0][j].value==(matchcard-1)) {
                        x++; matchcard--;
                    }
                }
                if (x>2) {
                    for (i=0;i<5;i++) {
                        match=0;
                        for (j=0;j<x;j++)
                            if (hand[0][i].value==matchcard+j) match=1;
                        if (!match)
                            hand[0][i]=deck[cur_card++];
                    }
                    bprintf("\r\n\1m\1h%s discards %d cards.",comp_name,5-x);
                    putdeck(); next_player();
                    return;
                }
            }
        }
    }
    /***************************************************************/
    /* This makes the computer discard all but the 2 highest cards */
    /* This is the default when nothing else has been done yet     */
    /***************************************************************/
    for (x=0;x<5;x++) {
        if (hand[0][x].value!=hicard)
            if (hand[0][x].value>temp_hc)
                temp_hc=hand[0][x].value;
    }
    for (x=0;x<5;x++) {
        if (hand[0][x].value!=hicard && hand[0][x].value!=temp_hc)
            hand[0][x]=deck[cur_card++];
    }
    bprintf("\r\n\1m\1h%s discards 3 cards.",comp_name);
    putdeck();
    next_player(); return;
}
/*************************************************************************
 This is where the computer decides how and what to bet or whether to call
 or fold
**************************************************************************/
void computer_bet()
{
    int add=0,call=0,fold=0,raise=0;

    if (!firstbet && stage==BET && current_bet==0) { next_player(); return; }
    if (!firstbet && stage==BET2 && (comp_bet-current_bet==0) && firstbid) {
        bet_menu();
        if (comp_bet-current_bet!=0 || node[node_num-1]==FOLDED) {
            next_player();
            return;
        }
    }

    gethandtype(0);
    get_game_status(cur_table);
    bprintf("\r\n\1g\1h%s \1n\1gis betting.",comp_name);
    max_bet=bet_limit[cur_table];           /* set to predifined limit */
    if (max_bet>(newpts/1024L)-(current_bet-player_bet))
       max_bet=((newpts/1024L)-(current_bet-player_bet));
    /* set to lowest amount minus the minimum required */
    if (current_bet+max_bet>=max_total[cur_table])
        max_bet=max_total[cur_table]-current_bet;

    while(1) {
        /* No credits available, gotta call! */
        if (max_bet==0) {
            call=1; break;
        }
        if (stage==BET) {
            /* 20% of the time, bet different hand to confuse player */
            /* or raise if the user initial bet was <= 10% of max  & */
            /* random 33% of the time                                */
            if (!rand()%4 || (!firstbet && current_bet<=(.1
                *bet_limit[cur_table]) && !rand()%2)) {
                if (!xraised) {
                    add=(max_bet*.75)+xp_random(max_bet*.25);
                    raise=1; break;
                }
            }

            if (rank==NOTHING) {
                if (!firstbet || current_bet>0) {
                    call=1; break;
                }
                if (hicard<11) {
                    add=(max_bet*.40)+xp_random(max_bet*.20);
                    raise=1; break;
                }
                if (hicard>10) {
                    add=(max_bet*.50)+xp_random(max_bet*.25);
                    raise=1; break;
                }
            }
            if (rank==ONE_PAIR) {
                if (firstbet && xraised>=1) {
                    /* (!firstbet || current_bet>0) */
                    call=1; break;
                }
                if (dupcard1<11) {
                    add=(max_bet*.60)+xp_random(max_bet*.40);
                    raise=1; break;
                }
                if (dupcard1>10) {
                    add=(max_bet*.75)+xp_random(max_bet*.25);
                    raise=1; break;
                }
            }
            if (rank==TWO_PAIR) {
                if ((firstbet && xraised>=2) || xraised>=1) {
                    call=1; break;
                }
                if (dupcard1<11 && dupcard2<11) {
                    add=(max_bet*.80)+xp_random(max_bet*.20);
                    raise=1; break;
                }
                if (dupcard1>10 || dupcard2>10) {
                    add=(max_bet*.90)+xp_random(max_bet*.10);
                    raise=1; break;
                }
            }
            if (rank>=THREE_OF_A_KIND) {
                if ((firstbet && xraised>=3) || xraised>=2) {
                    call=1; break;
                }
                add=max_bet;
                raise=1; break;
            }
            bprintf("\r\nPROBLEM: Stage BET1, please report to SysOp!");
            pause();
        }
        if (stage==BET2) {
            if (rank==NOTHING) {
                if (firstbet) {
                    if (!rand()%5 && hicard>10) {
                        if (xraised) {
                            call=1; break;
                        } else {
                            add=(max_bet*.30)+
                                xp_random(max_bet*.10);
                            raise=1; break;
                        }
                    } else {
                        fold=1; break;
                    }
                }
                if (!firstbet) {
                    if (current_bet-comp_bet==0) {
                        call=1; xraised=1; break;
                    }
                    if (!rand()%3 && hicard>10) {
                        call=1; break;
                    } else {
                        fold=1; break;
                    }
                }
            }
            if (rank==ONE_PAIR) {
                if (!firstbet && current_bet-comp_bet==0) {
                    if (dupcard1<5) {
                        call=1; break;
                    }
                    if (dupcard1<11) {
                        add=(max_bet*.70)+xp_random(max_bet*.30);
                        raise=1; break;
                    }
                    if (dupcard1>10) {
                        add=(max_bet*.80)+xp_random(max_bet*.20);
                        raise=1; break;
                    }
                }
                if (!firstbet || xraised) {
                    call=1; break;
                }
                if (dupcard1<11) {
                    add=(max_bet*.60)+xp_random(max_bet*.40);
                    raise=1; break;
                }
                if (dupcard1>10) {
                    add=(max_bet*.80)+xp_random(max_bet*.20);
                    raise=1; break;
                }
            }
            if (rank==TWO_PAIR) {
                if (xraised>=2) {
                    call=1; break;
                }
                if (dupcard1<11 && dupcard2<11) {
                    add=(max_bet*.80)+xp_random(max_bet*.20);
                    raise=1; break;
                }
                if (dupcard1>10 || dupcard2>10) {
                    add=(max_bet*.90)+xp_random(max_bet*.10);
                    raise=1; break;
                }
            }
            if (rank>=THREE_OF_A_KIND && rank<=FULL_HOUSE) {
                if (xraised>=3) {
                    call=1; break;
                }
                add=max_bet;
                raise=1; break;
            }
            if (rank>=FOUR_OF_A_KIND) {
                if (xraised>=4) {
                    call=1; break;
                }
                add=max_bet;
                raise=1; break;
            }
            bprintf("\r\nPROBLEM: Stage BET2, please report to SysOp!");
            pause();
        }
    }
    if (fold) {
        bprintf("\r\n\1g\1h%s \1n\1gfolds.",comp_name); comp_stat=FOLDED;
        next_player(); return;
    }

    if (call) {
        total_pot+=(add+(current_bet-comp_bet));
        put_game_status(-1);
        comp_bet=current_bet;
        bprintf("\r\n\1g\1h%s \1n\1gcalls - \1b\1h The total pot is now %luK"
                ,comp_name,total_pot);
        next_player(); return;
    }

    if (raise) {
        if (add>10) add-=add%10;
        if (add<1) add=1;
        total_pot+=(add+(current_bet-comp_bet));
        current_bet+=add;
        put_game_status(-1);
        comp_bet+=(current_bet-comp_bet);
        bprintf("\r\n\1g\1h%s \1m\1hhas increased the bet to \1y\1h%luK"
                "\r\n\1b\1hThe total pot is now \1y\1h%luK\1n",comp_name
                ,current_bet,total_pot);
        next_player(); xraised++; return;
    }
}
/************************************************************************
 This function shows the hands of all the players.  If WINNER is a number
 greater than zero, it will show that player as being the winner of the
 game.
*************************************************************************/
void show_all_hands(int winner)
{
    char str[256],str1[256];
    int show;

    str1[0]=0;
    get_game_status(cur_table);
    if (total_players>1) {
        for (show=1;show<=sys_nodes;show++) {
            if (node[show-1]) {
                get_player(show);
                gethandtype(show);
                sprintf(str,"\r\n\1g\1h(%2d) %-25.25s %s \1g\1h(\1y%-15.15s\1g)"
                        ,show,username(temp_num),handstr(show),ranking(rank));
                if (show==winner) strcat(str," \1m\1hWINNER");
                if (node[show-1]==FOLDED) strcat(str," \1r\1hFolded");
                strcpy(str1,str);
                if (!symbols) strip_symbols(str);
                bprintf(str); str[0]=0;
            }
        }
        send_all_message(str1,0); str1[0]=0;
    }
    if (total_players==1) {                 /* Single Player Stuff */
        for (show=1;show<=sys_nodes;show++) {
            if (node[show-1]) {
                get_player(show);
                gethandtype(show);
                sprintf(str,"\r\n\1g\1h(%2d) %-25.25s %s \1g\1h(\1y%-15.15s\1g)"
                        ,show,user_name,handstr(show),ranking(rank));
                if (show==winner) strcat(str," \1m\1hWINNER");
                if (node[show-1]==FOLDED) strcat(str," \1r\1hFolded");
                if (!symbols) strip_symbols(str);
                bprintf(str); str[0]=0;
            }
        }
        gethandtype(0);
        sprintf(str,"\r\n\1g\1h(%2d) %-25.25s %s \1g\1h(\1y%-15.15s\1g)"
                ,sys_nodes+1,comp_name,handstr(0),ranking(rank));
        if (!winner) strcat(str," \1m\1hWINNER!");
        if (comp_stat==FOLDED) strcat(str," \1r\1hFolded");
        if (!symbols) strip_symbols(str);
        bprintf(str); str[0]=0;
    }
}


/*********************************************************
 This function declares the winner on a single player game
**********************************************************/
void single_winner()
{
    int winner=0,comprank,compdup1,compdup2,x;
    long l=0;


    firstbet=!firstbet;     /* Toggle the betting stage */

    gethandtype(0);
    comprank=rank; compdup1=dupcard1; compdup2=dupcard2;
    get_game_status(cur_table); get_player(node_num);

    if (node[node_num-1]==FOLDED || (comprank>rank && comp_stat!=FOLDED))
        winner=0;

    if (comp_stat==FOLDED || (rank>comprank && node[node_num-1]!=FOLDED))
        winner=1;

    if (comp_stat!=FOLDED && node[node_num-1]!=FOLDED) {
        if (rank==comprank) {
            if (rank==ROYAL_FLUSH)
                winner=2;
            if (rank==ONE_PAIR || rank==THREE_OF_A_KIND || rank==FOUR_OF_A_KIND
                || rank==FULL_HOUSE) {
                if (dupcard1>compdup1)
                    winner=1;
                else
                    winner=0;
            }
            if (rank==TWO_PAIR) {
                if ((dupcard1>compdup1 && dupcard1>compdup2) ||
                    (dupcard2>compdup1 && dupcard2>compdup2))
                    winner=1;
                else
                    winner=0;
            }
            if (rank==NOTHING || rank==STRAIGHT || rank==FLUSH ||
                rank==STRAIGHT_FLUSH || (rank==ONE_PAIR &&
                dupcard1==compdup1)) {
                if (hand_total(node_num)>hand_total(0))
                    winner=1;
                if (hand_total(0)>hand_total(node_num))
                    winner=0;
                if (hand_total(0)==hand_total(node_num))
                    winner=2;
            }
        }
    }

    if (create_log) {
        if (!winner)
                l+=(1024L*total_pot);
            else if (winner==1)
                l-=(1024L*total_pot);
            else if (winner==2)
                l+=(1024L*total_pot)/2L;
        computer_log(l);
    }
    if (!winner) {
        show_all_hands(0);
        bprintf("\r\n\r\n\1b\1hThe pot was \1c\1h%luK\1b\1h of credits!\1n"
                ,total_pot);
    }

    if (winner==1) {
        show_all_hands(node_num);
        get_player(node_num);
        temppts=temppts+(1024L*total_pot);
        newpts=temppts; moduser();
        bprintf("\r\n\r\n\1b\1hYou win \1c\1h%luK\1b\1h of credits!\1n"
                ,total_pot);
    }

    if (winner==2) {
        show_all_hands(-1);
        bprintf("\r\n\r\n\1b\1hThere was a tie!");
        total_pot=total_pot/2;
        bprintf("\r\n\1g\1hNeither of us gains or loses anything!\1n\r\n");
        temppts=temppts+(1024L*total_pot);
        newpts=temppts; moduser();
    }

    node[node_num-1]=WAITING; comp_stat=WAITING;
    get_player(node_num); player_bet=0; rank=0;
    for (x=0;x<5;x++) hand[node_num-1][x].value=0;
        put_player(node_num);
    cur_node=dealer=node_num;

    total_pot=current_bet=0; firstbid=0; stage=OPEN; comp_bet=0;
    comp_stat=0; put_game_status(-1);
    get_player(node_num); return;
}

/*********************************
 This is the main loop of the game
**********************************/
void play_menu()
{
    char ch, fname[81],str[256],
        *waitcmd="\1y\1hCommand \1b\1h(\1c\1hL,S,Y,Q,^P,^U,?\1b\1h):\1n ",
        *dealcmd="\1m\1hDealer \1n\1b\1h(\1c\1hD,L,S,Y,Q,^P,^U,?\1b\1h):\1n ";
    int warn,tt;
    time_t timeout,timeout2,now;

    sprintf(fname,"message.%d",node_num);
    get_game_status(cur_table);

    bprintf("\r\n");
    nodesync();
    bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
    if (dealer==node_num)
        bprintf(dealcmd);
    else
        bprintf(waitcmd);               /* Show the command prompt */
    timeout=time(NULL);                 /* Set entry time here */
    timeout2=time(NULL);                /* for the demo game timer only */

    do {
        now=time(NULL);                 /* Start the timer here */
        if ((ch=toupper(inkey(0)))!=0) {
            lncntr=0;                   /* Reset the line counter */
            timeout=time(NULL); warn=0; /* Reset the timer if a key is hit */
            bprintf("%c\r\n",ch);
            switch(ch) {
                case '?':                   /* List of commands */
                    bprintf("\r\n");
                    if (dealer==node_num)
                        mnemonics("~Deal the cards\r\n");
                    mnemonics("~List players\r\n"
                              "~Send message within game\r\n"
                              "~Your player status\r\n");
                    mnemonics("~^~P Send private message anywhere\r\n"
                              "~^~U List users online\r\n"
                              "~Quit Playing\r\n");
                    break;

                case 'D':                   /* Deal the cards */
                    if (total_players==1 && !(options[cur_table]&COMPUTER)) {
                        bprintf("\r\n\1m\1hYou must wait for another player to "
                                "arrive.");
                        break;
                    }
                    if (total_players==1 && !SYSOP && time_allowed &&
                       (time_played>=time_allowed)) {
                        bprintf("\r\n\1m\1h%s has left the casino!\r\nYou must "
                                "wait for another player to arrive.",comp_name);
                        break;
                    }
                    if (dealer==node_num && stage==OPEN) {
                        node[node_num-1]=DEALING; deal();
                    }
                    break;

                case 'L':                   /* List all players */
                    do {
                    bprintf("\r\n\1b\1hWhich table [\1c1-%d\1b][\1cT=Table "
                            "list\1b][\1yEnter=This Table\1b]: ",num_tables);
                    tt=getkeys("t",num_tables);
                    if (tt==0) {
                        tt=cur_table; show_players(tt); bputs("\r\n"); }
                    if (tt&0x8000) { tt&=~0x8000;
                        show_players(tt); bputs("\r\n"); break; }
                    if (tt=='T') show_tables();
                    } while (tt=='T');
                    break;
                case 'S':                   /* Send messages */
                    send_message();
                    break;
                case 'Y':                   /* Player status */
                    player_status();
                    break;
                case 'Q':                   /* Quit game */
                    if (stage==OPEN) { quit_out(); return; }
                    else bprintf("\r\n\1r\1hA game is in play - No quitting!\1n");
                    break;
            }
            bprintf("\r\n");
            nodesync();
            bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
            if (dealer==node_num)
                bprintf(dealcmd);
            else
                bprintf(waitcmd);               /* Show the command prompt */
        }
        mswait(0);
        mswait(100);
        get_game_status(cur_table);
        get_player(node_num);
        if (flength(fname)>0) {                 /* Checks for new player   */
            bprintf("\r\n");                    /* Messages and reads them */
            do {                                
                read_player_message();
                mswait(100);
            } while (flength(fname)>0);
            get_game_status(cur_table); lncntr=0;   /* Reset Line Counter */
            bprintf("\r\n"); nodesync();
            bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
            if (node_num==dealer)
                bprintf(dealcmd);
            else
                bprintf(waitcmd);
        }

        if (cur_node==node_num && flength(fname)<1) {
            if (node_num==dealer && stage!=OPEN) {
                dealer_ctrl(); timeout=time(NULL); warn=0;
                bprintf("\r\n"); nodesync();
                bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
                if (node_num==dealer)
                    bprintf(dealcmd);
                else
                    bprintf(waitcmd);
            }
        }
        get_player(node_num);
        if (newpts!=temppts) {                  /* Updates player credits*/
            newpts=temppts;                     /* if they have changed  */
			moduser();							/* since file was last   */
		}										/* looked at			 */

        if ((newpts<=(max_total[cur_table]*1024L)) && (stage==OPEN)) {
            get_player(node_num);               /* Kicks player out if he */
            newpts=temppts;                     /* is lacking xfer credit */
            if (newpts<=(max_total[cur_table]*1024L)) {
                bprintf("\r\n\r\n\1r\1hYou have reached the minimum number of "
                        "credits required to play this table!\1n\r\n");
                sprintf(str,"\r\n\1n\1gNode %2d: \1h%s  \1rWaaa! Now I'm too "
                        "poor to play this table!",node_num,user_name);
                send_all_message(str,0);
				quit_out();
				break;
			}
        }

        if (!cur_node && dealer==node_num) {    /* Computer is cur_node */
            if (stage==BET && comp_bet!=current_bet || !current_bet) {
                bprintf("\r\n");
                computer_bet(); timeout=time(NULL); warn=0;
                bprintf("\r\n"); nodesync();
                bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
                bprintf(dealcmd); }
            if (stage==BET2 && (firstbid || comp_bet!=current_bet)) {
                bprintf("\r\n");
                computer_bet(); timeout=time(NULL); warn=0;
                bprintf("\r\n"); nodesync();
                bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
                bprintf(dealcmd); }
            if (stage==DISCARD) {
                bprintf("\r\n");
                computer_discard();
                bprintf("\r\n"); nodesync();
                bprintf("\r\n\1b\1h[\1c%s\1b ",activity2(stage));
                bprintf(dealcmd); }
        }

        if (cur_node==node_num && node_num!=dealer && flength(fname)<1) {
            if (stage==BET) {
                if (player_bet!=current_bet || !current_bet) {
                    bet_menu(); timeout=time(NULL); warn=0;
                    bprintf("\r\n"); nodesync();
                    bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
                    bprintf(waitcmd);
                } else next_player();
            }
            if (stage==BET2) {
                if (player_bet!=current_bet || firstbid) {
                    bet_menu(); timeout=time(NULL); warn=0;
                    bprintf("\r\n"); nodesync();
                    bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
                    bprintf(waitcmd);
                } else next_player();
            }
            if (stage==DISCARD) {
                discard();
                bprintf("\r\n"); nodesync();
                bprintf("\r\n\1b\1h[\1c%s\1b] ",activity2(stage));
                bprintf(waitcmd); }
        }
    if (time_allowed && total_players==1 && (time_played<time_allowed)) {
        time_played+=time(NULL)-now;
        if (time_played>time_allowed) time_played=time_allowed;
    }
    if (timeout2);

        if((now-timeout)>=240 && !warn) /* Warning Beep */
        for(warn=0;warn<5;warn++) {
            putchar(7);
        }
        checkline();
    } while ((now-timeout)<300);        /* Inactivity Timeout */
    if ((now-timeout)>=300) { quit_out(); exit(0); }
}

void player_status()
{
    char str[256];

    get_game_status(cur_table);
    bprintf("\r\n\1b\1hName       : \1c\1h%s\r\n\1b\1hCredits    : \1c\1h"
            "%ldK\1b\1h\r\nCurrent Bet: \1c\1h%luK\r\n\1b\1hCurrent Pot: "
            "\1c\1h%luK\1n",user_name,(newpts/1024L),current_bet,total_pot);
    if (stage!=OPEN) {
        get_player(node_num);
        gethandtype(node_num);
        sprintf(str,"\r\n\1b\1hYour hand  : %s \1b\1h- (\1c%s\1b)\1n"
                ,handstr(node_num),ranking(rank));
        if (!symbols) strip_symbols(str);
        bprintf(str);
    }
}
/******************************************************************************
 This function lists the active nodes, player names, and what the node is doing
*******************************************************************************/
void show_players(int tablenum)
{
    char str[256];
    int num;

    if (!tablenum) tablenum=cur_table;
    if (!tablenum) tablenum=1;
    sprintf(str,"gamestat.%d",tablenum);

    if (fexist(str)) get_game_status(tablenum);
    else total_players=0;

    if (!total_players) {
        bprintf("\r\n\1c\1hThere is no one currently playing on table %d.\1n"
                ,tablenum);
		return;
	}
	for (num=1;num<=sys_nodes;num++) {
		if (node[num-1]) {
            get_player(num);
            if (dealer==num) {
                bprintf("\r\n\t\1r\1h\1i\b*\1n\1gNode %2d: \1h%-25.25s"
                        ,num,username(temp_num));
                bprintf(" (\1y\1h%s\1g\1h)",activity(node[num-1]));
            } else {
                bprintf("\r\n\t\1n\1gNode %2d: \1h%-25.25s",num
                        ,username(temp_num));
                bprintf(" (\1y\1h%s\1g\1h)",activity(node[num-1]));
            }
		}
	}
    if ((total_players==1) && (!time_allowed || time_played<time_allowed) &&
        (options[cur_table]&COMPUTER)) {
        if (dealer>0)
            bprintf("\r\n\t\1n\1gNode %2d: \1h%s",sys_nodes+1,comp_name);
        else
            bprintf("\r\n\t\1r\1h\1i\b*\1n\1gNode %2d: \1h%s",sys_nodes+1
                    ,comp_name);
    }
    get_game_status(cur_table);
}

/***********************************************************
 This function makes the next active node the current player
************************************************************/
void next_player()
{
    int i;

    get_game_status(cur_table);                  /* get player info */


    if (node[node_num-1]!=FOLDED) {
        node[node_num-1]=WAITING; put_game_status(node_num-1);
    }

    if (total_players==1 && node[node_num-1]) {  /* for single player game */
        if (cur_node==node_num)
            cur_node=0;
        else
            cur_node=node_num;
        put_game_status(node_num-1);
        return;
    }

    for(i=cur_node+1;i<=sys_nodes;i++)  /* search for the next highest node */
        if(node[i-1]==WAITING || i==dealer)  /* that is active */
            break;
    if(i>sys_nodes) {                   /* if no higher active nodes, */
        for(i=1;i<cur_node;i++)         /* start at bottom and go up  */
            if(node[i-1]==WAITING || i==dealer)
                break;
        if(i==cur_node)                 /* if no active nodes found */
            cur_node=dealer;            /* make current player the dealer */
        else
            cur_node=i; }
    else
        cur_node=i;                     /* else active node current player */
    put_game_status(-1);                /* write info and unlock */
}

/******************************************
 This function deals 5 cards to each player
*******************************************/
void deal()
{
    int x,num;
    char str[256],str1[256];

    get_game_status(cur_table); stage=DEAL;
    put_game_status(node_num-1);
    shuffle();
    getdeck(); get_game_status(cur_table);

    for (num=1;num<=sys_nodes;num++) {  /* Reset all nodes to WAITING */
        if (node[num-1]) {
            if (num==node_num)
                node[num-1]=DEALING;
            else
                node[num-1]=WAITING;
            sprintf(str,"\r\n\1m\1hAnte up!  Everyone antes \1y%dK \1mof "
                    "credits.\r\n",(ante[cur_table]));
            if (num==node_num)
                bprintf(str);
            else
                send_player_message(num,str);
            get_player(num);
            temppts-=((long)(ante[cur_table])*1024L);
            if (num==node_num) newpts=temppts;
            put_player(num); moduser();
        }
    }
    if (total_players>1)        /* Put everyones ante in the pot */
        total_pot+=((ante[cur_table])*total_players);
    else
        total_pot+=((ante[cur_table])*2);
    for (x=0;x<5;x++) {
        for (num=1;num<=sys_nodes;num++) {
            if (node[num-1]==WAITING || node[num-1]==DEALING) {
                get_player(num);
                hand[num][x]=deck[cur_card++];
                put_player(num);
                if (total_players==1)       /* Deal in the computer */
                    hand[0][x]=deck[cur_card++];
            }
        }
    } putdeck();
    for (num=1;num<=sys_nodes;num++) {
        if (node[num-1]) {
            get_player(num);
            gethandtype(num);
            put_player(num);
            sprintf(str,"\r\n\1b\1hYou were dealt: %s - ",handstr(num));
            sprintf(str1,"\1b\1h(\1c\1h%s\1b\1h)\1n",ranking(rank));
            strcat(str,str1);
            if (num==node_num) {
                if (!symbols) strip_symbols(str);
                bprintf(str);
            } else
                send_player_message(num,str);
        }
    }
    stage=BET; put_game_status(-1);                  /* Set to betting stage */
    if (total_players==1) xraised=0;
    next_player();                                   /* Go to next player    */
}

/*************************************************************************
 This function is where the dealer machine goes to find out what should
 happen next in the game.  The game loop must ALWAYS return here to decide
 how to continue.
**************************************************************************/
void dealer_ctrl()
{
    int num,x=0;

    get_game_status(cur_table); get_player(node_num);
    if (node_num!=cur_node) return;

    if (total_players==1)                   /* Single Player Stuff */
        if (comp_stat==FOLDED || node[node_num-1]==FOLDED) {
            stage=GET_WINNER; put_game_status(-1); declare_winner();
            return;
        }

    for (num=1;num<=sys_nodes;num++)
        if (node[num-1]==WAITING || node[num-1]==DEALING) x++;

    if (!x) { stage=GET_WINNER; put_game_status(-1);
        declare_winner();
        return;                             /* Everyone has folded */
    }
    if (x==1 && total_players>1) { stage=GET_WINNER; put_game_status(-1);
        declare_winner();
        return;                             /* All but 1 has folded */
    }
    x=0;
    if (stage==BET || stage==BET2) {
        if (node[node_num-1]!=FOLDED && (firstbid || !current_bet ||
            player_bet!=current_bet)) {
            bet_menu();
        }
        for (num=1;num<=sys_nodes;num++)
            if (node[num-1]==WAITING || node[num-1]==DEALING) x++;

        if (!x) { stage=GET_WINNER; put_game_status(-1);
            declare_winner();
            return;                             /* Everyone has folded */
        }
        if (x==1 && total_players>1) { stage=GET_WINNER; put_game_status(-1);
            declare_winner();
            return;                             /* All but 1 has folded */
        }
        if (firstbid) firstbid=0; put_game_status(-1); x=0;
        for (num=1;num<=sys_nodes;num++) {
            if (node[num-1]==WAITING) {
                get_player(num);
                if (player_bet!=current_bet) {
                    get_game_status(cur_table); cur_node=num;
                    put_game_status(-1); x++; return;
                }
            }
        }
        /* If single player has raised the bet */
        if (total_players==1 && (stage==BET || stage==BET2) &&
           (comp_bet!=current_bet)) {
            next_player(); return;
        }
        /* Bug fix - if dealer folded but nobody else, game would hang */
        if (!x && stage==BET2) { stage=GET_WINNER; put_game_status(-1);
            declare_winner();
            return; }
        if (stage==BET) {
            get_game_status(cur_table); stage=DISCARD; put_game_status(-1);
            next_player(); return;
        } else if (stage==BET2) {
            get_game_status(cur_table); stage=GET_WINNER; put_game_status(-1);
            declare_winner(); return;
        }
    }
    if (stage==DISCARD) {
        if (node[node_num-1]!=FOLDED)
            discard();
        get_game_status(cur_table); stage=BET2; firstbid=1; put_game_status(-1);
        if (total_players==1)
            xraised=0;
        next_player();
    }
}
/**************************************************
 This function is where the players make their bets
***************************************************/
void bet_menu()
{
    char str[256],betcmd[256],ch;

    get_game_status(cur_table); node[node_num-1]=BETTING;
    put_game_status(node_num-1);
    sprintf(str,"\r\n\1n\1gNode %2d: \1h%s \1n\1gis betting.",node_num,user_name);
    send_all_message(str,0);

    lncntr=0; bprintf("\r\n"); player_status();

    strcpy(betcmd,"\r\n\r\n\1r\1hYour Bet \1n\1b\1h(\1c\1h");
    if (!current_bet) strcat(betcmd,"I,");
    if (current_bet) strcat(betcmd,"C,R,");
    strcat (betcmd,"F,Y,?\1b\1h):\1n ");

    do {
        bprintf(betcmd); ch=getkey(K_UPPER);
        bprintf("%c\r\n",ch);

        switch (ch) {
            case '?':                   /* Betting Menu */
                bprintf("\r\n");
                if (!current_bet)  mnemonics("~Initial Bet  ");
                if (current_bet)   mnemonics("~Call  ~Raise  ");
                mnemonics("~Fold  ~Your status  ~?This menu");
                break;
            case 'Y':
                player_status(); break; /* Player Status */
            case 'C':                   /* Call */
                if (!current_bet) break;
                get_game_status(cur_table); node[node_num-1]=WAITING;
                get_player(node_num);
                total_pot+=(current_bet-player_bet);
                newpts-=(1024L*(current_bet-player_bet));
                player_bet=current_bet; moduser();
                put_player(node_num); put_game_status(-1);
                bprintf("\r\n\1n\1g\1hYou call - ");
                bprintf("\1b\1hThe total pot is now \1h\1y%luK\1n",total_pot);
                sprintf(str,"\r\n\1n\1g\1h%s calls - \1b\1hThe total pot is now "
                        "\1y\1h%luK\1n",user_name,total_pot);
                send_all_message(str,node_num);
                if (total_players==1 && firstbid) {
                    firstbid=0;
                    put_game_status(-1);
                }
                if (dealer!=node_num)
                    next_player();
                break;
            case 'F':                   /* Fold */
                node[node_num-1]=FOLDED;
                bprintf("\r\n\1n\1g\1hYou fold.");
                sprintf(str,"\r\n\1n\1gNode %2d: \1h%s\1n\1g folds",node_num,
                        user_name);
                send_all_message(str,node_num);
                put_game_status(node_num-1);
                if (dealer!=node_num)
                    next_player();
                break;
            case 'I':
            case 'R':
                increase_bet();
                break;
        }
    } while(node[node_num-1]!=FOLDED && node[node_num-1]!=WAITING);
}

/*********************************************
 Increases the current bet to allowable limits
**********************************************/
void increase_bet()
{
    int num,add;
    char str[256];

    get_game_status(cur_table);
    max_bet=bet_limit[cur_table];           /* set to predifined limit */
        for (num=1;num<=sys_nodes;num++) {
            if (node[num-1] && node[num-1]!=FOLDED) {
                get_player(num);
                if (max_bet>(temppts/1024L)-(current_bet-player_bet))
                    max_bet=(temppts/1024L)-(current_bet-player_bet);
                /* set to lowest amount minus the minimum required */
            }
        }

    if (max_bet<bet_limit[cur_table]) {
        bprintf ("\r\n\1r\1hSomeone has run out of credits, you must CALL or "
                 "FOLD!");
        return;
    }
    if (current_bet>=max_total[cur_table]) {
        bprintf("\r\n\1r\1hThe table limit has been reached!  You must CALL or "
                "FOLD!");
        return;
    }

    if (current_bet+max_bet>=max_total[cur_table])
        max_bet=max_total[cur_table]-current_bet;

    if (last_bet>max_bet) last_bet=max_bet;

    while (1) {
        bprintf("\r\n\1b\1hEnter amount to increase by in K [\1c\1hMAX="
                "%ldK\1b\1h] [\1c\1hQ=Don't Bet\1b\1h] [\1c\1hEnter=\1y\1h%d"
                "\1b\1h]: \1n",max_bet,last_bet);
        add=getnum(max_bet); if (add==0) add=last_bet;
		if (add<=0) break;
        if (add!=last_bet) last_bet=add;
		if (add<=max_bet) {
            get_game_status(cur_table); get_player(node_num);
            node[node_num-1]=WAITING;
            total_pot+=(add+(current_bet-player_bet)); firstbid=0;
            current_bet+=add;
            put_game_status(-1);
            newpts-=(1024L*(current_bet-player_bet));
            player_bet=current_bet;
            put_player(node_num);
			moduser();
            bprintf("\r\n\1m\1hYou have increased the bet to \1y\1h%luK\r\n"
                    "\1b\1hThe total pot is now \1h\1y%luK\1n"
                    ,current_bet,total_pot);
            sprintf(str,"\r\n\1n\1gNode %2d: \1h%s \1m\1hhas increased the bet "
                    "to \1y\1h%luK\r\n\1b\1hThe total pot is now \1y\1h%luK\1n",
                    node_num,user_name,current_bet,total_pot);
            send_all_message(str,node_num);
            if (dealer!=node_num)
                next_player();
			break;
		}
        else bprintf("\r\n\r\nMust be less than %ldK!!",max_bet);
	}
}

/*********************************************************************
 This function writes a players hand along with it's ranking to a file
**********************************************************************/
void put_player(int player)
{
    int file;
    char str[256];

    if (player==node_num) { temp_num=user_number; temppts=newpts; }

    sprintf(str,"player.%d",player);
    if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
    bprintf("put_player: Error opening '%s'\r\n",str);
        return; }
    write(file,&cur_table,sizeof(cur_table));
    write(file,&temp_num,sizeof(temp_num));
    write(file,&player_bet,sizeof(player_bet));
    write(file,&temppts,sizeof(temppts));
    gethandtype(player);
    write(file,&rank,sizeof(rank));
    write(file,&dupcard1,sizeof(dupcard1));
    write(file,&dupcard2,sizeof(dupcard2));
    write(file,&hicard,sizeof(hicard));
    write(file,hand[player],5*sizeof(card_t));
    close(file);
}


/**** !!!!!!!!!!!!!! LOOK NO FURTHER !!!!!!!!!!!!!!!! ****/

/*************************************************************************
 This function writes the shuffled deck of cards to a file, along with the
 current card position
**************************************************************************/
void putdeck()
{
    char str[20];
	int file;

    sprintf(str,"deck.%d",cur_table);
    if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
        bputs("putdeck: Error opening DECK for write\r\n");
        return; }
    write(file,&cur_card,sizeof(cur_card));
    write(file,deck,52*sizeof(card_t));
    close(file);
}

/**********************************************************************
 This function reads in the shuffled deck and the current card position
***********************************************************************/
void getdeck()
{
    char str[20];
    int file;

    sprintf(str,"deck.%d",cur_table);
    if((file=nopen(str,O_RDONLY))==-1) {
        bputs("getdeck: Error opening DECK for read\r\n");
        return; }
    read(file,&cur_card,sizeof(cur_card));
    read(file,deck,52*sizeof(card_t));
    close(file);
}

/********************************************************
 This function returns the total value of a players hand
********************************************************/
int hand_total(int player)
{
    int x,total=0;

    for (x=0;x<5;x++) {
        if (hand[player][x].value<11)
            total+=hand[player][x].value^2;
        if (hand[player][x].value==11)
            total+=400;
        if (hand[player][x].value==12)
            total+=800;
        if (hand[player][x].value==13)
            total+=1600;
        if (hand[player][x].value==14)
            total+=4200;
    }
    return(total);
}
/**************************************
 This function ranks a hand of 5 cards
**************************************/
void gethandtype(int player)
{
    int i,j,x=0,same=0,highest_card=0,lowest_card=0,m,m1,m2;
    card_t temphand[52];

    rank=0; m=m1=m2=-1; dupcard1=dupcard2=0;
    for (i=0;i<5;i++) {
        temphand[i]=hand[player][i];
        if (hand[player][i].value>highest_card)
            highest_card=hand[player][i].value;
        if (temphand[0].suit==hand[player][i].suit) x++;
        for (j=0;j<5;j++) {
            if (m==j)  j++; if (m1==j) j++; if (m2==j) j++;
            if (temphand[i].value==hand[player][j].value) {
                same++;
                if (same==2 && i<5 && m>-1 && m1==-1) {
                    m1=j; dupcard2=temphand[i].value; }
                if (same==2 && i<5 && m==-1) {
                    m=j; dupcard1=temphand[i].value; }
                if (same==3 && i<5 && i!=m1 && i!=m) {
                    m2=j; dupcard1=temphand[i].value; }
            }
        }
        if (same==2 && rank==ONE_PAIR)          rank=TWO_PAIR;
        if (same==2 && rank==NOTHING)           rank=ONE_PAIR;
        if (same==2 && rank==THREE_OF_A_KIND)   rank=FULL_HOUSE;
        if (same==3 && rank==ONE_PAIR)          rank=FULL_HOUSE;
        if (same==3 && rank==NOTHING)           rank=THREE_OF_A_KIND;
        if (same==4)                            rank=FOUR_OF_A_KIND;
        same=0;
    }
    hicard=highest_card;
    if (x==5 && rank<FLUSH) rank=FLUSH;     /* Suits of all 5 are same */

    if (rank==FLUSH) {                      /* Check for Royal Flush */
        x=0;
        for (i=0;i<5;i++) {
            x=x+hand[player][i].value;
        }
        if (x==60) { rank=ROYAL_FLUSH; return; }
    }

    x=0;                                    /* Check for a Straight */
    if (highest_card==14) {                 /* Convert A to a 1     */
        lowest_card=1;
        for (i=0;i<5;i++) {
            for (j=0;j<5;j++) {
                if (hand[player][j].value==(lowest_card+1)) {
                    x++; lowest_card++;
                }
            }
        }
    }
    if (x<4) { x=0;                         /* Leave cards at their */
        for (i=0;i<5;i++) {                 /* normal values        */
            for (j=0;j<5;j++) {
                if (hand[player][j].value==(highest_card-1)) {
                    x++; highest_card--;
                }
            }
        }
    }
    if (x==4) {                             /* We've got a Straight */
        if (rank==FLUSH) rank=STRAIGHT_FLUSH;
        if (rank<STRAIGHT) rank=STRAIGHT;
    }
    return;
}
/*******************************
 This is used for centering text
********************************/
void center_wargs(char *str,...)
{
	int x;
    char str1[256],sbuf[1024];
    va_list argptr;

    va_start(argptr,str);
    vsprintf(sbuf,str,argptr);
    va_end(argptr);

/*    vsprintf(sbuf,str,_va_ptr); */
    for (x=0;x<(80-bstrlen(sbuf))/2;x++)
		str1[x]=32;
	str1[x]=0;
	strcat(str1,sbuf);
	strcat(str1,"\r\n\1n");
	bputs(str1);
}

/*************************************************
 This function reads in a line of data from a file
**************************************************/
void readline(char *outstr, int maxlen, FILE *stream)
{
	char str[257];

fgets(str,256,stream);
sprintf(outstr,"%-.*s",maxlen,str);
truncsp(outstr);
}

/***************************
 Writes the MODUSER.DAT file
****************************/
void moduser()
{
    char str[256];
	long xfer_pts;
    FILE *fp;

    xfer_pts=(newpts-user_cdt);
    sprintf(str,"%smoduser.dat",node_dir);
    if((fp=fopen(str,"wt"))==NULL) {
        printf("Can't open %s\r\n",str);
        exit(1); }
    fprintf(fp,"%ld\r\n",xfer_pts);
    fclose(fp);
    return;
}

/****************************************************************************/
/* This function shuffles the deck. 										*/
/****************************************************************************/
void shuffle()
{
    char str[256];
	uint i,j;
    card_t shufdeck[52];

    sprintf(str,"\1_\1w\1h\r\nShuffling...");
    bputs(str);

    memcpy(shufdeck,newdeck,sizeof(newdeck));      /* fresh deck */

    i=0;
    while(i<51) {
        j=xp_random((52)-1);
        if(!shufdeck[j].value)  /* card already used */
            continue;
        deck[i]=shufdeck[j];
        shufdeck[j].value=0;    /* mark card as used */
        i++; }

    cur_card=0;
    putdeck();
    bputs("\r\n");
    return;
}
/*****************************************************
 This function reads any messages waiting for the user
******************************************************/
void read_player_message()
{
    char *buf,str[256];
	int file;
	ulong length;

    sprintf(str,"message.%d",node_num);
    if((file=nopen(str,O_RDONLY))==-1) {
        bprintf("File not Found: %s\r\r\n",str);
        return; }
    length=filelength(file);
    if((buf=malloc(length+1L))==NULL) {
        close(file);
        bprintf("\7\r\r\nPRINTFILE: Error allocating %lu bytes of memory for %s.\r\r\n"
            ,length+1L,str);
        return; }
    buf[read(file,buf,length)]=0;
    close(file);
    if((file=nopen(str,O_WRONLY|O_TRUNC))==-1) {
        bprintf("File not Found: %s\r\r\n",str);
        return; }
    close(file);
    if (!symbols)
        strip_symbols(buf);
    bputs(buf);
    free(buf);
    return;
}

/*************************************************
 This function sends messages to player number plr
**************************************************/
void send_message()
{
	int num;
    char str[256],str1[256];

    if (total_players==1) {
        bprintf("\r\n\r\n\1r\1hYou are the only player!\1n");
        return;
    }
    show_players(0);
    while(1) {
        bprintf("\r\n\r\n\1b\1hTo Whom (\1c\1hA,Q,#\1b\1h):\1n");
        strcpy(str,"aq");
        num=getkeys(str,sys_nodes);
        if (num&0x8000 || num=='A') {
            if (num&0x8000) num&=~0x8000;
            if (num==node_num) {
                bprintf("\r\n\1r\1hDon't send messages to yourself!\1n");
                return;
            }
            get_game_status(cur_table);
            if ((node[num-1] && (num!=node_num)) || num=='A') {
                bprintf("\r\n\1b\1hMessage:\1n ");
                do {
                    if (!getstr(str1,45,K_CHAT|K_WRAP))
                        break;
                    sprintf(str,"\r\n\1g\1hFrom %s: \1n\1g%s\r\n",user_name
                            ,str1);
                    if (num=='A') {
                        send_all_message(str,0);
                    } else {
                        send_player_message(num,str);
                    }
                } while (inkey(0) || wordwrap[0]);
                break;
            } else bprintf("\r\n\r\n\1r\1hInvalid Player Number!\r\n");
        } else if (num=='L') show_players(0);
          else if (num=='Q') break;
    }
    return;
}

/*******************************************************
 This function returns the players 5 cards as one string
********************************************************/
char *handstr(int player)
{
    int x;
    static char str[256];

    str[0]=0;
    for (x=0;x<5;x++) {
        strcat(str,cardstr(hand[player][x]));
        strcat(str," ");
    }

    return(str);
}

/****************************************************************************/
/* This function returns the string that represents a playing card. 		*/
/****************************************************************************/
char *cardstr(card_t card)
{
    static char str[256];
	char tmp[20];

strcpy(str,"\1n\0017"); /* card color - background always white */
if(card.value==0) {     /* card has been discarded */
    strcat(str,"\1kDIS\1n"); return(str);
}
if(card.suit==H || card.suit==D)
	strcat(str,"\1r");  /* hearts and diamonds - foreground red */
else
	strcat(str,"\1k");  /* spades and clubs - foreground black */
if(card.value>10)	/* face card */
	switch(card.value) {
		case J:
            strcat(str," J");
			break;
		case Q:
            strcat(str," Q");
			break;
		case K:
            strcat(str," K");
			break;
		case A:
            strcat(str," A");
			break; }
else {
    sprintf(tmp,"%2d",card.value);
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

/*********************************************
 This shows verbally what stage the game is in
**********************************************/
char *activity2(int num)
{
    static char str[256];

    switch(num) {
        case OPEN:
            strcpy(str,"Open");
            break;
        case DEAL:
            strcpy(str,"Dealing");
            break;
        case BET:
        case BET2:
            strcpy(str,"Betting");
            break;
        case DISCARD:
            strcpy(str,"Discard");
            break;
    }
    return(str);
}

/************************************************
 This function explains a nodes activity in words
*************************************************/
char *activity(int num)
{
    static char str[256];

    switch(num) {
        case WAITING:
            strcpy(str,"Waiting");
            break;
        case BETTING:
            strcpy(str,"Betting");
            break;
        case DISCARDING:
            strcpy(str,"Discarding");
            break;
        case FOLDED:
            strcpy(str,"Folded");
            break;
        case DEALING:
            strcpy(str,"Dealing");
            break;
    }
    return(str);
}

/*******************************************
 This gives the ranking, in words, of a hand
********************************************/
char *ranking(int num)
{
    static char str[256];

    switch(num) {
        case NOTHING:
            strcpy(str,"High Card");
            break;
        case ONE_PAIR:
            strcpy(str,"One Pair");
            break;
        case TWO_PAIR:
            strcpy(str,"Two Pair");
            break;
        case THREE_OF_A_KIND:
            strcpy(str,"Three of a Kind");
            break;
        case STRAIGHT:
            strcpy(str,"Straight");
            break;
        case FLUSH:
            strcpy(str,"Flush");
            break;
        case FULL_HOUSE:
            strcpy(str,"Full House");
            break;
        case FOUR_OF_A_KIND:
            strcpy(str,"Four of a Kind");
            break;
        case STRAIGHT_FLUSH:
            strcpy(str,"Straight Flush");
            break;
        case ROYAL_FLUSH:
            strcpy(str,"Royal Flush");
            break;
    }
    return(str);
}

/*********************************************
 This routine allows players to enter the game
**********************************************/
void join_game()
{
    int num,x=0,i=0;
    char str[256];

    memset(node,'\0',MAX_NODES);
    sprintf(str,"gamestat.%d",cur_table);
    if (!fexist(str)) {
        if(!(options[cur_table]&COMPUTER))
            bprintf("\r\n\1r\1hNOTICE: No computer player available at this "
                    "table!\r\n\1n");
        if(options[cur_table]&PASSWORD && (!noyes("\r\nPassword protect this "
            "table"))) {
            bprintf("\1b\1hEnter an 8 (or less) character password: ");
            if(getstr(str,8,K_UPPER)) strcpy(password,str);
            else password[0]=0;
        } else password[0]=0;
        node[node_num-1]=WAITING; cur_node=node_num; total_players=1;
        current_bet=0; dealer=node_num; stage=OPEN;
        sprintf(str,"gamestat.%d",cur_table);
        if (!fexist(str)) {
            put_game_status(-1); put_player(node_num);
            return;
        } else
            bprintf("\r\n\1r\1hOOPS, someone joined this table while you were "
                    "entering a password!\r\n");
	}
    get_game_status(cur_table);
    if (total_players==6) {
        bprintf("\r\n\r\nThere are already 6 players at this table!");
        return;
    }
    if (password[0] && !SYSOP) {
        bprintf("\1b\1hPassword: ");
        if (!getstr(str,8,K_UPPER)) return;
        if (stricmp(str,password)) {
            bprintf("\r\n\1r\1h\1iINCORRECT!");
            return;
        }
    }

    if (password[0] && SYSOP) {
        if (!yesno("\r\nThis table is password protected, enter anyway"))
            return;
    }

    for (num=1;num<=sys_nodes;num++)    /* Make SURE someone is playing */
        if (node[num-1]) { x=1; break; }

    if (!x) {                    /* The file is there, but nobody's home */
        node[node_num-1]=WAITING; cur_node=node_num; total_players=1;
        current_bet=0; dealer=node_num; stage=OPEN;
        put_game_status(-1); put_player(node_num);
		return;
	}
    bprintf("\r\n\r\n\1r\1hHang on!  \1b\1hWaiting to enter game! "
            "(\1c\1h^A to Abort\1b\1h)\r\n");
    while (stage!=OPEN) {
        if ((i=inkey(0))!=0) {
        if (i==1)
            return;
        }
        mswait(100);
        get_game_status(cur_table);
	}

    for (num=1;num<=sys_nodes;num++) {
        if (node[num-1] && node[num-1]!=WAITING) {
            node[num-1]=WAITING;
        }
    }
    put_game_status(-1);
    sprintf(str,"message.%d",node_num);     /* Kill off any stray messages */
    unlink(str);
    put_player(node_num);
    get_game_status(cur_table);
    total_players++;
    dealer=cur_node=node_num;       /* The new guy is the dealer */
    node[node_num-1]=WAITING;
    put_game_status(node_num-1);
    sprintf(str,"\r\n\1n\1gNode %2d: \1h%s \1n\1ghas entered the game.\1n"
            ,node_num,user_name);
    send_all_message(str,0);
    bprintf("\r\n\1m\1hYou are now the dealer!\r\n");
    sprintf(str,"\r\n\1n\1gNode %2d: \1h%s \1n\1gis now the dealer.",dealer,
            username(temp_num));
    send_all_message(str,0);
    show_players(0);
}

/************************************************************************
 This is part of the game joining process.  Here a user selects the table
 he/she wishes to "sit" at.  Returns a 1 if they are going to play or a
 0 if they are quitting.
*************************************************************************/
int select_table()
{
    int ch;

    /* first show all tables & players at each (& if full or not)
       then allow to list players at a particular table by typing in
       that table's number.  Allow to join a table that is not full */

    show_tables();
    while(1) {
        mnemonics("\r\n\r\n~List players on a table\r\n~Table status");
        mnemonics("\r\n~# Join table #\r\n~Quit to main menu\r\n");
        bprintf("\r\n\1h\1mYour selection: ");
        ch=getkeys("ltq",num_tables);

        if (ch&0x8000) {
            ch&=~0x8000;
            cur_table=ch;
            if (cur_table<1 || cur_table>num_tables) {
                bprintf("\r\n\1r\1hInvalid Table Number!\1n"); return 0;
            }
            else { return 1; }
        }
        if (ch=='L') {
            bprintf("\r\n\1b\1hWhich table [\1c1-%d\1b]: ",num_tables);
            show_players(getnum(num_tables));
        }
        if (ch=='T') show_tables();
        if (ch=='Q') return 0;
    }
}

/************************************************************
 This function shows the status of all the tables in the game
*************************************************************/
void show_tables()
{
    char str[256],statstr[256];
    int x;

    bprintf("\1y\1h");
    bprintf("\r\n Table     Number of    Bet    Table  Table  Table");
    bprintf("\r\n Number     Players    Limit   Limit  Ante   Status\r\n");
    for (x=1;x<=num_tables;x++) {
        sprintf(str,"gamestat.%d",x);
        if (fexist(str)) {
            get_game_status(x);
            strcpy(statstr,"\1b\1h[\1cIn Play\1b]");
            if(stage==OPEN)
                strcpy(statstr,"\1b\1h[\1cOpen\1b]");
            if(total_players==6)
                strcpy(statstr,"\1b\1h[\1cClosed\1b]");
            if(password[0])
                strcat(statstr," [\1rPW Lock\1b]");
            else
                strcat(statstr,"          ");
            if(!(options[x]&COMPUTER))
                strcat(statstr," [\1rNo Comp Player\1b]");
            bprintf("\r\n\1m\1hTable %-2d: \1n\1m %-2d Players  "
                    "\1b\1h[\1y%-4d\1b] [\1y%-5ld\1b] [\1y%-4d\1b] %s"
                    ,x,total_players,bet_limit[x],max_total[x],ante[x]
                    ,statstr);
        } else {
            bprintf("\r\n\1m\1hTable %-2d: \1n\1m No Players  \1b\1h"
                    "[\1y%-4d\1b] [\1y%-5ld\1b] [\1y%-4d\1b] [\1cOpen\1b]"
                    ,x,bet_limit[x],max_total[x],ante[x]);
            if(!(options[x]&COMPUTER))
                bprintf("           [\1rNo Comp Player\1b]");
        }
    }
    sprintf(str,"gamestat.%d",cur_table);
    if (cur_table && fexist(str))
        get_game_status(cur_table);
}
/**************************************************************************/
/* This function writes the computer log, used when logging single player */
/* games (v. the computer) and when logging 'skimming'                    */
/**************************************************************************/
void computer_log(long points)
{
    char str[256],temp[256];
    int file;
    long l;
    time_t now;
	struct tm *tp;

    now=time(NULL); /* Creates log to show wins/losses for the day */
	tp=localtime(&now);
    sprintf(str,"%02d%02d%02d.log",tp->tm_mon+1,tp->tm_mday
        ,tp->tm_year%100);
    if ((file=nopen(str,O_RDWR|O_CREAT))==-1)
        printf("Error opening %s\r\r\n",str);
    else {
        read(file,temp,filelength(file));
        temp[filelength(file)]=0;
        l=atol(temp);
        close(file);
        l+=points;
        if ((file=nopen(str,O_WRONLY|O_TRUNC))==-1)
            printf("Error opening %s\r\r\n",str);
        else {
            sprintf(temp,"%ld",l);
            write(file,temp,strlen(temp));
            close(file);
        }
    }
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

/***********************************************************************
 This function figures out who the winner of the game is, and splits the
 pot in the case of a tie (rare)
************************************************************************/
void declare_winner()
{
    char str[256];
    int highrank=0, highdup1=0, highdup2=0, winner[128], num,
        x=0, compare;
    ulong skim_amt;

    get_game_status(cur_table);

    if (!total_pot) {                   /* if everyone folds and no pot */
        sprintf(str,"\r\n\r\n\1r\1h\1iEveryone has folded!  Re-deal!\1n");
        bprintf(str);
        send_all_message(str,0);
        for (num=0;num<sys_nodes;num++)
            if (node[num]) node[num]=WAITING;
        stage=OPEN; cur_node=dealer; put_game_status(-1); return;
    }
    /* if set to skim the pot, take away amount now from total pot */
    if (skim) {
        skim_amt=((total_pot*skim)/100L);
        sprintf(str,"\r\n\r\n\1y\1h%ldK \1mof credits goes to the house.\r\n",
                skim_amt);
        send_all_message(str,node_num); bprintf(str);
        total_pot=total_pot-skim_amt; put_game_status(-1);
        if (create_log) computer_log(skim_amt);
    }
    if (total_players==1) { single_winner(); return; }
    for (num=1;num<=sys_nodes;num++) {
        if (node[num-1] && node[num-1]!=FOLDED) {
            get_player(num);
            if (rank>highrank) {
                highrank=rank; highdup1=dupcard1;
                highdup2=dupcard2; winner[0]=compare=num;
            }
        }
    }
    for (num=1;num<=sys_nodes;num++) {          /* Check to see if anyone  */
        if (node[num-1]&&node[num-1]!=FOLDED) { /* has duplicate hands     */
            get_player(num);
            if (rank==highrank && num!=compare) {
                if (rank==ROYAL_FLUSH)
                    winner[++x]=num;
                if (rank==ONE_PAIR || rank==THREE_OF_A_KIND ||
                    rank==FOUR_OF_A_KIND || rank==FULL_HOUSE) {
                    if (dupcard1>highdup1) {
                        winner[0]=num; highdup1=dupcard1;
                    }
                }
                if (rank==TWO_PAIR) {
                    if ((dupcard1>highdup1 && dupcard1>highdup2) ||
                        (dupcard2>highdup1 && dupcard2>highdup2)) {
                        winner[0]=num; highdup1=dupcard1; highdup2=dupcard2;
                    }
                }
                if (rank==NOTHING || rank==STRAIGHT || rank==FLUSH ||
                    rank==STRAIGHT_FLUSH || (rank==ONE_PAIR &&
                    dupcard1==highdup1)) {
                    if (hand_total(winner[0])==hand_total(num))
                        winner[++x]=num;
                    if (hand_total(num)>hand_total(winner[0]))
                        winner[0]=num;
                }
            }
        }
    }

    if (!x || x==1) {
        show_all_hands(winner[0]);
        if (!winner[0] || winner[0]>sys_nodes) winner[0]=cur_node;
        get_player(winner[0]);
        temppts=temppts+(1024L*total_pot);
        if (node_num==winner[0]) { newpts=temppts; moduser(); }
        put_player(winner[0]);
        sprintf(str,"\r\n\r\n\1b\1hThe winner receives \1c\1h%luK\1b\1h of "
                "credits!\1n",total_pot);
        if (node_num!=winner[0])
            bprintf(str);
        send_all_message(str,winner[0]);
        sprintf(str,"\r\n\r\n\1b\1hYou win \1c\1h%luK\1b\1h of credits!\1n"
                ,total_pot);
        if (node_num==winner[0])
            bprintf(str);
        else
            send_player_message(winner[0],str);
    } else {
        show_all_hands(-1);
        sprintf(str,"\r\n\r\n\1b\1hThere was a \1c\1h%d \1b\1hway \1b\1htie!"
                ,x);
        bprintf(str);
        send_all_message(str,0);
        total_pot=total_pot/x;
        for (num=0;num<x;num++) {
            get_player(winner[num]);
            sprintf(str,"\1g\1h%s wins \1c\1h%luK\1b\1h of credits\1n\r\n",
                    username(temp_num),total_pot);
            if (node_num!=winner[num])
                bprintf(str);
            send_all_message(str,winner[num]);
            sprintf(str,"\1b\1hYou win \1c\1h%luK\1b\1h of credits\1n\r\n"
                    ,total_pot);
            if (node_num==winner[num])
                bprintf(str);
            else
                send_player_message(winner[num],str);
            temppts=temppts+(1024L*total_pot); put_player(winner[num]);
        }
    }

    get_game_status(cur_table);
    for (num=1;num<=sys_nodes;num++)
        if (node[num-1]) { node[num-1]=WAITING;
            get_player(num); player_bet=0; rank=0;
            for (x=0;x<5;x++) hand[num-1][x].value=0;
            put_player(num);
        }
    if (total_players>1) {
        for(num=dealer+1;num<=sys_nodes;num++)  /* search for the next dealer */
            if(node[num-1])                     /* that is active */
                break;
        if(num>sys_nodes) {                     /* if no higher active nodes, */
            for(num=1;num<dealer;num++)         /* start at bottom and go up  */
                if(node[num-1])
                    break;
            if(num==cur_node)             /* if no active nodes found       */
                cur_node=dealer;          /* make current player the dealer */
            else
                cur_node=dealer=num; }
        else
            cur_node=dealer=num;
    } else {
        cur_node=dealer=node_num;
    }

    total_pot=current_bet=0; firstbid=0; stage=OPEN;
    put_game_status(-1);
    get_player(cur_node);
    sprintf(str,"\r\n\r\n\1n\1gNode %2d: \1h%s \1n\1gis now the dealer.",dealer
            ,username(temp_num));
    bprintf(str);
    send_all_message(str,0);
}

#endif              /* End of files needed for dpclean */

/*****************************************************
 This function reads in the current status of the game
******************************************************/
void get_game_status(int tablenum)
{
    char str[256];
    int file;

    memset(node,'\0',MAX_NODES);
    password[0]=0;

    sprintf(str,"gamestat.%d",tablenum);
    if ((file=nopen(str,O_RDONLY))==-1) {
        sprintf(str,"Couldn't open gamestat.%d for READ",tablenum);
        bputs(str);
        return;
    }
    read(file,password,8);
    read(file,&total_players,sizeof(total_players));
    read(file,&dealer,sizeof(dealer));
    read(file,&cur_node,sizeof(cur_node));
    read(file,&firstbid,sizeof(firstbid));
    read(file,&stage,sizeof(stage));
    read(file,&current_bet,sizeof(current_bet));
    read(file,&total_pot,sizeof(total_pot));
    read(file,node,sys_nodes);
    close(file);
}

/**************************************************************************
 This function writes the current game status to a file, if a specific node
 (specnode) is specified, it will write only that particular nodes status
***************************************************************************/
void put_game_status(int specnode)
{
    char str[256];
    int file;

    sprintf(str,"gamestat.%d",cur_table);
    if ((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
        bputs("Couldn't open GAMESTAT for WRITE");
        return;
    }
    write(file,password,8);
    write(file,&total_players,sizeof(total_players));
    write(file,&dealer,sizeof(dealer));
    write(file,&cur_node,sizeof(cur_node));
    write(file,&firstbid,sizeof(firstbid));
    write(file,&stage,sizeof(stage));
    write(file,&current_bet,sizeof(current_bet));
    write(file,&total_pot,sizeof(total_pot));
    if (specnode>=0 && specnode<sys_nodes) {
        lseek(file,specnode,SEEK_CUR);
        write(file,&node[specnode],1);
    } else write(file,node,MAX_NODES);      /* Was 'sys_nodes' length */
    close(file);
}

/**********************************************************************
 This function reads a players hand along with it's ranking from a file
***********************************************************************/
void get_player(int player)
{
    int file;
    char str[256];

    sprintf(str,"player.%d",player);
    if((file=nopen(str,O_RDONLY))==-1) {
    bprintf("get_player: Error opening '%s'\r\n",str);
        return; }
    read(file,&cur_table,sizeof(cur_table));
    read(file,&temp_num,sizeof(temp_num));
    read(file,&player_bet,sizeof(player_bet));
    read(file,&temppts,sizeof(temppts));
    read(file,&rank,sizeof(rank));
    read(file,&dupcard1,sizeof(dupcard1));
    read(file,&dupcard2,sizeof(dupcard2));
    read(file,&hicard,sizeof(hicard));
    read(file,hand[player],5*sizeof(card_t));
    close(file);
}

/*************************************************
 This function sends messages to player number plr
**************************************************/
void send_player_message(int plr, char *msg)
{
    int file;
    char fname[81];

    sprintf(fname,"message.%d",plr);

    if ((file=nopen(fname,O_WRONLY|O_APPEND|O_CREAT))==-1) {
        bputs("Couldn't open message.xxx for WRITE");
        return;
    }
    write(file,msg,strlen(msg));
    close(file);
}

/**********************************************
 Sends redundant message to all necessary nodes
***********************************************/
void send_all_message(char *msg,int exception)
{
    int num;

    for (num=1;num<=sys_nodes;num++) {
        if (node[num-1] && (num!=node_num) && (num!=exception)) {
            send_player_message(num,msg);
        }
    }
}
/***********************************************************************
 This is where the player exits the game, and the game does its clean-up
 functions
************************************************************************/
void quit_out()
{
    char str[256];
    int num,file;
    long xfer_pts;
    ulong x;

    xfer_pts=(newpts-user_cdt)-xfer_pts2;
    xfer_pts2+=xfer_pts;
    xfer_pts=xfer_pts/1024L;
    if (xfer_pts!=0) {
        x=time_played/60;
        sprintf(str,"%-25.25s",user_name);
        if ((file=nopen("dpoker.plr",O_WRONLY|O_CREAT|O_APPEND))!=-1) {
            write(file,str,25);
            write(file,&x,sizeof(x));
            write(file,&xfer_pts,sizeof(xfer_pts));
            close(file);
        }
    }
    xfer_pts=0;

    sprintf(str,"message.%d",node_num);
    unlink(str);
    sprintf(str,"player.%d",node_num);
    unlink(str);
    get_game_status(cur_table);
    if (node[node_num-1]>0 && node[node_num-1]<6) {
        node[node_num-1]=0; --total_players;
        put_game_status(-1);
        sprintf(str,"message.%d",node_num);
        unlink(str);
        sprintf(str,"player.%d",node_num);
        unlink(str);
        sprintf(str,"\r\n\1n\1gNode %2d: \1h%s \1n\1ghas left the game.\1n"
                ,node_num,user_name);
        send_all_message(str,0);
        if (!total_players) {
            sprintf(str,"gamestat.%d",cur_table);
            unlink(str);
            return;
        } else {
            if (dealer==node_num) {
                for (num=1;num<=sys_nodes;num++) {
                    if (node[num-1]==1) {           /* 1 == WAITING */
                        dealer=num; if (cur_node==node_num) cur_node=num;
                        if (total_players==1) { current_bet=0; stage=0; }
                        put_game_status(-1); get_player(num);
                        sprintf(str,"\r\n\1y\1h%s \1m\1his the new dealer."
                                ,username(temp_num));
                        bprintf(str);
                        send_all_message(str,num);
                        sprintf(str,"\r\n\1m\1hYou are now the dealer.");
                        send_player_message(num,str);
                        return;
                    }
                }
            }
        }
    }
}
