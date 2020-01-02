// TODO: dk.system.name doesn't get populated...

load("dorkit.js");
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
	var ver;

	// Disable parsed input... we need to do ESC processing ourselves here.
	dk.console.print("\x1b[c");
	var response='';

	while(1) {
		if (!dk.console.waitkey(5000))
			break;
		var ch=dk.console.getbyte();
		if(ch === undefined || ch==="")
			break;
		response += ch;
		if(ch != '\x1b' && ch != '[' && (ch < ' ' || ch > '/') && (ch<'0' || ch > '?'))
			break;
	}

	if(response.substr(0,21) != "\x1b[=67;84;101;114;109;") {	// Not CTerm
		return(0);
	}
	if(response.substr(-1) != "c") {	// Not a DA
		return(0);
	}
	var version_str=response.substr(21);
	version_str=version_str.replace(/c/,"");
	ver=version_str.split(/;/);
	cterm_major=ver[0]|0;
	cterm_minor=ver[1]|0;
}

function getkeys(str)
{
	var key;

	while(1) {
		while(!dk.console.waitkey(10000));
		key=dk.console.getkey().toUpperCase();
		if(str.indexOf(key)!=-1)
			break;
	}
	if (dk.console.remote_screen !== undefined) {
		dk.console.remote_screen.new_lines = 0;
	}
	/* Don't echo control keys */
	if(ascii(key)<32)
		dk.console.println('');
	else
		dk.console.println(key);
	return(key);
}

function playmusic(str)
{
	if(music) {
		if(cterm_major == 1 && cterm_minor < 111 && cterm_minor >= 37) {
			str=str.replace(/^M[FBNLS]?/,'N');
		}
		else if((cterm_major == 1 && cterm_minor >= 111) || cterm_major > 1) {
			if(str.charAt(0)=='M')
				str='|'+str.substr(1);
		}
		dk.console.print("\x1b["+str+"\x0e");
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
	dk.console.clear();
	dk.console.attr = Attribute.CYAN;
	dk.console.println("");
	dk.console.center("Welcome to Kannons and Katapults "+dk.user.full_name+"!");
	dk.console.println("");
	dk.console.center("Version "+version+" * KNK * Last Updated "+updated);
	dk.console.println("");
	dk.console.center("ORIGINAL VERSION");
	dk.console.println("");
	dk.console.center("Copyright (c) 1991,1992,1993,1994,1995 by Alan Davenport. All Rights Reserved.");
	dk.console.println("");
	dk.console.center("by Alan Davenport");
	dk.console.println("");
	dk.console.center("of Al's Cabin BBS");
	dk.console.println("");
	dk.console.center("JavaScript Version");
	dk.console.println("");
	dk.console.center("Copyright (c) 2009 by Stephen Hurd. All Rights Reserved.");
	dk.console.println("");
	dk.console.center("Running on "+dk.system.name+" courtesy of "+dk.system.sysop_name+".");
	dk.console.println("");
	check_syncterm_music();
	dk.console.gotoxy(0,dk.console.rows-1);
	dk.console.pause();

	dk.console.clear();
	dk.console.println("");
	dk.console.println("");
	dk.console.println("");
	dk.console.println("");	dk.console.attr="HY";
	dk.console.println("The object of Kannons & Kataputs is to defeat King Computer. There are 3");
	dk.console.println("ways to do this. You may destroy his kastle, defeat his army or have him");
	dk.console.print(word_wrap("assassinated. If this sounds easy to you "+dk.user.alias+", you should be warned that King Computer is no push over!",77).replace(/\n/g,"\r\n"));
	dk.console.println("");
	dk.console.println(""); dk.console.attr="HW";
	dk.console.println("Although it may seem that the game is rigged, the odds are exactly even.");
	dk.console.println("A LOT of work and thousands of games have gone into making King Computer");
	dk.console.println("as smart as possible. It's you against him. Play carefully!!");
	dk.console.println("");
	dk.console.println(""); dk.console.attr="HG";
	dk.console.println("You each will recieve one point for every soldier killed, one point for every");
	dk.console.println("kastle point destroyed and 1,000 points for each guard passed by an assassin");
	dk.console.println("when successful. The loser loses their points. The scoreboard resets every");
	dk.console.println("month. Good Luck!");
	dk.console.println("");
	dk.console.println("");
	dk.console.println("");
	dk.console.println("");
	dk.console.aprint("\1h\1bPress [\1mP\1b] to Play KNK or [\1mQ\1b] to quit back to the BBS. -=> \1m");
	if(getkeys("PQ")=='Q')
		exit(0);

	dk.console.attr='M';
	playmusic("MFT64L64O5CDP32CDP32CDP16");
	dk.console.print("\x0f\b \b");
	dk.console.println("");
	dk.console.attr='G';
	dk.console.println("If your terminal supports ANSI sound, you just heard some.");
	dk.console.println("If it doesnt, you just saw some wierd characters.");
	dk.console.println("");
	dk.console.aprint("\1h\1gDo you want the ANSI sound effects? [Y/N] -=> \1n\1g");
	if(getkeys("YN")!='Y')
		music=false;
	playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
	while(1) {
		dk.console.println("");
		dk.console.aprint("\1hArt thou a [K] King or a [Q] Queen? -=> \1n\1g");
		if(getkeys("KQ")=='Q')
			title="Queen";
		else
			title="King";
		dk.console.println("");
		dk.console.aprint("\1hWhat name shall you be known by sire? -=> \1n\1g");
		name=dk.console.getstr({len:35});
		dk.console.println("");
		dk.console.aprint('\1h"'+title+' '+name+'" Is this OK? [y/N] -=> \1n\1g');
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
	this.kastle=12500+random(10000);
	this.soldiers=1750+random(3500);
	this.civilians=12500+random(5000);
	this.kannons=200+random(100);
	this.katapults=random(2);
	this.assassins=1+random(4);
	this.guards=8+random(15);
	this.gold=17500+random(2500);
	this.food=200000+random(100000);
	this.__defineGetter__("power",  function() { var p=((this.kastle/500)|0)+((this.soldiers/750)|0); if(p>46) return(46); return(p); });
	this.__defineGetter__("mfood", function() { if(this.soldiers==0 && this.civilians==0) return(0); if(this.food==0) return(0); var f=this.food/(this.soldiers*2+this.civilians); if(f<0.1) return(0); return(f); });
	this.__defineGetter__("mfoodstr", function() { return(format("%-6.3f", this.mfood).substr(0,6).replace(/\.$/,'')); });
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

	count=count|0;

	/*
	 * Round sacks up
	 */
	if(propname=='food') {
		if(count%5)
			count += (5-(count%5));
	}
	this.gold -= (price[propname]*count)|0;
	this[propname] += count;
	for(i=0; i<count*load_count[propname]; i++)
		playmusic("MFT80O5L64FP64CFP64F");
}

function Player_produce()
{
	var tmp;

	dk.console.println("");
	dk.console.attr='HW';
	dk.console.center("* Monthly Update *");
	dk.console.println("");
	if(this.isplayer)
		dk.console.attr='G';
	else
		dk.console.attr='C';
	tmp=(this.kastle/100)|0;
	if(tmp > 0)
		dk.console.println("* "+tmp+" civilians have immigrated to "+this.refer_posessive+" kastle.");
	this.civilians += tmp;
	tmp=0;
	if(this.civilians > 0)
		dk.console.println("* "+this.civilians+" gold was collected in taxes!");
	this.gold += this.civilians;
	tmp += this.civilians;

	if(this.soldiers) {
		if(this.gold < this.soldiers) {
			dk.console.attr.bright = true;;
			dk.console.println("* "+(this.soldiers-this.gold)+" of "+this.refer_posessive+" men defected from not being paid!!");
			this.soldiers=this.gold;
			dk.console.attr.bright = false;
		}
	}
	if(this.soldiers) {
		dk.console.println("* "+this.soldiers+" gold was paid to "+this.refer_posessive+" soldiers.");
		this.gold -= this.soldiers;
		tmp -= this.soldiers;
	}
	if(tmp > 0) {
		dk.console.println("* Treasury increased by "+tmp+" gold pieces!");
	}
	else {
		if(tmp < 0)
			dk.console.println("* Treasury DECREASED by "+(0-tmp)+" gold pieces!");
	}

	if(this.soldiers) {
		var canfeed=(this.food/2)|0;
		if(canfeed < this.soldiers) {
			dk.console.attr.bright = true;
			dk.console.println("* "+(this.soldiers-canfeed)+" of "+this.refer_posessive+" soldiers have starved to death!!");
			this.soldiers=canfeed;
			dk.console.attr.bright = false;
		}
	}
	tmp=this.soldiers*2;
	if(tmp > 0)
		dk.console.println("* "+tmp+" sacks of food were consumed by "+this.refer_posessive+" army.");
	this.food -= tmp;

	if(this.civilians) {
		if(this.food < this.civilians) {
			dk.console.attr.bright = true;
			dk.console.println("* "+(this.civilians-this.food)+" of "+this.refer_posessive+" citizens have starved to death!!");
			this.civilians=this.food;
			dk.console.attr.bright = false;
		}
	}
	if(this.civilians)
		dk.console.println("* "+this.civilians+" sacks of food were consumed by "+this.refer_posessive+" citizens.");
	this.food -= this.civilians;
	dk.console.println("");
}

function Player_powerbar()
{
	var lastattr=dk.console.attr.value;
	var pow=this.power;
	var i;
	var bars="\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe";

	if(pow > 0) {
		if(pow <= 10)
			dk.console.attr = 'HIR';
		else
			dk.console.attr='HR';
		dk.console.print(format("%.*s", pow>10?10:pow, bars));
	}
	if(pow > 10) {
		dk.console.attr='HY';
		dk.console.print(format("%.*s", pow>20?10:pow-10, bars));
	}
	if(pow > 20) {
		dk.console.attr='HG';
		dk.console.print(format("%.*s", pow-20, bars));
	}
	for(i=pow; i<46; i++)
		dk.console.print(' ');
	dk.console.attr.value=lastattr;
}

function Player_drawscreen(month)
{
	dk.console.clear();
	dk.console.println("");
	if(this.isplayer) {
		dk.console.aprint("\1h\1w*  Your Turn  *            \1h\1r*  Month # "+month+"  *\r\n");
		dk.console.println("");
		dk.console.attr='G';
	}
	else {
		var mstr=format("%-27s", "*  Month # "+month+"  *");
		dk.console.aprint("\1h\1r"+mstr+"\1w*  "+this.full_name+"'s Turn  *\r\n");
		dk.console.println("");
		dk.console.attr='C';
	}
	dk.console.println("\xc9\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcb\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbb");
	dk.console.print("\xba  "+format("%-24s",player.full_name)+"\xba  "+format("%-24s",computer.full_name)+"\xba\r\n");
	dk.console.println("\xcc\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xca\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xb9");
	dk.console.print("\xba You: ");
	player.powerbar();
	dk.console.println(" \xba");
	dk.console.print("\xba Him: ");
	computer.powerbar();
	dk.console.println(" \xba");
	dk.console.println("\xcc\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcb\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xb9");
	dk.console.println(format("\xba  Score..........: %-6d \xba  Score..........: %-6d \xba",player.score,computer.score));
	dk.console.println(format("\xba  Kastle Points..: %-6d \xba  Kastle Points..: %-6d \xba",player.kastle,computer.kastle));
	dk.console.println(format("\xba  Soldiers.......: %-6d \xba  Soldiers.......: %-6d \xba",player.soldiers,computer.soldiers));
	dk.console.println(format("\xba  Civilians......: %-6d \xba  Civilians......: %-6d \xba",player.civilians,computer.civilians));
	dk.console.println(format("\xba  Kannons........: %-6d \xba  Kannons........: %-6d \xba",player.kannons,computer.kannons));
	dk.console.println(format("\xba  Katapults......: %-6d \xba  Katapults......: %-6d \xba",player.katapults,computer.katapults));
	dk.console.println(format("\xba  Assassins......: %-6d \xba  Assassins......: %-6d \xba",player.assassins,computer.assassins));
	dk.console.println(format("\xba  Guards.........: %-6d \xba  Guards.........: %-6d \xba",player.guards,computer.guards));
	dk.console.println(format("\xba  Gold...........: %-6d \xba  Gold...........: %-6d \xba",player.gold,computer.gold));
	dk.console.println(format("\xba  Months of Food.: %-6s \xba  Months of Food.: %-6s \xba",player.mfoodstr,computer.mfoodstr));
	dk.console.println("\xc8\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xca\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbc");
}

function Player_assassinate(other)
{
	var passed=0;
	var killed=0;
	var i;

	this.assassins--;
	dk.console.println("");
	dk.console.attr='HG';
	dk.console.println("Attempting infiltration!");
	playmusic("MFO1T32L8EL64DL8EL64DL8EL64DL8EP8");
	dk.console.println("");
	dk.console.attr='HR';
	dk.console.println("He has to pass "+other.guards+" guards.");
	while(other.guards && passed < other.guards) {
		switch(random(5)) {
			case 0: // Failed
				dk.console.attr="HIR";
				dk.console.println(this.refer_posessive+" assassin was killed!");
				playmusic("MFT32L2O1L8dL4C");
				dk.console.attr.blink = false;
				return(false);
			case 1:	// Killed
				dk.console.attr="HIR"
				dk.console.println("KILLED "+(++killed));
				passed++;
				other.guards--;
				for(i=0; i<5; i++)
					playmusic("MFO1T128L64CEB");
				break;
			default:
				dk.console.attr="HR"
				dk.console.println("Passed "+(++passed)+"!");
				playmusic("MFO1T128L64CEB");
		}
	}
	this.score += 1000*passed;
	playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
	if(this.isplayer) {
		dk.console.attr="HG"
		dk.console.println("Stealthily, quietly, "+this.posessive+" assassin creeps into the inner chambers of "+other.title);
		dk.console.println(other.name+" and pours a small vial of colorless fluid into a goblet which sits");
		dk.console.println("upon "+other.refer_posessive+" banquet table. During dinner, "+other.singular+" suddenly clutch"+other.conjugate2+" "+other.posessive);
		dk.console.println("throat and tumble"+other.conjugate+" backward off of "+other.posessive+" seat.");
	}
	else {
		dk.console.attr="HR"
		dk.console.println(proper(other.singular)+" wake"+other.conjugate+" up, for a second, and see"+other.conjugate+" "+this.refer_posessive+" assassin standing");
		dk.console.println("over "+other.singular+" with a grim smile. His hand thrusts his knife into "+other.refer_posessive+" heart.");
		dk.console.println(proper(this.singular)+" sink"+this.conjugate+" into the long and everlasting sleep.");
	}
	playmusic("MFT64L64O5CDP32CDP32CDP16");
	return(this);
}

function Player_purchase(name, propname, cost)
{
	var max;

	max=(this.gold/cost)|0;
	dk.console.println("");
	dk.console.attr="HY1";
	dk.console.aprint("Press \1w[ENTER]\1y to abort.");
	dk.console.attr="HY"
	dk.console.println("");
	dk.console.println("");
	if(propname=="food") {
		var i=this.civilians+this.soldiers*2;
		dk.console.attr="R"
		dk.console.println("You have "+this.food+" sacks of food.");
		dk.console.println("This is enough to feed your population for "+((this.mfood)|0)+" months.");
		dk.console.println("You have "+this.gold+" gold and can afford "+max+" sacks of food.");
		dk.console.println("This will increase your reserves of food by "+(i==0?0:((max/i)|0))+" months.");
		dk.console.println("You need "+i+" sacks of food per month to feed your population.");
		dk.console.attr="HR"
		dk.console.println("");
		dk.console.print("Buy how many sacks? (M = MAX) -=> ");
		dk.console.attr="R"
	}
	else {
		dk.console.attr="HM"
		dk.console.print("You can afford "+max+" "+name+".");
		dk.console.attr="HC"
		dk.console.print(" Buy how many? (M = MAX) -=> ");
//		dk.console.attr="C"
	}
	i=dk.console.getstr({hotkeys:"M\r\n", upper_case:true, max:max, min:1, integer:true});
	if(i==='' || i=='\r' || i=='\n' || i==0)
		return(false);
	if(i=='M')
		i=max;
	i|=0;
	dk.console.attr="C"
	dk.console.println("");
	dk.console.println("Loading your wagons sire. Thank you for your gold!");
	this.loadwagons(propname,i);
	dk.console.println("");
	dk.console.attr="HB"
	dk.console.println("* A real pleasure serving you sire! *");
	return(i);
}

function Player_soldierattack(other)
{
	var i;
	var this_lost=0;
	var oth_lost=0;
	var orig_soldiers=this.soldiers;
	var clashes=0;

	dk.console.println("");
	dk.console.println("You have "+player.soldiers+" soldiers and "+computer.full_name+" has "+computer.soldiers+".");
	dk.console.println("");
	dk.console.println("Forces engaging!");
	playmusic("MFT32O3L16CL8FL16CL8FL64CP64CP64CP64L8F");
	dk.console.println("");
	dk.console.attr="HG"
	for(i=0; i<orig_soldiers && other.soldiers > 0; i++) {
		if(i%1000==0) {
			switch(random(3)) {
				case 0:
					dk.console.print(" * SMASH! *");
					break;
				case 1:
					dk.console.print(" * CLASH! *");
					break;
				case 2:
					dk.console.print(" * CRASH! *");
					break;
			}
			playmusic("MFO1T128L64CEB");
			clashes++;
			if(clashes%7==0)
				dk.console.println("");
		}
		switch(random(3)) {
			case 0:
				if(random(10)) {
					oth_lost++;
					other.soldiers--;
					this.score++;
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
		dk.console.println("");
	dk.console.println("");
	dk.console.attr="G"
	if(this.isplayer) {
		dk.console.println("You lost "+this_lost+" men and have "+this.soldiers+" left!");
		dk.console.println(other.full_name+" lost "+oth_lost+" men and has "+other.soldiers+" left!");
	}
	else {
		dk.console.println("You lost "+oth_lost+" men and have "+other.soldiers+" left!");
		dk.console.println(this.full_name+" lost "+this_lost+" men and has "+this.soldiers+" left!");
	}
	dk.console.println("");
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
		dk.console.attr="HY"
	else
		dk.console.attr="HC"

	for(i=0; i<damage; i++) {
		bad=random(7);	// Each damage does 0-7 points of "bad"
		for(j=0; j<bad; j++) {
			rnd=other.soldiers+other.kastle+other.guards+other.assassins+other.kannons+other.katapults+other.civilians;
			if(men)
				rnd += other.soldiers;
			else
				rnd += other.kastle;

			if(other.kastle==0 || other.soldiers==0 || rnd==0) {
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

			if(men) {
				if(rnd < other.soldiers) {
					this.score++;
					loss.soldiers++;
					other.soldiers--;
					total_cost+=1000;
				}
			}
			if(rnd < other.kastle) {
				this.score++;
				loss.kastle++;
				other.kastle--;
				total_cost+=1000;
			}
		}
		while(total_cost > 100000) {
			switch(random(2)) {
				case 0:
					dk.console.print("**BANG!** ");
					break;
				case 1:
					dk.console.print("**BOOM!** ");
					break;
			}
			playmusic("MFO1T128L64CEB");
			booms++;
			if(booms%7==0)
				dk.console.println("");
			total_cost -= 100000;
		}
	}

	if(total_cost) {
			switch(random(2)) {
				case 0:
					dk.console.print("**BANG!** ");
					break;
				case 1:
					dk.console.print("**BOOM!** ");
					break;
			}
			playmusic("MFO1T128L64CEB");
			booms++;
			if(booms%7==0)
				dk.console.println("");
	}
	if(booms%7)
		dk.console.println("");

	if(nodamage) {
		if(this.isplayer)
			dk.console.attr="HIY"
		else
			dk.console.attr="HIC"
		dk.console.println("No damage!");
		playmusic("MFT32L2O1L8dL4C");
	}

	dk.console.println("");

	if(loss.soldiers > 0) {
		dk.console.attr="HR"
		dk.console.println(proper(this.refer_singular+" killed "+loss.soldiers+" men!"));
	}
	if(loss.kastle > 0) {
		dk.console.attr="HG"
		dk.console.println(proper(this.refer_singular+" destroyed "+loss.kastle+" kastle points!"));
	}
	if(loss.guards > 0) {
		dk.console.attr="HY"
		dk.console.println(proper(this.refer_singular+" killed "+loss.guards+" guards!"));
	}
	if(loss.assassins > 0) {
		dk.console.attr="HB"
		dk.console.println(proper(this.refer_singular+" killed "+loss.assassins+" assassins!"));
	}
	if(loss.kannons > 0) {
		dk.console.attr="HM"
		dk.console.println(proper(this.refer_singular+" destroyed "+loss.kannons+" kannons!"));
	}
	if(loss.katapults > 0) {
		dk.console.attr="HC"
		dk.console.println(proper(this.refer_singular+" destroyed "+loss.katapults+" katapults!"));
	}
	if(loss.civilians > 0) {
		dk.console.attr="HW"
		dk.console.println(proper(this.refer_singular+" killed "+loss.civilians+" civilians!"));
	}

	dk.console.println("");
	return(this.checkresults(other));
}

function Player_kannonattack(other, men)
{
	dk.console.println("");
	dk.console.attr="Y"
	if(this.isplayer)
		dk.console.println("Firing Kannons!");
	else {
		dk.console.print("Firing kannons at your ");
		if(men)
			dk.console.println("men!");
		else
			dk.console.println("kastle!");
	}
	playmusic("MFO1L64T128BAAGGFFEEDDCP4");
	dk.console.println("");
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

	dk.console.println("");
	dk.console.attr="Y"
	if(this.isplayer)
		dk.console.println("Firing Katapult!");
	else {
		dk.console.print("Firing katapult at your ");
		if(men)
			dk.console.println("men!");
		else
			dk.console.println("kastle!");
	}
	playmusic("MFT76L64O2CDEFGABAGFEDCP4");
	dk.console.println("");
	return(this.dodamage(other, damage, men));
}

function show_scoreboard(thismonth)
{
	var f=new File(game_dir+(thismonth?"knk-best.asc":"knk-last.asc"));

	if(!file_exists(f.name)) {
		dk.console.println("");
		dk.console.attr="HG"
		dk.console.println("* Scoreboard file not found *");
		dk.console.println("");
		return;
	}
	if(Lock(f.name, dk.connection.node, false, 0.5)==false) {
		dk.console.println("");
		dk.console.attr="HG"
		dk.console.println("* Scoreboard file locked for write *");
		dk.console.println("");
		return;
	}
	if(!f.open("r", true)) {
		Unlock(f.name);
		dk.console.println("");
		dk.console.attr="HG"
		dk.console.println("* Scoreboard file can't be opened *");
		dk.console.println("");
		return;
	}
	var lines=f.readAll();
	f.close();
	Unlock(f.name);
	dk.console.attr="G"
	dk.console.println(lines.join("\r\n"));
}

function Player_playermove(month, other)
{
	var loop;
	var max;
	var i;
	var tl=dk.user.seconds_remaining;

	do {
		dk.console.aprint("\1n\1h\1cTime:\1g "+((tl/60)|0)+":"+format("%02d",tl%80)+"  \1y*  A,C,D,F,K,P,Q,R,S,T,Z,$ or ? for Help -=> \1n\1g");
		loop=false;
		switch(getkeys("ACDFKPQRSTZ$?\r\n")) {
			case '?':
				dk.console.println("");
				dk.console.println("A - Send Assassin");
				dk.console.println("C - Toggle Color ON/OFF");
				dk.console.println("D - Draft Civilians");
				dk.console.println("F - Fire Kannon");
				dk.console.println("K - Fire Katapult");
				dk.console.println("P - Pass Your Turn");
				dk.console.println("Q - Quit (You Lose)");
				dk.console.println("R - Release Troops from Service");
				dk.console.println("S - Soldiers vs. Soldiers");
				dk.console.println("T - Visit Trading Post");
				dk.console.println("Z - Toggle Sound ON/OFF");
				dk.console.println("$ - Show Scoreboard");
				dk.console.println("[ENTER] - Redraw Main Screen");
				dk.console.println("");
				loop=true;
				break;
			case 'A':
				if(this.assassins==0) {
					dk.console.println("");
					dk.console.attr="HIG";
					dk.console.println("You don't have any assassins!");
					dk.console.println("");
					loop=true;
					break;
				}
				return(this.assassinate(other));
			case 'C':
				/* TODO: Disable colour */
				dk.console.println("");
				dk.console.println("Toggle color not yet implemented!");
				dk.console.println("");
				loop=true;
				break;
			case 'D':
				dk.console.println("");
				dk.console.attr="HY1";
				dk.console.aprint("Press \1w[ENTER]\1y to abort.");
				dk.console.attr="G"
				dk.console.println("");
				dk.console.println("");
				dk.console.println("You have "+this.civilians+" civilians and "+this.soldiers+" soldiers.");
				dk.console.print("Draft how many civilians into the army? [0 - "+this.civilians+"] -=> ");
				var draft=parseInt(dk.console.getstr({max:this.civilians}), 10);
				if(draft<1)
					loop=true;
				else {
					dk.console.println("");
					this.civilians -= draft;
					this.soldiers += draft;
					dk.console.println("Done. You now have "+this.civilians+" civilians and "+this.soldiers+" soldiers");
					for(i=0; i<2; i++)
						playmusic("MFT64L64O5CDP32CDP32CDP16");
					return(false);
				}
				dk.console.println("");
				break;
			case 'F':
				if(this.kannons==0) {
					dk.console.println("");
					dk.console.attr="HIG";
					dk.console.println("You don't have any kannons!!");
					dk.console.println("");
					loop=true;
					break;
				}
				dk.console.println("");
				dk.console.attr="HY1";
				dk.console.aprint("Press \1w[ENTER]\1y to abort.");
				dk.console.attr="G"
				dk.console.println("");
				dk.console.println("");
				dk.console.print("Fire at the [M] Men or the [K] Kastle? [M/K] -=> ");
				switch(getkeys("MK\r\n")) {
					case '\r':
					case '\n':
						break;
					case 'M':
						return(this.kannonattack(other, true));
					case 'K':
						return(this.kannonattack(other, false));
				}
				dk.console.println("");
				loop=true;
				break;
			case 'K':
				if(this.katapults==0) {
					dk.console.println("");
					dk.console.attr="HIG";
					dk.console.println("You don't have any katapults!!");
					dk.console.println("");
					loop=true;
					break;
				}
				dk.console.println("");
				dk.console.attr="HY1";
				dk.console.aprint("Press \1w[ENTER]\1y to abort.");
				dk.console.attr="G"
				dk.console.println("");
				dk.console.println("");
				dk.console.print("Fire at the [M] Men or the [K] Kastle? [M/K] -=> ");
				switch(getkeys("MK\r\n")) {
					case '\r':
					case '\n':
						break;
					case 'M':
						return(this.katapultattack(other, true));
					case 'K':
						return(this.katapultattack(other, false));
				}
				dk.console.println("");
				loop=true;
				break;
			case 'P':
				dk.console.println("");
				dk.console.println("You sit on your throne and ponder the war.");
				dk.console.println("");
				return(false);
			case 'Q':
				dk.console.println("");
				dk.console.print("QUIT!? (You Lose) Are you SURE? -=> ");
				if(getkeys("YN\r\n")=='Y')
					return(other);
				dk.console.println("");
				loop=true;
				break;
			case 'R':
				dk.console.println("");
				dk.console.attr="HY1";
				dk.console.aprint("Press \1w[ENTER]\1y to abort.");
				dk.console.attr="G"
				dk.console.println("");
				dk.console.println("");
				dk.console.println("You have "+this.civilians+" civilians and "+this.soldiers+" soldiers.");
				dk.console.print("Send how many soldiers home? -=> ");
				var draft=parseInt(dk.console.getstr({max:this.soldiers}), 10);
				if(draft<1)
					loop=true;
				else {
					dk.console.println("");
					this.civilians += draft;
					this.soldiers -= draft;
					dk.console.println("Done. You now have "+this.civilians+" civilians and "+this.soldiers+" soldiers");
					playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
					return(false);
				}
				dk.console.println("");
				break;
			case 'S':
				return(this.soldierattack(other));
				break;
			case 'T':
				dk.console.println("");
				dk.console.println("");
				dk.console.attr="HG"
				dk.console.println(" * Welcome to the Trading Post sire! *");
				loop=true;
				while(loop) {
					dk.console.println("");
					dk.console.attr="Y"
					dk.console.println("[L] - Leave the Trading Post");
					dk.console.println("[B] - Buy Kannons @ 100 Gold Each");
					dk.console.println("[G] - Hire Guards @ 1000 Gold Each");
					dk.console.println("[A] - Hire Assasins @ 7500 Gold Each");
					dk.console.println("[K] - Buy Katapults @ 25000 Gold Each");
					dk.console.println("[F] - Buy Sacks of Food @ 5 Per Each Gold");
					dk.console.println("[R] - Raise Fortifications @ 10 Gold per Point");
					dk.console.println("");
					dk.console.attr="HG"
					dk.console.print("Total Gold: "+this.gold+" -=> ");
					dk.console.attr="G"
					switch(getkeys("LBGAKFR")) {
						case 'L':
							dk.console.println("");
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
				dk.console.println("");
				music=!music;
				dk.console.println("Sounds "+(music?"ON":"OFF"));
				dk.console.println("");
				loop=true;
				break;
			case '$':
				dk.console.println("");
				dk.console.attr="HG"
				dk.console.print("Show which [T] this months or [L] last months scoreboard? -=> ");
				dk.console.attr="G"
				if(getkeys("TL")=='L')
					show_scoreboard(false);
				else
					show_scoreboard(true);
				loop=true;
				break;
			case '\r':
			case '\n':
				this.drawscreen(month);
				dk.console.println("");
				loop=true;
				break;
		}
	} while(loop);
}

function Player_computermove(month, other)
{
	var weight={loss_soldiers:0, loss_kastle:0, loss_guards:0, loss_food:0
				,win_soldiers:0, win_kastle:0, win_assassins:0
				,need_assassins:0, need_weapons:0, need_food:0
				,need_soldiers:0, need_guards:0, need_kastle:0
				,need_release:0};
	var names=[];
	var name;

	dk.console.aprint("\1n\1h\1c                      * \1h\1i\1rT\1gh\1yi\1bn\1mk\1ci\1wn\1yg\1c\1n\1h\1c *\1n\1c");
	mswait(2000);
	if (dk.console.remote_screen !== undefined)
		dk.console.remote_screen.new_lines = 0;
	dk.console.println("");
	dk.console.println("");

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
		weight.need_weapons=1;
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
		if(this.soldiers < 3000) {
			weight.need_soldiers += 1 - this.soldiers/3000;
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

	if(this.mfood<.5)
		weight.loss_food=1;
	else {
		if(this.mfood < 2)
			weight.need_food += 1 - ((this.mfood-.5)/2+.25);
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
		weight.need_weapons=1;
	}
	else {
		if(other.kastle < 10000)
			weight.win_kastle += 1-other.kastle/10000;
	}
	if(weight.need_weapons==0)
		weight.need_weapons=0.05;

	if(weight.win_kastle==0)
		weight.win_kastle=0.01;

	if(other.mfood < .9 && this.mfood > 2) {
		if(this.soldiers + this.civilians > other.soldiers * 3)
			weight.need_soldiers =  1-(other.mfood/3);
	}

	if(this.gold + this.civilians - this.soldiers <= 0) {
		need_release=1;
	}
	else if(this.soldiers > other.soldiers * 1.66) {
		/* If we don't have enough tax revenue to pay for our soldiers
			this is vital */
		if(this.civilans - this.soldiers <= 0) {
			/* If we will lose gold, this is merely very important */
			need_release=0.75;
		}
		else {
			/* Just a thought... */
			need_release=0.1;
		}
	}

	/* Now, we choose the highest of the three objects */
	names = names.sort(function(a,b) {
							return(weight[b]-weight[a]);
						});

	while((name=names.shift())!==undefined) {
		var amount=0;
		switch(name) {
			case 'loss_soldiers':
			case 'need_soldiers':
				if(this.civilians > 1000) {
					amount=(other.soldiers * 1.25)|0;
					if(other.mfood < .9 && (this.soldiers + this.civilians > other.soldiers * 3))
						amount=this.civilians;
					else {
						if(amount > this.civilians/2)
							amount=(this.civilians/2)|0;
						if(amount < other.soldiers * .5)
							amount=(other.soldiers * .5)|0;
						amount -= this.soldiers;
						if(amount > this.civilians*.8)
							amount=(this.civilians*.75)|0;
						if(amount < 1000)
							break;
					}
					dk.console.println("Drafting "+amount+" civilians into the army.");
					dk.console.println("");
					this.civilians -= amount;
					this.soldiers += amount;
					for(amount=0;amount<4;amount++)
						playmusic("MFT64L64O5CDP32CDP32CDP16");
					return(false);
				}
				break;
			case 'loss_kastle':
			case 'need_kastle':
				amount=(this.gold/10)|0;
				if(this.kastle + amount > other.kastle * 2)
					amount = (other.kastle*2)-this.kastle;
				if(amount < 2000)
					break;
				dk.console.println("Kastle reinforced by "+amount+" points!");
				this.loadwagons('kastle',amount);
				return(false);
			case 'loss_guards':
			case 'need_guards':
				amount=20-this.guards;
				if(amount*1000 > this.gold)
					amount=(this.gold/1000)|0;
				if(amount < 5)
					break;
				dk.console.println("Hiring "+amount+" guards for "+amount*1000);
				dk.console.println("");
				dk.console.println("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
				this.loadwagons('guards',amount);
				return(false);
			case 'loss_food':
			case 'need_food':
				amount=this.gold*5;
				if(amount < 100)
					break;
				dk.console.println("Buying "+amount+" sacks of food for "+amount/5+" gold!");
				dk.console.println("");
				dk.console.println("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
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
					amount=(this.gold/7500)|0;
				if(amount < 1)
					break;
				dk.console.println("Hiring "+amount+" assassins for "+amount*7500+" gold!");
				dk.console.println("");
				dk.console.println("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
				this.loadwagons('assassins',amount);
				return(false);
			case 'need_weapons':
				if(this.kannons > this.katapults * 500 && (this.kannons > 500 || this.gold < 25000)) {
					amount=(this.gold/100)|0;
					if(this.mfood < 2)
						amount=amount>>1;
					if(this.kannons > 2 && amount < this.kannons/2)
						break;
					if(amount < 20)
						break;
					dk.console.println("Buying "+amount+" kannons for "+amount*100+" gold!");
					dk.console.println("");
					dk.console.println("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
					this.loadwagons('kannons',amount);
					return(false);
				}
				else {
					amount=(this.gold/25000)|0;
					if(this.mfood < 2)
						amount=(amount/2)|0;
					if(this.katapults > 2 && amount < this.katapults/2)
						break;
					if(amount < 1)
						break;
					dk.console.println("Buying "+amount+" katapults for "+amount*25000+" gold!");
					dk.console.println("");
					dk.console.println("The draft animals strain to haul the heavy wagons to "+this.refer_posessive+" kastle!");
					this.loadwagons('katapults',amount);
					return(false);
				}
				break;
			case 'need_release':
				if(this.gold + this.civilians - this.soldiers <= 0)
					amount=this.soldiers-(this.gold+this.civilians);
				else if(this.soldiers > other.soldiers * 1.66)
					amount=this.soldiers-(other.soldiers * 1.66);
				if(amount < this.civilians / 10)
					break;
				if(amount > this.soldiers)
					amount=this.soldiers / 2;
				if(amount < 1)
					break;
				dk.console.println("Retiring "+amount+" soldiers from service.");
				this.soldiers -= amount;
				this.civilians += amount;
				playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
				return(false);
				
		}
	}
	dk.console.attr="C"
	dk.console.println("* King Computer calls an urgent counsel with his closest advisors! *");
	return(false);
}

function Player_checkresults(other)
{
	var i;

	if(other.kastle==0) {
		if(other.isplayer) {
			dk.console.attr="HR"
			dk.console.println(proper(other.singular)+" stare"+other.conjugate+" in dismay as "+other.posessive+" kastle shudders, and then disintegrates");
			dk.console.println("into a pile of rubble. "+proper(other.posessive)+" last vision is of a massive roof beam");
			dk.console.println("as it comes crashing down towards "+other.singular+".");
		}
		else {
			dk.console.attr="HG"
			playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
			dk.console.println("A great cloud of dust and debris flies into the air after "+this.posessive+" final volley.");
			dk.console.println(other.full_name+", outside in "+other.posessive+" gardens, stare"+other.conjugate+" in disbelief and horror as "+other.posessive);
			dk.console.println("kastle disintegrates into a jagged mound of wood and stone.");
		}
		return(this);
	}
	if(other.soldiers==0 && this.soldiers > 0) {
		if(other.isplayer) {
			dk.console.attr="HR"
			dk.console.println("The last of "+other.posessive+" soldiers lay slain upon the once green fields of");
			dk.console.println(other.posessive+" kingdom. Vultures are already decending. "+proper(other.singular)+" comsume"+other.conjugate+" a chalice");
			dk.console.println("of poison laced wine as "+other.singular+" hear"+other.conjugate+" the guttural voices of "+this.full_name+"'s");
			dk.console.println("soldiers smashing their way through "+other.posessive+" kastle gates.");
			for(i=0; i<40; i++)
				playmusic("MFO1T128L64CEB");
			return(this);
		}
		else {
			dk.console.attr="HG"
			playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
			dk.console.println(other.refer_posessive+" army lay in ruins upon the battle field. With noone left");
			dk.console.println("to defend "+other.posessive+" kingdom, "+other.singular+" mounts a swift steed and escape"+other.conjugate+" into the night.");
			dk.console.println(proper(this.singular)+" smile"+this.conjugate+" in grim satisfaction at the hard-won spoils of victory!");
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
	this.wins=(wins)|0;
	this.losses=(losses)|0;
	this.score=(score)|0;
	this.__defineGetter__("lines", function() { return([this.alias, this.wins, this.losses, this.score].join('\r\n')); });
}

function pretty_number(num)
{
	var delimiter = ","; // replace comma if desired
	var i = (num)|0;
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
	var lines=[];
	var line;
	var now=new Date();
	var nowmonth=['January','February','March','April','May','June','July','August','September','Optober','November','December'][now.getMonth()];
	var all_ud=[];

	if(!Lock(f.name, dk.connection.node, true, 1))
		return;
	if(!Lock(game_dir+"knk-best.asc", dk.connection.node, true, 1)) {
		return;
	}
	if(f.open("r")) {
		lines=f.readAll();
		f.close();
	}
	if(lines.length > 0) {
		var lastdate=new Date(lines[0]);
		if(now.getMonth() != lastdate.getMonth() || now.getYear() != lastdate.getYear()) {
			lines=[];
			if(Lock(game_dir+"knk-last.asc", dk.connection.node, true, 1)) {
				file_remove(game_dir+"knk-last.asc");
				file_rename(game_dir+"knk-best.asc", game_dir+"knk-last.asc")
				Unlock(game_dir+"knk-last.asc");
			}
		}
	}
	if(lines.length > 1)
		computer_total=(lines[1])|0;
	computer_total+=computer.score;
	for(line=2; line<lines.length; line+=4) {
		var ud=new UserData(lines[line], lines[line+1], lines[line+2], lines[line+3]);
		if(ud.alias==dk.user.alias) {
			if(won)
				ud.wins++;
			else
				ud.losses++;
			ud.score+=player.score;
			updated=true;
		}
		computer_losses+=(ud.wins)|0;
		computer_wins+=(ud.losses)|0;
		all_ud.push(ud);
	}
	if(!updated)
		all_ud.push(new UserData(dk.user.alias, won?1:0, won?0:1, player.score));
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
							,(all_ud[ud].wins/(all_ud[ud].wins+all_ud[ud].losses)*100)|0
							,pretty_number(all_ud[ud].score)));
		}
		f.writeln("--------------------------------------------------------------------");
		f.writeln("");
		f.writeln("* Kannons and Katapults Total Score for the Month of January *");
		f.writeln("");
		f.writeln("Rank  Name                   Games  Wins  Losses  Win %       Score");
		f.writeln("--------------------------------------------------------------------");
		for(line=0; line<2; line++) {
			if((computer_total <= player_total && line==0) || (computer_total > player_total && line==1)) {
				f.writeln(format("%3u   %-22s %5u  %4u  %6u  %3u %% %11s"
								,line+1
								,'All Users'
								,computer_wins+computer_losses
								,computer_losses
								,computer_wins
								,(computer_losses/(computer_losses+computer_wins)*100)|0
								,pretty_number(player_total)));
			}
			else {
				f.writeln(format("%3u   %-22s %5u  %4u  %6u  %3u %% %11s"
								,line+1
								,'King Computer'
								,computer_wins+computer_losses
								,computer_wins
								,computer_losses
								,(computer_wins/(computer_wins+computer_losses)*100)|0
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

	if(Lock(f.name, dk.connection.node, true, 1)) {
		if(f.open("r")) {
			dat_file.shortestgame.months=(f.readln())|0;
			dat_file.shortestgame.winner=f.readln();
			dat_file.shortestgame.loser=f.readln();
			dat_file.longestgame.months=(f.readln())|0;
			dat_file.longestgame.winner=f.readln();
			dat_file.longestgame.loser=f.readln();
			dat_file.lastgame.months=(f.readln())|0;
			dat_file.lastgame.winner=f.readln();
			dat_file.lastgame.loser=f.readln();
			f.close();
		}
	}
	return(dat_file);
}

function write_dat(dat_file)
{
	var f=new File(game_dir+"knk.dat");

	if(Lock(f.name, dk.connection.node, true, 1)) {
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
	turn_order=random(2)?([player,computer]):([computer,player]);
	while(winner===false) {
		turn_order[turn].drawscreen(month);
		dk.console.println("");
		winner=turn_order[turn].move(month, turn_order[1-turn]);
		dk.console.pause();
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
		dk.console.println("");
		dk.console.attr="HIG";
		dk.console.println("Hail to the new "+winner.title+" of the empire!");
		dk.console.attr="HG"
		playmusic("MFT96O4L32P32CP64CP64CP64L16EP64L32CP64L12E");
	}
	else {
		losses++;
		loser=player;
	}
	dk.console.println("");
	if(dk.console.pos.y > dk.console.rows/2)
		dk.console.pause();
	winner.drawscreen(month);
	dk.console.println("");
	dk.console.attr="HW"
	loser.score=0;
	dk.console.println(computer.full_name+" got "+computer.score+" points and you got "+player.score+" points for this game.");
	dk.console.println('');
	if(dk.console.pos.y > dk.console.rows/2)
		dk.console.pause();
	update_userfile(player, computer, winner.isplayer);
	show_scoreboard(true);
	if(dk.console.pos.y > dk.console.rows/2)
		dk.console.pause();
	var dat_file=read_dat();
	if(dat_file===undefined || dat_file.shortestgame === undefined || dat_file.shortestgame.months === undefined || dat_file.shortestgame.months >= month) {
		dk.console.attr="HIY"
		dk.console.println("This is the shortest game to date!!!!");
		dk.console.println("");
		dat_file.shortestgame=new Object();
		dat_file.shortestgame.months=month;
		dat_file.shortestgame.winner=winner.full_name;
		dat_file.shortestgame.loser=loser.full_name;
	}
	if(dat_file===undefined || dat_file.longestgame === undefined || dat_file.longestgame.months === undefined || dat_file.longestgame.months <= month) {
		dk.console.attr="HIY"
		dk.console.println("This is the longest game to date!!!!");
		dk.console.println("");
		dat_file.longestgame=new Object();
		dat_file.longestgame.months=month;
		dat_file.longestgame.winner=winner.full_name;
		dat_file.longestgame.loser=loser.full_name;
	}
	dk.console.attr="HW"
	if(dat_file.lastgame !== undefined && dat_file.lastgame.months !== undefined)
		dk.console.println("The last game lasted "+dat_file.lastgame.months+" months won by "+dat_file.lastgame.winner+" vs. "+dat_file.lastgame.loser+".");
	dat_file.lastgame=new Object();
	dat_file.lastgame.months=month;
	dat_file.lastgame.winner=winner.full_name;
	dat_file.lastgame.loser=loser.full_name;
	write_dat(dat_file);
	dk.console.attr="HW"
	dk.console.println("It took "+winner.refer_singular+" "+month+" to beat "+loser.refer_singular+" this game.");
	dk.console.println("");
	dk.console.attr="C"
	dk.console.println("The longest game was "+dat_file.longestgame.months+" months won by "+dat_file.longestgame.winner+" vs. "+dat_file.longestgame.loser+".");
	dk.console.println("");
	dk.console.attr="C"
	dk.console.println("The shortest game was "+dat_file.shortestgame.months+" months won by "+dat_file.shortestgame.winner+" vs. "+dat_file.shortestgame.loser+".");
	dk.console.println("");
	dk.console.attr="HM"
	dk.console.println(proper(player.refer_singular)+" won "+wins+" games and "+computer.refer_singular+" won "+losses+" games this session.");
}

function play_again()
{
	dk.console.println("");
	dk.console.attr="HY"
	dk.console.print("Play again? [Y/N] -=> ");
	dk.console.attr="Y"
	return(getkeys("YN")=='Y');
}

show_intro();
do {
	play_game();
} while(play_again());
