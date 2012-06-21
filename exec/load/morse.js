function Morse2ANSI(string, wpm)
{
	var morse={
		'A':'.-',
		'B':'-...',
		'C':'-.-.',
		'D':'-..',
		'E':'.',
		'F':'..-.',
		'G':'--.',
		'H':'....',
		'I':'..',
		'J':'.---',
		'K':'-.-',
		'L':'.-..',
		'M':'--',
		'N':'-.',
		'O':'---',
		'P':'.--.',
		'Q':'--.-',
		'R':'.-.',
		'S':'...',
		'T':'-',
		'U':'..-',
		'V':'...-',
		'W':'.--',
		'X':'-..-',
		'Y':'-.--',
		'Z':'--..',
		'1':'.----',
		'2':'..---',
		'3':'...--',
		'4':'....-',
		'5':'.....',
		'6':'-....',
		'7':'--...',
		'8':'---..',
		'9':'----.',
		'0':'-----',
		'.':'.-.-.-',
		',':'--..--',
		'?':'..--..',
		"'":'.----.',
		'-':'-....-',
		'/':'-..-.',
		'=':'-...-',
		'@':'.--.-'
	};

	var str='MFMLO4';
	if(wpm==undefined) {
		wpm=25;
	}
	// Calculate the tempo...
	var etime=1.2/wpm;		// The duration of one element (seconds).
	var epm=60/etime;		// Number of elements per minute
	var T;
	var L=4;
	T=epm;
	while(T > (255)) {
		T/=2;
		L*=2;
	}
	str += 'T'+parseInt(T,10)+'L'+parseInt(L,10);
	var i,j;
	var skipped=false;
	for(i=0;i<string.length;i++) {
		var ch=string.charAt(i).toUpperCase();
		if(morse[ch] != undefined) {
			skipped=false;
			for(j=0;j<morse[ch].length;j++) {
				if(morse[ch].charAt(j)=='.') {
					str += 'C';
				}
				else {
					str += 'C'+(L/2)+'.';
				}
				str += 'P';
			}
			str += (L/2)+'.';
		}
		else if(!skipped) {
			skipped=true;
			if(ch=~/^\s$/) {
				str += 'P'+(L/4);
			}
		}
	}
	return("\x1b[|"+str+"\x0e");
}
