#ifdef TODO_HEADERS
#include<process.h>
#include<dos.h>
#endif
#include<time.h>
#include<OpenDoor.h>
#include<stdio.h>
#include<string.h>
#include<ciolib.h>
#include<stdlib.h>
#include<genwrap.h>
#include<datewrap.h>
#define SD_GREEN  0x0a
#define SD_CYAN   0x0b
#define SD_RED    0x0c
#define SD_YELLOW 0x0e
int             fix_mode = 0;	       /* 1 = patch to fix... */
int             corrupt_mode = 0;      /* 1 = Corrupt to force error */
unsigned        fix_value;
int             patch_overide = 1;     /* MSC uses the checksum area ?! */
int             thishostcount = 0;
void            __REG__main(void);
extern int      registered;
void            __SET__main(void);
void            __mess(int x);
void            detectwin(void);
void            __CNV__main(void);
void            __DAY__main(void);
void            __NEW__main(void);
void            wingame(void);
void            wongame(void);
void            killsmurf(int numba);
void            what(void);
void            __hit(int hit);
void            __ehit(int hit, int numba);
void            increasemorale(void);
void            deductmorale(void);
void            deductettemorale(void);
void            __etteit(int a, int b, int c, unsigned long d);
void            __incite(int x, int c, int y);
void            killeduser(void);
void            toomany(void);
void            abilities(void);
void            stealmoney(void);
void            spy(void);
void            inciterevolt(void);
void            service(void);
void            givemoney(void);
void            rva(int sel__);
void            ettemenu(void);
void            rendezvous(void);
void            killsmurf(int numba);
void            treathostage(int mere);
void            counthostages(void);
void            asksex(void);
void            detectsave(void);
void            detectversion(void);
void            hostagemenu(void);
void            mustregister(void);
void            tolocal(void);
void            checkkey(void);
void            automate(void);
void            writehostage(void);
void            phostage(void);
void            notdone(void);
void            registerme(void);
void            heal(void);
void            lastquote(void);
void            userarena(void);
void            arena(void);
void            writeSGL(void);
void            ettelist(int high);
void            savegame(void);
void            checklevel(void);
void            loadgame(void);
void            maint(int level);
void            newplayer(int x);
void            buyitem(void);
void            buyconf(void);
void            getcommand(void);
void            titlescreen(void);
void            displaystatus(void);
void            checkstatus(void);
void            smurf_pause(void);
void            nl(void);
void            displaymenu(void);
void            itemlist(void);
void            conflist(void);
void            rankuserlist(int ink);
void            userlist(int ink);
void            changename(void);
void            changelast(void);
int             torturepoints, vpoints, etpoints;
int             showexit, numhost[99];
FILE           *stream;
char            registeredxx = 0;      /* 1 if registered, 0 if not */
char            registered_name[201];  /* Name of registered user */
int             inuser, cyc, bbsexit, thisuserno, noplayers, displayreal,
                cyc2, scrnlen, cyc3, enterpointer, epold, defturns = 3,
                userhp, statis = 0, hostage[1000], holder[1000], hcount;
char            tempenform[80];
char            logname[13];
char            power[41], weap[41], armr[41], ette[41], conf[41], host[41],
                shost[10], bbsinkey;
char            smurfname[99][41];
char            realname[99][41];
int             realnumb[99];
char            smurfweap[99][41];
char            smurfarmr[99][41];
char            smurftext[99][81];
char            smurfettename[99][41];
int             smurfettelevel[99];
int             smurfweapno[99];
int             smurfarmrno[99];
char            smurfconf[99][41];
int             smurfconfno[99];
int             smurfhost[99][10];
int             smurflevel[99];
float           smurfmoney[99];
float           smurfbankd[99];
float           smurfexper[99];
int             smurffights[99];
unsigned int    smurfwin[99];
unsigned int    smurflose[99];
int             smurfhp[99];
int             smurfhpm[99];
unsigned int    smurfstr[99];
unsigned int    smurfspd[99];
unsigned int    smurfint[99];
unsigned int    smurfcon[99];
unsigned int    smurfbra[99];
unsigned int    smurfchr[99];
int             smurfturns[99];
int             defrounds;
int             roundsleft;
int             turnsaday;
int             smurfspcweapon[99][6];
int             smurfqtyweapon[99][6];
char            smurfsex[99];
int             __morale[99];
int             __ettemorale[99];
void            killsmurf(int numba);

char           *ettenamez[20] = {
    "Jennifer",
    "Cassie",
    "Sassy",
    "Wanda",
    "Michelle",
    "Elizabeth",
    "Animal Babe",
    "Estelle",
    "Jenny",
    "Bunga Babe",
    "Rasta Babe",
    "Troopet",
    "Sophisticette",
    "Teacher Ette",
    "Waiter Ette",
    "Play Thing",
    "Flirt Ette",
    "Fatty",
    "Perfect Ette",
    "Joanna"
};

char           *enemieweap[10] = {
    "Feet", "Body", "Bite", "Claws", "H2O Uzi",
"Rubber Band", "H2O Uzi", "Uzi", "Ice-T Album", "Stick"};

char           *enemiearmr[10] = {
    "No Armor", "Smurf Attire", "I'm a Vet Blazer", "Middle Finger", "Cyber Armor",
    "Fission Suit", "Fusion Suit", "WWIV Suit", "Blessed Pendant", "No Fear Clothes"
};

char           *defstartline[10] = {
    "fires H2O at you!", "has you in his sights!", "bombards you with seeds!",
    "is charging you!", "has you in his sights!", "launches a band at you!",
    "rockets through the air!", "challenges you!", "turns up the bass", "flys into the arena!"
};

int             eweapno[10][10] = {
    0, 1, 0, 0, 0, 1, 100, 100, 100, 2,
    1, 3, 3, 3, 2, 2, 0, 2, 3, 3,
    5, 4, 5, 4, 5, 5, 4, 101, 6, 5,
    103, 102, 7, 7, 6, 7, 7, 7, 7, 103,
    8, 9, 9, 9, 8, 8, 9, 9, 8, 104,
    12, 10, 10, 16, 12, 9, 10, 9, 7, 7,
    104, 103, 14, 102, 13, 9, 12, 14, 14, 8,
    103, 16, 100, 15, 16, 9, 12, 8, 8, 12,
    7, 10, 8, 17, 17, 10, 11, 17, 18, 15,
    17, 17, 101, 18, 18, 17, 16, 16, 15, 16
};

int             earmrno[10][10] = {
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 109, 109, 109, 109
};

void            newshit(char shitlog[80]);
void            logshit(int numba, char shitlog[80]);
