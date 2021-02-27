/*********************************
 *		STAR STOCKS (2007) 		 *
 *		for Synchronet v3.15 	 *
 *		by Matt Johnson			 *
 *********************************/

load("sbbsdefs.js");
load("funclib.js");
load("graphic.js");

var oldpass=console.ctrlkey_passthru;
console.ctrlkey_passthru="+ACGKLOPQRTUVWXYZ";
bbs.sys_status|=SS_MOFF;
client.subscribe("starstocks","scores");

const 	cfgname=			"stars.cfg";  
const 	space=				"\xFA";
var 	max_companies=		6;
var 	max_turns=			80;
const	starting_cash=		10000;
const  	interest_rate=		.05;
const  	ccolor=				"\1w\1h"
var   	partial_company=	"\1n+";
var 	difficulty=			1;
var 	min_difficult=		20;
var 	max_difficult=		35;
var 	scores=				[];	
var 	border_row=			4;
var  	border_column=		1;
var		menu_row=			1;
var		menu_column=		63;
var 	columns=
		rows=
		min_stars=
		max_stars=
		bcolor=
		starcolor=
		star=
		scolor="";
		
loadSettings();
loadScores();

var 	min_normal=		min_stars;
var 	max_normal=		max_stars;
var 	game;

gameMenu();
client.unsubscribe("starstocks","scores");
quit();

//########################## MAIN FUNCTIONS ###################################

function 	gameMenu()
{
	console.clear();
	console.printfile(root + "starstocks.ans");
	if(!(user.settings & USER_PAUSE)) console.pause();
	var mx=menu_column; var my=menu_row;
	var gMenu=new Menu(		"\1h\1c: \1wSTAR STOCKS"		,mx,my);
	var menu_items=[		"~Play Game"					, 
							"~Difficulty"					,
							"~High Scores"					,
							"~Instructions"					,
							"~Redraw Screen"				,
							"~Quit"							];
	gMenu.add(menu_items);
	redraw();
	while(1)
	{
		gMenu.display();
		console.gotoxy(79,24);
		var cmd=console.getkey((K_NOECHO,K_NOCRLF,K_UPPER));
		switch(cmd)
		{
		case "P":
			redraw(true);
			clearRightColumn(10);
			clearRows(23);
			playGame();
			displayHeader();
			break;
		case "D":
			resized=mapResize(mx,my); 
			if(!resized) 
			{
				console.clear();
				displayHeader();
				displayGrid("\xFA");
				displayBorder(bcolor);
			}
			game=false;
			continue;
		case "I":
			viewInstructions();
			redraw();
			continue;
		case "H":
			viewHighScores();
			redraw();
			continue;
		case "R":
			redraw();
			continue;
		case "Q": 
			return;
		}
		cycle();
	}
}
function 	loadScores()
{ 
	scores = client.read("starstocks","scores",1);
	if(!scores)
		scores = {};
}
function	storeScore(score)
{
	var loc = "scores." + score.difficulty + "." + score.player;
	client.lock("starstocks",loc,2);
	var currscore=client.read("starstocks",loc);
	if(!currscore || score.score > currscore.score) {
		client.write("starstocks",loc,score);
		if(!scores[score.difficulty])
			scores[score.difficulty] = {};
		scores[score.difficulty][score.player] = score;
	}
	client.unlock("starstocks",loc);
}
function 	loadSettings()
{ 
	var dfile=new File(root + cfgname);
	dfile.open('r', false);
	var count=0;
	var timeout=200;
	while(!dfile.is_open && count<timeout)
	{
		mswait(5);
		count++;
		dfile.open('r', true);
	}
	if(dfile.is_open)
	{
		columns=parseInt(dfile.readln());
		rows=parseInt(dfile.readln());
		min_stars=parseInt(dfile.readln());
		max_stars=parseInt(dfile.readln());
		bcolor=getColor(dfile.readln());
		starcolor=getColor(dfile.readln());
		star=dfile.readln();
		scolor=getColor(dfile.readln());
		maxturns=dfile.readln();
		dfile.close();
	}
	else
	{
		log("unable to load game settings");
		exit();
	}
}
function 	mapResize(x,y)
{
	var cont=true;
	while(cont)
	{
		var rMenu=new Menu(		"SET DIFFICULTY"	,x,y);
		var menu_items=[		"~1 Easy"			, 
								"~2 Normal"			, 
								"~3 Difficult"		];
		rMenu.add(menu_items);
		rMenu.display();
		var cmd=console.getkey((K_NOSPIN,K_NOECHO,K_NOCRLF,K_UPPER));
		if(cmd==undefined || cmd=="") break;
		else if(cmd=="1") {rows=10; columns=25; cont=false; border_row=6; border_column=5; max_turns=60; max_companies=5; max_stars=max_normal; min_stars=min_normal;}
		else if(cmd=="2") {rows=15; columns=30; cont=false; border_row=4; border_column=1; max_turns=80; max_companies=6; max_stars=max_normal; min_stars=min_normal;}
		else if(cmd=="3") {rows=19; columns=30; cont=false; border_row=2; border_column=1; max_turns=100; max_companies=8; max_stars=max_difficult; min_stars=min_difficult;}
		else break;
		difficulty=(cmd-1);
	}
	
	return(cont);
}
function 	playGame()
{
	//var mx=((columns*2) + border_column + 2); var my=2;    //FOR FLEXIBLE MENU LOCATION
	var mx=menu_column; var my=menu_row;
	var pMenu=new Menu(		"\1h\1c: \1wSTOCK OPTIONS"	,mx,my);
	var menu_items=[		"~Start Game"				,
							"~New Game"					,
							"~Redraw Screen"			, 
							"~View Summary"				, 
							"~Abandon Company"			,
							"~Xpert Toggle"				,
							"~Quit"						];
	pMenu.add(menu_items);
	game=new Map(columns,rows);
	var continue_game=true;
	var turn=1;
	pMenu.disable("V");
	pMenu.disable("A");

	while(continue_game && turn<=max_turns)
	{
		while(continue_game)
		{
			pMenu.display();
			displayHeader(turn);
			if(!game.options.length && game.inProgress) 
				game.generateCompanies();
			if(game.options.length)
			{
				message("\1r\1hChoose a location to start your company.");
			}
			var cmd=console.getkey(K_NOCRLF|K_NOSPIN|K_UPPER|K_NOECHO|K_COLD);	
			if(!(pMenu.disabled[cmd]))
			{
				switch(cmd)
				{
				case "1":
				case "2":
				case "3":
				case "4":
				case "5":
				case "6":
				case "7":
				case "8":
				case "9":
					if(game.inProgress)
					{
						processSelection(cmd-1);
						turn++;
						break;
					}
					continue;
				case "A":
					abandonCompany();
					continue;
				case "V":
					displaySummary();
					continue;
				case "S":
					if(!(game.inProgress)) startGame(pMenu,turn);
					break;
				case "N":
					if(newGame(pMenu)) turn=1;
					clearRightColumn(10);
					clearRows(23);
					continue;
				case "R":
					redraw();
					continue;
				case "X":
					if(game.xpert) 
					{
						game.xpert=false;
						message("\1y\1hExpert mode OFF",63,24,16);
					}
					else 
					{
						game.xpert=true;
						message("\1y\1hExpert mode ON",63,24,16);
					}
					continue;
				case "Q":
					if(game.inProgress)
					{
						console.home();
						console.cleartoeol();
						if(!(console.noyes("End this game?")))
						{
							game.clearOptions();
							clearRows(23);
							continue_game=false;
						}
					}
					else 
					{
						continue_game=false;
						delete game;
					}
					if(continue_game) 
						continue;
					else 
						break;
				default:
					continue;
				}
				break;
			}
		}
		if(game.inProgress && continue_game)
		{
			if(countCompanies(game.companies)>0) 
			{
				if(pMenu.disabled["V"]) pMenu.enable("V");
				if(pMenu.disabled["A"]) pMenu.enable("A");
				game.displayCompanies();
				displayStocks();
				buyStock();
			}
			else 
			{
				pMenu.disable("V");
				pMenu.disable("A");
			}
			displayStocks();
			getMoney();
			showMeTheMoney();
			if(turn>max_turns) 
				game.inProgress=false;
		}
		cycle();
	}
	if(countCompanies(game.companies))
	{
		var useralias = user.alias.replace(/\./g,"_");
		var score=new Score(useralias,game.networth,difficulty,system.datestr());
		storeScore(score);
		game.completed=true;
		displaySummary();
	}
}
function 	cycle() 
{
	client.cycle();
}
function 	processUpdates(update) 
{
	switch(update.operation) {
	case "ERROR":
		throw(update.error_desc);
		break;
	case "UPDATE":
		var p=update.location.split(".");
		/* if we received something for a different game? */
		if(p.shift().toUpperCase() != "STARSTOCKS") 
			return;
		var obj=scores;
		p.shift();
		while(p.length > 1) {
			var child=p.shift();
			obj=obj[child];
		}
		obj[p.shift()] = update.data;
		break;
	}
}
client.callback=processUpdates;

//########################## DISPLAY FUNCTIONS #################################
function	shortNumber(number)
{
	var newnum="";
	Num = "" + parseInt(number);
	if(Num.length<=3) return Num;
	if(Num.length>3 && Num.length<7) 
	{
		cut=Num.length-3;
		newnum=Num.substring(0,cut);
		if(cut==1 && Num.charAt(cut)!=0) newnum+=("." + Num.charAt(cut));
		newnum+="k";
	}
	else if(Num.length>=7 && Num.length<10) 
	{
		cut=Num.length-6;
		newnum=Num.substring(0,cut);
		if(cut==1 && Num.charAt(cut)!=0) newnum+=("." + Num.charAt(cut));
		newnum+="m";
	}
	else if(Num.length>=10 && Num.length<13) 
	{
		cut=Num.length-9;
		newnum=Num.substring(0,cut);
		if(cut==1 && Num.charAt(cut)!=0) newnum+=("." + Num.charAt(cut));
		newnum+="b";
	}
	else return "?";
	return newnum;
}
function 	sortScores(scores)
{ 
	var list=[];
	for each(var s in scores) {
		list.push(s);
	}
	// The Bubble Sort method.
	for(n = 0; n < list.length; n++) {
		for(m = 0; m < (list.length-1); m++) {
			if(list[m].score < list[m+1].score) {
				holder = list[m+1];
				list[m+1] = list[m];
				list[m] = holder;
			}
		}
	}
	return list;
}
function	viewHighScores()
{
	loadScores();
	var difficulties={0:"Easy",1:"Normal",2:"Difficult"};
	for(var d in difficulties)
	{
		var scoreList = sortScores(scores[d]);
		console.clear();
		console.down(3);
		console.putmsg("   \1w\1hHIGH SCORES: \1c\1h" + difficulties[d]);
		console.crlf();
		console.crlf();
		for(hs=0;hs<scoreList.length && hs<10;hs++)
		{
			console.putmsg("  " + printPadded("\1w\1h" + (hs+1) + ": ",4," ","right")); 
			console.putmsg(printPadded("\1y\1h" + scoreList[hs].player,20," ","left"));
			console.putmsg("\1c\1h$" + printPadded( "\1w\1h" + dollarAmount(scoreList[hs].score),18,"\1h\1k.","right"));
			console.putmsg("\1n\1g  " + scoreList[hs].date);
			console.crlf();
		}
		console.gotoxy(1,24);
		console.pause();
		console.aborted=false;
	}
}
function	viewInstructions()
{
	console.clear();
	console.printfile(root + "stars.doc");
	if(!(user.settings & USER_PAUSE)) console.pause();
}
function	clearRightColumn(from)
{
	var clear=24-from;
	xx=menu_column;
	yy=from;
	for(ccc=0;ccc<clear;ccc++)
	{
		console.gotoxy(xx,yy); yy++;
		console.cleartoeol();
	}
}
function 	clearRows(from)
{
	var clear=24-from;
	xx=1;
	yy=from;
	for(ccc=0;ccc<=clear;ccc++)
	{
		console.gotoxy(xx,yy); yy++;
		console.cleartoeol();
	}
}
function	displaySummary()
{
	console.clear();
	console.gotoxy(2,2);
	console.putmsg("\1g\1h[ STOCK SUMMARY ]\r\n\1h\1k");
	drawLine(false,false,79); console.crlf();
	var sorted=sortCompanies();
	for(c=0;c<sorted.length;c++)
	{
		game.companies[sorted[c]].displayLong();
		console.crlf();
	}
	showMeTheMoney();
	console.gotoxy(66,24);
	console.pause();
	redraw();
}
function 	redraw(grid_only)
{
	console.aborted=false;
	if(grid_only) 
	{	
		displayGrid("\xFA");
		return;
	}
	console.clear();
	displayHeader();
	displayGrid("\xFA");
	displayBorder(bcolor);
	if(!game) return;
	if(game.star_data) game.display(game.star_data);
	if(game.inProgress || game.completed) 
	{
		if(game.partial_data) game.display(game.partial_data);
		if(game.companies) game.displayCompanies();
		showMeTheMoney();
		displayStocks();
	}
	if(game.inProgress && game.options) game.displayOptions();
}
function 	displayHeader(turn)
{
	console.home();
	var alert="";
	if((max_turns-turn)<6) alert="\1h\1r";
	
	console.putmsg("\1w\1h[\1nSTAR STOCKS\1h]   :  \1r [\1n\1r2007 \1h-\1n\1r Matt Johnson\1h]   \1w:   ");
	if(turn) console.putmsg("\1w\1hTurn \1n[\1h" + alert + turn + "\1n/\1h" + max_turns + "\1n]" );
	wipeCursor();
}
function 	displayGrid(space)
{
	var y=border_row+1;
	var x=border_column+1; 
	console.attributes=scolor;
	for(row=0;row<rows;row++)
	{ 
		for(column=0;column<columns;column++)
		{
			console.gotoxy(x+(column*2),row+y);
			console.putmsg(space,P_SAVEATR);
		}
	}
}
function 	displayStocks()
{
	var num_companies=countCompanies(game.companies);
	if(!num_companies>0) return;
	var xx=menu_column; var yy=menu_row+9;
	var clear=max_companies;
	var cleared=0;
	console.gotoxy(xx,yy); yy++
	console.putmsg("\1h\1c: \1wPORTFOLIO");
	console.gotoxy(xx,yy); yy++;
	console.cleartoeol();
	drawLine(false,false,17);
	var sorted=sortCompanies();
	for(c=0;c<sorted.length;c++)
	{
		console.cleartoeol();
		game.companies[sorted[c]].display(xx,yy);
		yy++;
		cleared++;
	}
	for(;cleared<clear;cleared++)
	{
		console.gotoxy(xx,yy); yy++;
		console.cleartoeol();
	}
}
function 	displayBorder(color)
{
	var x=border_column; 
	var y=border_row;
	console.gotoxy(x,y);
	console.attributes=color;
	console.putmsg("\xDA",P_SAVEATR);
	drawLine(false,false,(columns*2)-1);
	console.putmsg("\xBF",P_SAVEATR);
	y++;
	for(row=0;row<rows;row++)
	{ 
		console.gotoxy(x,y);
		console.putmsg("\xB3",P_SAVEATR);
		console.gotoxy(x+(columns*2),y);
		console.putmsg("\xB3",P_SAVEATR);
		y++;
	}
	console.gotoxy(x,y);
	console.putmsg("\xC0",P_SAVEATR);
	drawLine(false,false,(columns*2)-1);
	console.putmsg("\xD9",P_SAVEATR);
}
function 	wipeCursor()					//SEND CURSOR TO BOTTOM RIGHT CORNER OF SCREEN
{											
	console.gotoxy(79,24);
	console.putmsg("\1n\1k");
}
function 	message(msg,x,y,cc)  			//ALERT MESSAGE OUTPUT FUNCTION
{
	var clear=cc?cc:59;
	console.gotoxy(x?x:1,y?y:23);
	console.putmsg(msg);
	cleaned=strip_ctrl(msg);
	clear-=cleaned.length;
	for(z=0;z<clear;z++)
	{
		console.putmsg(" ");
	}
	wipeCursor();
}
function	sortCompanies()
{
	var sorted=[];
	for(cc in game.companies)
	{
		sorted.push(cc);
	}
	sorted.sort();
	return sorted;
}

//########################## MISC FUNCTIONS ###################################
function 	dollarAmount(number) 
{ 
	var Num="" + number.toFixed(2);
	dec = Num.indexOf(".");
	end = ("" + Num.substring(dec,Num.length));
	Num = "" + parseInt(Num);
	var temp1 = "";
	var temp2 = "";

	var count = 0;
	for (var k = Num.length-1; k >= 0; k--) 
	{
		var oneChar = Num.charAt(k);
		if (count == 3) 
		{
			temp1 += ",";
			temp1 += oneChar;
			count = 1;
			continue;
		}
		else 
		{
			temp1 += oneChar;
			count ++;
		}
	}
	for (var k = temp1.length-1; k >= 0; k--) 
	{
		var oneChar = temp1.charAt(k);
		temp2 += oneChar;
	}
	temp2+=end;
	return temp2;
}
function	abandonCompany()
{
	while(1)
	{
		console.gotoxy(1,23);
		message("\1r\1hAbandon which company? or hit [ENTER] to cancel: ");
		wipeCursor();

		var cmd=console.getkey(K_NOCRLF|K_NOSPIN|K_UPPER|K_NOECHO|K_COLD);	
		if(cmd=="\r" || cmd==undefined) break;
			switch(cmd)
			{
			case "A":
			case "B":
			case "C":
			case "D":
			case "E":
			case "F":
			case "G":
			case "H":
				if(!game.companies[cmd]) continue;
				game.abandonCompany(cmd);
				console.gotoxy(1,23);
				console.cleartoeol();
				break;
			case "V":
				displaySummary();
				continue;
			default:
				break;
			}
		break;
	}
	displayStocks();
}
function	buyStock()
{
	while(1)
	{
		if(!canBuyStocks()) break;
		message("\1c\1hBuy stock in which company? or hit [\1n\1cENTER\1h] to cancel\1n\1c: ",1,23,79);
		var cmd=console.getkey(K_NOCRLF|K_NOSPIN|K_UPPER|K_NOECHO|K_COLD);	
		if(cmd=="\r" || cmd==undefined) break;
			switch(cmd)
			{
			case "A":
			case "B":
			case "C":
			case "D":
			case "E":
			case "F":
			case "G":
			case "H":
				if(!game.companies[cmd]) continue;
				if(!game.xpert)
				{
					game.companies[cmd].shares+=(game.cash/game.companies[cmd].stock_value);
					game.cash=game.cash%game.companies[cmd].stock_value;
					console.gotoxy(1,23);
					console.cleartoeol();
					break;
				}
				else 
				{
					if(game.cash<game.companies[cmd].stock_value) continue;
					buyStockXpert(cmd);
					continue;
				} 
			case "V":
				displaySummary();
				continue;
			default:
				continue;
			}
		break;
	}
	displayStocks();
}
function	buyStockXpert(cmd)
{
	var max=parseInt(game.cash/game.companies[cmd].stock_value);
	console.gotoxy(1,23);
	console.cleartoeol();
	console.putmsg("\1c\1hBuy how many stocks in company \1y" + cmd + "\1c? [\1n\1cMAX \1y\1h" + max + "\1c]\1n\1c:");
	var num=console.getnum(max);
	game.cash-=(game.companies[cmd].stock_value*num);
	game.companies[cmd].shares+=num;	
}
function	canBuyStocks()
{
	for(cb in game.companies) {
		if(game.cash>=game.companies[cb].stock_value) return true;
	}
	return false;
}
function 	getMoney()
{
	game.cash=game.getCash()
	game.networth=game.getNetWorth()+game.cash;
}
function	showMeTheMoney()
{
	var money;
	money="\1c\1hNetworth\1w: \1c$\1w" + dollarAmount(game.networth);
	money+=" \1c\1hCash\1w: \1c$\1w" + dollarAmount(game.cash);
	message(money,1,24,58);
}
function 	countCompanies(companies)
{									//COUNT THE NUMBER OF COMPANIES CURRENTLY USED
	var count=0;
	for(i in companies)
	{
		count++;
	}
	return count;
}
function 	newGame(menu)
{									//RESET GAME MAP, CLEAR GAME AND COMPANY DATA
	if(game.inProgress) 
	{
		console.home();
		console.cleartoeol();
		if(console.noyes("End this game?")) return false;
		else 
		{
			menu.enable("S");
			game.clearOptions();
			menu.disable("V");
			menu.disable("A");
		}
	}
	game.clearGrid(game.star_data);
	game.clearGrid(game.partial_data);
	for(i in game.companies)
	{
		game.clearGrid(game.companies[i].data);
	}
	game=new Map(columns,rows);
	game.display(game.star_data);
	return true;
}
function 	startGame(menu,turn)
{									//BEGIN GAMEPLAY CYCLE
	menu.disable("S");
	displayHeader(turn);
	game.inProgress=true;
}
function 	maxCompanies()
{									//DETERMINE WHETHER CURRENT COMPANIES ARE AT MAXIMUM ALLOWABLE AMOUNT
	var count=countCompanies(game.companies);
	if(count<max_companies) return false;
	return true;
}
function 	processSelection(cmd)
{									//DETERMINE THE CONTENTS OF THE AREA SURROUNDING COMPANY SELECTION, AND UPDATE COMPANY DATA
	if(cmd<game.options.length)	
	{
		console.gotoxy(1,24);
		console.clearline();
		var location=game.options[cmd];
		game.options.splice(cmd,1);
		var prox=game.sortProximity(location);
		processNearby(location,prox);
		game.grid[location]=true;
		game.clearOptions();
		game.display(game.partial_data);
		showMeTheMoney();
	}
}
function 	processNearby(location,prox)
{									//USED WITH processSelection, WHEN SURROUNDING AREA IS NOT EMPTY
	var nearby=prox;
	if(!nearby) 
	{
		game.partial_data[location]=partial_company;
		return;
	}
	var num_companies=countCompanies(nearby.companies);
	if(num_companies==1) game.addToCompany(location,nearby);
	else if(num_companies>1) game.mergeCompanies(location,nearby);
	else game.makeNewCompany(location,nearby);
}
function 	quit() 
{
	console.ctrlkey_passthru=oldpass;
	bbs.sys_status&=~SS_MOFF;
	console.clear();
	var splash_filename=root + "exit.bin";
	if(!file_exists(splash_filename)) exit();
	
	var splash_size=file_size(splash_filename);
	splash_size/=2;		
	splash_size/=80;	
	var splash=new Graphic(80,splash_size);
	splash.load(splash_filename);
	splash.draw();
	
	console.gotoxy(1,23);
	console.center("\1n\1c[\1hPress any key to continue\1n\1c]");
	while(console.inkey(K_NOECHO|K_NOSPIN)==="");
	console.clear();
}
//########################## CLASSES #########################################

function 	Map(c,r) 				
{								//MAP CLASS
	this.grid=[]; 						//ROWS * COLUMNS IN LENGTH, TRACKS GRID OCCUPANCY (BOOLEAN)
	this.star_data=[]; 					//STORES GRID[] INDICES FOR FASTER RETRIEVAL & MODIFICATION (SPARSE)
	this.partial_data=[];				//STORES GRID[] INDICES FOR FASTER RETRIEVAL & MODIFICATION (SPARSE)
	this.companies=[];					//STORES COMPANY DATA (ASSOCIATIVE)
	this.max_options=6;					//NUMBER OF OPTIONS FOR COMPANY CREATION
	this.cash=starting_cash;			//INITIALIZE USER SPENDING MONEY
	this.networth=0;
	this.inProgress=false;				//GAME STATUS INDICATOR
	this.completed=false;
	this.xpert=false;
	this.rows=r;						//MAP ROWS
	this.columns=c;						//MAP COLUMNS
	this.map_row=border_row+1;
	this.map_column=border_column+1;

									//####CLEARED EVERY TURN####
	this.options=[];					//TEMPORARY GRID INDEX STORAGE FOR INITIAL COMPANY SELECTION
	this.proximity=[];					//TEMPORARY STORAGE FOR COORDINATE PROXIMITY CHECKING

	//DISPLAY FUNCTIONS
	this.displayCompanies=		function()
	{										//DISPLAYS THE LOCATION DATA FOR EACH COMPANY
		for(i in this.companies)
		{
			for(a in this.companies[i].data)
			{
				location=a;
				this.getXY(location);
				console.putmsg(ccolor+this.companies[i].name);
			}
		}
	}
	this.displayOptions=		function()
	{										//DISPLAYS CONTENTS OF options GENERATED BY generateCompanies
		for(i=0;i<this.options.length;i++)
		{
			location=this.options[i];
			this.getXY(location);
			console.putmsg("\1h\1g"+(i+1));
		}
	}
	this.clearGrid=				function(array)
	{										//CLEARS SCREEN POSITIONS LISTED IN A SPARSE ARRAY
		console.attributes=scolor;
		for(i in array) 
		{
			this.getXY(i);
			this.grid[i]=false;
			console.putmsg(space,P_SAVEATR);
		}
		array=[];
		return(array);
	}
	this.clearOptions=			function()
	{										//CLEARS OPTIONS CREATED BY generateCompanies
		console.attributes=scolor;
		for(i=0;i<this.options.length;i++) 
		{
			this.getXY(this.options[i]);
			console.putmsg(space);
		}
		this.options=[];
	}
	this.display=				function(data)
	{										//TAKES A SPARSE ARRAY AND DISPLAYS THE CONTENTS
		console.attributes=starcolor;
		for(i in data)
		{
			location=i;
			this.getXY(location);
			console.putmsg(data[location],P_SAVEATR);
		}
	}	
	this.getXY=					function(place)
	{										//TAKES A GRID INDEX, AND RETURNS THE CORRESPONDING X AND Y COORDINATES FOR DISPLAY
		var index=place;
		x=this.map_column;
		y=this.map_row;
		x+=((index%this.columns)*2);
		y+=(parseInt(index/this.columns));
		console.gotoxy(x,y);
		return(0);
	}
	this.getIndex=				function(x,y)
	{										//DOES THE OPPOSITE OF getXY (NOT USED)
		index=(x*y)+x;
		return(index);
	}

	//DATA FUNCTIONS
	this.abandonCompany=		function(company)
	{
		this.clearGrid(this.companies[company].data);
		this.cash+=(.8 * (this.companies[company].networth)); //receive back 80% of company value in cash
		delete this.companies[company];
	}
	this.generateMap=			function()
	{										//RANDOMLY GENERATE A NEW MAP OF STARS 
		var num_stars=min_stars+random(max_stars-min_stars);
		for(i=0;i<num_stars;i++)
		{
			location=random(this.rows*this.columns);
			if(this.grid[location]) i--;
			else 
			{
				this.grid[location]=true;
				this.star_data[location]=star;
			}
		}
	}	
	this.generateCompanies=		function() 						
	{										//RANDOMLY GENERATE COMPANY SELECTION OPTIONS EACH TURN
		var max=maxCompanies();
		var temp=[];
		for(options=0;options<this.max_options;options++)
		{
			location=random(this.rows*this.columns);
			if(!this.grid[location] && !temp[location]) 
			{
				if(!max) 
				{
					this.options.push(location);
					temp[location]=true;
				}
				else 
				{
					var proxg=this.sortProximity(location);
					if(!proxg || countCompanies(proxg.companies))
					{
						this.options.push(location);
						temp[location]=true;
					}
					else options--;
				}
			}
			else options--;
		}
		this.displayOptions();
	}
	this.getNextCompany=		function()
	{										//FIND THE NEXT AVAILABLE COMPANY NAME
		c="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		var num_companies=countCompanies(this.companies);
		for(i=0;i<=num_companies;i++)
		{
			var comp=c.charAt(i);
			if(!this.companies[comp]) 
			{
				return comp;
			}
		}
	}
	this.compareCompanies=		function(data)
	{										//COMPARE NETWORTH OF SEVERAL COMPANIES AND RETURN HIGHEST VALUE (TEMPORARY SETUP)
		var worth=0;
		var highest;
		for(i in data.companies)
		{
 			var company=this.companies[i];
			if(company.networth>worth) 
			{
				highest=company.name;
				worth=company.networth;
			}
		}
		return highest;
	}
	this.getNetWorth=			function()
	{										//CALCULATE NETWORTH OF ALL INDIVIDUAL COMPANIES
		var networth=0;
		for(ii in this.companies)
		{
			networth+=this.companies[ii].networth;
		}
		return networth;
	}
	this.getCash=				function()
	{										
		for(ii in this.companies)
		{
			this.cash+=this.companies[ii].getInterest();
		}
		return this.cash;
	}
	this.mergeCompanies=		function(location,data)
	{										//CALL compareCompanies AND MERGE COMPANIES IN DATA INTO RETURN VALUE FROM COMPARISON (HIGHER VALUED COMPANY)
		var comp=this.compareCompanies(data);
		this.companies[comp].data[location]=ccolor+comp;
		this.allocatePartials(data.partials,comp);
		for(i in data.companies)
		{
			if(comp==i) 
			{
				delete data.companies[i];
			}
		}
		
		var bonus=0;
		for(m in data.companies)
		{
			for(a in this.companies[m].data)
			{
				this.companies[comp].data[a]=ccolor+comp;
			}
			this.companies[comp].num_stars+=this.companies[m].num_stars;
			this.companies[comp].shares+=this.companies[m].shares/2;
			this.companies[comp].calculateValue();
			bonus+=parseInt(this.companies[m].stock_value*10);
			delete this.companies[m];
		}
		this.cash+=bonus;
		message("\1y\1hMerge Bonus: \1r\1h$" + bonus,60,24,19);
		mswait(500);
	}
	this.allocatePartials=		function(partials,name)	
	{										//FIND AND CHANGE ALL PARTIAL COMPANIES IN THE AREA TO THE COMPANY SUPPLIED
		for(i in partials)
		{
			location=partials[i];
			game.companies[name].data[location]=ccolor+name;
			delete game.partial_data[location];
		}
	}
	this.sortProximity=			function(location)
	{										//SORT THE CONTENTS FROM PROXIMITY SCAN AND STORE THE DATA FOR THE DURATION OF THIS TURN
		var companies=[];
		var partials=[];
		var stars=[];
		var empty=true;
		var prox=this.scanProximity(location);

		//find stars, partial companies, and companies  in area
		for(i=0;i<prox.length;i++)
		{
			if(this.star_data[prox[i]]) 
			{ 
				stars.push(prox[i]); 
				empty=false;
			}
			else if(this.partial_data[prox[i]]) 
			{ 
				partials.push(prox[i]); 
				empty=false;
			}
			else
			{
				for(c in this.companies)
				{
					name=c;
					if(this.companies[c].data[prox[i]]) 
					{ 
						companies[name]=prox[c];
						empty=false;
					}
				}
			}
		}
		if(!empty)
			this.proximity[location]={'companies':companies,'stars':stars,'partials':partials};
		return(this.proximity[location]);
	}
	this.scanProximity=			function(location) 	
	{										//LOCATE GRID INDICES FOR NORTH,SOUTH,EAST,WEST OF CURRENT POSITION
		var prox=[];
//		console.home();
//		console.cleartoeol();
		if(location>=this.columns) 					//if not in the first row
			prox[0]=(location-this.columns);		//north
		else
		{
			if(difficulty>1)
			{
				prox[0]=(location+(this.columns * (this.rows-1)));
			}
		}
		if(location<(this.columns * (this.rows-1)))	//if not in the last row
			prox[1]=(location+this.columns);		//south
		else
		{
			if(difficulty>1)
			{
				prox[1]=(location-(this.columns * (this.rows-1)));
			}
		}
		if(((location+1)%this.columns)!=0)			//if not in the last column
			prox[2]=(location+1);					//east		
		else
		{
			if(difficulty>1)
			{
				prox[2]=(location+1)-this.columns;
			}
		}
		if(((location+1)%this.columns)!=1)			//if not in the first column
			prox[4]=(location-1);					//west
		else
		{
			if(difficulty>1)
			{
				prox[3]=(location-1)+this.columns;
			}
		}
		
		return prox;
	}	
	this.addToCompany=			function(location,data)
	{										//IN THE EVENT THERE IS ONE NEARBY COMPANY, ADD THE CURRENT LOCATION TO THE COMPANY DATA
		var name=this.getName(data.companies);
		this.companies[name].data[location]=ccolor+name;
		if(data.stars.length) this.companies[name].num_stars+=data.stars.length;
		if(data.partials.length) this.allocatePartials(data.partials,name);
	}
	this.getName=				function(data)
	{
		for(i in data)
		{
			return i;
		}
	}
	this.makeNewCompany=		function(location,data)
	{										//IN THE EVENT THERE ARE NO NEARBY COMPANIES OR PARTIAL COMPANIES, CREATE A NEW COMPANY
		var name=this.getNextCompany();
		this.companies[name]=new Company(name);
		this.companies[name].newData(data,location);
		this.companies[name].calculateValue();
	}
	this.generateMap();
	this.display(this.star_data);
}
function 	Company(name)	
{								//COMPANY CLASS
{										//COMPANY VARIABLES
	this.name=name;							//COMPANY DISPLAY CHARACTER AND game.Companies[] INDEX
	this.shares=5; 							//INITIAL COMPANY SHARES
	this.networth=0;						//TOTAL COMPANY VALUE
	this.stock_value=0;						//TOTAL INDIVIDUAL STOCK VALUE
	this.base_value=100; 					//BASE COMPANY VALUE (PER SECTOR)
	this.star_value=500; 					//PROXIMITY STAR VALUE
	this.num_stars=0;						//NUMBER OF STARS TOUCHING COMPANY
	this.times_split=1;						//NUMBER OF TIMES COMPANY HAS SPLIT (1 = 0)
	this.data=[];							//COMPANY LOCATION DATA
}
	this.companySize=		function()
	{
		var count=0;
		for(i in this.data)
		{
			count++;
		}
		return count;
	}
	this.calculateValue=	function()
	{										//CALCULATE COMPANY NETWORTH
		this.getValue();
		while(this.stock_value>=3000) 
		{
			this.splitCompany();
			this.getValue();
		}
		this.networth=this.shares*this.stock_value;
	}
	this.getValue=	function()
	{										//CALCULATE COMPANY NETWORTH
		this.stock_value=((this.num_stars * this.star_value)+(this.companySize() * this.base_value)) 
							/this.times_split;
	}
	this.splitCompany=		function()
	{										//SPLIT COMPANY WHEN SHARE VALUE MEETS OR EXCEEDS 3000 PER SHARE
		this.shares*=2;
	//	this.base_value+=50;
	//	this.star_value+=50;
		this.times_split++;
		message("\1y\1hCompany \1r" + this.name + " \1ysplit",60,24,19);
		mswait(500);
	}
	this.newData=			function(data,location)
	{										//INITIALIZE NEW COMPANY
		this.data[location]=ccolor+this.name;
		if(data.partials.length) game.allocatePartials(data.partials,this.name);
		if(data.stars.length) this.num_stars=data.stars.length;
	}
	this.getInterest=		function()
	{
		this.calculateValue();
		earned_interest=this.networth*interest_rate;
		return earned_interest;
	}
	this.displayLong=		function()
	{
		this.calculateValue();
		console.putmsg(	"\1n\1gCOMPANY: \1h\1y" + this.name + 
				" \1n\1gWORTH: \1h$" + printPadded("\1w\1h"+parseInt(this.networth)+" ",14," ","right") +
				"\1n\1gSHARE VALUE: \1h$" + printPadded("\1w\1h"+parseInt(this.stock_value)+" ",5," ","right") +
				"\1n\1gSHARES: " + printPadded("\1w\1h"+parseInt(this.shares)+" ",10," ","right") +
				"\1n\1gSPLIT: \1w\1h" + (this.times_split-1) + "x");
	}
	this.display=			function(xxxx,yyyy)
	{
		this.calculateValue();
		console.gotoxy(xxxx,yyyy); 
		console.cleartoeol();
		console.putmsg(	"\1h\1c[\1w" + this.name + "\1c]\1w" +
				" $\1c" + printPadded(parseInt(this.stock_value),4," ","left") +
				" \1w(\1c" + shortNumber(this.shares) + "\1w)" );
	}
}
function 	Menu(title,x,y)		
{								//MENU CLASSES
	
	this.title=title;
	this.disabled=[];
	this.items=[];

	this.display=function()
	{
		var clear=5;
		var cleared=-1;
		var xx=x;
		var yy=y;4

		console.gotoxy(xx,yy); yy++;
		console.cleartoeol();
		console.putmsg("\1h\1w" + this.title);
		console.gotoxy(xx,yy); yy++;
		console.cleartoeol();
		drawLine(false,false,17);
		for(i in this.items)
		{
			if(!this.disabled[i])
			{
				console.gotoxy(xx,yy); yy++;
				console.putmsg(this.items[i].text);
				console.cleartoeol();
				cleared++;
			}
		}
		for(i=cleared;i<clear;i++)
		{
			console.gotoxy(xx,yy); yy++;
			console.cleartoeol();

		}
		console.putmsg("\1n\1k");
		console.gotoxy(79,24);
	}
	this.disable=function(item)
	{
		this.disabled[item]=true;
	}
	this.enable=function(item)
	{
		this.disabled[item]=false;
	}
	this.add=function(items)
	{
		for(i=0;i<items.length;i++)
		{
			hotkey=this.getHotKey(items[i]);
			this.items[hotkey]=new menuItem(items[i],hotkey);
		}
	}
	this.getHotKey=function(item)
	{
		keyindex=item.indexOf("~")+1;
		return item.charAt(keyindex);
	}	
	
}
function 	menuItem(item,hotkey)
{								//MENU ITEM OBJECT
	var displayColor="\1c\1h";
	var keyColor="\1w\1h";

	this.hotkey=hotkey;
	this.text=item.replace(("~" + hotkey) , (displayColor + "[" + keyColor + hotkey + displayColor + "]"));
}
function	Score(p,s,d,date)
{
	this.player=p;
	this.score=s;
	this.difficulty=d;
	this.date=date;
}
