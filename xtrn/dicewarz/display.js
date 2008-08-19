//##########################DISPLAY FUNCTIONS#################################
function 	ClearLine(row,toColumn)
{
	console.gotoxy(1,row);
	for(col=0;col<toColumn;col++)
	{
		console.putmsg(" ");
	}
}
function	ClearArea(fromRow,fromColumn,qty)
{
	xx=fromColumn;
	yy=fromRow;
	for(ccc=0;ccc<qty;ccc++)
	{
		console.gotoxy(xx,yy); yy++;
		console.cleartoeol();
	}
}
function	Wrap(msg,lst)
{
	console.pushxy();
	console.putmsg(msg);
	console.popxy();
	console.right(29);
	console.putmsg("\1w\1h: ");
	var col=32;
	var delimiter="\1n\1g,";
	for(aa=0;aa<lst.length;aa++)
	{
		if(aa==lst.length-1) delimiter="";
		var item=lst[aa]+delimiter;
		if((col + console.strlen(item))>79) {
			console.crlf();
			console.right(31);
			col=32;
		}
		console.putmsg("\1h\1g" + item);
		col += console.strlen(item);
	}
	console.crlf();
}
function 	PrintPadded(string,length,padding,justification)
{
	var padlength=length-console.strlen(string);
	var newstring=string;
	var padded="\1k";
	for(p=0;p<padlength;p++) padded+=padding;
	if(justification=="left") newstring+=(padded);
	if(justification=="right") newstring=(padded + newstring);
	return(newstring);
}
function 	DrawLine(color,length,character)
{
	var printchar= "\xC4";
	if(character) printchar=character;
	for(i=0;i<length;i++) console.putmsg(color + printchar);
}
function 	DrawVerticalLine(color/*, side*/)
{
	var ly=1;
	var lx=menuColumn-1;
	
/*	OPTIONAL CODE FOR BOXING IN GAME AREA
	
	var topside="\xB4";
	var bottomside="\xD9";
	
	//PARAMETER FOR DRAWING LEFT-HAND OR RIGHT-HAND SIDE OF BOXED IN AREA
	if(side) 
	{
		lx=1;
		topside="\xC3";
		bottomside="\xC0";
	}
 	console.gotoxy(lx,ly); ly++
	console.putmsg(color + "\xB3");
	console.gotoxy(lx,ly); ly++
	console.putmsg(color + topside); 
*/
	for(;ly<=24;ly++)
	{
		console.gotoxy(lx,ly);
		console.putmsg(color + "\xBA");
	}
//	console.gotoxy(lx,ly);
//	console.putmsg(color + bottomside);
}
function 	WipeCursor(lr)			//SEND CURSOR TO BOTTOM RIGHT CORNER OF SCREEN
{	
	if(lr=="left") { side=1; row=24; }
	else { side=79; row=1; }
	console.gotoxy(side,row);
}
function 	GetColor(color, intensity)
{									//TAKE A STRING AND RETURN THE CORRESPONDING ANSI COLOR CODE
	if(intensity=="high") inten="\1h";
	else inten="\1n";
	
	if(color=="black") return (inten+"\1k");
	if(color=="grey") return ("\1k\1h");
	if(color=="cyan") return (inten+"\1c");
	if(color=="yellow") return (inten+"\1y");
	if(color=="green") return (inten+"\1g");
	if(color=="white") return (inten+"\1w");
	if(color=="red") return (inten+"\1r");
	if(color=="blue") return (inten+"\1b");
	if(color=="magenta") return (inten+"\1m");
    else return false;
}
function 	QueueMessage(message,x,y)
{
	messages.push({'msg':message,'x':x,'y':y});
}
function 	DisplayMessages()
{
	for(mess in messages) {
		var x=messages[mess].x;
		var y=messages[mess].y;
		var msg=messages[mess].msg;
		
		console.gotoxy(x,y+1);
		console.putmsg(msg);
		console.cleartoeol();
		mswait(500);
	}
	messages=[];
}
function 	PutMessage(message,x,y)
{
	console.gotoxy(x,y+1);
	console.putmsg(message);
	mswait(750);
	console.gotoxy(x,y+1);
	console.cleartoeol();
}
function	ShowWinner(g)
{
	ClearArea(16,menuColumn,9);
	console.gotoxy(51,17);
	console.putmsg("\1n\1r\1hThis game was won by: ");
	console.gotoxy(53,18);
	if(g.winner>=0)
	{
		console.putmsg("\1n\1r\1h" + system.username(g.winner));
	}
	else
	{
		console.putmsg("\1n\1r\1hcomputer player");
	}
	WipeCursor("left");
}
