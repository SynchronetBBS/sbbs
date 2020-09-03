// playtone.js

/* Tone Generation Utility (using PC speaker, not sound card) */

const REVISION = "$Revision: 1.4 $".split(' ')[1];

const NO_VISUAL		=(1<<3)

var mode=0; 	/* Optional modes */
var t=1;		/* Timing */
var s=0;		/* Stacato */
var octave=4;	/* Default octave */
var stop=false;

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

	d=parseFloat(dur);
	if(isNaN(f=parseInt(freq)))
		switch(freq.charAt(0).toUpperCase()) {
			case 'O':               /* default octave */
				if(Number(dur.charAt(0)))
					octave=d;
				else
					octave+=d;
				return;
			case 'P':               /* pitch variation */
				if(Number(dur.charAt(0)))
					pitch=parseFloat(dur)/32.0;
				else
					pitch+=parseFloat(dur);
				return;
			case 'R':               /* rest */
				f=0;
				break;
			case 'S':               /* stacato */
				if(Number(dur.charAt(0)))
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
					write(truncstr(dur,'\\'));
				else
					writeln(dur);
				return;
			case 'Q':
			case 'X':               /* exit */
				stop=true;
				break;;
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


printf("\r\nSynchronet Tone Generation Module %s\r\n\r\n", REVISION);

var filename;
var debug=false;

for(i=0;i<argc;i++) {
	switch(argv[i]) {
		case "-d":
			debug=true;
			break;
		default:
			filename=argv[i];
			break;
	}
}
if(filename==undefined) {
	alert("No filename specified");
	exit();
}

if(!file_exists(filename))
	filename = system.exec_dir + filename;	
file = new File(filename);
if(!file.open("r")) {
	alert("Error " + file.error + " opening " + file.name);
	exit();
}

text=file.readAll();
file.close();

stop=false;
for(i in text) {
	if(js.terminated || stop)
		break;
	if(debug)
		writeln(text[i]);
	if(text[i].charAt(0)==':')	/* comment */
		continue;
	token=truncsp(text[i]).split(/\s+/);
	play(token.shift(), token.join(' '));
}

