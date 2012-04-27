//##########################DISPLAY FUNCTIONS#################################
function 	clearLine(row,toColumn)
{
	console.gotoxy(1,row);
	console.putmsg(format("%*s", toColumn-1, ""));
}

function	clearArea(fromRow,fromColumn,qty)
{
	var count;

	console.gotoxy(fromColumn, fromRow);
	for(var count=0;count < qty; count++) {
		console.cleartoeol();
		console.down();
	}
}

function	wrap(msg,lst)
{
	console.pushxy();
	console.putmsg(msg);
	console.popxy();
	console.right(29);
	console.putmsg("\1w\1h: ");
	var col=32;
	var delimiter="\1n\1g,";
	for(var aa=0;aa<lst.length;aa++)
	{
		if(aa==lst.length-1) 
			delimiter="";
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

function 	printPadded(string,length,padding,justification)
{
	var padlength=length-console.strlen(string);
	var newstring=string;
	var padded="\1k";

	for(p=0;p<padlength;p++)
		padded+=padding;
	if(justification=="left")
		newstring+=(padded);
	if(justification=="right")
		newstring=(padded + newstring);
	return(newstring);
}

function 	drawLine(color,length,character)
{
	var printchar= "\xC4";

	if(character)
		printchar=character;
	for(i=0;i<length;i++)
		console.putmsg(color + printchar);
}

function 	drawVerticalLine(color/*, side*/)
{
	var ly=1;
	var lx=menuColumn-1;
	for(;ly<=24;ly++) {
		console.gotoxy(lx,ly);
		console.putmsg(color + "\xBA");
	}
}

function 	wipeCursor(lr)			//SEND CURSOR TO BOTTOM RIGHT CORNER OF SCREEN
{	
	var side = console.screen_columns;
	var row = 1;
	if(lr=="left") { 
		side=1; 
		row=console.screen_rows; 
	}
	console.gotoxy(side,row);
}

function 	queueMessage(message)
{
	messages.push(message);
}

function 	displayMessages()
{
	var x=30;
	var y=20;
	var fileName=root + user.alias + ".usr";
	if(file_size(fileName)>0) {
		var file=new File(fileName);
		file.open('r',false);
		messages.concat(file.readAll());
		file.truncate();
		file.close();
	}
	
	while(messages.length) {
		console.gotoxy(x,y);
		console.putmsg(messages.shift());
		console.cleartoeol();
		mswait(500);
	}
}

function 	putMessage(message,x,y)
{
	console.gotoxy(x,y+1);
	console.putmsg(message);
	mswait(750);
	console.gotoxy(x,y+1);
	console.cleartoeol();
}

function	showWinner(g)
{
	console.gotoxy(51,18);
	console.putmsg("\1n\1r\1hThis game was won by: ");
	console.gotoxy(53,19);
	if(g.winner>=0)	{
		console.putmsg("\1n\1r\1h" + system.username(g.winner));
	}
	else {
		console.putmsg("\1n\1r\1hcomputer player");
	}
	wipeCursor("left");
}
