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

#define	BUFSIZE	2048

#define DUMP

int backlines=2000;

struct terminal term;

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

/* Zmodem Routines */
static int lputs(void* unused, int level, const char* str)
{
#if defined(_WIN32) && defined(_DEBUG)
	char msg[512];

//	if(log_level==LOG_DEBUG || level==LOG_DEBUG)
		sprintf(msg,"SyncTerm: %s",str);
		OutputDebugString(msg);
#endif
	return 0;
}

static int send_byte(void* unused, uchar ch, unsigned timeout)
{
	return conn_send(&ch,sizeof(char),timeout*1000);
}

static int recv_byte(void* unused, unsigned timeout)
{
	char str[128];
	uchar ch;

	if(conn_recv(&ch,sizeof(ch),timeout*1000)!=sizeof(ch)) {
		lputs(NULL,LOG_ERR,"RECEIVE TIMEOUT");
		return(-1);
	}
//	sprintf(str,"RX: %02X", ch);
//	lputs(NULL,LOG_DEBUG,str);
	return(ch);
}

static BOOL is_connected(void* unused)
{
	return socket_check(conn_socket,NULL,NULL,0);
}

void zmodem_receive(void)
{
	char		fpath[MAX_PATH+1];
	char*		fname;
	char		str[MAX_PATH*2];
	FILE*		fp;
	ulong		bytes;
	ulong		total_files;
	ulong		total_bytes;
	time_t		ftime;
	unsigned	errors;
	zmodem_t	zm;

	zmodem_init(&zm,NULL /* cbdata */,lputs,NULL /* zmodem_progress */
		,send_byte,recv_byte,is_connected);

	while(zmodem_recv_init(&zm
			,fpath,sizeof(fpath)
			,&bytes
			,&ftime
			,NULL
			,NULL
			,&total_files
			,&total_bytes)==ZFILE) {
		fname=getfname(fpath);
		SAFEPRINTF2(str,"Downloading %s via Zmodem (%lu bytes)", fname, bytes);
		uifc.pop(str);

		do {	/* try */
			SAFEPRINTF2(fpath,"%s/%s","."/* download_dir */,fname);
			if(fexist(fpath)) {
				/* lprintf(LOG_WARNING,"%s already exists",str); */
				zmodem_send_zskip(&zm);
				break;
			}

			if((fp=fopen(fpath,"wb"))==NULL) {
				/* lprintf(LOG_ERR,"Error %d creating %s",errno,fpath); */
				zmodem_send_zskip(&zm);
				break;
			}

			errors=zmodem_recv_file_data(&zm,fp,/* offset */0,bytes, /* start time */0);

			for(;errors<=zm.max_errors && !zm.cancelled; errors++) {
				if(zmodem_recv_header_and_check(&zm))
					break;
			}

			fclose(fp);

		} while(0);
		/* finally */

		uifc.pop(NULL);
	}
}

void doterm(struct bbslist *bbs)
{
	unsigned char ch[2];
	unsigned char buf[BUFSIZE];
	unsigned char prn[BUFSIZE];
	int	key;
	int i,j,k;
	unsigned char *scrollback;
	char *p;
	char zrqinit[] = { ZDLE, ZHEX, '0', '0', 0 };

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
				if(strstr(buf,zrqinit)!=NULL)	{ /* this should be more robust */
					zmodem_receive();
					break;
				}
				cterm_write(buf,i,prn,sizeof(prn));
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
