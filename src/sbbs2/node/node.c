/* NODE.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Synchronet BBS Node control program */

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <dos.h>
#include <fcntl.h>
#include <string.h>
#include "sbbsdefs.h"

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

/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from NODE.DAB															*/
/* if lockit is non-zero, locks this node's record. putnodedat() unlocks it */
/****************************************************************************/
void getnodedat(uchar number, node_t *node, char lockit)
{
	char str[256];
	int count=0;

number--;	/* make zero based */
while(count<LOOP_NODEDAB) {
	lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
	if(lockit
		&& lock(nodefile,(long)number*sizeof(node_t),sizeof(node_t))==-1) {
		count++;
		continue; }
	if(read(nodefile,node,sizeof(node_t))==sizeof(node_t))
		break;
	count++; }
if(count)
	printf("NODE.DAB COLLISION (READ) - Count:%d\n",count);
if(count==LOOP_NODEDAB) {
	printf("Error reading nodefile for node %d\n",number+1);
	return; }
}

/****************************************************************************/
/* Write the data from the structure 'node' into NODE.DAB  					*/
/* getnodedat(num,&node,1); must have been called before calling this func  */
/*          NOTE: ------^   the indicates the node record has been locked   */
/****************************************************************************/
void putnodedat(uchar number, node_t node)
{
	char str[256];
	int count;

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
pass[1]=((node.aux&0xe000)>>13)|((node.extaux&0x3)<<3);
bits=2;
for(i=2;i<8;i++) {
	pass[i]=(node.extaux>>bits)&0x1f;
	bits+=5; }
pass[8]=0;
for(i=0;i<8;i++)
	if(pass[i])
		pass[i]+=64;
return(pass);
}

/****************************************************************************/
/* Displays the information for node number 'number' contained in 'node'    */
/****************************************************************************/
void printnodedat(uchar number, node_t node)
{
	uint i;
    char hour,mer[3];

printf("Node %2d: ",number);
switch(node.status) {
    case NODE_WFC:
		printf("Waiting for call");
        break;
    case NODE_OFFLINE:
		printf("Offline");
        break;
    case NODE_NETTING:
		printf("Networking");
        break;
    case NODE_LOGON:
		printf("At logon prompt");
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
        if(!node.connection)
			printf("Locally");
        else
			printf("at %ubps",node.connection);
        break;
	case NODE_QUIET:
    case NODE_INUSE:
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
				printf("in local chat with sysop");
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
				printf(itoa(node.action,tmp,10));
                break;  }
        if(!node.connection)
			printf(" locally");
        else
			printf(" at %ubps",node.connection);
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
	uchar sys_nodes,node_num=0,onoff=0;
	uint i,j,mode=0,misc;
	long value;
	node_t node;

printf("\nSynchronet Node Display/Control Utility v1.03\n\n");

if(argc<2) {
	printf("usage: node [/debug] [action [on|off]] [node numbers] [...]"
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
	printf("\nExample: SET SBBSCTRL=C:\\SBBS\\CTRL\n");
	exit(1); }
sprintf(ctrl_dir,"%.40s",p);
strupr(ctrl_dir);
if(ctrl_dir[strlen(ctrl_dir)-1]!='\\')
	strcat(ctrl_dir,"\\");

sprintf(str,"%sNODE.DAB",ctrl_dir);
if((nodefile=open(str,O_RDWR|O_DENYNONE|O_BINARY))==-1) {
	printf("\7\nError opening %s.\n",str);
	exit(1); }

sys_nodes=filelength(nodefile)/sizeof(node_t);
if(!sys_nodes) {
	printf("%s reflects 0 nodes!\n",str);
	exit(1); }

for(i=1;i<argc;i++) {
	if(isdigit(argv[i][0]))
		node_num=atoi(argv[i]);
	else {
		node_num=onoff=value=0;
		if(!stricmp(argv[i],"/DEBUG"))
			debug=1;
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
			value=atoi(argv[i]+7); }
		else if(!strnicmp(argv[i],"ERRORS=",7)) {
			mode=MODE_ERRORS;
			value=atoi(argv[i]+7); }
		else if(!strnicmp(argv[i],"USERON=",7)) {
			mode=MODE_USERON;
			value=atoi(argv[i]+7); }
		else if(!strnicmp(argv[i],"ACTION=",7)) {
			mode=MODE_ACTION;
            value=atoi(argv[i]+7); }
		else if(!strnicmp(argv[i],"CONN=",5)) {
			mode=MODE_CONN;
			value=atoi(argv[i]+5); }
		else if(!strnicmp(argv[i],"MISC=",5)) {
			mode=MODE_MISC;
			value=atoi(argv[i]+5); }
		else if(!strnicmp(argv[i],"AUX=",4)) {
			mode=MODE_AUX;
			value=atoi(argv[i]+4); }
		else if(!strnicmp(argv[i],"EXTAUX=",7)) {
			mode=MODE_EXTAUX;
			value=atoi(argv[i]+7); }
		}
	if((mode && node_num) || i+1==argc)
		for(j=1;j<=sys_nodes;j++)
			if(!node_num || j==node_num) {
				getnodedat(j,&node,1);
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
						node.status=value;
						break;
					case MODE_ERRORS:
						node.errors=value;
						break;
					case MODE_ACTION:
						node.action=value;
						break;
					case MODE_USERON:
						node.useron=value;
						break;
					case MODE_MISC:
						node.misc=value;
						break;
					case MODE_CONN:
						node.connection=value;
						break;
					case MODE_AUX:
						node.aux=value;
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
					printf("extaux=%lu\n",node.extaux); } } }

close(nodefile);
return(0);
}
