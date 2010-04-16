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
			return BLACK;
		case "BLUE":
			return BLUE;
		case "CYAN":
			return CYAN;
		case "RED":
			return RED;
		case "GREEN":
			return GREEN;
		case "BROWN":
			return BROWN;
		case "MAGENTA":
			return MAGENTA;
		case "BG_BLUE":
			return BG_BLUE;
		case "BG_CYAN":
			return BG_CYAN;
		case "BG_RED":
			return BG_RED;
		case "BG_GREEN":
			return BG_GREEN;
		case "BG_BROWN":
			return BG_BROWN;
		case "BG_MAGENTA":
			return BG_MAGENTA;
		case "BG_LIGHTGRAY":
			return BG_LIGHTGRAY;
		case "WHITE":
			return WHITE;
		case "LIGHTCYAN":
			return LIGHTCYAN;
		case "LIGHTRED":
			return LIGHTRED;
		case "LIGHTGREEN":
			return LIGHTGREEN;
		case "YELLOW":
			return YELLOW;
		case "LIGHTMAGENTA":
			return LIGHTMAGENTA;
		case "LIGHTGRAY":
			return LIGHTGRAY;
		case "DARKGRAY":
			return DARKGRAY;
		default:
			return WHITE;
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
	while(text.indexOf(" ")==0) text=text.substr(1);
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
	for(line=0;line<h;line++)
	{
		clearLine(w,x,y+line);
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
	LOG_LEVEL=LOG_LEVEL?LOG_LEVEL:LOG_INFO;
	if(typeof data=="object") {
		for(p in data) {
			debug(p + "=" + data[p],LOG_LEVEL);
		}
	}
	else log(LOG_LEVEL,data);
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
