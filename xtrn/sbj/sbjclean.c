/* SBJCLEAN.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Clean-up program for Synchronet Blackjack Online External Program */

#define SBJCLEAN

#include "sbj.c"


int main(int argc, char **argv)
{
	char *p;
	int i;

node_dir[0]=0;
for(i=1;i<argc;i++)
	if(!stricmp(argv[i],"/L"))
		logit=1;
	else strcpy(node_dir,argv[i]);

p=getenv("SBBSNODE");
if(!node_dir[0] && p)
	strcpy(node_dir,p);

if(!node_dir[0]) {	  /* node directory not specified */
	printf("usage: sbjclean <node directory>\r\n");
	getch();
	return(1); }

if(node_dir[strlen(node_dir)-1]!='\\'
	&& node_dir[strlen(node_dir)-1]!='/')  /* make sure node_dir ends in '/' */
	strcat(node_dir,"/");

initdata();                                 /* read XTRN.DAT and more */

if((gamedab=open("GAME.DAB",O_RDWR|O_DENYNONE|O_BINARY))==-1) {
	printf("Error opening GAME.DAB\r\n");                /* open deny none */
    return(1); }
getgamedat(1);
node[node_num-1]=0;
status[node_num-1]=0;
putgamedat();
if(curplayer==node_num)
	nextplayer();
close(gamedab);
return(0);
}
