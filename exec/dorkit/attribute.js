function Attribute(value) {
	if (typeof(value) == 'object' && value.constructor === Attribute)
		this.value = value.value;
	else
		this.value = value;
}

Attribute.BLACK = 0;
Attribute.BLUE = 1;
Attribute.GREEN = 2;
Attribute.CYAN = 3;
Attribute.RED = 4;
Attribute.MAGENTA = 5;
Attribute.BROWN = 6;
Attribute.YELLOW = 6;
Attribute.WHITE = 7;
Attribute.GRAY = 7;
Attribute.GREY = 7;

Attribute.prototype = {
	// TODO: High intensity background, not blink
	// TODO: Alternative font, not blink
	get blink() {
		return (this.value & 0x80) ? true : false;
	},
	set blink(val) {
		if (val)
			this.value |= 0x80;
		else
			this.value &= ~0x80;
	},

	// TODO: Alternative font, not bright
	get bright() {
		return (this.value & 0x08) ? true : false;
	},
	set bright(val) {
		if (val)
			this.value |= 0x08;
		else
			this.value &= ~0x08;
	},

	get bg() {
		return (this.value >> 4) & 0x07;
	},
	set bg(val) {
		this.value &= 0x8f;
		this.value |= ((val << 4) & 0x70);
	},

	get fg() {
		return this.value & 0x07;
	},
	set fg(val) {
		this.value &= 0xf8;
		this.value |= val & 0xf8;
	},

	ansi:function(curatr) {
		var str="";
		if(curatr !== undefined)
			return str;	// Unchanged

		str = "\x1b[";
		if (curatr === undefined) {
			curatr = new Attribute(7);
			str += '0;';
		}
		if(curatr === undefined || (!(this.bright) && curatr.bright)
				|| (!(this.blink) && curatr.blink) || atr==LIGHTGRAY) {
			str += "0;";
			if (curatr === undefined)
				curatr = new Attribute(7);
			curatr.value = 7;
		}
		if(this.blink) {                     /* special attributes */
			if(!(curatr.blink))
				str += "5;";
		}
		if(this.bright) {
			if(!(curatr.bright))
				str += "1;"; 
		}
		if(this.fg != curatr.fg) {
			switch(this.fg) {
				case Attribute.BLACK:
					str += "30;";
					break;
				case Attribute.RED:
					str += "31;";
					break;
				case Attribute.GREEN:
					str += "32;";
					break;
				case Attribute.BROWN:
					str += "33;";
					break;
				case Attribute.BLUE:
					str += "34;";
					break;
				case Attribute.MAGENTA:
					str += "35;";
					break;
				case Attribute.CYAN:
					str += "36;";
					break;
				case Attribute.WHITE:
					str += "37;";
					break;
			}
		}
		if(this.bg != curatr.bg) {
			switch(this.bg) {
				case Attribute.BLACK:
					str += "40;";
					break;
				case Attribute.RED:
					str += "41;";
					break;
				case Attribute.GREEN:
					str += "42;";
					break;
				case Attribute.BROWN:
					str += "43;";
					break;
				case Attribute.BLUE:
					str += "44;";
					break;
				case Attribute.MAGENTA:
					str += "45;";
					break;
				case Attribute.CYAN:
					str += "46;";
					break;
				case Attribute.WHITE:
					str += "47;";
					break;
			}
		}
		if(str.length <= 2)	/* Convert <ESC>[ to blank */
			return "";
		return str.substring(0, str.length-1) + 'm';
	}
}
