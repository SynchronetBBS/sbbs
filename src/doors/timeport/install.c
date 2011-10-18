#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#ifdef __NetBSD__
	#include <ncurses.h>
#else
	#include <curses.h>
#endif

char Aee[256];
char Bee[256];
char Cee[256];
char Dee[256];
char Fee[256];
char Jee[256];
char Pdd[256];
short NodeNum;
char b1[256];
char c1[256];
char d1[256];
char e1[256];
char f1[256];

char *GetWord(char *dest, char *Pro,char *Def);
void Menu1(void);
void Menu2(void);
void Menu3(void);
void Perf(char *a);
void ResetGame(void);
void ViewEdit(void);
void waiter(void);
void WasCreated(void);
void WhyRegister(void);

void RTrim(char *str)
{
	char *p;

	if(!*str)
		return;
	for(p=str+strlen(str)-1;(isspace(*p) || !*p) && p>=str; p--)
	{
		*p=0;
	}
}

void readline(char *dest, size_t size, FILE *file)
{
	char *p;

	if(fgets(dest,size,file)!=dest)
		dest[0]=0;
	
	for(p=dest;*p;p++)
	{
		if(*p=='\r' || *p=='\n')
		{
			*p=0;
			break;
		}
	}
}

void UCase(char *str)
{
	char *p;

	for(p=str;*p;p++)
	{
		*p=toupper(*p);
	}
}

void COLOR(fg, bg)
{
	int pair;

	if(fg>7)
	{
		attrset(A_BOLD);
		fg-=8;
	}
	else
		attrset(A_NORMAL);

	pair=(bg<<4)|fg;
	if(pair==7)
		pair=0;
	else if(pair==0)
		pair=7;
	color_set(pair,NULL);
}

void LOCATE(xpos, ypos)
{
	xpos--;
	ypos--;
	move(xpos,ypos);
}

void PRINT(char *str)
{
	char *p;
	int y,x;
	int maxy,maxx;
	
	getmaxyx(stdscr,maxy,maxx);
	for(p=str;*p;p++)
	{
		switch(*p)
		{
			case '\n':
				getyx(stdscr,y,x);
				if(y+1==maxy)
				{
					y--;
					scrl(1);
				}
				move(y+1,x);
				break;
			case '\r':
				getyx(stdscr,y,x);
				move(y,0);
				break;
			case 0:
				refresh();
				return;
			default:
				addch(*p);
		}
	}
	refresh();
}

void INKEY(char *str)
{
	str[0]=getch();
	str[1]=0;
}

short curses_color(short color)
{
	switch(color)
	{
		case 0 :
			return(COLOR_BLACK);
		case 1 :
			return(COLOR_BLUE);
		case 2 :
			return(COLOR_GREEN);
		case 3 :
			return(COLOR_CYAN);
		case 4 :
			return(COLOR_RED);
		case 5 :
			return(COLOR_MAGENTA);
		case 6 :
			return(COLOR_YELLOW);
		case 7 :
			return(COLOR_WHITE);
		case 8 :
			return(COLOR_BLACK);
		case 9 :
			return(COLOR_BLUE);
		case 10 :
			return(COLOR_GREEN);
		case 11 :
			return(COLOR_CYAN);
		case 12 :
			return(COLOR_RED);
		case 13 :
			return(COLOR_MAGENTA);
		case 14 :
			return(COLOR_YELLOW);
		case 15 :
			return(COLOR_WHITE);
	}
	return(0);
}

char *input_line(char *dest,int maxlen)
{
	int ch;
	int inspos=0;
	int xpos,ypos;
	int xstart,ystart;
	int started=0;
	char buf[256];
	int j;

	memset(dest+strlen(dest),0,maxlen-strlen(dest));
	memset(buf,0,sizeof(buf));
	strcpy(buf,dest);
	getyx(stdscr,ystart,xstart);
	xpos=xstart;
	ypos=ystart;
	inspos=1;
	move(ystart,xstart);
	PRINT(dest);
	move(ypos,xpos);
	while(1)
	{
		ch=getch();
		switch(ch)
		{
			case KEY_LEFT:
				if(inspos>1)
				{
					xpos--;
					inspos--;
				}
				started=1;
				break;
			case KEY_RIGHT:
				if(inspos<=strlen(dest))
				{
					xpos++;
					inspos++;
				}
				started=1;
				break;
			case 8:
			case KEY_BACKSPACE:
				if(!started)
				{
					for(j=0; j<strlen(dest); j++)
						PRINT(" ");
					memset(buf,0,sizeof(buf));
					memset(dest,0,sizeof(maxlen));
					inspos=1;
				}
				if(inspos>1)
				{
					sprintf(buf,"%.*s%s",inspos-2,dest,dest+inspos-1);
					inspos--;
					xpos--;
				}
				started=1;
				break;
			case KEY_HOME:
				xpos=xstart;
				inspos=1;
				started=1;
				break;
			case KEY_END:
				xpos=xstart+strlen(dest);
				inspos=strlen(dest)+1;
				started=1;
				break;
			case KEY_DC:
				if(!started)
				{
					for(j=0; j<strlen(dest); j++)
						PRINT(" ");
					memset(buf,0,sizeof(buf));
					memset(dest,0,sizeof(maxlen));
					inspos=1;
				}
				if(inspos<=strlen(dest))
				{
					sprintf(buf,"%.*s%s",inspos-1,dest,dest+inspos);
				}
				started=1;
				break;
			case '\n':
			case '\r':
				PRINT("\r\n");
				return(dest);
			default:
				if(ch>=' ' && ch<='~')
				{
					if(!started)
					{
						for(j=0; j<strlen(dest); j++)
							PRINT(" ");
						memset(buf,0,sizeof(buf));
						memset(dest,0,sizeof(maxlen));
						inspos=1;
					}
					sprintf(buf,"%*.*s%c%s",inspos-1,inspos-1,dest,ch,dest+inspos-1);
					inspos++;
					xpos++;
					started=1;
				}
				break;
		}
		strcpy(dest,buf);
		move(ystart,xstart);
		PRINT(dest);
		PRINT(" ");
		move(ypos,xpos);
		refresh();
	}
}

int main(int argc, char **argv)
{
	char a[256];
	int bg,fg,pair;

	FILE *file;
	file=fopen("tp1.cfg", "r");
	if(file==NULL)
	{
		file=fopen("tp1.cfg","w");
		fputs("Time Port: A BBS Door Game\r\n", file);
		fputs("Written by Mike Snyder\r\n",file);
	}
	else
	{
		readline(a, sizeof(a), file);
		readline(a, sizeof(a), file);
	}
	fclose(file);
	strcpy(Pdd,"                  ");
	
	/* Init. Curses */
		initscr();
	start_color();
	cbreak();
	noecho();
	nonl();
	keypad(stdscr, TRUE);
	scrollok(stdscr,TRUE);
	raw();
	halfdelay(1);

	/* Set up color pairs */
	for(bg=0;bg<8;bg++)  {
		for(fg=0;fg<8;fg++) {
			pair=(bg<<4)|fg;
			if(pair==7)
				pair=0;
			else if(pair==0)
				pair=7;
			init_pair(pair,curses_color(fg),curses_color(bg));
		}
	}

	
	Menu1();
	clear();
	refresh();
	endwin();
	return(0);
}

char *Center(char *dest, char *a)
{
	int sp, len, x;
	len=strlen(a);
	if(len>80)
		len=80;
	sp=(80 - len) / 2;
	for(x=0; x<sp; x++)
	{
		dest[x]=' ';
		dest[x+1]=0;
	}
	strcat(dest, a);
	dest[80]=0;
	return(dest);
}

void EnterRegis(void)
{
	char a[256];
	char b[256];
	char t[256];
	char u[256];
	FILE *file;
	int regis;

	LOCATE(16, 10);
	COLOR(2, 0);
	PRINT("Are you ready to enter your Registration Codes? ");
	COLOR(10, 0);
	PRINT("(Y/N)\r\n");
	a[0]=0;
	while(a[0]!='Y')
	{
		INKEY(a);
		a[0] = toupper(a[0]);
		if(a[0] == 'N')
			return;
	}

	file=fopen("tp1.cfg","w");

	GetWord(a,"Registration Line 1 (Sysop name)\r\n: ", "0");
	fputs(a, file);
	fputs("\r\n",file);

	GetWord(b,"Registration Line 2 (BBS name)\r\n: ", "0");
	fputs(b, file);
	fputs("\r\n",file);
	fclose(file);

	regis = 1;
	strcpy(t,a);
	strcpy(u,b);
	if(strlen(t) <= 1)
		regis = 0;
	if(strlen(u) <= 1)
		regis = 0;

	COLOR(14, 0);
	if(regis == 0)
		PRINT("             The Registration codes you entered are NOT valid!");
	if(regis == 1)
	{
		PRINT("                  Thank you for Registering Time Port, ");
		PRINT(t);
		PRINT(".");
	}
	PRINT("\r\n");
	COLOR(11,0);
	COLOR(3,0);

	waiter();
}

void GetDefaults(void)
{
	COLOR(12,0);
	PRINT("     Modem Setup: Express in Parity,Databits,StopBits order\r\n");
	GetWord(Aee,"Your Modem SETUP?", "N81");

	COLOR(12,0);
	PRINT("             IRQ: If your IRQ is standard for the port, (0) will work\r\n");
	GetWord(Bee, "Your IRQ (0 is default, or 2 through 7)", "0");

	COLOR(12,0);
	PRINT("     Handshaking: if the default (3) causes trouble, try other values\r\n");
	GetWord(Cee,"Handshaking: 0=None, 1=XON/XOFF, 2=RTS/CTS, 3=Both", "3");

	COLOR(12,0);
	PRINT("     Locked Port: Select (1) if your Com Port is locked, (0) otherwise\r\n");
	GetWord(Dee,"Locked Com Port? 0=NO, 1=YES", "1");

	COLOR(12,0);
	PRINT("  Vectors: Choose (1) right now.  DO NOT choose 0 unless nothing else works\r\n");
	GetWord(Fee,"Release Vectors upon Exit? 1=Yes, 0=No", "1");

	COLOR(12,0);
	PRINT("          Fossil: Choose (1) if a Fossil driver will be in use\r\n");
	PRINT("                  If Time Port refuses to use your Fossil, or if you\r\n");
	COLOR(12,0);
	PRINT("        have lockup-problems, try changing this value to (0)\r\n");
	GetWord(Jee,"FOSSIL driver in use? 0=NO, 1=YES", "0");
}

char *GetDrop(char *dest, char *a)
{
	char str[256];

	sprintf(str, "Path to Drop File %s",a);
	GetWord(dest,str,"./");
	if(dest[strlen(dest)-1]!='/')
	{
		COLOR(4,0);
		PRINT(" -=-=- ");
		COLOR(12,0);
	    PRINT("Paths almost always end with a '/' symbol.  Add it now? ");
    	COLOR(14,0);
    	PRINT("(Y/N)");
		str[0]=0;
		str[1]=0;
		while(str[0]!='Y' && str[0] !='N')
		{
			INKEY(str);
			UCase(str);
			if(str[0]=='Y')
			{
				strcat(dest,"/");
			}
		}
	}
	return(dest);
}

int GetKey(int x, int Y)
{
	struct timeval n;
	struct timeval n2;
	int c = 1;
	int Max;
	int g = 14;
	int a, De;
	
	Max=x;
	gettimeofday(&n,NULL);
	while(1)
	{
		gettimeofday(&n2,NULL);
		if((n2.tv_sec - n.tv_sec)*10+(n2.tv_usec - n.tv_usec/100000) > 1)
		{
			switch(g)
			{
				case 14:
					g=10;
					break;
				case 13:
					g=14;
					break;
				case 12:
					g=13;
					break;
				default:
					g=12;
			}
		}
		LOCATE(Y + c, strlen(Pdd));
		COLOR(0,0);
		addch(' ');
			LOCATE(Y + c, strlen(Pdd) - 1);
		COLOR(g,0);
		PRINT("$");
		a=getch();
		if(a >= 'a' && a <= 'z')
			a -= 32;
		if(a == '\n' || a == '\r' || a == 32)
		{
    		return(c);
		}
		De=a-64;
		if(De >= 1 && De <= x)
		{
			return(De);
		}
		switch (a)
		{
			case KEY_UP:
				LOCATE(Y + c, strlen(Pdd) - 1);
				PRINT(" ");
				c = c - 1;
				if(c<1)
					c=Max;
				break;
			case KEY_DOWN:
				LOCATE(Y + c, strlen(Pdd) - 1);
				PRINT(" ");
				c = c + 1;
				if(c>Max)
					c=1;
				break;
			case KEY_PPAGE:
			case KEY_HOME:
				LOCATE(Y + c, strlen(Pdd) - 1);
				PRINT(" ");
				c = 1;
				break;
			case KEY_NPAGE:
			case KEY_END:
				LOCATE(Y + c, strlen(Pdd) - 1);
				PRINT(" ");
				c = Max;
				break;
			case 27:
				return(0);
		}
	}
}

void GetLinkNums(char *a)
{
	char b[256];

	strcpy(b,a);
	UCase(b);
	if(strstr(b, "CALLINFO")!=NULL)
	{
    	strcpy(b1, "01 01 25");
	    strcpy(c1, "29 04 02");
    	strcpy(d1, "05 01 06");
	    strcpy(e1, "06 01 05 COLOR");
    	strcpy(f1, "31 01 08");
	}

	if(strstr(b, "CHAIN.TXT")!=NULL)
	{
	    strcpy(b1, "02 01 25");
    	strcpy(c1, "21 01 2");
	    strcpy(d1, "00 00 0");
    	strcpy(e1, "14 01 01 1");
	    strcpy(f1, "20 01 07");
	}

	if(strstr(b, "DOOR.SYS")!=NULL)
	{
	    strcpy(b1, "10 01 25");
    	strcpy(c1, "01 04 02");
	    strcpy(d1, "19 01 06");
    	strcpy(e1, "20 01 02 GR");
	    strcpy(f1, "02 01 07");
	}

	if(strstr(b, "DORINFO")!=NULL)
	{
	    strcpy(b1, "07 01 25");
	    strcpy(c1, "04 04 02");
	    strcpy(d1, "12 01 06");
	    strcpy(e1, "10 01 01 1");
	    strcpy(f1, "05 01 07");
	}

}

char *GetWord(char *dest, char *Pro,char *Def)
{
	COLOR(3,0);
	PRINT(Pro);
	PRINT(" [");
	COLOR(11,0);
	PRINT(Def);
	COLOR(3);
	PRINT("]: ");
	strcpy(dest,Def);
	/* This should be better! */
	input_line(dest,80);
	if(strlen(dest)==0)
		strcpy(dest,Def);
	return(dest);
}

void MakeGap(void)
{
	char Cv[256];
	FILE *file;

	file=fopen("linkto.bbs", "w");
	LOCATE(19, 1);
	GetDrop(Cv,"door.sys");
	fprintf(file,"%sdoor.sys\r\n",Cv);
	fputs("10 01 25\r\n",file);
	fputs("01 04 02\r\n",file);
	fputs("19 01 06\r\n",file);
	fputs("20 01 02 GR\r\n",file);
	fputs("02 01 07\r\n",file);

	GetDefaults();

	fputs(Aee,file);
	fputs("\r\n",file);
	fputs(Bee,file);
	fputs("\r\n",file);
	fputs(Cee,file);
	fputs("\r\n",file);
	fputs(Dee,file);
	fputs("\r\n",file);
	fputs(Fee,file);
	fputs("\r\n",file);
	fputs(Jee,file);
	fputs("\r\n",file);
	fclose(file);

	WasCreated();
}

void MakeRbbs(void)
{
	char Cv[256];
	FILE *file;

	file=fopen("linkto.bbs", "w");
	LOCATE(19, 1);
	GetDrop(Cv,"dorinfo1.def");
	fprintf(file,"%sdorinfo1.def\r\n",Cv);
	fputs("07 01 25\r\n",file);
	fputs("04 04 02\r\n",file);
	fputs("12 01 06\r\n",file);
	fputs("10 01 01 1\r\n",file);
	fputs("05 01 07\r\n",file);

	GetDefaults();

	fputs(Aee,file);
	fputs("\r\n",file);
	fputs(Bee,file);
	fputs("\r\n",file);
	fputs(Cee,file);
	fputs("\r\n",file);
	fputs(Dee,file);
	fputs("\r\n",file);
	fputs(Fee,file);
	fputs("\r\n",file);
	fputs(Jee,file);
	fputs("\r\n",file);
	fclose(file);

	WasCreated();
}

void makeWeather(void)
{
	char a[256];
	FILE *file;

	endwin();
	system("makemod");
	refresh();

	PRINT("\r\n");
	COLOR(14, 0);
	PRINT("  Reset the Player Talk file? (MUST do this the 1st time) ");
	COLOR(12, 0);
	PRINT("(Y/N)");
	a[0]=0;
	a[1]=0;
	while(a[0]!='Y' && a[0]!='N')
	{
		INKEY(a);
		UCase(a);
	}
	if(a[0]=='N')
		PRINT("\r\n");
	else
	{
		file=fopen("convers.txt","w");
    	fprintf(file,"%-80.80s","`%Somebody `7asks:");
    	fprintf(file,"%-80.80s","`3We all agree, then, that we haven't been told the whole story?");
    	fprintf(file,"%-80.80s","`%Somebody else `7replies:");
    	fprintf(file,"%-80.80s","`3Without a doubt.  Why would the bunch of us be uprooted from our own");
    	fprintf(file,"%-80.80s","`%Somebody else `7adds:");
    	fprintf(file,"%-80.80s","`3times?  And have any of you even seen one of our hosts?");
    	fprintf(file,"%-80.80s","`%Somebody `7replies:");
    	fprintf(file,"%-80.80s","`3No... Only their voices when I first arrived.  Anybody else?");
    	fprintf(file,"%-80.80s","`%Someone else `7asks:");
    	fprintf(file,"%-80.80s","`3Same here.  And why are we being trusted to use this Time Portal?");
    	fprintf(file,"%-80.80s","`%Someone `7replies:");
    	fprintf(file,"%-80.80s","`3When I arrived, I was told not to trust anyone.  So why would our Hosts");
    	fprintf(file,"%-80.80s","`%Someone `7adds:");
    	fprintf(file,"%-80.80s","`3allow the possibility of some serious temporal damage if not all of us");
    	fprintf(file,"%-80.80s","`%Someone `7adds:");
    	fprintf(file,"%-80.80s","`3are trustworthy?  And what are we expected to do for them?");
    	fclose(file);
    	PRINT("    CONVERS.TXT created\r\n");
	}
	PRINT("\r\n");
	waiter();
}

void MakeWildcat(void)
{
	char Cv[256];
	FILE *file;

	file=fopen("linkto.bbs", "w");
	LOCATE(19, 1);
	GetDrop(Cv,"callinfo.bbs");
	fprintf(file,"%scallinfo.bbs\r\n",Cv);
	fputs("01 01 25\r\n",file);
	fputs("29 04 02\r\n",file);
	fputs("05 01 06\r\n",file);
	fputs("06 01 05 COLOR\r\n",file);
	fputs("31 01 08\r\n",file);

	GetDefaults();

	fputs(Aee,file);
	fputs("\r\n",file);
	fputs(Bee,file);
	fputs("\r\n",file);
	fputs(Cee,file);
	fputs("\r\n",file);
	fputs(Dee,file);
	fputs("\r\n",file);
	fputs(Fee,file);
	fputs("\r\n",file);
	fputs(Jee,file);
	fputs("\r\n",file);
	fclose(file);

	WasCreated();
}

void MakeWwiv(void)
{
	char Cv[256];
	FILE *file;

	file=fopen("linkto.bbs", "w");
	LOCATE(19, 1);
	GetDrop(Cv,"chain.txt");
	fprintf(file,"%schain.txt\r\n",Cv);
	fputs("02 01 25",file);
	fputs("21 01 2",file);
	fputs("00 00 0",file);
	fputs("14 01 01 1",file);
	fputs("20 01 07",file);

	GetDefaults();

	fputs(Aee,file);
	fputs("\r\n",file);
	fputs(Bee,file);
	fputs("\r\n",file);
	fputs(Cee,file);
	fputs("\r\n",file);
	fputs(Dee,file);
	fputs("\r\n",file);
	fputs(Fee,file);
	fputs("\r\n",file);
	fputs(Jee,file);
	fputs("\r\n",file);
	fclose(file);

	WasCreated();
}

void Menu1(void)
{
	char str[256];
	int d;
	int ch;
	
	while(1)
	{
		COLOR(15, 0);
		clear();
		PRINT("\r\n\r\n");
		PRINT(Center(str,"TIME PORT: Written by Mike Snyder"));
		PRINT("\r\n");
		COLOR(14, 0);
		PRINT(Center(str,"Configuration Menu"));
		PRINT("\r\n");
		COLOR(8, 0);
		PRINT(Center(str,"Use the UP and DOWN arrows to select and press [enter]"));
		PRINT("\r\n");
		COLOR(7, 0);
		PRINT(Center(str,"User the [ESC] escape key to exit from a menu"));
		PRINT("\r\n\r\n\r\n");

		PRINT(Pdd); Perf("(A)"); COLOR(8,0); PRINT(" Enter your Time Port REGISTRATION Code\r\n");
		PRINT(Pdd); Perf("(B)"); COLOR(15,0); PRINT(" Setup SETUP.CFG main configuration\r\n");
		PRINT(Pdd); Perf("(C)"); COLOR(15,0); PRINT(" Create LINKTO.BBS to recognize BBS\r\n");
		PRINT(Pdd); Perf("(D)"); COLOR(15,0); PRINT(" Setup other NODES for Time Port\r\n");
		PRINT(Pdd); Perf("(E)"); COLOR(7,0); PRINT(" Create .DAT and .TXT mandatory files\r\n");
		PRINT(Pdd); Perf("(F)"); COLOR(7,0); PRINT(" Reset the Player files - Start Over\r\n");
		PRINT(Pdd); Perf("(G)"); COLOR(8,0); PRINT(" Why should I spend $15 to register?\r\n");
		d = GetKey(7, 8);
		switch (d)
		{
			case 0:
				return;
			case 2:
				Menu3();
				break;
			case 5:
				clear();
				makeWeather();
				break;
			case 3:
				Menu2();
				break;
			case 6:
				PRINT("\r\n\r\n");
	    		COLOR(12, 0);
    			LOCATE(16, 1);
    			PRINT("   Are you POSITIVE you want to reset the game?  (Y/N)");
				ch=ERR;
				while(ch==ERR)
					ch=getch();
				if(toupper(ch=='Y'))
	    			ResetGame();
    			break;
			case 7:
				WhyRegister();
				break;
			case 4:
				ViewEdit();
				break;
			case 1:
				EnterRegis();
				break;
		}
	}
}

void Menu2(void)
{
	int d;
	char str[256];

	clear();
	PRINT("\r\n\r\n");
	PRINT(Center(str,"TIME PORT: Written by Mike Snyder"));
	PRINT("\r\n");
	COLOR(14, 0);
	PRINT(Center(str,"LINKTO.BBS Creation Menu"));
	PRINT("\r\n");
	COLOR(8, 0);
	PRINT(Center(str,"Use the UP and DOWN arrows to select and press [enter]"));
	PRINT("\r\n");
	COLOR(10, 0);
	PRINT("\r\n");
	PRINT(Center(str,"If your door file format is listed here, you will not need to worry"));
	PRINT("\r\n");
	PRINT(Center(str,"about the format of the LINKTO.BBS file.  This will be done for you."));
	PRINT("\r\n\r\n\r\n");

	PRINT(Pdd); Perf("(A)"); COLOR(14,0); PRINT(" Link to WILDCAT door (Callinfo.Bbs)\r\n");
	PRINT(Pdd); Perf("(B)"); COLOR(14,0); PRINT(" Link to WWIV door (Chain.Txt)\r\n");
	PRINT(Pdd); Perf("(C)"); COLOR(14,0); PRINT(" Link to RBBS door (Dorinfo1.Def)\r\n");
	PRINT(Pdd); Perf("(D)"); COLOR(14,0); PRINT(" Link to Gap door (Door.Sys)\r\n");
	d = GetKey(4, 10);

	switch(d)
	{
		case 1:
			MakeWildcat();
			return;
		case 2:
			MakeWwiv();
			return;
		case 3:
			MakeRbbs();
			return;
		case 4:
			MakeGap();
			return;
	}
	return;
}

void Menu3(void)
{
	char str[256];
	char a[256];
	int d,t;
	FILE *file;
	FILE *file2;

	clear();
	COLOR(13, 0);
	while(1)
	{
		clear();
		PRINT("\r\n\r\n");
		PRINT(Center(str,"TIME PORT: Written by Mike Snyder"));
		PRINT("\r\n");
		COLOR(14, 0);
		PRINT(Center(str,"SETUP.CFG Creation Menu"));
		PRINT("\r\n");
		COLOR(8, 0);
		PRINT(Center(str,"Use the UP and DOWN arrows to select and press [enter]"));
		PRINT("\r\n");
		COLOR(4, 0);

		PRINT("\r\n\r\n");

		PRINT(Pdd); Perf("(A)"); COLOR(14,0); PRINT(" Continue with creation of SETUP.CFG\r\n");
		d = GetKey(1, 7);

		if(d == 1)
			break;
		if(d == 0)
			return;
	}

	PRINT("\r\n\r\n");
	file=fopen("setup.cfg","w");

	PRINT("\r\n");
	GetWord(a,"Path and name of Today's News", "./todays.txt");
	fprintf(file,"%-80.80s",a);
    t = 0;
	file2=fopen(a,"a");
	fclose(file2);
	file2=fopen(a,"r");
	if(feof(file2))
		t=1;
	fclose(file2);
	if(t)
	{
		file2=fopen(a,"w");
		fputs("`%Time Port `7was installed on this system!\r\n\r\n",file2);
       	fclose(file2);
	}

	PRINT("\r\n");
	GetWord(a,"Path and name of Yesterday's News", "./yester.txt");
	fprintf(file,"%-80.80s",a);
    t = 0;
	file2=fopen(a,"a");
	fclose(file2);
	file2=fopen(a,"r");
	if(feof(file2))
		t=1;
	fclose(file2);
	if(t)
	{
		file2=fopen(a,"w");
		fputs("`4It was a bleak time indeed, before Time Port came to this system.\r\n\r\n",file2);
		fclose(file2);
	}

	PRINT("\r\n");
	GetWord(a,"Fights (turns) per day (10 - 25)", "20");
	if(atoi(a) < 10)
		strcpy(a,"10");
	else if(atoi(a) > 25)
		strcpy(a,"25");
	fprintf(file,"%-80.80s",a);

	PRINT("\r\n");
	GetWord(a,"Player Fights per day (1 - 3)", "1");
	if(atoi(a)<1)
		strcpy(a,"1");
	else if(atoi(a)>3)
		strcpy(a,"3");
	fprintf(file,"%-80.80s",a);
/*  This was after the put command... perhaps this was broken?
	PRINT #1, a$ */

	PRINT("\r\n");
	GetWord(a,"Path to master node TIMEPORT directory", "/user/local/timeport/");

	if(a[strlen(a)-1]!='/')
	{
		COLOR(4,0);
		PRINT(" -=-=- ");
		COLOR(12,0);
	    PRINT("Paths almost always end with a '\' symbol.  Add it now? ");
    	COLOR(14,0);
    	PRINT("(Y/N)");
		str[0]=0;
		str[1]=0;
		while(str[0]!='Y' && str[0] !='N')
		{
			INKEY(str);
			UCase(str);
			if(toupper(str[0])=='Y')
			{
				strcat(a,"/");
			}
		}
	}

	fprintf(file,"%-80.80s",a);
	fclose(file);

}

void Perf(char *a)
{
	COLOR(15,0);
	addch(a[0]);
	COLOR(12,0);
	addch(a[1]);
	COLOR(15,0);
	addch(a[2]);
}

void ResetGame(void)
{
	glob_t pglob;
	int j;

	if(!glob("mail.*",GLOB_NOSORT,NULL,&pglob))
	{
		for(j=0;j<pglob.gl_pathc;j++)
		{
			unlink(pglob.gl_pathv[j]);
		}
	}
	globfree(&pglob);
	if(!glob("invent.*",GLOB_NOSORT,NULL,&pglob))
	{
		for(j=0;j<pglob.gl_pathc;j++)
		{
			unlink(pglob.gl_pathv[j]);
		}
	}
	unlink("players.dat");
	unlink("namelist.dat");
	PRINT("\r\n");
	COLOR(5, 0);
	PRINT("    Okay, boss, the deed was done!");
	sleep(2);
}

void ViewEdit(void)
{
	char a1[256];
	char cc[256];
	char op[256];
	char Az1[256];
	char Az2[256];
	char Ptt1[256];
	char Ptt2[256];
	char Ro1[81];
	char Ro2[81];
	char Ro3[81];
	char Ro4[81];
	char Ro5[81];
	char str[256];
	char Cmd[256];
	char o[256];
	char *p;
	int b,NodeNum,d;
	FILE *file;

	clear();
	COLOR(2,0);
	PRINT(" Okay, now comes an important part, which you can skip if you just have\r\n");
	PRINT(" a single-node BBS.  The setup files for node 0 are LINKTO.BBS and\r\n");
	PRINT(" SETUP.CFG.  Other nodes will have LINKTO.x and SETUP.x such\r\n");
	PRINT(" as LINKTO.4 and SETUP.4 for node #4.  (Note: There will be NO\r\n");
	PRINT(" SETUP.0 or LINKTO.0, as the 0-node files are named .CFG and .BBS)\r\n");
	PRINT(" You also must call TIMEPORT with an Nx command, TIMEPORT N0 or\r\n");
	PRINT(" TIMEPORT N1 or TIMEPORT N18 and so on...\r\n");
	PRINT("\r\n");
	PRINT(" You can load any available node setup files now, and edit them if you\r\n");
	PRINT(" need, then save them as either the same node config, or another node\r\n");
	PRINT(" config.  In this manner, you can rapidly duplicate your setup files\r\n");
	PRINT(" for each node, and change anything you need to for the configuration\r\n");
	PRINT(" of each node.\r\n");
	PRINT("\r\n");
	COLOR(12,0);
	PRINT("  View/Edit configuration of which existing Node? (0-99): ");
	op[0]=0;
	input_line(op,sizeof(op));
	if(strlen(op) > 3)
		return;
	b = atoi(op);
	if(b > 99 || b < 0)
		return;
	if(b)
	{
		sprintf(Az1,"linkto.%d",b);
		sprintf(Az2,"setup.%d",b);
	}
	else
	{
		strcpy(Az1,"linkto.bbs");
		strcpy(Az2,"setup.cfg");
	}

	NodeNum = b;

Chinky:
	clear();

	file=fopen(Az1,"r");
	if(file==NULL) {
		COLOR(4,0);
		PRINT("  View/Edit configuration of which ");
		COLOR(12,0);
		PRINT("existing");
		COLOR(4,0);
		PRINT(" Node?\r\n");
		PRINT("  (Node is not yet existing!)");
		sleep(2);
		return;
	}
	readline(a1,sizeof(a1),file); // 'Path and name of drop file
	readline(b1,sizeof(b1),file);
	readline(c1,sizeof(c1),file);
	readline(d1,sizeof(d1),file);
	readline(e1,sizeof(e1),file);
	readline(f1,sizeof(f1),file);
	readline(Aee,sizeof(Aee),file); // 'n81
	readline(Bee,sizeof(Bee),file); // 'IRQ
	readline(Cee,sizeof(Cee),file); // 'handshaking
	readline(Dee,sizeof(Dee),file); // 'Locked?
	readline(Fee,sizeof(Fee),file); // 'Vector release?
	readline(Jee,sizeof(Jee),file); // 'Fossil
	fclose(file);

	strcpy(Ptt1,a1);
	p=strrchr(Ptt1,'/');
	if(p==NULL)
	{
		strcpy(Ptt2,Ptt1);
		strcpy(Ptt1,"./");
	}
	else
	{
		strcpy(Ptt2,p+1);
		*(p+1)=0;
	}

	file=fopen(Az2,"r");
	fread(Ro1,80,1,file);
	Ro1[80]=0;
	RTrim(Ro1);
	fread(Ro2,80,1,file);
	Ro2[80]=0;
	RTrim(Ro2);
	fread(Ro3,80,1,file);
	Ro3[80]=0;
	RTrim(Ro3);
	fread(Ro4,80,1,file);
	Ro4[80]=0;
	RTrim(Ro4);
	fread(Ro5,80,1,file);
	Ro5[80]=0;
	RTrim(Ro5);
	fclose(file);

	clear(); // 'now display this info
	COLOR(14,0); PRINT("------- "); COLOR(10,0);
	sprintf(str,"NODE %d",NodeNum);
	PRINT(str);
	COLOR(14,0);
	PRINT("-------\r\n\r\n");

	COLOR(11,0); PRINT(" 1) "); COLOR(9,0); PRINT("Name and Path to TODAY'S news file:\r\n");
	COLOR(12,0); PRINT("    "); PRINT(Ro1); PRINT("\r\n");
	COLOR(11,0); PRINT(" 2) "); COLOR(9,0); PRINT("Name and Path to YESTERDAY'S news file:\r\n");
	COLOR(12,0); PRINT("    "); PRINT(Ro2); PRINT("\r\n");
	COLOR(11,0); PRINT(" 3) "); COLOR(9,0); PRINT("Maximum Mutant Fights per day (10-25): ");
	COLOR(12,0); PRINT(Ro3); PRINT("\r\n");
	COLOR(11,0); PRINT(" 4) "); COLOR(9,0); PRINT("Maximum Player Fights per day (1-3): ");
	COLOR(12,0); PRINT(Ro4); PRINT("\r\n");
	COLOR(11,0); PRINT(" 5) "); COLOR(9,0); PRINT("Full Path to MASTER T- Port Directory:\r\n");
	COLOR(12,0); PRINT("    "); PRINT(Ro5); PRINT("\r\n");
	COLOR(11,0); PRINT(" 6) "); COLOR(9,0); PRINT("Name of DROP file: ");
	COLOR(12,0); PRINT(Ptt2); PRINT("\r\n");
	COLOR(11,0); PRINT(" 7) "); COLOR(9,0); PRINT("Path to the DROP file:\r\n");
	COLOR(12,0); PRINT("    "); PRINT(Ptt1); PRINT("\r\n");
	COLOR(11,0); PRINT(" 8) "); COLOR(9,0); PRINT("Modem Setup (such as N81, E71, etc...): ");
	COLOR(12,0); PRINT(Aee); PRINT("\r\n");
	COLOR(11,0); PRINT(" 9) "); COLOR(9,0); PRINT("Handshaking (0=none,1=XON,XOFF,2=RTS,CTS,3=Both): ");
	COLOR(12,0); PRINT(Cee); PRINT("\r\n");
	COLOR(11,0); PRINT("10) "); COLOR(9,0); PRINT("IRQ for this node (0=default for the port): ");
	COLOR(12,0); PRINT(Bee); PRINT("\r\n");
	COLOR(11,0); PRINT("11) "); COLOR(9,0); PRINT("Com Port Locked or Speed pre-set? (1=Yes, 0=No): ");
	COLOR(12,0); PRINT(Dee); PRINT("\r\n");
	COLOR(11,0); PRINT("12) "); COLOR(9,0); PRINT("Release Vectors upon Exit (1=Yes, 0=No) Note: Probably YES: ");
	COLOR(12,0); PRINT(Fee); PRINT("\r\n");
	COLOR(11,0); PRINT("13) "); COLOR(9,0); PRINT("FOSSIL driver will be in use? (1=Yes, 0=No): ");
	COLOR(12,0); PRINT(Jee); PRINT("\r\n");
	PRINT("\r\n");

	LOCATE(21, 1); COLOR(15,0);
	PRINT("1-13 ");
	COLOR(10,0);
	PRINT("= Change Option, ");
	COLOR(15,0); PRINT("S ");
	COLOR(10,0); PRINT("= Save as Node#, ");
	COLOR(15,0); PRINT("L ");
	COLOR(10,0); PRINT("= Load existing Node# ");
	COLOR(15,0); PRINT("Q ");
	COLOR(10,0); PRINT("= Quit/Abort\r\n");

Billz:
	LOCATE(23, 1); COLOR(14,0); PRINT(" -==>                                                                         ");
	LOCATE(23, 7); Cmd[0]=0; input_line(Cmd,sizeof(Cmd));
	if(strlen(Cmd) > 3)
		goto Billz;
	d = atoi(Cmd);
	UCase(Cmd);
	if(d >= 1 && d <= 13) // 'change an option
	{
    	COLOR(12,0);
    	if(d == 1)
		{
			LOCATE(4, 5);
			strcpy(o, Ro1);
			input_line(Ro1,sizeof(Ro1));
			if(Ro1[0] == 0)
				strcpy(Ro1, o);
		}
    	if(d == 2)
		{
			LOCATE(6, 5);
			strcpy(o, Ro2);
			input_line(Ro2,sizeof(Ro2));
			if(Ro2[0] == 0)
				strcpy(Ro2, o);
		}
    	if(d == 3)
		{
			LOCATE(7, 44);
			strcpy(o, Ro3);
			input_line(Ro3,sizeof(Ro3));
			if(Ro3[0] == 0)
				strcpy(Ro3, o);
        	LOCATE(7, 44);
			if(atoi(Ro3)<10)
				strcpy(Ro3,"10     ");
        	if(atoi(Ro3) > 25)
				strcpy(Ro3, "25     ");
        	PRINT(Ro3);
		}
    	if(d == 4)
		{
			LOCATE(8, 42);
			strcpy(o, Ro4);
			input_line(Ro4,sizeof(Ro4));
			if(Ro4[0] == 0)
				strcpy(Ro4, o);
        	LOCATE(8, 42);
			if(atoi(Ro4) < 1)
				strcpy(Ro4, "1    ");
        	if(atoi(Ro4) > 3)
				strcpy(Ro4, "3    ");
        	PRINT(Ro4);
			PRINT("\r\n");
		}
    	if(d== 5)
		{
			LOCATE(10, 5);
			strcpy(o, Ro5);
			input_line(Ro5,sizeof(Ro5));
			if(Ro5[0] == 0)
				strcpy(Ro5, o);
		}
    	if(d== 6)
		{
			LOCATE(11, 24);
			strcpy(o, Ptt2);
			input_line(Ptt2,sizeof(Ptt2));
			if(Ptt2[0] == 0)
				strcpy(Ptt2, o);
		}
    	if(d== 7)
		{
			LOCATE(13, 5);
			strcpy(o, Ptt1);
			input_line(Ptt1,sizeof(Ptt1));
			if(Ptt1[0] == 0)
				strcpy(Ptt1, o);
		}
    	if(d== 8)
		{
			LOCATE(14, 45);
			strcpy(o, Aee);
			input_line(Aee,sizeof(Aee));
			if(Aee[0] == 0)
				strcpy(Aee, o);
		}
    	if(d== 9)
		{
			LOCATE(15, 55);
			strcpy(o, Cee);
			input_line(Cee,sizeof(Cee));
			if(Cee[0] == 0)
				strcpy(Cee, o);
		}
    	if(d== 10)
		{
			LOCATE(16, 49);
			strcpy(o, Bee);
			input_line(Bee,sizeof(Bee));
			if(Bee[0] == 0)
				strcpy(Bee, o);
		}
    	if(d== 11)
		{
			LOCATE(17, 54);
			strcpy(o, Dee);
			input_line(Dee,sizeof(Dee));
			if(Dee[0] == 0)
				strcpy(Dee, o);
		}
    	if(d== 12)
		{
			LOCATE(18, 65);
			strcpy(o, Fee);
			input_line(Fee,sizeof(Fee));
			if(Fee[0] == 0)
				strcpy(Fee, o);
		}
    	if(d== 12)
		{
			LOCATE(19, 50);
			strcpy(o, Jee);
			input_line(Jee,sizeof(Jee));
			if(Jee[0] == 0)
				strcpy(Jee, o);
		}
    	goto Billz;
	}
	if(Cmd[0]=='Q')
		return;

	if(Cmd[0]=='L')
	{
		LOCATE(23, 1);
    	PRINT("  View/Edit configuration of which existing Node? (0-99): ");
		op[0]=0;
    	input_line(op,sizeof(op));
    	if(strlen(op)>2)
			return;
    	b = atoi(op);
		if(b > 99 || b < 0)
			return;
		if(b)
		{
			sprintf(Az1,"linkto.%d",b);
			sprintf(Az2,"setup.%d",b);
		}
		else
		{
			strcpy(Az1,"linkto.bbs");
			strcpy(Az2,"setup.cfg");
		}
    	NodeNum = b;
    	goto Chinky;
	}


	if(Cmd[0]=='S') // 'save to a node
	{
    	LOCATE(23, 1);
    	COLOR(13,0); PRINT("Save this information as which Node? (0-99) <enter> alone = cancel: ");
    	COLOR(12,0); cc[0]=0; input_line(cc,strlen(cc));
    	if(strlen(cc)>2)
			goto Billz;
    	if(cc[0]==0)
			goto Billz;
    	d = atoi(cc);
    	if(d < 0 || d > 99)
			goto Billz;
		if(d)
		{
			sprintf(Az1,"linkto.%d",d);
			sprintf(Az2,"setup.%d",d);
		}
		else
		{
			strcpy(Az1,"linkto.bbs");
			strcpy(Az2,"setup.cfg");
		}
    	GetLinkNums(Ptt2);
    	file=fopen(Az1,"w");
		fprintf(file,"%s%s\r\n",Ptt1,Ptt2); // 'Path and name of drop file
       	fprintf(file,"%s\r\n", b1);
        fprintf(file,"%s\r\n", c1);
        fprintf(file,"%s\r\n", d1);
        fprintf(file,"%s\r\n", e1);
        fprintf(file,"%s\r\n", f1);
        fprintf(file,"%s\r\n", Aee); //  'n81
        fprintf(file,"%s\r\n", Bee); // 'handshaking
        fprintf(file,"%s\r\n", Cee); // 'IRQ
        fprintf(file,"%s\r\n", Dee); // 'Locked?
        fprintf(file,"%s\r\n", Fee); // 'Vector release?
        fprintf(file,"%s\r\n", Jee); // 'Fossil
    	fclose(file);
		file=fopen(Az2,"w");
		fprintf(file,"%-80.80s",Ro1);
		fprintf(file,"%-80.80s",Ro2);
		fprintf(file,"%-80.80s",Ro3);
		fprintf(file,"%-80.80s",Ro4);
		fprintf(file,"%-80.80s",Ro5);
    	fclose(file);
    	NodeNum = d;
    	goto Chinky;
	}
}

void waiter(void)
{
	char str[256];
	int key;

/*	FOR x = 1 TO 15
	a$ = INKEY$
	NEXT*/

	COLOR(12, 0);
	PRINT(Center(str,"Press <enter> to continue"));
	PRINT("\r\n");

	while(1)
	{
		key=getch();
		if(key == '\n' || key =='\r')
			return;
	}
}

void WasCreated(void)
{
	COLOR(14,0);
	PRINT("       A new LINKTO.BBS has now been created.\r\n");
	waiter();
}

void WhyRegister(void)
{
	char str[256];

	clear();
	COLOR(12,0);
	PRINT("\r\n");
	PRINT(Center(str,"Why Should I, or anyone else, register Time Port?"));
	PRINT("\r\n\r\n\r\n");
	COLOR(15,0); PRINT("    1) ");
	COLOR(11,0); PRINT("Only registered users receive Time Port tech support from HCI\r\n");
	COLOR(15,0); PRINT("    2) ");
	COLOR(11,0); PRINT("Players can only reach Skill Level 5 when unregistered\r\n");
	COLOR(15,0); PRINT("    3) ");
	COLOR(11,0); PRINT("When the highest Skill Level is just 5, the game is unwinnable\r\n");
	COLOR(15,0); PRINT("    4) ");
	COLOR(11,0); PRINT("After registered, Players get a 10% return on stashed money\r\n");
	COLOR(15,0); PRINT("    5) ");
	COLOR(11,0); PRINT("The cost of registration is cheap: Just $15...\r\n");
	COLOR(15,0); PRINT("    6) ");
	COLOR(11,0); PRINT("Time Port will be updated frequently... new stuff, new modules..\r\n");
	COLOR(15,0); PRINT("    7) ");
	COLOR(11,0); PRINT("Just register once, you'll be registered for EVERY new version\r\n");
	COLOR(15,0); PRINT("    8) ");
	COLOR(11,0); PRINT("Everyone can see your name as 'Registered To.'\r\n");
	COLOR(15,0); PRINT("    9) ");
	COLOR(11,0); PRINT("In a registered game, players can change their names.\r\n");
	COLOR(15,0); PRINT("   10) ");
	COLOR(11,0); PRINT("Players can put hits out on other players... a very cool option.\r\n");
	COLOR(15,0); PRINT("   11) ");
	COLOR(11,0); PRINT("Most importantly, the players will appreciate your kindness\r\n");
	PRINT("\r\n");

	COLOR(9,0); waiter();
}
