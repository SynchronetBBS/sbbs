var startup_path='.';
try { throw barfitty.bar.barf() } catch(e) { startup_path=e.fileName }
startup_path=startup_path.replace(/[\/\\][^\/\\]*$/,'');
startup_path=backslash(startup_path);

load("sbbsdefs.js");
load("lockfile.js");
var Config = {
	FLIGHTS_PER_DAY : 3,
	BATTLES_PER_DAY : 3,
	FIGHTS_PER_DAY : 10,
	LASTRAN : "",
};

var required=new Array(
	 0
	,1500
	,2800
	,5500
	,9000
	,16000
	,26000
	,37000
	,66000
	,100000
	,140000
	,198000
	,240000
	,380000
	,480000
	,600000
	,800000
	,1000000
	,2000000
	,3000000
	,4000000
	,5000000
	,6000000
	,7000000
	,8000000
	,9000000
	,10000000
	,20000000
	,30000000
	,40000000
);

load(getpath()+"player.js");
load(getpath()+"armour.js");
load(getpath()+"weapon.js");
// main()
var player=new Player();

exit(main());

function getpath()
{
	return(startup_path);
}

function dgn_getkeys(keys)
{
	var key='';
	keys=keys.toUpperCase();
	while(1) {
		key=console.getkey(K_UPPER|K_NOCRLF);
		if(key=='')
			return(key);
		if(keys.indexOf(key)!=-1)
			return(key);
	}
}

function checkday()
{
	var newday=false;
	var bfile=new File(getpath()+"btoday.asc");

	if(Config.LASTRAN != system.datestr()) {
		Config.LASTRAN = system.datestr();
		var conf=new File(getpath()+"dgnlance.ini");
		if(conf.open("r+",true)) {
			conf.iniSetValue(null, "LastRan", Config.LASTRAN);
			newday=true;
			conf.close();
		}
	}

	if(newday) {
		var all=getplayerlist();
		var i;
		for(i=0; i<all.length; i++) {
			all[i].battles=Config.BATTLES_PER_DAY;
			all[i].flights=Config.FLIGHTS_PER_DAY;
			all[i].fights=Config.FIGHTS_PER_DAY;
			all[i].status &= ~Status.DEAD;
			all[i].damage=0;
			all[i].vary=supplant();
			all[i].save();
		}
		file_remove(getpath()+"yester.log");
		file_rename(getpath()+"today.log", getpath()+"yester.log");
		file_remove(getpath()+"byester.asc");
		file_rename(getpath()+"btoday.asc", getpath()+"byester.asc");
		if(bfile.open("w")) {
			bfile.writeln(system.datestr()+": A new day begins...");
			bfile.writeln();
			bfile.close();
		}
	}
}

function playerlist()
{
	var list=getplayerlist();
	var i;

	console.attributes="M0";
	console.writeln("Hero Rankings:");
	console.writeln("!!!!!!!!!!!!!!");
	for(i=0; i<list.length; i++) {
		console.attributes="M0";
		console.print(format("%2u. \x01H\x01C%.30s%.*s\x01N\x01GLev=%-2u  W=%-6u L=%-6u S=%s \x01I\x01R%s\r\n"
			, i+1
			, list[i].pseudo
			, 30-list[i].pseudo.length
			, '...............................'
			, list[i].level
			, list[i].wins
			, list[i].losses
			, (list[i].status & Status.DEAD)?"SLAIN":"ALIVE"
			, (list[i].status & Status.ONLINE)?"ONLINE":""
		));
		if(((i+1) % console.rows)==0)
			console.pause();
	}
}

function weaponlist()
{
	var i;
	var max=weapon.length;

	if(max<armour.length)
		max=armour.length;

    console.writeln("Num. Weapon                    Price       Armour                    Price");
	console.writeln("------------------------------------------------------------------------------");
	for(i=1; i<max; i++) {
		if((i < weapon.length && weapon[i].cost != 0) || (i < armour.length && armour[i].cost != 0)) {
			console.writeln(format("%2d>  %-25.25s %9s   %-25.25s %9s"
					, i
					, ((i>=weapon.length || weapon[i].cost==0)?"":weapon[i].name)
					, ((i>=weapon.length || weapon[i].cost==0)?"":weapon[i].cost)
					, ((i>=armour.length || armour[i].cost==0)?"":armour[i].name)
					, ((i>=armour.length || armour[i].cost==0)?"":armour[i].cost)
		));
		}
	}
}

function credits()
{
	console.clear();
	console.attributes="G0";
	console.writeln("Dragonlance 3.0 Credits");
	console.writeln("@@@@@@@@@@@@@@@@@@@@@@@");
	console.writeln("Original Dragonlance   : Raistlin Majere & TML");
	console.writeln("Special Thanks To      : The Authours of Dragonlance");
	console.writeln("Drangonlance's Home    : The Tower Of High Sorcery");
	console.writeln("Originally modified from the Brazil Source Code");
	console.writeln("C Port                 : Deuce");
	console.writeln("JavaScript Port        : Deuce");
	console.writeln("Home Page              : http://doors.bbsdev.net/");
	console.writeln("Support                : deuce@nix.synchro.net");
	console.crlf();
	console.pause();
}

function docs()
{
	console.clear();
	console.printfile(getpath()+"docs.asc");
	
}

function listplayers()
{
	console.clear();
	console.attributes="HY0";
	console.writeln("Heroes That Have Been Defeated");
	console.writeln("++++++++++++++++++++++++++++++");
	console.attributes="HC0";
	if(file_exists(getpath()+"yester.log"))
		console.printfile(getpath()+"yester.log");
	if(file_exists(getpath()+"today.log"))
		console.printfile(getpath()+"today.log");
}

function loadconfig()
{
	var conf=new File(getpath()+"dgnlance.ini");

	if(conf.open("r",true)) {
		Config.FLIGHTS_PER_DAY=conf.iniGetValue(null, "FlightsPerDay", Config.FLIGHTS_PER_DAY);
		Config.BATTLES_PER_DAY=conf.iniGetValue(null, "BattlesPerDay", Config.BATTLES_PER_DAY);
		Config.FIGHTS_PER_DAY=conf.iniGetValue(null, "FightsPerDay", Config.FIGHTS_PER_DAY);
		Config.LASTRAN=conf.iniGetValue(null, "LastRan", "");
		conf.close();
	}
}

function weaponshop()
{
	var buy=0;
	var buyprice=0;
	var type="";
	var max=weapon.length;

	if(max<armour.length)
		max=armour.length;

	console.clear();
	console.attributes="HY0";
	console.writeln("Weapon & Armour Shop");
	console.attributes="G0";
	console.writeln("$$$$$$$$$$$$$$$$$$$$");
	while(1) {
		console.crlf();
		console.write("(B)rowse, (S)ell, (P)urchase, or (Q)uit? ");
		switch(dgn_getkeys("BSQP")) {
			case 'Q':
				console.writeln("Quit");
				return;
			case 'B':
				console.writeln("Browse");
				weaponlist();
				break;
			case 'P':
				console.writeln("Purchase");
				console.crlf();
				console.crlf();
				console.write("Enter weapon/armour # you wish to buy: ");
				buy=console.getnum(max);
				if(buy==0)
					continue;
				console.crlf();
				if(buy >= weapon.length)
					type="A";
				else if(buy >= armour.length)
					type="W";
				else {
					console.write("(W)eapon or (A)rmour: ");
					type=dgn_getkeys("WA");
					if(type=='A')
						console.writeln("Armour");
					else if(type=='W')
						console.writeln("Weapon");
					else
						continue;
				}
				switch(type) {
					case 'W':
						if(weapon[buy].cost==0) {
							console.writeln("You want to buy a what?");
							continue;
						}
						console.write("Are you sure you want to buy it? ");
						if(dgn_getkeys("YN")=='Y') {
							console.writeln("Yes");
							if(weapon[buy].cost > player.gold) {
								console.writeln("You do not have enough Steel.");
								continue;
							}
							if(weapon[buy].cost==0) {
								console.writeln("You can not buy one of those...");
								continue;
							}
							player.gold -= weapon[buy].cost;
							player.weapon=new Weapon(weapon[buy].name, weapon[buy].attack, weapon[buy].power, weapon[buy].cost);
							console.crlf();
							console.attributes="M0";
							console.writeln("You've bought a "+player.weapon.name);
						}
						else
							console.writeln("No");
						break;
					case 'A':
						if(weapon[buy].cost==0) {
							console.writeln("You want to buy a what?");
							continue;
						}
						console.write("Are you sure you want to buy it? ");
						if(dgn_getkeys("YN")=='Y') {
							console.writeln("Yes");
							if(armour[buy].cost > player.gold) {
								console.writeln("You do not have enough Steel.");
								continue;
							}
							if(armour[buy].cost==0) {
								console.writeln("You can not buy one of those...");
								continue;
							}
							player.gold -= armour[buy].cost;
							player.armour=new Armour(armour[buy].name, armour[buy].power, armour[buy].cost);
							console.crlf();
							console.attributes="M0";
							console.writeln("You've bought a "+player.armour.name);
						}
						else
							console.writeln("No");
						break;
				}
				break;
			case 'S':
				console.writeln("Sell");
				console.crlf();
				console.write("(W)eapon, (A)rmour, (Q)uit: ");
				switch(dgn_getkeys("AWQ")) {
					case 'Q':
						console.writeln("Quit");
						break;
					case 'W':
						console.writeln("Weapon");
						buyprice=player.charisma*player.weapon.cost/20;
						console.crlf();
						console.write("I will purchase it for "+buyprice+", okay? ");
						if(dgn_getkeys("YN")=='Y') {
							console.writeln("Yes");
							console.attributes="G0";
							console.writeln("Is it Dwarven Made?");
							player.weapon=new Weapon(weapon[0].name, weapon[0].attack, weapon[0].power, weapon[0].cost);
							player.gold += buyprice;
						}
						else
							console.writeln("No");
						break;
					case 'A':
						console.writeln("Armour");
						buyprice=player.charisma*player.armour.cost/20;
						console.crlf();
						console.write("I will purchase it for "+buyprice+", okay? ");
						if(dgn_getkeys("YN")=='Y') {
							console.writeln("Yes");
							console.writeln("Fine Craftsmanship!");
							player.armour=new Armour(armour[0].name, armour[0].power, armour[0].cost);
							player.gold += buyprice;
						}
						else
							console.writeln("No");
						break;

				}
				break;
		}
	}
}

function spy()
{
	var users=getplayerlist();
	var spyon;

	console.clear();
	console.writeln("Spying on another user eh.. well you may spy, but to keep");
	console.writeln("you from copying this person's stats, they will not be");
	console.writeln("available to you.  Note that this is gonna cost you some");
	console.writeln("cash too.  Cost: 20 Steel pieces");
	console.crlf();
	console.write("Who do you wish to spy on? ");
	spyon=console.getstr("", 30, K_LINE);
	if(spyon=='')
		return;
	if(player.gold < 20) {
		console.attributes="HR0";
		console.writeln("You do not have enough Steel!");
	}
	else {
		/* Find this user */
		for(i=0; i<users.length; i++) {
			if(i.pseudo.match(new RegExp("spyon","i")!=-1)) {
				console.write("Do you mean \""+users[i].pseudo+"\"?");
				if(dgn_getkeys("YN")=="Y") {
					console.writeln("Yes");
					break;
				}
				console.writeln("No");
			}
		}
		if(i>=users.length)
			return;
	
		player.gold -= 20;
		console.crlf();
		console.attributes="HR0";
		console.writeln(users[i].pseudo);
		console.crlf();
		console.writeln("Level  : "+users[i].level);
		console.writeln("Exp    : "+users[i].experience);
		console.writeln("Flights: "+users[i].flights);
		console.writeln("HPs    : "+users[i].hps);
		console.crlf();
		console.attributes="M0";
		console.writeln("Weapon : "+users[i].weapon.name);
		console.writeln("Armour : "+users[i].armour.name);
		console.crlf();
		console.attributes="HY0";
		console.writeln("Steel  (in hand): "+users[i].gold);
		console.writeln("Steel  (in bank): "+users[i].bank);
		console.crlf();
		console.pause();
	}
}

function bulletin()
{
	var bfile=new File(getpath()+"btoday.asc");
	var bline="";

	console.clear();
	if(file_exists(getpath()+"byester.asc"))
		console.printfile(getpath()+"byester.asc");
	if(file_exists(getpath()+"btoday.asc"))
		console.printfile(getpath()+"btoday.asc");
	console.crlf();
	console.write("Do you wish to enter a News Bulletin? ");
	if(dgn_getkeys("YN")=='Y') {
		console.writeln("Yes");
		console.crlf();
		var bullet=new Array();;
		while(bullet.length<4) {
			console.attributes="HY0";
			console.write((bullet.length+1)+"> ");
			console.attributes="W0";
			bline=console.getstr("", 60, K_LINE);
			if(bline=="")
				break;
			bullet.push(bline);
		}
		console.crlf();
		console.write("Is the bulletin correct? ");
		if(dgn_getkeys("YN")=='Y') {
			console.writeln("Yes");
			console.writeln("Saving Bulletin...");
			if(bfile.open("a",true)) {
				bfile.writeln("At "+system.timestr()+", "+player.pseudo+" wrote:");
				while(bullet.length) {
					bfile.write("     ");
					bfile.writeln(bullet.shift());
				}
				bfile.writeln();
				bfile.close();
			}
		}
		else
			console.writeln("No");
	}
	else
		console.writeln("No");
}

function afight(lvl)
{
	var monsters;
	var monster;

	if(player.fights<1) {
		console.crlf();
		console.attributes="M0";
		console.writeln("It's Getting Dark Out!");
		console.writeln("Return to the Nearest Inn!");
	}
	else {
		console.clear();
		monsters=getplayerlist(getpath()+"monsters"+lvl+".ini");
		if(monsters.length < 1)
			return;
		monster=monsters[random(monsters.length)];
		monster.status=Status.MONSTER;
		monster.weapon.attack *= supplant();
		if(monster.weapon.attack<1)
			monster.weapon.attack=1;
		monster.weapon.power *= supplant();
		if(monster.weapon.power<1)
			monster.weapon.power=1;
		monster.armour.power *= supplant();
		if(monster.armour.power<1)
			monster.armour.power=1;
		monster.luck *= supplant();
		if(monster.luck<1)
			monster.luck=1;
		monster.strength *= supplant();
		if(monster.strength<1)
			monster.strength=1;
		monster.dexterity *= supplant();
		if(monster.dexterity<1)
			monster.dexterity=1;
		monster.hps *= supplant();
		if(monster.hps<1)
			monster.hps=1;
		monster.vary=supplant();
		player.fights--;
		player.battle(monster);
	}
}

function doggie()
{
	var players;
	var avail_players=new Array();
	var i;
	var opp=0;

	if(player.battles > 0) {
		console.clear();
		console.attributes="HY0";
		console.writeln("Battle Another Hero");
		console.writeln("*******************");
		players=getplayerlist();
		for(i=0; i<players.length; i++) {
			if((players[i].level > player.level-4)
					&& players[i].name!=player.name
					&& (players[i].status & Status.DEAD)==0
					/* TODO: Allow internode battles */
					&& (players[i].status & Status.ONLINE)==0) {
				avail_players.push(players[i]);
				console.print(format("\01N\01H\01Y%2u.  \01H\01C%.30s%.*s\01BLev=%-2u  W=%-2d  L=%-2dS=%s \01I\01R%s\r\n"
					, avail_players.length
					, players[i].pseudo
					, 30-players[i].pseudo.length
					, "..............................."
					, players[i].level
					, players[i].wins
					, players[i].losses
					, "ALIVE"
					, ""));
				if(avail_players.length % (console.rows-1))
					console.pause();
			}
		}
		console.crlf();
		console.attributes="HC0";
		console.write("Enter the # of your opponent: ");
		opp=console.getnum(avail_players.length);
		if(opp>0 && opp<=avail_players.length) {
			opp--;
			avail_players[opp].status &= Status.PLAYER;
			avail_players[opp].status &= ~Status.MONSTER;
			player.battles--;
			avail_players[opp].experience /= 10;
			avail_players[opp].damage = 0;
			console.crlf();
			console.writeln("Your opponent screams out:");
			console.writeln('"'+avail_players[opp].bcry+'" as battle is joined.');
			console.crlf();
			player.battle(avail_players[opp]);
		}
	}
}

function main()
{
	console.line_counter=0;
	loadconfig();
	checkday();
	credits();
	console.clear();
	console.print("\x01H\x01Y----------\x01N\x01I\x01R   -=-=DRAGONLANCE=-=-     \x01N\x01H\x01Y     /\\     \r\n");
	console.print("\\        /\x01N\x01W       Version 3.0          \x01H\x01Y    ||     \r\n");
	console.print(" \\      /                                 ||     \r\n");
	console.print("  \\    /  \x01B  Welcome To The World of   \x01Y    ||     \r\n");
	console.print("   |  |   \x01C  Krynn, Where the gods     \x01Y    ||     \r\n");
	console.print("  /    \\  \x01B  of good and evil battle.  \x01Y  \\ || /   \r\n");
	console.print(" /      \\ \x01C  Help the People Of Krynn. \x01Y   \\==/    \r\n");
	console.print("/        \\                                ##     \r\n");
	console.print("----------\x01N\x01I\x01R        ON WARD!!!          \x01N\x01H\x01Y    ##     \r\n");
	console.crlf();
	console.writeln("News Bulletin:");
	console.crlf();
	if(file_exists(getpath()+"byester.asc"))
		console.printfile(getpath()+"byester.asc");
	if(file_exists(getpath()+"btoday.asc"))
		console.printfile(getpath()+"btoday.asc");
	if(!player.load())
		player.create(true);
	js.on_exit("player.leave()");
	if(player.status & Status.DEAD) {
		if(player.laston==system.datestr()) {
			console.writeln("You have already died today...");
			console.writeln("Return tomorrow for your revenge!");
			return(0);
		}
		console.crlf();
		console.attributes="HC0";
		console.writeln("A defeat was lead over you by "+player.killer);
	}
	player.lastone=system.datestr();
	if(player.flights < 1) {
		player.fights=0;
		player.battles=0;
	}
	else
		player.flights--;
	console.crlf();
	console.pause();
	console.clear();
	console.attributes="HY0";
	player.status &= Status.ONLINE;
	player.statshow();
	player.save();
	while(player.damage < player.hps) {
		player.levelupdate();
		if((player.wins+1)*4 < player.losses) {
			console.crlf();
			console.writeln("As you were Travelling along a Wilderness Page an");
			console.writeln("Evil Wizard Confronted You. When you tried to fight");
			console.writeln("Him off, he cast a Spell of Instant Death Upon you.");
			console.writeln("Instantly you were slain by the Archmage, Leaving you");
			console.writeln("as nothing more than a pile of ashes. Re-Rolled!");
			console.crlf();
			console.pause();
			player.create(false);
			if(player.flights)
				player.flights--;
		}
		console.crlf();
		console.crlf();
		console.attributes="HY0";
		console.write("Command (?): ");
		switch(dgn_getkeys("QVP12345CHWLADGFRSTX+-?EZ*#")) {
			case 'Q':
				console.writeln("Quit");
				console.write("LEAVE KRYNN? Are you sure? ");
				if(dgn_getkeys("YN")=='Y') {
					console.writeln("Yes");
					return(0);
				}
				console.writeln("No");
				break;
			case '1':
				console.writeln("1");
				afight(1);
				break;
			case '2':
				console.writeln("2");
				afight(2);
				break;
			case '3':
				console.writeln("3");
				afight(3);
				break;
			case '4':
				console.writeln("4");
				afight(4);
				break;
			case '5':
				console.writeln("5");
				afight(5);
				break;
			case 'C':
				console.writeln("Change Stats");
				player.chstats();
				break;
			case 'H':
				console.writeln("Heal");
				player.heal();
				break;
			case 'W':
				console.writeln("Weapon Shop");
				weaponshop();
				break;
			case 'L':
				console.writeln("Level Update");
				player.levelupdate();
				break;
			case 'A':
				console.writeln("Battle Another User");
				doggie();
				break;
			case 'D':
				console.writeln("Docs");
				docs();
				break;
			case 'G':
				console.writeln("Gamble");
				player.gamble();
				break;
			case 'F':
				console.writeln("Battles Today");
				listplayers();
				break;
			case 'R':
				console.writeln("Rank Players");
				playerlist();
				break;
			case 'S':
				console.writeln("Status");
				player.statshow();
				break;
			case 'T':
				console.writeln("Training Grounds");
				player.training();
				break;
			case 'X':
				console.writeln("Re-Roll");
				console.writeln("Please not that this will completely purge");
				console.writeln("your current hero of all attributes!");
				console.crlf();
				console.write("Are you sure you want to REROLL your character? ");
				if(dgn_getkeys("YN")=='Y') {
					console.writeln("Yes");
					player.create(false);
					if(player.flights)
						player.flights--;
				}
				else
					console.writeln("No");
				break;
			case '+':
				console.writeln("Deposit");
				player.depobank();
				break;
			case '-':
				console.writeln("Withdraw");
				player.withdrawbank();
				break;
			case 'P':
				console.writeln("Plug");
				console.clear();
				console.printfile(getpath()+"plug.asc");
				break;
			case '?':
				console.writeln("Help");
				console.clear();
				console.printfile(getpath()+"menu.asc");
				break;
			case 'E':
				console.writeln("Edit Announcement");
				bulletin();
				break;
			case 'Z':
				console.writeln("Spy");
				spy();
				break;
			case 'V':
				console.writeln("Version");
				console.attributes="HB0";
				console.writeln("This Is Dragonlance version 3.0 (JS)");
				console.pause();
				break;
			case '*':
				console.writeln("Change Name");
				player.changename();
				break;
			case '#':
				console.writeln("Change Battle Cry");
				player.vic();
				break;
		}
	}
	return(0);
}
