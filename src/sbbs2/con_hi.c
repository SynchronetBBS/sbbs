#line 1 "CON_HI.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

extern char question[128];

char *mnestr;

/****************************************************************************/
/* Waits for remote or local user to input a CR terminated string. 'length' */
/* is the maximum number of characters that getstr will allow the user to   */
/* input into the string. 'mode' specifies upper case characters are echoed */
/* or wordwrap or if in message input (^A sequences allowed). ^W backspaces */
/* a word, ^X backspaces a line, ^Gs, BSs, TABs are processed, LFs ignored. */
/* ^N non-destructive BS, ^V center line. Valid keys are echoed.            */
/****************************************************************************/
int getstr(char *strout, int maxlen, long mode)
{
    int i,l,x,z;    /* i=current position, l=length, j=printed chars */
                    /* x&z=misc */
    uchar ch,str1[256],str2[256],ins=0,atr;

console&=~CON_UPARROW;
sys_status&=~SS_ABORT;
if(mode&K_LINE && useron.misc&ANSI && !(mode&K_NOECHO)) {
    attr(color[clr_inputline]);
    for(i=0;i<maxlen;i++)
        outchar(SP);
    bprintf("\x1b[%dD",maxlen); }
if(wordwrap[0]) {
    strcpy(str1,wordwrap);
    wordwrap[0]=0; }
else str1[0]=0;
if(mode&K_EDIT)
    strcat(str1,strout);
if(strlen(str1)>maxlen)
    str1[maxlen]=0;
atr=curatr;
if(!(mode&K_NOECHO)) {
	if(mode&K_AUTODEL && str1[0]) {
		i=(color[clr_inputline]&0x77)<<4;
		i|=(color[clr_inputline]&0x77)>>4;
		attr(i); }
	rputs(str1);
	if(mode&K_EDIT && !(mode&(K_LINE|K_AUTODEL)) && useron.misc&ANSI)
		bputs("\x1b[K");  /* destroy to eol */ }

i=l=strlen(str1);
if(mode&K_AUTODEL && str1[0] && !(mode&K_NOECHO)) {
	ch=getkey(mode|K_GETSTR);
    attr(atr);
    if(isprint(ch) || ch==0x7f) {
        for(i=0;i<l;i++)
            bputs("\b \b");
        i=l=0; }
    else {
        for(i=0;i<l;i++)
            outchar(BS);
        rputs(str1);
        i=l; }
	if(ch!=SP && ch!=TAB)
		ungetkey(ch); }

while(!(sys_status&SS_ABORT) && (ch=getkey(mode|K_GETSTR))!=CR && online) {
	if(sys_status&SS_ABORT)
		break;
	if(ch==LF) /* Ctrl-J same as CR */
		break;
	if(ch==TAB && !(mode&K_WRAP))	/* TAB same as CR */
		break;
	if(!i && mode&K_UPRLWR && (ch==SP || ch==TAB))
		continue;	/* ignore beginning white space if upper/lower */
    if(mode&K_E71DETECT && ch==(CR|0x80) && l>1) {
        if(strstr(str1,"")) {
            bputs("\r\n\r\nYou must set your terminal to NO PARITY, "
                "8 DATA BITS, and 1 STOP BIT (N-8-1).\7\r\n");
            return(0); } }
    switch(ch) {
        case 1: /* Ctrl-A for ANSI */
            if(!(mode&K_MSG) || useron.rest&FLAG('A') || i>maxlen-3)
                break;
            if(ins) {
                if(l<maxlen)
                    l++;
                for(x=l;x>i;x--)
                    str1[x]=str1[x-1];
                rprintf("%.*s",l-i,str1+i);
                rprintf("\x1b[%dD",l-i);
                if(i==maxlen-1)
                    ins=0; }
            outchar(str1[i++]=1);
            break;
        case 2: /* Ctrl-B Beginning of Line */
			if(useron.misc&ANSI && i && !(mode&K_NOECHO)) {
                bprintf("\x1b[%dD",i);
                i=0; }
            break;
        case 4: /* Ctrl-D Delete word right */
            if(i<l) {
                x=i;
                while(x<l && str1[x]!=SP) {
                    outchar(SP);
                    x++; }
                while(x<l && str1[x]==SP) {
                    outchar(SP);
                    x++; }
                bprintf("\x1b[%dD",x-i);   /* move cursor back */
                z=i;
                while(z<l-(x-i))  {             /* move chars in string */
                    outchar(str1[z]=str1[z+(x-i)]);
                    z++; }
                while(z<l) {                    /* write over extra chars */
                    outchar(SP);
                    z++; }
                bprintf("\x1b[%dD",z-i);
                l-=x-i; }                       /* l=new length */
            break;
        case 5: /* Ctrl-E End of line */
			if(useron.misc&ANSI && i<l) {
                bprintf("\x1b[%dC",l-i);  /* move cursor to eol */
                i=l; }
            break;
        case 6: /* Ctrl-F move cursor forewards */
            if(i<l && (useron.misc&ANSI)) {
                bputs("\x1b[C");   /* move cursor right one */
                i++; }
            break;
        case 7:
            if(!(mode&K_MSG))
                break;
            if(useron.rest&FLAG('B')) {
                if (i+6<maxlen) {
                    if(ins) {
                        for(x=l+6;x>i;x--)
                            str1[x]=str1[x-6];
                        if(l+5<maxlen)
                            l+=6;
                        if(i==maxlen-1)
                            ins=0; }
                    str1[i++]='(';
                    str1[i++]='b';
                    str1[i++]='e';
                    str1[i++]='e';
                    str1[i++]='p';
                    str1[i++]=')';
					if(!(mode&K_NOECHO))
						bputs("(beep)"); }
                if(ins)
                    redrwstr(str1,i,l,0);
                break; }
             if(ins) {
                if(l<maxlen)
                    l++;
                for(x=l;x>i;x--)
                    str1[x]=str1[x-1];
                if(i==maxlen-1)
                    ins=0; }
             if(i<maxlen) {
                str1[i++]=7;
				if(!(mode&K_NOECHO))
					outchar(7); }
             break;
        case 14:    /* Ctrl-N Next word */
            if(i<l && (useron.misc&ANSI)) {
                x=i;
                while(str1[i]!=SP && i<l)
                    i++;
                while(str1[i]==SP && i<l)
                    i++;
                bprintf("\x1b[%dC",i-x); }
            break;
        case 0x1c:    /* Ctrl-\ Previous word */
			if(i && (useron.misc&ANSI) && !(mode&K_NOECHO)) {
                x=i;
                while(str1[i-1]==SP && i)
                    i--;
                while(str1[i-1]!=SP && i)
                    i--;
                bprintf("\x1b[%dD",x-i); }
            break;
        case 18:    /* Ctrl-R Redraw Line */
			if(!(mode&K_NOECHO))
				redrwstr(str1,i,l,0);
            break;
        case TAB:
            if(!(i%TABSIZE)) {
                if(ins) {
                    if(l<maxlen)
                        l++;
                    for(x=l;x>i;x--)
                        str1[x]=str1[x-1];
                    if(i==maxlen-1)
                        ins=0; }
                str1[i++]=SP;
				if(!(mode&K_NOECHO))
					outchar(SP); }
            while(i<maxlen && i%TABSIZE) {
                if(ins) {
                    if(l<maxlen)
                        l++;
                    for(x=l;x>i;x--)
                        str1[x]=str1[x-1];
                    if(i==maxlen-1)
                        ins=0; }
                str1[i++]=SP;
				if(!(mode&K_NOECHO))
					outchar(SP); }
			if(ins && !(mode&K_NOECHO))
                redrwstr(str1,i,l,0);
            break;
        case BS:
            if(!i)
                break;
            i--;
            l--;
            if(i!=l) {              /* Deleting char in middle of line */
                outchar(BS);
                z=i;
                while(z<l)  {       /* move the characters in the line */
                    outchar(str1[z]=str1[z+1]);
                    z++; }
                outchar(SP);        /* write over the last char */
                bprintf("\x1b[%dD",(l-i)+1); }
			else if(!(mode&K_NOECHO))
                bputs("\b \b");
            break;
        case 22:    /* Ctrl-V   Center line */
            str1[l]=0;
            l=bstrlen(str1);
            if(!l) break;
            for(x=0;x<(maxlen-l)/2;x++)
                str2[x]=SP;
            str2[x]=0;
            strcat(str2,str1);
            strcpy(strout,str2);
            l=strlen(strout);
			if(mode&K_NOECHO)
				return(l);
            if(mode&K_MSG)
                redrwstr(strout,i,l,K_MSG);
            else {
                while(i--)
                    bputs("\b");
                bputs(strout);
                if(mode&K_LINE)
                    attr(LIGHTGRAY); }
            CRLF;
            return(l);
        case 23:    /* Ctrl-W   Delete word left */
            if(i<l) {
                x=i;                            /* x=original offset */
                while(i && str1[i-1]==SP) {
                    outchar(BS);
                    i--; }
                while(i && str1[i-1]!=SP) {
                    outchar(BS);
                    i--; }
                z=i;                            /* i=z=new offset */
                while(z<l-(x-i))  {             /* move chars in string */
                    outchar(str1[z]=str1[z+(x-i)]);
                    z++; }
                while(z<l) {                    /* write over extra chars */
                    outchar(SP);
                    z++; }
                bprintf("\x1b[%dD",z-i);        /* back to new x corridnant */
                l-=x-i; }                       /* l=new length */
            else {
                while(i && str1[i-1]==SP) {
                    i--;
                    l--;
					if(!(mode&K_NOECHO))
						bputs("\b \b"); }
                while(i && str1[i-1]!=SP) {
                    i--;
                    l--;
					if(!(mode&K_NOECHO))
						bputs("\b \b"); } }
            break;
        case 24:    /* Ctrl-X   Delete entire line */
			if(mode&K_NOECHO)
				l=0;
			else {
				while(i<l) {
					outchar(SP);
					i++; }
				while(l) {
					l--;
					bputs("\b \b"); } }
            i=0;
            break;
        case 25:    /* Ctrl-Y   Delete to end of line */
			if(useron.misc&ANSI && !(mode&K_NOECHO)) {
                bputs("\x1b[K");
                l=i; }
            break;
        case 31:    /* Ctrl-Minus       Toggles Insert/Overwrite */
			if(!(useron.misc&ANSI) || mode&K_NOECHO)
                break;
            if(ins) {
                ins=0;
                redrwstr(str1,i,l,0); }
            else if(i<l) {
                ins=1;
				bprintf("\x1b[s\x1b[%dC",79-lclwx());    /* save pos  */
                z=curatr;                                /* and got to EOL */
                attr(z|BLINK|HIGH);
                outchar('°');
                attr(z);
                bputs("\x1b[u"); }  /* restore pos */
            break;
        case 0x1e:  /* Ctrl-^ */
            if(!(mode&K_EDIT))
                break;
            if(i>l)
                l=i;
            str1[l]=0;
            strcpy(strout,str1);
			if((stripattr(strout) || ins) && !(mode&K_NOECHO))
                redrwstr(strout,i,l,K_MSG);
			if(mode&K_LINE && !(mode&K_NOECHO))
				attr(LIGHTGRAY);
            console|=CON_UPARROW;
            return(l);
        case 0x1d:  /* Ctrl-]  Reverse Cursor Movement */
			if(i && (useron.misc&ANSI) && !(mode&K_NOECHO)) {
                bputs("\x1b[D");   /* move cursor left one */
                i--; }
            break;
        case 0x7f:  /* Ctrl-BkSpc (DEL) Delete current char */
            if(i==l)
                break;
            l--;
            z=i;
            while(z<l)  {       /* move the characters in the line */
                outchar(str1[z]=str1[z+1]);
                z++; }
            outchar(SP);        /* write over the last char */
            bprintf("\x1b[%dD",(l-i)+1);
            break;
        default:
            if(mode&K_WRAP && i==maxlen && ch>=SP && !ins) {
                str1[i]=0;
				if(ch==SP && !(mode&K_CHAT)) { /* don't wrap a space */ 
					strcpy(strout,str1);	   /* as last char */
					if(stripattr(strout) && !(mode&K_NOECHO))
                        redrwstr(strout,i,l,K_MSG);
					if(!(mode&K_NOECHO))
						CRLF;
                    return(i); }
                x=i-1;
                z=1;
                wordwrap[0]=ch;
                while(str1[x]!=SP && x)
                    wordwrap[z++]=str1[x--];
                if(x<(maxlen/2)) {
                    wordwrap[1]=0;  /* only wrap one character */
                    strcpy(strout,str1);
					if(stripattr(strout) && !(mode&K_NOECHO))
                        redrwstr(strout,i,l,K_MSG);
					if(!(mode&K_NOECHO))
						CRLF;
                    return(i); }
                wordwrap[z]=0;
				if(!(mode&K_NOECHO))
					while(z--) {
						bputs("\b \b");
						i--; }
                strrev(wordwrap);
                str1[x]=0;
                strcpy(strout,str1);
				if(stripattr(strout) && !(mode&K_NOECHO))
					redrwstr(strout,i,x,mode);
				if(!(mode&K_NOECHO))
					CRLF;
                return(x); }
            if(i<maxlen && ch>=SP) {
                if(mode&K_UPRLWR)
                    if(!i || (i && (str1[i-1]==SP || str1[i-1]=='-'
                        || str1[i-1]=='.' || str1[i-1]=='_')))
                        ch=toupper(ch);
                    else
                        ch=tolower(ch);
                if(ins) {
                    if(l<maxlen)    /* l<maxlen */
                        l++;
                    for(x=l;x>i;x--)
                        str1[x]=str1[x-1];
					rprintf("%.*s",l-i,str1+i);
					rprintf("\x1b[%dD",l-i);
					if(i==maxlen-1) {
						bputs("  \b\b");
						ins=0; } }
                str1[i++]=ch;
				if(!(mode&K_NOECHO))
					outchar(ch); } }
    if(i>l)
        l=i;
    if(mode&K_CHAT && !l)
        return(0); }
if(!online)
    return(0);
if(i>l)
    l=i;
str1[l]=0;
if(!(sys_status&SS_ABORT)) {
    strcpy(strout,str1);
	if((stripattr(strout) || ins) && !(mode&K_NOECHO))
        redrwstr(strout,i,l,K_MSG); }
else
    l=0;
if(mode&K_LINE && !(mode&K_NOECHO)) attr(LIGHTGRAY);
if(!(mode&(K_NOCRLF|K_NOECHO))) {
    outchar(CR);
    if(!(mode&K_MSG && sys_status&SS_ABORT))
        outchar(LF);
    lncntr=0; }
return(l);
}

/****************************************************************************/
/* Hot keyed number input routine.                                          */
/* Returns a valid number between 1 and max, 0 if no number entered, or -1  */
/* if the user hit 'Q' or ctrl-c                                            */
/****************************************************************************/
long getnum(ulong max)
{
    uchar ch,n=0;
	long i=0;

while(online) {
	ch=getkey(K_UPPER);
    if(ch>0x7f)
        continue;
	if(ch=='Q') {
		outchar('Q');
		if(useron.misc&COLDKEYS)
			ch=getkey(K_UPPER);
		if(ch==BS) {
			bputs("\b \b");
			continue; }
        CRLF;
        lncntr=0;
        return(-1); }
    else if(sys_status&SS_ABORT) {
        CRLF;
        lncntr=0;
        return(-1); }
    else if(ch==CR) {
        CRLF;
        lncntr=0;
        return(i); }
    else if(ch==BS && n) {
        bputs("\b \b");
        i/=10;
        n--; }
	else if(isdigit(ch) && (i*10L)+(ch&0xf)<=max && (ch!='0' || n)) {
		i*=10L;
        n++;
        i+=ch&0xf;
        outchar(ch);
		if(i*10L>max && !(useron.misc&COLDKEYS)) {
            CRLF;
            lncntr=0;
            return(i); } } }
return(0);
}

/*****************************************************************************/
/* Displays or erases [WAIT] message                                         */
/*****************************************************************************/
void waitforsysop(char on)
{
    static saveatr;
    int i,j;

if(on) {
	saveatr=curatr;
    bputs(text[Wait]);
    lclatr(LIGHTGRAY);
    return; }
j=bstrlen(text[Wait]);
attr(saveatr);
for(i=0;i<j;i++)
    bputs("\b \b");
}

/****************************************************************************/
/* Returns 1 if a is a valid ctrl-a code, 0 if it isn't.                    */
/****************************************************************************/
char validattr(char a)
{

switch(toupper(a)) {
    case '-':   /* clear        */
    case '_':   /* clear        */
    case 'B':   /* blue     fg  */
    case 'C':   /* cyan     fg  */
    case 'G':   /* green    fg  */
    case 'H':   /* high     fg  */
    case 'I':   /* blink        */
    case 'K':   /* black    fg  */
    case 'L':   /* cls          */
    case 'M':   /* magenta  fg  */
    case 'N':   /* normal       */
    case 'P':   /* pause        */
    case 'R':   /* red      fg  */
    case 'W':   /* white    fg  */
    case 'Y':   /* yellow   fg  */
    case '0':   /* black    bg  */
    case '1':   /* red      bg  */
    case '2':   /* green    bg  */
    case '3':   /* brown    bg  */
    case '4':   /* blue     bg  */
    case '5':   /* magenta  bg  */
    case '6':   /* cyan     bg  */
    case '7':   /* white    bg  */
		return(1); }
return(0);
}

/****************************************************************************/
/* Strips invalid Ctrl-Ax sequences from str                                */
/* Returns number of ^A's in line                                           */
/****************************************************************************/
char stripattr(char *strin)
{
    uchar str[256];
    uchar a,c,d,e;

e=strlen(strin);
for(a=c=d=0;c<e;c++) {
    if(strin[c]==1) {
        a++;
        if(!validattr(strin[c+1])) {
            c++;
            continue; } }
    str[d++]=strin[c]; }
str[d]=0;
strcpy(strin,str);
return(a);
}

/****************************************************************************/
/* Redraws str using i as current cursor position and l as length           */
/****************************************************************************/
void redrwstr(char *strin, int i, int l, char mode)
{
    char str[256],c;

sprintf(str,"%-*.*s",l,l,strin);
c=i;
while(c--)
    outchar(BS);
if(mode&K_MSG)
    bputs(str);
else
    rputs(str);
if(useron.misc&ANSI) {
    bputs("\x1b[K");
    if(i<l)
        bprintf("\x1b[%dD",l-i); }
else {
    while(c<79) { /* clear to end of line */
        outchar(SP);
        c++; }
    while(c>l) { /* back space to end of string */
        outchar(BS);
        c--; } }
}

/****************************************************************************/
/* Outputs a string highlighting characters preceeded by a tilde            */
/****************************************************************************/
void mnemonics(char *str)
{
    char *ctrl_a_codes;
    long l;

if(!strchr(str,'~')) {
	mnestr=str;
	bputs(str);
	return; }
ctrl_a_codes=strchr(str,1);
if(!ctrl_a_codes) {
	if(str[0]=='@' && str[strlen(str)-1]=='@' && !strchr(str,SP)) {
		mnestr=str;
		bputs(str);
		return; }
	attr(color[clr_mnelow]); }
l=0L;
while(str[l]) {
    if(str[l]=='~' && str[l+1]) {
        if(!(useron.misc&ANSI))
            outchar('(');
        l++;
        if(!ctrl_a_codes)
            attr(color[clr_mnehigh]);
        outchar(str[l]);
        l++;
        if(!(useron.misc&ANSI))
            outchar(')');
        if(!ctrl_a_codes)
            attr(color[clr_mnelow]); }
    else {
        if(str[l]==1) {             /* ctrl-a */
            ctrl_a(str[++l]);       /* skip the ctrl-a */
            l++; }                  /* skip the attribute code */
        else
            outchar(str[l++]); } }
if(!ctrl_a_codes)
    attr(color[clr_mnecmd]);
}

/****************************************************************************/
/* Prompts user for Y or N (yes or no) and CR is interpreted as a Y         */
/* Returns 1 for Y or 0 for N                                               */
/* Called from quite a few places                                           */
/****************************************************************************/
char yesno(char *str)
{
    char ch;

strcpy(question,str);
SYNC;
if(useron.misc&WIP) {
	strip_ctrl(question);
	menu("YESNO"); }
else
	bprintf(text[YesNoQuestion],str);
while(online) {
	if(sys_status&SS_ABORT)
		ch=text[YN][1];
	else
		ch=getkey(K_UPPER|K_COLD);
	if(ch==text[YN][0] || ch==CR) {
		if(bputs(text[Yes]))
			CRLF;
        lncntr=0;
        return(1); }
	if(ch==text[YN][1]) {
		if(bputs(text[No]))
			CRLF;
        lncntr=0;
        return(0); } }
return(1);
}

/****************************************************************************/
/* Prompts user for N or Y (no or yes) and CR is interpreted as a N         */
/* Returns 1 for N or 0 for Y                                               */
/* Called from quite a few places                                           */
/****************************************************************************/
char noyes(char *str)
{
    char ch;

strcpy(question,str);
SYNC;
if(useron.misc&WIP) {
	strip_ctrl(question);
	menu("NOYES"); }
else
	bprintf(text[NoYesQuestion],str);
while(online) {
	if(sys_status&SS_ABORT)
		ch=text[YN][1];
	else
		ch=getkey(K_UPPER|K_COLD);
	if(ch==text[YN][1] || ch==CR) {
		if(bputs(text[No]))
			CRLF;
        lncntr=0;
        return(1); }
	if(ch==text[YN][0]) {
		if(bputs(text[Yes]))
			CRLF;
        lncntr=0;
        return(0); } }
return(1);
}

/****************************************************************************/
/* Waits for remote or local user to hit a key that is contained inside str.*/
/* 'str' should contain uppercase characters only. When a valid key is hit, */
/* it is echoed (upper case) and is the return value.                       */
/* Called from quite a few functions                                        */
/****************************************************************************/
long getkeys(char *str, ulong max)
{
	uchar ch,n=0,c;
	ulong i=0;

strupr(str);
while(online) {
	ch=getkey(K_UPPER);
    if(max && ch>0x7f)  /* extended ascii chars are digits to isdigit() */
        continue;
    if(sys_status&SS_ABORT) {   /* return -1 if Ctrl-C hit */
        attr(LIGHTGRAY);
        CRLF;
        lncntr=0;
        return(-1); }
    if(ch && !n && (strchr(str,ch))) {  /* return character if in string */
		outchar(ch);
		if(useron.misc&COLDKEYS && ch>SP) {
			while(online && !(sys_status&SS_ABORT)) {
				c=getkey(0);
				if(c==CR || c==BS)
					break; }
			if(sys_status&SS_ABORT) {
				CRLF;
				return(-1); }
			if(c==BS) {
				bputs("\b \b");
				continue; } }
		attr(LIGHTGRAY);
        CRLF;
        lncntr=0;
        return(ch); }
    if(ch==CR && max) {             /* return 0 if no number */
        attr(LIGHTGRAY);
        CRLF;
        lncntr=0;
        if(n)
			return(i|0x80000000L);		 /* return number plus high bit */
        return(0); }
    if(ch==BS && n) {
        bputs("\b \b");
        i/=10;
        n--; }
    else if(max && isdigit(ch) && (i*10)+(ch&0xf)<=max && (ch!='0' || n)) {
        i*=10;
        n++;
        i+=ch&0xf;
        outchar(ch);
		if(i*10>max && !(useron.misc&COLDKEYS)) {
            attr(LIGHTGRAY);
            CRLF;
            lncntr=0;
			return(i|0x80000000L); } } }
return(-1);
}

void center(char *str)
{
	 int i,j;

j=bstrlen(str);
for(i=0;i<(80-j)/2;i++)
	outchar(SP);
bputs(str);
CRLF;
}


int uselect(int add, int n, char *title, char *item, char *ar)
{
	static uint total,num[500];
	char str[128];
	int i,t;

if(add) {
	if(ar && !chk_ar(ar,useron))
		return(0);
	if(!total)
		bprintf(text[SelectItemHdr],title);
	num[total++]=n;
	bprintf(text[SelectItemFmt],total,item);
	return(0); }

if(!total)
	return(-1);

for(i=0;i<total;i++)
	if(num[i]==n)
		break;
if(i==total)
	i=0;
sprintf(str,text[SelectItemWhich],i+1);
mnemonics(str);
i=getnum(total);
t=total;
total=0;
if(i<0)
	return(-1);
if(!i) {					/* User hit ENTER, use default */
	for(i=0;i<t;i++)
		if(num[i]==n)
			return(num[i]);
	if(n<t)
		return(num[n]);
	return(-1); }
return(num[i-1]);
}
