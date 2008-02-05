function supplant()
{
	return((random(40)+80)/100);
}

var Status = {
	DEAD : (1<<0),
	ONLINE : (1<<1),
	PLAYER : (1<<2),
	MONSTER : (1<<3),
};

var SexType = {
	HE : 0,
	HIS : 1,
	HIM : 2,
};

function Player(name) {
	if(name==undefined)
		name=user.alias;
	this.name=name;		// User name
	this.pseudo=name;	// Game alias
	this.killer="";		// Killed by if status==DEAD
	this.bcry="";		// Battle cry
	this.winmsg="";		// What the user says when he wins
	this.gaspd="";		// Users dying curse
	this.laston=system.datestr();		// Date last on
	this.sex='M';
	this.status=Status.DEAD;		// ALIVE, DEAD, PLAYER, MONSTER
	this.flights=0;		// Number of times the user can play today
	this.level=0;			// Level (???)
	this.strength=0;
	this.intelligence=0;
	this.dexterity=0;
	this.constitution=0;
	this.charisma=0;
	this.experience=0;
	this.luck=0;
	this.weapon=new Weapon("Hands",1,1,0);		// Weapon
	this.armour=new Armour("Tunic",1,0);		// Armour
	this.hps=0;			// Hitpoints
	this.damage=0;		// Damage taken so far
	this.gold=0;		// Gold on hand
	this.bank=0;		// Gold in the bank
	this.wins=0;		// Wins so far
	this.losses=0;		// Losses so far
	this.battles=0;		// Battles left today
	this.fights=0;		// Fights left today
    // Players variance... applied to various rolls, Generally, how good the
    // player is feeling. Is set at start and not modified
	this.vary=0;

	this.his="his";
	this.he="he";
	this.him="him";
	this.His="His";
	this.He="He";
	this.Him="Him";
	this.load=Player_load;
	this.save=Player_save;
	this.statshow=Player_statshow;
	this.levelupdate=Player_levelupdate;
	this.chstats=Player_chstats;
	this.depobank=Player_depobank;
	this.withdrawbank=Player_withdrawbank;
	this.heal=Player_heal;
	this.vic=Player_changemessage;
	this.create=Player_create;
	this.gamble=Player_gamble;
	this.training=Player_training;
	this.leave=Player_leave;
	this.battle=Player_battle;
	this.tohit=Player_tohit;
	this.damageroll=Player_damage;
	this.changename=Player_changename;

	this.__defineGetter__("his", function() {
		if(this.charisma < 8)
			return("its");
		switch(this.sex) {
			case 'F':
				return("hers");
			case 'M':
				return("his");
			default:
				return("its");
		}
	});
	
	this.__defineGetter__("he", function() {
		if(this.charisma < 8)
			return("it");
		switch(this.sex) {
			case 'F':
				return("she");
			case 'M':
				return("he");
			default:
				return("it");
		}
	});
	
	this.__defineGetter__("him", function() {
		if(this.charisma < 8)
			return("it");
		switch(this.sex) {
			case 'F':
				return("her");
			case 'M':
				return("him");
			default:
				return("it");
		}
	});

	this.__defineGetter__("His", function() {
		if(this.charisma < 8)
			return("Its");
		switch(this.sex) {
			case 'F':
				return("Hers");
			case 'M':
				return("His");
			default:
				return("Its");
		}
	});
	
	this.__defineGetter__("He", function() {
		if(this.charisma < 8)
			return("It");
		switch(this.sex) {
			case 'F':
				return("She");
			case 'M':
				return("He");
			default:
				return("It");
		}
	});
	
	this.__defineGetter__("Him", function() {
		if(this.charisma < 8)
			return("It");
		switch(this.sex) {
			case 'F':
				return("Her");
			case 'M':
				return("Him");
			default:
				return("It");
		}
	});
};

function Player_statshow()
{
	console.clear();
	console.attributes="M0";
	console.writeln("Name: "+this.pseudo+"   Level: "+this.level);
	console.attributes="HC0";
	console.write("W/L: "+this.wins+"/"+this.losses);
	console.writeln("   Exp: "+this.experience);
	console.crlf();
	console.attributes="HY0";
	console.writeln("Steel  (in hand): "+this.gold);
	console.writeln("Steel  (in bank): "+this.bank);
	console.crlf();
	console.attributes="HB0";
	console.write("Battles: "+this.battles);
	console.write("   Flights: "+this.flights);
	console.write("   Fights: "+this.fights);
	console.writeln("   HPs: "+(this.hps-this.damage)+"("+this.hps+")");
	console.crlf();
	console.attributes="HC0";
	console.write("Weapon: "+this.weapon.name);
	console.writeln("     Armor: "+this.armour.name);
}

function Player_load(filename)
{
	if(filename==undefined)
		filename=getpath()+"players.ini";
	var players=new File(filename);
	var item_name;
	var item_cost;
	var item_power;
	var item_attack;

	if(!Lock(players.name, bbs.node_num, false, 1)) {
		console.crlf();
		console.writeln("Cannot lock "+players.name+" for read!");
		console.crlf();
		console.pause();
		return(false);
	}
	if(!players.open("r", true)) {
		Unlock(players.name);
		return(false);
	}

	this.pseudo=players.iniGetValue(this.name, "Alias", this.name);		// Game alias
	this.killer=players.iniGetValue(this.name, "Killer", "");		// Killed by if status==DEAD
	this.bcry=players.iniGetValue(this.name, "BattleCry", "");		// Battle cry
	this.winmsg=players.iniGetValue(this.name, "WinMessage", "");		// What the user says when he wins
	this.gaspd=players.iniGetValue(this.name, "DyingCurse", "");		// Users dying curse
	this.laston=players.iniGetValue(this.name, "LastOn", system.datestr());		// Date last on
	this.sex=players.iniGetValue(this.name, "Sex", "M");
	this.status=players.iniGetValue(this.name, "Status", Status.DEAD);		// ALIVE, DEAD, PLAYER, MONSTER
	this.flights=players.iniGetValue(this.name, "Flights", 0);		// Number of times the user can play today
	this.level=players.iniGetValue(this.name, "Level", 0);			// Level (???)
	this.strength=players.iniGetValue(this.name, "Strength", 0);
	this.intelligence=players.iniGetValue(this.name, "Intelligence", 0);
	this.dexterity=players.iniGetValue(this.name, "Dexterity", 0);
	this.constitution=players.iniGetValue(this.name, "Constitution", 0);
	this.charisma=players.iniGetValue(this.name, "Charisma", 0);
	this.experience=players.iniGetValue(this.name, "Experience", 0);
	this.luck=players.iniGetValue(this.name, "Luck", 0);

	item_name=players.iniGetValue(this.name, "Weapon", "Hands");		// Weapon
	item_attack=players.iniGetValue(this.name, "WeaponAttack", 1);		// Weapon attack
	item_power=players.iniGetValue(this.name, "WeaponPower", 1);		// Weapon power
	item_cost=players.iniGetValue(this.name, "WeaponCost", 0);		// Weapon power
	this.weapon=new Weapon(item_name, item_attack, item_power, item_cost);

	item_name=players.iniGetValue(this.name, "Armour", "Tunic");		// Armour index
	item_power=players.iniGetValue(this.name, "ArmourPower", 1);		// Weapon power
	item_cost=players.iniGetValue(this.name, "ArmourCost", 0);		// Weapon power
	this.armour=new Armour(item_name, item_power, item_cost);

	this.hps=players.iniGetValue(this.name, "HitPoints", 0);			// Hitpoints
	this.damage=players.iniGetValue(this.name, "Damage", 0);		// Damage taken so far
	this.gold=players.iniGetValue(this.name, "GoldOnHand", 0);		// Gold on hand
	this.bank=players.iniGetValue(this.name, "GoldInBank", 0);		// Gold in the bank
	this.wins=players.iniGetValue(this.name, "Wins", 0);		// Wins so far
	this.losses=players.iniGetValue(this.name, "Losses", 0);		// Losses so far
	this.battles=players.iniGetValue(this.name, "Battles", 0);		// Battles left today
	this.fights=players.iniGetValue(this.name, "Fights", 0);		// Fights left today
    // Players variance... applied to various rolls, Generally, how good the
    // player is feeling. Is set at start and not modified
	this.vary=players.iniGetValue(this.name, "Vary", 0.8);
	if(this.hps-this.damage <=0)
		this.status |= Status.DEAD;
	players.close();
	Unlock(players.name);

	return(true);
}

function Player_save(name)
{
	var players=new File(getpath()+"players.ini");

	if(!Lock(players.name, bbs.node_num, true, 1)) {
		console.crlf();
		console.writeln("Cannot lock "+players.name+" for write!");
		console.crlf();
		console.pause();
		return(false);
	}
	if(!players.open(file_exists(players.name) ? 'r+':'w+')) {
		Unlock(players.name);
		return(false);
	}

	players.iniSetValue(this.name, "Alias", this.pseudo);		// Game alias
	players.iniSetValue(this.name, "Killer", this.killer);		// Killed by if status==DEAD
	players.iniSetValue(this.name, "BattleCry", this.bcry);		// Battle cry
	players.iniSetValue(this.name, "WinMessage", this.winmsg);		// What the user says when he wins
	players.iniSetValue(this.name, "DyingCurse", this.gaspd);		// Users dying curse
	players.iniSetValue(this.name, "LastOn", this.laston);		// Date last on
	players.iniSetValue(this.name, "Sex", this.sex);
	players.iniSetValue(this.name, "Status", this.status);		// ALIVE, DEAD, PLAYER, MONSTER
	players.iniSetValue(this.name, "Flights", this.flights);		// Number of times the user can play today
	players.iniSetValue(this.name, "Level", this.level);			// Level (???)
	players.iniSetValue(this.name, "Strength", this.strength);
	players.iniSetValue(this.name, "Intelligence", this.intelligence);
	players.iniSetValue(this.name, "Dexterity", this.dexterity);
	players.iniSetValue(this.name, "Constitution", this.constitution);
	players.iniSetValue(this.name, "Charisma", this.charisma);
	players.iniSetValue(this.name, "Experience", this.experience);
	players.iniSetValue(this.name, "Luck", this.luck);
	players.iniSetValue(this.name, "Weapon", this.weapon.name);		// Weapon index
	players.iniSetValue(this.name, "WeaponAttack", this.weapon.attack);		// Weapon attack
	players.iniSetValue(this.name, "WeaponPower", this.weapon.power);		// Weapon power
	players.iniSetValue(this.name, "WeaponCost", this.weapon.cost);		// Weapon cost
	players.iniSetValue(this.name, "Armour", this.armour.name);		// Armour index
	players.iniSetValue(this.name, "ArmourPower", this.armour.power);		// Armour index
	players.iniSetValue(this.name, "ArmourCost", this.armour.cost);		// Armour index
	players.iniSetValue(this.name, "HitPoints", this.hps);			// Hitpoints
	players.iniSetValue(this.name, "Damage", this.damage);		// Damage taken so far
	players.iniSetValue(this.name, "GoldOnHand", this.gold);		// Gold on hand
	players.iniSetValue(this.name, "GoldInBank", this.bank);		// Gold in the bank
	players.iniSetValue(this.name, "Wins", this.wins);		// Wins so far
	players.iniSetValue(this.name, "Losses", this.losses);		// Losses so far
	players.iniSetValue(this.name, "Battles", this.battles);		// Battles left today
	players.iniSetValue(this.name, "Fights", this.fights);		// Fights left today
    // Players variance... applied to various rolls, Generally, how good the
    // player is feeling. Is set at start and not modified
	players.iniSetValue(this.name, "Vary", this.vary);
	players.close();
	Unlock(players.name);
	return(true);
}

function Player_levelupdate()
{
	if(this.experience > required[this.level+1]) {
		this.level++;
		console.attributes="HY0";
		console.writeln("Welcome to level "+this.level+"!");
		x=random(6)+1;
		switch(random(6)) {
			case 0:
				this.strength++;
				break;
			case 1:
				this.intelligence++;
				break;
			case 2:
				this.luck++;
				break;
			case 3:
				this.dexterity++;
				break;
			case 4:
				this.constitution++;
				break;
			case 5:
				this.charisma++;
				break;
		}
		this.hps = parseInt(this.hps + random(5) + 1 + (this.constitution / 4));
	}
}

function Player_chstats()
{
	function incre(this_obj)
	{
		console.write("Increase which stat? ");
		switch(dgn_getkeys("123456Q")) {
			case '1':
				console.writeln("Strength");
				this_obj.strength++;
				break;
			case '2':
				console.writeln("Intelligence");
				this_obj.intelligence++;
				break;
			case '3':
				console.writeln("Dexterity");
				this_obj.dexterity++;
				break;
			case '4':
				console.writeln("Luck");
				this_obj.luck++;
				break;
			case '5':
				console.writeln("Constitution");
				this_obj.constitution++;
				break;
			case '6':
				console.writeln("Charisma");
				this_obj.charisma++;
				break;
			case 'Q':
				console.writeln("Quit");
				return(false);
		}
		return(true);
	}

	function decre(this_obj)
	{
		console.write("Decrease which stat? ");
		switch(dgn_getkeys("123456Q")) {
			case '1':
				console.writeln("Strength");
				this_obj.strength--;
				break;
			case '2':
				console.writeln("Intelligence");
				this_obj.intelligence--;
				break;
			case '3':
				console.writeln("Dexterity");
				this_obj.dexterity--;
				break;
			case '4':
				console.writeln("Luck");
				this_obj.luck--;
				break;
			case '5':
				console.writeln("Constitution");
				this_obj.constitution--;
				break;
			case '6':
				console.writeln("Charisma");
				this_obj.charisma--;
				break;
			case 'Q':
				console.writeln("Quit");
				return(false);
		}
		return(true);
	}

	function ministat(this_obj)
	{
		var valid=true;
		var orig=new Object();

		function check(value, name) {
			if(value < 6) {
				valid=false;
				console.crlf();
				console.writeln(name+" cannot go below 6");
			}
		}

		function restore(this_obj) {
			this_obj.strength=orig.strength;
			this_obj.intelligence=orig.intelligence;
			this_obj.dexterity=orig.dexterity;
			this_obj.luck=orig.luck;
			this_obj.constitution=orig.constitution;
			this_obj.charisma=orig.charisma;
		}

		orig.strength=this_obj.strength;
		orig.intelligence=this_obj.intelligence;
		orig.dexterity=this_obj.dexterity;
		orig.luck=this_obj.luck;
		orig.constitution=this_obj.constitution;
		orig.charisma=this_obj.charisma;

		console.crlf();
		console.writeln("Status Change:");
		console.writeln("^^^^^^^^^^^^^^");
		console.writeln("1> Str: "+(this_obj.strength));
		console.writeln("2> Int: "+(this_obj.intelligence));
		console.writeln("3> Dex: "+(this_obj.dexterity));
		console.writeln("4> Luk: "+(this_obj.luck));
		console.writeln("5> Con: "+(this_obj.constitution));
		console.writeln("6> Chr: "+(this_obj.charisma));
		console.crlf();
		check(this_obj.strength,"Strength");
		check(this_obj.intelligence,"Intelligence");
		check(this_obj.dexterity,"Dexterity");
		check(this_obj.luck,"Luck");
		check(this_obj.constitution,"Constitution");
		check(this_obj.charisma,"Charisma");
		if(valid) {
			console.write("Is this correct? ");
			if(dgn_getkeys("YN")=='Y') {
				console.writeln("Yes");
				return(true);
			}
			else {
				console.writeln("No");
				restore(this_obj);
				return(false);
			}
		}
		restore(this_obj);
		return(false);
	}

	console.clear();
	while(1) {
		console.writeln("Status Change:");
		console.writeln("@@@@@@@@@@@@@@");
		console.writeln("You may increase any stat by one,");
		console.writeln("yet you must decrease another by two.");
		console.crlf();
		console.writeln("1> Str: "+this.strength);
		console.writeln("2> Int: "+this.intelligence);
		console.writeln("3> Dex: "+this.dexterity);
		console.writeln("4> Luk: "+this.luck);
		console.writeln("5> Con: "+this.constitution);
		console.writeln("6> Chr: "+this.charisma);
		console.crlf();
		if(incre(this)) {
			if(decre(this)) {
				if(ministat(this))
					break;
			}
		}
		else
			break;
	}
}

function Player_depobank()
{
	var tmp=this.gold;
	this.gold -= tmp;
	this.bank += tmp;
	console.crlf();
	console.attributes="HC0";
	console.writeln(this.bank+" Steel pieces are in the bank.");
}

function Player_withdrawbank()
{
	var tmp=this.bank;
	this.gold += tmp;
	this.bank -= tmp;
	console.crlf();
	console.attributes="HC0";
	console.writeln("You are now carrying "+this.gold+" Steel pieces.");
}

function Player_heal()
{
	var opt;

	console.clear();
	console.attributes="M0";
	console.writeln("Clerics want "+(8*this.level)+" Steel pieces per wound.");
	console.attributes="HY0";
	console.writeln("You have "+this.damage+" points of damage to heal.");
	console.attributes="HC0";
	console.write("How many points do you want healed? ");
	// TODO: We need max as default an some way to cancel...
	opt=console.getnum(this.damage);
	if((opt*this.level*8) > this.gold) {
		console.attributes="HR0";
		console.writeln("Sorry, you do not have enough Steel.");
		opt=0;
	}
	else if (opt > this.damage)
		opt = this.damage;
	this.damage -= opt;
	this.gold -= 8*opt*this.level;
	console.writeln(opt+" hit points healed.");
}

function Player_changemessage()
{
	function getmessage(prompt, msg)
	{
		while(1) {
			console.crlf();
			console.attributes="HC0";
			console.writeln(prompt);
			console.write("> ");
			msg=console.getstr(msg, 77, K_EDIT|K_AUTODEL|K_LINE);
			console.write("Is this correct? ");
			if(dgn_getkeys("YN")=='Y') {
				console.writeln("Yes");
				return(msg);
			}
			console.writeln("No");
		}
	}

	this.bcry=getmessage("Enter your new Battle Cry.", this.bcry);
	this.gaspd=getmessage("What will you say when you're mortally wounded?", this.gaspd);
	this.winmsg=getmessage("What will you say when you win?", this.winmsg);
}

function Player_create(isnew)
{
	this.vary=supplant();
	this.strength=12;
	this.status=Status.ONLINE;
	this.intelligence=12;
	this.luck=12;
	this.damage=0;
	this.dexterity=12;
	this.constitution=12;
	this.charisma=12;
	this.gold=random(100)+175;
	this.weapon=new Weapon("Hands", 1, 1, 0);
	this.armour=new Armour("Tunic", 1, 0);
	this.experience=0;
	this.bank=random(199)+1;
	this.level=1;
	this.hps=random(4)+1+this.constitution;
	this.wins=0;
	this.loses=0;
	this.sex='M';
	if(isnew) {
		this.bcry="";
		this.gaspd="";
		this.winmsg="";
		console.crlf();
		this.pseudo=user.alias;
		this.vic();
		this.battles = Config.BATTLES_PER_DAY;
		this.fights = Config.FIGHTS_PER_DAY;
		this.flights = Config.FLIGHTS_PER_DAY;
	}
	console.write("What sex would you like your character to be? (M/F) ");
	this.sex=dgn_getkeys("MF");
	console.crlf();
}

function Player_gamble()
{
	var realgold=0;
	var okea=0;

	if(this.fighs==0)
		console.writeln("The Shooting Gallery is closed until tomorrow!");
	else {
		console.clear();
		console.writeln("  Welcome to the Shooting Gallery");
		console.writeln(" Maximum wager is 25,000 Steel pieces");
		console.crlf();
		console.attributes="HY0";
		console.writeln("How many Steel pieces do you wish to wager? ");
		console.attributes="W0";
		realgold=console.getnum(25000);
		if(realgold > this.gold) {
			console.crlf();
			console.writeln("You do not have enough Steel!");
		}
		if(realgold != 0 && this.gold >= realgold && realgold <= 25000 && realgold >= 1) {
			okea=random(99)+1;
			if(okea <= 3) {
				realgold *= 10;
				this.gold += realgold;
				console.writeln("You shot all the targets and win "+realgold+" Steel pieces!");
			}
			else if (okea <= 15) {
				realgold *= 4;
				this.gold += realgold;
				console.writeln("You shot 50% of the targets and win "+realgold+" Steel pieces!");
			}
			else if (okea <= 30) {
				realgold *= 2;
				this.gold += realgold;
				console.writeln("You shot 25% of the targets and win "+realgold+" Steel pieces!");
			}
			else {
				this.gold -= realgold;
				console.writeln("Sorry, You Hath Lost!");
			}
		}
	}
}

function Player_training()
{
	const upcost=1000000;

	function dotrain(this_obj, attr)
	{
		console.writeln(attr);
		console.write("Are you sure? ");
		if(dgn_getkeys("YN")=='Y') {
			console.writeln("Yes");
			this_obj.gold -= upcost;
			return(1);
		}
		console.writeln("No");
		return(0);
	}

	if(this.fights < 1)
		console.writeln("The Training Grounds are closed until tomorrow!");
	else {
		console.clear();
		console.writeln("Training Grounds");
		console.writeln("%%%%%%%%%%%%%%%%");
		console.writeln("Each characteristic you wish to upgrade");
		console.writeln("will cost 1,000,000 Steel pieces per point.");
		console.crlf();
		console.write("Do you wish to upgrade a stat? ");
		if(dgn_getkeys("YN")=='Y') {
			console.writeln("Yes");
			if(this.gold < upcost)
				console.writeln("Sorry, but you do not have enough Steel!");
			else {
				console.crlf();
				console.writeln("1> Strength       2> Intelligence");
				console.writeln("3> Dexterity      4> Luck");
				console.writeln("5> Constitution   5> Charisma");
				console.crlf();
				console.attributes="HY0";
				console.write("Which stat do you wish to increase? ");
				console.attributes="W0";
				switch(console.getnum(6)) {
					case 1:
						this.strength+=dotrain(this, "Strength");
						break;
					case 2:
						this.intelligence+=dotrain(this, "Intelligence");
						break;
					case 3:
						this.dexterity+=dotrain(this, "Dexterity");
						break;
					case 4:
						this.luck+=dotrain(this, "Luck");
						break;
					case 5:
						this.constitution+=dotrain(this, "Constitution");
						break;
					case 6:
						this.charisma+=dotrain(this, "Charisma");
						break;
				}
			}
		}
		else
			console.writeln("No");
	}
}

function Player_leave()
{
	this.status &= ~(Status.ONLINE);
	this.save();
}

function Player_tohit(opp)
{
	return((this.weapon.attack*this.strength*this.dexterity*(random(5)+1)*this.vary)
		/ (opp.armour.power*opp.dexterity*opp.luck*opp.vary));
}

function Player_damage(opp)
{
	return(parseInt((this.weapon.power*this.strength*(random(5)+1)*(random(5)+1)*this.vary)
		/ (opp.armour.power*opp.luck*opp.vary)));
}

function Player_battle(opp)
{
	var option;

	function endofbattle(this_obj, winner,loser)
	{
		var lstr,wstr;
		var logfile=new File(getpath()+"today.log");

		if(this_obj==winner) {
			won=true;
			lstr=loser.his;
			wstr="You";
		}
		else {
			won=false;
			wstr=loser.his;
			lstr="your";
		}
		if(opp.status==Status.MONSTER)
			opp.gold=opp.gold * supplant();
		console.crlf();
		if(won && opp.gold > 0)
			console.writeln("You take "+opp.his+" "+opp.gold+" Steel pieces.");
		else {
			if(opp.status != Status.MONSTER && this_obj.gold > 0)
				console.writeln(opp.He+" takes your "+this_obj.gold+" Steel pieces");
		}
		winner.gold += loser.gold;
		loser.gold = 0;
		if(opp.status != Status.MONSTER) {
			console.crlf();
			console.attributes="G0";
			if(won) {
				console.writeln("The Last Words "+loser.he+" utters are...");
				console.crlf();
				console.writeln(opp.gaspd);
				console.crlf();
			}
			else {
				console.writeln("The Last Words You Hear Are...");
				console.crlf();
				console.writeln(opp.winmsg);
				console.crlf();
			}
			winner.wins++;
			loser.losses++;
			loser.killer=winner.pseudo;
			loser.status|=Status.DEAD;
			if(loser.weapon.attack+loser.weapon.power
					> winner.weapon.attack+winner.weapon.power) {
				var tmpweap=loser.weapon;

				loser.weapon=winner.weapon;
				winner.weapon=tmpweap;
				console.attributes="G0";
				console.writeln(wstr+" Hath Taken "+lstr+" Weapon.");
			}
			if(loser.armour.power > winner.armour.power) {
				var tmparm=loser.armour;

				loser.armour=winner=armour;
				winner.armour=tmparm;
				console.attributes="HY0";
				console.writeln(wstr+" Hath Taken "+lstr+" Armour.");
			}
			if(Lock(logfile.name, bbs.node_num, true, 1)) {
				if(logfile.open("a",true)) {
					logfile.writeln(system.datestr()+system.timestr()+": "+this_obj.pseudo+" attacked "+opp.pseudo+" and "+winner.pseudo+" was victorious!");
					logfile.close();
				}
				Unlock(logfile.name);
			}
			else {
				console.crlf();
				console.writeln("Cannot lock "+logfile.name+" for write!");
				console.crlf();
				console.pause();
			}
		}
		opp.experience *= supplant();
		winner.experience += opp.experience;
		if(won || opp.status != Status.MONSTER)
			console.writeln(wstr+" obtain "+opp.experience+" exp points.");
		if(!(opp.status & Status.MONSTER))
			opp.save();
	}

	function findo(this_obj)
	{
		var rnd;
	
		console.crlf();
		console.attributes="HY0";
		console.writeln("The Vile Enemy Dropped Something!");
		console.write("Do you want to get it? ");
		if(dgn_getkeys("YN")=='Y') {
			console.writeln("Yes");
			rnd=random(100)+1;
			if(rnd<10) {
				var i;
				for(i=1; i<weapon.length; i++) {
					if(weapon[i].attack+weapon[i].power > this_obj.weapon.attack+this_obj.weapon.power) {
						console.writeln("You have found a "+weapon[i].name+".");
						this_obj.weapon=new Weapon(weapon[i].name, weapon[i].attack, weapon[i].power, weapon[i].cost);
						break;
					}
				}
				if(i>=weapon.length) {
					console.writeln("It's gone!!!");
				}
				// TODO: Next weapon!
			}
			else if(rnd < 40) {
				this_obj.gold += 40;
				console.writeln("You have come across 40 Steel pieces");
			}
			else {
				console.writeln("It's gone!!!!");
			}
		}
		else {
			console.writeln("No");
			console.writeln("I wonder what it was?!?");
		}
	}

	function amode(this_obj)
	{
		var roll;

		roll=this_obj.tohit(opp);
		if(roll<1.5) {
			switch(random(4)) {
				case 0:
					console.writeln("Swish you missed!");
					break;
				case 1:
					console.writeln("HA HA! "+opp.he+" dodges your swing!");
					break;
				case 2:
					console.writeln("CLANG, The attack is blocked");
					break;
				case 3:
					console.writeln("You Fight vigorously and miss!");
					break;
			}
		}
		else {
			roll=this_obj.damageroll(opp);
			if(roll > 5*this_obj.power)
				roll=5*this_obj.power;
			if(roll < 1)
				roll=1;
			opp.damage += roll;
			switch(random(3)) {
				case 0:
					console.writeln(format("You sliced %s for %1.0f.",opp.him,roll));
					break;
				case 1:
					console.writeln(format("You made contact to %s body for %1.0f.",opp.his,roll));
					break;
				case 2:
					console.writeln(format("You hacked %s for %1.0f.",opp.him,roll));
					break;
			}
			if(opp.hps <= opp.damage) {
				console.crlf();
				switch(random(4)) {
					case 0:
						console.writeln("A Painless Death!");
						break;
					case 1:
						console.writeln(opp.He+" Had died!");
						break;
					case 2:
						console.writeln("A Smooth killing!");
						break;
					case 3:
						console.writeln(opp.He+" has gone to the Abyss!");
						break;
				}
				if(random(100)<30)
					findo(this_obj);
				endofbattle(this_obj,this_obj,opp);
			}
		}
	}

	function bmode(this_obj)
	{
		var roll;
		
		if(opp.hps > opp.damage && this_obj.damage < this_obj.hps) {
			roll=opp.tohit(this_obj);
			if(roll < 1.5) {
				console.attributes="G0";
				console.writeln(opp.His+" attack tears your armour.");
			}
			else {
				roll=opp.damageroll(this_obj);
				if(roll > 5*opp.power)
					roll=5*opp.power;
				if(roll<1)
					roll=1;
				switch(random(3)) {
					case 0:
						console.writeln(format("%s hammered you for %1.0f",opp.He,roll));
						break;
					case 1:
						console.writeln(format("%s swung and hit for %1.0f",opp.He,roll));
						break;
					case 2:
						console.writeln(format("You are surprised when %s hits you for %1.0f",opp.he,roll));
						break;
				}
				this_obj.damage += roll;
				if(this_obj.damage >= this_obj.hps) {
					console.crlf();
					switch(random(4)) {
						case 0:
							console.writeln("Return This Knight To Huma's Breast....");
							break;
						case 1:
							console.writeln("You Hath Been Slain!!");
							break;
						case 2:
							console.writeln("Return Soon Brave Warrior!");
							break;
						case 3:
							console.writeln("May Palidine Be With You!");
							break;
					}
					endofbattle(this_obj,opp,this_obj);
				}
			}
		}
	}

	function attackmodes(this_obj)
	{
		if(opp.dexterity > this_obj.dexterity) {
			if(this_obj.damage < this_obj.hps && opp.damage < opp.hps)
				bmode(this_obj);
			if(this_obj.damage < this_obj.hps && opp.damage < opp.hps)
				amode(this_obj);
		}
		else {
			if(this_obj.damage < this_obj.hps && opp.damage < opp.hps)
				amode(this_obj);
			if(this_obj.damage < this_obj.hps && opp.damage < opp.hps)
				bmode(this_obj);
		}
	}

	while(this.damage < this.hps && opp.damage < opp.hps) {
		console.crlf();
		console.attributes="HY0";
		console.writeln("You are attacked by a "+opp.pseudo);
		for(option='?'; option=='?'; ) {
			console.attributes="HB0";
			console.write("Combat ("+(this.hps-this.damage)+"): (B,F,S): ");
			option=dgn_getkeys("BFS?");
			if(option=="?") {
				console.writeln("Help");
				console.crlf();
				console.crlf();
				console.writeln("(B)attle your opponent.");
				console.writeln("(F)lee from your opponent.");
				console.writeln("(S)tatus check");
			}
		}
		switch(option) {
			case 'B':
				console.writeln("Battle");
				attackmodes(this);
				break;
			case 'S':
				console.writeln("Stats");
				this.statshow();
				break;
			case 'F':
				console.writeln("Flee");
				if(random(4)+1 + this.dexterity > opp.dexterity) {
					console.crlf();
					console.attributes="G0";
					console.writeln("You Ride away on a Silver Dragon.");
					return;
				}
				else {
					console.attributes="G0";
					console.writeln("You cannot escape!.");
					bmode(this);
				}
				break;
		}
	}
}

function Player_changename()
{
	var oldpseudo=this.pseudo;

	console.crlf();
	console.attributes="HC0";
	console.writeln("Your family crest has been stolen, they");
	console.writeln("inscribe a new one with the name...");
	console.write("> ");
	this.pseudo=console.getstr(this.pseudo, 30, K_EDIT|K_AUTODEL|K_LINE);
	console.crlf();
	console.write("Are you sure? ");
	if(dgn_getkeys("YN")=='Y') {
		// TODO: No duplicate names!
		if(this.pseudo=='')
			this.pseudo=oldpseudo;
		console.writeln("Yes");
	}
	else {
		console.writeln("No");
		this.pseudo=oldpseudo;
	}
}

function getplayerlist(filename)
{
	function sortlist(a, b)
	{
		var asum,bsum;

		if(a.level > b.level)
			return(-1);
		if(b.level > a.level)
			return(1);
		asum=a.wins-a.losses;
		bsum=b.wins-b.losses;
		if(asum > bsum)
			return(-1);
		if(bsum > asum)
			return(1);
		return(0);
	}

	if(filename==undefined)
		filename=getpath()+"players.ini";
	var players=new File(filename);
	var list=new Array();

	if(!Lock(players.name, bbs.node_num, false, 1)) {
		console.crlf();
		console.writeln("Cannot lock "+players.name+" for read!");
		console.crlf();
		console.pause();
		return(list);
	}
	if(!players.open("r", true)) {
		Unlock(players.name);
		console.crlf();
		console.writeln("Cannot open "+players.name+" for read!");
		console.crlf();
		console.pause();
		return(list);
	}
	var all=players.iniGetSections();
	players.close();
	Unlock(players.name);

	while(all.length) {
		var newplayer=new Player(all.shift());
		newplayer.load(filename);
		list.push(newplayer);
	}

	list.sort(sortlist);

	return(list);
}
