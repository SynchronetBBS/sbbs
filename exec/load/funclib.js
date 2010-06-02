/*
	Javascript Library of useful functions
	by MCMLXXIX 
*/

function getColor(color)
{
	if(!color) return false;
	switch(color.toUpperCase())
	{
		case "BLACK":
		case "\1K":
			return BLACK;
		case "BLUE":
		case "\1B":
			return BLUE;
		case "CYAN":
		case "\1C":
			return CYAN;
		case "RED":
		case "\1R":
			return RED;
		case "GREEN":
		case "\1G":
			return GREEN;
		case "BROWN":
		case "\1Y":
			return BROWN;
		case "MAGENTA":
		case "\1M":
			return MAGENTA;
		case "BG_BLUE":
		case "\0014":
			return BG_BLUE;
		case "BG_CYAN":
		case "\0016":
			return BG_CYAN;
		case "BG_RED":
		case "\0011":
			return BG_RED;
		case "BG_GREEN":
		case "\0012":
			return BG_GREEN;
		case "BG_BROWN":
		case "\0013":
			return BG_BROWN;
		case "BG_MAGENTA":
		case "\0015":
			return BG_MAGENTA;
		case "BG_LIGHTGRAY":
		case "\0017":
		return BG_LIGHTGRAY;
		case "WHITE":
		case "\1W\1H":
			return WHITE;
		case "LIGHTCYAN":
		case "\1C\1H":
			return LIGHTCYAN;
		case "LIGHTRED":
		case "\1R\1H":
			return LIGHTRED;
		case "LIGHTGREEN":
		case "\1G\1H":
			return LIGHTGREEN;
		case "LIGHTBLUE":
		case "\1B\1H":
			return LIGHTBLUE;
		case "YELLOW":
		case "\1Y\1H":
			return YELLOW;
		case "LIGHTMAGENTA":
		case "\1M\1H":
			return LIGHTMAGENTA;
		case "LIGHTGRAY":
		case "\1N":
			return LIGHTGRAY;
		case "DARKGRAY":
		case "\1K\1H":
			return DARKGRAY;
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
	while(text[0]==" ") text=text.substr(1);
	return truncsp(text);
}
function splitPadded(string1,string2,length,padding)
{
	if(!padding) padding=" ";
	var strlength=console.strlen(string1 + string2);
	if(strlength>length)
	{
		string=string.substring(0,length);
	}
	var padlength=length-strlength;
	var padded="";
	for(p=0;p<padlength;p++) padded+=padding;
	newstring=(string1 + padded + string2);
	return(newstring);
}
function printPadded(string,length,padding,justification)
{
	if(!padding) padding=" ";
	if(!justification) justification="left";
	var strlength=console.strlen(string);
	if(strlength>length)
	{
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
	if(strlength>length)
	{
		string=string.substring(0,length);
	}
	var padlength=length-strlength;
	var newstring=string;
	for(p=1;p<=padlength;p++)
	{
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
	if(x && y)
	{
		console.gotoxy(x,y);
		if(y==24 && x+length>80) length-=(length-80);
	}
	for(i=0;i<length;i++)
	{
		if(color) console.attributes=color;
		console.putmsg("\xc4",P_SAVEATR);
	}
}
function clearBlock(x,y,w,h,bg)
{
	console.attributes=bg?bg:ANSI_NORMAL;
	console.gotoxy(x,y);
	for(line=0;line<h;line++)
	{
		console.pushxy();
		clearLine(w);
		if(line<h-1) {
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
