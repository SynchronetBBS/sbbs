#include <genwrap.h>
#include <ciolib.h>
#include <cterm.h>
#include <mouse.h>
#include <keys.h>

#include "conn.h"
#include "term.h"
#include "uifcinit.h"
#include "menu.h"
#include "dirwrap.h"
#include "zmodem.h"
#include "crc32.h"

#define	BUFSIZE	2048

#define DUMP

int backlines=2000;

struct terminal term;

#define TRANSFER_WIN_WIDTH	66
#define TRANSFER_WIN_HEIGHT	18
static char winbuf[(TRANSFER_WIN_WIDTH + 2) * (TRANSFER_WIN_HEIGHT + 1) * 2];	/* Save buffer for transfer window */
static struct text_info	trans_ti;
static struct text_info	log_ti;

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
	screen=(unsigned char*)malloc(sbufsize);
	sbuffer=(unsigned char*)malloc(sbufsize);
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
						copybuf=malloc(endpos-startpos+4+lines*2);
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
						free(copybuf);
						free(screen);
						free(sbuffer);
						return;
				}
				break;
			default:
				puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
				ungetch(key);
				free(screen);
				free(sbuffer);
				return;
		}
	}
}

void update_status(struct bbslist *bbs)
{
	char buf[160];
	int oldscroll;
	int olddmc;
	struct	text_info txtinfo;
	int now;
	static int lastupd=0;
	int	timeon;

	now=time(NULL);
	if(now==lastupd)
		return;
	lastupd=now;
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
	switch(cio_api.mode) {
		case CIOLIB_MODE_CURSES:
		case CIOLIB_MODE_CURSES_IBM:
		case CIOLIB_MODE_ANSI:
			if(timeon>359999)
				cprintf(" %-29.29s \263 %-6.6s \263 Connected: Too Long \263 CTRL-S for menu ",bbs->name,conn_types[bbs->conn_type]);
			else
				cprintf(" %-29.29s \263 %-6.6s \263 Connected: %02d:%02d:%02d \263 CTRL-S for menu ",bbs->name,conn_types[bbs->conn_type],timeon/3600,(timeon/60)%60,timeon%60);
			break;
		default:
			if(timeon>359999)
				cprintf(" %-30.30s \263 %-6.6s \263 Connected: Too Long \263 ALT-M for menu ",bbs->name,conn_types[bbs->conn_type]);
			else
				cprintf(" %-30.30s \263 %-6.6s \263 Connected: %02d:%02d:%02d \263 ALT-M for menu ",bbs->name,conn_types[bbs->conn_type],timeon/3600,(timeon-(timeon/3600))/60,timeon%60);
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
static int log_level = LOG_INFO;

static void zmodem_check_abort(zmodem_t* zm)
{
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
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static int lputs(void* cbdata, int level, const char* str)
{
	char msg[512];
	int chars;

	zmodem_check_abort((zmodem_t*)cbdata);

#if defined(_WIN32) && defined(_DEBUG) && TRUE
	sprintf(msg,"SyncTerm: %s",str);
	OutputDebugString(msg);
#endif
	if(level > log_level)
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

	zmodem_check_abort(zm);
	
	old_hold = hold_update;
	hold_update = TRUE;
	window(((trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2)+2
			, ((trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2)+1
			, ((trans_ti.screenwidth-TRANSFER_WIN_WIDTH)/2) + TRANSFER_WIN_WIDTH - 2
			, ((trans_ti.screenheight-TRANSFER_WIN_HEIGHT)/2)+5);
	gotoxy(1,1);
	textattr(LIGHTCYAN | (BLUE<<4));
	now=time(NULL);
	if(now-last_progress>0 || current_pos >= zm->current_file_size) {
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
			,zm->receive_32bit_data ? 32 : 16
			,cps
			);
		clreol();
		cputs("\r\n");
		cprintf("%*s%3d%%\r\n", TRANSFER_WIN_WIDTH/2-5, ""
			,(long)(((float)current_pos/(float)zm->current_file_size)*100.0));
		l = 60*((float)current_pos/(float)zm->current_file_size);
		cprintf("[%*.*s%*s]", l, l, 
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				"\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1"
				, 60-l, "");
		last_progress=now;
	}
	hold_update = FALSE;
	gotoxy(wherex(), wherey());
	hold_update = old_hold;
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static int send_byte(void* unused, uchar ch, unsigned timeout)
{
	return conn_send(&ch,sizeof(char),timeout*1000);
}

static	ulong	bufbot;
static	ulong	buftop;

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static int recv_byte(void* unused, unsigned timeout)
{
	BYTE	ch;
	int		i;
	time_t start=time(NULL);
	static	BYTE	buf[2048];

	if(bufbot==buftop) {
		if((i=conn_recv(buf,sizeof(buf),timeout*1000))<1) {
			if(timeout)
				lprintf(LOG_ERR,"RECEIVE ERROR %d (after %u seconds)"
					,i, time(NULL)-start);
			return(-1);
		}
		buftop=i;
		bufbot=0;
	}
	ch=buf[bufbot++];
//	lprintf(LOG_DEBUG,"RX: %02X", ch);
	return(ch);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
static BOOL is_connected(void* unused)
{
	return socket_check(conn_socket,NULL,NULL,0);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif
BOOL data_waiting(void* unused, unsigned timeout)
{
	BOOL rd;

	if(!socket_check(conn_socket,&rd,NULL,timeout))
		return(FALSE);
	return(rd);
}

void draw_recv_window(void)
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
		outline[i] = 0xcd;	/* Double horizontal line */
	}
	outline[0]=0xc9;
	outline[sizeof(outline)-2]=0xbb;
	puttext(left, top, left + TRANSFER_WIN_WIDTH - 1, top, outline);

	for(i=2;i < sizeof(outline) - 2; i+=2) {
		outline[i] = 0xc4;	/* Single horizontal line */
	}
	outline[0] = 0xc7;	// 0xcc
	outline[sizeof(outline)-2]=0xb6;	// 0xb6
	puttext(left, top+6, left + TRANSFER_WIN_WIDTH - 1, top+6, outline);

	for(i=2;i < sizeof(outline) - 2; i+=2) {
		outline[i] = 0xcd;	/* Double horizontal line */
	}
	outline[0]=0xc8;
	outline[sizeof(outline)-2]=0xbc;
	puttext(left, top + TRANSFER_WIN_HEIGHT - 1, left+TRANSFER_WIN_WIDTH - 1, top + TRANSFER_WIN_HEIGHT - 1, outline);
	outline[0]=0xba;
	outline[sizeof(outline)-2]=0xba;
	for(i=2;i < sizeof(outline) - 2; i+=2) {
		outline[i] = ' ';
	}
	for(i=1; i<6; i++) {
		puttext(left, top + i, left + TRANSFER_WIN_WIDTH - 1, top+i, outline);
	}
//	for(i=3;i < sizeof(outline) - 2; i+=2) {
//		outline[i] = LIGHTGRAY | (BLACK << 8);
//	}
	for(i=7; i<TRANSFER_WIN_HEIGHT-1; i++) {
		puttext(left, top + i, left + TRANSFER_WIN_WIDTH - 1, top+i, outline);
	}

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

void erase_recv_window(void) {
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

void zmodem_receive(void)
{
	zmodem_t	zm;
	char*		download_dir=".";
	int			files_received;
	ulong		bytes_received;

	bufbot=buftop=0;	/* purge our receive buffer */
	draw_recv_window();

	zmodem_init(&zm
		,/* cbdata */&zm
		,lputs, zmodem_progress
		,send_byte,recv_byte,is_connected,data_waiting);

	files_received=zmodem_recv_files(&zm,download_dir,&bytes_received);

	if(files_received>1)
		lprintf(LOG_INFO,"Received %u files (%lu bytes) successfully", files_received, bytes_received);

	lprintf(LOG_NOTICE,"Hit any key to continue...");
	getch();

	erase_recv_window();
}
/* End of Zmodem Stuff */

void doterm(struct bbslist *bbs)
{
	unsigned char ch[2];
	unsigned char buf[BUFSIZE];
	unsigned char prn[BUFSIZE];
	int	key;
	int i,j,k;
	unsigned char *scrollback;
	unsigned char *p;
	char zrqinit[] = { ZDLE, ZHEX, '0', '0', 0 };
	char zrqbuf[5];

	ciomouse_setevents(0);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_2_CLICK);
	scrollback=malloc(term.width*2*backlines);
	memset(scrollback,0,term.width*2*backlines);
	cterm_init(term.height,term.width,term.x-1,term.y-1,backlines,scrollback);
	ch[1]=0;
	zrqbuf[0]=0;

	/* Main input loop */
	for(;;) {
		/* Get remote input */
		i=conn_recv(buf,sizeof(buf),0);

		if(!term.nostatus)
			update_status(bbs);
		switch(i) {
			case -1:
				free(scrollback);
				cterm_end();
				conn_close();
				uifcmsg("Disconnected","`Disconnected`\n\nRemote host dropped connection");
				return;
			case 0:
				break;
			default:
#if defined(_WIN32) && defined(_DEBUG) && defined(DUMP)
				dump(buf,i);
#endif
				buf[i]=0;
				p=buf;
				if(!zrqbuf[0]) {
					p=memchr(buf, zrqinit[0], i);
					if(p!=NULL) {
						cterm_write(buf, p-buf, prn, sizeof(prn));
						if(prn[0])
							conn_send(prn,strlen(prn),0);
						zrqbuf[0]=*(p++);
						zrqbuf[1]=0;
					}
					else
						p=buf;
				}
				if(zrqbuf[0]) {	/* Already have the start of the sequence */
					j=strlen(zrqbuf);
					while(j<4 /* strlen(zrqinit) */ && p<buf+i) {
						if(*p==zrqinit[j]) {
							zrqbuf[j++]=zrqinit[j];
							zrqbuf[j]=0;
							p++;
						}
						else
							break;
					}
					if(j==4 /* strlen(zrqinit) */) {	/* Have full sequence */
						zmodem_receive();
						zrqbuf[0]=0;
					}
					else if(p<=buf+i-(4 /* strlen(zrqinit */ - j)) {	/* Not a real zrqinit */
						cterm_write(zrqbuf, j, prn, sizeof(prn));
						if(prn[0])
							conn_send(prn,strlen(prn),0);
						zrqbuf[0]=0;
					}
				}
				cterm_write(p,(buf+i)-p,prn,sizeof(prn));
				if(prn[0])
					conn_send(prn,strlen(prn),0);
				break;
		}

		/* Get local input */
		while(kbhit()) {
			struct mouse_event mevent;
			key=getch();
			if(key==0 || key==0xff)
				key|=getch()<<8;
			switch(key) {
				case CIO_KEY_MOUSE:
					getmouse(&mevent);
					switch(mevent.event) {
						case CIOLIB_BUTTON_1_DRAG_START:
							mousedrag(scrollback);
							break;
						case CIOLIB_BUTTON_2_CLICK:
						case CIOLIB_BUTTON_3_CLICK:
							p=getcliptext();
							if(p!=NULL) {
								conn_send(p,strlen(p),0);
								free(p);
							}
							break;
					}

					break;
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
				case CIO_KEY_F(1):
					conn_send("\033OP",3,0);
					break;
				case CIO_KEY_F(2):
					conn_send("\033OQ",3,0);
					break;
				case CIO_KEY_F(3):
					conn_send("\033Ow",3,0);
					break;
				case CIO_KEY_F(4):
					conn_send("\033Ox",3,0);
					break;
				case 0x1f00:	/* ALT-S */
					viewscroll();
					break;
				case 0x2600:	/* ALT-L */
					conn_send(bbs->user,strlen(bbs->user),0);
					conn_send("\r",1,0);
					SLEEP(10);
					conn_send(bbs->password,strlen(bbs->password),0);
					conn_send("\r",1,0);
				case 17:		/* CTRL-Q */
					if(cio_api.mode!=CIOLIB_MODE_CURSES
							&& cio_api.mode!=CIOLIB_MODE_CURSES_IBM
							&& cio_api.mode!=CIOLIB_MODE_ANSI) {
						ch[0]=key;
						conn_send(ch,1,0);
						break;
					}
					/* FALLTHROUGH for curses/ansi modes */
				case 0x2300:	/* ALT-H */
					{
						char *opts[3]={
										 "Yes"
										,"No"
										,""
									  };
						char *buf;
						struct	text_info txtinfo;

    					gettextinfo(&txtinfo);
						buf=(char *)malloc(txtinfo.screenheight*txtinfo.screenwidth*2);
						gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
						i=0;
						init_uifc();
						if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Disconnect... Are you sure?",opts)==0) {
							uifcbail();
							free(buf);
							cterm_end();
							free(scrollback);
							conn_close();
							return;
						}
						uifcbail();
						puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
						window(txtinfo.winleft,txtinfo.wintop,txtinfo.winright,txtinfo.winbottom);
						textattr(txtinfo.attribute);
						gotoxy(txtinfo.curx,txtinfo.cury);
						free(buf);
					}
					break;
				case 19:	/* CTRL-S */
					if(cio_api.mode!=CIOLIB_MODE_CURSES
							&& cio_api.mode!=CIOLIB_MODE_CURSES_IBM
							&& cio_api.mode!=CIOLIB_MODE_ANSI) {
						ch[0]=key;
						conn_send(ch,1,0);
						break;
					}
					/* FALLTHROUGH for curses/ansi modes */
				case 0x3200:		/* ALT-M */
					i=wherex();
					j=wherey();
					switch(syncmenu(bbs)) {
						case -1:
							cterm_end();
							free(scrollback);
							conn_close();
							return;
					}
					gotoxy(i,j);
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
		SLEEP(1);
	}
}
