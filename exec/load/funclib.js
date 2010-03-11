/*
	Javascript Library of useful functions
	by MCMLXXIX 
*/

function getColor(color)
{
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
function drawLine(x,y,length,color)
{
	if(x && y)
	{
		console.gotoxy(x,y);
		if(y==24) while(x+length>80) length-=1;
	}
	for(i=0;i<length;i++)
	{
		console.putmsg((color?color:"\1k\1h") + "\xc4");
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
