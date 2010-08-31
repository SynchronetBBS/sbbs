//#########################DICE ROLLING FUNCTIONS############################
var dice=loadDice();

function rollDice(n)
{	
	var total=0;
	for(var i=0;i<n;i++) {
		total+=rollDie();
		if(i<n-1) {
			console.up(2);
			console.right();
		}
	}
	return total;
}
function rollDie(value,attr,num)
{
	console.attributes=attr;
	console.pushxy();
	for(var r=0;num>0 && r<num;r++) {
		console.popxy();
		dice[random(6)+1].draw();
		mswait(10);
	}
	console.popxy();
	var roll=value?value:random(6)+1;
	dice[roll].draw();
	return roll;
}
function showRoll(rolls,attr,x,y)
{
	var total=0;
	console.gotoxy(x,y);
	console.pushxy();
	for(var r=0;r<rolls.length;r++) {
		rollDie(rolls[r],attr);
		total+=rolls[r];
		if(r<rolls.length-1) {
			console.popxy();
			console.right(4);
			console.pushxy();
		}
	}
	mswait(500);
	return total;
}
function loadDice()
{								//INITIALIZE SIX SIDED DICE OBJECTS
	var dice=[];
	for(d=1;d<=6;d++)
	{
		dice[d]=new Die(d);
	}
	return dice;
}

function Roll(pnum)
{
	this.pnum=pnum;
	this.rolls=[];
	this.total=0;
	this.roll=function(num)
	{
		this.rolls.push(num);
		this.total+=num;
	}
}
function Die(number)
{								//DICE CLASS
	this.number=number;
	this.line1="   ";
	this.line2="   ";
	this.line3="   ";
	this.dot="\xFE";

	if(this.number==2 || this.number==3)
	{
		this.line1=this.dot+"  ";
		this.line3="  "+this.dot;
	}
	if(this.number==4 || this.number==5 || this.number==6) 	
	{
		this.line1=this.dot+" "+this.dot;
		this.line3=this.dot+" "+this.dot;
	}
	if(this.number==1 || this.number==3 || this.number==5)
		this.line2=" "+this.dot+" ";
	if(this.number==6)	
		this.line2=this.dot+" "+this.dot;
	
	this.draw=function() 
	{
		console.putmsg(this.line1,P_SAVEATR);
		console.down();
		console.left(3);
		console.putmsg(this.line2,P_SAVEATR);
		console.down();
		console.left(3);
		console.putmsg(this.line3,P_SAVEATR);
	}
}
