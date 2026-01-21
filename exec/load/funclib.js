/*
	Javascript Library of useful functions
	by MCMLXXIX 
*/

function getColorExpression(colour)
{
	if (js.global.BG_BLACK==undefined)
		load("sbbsdefs.js");
	var ret = '';

	if (colour & BLINK)
		ret += 'BLINK|';

	switch(colour & (0x07<<4)) {
		case BG_BLACK:
			ret+="BG_BLACK";
			break;
		case BG_BLUE:
			ret+="BG_BLUE";
			break;
		case BG_GREEN:
			ret+="BG_GREEN";
			break;
		case BG_CYAN:
			ret+="BG_CYAN";
			break;
		case BG_RED:
			ret+="BG_RED";
			break;
		case BG_MAGENTA:
			ret+="BG_MAGENTA";
			break;
		case BG_BROWN:
			ret+="BG_BROWN";
			break;
		case BG_LIGHTGRAY:
			ret+="BG_LIGHTGRAY";
			break;
	}
	ret += '|';
	switch(colour & 0x0f) {
		case BLACK:
			ret+="BLACK";
			break;
		case BLUE:
			ret+="BLUE";
			break;
		case GREEN:
			ret+="GREEN";
			break;
		case CYAN:
			ret+="CYAN";
			break;
		case RED:
			ret+="RED";
			break;
		case MAGENTA:
			ret+="MAGENTA";
			break;
		case BROWN:
			ret+="BROWN";
			break;
		case LIGHTGRAY:
			ret+="LIGHTGRAY";
			break;
		case DARKGRAY:
			ret+="DARKGRAY";
			break;
		case LIGHTBLUE:
			ret+="LIGHTBLUE";
			break;
		case LIGHTGREEN:
			ret+="LIGHTGREEN";
			break;
		case LIGHTCYAN:
			ret+="LIGHTCYAN";
			break;
		case LIGHTRED:
			ret+="LIGHTRED";
			break;
		case LIGHTMAGENTA:
			ret+="LIGHTMAGENTA";
			break;
		case YELLOW:
			ret+="YELLOW";
			break;
		case WHITE:
			ret+="WHITE";
			break;
	}
	return ret;
}

function getColor(color)
{
	if(isNaN(color)) color=color.toUpperCase();
	switch(color)
	{
		case "BLACK":
		case "\x01K":
			return BLACK;
		case BLACK:
			return "\x01K";
		case "BLUE":
		case "\x01B":
			return BLUE;
		case BLUE:
			return "\x01B";
		case "CYAN":
		case "\x01C":
			return CYAN;
		case CYAN:
			return "\x01C";
		case "RED":
		case "\x01R":
			return RED;
		case RED:
			return "\x01R";
		case "GREEN":
		case "\x01G":
			return GREEN;
		case GREEN:
			return "\x01G";
		case "BROWN":
		case "\x01Y":
			return BROWN;
		case BROWN:
			return "\x01Y";
		case "MAGENTA":
		case "\x01M":
			return MAGENTA;
		case MAGENTA:
			return "\x01M";
		case "BG_BLACK":
		case "\x010":
			return BG_BLACK;
		case BG_BLACK:
			return "\x010";
		case "BG_BLUE":
		case "\x014":
			return BG_BLUE;
		case BG_BLUE:
			return "\x014";
		case "BG_CYAN":
		case "\x016":
			return BG_CYAN;
		case BG_CYAN:
			return "\x016";
		case "BG_RED":
		case "\x011":
			return BG_RED;
		case BG_RED:
			return "\x011";
		case "BG_GREEN":
		case "\x012":
			return BG_GREEN;
		case BG_GREEN:
			return "\x012";
		case "BG_BROWN":
		case "\x013":
			return BG_BROWN;
		case BG_BROWN:
			return "\x013";
		case "BG_MAGENTA":
		case "\x015":
			return BG_MAGENTA;
		case BG_MAGENTA:
			return "\x015";
		case "BG_LIGHTGRAY":
		case "\x017":
			return BG_LIGHTGRAY;
		case BG_LIGHTGRAY:
			return "\x017";
		case "WHITE":
		case "\x01W\x01H":
			return WHITE;
		case WHITE:
			return "\x01W\x01H";
		case "LIGHTCYAN":
		case "\x01C\x01H":
			return LIGHTCYAN;
		case LIGHTCYAN:
			return "\x01C\x01H";
		case "LIGHTRED":
		case "\x01R\x01H":
			return LIGHTRED;
		case LIGHTRED:
			return "\x01R\x01H";
		case "LIGHTGREEN":
		case "\x01G\x01H":
			return LIGHTGREEN;
		case LIGHTGREEN:
			return "\x01G\x01H";
		case "LIGHTBLUE":
		case "\x01B\x01H":
			return LIGHTBLUE;
		case LIGHTBLUE:
			return "\x01B\x01H";
		case "YELLOW":
		case "\x01Y\x01H":
			return YELLOW;
		case YELLOW:
			return "\x01Y\x01H";
		case "LIGHTMAGENTA":
		case "\x01M\x01H":
			return LIGHTMAGENTA;
		case LIGHTMAGENTA:
			return "\x01M\x01H";
		case "LIGHTGRAY":
		case "\x01N":
		case "\x01W":
			return LIGHTGRAY;
		case LIGHTGRAY:
			return "\x01W";
		case "DARKGRAY":
		case "\x01K\x01H":
			return DARKGRAY;
		case DARKGRAY:
			return "\x01K\x01H";
		default:
			return ANSI_NORMAL;
	}
}
function getLastWord(text)
{
	last_word=truncsp(text.substr(text.lastIndexOf(" ")));
	return removeSpaces(last_word);
}
function getFirstWord(text)
{
	first_word=truncsp(text.substring(0,text.indexOf(" ")));
	return removeSpaces(first_word);
}
function removeFirstWord(text)
{
	message=truncsp(text.substring(text.indexOf(" ")+1));
	return message;
}
function removeSpaces(text)
{
	if(text.length > 0)
		return text.replace(/^\s*/,"").replace(/\s*$/,"");
	return text;
}
function splitPadded(string1,string2,length,padding)
{
	if(!string1) string1="";
	if(!string2) string2="";
	if(!padding) padding=" ";
	var strlength=console.strlen(string1 + string2);
	if(strlength>length) {
		string1=string1.substring(0,length);
	}
	var padlength=length-strlength;
	var padded="";
	for(p=0;p<padlength;p++) padded+=padding;
	newstring=(string1 + padded + string2);
	return(newstring);
}
function printPadded(string,length,padding,justification)
{
	if(!string) string="";
	if(!padding) padding=" ";
	if(!justification) justification="left";
	var strlength=console.strlen(string);
	if(strlength>length) {
		string=string.substring(0,length);
	}
	var padlength=length-strlength;
	var newstring=string;
	var padded="";
	for(p=0;p<padlength;p++) padded+=padding;
	if(justification=="left") newstring+=(padded);
	if(justification=="right") newstring=(padded + newstring);
	return(newstring);
}
function centerString(string,length,padding)
{
	if(!padding) padding=" ";
	var strlength=console.strlen(string);
	if(strlength>length) {
		string=string.substring(0,length);
	}
	var padlength=length-strlength;
	var newstring=string;
	for(p=1;p<=padlength;p++) {
		if(p%2==0) newstring+=padding;
		else newstring=padding+newstring;
	}
	return newstring;
}
function shuffle(array)
{
	for(var j, x, i = array.length; i; j = parseInt(Math.random() * i), x = array[--i], array[i] = array[j], array[j] = x);
	return array;
}
function countMembers(array)
{
	var count=0;
	for(var i in array) count++;
	return count;
}
function timeStamp(time_t)
{
	return strftime("%H:%M:%S %m/%d/%y",time_t);
}
function strlen(str)
{
	return console.strlen(removeSpaces(str));
}
function drawLine(x,y,length,color)
{
	if(x && y) {
		console.gotoxy(x,y);
		if(y==24 && x+length>80) length-=(length-80);
	}
	for(i=0;i<length;i++) {
		if(color) console.attributes=color;
		console.putmsg("\xc4",P_SAVEATR);
	}
}
function clearBlock(x,y,w,h,bg)
{
	console.attributes=bg?bg:ANSI_NORMAL;
	console.gotoxy(x,y);
	for(var l=0;l<h;l++) {
		console.pushxy();
		clearLine(w);
		if(l<h-1) {
			console.popxy();
			console.down();
		}
	}
}
function clearLine(length,x,y,bg)
{
	if(x && y) console.gotoxy(x,y);
	if(bg) console.attributes=bg;
	if(length) console.putmsg(format("%*s",length,""),P_SAVEATR);
	else console.cleartoeol();
}
function debug(data,LOG_LEVEL)
{
	LOG_LEVEL=LOG_LEVEL?LOG_LEVEL:LOG_DEBUG;
	var str=debug_trace(data,1).split("\r\n");
	while(str.length) log(LOG_LEVEL,str.shift());
}
function debug_trace(data,level)
{
	if(typeof data=="object") {
		var prefix=format("%*s",level*2,"");
		var value="{\r\n";
		for(p in data) {
			value+=prefix + debug_trace(p + "=" + debug_trace(data[p],level+1) + "\r\n");
		}
		value+=prefix.substr(2)+"}";
		return value;
	} else if(typeof data=="function") {
		return data.toSource();
	}
	return data;
}
function testSocket(socket)
{
	if(socket.is_connected) return true;
	else {
		if(socket.error) {
			debug("SOCKET ERROR: " + socket.error,LOG_WARNING);
			debug(socket,LOG_WARNING);
		} else debug("socket disconnected",LOG_DEBUG);
		return false;
	}
}
function toXML(obj)
{
	var xml = new XMLList('<xml/>');
	for (var i in obj) {
		switch (typeof obj[i]) 
		{
			case "object": 
				var child=toXML(obj[i]);
				xml.appendChild(<{i}>{child.children()}</{i}>);
				break;
			case "array": //TODO: figure this shit out
				/*
				for(var x=0;x<obj[i].length;x++) {
					switch(typeof obj[i][x]) {
						case "object": 
						case "array":
							var child=toXML(obj[i][x]);
							xml.appendChild(<{i}>{child.children()}</{i}>);
							break;
						default:
							xml.appendChild(<{i}>{obj[i][x]}</{i}>);break;
							break;
					}
				}
				*/
				break;
			default: 
				xml.appendChild(<{i}>{obj[i]}</{i}>);break;
		}
	}
	return xml;
}
function setPosition(x,y) 
{
	console.gotoxy(x,y);
	console.pushxy();
}
function pushMessage(txt,container) 
{
	if(!container)
		container = console;
	container.popxy();
	container.putmsg(txt,P_SAVEATR);
	container.popxy();
	container.down();
	container.pushxy();
}	
function pushxy(name)
{
	if(!js.global.saved_coords)
		js.global.saved_coords=[];
	if(name)
		js.global.saved_coords[name]=console.getxy();
	else
		console.pushxy();
}	
function popxy(name)
{
	if(name)
		if(js.global.saved_coords[name])
			console.gotoxy(js.global.saved_coords[name]);
	else
		console.popxy();
}
function combine(obj1,obj2,concat_str) 
{
	var newobj = {};
	if(obj2 instanceof Array)
		newobj=[];
	else
		newobj={};
	for(var i in obj1) {
		switch(typeof obj1[i]) {
			case "object":
				newobj[i] = combine(newobj[i],obj1[i],concat_str);
				break;
			default:
				newobj[i] = obj1[i];
				break;
		}
	}
	for(var i in obj2) {
		switch(typeof obj2[i]) {
		case "function":
			if(!newobj[i])
				newobj[i]=obj2[i];
			else
				newobj["_"+i]=obj2[i];
			break;
		case "string":
			if(!newobj[i])
				newobj[i]=obj2[i];
			else if(newobj instanceof Array) {
				if(concat_str)
					newobj[i] += obj2[i];
				else
					newobj.push(obj2[i]);
			}
			else {
				if(concat_str && typeof newobj[i] == "string")
					newobj[i] += obj2[i];
				else
					newobj["_"+i]=obj2[i];
			}
			break;
		case "number":
			if(newobj[i]) {
				if(typeof newobj[i] == "number" || !isNaN(obj[i]))
					newobj[i] += obj2[i];
				else if(newobj instanceof Array)
					newobj.push(obj2[i]);
				else
					newobj["_"+i]=obj2[i];
			}
			else 
				newobj[i] = obj2[i];
			break;
		case "object":
			if(typeof newobj[i] == "object")
				newobj[i] = combine(newobj[i],obj2[i],concat_str);
			else if(newobj[i]) {
				newobj["_"+i] = {};
				newobj = combine(newobj["_"+i],obj2[i],concat_str);
			}
			else	
				newobj[i] = obj2[i];
			break;
		}
	}
	return newobj;
}
function compressList(list) 
{
	var c=[];
	for(var l in list)
		c.push(list[l]);
	return c;
}
function sortListByProperty(list,prop) 
{ 
	var data=compressList(list);
	var numItems=data.length;
	for(n=0; n<numItems; n++)	{
		for(m=0; m<(numItems-1); m++) {
			if(data[m][prop] < data[m+1][prop]) {
				var holder = data[m+1];
				data[m+1] = data[m];
				data[m] = holder;
			}
		}
	}
	return data;
}
function logStamp(msg) 
{
	if(!js.global.LogTimer)
		js.global.LogTimer={};
		
	var t = js.global.LogTimer;
	var now = Date.now();

	if(!t.start || (now-t.start > 10000)) {
		t.start = now;
		t.last = now;
	}
	if(!t.last)
		t.last = now;
		
	log(format("%4d",(now-t.last)) + " [" + (now - t.start) + "] " + msg);
	t.last = now;
}

function pipeToCtrlA(str) {
	str = str.replace(/\|00/g, "\x01N\x01K");
	str = str.replace(/\|01/g, "\x01N\x01B");
	str = str.replace(/\|02/g, "\x01N\x01G");
	str = str.replace(/\|03/g, "\x01N\x01C");
	str = str.replace(/\|04/g, "\x01N\x01R");
	str = str.replace(/\|05/g, "\x01N\x01M");
	str = str.replace(/\|06/g, "\x01N\x01Y");
	str = str.replace(/\|07/g, "\x01N\x01W");
	str = str.replace(/\|08/g, "\x01H\x01K");
	str = str.replace(/\|09/g, "\x01H\x01B");
	str = str.replace(/\|10/g, "\x01H\x01G");
	str = str.replace(/\|11/g, "\x01H\x01C");
	str = str.replace(/\|12/g, "\x01H\x01R");
	str = str.replace(/\|13/g, "\x01H\x01M");
	str = str.replace(/\|14/g, "\x01H\x01Y");
	str = str.replace(/\|15/g, "\x01H\x01W");
	str = str.replace(/\|16/g, "\x01" + 0);
	str = str.replace(/\|17/g, "\x01" + 4);
	str = str.replace(/\|18/g, "\x01" + 2);
	str = str.replace(/\|19/g, "\x01" + 6);
	str = str.replace(/\|20/g, "\x01" + 1);
	str = str.replace(/\|21/g, "\x01" + 5);
	str = str.replace(/\|22/g, "\x01" + 3);
	str = str.replace(/\|23/g, "\x01" + 7);
	return str;
}

function colorPicker(x, y, frame, attr) {

	var fgColours = [
		BLACK,
		BLUE,
		GREEN,
		CYAN,
		RED,
		MAGENTA,
		BROWN,
		LIGHTGRAY,
		DARKGRAY,
		LIGHTBLUE,
		LIGHTGREEN,
		LIGHTCYAN,
		LIGHTRED,
		LIGHTMAGENTA,
		YELLOW,
		WHITE
	];
	var bgColours = [
		BG_BLACK,
		BG_BLUE,
		BG_GREEN,
		BG_CYAN,
		BG_RED,
		BG_MAGENTA,
		BG_BROWN,
		BG_LIGHTGRAY
	];
	
	var fgmask = (1<<0)|(1<<1)|(1<<2)|(1<<3);
	var bgmask = (1<<4)|(1<<5)|(1<<6);
	var blkmask = (1<<7);
	if(frame === undefined)
		var frame = new Frame(x, y, 36, 6, BG_BLACK|LIGHTGRAY);
	if(attr === undefined)
		attr = frame.attr;
	var fgColour = attr&fgmask;
	var bgColour = ((attr&bgmask)>>4);
	var blink = ((attr&blkmask)>>7) ? blkmask : 0;
	
	var palette = new Frame(x, y, 36, 6, BG_BLUE|WHITE, frame);
	var subPalette = new Frame(palette.x + 2, palette.y + 1, palette.width - 4, palette.height - 2, BG_BLACK, palette);
	var pfgCursor = new Frame(x, y, 2, 1, BG_BLACK|WHITE|blink, subPalette);
	var pbgCursor = new Frame(x, y, 2, 1, BG_BLACK|WHITE, subPalette);
	palette.open();

	for(var c = 0; c < fgColours.length; c++) {
		var ch = (c == 0) ? "\x01h\x01w" + ascii(250) : ascii(219);
		subPalette.attr = fgColours[c];
		subPalette.putmsg(ch + ch);
		subPalette.attr = 0;
	}
	subPalette.gotoxy(1, 3);
	for(var c  = 0; c < bgColours.length; c++) {
		var ch = (c == 0) ? "\x01h\x01w" + ascii(250) : " ";
		subPalette.attr = bgColours[c];
		subPalette.putmsg(ch + ch + ch + ch);
		subPalette.attr = 0;
	}
	palette.top();
	pfgCursor.top();
	pbgCursor.top();
	palette.center("Foreground: Left / Right [B-Blink]");
	palette.gotoxy(1, 6);
	palette.center("Background: Up / Down");
	pfgCursor.putmsg("FG");
	pbgCursor.putmsg("BG");
	var userInput = "";
	pfgCursor.moveTo((fgColour * 2) + subPalette.x, subPalette.y + 1);
	pbgCursor.moveTo((bgColour * 4) + subPalette.x, subPalette.y + 3);
	palette.cycle();
	while(!js.terminated) {
		var userInput = console.inkey(K_NONE, 5);
		if(userInput == "")
			continue;
		switch(userInput) {
			case KEY_LEFT:
				if(pfgCursor.x > subPalette.x) {
					pfgCursor.move(-2, 0);
					fgColour = fgColour - 1;
				} else {
					pfgCursor.moveTo(subPalette.x + subPalette.width - 2, pfgCursor.y);
					fgColour = fgColours.length - 1;
				}
				break;
			case KEY_RIGHT:
				if(pfgCursor.x < subPalette.x + subPalette.width - 2) {
					pfgCursor.move(2, 0);
					fgColour = fgColour + 1;
				} else {
					pfgCursor.moveTo(subPalette.x, pfgCursor.y);
					fgColour = 0;
				}
				break;
			case KEY_DOWN:
				if(pbgCursor.x > subPalette.x) {
					pbgCursor.move(-4, 0);
					bgColour = bgColour - 1;
				} else {
					pbgCursor.moveTo(subPalette.x + subPalette.width - 4, pbgCursor.y);
					bgColour = bgColours.length - 1;
				}
				break;
			case KEY_UP:
				if(pbgCursor.x < subPalette.x + subPalette.width - 4) {
					pbgCursor.move(4, 0);
					bgColour = bgColour + 1;
				} else {
					pbgCursor.moveTo(subPalette.x, pbgCursor.y);
					bgColour = 0;
				}
				break;
			case 'B':
				blink ^= blkmask;
				pfgCursor.putmsg('FG',BG_BLACK|WHITE|blink);
				break;
			default:
				break;
		}
		attr = fgColours[fgColour]|bgColours[bgColour]|blink;
		if(palette.cycle())
			console.gotoxy(80, 24);
		if(ascii(userInput) == 27 || ascii(userInput) == 13 || ascii(userInput) == 9)
			break;
	}
	palette.close();
	return attr;
}
