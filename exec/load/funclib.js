/*
	Javascript Library of useful functions
	by MCMLXXIX 
*/

function getColor(color, intensity)
{									//TAKE A STRING AND RETURN THE CORRESPONDING ANSI COLOR CODE
	if(intensity=="high") inten="\1h";
	else inten="\1n";
	
	if(color=="black") return ("\1k" + inten);
	if(color=="grey") return ("\1h"+ inten);
	if(color=="cyan") return ("\1c"+ inten);
	if(color=="yellow") return ("\1y"+ inten);
	if(color=="green") return ("\1g"+ inten);
	if(color=="white") return ("\1w"+ inten);
	if(color=="red") return ("\1r"+ inten);
	if(color=="blue") return ("\1b"+ inten);
	if(color=="magenta") return ("\1m"+ inten);
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
	var padded="\1k";
	for(p=0;p<padlength;p++) padded+=padding;
	if(justification=="left") newstring+=(padded);
	if(justification=="right") newstring=(padded + newstring);
	return(newstring);
}
function ClearLine(length,x,y)
{
	if(x && y) console.gotoxy(x,y);
	if(length) printf("%*s",length,"");
	else console.cleartoeol();
}
