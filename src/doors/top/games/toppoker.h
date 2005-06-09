
#define POKHAND_HIGHCARD      0x0000
#define POKHAND_ONEPAIR       0x1000
#define POKHAND_TWOPAIR       0x2000
#define POKHAND_THREEKIND     0x3000
#define POKHAND_STRAIGHT      0x4000
#define POKHAND_FLUSH         0x5000
#define POKHAND_FULLHOUSE     0x6000
#define POKHAND_FOURKIND      0x7000
#define POKHAND_STRAIGHTFLUSH 0x8000
#define POKHAND_ROYALFLUSH    0x800D

#define POKRULE_ACEHIGH         0x01
#define POKRULE_ACELOW          0x02
#define POKRULE_USEJOKERS       0x04

#define POKGAME_5CARDDRAW          0

#define POKPL_WAITING              0
#define POKPL_PLAYING              1
#define POKPL_JOINED             100

#define POKSTG_BET                0
#define POKSTG_DISCARD            1
#define POKSTG_FINALBET           2
#define POKSTG_ENDCYCLE           3     /* Used by turn advance function. */
#define POKSTG_ANYBET           100     /* Used for turn checking. */
#define POKSTG_ANYTURN          110     /* Used for turn checking. */

#define POKERMAXGAMES          32767
#define POKERMAXCARDS             10
#define POKERMAXPLAYERS           10
#define POKERMAXDECK              54

typedef struct poker_plyr_str
    {
    XINT          node;
    char          cards[POKERMAXCARDS];
    unsigned long totalbet;
    unsigned long origcash;
    unsigned long winchksum;
    } poker_plyr_typ;

typedef struct poker_game_str
    {
    poker_plyr_typ player[POKERMAXPLAYERS];
    char          inuse;
    char          type;
    XINT          dealer;
    unsigned char rules;
    unsigned long minbet;
    unsigned long maxbet;
    unsigned long ante;
    XINT          maxrounds;
    char          maxcycles;
    char          minpl;
    char          maxpl;
    char          minai;
    char          maxai;
    char          mindisc;
    char          maxdisc;
    char          wildcards[14];
    unsigned char decksize;
    unsigned char numcards;
    XINT          delaytime;
    XINT          waittime;
    char          held;
    char          speedup;
    XINT          wanttoplay[POKERMAXPLAYERS];
    XINT          wtpcount;
    XINT          plcount;
    time_t        eventtime;
    char          cycle;
    char          stage;
    XINT          round;
    char          turn;
    unsigned long pot;
    unsigned long highbet;
    char          numcalls;
    char          deck[POKERMAXDECK];
    char          cardvals[POKERMAXDECK];
    char          cardsuits[POKERMAXDECK];
    char          started[POKERMAXPLAYERS];
    char          finished[POKERMAXPLAYERS];
    char          winners[POKERMAXPLAYERS];
    } poker_game_typ;

extern void poker_game(XINT pgwords);
extern void poker_core(XINT msggam, byte *msgstr);
extern void poker_eventkernel(XINT gamenum);
extern XINT poker_startnewgame(poker_game_typ *sgame);
extern void poker_initnew(poker_game_typ *newgame);
extern void poker_resetgame(poker_game_typ *rgame);
extern void poker_dealhand(poker_game_typ *dhgame, char *cardbuf);
extern void poker_dealcard(poker_game_typ *dgame, char *dcard);
extern void poker_shuffle(poker_game_typ *shufgame);
extern char poker_joingame(XINT gamenum, poker_game_typ *jgame);
extern void poker_foldplayer(XINT fpnode, XINT fpnum, 
                             poker_game_typ *fpgame);
extern char XFAR *poker_gethandname(poker_game_typ *hgame, char *handdat);
extern char XFAR *poker_getshortname(poker_game_typ *hgame, char *handdat);
extern XINT poker_findwinners(XINT fwnode, unsigned XINT *fwwin,
                              poker_game_typ *fwgame);
extern unsigned XINT poker_scorehand(poker_game_typ *sgame, char *shand);
extern unsigned long poker_rawscorehand(poker_game_typ *sgame, char *shand);
extern void poker_sorthand(poker_game_typ *sgame, char *shand);
extern XINT poker_getplayernum(char gpntype, XINT gpnnode,
							   poker_game_typ *gpngame);
extern XINT poker_gethighplayer(char ghptype, poker_game_typ *ghpgame);
extern void poker_displayhand(poker_game_typ *dhgame, char *dhhand);
extern void poker_endgamedisplay(poker_game_typ *egame);
extern char poker_checkturn(char ctstage, XINT ctnode,
							poker_game_typ *ctgame);
extern void poker_advanceturn(XINT atnum, poker_game_typ *atgame);
extern char poker_loadgame(XINT gamenum, poker_game_typ *gamebuf);
extern char poker_savegame(XINT gamenum, poker_game_typ *gamebuf);
extern void poker_lockgame(XINT gamenum);
extern void poker_unlockgame(XINT gamenum);
extern void poker_sendmessage(XINT smtype, char smecho, XINT smnum,
                              char smplayonly, unsigned char *smstr,
							  poker_game_typ *smgame);

extern char XFAR *pokerlockflags;
extern time_t XFAR *pokeretimes;
extern XINT XFAR *pokerdtimes;
extern XINT XFAR *pokerwtimes;
extern char XFAR *pokeringame;

extern XINT pokermaingame;

extern char XFAR pokerdefcardvals[POKERMAXDECK];
extern char XFAR pokerdefcardsuits[POKERMAXDECK];
