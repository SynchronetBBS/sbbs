/* TONE.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Test sound freqs and durations from command line */

#ifdef __OS2__
	#define INCL_DOS
	#include <os2.h>
#endif
#include <io.h>
#include <dos.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef __OS2__
	#define mswait(x) DosSleep(x)
	#define delay(x) DosSleep(x)
	#define sound(x) DosBeep(x,0)
	#define nosound() DosBeep(0,0)
#endif

#define NOT_ABORTABLE	(1<<0)
#define SHOW_DOT		(1<<1)
#define SHOW_FREQ		(1<<2)
#define NO_VISUAL		(1<<3)
#define USE_MSWAIT		(1<<4)

#ifdef __OS2__
	int mswtyp;
#else
	extern mswtyp;
#endif

int aborted=0;	/* Ctrl-C hit? */
int mode=0; 	/* Optional modes */
int t=1;		/* Timing */
int s=0;		/* Stacato */
int octave=4;	/* Default octave */

double pitch=523.50/32.0;	 /* low 'C' */

void play(char *freq, char *dur)
{
	char *notes="c d ef g a b";
	char *sharp="BC D EF G A ";
	int i,n,d,o=octave;
	double f;

d=atoi(dur);
if(isdigit(freq[0]))
	f=atoi(freq);
else
	switch(toupper(freq[0])) {
		case 'O':               /* default octave */
			if(isdigit(dur[0]))
				octave=d;
			else
				octave+=d;
            return;
		case 'P':               /* pitch variation */
			if(isdigit(dur[0]))
				pitch=atof(dur)/32.0;
			else
				pitch+=atof(dur);
            return;
		case 'Q':               /* quit */
			nosound();
			exit(0);
		case 'R':               /* rest */
            f=0;
            break;
		case 'S':               /* stacato */
			if(isdigit(dur[0]))
				s=d;
			else
				s+=d;
			return;
		case 'T':               /* time adjust */
			t=d;
            return;
		case 'V':
			if(mode&NO_VISUAL)
				return;
			n=strlen(dur);
			while(n && dur[n]<=' ')
				n--;
			dur[n+1]=0;
			if(dur[n]=='\\') {
				dur[n]=0;
				printf("%s",dur); }
			else
				printf("%s\r\n",dur);
			return;
		case 'X':               /* exit */
			exit(1);
		default:
			for(n=0;notes[n];n++)
				if(freq[0]==notes[n] || freq[0]==sharp[n])
					break;
			if(isdigit(freq[1]))
				o=(freq[1]&0xf);
			else
				o=octave;
			f=pitch*pow(2,o+(double)n/12);
			break; }
if(!f)
	nosound();
else
	sound(f);
if(f && mode&SHOW_FREQ) {
	for(i=0;freq[i]>' ';i++)
		;
	freq[i]=0;
	printf("%-4.4s",freq); }
if(mode&SHOW_DOT)
	printf(".");
if(t>10) {
	if(mode&USE_MSWAIT)
		mswait((d*t)-(d*s));
	else
		delay((d*t)-(d*s)); }
else {
	if(mode&USE_MSWAIT)
		mswait(d*t);
	else
		delay(d*t); }
if(s) {
	nosound();
	if(t>10) {
		if(mode&USE_MSWAIT)
			mswait(d*s);
		else
			delay(d*s); }
	else {
		if(mode&USE_MSWAIT)
			mswait(s);
		else
			delay(s); } }
}

void usage(void)
{
printf("usage: tone [/opts] [(note[oct]|freq) dur | (cmd val) [...]] "
	"[+filename]\n\n");
printf("where: note  = a,b,c,d,e,f, or g (naturals) or A,B,C,D,E,F, or "
	"G (sharps)\n");
printf("       oct   = octave 1 through 9 (default=%d)\n",octave);
printf("       freq  = frequency (in Hz) or 0 for silence\n");
printf("       dur   = duration (in timer counts)\n");
printf("       cmd   = o set default octave (+/- to adjust) "
	"(default=%d)\n",octave);
printf("               p set middle c pitch (+/- to adjust) "
	"(default=%.2f)\n",pitch*32.0);
printf("               q quit program immediately\n");
printf("               r rest (silence) for val timer counts\n");
printf("               s set stacato duration (in ms) (+/- to adjust) "
	"(default=%d)\n",s);
printf("               t set timer count value (in ms) "
	"(default=%d)\n",t);
printf("               v visual text diplay of val (no val=cr/lf)\n");
printf("               x quit program immediately (leave tone on)\n");
printf("       opts  = d display dot for each note\n");
printf("               f display frequency or note value\n");
printf("               n not abortable with key-stroke\n");
printf("               v disable visual text commands\n");
printf("               t use time-slice aware delays\n");
exit(0);
}

int cbreakh()	/* ctrl-break handler */
{
aborted=1;
return(1);		/* 1 to continue, 0 to abort */
}

void main(int argc, char **argv)
{
    char *p,str[128];
	int i,j,file;
	FILE *stream;

#ifndef __OS2__
ctrlbrk(cbreakh);
#endif

printf("\nTone Generation Utility  v1.01  Developed 1993 Rob Swindell\n\n");

if(argc<2)
	usage();

mswtyp=0;
delay(0);
for(i=1;i<argc;i++) {
	if(argv[i][0]=='/') {
		for(j=1;argv[i][j];j++)
			switch(toupper(argv[i][j])) {
				case 'D':
					mode^=SHOW_DOT;
					break;
				case 'F':
					mode^=SHOW_FREQ;
					break;
				case 'N':
					mode^=NOT_ABORTABLE;
					break;
				case 'V':
					mode^=NO_VISUAL;
					break;
				case 'T':
					mode^=USE_MSWAIT;
					mswtyp=atoi(argv[i]+j+1);
					while(isdigit(argv[i][j+1]))
						j++;
					break;
				default:
					usage(); }
		continue; }
	if(argv[i][0]=='+') {
		if((file=open(argv[i]+1,O_RDONLY|O_DENYNONE|O_BINARY))==-1
			|| (stream=fdopen(file,"rb"))==NULL) {
			strcpy(str,argv[0]);
			p=strrchr(str,'\\');
			if(p)
				*(p+1)=0;
			strcat(str,argv[i]+1);
			if((file=open(str,O_RDONLY|O_DENYNONE|O_BINARY))==-1
				|| (stream=fdopen(file,"rb"))==NULL) {
				printf("\7Error opening %s\n",argv[i]+1);
				exit(1); } }
		while(mode&NOT_ABORTABLE || !kbhit()) {
			if(!fgets(str,81,stream))
				break;
			if(!isalnum(str[0]))
				continue;
			p=str;
			while(*p>' ')
				p++;
			while(*p && *p<=' ')
				p++;
			play(str,p); }
		fclose(stream);
		continue; }
	play(argv[i],argv[i+1]);
	i++;
	if(aborted)
		break; }
nosound();
}
