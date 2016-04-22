// $Id$

load("sbbsdefs.js");

function attr(atr, curatr, color)
{
    var str="";
    if(curatr==undefined)
        curatr=0;

	if(color==false) {  /* eliminate colors if terminal doesn't support them */
		if(atr&LIGHTGRAY)       /* if any foreground bits set, set all */
			atr|=LIGHTGRAY;
		if(atr&BG_LIGHTGRAY)  /* if any background bits set, set all */
			atr|=BG_LIGHTGRAY;
		if((atr&LIGHTGRAY) && (atr&BG_LIGHTGRAY))
			atr&=~LIGHTGRAY;    /* if background is solid, foreground is black */
		if(!atr)
			atr|=LIGHTGRAY;		/* don't allow black on black */
	}
	if(curatr==atr) { /* text hasn't changed. no sequence needed */
		return str;
	}

	str = "\x1b[";
	if((!(atr&HIGH) && curatr&HIGH) || (!(atr&BLINK) && curatr&BLINK)
		|| atr==LIGHTGRAY) {
		str += "0;";
		curatr=LIGHTGRAY;
	}
	if(atr&BLINK) {                     /* special attributes */
		if(!(curatr&BLINK))
			str += "5;";
	}
	if(atr&HIGH) {
		if(!(curatr&HIGH))
			str += "1;"; 
	}
	if((atr&0x07) != (curatr&0x07)) {
		switch(atr&0x07) {
			case BLACK:
				str += "30;";
				break;
			case RED:
				str += "31;";
				break;
			case GREEN:
				str += "32;";
				break;
			case BROWN:
				str += "33;";
				break;
			case BLUE:
				str += "34;";
				break;
			case MAGENTA:
				str += "35;";
				break;
			case CYAN:
				str += "36;";
				break;
			case LIGHTGRAY:
				str += "37;";
				break;
		}
	}
	if((atr&0x70) != (curatr&0x70)) {
		switch(atr&0x70) {
			/* The BG_BLACK macro is 0x200, so isn't in the mask */
			case 0 /* BG_BLACK */:	
				str += "40;";
				break;
			case BG_RED:
				str += "41;";
				break;
			case BG_GREEN:
				str += "42;";
				break;
			case BG_BROWN:
				str += "43;";
				break;
			case BG_BLUE:
				str += "44;";
				break;
			case BG_MAGENTA:
				str += "45;";
				break;
			case BG_CYAN:
				str += "46;";
				break;
			case BG_LIGHTGRAY:
				str += "47;";
				break;
		}
	}
	if(str.length <= 2)	/* Convert <ESC>[ to blank */
		return "";
	return str.substring(0, str.length-1) + 'm';
}

/* Leave as last line for convenient load() usage: */
this;
