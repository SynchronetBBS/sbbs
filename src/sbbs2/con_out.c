#line 1 "CON_OUT.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/**********************************************************************/
/* Functions that pertain to console i/o - color, strings, chars etc. */
/* Called from functions everywhere                                   */
/**********************************************************************/

#include "sbbs.h"

extern char *mnestr;

/***************************************************/
/* Seven bit table for EXASCII to ASCII conversion */
/***************************************************/
char *sbtbl="CUeaaaaceeeiiiAAEaAooouuyOUcLYRfaiounNao?--24!<>"
			"###||||++||++++++--|-+||++--|-+----++++++++##[]#"
			"abrpEout*ono%0ENE+><rj%=o..+n2* ";

char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};

uchar outchar_esc=0;

#ifdef __WIN32__

int lclatr(int atr)
{

textattr(atr);
return(curatr);
}

void lputc(ch)
{
switch(ch) {
    case CLREOL:
		clreol();
        break;
    case FF:
		clrscr();
        break;
    case TAB:
		if(!(wherex()%8))
			putchar(SP);
		while(wherex()%8)
			putchar(SP);
        break;
    default:
		putchar(ch);
		break; }
}

#endif

#ifdef __OS2__

HEV con_out_sem;					/* Console command semaphore */

#define CON_OUT_BUFLEN		8192	/* Console output (ANSI) buffer */
#define CON_BLK_BUFLEN		4096	/* Console block (all same atr) buffer */

uchar conoutbuf[CON_OUT_BUFLEN];
uchar conblkbuf[CON_BLK_BUFLEN];
volatile uint  conblkcnt=0;
volatile uint  conoutbot=0;
volatile uint  conouttop=0;

uchar lcl_curatr=LIGHTGRAY;

/****************************************************************************/
/* Prints one block of text (all same attribute) at current cursor position */
/****************************************************************************/
void print_conblkbuf()
{
conblkbuf[conblkcnt]=0;
cputs(conblkbuf);
conblkcnt=0;
}

void con_outch(int ch)
{
	static uchar ansi_esc;
	static uchar ansi_curval;
	static uchar ansi_val[3];
	static uchar ansi_x;
	static uchar ansi_y;
    int x,y;

if(ch==ESC) {
    ansi_esc=1;
	ansi_val[0]=ansi_val[1]=ansi_val[2]=ansi_curval=0;
    return; }

if(ansi_esc==1) {           /* Received ESC already */
    if(ch=='[') {
        ansi_esc++;
        return; }
	ansi_esc=ansi_val[0]=ansi_val[1]=ansi_val[2]=ansi_curval=0; }

if(ansi_esc==2) {           /* Received ESC[ already */

    if(isdigit(ch)) {
		if(ansi_curval>2) ansi_curval=0;
        ansi_val[ansi_curval]*=10;
        ansi_val[ansi_curval]+=ch&0xf;
        return; }

    if(ch==';') {
        ansi_curval++;
        return; }

    /* looks like valid ANSI, so purge the output block */
    if(conblkcnt)
        print_conblkbuf();
    switch(ch) {
        case 'A': // Move cursor up
            y=wherey();
            if(ansi_val[0])
                y-=ansi_val[0];
            else
                y--;
            if(y<1) y=1;
            gotoxy(wherex(),y);
            break;
        case 'B': // Move cursor down
            y=wherey();
            if(ansi_val[0])
                y+=ansi_val[0];
            else
                y++;
            if(y>node_scrnlen) y=node_scrnlen;
            gotoxy(wherex(),y);
            break;
        case 'C': // Move cursor right
            x=wherex();
            if(ansi_val[0])
                x+=ansi_val[0];
            else
                x++;
            if(x>80) x=80;
            gotoxy(x,wherey());
            break;
        case 'D': // Move cursor left
            x=wherex();
            if(ansi_val[0])
                x-=ansi_val[0];
            else
                x--;
            if(x<1) x=1;
            gotoxy(x,wherey());
            break;
        case 'J': // clear screen
            if(ansi_val[0]==2)
                clrscr();
            break;
        case 'K': // clearn from cursor to end of line
            clreol();
            break;
        case 'H': // Position cursor
        case 'f':
            y=ansi_val[0];
            x=ansi_val[1];
            if(x<1) x=1;
            if(x>80) x=80;
            if(y<1) y=1;
            if(y>node_scrnlen) y=node_scrnlen;
            gotoxy(x,y);
            break;
		case 's': // Save cursor position
			ansi_x=wherex();
			ansi_y=wherey();
			break;
		case 'u': // Restore cursor position
			if(ansi_x)
				gotoxy(ansi_x,ansi_y);
			break;
        case 'm': // Select character attributes
            for(x=0;x<=ansi_curval;x++)
                switch(ansi_val[x]) {
                    case 0: // no special attributes
                    case 8: // concealed text (no display)
                        lcl_curatr=LIGHTGRAY;
                        break;
                    case 1: // high intensity
                    case 3: // italic
                    case 4: // underline
                        lcl_curatr|=HIGH;
                        break;
                    case 2: // low intensity
                        lcl_curatr&=~HIGH;
                        break;
                    case 5: // blink
                    case 6: // rapid blink
                    case 7: // reverse video
                        lcl_curatr|=BLINK;
                        break;
                    case 30: // foreground black
                        lcl_curatr&=0xf8;
                        lcl_curatr|=BLACK;
                        break;
                    case 31: // foreground red
                        lcl_curatr&=0xf8;
                        lcl_curatr|=RED;
                        break;
                    case 32: // foreground green
                        lcl_curatr&=0xf8;
                        lcl_curatr|=GREEN;
                        break;
                    case 33: // foreground yellow
                        lcl_curatr&=0xf8;
                        lcl_curatr|=BROWN;
                        break;
                    case 34: // foreground blue
                        lcl_curatr&=0xf8;
                        lcl_curatr|=BLUE;
                        break;
                    case 35: // foreground magenta
                        lcl_curatr&=0xf8;
                        lcl_curatr|=MAGENTA;
                        break;
                    case 36: // foreground cyan
                        lcl_curatr&=0xf8;
                        lcl_curatr|=CYAN;
                        break;
                    case 37: // foreground white
                        lcl_curatr&=0xf8;
                        lcl_curatr|=LIGHTGRAY;
                        break;
                    case 40: // background black
                        lcl_curatr&=0x8f;
                        lcl_curatr|=(BLACK<<4);
                        break;
                    case 41: // background red
                        lcl_curatr&=0x8f;
                        lcl_curatr|=(RED<<4);
                        break;
                    case 42: // background green
                        lcl_curatr&=0x8f;
                        lcl_curatr|=(GREEN<<4);
                        break;
                    case 43: // background yellow
                        lcl_curatr&=0x8f;
                        lcl_curatr|=(BROWN<<4);
                        break;
                    case 44: // background blue
                        lcl_curatr&=0x8f;
                        lcl_curatr|=(BLUE<<4);
                        break;
                    case 45: // background magenta
                        lcl_curatr&=0x8f;
                        lcl_curatr|=(MAGENTA<<4);
                        break;
                    case 46: // background cyan
                        lcl_curatr&=0x8f;
                        lcl_curatr|=(CYAN<<4);
                        break;
                    case 47: // background white
                        lcl_curatr&=0x8f;
                        lcl_curatr|=(LIGHTGRAY<<4);
                        break; }
			textattr(lcl_curatr);
            break; }

	ansi_esc=ansi_val[0]=ansi_val[1]=ansi_val[2]=ansi_curval=0;
    return; }

if(conblkcnt+1>=CON_BLK_BUFLEN)
    print_conblkbuf();
switch(ch) {
    case CLREOL:
		if(conblkcnt) print_conblkbuf();
		clreol();
        break;
    case FF:
		if(conblkcnt) print_conblkbuf();
        clrscr();
        break;
    case TAB:
		if(conblkcnt) print_conblkbuf();
		if(!(wherex()%8))
			putch(SP);
		while(wherex()%8)
			putch(SP);
        break;
    default:
		conblkbuf[conblkcnt++]=ch;
        break; }
}


/****************************************************************************/
/* Thread that services the console output buffer (conoutbuf) which 		*/
/* contains ANSI escape sequences											*/
/* All output is for the text window (mirroring the remote console) 		*/
/****************************************************************************/
void con_out_thread(void *unused)
{
	int i,top,cnt;
	ulong l;

while(1) {
	/* mswait(1);	Removed 12/99 via Enigma */
	if(conoutbot==conouttop) {
		DosWaitEventSem(con_out_sem,10000);  /* every 10 seconds */
		DosResetEventSem(con_out_sem,&l);
        continue; }
	top=conouttop;
	if(top<conoutbot)
		cnt=CON_OUT_BUFLEN-conoutbot;
	else
		cnt=top-conoutbot;
	for(i=conoutbot;i<conoutbot+cnt;i++)
		con_outch(conoutbuf[i]);
	conoutbot=i;
	if(conblkcnt)
		print_conblkbuf();
	if(conoutbot==CON_OUT_BUFLEN)
		conoutbot=0; }
}



void outcon(char ch)
{
	int i=conouttop+1;

if(i==CON_OUT_BUFLEN)
	i=0;
while(conoutbot==i) // Wait for thread to service queue
	mswait(1);
conoutbuf[conouttop++]=ch;
if(conouttop==CON_OUT_BUFLEN)
	conouttop=0;
DosPostEventSem(con_out_sem);	 // Enable output
}

int outcon_pending()
{
if(conoutbot!=conouttop)
	return(1);
if(conblkcnt) {
	print_conblkbuf();
	return(1); }
return(0);
}

int conaes()
{
return(outchar_esc==2);
}

void lputc(int ch)
{

while(outcon_pending())
	mswait(1);
switch(ch) {
    case CLREOL:
		clreol();
        break;
    case FF:
		clrscr();
        break;
    case TAB:
		if(!(wherex()%8))
			putch(SP);
		while(wherex()%8)
			putch(SP);
        break;
    default:
		putch(ch);
		break; }
}

long lputs(char *str)
{
while(outcon_pending())
	mswait(1);
return(cputs(str));
}

int lclwx(void)
{
while(outcon_pending())
	mswait(1);
return(wherex());
}

int lclwy(void)
{
while(outcon_pending())
	mswait(1);
return(wherey());
}

void lclxy(int x, int y)
{
while(outcon_pending())
	mswait(1);
gotoxy(x,y);
}

int lclatr(int x)
{
	int i;

while(conoutbot!=conouttop) 	/* wait for output buf to empty */
	mswait(1);
if(x==-1)
	return(lcl_curatr);

textattr(x);
i=lcl_curatr;
lcl_curatr=x;
return(i);		/* Return previous attribute */
}

#endif

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable)    */
/* Handles ctrl-a characters                                                */
/****************************************************************************/
int bputs(char *str)
{
	int i;
    ulong l=0;

while(str[l]) {
    if(str[l]==1) {             /* ctrl-a */
        ctrl_a(str[++l]);       /* skip the ctrl-a */
		l++;					/* skip the attribute code */
		continue; }
	if(str[l]=='@') {           /* '@' */
		if(str==mnestr			/* Mnemonic string or */
			|| (str>=text[0]	/* Straight out of TEXT.DAT */
				&& str<=text[TOTAL_TEXT-1])) {
			i=atcodes(str+l);		/* return 0 if not valid @ code */
			l+=i;					/* i is length of code string */
			if(i)					/* if valid string, go to top */
				continue; }
		for(i=0;i<TOTAL_TEXT;i++)
			if(str==text[i])
				break;
		if(i<TOTAL_TEXT) {		/* Replacement text */
			//lputc(7);
			i=atcodes(str+l);
			l+=i;
			if(i)
				continue; } }
	outchar(str[l++]); }
return(l);
}

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable)    */
/* Does not expand ctrl-a characters (raw)                                  */
/* Max length of str is 64 kbytes                                           */
/****************************************************************************/
int rputs(char *str)
{
    ulong l=0;

while(str[l])
    outchar(str[l++]);
return(l);
}

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(char *fmat, ...)
{
	va_list argptr;
	char sbuf[256];
	int chcount;

va_start(argptr,fmat);
chcount=vsprintf(sbuf,fmat,argptr);
va_end(argptr);
lputs(sbuf);
return(chcount);
}

/****************************************************************************/
/* Performs printf() using bbs bputs function								*/
/****************************************************************************/
int bprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

if(!strchr(fmt,'%'))
	return(bputs(fmt));
va_start(argptr,fmt);
vsprintf(sbuf,fmt,argptr);
va_end(argptr);
return(bputs(sbuf));
}

/****************************************************************************/
/* Performs printf() using bbs rputs function								*/
/****************************************************************************/
int rprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

va_start(argptr,fmt);
vsprintf(sbuf,fmt,argptr);
va_end(argptr);
return(rputs(sbuf));
}

/****************************************************************************/
/* Outputs character locally and remotely (if applicable), preforming echo  */
/* translations (X's and r0dent emulation) if applicable.					*/
/****************************************************************************/
void outchar(char ch)
{
	char lch;
	int i;

if(console&CON_ECHO_OFF)
	return;
if(ch==ESC)
	outchar_esc=1;
else if(outchar_esc==1) {
	if(ch=='[')
		outchar_esc++;
	else
		outchar_esc=0; }
else
	outchar_esc=0;
if(useron.misc&NO_EXASCII && ch&0x80)
	ch=sbtbl[(uchar)ch^0x80];  /* seven bit table */
if(ch==FF && lncntr>1 && !tos) {
	lncntr=0;
	CRLF;
	pause();
	while(lncntr && online && !(sys_status&SS_ABORT))
		pause(); }
if(sys_status&SS_CAP	/* Writes to Capture File */
	&& (sys_status&SS_ANSCAP || (ch!=ESC && !lclaes())))
	fwrite(&ch,1,1,capfile);
if(console&CON_L_ECHO) {
	if(console&CON_L_ECHOX && (uchar)ch>=SP)
		outcon('X');
	else if(node_misc&NM_NOBEEP && ch==7);	 /* Do nothing if beep */
	else if(ch==7) {
			beep(2000,110);
			nosound(); }
	else outcon(ch); }
if(online==ON_REMOTE && console&CON_R_ECHO) {
	if(console&CON_R_ECHOX && (uchar)ch>=SP)
		ch='X';
	i=0;
	while(outcom(ch)&TXBOF && i<1440) { /* 3 minute pause delay */

        if(lkbrd(1)) {
			lch=lkbrd(0);	/* ctrl-c */
			if(lch==3) {
				lputs("local abort (outchar)\r\n");
				i=1440;
				break; }
			ungetkey(lch); }
		if(!DCDHIGH)
			break;
		i++;
		if(sys_status&SS_SYSPAGE)
			beep(i,80);
		else
			mswait(80); }
	if(i==1440) {							/* timeout - beep flush outbuf */
		i=rioctl(TXBC);
		lprintf("timeout(outchar) %04X %04X\r\n",i,rioctl(IOFO));
		outcom(7);
		lputc(7);
		rioctl(IOCS|PAUSE); } }
if(ch==LF) {
	lncntr++;
	lbuflen=0;
	tos=0; }
else if(ch==FF) {
	lncntr=0;
	lbuflen=0;
	tos=1; }

else {
	if(!lbuflen)
		latr=curatr;
	if(lbuflen<LINE_BUFSIZE)
		lbuf[lbuflen++]=ch; }

if(lncntr==rows-1 && ((useron.misc&UPAUSE && !(sys_status&SS_PAUSEOFF))
	|| sys_status&SS_PAUSEON)) {
	lncntr=0;
	pause(); }

}


/****************************************************************************/
/* performs the correct attribute modifications for the Ctrl-A code			*/
/****************************************************************************/
void ctrl_a(char x)
{
	int i,j;
	char tmp1[128],atr=curatr;

if(x && (uchar)x<ESC) {    /* Ctrl-A through Ctrl-Z for users with MF only */
	if(!(useron.flags1&FLAG(x+64)))
		console^=(CON_ECHO_OFF);
	return; }
if((uchar)x>=0x7f) {
	if(useron.misc&ANSI)
		bprintf("\x1b[%uC",(uchar)x-0x7f);
	else
		for(i=0;i<(uchar)x-0x7f;i++)
			outchar(SP);
	return; }
switch(toupper(x)) {
	case '!':   /* level 10 or higher */
		if(useron.level<10)
			console^=CON_ECHO_OFF;
		break;
	case '@':   /* level 20 or higher */
		if(useron.level<20)
			console^=CON_ECHO_OFF;
        break;
	case '#':   /* level 30 or higher */
		if(useron.level<30)
			console^=CON_ECHO_OFF;
        break;
	case '$':   /* level 40 or higher */
		if(useron.level<40)
			console^=CON_ECHO_OFF;
        break;
	case '%':   /* level 50 or higher */
		if(useron.level<50)
			console^=CON_ECHO_OFF;
        break;
	case '^':   /* level 60 or higher */
		if(useron.level<60)
			console^=CON_ECHO_OFF;
        break;
	case '&':   /* level 70 or higher */
		if(useron.level<70)
			console^=CON_ECHO_OFF;
        break;
	case '*':   /* level 80 or higher */
		if(useron.level<80)
			console^=CON_ECHO_OFF;
        break;
	case '(':   /* level 90 or higher */
		if(useron.level<90)
			console^=CON_ECHO_OFF;
        break;
	case ')':   /* turn echo back on */
		console&=~CON_ECHO_OFF;
		break;
	case '-':								/* turn off all attributes if */
		if(atr&(HIGH|BLINK|(LIGHTGRAY<<4)))	/* high intensity, blink or */
			attr(LIGHTGRAY);				/* background bits are set */
		break;
	case '_':								/* turn off all attributes if */
		if(atr&(BLINK|(LIGHTGRAY<<4)))		/* blink or background is set */
			attr(LIGHTGRAY);
		break;
	case 'P':	/* Pause */
		pause();
		break;
	case 'Q':   /* Pause reset */
		lncntr=0;
		break;
	case 'T':   /* Time */
		now=time(NULL);
		unixtodos(now,&date,&curtime);
		bprintf("%02d:%02d %s"
			,curtime.ti_hour==0 ? 12
			: curtime.ti_hour>12 ? curtime.ti_hour-12
			: curtime.ti_hour, curtime.ti_min, curtime.ti_hour>11 ? "pm":"am");
		break;
	case 'D':   /* Date */
		now=time(NULL);
		bputs(unixtodstr(now,tmp1));
		break;
	case ',':   /* Delay 1/10 sec */
		mswait(100);
		break;
	case ';':   /* Delay 1/2 sec */
		mswait(500);
		break;
	case '.':   /* Delay 2 secs */
		mswait(2000);
		break;
	case 'S':   /* Synchronize */
		ASYNC;
		break;
	case 'L':	/* CLS (form feed) */
		CLS;
		break;
	case '>':   /* CLREOL */
		if(useron.misc&ANSI)
			bputs("\x1b[K");
		else {
			i=j=lclwx();
			while(i++<79)
				outchar(SP);
			while(j++<79)
				outchar(BS); }
		break;
	case '<':   /* Non-destructive backspace */
		outchar(BS);
		break;
	case '[':   /* Carriage return */
		outchar(CR);
		break;
	case ']':   /* Line feed */
		outchar(LF);
		break;
	case 'A':   /* Ctrl-A */
		outchar(1);
		break;
	case 'H': 	/* High intensity */
		atr|=HIGH;
		attr(atr);
		break;
	case 'I':	/* Blink */
		atr|=BLINK;
		attr(atr);
		break;
	case 'N': 	/* Normal */
		attr(LIGHTGRAY);
		break;
	case 'R':
		atr=(atr&0xf8)|RED;
		attr(atr);
		break;
	case 'G':
		atr=(atr&0xf8)|GREEN;
		attr(atr);
		break;
	case 'B':
		atr=(atr&0xf8)|BLUE;
		attr(atr);
		break;
    case 'W':	/* White */
		atr=(atr&0xf8)|LIGHTGRAY;
		attr(atr);
		break;
    case 'C':
		atr=(atr&0xf8)|CYAN;
		attr(atr);
		break;
	case 'M':
		atr=(atr&0xf8)|MAGENTA;
		attr(atr);
		break;
	case 'Y':   /* Yellow */
		atr=(atr&0xf8)|BROWN;
		attr(atr);
		break;
    case 'K':	/* Black */
		atr=(atr&0xf8)|BLACK;
		attr(atr);
		break;
    case '0':	/* Black Background */
		atr=(atr&0x8f)|(BLACK<<4);
		attr(atr);
		break;
	case '1':	/* Red Background */
		atr=(atr&0x8f)|(RED<<4);
		attr(atr);
		break;
	case '2':	/* Green Background */
		atr=(atr&0x8f)|(GREEN<<4);
		attr(atr);
		break;
	case '3':	/* Yellow Background */
		atr=(atr&0x8f)|(BROWN<<4);
		attr(atr);
		break;
	case '4':	/* Blue Background */
		atr=(atr&0x8f)|(BLUE<<4);
		attr(atr);
		break;
	case '5':	/* Magenta Background */
		atr=(atr&0x8f)|(MAGENTA<<4);
		attr(atr);
		break;
	case '6':	/* Cyan Background */
		atr=(atr&0x8f)|(CYAN<<4);
		attr(atr);
		break;
	case '7':	/* White Background */
		atr=(atr&0x8f)|(LIGHTGRAY<<4);
		attr(atr);
		break; }
}

/***************************************************************************/
/* Changes local and remote text attributes accounting for monochrome      */
/***************************************************************************/
/****************************************************************************/
/* Sends ansi codes to change remote ansi terminal's colors                 */
/* Only sends necessary codes - tracks remote terminal's current attributes */
/* through the 'curatr' variable                                            */
/****************************************************************************/
void attr(int atr)
{

if(!(useron.misc&ANSI))
    return;
if(!(useron.misc&COLOR)) {  /* eliminate colors if user doesn't have them */
    if(atr&LIGHTGRAY)       /* if any foreground bits set, set all */
        atr|=LIGHTGRAY;
    if(atr&(LIGHTGRAY<<4))  /* if any background bits set, set all */
        atr|=(LIGHTGRAY<<4);
    if(atr&LIGHTGRAY && atr&(LIGHTGRAY<<4))
        atr&=~LIGHTGRAY;    /* if background is solid, foreground is black */
    if(!atr)
        atr|=LIGHTGRAY; }   /* don't allow black on black */
if(curatr==atr) /* text hasn't changed. don't send codes */
    return;

if((!(atr&HIGH) && curatr&HIGH) || (!(atr&BLINK) && curatr&BLINK)
    || atr==LIGHTGRAY) {
    bputs("\x1b[0m");
    curatr=LIGHTGRAY; }

if(atr==LIGHTGRAY)                  /* no attributes */
    return;

if(atr&BLINK) {                     /* special attributes */
    if(!(curatr&BLINK))
        bputs(ansi(BLINK)); }
if(atr&HIGH) {
    if(!(curatr&HIGH))
        bputs(ansi(HIGH)); }

if((atr&0x7)==BLACK) {              /* foreground colors */
    if((curatr&0x7)!=BLACK)
        bputs(ansi(BLACK)); }
else if((atr&0x7)==RED) {
    if((curatr&0x7)!=RED)
        bputs(ansi(RED)); }
else if((atr&0x7)==GREEN) {
    if((curatr&0x7)!=GREEN)
        bputs(ansi(GREEN)); }
else if((atr&0x7)==BROWN) {
    if((curatr&0x7)!=BROWN)
        bputs(ansi(BROWN)); }
else if((atr&0x7)==BLUE) {
    if((curatr&0x7)!=BLUE)
        bputs(ansi(BLUE)); }
else if((atr&0x7)==MAGENTA) {
    if((curatr&0x7)!=MAGENTA)
        bputs(ansi(MAGENTA)); }
else if((atr&0x7)==CYAN) {
    if((curatr&0x7)!=CYAN)
        bputs(ansi(CYAN)); }
else if((atr&0x7)==LIGHTGRAY) {
    if((curatr&0x7)!=LIGHTGRAY)
        bputs(ansi(LIGHTGRAY)); }

if((atr&0x70)==(BLACK<<4)) {        /* background colors */
    if((curatr&0x70)!=(BLACK<<4))
        bputs("\x1b[40m"); }
else if((atr&0x70)==(RED<<4)) {
    if((curatr&0x70)!=(RED<<4))
        bputs(ansi(RED<<4)); }
else if((atr&0x70)==(GREEN<<4)) {
    if((curatr&0x70)!=(GREEN<<4))
        bputs(ansi(GREEN<<4)); }
else if((atr&0x70)==(BROWN<<4)) {
    if((curatr&0x70)!=(BROWN<<4))
        bputs(ansi(BROWN<<4)); }
else if((atr&0x70)==(BLUE<<4)) {
    if((curatr&0x70)!=(BLUE<<4))
        bputs(ansi(BLUE<<4)); }
else if((atr&0x70)==(MAGENTA<<4)) {
    if((curatr&0x70)!=(MAGENTA<<4))
        bputs(ansi(MAGENTA<<4)); }
else if((atr&0x70)==(CYAN<<4)) {
    if((curatr&0x70)!=(CYAN<<4))
        bputs(ansi(CYAN<<4)); }
else if((atr&0x70)==(LIGHTGRAY<<4)) {
    if((curatr&0x70)!=(LIGHTGRAY<<4))
        bputs(ansi(LIGHTGRAY<<4)); }

curatr=atr;
}

/****************************************************************************/
/* Returns the ANSI code to obtain the value of atr. Mixed attributes		*/
/* high intensity colors, or background/forground cobinations don't work.   */
/* A call to attr is more appropriate, being it is intelligent				*/
/****************************************************************************/
char *ansi(char atr)
{

switch(atr) {
	case (char)BLINK:
		return("\x1b[5m");
	case HIGH:
		return("\x1b[1m");
	case BLACK:
		return("\x1b[30m");
	case RED:
		return("\x1b[31m");
	case GREEN:
		return("\x1b[32m");
	case BROWN:
		return("\x1b[33m");
	case BLUE:
		return("\x1b[34m");
	case MAGENTA:
		return("\x1b[35m");
	case CYAN:
		return("\x1b[36m");
	case LIGHTGRAY:
		return("\x1b[37m");
	case (RED<<4):
		return("\x1b[41m");
	case (GREEN<<4):
		return("\x1b[42m");
	case (BROWN<<4):
		return("\x1b[43m");
	case (BLUE<<4):
		return("\x1b[44m");
	case (MAGENTA<<4):
		return("\x1b[45m");
	case (CYAN<<4):
		return("\x1b[46m");
	case (LIGHTGRAY<<4):
		return("\x1b[47m"); }

return("-Invalid use of ansi()-");
}


/****************************************************************************/
/* Checks to see if user has hit Pause or Abort. Returns 1 if user aborted. */
/* If the user hit Pause, waits for a key to be hit.                        */
/* Emulates remote XON/XOFF flow control on local console                   */
/* Preserves SS_ABORT flag state, if already set.                           */
/* Called from various listing procedures that wish to check for abort      */
/****************************************************************************/
char msgabort()
{
    char ch;

if(sys_status&SS_SYSPAGE)
	beep(random(800),1);
if(lkbrd(1)) {
    ch=inkey(0);
    if(sys_status&SS_ABORT) {                   /* ^c */
        keybufbot=keybuftop=0;
        return(1); }
    else if(ch==17 && online==ON_REMOTE)    /* ^q */
        rioctl(IOCS|PAUSE);
    else if(ch==19)                         /* ^s */
        while(online) {
            if((ch=inkey(0))!=0) {
                if(ch==17)  /* ^q */
                    return(0);
                ungetkey(ch); }
             if(sys_status&SS_ABORT) { /* ^c */
                keybufbot=keybuftop=0;
                return(1); }
             checkline(); }
    else if(ch)
        ungetkey(ch); }
checkline();
if(sys_status&SS_ABORT)
    return(1);
if(online==ON_REMOTE && rioctl(IOSTATE)&ABORT) {
    rioctl(IOCS|ABORT);
    sys_status|=SS_ABORT;
    return(1); }
if(!online)
    return(1);
return(0);
}


/****************************************************************************/
/* Takes the value 'sec' and makes a string the format HH:MM:SS             */
/****************************************************************************/
char *sectostr(uint sec,char *str)
{
    uchar hour,min,sec2;

hour=(sec/60)/60;
min=(sec/60)-(hour*60);
sec2=sec-((min+(hour*60))*60);
sprintf(str,"%2.2d:%2.2d:%2.2d",hour,min,sec2);
return(str);
}


/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()                                        */
/****************************************************************************/
char *timestr(time_t *intime)
{
    static char str[256];
    char mer[3],hour;
    struct tm *gm;

gm=localtime(intime);
if(gm==NULL) {
	strcpy(str,"Invalid Time");
	return(str); }
if(sys_misc&SM_MILITARY) {
    sprintf(str,"%s %s %02d %4d %02d:%02d:%02d"
        ,wday[gm->tm_wday],mon[gm->tm_mon],gm->tm_mday,1900+gm->tm_year
        ,gm->tm_hour,gm->tm_min,gm->tm_sec);
    return(str); }
if(gm->tm_hour>=12) {
    if(gm->tm_hour==12)
        hour=12;
    else
        hour=gm->tm_hour-12;
    strcpy(mer,"pm"); }
else {
    if(gm->tm_hour==0)
        hour=12;
    else
        hour=gm->tm_hour;
    strcpy(mer,"am"); }
sprintf(str,"%s %s %02d %4d %02d:%02d %s"
    ,wday[gm->tm_wday],mon[gm->tm_mon],gm->tm_mday,1900+gm->tm_year
    ,hour,gm->tm_min,mer);
return(str);
}

