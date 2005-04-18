#ifdef TODO_HEADERS
#include<process.h>
#include<dos.h>
#endif
#include<time.h>
#include<stdio.h>
#include<string.h>
#include<ciolib.h>
#include<stdlib.h>
#include<OpenDoor.h>
#include<genwrap.h>
#define SD_GREEN  0x0a
#define SD_CYAN 0x0b
#define SD_RED  0x0c
#define SD_YELLOW 0x0e
extern void     __REG__main(void);
extern int      registered;
extern int      thishostcount;
extern void     __SET__main(void);
extern void     __mess(int x);
extern void     detectwin(void);
extern void     __CNV__main(void);
extern void     __DAY__main(void);
extern void     __NEW__main(void);
extern void     wingame(void);
extern void     wongame(void);
extern void     killsmurf(int numba);
extern void     treathostage(int mere);
extern void     what(void);
extern void     __hit(int hit);
extern void     __ehit(int hit, int numba);
extern void     increasemorale(void);
extern void     deductmorale(void);
extern void     deductettemorale(void);
extern void     __etteit(int a, int b, int c, unsigned long d);
extern void     __incite(int x, int c, int y);
extern void     killeduser(void);
extern void     toomany(void);
extern void     abilities(void);
extern void     stealmoney(void);
extern void     spy(void);
extern void     inciterevolt(void);
extern void     service(void);
extern void     givemoney(void);
extern void     rva(int sel__);
extern void     ettemenu(void);
extern void     rendezvous(void);
extern void     counthostages(void);
extern void     asksex(void);
extern void     detectsave(void);
extern void     detectversion(void);
extern void     hostagemenu(void);
extern void     mustregister(void);
extern void     tolocal(void);
extern void     checkkey(void);
extern void     automate(void);
extern void     writehostage(void);
extern void     phostage(void);
extern void     notdone(void);
extern void     registerme(void);
extern void     heal(void);
extern void     lastquote(void);
extern void     userarena(void);
extern void     arena(void);
extern void     writeSGL(void);
extern void     ettelist(int high);
extern void     savegame(void);
extern void     checklevel(void);
extern void     loadgame(void);
extern void     maint(int level);
/* extern void maint(void); */
extern void     newplayer(int x);
extern void     buyitem(void);
extern void     buyconf(void);
extern void     getcommand(void);
extern void     titlescreen(void);
extern void     displaystatus(void);
extern void     checkstatus(void);
extern void     smurf_pause(void);
extern void     nl(void);
extern void     displaymenu(void);
extern void     itemlist(void);
extern void     conflist(void);
extern void     rankuserlist(int ink);
extern void     userlist(int ink);
extern void     changename(void);
extern void     changelast(void);
extern char     tempenform[80];
extern char     datasex[3][7];
extern char     dataref[3][4];
extern char     dataref2[3][6];
extern char     dataref3[3][4];
extern int      torturepoints, vpoints, etpoints;
extern int      showexit, numhost[99];
extern FILE    *stream;
extern char     registeredxx;	       /* 1 if registered, 0 if not */
extern char     registered_name[201];  /* Name of registered user */
extern int      inuser, cyc, bbsexit, thisuserno, noplayers, displayreal,
                cyc2, scrnlen, cyc3, enterpointer, epold, defturns, userhp,
                statis, hostage[1000], holder[1000], hcount;
extern char     logname[13];
extern char     power[41], weap[41], armr[41], ette[41], conf[41], host[41],
                shost[10], bbsinkey;
extern char     smurfname[99][41];
extern char     realname[99][41];
extern int      realnumb[99];
extern char     smurfweap[99][41];
extern char     smurfarmr[99][41];
extern char     smurftext[99][81];
extern char     smurfettename[99][41];
extern int      smurfettelevel[99];
extern int      smurfweapno[99];
extern int      smurfarmrno[99];
extern char     smurfconf[99][41];
extern int      smurfconfno[99];
extern int      smurfhost[99][10];
extern int      smurflevel[99];
extern float    smurfmoney[99];
extern float    smurfbankd[99];
extern float    smurfexper[99];
extern int      smurffights[99];
extern unsigned int smurfwin[99];
extern unsigned int smurflose[99];
extern int      smurfhp[99];
extern int      smurfhpm[99];
extern unsigned int smurfstr[99];
extern unsigned int smurfspd[99];
extern unsigned int smurfint[99];
extern unsigned int smurfcon[99];
extern unsigned int smurfbra[99];
extern unsigned int smurfchr[99];
extern int      smurfturns[99];
extern int      defrounds;
extern int      roundsleft;
extern int      turnsaday;
extern int      smurfspcweapon[99][6];
extern int      smurfqtyweapon[99][6];
extern char     smurfsex[99];
extern char    *enemieweap[10];
extern char    *defstartline[10];
extern int      eweapno[10][10];
extern int      earmrno[10][10];
extern char    *defenemie[10][10];
extern float    defcprices[11];
extern char    *defconfine[11];
extern char    *defweapon[21];
extern char    *defarmor[21];
extern float    defprices[21];
extern float    defexper[51];
extern char    *enemieweap[10];
extern char    *enemiearmr[10];
extern char    *ettenamez[20];
extern int      __morale[99];
extern int      __ettemorale[99];
extern char    *_morale[14];
extern char    *_ettemorale[12];
extern char    *__ahit[10];
extern char    *__amiss[10];
extern char    *__version;
extern char    *__vspace;
extern char    *__saveversion;
extern char    *__vnew;
extern char    *__vnewsp;
extern char    *__vday;
extern char    *__vdaysp;
extern char    *__vcnv;
extern char    *__vcnvsp;
extern char    *__vkey;
extern char    *__vkeysp;
extern char    *__vlog;
extern char    *__vlogsp;
extern void     newshit(char shitlog[80]);
extern void     logshit(int numba, char shitlog[80]);
