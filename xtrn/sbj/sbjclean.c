/* sbjclean.c */

/* Clean-up program for Synchronet Blackjack Online External Program */

/* $Id$ */

#define SBJCLEAN

#include "sbj.c"	/* Just for a couple of functions we need */

uchar	node_num;

int main(int argc, char **argv)
{
	char*	p;
	char	node_dir[MAX_PATH+1];

	if((p=getenv("SBBSNODE"))==NULL) {
		fprintf(stderr,"!Need SBBSNODE env var\n");
		return(-1);
	}
	strcpy(node_dir,p);

	if((p=getenv("SBBSNNUM"))==NULL) {
		fprintf(stderr,"!Need SBBSNNUM env var\n");
		return(-1);
	}
	node_num=atoi(p);

	if(node_dir[strlen(node_dir)-1]!='\\'
		&& node_dir[strlen(node_dir)-1]!='/')  /* make sure node_dir ends in '/' */
		strcat(node_dir,"/");

	if((gamedab=sopen("GAME.DAB",O_RDWR|O_BINARY,SH_DENYNO))==-1) {
		fprintf(stderr,"Error opening GAME.DAB\r\n");  /* open deny none */
		return(1); 
	}
	getgamedat(1);
	node[node_num-1]=0;
	status[node_num-1]=0;
	putgamedat();
	if(curplayer==node_num)
		nextplayer();
	close(gamedab);
	return(0);
}
