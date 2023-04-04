'use strict';
/* 	
	The Warrior's Graveyard
	Javascript version for JSLord (LoRD v5)
	(c) 2023 Lloyd Hannesson - dasme@dasme.org
*/
var graName = 'The Warrior\'s Graveyard';
var graNameFancy = '`8ן `%T`7he `%W`7arrior\'s `%G`7raveyard `8ן';
var graVersion = 'JS v1.1';
var graDebug = false;
var menu_redisplay = true;
var menu_file;
var menu_index = {};
var graFile;
var graRecord;

var Graveplots = [
	{
		name:'Edward Drake',
		desc1:'       A good man, who lived a short',
		desc2:'       life. Died by the hands of the',
		desc3:'       of the evil Barbarrion Syril.',
		desc4:'           May He Rest In Peace.',
		desc5:'',
		good:true,
		sex:'M',
		gold:1024,
		gems:5
	},
	{
		name:'Sir Thomas of Marks',
		desc1:'       This Valliant knight, who faught hard',
		desc2:'       in every battle. Died by arrow from an',
		desc3:'       archer, while he slept. A tragic loss.',
		desc4:'           May He Rest In Peace.',
		desc5:'',
		good:true,
		sex:'M',
		gold:1200,
		gems:4
	},	
 	{
		name:'Duke Osgood',
		desc1:'       A good man, who lived a short life.',
		desc2:'       Whilst walking through his castle',
		desc3:'       castle one day, a Battle axe suddenly',
		desc4:'       fell and split him in two!',
		desc5:'           May They Rest In Two Places.',
		good:true,
		sex:'M',
		gold:1000,
		gems:2
	},
 	{
		name:'Chelsea Cruz',
		desc1:'       She was a sinner.  She sold her body',
		desc2:'       for a few measly pieces of gold. She',
		desc3:'       died from a VD she caught from her goat.',
		desc4:'           May She Rest In Peace.',
		desc5:'',
		good:false,
		sex:'F',
		gold:2000,
		gems:7
	},
 	{
		name:'Lady Claria',
		desc1:'       A tragic loss to her family,',
		desc2:'       and all who knew her. Janet',
		desc3:'       Claria died giving birth to',
		desc4:'       her son Marcus.',
		desc5:'          May She Rest In Peace',
		good:true,
		sex:'F',
		gold:3000,
		gems:7
	},
	{
		name:'Princess Scarlet',
		desc1:'       She died a quick death. Sentenced',
		desc2:'       to beheading after her husband',
		desc3:'       found her in bed with 3 other men!',
		desc4:'            May She Rest In Pieces',
		desc5:'',
		good:false,
		sex:'F',
		gold:4000,
		gems:2
	},
 	{
		name:'Baron Vandersquat',
		desc1:'       Many have praised the death of',
		desc2:'       this cold hearted landlord. It\'s',
		desc3:'       not hard to believe that he won\'t',
		desc4:'       be missed.',
		desc5:'            May He Rest In Peace',
		good:false,
		sex:'M',
		gold:5000,
		gems:11
	},
 	{
		name:'Red, the BlackSmith',
		desc1:'       Thought of to be the most dedicated',
		desc2:'       man in town, Red worked long and hard',
		desc3:'       for 50 years as the town Smith. Red',
		desc4:'       died of natural causes.',
		desc5:'           May He Rest In Peace',
		good:true,
		sex:'F',
		gold:1024,
		gems:4
	},
 	{
		name:'Joe The Town Leper',
		desc1:'       Died after he was beaten by the',
		desc2:'       town drunk. He kind of crumbled.',
		desc3:'             May He Rest In Chunks.',
		desc4:'',
		desc5:'',
		good:true,
		sex:'M',
		gold:1500,
		gems:8
	},
 	{
		name:'Cliff The Town Drunk',
		desc1:'       Died from leprosy. Caught it from Joe',
		desc2:'       the Town Leper. Sadly, he also crumbled.',
		desc3:'             May He Rest In Chunks',
		desc4:'',
		desc5:'',
		good:false,
		sex:'M',
		gold:480,
		gems:2
	},
 	{
		name:'Glyn, The Royal Hog Farmer',
		desc1:'       He lived a happy life, killing pigs here,',
		desc2:'       killing pigs there, here, there, E-I-E-I-O!',
		desc3:'       Sadly he died when his pigs stampeded.',
		desc4:'                  May He Rest Flat',
		desc5:'            He always did like pancakes.',
		good:true,
		sex:'M',
		gold:2024,
		gems:5
	},
 	{
		name:'Mike Fergiono a.k.a. Bandit. [`@Donated!`%]',
		desc1:'      He lived a rich life, but by his name',
		desc2:'      it makes you wonder how it was achieved!',
		desc3:'      He stole from the rich and gave to the',
		desc4:'      poor [`@Sysops!?`%].',
		desc5:'            May He Rest In Peace',
		good:true,
		sex:'M',
		gold:2040,
		gems:8
	},
 	{
		name:'Bennie Hutto',
		desc1:'      A king among men. It\'s a shame to see',
		desc2:'      him go. But the tales of the things he',
		desc3:'      did [`@Beta Test!?`%] will live on for ever.',
		desc4:'      You can find and talk to his ghostly spirt',
		desc5:'               The Wall BBS.',
		good:true,
		sex:'M',
		gold:1745,
		gems:6
	}	
]	
	
var Graveyard_Defs = [
	{
		prop:'lrdrecord',
		name:'Lord Player Record #',
		type:'SignedInteger',
		def:-1
	},
	{
		prop:'day',
		name:'Lord Day last played.',
		type:'Integer',
		def:123456
	},
	{
		prop:'potioned',
		name:'Drank a Potion',
		type:'Boolean',
		def:false
	},
	{
		prop:'flirted',
		name:'Flirted Today (Mike or Tanya)',
		type:'Boolean',
		def:false
	},	
	{
		prop:'graverobbed',
		name:'Grave Robbed Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'lemonaide',
		name:'Drank Lemon-Aide Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'ghost',
		name:'Visited one of the 2 special ghosts?',
		type:'Boolean',
		def:false
	},
	{
		prop:'seenjim',
		name:'Seen Jim Bob Jones',
		type:'Boolean',
		def:false
	},	
	{
		prop:'kitten',
		name:'Found the Kitten Today',
		type:'Boolean',
		def:false
	},
	{
		prop:'gamed',
		name:'Played a Game of Chance Today',
		type:'Boolean',
		def:false
	}
];

/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*	Utility Functions                                                                   */ 
/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

function exit_game() {
	var i;
	sclrscr();
  	lln('`2Thanks for playing`% '+graName+' `0'+graVersion);
  	lw('`2Now returning to Other Places');
	for (i = 0; i < 5; i++) {
		mswait(300);
		lw('`4.');
	}
	//Catch any unsaved record changes and close out all open files.
	player.put();
	graFile.close();
	menu_file.close();	
}

function build_menu_index() {
	// Ripped from lord.js -- better than rolling my own. 
	// Would be great for the internal functions to support passing a 3rd party file
	// for use in IGMs.

	var l;

	menu_file = new File(js.exec_dir+'gravyard.gra');
	if (!menu_file.open('r')) {
		sln('Unable to open '+menu_file.name+'!');
		exit(1);
	}

	while (true) {
		l = menu_file.readln();
		if (l === null) {
			break;
		}
		if (l.substr(0,2) === '@#') {
			menu_index[l.substr(2)] = menu_file.position;
		}
	}
}

function display_menu(fname, more) {
	// Ripped from lord.js -- better than rolling my own. 
	// Would be great for the internal functions to support passing a 3rd party file
	// for use in IGMs.
	
	var ln;
	var mc = morechk;

	sclrscr();
	
	if (more === undefined) {
		more = false;
	}

	if (menu_index[fname] === undefined) {
		return;
	}
	menu_file.position = menu_index[fname];

	if (!more) {
		morechk = false;
	}
	while(true) {
		ln = menu_file.readln(65535);
		if (ln === null) {
			break;
		}
		if (ln.search(/^@#/) === 0) {
			break;
		}
		lln(ln);
	}
	morechk = mc;
	curlinenum = 1;
}

function command_prompt(currentPlace,menu_keys) {
	var ch;

	sln('');
	lln('  `#[`5'+currentPlace+'`#]  `2(? for menu)');
	lw('`2  Your command, `0'+player.name+'`2 : ');
	
	do {
		ch = getkey().toUpperCase();
	}while (menu_keys.indexOf(ch) === -1);
	
	lw('`2'+ch);
	return(ch);
}

function press_a_key(no_clear) {
	var ch;
	//lw('  `2<`0MORE`2>');
	lw(' `@ם `0Press A Key `@ם');	
	flush_keys();	
	ch = getkey();
	if(no_clear) {
		dk.console.print('\r');
		dk.console.cleareol();
	} else {
		sclrscr();
	}
}

function are_you_sure() {
	var ch;
	
	sln('');
	lw('  `2Really QUIT? [`0Y`2/`0N`2]  ');
	ch = getkey().toUpperCase();
	if(ch === 'Y') {
		return(true);
	}
	sln('');
	return(false);
}

function flush_keys() {
	while (dk.console.waitkey(0)) {
		dk.console.getkey();
	}
}

// Adding in some functions to match the old door driver I used..
function CharmCheck(Charm) {
	player.cha = player.cha + parseInt(Charm);
	if (player.cha > 32000) {
		player.cha = 32000;
	}
	if (player.cha < 0) {
		player.cha = 0;
	}	
}

function GemCheck(Gems) {
	player.gem = player.gem + parseInt(Gems);
	if (player.gem > 32000) {
		player.gem = 32000;
	}
	if (player.gem < 0) {
		player.gem = 0;
	}	
}

function GoldCheck(Gold) {
	player.gold = player.gold + parseInt(Gold,10);
	if (player.gold > 2000000000) {
		player.gold = 2000000000;
	}
	if (player.gold < 0) {
		player.gold = 0;
	}
}

function DefCheck(Def) {
	player.def = player.def + parseInt(Def);
	if (player.def > 32000) {
		player.def = 32000;
	}
	if (player.def < 0) {
		player.def = 0;
	}
}

function StrCheck(Str) {
	player.str = player.str + parseInt(Str);
	if (player.str > 32000) {
		player.str = 32000;
	}
	if (player.str < 0) {
		player.str = 0;
	}	
}

function ForestCheck(Forest) {
	player.forest_fights = player.forest_fights + parseInt(Forest);	
	if (player.forest_fights > 32000) {
		player.forest_fights = 32000;	
	}
	if (player.forest_fights < 0) {
		player.forest_fights = 0;
	}
}

function ExpCheck(Exp) {
	player.exp = player.exp + parseInt(Exp,10);	
	if (player.exp > 2000000000) {
		player.exp = 2000000000;
	}
	if (player.exp < 1) {
		player.exp = 1;
	}
}

function HitMaxCheck(HitMax) {
	player.hp_max = player.hp_max + parseInt(HitMax);
	if (player.hp_max > 32000) {
		player.hp_max = 32000;
	}
}	

/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*	Other Functions                                                                     */
/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

function doRandomStuff() {
	var num;

	num=random(100);
	if (num==1 && graRecord.seenjim == false) {
		// Write this out to the lord player log
		log_line('`0  Jim Bob Jones `2visited `%'+player.name+' `2today in `5The Warrior\'s Graveyard!');
		graJimBob();
		graRecord.seenjim = true;
		graRecord.put();
		menu_redisplay=true;
	}
}

function graIntro() {
	//	Last version (gra1_8.zip) was released in 2002... it's been a while...
	lln('`%'+graName+' - Ver '+graVersion);
	sln('');
	lln('`2An IGM for LORD by `0SETH ABLE ROBINSON');
	lln('`2Thanks to Stephen Hurd (Deuce) and other contributors');
	lln('`2for porting LORD to JS!');
	sln('');
	lln('`2JS Version is based on The Warrior\'s Graveyard v1.8 (2002)');
	sln('');
	lln('`2Written By `%Lloyd Hannesson');
	lln('`2Email me for support/bug reports: `%dasme@dasme.org');
	sln('');
	lln('`4Copyright (c) 1995-2023 - Lloyd Hannesson');
	sln('');
	press_a_key();
}

function graMaint() {
	graRecord.day = state.days;
	graRecord.potioned = false;
	graRecord.flirted = false;
	graRecord.graverobbed = false;
	graRecord.lemonaide = false;
	graRecord.ghost = false;
	graRecord.seenjim = false;
	graRecord.kitten = false;
	graRecord.gamed = false;
	graRecord.put();	
}

function graInitialize() {
	var i;
	var recordFound=false;

	graFile = new RecordFile(js.exec_dir+'gravyard.dat', Graveyard_Defs);
	js.on_exit('graFile.locks.forEach(function(x) {graFile.unLock(x); graFile.file.close()});');

	if (graFile.length < 1) {
		graRecord = graFile.new();
		graRecord.lrdrecord = player.Record;
		graRecord.day = state.days;
		graRecord.put();		
	} else {
		
		/*
		We will have to iterate through all of the records. If we can't match the
		LoRD record ID, we'll have to create a new record. 
		*/
		for (i = 0; i < graFile.length; i++) {
			graRecord=graFile.get(i);
			if(graRecord.lrdrecord == player.Record) { 
				recordFound=true;
				break; 
			}
		}

		// If we didn't find our record, we'll have to add one here.
		if(!recordFound){
			graRecord = graFile.new();
			graRecord.lrdrecord = player.Record;
			graRecord.day = state.days;
			graRecord.put();		
		}
		
		// If we do have a record and it's a new day, reset all of the booleans. 
		if(graRecord.day != state.days){
			graMaint();
		}
	}

}

function graPlayGame() {
	var ch;
	var menu_keys;
	var bet;
	var gold;
	var rand;
	var tempStr;
	
	if(player.gold <= 0) {
		sln('');
		lln('`2  Wait, you don\'t have any money on you! Come back when you are less poor.');
		press_a_key();
	} else {
		sclrscr();
		sln('');
		lln('  `5.ת`#שת`2[`0Game of Chance`2]`#תש`5ת.');
		sln('');
		lln('  `0"Let\'s play a game!" `2you say to the Old Hag. She thinks for a second');
		lln('  `2and then agrees to play a game. I have four marbles in this bag, 2 are');
		lln('  `0GREEN`2, and 2 are `9BLUE`2. What you hafta do is pull out 2 marbles one at');
		lln('  `2a time. The order and the colors you pull them determine if you win or lose!');
		sln('');
		lln('  `2   Here are the different combinations and their prize values: ');
		lln('                  `9Blue  `2and `9Blue  `4: LOSE ALL YOUR BET.');
		lln('                  `9Blue  `2and `0Green `4: LOSE HALF YOUR BET.');
		lln('                  `0Green `2and `9Blue  `%: WIN YOUR BET BACK.');
		lln('                  `0Green `2and `0Green `%: DOUBLE YOUR BET.');
		sln('');

		

		lw('  `2Do you want to play? [`0Y`2/`0N`2]  ');

		menu_keys = ['Y','N'];
		do {
			ch = getkey().toUpperCase();
		}while (menu_keys.indexOf(ch) === -1);

		lw('`2'+ch);
		sln('');
		sln('');
		
		switch (ch) {
			case 'N':
				sln('');
				lln('`2  "Huh. Well thanks for wastin\' my time!" shouts the Hag as she');
				lln('  returns to her book...');
				sln('');
				press_a_key();
				break;
			case 'Y':

				graRecord.gamed = true;
				graRecord.put();

				gold = (player.gold > 1000000)? 1000000 : player.gold;
				
				lw('`2  You have a total of '+pretty_int(player.gold)+' gold to wager');
				if(player.gold > 1000000){
					lln(' but you can only risk');
					lln('   a max of 1,000,000 coins.');
				}
				sln('');
				lln('  `2Please enter your wager. (A number between 1 and '+pretty_int(gold)+')');
				lw('  `2: ');

	//                x, y, length, foreground, background, default value, options, 
	//function getstr(x, y, len, c, c1, str, mode)
				bet = getstr(0, 0, 53, 1, 15, '', {integer:true, min:1, max:gold});
				if(bet == '') { bet =1; }
				sln('');
				sln('');
				lln('`2  Alright, well your wager of '+pretty_int(bet)+' gold is on the line!');
				
				rand = random(4);
				sln('');

				switch (rand) {
					case 0:
						// lose your full bet
						lln('  `2You plunge your hand in and pull out a `9BLUE `2marble.');
						mswait(800);
						lln('  `2Your second pick is a `9BLUE `2marble.');
						mswait(800);
						sln('');
						lln('  `0"Awwww darn! You lost all of your bet! I\'m *so* sorry." `2laughs the Hag.');
						lln('  `4YOU LOST '+pretty_int(bet)+' GOLD!');
						sln('');
						lln('`2  You can hear the Hag laughing to herself as she counts the coins.');
						sln('');
						GoldCheck(-bet);
						player.put();					
						break;
					case 1:
						// lose half your bet
						bet=Math.round(bet/2);
						lln('  `2You plunge your hand in and pull out a `9BLUE `2marble.');
						mswait(800);
						lln('  `2Your second pick is a `0GREEN `2marble.');
						mswait(800);
						sln('');
						lln('  `2You lost half of your bet. `0"Better luck next time!" `2laughs the Hag.');
						lln('  `4YOU LOST '+pretty_int(bet)+' GOLD!');
						sln('');
						GoldCheck(-bet);
						player.put();
						break;				
					case 2:
						//win your bet back
						lln('  `2You plunge your hand in and pull out a `0GREEN `2marble.');
						mswait(800);
						lln('  `2Your second pick is a `9BLUE `2marble.');
						mswait(800);
						sln('');
						lln('  `0"Dangnabit! You win all of your bet back!" `2whines the Hag.');
						sln('');
						tempStr = center('  `%YOU WON '+pretty_int(bet)+' GOLD!');
						lln('`%'+tempStr);
						sln('');
						GoldCheck(bet);
						player.put();
						break;				
					case 3:
						//double gold back
						bet = bet*2;
						lln('  `2You plunge your hand in and pull out a `0GREEN `2marble.');
						mswait(800);
						lln('  `2Your second pick is a `0GREEN `2marble.');
						mswait(800);
						sln('');
						lln('  `0"DAMN YOU!" `2screams the Hag in anger, `0"You took my lunch money from me!"');
						sln('');
						tempStr = center('YOU WON DOUBLE YOUR BET BACK! YOU WON '+pretty_int(bet)+' GOLD!');
						lln('`%'+tempStr);
						sln('');			
						lln('`2  You place your winnings into your pockets and thank the Hag for funding your');
						lln('`2  night at the tavern!');
						sln('');
						GoldCheck(bet);
						player.put();
				}
				
			press_a_key();
		}
	}	

}

function graFlirtMike() {
	log_line('`%  '+player.name+' `2spent the night with `!Mike The Ghost!');

	player.laid += 1;

	display_menu('FLIRTMIKE');
	press_a_key();
}

function graFlirtTanya() {
	log_line('`%  '+player.name+' `2spent the night with `#Tanya The Ghost!');

	player.laid += 1;

	display_menu('FLIRTTANYA');
	press_a_key();
}

function graBuyAide() {
	var ch;
	var menu_keys;
	var rand;
	var tempStr;
	var price;
	var gold;

	// Level less than 4 $1000 else $10000
	price = (player.level <= 4) ? 1000 : 10000;

	sclrscr();
	sln('');
	lln('  `5.ת`#שת`2[`0Buying Some Lemon Aide`2]`#תש`5ת.');
	sln('');
	lln('    `2You look at the Lemon-Aide that the Juice-Seller is pushing on you and');
	lln('  `2ask, `0"What the hell is in that stuff? It almost looks green!?"');
	lln('  `2The Juice-Seller says, `0"Well it\'s `$Lemon-Aide`0! Duh. Why else would');
	lln('  `0I be at a Lemon-Aide stand!? That greenish tinge is from the special');
	lln('  `0ingredient I add... It has some weird effects, but the ghosts seem to');
	lln('  `0like it."');
	sln('');
	lw('  `2  Do you want to try the `$Lemon-Aide`2? It\'s only '+price+' Gold! [`0Y`2/`0N`2]  ');
	
	menu_keys = ['Y','N'];
	do {
		ch = getkey().toUpperCase();
	}while (menu_keys.indexOf(ch) === -1);
	
	lw('`2'+ch);
	sln('');
	sln('');
	
	switch (ch) {
		case 'N':
			lln('`0    "Well fine then. That just means more for my transparent friends!"');
			lln('`2  barks the Juice-Seller.');
			sln('');
			break;
		case 'Y':
			gold = player.gold;
			
			if(gold >= price) {
			
				GoldCheck(-price); // pay for the juice...

				lln('  `2You take the sweet smelling `$Lemon-Aide`2, and swallow it in 1 gulp.');
				sln('');
				lln('  `2You start to feel weird and notice that.......');
				sln('');
				press_a_key(1);

				rand=random(4);
				switch (rand) {
					case 0:
						lln('  `2You feel like you\'ve been fully healed!');
						sln('');
						lln('  `%YOUR HIT POINTS ARE MAXED OUT!');
						sln('');
						player.hp = player.hp_max;
						player.put();
						break;
					case 1:
						lln('  `2You feel vigorous once again! Maybe you could try flirting?');
						sln('');
						lln('  `%WHAT COULD THIS MEAN!?');
						sln('');
						lln('  `2Oh yeah, I already told you...');
						sln('');
						player.seen_violet = false;
						player.seen_bard = false;
						player.flirted = false;
						player.put();
						graRecord.flirted = false;
						graRecord.put();
						break;
					case 2:
						lln('  `2You feel like you are slightly glowing! A Fairy flys over and enters');
						lln('  `2your pocket.');
						sln('');
						lln('  `%YOU NOW HAVE A FAIRY!');
						sln('');
						player.has_fairy = true;
						player.put();
						break;
					case 3:
						lln('  `2You feel lucky! You look down by your feet and notice something sticking out ');
						lln('  `2from under a rock. Upon closer look you see that it\'s a small bag!');
						lln('  `2You empty it\'s contents into your hand and find...');
						sln('');
						lln('  `%YOU FOUND `65 `%GEMS!');
						sln('');
						GemCheck(5);
						player.put();
				}
				graRecord.lemonaide = true;
				graRecord.put();
			
			} else {
				sln('');
				lln('  `0"Hey! You don\'t have '+price+' gold coins! Thanks for wastin\' my time buddy!"');			
				sln('');
			}
	}

	press_a_key();			

}

function graGetPotion(only_good) {
	var rand;
	var num
	var temp;

	lln('  `2You swallow the thick concoction and......');
	sln('');
	num = (only_good == true)? 6 : 12;
	rand = random(num);
	
	switch (rand) {
		case 0: 
			lln('  `2You feel somewhat better looking! `%CHARM RAISED BY 2!!!!');
			CharmCheck(2);
			break;
		case 1:
			lln('  `2You feel somewhat more agile! `%DEFENCE UP BY 2!!!');
			DefCheck(2);
			break;
		case 2:
			lln('  `2You feel somewhat stronger! `%STRENGTH RAISED BY 2!!!!');
			StrCheck(2);
			break;
		case 3:
			lln('  `2You feel less tired! `%FOREST FIGHTS UP BY 5!!!');
			ForestCheck(5);
			break;
		case 4:
			temp=player.level*1024;
			lln('  `2You feel somewhat wiser! `%EXPERIENCE UP BY '+temp.toString()+'!!!!');
			ExpCheck(temp);
			break;
		case 5:
			lln('  `2You feel a lot healthier! `%MAX HIT POINTS UP BY 1!!!!');
			HitMaxCheck(1);
			break;
		case 6:
			lln('  `2You feel the same as before. I guess the stupid potion did nothing.');
			break;
		case 7:
			lln('  `2You feel as though nothing has happened.');
			break;
		case 8:
			lln('  `2You feel as though you died! `4YOUR HIT POINTS ARE WAY DOWN!');
			player.hp=1;
			break;
		case 9:
			lln('  `2You feel somewhat weaker! `4STRENGTH DOWN BY 2!!!!');
			StrCheck(-2);
			break;
		case 10:
			lln('  `2You feel somewhat slower! `4DEFENCE DOWN BY 2!!');
			DefCheck(-2);
			break;
		case 11:
			lln('  `2You feel UGLY! `4CHARM DOWN BY 1!!!!');
			CharmCheck(-1);
	}
	sln('');
}

function graJimBob() {
	var menu_keys;
	var ch;
	var tempPick;
	
	sclrscr();
	sln('');
	lln('  `%ULTRA GHASTLY EVENT!!!!!!');
	lln('`5 -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- ');
	sln('');
	lln('  `2Seemingly out of nowhere a Ghost materializes in front of you.');
	lln('  `0"I am Jim Bob Jones! The Greatest Warrior that ever lived. I see that');
	lln('  `0you have a lot of potential, and I have decided to help you on your quest."');
	lln('  `2You stare in awe at this muscular Ghost, still adorned in his armor and');
	lln('  `2weapon. You wonder what he could do to help you?');
	sln('');
	lln('  `0"I am prepared to help you on your quest. What would help you the most?"');
	sln('');
	lln('       `2(`0S`2)trength');
	lln('       `2(`0D`2)efence');
	lln('       `2(`0E`2)xperience');
	lln('       `2(`0G`2)ems');
	lln('       `2(`0C`2)harm');
	lln('       `2(`0R`2)iches');
	sln('');
	lw(' `2Choose Carefully : ');
	
	menu_keys = ['S','D','E','G','C','R'];
	do {
		ch = getkey().toUpperCase();
	}while (menu_keys.indexOf(ch) === -1);
	
	lw('`2'+ch);
	sln('');
	
	switch (ch) {
		case 'S':
			menu_redisplay=true;
			lln(' `2You ask Jim Bob Jones for some Strength.');
			lln(' `0"Yes, I can see why you asked, my Grandma could have killed you!!');
			sln('');
			tempPick=player.level*5;
			lw(' `5JIM INCREASED YOUR STRENGTH BY ');
			lw(tempPick.toString());
			lln(' !!!');
			StrCheck(tempPick);
			break;
		case 'D':
			menu_redisplay=true;
			lln(' `2You ask Jim Bob Jones for some Defence.');
			lln(' `0"Yah you do look a little soft.');
			sln('');
			tempPick=player.level*5;
			lw(' `5JIM INCREASED YOUR DEFENCE BY ');
			lw(tempPick.toString());
			lln(' !!!');
			DefCheck(tempPick);
			break;
		case 'E':
			menu_redisplay=true;
			lln(' `2You ask Jim Bob Jones for some Experience.');
			lln(' `0"Ok I see that you are a little Unexperienced.');
			sln('');
			tempPick=player.level*1024;
			lw(' `5JIM INCREASED YOUR EXPERIENCE BY ');
			lw(tempPick.toString());
			lln(' !!!');
			ExpCheck(tempPick);
			break;
		case 'G':
			menu_redisplay=true;
			lln(' `2You ask Jim Bob Jones for some Gems.');
			lln(' `0"Gems eh? Well ok I guess I can spare you a few.');
			sln('');
			tempPick=4;
			lw(' `5JIM GAVE YOU ');
			lw(tempPick.toString());
			lln(' GEMS!!!');
			GemCheck(tempPick);
			break;
		case 'C':
			menu_redisplay=true;
			lln(' `2You ask Jim Bob Jones for some Charm.');
			lln(' `0"Yes you are kind of UGLY. I\'ll try.');
			sln('');
			tempPick=player.level;
			lw(' `5JIM INCREASED YOUR CHARM BY ');
			lw(tempPick.toString());
			lln(' !!!');
			CharmCheck(tempPick);
			break;
		case 'R':
			menu_redisplay=true;
			lln(' `2You ask Jim Bob Jones for some Gold.');
			lln(' `0"Well, I guess you are a little poor!!');
			sln('');
			tempPick=player.level;
			tempPick=tempPick*5000;
			lw(' `5JIM GAVE YOU ');
			lw(tempPick.toString());
			lln(' GOLD COINS!!!');
			GoldCheck(tempPick);
	}
	
	sln('');
	press_a_key();
	
}

function graKitten() {
	
	var TempInt;

	log_line('`%  '+player.name+' `2has found `0The Kitten `2in `5The Warrior\'s Graveyard!');

	sln('');
	lln('  `2You hear a scratching sound coming from an old box to your left');
	lln('  `2and decide to check it out. To your surprise you see a small kitten');
	lln('  `2licking it\'s paws. You bend down for a closer look and notice that');
	lln('  `2this isn\'t any normal kitten...');
	sln('');
	lln('  `% **** `0YOU FOUND THE KITTEN!!!!! `%****');
	mswait(600);
//	press_a_key(1);
	sln('');
	lln('  `2The kitten notices your presence and starts purring. The fact that');
	lln('  `2it is glowing and translucent (as most ghosts are) doesn\'t seem to');
	lln('  `2bother you as much as it should. It slowly walks over, and you');
	lln('  `2pick it up.');
	mswait(600);
//	press_a_key(1);
	sln('');
	lln('  `2It lightly kisses your nose and you notice that...');
	mswait(600);
//	press_a_key(1);
	sln('');

	switch (random(4)) {
		case 0:
			TempInt = player.level;
			lw('  `0You gain '+TempInt.toString()+' Strength!');
			sln('');
			StrCheck(TempInt);
			sln('');
			press_a_key();
			break;
		case 1:
			TempInt = player.level;
			lw('  `0You gain '+TempInt.toString()+' Defence!');
			sln('');
			DefCheck(TempInt);
			sln('');			
			press_a_key();
			break;
		case 2:
			lw('  `0You gain 1 Charm!');
			sln('');
			CharmCheck(1);
			sln('');			
			press_a_key();
			break;
		case 3:
			lw('  `0You gain 15 Forest Fights for today!');
			sln('');
			ForestCheck(15);
			sln('');			
			press_a_key();
	}

}

function graGraveRob() {
	var menu_keys;
	var menu_choice;
	var ch;
	var gold;
	var gems;
	var pot;
	var rand;
	var chance;
	var limit;

	rand = random(13);
	sclrscr();

    sln('');
    lln('`2  Looking upon the Tombstone you read...');
    sln('');
    lw('`%       Here lies ');

	lln(''+Graveplots[rand].name);
	sln('');
	lln('  '+Graveplots[rand].desc1);
	lln('  '+Graveplots[rand].desc2);
	lln('  '+Graveplots[rand].desc3);
	if(Graveplots[rand].desc4 != '') {
		lln('  '+Graveplots[rand].desc4);
	}
	if(Graveplots[rand].desc5 != '') {
		lln('  '+Graveplots[rand].desc5);
	}
	sln('');
	lln('`2  You think to yourself, `0"Surely there must be some riches to be found here!"');
	sln('');
	
	pot = random(2); // Is there also a potion in the grave?
	gold = Graveplots[rand].gold * player.level;
	gems = Graveplots[rand].gems;

	lw('`2  You dig up the grave of this`0 ');
	lw((Graveplots[rand].good == true ? "Good" : "Evil"));
	lw(' ');
	lw((Graveplots[rand].sex == "M" ? "Man" : "Woman"));
	lln('`2, and find...');
	lw('  `0 ');
	lw(gold.toString());
	lw('`2 gold, and`0 ');
	lw(gems.toString());
	lln('`2 gems.');
	sln('');
	lw('  `2Do you want to rob the grave of this ');
	lw((Graveplots[rand].good == true ? "Good" : "Evil"));
	lw(' person? `0[`2Y`0/`2N`0]`2 : ');
	
	menu_keys = ['Y','N','G'];
	do {
		ch = getkey().toUpperCase();
	}while (menu_keys.indexOf(ch) === -1);
	
	lw('`2'+ch);
	sln('');
	
	switch (ch) {
		case 'N':
			sln('');
			lln('  `2You decide to let this corpse keep it\'s valuables...');
			if(Graveplots[rand].good == true) {
				sln('');
				lln('  `0For leaving this grave alone you gain `%1 CHARM!');
				sln('');
				CharmCheck(1);	
				press_a_key();
			}
			break;
		case 'Y':
			chance=random(100);
			limit = (Graveplots[rand].good == true) ? 10 : 5; // 5 or 10% chance of failure...
			if(chance <= limit) {
				sln('');
				lln('`2  Just as you are about to shove some of the valuables into your pockets,');
				lln('  you hear a twig crack just behind you.  You turn around just in time to');
				lln('  feel the Gravediggers shovel hitting your skull....Just before you drift');
				lln('  off to dreamland you hear the Gravedigger say sadly,`0 "What did I tell you');
				lln('`0  about grave robbing? And to think that I thought you were a good person.."');
				sln('');
				lln('`2  As the Gravedigger trudges away you notice that you still have a little ');
				lln('`2  life left in your old body. You chalk this up to experience and decide to');
				lln('`2  not make this mistake again.');
				// This IGM used to kill people here... that's no fun, so let's just leave them with 1 HP
//				lln('`2  You drift off into unconscienceness....maybe you shouldn\'t steal anymore!?');
				log_line('  `%'+player.name+' `2was caught Grave Robbing today in `5The Warrior\'s Graveyard!');
				sln('');
				player.hp=1;
				press_a_key();
			} else {
				sln('');
				lln('  `2Thinking nothing of it you bend down and stuff all this corpse\'s valuables');
				lln('  into your pockets.`0 "Geeze! This is too easy!"`2, you exclaim.');
				sln('');
				lln('`%  YOU GET...');
				lln('   '+gold.toString()+' `0GOLD PIECES AND`% '+gems.toString()+' GEMS!!!`2 What an easy score!');
				GemCheck(gems);
				GoldCheck(gold);
				if(pot == 1){
					sln('');
					lln('  `2You also find `%A POTION!!!!');
					lln('  `2You decide to drink it.......');
					graGetPotion();
				}
				sln('');
				sln('');
				press_a_key();				
			}
			break;
		case 'G':
			graGhostGeorge();
	}

}

/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*	Menus                                                                               */
/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

function graDebugMenu() {
// not included in official release.
}

function graGhostMike() {
	var menu_keys;
	var menu_choice;
	var menu_done = false;
	var ch;
	var mike_ask = false;

	sclrscr();
	log_line('`%  '+player.name+' `2has found `!Mike the Ghost `2in `5The Warrior\'s Graveyard!');
	display_menu('FOUNDMIKE');
    press_a_key(); 

	do {
		if (menu_redisplay == true) {
			display_menu('MIKEMENU');
			menu_redisplay = false;
		}

		menu_keys=['A','T','F','L','?','V'];
		menu_choice = command_prompt('Ghostly Event',menu_keys);
		sln('');
		
		switch (menu_choice) {
			case 'A':
				menu_redisplay=true;
				if(!mike_ask) {
					display_menu('MIKEASK');
					CharmCheck(2);
					lln('  `2You thank Mike with a full passionate kiss, he just smiles.');
					sln('');
					press_a_key(true);
					mike_ask=true;
				} else {
					sln('');
					lln('  `2Don\'t you think you\'ve already asked enough of Mike?');
					sln('');
					press_a_key();
				}
				break;
			case 'T':
				menu_redisplay=true;
				display_menu('TALKMIKE');
				press_a_key();
				break;
			case 'F':
				menu_redisplay=true;
				if(!graRecord.flirted) {
					graFlirtMike();
					graRecord.flirted = true;
					graRecord.put();
				} else {
					sln('');
					lln('  `2I think once is enough! Flirt with this ghost too much and he might');
					lln('  `2get sick of you!');
					sln('');
					press_a_key();
				}
				break;
			case 'V':
				menu_redisplay=true;
				show_stats();
				break;			
			case '~':
				menu_redisplay=true;
				graDebugMenu();
				break;
			case 'L':
				menu_done = true;			
				menu_redisplay = true;
				break;
			case '?':
				menu_redisplay=true;
		}
	} while (!menu_done);
	
}

function graGhostTanya() {
	var menu_keys;
	var menu_choice;
	var menu_done = false;
	var ch;
	var tanya_ask = false;

	sclrscr();
	log_line('`%  '+player.name+' `2has found `#Tanya the Ghost `2in `5The Warrior\'s Graveyard!');
	display_menu('FOUNDTANYA');
    press_a_key(); 


	do {
		if (menu_redisplay == true) {
			display_menu('TANYAMENU');
			menu_redisplay = false;
		}

		menu_keys=['A','T','F','L','?','V'];
		menu_choice = command_prompt('Ghostly Event',menu_keys);
		sln('');
		
		switch (menu_choice) {
			case 'A':
				menu_redisplay=true;
				if(!tanya_ask) {
					display_menu('TANYAPOTION');
					graGetPotion(true);
					press_a_key(true);
				} else {
					sln('');
					lln(' `2She already gave you a potion!!! Whatmore could you want from her!!!!');
					lln(' `2Then again.........');
					press_a_key();
				}
				break;
			case 'T':
				menu_redisplay=true;
				display_menu('TANYATALK');
				press_a_key();
				break;
			case 'F':
				menu_redisplay=true;
				if(!graRecord.flirted) {
					graFlirtTanya();
					graRecord.flirted = true;
					graRecord.put();
				} else {
					sln('');
					lln('  `2I think once is enough! Flirt with this ghost too much and she might');
					lln('  `2get sick of you!');
					sln('');
					press_a_key();
				}
				break;
			case 'V':
				menu_redisplay=true;
				show_stats();
				break;			
			case '~':
				menu_redisplay=true;
				graDebugMenu();
				break;
			case 'L':
				menu_done = true;			
				menu_redisplay = true;
				break;
			case '?':
				menu_redisplay=true;
		}
	} while (!menu_done);

}

function graGhostGeorge() {
	var menu_keys;
	var menu_choice;
	var menu_done = false;
	var ch;
	var rand;
	var talked = false;
	var asked = false;

	log_line('`%  '+player.name+' `2has found `0George the Ghost `2in `5The Warrior\'s Graveyard!');

	display_menu('FOUNDGEORGE');
	press_a_key();

	do {

		if (menu_redisplay == true) {
			display_menu('GEORGEMENU');
			menu_redisplay = false;
		}

		menu_keys = ['T','A','V','L','?'];
		menu_choice = command_prompt('Ghostly Event',menu_keys);
		sln('');
		
		switch (menu_choice) {
			case 'A':
				menu_redisplay=true;
				sln('');
				lln('  `2You ask nicely if George will help you. He thinks for a while and then...');
				rand=random(3);
				if(!asked) {
					switch (rand) {
						case 0:
							sln('');
							lln('  `0"Well here, I\'ll give you the Strength to fight more monsters today!"');
							sln('');
							lln('  `%YOU GET 10 MORE FOREST FIGHTS!!!!!');
							sln('');
							ForestCheck(10);
							press_a_key();
							break;
						case 1:
							sln('');
							lln('  `0"Hmmm... Well, you could sure use some help in the field..."');
							sln('');
							lln('  `%YOU GAIN 1 DEFENSE AND 1 STRENGTH!!!!!');
							sln('');
							StrCheck(1);
							DefCheck(1);
							press_a_key();
							break;
						case 2:
							sln('');
							lln('  `0"Well here I have a potion I don\'t need, you can have it!"');
							sln('');
							graGetPotion(true);
							press_a_key();
					}
					asked=true;
				} else {
					sln('');
					lln('  `2Better not ask for too many things, George Might get mad at you!!');
					sln('');
					press_a_key();
				}
				break;
			case 'T':
				menu_redisplay=true;
				if(!talked) {
					rand=random(3);
					switch(rand) {
						case 0:
							sln('');
							lln('  `0"Well I\'m not the only ghost in this Graveyard y\'know. You should');
							lln('  `0be able to find the others like you did me."');
							sln('');
							press_a_key();
							break;
						case 1:
							sln('');
							lln('  `0"Y\'know there is something good to be said about sparing the');
							lln('  `0graves of Good Hearted people. It\'ll make you look better as a person"');
							sln('');
							press_a_key();
							break;
						case 2:
							sln('');
							lln('  `0"Jim Bob Jones is a tough ghost to get a hold of. You\'d think he would always');
							lln('  `0be in just one place, but he is a random kinda guy!"');
							sln('');
							press_a_key();
					}
					talked=true;
				} else {
					sln('');
					lln(' `2You think that you\'ve asked enough of this Ghost for today...');
					sln('');
					press_a_key();
				}
				break;
			case '~':
				menu_redisplay=true;
				graDebugMenu();
				break;
			case 'V':
				menu_redisplay=true;
				show_stats();
				break;					
			case 'L':
				menu_done = true;
				menu_redisplay=true;
				break;
			case '?':
				menu_redisplay=true;
		}

	} while (!menu_done);

}

function graShack() {
	var menu_keys;
	var menu_choice;
	var menu_done = false;
	var ch;
	
	doRandomStuff();
	
	do {

		if (menu_redisplay == true) {
			display_menu('SHACK');
			menu_redisplay = false;
		}

		menu_keys = (player.sex === 'M' ? ['A','P','V','L','?'] : ['1','A','P','V','L','?']);
		menu_choice = command_prompt('In The Shack',menu_keys);
		sln('');
		
		switch (menu_choice) {
			case 'A':
				menu_redisplay=true;
				if(!graRecord.potioned) {
					sln('');
					lln('  `2Looking around at all the little labeled bottles the Hag owns you decide');
					lln('  `2to ask her to make you a potion. Hell what harm could it do!? Hmmmmm.....');
					sln('');
					lln('  `0"So you want that I should make you a potion eh??? Well I guess so, but ');
					lln('  `0just one as I haven\'t much time."`2 she says eagerly. After many minutes,');
					lln('  `2and just as many broken bottles, the potion is done.');
					sln('');
					lln('  `2She holds it before you and smiles. You look at it and take a whiff. ');
					lln('  `2It smells like yesterday\'s garbage, but you decide to drink it anyways.');
					sln('');
					graGetPotion();
					graRecord.potioned = true;
					graRecord.put();					
				} else {
					sln('');
					lln('  `2You scream `0"Hey Hag! I want another potion!!"`2. The Hag just looks back');
					lln('  `2at you and shakes her head. She just goes back to reading.');
					sln('');
					lln('  `2I guess she\'s too busy to make you another one. Try again tomorrow.');
					sln('');
				}
				press_a_key();			
				break;
			case 'P':
				menu_redisplay=true;
				if(!graRecord.gamed) {
					graPlayGame();
				} else {
					lln('');
					lln('`2  You think that placing another wager today would be unwise, the');
					lln('`2  Hag is mumbling to herself a little more than usual...');
					sln('');
					press_a_key();
				}
				break;
			case '1':
				menu_redisplay=true;
				if(!graRecord.ghost) {
					graRecord.ghost = true;
					graRecord.put();
					graGhostMike();
				}
				break;
			case '~':
				menu_redisplay=true;
				graDebugMenu();
				break;
			case 'V':
				menu_redisplay=true;
				show_stats();
				break;					
			case 'L':
				menu_done = true;
				menu_redisplay=true;
				break;
			case '?':
				menu_redisplay=true;
		}

	} while (!menu_done);

}

function graLemon() {
	var menu_keys;
	var menu_choice;
	var menu_done = false;
	var ch;
	
	doRandomStuff();
	do {

		if (menu_redisplay == true) {
			display_menu('LEMON');
			menu_redisplay = false;
		}

		menu_keys = ['B','T','V','L','?'];
		menu_choice = command_prompt('The Lemon-Aide Stand',menu_keys);
		sln('');
		
		switch (menu_choice) {
			case 'B':
				menu_redisplay=true;
				if(!graRecord.lemonaide) {
					graBuyAide();
				} else {
					sln('');
					lln('`2  You think that drinking too much of this stuff would be a bad idea...');
					sln('');
					press_a_key();
				}
				break;
			case 'T':
				menu_redisplay=true;
				display_menu('AIDETALK');
				press_a_key();
				break;
			case '~':
				menu_redisplay=true;
				graDebugMenu();
				break;
			case 'V':
				menu_redisplay=true;
				show_stats();
				break;					
			case 'L':
				menu_done = true;
				menu_redisplay=true;
				break;
			case '?':
				menu_redisplay=true;
		}

	} while (!menu_done);

}

function graDigger() {
	var menu_keys;
	var menu_choice;
	var menu_done = false;
	var ch;
	
	doRandomStuff();
	do {

		if (menu_redisplay == true) {
			display_menu('DIGGER');
			menu_redisplay = false;
		}

		menu_keys = (player.sex === 'M' ? ['1','W','T','G','H','V','L','?'] : ['W','T','G','H','V','L','?']);
		menu_choice = command_prompt('The Grave Digger',menu_keys);
		sln('');
		
		switch (menu_choice) {
			case 'W':
				menu_redisplay=true;
				sln('');
				sln('');
				lln('  `2Who is this grave for you ask??? Well it\'s for `%'+player.name+'`2!');
				lln('  `2Hehehe just kidding. This is actualy a grave for the great warrior');
				lln('  `2Jim Bob Jones! People say that his generous soul is still roaming the');
				lln('  `2night, looking for a good deed or two to do. Consider yourself lucky');
				lln('  `2if you meet him.');
				sln('');
				sln('');				
				press_a_key();			
				break;
			case 'G':
				menu_redisplay=true;
				sln('');
				sln('');
				lln('  `2Ghosts yah say??? Well yah there are always a few ghosts to be found');
				if (player.sex === 'M') {
					lln('  `2in a Graveyard, you just hafta know where to `01`2ook! I hear that a few');
				} else {
					lln('  `2in a Graveyard, you just hafta know where to 1ook! I hear that a few');
				}	
				lln('  `2of the ghosts that have been seen here have actually been friendly!');
				sln('');
				sln('');
				press_a_key();
				break;
			case 'T':
				menu_redisplay=true;
				sln('');
				sln('');
				lln('  `2So you want me to tell you about treasure eh? Well alls I can tell you');
				lln('  `2is that most Great Warriors ask to be buried intact. That means all of');
				lln('  `2their Weapons, Armor, and riches! Don\'t be Grave robbing tho, it\'s far');
				lln('  `2too dangerous for a Warrior like you.');
				sln('');
				sln('');				
				press_a_key();
				break;
			case 'H':
				menu_redisplay=true;
				sln('');
				sln('');
				lln('  `2Hmmmmm, the hag. What can I say about her that her looks don\'t explain');
				lln('  `2for themselves! As of late the Hag has taken a real likin\' to the art');
				lln('  `2of Alchemy! Who knows what she can mix up. Legend has it that this Hag');
				lln('  `2wasn\'t always such a foul creature, people say that she once was even');
				lln('  `2more beautiful then the Barmaid `5VIOLET`2!!! The stories people make up.');
				sln('');
				sln('');
				press_a_key();
				break;
			case '1':
				menu_redisplay=true;
				if(!graRecord.ghost) {
					graRecord.ghost = true;
					graRecord.put();
					graGhostTanya();
				}
				break;
			case '~':
				menu_redisplay=true;
				graDebugMenu();
				break;
			case 'V':
				menu_redisplay=true;
				show_stats();
				break;					
			case 'L':
				menu_done = true;
				menu_redisplay=true;
				break;
			case '?':
				menu_redisplay=true;
		}

	} while (!menu_done);

}	

function mainMenu() {
	var menu_keys;
	var menu_choice;
	var menu_done = false;
	var ch;

	do {
		if (menu_redisplay == true) {
			display_menu('MAIN');
			menu_redisplay = false;
		}

		menu_keys=['`','E','T','G','S','L','?','V'];
		menu_choice = command_prompt('The Graveyard',menu_keys);
		sln('');
		
		switch (menu_choice) {
			case '`':
				menu_redisplay=true;
				if(!graRecord.kitten) {
					graRecord.kitten = true;
					graRecord.put();
					graKitten();
				}
				break;
			case 'E':
				menu_redisplay=true;
				graShack();
				break;
			case 'T':
				menu_redisplay=true;
				graDigger();
				break;
			case 'G':
				menu_redisplay=true;
				if(graRecord.graverobbed == true) {
					sln('');
					lln('  `2You think that robbing more than one grave today would be too DANGEROUS!');
					lln('  `2So at the last minute you stop yourself.');
					sln('');
					lln('  `2Tomorrow is a new day afterall...');
					sln('');
					press_a_key();
				} else {
					graRecord.graverobbed = true;
					graRecord.put();
					graGraveRob();
				}
				break;
			case 'S':
				menu_redisplay=true;
				graLemon();
				break;
			case 'V':
				menu_redisplay=true;
				show_stats();
				break;			
			case '~':
				menu_redisplay=true;
				graDebugMenu();
				break;
			case 'L':
				if(are_you_sure()) {
					menu_done = true;
				}
				menu_redisplay = true;
				break;
			case '?':
				menu_redisplay=true;
		}
	} while (!menu_done);
}

/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

function main() {
	build_menu_index(); //Index the gravayard.gra menu file.
	graInitialize();  	//Set and read the IGM data file and set any needed variables

	sclrscr();
	graIntro(); 
	
	mainMenu();
	exit_game();
}

if (argc == 1 && argv[0] == 'INSTALL') {
	var install = {
		desc:graNameFancy
	}
	exit(0);
} else {
	main();
	exit(0);
}
