#include <ctype.h>
#include <signal.h>
#include <stdlib.h>

#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include "doors.h"
#include "tplib.h"

// COMMON SHARED MinsLeft AS LONG, SecsLeft AS LONG, Rvects AS INTEGER, Parity%
long MinsLeft;
long SecsLeft;
short Rvects;
short Parity;
// COMMON SHARED OldBg AS INTEGER, OldFg AS INTEGER, StatsIn AS INTEGER
short OldBg;
short OldFg;
short StatsIn;
// COMMON SHARED PortNum AS INTEGER, OldTime AS LONG, Rate&, Length%
short PortNum;
long OldTime;
long Rate;
long KeepRate;
short Length;
// COMMON SHARED ret$, NewColor$, GraphMode AS INTEGER, HS%, IRQ%, Port%
#define ret "\r\n"
char NewColor[16];
short GraphMode;
short HS;
short IRQ;
short Port;
// COMMON SHARED NodeNum AS INTEGER
short NodeNum;
// COMMON SHARED PlayingNow AS INTEGER, ChatEnabled AS INTEGER, FossAct%, DBits%
//                       ' when PlayingNow is set to 1 (by code) then the status
//                       ' line begins to be displayed after every Ocls
short PlayingNow=0;
short ChatEnabled=0;
short FossAct;
short DBits;
// COMMON SHARED Oldx1 AS INTEGER, Oldx2 AS INTEGER, Oldx3 AS INTEGER
short Oldx1=0;
short Oldx2=0;
short Oldx3=0;

int CarrierLost=0;

struct termios origterm;

void ParseCmdLine(int argc, char **argv)
{
	int x;

	NodeNum=-1;
	for(x=1;x<argc;x++)
	{
		if(argv[x][0]=='N')
		{
			NodeNum=atoi(argv[x]+1);
			break;
		}
	}

	if(NodeNum==-1)
	{
    	Ocls();
    	SetColor(15,0,0);
    	Oprint("Note: You must now pass a node number to Time Port, in the\n");
    	Oprint("      form Nx.  such as TIMEPORT N0 (for a single-node bbs) or\n");
    	Oprint("      TIMEPORT N1, TIMEPORT N2, TIMEPORT N3, TIMEPORT N18 and\n");
    	Oprint("      so on.  Read the .docs for info on setting this up.\n\n\n");
    	exit(1);
	}
}

void CloseComm(void)
{
	tcsetattr(0, TCSANOW, &origterm);
}


void AnsPrint(char *a)
{
}
/*  Local stuff... don't need it. */
/*
	int xz;
	char hot[2]={0,0};
	Yuk$ = ""

	for(xz=1; x<=strlen(a); x++)
	{
		if(PortNum && CarrierLost)
		{
			ExitGame();
			exit(1);
		}
		hot[0]=a[xz-1];
		IF hot$ = CHR$(8) THEN ntt = POS(n) - 1: IF ntt < 1 THEN ntt = 1
		IF hot$ = CHR$(8) THEN LOCATE CSRLIN, ntt: RETURN
		IF hot$ = CHR$(27) THEN Esc = 1: Yuk$ = CHR$(27): RETURN
		IF Esc THEN IF (hot$ >= "A" AND hot$ <= "Z") OR (hot$ >= "a" AND hot$ <= "z") THEN GOSUB ANSI: Esc = 0: Yuk$ = "": RETURN
		IF Esc THEN Yuk$ = Yuk$ + hot$: RETURN
		PRINT hot$;
		RETURN
	}
}

'****************************************************************************
REM Ansi-Translation routines: call to print very few ansi-escape codes
REM supports H, m, and J (LOCATE, COLOR, and CLS);
'****************************************************************************

ANSI:
Esc = 0
REM deduce the ansi code!
p1 = -1: p2 = -1: p3 = -1: p4 = -1

IF hot$ = "J" THEN CLS : Yuk$ = "": RETURN

IF hot$ <> "m" THEN GOTO NotM

p1 = -1: p2 = -1: p3 = -1: p4 = -1
IF hot$ = "m" THEN p1 = VAL(MID$(Yuk$, 3, 2))
nul1 = 0: nul2 = 0: nul3 = 0
nul1 = INSTR(Yuk$, ";")
IF nul1 THEN p2 = VAL(MID$(Yuk$, nul1 + 1, 2)) ELSE GOTO tzer
nul2 = INSTR(nul1 + 1, Yuk$, ";")
IF nul2 THEN p3 = VAL(MID$(Yuk$, nul2 + 1, 2)) ELSE GOTO tzer
tzer:
n = 0: t = 0

x1 = -1: x2 = -1: x3 = -1: x4 = -1
IF p1 = 0 OR p1 = 1 THEN x1 = p1 ELSE IF p1 >= 30 AND p1 < 40 THEN x2 = p1 ELSE IF p1 >= 40 THEN x3 = p1
IF p2 = 0 OR p2 = 1 THEN x1 = p2 ELSE IF p2 >= 30 AND p2 < 40 THEN x2 = p2 ELSE IF p2 >= 40 THEN x3 = p2
IF p3 = 0 OR p3 = 1 THEN x1 = p3 ELSE IF p3 >= 30 AND p3 < 40 THEN x2 = p3 ELSE IF p3 >= 40 THEN x3 = p3

IF x3 = -1 AND x2 = -1 AND x1 >= 0 THEN x2 = Oldx2: x3 = Oldx3
IF x1 = -1 THEN x1 = Oldx1 'dupj
IF x1 = 0 AND x2 = 0 AND x3 = 0 THEN COLOR 7, 0: Oldx1 = x1: Oldx2 = x2: Oldx3 = x3: RETURN
IF x1 = 0 THEN IF x2 = 30 THEN t = 0 ELSE IF x2 = 31 THEN t = 4 ELSE IF x2 = 32 THEN t = 2 ELSE IF x2 = 33 THEN t = 6 ELSE IF x2 = 34 THEN t = 1 ELSE IF x2 = 35 THEN t = 5 ELSE IF x2 = 36 THEN t = 3 ELSE IF x2 = 37 THEN t = 7
IF x1 = 1 THEN IF x2 = 30 THEN t = 8 ELSE IF x2 = 31 THEN t = 12 ELSE IF x2 = 32 THEN t = 10 ELSE IF x2 = 33 THEN t = 14 ELSE IF x2 = 34 THEN t = 9 ELSE IF x2 = 35 THEN t = 13 ELSE IF x2 = 36 THEN t = 11 ELSE IF x2 = 37 THEN t = 15
IF x3 = 40 THEN n = 0 ELSE IF x3 = 41 THEN n = 4 ELSE IF x3 = 42 THEN n = 2 ELSE IF x3 = 43 THEN n = 6 ELSE IF x3 = 44 THEN n = 1 ELSE IF x3 = 45 THEN n = 5 ELSE IF x3 = 46 THEN n = 3 ELSE IF x3 = 47 THEN n = 7
grk:
IF x3 >= 0 AND x2 >= 0 THEN COLOR t, n: OldFg = t: OldBg = n
IF x2 >= 0 AND x3 < 0 THEN COLOR t: OldFg = t
IF x3 >= 0 AND x2 < 0 THEN COLOR , n: OldBg = n
Oldx1 = x1
Oldx2 = x2
Oldx3 = x3

Yuk$ = "": RETURN

NotM:
IF UCASE$(hot$) <> "F" AND UCASE$(hot$) <> "H" THEN GOTO NotF
x1 = VAL(MID$(Yuk$, 3, 2))
x2 = VAL(MID$(Yuk$, INSTR(4, Yuk$, ";") + 1, 2))
LOCATE x1, x2
RETURN

NotF:
IF hot$ = "A" OR hot$ = "B" OR hot$ = "C" OR hot$ = "D" THEN v = VAL(MID$(Yuk$, INSTR(Yuk$, "[") + 1, 1)) ELSE GOTO NotA
a = CSRLIN: b = POS(n)
IF hot$ = "A" THEN LOCATE a - v, b: RETURN
IF hot$ = "B" THEN LOCATE a + v, b: RETURN
IF hot$ = "C" THEN LOCATE a, b + v: RETURN
IF hot$ = "D" AND b >= 3 THEN LOCATE a, b - v: RETURN
RETURN

NotA:
RETURN

END SUB
*/

void DisplayText(char *a)
{
	FILE *file;
	char path[MAXPATHLEN];
	char line[1024];
	
	sprintf(path,"%s%s",DatPath,a);
	file=SharedOpen(path,"r",LOCK_SH);
	while(!feof(file))
	{
		readline(line, sizeof(line), file);
		RTrim(line);
		if(line[0]=='*')
		{
			Oprint("\r\n");
			PressAnyKey();
			Ocls();
		}
		else
		{
			Oprint(line);
			Oprint("\r\n");
			if(PortNum && CarrierLost)
			{
				ExitGame();
				exit(1);
			}
		}
	}
	fclose(file);
}

void DisplayTitle(char *a)
{
	char path[MAXPATHLEN];
	FILE *file;
	char line[1024];
	
	sprintf(path,"%s%s",DatPath,a);
	file=SharedOpen(path,"r",LOCK_SH);
	while(!feof(file))
	{
		readline(line, sizeof(line), file);
		Oprint(line);
		Oprint("\r\n");
		if(PortNum && CarrierLost)
		{
			ExitGame();
			exit(1);
		}
	}
	fclose(file);
}

void ExitGame(void)
{
// 'Final Commands before ending-- no saving is done
// 'because routine should already do it if need be.
 

//    'CarrierDetect 1         ' <-- Re-enable carrier detection. Carrier had
//                            '     better be true or UEVENT willl occur on
//                            '     next Transmit

	SaveGame();
//    TIMER OFF
	if(PortNum && Rvects)
		CloseComm();
    exit(0);
}

void GotHUP(int sig)
{
	CarrierLost=1;
}

void InitPort(void)
{
	struct termios newterm;
	tcgetattr(0, &origterm);
	memcpy(&newterm, &origterm, sizeof(newterm));
	cfmakeraw(&newterm);
	tcsetattr(0, TCSANOW, &newterm);
	signal(SIGHUP, GotHUP);
	setvbuf(stdout, NULL, _IONBF, 0);
/*
	OpenComm(Port, IRQ, Length, Parity, DBits, Rate, HS, FossAct
IF Port% > 0 AND IRQ% <> 6484 AND FossAct% = 1 THEN
    PRINT " SYSOP: Fossil was not initialized correctly."
    END
END IF
*/
}

char *KbIn (char *dest, long Lmax)
{
	time_t start;
	fd_set readfds;
	struct timeval timeout;
	char inch[2]={0,0};
	int v;
	
	start=time(NULL);
	timeout.tv_sec=0;
	timeout.tv_usec=0;
	while(1)
	{
		if(PortNum && CarrierLost)
		{
			ExitGame();
			exit(1);
		}
		FD_ZERO(&readfds);
		FD_SET(0,&readfds);
		if(select(1, &readfds, NULL, NULL, &timeout)>0)
		{
			fread(inch, 1, 1, stdin);
		}
		else
			break;
	}

	Oprint("Þ\b"); // '³=179

	timeout.tv_sec=1;
	timeout.tv_usec=0;

	dest[0]=0;
	while(1)
	{
		if(PortNum && CarrierLost)
		{
			ExitGame();
			exit(1);
		}
		FD_ZERO(&readfds);
		FD_SET(0,&readfds);
		if(select(1, &readfds, NULL, NULL, &timeout)>0)
		{
			fread(inch, 1, 1, stdin);
        	if(inch[0]=='\b')
			{
				v = strlen(dest);
				if(v>0)
				{
					Oprint(" \b\b \bÞ\b");
					dest[v-1]=0;
				}
			}
			if(inch[0]=='\n' || inch[0]=='\r')
			{
				Oprint(" \b");
				return(dest);
			}
			if(toupper(inch[0]) > '\x1f')
			{
				write(0,inch, 1);
				strcat(dest,inch);
				Oprint("Þ\b");
			}
			if(strlen(dest)>=Lmax)
			{
				Oprint(" \b");
				return(dest);
			}
		}
		else
		{
			if(time(NULL)-start >= 120)
			{
	            ExitGame();  // 'close the communications and end program
			}
		}
	}
}

void Ocls(void)
{
    SetColor(7, 0, 0);
	if(PortNum && CarrierLost)
	{
		ExitGame();
		exit(1);
	}
    Oprint("\e[2J\e[1;1H");
/* Status line --- unneeded */
/*    IF PlayingNow > 0 THEN
        'now, code to display status line
        LOCATE 25, 1
        t$ = LTRIM$(STR$(SecsLeft)): IF LEN(t$) < 2 THEN t$ = "0" + t$
        q$ = Pad$(LTRIM$(STR$(MinsLeft)) + ":" + t$, 9)
        COLOR 9, 1
        PRINT "Û²±° ";
        COLOR 10, 1
        PRINT Pad$(RemoveColor$(Alias$), 16);
        COLOR 0, 1: PRINT "³ ";
        COLOR 10, 1: PRINT Pad$(User$, 22);
        COLOR 0, 1: PRINT " ³ ";
        COLOR 11, 1: PRINT "Mins: " + q$;
        COLOR 0, 1: PRINT " ³ ";
        COLOR 14, 1: PRINT "^T = Chat ";
        COLOR 9, 1: PRINT "°±²Û";
        Position 1, 1
        COLOR 7, 0
    END IF */
}

void Oprint(char *instr)
{
	char *str;
	char a[256];

	strcpy(a,instr);
	if(PortNum && CarrierLost)
	{
		ExitGame();
		exit(1);
	}
	for(str=a;*str;str++)
	{
		if(str[0]=='`')
		{
			str++;
			switch(str[0])
			{
				case '0':
					SetColor(10, 0, 0);
					break;
				case '1':
					SetColor(1, 0, 0);
					break;
				case '2':
					SetColor(2, 0, 0);
					break;
				case '3':
					SetColor(3, 0, 0);
					break;
				case '4':
					SetColor(4, 0, 0);
					break;
				case '5':
					SetColor(5, 0, 0);
					break;
				case '6':
					SetColor(6, 0, 0);
					break;
				case '7':
					SetColor(7, 0, 0);
					break;
				case '8':
					SetColor(8, 0, 0);
					break;
				case '9':
					SetColor(9, 0, 0);
					break;
				case '!':
					SetColor(11, 0, 0);
					break;
				case '@':
					SetColor(12, 0, 0);
					break;
				case '#':
					SetColor(13, 0, 0);
					break;
				case '$':
					SetColor(14, 0, 0);
					break;
				case '%':
					SetColor(15, 0, 0);
					break;
				default:
					write(0,"`",1);
			}
		}
		else
			write(0,str,1);
	}
}

char *Owait(char *dest)
{
	time_t start;
	fd_set readfds;
	struct timeval timeout;

	timeout.tv_sec=0;
	timeout.tv_usec=0;
	while(1)
	{
		if(PortNum && CarrierLost)
		{
			ExitGame();
			exit(1);
		}
		FD_ZERO(&readfds);
		FD_SET(0,&readfds);
		if(select(1, &readfds, NULL, NULL, &timeout)>0)
		{
			fread(dest, 1, 1, stdin);
		}
		else
			break;
	}

	dest[0]=0;
	dest[1]=0;
	start=time(NULL);

	timeout.tv_sec=1;
	timeout.tv_usec=0;
	while(1)
	{
		if(PortNum && CarrierLost)
		{
			ExitGame();
			exit(1);
		}
		FD_ZERO(&readfds);
		FD_SET(0,&readfds);
		if(select(1, &readfds, NULL, NULL, &timeout)>0)
		{
			fread(dest, 1, 1, stdin);
			return(dest);
		}
		else
		{
			if(time(NULL)-start >= 120)
			{
	            ExitGame();  // 'close the communications and end program
			}
		}
	}
}

char *PickRandom(char *dest, char *a)
{
	FILE *file;
	char path[MAXPATHLEN];
	char line[1024];
	int x;
	int y;
	
	sprintf(path,"%s%s",DatPath,a);
	file=SharedOpen(path, "r", LOCK_SH);
	readline(line, sizeof(line), file);
	RTrim(line);
	y=random()%atoi(line)+1;
	for(x=1;x<=y;x++)
	{
		readline(dest, 256, file);
		RTrim(dest);
	}
	fclose(file);
	return(dest);
}

void Position (short x, short y)
{
	char code[12];

	if(x < 1 || y < 1 || x > 80 || y > 24)
		return;
	sprintf(code,"\e[%d;%dH",y,x);

	if(PortNum && CarrierLost)
	{
		ExitGame();
		exit(1);
	}

	Oprint(code);
}

void PressAnyKey(void)
{
	char inch[2];
	Oprint("                    `1-`9-`3-`!-Press any key to continue-`3-`9-`1-");
	Owait(inch);
	Oprint("`7\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\r\n");
}

void UCase(char *str)
{
	char *p;

	for(p=str;*p;p++)
	{
		*p=toupper(*p);
	}
}
	
void ReadLinkTo(void)
{
	char path[MAXPATHLEN];
	FILE *file;
	char a[256];
	char b[256];
	char e[256];
	char f[256];
	char i[256];
	char s[256];
	char g[256];
	char h[256];
	char ii[256];
	char Jj[256];
	char aia[256];
	char Fas[256];
	char buf[256];
	char y[256];
	char o1[256];
	char o2[256];
	char o3[256];
	char c[256];
	char *istr;
	char str[256];
	short B1;
	short b2;
	short b3;
	short E1;
	short e2;
	short e3;
	short f1;
	short f2;
	short f3;
	short i1;
	short I2;
	short i3;
	short s1;
	short s2;
	short s3;
	short x;

	IRQ = 0;
	Rate = 0;
	Port = 0;

	strcpy(Bad, "Unknown");
	if(NodeNum == 0)
		strcpy(path,"linkto.bbs");
	else
		sprintf(path,"linkto.%d",NodeNum);

	file=SharedOpen(path, "r", LOCK_SH);
	strcpy(Bad, "Not all 10 lines in LINKTO");
	readline(a, 256, file);
	RTrim(a);
	readline(b, 256, file);
	RTrim(b);
	readline(e, 256, file);
	RTrim(e);
	readline(f, 256, file);
	RTrim(f);
	readline(i, 256, file);
	RTrim(i);
	readline(s, 256, file);
	RTrim(s);
	readline(g, 256, file);
	RTrim(g);
	readline(h, 256, file);
	RTrim(h);
	readline(ii, 256, file);
	RTrim(ii);
	readline(Jj, 256, file);
	RTrim(Jj);
	readline(aia, 256, file);
	RTrim(aia);
	readline(Fas, 256, file);
	RTrim(Fas);

	FossAct = atoi(Fas);
	Rvects = atoi(aia);
	HS = atoi(ii);
	fclose(file);
	B1 = atoi(Mid(buf, b, 1, 2));
	b2 = atoi(Mid(buf, b, 4, 2));
	b3 = atoi(Mid(buf, b, 7, 2));

	E1 = atoi(Mid(buf, e, 1, 2));
	e2 = atoi(Mid(buf, e, 4, 2));
	e3 = atoi(Mid(buf, e, 7, 2));

	f1 = atoi(Mid(buf, f, 1, 2));
	f2 = atoi(Mid(buf, f, 4, 2));
	f3 = atoi(Mid(buf, f, 7, 2));

	i1 = atoi(Mid(buf, i, 1, 2));
	I2 = atoi(Mid(buf, i, 4, 2));
	i3 = atoi(Mid(buf, i, 7, 2));
	istr=i+9;

	s1 = atoi(Mid(buf, s, 1, 2));
	s2 = atoi(Mid(buf, s, 4, 2));
	s3 = atoi(Mid(buf, s, 7, 2));

	strcpy(Bad, "L:2");
	UCase(g);
	Mid(o1, g, 1, 1);
	Mid(o2, g, 2, 1);
	Mid(o3, g, 3, 1);
	if(o1[0]=='E')
		Parity = 2;
	else if (o1[0] == 'O')
		Parity = 1;
	else
		Parity = 0;
	if(o2[0] == '7')
		Length = 7;
	else
		Length = 8;

	DBits = atoi(o3);

	IRQ = atoi(h);

	sprintf(Bad, "Where's %s Door file?", a);

	file=SharedOpen(a, "r", LOCK_SH);
	sprintf(Bad, "ErrPars %s.",a);
	for(x = 1; x<= B1; x++)
		readline(buf, sizeof(buf), file);
	RTrim(buf);
	Mid(y, buf, b2, b3);
	strcpy(User, y);
	strcpy(str, a);
	UCase(str);
	if(strstr(str,"DORINFO")!=NULL)
	{
		readline(c, sizeof(c), file);
		RTrim(c);
		strcat(User," ");
		strcat(User, c);
	}
	fclose(file);

	file=SharedOpen(a, "r", LOCK_SH);
	for(x=1; x<=E1; x++)
		readline(buf, sizeof(buf), file);
	RTrim(buf);
	Mid(y, buf, e2, e3);
	Port = atoi(y);
	fclose(file);

	file=SharedOpen(a, "r", LOCK_SH);
	if (f1>0 && f2>0 && f3>0)
	{
		for(x=1; x<=f1; x++)
			readline(buf, sizeof(buf), file);
		RTrim(buf);
		Mid(y, buf, f2, f3);
		MinsLeft = atoi(y);
		SecsLeft = 30;
	}
	else
	{
		MinsLeft = 15;
		SecsLeft = 30;
	}
	fclose(file);

	GraphMode = -1;
	file=SharedOpen(a, "r", LOCK_SH);
	if (i1 > 0 && I2 > 0 && i3 > 0)
	{
		GraphMode = 0;
		for(x=1; x<=i1; x++)
			readline(buf, sizeof(buf), file);
		RTrim(buf);
		Mid(c, buf, I2, i3);
		if(!strcasecmp(c, istr))
			GraphMode = 1;
	}
	fclose(file);

	file=SharedOpen(a, "r", LOCK_SH);
	for(x = 1; x <= s1; x++)
			readline(buf, sizeof(buf), file);
	RTrim(buf);
	Mid(y, buf, s2, s3);
	Rate = atoi(y);
	KeepRate = Rate;
	fclose(file);

	if(atoi(Jj) == 1 && FossAct == 0)
		Rate = 0;

	if(FossAct == 1)
	{
    	if(Rate > 19200)
			Rate = 38400;
    	else if (Rate > 9600)
			Rate = 19200;
    	else if (Rate > 4800)
			Rate = 9600;
    	else if (Rate > 2400)
			Rate = 4800;
    	else if (Rate > 1200)
			Rate = 2400;
    	else if (Rate > 300)
			Rate = 1200;
    	else if (Rate > 0)
			Rate = 300;
		else
	    	Rate = 0;
	}

	RTrim(User);

	InitPort();

	PortNum = Port;

	OldTime = time(NULL);
}

void SetColor(short f, short b, short oeo)
{
	short Fg = 0;
	short Bg = 0;
	short Rg = -1;
	
	if(f == 1)
		Fg = 4;
    if (f == 2) Fg = 2;
    if (f == 3) Fg = 6;
    if (f == 4) Fg = 1;
    if (f == 5) Fg = 5;
    if (f == 6) Fg = 3;
    if (f == 7) Fg = 7;
    if (f == 8) Fg = 8;
    if (f == 9) Fg = 12;
    if (f == 10) Fg = 10;
    if (f == 11) Fg = 14;
    if (f == 12) Fg = 9;
    if (f == 13) Fg = 13;
    if (f == 14) Fg = 11;
    if (f == 15) Fg = 15;
    if (b == 1) Bg = 4;
    if (b == 2) Bg = 2;
    if (b == 3) Bg = 6;
    if (b == 4) Bg = 1;
    if (b == 5) Bg = 5;
    if (b == 6) Bg = 3;
    if (b == 7) Bg = 7;

/*  '*** specify a -1 to omit that field
	'0=black, 1=red, 2=green, 3=brown
	'4-blue,  5=mag, 6=cyan,  7=gray
	'-----------------------------------
	'although these are translated from standard */

	if (Fg >= 0 && Fg <= 7)
	{
		Rg = 0;
		Fg = 30 + Fg;
	}
	if (Fg >= 8 && Fg <= 15)
	{
		Rg = 1;
		Fg = 30 + Fg - 8;
	}
	if (Bg >= 0 && Bg <= 7)
		Bg = 40 + Bg;
	if (Bg >= 8 && Bg <= 15)
		Bg = 40 + Bg - 8;

	if(Rg>=0 && Fg>0 && Bg>0)
		sprintf(NewColor,"\e[%hd;%hd;%hdm",Rg,Fg,Bg);
	else if (Rg>=0 && Fg>0)
		sprintf(NewColor,"\e[%hd;%hdm",Rg,Fg);
	else if (Fg>0 && Bg>0)
		sprintf(NewColor,"\e[%hd;%hdm",Fg,Bg);
	else if (Rg>=0 && Bg>0)
		sprintf(NewColor,"\e[%hd;%hdm",Rg,Bg);
	else if (Rg>=0)
		sprintf(NewColor,"\e[%hdm",Rg);
	else if (Fg>0)
		sprintf(NewColor,"\e[%hdm",Fg);
	else if (Bg>0)
		sprintf(NewColor,"\e[%hdm",Bg);

	if(PortNum && CarrierLost)
	{
		ExitGame();
		exit(1);
	}
	if(!oeo)
		Oprint(NewColor);
}

void Paus(long x)
{
	struct timeval ti;
	ti.tv_sec=x;
	ti.tv_usec=0;
	select(0,NULL,NULL,NULL,&ti);
}
