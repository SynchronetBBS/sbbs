void ParseCmdLine(int argc, char **argv);
void AnsPrint(char *a);
void DisplayText(char *a);
void DisplayTitle(char *a);
void ExitGame(void);
void InitPort(void);
char *KbIn(char *dest, long Lmax);
void Ocls(void);
void Oprint(char *a);
char *Owait(char *dest);
char *PickRandom(char *dest, char *a);
void Position (short x, short y);
void PressAnyKey(void);
void UCase(char *str);
void ReadLinkTo(void);
void SetColor(short f, short b, short oeo);
void Paus(long x);

// COMMON SHARED MinsLeft AS LONG, SecsLeft AS LONG, Rvects AS INTEGER, Parity%
extern long MinsLeft;
extern long SecsLeft;
extern short Rvects;
extern short Parity;
// COMMON SHARED OldBg AS INTEGER, OldFg AS INTEGER, StatsIn AS INTEGER
extern short OldBg;
extern short OldFg;
extern short StatsIn;
// COMMON SHARED PortNum AS INTEGER, OldTime AS LONG, Rate&, Length%
extern short PortNum;
extern long OldTime;
extern long Rate;
extern long KeepRate;
extern short Length;
// COMMON SHARED ret$, NewColor$, GraphMode AS INTEGER, HS%, IRQ%, Port%
#define ret "\r\n"
extern char NewColor[16];
extern short GraphMode;
extern short HS;
extern short IRQ;
extern short Port;
// COMMON SHARED NodeNum AS INTEGER
extern short NodeNum;
// COMMON SHARED PlayingNow AS INTEGER, ChatEnabled AS INTEGER, FossAct%, DBits%
//                       ' when PlayingNow is set to 1 (by code) then the status
//                       ' line begins to be displayed after every Ocls
extern short PlayingNow;
extern short ChatEnabled;
extern short FossAct;
extern short DBits;
// COMMON SHARED Oldx1 AS INTEGER, Oldx2 AS INTEGER, Oldx3 AS INTEGER
extern short Oldx1;
extern short Oldx2;
extern short Oldx3;
