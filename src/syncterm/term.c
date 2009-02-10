/* Copyright (C), 2007 by Stephen Hurd */

/* $Id$ */

#include <genwrap.h>
#include <ciolib.h>
#include <cterm.h>
#include <mouse.h>
#include <keys.h>

#include "threadwrap.h"
#include "filewrap.h"
#include "xpbeep.h"

#include "conn.h"
#include "syncterm.h"
#include "term.h"
#include "uifcinit.h"
#include "filepick.h"
#include "menu.h"
#include "dirwrap.h"
#include "sexyz.h"
#include "zmodem.h"
#include "xmodem.h"
#include "telnet_io.h"
#ifdef WITH_WXWIDGETS
#include "htmlwin.h"
#endif

#ifdef GUTS_BUILTIN
#include "gutsz.h"
#endif

#ifndef WITHOUT_OOII
#include "ooii.h"
#endif

#define	ANSI_REPLY_BUFSIZE	2048

#define DUMP

struct terminal term;

#define TRANSFER_WIN_WIDTH	66
#define TRANSFER_WIN_HEIGHT	18
static char winbuf[(TRANSFER_WIN_WIDTH + 2) * (TRANSFER_WIN_HEIGHT + 1) * 2];	/* Save buffer for transfer window */
static struct text_info	trans_ti;
static struct text_info	log_ti;
#ifdef WITH_WXWIDGETS
enum html_mode {
	 HTML_MODE_HIDDEN
	,HTML_MODE_ICONIZED
	,HTML_MODE_RAISED
	,HTML_MODE_READING
};
static enum html_mode html_mode=HTML_MODE_HIDDEN;
enum {
	 HTML_SUPPORT_UNKNOWN
	,HTML_NOTSUPPORTED
	,HTML_SUPPORTED
};

static int html_supported=HTML_SUPPORT_UNKNOWN;

char *html_addr=NULL;
#endif

void setup_mouse_events(void)
{
	ciomouse_setevents(0);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_2_CLICK);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
void mousedrag(unsigned char *scrollback)
{
	int	key;
	struct mouse_event mevent;
	unsigned char *screen;
	unsigned char *sbuffer;
	int sbufsize;
	int pos, startpos,endpos, lines;
	int outpos;
	char *copybuf;
	int lastchar;

	sbufsize=term.width*2*term.height;
	screen=(unsigned char*)alloca(sbufsize);
	sbuffer=(unsigned char*)alloca(sbufsize);
	gettext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
	while(1) {
		key=getch();
		if(key==0 || key==0xff)
			key|=getch()<<8;
		switch(key) {
			case CIO_KEY_MOUSE:
				getmouse(&mevent);
				startpos=((mevent.starty-1)*term.width)+(mevent.startx-1);
				endpos=((mevent.endy-1)*term.width)+(mevent.endx-1);
				if(startpos>=term.width*term.height)
					startpos=term.width*term.height-1;
				if(endpos>=term.width*term.height)
					endpos=term.width*term.height-1;
				if(endpos<startpos) {
					pos=endpos;
					endpos=startpos;
					startpos=pos;
				}
				switch(mevent.event) {
					case CIOLIB_BUTTON_1_DRAG_MOVE:
						memcpy(sbuffer,screen,sbufsize);
						for(pos=startpos;pos<=endpos;pos++) {
							if((sbuffer[pos*2+1]&0x70)!=0x10)
								sbuffer[pos*2+1]=(sbuffer[pos*2+1]&0x8F)|0x10;
							else
								sbuffer[pos*2+1]=(sbuffer[pos*2+1]&0x8F)|0x60;
							if(((sbuffer[pos*2+1]&0x70)>>4) == (sbuffer[pos*2+1]&0x0F)) {
								sbuffer[pos*2+1]|=0x08;
							}
						}
						puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,sbuffer);
						break;
					default:
						lines=abs(mevent.endy-mevent.starty)+1;
						copybuf=alloca(endpos-startpos+4+lines*2);
						outpos=0;
						lastchar=0;
						for(pos=startpos;pos<=endpos;pos++) {
							copybuf[outpos++]=screen[pos*2];
							if(screen[pos*2]!=' ' && screen[pos*2])
								lastchar=outpos;
							if((pos+1)%term.width==0) {
								outpos=lastchar;
								#ifdef _WIN32
									copybuf[outpos++]='\r';
								#endif
								copybuf[outpos++]='\n';
								lastchar=outpos;
							}
						}
						copybuf[outpos]=0;
						copytext(copybuf, strlen(copybuf));
						puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
						return;
				}
				break;
			default:
				puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
				ungetch(key);
				return;
		}
	}
}

void update_status(struct bbslist *bbs, int speed, int ooii_mode)
{
	char nbuf[LIST_NAME_MAX+10+11+1];	/* Room for "Name (Logging) (115300)" and terminator */
						/* SAFE and Logging should me be possible. */
	int oldscroll;
	int olddmc=hold_update;
	struct	text_info txtinfo;
	int now;
	static int lastupd=0;
	static int oldspeed=0;
	int	timeon;

	now=time(NULL);
	if(now==lastupd && speed==oldspeed)
		return;
	lastupd=now;
	oldspeed=speed;
	timeon=now - bbs->connected;
    gettextinfo(&txtinfo);
	oldscroll=_wscroll;
	hold_update=TRUE;
	textattr(YELLOW|(BLUE<<4));
	/* Move to status line thinger */
	window(term.x-1,term.y+term.height-1,term.x+term.width-2,term.y+term.height-1);
	gotoxy(1,1);
	_wscroll=0;
	strcpy(nbuf, bbs->name);
	if(safe_mode)
		strcat(nbuf, " (SAFE)");
	if(cterm.log)
		strcat(nbuf, " (Logging)");
	if(speed)
		sprintf(strchr(nbuf,0)," (%d)", speed);
	if(cterm.doorway_mode)
		strcat(nbuf, " (DrWy)");
	switch(ooii_mode) {
	case 1:
		strcat(nbuf, " (OOTerm)");
		break;
	case 2:
		strcat(nbuf, " (OOTerm1)");
		break;
	case 3:
		strcat(nbuf, " (OOTerm2)");
		break;
	}
	switch(cio_api.mode) {
		case CIOLIB_MODE_CURSES:
		case CIOLIB_MODE_CURSES_IBM:
		case CIOLIB_MODE_ANSI:
			if(timeon>359999)
				cprintf(" %-29.29s \263 %-6.6s \263 Connected: Too Long \263 CTRL-S for menu ",nbuf,conn_types[bbs->conn_type]);
			else
				cprintf(" %-29.29s \263 %-6.6s \263 Connected: %02d:%02d:%02d \263 CTRL-S for menu ",nbuf,conn_types[bbs->conn_type],timeon/3600,(timeon/60)%60,timeon%60);
			break;
		default:
			if(timeon>359999)
				cprintf(" %-30.30s \263 %-6.6s \263 Connected: Too Long \263 "ALT_KEY_NAME3CH"-Z for menu ",nbuf,conn_types[bbs->conn_type]);
			else
				cprintf(" %-30.30s \263 %-6.6s \263 Connected: %02d:%02d:%02d \263 "ALT_KEY_NAME3CH"-Z for menu ",nbuf,conn_types[bbs->conn_type],timeon/3600,(timeon/60)%60,timeon%60);
			break; /*    1+29     +3    +6    +3    +11        +3+3+2        +3    +6    +4  +5 */
	}
	if(wherex()>=80)
		clreol();
	_wscroll=oldscroll;
	textattr(txtinfo.attribute);
	window(txtinfo.winleft,txtinfo.wintop,txtinfo.winright,txtinfo.winbottom);
	gotoxy(txtinfo.curx,txtinfo.cury);
	hold_update=olddmc;
}

#if defined(_WIN32) && defined(_DEBUG) && defined(DUMP)
void dump(BYTE* buf, int len)
{
	char str[128];
	int i,j;
	size_t slen=0;

	slen=sprintf(str,"RX: ");
	for(i=0;i<len;i+=j) {
		for(j=0;i+j<len && j<32;j++)
			slen+=sprintf(str+slen,"%02X ",buf[i+j]);
		OutputDebugString(str);
		slen=sprintf(str,"RX: ");
	}
}
#endif

/* Zmodem Stuff */
int log_level = LOG_INFO;

struct zmodem_cbdata {
	zmodem_t		*zm;
	struct bbslist	*bbs;
};

enum { ZMODEM_MODE_SEND, ZMODEM_MODE_RECV } zmodem_mode;

static BOOL zmodem_check_abort(void* vp)
{
	struct zmodem_cbdata	*zcb=(struct zmodem_cbdata *)vp;
	zmodem_t*				zm=zcb->zm;

	if(zm!=NULL && kbhit()) {
		switch(getch()) {
			case ESC:
			case CTRL_C:
			case CTRL_X:
				zm->cancelled=TRUE;
				zm->local_abort=TRUE;
				break;
		}
	}
	return(zm->cancelled);
}

extern FILE* log_fp;
extern char *log_levels[];

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static int lputs(void* cbdata, int level, const char* str)
{
	char msg[512];
	int chars;

#if defined(_WIN32) && defined(_DEBUG) && FALSE
	sprintf(msg,"SyncTerm: %s\n",str);
	OutputDebugString(msg);
#endif

	if(log_fp!=NULL && level <= log_level)
		fprintf(log_fp,"Xfer %s: %s\n",log_levels[level], str);

	if(level > LOG_INFO)
		return 0;

	/* Assumes the receive window has been drawn! */
	window(log_ti.winleft, log_ti.wintop, log_ti.winright, log_ti.winbottom);
	gotoxy(log_ti.curx, log_ti.cury);
	textbackground(BLUE);
	switch(level) {
		case LOG_DEBUG:
			textcolor(LIGHTCYAN);
			SAFEPRINTF(msg,"%s\r\n",str);
			break;
		case LOG_INFO:
			textcolor(WHITE);
			SAFEPRINTF(msg,"%s\r\n",str);
			break;
		case LOG_NOTICE:
			textcolor(YELLOW);
			SAFEPRINTF(msg,"%s\r\n",str);
			break;
		case LOG_WARNING:
			textcolor(LIGHTMAGENTA);
			SAFEPRINTF(msg,"Warning: %s\r\n",str);
			break;
		default:
			textcolor(LIGHTRED);
			SAFEPRINTF(msg,"!ERROR: %s\r\n",str);
			break;
	}
	chars=cputs(msg);
	gettextinfo(&log_ti);

	return chars;
}

static int lprintf(int level, const char *fmt, ...)
{
	char sbuf[1024];
	va_list argptr;

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(lputs(NULL,level,sbuf));
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
void zmodem_progress(void* cbdata, uint32_t current_pos)
{
	char		orig[128];
	unsigned	cps;
	time_t		l;
	time_t		t;
	time_t		now;
	static time_t last_progress;
	int			old_hold=hold_update;
	struct zmodem_cbdata *zcb=(struct zmodem_cbdata *)cbdata;
	zmodem_t*	zm=zcb->zm;

	zmodem_check_abort(cbdata);

	now=time(NULL);
	if(now-last_progress>0 || current_pos >= zm->current_file_size) {
		hold_update = TRUE;
		window(((trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2)+2
				, ((trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2)+1
				, ((trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2) + TRANSFER_WIN_WIDTH - 2
				, ((trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2)+5);
		gotoxy(1,1);
		textattr(LIGHTCYAN | (BLUE<<4));
		t=now-zm->transfer_start_time;
		if(t<=0)
			t=1;
		if(zm->transfer_start_pos>current_pos)
			zm->transfer_start_pos=0;
		if((cps=(current_pos-zm->transfer_start_pos)/t)==0)
			cps=1;		/* cps so far */
		l=zm->current_file_size/cps;	/* total transfer est time */
		l-=t;			/* now, it's est time left */
		if(l<0) l=0;
		cprintf("File (%u of %u): %-.*s"
			,zm->current_file_num, zm->total_files, TRANSFER_WIN_WIDTH - 20, zm->current_file_name);
		clreol();
		cputs("\r\n");
		if(zm->transfer_start_pos)
			sprintf(orig,"From: %lu  ", zm->transfer_start_pos);
		else
			orig[0]=0;
		cprintf("%sByte: %lu of %lu (%lu KB)"
			,orig, current_pos, zm->current_file_size, zm->current_file_size/1024);
		clreol();
		cputs("\r\n");
		cprintf("Time: %lu:%02lu  ETA: %lu:%02lu  Block: %u/CRC-%u  %u cps"
			,t/60L
			,t%60L
			,l/60L
			,l%60L
			,zm->block_size
			,zmodem_mode==ZMODEM_MODE_RECV ? (zm->receive_32bit_data ? 32:16) : 
				(zm->can_fcs_32 && !zm->want_fcs_16) ? 32:16
			,cps
			);
		clreol();
		cputs("\r\n");
		cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH/2-5, ""
			,(long)(((float)current_pos/(float)zm->current_file_size)*100.0));
		l = (long)(60*((float)current_pos/(float)zm->current_file_size));
		cprintf("[%*.*s%*s]", l, l, 
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				, 60-l, "");
		last_progress=now;
		hold_update = FALSE;
		gotoxy(wherex(), wherey());
		hold_update = old_hold;
	}
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static int send_byte(void* unused, uchar ch, unsigned timeout /* seconds */)
{
	return(conn_send(&ch,sizeof(ch),timeout*1000)!=1);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static int recv_byte(void* unused, unsigned timeout /* seconds */)
{
	BYTE	ch;

	if(conn_recv(&ch, sizeof(ch), timeout*1000))
		return(ch);
	return(-1);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
BOOL data_waiting(void* unused, unsigned timeout)
{
	return(conn_data_waiting()!=0);
}

void draw_transfer_window(char* title)
{
	char	outline[TRANSFER_WIN_WIDTH*2];
	char	shadow[TRANSFER_WIN_WIDTH*2];	/* Assumes that width*2 > height * 2 */
	int		i, top, left, old_hold;

	old_hold = hold_update;
	hold_update=TRUE;
	gettextinfo(&trans_ti);
	top=(trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2;
	left=(trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2;
	gettext(left, top, left + TRANSFER_WIN_WIDTH + 1, top + TRANSFER_WIN_HEIGHT, winbuf);
	memset(outline, YELLOW | (BLUE<<4), sizeof(outline));
	for(i=2;i < sizeof(outline) - 2; i+=2) {
		outline[i] = (char)0xcd;	/* Double horizontal line */
	}
	outline[0]=(char)0xc9;
	outline[sizeof(outline)-2]=(char)0xbb;
	puttext(left, top, left + TRANSFER_WIN_WIDTH - 1, top, outline);\

	/* Title */
	gotoxy(left+4,top);
	textattr(YELLOW|(BLUE<<4));
	cprintf("\xb5 %*s \xc6",strlen(title),"");
	gotoxy(left+6,top);
	textattr(WHITE|(BLUE<<4));
	cprintf("%s",title);

	for(i=2;i < sizeof(outline) - 2; i+=2) {
		outline[i] = (char)0xc4;	/* Single horizontal line */
	}
	outline[0] = (char)0xc7;	/* 0xcc */
	outline[sizeof(outline)-2]=(char)0xb6;	/* 0xb6 */
	puttext(left, top+6, left + TRANSFER_WIN_WIDTH - 1, top+6, outline);

	for(i=2;i < sizeof(outline) - 2; i+=2) {
		outline[i] = (char)0xcd;	/* Double horizontal line */
	}
	outline[0]=(char)0xc8;
	outline[sizeof(outline)-2]=(char)0xbc;
	puttext(left, top + TRANSFER_WIN_HEIGHT - 1, left+TRANSFER_WIN_WIDTH - 1, top + TRANSFER_WIN_HEIGHT - 1, outline);
	outline[0]=(char)0xba;
	outline[sizeof(outline)-2]=(char)0xba;
	for(i=2;i < sizeof(outline) - 2; i+=2) {
		outline[i] = ' ';
	}
	for(i=1; i<6; i++) {
		puttext(left, top + i, left + TRANSFER_WIN_WIDTH - 1, top+i, outline);
	}
/*	for(i=3;i < sizeof(outline) - 2; i+=2) { */
/*		outline[i] = LIGHTGRAY | (BLACK << 8); */
/*	} */
	for(i=7; i<TRANSFER_WIN_HEIGHT-1; i++) {
		puttext(left, top + i, left + TRANSFER_WIN_WIDTH - 1, top+i, outline);
	}

	/* Title */
	gotoxy(left + TRANSFER_WIN_WIDTH - 20, top + i);
	textattr(YELLOW|(BLUE<<4));
	cprintf("\xb5              \xc6");
	textattr(WHITE|(BLUE<<4));
	gotoxy(left + TRANSFER_WIN_WIDTH - 18, top + i);
	cprintf("ESC to Abort");

	/* Shadow */
	if(uifc.bclr==BLUE) {
		gettext(left + TRANSFER_WIN_WIDTH
				, top+1
				, left + TRANSFER_WIN_WIDTH + 1
				, top + (TRANSFER_WIN_HEIGHT - 1)
				, shadow);
		for(i=1;i<sizeof(shadow);i+=2)
			shadow[i]=DARKGRAY;
		puttext(left + TRANSFER_WIN_WIDTH
				, top+1
				, left + TRANSFER_WIN_WIDTH + 1
				, top + (TRANSFER_WIN_HEIGHT - 1)
				, shadow);
		gettext(left + 2
				, top + TRANSFER_WIN_HEIGHT
				, left + TRANSFER_WIN_WIDTH + 1
				, top + TRANSFER_WIN_HEIGHT
				, shadow);
		for(i=1;i<sizeof(shadow);i+=2)
			shadow[i]=DARKGRAY;
		puttext(left + 2
				, top + TRANSFER_WIN_HEIGHT
				, left + TRANSFER_WIN_WIDTH + 1
				, top + TRANSFER_WIN_HEIGHT
				, shadow);
	}

	window(left+2, top + 7, left + TRANSFER_WIN_WIDTH - 3, top + TRANSFER_WIN_HEIGHT - 2);
	hold_update = FALSE;
	gotoxy(1,1);
	hold_update = old_hold;
	gettextinfo(&log_ti);
	_setcursortype(_NOCURSOR);
}

void erase_transfer_window(void) {
	puttext(
		  ((trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2)
		, ((trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2)
		, ((trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2) + TRANSFER_WIN_WIDTH + 1
		, ((trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2) + TRANSFER_WIN_HEIGHT
		, winbuf);
	window(trans_ti.winleft, trans_ti.wintop, trans_ti.winright, trans_ti.winbottom);
	gotoxy(trans_ti.curx, trans_ti.cury);
	textattr(trans_ti.attribute);
	_setcursortype(_NORMALCURSOR);
}

void ascii_upload(FILE *fp);
void raw_upload(FILE *fp);
#define XMODEM_128B		(1<<10)	/* Use 128 byte block size (ick!) */
void zmodem_upload(struct bbslist *bbs, FILE *fp, char *path);
void xmodem_upload(struct bbslist *bbs, FILE *fp, char *path, long mode, int lastch);
void xmodem_download(struct bbslist *bbs, long mode, char *path);
void zmodem_download(struct bbslist *bbs);

void begin_upload(struct bbslist *bbs, BOOL autozm, int lastch)
{
	char	str[MAX_PATH*2+1];
	char	path[MAX_PATH+1];
	int		result;
	int i;
	FILE*	fp;
	struct file_pick fpick;
	char	*opts[6]={
			 "ZMODEM"
			,"YMODEM"
			,"XMODEM"
			,"ASCII"
			,"Raw"
			,""
		};
	struct	text_info txtinfo;
	char	*buf;

    gettextinfo(&txtinfo);
	buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);

	if(safe_mode)
		return;

	init_uifc(FALSE, FALSE);
	result=filepick(&uifc, "Upload", &fpick, bbs->uldir, NULL, UIFC_FP_ALLOWENTRY);
	
	if(result==-1 || fpick.files<1) {
		filepick_free(&fpick);
		uifcbail();
		puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
		gotoxy(txtinfo.curx, txtinfo.cury);
		setup_mouse_events();
		return;
	}
	SAFECOPY(path,fpick.selected[0]);
	filepick_free(&fpick);
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);

	if((fp=fopen(path,"rb"))==NULL) {
		SAFEPRINTF2(str,"Error %d opening %s for read",errno,path);
		uifcmsg("ERROR",str);
		uifcbail();
		setup_mouse_events();
		puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
		gotoxy(txtinfo.curx, txtinfo.cury);
		return;
	}
	setvbuf(fp,NULL,_IOFBF,0x10000);

	if(autozm) 
		zmodem_upload(bbs, fp, path);
	else {
		i=0;
		uifc.helpbuf="Select Protocol";
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Protocol",opts)) {
			case 0:
				zmodem_upload(bbs, fp, path);
				break;
			case 1:
				xmodem_upload(bbs, fp, path, YMODEM|SEND, lastch);
				break;
			case 2:
				xmodem_upload(bbs, fp, path, XMODEM|SEND, lastch);
				break;
			case 3:
				ascii_upload(fp);
				break;
			case 4:
				raw_upload(fp);
				break;
		}
	}
	uifcbail();
	setup_mouse_events();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	gotoxy(txtinfo.curx, txtinfo.cury);
}

void begin_download(struct bbslist *bbs)
{
	char	path[MAX_PATH+1];
	int i;
	char	*opts[5]={
			 "ZMODEM"
			,"YMODEM-g"
			,"YMODEM"
			,"XMODEM"
			,""
		};
	struct	text_info txtinfo;
	char	*buf;
	int old_hold=hold_update;

	if(safe_mode)
		return;

    gettextinfo(&txtinfo);
	buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);

	init_uifc(FALSE, FALSE);

	i=0;
	uifc.helpbuf="Select Protocol";
	hold_update=FALSE;
	switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Protocol",opts)) {
		case 0:
			zmodem_download(bbs);
			break;
		case 1:
			xmodem_download(bbs, YMODEM|CRC|GMODE|RECV, NULL);
			break;
		case 2:
			xmodem_download(bbs, YMODEM|CRC|RECV, NULL);
			break;
		case 3:
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Filename",path,sizeof(path),0)!=-1)
				xmodem_download(bbs, XMODEM|CRC|RECV,path);
			break;
	}
	hold_update=old_hold;
	uifcbail();
	setup_mouse_events();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	gotoxy(txtinfo.curx, txtinfo.cury);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static BOOL is_connected(void* unused)
{
	return(conn_connected());
}

#ifdef GUTS_BUILTIN
static int guts_lputs(void* cbdata, int level, const char* str)
{
	struct GUTS_info *gi=cbdata;
	/* ToDo: Do something usefull here. */
	/* fprintf(stderr,"%s\n",str); */
	return(0);
}

void guts_zmodem_progress(void* cbdata, ulong current_pos)
{
	struct GUTS_info *gi=cbdata;
	/* ToDo: Do something usefull here. */
	return;
}

static int guts_send_byte(void* cbdata, uchar ch, unsigned timeout)
{
	int	i;
	struct GUTS_info *gi=cbdata;

	if(!socket_check(gi->oob_socket, NULL, &i, timeout*1000))
		return(-1);

	if(!i)
		return(-1);

	if(send(gi->oob_socket,&ch,1,0)==-1)
		return(-1);

	return(0);
}

static int guts_recv_byte(void* cbdata, unsigned timeout)
{
	BOOL	data_waiting;
	BYTE	ch;
	struct GUTS_info *gi=cbdata;

	if(!socket_check(gi->oob_socket, &data_waiting, NULL, timeout*1000))
		return(-1);

	if(!data_waiting)
		return(-1);

	if(recv(gi->oob_socket,&ch,1,0)!=1)
		return(-1);

	return(ch);
}

static BOOL guts_is_connected(void* cbdata)
{
	struct GUTS_info *gi=cbdata;
	return socket_check(gi->oob_socket,NULL,NULL,0);
}

BOOL guts_data_waiting(void* cbdata, unsigned timeout)
{
	BOOL rd;
	struct GUTS_info *gi=cbdata;

	if(!socket_check(gi->oob_socket,&rd,NULL,timeout*1000))
		return(FALSE);
	return(rd);
}

void guts_background_download(void *cbdata)
{
	struct GUTS_info gi=*(struct GUTS_info *)cbdata;

	zmodem_t	zm;
	ulong		bytes_received;

	zmodem_mode=ZMODEM_MODE_RECV;

	zmodem_init(&zm
		,&gi
		,guts_lputs, guts_zmodem_progress
		,guts_send_byte,guts_recv_byte,guts_is_connected
		,NULL /* is_cancelled */
		,guts_data_waiting);

	/* ToDo: This would be a good time to detach or something. */
	zmodem_recv_files(&zm,gi.files[0],&bytes_received);

	oob_close(&gi);
}

void guts_background_upload(void *cbdata)
{
	struct GUTS_info gi=*(struct GUTS_info *)cbdata;

	zmodem_t	zm;
	ulong	fsize;
	FILE*	fp;

	if((fp=fopen(gi.files[0],"rb"))==NULL) {
		fprintf(stderr,"Error %d opening %s for read",errno,gi.files[0]);
		return;
	}

	setvbuf(fp,NULL,_IOFBF,0x10000);


	zmodem_mode=ZMODEM_MODE_SEND;

	zmodem_init(&zm
		,&gi
		,guts_lputs, guts_zmodem_progress
		,guts_send_byte,guts_recv_byte,guts_is_connected
		,NULL /* is_cancelled */
		,guts_data_waiting);

	zm.current_file_num = zm.total_files = 1;	/* ToDo: support multi-file/batch uploads */

	fsize=filelength(fileno(fp));

	if(zmodem_send_file(&zm, gi.files[0], fp
		,/* ZRQINIT? */TRUE, /* start_time */NULL, /* sent_bytes */ NULL))
		zmodem_get_zfin(&zm);

	fclose(fp);

	oob_close(&gi);
}

void guts_transfer(struct bbslist *bbs)
{
	struct GUTS_info gi;

	if(safe_mode)
		return;
	setup_defaults(&gi);
	gi.socket=conn_socket;
	gi.telnet=bbs->conn_type==CONN_TYPE_TELNET;
	gi.server=FALSE;
	gi.use_daemon=FALSE;
	gi.orig=FALSE;

	if(negotiation(&gi)) {
		oob_close(&gi);
		return;
	}

	/* Authentication Phase */
	if(!gi.inband) {
		if(authenticate(&gi)) {
			oob_close(&gi);
			return;
		}
	}

	if(gi.inband) {
		if(gi.direction==UPLOAD)
			begin_upload(bbs, TRUE);
		else
			zmodem_download(bbs);
		oob_close(&gi);
	}
	else {
		if(gi.direction==UPLOAD) {
			int		result;
			struct file_pick fpick;

			init_uifc(FALSE, FALSE);
			result=filepick(&uifc, "Upload", &fpick, bbs->uldir, NULL, UIFC_FP_ALLOWENTRY);

			if(result==-1 || fpick.files<1) {
				filepick_free(&fpick);
				uifcbail();
				setup_mouse_events();
				return;
			}
			strListPush(&gi.files, fpick.selected[0]);
			filepick_free(&fpick);

			uifcbail();
			setup_mouse_events();

			_beginthread(guts_background_upload, 0, &gi);
		}
		else {
			strListPush(&gi.files, bbs->dldir);
			_beginthread(guts_background_download, 0, &gi);
		}
	}

	return;
}
#endif

void raw_upload(FILE *fp)
{
	char	buf[1024];
	int		r;
	int		inch;
	char	ch[2];

	ch[1]=0;
	for(;;) {
		r=fread(buf, 1, sizeof(buf), fp);
		if(r)
			conn_send(buf, r,0);
		/* Note, during RAW uploads, do NOT send ANSI responses and don't
		 * allow speed changes. */
		while((inch=recv_byte(NULL, 0))>=0) {
			ch[0]=inch;
			cterm_write(ch, 1, NULL, 0, NULL);
		}
		if(r==0)
			break;
	}
	fclose(fp);
}

void ascii_upload(FILE *fp)
{
	char linebuf[1024+2];	/* One extra for terminator, one extra for added CR */
	char *p;
	char ch[2];
	int  inch;
	BOOL lastwascr=FALSE;

	ch[1]=0;
	while(!feof(fp)) {
		if(fgets(linebuf, 1025, fp)!=NULL) {
			if((p=strrchr(linebuf,'\n'))!=NULL) {
				if((p==linebuf && !lastwascr) || (p>linebuf && *(p-1)!='\n')) {
					*p='\r';
					p++;
					*p='\n';
					p++;
					*p=0;
				}
			}
			lastwascr=FALSE;
			p=strchr(p,0);
			if(p!=NULL && p>linebuf) {
				if(*(p-1)=='\r')
					lastwascr=TRUE;
			}
			conn_send(linebuf,strlen(linebuf),0);
		}
		/* Note, during ASCII uploads, do NOT send ANSI responses and don't
		 * allow speed changes. */
		while((inch=recv_byte(NULL, 0))>=0) {
			ch[0]=inch;
			cterm_write(ch, 1, NULL, 0, NULL);
		}
	}
	fclose(fp);
}

void zmodem_upload(struct bbslist *bbs, FILE *fp, char *path)
{
	zmodem_t	zm;
	ulong	fsize;
	struct zmodem_cbdata cbdata;

	draw_transfer_window("ZMODEM Upload");

	zmodem_mode=ZMODEM_MODE_SEND;

	cbdata.zm=&zm;
	cbdata.bbs=bbs;
	conn_binary_mode_on();
	zmodem_init(&zm
		,/* cbdata */&cbdata
		,lputs, zmodem_progress
		,send_byte,recv_byte
		,is_connected
		,zmodem_check_abort
		,data_waiting);

	zm.current_file_num = zm.total_files = 1;	/* ToDo: support multi-file/batch uploads */
	
	fsize=filelength(fileno(fp));

	lprintf(LOG_INFO,"Sending %s (%lu KB) via ZMODEM"
		,path,fsize/1024);

	if(zmodem_send_file(&zm, path, fp
		,/* ZRQINIT? */TRUE, /* start_time */NULL, /* sent_bytes */ NULL))
		zmodem_get_zfin(&zm);

	fclose(fp);

	conn_binary_mode_off();
	lprintf(LOG_NOTICE,"Hit any key to continue...");
	if(log_fp!=NULL)
		fflush(log_fp);
	getch();

	erase_transfer_window();
}

BOOL zmodem_duplicate_callback(void *cbdata, void *zm_void)
{
	struct	text_info txtinfo;
	char	*buf;
	BOOL	ret=FALSE;
	int		i;
	char 	*opts[4]={
					 "Overwrite"
					,"Choose New Name"
					,"Cancel Download"
					,NULL
				  };
	struct zmodem_cbdata *cb=(struct zmodem_cbdata *)cbdata;
	zmodem_t	*zm=(zmodem_t *)zm_void;
	char		fpath[MAX_PATH+1];
	BOOL		loop=TRUE;
	int			old_hold=hold_update;

    gettextinfo(&txtinfo);
	buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	window(1, 1, txtinfo.screenwidth, txtinfo.screenheight);
	init_uifc(FALSE, FALSE);
	hold_update=FALSE;

	while(loop) {
		loop=FALSE;
		i=0;
		uifc.helpbuf="Duplicate file... choose action\n";
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Duplicate File Name",opts)) {
			case 0:	/* Overwrite */
				sprintf(fpath,"%s/%s",cb->bbs->dldir,zm->current_file_name);
				unlink(fpath);
				ret=TRUE;
				break;
			case 1:	/* Choose new name */
				uifc.changes=0;
				uifc.helpbuf="Duplicate Filename... enter new name";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"New Filename: ",zm->current_file_name,sizeof(zm->current_file_name)-1,K_EDIT)==-1) {
					loop=TRUE;
				}
				else {
					if(uifc.changes)
						ret=TRUE;
					else
						loop=TRUE;
				}
				break;
		}
	}

	uifcbail();
	setup_mouse_events();
	window(txtinfo.winleft, txtinfo.wintop, txtinfo.winright, txtinfo.winbottom);
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	gotoxy(txtinfo.curx, txtinfo.cury);
	hold_update=old_hold;
	return(ret);
}

void zmodem_download(struct bbslist *bbs)
{
	zmodem_t	zm;
	int			files_received;
	uint32_t	bytes_received;
	struct zmodem_cbdata cbdata;

	if(safe_mode)
		return;
	draw_transfer_window("ZMODEM Download");

	zmodem_mode=ZMODEM_MODE_RECV;

	conn_binary_mode_on();
	cbdata.zm=&zm;
	cbdata.bbs=bbs;
	zmodem_init(&zm
		,/* cbdata */&cbdata
		,lputs, zmodem_progress
		,send_byte,recv_byte
		,is_connected
		,zmodem_check_abort
		,data_waiting);

	zm.duplicate_filename=zmodem_duplicate_callback;

	files_received=zmodem_recv_files(&zm,bbs->dldir,&bytes_received);

	if(files_received>1)
		lprintf(LOG_INFO,"Received %u files (%lu bytes) successfully", files_received, bytes_received);

	conn_binary_mode_off();
	lprintf(LOG_NOTICE,"Hit any key to continue...");
	if(log_fp!=NULL)
		fflush(log_fp);
	getch();

	erase_transfer_window();
}
/* End of Zmodem Stuff */

/* X/Y-MODEM stuff */

uchar	block[1024];					/* Block buffer 					*/
ulong	block_num;						/* Block number 					*/

static BOOL xmodem_check_abort(void* vp)
{
	xmodem_t* xm = (xmodem_t*)vp;
	if(xm!=NULL && kbhit()) {
		switch(getch()) {
			case ESC:
			case CTRL_C:
			case CTRL_X:
				xm->cancelled=TRUE;
				break;
		}
	}
	return(xm->cancelled);
}
/****************************************************************************/
/* Returns the number of blocks required to send len bytes					*/
/****************************************************************************/
unsigned num_blocks(unsigned curr_block, ulong offset, ulong len, unsigned block_size)
{
	ulong blocks;

	len-=offset;
	blocks=len/block_size;
	if(len%block_size)
		blocks++;
	return(curr_block+blocks);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
void xmodem_progress(void* cbdata, unsigned block_num, ulong offset, ulong fsize, time_t start)
{
	unsigned	total_blocks;
	unsigned	cps;
	time_t		l;
	time_t		t;
	time_t		now;
	static time_t last_progress;
	int			old_hold=hold_update;
	xmodem_t*	xm=(xmodem_t*)cbdata;

	xmodem_check_abort(cbdata);

	now=time(NULL);
	if(now-last_progress>0 || offset >= fsize) {
		hold_update = TRUE;
		window(((trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2)+2
				, ((trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2)+1
				, ((trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2) + TRANSFER_WIN_WIDTH - 2
				, ((trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2)+5);
		gotoxy(1,1);
		textattr(LIGHTCYAN | (BLUE<<4));
		t=now-start;
		if(t<=0)
			t=1;
		if((cps=offset/t)==0)
			cps=1;		/* cps so far */
		l=fsize/cps;	/* total transfer est time */
		l-=t;			/* now, it's est time left */
		if(l<0) l=0;
		if((*(xm->mode))&SEND) {
			total_blocks=num_blocks(block_num,offset,fsize,xm->block_size);
			cprintf("Block (%lu%s): %lu/%lu  Byte: %lu"
				,xm->block_size%1024L ? xm->block_size: xm->block_size/1024L
				,xm->block_size%1024L ? "" : "K"
				,block_num
				,total_blocks
				,offset);
			clreol();
			cputs("\r\n");
			cprintf("Time: %lu:%02lu/%lu:%02lu  %u cps"
				,t/60L
				,t%60L
				,l/60L
				,l%60L
				,cps
				,(long)(((float)offset/(float)fsize)*100.0)
				);
			clreol();
			cputs("\r\n");
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH/2-5, ""
				,(long)(((float)offset/(float)fsize)*100.0));
			l = (long)(((float)offset/(float)fsize)*60.0);
			cprintf("[%*.*s%*s]", l, l, 
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					, 60-l, "");
		} else if((*(xm->mode))&YMODEM) {
			cprintf("Block (%lu%s): %lu  Byte: %lu"
				,xm->block_size%1024L ? xm->block_size: xm->block_size/1024L
				,xm->block_size%1024L ? "" : "K"
				,block_num
				,offset);
			clreol();
			cputs("\r\n");
			cprintf("Time: %lu:%02lu/%lu:%02lu  %u cps"
				,t/60L
				,t%60L
				,l/60L
				,l%60L
				,cps);
			clreol();
			cputs("\r\n");
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH/2-5, ""
				,(long)(((float)offset/(float)fsize)*100.0));
			l = (long)(((float)offset/(float)fsize)*60.0);
			cprintf("[%*.*s%*s]", l, l, 
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					, 60-l, "");
		} else { /* XModem receive */
			cprintf("Block (%lu%s): %lu  Byte: %lu"
				,xm->block_size%1024L ? xm->block_size: xm->block_size/1024L
				,xm->block_size%1024L ? "" : "K"
				,block_num
				,offset);
			clreol();
			cputs("\r\n");
			cprintf("Time: %lu:%02lu  %u cps"
				,t/60L
				,t%60L
				,cps);
			clreol();
		}
		last_progress=now;
		hold_update = FALSE;
		gotoxy(wherex(), wherey());
		hold_update = old_hold;
	}
}

static int recv_g(void *cbdata, unsigned timeout)
{
	xmodem_t	*xm=(xmodem_t *)cbdata;
	
	xm->recv_byte=recv_byte;
	return('G');
}

static int recv_c(void *cbdata, unsigned timeout)
{
	xmodem_t	*xm=(xmodem_t *)cbdata;
	
	xm->recv_byte=recv_byte;
	return('C');
}

static int recv_nak(void *cbdata, unsigned timeout)
{
	xmodem_t	*xm=(xmodem_t *)cbdata;
	
	xm->recv_byte=recv_byte;
	return(NAK);
}

void xmodem_upload(struct bbslist *bbs, FILE *fp, char *path, long mode, int lastch)
{
	xmodem_t	xm;
	ulong		fsize;

	conn_binary_mode_on();

	xmodem_init(&xm
		,/* cbdata */&xm
		,&mode
		,lputs
		,xmodem_progress
		,send_byte
		,recv_byte
		,is_connected
		,xmodem_check_abort);
	if(!data_waiting(&xm, 0)) {
		switch(lastch) {
			case 'G':
				xm.recv_byte=recv_g;
				break;
			case 'C':
				xm.recv_byte=recv_c;
				break;
			case NAK:
				xm.recv_byte=recv_nak;
				break;
		}
	}

	if(mode & XMODEM_128B)
		xm.block_size=128;

	xm.total_files = 1;	/* ToDo: support multi-file/batch uploads */

	fsize=filelength(fileno(fp));

	if(mode&XMODEM) {
		if(mode&GMODE)
			draw_transfer_window("XMODEM-g Upload");
		else
			draw_transfer_window("XMODEM Upload");
		lprintf(LOG_INFO,"Sending %s (%lu KB) via XMODEM%s"
			,path,fsize/1024,(mode&GMODE)?"-g":"");
	}
	else if(mode&YMODEM) {
		if(mode&GMODE)
			draw_transfer_window("YMODEM-g Upload");
		else
			draw_transfer_window("YMODEM Upload");
		lprintf(LOG_INFO,"Sending %s (%lu KB) via YMODEM%s"
			,path,fsize/1024,(mode&GMODE)?"-g":"");
	}
	else {
		return;
	}

	if(xmodem_send_file(&xm, path, fp
		,/* start_time */NULL, /* sent_bytes */ NULL)) {
		if(mode&YMODEM) {

			if(xmodem_get_mode(&xm)) {

				lprintf(LOG_INFO,"Sending YMODEM termination block");

				memset(block,0,128);	/* send short block for terminator */
				xmodem_put_block(&xm, block, 128 /* block_size */, 0 /* block_num */);
				if(xmodem_get_ack(&xm,/* tries: */6, /* block_num: */0) != ACK) {
					lprintf(LOG_WARNING,"Failed to receive ACK after terminating block"); 
				}
			}
		}
	}

	fclose(fp);

	conn_binary_mode_off();
	lprintf(LOG_NOTICE,"Hit any key to continue...");
	if(log_fp!=NULL)
		fflush(log_fp);
	getch();

	erase_transfer_window();
}

BOOL xmodem_duplicate(xmodem_t *xm, struct bbslist *bbs, char *path, size_t pathsize, char *fname)
{
	struct	text_info txtinfo;
	char	*buf;
	BOOL	ret=FALSE;
	int		i;
	char 	*opts[4]={
					 "Overwrite"
					,"Choose New Name"
					,"Cancel Download"
					,NULL
				  };
	char	newfname[MAX_PATH+1];
	BOOL	loop=TRUE;
	int		old_hold=hold_update;

    gettextinfo(&txtinfo);
	buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	window(1, 1, txtinfo.screenwidth, txtinfo.screenheight);

	init_uifc(FALSE, FALSE);

	hold_update=FALSE;
	while(loop) {
		loop=FALSE;
		i=0;
		uifc.helpbuf="Duplicate file... choose action\n";
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Duplicate File Name",opts)) {
			case 0:	/* Overwrite */
				unlink(path);
				ret=TRUE;
				break;
			case 1:	/* Choose new name */
				uifc.changes=0;
				uifc.helpbuf="Duplicate Filename... enter new name";
				SAFECOPY(newfname, getfname(fname));
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"New Filename: ",newfname,sizeof(newfname)-1,K_EDIT)==-1) {
					loop=TRUE;
				}
				else {
					if(uifc.changes) {
						sprintf(path,"%s/%s",bbs->dldir,newfname);
						ret=TRUE;
					}
					else
						loop=TRUE;
				}
				break;
		}
	}

	uifcbail();
	setup_mouse_events();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	window(txtinfo.winleft, txtinfo.wintop, txtinfo.winright, txtinfo.winbottom);
	gotoxy(txtinfo.curx, txtinfo.cury);
	hold_update=old_hold;
	return(ret);
}

void xmodem_download(struct bbslist *bbs, long mode, char *path)
{
	xmodem_t	xm;
	/* The better to -Wunused you with my dear! */
	char	str[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	int		i=0;
	int		fnum=0;
	uint	errors;
	uint	total_files=0;
	uint	cps;
	uint	wr;
	BOOL	success=FALSE;
	long	fmode;
	long	serial_num=-1;
	ulong	tmpftime;
	ulong	file_bytes=0,file_bytes_left=0;
	ulong	total_bytes=0;
	FILE*	fp=NULL;
	time_t	t,startfile,ftime;
	int		old_hold=hold_update;

	if(safe_mode)
		return;

	if(mode&XMODEM)
		if(mode&GMODE)
			draw_transfer_window("XMODEM-g Download");
		else
			draw_transfer_window("XMODEM Download");
	else if(mode&YMODEM) {
		if(mode&GMODE)
			draw_transfer_window("YMODEM-g Download");
		else
			draw_transfer_window("YMODEM Download");
	}
	else
		return;

	conn_binary_mode_on();
	xmodem_init(&xm
		,/* cbdata */&xm
		,&mode
		,lputs
		,xmodem_progress
		,send_byte
		,recv_byte
		,is_connected
		,xmodem_check_abort);
	while(is_connected(NULL)) {
		if(mode&XMODEM) {
			if(isfullpath(path))
				SAFECOPY(str,path);
			else
				sprintf(str,"%s/%s",bbs->dldir,path);
			file_bytes=file_bytes_left=0x7fffffff;
		}

		else {
			lprintf(LOG_INFO,"Fetching YMODEM header block");
			for(errors=0;errors<=xm.max_errors && !xm.cancelled;errors++) {
				xmodem_put_nak(&xm, /* expected_block: */ 0);
				i=xmodem_get_block(&xm, block, /* expected_block: */ 0);
				if(i==SUCCESS) {
					send_byte(NULL,ACK,10);
					break;
				}
				if(i==NOINP && (mode&GMODE)) {			/* Timeout */
					mode &= ~GMODE;
					lprintf(LOG_WARNING,"Falling back to %s", 
						(mode&CRC)?"CRC-16":"Checksum");
				}
				if(i==NOT_YMODEM) {
					lprintf(LOG_WARNING,"Falling back to XMODEM%s",(mode&GMODE)?"-g":"");
					mode &= ~(YMODEM);
					mode |= XMODEM|CRC;
					erase_transfer_window();
					hold_update=0;
					if(uifc.input(WIN_MID|WIN_SAV,0,0,"XMODEM Filename",fname,sizeof(fname),0)==-1) {
						xmodem_cancel(&xm);
						goto end;
					}
					hold_update=old_hold;
					if(mode&GMODE)
						draw_transfer_window("XMODEM Download");
					else
						draw_transfer_window("XMODEM-g Download");
					lprintf(LOG_WARNING,"Falling back to XMODEM%s",(mode&GMODE)?"-g":"");
					if(isfullpath(fname))
						SAFECOPY(str,fname);
					else
						sprintf(str,"%s/%s",bbs->dldir,fname);
					file_bytes=file_bytes_left=0x7fffffff;
					break;
				}
				if(errors+1>xm.max_errors/3 && mode&CRC && !(mode&GMODE)) {
					lprintf(LOG_NOTICE,"Falling back to 8-bit Checksum mode");
					mode&=~CRC;
				}
			}
			if(errors>xm.max_errors || xm.cancelled) {
				xmodem_cancel(&xm);
				goto end;
			}
			if(i!=NOT_YMODEM) {
				if(!block[0]) {
					lprintf(LOG_INFO,"Received YMODEM termination block");
					goto end; 
				}
				file_bytes=ftime=total_files=total_bytes=0;
				i=sscanf(block+strlen(block)+1,"%ld %lo %lo %lo %d %ld"
					,&file_bytes			/* file size (decimal) */
					,&tmpftime 				/* file time (octal unix format) */
					,&fmode 				/* file mode (not used) */
					,&serial_num			/* program serial number */
					,&total_files			/* remaining files to be sent */
					,&total_bytes			/* remaining bytes to be sent */
					);
				ftime=tmpftime;
				lprintf(LOG_DEBUG,"YMODEM header (%u fields): %s", i, block+strlen(block)+1);
				SAFECOPY(fname,block);

				if(!file_bytes)
					file_bytes=0x7fffffff;
				file_bytes_left=file_bytes;
				if(!total_files)
					total_files=1;
				if(total_bytes<file_bytes)
					total_bytes=file_bytes;

				lprintf(LOG_DEBUG,"Incoming filename: %.64s ",getfname(fname));

				sprintf(str,"%s/%s",bbs->dldir,getfname(fname));
				lprintf(LOG_INFO,"File size: %lu bytes", file_bytes);
				if(total_files>1)
					lprintf(LOG_INFO,"Remaining: %lu bytes in %u files", total_bytes, total_files);
			}
		}

		lprintf(LOG_DEBUG,"Receiving: %.64s ",str);

		fnum++;

		while(fexistcase(str) && !(mode&OVERWRITE)) {
			lprintf(LOG_WARNING,"%s already exists",str);
			xmodem_duplicate(&xm, bbs, str, sizeof(str), getfname(fname));
			xmodem_cancel(&xm);
			goto end; 
		}
		if((fp=fopen(str,"wb"))==NULL) {
			lprintf(LOG_ERR,"Error %d creating %s",errno,str);
			xmodem_cancel(&xm);
			goto end; 
		}

		if(mode&XMODEM)
			lprintf(LOG_INFO,"Receiving %s via %s %s"
				,str
				,mode&GMODE ? "XMODEM-g" : "XMODEM"
				,mode&CRC ? "CRC-16" : "Checksum");
		else
			lprintf(LOG_INFO,"Receiving %s (%lu KB) via %s %s"
				,str
				,file_bytes/1024
				,mode&GMODE ? "YMODEM-g" : "YMODEM"
				,mode&CRC ? "CRC-16" : "Checksum");

		startfile=time(NULL);
		success=FALSE;

		errors=0;
		block_num=1;
		if(i!=NOT_YMODEM)
			xmodem_put_nak(&xm, block_num);
		while(is_connected(NULL)) {
			xmodem_progress(&xm,block_num,ftell(fp),file_bytes,startfile);
			if(xm.is_cancelled(&xm)) {
				lprintf(LOG_WARNING,"Cancelled locally");
				xmodem_cancel(&xm);
				goto end; 
			}
			if(i==NOT_YMODEM)
				i=SUCCESS;
			else
				i=xmodem_get_block(&xm, block, block_num);

			if(i!=SUCCESS) {
				if(i==EOT)	{		/* end of transfer */
					success=TRUE;
					xmodem_put_ack(&xm);
					break;
				}
				if(i==CAN) {		/* Cancel */
					xm.cancelled=TRUE;
					break;
				}
				if(mode&GMODE) {
					lprintf(LOG_ERR,"Too many errors (%u)",++errors);
					goto end; 
				}

				if(++errors>xm.max_errors) {
					lprintf(LOG_ERR,"Too many errors (%u)",errors);
					xmodem_cancel(&xm);
					break;
				}
				if(i!=NOT_XMODEM
					&& block_num==1 && errors>(xm.max_errors/3) && mode&CRC && !(mode&GMODE)) {
					lprintf(LOG_NOTICE,"Falling back to 8-bit Checksum mode (error=%d)", i);
					mode&=~CRC;
				}
				xmodem_put_nak(&xm, block_num);
				continue;
			}
			if(!(mode&GMODE))
				send_byte(NULL,ACK,10);
			if(file_bytes_left<=0L)  { /* No more bytes to receive */
				lprintf(LOG_WARNING,"Sender attempted to send more bytes than were specified in header");
				break; 
			}
			wr=xm.block_size;
			if(wr>file_bytes_left)
				wr=file_bytes_left;
			if(fwrite(block,1,wr,fp)!=wr) {
				lprintf(LOG_ERR,"Error writing %u bytes to file at offset %lu"
					,wr,ftell(fp));
				xmodem_cancel(&xm);
				goto end; 
			}
			file_bytes_left-=wr; 
			block_num++;
		}

		/* Use correct file size */
		fflush(fp);

		lprintf(LOG_DEBUG,"file_bytes=%u", file_bytes);
		lprintf(LOG_DEBUG,"file_bytes_left=%u", file_bytes_left);
		lprintf(LOG_DEBUG,"filelength=%u", filelength(fileno(fp)));

		if(file_bytes < (ulong)filelength(fileno(fp))) {
			lprintf(LOG_INFO,"Truncating file to %lu bytes", file_bytes);
			chsize(fileno(fp),file_bytes);
		} else
			file_bytes = filelength(fileno(fp));
		fclose(fp);
		
		t=time(NULL)-startfile;
		if(!t) t=1;
		if(success)
			lprintf(LOG_INFO,"Successful - Time: %lu:%02lu  CPS: %lu"
				,t/60,t%60,file_bytes/t);
		else
			lprintf(LOG_ERR,"File Transfer %s", xm.cancelled ? "Cancelled":"Failure");

		if(!(mode&XMODEM) && ftime)
			setfdate(str,ftime); 

		if(!success && file_bytes==0)	/* remove 0-byte files */
			remove(str);

		if(mode&XMODEM)	/* maximum of one file */
			break;
		if((cps=file_bytes/t)==0)
			cps=1;
		total_files--;
		total_bytes-=file_bytes;
		if(total_files>1 && total_bytes)
			lprintf(LOG_INFO,"Remaining - Time: %lu:%02lu  Files: %u  KBytes: %lu"
				,(total_bytes/cps)/60
				,(total_bytes/cps)%60
				,total_files
				,total_bytes/1024
				);
	}

end:
	if(fp)
		fclose(fp);
	conn_binary_mode_off();
	lprintf(LOG_NOTICE,"Hit any key to continue...");
	if(log_fp!=NULL)
		fflush(log_fp);
	getch();

	erase_transfer_window();
}

/* End of X/Y-MODEM stuff */

void music_control(struct bbslist *bbs)
{
	char *buf;
	struct	text_info txtinfo;
	int i;
	char *opts[4]={
			 "ESC[| ANSI Music only"
			,"ESC[N (BANSI-Style) and ESC[| ANSI Music"
			,"ANSI Music Enabled"
	};

   	gettextinfo(&txtinfo);
	buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	init_uifc(FALSE, FALSE);

	i=cterm.music_enable;
	uifc.helpbuf="`ANSI Music Setup`\n\n"
				"~ ANSI Music Disabled ~ Completely disables ANSI music\n"
				"                      Enables Delete Line\n"
				"~ ESC[N ~               Enables BANSI-Style ANSI music\n"
				"                      Enables Delete Line\n"
				"~ ANSI Music Enabled ~  Enables both ESC[M and ESC[N ANSI music.\n"
				"                      Delete Line is disabled.\n"
				"\n"
				"So-Called ANSI Music has a long and troubled history.  Although the\n"
				"original ANSI standard has well defined ways to provide private\n"
				"extensions to the spec, none of these methods were used.  Instead,\n"
				"so-called ANSI music replaced the Delete Line ANSI sequence.  Many\n"
				"full-screen editors use DL, and to this day, some programs (Such as\n"
				"BitchX) require it to run.\n\n"
				"To deal with this, BananaCom decided to use what *they* thought was an\n"
				"unspecified escape code, ESC[N, for ANSI music.  Unfortunately, this is\n"
				"broken also.  Although rarely implemented in BBS clients, ESC[N is\n"
				"the erase field sequence.\n\n"
				"SyncTERM has now defined a third ANSI music sequence which *IS* legal\n"
				"according to the ANSI spec.  Specifically ESC[|.";
	if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"ANSI Music Setup",opts)!=-1)
		cterm.music_enable=i;
	uifcbail();
	setup_mouse_events();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	window(txtinfo.winleft,txtinfo.wintop,txtinfo.winright,txtinfo.winbottom);
	textattr(txtinfo.attribute);
	gotoxy(txtinfo.curx,txtinfo.cury);
}

void font_control(struct bbslist *bbs)
{
	char *buf;
	struct	text_info txtinfo;
	int i,j,k;

	if(safe_mode)
		return;
   	gettextinfo(&txtinfo);
	buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	init_uifc(FALSE, FALSE);

	switch(cio_api.mode) {
		case CIOLIB_MODE_CONIO:
		case CIOLIB_MODE_CONIO_FULLSCREEN:
		case CIOLIB_MODE_CURSES:
		case CIOLIB_MODE_CURSES_IBM:
		case CIOLIB_MODE_ANSI:
			uifcmsg("Not supported in this video output mode."
				,"Font cannot be changed in the current video output mode");
			break;
		default:
			i=j=getfont();
			uifc.helpbuf="`Font Setup`\n\n"
						"Change the current font.  Must support the current video mode.";
			k=uifc.list(WIN_MID|WIN_SAV|WIN_INS,0,0,0,&i,&j,"Font Setup",font_names);
			if(k!=-1) {
				if(k & MSK_INS) {
					struct file_pick fpick;
					j=filepick(&uifc, "Load Font From File", &fpick, ".", NULL, 0);

					if(j!=-1 && fpick.files>=1) {
						loadfont(fpick.selected[0]);
						uifc_old_font=getfont();
					}
					filepick_free(&fpick);
				}
				else {
					setfont(i,FALSE,1);
					uifc_old_font=getfont();
				}
			}
		break;
	}
	uifcbail();
	setup_mouse_events();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	window(txtinfo.winleft,txtinfo.wintop,txtinfo.winright,txtinfo.winbottom);
	textattr(txtinfo.attribute);
	gotoxy(txtinfo.curx,txtinfo.cury);
}

void capture_control(struct bbslist *bbs)
{
	char *buf;
	struct	text_info txtinfo;
	int i,j;

	if(safe_mode)
		return;
   	gettextinfo(&txtinfo);
	buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	init_uifc(FALSE, FALSE);

	if(!cterm.log) {
		struct file_pick fpick;
		char *opts[3]={
						 "ASCII"
						,"Raw"
						,""
					  };

		i=0;
		uifc.helpbuf="`Capture Type`\n\n"
					"~ ASCII ~ Strips out ANSI sequences\n"
					"~ Raw ~   Leaves ANSI sequences in\n\n"
					"Raw is usefull for stealing ANSI screens from other systems.\n"
					"Don't do that though.  :-)";
		if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Capture Type",opts)!=-1) {
			j=filepick(&uifc, "Capture File", &fpick, bbs->dldir, NULL, UIFC_FP_ALLOWENTRY);

			if(j!=-1 && fpick.files>=1)
				cterm_openlog(fpick.selected[0], i?CTERM_LOG_RAW:CTERM_LOG_ASCII);
			filepick_free(&fpick);
		}
	}
	else {
		if(cterm.log & CTERM_LOG_PAUSED) {
			char *opts[3]={
							 "Unpause"
							,"Close"
						  };
			i=0;
			uifc.helpbuf="`Capture Control`\n\n"
						"~ Unpause ~ Continues logging\n"
						"~ Close ~   Closes the log\n\n";
			if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Capture Control",opts)!=-1) {
				switch(i) {
					case 0:
						cterm.log=cterm.log & CTERM_LOG_MASK;
						break;
					case 1:
						cterm_closelog();
						break;
				}
			}
		}
		else {
			char *opts[3]={
							 "Pause"
							,"Close"
						  };
			i=0;
			uifc.helpbuf="`Capture Control`\n\n"
						"~ Pause ~ Suspends logging\n"
						"~ Close ~ Closes the log\n\n";
			if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Capture Control",opts)!=-1) {
				switch(i) {
					case 0:
						cterm.log=cterm.log |= CTERM_LOG_PAUSED;
						break;
					case 1:
						cterm_closelog();
						break;
				}
			}
		}
	}
	uifcbail();
	setup_mouse_events();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	window(txtinfo.winleft,txtinfo.wintop,txtinfo.winright,txtinfo.winbottom);
	textattr(txtinfo.attribute);
	gotoxy(txtinfo.curx,txtinfo.cury);
}

#ifdef WITH_WXWIDGETS
void html_send(const char *buf)
{
	conn_send((char *)buf,strlen(buf),0);
}

static char cachedir[MAX_PATH+6];
static int cachedirlen=0;

void html_cleanup(void)
{
	if(cachedirlen)
		delfiles(cachedir+5,ALLFILES);
}

int html_urlredirect(const char *uri, char *buf, size_t bufsize, char *uribuf, size_t uribufsize)
{
	char *in;
	size_t out;

	if(!cachedirlen) {
		strcpy(cachedir,"file:");
		get_syncterm_filename(cachedir+5, sizeof(cachedir)-5, SYNCTERM_PATH_CACHE, FALSE);
		cachedirlen=strlen(cachedir);
		html_cleanup();
	}

	if(!memcmp(uri, cachedir, cachedirlen)) {
		/* Reading from the cache... no redirect */
		return(URL_ACTION_ISGOOD);
	}

	strncpy(buf, cachedir, bufsize);
	buf[bufsize-1]=0;
	backslash(buf);
	/* Append mangledname */
	in=(char *)uri;
	out=strlen(buf);
	while(*in && out < bufsize-1) {
		char ch;
		ch=*(in++);
		if(ch < ' ')
			ch='^';
		if(ch > 126)
			ch='~';
		switch(ch) {
			case '*':
			case '?':
			case ':':
			case '[':
			case ']':
			case '"':
			case '<':
			case '>':
			case '|':
			case '(':
			case ')':
			case '{':
			case '}':
			case '/':
			case '\\':
				buf[out++]='_';
				break;
			default:
				buf[out++]=ch;
		}
	}
	buf[out]=0;

	/* We now have the cache filename... does it already exist? */
	if(fexist(buf+5))
		return(URL_ACTION_REDIRECT);

	/* If not, we need to fetch it... convert relative URIs */
	if(strstr(uri,"://")) {
		/* Good URI */
		strncpy(uribuf, uri, uribufsize);
		uribuf[uribufsize-1]=0;
		return(URL_ACTION_DOWNLOAD);
	}

	strcpy(uribuf, "http://");
	if(html_addr)
		strcat(uribuf, html_addr);
	if(uri[0]!='/')
		strcat(uribuf, "/");
	strcat(uribuf,uri);

	return(URL_ACTION_DOWNLOAD);
}

#endif

BOOL doterm(struct bbslist *bbs)
{
	unsigned char ch[2];
	unsigned char prn[ANSI_REPLY_BUFSIZE];
	int	key;
	int i,j;
	unsigned char *p,*p2;
	BYTE zrqinit[] = { ZDLE, ZHEX, '0', '0', 0 };	/* for Zmodem auto-downloads */
	BYTE zrinit[] = { ZDLE, ZHEX, '0', '1', 0 };	/* for Zmodem auto-uploads */
	BYTE zrqbuf[sizeof(zrqinit)];
#ifdef GUTS_BUILTIN
	BYTE gutsinit[] = { ESC, '[', '{' };	/* For GUTS auto-transfers */
	BYTE gutsbuf[sizeof(gutsinit)];
#endif
#ifdef WITH_WXWIDGETS
	BYTE htmldetect[]="\2\2?HTML?";
	BYTE htmlresponse[]="\2\2!HTML!";
	BYTE htmlstart[]="\2\2<HTML>";
	BYTE htmldet[sizeof(htmldetect)];
	int html_startx;
	int html_starty;
#endif
	int	inch;
	long double nextchar=0;
	long double lastchar=0;
	long double thischar=0;
	int	speed;
	int	oldmc;
	int	updated=FALSE;
	BOOL	sleep;
	int 	emulation=CTERM_EMULATION_ANSI_BBS;
	size_t	remain;
	struct text_info txtinfo;
#ifndef WITHOUT_OOII
	BYTE ooii_buf[256];
	BYTE ooii_init1[] = "\xdb\b \xdb\b \xdb\b[\xdb\b[\xdb\b \xdb\bM\xdb\ba\xdb\bi\xdb\bn\xdb\bt\xdb\be\xdb\bn\xdb\ba\xdb\bn\xdb\bc\xdb\be\xdb\b \xdb\bC\xdb\bo\xdb\bm\xdb\bp\xdb\bl\xdb\be\xdb\bt\xdb\be\xdb\b \xdb\b]\xdb\b]\xdb\b \b\r\n\r\n\r\n\x1b[0;0;36mDo you have the Overkill Ansiterm installed? (y/N)  \xe9 ";	/* for OOII auto-enable */
	BYTE ooii_init2[] = "\xdb\b \xdb\b \xdb\b[\xdb\b[\xdb\b \xdb\bM\xdb\ba\xdb\bi\xdb\bn\xdb\bt\xdb\be\xdb\bn\xdb\ba\xdb\bn\xdb\bc\xdb\be\xdb\b \xdb\bC\xdb\bo\xdb\bm\xdb\bp\xdb\bl\xdb\be\xdb\bt\xdb\be\xdb\b \xdb\b]\xdb\b]\xdb\b \b\r\n\r\n\x1b[0m\x1b[2J\r\n\r\n\x1b[0;1;30mHX Force retinal scan in progress ... \x1b[0;0;30m";	/* for OOII auto-enable */
#endif
	int ooii_mode=0;

	gettextinfo(&txtinfo);
	if(bbs->conn_type == CONN_TYPE_SERIAL)
		speed = 0;
	else
		speed = bbs->bpsrate;
	log_level = bbs->xfer_loglevel;
	conn_api.log_level = bbs->telnet_loglevel;
	setup_mouse_events();
	p=(unsigned char *)realloc(scrollback_buf, term.width*2*settings.backlines);
	if(p != NULL) {
		scrollback_buf=p;
		memset(scrollback_buf,0,term.width*2*settings.backlines);
	}
	else
		FREE_AND_NULL(scrollback_buf);
	scrollback_lines=0;
	scrollback_mode=txtinfo.currmode;
	switch(bbs->screen_mode) {
		case SCREEN_MODE_C64:
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			emulation = CTERM_EMULATION_PETASCII;
			break;
		case SCREEN_MODE_ATARI:
		case SCREEN_MODE_ATARI_XEP80:
			emulation = CTERM_EMULATION_ATASCII;
			break;
	}
	cterm_init(term.height,term.width,term.x-1,term.y-1,settings.backlines,scrollback_buf, emulation);
	scrollback_cols=term.width;
	cterm.music_enable=bbs->music;
	ch[1]=0;
	zrqbuf[0]=0;
#ifndef WITHOUT_OOII
	ooii_buf[0]=0;
#endif
#ifdef GUTS_BUILTIN
	gutsbuf[0]=0;
#endif
#ifdef WITH_WXWIDGETS
	htmldet[0]=0;
#endif

	/* Main input loop */
	oldmc=hold_update;
	showmouse();
	for(;;) {
		hold_update=TRUE;
		sleep=TRUE;
		if(!term.nostatus)
			update_status(bbs, (bbs->conn_type == CONN_TYPE_SERIAL)?bbs->bpsrate:speed, ooii_mode);
		for(remain=conn_data_waiting() /* Hack for connection check */ + (!conn_connected()); remain; remain--) {
			if(speed)
				thischar=xp_timer();

			if((!speed) || thischar < lastchar /* Wrapped */ || thischar >= nextchar) {
				/* Get remote input */
				inch=recv_byte(NULL, 0);

				switch(inch) {
					case -1:
						if(!conn_connected()) {
							hold_update=oldmc;
#ifdef WITH_WXWIDGETS
							if(html_mode != HTML_MODE_HIDDEN) {
								hide_html();
								html_cleanup();
								html_mode=HTML_MODE_HIDDEN;
							}
#endif
							uifcmsg("Disconnected","`Disconnected`\n\nRemote host dropped connection");
							cterm_clearscreen(cterm.attr);	/* Clear screen into scrollback */
							scrollback_lines=cterm.backpos;
							cterm_end();
							conn_close();
							hidemouse();
							return(FALSE);
						}
						break;
					default:
						if(speed) {
							lastchar = xp_timer();
							nextchar = lastchar + 1/(long double)(speed/10);
						}

#ifdef GUTS_BUILTIN
						j=strlen(gutsbuf);
						if(inch == gutsinit[j]) {
							gutsbuf[j]=inch;
							gutsbuf[++j]=0;
							if(j==sizeof(gutsinit)) { /* Have full sequence */
								guts_transfer(bbs);
								remain=1;
							}
						}
						else
							gutsbuf[0]=0;
#endif
#ifdef WITH_WXWIDGETS
						if(html_mode==HTML_MODE_READING) {
							if(inch==2) {
								html_startx=wherex();
								html_starty=wherey();
								html_commit();
								raise_html();
								html_mode=HTML_MODE_RAISED;
							}
							else {
								add_html_char(inch);
							}
							continue;
						}

						j=strlen(htmldet);
						if(inch == htmldetect[j] || toupper(inch)==htmlstart[j]) {
							htmldet[j]=inch;
							htmldet[++j]=0;
							if(j==sizeof(htmldetect)-1) {
								if(!strcmp(htmldet, htmldetect)) {
									if(html_supported==HTML_SUPPORT_UNKNOWN) {
										int width,height,xpos,ypos;
										html_addr=bbs->addr;

										get_window_info(&width, &height, &xpos, &ypos);
										if(!run_html(width, height, xpos, ypos, html_send, html_urlredirect))
											html_supported=HTML_SUPPORTED;
										else
											html_supported=HTML_NOTSUPPORTED;
									}
									if(html_supported==HTML_SUPPORTED) {
										conn_send(htmlresponse, sizeof(htmlresponse)-1, 0);
										hide_html();
									}
								}
								else {
									show_html("");
									html_mode=HTML_MODE_READING;
								}
								htmldet[0]=0;
							}
						}
						else
							htmldet[0]=0;
#endif

						j=strlen(zrqbuf);
						if(inch == zrqinit[j] || inch == zrinit[j]) {
							zrqbuf[j]=inch;
							zrqbuf[++j]=0;
							if(j==sizeof(zrqinit)-1) {	/* Have full sequence (Assumes zrinit and zrqinit are same length */
								if(!strcmp(zrqbuf, zrqinit))
									zmodem_download(bbs);
								else
									begin_upload(bbs, TRUE, inch);
								zrqbuf[0]=0;
								remain=1;
							}
						}
						else
							zrqbuf[0]=0;
#ifndef WITHOUT_OOII
						if(ooii_mode) {
							if(ooii_buf[0]==0) {
								if(inch == 0xab) {
									ooii_buf[0]=inch;
									ooii_buf[1]=0;
									continue;
								}
							}
							else { /* Already have the start of the sequence */
								j=strlen(ooii_buf);
								if(j+1 >= sizeof(ooii_buf))
									j--;
								ooii_buf[j++]=inch;
								ooii_buf[j]=0;
								if(inch == '|') {
									if(handle_ooii_code(ooii_buf, &ooii_mode, prn, sizeof(prn))) {
										ooii_mode=0;
										xptone_close();
									}
									if(prn[0])
										conn_send(prn,strlen(prn),0);
									ooii_buf[0]=0;
								}
								continue;
							}
						}
						else {
							j=strlen(ooii_buf);
							if(inch==ooii_init1[j]) {
								ooii_buf[j++]=inch;
								ooii_buf[j]=0;
								if(ooii_init1[j]==0) {
									if(strcmp(ooii_buf, ooii_init1)==0) {
										ooii_mode=1;
										xptone_open();
									}
									ooii_buf[0]=0;
								}
							}
							else if(inch==ooii_init2[j]) {
								ooii_buf[j++]=inch;
								ooii_buf[j]=0;
								if(ooii_init2[j]==0) {
									if(strcmp(ooii_buf, ooii_init2)==0) {
										ooii_mode=2;
										xptone_open();
									}
									ooii_buf[0]=0;
								}
							}
							else
								ooii_buf[0]=0;
						}
#endif

						ch[0]=inch;
						cterm_write(ch, 1, prn, sizeof(prn), &speed);
#ifdef WITH_WXWIDGETS
						if(html_mode==HTML_MODE_RAISED) {
							if(html_startx!=wherex() || html_starty!=wherey()) {
								iconize_html();
								html_mode=HTML_MODE_ICONIZED;
							}
						}
#endif
						if(prn[0])
							conn_send(prn, strlen(prn), 0);
						updated=TRUE;
						continue;
				}
			}
			else {
				if (speed)
					sleep=FALSE;
				break;
			}
		}
		if(updated && sleep) {
			hold_update=FALSE;
			gotoxy(wherex(), wherey());
		}
		hold_update=oldmc;

		/* Get local input */
		while(kbhit()) {
			struct mouse_event mevent;

			updated=TRUE;
			gotoxy(wherex(), wherey());
			key=getch();
			if(key==0 || key==0xff) {
				key|=getch()<<8;
				if(cterm.doorway_mode && ((key & 0xff) == 0) && key != 0x2c00 /* ALT-Z */) {
					ch[0]=0;
					ch[1]=key>>8;
					conn_send(ch,2,0);
					key=0;
					continue;
				}
			}

			/* These keys are SyncTERM control keys */
			/* key is set to zero if consumed */
			switch(key) {
				case CIO_KEY_MOUSE:
					getmouse(&mevent);
					switch(mevent.event) {
						case CIOLIB_BUTTON_1_DRAG_START:
							mousedrag(scrollback_buf);
							key = 0;
							break;
						case CIOLIB_BUTTON_2_CLICK:
						case CIOLIB_BUTTON_3_CLICK:
							p=getcliptext();
							if(p!=NULL) {
								for(p2=p; *p2; p2++) {
									if(*p2=='\n') {
										/* If previous char was not \r, send a \r */
										if(p2==p || *(p2-1)!='\r')
											conn_send("\r",1,0);
									}
									else
										conn_send(p2,1,0);
								}
								free(p);
							}
							key = 0;
							break;
					}

					key = 0;
					break;
				case 0x3000:	/* ALT-B - Scrollback */
					viewscroll();
					showmouse();
					key = 0;
					break;
				case 0x2e00:	/* ALT-C - Capture */
					capture_control(bbs);
					showmouse();
					key = 0;
					break;
				case 0x2000:	/* ALT-D - Download */
					begin_download(bbs);
					showmouse();
					key = 0;
					break;
				case 0x1200:	/* ALT-E */
					{
						p=(char *)malloc(txtinfo.screenheight*txtinfo.screenwidth*2);
						if(p) {
							gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,p);
							show_bbslist(bbs->name, TRUE);
							uifcbail();
							setup_mouse_events();
							puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,p);
							free(p);
							showmouse();
							_setcursortype(_NORMALCURSOR);
						}
					}
					break;
				case 0x2100:	/* ALT-F */
					font_control(bbs);
					showmouse();
					key = 0;
					break;
				case 0x2600:	/* ALT-L */
					if(bbs->user[0]) {
						conn_send(bbs->user,strlen(bbs->user),0);
						conn_send(cterm.emulation==CTERM_EMULATION_ATASCII?"\x9b":"\r",1,0);
						SLEEP(10);
					}
					if(bbs->password[0]) {
						conn_send(bbs->password,strlen(bbs->password),0);
						conn_send(cterm.emulation==CTERM_EMULATION_ATASCII?"\x9b":"\r",1,0);
						SLEEP(10);
					}
					if(bbs->syspass[0]) {
						conn_send(bbs->syspass,strlen(bbs->syspass),0);
						conn_send(cterm.emulation==CTERM_EMULATION_ATASCII?"\x9b":"\r",1,0);
					}
					key = 0;
					break;
				case 0x3200:	/* ALT-M */
					music_control(bbs);
					showmouse();
					key = 0;
					break;
				case 0x1600:	/* ALT-U - Upload */
					begin_upload(bbs, FALSE, inch);
					showmouse();
					key = 0;
					break;
				case 17:		/* CTRL-Q */
					if(cio_api.mode!=CIOLIB_MODE_CURSES
							&& cio_api.mode!=CIOLIB_MODE_CURSES_IBM
							&& cio_api.mode!=CIOLIB_MODE_ANSI) {
						break;
					}
					/* FALLTHROUGH for curses/ansi modes */
				case 0x2d00:	/* Alt-X - Exit */
				case 0x2300:	/* Alt-H - Hangup */
					{
						char *opts[3]={
										 "Yes"
										,"No"
										,""
									  };
						char *buf;

   						gettextinfo(&txtinfo);
						buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
						gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
						i=0;
						init_uifc(FALSE, FALSE);
						uifc.helpbuf="Selecting Yes closes the connection\n";
						if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Disconnect... Are you sure?",opts)==0) {
#ifdef WITH_WXWIDGETS
							if(html_mode != HTML_MODE_HIDDEN) {
								hide_html();
								html_cleanup();
								html_mode=HTML_MODE_HIDDEN;
							}
#endif
							uifcbail();
							setup_mouse_events();
							cterm_clearscreen(cterm.attr);	/* Clear screen into scrollback */
							scrollback_lines=cterm.backpos;
							cterm_end();
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(key==0x2d00 /* Alt-X? */);
						}
						uifcbail();
						setup_mouse_events();
						puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
						window(txtinfo.winleft,txtinfo.wintop,txtinfo.winright,txtinfo.winbottom);
						textattr(txtinfo.attribute);
						gotoxy(txtinfo.curx,txtinfo.cury);
						showmouse();
					}
					key = 0;
					break;
				case 19:	/* CTRL-S */
					if(cio_api.mode!=CIOLIB_MODE_CURSES
							&& cio_api.mode!=CIOLIB_MODE_CURSES_IBM
							&& cio_api.mode!=CIOLIB_MODE_ANSI) {
						break;
					}
					/* FALLTHROUGH for curses/ansi modes */
				case 0x2c00:		/* ALT-Z */
					i=wherex();
					j=wherey();
					switch(syncmenu(bbs, &speed)) {
						case -1:
#ifdef WITH_WXWIDGETS
							if(html_mode != HTML_MODE_HIDDEN) {
								hide_html();
								html_cleanup();
								html_mode=HTML_MODE_HIDDEN;
							}
#endif
							cterm_clearscreen(cterm.attr);	/* Clear screen into scrollback */
							scrollback_lines=cterm.backpos;
							cterm_end();
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(FALSE);
						case 3:
							begin_upload(bbs, FALSE, inch);
							break;
						case 4:
							zmodem_download(bbs);
							break;
						case 7:
							capture_control(bbs);
							break;
						case 8:
							music_control(bbs);
							break;
						case 9:
							font_control(bbs);
							break;
						case 10:
							cterm.doorway_mode=!cterm.doorway_mode;
							break;

#ifdef WITHOUT_OOII
						case 11:
#else
						case 11:
							ooii_mode++;
							if(ooii_mode > MAX_OOII_MODE) {
								xptone_close();
								ooii_mode=0;
							}
							else
								xptone_open();
							break;
						case 12:
#endif
				
#ifdef WITH_WXWIDGETS
							if(html_mode != HTML_MODE_HIDDEN) {
								hide_html();
								html_cleanup();
								html_mode=HTML_MODE_HIDDEN;
							}
#endif
							cterm_clearscreen(cterm.attr);	/* Clear screen into scrollback */
							scrollback_lines=cterm.backpos;
							cterm_end();
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(TRUE);
#ifdef WITHOUT_OOII
						case 12:
#else
						case 13:
#endif
							{
								p=(char *)malloc(txtinfo.screenheight*txtinfo.screenwidth*2);
								if(p) {
									gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,p);
									show_bbslist(bbs->name, TRUE);
									puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,p);
									free(p);
								}
							}
							break;
					}
					setup_mouse_events();
					showmouse();
					gotoxy(i,j);
					key = 0;
					break;
				case 0x9800:	/* ALT-Up */
					if(bbs->conn_type != CONN_TYPE_SERIAL) {
						if(speed)
							speed=rates[get_rate_num(speed)+1];
						else
							speed=rates[0];
						key = 0;
					}
					break;
				case 0xa000:	/* ALT-Down */
					if(bbs->conn_type != CONN_TYPE_SERIAL) {
						i=get_rate_num(speed);
						if(i==0)
							speed=0;
						else
							speed=rates[i-1];
						key = 0;
					}
					break;
			}
			if(key && cterm.emulation == CTERM_EMULATION_ATASCII) {
				/* Translate keys to ATASCII */
				switch(key) {
					case '\r':
					case '\n':
						ch[0]=155;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_DOWN:
						ch[0]=29;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_DC:		/* "Delete" key */
					case '\b':				/* Backspace */
						ch[0]=126;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_RIGHT:
						ch[0]=31;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_UP:
						ch[0]=28;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_LEFT:
						ch[0]=30;
						conn_send(ch,1,0);
						break;
					case '\t':
						ch[0]=127;
						conn_send(ch,1,0);
						break;
					case 96:	/* No backtick */
						break;
					default:
						if(key<256) {
							/* ASCII Translation */
							if(key<32) {
								break;
							}
							else if(key<123) {
								ch[0]=key;
								conn_send(ch,1,0);
							}
						}
						break;
				}
			}
			else if(key && cterm.emulation == CTERM_EMULATION_PETASCII) {
				/* Translate keys to PETSCII */
				switch(key) {
					case '\r':
					case '\n':
						ch[0]=13;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_DOWN:
						ch[0]=17;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_HOME:
						ch[0]=19;
						conn_send(ch,1,0);
						break;
					case '\b':
					case CIO_KEY_DC:		/* "Delete" key */
						ch[0]=20;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_RIGHT:
						ch[0]=29;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_F(1):
						ch[0]=133;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_F(3):
						ch[0]=134;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_F(5):
						ch[0]=135;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_F(7):
						ch[0]=136;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_F(2):
						ch[0]=137;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_F(4):
						ch[0]=138;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_F(6):
						ch[0]=139;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_F(8):
						ch[0]=140;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_UP:
						ch[0]=145;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_IC:
						ch[0]=148;
						conn_send(ch,1,0);
						break;
					case CIO_KEY_LEFT:
						ch[0]=157;
						conn_send(ch,1,0);
						break;
					default:
						if(key<256) {
							/* ASCII Translation */
							if(key<32) {
								break;
							}
							else if(key<65) {
								ch[0]=key;
								conn_send(ch,1,0);
							}
							else if(key<91) {
								ch[0]=tolower(key);
								conn_send(ch,1,0);
							}
							else if(key<96) {
								ch[0]=key;
								conn_send(ch,1,0);
							}
							else if(key==96) {
								break;
							}
							else if(key<123) {
								ch[0]=toupper(key);
								conn_send(ch,1,0);
							}
						}
						break;
				}
			}
			else if(key) {
				switch(key) {
					case CIO_KEY_LEFT:
						conn_send("\033[D",3,0);
						break;
					case CIO_KEY_RIGHT:
						conn_send("\033[C",3,0);
						break;
					case CIO_KEY_UP:
						conn_send("\033[A",3,0);
						break;
					case CIO_KEY_DOWN:
						conn_send("\033[B",3,0);
						break;
					case CIO_KEY_HOME:
						conn_send("\033[H",3,0);
						break;
					case CIO_KEY_END:
#ifdef CIO_KEY_SELECT
					case CIO_KEY_SELECT:	/* Some terminfo/termcap entries use KEY_SELECT as the END key! */
#endif
						conn_send("\033[K",3,0);
						break;
					case CIO_KEY_DC:		/* "Delete" key, send ASCII 127 (DEL) */
						conn_send("\x7f",1,0);
						break;
					case CIO_KEY_NPAGE:		/* Page down */
						conn_send("\033[U",3,0);
						break;
					case CIO_KEY_PPAGE:	/* Page up */
						conn_send("\033[V",3,0);
						break;
					case CIO_KEY_F(1):
						conn_send("\033OP",3,0);
						break;
					case CIO_KEY_F(2):
						conn_send("\033OQ",3,0);
						break;
					case CIO_KEY_F(3):
						conn_send("\033OR",3,0);
						break;
					case CIO_KEY_F(4):
						conn_send("\033OS",3,0);
						break;
					case CIO_KEY_F(5):
						conn_send("\033Ot",3,0);
						break;
					case CIO_KEY_F(6):
						conn_send("\033[17~",5,0);
						break;
					case CIO_KEY_F(7):
						conn_send("\033[18~",5,0);
						break;
					case CIO_KEY_F(8):
						conn_send("\033[19~",5,0);
						break;
					case CIO_KEY_F(9):
						conn_send("\033[20~",5,0);
						break;
					case CIO_KEY_F(10):
						conn_send("\033[21~",5,0);
						break;
					case CIO_KEY_F(11):
						conn_send("\033[23~",5,0);
						break;
					case CIO_KEY_F(12):
						conn_send("\033[24~",5,0);
						break;
					case CIO_KEY_IC:
						conn_send("\033[@",3,0);
						break;
					case 17:		/* CTRL-Q */
						ch[0]=key;
						conn_send(ch,1,0);
						break;
					case 19:	/* CTRL-S */
						ch[0]=key;
						conn_send(ch,1,0);
						break;
					case '\b':
						key='\b';
						/* FALLTHROUGH to default */
					default:
						if(key<256) {
							ch[0]=key;
							conn_send(ch,1,0);
						}
				}
			}
		}
		if(sleep)
			SLEEP(1);
		else
			MAYBE_YIELD();
	}
/*
	hidemouse();
	hold_update=oldmc;
	return(FALSE);
 */
}
