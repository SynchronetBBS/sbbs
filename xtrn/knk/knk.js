load("sbbsdefs.js");
load("lockfile.js");
var game_dir='.';
try { throw barfitty.barf(barf) } catch(e) { game_dir=e.fileName }
game_dir=game_dir.replace(/[\/\\][^\/\\]*$/,'');
game_dir=backslash(game_dir);

var version="5.0js";
var updated="01/04/09";
var music=true;
var cterm_major=0;
var cterm_minor=0;
var title="King";
var name="Nobody";
var player;
var computer;
var wins=0;
var losses=0;

function check_syncterm_music()
{
	// Check if it's CTerm and supports ANSI music...
	var ver=new Array(0,0);

	// Disable parsed input... we need to do ESC processing ourselves here.
	var oldctrl=console.ctrlkey_passthru;
	console.ctrlkey_passthru=-1;

	write("\x1b[c");
	var response='';

	while(1) {
		var ch=console.inkey(0, 5000);
		if(ch=="")
			break;
		response += ch;
		if(ch != '\x1b' && ch != '[' && (ch < ' ' || ch > '/') && (ch<'0' || ch > '?'))
			break;
	}

	if(response.substr(0,21) != "\x1b[=67;84;101;114;109;") {	// Not CTerm
		bbs.mods.CTerm_Version='0';
		console.ctrlkey_passthru=oldctrl;
		return(0);
	}
	if(response.substr(-1) != "c") {	// Not a DA
		bbs.mods.CTerm_Version='0';
		console.ctrlkey_passthru=oldctrl;
		return(0);
	}
	var version_str=response.substr(21);
	version_str=version_str.replace(/c/,"");
	var ver=version_str.split(/;/);
	cterm_major=parseInt(ver[0]);
	cterm_minor=parseInt(ver[1]);
	console.ctrlkey_passthru=oldctrl;
}

function getkeys(str)
{
	var key;

	while(1) {
		key=console.getkey(K_UPPER|K_NOECHO);
		if(str.indexOf(key)!=-1)
			break;
	}
	/* Don't echo control keys */
	if(ascii(key)<32)
		console.crlf();
	else
		console.writeln(key);
	return(key);
}

function playmusic(str)
{
	if(music) {
		if(cterm_major == 1 && cterm_minor < 111 && cterm_minor >= 37) {
			str=str.replace(/^M[FBNLS]/,'N');
		}
		else if((cterm_major == 1 && cterm_minor >= 111) || cterm_major > 1) {
			if(str.charAt(0)=='M')
				str='|'+str.substr(1);
		}
		console.write("\x1b["+str+"\x0e");
	}
}

function proper(str)
{
	return(str.toLowerCase().replace(/(\b.)/g,function(s) {
												return(s.toUpperCase());
											}));
}

function show_intro()
{
	console.clear(CYAN);
	console.crlf();
	console.center("Welcome to Kannons and Katapults "+user.name+"!");
	console.crlf();
	console.center("Version "+version+" * KNK * Last Updated "+updated);
	console.crlf();
	console.center("ORIGINAL VERSION");
	console.crlf();
	console.center("Copyright (c) 1991,1992,1993,1994,1995 by Alan Davenport. All Rights Reserved.");
	console.crlf();
	console.center("by Alan Davenport");
	console.crlf();
	console.center("of Al's Cabin BBS");
	console.crlf();
	console.center("JavaScript Version");
	console.crlf();
	console.center("Copyright (c) 2009 by Stephen Hurd. All Rights Reserved.");
	console.crlf();
	console.center("Running on "+system.name+" courtesy of "+system.operator+".");
	console.crlf();
	syncterm_music=check_syncterm_music();
	console.pause();

	console.clear();
	console.crlf();
	console.crlf();
	console.crlf();
	console.crlf();	console.attributes=YELLOW; 
	console.writeln("The object of Kannons & Kataputs is to defeat King Computer. There are 3");
	console.writeln("ways to do this. You may destroy his kastle, defeat his army or have him");
	console.writeln("assassinated. If this sounds easy to you Deuce, you should be warned that");
	console.writeln("King Computer is no push over!");
	console.crlf();
	console.crlf(); console.attributes=WHITE;
	console.writeln("Although it may seem that the game is rigged, the odds are exactly even.");
	console.writeln("A LOT of work and thousands of games have gone into making King Computer");
	console.writeln("as smart as possible. It's you against him. Play carefully!!");
	console.crlf();
	console.crlf(); console.attributes=LIGHTGREEN;
	console.writeln("You each will recieve one point for every soldier killed, one point for every");
	console.writeln("kastle point destroyed and 1,000 points for each guard passed by an assassin");
	console.writeln("when successful. The loser loses their points. The scoreboard resets every");
	console.writeln("month. Good Luck!");
	console.crlf();
	console.crlf();
	console.crlf();
	console.line_counter--;	// Allow to print on last line...
	console.crlf();
	console.print("\1h\1bPress [\1mP\1b] to Play KNK or [\1mQ\1b] to quit back to the BBS. -=> \1m");
	if(getkeys("PQ")=='Q')
		exit(0);

	console.crlf();
	console.attributes=MAGENTA;
	playmusic("MFT64L64O5CDP32CDP32CDP16");
	console.attributes=GREEN;
	console.writeln("If your terminal supports ANSI sound, you just heard some.");
	console.writeln("If it doesnt, you just saw some wierd characters.");
	console.crlf();
	console.print("\1h\1gDo you want the ANSI sound effects? [Y/N] -=> \1n\1g");
	if(getkeys("YN")!='Y')
		music=false;
	playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
	while(1) {
		console.crlf();
		console.print("\1hArt thou a [K] King or a [Q] Queen? -=> \1n\1g");
		if(getkeys("KQ")=='Q')
			title="Queen";
		else
			title="King";
		console.crlf();
		console.print("\1hWhat name shall you be known by sire? -=> \1n\1g");
		name=console.getstr("", 35);
		console.crlf();
		console.print('\1h"'+title+' '+name+'" Is this OK? [y/N] -=> \1n\1g');
		if(getkeys("YN\r\n")=='Y')
			break;
	}
}

function Player(t, n, p)
{
	this.isplayer=p;
	// Pronouns
	this.title=t;
	this.name=n;
	this.full_name=t+" "+n;
	if(p) {
		this.posessive="your";
		this.singular="you";
		this.conjugate="";
		this.conjugate2="";
		this.refer_posessive=this.posessive;
		this.refer_singular=this.singular;
	}
	else {
		if(t=='King') {
			this.posessive="his";
			this.singular="he";
		}
		else {
			this.posessive="her";
			this.singular="she";
		}
		this.conjugate="s";
		this.conjugate2="es";
		this.refer_posessive=this.full_name+"'s";
		this.refer_singular=this.full_name;
	}
	this.score=0;
	this.kastle=10000+random(10000);
	this.soldiers=2000+random(4000);
	this.civilians=10000+random(10000);
	this.kannons=150+random(150);
	this.katapults=random(1);
	this.assassins=1+random(4);
	this.guards=10+random(10);
	this.gold=15000+random(15000);
	this.food=100000+random(100000);
	this.power getter=function() { var p=parseInt(this.kastle/500)+parseInt(this.soldiers/750); if(p>46) return(46); return(p); };
	this.mfood getter=function() { var f=this.food/(this.soldiers*2+this.civilians); if(isNaN(f)) return(0); if(f<0.1) return(0); return(f); };
	this.produce=Player_produce;
	this.powerbar=Player_powerbar;
	this.drawscreen=Player_drawscreen;
	this.assassinate=Player_assassinate;
	if(this.isplayer)
		this.move=Player_playermove;
	else
		this.move=Player_computermove;
	this.purchase=Player_purchase;
	this.soldierattack=Player_soldierattack;
	this.kannonattack=Player_kannonattack;
	this.katapultattack=Player_katapultattack;
	this.dodamage=Player_dodamage;
	this.checkresults=Player_checkresults;
	this.loadwagons=Player_loadwagons;
}

function Player_loadwagons(propname, count)
{
	var load_count={kannons:0.01, guards:0.1, assassins:1, katapults:3, food:0.00005, kastle:0.001};
	var price={kannons:100, guards:1000, assassins:7500, katapults:25000, food:0.2, kastle:10};
	var i;

	count=parseInt(count);

	/*
	 * Round sacks up
	 */
	if(propname=='food') {
		if(count%5)
			count += (5-(count%5));
	}
	this.gold -= parseInt(price[propname]*count);
	this[propname] += count;
	for(i=0; i<count*load_count[propname]; i++)
		playmusic("MFT80O5L64FP64CFP64F");
}

function Player_produce()
{
	var tmp;

	console.crlf();
	console.attributes=WHITE;
	console.center("* Monthly Update *");
	console.crlf();
	if(this.isplayer)
		console.attributes=GREEN;
	else
		console.attributes=CYAN;
	tmp=parseInt(this.kastle/100);
	if(tmp > 0)
		console.writeln("* "+tmp+" civilians have immigrated to "+this.refer_posessive+" kastle.");
	this.civilians += tmp;
	tmp=0;
	if(this.civilians > 0)
		console.writeln("* "+this.civilians+" gold was collected in taxes!");
	this.gold += this.civilians;
	tmp += this.civilians;

	if(this.soldiers) {
		if(this.gold < this.soldiers) {
			console.attributes=LIGHTGREEN;
			console.writeln("* "+(this.soldiers-this.gold)+" of "+this.refer_posessive+" men defected from not being paid!!");
			this.soldiers=this.gold;
			console.attributes=GREEN;
		}
	}
	if(this.soldiers) {
		console.writeln("* "+this.soldiers+" gold was paid to "+this.refer_posessive+" soldiers.");
		this.gold -= this.soldiers;
		tmp -= this.soldiers;
	}
	if(tmp > 0) {
		console.writeln("* Treasury increased by "+tmp+" gold pieces!");
	}
	else {
		if(tmp < 0)
			console.writeln("* Treasury DECREASED by "+(0-tmp)+" gold pieces!");
	}

	if(this.soldiers) {
		var canfeed=parseInt(this.food/2);
		if(canfeed < this.soldiers) {
			console.attributes=LIGHTGREEN;
			console.writeln("* "+(this.soldiers-canfeed)+" of "+this.refer_posessive+" soldiers have starved to death!!");
			this.soldiers=canfeed;
			console.attributes=GREEN;
		}
	}
	tmp=this.soldiers*2;
	if(tmp > 0)
		console.writeln("* "+tmp+" sacks of food were consumed by "+this.refer_posessive+" army.");
	this.food -= tmp;

	if(this.civilians) {
		if(this.food < this.civilians) {
			console.attributes=LIGHTGREEN;
			console.writeln("* "+(this.civilians-this.food)+" of "+this.refer_posessive+" citizens have starved to death!!");
			this.civilians=this.food;
			console.attributes=GREEN;
		}
	}
	if(this.civilians)
		console.writeln("* "+this.civilians+" sacks of food were consumed by "+this.refer_posessive+" citizens.");
	this.food -= this.civilians;
	console.crlf();
}

function Player_powerbar()
{
	var lastattr=console.attributes;
	var pow=this.power;
	var bars="\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe";

	if(pow > 0) {
		if(pow <= 10)
			console.attributes=LIGHTRED|BLINK;
		else
			console.attributes=LIGHTRED;
		console.write(format("%.*s", pow>10?10:pow, bars));
	}
	if(pow > 10) {
		console.attributes=YELLOW;
		console.write(format("%.*s", pow>20?10:pow-10, bars));
	}
	if(pow > 20) {
		console.attributes=LIGHTGREEN;
		console.write(format("%.*s", pow-20, bars));
	}
	for(i=pow; i<46; i++)
		console.write(' ');
	console.attributes=lastattr;
}

function Player_drawscreen(month)
{
	console.clear();
	console.crlf();
	if(this.isplayer) {
		console.print("\1h\1w*  Your Turn  *            \1h\1r*  Month # "+month+"  *\r\n");
		console.crlf();
		console.attributes=GREEN;
	}
	else {
		var mstr=mstr = format("%-27s", "*  Month # "+month+"  *");
		console.print("\1h\1r"+mstr+"\1w*  "+this.full_name+"'s Turn  *\r\n");
		console.crlf();
		console.attributes=CYAN;
	}
	console.writeln("\xc9\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcb\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbb");
	console.print("\xba  "+format("%-24s",player.full_name)+"\xba  "+format("%-24s",computer.full_name)+"\xba\r\n");
	console.writeln("\xcc\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xca\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xb9");
	console.print("\xba You: ");
	player.powerbar();
	console.writeln(" \xba");
	console.print("\xba Him: ");
	computer.powerbar();
	console.writeln(" \xba");
	console.writeln("\xcc\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcb\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xb9");
	console.writeln(format("\xba  Score..........: %-6d \xba  Score..........: %-6d \xba",player.score,computer.score));
	console.writeln(format("\xba  Kastle Points..: %-6d \xba  Kastle Points..: %-6d \xba",player.kastle,computer.kastle));
	console.writeln(format("\xba  Soldiers.......: %-6d \xba  Soldiers.......: %-6d \xba",player.soldiers,computer.soldiers));
	console.writeln(format("\xba  Civilians......: %-6d \xba  Civilians......: %-6d \xba",player.civilians,computer.civilians));
	console.writeln(format("\xba  Kannons........: %-6d \xba  Kannons........: %-6d \xba",player.kannons,computer.kannons));
	console.writeln(format("\xba  Katapults......: %-6d \xba  Katapults......: %-6d \xba",player.katapults,computer.katapults));
	console.writeln(format("\xba  Assassins......: %-6d \xba  Assassins......: %-6d \xba",player.assassins,computer.assassins));
	console.writeln(format("\xba  Guards.........: %-6d \xba  Guards.........: %-6d \xba",player.guards,computer.guards));
	console.writeln(format("\xba  Gold...........: %-6d \xba  Gold...........: %-6d \xba",player.gold,computer.gold));
	console.writeln(format("\xba  Months of Food.: %-6.3f \xba  Months of Food.: %-6.3f \xba",player.mfood,computer.mfood));
	console.writeln("\xc8\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xca\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbc");
}

function Player_assassinate(other)
{
	var passed=0;
	var killed=0;
	var i;

	this.assassins--;
	console.crlf();
	console.attributes=LIGHTGREEN;
	console.writeln("Attempting infiltration!");
	playmusic("MFO1T32L8EL64DL8EL64DL8EL64DL8EP8");
	console.crlf();
	console.attributes=LIGHTRED;
	console.writeln("He has to pass "+other.guards+" guards.");
	while(other.guards && passed < other.guards) {
		switch(random(5)) {
			case 0: // Failed
				console.attributes=LIGHTRED|BLINK;
				console.writeln(this.refer_posessive+" assassin was killed!");
				playmusic("MFT32L2O1L8dL4C");
				console.attributes=LIGHTRED;
				return(false);
			case 1:	// Killed
				console.attributes=LIGHTRED|BLINK;
				console.writeln("KILLED "+(++killed));
				passed++;
				other.guards--;
				for(i=0; i<5; i++)
					playmusic("MFO1T128L64CEB");
				break;
			default:
				console.attributes=LIGHTRED;
				console.writeln("Passed "+(++passed)+"!");
				playmusic("MFO1T128L64CEB");
		}
	}
	this.score += 1000*passed;
	playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
	if(this.isplayer) {
		console.attributes=LIGHTGREEN;
		console.writeln("Stealthily, quietly, "+this.posessive+" assassin creeps into the inner chambers of "+other.title);
		console.writeln(other.name+" and pours a small vial of colorless fluid into a goblet which sits");
		console.writeln("upon "+other.refer_posessive+" banquet table. During dinner, "+other.singular+" suddenly clutch"+other.conjugate2+" "+other.posessive);
		console.writeln("throat and tumble"+other.conjugate+" backward off of "+other.posessive+" seat.");
	}
	else {
		console.attributes=LIGHTRED;
		console.writeln(proper(other.singular)+" wake"+other.conjugate+" up, for a second, and see"+other.conjugate+" "+this.refer_posessive+" assassin standing");
		console.writeln("over "+other.singular+" with a grim smile. His hand thrusts his knife into "+other.refer_posessive+" heart.");
		console.writeln(proper(this.singular)+" sink"+this.conjugate+" into the long and everlasting sleep.");
	}
	playmusic("MFT64L64O5CDP32CDP32CDP16");
	return(this);
}

function Player_purchase(name, propname, cost)
{
	var max;

	max=parseInt(this.gold/cost);
	console.crlf();
	console.attributes=YELLOW|BG_RED;
	console.print("Press \1w[ENTER]\1y to abort.");
	console.attributes=YELLOW;
	console.crlf();
	console.crlf();
	if(propname=="food") {
		var i=this.civilians+this.soldiers*2;
		console.attributes=RED;
		console.writeln("You have "+this.food+" sacks of food.");
		console.writeln("This is enough to feed your population for "+parseInt(this.mfood)+" months.");
		console.writeln("You have "+this.gold+" gold and can afford "+max+" sacks of food.");
		console.writeln("This will increase your reserves of food by "+(i==0?0:parseInt(max/i))+" months.");
		console.writeln("You need "+i+" sacks of food per month to feed your population.");
		console.attributes=LIGHTRED;
		console.crlf();
		console.write("Buy how many sacks? (M = MAX) -=> ");
		console.attributes=RED;
	}
	else {
		console.attributes=LIGHTMAGENTA;
		console.write("You can afford "+max+" "+name+".");
		console.attributes=LIGHTCYAN;
		console.write(" Buy how many? (M = MAX) -=> ");
//		console.attributes=CYAN;
	}
	i=console.getkeys("M\r\n", max);
	if(i=='' || i=='\r' || i=='\n' || i==0)
		return(false);
	if(i=='M')
		i=max;
	i=parseInt(i);
	console.attributes=CYAN;
	console.crlf();
	console.writeln("Loading your wagons sire. Thank you for your gold!");
	this.loadwagons(propname,i);
	console.crlf();
	console.attributes=LIGHTBLUE;
	console.writeln("* A real pleasure serving you sire! *");
	return(i);
}

function Player_soldierattack(other)
{
	var i;
	var this_lost=0;
	var oth_lost=0;
	var orig_soldiers=this.soldiers;
	var clashes=0;

	console.crlf();
	console.writeln("You have "+player.soldiers+" soldiers and "+computer.full_name+" has "+computer.soldiers+".");
	console.crlf();
	console.writeln("Forces engaging!");
	playmusic("MFT32O3L16CL8FL16CL8FL64CP64CP64CP64L8F");
	console.crlf();
	console.attributes=LIGHTGREEN;
	for(i=0; i<orig_soldiers && other.soldiers > 0; i++) {
		if(i%1000==0) {
			switch(random(3)) {
				case 0:
					console.write(" * SMASH! *");
					break;
				case 1:
					console.write(" * CLASH! *");
					break;
				case 2:
					console.write(" * CRASH! *");
					break;
			}
			playmusic("MFO1T128L64CEB");
			clashes++;
			if(clashes%7==0)
				console.crlf();
		}
		switch(random(3)) {
			case 0:
				if(random(10)) {
					oth_lost++;
					other.soldiers--;
				}
				break;
			case 1:
			case 2:
				if(random(10)) {
					this_lost++;
					this.soldiers--;
				}
				break;
		}
	}

	if(clashes%7)
		console.crlf();
	console.crlf();
	console.attributes=GREEN;
	if(this.isplayer) {
		console.writeln("You lost "+this_lost+" men and have "+this.soldiers+" left!");
		console.writeln(other.full_name+" lost "+oth_lost+" men and has "+other.soldiers+" left!");
	}
	else {
		console.writeln("You lost "+oth_lost+" men and have "+other.soldiers+" left!");
		console.writeln(this.full_name+" lost "+this_lost+" men and has "+this.soldiers+" left!");
	}
	console.crlf();
	return(this.checkresults(other));
}

function Player_dodamage(other, damage, men)
{
	var i;
	var j;
	var bad;
	var rnd;
	var loss={soldiers:0, kastle:0, guards:0, assassins:0, kannons:0, katapults:0, civilians:0};
	var total_cost=0;
	var nodamage=true;
	var booms=0;

	if(this.isplayer)
		console.attributes=YELLOW;
	else
		console.attributes=LIGHTCYAN;
		
	for(i=0; i<damage; i++) {
		bad=random(7);	// Each damage does 0-7 points of "bad"
		for(j=0; j<bad; j++) {
			rnd=other.soldiers+other.kastle+other.guards+other.assassins+other.kannons+other.katapults+other.civilians;
			if(men)
				rnd += other.soldiers;
			else
				rnd += other.kastle;

			if(other.kastle==0 || other.soldiers==0) {
				if(nodamage=false)
					continue;
			}
			nodamage=false;

			rnd=random(rnd);

			if(rnd < other.soldiers) {
				loss.soldiers++;
				other.soldiers--;
				this.score++;
				total_cost+=1000;
				continue;
			}
			rnd -= other.soldiers;

			if(rnd < other.kastle) {
				loss.kastle++;
				other.kastle--;
				this.score++;
				total_cost+=1000;
				continue;
			}
			rnd -= other.kastle;

			if(rnd < other.guards) {
				loss.guards++;
				other.guards--;
				total_cost+=10000;
				continue;
			}
			rnd -= other.guards;

			if(rnd < other.assassins) {
				loss.assassins++;
				other.assassins--;
				total_cost+=10000;
				continue;
			}
			rnd -= other.assassins;

			if(rnd < other.kannons) {
				loss.kannons++;
				other.kannons--;
				total_cost+=1000;
				continue;
			}
			rnd -= other.kannons;

			if(rnd < other.katapults) {
				loss.katapults++;
				other.katapults--;
				total_cost+=100000;
				continue;
			}
			rnd -= other.katapults;

			if(rnd < other.civilians) {
				loss.civilians++;
				other.civilians--;
				total_cost+=4;
				continue;
			}
			rnd -= other.civilians;

			this.score++;
			if(men) {
				loss.soldiers++;
				other.soldiers--;
				total_cost+=1000;
				continue;
			}
			loss.kastle++;
			other.kastle--;
			total_cost+=1000;
		}
		while(total_cost > 100000) {
			switch(random(2)) {
				case 0:
					console.write("**BANG!** ");
					break;
				case 1:
					console.write("**BOOM!** ");
					break;
			}
			playmusic("MFO1T128L64CEB");
			booms++;
			if(booms%7==0)
				console.crlf();
			total_cost -= 100000;
		}
	}

	if(total_cost) {
			switch(random(2)) {
				case 0:
					console.write("**BANG!** ");
					break;
				case 1:
					console.write("**BOOM!** ");
					break;
			}
			playmusic("MFO1T128L64CEB");
			booms++;
			if(booms%7==0)
				console.crlf();
	}
	if(booms%7)
		console.crlf();

	if(nodamage) {
		if(this.isplayer)
			console.attributes=YELLOW|BLINK;
		else
			console.attributes=LIGHTCYAN|BLINK;
		console.writeln("No damage!");
		playmusic("MFT32L2O1L8dL4C");
	}

	console.crlf();

	if(loss.soldiers > 0) {
		console.attributes=LIGHTRED;
		console.writeln(proper(this.refer_singular+" killed "+loss.soldiers+" men!"));
	}
	if(loss.kastle > 0) {
		console.attributes=LIGHTGREEN;
		console.writeln(proper(this.refer_singular+" destroyed "+loss.kastle+" kastle points!"));
	}
	if(loss.guards > 0) {
		console.attributes=YELLOW;
		console.writeln(proper(this.refer_singular+" killed "+loss.guards+" guards!"));
	}
	if(loss.assassins > 0) {
		console.attributes=LIGHTBLUE;
		console.writeln(proper(this.refer_singular+" killed "+loss.assassins+" assassins!"));
	}
	if(loss.kannons > 0) {
		console.attributes=LIGHTMAGENTA;
		console.writeln(proper(this.refer_singular+" destroyed "+loss.kannons+" kannons!"));
	}
	if(loss.katapults > 0) {
		console.attributes=LIGHTCYAN;
		console.writeln(proper(this.refer_singular+" destroyed "+loss.katapults+" katapults!"));
	}
	if(loss.civilians > 0) {
		console.attributes=WHITE;
		console.writeln(proper(this.refer_singular+" killed "+loss.civilians+" civilians!"));
	}

	return(this.checkresults(other));
}

function Player_kannonattack(other, men)
{
	console.crlf();
	console.attributes=BROWN;
	if(this.isplayer)
		console.writeln("Firing Kannons!");
	else {
		console.write("Firing kannons at your ");
		if(men)
			console.writeln("men!");
		else
			console.writeln("kastle!");
	}
	playmusic("MFO1L64T128BAAGGFFEEDDCP4");
	console.crlf();
	return(this.dodamage(other, random(this.kannons+1), men));
}

function Player_katapultattack(other, men)
{
	var damage=0;
	var	i;

	for(i=0; i<this.katapults; i++) {
		if(random(2))
			damage += random(1500)+1;
	}

	console.crlf();
	console.attributes=BROWN;
	if(this.isplayer)
		console.writeln("Firing Katapult!");
	else {
		console.write("Firing katapult at your ");
		if(men)
			console.writeln("men!");
		else
			console.writeln("kastle!");
	}
	playmusic("MFT76L64O2CDEFGABAGFEDCP4");
	console.crlf();
	return(this.dodamage(other, damage, men));
}

function show_scoreboard(thismonth)
{
	var f=new File(game_dir+(thismonth?"knk-best.asc":"knk-last.asc"));

	if(!file_exists(f.name)) {
		console.crlf();
		console.attributes=LIGHTGREEN;
		console.writeln("* Scoreboard file not found *");
		console.crlf();
		return;
	}
	if(Lock(f.name, system.node_num, false, 0.5)==false) {
		console.crlf();
		console.attributes=LIGHTGREEN;
		console.writeln("* Scoreboard file locked for write *");
		console.crlf();
		return;
	}
	if(!f.open("r", true)) {
		Unlock(f.name);
		console.crlf();
		console.attributes=LIGHTGREEN;
		console.writeln("* Scoreboard file can't be opened *");
		console.crlf();
		return;
	}
	var lines=f.readAll();
	f.close();
	Unlock(f.name);
	console.attributes=GREEN;
	console.writeln(lines.join("\r\n"));
}

function Player_playermove(month, other)
{
	var loop;
	var max;
	var i;
	var tl=bbs.time_left;

	do {
		console.print("\1h\1cTime:\1g "+parseInt(tl/60)+":"+format("%02d",tl%80)+"  \1y*  A,C,D,F,K,P,Q,R,S,T,Z,$ or ? for Help -=> \1n\1g");
		loop=false;
		switch(getkeys("ACDFKPQRSTZ$?\r\n")) {
			case '?':
				console.crlf();
				console.writeln("A - Send Assassin");
				console.writeln("C - Toggle Color ON/OFF");
				console.writeln("D - Draft Civilians");
				console.writeln("F - Fire Kannon");
				console.writeln("K - Fire Katapult");
				console.writeln("P - Pass Your Turn");
				console.writeln("Q - Quit (You Lose)");
				console.writeln("R - Release Troops from Service");
				console.writeln("S - Soldiers vs. Soldiers");
				console.writeln("T - Visit Trading Post");
				console.writeln("Z - Toggle Sound ON/OFF");
				console.writeln("$ - Show Scoreboard");
				console.writeln("[ENTER] - Redraw Main Screen");
				console.crlf();
				loop=true;
				break;
			case 'A':
				return(this.assassinate(other));
			case 'C':
				/* TODO: Disable colour */
				console.crlf();
				console.writeln("Toggle color not yet implemented!");
				console.crlf();
				loop=true;
				break;
			case 'D':
				console.crlf();
				console.attributes=YELLOW|BG_RED;
				console.print("Press \1w[ENTER]\1y to abort.");
				console.attributes=GREEN;
				console.crlf();
				console.crlf();
				console.writeln("You have "+this.civilians+" civilians and "+this.soldiers+" soldiers.");
				console.write("Draft how many civilians into the army? [0 - "+this.civilians+"] -=> ");
				var draft=console.getnum(this.civilians);
				if(draft<1)
					loop=true;
				else {
					console.crlf();
					this.civilians -= draft;
					this.soldiers += draft;
					console.writeln("Done. You now have "+this.civilians+" civilians and "+this.soldiers+" soldiers");
					for(i=0; i<2; i++)
						playmusic("MFT64L64O5CDP32CDP32CDP16");
					return(false);
				}
				break;
			case 'F':
				if(this.kannons==0) {
					console.crlf();
					console.attributes=LIGHTGREEN|BLINK;
					console.writeln("You don't have any kannons!!");
					console.crlf();
					loop=true;
					break;
				}
				console.crlf();
				console.attributes=YELLOW|BG_RED;
				console.print("Press \1w[ENTER]\1y to abort.");
				console.attributes=GREEN;
				console.crlf();
				console.crlf();
				console.write("Fire at the [M] Men or the [K] Kastle? [M/K] -=> ");
				switch(getkeys("MK\r\n")) {
					case '\r':
					case '\n':
						break;
					case 'M':
						return(this.kannonattack(other, true));
					case 'K':
						return(this.kannonattack(other, false));
				}
				console.crlf();
				loop=true;
				break;
			case 'K':
				if(this.katapults==0) {
					console.crlf();
					console.attributes=LIGHTGREEN|BLINK;
					console.writeln("You don't have any katapults!!");
					console.crlf();
					loop=true;
					break;
				}
				console.crlf();
				console.attributes=YELLOW|BG_RED;
				console.print("Press \1w[ENTER]\1y to abort.");
				console.attributes=GREEN;
				console.crlf();
				console.crlf();
				console.write("Fire at the [M] Men or the [K] Kastle? [M/K] -=> ");
				switch(getkeys("MK\r\n")) {
					case '\r':
					case '\n':
						break;
					case 'M':
						return(this.katapultattack(other, true));
					case 'K':
						return(this.katapultattack(other, false));
				}
				console.crlf();
				loop=true;
				break;
			case 'P':
				console.crlf();
				console.writeln("You sit on your throne and ponder the war.");
				console.crlf();
				return(false);
			case 'Q':
				console.crlf();
				console.write("QUIT!? (You Lose) Are you SURE? -=> ");
				if(getkeys("YN\r\n")=='Y')
					return(other);
				loop=true;
				break;
			case 'R':
				console.crlf();
				console.attributes=YELLOW|BG_RED;
				console.print("Press \1w[ENTER]\1y to abort.");
				console.attributes=GREEN;
				console.crlf();
				console.crlf();
				console.writeln("You have "+this.civilians+" civilians and "+this.soldiers+" soldiers.");
				console.write("Send how many soldiers home? -=> ");
				var draft=console.getnum(this.soldiers);
				if(draft<1)
					loop=true;
				else {
					console.crlf();
					this.civilians += draft;
					this.soldiers -= draft;
					console.writeln("Done. You now have "+this.civilians+" civilians and "+this.soldiers+" soldiers");
					playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
					return(false);
				}
				break;
			case 'S':
				return(this.soldierattack(other));
				break;
			case 'T':
				console.crlf();
				console.crlf();
				console.attributes=LIGHTGREEN;
				console.writeln(" * Welcome to the Trading Post sire! *");
				loop=true;
				while(loop) {
					console.crlf();
					console.attributes=BROWN;
					console.writeln("[L] - Leave the Trading Post");
					console.writeln("[B] - Buy Kannons @ 100 Gold Each");
					console.writeln("[G] - Hire Guards @ 1000 Gold Each");
					console.writeln("[A] - Hire Assasins @ 7500 Gold Each");
					console.writeln("[K] - Buy Katapults @ 25000 Gold Each");
					console.writeln("[F] - Buy Sacks of Food @ 5 Per Each Gold");
					console.writeln("[R] - Raise Fortifications @ 10 Gold per Point");
					console.crlf();
					console.attributes=LIGHTGREEN;
					console.write("Total Gold: "+this.gold+" -=> ");
					console.attributes=GREEN;
					switch(getkeys("LBGAKFR")) {
						case 'L':
							console.crlf();
							loop=false;
							break;
						case 'B':
							if(this.purchase("kannons", "kannons", 100)!==false)
								return(false);
							break;
						case 'G':
							if(this.purchase("guards", "guards", 1000)!==false)
								return(false);
							break;
						case 'A':
							if(this.purchase("assassins", "assassins", 7500)!==false)
								return(false);
							break;
						case 'K':
							if(this.purchase("katapults", "katapults", 25000)!==false)
								return(false);
							break;
						case 'F':
							if(this.purchase("sacks", "food", 0.2)!==false)
								return(false);
							break;
						case 'R':
							if(this.purchase("fortification points", "kastle", 10)!==false)
								return(false);
							break;
					}
				}
				loop=true;
				break;
			case 'Z':
				console.crlf();
				music=!music;
				console.writeln("Sounds "+(music?"ON":"OFF"));
				console.crlf();
				loop=true;
				break;
			case '$':
				/* TODO: Show Scoreboard */
				console.crlf();
				console.attributes=LIGHTGREEN;
				console.write("Show which [T] this months or [L] last months scoreboard? -=> ");
				console.attributes=GREEN;
				if(getkeys("TL")=='L')
					show_scoreboard(false);
				else
					show_scoreboard(true);
				loop=true;
				break;
			case '\r':
			case '\n':
				this.drawscreen(month);
				console.crlf();
				loop=true;
				break;
		}
	} while(loop);
}

function Player_computermove(month, other)
{
	var weight={loss_soldiers:0, loss_kastle:0, loss_guards:0, loss_food:0
				,win_soldiers:0, win_kastle:0, win_assassins:0
				,need_assassins:0, need_katapults:0, need_kannons:0, need_food:0, need_soldiers:0, need_guards:0, need_kastle:0};
	var names=new Array();
	var name;

	console.print("\1n\1h\1c                      * \1h\1i\1rT\1gh\1yi\1bn\1mk\1ci\1wn\1yg\1c\1n\1h\1c *\1n\1c");
	mswait(2000);
	console.line_counter=0;
	console.crlf();
	console.crlf();

	for(name in weight)
		names.push(name);

	/* First, we do absolutes... */
	if(other.guards==0) {
		if(this.assassins > 0)
			return(this.assassinate(other));
		else
			weight.need_assassins = 1;
	}
	if(other.guards < 20) {
		if(this.assassins > 0)
			weight.win_assassins += 1-other.guards/15;
		else
			weight.need_assassins += 1-other.guards/20;
	}

	if(other.soldiers==0) {
		if(this.soldiers > 0)
			return(this.soldierattack(other));
		weight.need_soldiers=1;
		if(this.kannons > 50)
			return(this.kannonattack(other, true));
		if(this.katapults > 1)
			return(this.katapultattack(other, true));
		if(this.kannons > this.katapults * 500)
			weight.need_kannons=1;
		else
			weight.need_katapults=1;
	}
	else {
		if(other.soldiers < this.soldiers * .66)
			weight.win_soldiers += 1 - this.soldiers/other.soldiers;
		if(other.soldiers < this.soldiers * .33)
			weight.win_soldiers = 1;
	}

	if(this.soldiers==0)
		weight.loss_soldiers=1;
	else {
		if(this.soldiers < other.soldiers * .66)
			weight.need_soldiers += 1 - other.soldiers/this.soldiers;
		if(this.soldiers < 4000) {
			weight.need_soldiers += 1 - this.soldiers/4000;
			if(weight.need_soldiers > 1)
				weight.need_soldiers = 1;
		}
		if(this.soldiers < other.soldiers * .50)
			weight.loss_soldiers += 1 - weight.loss_soldiers/this.soldiers;
	}

	if(this.kastle==0)
		weight.loss_kastle=1;
	else {
		if(this.kastle < 10000)
			weight.need_kastle += 1 - this.kastle/10000;
	}

	if(this.mfood==0)
		weight.loss_food=1;
	else {
		if(this.mfood < 2)
			weight.need_food += 1 - this.mfood/2;
	}

	if(this.guards==0)
		weight.loss_guards=1;
	else {
		if(this.guards < 20)
			weight.need_guards += 1 - this.guards/20;
	}

	if(other.kastle==0) {
		if(this.kannons > 50)
			return(this.kannonattack(other, true));
		if(this.katapults > 1)
			return(this.katapultattack(other, true));
		if(this.kannons > this.katapults * 500)
			weight.need_kannons=1;
		else
			weight.need_katapults=1;
	}
	else {
		if(other.kastle < 10000)
			weight.win_kastle += 1-other.kastle/10000;
	}
	if(this.kannons > this.katapults?this.katapults:1 * 500) {
		if(weight.need_kannons==0)
			weight.need_kannons=0.1;
		if(weight.need_katapults==0)
			weight.need_katapults=0.05;
	}
	else {
		if(weight.need_kannons==0)
			weight.need_kannons=0.05;
		if(weight.need_katapults==0)
			weight.need_katapults=0.1;
	}

	if(weight.win_kastle==0)
		weight.win_kastle=0.01;

	/* Now, we choose the highest of the three objects */
	names = names.sort(function(a,b) {
							return(weight[b]-weight[a]);
						});

	while((name=names.shift())!=undefined) {
		var amount=0;
		switch(name) {
			case 'loss_soldiers':
			case 'need_soldiers':
				if(this.civilians > 1000) {
					amount=parseInt(other.soldiers * 1.25);
					if(amount > this.civilians/2)
						amount=parseInt(this.civilians/2);
					if(amount < other.soldiers * .5)
						amount=parseInt(other.soldiers * .5);
					amount -= this.soldiers;
					if(amount > this.civilians*.8)
						amount=parseInt(this.civilians*.75);
					if(amount < 1000)
						break;
					console.writeln("Drafting "+amount+" civilians into the army.");
					console.crlf();
					this.civilians -= amount;
					this.soldiers += amount;
					for(amount=0;amount<4;amount++)
						playmusic("MFT64L64O5CDP32CDP32CDP16");
					return(false);
				}
				break;
			case 'loss_kastle':
			case 'need_kastle':
				amount=parseInt(this.gold/10);
				if(this.kastle + amount > other.kastle * 2)
					amount = (other.kastle*2)-this.kastle;
				if(amount < 2000)
					break;
				console.writeln("Kastle reinforced by "+amount+" points!");
				this.loadwagons('kastle',amount);
				return(false);
			case 'loss_guards':
			case 'need_guards':
				amount=20-this.guards;
				if(amount*1000 > this.gold)
					amount=parseInt(this.gold/1000);
				if(amount < 5)
					break;
				console.writeln("Hiring "+amount+" guards for "+amount*1000);
				console.crlf();
				console.writeln("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
				this.loadwagons('guards',amount);
				return(false);
			case 'loss_food':
			case 'need_food':
				amount=this.gold*5;
				if(amount < 100)
					break;
				console.writeln("Buying "+amount+" sacks of food for "+amount/5+" gold!");
				console.crlf();
				console.writeln("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
				this.loadwagons('food',amount);
				return(false);
			case 'win_soldiers':
				if(this.soldiers > other.soldiers * 1.7)
					return(this.soldierattack(other));
				if(this.kannons > this.katapults * 500)
					return(this.kannonattack(other, true));
				if(this.katapults > 0)
					return(this.katapultattack(other, true));
				break;
			case 'win_kastle':
				if(this.kannons > this.katapults * 500)
					return(this.kannonattack(other, false));
				if(this.katapults > 0)
					return(this.katapultattack(other, false));
				break;
			case 'win_assassins':
				return(this.assassinate(other));
			case 'need_assassins':
				amount=4-this.assassins;
				if(amount*7500 > this.gold)
					amount=parseInt(this.gold/7500);
				if(amount < 1)
					break;
				console.writeln("Hiring "+amount+" assassins for "+amount*7500+" gold!");
				console.crlf();
				console.writeln("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
				this.loadwagons('assassins',amount);
				return(false);
			case 'need_katapults':
				amount=parseInt(this.gold/25000);
				if(this.mfood < 2)
					amount=parseInt(amount/2);
				if(this.katapults > 2 && amount < this.katapults/2)
					break;
				if(amount < 1)
					break;
				console.writeln("Buying "+amount+" katapults for "+amount*25000+" gold!");
				console.crlf();
				console.writeln("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
				this.loadwagons('katapults',amount);
				return(false);
			case 'need_kannons':
				amount=parseInt(this.gold/100);
				if(this.mfood < 2)
					amount=parseInt(amount/2);
				if(this.kannons > 2 && amount < this.kannons/2)
					break;
				if(amount < 20)
					break;
				console.writeln("Buying "+amount+" kannons for "+amount*100+" gold!");
				console.crlf();
				console.writeln("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
				this.loadwagons('kannons',amount);
				return(false);
		}
	}
	console.attributes=CYAN;
	console.writeln("* King Computer calls an urgent counsel with his closest advisors! *");
	return(false);
}

function Player_checkresults(other)
{
	var i;

	if(other.kastle==0) {
		if(other.isplayer) {
			console.attributes=LIGHTRED;
			console.writeln(proper(other.singular)+" stare"+other.conjugate+" in dismay as "+other.posessive+" kastle shudders, and then disintegrates");
			console.writeln("into a pile of rubble. "+proper(other.posessive)+" last vision is of a massive roof beam");
			console.writeln("as it comes crashing down towards "+other.singular+".");
		}
		else {
			console.attributes=LIGHTGREEN;
			playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
			console.writeln("A great cloud of dust and debris flies into the air after "+this.posessive+" final volley.");
			console.writeln(other.full_name+", outside in "+other.posessive+" gardens, stare"+other.conjugate+" in disbelief and horror as "+other.posessive);
			console.writeln("kastle disintegrates into a jagged mound of wood and stone.");
		}
		return(this);
	}
	if(other.soldiers==0 && this.soldiers > 0) {
		if(other.isplayer) {
			console.attributes=LIGHTRED;
			console.writeln("The last of "+other.posessive+" soldiers lay slain upon the once green fields of");
			console.writeln(other.posessive+" kingdom. Vultures are already decending. "+proper(other.singular)+" comsume"+other.conjugate+" a chalice");
			console.writeln("of poison laced wine as "+other.singular+" hear"+other.conjugate+" the guttural voices of "+this.full_name+"'s");
			console.writeln("soldiers smashing their way through "+other.posessive+" kastle gates.");
			for(i=0; i<40; i++)
				playmusic("MFO1T128L64CEB");
			return(this);
		}
		else {
			console.attributes=LIGHTGREEN;
			playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
			console.writeln(other.refer_posessive+" army lay in ruins upon the battle field. With noone left");
			console.writeln("to defend "+other.posessive+" kingdom, "+other.singular+" mounts a swift steed and escape"+other.conjugate+" into the night.");
			console.writeln(proper(this.singular)+" smile"+this.conjugate+" in grim satisfaction at the hard-won spoils of victory!");
			for(i=0; i<10; i++)
				playmusic("MFO1T128L64CEB");
		}
		return(this);
	}
	return(false);
}

function UserData(alias, wins, losses, score)
{
	this.alias=alias;
	this.wins=parseInt(wins);
	this.losses=parseInt(losses);
	this.score=parseInt(score);
	this.lines getter=function() { return(new Array(this.alias, this.wins, this.losses, this.score)); };
}

function pretty_number(num)
{
	var delimiter = ","; // replace comma if desired
	var i = parseInt(num);
	if(isNaN(i)) { return ''; }
	var minus = '';
	if(i < 0) { minus = '-'; }
	i = Math.abs(i);
	var n = new String(i);
	var a = [];
	while(n.length > 3)
	{
		var nn = n.substr(n.length-3);
		a.unshift(nn);
		n = n.substr(0,n.length-3);
	}
	if(n.length > 0) { a.unshift(n); }
	n = a.join(delimiter);
	num = n;
	num = minus + num;
	return num;
}

function update_userfile(player, computer, won)
{
	var f=new File(game_dir+"knk-user.dat");
	var computer_total=0;
	var player_total=0;
	var computer_wins=0;
	var computer_losses=0;
	var	updated=false;
	var lines=new Array();
	var line;
	var now=new Date();
	var nowmonth=['January','February','March','April','May','June','July','August','September','Optober','November','December'][now.getMonth()];
	var all_ud=new Array();

	if(!Lock(f.name, system.node_num, true, 1))
		return;
	if(!Lock(game_dir+"knk-best.asc", system.node_num, true, 1)) {
		return;
	}
	if(f.open("r")) {
		lines=f.readAll();
		f.close();
	}
	if(lines.length > 0) {
		var lastdate=new Date(lines[0]);
		if(now.getMonth() != lastdate.getMonth() || now.getYear() != lastdate.getYear()) {
			lines=new Array();
			if(Lock(game_dir+"knk-last.asc", system.node_num, true, 1)) {
				file_remove(game_dir+"knk-last.asc");
				file_rename(game_dir+"knk-best.asc", game_dir+"knk-last.asc")
				Unlock(game_dir+"knk-last.asc");
			}
		}
	}
	if(lines.length > 1)
		computer_total=parseInt(lines[1]);
	computer_total+=computer.score;
	for(line=2; line<lines.length; line+=4) {
		var ud=new UserData(lines[line], lines[line+1], lines[line+2], lines[line+3]);
		if(ud.alias==user.alias) {
			if(won)
				ud.wins++;
			else
				ud.losses++;
			ud.score+=player.score;
			updated=true;
		}
		computer_losses+=parseInt(ud.wins);
		computer_wins+=parseInt(ud.losses);
		all_ud.push(ud);
	}
	if(!updated)
		all_ud.push(new UserData(user.alias, won?1:0, won?0:1, player.score));
	all_ud=all_ud.sort(function(a,b) {
							return(b.score-a.score);
						});
	if(f.open("w")) {
		f.writeln(now.toString());
		f.writeln(computer_total);
		for(ud in all_ud) {
			f.writeln(all_ud[ud].lines);
			player_total += all_ud[ud].score;
		}
	}
	Unlock(f.name);
	f.close();
	f=new File(game_dir+"knk-best.asc");
	if(f.open("w")) {
		f.writeln("");
		f.writeln("* Kannons and Katapults Scoreboard for the Month of "+nowmonth+" *");
		f.writeln("");
		f.writeln("Rank  Name                   Games  Wins  Losses  Win %       Score");
		f.writeln("--------------------------------------------------------------------");
		for(ud in all_ud) {
			f.writeln(format("%3u   %-22s %5u  %4u  %6u  %3u %% %11s"
							,ud
							,all_ud[ud].alias
							,all_ud[ud].wins+all_ud[ud].losses
							,all_ud[ud].wins
							,all_ud[ud].losses
							,parseInt(all_ud[ud].wins/(all_ud[ud].wins+all_ud[ud].losses)*100)
							,pretty_number(all_ud[ud].score)));
		}
		f.writeln("--------------------------------------------------------------------");
		f.writeln("");
		f.writeln("* Kannons and Katapults Total Score for the Month of January *");
		f.writeln("");
		f.writeln("Rank  Name                   Games  Wins  Losses  Win %       Score");
		f.writeln("--------------------------------------------------------------------");
		for(line=0; line<2; line++) {
			if(computer_total < player_total || line==1) {
				f.writeln(format("%3u   %-22s %5u  %4u  %6u  %3u %% %11s"
								,line+1
								,'All Users'
								,computer_wins+computer_losses
								,computer_losses
								,computer_wins
								,parseInt(computer_losses/(computer_losses+computer_wins)*100)
								,pretty_number(player_total)));
			}
			else {
				f.writeln(format("%3u   %-22s %5u  %4u  %6u  %3u %% %11s"
								,line+1
								,'King Computer'
								,computer_wins+computer_losses
								,computer_wins
								,computer_losses
								,parseInt(computer_wins/(computer_wins+computer_losses)*100)
								,pretty_number(computer_total)));
			}
		}
		f.writeln("--------------------------------------------------------------------");
		f.writeln('');
		f.close();
	}
	Unlock(f.name);
}

function read_dat()
{
	var dat_file=new Object();
	var f=new File(game_dir+"knk.dat");

	dat_file.shortestgame=new Object();
	dat_file.longestgame=new Object();
	dat_file.lastgame=new Object();

	if(Lock(f.name, system.node, true, 1)) {
		if(f.open("r")) {
			dat_file.shortestgame.months=parseInt(f.readln());
			dat_file.shortestgame.winner==f.readln();
			dat_file.shortestgame.loser==f.readln();
			dat_file.longestgame.months==parseInt(f.readln());
			dat_file.longestgame.winner==f.readln();
			dat_file.longestgame.loser==f.readln();
			dat_file.lastgame.months==parseInt(f.readln());
			dat_file.lastgame.winner==f.readln();
			dat_file.lastgame.loser==f.readln();
			f.close();
		}
	}
	return(dat_file);
}

function write_dat(dat_file)
{
	var f=new File(game_dir+"knk.dat");

	if(Lock(f.name, system.node, true, 1)) {
		if(f.open("w")) {
			f.writeln(dat_file.shortestgame.months);
			f.writeln(dat_file.shortestgame.winner);
			f.writeln(dat_file.shortestgame.loser);
			f.writeln(dat_file.longestgame.months);
			f.writeln(dat_file.longestgame.winner);
			f.writeln(dat_file.longestgame.loser);
			f.writeln(dat_file.lastgame.months);
			f.writeln(dat_file.lastgame.winner);
			f.writeln(dat_file.lastgame.loser);
			f.close();
		}
		Unlock(f.name);
	}
}

function play_game()
{
	var month;
	var turn_order;
	var turn;
	var winner;
	var loser;

	turn=0;
	winner=false;
	month=1;
	player=new Player(title, name, true);
	computer=new Player("King", "Computer", false);
	turn_order=random(2)?(new Array(player,computer)):(new Array(computer,player));
	while(winner===false) {
		turn_order[turn].drawscreen(month);
		console.crlf();
		winner=turn_order[turn].move(month, turn_order[1-turn]);
		if(winner===false) {
			turn_order[turn].produce();
			turn++;
			month++;
			if(turn==2)
				turn=0;
		}
	}

	playmusic("MFO1T128L64CEB");
	playmusic("MFO1T128L64CEB");
	playmusic("MFO1T128L64CEB");
	playmusic("MFO1T128L64CEB");
	playmusic("MFO1T128L64CEB");
	playmusic("MFO2T96L2P32CL3CL8CP32L3CP6E-L8DL3DL8CL3CO1L8BO2L1C");
	if(winner.isplayer) {
		wins++;
		loser=computer;
		console.crlf();
		console.attributes=LIGHTGREEN|BLINK;
		console.writeln("Hail to the new "+winner.title+" of the empire!");
		console.attributes=LIGHTGREEN;
		playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
	}
	else {
		losses++;
		loser=player;
		playmusic("MFO2T96L2P32CL3CL8CP32L3CP6E-L8DL3DL8CL3CO1L8BO2L1C");
	}
	console.crlf();
	console.pause();
	winner.drawscreen(month);
	console.crlf();
	console.attributes=WHITE;
	loser.score=0;
	console.writeln(computer.full_name+" got "+computer.score+" points and you got "+player.score+" points for this game.");
	console.writeln();
	console.pause();
	update_userfile(player, computer, winner.isplayer);
	show_scoreboard(true);
	var dat_file=read_dat();
	if(dat_file==undefined || dat_file.shortestgame == undefined || dat_file.shortestgame.months == undefined || dat_file.shortestgame.months >= month) {
		console.attributes=YELLOW|BLINK;
		console.writeln("This is the shortest game to date!!!!");
		console.crlf();
		dat_file.shortestgame=new Object();
		dat_file.shortestgame.months=month;
		dat_file.shortestgame.winner=winner.full_name;
		dat_file.shortestgame.loser=loser.full_name;
	}
	if(dat_file==undefined || dat_file.longestgame == undefined || dat_file.longestgame.months == undefined || dat_file.longestgame.months <= month) {
		console.attributes=YELLOW|BLINK;
		console.writeln("This is the longest game to date!!!!");
		console.crlf();
		dat_file.longestgame=new Object();
		dat_file.longestgame.months=month;
		dat_file.longestgame.winner=winner.full_name;
		dat_file.longestgame.loser=loser.full_name;
	}
	console.attributes=WHITE;
	if(dat_file.lastgame != undefined && dat_file.lastgame.months != undefined)
		console.writeln("The last game lasted "+dat_file.lastgame.months+" months won by "+dat_file.lastgame.winner+" vs. "+dat_file.lastgame.loser+".");
	dat_file.lastgame=new Object();
	dat_file.lastgame.months=month;
	dat_file.lastgame.winner=winner.full_name;
	dat_file.lastgame.loser=loser.full_name;
	write_dat(dat_file);
	console.attributes=WHITE;
	console.writeln("It took "+winner.refer_singular+" "+month+" to beat "+loser.refer_singular+" this game.");
	console.crlf();
	console.attributes=CYAN;
	console.writeln("The longest game was "+dat_file.longestgame.months+" months won by "+dat_file.longestgame.winner+" vs. "+dat_file.longestgame.loser+".");
	console.crlf();
	console.attributes=CYAN;
	console.writeln("The shortest game was "+dat_file.shortestgame.months+" months won by "+dat_file.shortestgame.winner+" vs. "+dat_file.shortestgame.loser+".");
	console.crlf();
	console.attributes=LIGHTMAGENTA;
	console.writeln(proper(player.refer_singular)+" won "+wins+" games and "+computer.refer_singular+" won "+losses+" games this session.");
}

function play_again()
{
	console.crlf();
	console.attributes=YELLOW;
	console.write("Play again? [Y/N] -=> ");
	console.attributes=BROWN;
	return(getkeys("YN")=='Y');
}

show_intro();
do {
	play_game();
} while(play_again());
