/* DCDWATCH.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Program to execute another program, but terminate if carrier detect is	*/
/* lost */

/* Compile small memory model */

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <dos.h>
#include <io.h>
#include <alloc.h>

extern unsigned _heaplen=2048;

#define uint unsigned int

#pragma warn -par

char hungup=0;
uint far *msr;

void interrupt (*old_int21)(void);

struct	REGPACK r;

void interrupt new_int21(uint bp, uint di, uint si, uint ds, uint es,
	uint dx, uint cx, uint bx, uint ax, uint ip, uint cs, uint flags)
{

r.r_ax=ax;
r.r_bx=bx;
r.r_cx=cx;
r.r_dx=dx;
r.r_bp=bp;
r.r_si=si;
r.r_di=di;
r.r_flags=flags;
r.r_ds=ds;
r.r_es=es;

intr(0xaa,&r);

ax=r.r_ax;
bx=r.r_bx;
cx=r.r_cx;
dx=r.r_dx;
bp=r.r_bp;
si=r.r_si;
di=r.r_di;
flags=r.r_flags;
ds=r.r_ds;
es=r.r_es;

if(!(*(msr)&0x8000) && !hungup) {
    hungup=1;
	r.r_ax=0x4c00;
	intr(0xaa,&r); }					 /* so exit */
}

#pragma warn +par


int main(int argc, char **argv)
{
	char str[256],*arg[30],*comspec;
	int i;
	FILE *stream;

if(argc<3) {
	printf("usage: %%!dcdwatch %%& <program> <arguments>\n");
	exit(1) ;}

msr=(uint far *)atol(argv[1]);

if((stream=fopen("INTRSBBS.DAT","r"))!=NULL) {
	fgets(str,81,stream);
	msr=(uint far *)atol(str);
	fclose(stream);
	remove("INTRSBBS.DAT"); }

if(msr) {
	old_int21=getvect(0x21);
	setvect(0xAA,old_int21);
	setvect(0x21,new_int21); }

comspec=getenv("COMSPEC");

arg[0]=comspec;
arg[1]="/c";
for(i=2;i<30;i++)
	arg[i]=argv[i];

i=spawnvp(P_WAIT,arg[0],arg);

if(msr)
	setvect(0x21,old_int21);
return(i);
}
