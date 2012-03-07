/*
	Javascript Library of useful functions
	by MCMLXXIX 
*/

function getColor(color)
{
	if(isNaN(color)) color=color.toUpperCase();
	switch(color)
	{
		case "BLACK":
		case "\1K":
			return BLACK;
		case BLACK:
			return "\1K";
		case "BLUE":
		case "\1B":
			return BLUE;
		case BLUE:
			return "\1B";
		case "CYAN":
		case "\1C":
			return CYAN;
		case CYAN:
			return "\1C";
		case "RED":
		case "\1R":
			return RED;
		case RED:
			return "\1R";
		case "GREEN":
		case "\1G":
			return GREEN;
		case GREEN:
			return "\1G";
		case "BROWN":
		case "\1Y":
			return BROWN;
		case BROWN:
			return "\1Y";
		case "MAGENTA":
		case "\1M":
			return MAGENTA;
		case MAGENTA:
			return "\1M";
		case "BG_BLACK":
		case "\0010":
			return BG_BLACK;
		case BG_BLACK:
			return "\0010";
		case "BG_BLUE":
		case "\0014":
			return BG_BLUE;
		case BG_BLUE:
			return "\0014";
		case "BG_CYAN":
		case "\0016":
			return BG_CYAN;
		case BG_CYAN:
			return "\0016";
		case "BG_RED":
		case "\0011":
			return BG_RED;
		case BG_RED:
			return "\0011";
		case "BG_GREEN":
		case "\0012":
			return BG_GREEN;
		case BG_GREEN:
			return "\0012";
		case "BG_BROWN":
		case "\0013":
			return BG_BROWN;
		case BG_BROWN:
			return "\0013";
		case "BG_MAGENTA":
		case "\0015":
			return BG_MAGENTA;
		case BG_MAGENTA:
			return "\0015";
		case "BG_LIGHTGRAY":
		case "\0017":
			return BG_LIGHTGRAY;
		case BG_LIGHTGRAY:
			return "\0017";
		case "WHITE":
		case "\1W\1H":
			return WHITE;
		case WHITE:
			return "\1W\1H";
		case "LIGHTCYAN":
		case "\1C\1H":
			return LIGHTCYAN;
		case LIGHTCYAN:
			return "\1C\1H";
		case "LIGHTRED":
		case "\1R\1H":
			return LIGHTRED;
		case LIGHTRED:
			return "\1R\1H";
		case "LIGHTGREEN":
		case "\1G\1H":
			return LIGHTGREEN;
		case LIGHTGREEN:
			return "\1G\1H";
		case "LIGHTBLUE":
		case "\1B\1H":
			return LIGHTBLUE;
		case LIGHTBLUE:
			return "\1B\1H";
		case "YELLOW":
		case "\1Y\1H":
			return YELLOW;
		case YELLOW:
			return "\1Y\1H";
		case "LIGHTMAGENTA":
		case "\1M\1H":
			return LIGHTMAGENTA;
		case LIGHTMAGENTA:
			return "\1M\1H";
		case "LIGHTGRAY":
		case "\1N":
		case "\1W":
			return LIGHTGRAY;
		case LIGHTGRAY:
			return "\1W";
		case "DARKGRAY":
		case "\1K\1H":
			return DARKGRAY;
		case DARKGRAY:
			return "\1K\1H";
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
	return text.replace(/^\s*/,"").replace(/\s*$/,"");
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

	return data.sort(function(a,b) {
		if(a[prop] < b[prop])
			return -1;
		if(a[prop] > b[prop])
			return 1;
		return 0;
	});
}
