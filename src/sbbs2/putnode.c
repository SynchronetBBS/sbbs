#line 1 "PUTNODE.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

/****************************************************************************/
/* Write the data from the structure 'node' into NODE.DAB  					*/
/* getnodedat(num,&node,1); must have been called before calling this func  */
/*          NOTE: ------^   the indicates the node record has been locked   */
/****************************************************************************/
void putnodedat(uint number, node_t node)
{
	char str[256],firston[25];

if(!(sys_status&SS_NODEDAB))
	return;
if(!number || number>sys_nodes) {
	errormsg(WHERE,ERR_CHK,"node number",number);
	return; }
if(number==node_num) {
	if((node.status==NODE_INUSE || node.status==NODE_QUIET)
		&& text[NodeActionMain+node.action][0]) {
		node.misc|=NODE_EXT;
		memset(str,0,128);
		sprintf(str,text[NodeActionMain+node.action]
			,useron.alias
			,useron.level
			,getage(useron.birth)
			,useron.sex
			,useron.comp
			,useron.note
			,unixtodstr(useron.firston,firston)
			,node.aux&0xff
			,node.connection
			);
		putnodeext(number,str); }
	else
		node.misc&=~NODE_EXT; }
number--;	/* make zero based */
lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
if(write(nodefile,&node,sizeof(node_t))!=sizeof(node_t)) {
	unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
	errormsg(WHERE,ERR_WRITE,"nodefile",number+1);
	return; }
unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
}

/****************************************************************************/
/* Creates a short message for node 'num' than contains 'strin'             */
/****************************************************************************/
void putnmsg(int num, char *strin)
{
    char str[256];
    int file,i;
    node_t node;

sprintf(str,"%sMSGS\\N%3.3u.MSG",data_dir,num);
if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
    return; }
lseek(file,0L,SEEK_END);	// Instead of opening with O_APPEND
i=strlen(strin);
if(write(file,strin,i)!=i) {
    close(file);
    errormsg(WHERE,ERR_WRITE,str,i);
    return; }
close(file);
getnodedat(num,&node,0);
if((node.status==NODE_INUSE || node.status==NODE_QUIET)
    && !(node.misc&NODE_NMSG)) {
    getnodedat(num,&node,1);
    node.misc|=NODE_NMSG;
    putnodedat(num,node); }
}

void putnodeext(uint number, char *ext)
{
    char str[256];
    int count=0;

if(!(sys_status&SS_NODEDAB))
    return;
if(!number || number>sys_nodes) {
    errormsg(WHERE,ERR_CHK,"node number",number);
    return; }
number--;   /* make zero based */
while(count<LOOP_NODEDAB) {
    if(count>10)
        mswait(55);
    lseek(node_ext,(long)number*128L,SEEK_SET);
    if(lock(node_ext,(long)number*128L,128)==-1) {
        count++;
        continue; }
    if(write(node_ext,ext,128)==128)
        break;
    count++; }
unlock(node_ext,(long)number*128L,128);
if(count>(LOOP_NODEDAB/2) && count!=LOOP_NODEDAB) {
    sprintf(str,"NODE.EXB COLLISION - Count: %d",count);
    logline("!!",str); }
if(count==LOOP_NODEDAB) {
    errormsg(WHERE,ERR_WRITE,"NODE.EXB",number+1);
    return; }
}

/****************************************************************************/
/* Creates a short message for 'usernumber' than contains 'strin'           */
/****************************************************************************/
void putsmsg(int usernumber, char *strin)
{
    char str[256];
    int file,i;
    node_t node;

sprintf(str,"%sMSGS\\%4.4u.MSG",data_dir,usernumber);
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
    return; }
i=strlen(strin);
if(write(file,strin,i)!=i) {
    close(file);
    errormsg(WHERE,ERR_WRITE,str,i);
    return; }
close(file);
for(i=1;i<=sys_nodes;i++) {     /* flag node if user on that msg waiting */
    getnodedat(i,&node,0);
    if(node.useron==usernumber
        && (node.status==NODE_INUSE || node.status==NODE_QUIET)
        && !(node.misc&NODE_MSGW)) {
        getnodedat(i,&node,1);
        node.misc|=NODE_MSGW;
        putnodedat(i,node); } }
}

