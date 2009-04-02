/*
	Javascript Library of useful functions
	by MCMLXXIX 
*/

function getColor(color, intensity)
{									//TAKE A STRING AND RETURN THE CORRESPONDING ANSI COLOR CODE
	if(intensity=="high") inten="\1h";
	else inten="\1n";

	switch(color)
	{
		case "black":
			return(inten + "\1k");
		case "grey":
			return(inten?("\1n" + inten):"\1n");
		case "cyan":
			return(inten + "\1c");
		case "yellow":
			return(inten + "\1y");
		case "green":
			return(inten + "\1g");
		case "white":
			return(inten + "\1w");
		case "red":
			return(inten + "\1r");
		case "blue":
			return(inten + "\1b");
		case "magenta":
			return(inten + "\1m");
		default:
			return("\1n");
	}
}
function GetLastWord(text)
{
	last_word=truncsp(text.substr(text.lastIndexOf(" ")));
	return RemoveSpaces(last_word);
}
function GetFirstWord(text)
{
	first_word=truncsp(text.substring(0,text.indexOf(" ")));
	return RemoveSpaces(first_word);
}
function RemoveFirstWord(text)
{
	message=truncsp(text.substring(text.indexOf(" ")+1));
	return message;
}
function RemoveSpaces(text)
{
	while(text.indexOf(" ")==0) text=text.substr(1);
	return truncsp(text);
}
function PrintPadded(string,length,padding,justification)
{
	if(!padding) padding=" ";
	if(!justification) justification="left";
	var padlength=length-console.strlen(string);
	var newstring=string;
	var padded="";
	for(p=0;p<padlength;p++) padded+=padding;
	if(justification=="left") newstring+=(padded);
	if(justification=="right") newstring=(padded + newstring);
	return(newstring);
}
function DrawLine(x,y,length,color)
{
	console.gotoxy(x,y);
	for(i=0;i<length;i++)
	{
		console.putmsg((color?color:"\1k\1h") + "\xc4");
	}
}
function ClearLine(length,x,y)
{
	write(console.ansi(ANSI_NORMAL));
	if(x && y) console.gotoxy(x,y);
	if(length) printf("%*s",length,"");
	else console.cleartoeol();
}
