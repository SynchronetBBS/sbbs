// playtone.js

/* Tone Generation Utility (using PC speaker, not sound card) */

const REVISION = "$Revision$".split(' ')[1];

const NOT_ABORTABLE	=(1<<0)
const SHOW_DOT		=(1<<1)
const SHOW_FREQ		=(1<<2)
const NO_VISUAL		=(1<<3)

var mode=0; 	/* Optional modes */
var t=1;		/* Timing */
var s=0;		/* Stacato */
var octave=4;	/* Default octave */

var pitch=523.50/32.0;	 /* low 'C' */

function play(freq, dur)
{
	var	notes=['c',' ','d',' ','e','f',' ','g',' ','a',' ','b'];
	var	sharp=['B','C',' ','D',' ','E','F',' ','G',' ','A',' '];
	var	i,n,o,d;
	var len;
	var	f;

//	writeln("play " + freq + ", " + dur);

	if(dur==undefined)
		dur="0";

	d=parseInt(dur);
	if((f=parseInt(freq))!=0)
		switch(freq.charAt(0).toUpperCase()) {
			case 'O':               /* default octave */
				if(parseInt(dur))
					octave=d;
				else
					octave+=d;
				return;
			case 'P':               /* pitch variation */
				if(parseInt(dur))
					pitch=atof(dur)/32.0;
				else
					pitch+=atof(dur);
				return;
			case 'Q':               /* quit */
				exit(0);
			case 'R':               /* rest */
				f=0;
				break;
			case 'S':               /* stacato */
				if(parseInt(dur))
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
				truncsp(dur);
				if(dur.charAt(dur.length-1)=='\\')
					write(dur);
				else
					writeln(dur);
				return;
			case 'X':               /* exit */
				exit(1);
			default:
				for(n=0;notes[n];n++)
					if(freq.charAt(0)==notes[n] || freq.charAt(0)==sharp[n])
						break;
				if(parseInt(freq.charAt(1)))
					o=(freq.charAt(1)&0xf);
				else
					o=octave;
				f=pitch*Math.pow(2,o+n/12);
				break; 
	}

	if(f && mode&SHOW_FREQ) {
		for(i=0;freq[i]>' ';i++)
			;
		freq[i]=0;
		printf("%-4.4s",freq); 
	}
	if(mode&SHOW_DOT)
		printf(".");
	if(t>10)
		len=(d*t)-(d*s);
	else
		len=d*t;
	if(isNaN(len))
		return;
	if(f)
		beep(f,len);
	else
		sleep(len);
	if(s) {
		if(t>10)
			sleep(d*s);
		else
			sleep(s);
	}
}


printf("\nTone Generation Module  %s  Copyright 2003 Rob Swindell\n\n", REVISION);

if(argc<1) {
	alert("!No filename specified");
	exit();
}

file = new File(argv[0]);
if(!file.exists)
	file.name=system.exec_dir + file.name;
writeln("Opening " + file.name);
if(!file.open("r")) {
	alert("!Error " + file.error + " opening " + file.name);
	exit();
}

text=file.readAll();
file.close();

for(i in text) {
	if(js.terminated)
		break;
//	writeln(text[i]);
	token=text[i].split(/\s+/);
	cmd=token[0];
	token.shift();
	play(cmd, token.join(' '));
}

