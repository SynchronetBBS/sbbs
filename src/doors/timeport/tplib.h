#include <stdio.h>
#include <sys/file.h>
#include <sys/param.h>

void AddToMail(short x, char *e);
void DelOldFromNameList(void);
char *FixTheString(char *dest, char *a, char *b, char *c);
char *GetCommand(char *dest);
char *GetYesNo(char *dest);
void InitPlayer(void);
void Inventory(void);
void ListItems(float ee);
void ListPlayers(void);
void LoadGame(void);
void LoadItems(char *a);
void MakeInputArea(short v);
char *MakeNode(char *dest, char *b);
char *Mid(char *dest, char *source, int start, int len);
void OpenInventory(void);
void OpenStats(char *a1);
char *Pad(char *dest, char *a, long l);
void ReadConfig(void);
void ReadItem(int n, int o);
char *RealName(char *dest, char *a);
char *RemoveColor(char *dest, char *te);
void RTrim(char *str);
void SaveInvItem(short x, char *c, long b1, long b2);
void SaveStats(char *a0, char *a1, int b1, int c1, char *d1, char *E1, char *f1, char g1, char h1, 
				int i1, int j1, int k1, char *l1, int m1, int n1, int o1, char *p1, char *q1, char *Thr);
void SaveStrStats(char *a0, char *a1, char *b1, char *c1, char *d1, char *E1, char *f1, char *g1, char *h1, 
				char *i1, char *j1, char *k1, char *l1, char *m1, char *n1, char *o1, char *p1, char *q1, char *Thr);
FILE *SharedOpen(const char *filename, const char *mode, int lockop);
short sip(void);
void ViewStats(void);
short WepNum(char a);
void WriteNews(char *e, short x);
void readline(char *dest, size_t size, FILE *file);
char *IsHeNew(char *dest);
char *FindName(char *dest, char *Rr);
short BuyStuff(char *v, char *q);
short FindTheLevel(long g);
short GetHitLev(long lx, long ly, float l1, float L2);
short GetMonHp(short l, float l1, float L2);
long GetValue(long Tv1, float l1, float L2);
short TalkToPress(void);
short HasItem(char *a);
#if defined(__linux__) || defined(__NetBSD__)
void srandomdev(void);
#endif

// DIM SHARED ch1$, ch2$, ch3$, ch4$, ch5$, Ch6$, Ch7$, Ch8$, Ch9$, ChRest$
extern char ch1[256];
extern char ch2[256];
extern char ch3[256];
extern char ch4[256];
extern char ch5[256];
extern char Ch6[256];
extern char Ch7[256];
extern char Ch8[256];
extern char Ch9[256];
extern char ChRest[256];
// DIM SHARED Ch10$, Ch11$, Ch12$, Ch13$, Ch14$, Ch15$, Ch16$, ch17$, Ch18$
extern char Ch10[256];
extern char Ch11[256];
extern char Ch12[256];
extern char Ch13[256];
extern char Ch14[256];
extern char Ch15[256];
extern char Ch16[256];
extern char ch17[256];
extern char Ch18[256];
// DIM SHARED RecLen AS INTEGER, DatPath$, BarBar$, PlayerNum AS INTEGER
extern short int RecLen;
extern char DatPath[MAXPATHLEN];
extern char *BarBar;
extern short int PlayerNum;
// DIM SHARED Today$, Yesterday$, Regis AS INTEGER, FirstAvail AS INTEGER
extern char Today[MAXPATHLEN];
extern char Yesterday[MAXPATHLEN];
extern short int Regis, FirstAvail;
// DIM SHARED RegisterTo1$, RegisterTo2$, MaxClosFight$, MaxPlayFight$
extern char RegisterTo1[256];
extern char RegisterTo2[256];
extern short MaxClosFight;
extern short MaxPlayFight;
// DIM SHARED RecordNumber AS INTEGER, OriginNumber AS INTEGER, Version$
extern short RecordNumber, OriginNumber;
extern char *Version;
// DIM SHARED Quor$, ItemCount AS INTEGER, Def$, User$, Alias$
// char Quor[256]; /* Not used */
extern char Def[256];
extern char User[256];
extern char Alias[256];
extern short ItemCount;
// DIM SHARED into(10, 2) AS LONG, ItemName$(10) 'stored to read
extern long int into[11][3];
extern char ItemName[11][21];
// DIM SHARED Invent$(8), inv(8, 2) AS LONG 'access to inventory
extern char Invent[9][256];
extern long int inv[9][3];
// DIM SHARED MaxBadTimes AS INTEGER, KickOut AS INTEGER
extern short MaxBadTimes;
extern short KickOut;

// Extras I need
extern char Prompt[128];

// Stuff common to all (May as well be in here eh?)
extern short Hit;
extern short Level;
extern char Bad[256];
extern char NotAgain[256];
extern char Weapon;
extern long Money;
extern char Defense;
extern long Stash;
extern long Exp;
extern char lastplay[9];
extern short ClosFight;
extern short PlayFight;
extern char LineRest[256];
extern char ExeLine[256];
extern char ComLine[256];
extern short PlayKill;
extern char sex[16];
extern char CurCode[256];

extern char *DefName[13];
extern char *WepName[13];
extern long WepCost[13];
extern long PlayerHP[13];
extern long WeaponAdd[13];
extern long DefenseAdd[13];
extern long MonMinHp[13];
extern long GoldFromEach[13];
extern long ExpFromEach[13];
extern long UpToNext[12];
extern long HealAHit[13];
extern char *Rank[13];
extern int MaximumPlayers;
extern char guardname[256];
extern char BribeGuard[256];
extern int Playercount;

/* All IGMs need these routines */
void UseItem(long c,short b);
void SaveGame(void);
