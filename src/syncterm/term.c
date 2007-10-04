/* $Id$ */

#include <genwrap.h>
#include <ciolib.h>
#include <cterm.h>
#include <mouse.h>
#include <keys.h>

#include "threadwrap.h"
#include "filewrap.h"

#include "conn.h"
#include "syncterm.h"
#include "term.h"
#include "uifcinit.h"
#include "filepick.h"
#include "menu.h"
#include "dirwrap.h"
#include "zmodem.h"
#include "telnet_io.h"
#ifdef WITH_WXWIDGETS
#include "htmlwin.h"
#endif

#ifdef GUTS_BUILTIN
#include "gutsz.h"
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
								sbuffer[pos*2+1]=sbuffer[pos*2+1]&0x8F|0x10;
							else
								sbuffer[pos*2+1]=sbuffer[pos*2+1]&0x8F|0x60;
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
								copybuf[outpos++]='\r';
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

void update_status(struct bbslist *bbs, int speed)
{
	char nbuf[LIST_NAME_MAX+10+11+1];	/* Room for "Name (Logging) (115300)" and terminator */
						/* SAFE and Logging should me be possible. */
	int oldscroll;
	int olddmc;
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
	olddmc=hold_update;
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
				cprintf(" %-30.30s \263 %-6.6s \263 Connected: Too Long \263 ALT-Z for menu ",nbuf,conn_types[bbs->conn_type]);
			else
				cprintf(" %-30.30s \263 %-6.6s \263 Connected: %02d:%02d:%02d \263 ALT-Z for menu ",nbuf,conn_types[bbs->conn_type],timeon/3600,(timeon/60)%60,timeon%60);
			break;
	}
	_wscroll=oldscroll;
	textattr(txtinfo.attribute);
	window(txtinfo.winleft,txtinfo.wintop,txtinfo.winright,txtinfo.winbottom);
	hold_update=olddmc;
	gotoxy(txtinfo.curx,txtinfo.cury);
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

enum { ZMODEM_MODE_SEND, ZMODEM_MODE_RECV } zmodem_mode;

static BOOL zmodem_check_abort(void* vp)
{
	zmodem_t* zm = (zmodem_t*)vp;
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
		fprintf(log_fp,"%s: %s\n",log_levels[level], str);

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
void zmodem_progress(void* cbdata, ulong current_pos)
{
	char		orig[128];
	unsigned	cps;
	long		l;
	long		t;
	time_t		now;
	static time_t last_progress;
	int			old_hold;
	zmodem_t*	zm=(zmodem_t*)cbdata;

	zmodem_check_abort(cbdata);

	now=time(NULL);
	if(now-last_progress>0 || current_pos >= zm->current_file_size) {
		old_hold = hold_update;
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
	return(conn_data_waiting());
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
void zmodem_upload(struct bbslist *bbs, FILE *fp, char *path);

void begin_upload(struct bbslist *bbs, BOOL autozm)
{
	char	str[MAX_PATH*2+1];
	char	path[MAX_PATH+1];
	int		result;
	int i;
	FILE*	fp;
	struct file_pick fpick;
	char	*opts[3]={
			 "Zmodem"
			,"ASCII"
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
		return;
	}
	SAFECOPY(path,fpick.selected[0]);
	filepick_free(&fpick);

	if((fp=fopen(path,"rb"))==NULL) {
		SAFEPRINTF2(str,"Error %d opening %s for read",errno,path);
		uifcmsg("ERROR",str);
		uifcbail();
		puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
		return;
	}
	setvbuf(fp,NULL,_IOFBF,0x10000);

	if(autozm) 
		zmodem_upload(bbs, fp, path);
	else {
		i=0;
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Transfer Type",opts)) {
			case 0:
				zmodem_upload(bbs, fp, path);
				break;
			case 1:
				ascii_upload(fp);
				break;
		}
	}
	uifcbail();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
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

void zmodem_download(struct bbslist *bbs);

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
				return;
			}
			strListPush(&gi.files, fpick.selected[0]);
			filepick_free(&fpick);

			uifcbail();

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

	draw_transfer_window("Zmodem Upload");

	zmodem_mode=ZMODEM_MODE_SEND;

	conn_binary_mode_on();
	zmodem_init(&zm
		,/* cbdata */&zm
		,lputs, zmodem_progress
		,send_byte,recv_byte
		,is_connected
		,zmodem_check_abort
		,data_waiting);

	zm.current_file_num = zm.total_files = 1;	/* ToDo: support multi-file/batch uploads */
	
	fsize=filelength(fileno(fp));

	lprintf(LOG_INFO,"Sending %s (%lu KB) via Zmodem"
		,path,fsize/1024);

	if(zmodem_send_file(&zm, path, fp
		,/* ZRQINIT? */TRUE, /* start_time */NULL, /* sent_bytes */ NULL))
		zmodem_get_zfin(&zm);

	fclose(fp);

	conn_binary_mode_off();
	lprintf(LOG_NOTICE,"Hit any key to continue...");
	getch();

	erase_transfer_window();
}

void zmodem_download(struct bbslist *bbs)
{
	zmodem_t	zm;
	int			files_received;
	ulong		bytes_received;

	if(safe_mode)
		return;
	draw_transfer_window("Zmodem Download");

	zmodem_mode=ZMODEM_MODE_RECV;

	conn_binary_mode_on();
	zmodem_init(&zm
		,/* cbdata */&zm
		,lputs, zmodem_progress
		,send_byte,recv_byte
		,is_connected
		,zmodem_check_abort
		,data_waiting);

	files_received=zmodem_recv_files(&zm,bbs->dldir,&bytes_received);

	if(files_received>1)
		lprintf(LOG_INFO,"Received %u files (%lu bytes) successfully", files_received, bytes_received);

	conn_binary_mode_off();
	lprintf(LOG_NOTICE,"Hit any key to continue...");
	getch();

	erase_transfer_window();
}
/* End of Zmodem Stuff */

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

	i=j=getfont();
	uifc.helpbuf="`Font Setup`\n\n"
				"Change the current font.  Must support the current video mode.";
	k=uifc.list(WIN_MID|WIN_SAV|WIN_INS,0,0,0,&i,&j,"Font Setup",font_names);
	if(k!=-1) {
		if(k & MSK_INS) {
			struct file_pick fpick;
			j=filepick(&uifc, "Load Font From File", &fpick, ".", NULL, 0);

			if(j!=-1 && fpick.files>=1)
				loadfont(fpick.selected[0]);
			filepick_free(&fpick);
		}
		else
			setfont(i,FALSE);
	}
	uifcbail();
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
	unsigned char *p;
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
	BOOL	rd;
	int 	emulation=CTERM_EMULATION_ANSI_BBS;

	speed = bbs->bpsrate;
	log_level = bbs->xfer_loglevel;
	conn_api.log_level = bbs->telnet_loglevel;
	ciomouse_setevents(0);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_2_CLICK);
	if(scrollback_buf != NULL)
		memset(scrollback_buf,0,term.width*2*settings.backlines);
	switch(bbs->screen_mode) {
		case SCREEN_MODE_C64:
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			emulation = CTERM_EMULATION_PETASCII;
			break;
		case SCREEN_MODE_ATARI:
			emulation = CTERM_EMULATION_ATASCII;
			break;
	}
	cterm_init(term.height,term.width,term.x-1,term.y-1,settings.backlines,scrollback_buf, emulation);
	cterm.music_enable=bbs->music;
	ch[1]=0;
	zrqbuf[0]=0;
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
		if(!speed && bbs->bpsrate)
			speed = bbs->bpsrate;
		if(speed)
			thischar=xp_timer();

		if(!term.nostatus)
			update_status(bbs, speed);
		while(conn_data_waiting() || !conn_connected()) {
			if(!speed || thischar < lastchar /* Wrapped */ || thischar >= nextchar) {
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
							cterm_write("\x0c",1,NULL,0,NULL);	/* Clear screen into scrollback */
							scrollback_lines=cterm.backpos;
							cterm_end();
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(FALSE);
						}
						break;
					default:
						if(speed) {
							lastchar = xp_timer();
							nextchar = lastchar + 1/(long double)(speed/10);
						}

#ifdef GUTS_BUILTIN
						if(!gutsbuf[0]) {
							if(inch == gutsinit[0]) {
								gutsbuf[0]=inch;
								gutsbuf[1]=0;
								continue;
							}
						}
						else {		/* Already have the start of the sequence */
							j=strlen(gutsbuf);
							if(inch == gutsinit[j]) {
								gutsbuf[j]=inch;
								gutsbuf[++j]=0;
								if(j==sizeof(gutsinit)) /* Have full sequence */
									guts_transfer(bbs);
							}
							else {
								gutsbuf[j++]=inch;
								cterm_write(gutsbuf, j, prn, sizeof(prn), &speed);
								if(prn[0])
									conn_send(prn,strlen(prn),0);
								updated=TRUE;
								gutsbuf[0]=0;
							}
							continue;
						}
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

						if(!htmldet[0]) {
							if(inch == htmldetect[0]) {
								htmldet[0]=inch;
								htmldet[1]=0;
								continue;
							}
						}
						else {
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
							else {
								htmldet[j++]=inch;
								cterm_write(htmldet, j, prn, sizeof(prn), &speed);
								if(prn[0])
									conn_send(prn,strlen(prn),0);
								updated=TRUE;
								htmldet[0]=0;
							}
							continue;
						}
#endif

						if(!zrqbuf[0]) {
							if(inch == zrqinit[0] || inch == zrinit[0]) {
								zrqbuf[0]=inch;
								zrqbuf[1]=0;
								continue;
							}
						}
						else {	/* Already have the start of the sequence */
							j=strlen(zrqbuf);
							if(inch == zrqinit[j] || inch == zrinit[j]) {
								zrqbuf[j]=inch;
								zrqbuf[++j]=0;
								if(j==sizeof(zrqinit)-1) {	/* Have full sequence (Assumes zrinit and zrqinit are same length */
									if(!strcmp(zrqbuf, zrqinit))
										zmodem_download(bbs);
									else
										begin_upload(bbs, TRUE);
									zrqbuf[0]=0;
								}
							}
							else {	/* Not a real zrqinit */
								zrqbuf[j++]=inch;
								cterm_write(zrqbuf, j, prn, sizeof(prn), &speed);
								if(prn[0])
									conn_send(prn,strlen(prn),0);
								updated=TRUE;
								zrqbuf[0]=0;
							}
							continue;
						}

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
		hold_update=FALSE;
		if(updated && sleep)
			gotoxy(wherex(), wherey());

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
								conn_send(p,strlen(p),0);
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
					zmodem_download(bbs);
					showmouse();
					key = 0;
					break;
				case 0x2100:	/* ALT-F */
					font_control(bbs);
					showmouse();
					key = 0;
					break;
				case 0x2600:	/* ALT-L */
					conn_send(bbs->user,strlen(bbs->user),0);
					conn_send("\r",1,0);
					SLEEP(10);
					conn_send(bbs->password,strlen(bbs->password),0);
					conn_send("\r",1,0);
					if(bbs->syspass[0]) {
						SLEEP(10);
						conn_send(bbs->syspass,strlen(bbs->syspass),0);
						conn_send("\r",1,0);
					}
					key = 0;
					break;
				case 0x3200:	/* ALT-M */
					music_control(bbs);
					showmouse();
					key = 0;
					break;
				case 0x1600:	/* ALT-U - Upload */
					begin_upload(bbs, FALSE);
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
						struct	text_info txtinfo;

   						gettextinfo(&txtinfo);
						buf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
						gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
						i=0;
						init_uifc(FALSE, FALSE);
						if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Disconnect... Are you sure?",opts)==0) {
#ifdef WITH_WXWIDGETS
							if(html_mode != HTML_MODE_HIDDEN) {
								hide_html();
								html_cleanup();
								html_mode=HTML_MODE_HIDDEN;
							}
#endif
							uifcbail();
							cterm_write("\x0c",1,NULL,0,NULL);	/* Clear screen into scrollback */
							scrollback_lines=cterm.backpos;
							cterm_end();
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(key==0x2d00 /* Alt-X? */);
						}
						uifcbail();
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
							cterm_write("\x0c",1,NULL,0,NULL);	/* Clear screen into scrollback */
							scrollback_lines=cterm.backpos;
							cterm_end();
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(FALSE);
						case 3:
							begin_upload(bbs, FALSE);
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
						case 11:
#ifdef WITH_WXWIDGETS
							if(html_mode != HTML_MODE_HIDDEN) {
								hide_html();
								html_cleanup();
								html_mode=HTML_MODE_HIDDEN;
							}
#endif
							cterm_write("\x0c",1,NULL,0,NULL);	/* Clear screen into scrollback */
							scrollback_lines=cterm.backpos;
							cterm_end();
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(TRUE);
					}
					showmouse();
					gotoxy(i,j);
					key = 0;
					break;
				case 0x9800:	/* ALT-Up */
					if(speed)
						speed=rates[get_rate_num(speed)+1];
					else
						speed=rates[0];
					key = 0;
					break;
				case 0xa000:	/* ALT-Down */
					i=get_rate_num(speed);
					if(i==0)
						speed=0;
					else
						speed=rates[i-1];
					key = 0;
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
