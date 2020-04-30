#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>

#include "sbbs.h"
#include "chatfuncs.h"

#define PCHAT_LEN 1000

char			usrname[128];

static node_t	node;
static int		nodenum;
static int		in,out;
static char		inpath[MAX_PATH+1];
static char		outpath[MAX_PATH+1];
static scfg_t	cfg;

static int togglechat(int on)
{
    static int  org_act;
	int nodefile = -1;

	if(getnodedat(&cfg,nodenum,&node,TRUE,&nodefile))
		return(-1);
    if(on) {
        org_act=node.action;
        if(org_act==NODE_PCHT)
            org_act=NODE_MAIN;
        node.misc|=NODE_LCHAT;
    }
	else {
        node.action=org_act;
        node.misc&=~NODE_LCHAT;
    }
	if(putnodedat(&cfg,nodenum,&node,TRUE,nodefile))
		return(-1);
    return(0);
}

int chat_open(int node_num, char *ctrl_dir)
{
	char	*p;
	char	str[1024];

    /* Read .cfg files here */
    memset(&cfg,0,sizeof(cfg));
    cfg.size=sizeof(cfg);
    SAFECOPY(cfg.ctrl_dir,ctrl_dir);
    if(!load_cfg(&cfg, NULL, TRUE, str))
        return(-1);

	nodenum=node_num;

	if(getnodedat(&cfg,nodenum,&node,FALSE,NULL))
		return(-1);

	username(&cfg,node.useron,usrname);

	sprintf(outpath,"%slchat.dab",cfg.node_path[nodenum-1]);
	if((out=sopen(outpath,O_RDWR|O_CREAT|O_BINARY,O_DENYNONE
		,DEFFILEMODE))==-1) {
		return(-1);
	}

	sprintf(inpath,"%schat.dab",cfg.node_path[nodenum-1]);
	if((in=sopen(inpath,O_RDWR|O_CREAT|O_BINARY,O_DENYNONE
		,DEFFILEMODE))==-1) {
		close(out);
		return(-1);
    }

	if((p=(char *)malloc(PCHAT_LEN))==NULL) {
		close(in);
		close(out);
		return(-1);
    }
	memset(p,0,PCHAT_LEN);
	write(in,p,PCHAT_LEN);
	write(out,p,PCHAT_LEN);
	free(p);
	lseek(in,0,SEEK_SET);
	lseek(out,0,SEEK_SET);

	if(togglechat(TRUE))
		return(-1);
	return(0);
}

int chat_check_remote(void)
{
	time_t			now;
	static time_t	last_nodechk=0;

	now=time(NULL);
	if(now!=last_nodechk) {
		if(getnodedat(&cfg,nodenum,&node,FALSE,NULL)!=0)
			return(-1);			/* Failed to read nodedat! */
		last_nodechk=now;
	}
	if(node.misc&NODE_LCHAT)
		return(1);				/* Still Waiting */

	if(node.status==NODE_WFC || node.status>NODE_QUIET || node.action!=NODE_PCHT)
		return(0);				/* Remote has gone away */

	if(in==-1)
		return(0);				/* Remote has gone away */

	if(out==-1)
		return(-1);				/* Write error or some such */

	return(2);					/* Everything is good! */
}

int chat_read_byte(void)
{
	unsigned char ch=0;

	if(in==-1)
		return(-1);
	utime(inpath,NULL);
	switch(read(in,&ch,1)) {
		case -1:
			close(in);
			in=-1;
			return(-1);
		case 0:
			if(lseek(in,0,SEEK_SET)==-1)	/* Wrapped */
				return(-1);
			switch(read(in,&ch,1)) {
				case -1:
					close(in);
					in=-1;
					return(-1);
				case 0:
					return 0;
			}
			/* Fall-through */
		case 1:
			lseek(in,-1L,SEEK_CUR);
			if(ch) {
				write(in,"",1);
				return(ch);
			}
	}

	return(0);
}

int chat_write_byte(unsigned char ch)
{
	if(out==-1)
		return(-1);
	if(lseek(out,0,SEEK_CUR)>=PCHAT_LEN)
		lseek(out,0,SEEK_SET);
	switch(write(out,&ch,1)) {
		case -1:
			close(out);
			out=-1;
			return(-1);
	}
	utime(outpath,NULL);
	return(0);
}

int chat_close(void)
{
	if(in != -1) {
		close(in);
		in = -1;
	}
	if(out != -1) {
		close(out);
		out = -1;
	}
	return(togglechat(FALSE));
}
