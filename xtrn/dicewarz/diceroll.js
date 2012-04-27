//#########################DICE ROLLING FUNCTIONS############################
function	rollDice(a,b,dice)
{								//MAIN DICE ROLLING FUNCTION
	var totals=[0,0];
	var x=menuColumn;		
	var y=16;

	var bc=console.ansi(BG_RED);
	var fc=console.ansi(LIGHTGRAY);

	fancyRoll(a,x,y,fc,bc,dice);
	var xx=x;
	var yy=y;
	
	for(var aa=0;aa<a;aa++) {
		var rand=random(6)+1;
		while(rand < 1 || rand > 6)
			rand = random(6)+1;
		dice[rand].display(xx,yy,fc,bc);
		xx+=4;
		totals[0]+=rand;
	}
	mswait(50);

	y+=4;
	bc=console.ansi(BG_LIGHTGRAY);
	fc=console.ansi(RED);

	fancyRoll(b,x,y,fc,bc,dice);
	xx=x;
	yy=y;
	for(var bb=0;bb<b;bb++) {
		var rand=random(6)+1;
		while(rand < 1 || rand > 6)
			rand = random(6) + 1;
		totals[1]+=rand;
		if(!dice[rand]) {
			log(LOG_ERROR,"dice error: " + rand);
		}
		dice[rand].display(xx,yy,fc,bc);
		xx+=4;
	}
	console.gotoxy((menuColumn+1),24);
	console.cleartoeol();
	printf("\1n\1r\1hAttacker: " + totals[0] + "\1n   Defender: " + totals[1]);
	return totals;
}
function	fancyRoll(qty,x,y,fc,bc,dice)
{								//"ROLLING DICE" DISPLAY
	for(var roll=0;roll<8;roll++) {
		var xx=x;
		var yy=y;
		for(var dr=0;dr<qty;dr++) {
			var num = random(6) + 1;
			while(num < 1 || num > 6)
				num = random(6) + 1;
			dice[num].display(xx,yy,fc,bc);
			xx+=4;
		}
		mswait(40);
	}
}
function	Die(number)
{								//DICE CLASS
	this.number=number;
	this.line1="   ";
	this.line2="   ";
	this.line3="   ";
	this.dot="\xFE";

	if(this.number==2 || this.number==3) {
		this.line1=this.dot+"  ";
		this.line3="  "+this.dot;
	}
	if(this.number==4 || this.number==5 || this.number==6) {
		this.line1=this.dot+" "+this.dot;
		this.line3=this.dot+" "+this.dot;
	}
	if(this.number==1 || this.number==3 || this.number==5)
		this.line2=" "+this.dot+" ";
	if(this.number==6)	
		this.line2=this.dot+" "+this.dot;
	this.display=function(x,y,b,f) {
		console.gotoxy(x,y); y++;
		printf(f + b + this.line1);
		console.gotoxy(x,y); y++;
		printf(f + b + this.line2);
		console.gotoxy(x,y);
		printf(f + b + this.line3);
		printf(blackbg);
	}
}
function	loadDice()
{								//INITIALIZE SIX SIDED DICE OBJECTS
	var dice=[];
	for(var d=1;d<=6;d++) 
		dice[d]=new Die(d);
	return dice;
}
