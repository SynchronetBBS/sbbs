/* test_termgfx_termio_palfade.c -- a palette change must be PATCHED, not
 * repainted, and the moved registers must reach a SyncTERM (whose boxes carry
 * no palette of their own).
 *
 * The hard case is a PURE fade: not one index changes, only what an index
 * MEANS. The index diff sees nothing at all, so only the stale-palette mask can
 * catch it -- and if nothing catches it, the screen keeps the old colours
 * indefinitely. Before this was handled, ANY palette change forced a whole
 * frame: on a played Flight of the Amazon Queen session that was 267 frames
 * carrying 77% of every byte the door sent.
 *
 * The scene is deliberately shaped so the fade is LOCAL -- a flat background
 * with the moving colour confined to one block. A colour spread over every
 * tile legitimately dirties the whole frame, and would prove nothing here.
 *
 * Its own binary because termgfx_termio keeps file-static session state with no
 * reset. cc'd + run by unit_termgfx_termio.sh. */
#include "termgfx_termio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
static char out[1 << 20];
static int drain(int fd){ ssize_t n; int t=0;
	while((n=recv(fd,out+t,sizeof out-1-t,MSG_DONTWAIT))>0) t+=n; out[t]=0; return t; }
static int rasters(const char *s, int n, int *maxpv){
	int i,c=0; *maxpv=0;
	for(i=0;i<n;i++) if(s[i]=='"'){ int f=0,pv=0,j=i+1;
		while(j<n&&((s[j]>='0'&&s[j]<='9')||s[j]==';')){ if(s[j]==';')f++;
			else if(f==3)pv=pv*10+(s[j]-'0'); j++; }
		if(f>=3){ c++; if(pv>*maxpv)*maxpv=pv; } }
	return c;
}
int main(void){
	int sv[2]; char fdarg[32]; char *av[3];
	static uint8_t idx[320*200], pal[768];
	int i, n, c, pv, full_pv;
	assert(socketpair(AF_UNIX,SOCK_STREAM,0,sv)==0);
	snprintf(fdarg,sizeof fdarg,"-s%d",sv[1]);
	av[0]=(char*)"palcheck"; av[1]=fdarg; av[2]=NULL;
	assert(termgfx_termio_init(2,av)==1);
	termgfx_termio_flush(); drain(sv[0]);
	{ const char *r="\x1b[?62;4c" "\x1b[24;80R" "\x1b[4;600;1200t" "\x1b[6;20;20t"
	                "\x1b[=67;84;101;114;109;1;332c";   /* identify as SyncTERM */
	  assert(send(sv[0],r,strlen(r),0)>0); }
	termgfx_termio_pump();
	/* Flat background, with colour 3 confined to ONE small block -- so a fade of
	 * colour 3 is a LOCAL change, which is the case patching should catch. */
	memset(idx,1,sizeof idx);
	for(i=0;i<24;i++) memcpy(idx+(80+i)*320+96, "\3\3\3\3\3\3\3\3\3\3\3\3\3\3\3\3", 16);
	for(i=0;i<768;i++) pal[i]=(uint8_t)(i*7);
	termgfx_termio_present(idx,pal); termgfx_termio_flush(); n=drain(sv[0]);
	c=rasters(out,n,&full_pv);
	assert(c >= 1 && full_pv > 400);          /* a whole frame, as the first must be */

	/* A PURE palette fade: not one index changes, colour 3 moves. */
	pal[3*3+0]^=0x40; pal[3*3+1]^=0x40; pal[3*3+2]^=0x40;
	termgfx_termio_present(idx,pal); termgfx_termio_flush(); n=drain(sv[0]);
	c=rasters(out,n,&pv);
	/* Something must go out -- silence would leave the old colour on screen --
	 * and it must be smaller than the frame, or nothing was gained. */
	assert(c >= 1);
	assert(pv < full_pv);
	/* SyncTERM boxes carry no palette, so the mover has to ride the first one. */
	assert(strstr(out, "#3;2;") != NULL);
	/* ...and only the movers: an untouched register must not be re-sent. */
	assert(strstr(out, "#7;2;") == NULL);

	/* An INVISIBLE palette change -- an entry the scene never draws with --
	 * must send nothing at all. An engine rewrites the whole palette freely
	 * and a scene uses a fraction of it; repainting for an entry no pixel
	 * references is pure waste (a quarter of the frames in Queen's opening
	 * sequence). */
	pal[200*3+0]^=0x55; pal[200*3+1]^=0x55; pal[200*3+2]^=0x55;
	termgfx_termio_present(idx,pal); termgfx_termio_flush(); n=drain(sv[0]);
	c=rasters(out,n,&pv);
	assert(c == 0);                        /* nothing on the wire at all */

	/* ...but it must not be FORGOTTEN. Draw with that entry now: the colour
	 * the terminal is handed has to be the NEW one, or deferring lost it. */
	memset(idx + 100*320, 200, 320);       /* one row, in the deferred colour */
	termgfx_termio_present(idx,pal); termgfx_termio_flush(); n=drain(sv[0]);
	c=rasters(out,n,&pv);
	assert(c >= 1);
	assert(strstr(out, "#200;2;") != NULL);

	printf("TERMGFX_TERMIO_PALFADE fade patched, invisible change silent,"
	       " deferred entry not lost OK\n");
	return 0;
}
