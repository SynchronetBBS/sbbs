/* Copyright (C), 2007 by Stephen Hurd */

/* $Id$ */

#include <genwrap.h>
#include <ciolib.h>
#include <cterm.h>

#include "threadwrap.h"
#include "filewrap.h"
#include "xpbeep.h"
#include "xpendian.h"

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
#include "saucedefs.h"

#ifndef WITHOUT_OOII
#include "ooii.h"
#endif
#include "base64.h"
#include "md5.h"

#define	ANSI_REPLY_BUFSIZE	2048
static char ansi_replybuf[2048];

#define DUMP

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

struct terminal term;
struct cterminal	*cterm;

#define TRANSFER_WIN_WIDTH	66
#define TRANSFER_WIN_HEIGHT	18
static struct vmem_cell winbuf[(TRANSFER_WIN_WIDTH + 2) * (TRANSFER_WIN_HEIGHT + 1) * 2];	/* Save buffer for transfer window */
static struct text_info	trans_ti;
static struct text_info	log_ti;

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
void mousedrag(struct vmem_cell *scrollback)
{
	int	key;
	struct mouse_event mevent;
	struct vmem_cell *screen;
	unsigned char *tscreen;
	struct vmem_cell *sbuffer;
	int sbufsize;
	int pos, startpos,endpos, lines;
	int outpos;
	char *copybuf=NULL;
	char *newcopybuf;
	int lastchar;
	int old_xlat = ciolib_xlat;
	struct ciolib_screen *savscrn;

	sbufsize=term.width*sizeof(*screen)*term.height;
	screen=malloc(sbufsize);
	sbuffer=malloc(sbufsize);
	tscreen=malloc(term.width*2*term.height);
	vmem_gettext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
	ciolib_xlat = CIOLIB_XLAT_CHARS;
	gettext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,tscreen);
	savscrn = savescreen();
	ciolib_xlat = old_xlat;
	while(1) {
		key=getch();
		if(key==0 || key==0xe0)
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
							if((sbuffer[pos].legacy_attr&0x70)!=0x10)
								sbuffer[pos].legacy_attr=(sbuffer[pos].legacy_attr&0x8F)|0x10;
							else
								sbuffer[pos].legacy_attr=(sbuffer[pos].legacy_attr&0x8F)|0x60;
							if(((sbuffer[pos].legacy_attr&0x70)>>4) == (sbuffer[pos].legacy_attr&0x0F)) {
								sbuffer[pos].legacy_attr|=0x08;
							}
							attr2palette(sbuffer[pos].legacy_attr, &sbuffer[pos].fg, &sbuffer[pos].bg);
						}
						vmem_puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,sbuffer);
						break;
					default:
						lines=abs(mevent.endy-mevent.starty)+1;
						newcopybuf=realloc(copybuf, endpos-startpos+4+lines*2);
						if (newcopybuf)
							copybuf = newcopybuf;
						else
							goto cleanup;
						outpos=0;
						lastchar=0;
						for(pos=startpos;pos<=endpos;pos++) {
							copybuf[outpos++]=tscreen[pos*2];
							if(tscreen[pos*2]!=' ' && tscreen[pos*2])
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
						vmem_puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
						goto cleanup;
				}
				break;
			default:
				vmem_puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
				ungetch(key);
				goto cleanup;
		}
	}

cleanup:
	free(screen);
	free(sbuffer);
	free(tscreen);
	if(copybuf)
		free(copybuf);
	restorescreen(savscrn);
	freescreen(savscrn);
	return;
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
	char sep;
	int old_xlat = ciolib_xlat;
	int oldfont_norm;
	int oldfont_bright;

	oldfont_norm=getfont(1);
	oldfont_bright=getfont(2);
	setfont(0, FALSE, 1);
	setfont(0, FALSE, 2);
	switch(getfont(1)) {
			case 0:
			case 17:
			case 18:
			case 19:
			case 25:
			case 26:
			case 27:
			case 28:
			case 29:
			case 31:
				sep = 0xb3;
				break;
			default:
				sep = '|';
	}
	now=time(NULL);
	if(now==lastupd && speed==oldspeed) {
		setfont(oldfont_norm,0,1);
		setfont(oldfont_bright,0,2);
		return;
	}
	ciolib_xlat = CIOLIB_XLAT_CHARS;
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
	if(cterm->log)
		strcat(nbuf, " (Logging)");
	if(speed)
		sprintf(strchr(nbuf,0)," (%d)", speed);
	if(cterm->doorway_mode)
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
	ciolib_setcolour(11, 4);
	switch(cio_api.mode) {
		case CIOLIB_MODE_CURSES:
		case CIOLIB_MODE_CURSES_IBM:
		case CIOLIB_MODE_ANSI:
			if(timeon>359999)
				cprintf(" %-29.29s %c %-6.6s %c Connected: Too Long %c CTRL-S for menu ",nbuf,sep,conn_types[bbs->conn_type],sep,sep);
			else
				cprintf(" %-29.29s %c %-6.6s %c Connected: %02d:%02d:%02d %c CTRL-S for menu ",nbuf,sep,conn_types[bbs->conn_type],sep,timeon/3600,(timeon/60)%60,timeon%60,sep);
			break;
		default:
			if(timeon>359999)
				cprintf(" %-30.30s %c %-6.6s %c Connected: Too Long %c "ALT_KEY_NAME3CH"-Z for menu ",nbuf,sep,conn_types[bbs->conn_type],sep,sep);
			else
				cprintf(" %-30.30s %c %-6.6s %c Connected: %02d:%02d:%02d %c "ALT_KEY_NAME3CH"-Z for menu ",nbuf,sep,conn_types[bbs->conn_type],sep,timeon/3600,(timeon/60)%60,timeon%60,sep);
			break; /*    1+29     +3    +6    +3    +11        +3+3+2        +3    +6    +4  +5 */
	}
	if(wherex()>=80)
		clreol();
	_wscroll=oldscroll;
	setfont(oldfont_norm,0,1);
	setfont(oldfont_bright,0,2);
	textattr(txtinfo.attribute);
	window(txtinfo.winleft,txtinfo.wintop,txtinfo.winright,txtinfo.winbottom);
	gotoxy(txtinfo.curx,txtinfo.cury);
	hold_update=olddmc;
	ciolib_xlat = old_xlat;
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
	static time_t			last_check=0;
	time_t					now=time(NULL);
	int						key;

	if (quitting) {
		zm->cancelled=TRUE;
		zm->local_abort=TRUE;
		return TRUE;
	}
	if(last_check != now) {
		last_check=now;
		if(zm!=NULL) {
			while(kbhit()) {
				switch((key=getch())) {
					case ESC:
					case CTRL_C:
					case CTRL_X:
						zm->cancelled=TRUE;
						zm->local_abort=TRUE;
						break;
					case 0:
					case 0xe0:
						key |= (getch() << 8);
						if(key==CIO_KEY_MOUSE)
							getmouse(NULL);
						if (key==CIO_KEY_QUIT) {
							if (check_exit(FALSE)) {
								zm->cancelled=TRUE;
								zm->local_abort=TRUE;
							}
						}
						break;
				}
			}
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
	int oldhold=hold_update;

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
	hold_update=FALSE;
	chars=cputs(msg);
	hold_update=oldhold;
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
void zmodem_progress(void* cbdata, int64_t current_pos)
{
	char		orig[128];
	unsigned	cps;
	int		l;
	time_t		t;
	time_t		now;
	static time_t last_progress=0;
	int			old_hold=hold_update;
	struct zmodem_cbdata *zcb=(struct zmodem_cbdata *)cbdata;
	zmodem_t*	zm=zcb->zm;
	BOOL		growing=FALSE;

	now=time(NULL);
	if(current_pos > zm->current_file_size)
		growing=TRUE;
	if(now != last_progress || (current_pos >= zm->current_file_size && growing==FALSE)) {
		zmodem_check_abort(cbdata);
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
		if((cps=(unsigned)((current_pos-zm->transfer_start_pos)/t))==0)
			cps=1;		/* cps so far */
		l=zm->current_file_size/cps;	/* total transfer est time */
		l-=t;			/* now, it's est time left */
		if(l<0) l=0;
		cprintf("File (%u of %u): %-.*s"
			,zm->current_file_num, zm->total_files, TRANSFER_WIN_WIDTH - 20, zm->current_file_name);
		clreol();
		cputs("\r\n");
		if(zm->transfer_start_pos)
			sprintf(orig,"From: %"PRId64"  ", zm->transfer_start_pos);
		else
			orig[0]=0;
		cprintf("%sByte: %"PRId64" of %"PRId64" (%"PRId64" KB)"
			,orig, current_pos, zm->current_file_size, zm->current_file_size/1024);
		clreol();
		cputs("\r\n");
		cprintf("Time: %lu:%02lu  ETA: %lu:%02lu  Block: %u/CRC-%u  %u cps"
			,(unsigned long)(t/60L)
			,(unsigned long)(t%60L)
			,(unsigned long)(l/60L)
			,(unsigned long)(l%60L)
			,zm->block_size
			,zmodem_mode==ZMODEM_MODE_RECV ? (zm->receive_32bit_data ? 32:16) : 
				(zm->can_fcs_32 && !zm->want_fcs_16) ? 32:16
			,cps
			);
		clreol();
		cputs("\r\n");
		if(zm->current_file_size==0) {
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH/2-5, "", 100);
			l = 60;
		}
		else{
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH/2-5, ""
				,(long)(((float)current_pos/(float)zm->current_file_size)*100.0));
			l = (long)(60*((float)current_pos/(float)zm->current_file_size));
		}
		cprintf("[%*.*s%*s]", l, l, 
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				, (int)(60-l), "");
		last_progress=now;
		hold_update = FALSE;
		gotoxy(wherex(), wherey());
		hold_update = old_hold;
	}
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif

unsigned char transfer_buffer[BUFFER_SIZE/2];
unsigned transfer_buf_len=0;

static void flush_send(void *unused)
{
	int	sent;

	sent=conn_send(transfer_buffer, transfer_buf_len, 120*1000);
	if(sent < transfer_buf_len) {
		memmove(transfer_buffer, transfer_buffer+sent, transfer_buf_len-sent);
		transfer_buf_len -= sent;
	}
	else
		transfer_buf_len=0;
}

static int send_byte(void* unused, uchar ch, unsigned timeout /* seconds */)
{
	transfer_buffer[transfer_buf_len++]=ch;
	if(transfer_buf_len==sizeof(transfer_buffer))
		flush_send(unused);
	return(!(transfer_buf_len < sizeof(transfer_buffer)));
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
BYTE	recv_byte_buffer[BUFFER_SIZE];
unsigned recv_byte_buffer_len=0;
unsigned recv_byte_buffer_pos=0;

static void recv_bytes(unsigned timeout /* Milliseconds */)
{
	if(recv_byte_buffer_len == 0)
		recv_byte_buffer_len=conn_recv_upto(recv_byte_buffer, sizeof(recv_byte_buffer), timeout);
}

static int recv_byte(void* unused, unsigned timeout /* seconds */)
{
	BYTE ch;

	recv_bytes(timeout*1000);

	if(recv_byte_buffer_len > 0) {
		ch=recv_byte_buffer[recv_byte_buffer_pos++];
		if(recv_byte_buffer_pos == recv_byte_buffer_len)
			recv_byte_buffer_len=recv_byte_buffer_pos=0;
		return ch;
	}

	return -1;
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
BOOL data_waiting(void* unused, unsigned timeout /* seconds */)
{
	BOOL	ret;

	if(recv_byte_buffer_len)
		return TRUE;
	pthread_mutex_lock(&(conn_inbuf.mutex));
	ret = conn_buf_wait_bytes(&conn_inbuf, 1, timeout*1000)!=0;
	pthread_mutex_unlock(&(conn_inbuf.mutex));
	return ret;
}

size_t count_data_waiting(void)
{
	recv_bytes(0);
	return recv_byte_buffer_len;
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
	window(1, 1, trans_ti.screenwidth, trans_ti.screenheight);

	vmem_gettext(left, top, left + TRANSFER_WIN_WIDTH + 1, top + TRANSFER_WIN_HEIGHT, winbuf);
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
	vmem_puttext(
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
	struct ciolib_screen *savscrn;

    gettextinfo(&txtinfo);
    savscrn = savescreen();

	if(safe_mode)
		return;

	init_uifc(FALSE, FALSE);
	result=filepick(&uifc, "Upload", &fpick, bbs->uldir, NULL, UIFC_FP_ALLOWENTRY);
	
	if(result==-1 || fpick.files<1) {
		check_exit(FALSE);
		filepick_free(&fpick);
		uifcbail();
		restorescreen(savscrn);
		freescreen(savscrn);
		gotoxy(txtinfo.curx, txtinfo.cury);
		setup_mouse_events();
		return;
	}
	SAFECOPY(path,fpick.selected[0]);
	filepick_free(&fpick);
	restorescreen(savscrn);

	if((fp=fopen(path,"rb"))==NULL) {
		SAFEPRINTF2(str,"Error %d opening %s for read",errno,path);
		uifcmsg("Error opening file",str);
		uifcbail();
		setup_mouse_events();
		restorescreen(savscrn);
		freescreen(savscrn);
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
	restorescreen(savscrn);
	freescreen(savscrn);
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
	int old_hold=hold_update;
	struct ciolib_screen *savscrn;

	if(safe_mode)
		return;

    gettextinfo(&txtinfo);
    savscrn = savescreen();

	init_uifc(FALSE, FALSE);

	i=0;
	uifc.helpbuf="Select Protocol";
	hold_update=FALSE;
	switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Protocol",opts)) {
		case -1:
			check_exit(FALSE);
			break;
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
	restorescreen(savscrn);
	freescreen(savscrn);
	gotoxy(txtinfo.curx, txtinfo.cury);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static BOOL is_connected(void* unused)
{
	if(recv_byte_buffer_len)
		return TRUE;
	return(conn_connected());
}

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
			cterm_write(cterm, ch, 1, NULL, 0, NULL);
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
			cterm_write(cterm, ch, 1, NULL, 0, NULL);
		}
	}
	fclose(fp);
}

static void transfer_complete(BOOL success)
{
	int timeout = success ? settings.xfer_success_keypress_timeout : settings.xfer_failure_keypress_timeout;

	conn_binary_mode_off();
	if(log_fp!=NULL)
		fflush(log_fp);
	/* TODO: Make this pretty (countdown timer) and don't delay a second between keyboard polls */
	lprintf(LOG_NOTICE,"Hit any key or wait %u seconds to continue...", timeout);
	while(timeout > 0) {
		if (kbhit()) {
			if(getch()==0 && getch()<<8 == CIO_KEY_QUIT)
				check_exit(FALSE);
			break;
		}
		timeout--;
		SLEEP(1000);
	}

	erase_transfer_window();
}

void zmodem_upload(struct bbslist *bbs, FILE *fp, char *path)
{
	BOOL		success;
	zmodem_t	zm;
	int64_t		fsize;
	struct zmodem_cbdata cbdata;

	draw_transfer_window("ZMODEM Upload");

	zmodem_mode=ZMODEM_MODE_SEND;

	cbdata.zm=&zm;
	cbdata.bbs=bbs;
	conn_binary_mode_on();
	transfer_buf_len=0;
	zmodem_init(&zm
		,/* cbdata */&cbdata
		,lputs, zmodem_progress
		,send_byte,recv_byte
		,is_connected
		,zmodem_check_abort
		,data_waiting
		,flush_send);
	zm.log_level=&log_level;

	zm.current_file_num = zm.total_files = 1;	/* ToDo: support multi-file/batch uploads */
	
	fsize=filelength(fileno(fp));

	lprintf(LOG_INFO,"Sending %s (%"PRId64" KB) via ZMODEM"
		,path,fsize/1024);

	if((success=zmodem_send_file(&zm, path, fp
		,/* ZRQINIT? */TRUE, /* start_time */NULL, /* sent_bytes */ NULL)) == TRUE)
		zmodem_get_zfin(&zm);

	fclose(fp);

	transfer_complete(success);
}

BOOL zmodem_duplicate_callback(void *cbdata, void *zm_void)
{
	struct	text_info txtinfo;
	struct ciolib_screen *savscrn;
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
    savscrn = savescreen();
	window(1, 1, txtinfo.screenwidth, txtinfo.screenheight);
	init_uifc(FALSE, FALSE);
	hold_update=FALSE;

	while(loop) {
		loop=FALSE;
		i=0;
		uifc.helpbuf="Duplicate file... choose action\n";
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Duplicate File Name",opts)) {
			case -1:
				if (check_exit(FALSE)) {
					ret=FALSE;
					break;
				}
				loop=TRUE;
				break;
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
	restorescreen(savscrn);
	freescreen(savscrn);
	gotoxy(txtinfo.curx, txtinfo.cury);
	hold_update=old_hold;
	return(ret);
}

void zmodem_download(struct bbslist *bbs)
{
	zmodem_t	zm;
	int			files_received;
	uint64_t	bytes_received;
	struct zmodem_cbdata cbdata;

	if(safe_mode)
		return;
	draw_transfer_window("ZMODEM Download");

	zmodem_mode=ZMODEM_MODE_RECV;

	conn_binary_mode_on();
	cbdata.zm=&zm;
	cbdata.bbs=bbs;
	transfer_buf_len=0;
	zmodem_init(&zm
		,/* cbdata */&cbdata
		,lputs, zmodem_progress
		,send_byte,recv_byte
		,is_connected
		,zmodem_check_abort
		,data_waiting
		,flush_send);
	zm.log_level=&log_level;

	zm.duplicate_filename=zmodem_duplicate_callback;

	files_received=zmodem_recv_files(&zm,bbs->dldir,&bytes_received);

	if(files_received>1)
		lprintf(LOG_INFO,"Received %u files (%"PRId64" bytes) successfully", files_received, bytes_received);

	transfer_complete(files_received);
}
/* End of Zmodem Stuff */

/* X/Y-MODEM stuff */

uchar	block[1024];					/* Block buffer 					*/
ulong	block_num;						/* Block number 					*/

static BOOL xmodem_check_abort(void* vp)
{
	xmodem_t* xm = (xmodem_t*)vp;
	static time_t			last_check=0;
	time_t					now=time(NULL);
	int						key;

	if (xm == NULL)
		return FALSE;

	if (quitting) {
		xm->cancelled=TRUE;
		return TRUE;
	}

	if(last_check != now) {
		last_check=now;
		while(kbhit()) {
			switch((key=getch())) {
				case ESC:
				case CTRL_C:
				case CTRL_X:
					xm->cancelled=TRUE;
					break;
				case 0:
				case 0xe0:
					key |= (getch() << 8);
					if(key==CIO_KEY_MOUSE)
						getmouse(NULL);
					if (key==CIO_KEY_QUIT) {
						if (check_exit(FALSE))
							xm->cancelled=TRUE;
					}
					break;
			}
		}
	}
	return(xm->cancelled);
}
/****************************************************************************/
/* Returns the number of blocks required to send len bytes					*/
/****************************************************************************/
uint64_t num_blocks(unsigned curr_block, uint64_t offset, uint64_t len, unsigned block_size)
{
	uint64_t blocks;

	len-=offset;
	blocks=len/block_size;
	if(len%block_size)
		blocks++;
	return(curr_block+blocks);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
void xmodem_progress(void* cbdata, unsigned block_num, int64_t offset, int64_t fsize, time_t start)
{
	uint64_t	total_blocks;
	unsigned	cps;
	time_t		l;
	time_t		t;
	time_t		now;
	static time_t last_progress;
	int			old_hold=hold_update;
	xmodem_t*	xm=(xmodem_t*)cbdata;

	now=time(NULL);
	if(now-last_progress>0 || offset >= fsize) {
		xmodem_check_abort(cbdata);

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
		if((cps=(unsigned)(offset/t))==0)
			cps=1;		/* cps so far */
		l=(time_t)(fsize/cps);	/* total transfer est time */
		l-=t;			/* now, it's est time left */
		if(l<0) l=0;
		if((*(xm->mode))&SEND) {
			total_blocks=num_blocks(block_num,offset,fsize,xm->block_size);
			cprintf("Block (%lu%s): %u/%"PRId64"  Byte: %"PRId64
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
				,fsize?(long)(((float)offset/(float)fsize)*100.0):100
				);
			clreol();
			cputs("\r\n");
			cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH/2-5, ""
				,fsize?(long)(((float)offset/(float)fsize)*100.0):100);
			l = fsize?(long)(((float)offset/(float)fsize)*60.0):60;
			cprintf("[%*.*s%*s]", l, l, 
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					, 60-l, "");
		} else if((*(xm->mode))&YMODEM) {
			cprintf("Block (%lu%s): %lu  Byte: %"PRId64
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
				,fsize?(long)(((float)offset/(float)fsize)*100.0):100);
			l = fsize?(long)(((float)offset/(float)fsize)*60.0):60;
			cprintf("[%*.*s%*s]", l, l, 
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
					, 60-l, "");
		} else { /* XModem receive */
			cprintf("Block (%lu%s): %lu  Byte: %"PRId64
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
	BOOL		success;
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
		,xmodem_check_abort
		,flush_send);
	xm.log_level=&log_level;
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
		lprintf(LOG_INFO,"Sending %s (%"PRId64" KB) via XMODEM%s"
			,path,fsize/1024,(mode&GMODE)?"-g":"");
	}
	else if(mode&YMODEM) {
		if(mode&GMODE)
			draw_transfer_window("YMODEM-g Upload");
		else
			draw_transfer_window("YMODEM Upload");
		lprintf(LOG_INFO,"Sending %s (%"PRId64" KB) via YMODEM%s"
			,path,fsize/1024,(mode&GMODE)?"-g":"");
	}
	else {
		fclose(fp);
		conn_binary_mode_off();
		return;
	}

	if((success=xmodem_send_file(&xm, path, fp
		,/* start_time */NULL, /* sent_bytes */ NULL)) == TRUE) {
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

	transfer_complete(success);
}

BOOL xmodem_duplicate(xmodem_t *xm, struct bbslist *bbs, char *path, size_t pathsize, char *fname)
{
	struct	text_info txtinfo;
	struct ciolib_screen *savscrn;
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
    savscrn = savescreen();
	window(1, 1, txtinfo.screenwidth, txtinfo.screenheight);

	init_uifc(FALSE, FALSE);

	hold_update=FALSE;
	while(loop) {
		loop=FALSE;
		i=0;
		uifc.helpbuf="Duplicate file... choose action\n";
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Duplicate File Name",opts)) {
			case -1:
				if (check_exit(FALSE)) {
					ret=FALSE;
					break;
				}
				loop=TRUE;
				break;
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
	restorescreen(savscrn);
	freescreen(savscrn);
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
	int64_t	file_bytes=0,file_bytes_left=0;
	int64_t	total_bytes=0;
	FILE*	fp=NULL;
	time_t	t,startfile,ftime=0;
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
		,xmodem_check_abort
		,flush_send);
	xm.log_level=&log_level;
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
					send_byte(&xm,ACK,10);
					flush_send(&xm);
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
				file_bytes=total_bytes=0;
				ftime=total_files=0;
				i=sscanf(((char *)block)+strlen((char *)block)+1,"%"PRId64" %lo %lo %lo %d %"PRId64
					,&file_bytes			/* file size (decimal) */
					,&tmpftime 				/* file time (octal unix format) */
					,&fmode 				/* file mode (not used) */
					,&serial_num			/* program serial number */
					,&total_files			/* remaining files to be sent */
					,&total_bytes			/* remaining bytes to be sent */
					);
				ftime=tmpftime;
				lprintf(LOG_DEBUG,"YMODEM header (%u fields): %s", i, block+strlen((char *)block)+1);
				SAFECOPY(fname,((char *)block));

				if(!file_bytes)
					file_bytes=0x7fffffff;
				file_bytes_left=file_bytes;
				if(!total_files)
					total_files=1;
				if(total_bytes<file_bytes)
					total_bytes=file_bytes;

				lprintf(LOG_DEBUG,"Incoming filename: %.64s ",getfname(fname));

				sprintf(str,"%s/%s",bbs->dldir,getfname(fname));
				lprintf(LOG_INFO,"File size: %"PRId64" bytes", file_bytes);
				if(total_files>1)
					lprintf(LOG_INFO,"Remaining: %"PRId64" bytes in %u files", total_bytes, total_files);
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
			lprintf(LOG_INFO,"Receiving %s (%"PRId64" KB) via %s %s"
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
			xmodem_progress(&xm,block_num,ftello(fp),file_bytes,startfile);
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
			if(!(mode&GMODE)) {
				send_byte(&xm,ACK,10);
				flush_send(&xm);
			}
			if(file_bytes_left<=0L)  { /* No more bytes to receive */
				lprintf(LOG_WARNING,"Sender attempted to send more bytes than were specified in header");
				break; 
			}
			wr=xm.block_size;
			if(wr>(uint)file_bytes_left)
				wr=(uint)file_bytes_left;
			if(fwrite(block,1,wr,fp)!=wr) {
				lprintf(LOG_ERR,"Error writing %u bytes to file at offset %"PRId64
					,wr,(int64_t)ftello(fp));
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
			lprintf(LOG_INFO,"Truncating file to %lu bytes", (ulong)file_bytes);
			chsize(fileno(fp),(ulong)file_bytes);	/* 4GB limit! */
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
		if((cps=(unsigned)(file_bytes/t))==0)
			cps=1;
		total_files--;
		total_bytes-=file_bytes;
		if(total_files>1 && total_bytes)
			lprintf(LOG_INFO,"Remaining - Time: %lu:%02lu  Files: %u  KBytes: %"PRId64
				,(total_bytes/cps)/60
				,(total_bytes/cps)%60
				,total_files
				,total_bytes/1024
				);
	}

end:
	if(fp)
		fclose(fp);
	transfer_complete(success);
}

/* End of X/Y-MODEM stuff */

void music_control(struct bbslist *bbs)
{
	struct	text_info txtinfo;
	struct ciolib_screen *savscrn;
	int i;
	char *opts[4]={
			 "ESC[| ANSI Music only"
			,"ESC[N (BANSI-Style) and ESC[| ANSI Music"
			,"ANSI Music Enabled"
	};

   	gettextinfo(&txtinfo);
   	savscrn = savescreen();
	init_uifc(FALSE, FALSE);

	i=cterm->music_enable;
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
		cterm->music_enable=i;
	else
		check_exit(FALSE);
	uifcbail();
	setup_mouse_events();
	restorescreen(savscrn);
	freescreen(savscrn);
}

void font_control(struct bbslist *bbs)
{
	struct ciolib_screen *savscrn;
	struct	text_info txtinfo;
	int i,j,k;
	int enable_xlat = CIOLIB_XLAT_NONE;

	if(safe_mode)
		return;
   	gettextinfo(&txtinfo);
   	savscrn = savescreen();
	init_uifc(FALSE, FALSE);

	switch(cio_api.mode) {
		case CIOLIB_MODE_CONIO:
		case CIOLIB_MODE_CONIO_FULLSCREEN:
		case CIOLIB_MODE_CURSES:
		case CIOLIB_MODE_CURSES_IBM:
		case CIOLIB_MODE_ANSI:
			uifcmsg("Not supported in this video output mode."
				,"Font cannot be changed in the current video output mode");
			check_exit(FALSE);
			break;
		default:
			i=j=getfont(1);
			uifc.helpbuf="`Font Setup`\n\n"
						"Change the current font.  Font must support the current video mode:\n\n"
						"`8x8`  Used for screen modes with 35 or more lines and all C64/C128 modes\n"
						"`8x14` Used for screen modes with 28 and 34 lines\n"
						"`8x16` Used for screen modes with 30 lines or fewer than 28 lines.";
			k=uifc.list(WIN_MID|WIN_SAV|WIN_INS,0,0,0,&i,&j,"Font Setup",font_names);
			if(k!=-1) {
				if(k & MSK_INS) {
					struct file_pick fpick;
					j=filepick(&uifc, "Load Font From File", &fpick, ".", NULL, 0);
					check_exit(FALSE);

					if(j!=-1 && fpick.files>=1)
						loadfont(fpick.selected[0]);
					filepick_free(&fpick);
				}
				else {
					setfont(i,FALSE,1);
					if (i >=32 && i<= 35 && cterm->emulation != CTERM_EMULATION_PETASCII)
						enable_xlat = CIOLIB_XLAT_CHARS;
					if (i==36 && cterm->emulation != CTERM_EMULATION_ATASCII)
						enable_xlat = CIOLIB_XLAT_CHARS;
				}
			}
			else
				check_exit(FALSE);
		break;
	}
	uifcbail();
	ciolib_xlat = enable_xlat;
	setup_mouse_events();
	restorescreen(savscrn);
	freescreen(savscrn);
}

void capture_control(struct bbslist *bbs)
{
	struct ciolib_screen *savscrn;
	char *cap;
	struct	text_info txtinfo;
	int i,j;

	if(safe_mode)
		return;
   	gettextinfo(&txtinfo);
   	savscrn = savescreen();
	cap=(char *)alloca(cterm->height*cterm->width*2);
	gettext(cterm->x, cterm->y, cterm->x+cterm->width-1, cterm->y+cterm->height-1, cap);

	init_uifc(FALSE, FALSE);

	if(!cterm->log) {
		struct file_pick fpick;
		char *opts[]={
						 "ASCII"
						,"Raw"
						,"Binary"
						,"Binary with SAUCE"
						,""
					  };

		i=0;
		uifc.helpbuf="~ Capture Type ~\n\n"
			"`ASCII`              ASCII only (no ANSI escape sequences)\n"
			"`Raw`                Preserves ANSI sequences\n"
			"`Binary`             Saves current screen in IBM-CGA/BinaryText format\n"
			"`Binary with SAUCE`  Saves current screen in BinaryText format with SAUCE\n"
			"\n"
			"Raw is useful for stealing ANSI screens from other systems.\n"
			"Don't do that though.  :-)";
		if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Capture Type",opts)!=-1) {
			j=filepick(&uifc, "Capture File", &fpick, bbs->dldir, i >= 2 ? "*.bin" : NULL
				, UIFC_FP_ALLOWENTRY|UIFC_FP_OVERPROMPT);
			check_exit(FALSE);

			if(j!=-1 && fpick.files>=1) {
				if(i >= 2) {
					FILE* fp = fopen(fpick.selected[0], "wb");
					if(fp == NULL) {
						char err[256];
						sprintf(err, "Error %u opening file '%s'", errno, fpick.selected[0]);
						uifc.msg(err);
					} else {
						char msg[256];
						uifc.pop("Writing to file");
						fwrite(cap, sizeof(uint8_t), cterm->width * cterm->height * 2, fp);
						if(i > 2) {
							time_t t = time(NULL);
							struct tm* tm;
							struct sauce sauce;

							memset(&sauce, 0, sizeof(sauce));
							memcpy(sauce.id, SAUCE_ID, sizeof(sauce.id));
							memcpy(sauce.ver, SAUCE_VERSION, sizeof(sauce.ver));
							memset(sauce.title, ' ', sizeof(sauce.title));
							memset(sauce.author, ' ', sizeof(sauce.author));
							memset(sauce.group, ' ', sizeof(sauce.group));
							if(bbs != NULL) {
								memcpy(sauce.title, bbs->name, MIN(strlen(bbs->name), sizeof(sauce.title)));
								memcpy(sauce.author, bbs->user, MIN(strlen(bbs->user), sizeof(sauce.author)));
							}
							if((tm=localtime(&t)) != NULL)	// The null-terminator overwrites the first byte of filesize
								sprintf(sauce.date, "%04u%02u%02u"
									,1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday);
							sauce.filesize = LE_INT32(ftell(fp));	// LE
							sauce.datatype = sauce_datatype_bin;
							sauce.filetype = cterm->width / 2;
							if(ciolib_getvideoflags() & (CIOLIB_VIDEO_BGBRIGHT|CIOLIB_VIDEO_NOBLINK))
								sauce.tflags |= sauce_ansiflag_nonblink;

							fputc(SAUCE_SEPARATOR, fp);
							/* No comment block (no comments) */
							fwrite(&sauce.id, sizeof(sauce.id), 1, fp);
							fwrite(&sauce.ver, sizeof(sauce.ver), 1, fp);
							fwrite(&sauce.title, sizeof(sauce.title), 1, fp);
							fwrite(&sauce.author, sizeof(sauce.author), 1, fp);
							fwrite(&sauce.group, sizeof(sauce.group), 1, fp);
							fwrite(&sauce.date, sizeof(sauce.date), 1, fp);
							fwrite(&sauce.filesize, sizeof(sauce.filesize), 1, fp);
							fwrite(&sauce.datatype, sizeof(sauce.datatype), 1, fp);
							fwrite(&sauce.filetype, sizeof(sauce.filetype), 1, fp);
							fwrite(&sauce.tinfo1, sizeof(sauce.tinfo1), 1, fp);
							fwrite(&sauce.tinfo2, sizeof(sauce.tinfo2), 1, fp);
							fwrite(&sauce.tinfo3, sizeof(sauce.tinfo3), 1, fp);
							fwrite(&sauce.tinfo4, sizeof(sauce.tinfo4), 1, fp);
							fwrite(&sauce.comments, sizeof(sauce.comments), 1, fp);
							fwrite(&sauce.tflags, sizeof(sauce.tflags), 1, fp);
							fwrite(&sauce.tinfos, sizeof(sauce.tinfos), 1, fp);
						}
						fclose(fp);
						uifc.pop(NULL);
						sprintf(msg, "Screen saved to '%s'", getfname(fpick.selected[0]));
						uifc.msg(msg);
					}
				} else
					cterm_openlog(cterm, fpick.selected[0], i?CTERM_LOG_RAW:CTERM_LOG_ASCII);
			}
			filepick_free(&fpick);
		}
		else
			check_exit(FALSE);
	}
	else {
		if(cterm->log & CTERM_LOG_PAUSED) {
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
					case -1:
						check_exit(FALSE);
						break;
					case 0:
						cterm->log=cterm->log & CTERM_LOG_MASK;
						break;
					case 1:
						cterm_closelog(cterm);
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
					case -1:
						check_exit(FALSE);
						break;
					case 0:
						cterm->log |= CTERM_LOG_PAUSED;
						break;
					case 1:
						cterm_closelog(cterm);
						break;
				}
			}
		}
	}
	uifcbail();
	setup_mouse_events();
	restorescreen(savscrn);
	freescreen(savscrn);
}

#define OUTBUF_SIZE	2048

#define WRITE_OUTBUF()	\
	if(outbuf_size > 0) { \
		cterm_write(cterm, outbuf, outbuf_size, (char *)ansi_replybuf, sizeof(ansi_replybuf), &speed); \
		outbuf_size=0; \
		if(ansi_replybuf[0]) \
			conn_send(ansi_replybuf, strlen((char *)ansi_replybuf), 0); \
		updated=TRUE; \
	}

static int get_cache_fn_base(struct bbslist *bbs, char *fn, size_t fnsz)
{
	get_syncterm_filename(fn, fnsz, SYNCTERM_PATH_CACHE, FALSE);
	backslash(fn);
	strcat(fn, bbs->name);
	backslash(fn);
	if (!isdir(fn))
		mkpath(fn);
	if (!isdir(fn))
		return 0;
	return 1;
}

static int clean_path(char *fn, size_t fnsz)
{
	char *fp;

	fp = _fullpath(NULL, fn, fnsz);
	if (fp == NULL || strcmp(fp, fn)) {
		FREE_AND_NULL(fp);
		return 0;
	}
	FREE_AND_NULL(fp);
	return 1;
}

static void apc_handler(char *strbuf, size_t slen, void *apcd)
{
	char fn[MAX_PATH+1];
	char fn_root[MAX_PATH+1];
	FILE *f;
	int rc;
	size_t sz;
	char *p;
	char *buf;
	struct bbslist *bbs = apcd;
	glob_t gl;
	int i;
	MD5	ctx;
	BYTE	digest[MD5_DIGEST_SIZE];
	unsigned long slot;

	if(ansi_replybuf[0]) \
		conn_send(ansi_replybuf, strlen((char *)ansi_replybuf), 0); \
	ansi_replybuf[0] = 0;
	if (get_cache_fn_base(bbs, fn_root, sizeof(fn_root)) == 0)
		return;
	strcpy(fn, fn_root);

	if (strncmp(strbuf, "SyncTERM:C;S;", 13)==0) {
		// Request to save b64 encoded data into the cache directory.
		p = strchr(strbuf+13, ';');
		if (p == NULL)
			return;
		strncat(fn, strbuf+13, p-strbuf-13);
		if (!clean_path(fn, sizeof(fn)))
			return;
		p++;
		sz = (slen - (p-strbuf)) * 3 / 4 + 1;
		buf = malloc(sz);
		if (!buf)
			return;
		rc = b64_decode(buf, sz, p, slen);
		if (rc < 0) {
			free(buf);
			return;
		}
		p = strrchr(fn, '/');
		if (p) {
			*p = 0;
			mkpath(fn);
			*p = '/';
		}
		f = fopen(fn, "wb");
		if (f == NULL) {
			free(buf);
			return;
		}
		fwrite(buf, rc, 1, f);
		free(buf);
		fclose(f);
	}
	else if (strncmp(strbuf, "SyncTERM:C;L", 12) == 0) {
		// Cache list
		if (strbuf[12] != 0 && strbuf[12] != ';')
			return;
		if (!clean_path(fn, sizeof(fn))) {
			conn_send("\x1b_SyncTERM:C;L\n\x1b\\", 17, 0);
			return;
		}
		if (!isdir(fn)) {
			conn_send("\x1b_SyncTERM:C;L\n\x1b\\", 17, 0);
			return;
		}
		if (slen == 12)
			p = "*";
		else
			p = strbuf+13;
		strcat(fn, p);
		conn_send("\x1b_SyncTERM:C;L\n", 15, 0);
		rc = glob(fn, GLOB_MARK, NULL, &gl);
		if (rc != 0) {
			conn_send("\x1b\\", 2, 0);
			return;
		}
		buf = malloc(1024*32);
		if (buf == NULL)
			return;
		for (i=0; i<gl.gl_pathc; i++) {
			/* Skip . and .. along with any fuckery */
			if (!clean_path(gl.gl_pathv[i], MAX_PATH))
				continue;
			p = getfname(gl.gl_pathv[i]);
			conn_send(p, strlen(p), 0);
			conn_send("\t", 1, 0);
			f = fopen(gl.gl_pathv[i], "rb");
			if (f) {
				MD5_open(&ctx);
				while (!feof(f)) {
					rc = fread(buf, 1, 1024*32, f);
					if (rc > 0)
						MD5_calc(digest, buf, rc);
				}
				fclose(f);
				MD5_hex((BYTE *)buf, digest);
				conn_send(buf, strlen(buf), 0);
			}
			conn_send("\n", 1, 0);
		}
		free(buf);
		conn_send("\x1b\\", 2, 0);
		globfree(&gl);
	}
	else if (strncmp(strbuf, "SyncTERM:C;SetFont;", 19) == 0) {
		slot = strtoul(strbuf+19, &p, 10);
		if (slot < CONIO_FIRST_FREE_FONT)
			return;
		if (slot > 255)
			return;
		if (*p != ';')
			return;
		p++;
		strcat(fn, p);
		if (!clean_path(fn, sizeof(fn)))
			return;
		if (!fexist(fn))
			return;
		sz = flength(fn);
		f = fopen(fn, "rb");
		if (f) {
			buf = malloc(sz);
			if (buf == NULL) {
				fclose(f);
				return;
			}
			if (fread(buf, sz, 1, f) != 1) {
				fclose(f);
				free(buf);
				return;
			}
			switch(sz) {
				case 4096:
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_sixteen);
					conio_fontdata[cterm->font_slot].eight_by_sixteen=buf;
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
					conio_fontdata[cterm->font_slot].desc=strdup("Cached Font");
					break;
				case 3584:
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_fourteen);
					conio_fontdata[cterm->font_slot].eight_by_fourteen=buf;
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
					conio_fontdata[cterm->font_slot].desc=strdup("Cached Font");
					break;
				case 2048:
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_eight);
					conio_fontdata[cterm->font_slot].eight_by_eight=buf;
					FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
					conio_fontdata[cterm->font_slot].desc=strdup("Cached Font");
					break;
				default:
					free(buf);
			}
			fclose(f);
		}
	}
}

BOOL doterm(struct bbslist *bbs)
{
	unsigned char ch[2];
	unsigned char outbuf[OUTBUF_SIZE];
	size_t outbuf_size=0;
	int	key;
	int i,j;
	unsigned char *p,*p2;
	struct vmem_cell *vc;
	BYTE zrqinit[] = { ZDLE, ZHEX, '0', '0', 0 };	/* for Zmodem auto-downloads */
	BYTE zrinit[] = { ZDLE, ZHEX, '0', '1', 0 };	/* for Zmodem auto-uploads */
	BYTE zrqbuf[sizeof(zrqinit)];
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
	recv_byte_buffer_len=recv_byte_buffer_pos=0;

	gettextinfo(&txtinfo);
	if(bbs->conn_type == CONN_TYPE_SERIAL)
		speed = 0;
	else
		speed = bbs->bpsrate;
	log_level = bbs->xfer_loglevel;
	conn_api.log_level = bbs->telnet_loglevel;
	setup_mouse_events();
	vc=realloc(scrollback_buf, term.width*sizeof(*vc)*settings.backlines);
	if(vc != NULL) {
		scrollback_buf=vc;
		memset(scrollback_buf,0,term.width*sizeof(*vc)*settings.backlines);
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
	cterm=cterm_init(term.height,term.width,term.x-1,term.y-1,settings.backlines,scrollback_buf, emulation);
	if(!cterm) {
		FREE_AND_NULL(cterm);
		return FALSE;
	}
	cterm->apc_handler = apc_handler;
	cterm->apc_handler_data = bbs;
	scrollback_cols=term.width;
	cterm->music_enable=bbs->music;
	ch[1]=0;
	zrqbuf[0]=0;
#ifndef WITHOUT_OOII
	ooii_buf[0]=0;
#endif

	/* Main input loop */
	oldmc=hold_update;
	showmouse();
	for(;!quitting;) {
		hold_update=TRUE;
		sleep=TRUE;
		if(!term.nostatus)
			update_status(bbs, (bbs->conn_type == CONN_TYPE_SERIAL)?bbs->bpsrate:speed, ooii_mode);
		for(remain=count_data_waiting() /* Hack for connection check */ + (!is_connected(NULL)); remain; remain--) {
			if(speed)
				thischar=xp_timer();

			if((!speed) || thischar < lastchar /* Wrapped */ || thischar >= nextchar) {
				/* Get remote input */
				inch=recv_byte(NULL, 0);

				switch(inch) {
					case -1:
						if(!is_connected(NULL)) {
							WRITE_OUTBUF();
							hold_update=oldmc;
							uifcmsg("Disconnected","`Disconnected`\n\nRemote host dropped connection");
							check_exit(FALSE);
							cterm_clearscreen(cterm, cterm->attr);	/* Clear screen into scrollback */
							scrollback_lines=cterm->backpos;
							cterm_end(cterm);
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

						j=strlen((char *)zrqbuf);
						if(inch == zrqinit[j] || inch == zrinit[j]) {
							zrqbuf[j]=inch;
							zrqbuf[++j]=0;
							if(j==sizeof(zrqinit)-1) {	/* Have full sequence (Assumes zrinit and zrqinit are same length */
								WRITE_OUTBUF();
								if(!strcmp((char *)zrqbuf, (char *)zrqinit))
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
								j=strlen((char *)ooii_buf);
								if(j+1 >= sizeof(ooii_buf))
									j--;
								ooii_buf[j++]=inch;
								ooii_buf[j]=0;
								if(inch == '|') {
									WRITE_OUTBUF();
									if(handle_ooii_code(ooii_buf, &ooii_mode, (unsigned char *)ansi_replybuf, sizeof(ansi_replybuf))) {
										ooii_mode=0;
										xptone_close();
									}
									if(ansi_replybuf[0])
										conn_send(ansi_replybuf,strlen((char *)ansi_replybuf),0);
									ooii_buf[0]=0;
								}
								continue;
							}
						}
						else {
							j=strlen((char *)ooii_buf);
							if(inch==ooii_init1[j]) {
								ooii_buf[j++]=inch;
								ooii_buf[j]=0;
								if(ooii_init1[j]==0) {
									if(strcmp((char *)ooii_buf, (char *)ooii_init1)==0) {
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
									if(strcmp((char *)ooii_buf, (char *)ooii_init2)==0) {
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
						if(outbuf_size >= sizeof(outbuf))
							WRITE_OUTBUF();
						outbuf[outbuf_size++]=inch;
						continue;
				}
			}
			else {
				if (speed)
					sleep=FALSE;
				break;
			}
		}
		WRITE_OUTBUF();
		if(updated) {
			hold_update=FALSE;
			gotoxy(wherex(), wherey());
		}
		hold_update=oldmc;

		/* Get local input */
		while(quitting || kbhit()) {
			struct mouse_event mevent;

			updated=TRUE;
			gotoxy(wherex(), wherey());
			if (quitting)
				key = CIO_KEY_QUIT;
			else {
				key=getch();
				if(key==0 || key==0xe0) {
					key|=getch()<<8;
					if(cterm->doorway_mode && ((key & 0xff) == 0) && key != 0x2c00 /* ALT-Z */) {
						ch[0]=0;
						ch[1]=key>>8;
						conn_send(ch,2,0);
						key=0;
						continue;
					}
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
							p=(unsigned char *)getcliptext();
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
						struct ciolib_screen *savscrn;
						savscrn = savescreen();
						show_bbslist(bbs->name, TRUE);
						uifcbail();
						setup_mouse_events();
						restorescreen(savscrn);
						freescreen(savscrn);
						if(cterm->scrollback != scrollback_buf || cterm->backlines != settings.backlines) {
							cterm->scrollback = scrollback_buf;
							cterm->backlines = settings.backlines;
							if(cterm->backpos>cterm->backlines)
								cterm->backpos=cterm->backlines;
						}
						showmouse();
						_setcursortype(_NORMALCURSOR);
					}
					break;
				case 0x2100:	/* ALT-F */
					font_control(bbs);
					showmouse();
					key = 0;
					break;
				case 0x2600:	/* ALT-L */
					if(bbs->conn_type != CONN_TYPE_RLOGIN && bbs->conn_type != CONN_TYPE_RLOGIN_REVERSED && bbs->conn_type != CONN_TYPE_SSH) {
						if(bbs->user[0]) {
							conn_send(bbs->user,strlen(bbs->user),0);
							conn_send(cterm->emulation==CTERM_EMULATION_ATASCII?"\x9b":"\r",1,0);
							SLEEP(10);
						}
						if(bbs->password[0]) {
							conn_send(bbs->password,strlen(bbs->password),0);
							conn_send(cterm->emulation==CTERM_EMULATION_ATASCII?"\x9b":"\r",1,0);
							SLEEP(10);
						}
					}
					if(bbs->syspass[0]) {
						conn_send(bbs->syspass,strlen(bbs->syspass),0);
						conn_send(cterm->emulation==CTERM_EMULATION_ATASCII?"\x9b":"\r",1,0);
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
				case CIO_KEY_QUIT:
					if(!check_exit(TRUE))
						break;
					// Fallthrough
				case 0x2300:	/* Alt-H - Hangup */
					{
						struct ciolib_screen *savscrn;
						savscrn = savescreen();
						if(quitting || confirm("Disconnect... Are you sure?", "Selecting Yes closes the connection\n")) {
							freescreen(savscrn);
							setup_mouse_events();
							cterm_clearscreen(cterm,cterm->attr);	/* Clear screen into scrollback */
							scrollback_lines=cterm->backpos;
							cterm_end(cterm);
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(key==0x2d00 /* Alt-X? */ || key == CIO_KEY_QUIT);
						}
						restorescreen(savscrn);
						freescreen(savscrn);
						setup_mouse_events();
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
							cterm_clearscreen(cterm, cterm->attr);	/* Clear screen into scrollback */
							scrollback_lines=cterm->backpos;
							cterm_end(cterm);
							conn_close();
							hidemouse();
							hold_update=oldmc;
							return(FALSE);
						case 3:
							begin_upload(bbs, FALSE, inch);
							break;
						case 4:
							begin_download(bbs);
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
							cterm->doorway_mode=!cterm->doorway_mode;
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
							cterm_clearscreen(cterm, cterm->attr);	/* Clear screen into scrollback */
							scrollback_lines=cterm->backpos;
							cterm_end(cterm);
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
								struct ciolib_screen *savscrn;

								savscrn = savescreen();
								show_bbslist(bbs->name, TRUE);
								restorescreen(savscrn);
								freescreen(savscrn);
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
			if(key && cterm->emulation == CTERM_EMULATION_ATASCII) {
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
			else if(key && cterm->emulation == CTERM_EMULATION_PETASCII) {
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
 */
	return(FALSE);
}
