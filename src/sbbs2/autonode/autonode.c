/* AUTONODE.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <io.h>
#include <dir.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <process.h>
#include <sys\stat.h>

#define uchar unsigned char
#define uint unsigned int
#define ulong unsigned long
#define LOOP_NODEDAB 100    /* Retries on NODE.DAB locking/unlocking    */

typedef struct {						/* Node information kept in NODE.DAB */
	uchar	status,						/* Current Status of Node */
			errors,						/* Number of Critical Errors */
			action;						/* Action User is doing on Node */
	uint	useron,						/* User on Node */
			connection,					/* Connection rate of Node */
			misc,						/* Miscellaneous bits for node */
			aux;						/* Auxillary word for node */
	ulong	extaux;						/* Extended aux dword for node */
            } node_t;

enum {								/* Node Status */
	 NODE_WFC			        	/* Waiting for Call */
	,NODE_LOGON                  	/* at logon prompt */
	,NODE_NEWUSER         			/* New user applying */
	,NODE_INUSE			 			/* In Use */
	,NODE_QUIET			 			/* In Use - quiet mode */
	,NODE_OFFLINE		 			/* Offline */
	,NODE_NETTING		 			/* Networking */
	,NODE_EVENT_WAITING				/* Waiting for all nodes to be inactive */
	,NODE_EVENT_RUNNING				/* Running an external event */
	,NODE_EVENT_LIMBO				/* Allowing another node to run an event */
    };

int nodefile;

void main();
void getnodedat(uchar number, node_t *node, char lockit);
void putnodedat(uchar number, node_t node);
void truncsp(char *str);

void main(int argc,char *argv[])
{
	FILE *fp;
	char str[256],nodepath[256],*p
		,sbbs_ctrl[256],sbbs_node[256],path[MAXPATH]
		,*arg[10]={NULL};
	int file,num_nodes,autonode,disk,x,y;
	node_t node;

printf("\nSynchronet AUTONODE v2.00\n");

	if(!strcmp(argv[1],"/?")) {
		printf("\nUsage: AUTONODE [file] [args,...]");
		printf("\n\nWhere [file]     is the name of the file to run and");
		printf("\n      [args,...] are the command line arguments to use");
		printf("\n\nNOTE: The default command line is 'SBBS l q'\n");
		return; }
	p=getenv("SBBSCTRL");
	if(!p) {
		printf("\n\7You must set the SBBSCTRL environment variable first.");
		printf("\n\nExample: SET SBBSCTRL=C:\\SBBS\\CTRL\n");
		return; }
	sprintf(sbbs_ctrl,"%.40s",p);
	strupr(sbbs_ctrl);
	if(sbbs_ctrl[strlen(sbbs_ctrl)-1]!='\\')
		strcat(sbbs_ctrl,"\\");
	p=getenv("SBBSNODE");
	if(!p) {
		printf("\n\7You must set the SBBSNODE environment variable first.");
		printf("\n\nExample: SET SBBSNODE=C:\\SBBS\\NODE1\n");
        return; }
	sprintf(sbbs_node,"%.40s",p);
	strupr(sbbs_node);
	if(sbbs_node[strlen(sbbs_node)-1]!='\\')
		strcat(sbbs_node,"\\");

	sprintf(str,"%sNODE.DAB",sbbs_ctrl);
	if((nodefile=open(str,O_RDWR|O_BINARY|O_DENYNONE))==-1) {
        printf("Error opening %s",str); exit(1); }

	sprintf(str,"%sMAIN.CNF",sbbs_ctrl);
	if((file=open(str,O_RDONLY|O_DENYNONE|O_BINARY))==-1) {
		printf("Error opening %s",str);
		exit(1); }
	lseek(file,227L,SEEK_SET);
	read(file,&num_nodes,2);
	printf("\nNumber of Available Nodes = %d",num_nodes);
	lseek(file,64L*(long)num_nodes,SEEK_CUR);
	lseek(file,328L,SEEK_CUR);
	read(file,&autonode,2);
	printf("\nNumber of First Autonode  = %d",autonode);
	for(x=autonode;x<=num_nodes;x++) {
		printf("\nChecking Node #%d",x);
		getnodedat(x,&node,1);
		if(node.status==NODE_OFFLINE) {
			printf("\nFOUND! Node #%d is OFFLINE\n",x);
			node.status=NODE_WFC;
			putnodedat(x,node);
			lseek(file,(229L+((long)(x-1)*64L)),SEEK_SET);
			read(file,nodepath,128);
			truncsp(nodepath);
			if(nodepath[strlen(nodepath)-1]=='\\')
				nodepath[strlen(nodepath)-1]=0; 	/* remove '\' */
			if(nodepath[0]=='.')
				sprintf(path,"%s%s",sbbs_node,nodepath);
			else strcpy(path,nodepath);
			if(path[1]==':')
				setdisk(path[0]-'A');
			if(chdir(path)) {
				printf("\nError changing into '%s'",path);
				getnodedat(x,&node,1);
				node.status=NODE_OFFLINE;
				putnodedat(x,node);
				exit(1); }
			if(argc==1) {
				execl(getenv("COMSPEC"),getenv("COMSPEC"),"/c","SBBS","l","q",
					NULL); }
			else {
				arg[0]=argv[0];
				strcpy(str,"/c"); arg[1]=str;
				for(x=1;x<argc;x++) arg[1+x]=argv[x];
				execv(getenv("COMSPEC"),arg); }
			getnodedat(x,&node,1);
			node.status=NODE_OFFLINE;
			putnodedat(x,node);
			return; } }
	printf("\n\n\7All local nodes are in use!\n");
}

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
if(count==LOOP_NODEDAB)
	printf("\7Error unlocking and reading NODE.DAB\n");
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
	printf("\7Error writing NODE.DAB for node %u\n",number+1);
	return; }
unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
}
/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && str[c-1]<=32) c--;
str[c]=0;
}
