/* Synchronet BBS Node control program */
// vi: tabstop=4

/* $Id: node.c,v 1.34 2020/08/01 22:04:03 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* Platform-specific headers */
#ifdef _WIN32
	#include <io.h>			/* open/close */
	#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>		/* isdigit */

/* Synchronet-specific */
#include "sbbsdefs.h"
#include "genwrap.h"	/* stricmp */
#include "filewrap.h"	/* lock/unlock/sopen */

enum {
	 MODE_LIST
	,MODE_ANON
	,MODE_LOCK
	,MODE_INTR
	,MODE_RRUN
	,MODE_DOWN
	,MODE_EVENT
	,MODE_NOPAGE
	,MODE_NOALERTS
	,MODE_STATUS
	,MODE_USERON
	,MODE_ACTION
	,MODE_ERRORS
	,MODE_MISC
	,MODE_CONN
	,MODE_AUX
	,MODE_EXTAUX
	};

char tmp[256];
int nodefile;
int nodeexb;

#if defined(_WIN32)	/* Microsoft-supplied cls() routine - ugh! */

/* Standard error macro for reporting API errors */
#define PERR(bSuccess, api){if(!(bSuccess)) printf("%s:Error %d from %s \
    on line %d\n", __FILE__, GetLastError(), api, __LINE__);}

void cls(void)
{
    COORD coordScreen = { 0, 0 };    /* here's where we'll home the
                                        cursor */
    BOOL bSuccess;
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */
    DWORD dwConSize;                 /* number of character cells in
                                        the current buffer */
	HANDLE hConsole;

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    /* get the number of character cells in the current buffer */

    bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );
    PERR( bSuccess, "GetConsoleScreenBufferInfo" );
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    /* fill the entire screen with blanks */

    bSuccess = FillConsoleOutputCharacter( hConsole, (TCHAR) ' ',
       dwConSize, coordScreen, &cCharsWritten );
    PERR( bSuccess, "FillConsoleOutputCharacter" );

    /* get the current text attribute */

    bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );
    PERR( bSuccess, "ConsoleScreenBufferInfo" );

    /* now set the buffer's attributes accordingly */

    bSuccess = FillConsoleOutputAttribute( hConsole, csbi.wAttributes,
       dwConSize, coordScreen, &cCharsWritten );
    PERR( bSuccess, "FillConsoleOutputAttribute" );

    /* put the cursor at (0, 0) */

    bSuccess = SetConsoleCursorPosition( hConsole, coordScreen );
    PERR( bSuccess, "SetConsoleCursorPosition" );
    return;
}
#else /* !_WIN32 */

#define cls()

#endif

#if !defined _MSC_VER && !defined __BORLANDC__
char* itoa(int val, char* str, int radix)
{
	switch(radix) {
		case 8:
			sprintf(str,"%o",val);
			break;
		case 10:
			sprintf(str,"%u",val);
			break;
		case 16:
			sprintf(str,"%x",val);
			break;
		default:
			sprintf(str,"bad radix: %d",radix);
			break;
	}
	return(str);
}
#endif

/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from NODE.DAB															*/
/* if lockit is non-zero, locks this node's record. putnodedat() unlocks it */
/****************************************************************************/
void getnodedat(int number, node_t *node, int lockit)
{
	int count;

	number--;	/* make zero based */
	for(count=0;count<LOOP_NODEDAB;count++) {
		if(count)
			SLEEP(100);
		lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
		if(lockit
			&& lock(nodefile,(long)number*sizeof(node_t),sizeof(node_t))==-1) 
			continue; 
		if(read(nodefile,node,sizeof(node_t))==sizeof(node_t))
			break;
	}
	if(count>=(LOOP_NODEDAB/2))
		printf("NODE.DAB (node %d) COLLISION (READ) - Count: %d\n"
			,number+1, count);
	else if(count==LOOP_NODEDAB) {
		printf("!Error reading nodefile for node %d\n",number+1);
	}
}

/****************************************************************************/
/* Write the data from the structure 'node' into NODE.DAB  					*/
/* getnodedat(num,&node,1); must have been called before calling this func  */
/*          NOTE: ------^   the indicates the node record has been locked   */
/****************************************************************************/
void putnodedat(int number, node_t node)
{
	number--;	/* make zero based */
	lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
	if(write(nodefile,&node,sizeof(node_t))!=sizeof(node_t)) {
		unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
		printf("Error writing to nodefile for node %d\n",number+1);
		return; }
	unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
}

/****************************************************************************/
/* Unpacks the password 'pass' from the 5bit ASCII inside node_t. 32bits in */
/* node.extaux, and the other 8bits in the upper byte of node.aux			*/
/****************************************************************************/
char *unpackchatpass(char *pass, node_t node)
{
	char 	bits;
	int 	i;

	pass[0]=(node.aux&0x1f00)>>8;
	pass[1]=(char)(((node.aux&0xe000)>>13)|((node.extaux&0x3)<<3));
	bits=2;
	for(i=2;i<8;i++) {
		pass[i]=(char)((node.extaux>>bits)&0x1f);
		bits+=5; }
	pass[8]=0;
	for(i=0;i<8;i++)
		if(pass[i])
			pass[i]+=64;
	return(pass);
}

static char* node_connection_desc(ushort conn, char* str)
{
	switch(conn) {
		case NODE_CONNECTION_LOCAL:
			strcpy(str,"Locally");
			break;
		case NODE_CONNECTION_TELNET:
			strcpy(str,"via telnet");
			break;
		case NODE_CONNECTION_RLOGIN:
			strcpy(str,"via rlogin");
			break;
		case NODE_CONNECTION_SSH:
			strcpy(str,"via ssh");
			break;
		case NODE_CONNECTION_RAW:
			strcpy(str,"via raw");
			break;
		default:
			sprintf(str,"at %ubps",conn);
			break;
	}

	return str;
}

static char* extended_status(int num, char* str)
{
	if(nodeexb < 0)
		return "No extended status file open";
	lseek(nodeexb, num * 128, SEEK_SET);
	read(nodeexb, str, 128);
	str[127] = 0;
	return str;
}

/****************************************************************************/
/* Displays the information for node number 'number' contained in 'node'    */
/****************************************************************************/
void printnodedat(int number, node_t node)
{
    char hour,mer[3];
	char tmp[128];

	printf("Node %2d: ",number);
	switch(node.status) {
		case NODE_WFC:
			printf("Waiting for connection");
			break;
		case NODE_OFFLINE:
			printf("Offline");
			break;
		case NODE_NETTING:
			printf("Networking");
			break;
		case NODE_LOGON:
			printf("At login prompt");
			break;
		case NODE_LOGOUT:
			printf("User #%d logging out", node.useron);
			break;
		case NODE_EVENT_WAITING:
			printf("Waiting for all nodes to become inactive");
			break;
		case NODE_EVENT_LIMBO:
			printf("Waiting for node %d to finish external event",node.aux);
			break;
		case NODE_EVENT_RUNNING:
			printf("Running external event");
			break;
		case NODE_NEWUSER:
			printf("New user");
			printf(" applying for access ");
			printf("%s",node_connection_desc(node.connection,tmp));
			break;
		case NODE_QUIET:
		case NODE_INUSE:
			if(node.misc&NODE_EXT) {
				printf("%s", extended_status(number - 1, tmp));
				break;
			}
			printf("User #%d",node.useron);
			printf(" ");
			switch(node.action) {
				case NODE_MAIN:
					printf("at main menu");
					break;
				case NODE_RMSG:
					printf("reading messages");
					break;
				case NODE_RMAL:
					printf("reading mail");
					break;
				case NODE_RSML:
					printf("reading sent mail");
					break;
				case NODE_RTXT:
					printf("reading text files");
					break;
				case NODE_PMSG:
					printf("posting message");
					break;
				case NODE_SMAL:
					printf("sending mail");
					break;
				case NODE_AMSG:
					printf("posting auto-message");
					break;
				case NODE_XTRN:
					if(!node.aux)
						printf("at external program menu");
					else
						printf("running external program #%d",node.aux);
					break;
				case NODE_DFLT:
					printf("changing defaults");
					break;
				case NODE_XFER:
					printf("at transfer menu");
					break;
				case NODE_RFSD:
					printf("retrieving from device #%d",node.aux);
					break;
				case NODE_DLNG:
					printf("downloading");
					break;
				case NODE_ULNG:
					printf("uploading");
					break;
				case NODE_BXFR:
					printf("transferring bidirectional");
					break;
				case NODE_LFIL:
					printf("listing files");
					break;
				case NODE_LOGN:
					printf("logging on");
					break;
				case NODE_LCHT:
					printf("in local chat with sysop (deprecated)");
					break;
				case NODE_MCHT:
					if(node.aux) {
						printf("in multinode chat channel %d",node.aux&0xff);
						if(node.aux&0x1f00) { /* password */
							putchar('*');
							printf(" %s",unpackchatpass(tmp,node)); } }
					else
						printf("in multinode global chat channel");
					break;
				case NODE_PAGE:
					printf("paging node %u for private chat",node.aux);
					break;
				case NODE_PCHT:
					if(node.aux==0)
						printf("in local chat with sysop");
					else
						printf("in private chat with node %u",node.aux);
					break;
				case NODE_GCHT:
					printf("chatting with The Guru");
					break;
				case NODE_CHAT:
					printf("in chat section");
					break;
				case NODE_TQWK:
					printf("transferring QWK packet");
					break;
				case NODE_SYSP:
					printf("performing sysop activities");
					break;
				default:
					fputs(itoa(node.action,tmp,10),stdout);
					break;  }
			printf(" %s",node_connection_desc(node.connection,tmp));
			if(node.action==NODE_DLNG) {
				if((node.aux/60)>=12) {
					if(node.aux/60==12)
						hour=12;
					else
						hour=(node.aux/60)-12;
					strcpy(mer,"pm"); }
				else {
					if((node.aux/60)==0)    /* 12 midnite */
						hour=12;
					else hour=node.aux/60;
					strcpy(mer,"am"); }
				printf(" ETA %02d:%02d %s"
					,hour,node.aux-((node.aux/60)*60),mer); }
			break; }
	if(node.misc&(NODE_LOCK|NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG)) {
		printf(" (");
		if(node.misc&NODE_AOFF)
			putchar('A');
		if(node.misc&NODE_LOCK)
			putchar('L');
		if(node.misc&(NODE_MSGW|NODE_NMSG))
			putchar('M');
		if(node.misc&NODE_POFF)
			putchar('P');
		putchar(')'); }
	if(((node.misc
		&(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN))
		|| node.status==NODE_QUIET)) {
		printf(" [");
		if(node.misc&NODE_ANON)
			putchar('A');
		if(node.misc&NODE_INTR)
			putchar('I');
		if(node.misc&NODE_RRUN)
			putchar('R');
		if(node.misc&NODE_UDAT)
			putchar('U');
		if(node.status==NODE_QUIET)
			putchar('Q');
		if(node.misc&NODE_EVENT)
			putchar('E');
		if(node.misc&NODE_DOWN)
			putchar('D');
		if(node.misc&NODE_LCHAT)
			putchar('C');
		if(node.misc&NODE_FCHAT)
			putchar('F');
		putchar(']'); }
	if(node.errors)
		printf(" %d error%c",node.errors, node.errors>1 ? 's' : '\0' );
	printf("\n");
}


/****************************/
/* Main program entry point */
/****************************/
int main(int argc, char **argv)
{
	char str[256],ctrl_dir[41],*p,debug=0;
	int sys_nodes,node_num=0,onoff=0;
	int i,j,mode=0,misc;
	int	modify=0;
	int loop=0;
	int pause=0;
	long value=0;
	node_t node;

	char		revision[16];

	sscanf("$Revision: 1.34 $", "%*s %s", revision);

	printf("\nSynchronet Node Display/Control Utility v%s\n\n", revision);

	if(sizeof(node_t)!=SIZEOF_NODE_T) {
		printf("COMPILER ERROR: sizeof(node_t)=%" XP_PRIsize_t "u instead of %d\n"
			,sizeof(node_t),SIZEOF_NODE_T);
		return(-1);
	}

	if(argc<2) {
		printf("usage: node [-debug] [action [on|off]] [node numbers] [...]"
			"\n\n");
		printf("actions (default is list):\n\n");
		printf("list        = list status\n");
		printf("anon        = anonymous user\n");
		printf("lock        = locked\n");
		printf("intr        = interrupt\n");
		printf("down        = shut-down\n");
		printf("rerun       = rerun\n");
		printf("event       = run event\n");
		printf("nopage      = page disable\n");
		printf("noalerts    = activity alerts disable\n");
		printf("status=#    = set status value\n");
		printf("              %d = Waiting for connection\n", NODE_WFC);
		printf("              %d = Offline\n", NODE_OFFLINE);
		printf("useron=#    = set useron number\n");
		printf("action=#    = set action value\n");
		printf("errors=#    = set error counter\n");
		printf("conn=#      = set connection value\n");
		printf("misc=#      = set misc value\n");
		printf("aux=#       = set aux value\n");
		printf("extaux=#    = set extended aux value\n");
		exit(0); }

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\7\nSBBSCTRL environment variable is not set.\n");
		printf("This environment variable must be set to your CTRL directory.");
		printf("\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1); }
	sprintf(ctrl_dir,"%.40s",p);
	if(ctrl_dir[strlen(ctrl_dir)-1]!='\\'
		&& ctrl_dir[strlen(ctrl_dir)-1]!='/')
		strcat(ctrl_dir,"/");

	sprintf(str,"%snode.dab",ctrl_dir);
	if((nodefile=sopen(str,O_RDWR|O_BINARY,SH_DENYNO))==-1) {
		printf("\7\nError %d opening %s.\n",errno,str);
		exit(1); }

	sprintf(str,"%snode.exb",ctrl_dir);
	nodeexb=sopen(str,O_RDWR|O_BINARY,SH_DENYNO);

	sys_nodes=(int)(filelength(nodefile)/sizeof(node_t));
	if(!sys_nodes) {
		printf("%s reflects 0 nodes!\n",str);
		exit(1); }

	for(i=1;i<argc;i++) {
		if(isdigit(argv[i][0]))
			node_num=atoi(argv[i]);
		else {
			node_num=onoff=value=0;
			if(!stricmp(argv[i],"-DEBUG"))
				debug=1;
			if(!stricmp(argv[i],"-LOOP"))
				loop=1;
			if(!stricmp(argv[i],"-PAUSE"))
				pause=1;

			else if(!stricmp(argv[i],"LOCK"))
				mode=MODE_LOCK;
			else if(!stricmp(argv[i],"ANON"))
				mode=MODE_ANON;
			else if(!stricmp(argv[i],"INTR"))
				mode=MODE_INTR;
			else if(!stricmp(argv[i],"DOWN"))
				mode=MODE_DOWN;
			else if(!stricmp(argv[i],"RERUN"))
				mode=MODE_RRUN;
			else if(!stricmp(argv[i],"EVENT"))
				mode=MODE_EVENT;
			else if(!stricmp(argv[i],"NOPAGE"))
				mode=MODE_NOPAGE;
			else if(!stricmp(argv[i],"NOALERTS"))
				mode=MODE_NOALERTS;
			else if(!stricmp(argv[i],"ON"))
				onoff=1;
			else if(!stricmp(argv[i],"OFF"))
				onoff=2;
			else if(!strnicmp(argv[i],"STATUS=",7)) {
				mode=MODE_STATUS;
				value=strtoul(argv[i]+7, NULL, 0); }
			else if(!strnicmp(argv[i],"ERRORS=",7)) {
				mode=MODE_ERRORS;
				value=strtoul(argv[i]+7, NULL, 0); }
			else if(!strnicmp(argv[i],"USERON=",7)) {
				mode=MODE_USERON;
				value=strtoul(argv[i]+7, NULL, 0); }
			else if(!strnicmp(argv[i],"ACTION=",7)) {
				mode=MODE_ACTION;
				value=strtoul(argv[i]+7, NULL, 0); }
			else if(!strnicmp(argv[i],"CONN=",5)) {
				mode=MODE_CONN;
				value=strtoul(argv[i]+5, NULL, 0); }
			else if(!strnicmp(argv[i],"MISC=",5)) {
				mode=MODE_MISC;
				value=strtoul(argv[i]+5, NULL, 0); }
			else if(!strnicmp(argv[i],"AUX=",4)) {
				mode=MODE_AUX;
				value=strtoul(argv[i]+4, NULL, 0); }
			else if(!strnicmp(argv[i],"EXTAUX=",7)) {
				mode=MODE_EXTAUX;
				value=strtoul(argv[i]+7, NULL, 0); }
			}
		if(mode!=MODE_LIST)
			modify=1;

		if((mode && node_num) || i+1==argc)
			while(1) {
				for(j=1;j<=sys_nodes;j++)
					if(!node_num || j==node_num) {
						getnodedat(j,&node,modify);
						misc=0;
						switch(mode) {
							case MODE_ANON:
								misc=NODE_ANON;
								break;
							case MODE_LOCK:
								misc=NODE_LOCK;
								break;
							case MODE_INTR:
								misc=NODE_INTR;
								break;
							case MODE_DOWN:
					            if(node.status==NODE_WFC)
            						node.status=NODE_OFFLINE;
								else
									misc=NODE_DOWN;
								break;
							case MODE_RRUN:
								misc=NODE_RRUN;
								break;
							case MODE_EVENT:
								misc=NODE_EVENT;
								break;
							case MODE_NOPAGE:
								misc=NODE_POFF;
								break;
							case MODE_NOALERTS:
								misc=NODE_AOFF;
								break;
							case MODE_STATUS:
								node.status=(uchar)value;
								break;
							case MODE_ERRORS:
								node.errors=(uchar)value;
								break;
							case MODE_ACTION:
								node.action=(uchar)value;
								break;
							case MODE_USERON:
								node.useron=(uint16_t)value;
								break;
							case MODE_MISC:
								node.misc=(uint16_t)value;
								break;
							case MODE_CONN:
								node.connection=(uint16_t)value;
								break;
							case MODE_AUX:
								node.aux=(uint16_t)value;
								break;
							case MODE_EXTAUX:
								node.extaux=value;
								break; }
						if(misc) {
							if(onoff==0)
								node.misc^=misc;
							else if(onoff==1)
								node.misc|=misc;
							else if(onoff==2)
								node.misc&=~misc; }
						if(modify)
							putnodedat(j,node);
						printnodedat(j,node);
						if(debug) {
							printf("status=%u\n",node.status);
							printf("errors=%u\n",node.errors);
							printf("action=%d\n",node.action);
							printf("useron=%u\n",node.useron);
							printf("conn=%u\n",node.connection);
							printf("misc=%u\n",node.misc);
							printf("aux=%u\n",node.aux);
							printf("extaux=%"PRIu32"\n",node.extaux); 
						}  /* debug */

						if(pause) {
							printf("Hit enter...");
							getchar();
							printf("\n");
						}

					} /* if(!node_num) */

				if(!loop)
					break;
				SLEEP(1000);
				cls();
			} /* while(1) */

	} /* for i<argc */

	close(nodefile);
	return(0);
}
