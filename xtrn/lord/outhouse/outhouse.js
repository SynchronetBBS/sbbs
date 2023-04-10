'use strict';
/* 	
	The Outhouse
	Javascript version for JSLord (LoRD v5)
	(c) 2023 Lloyd Hannesson - dasme@dasme.org
*/
var outhouseName = 'The Outhouse';
var outhouseNameFancy = '`0T`2he `0O`2uthouse';
var outhouseVersion = 'JS v1.0';
var menu_redisplay = true;
var menu_done = false;
var outhouseFile;
var outhouseRecord;

var Outhouse_Defs = [
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
		prop:'business',
		name:'Done your business?',
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
	sln('');
  	lln(' `2Thanks for visiting`% '+outhouseName+' `0'+outhouseVersion);
  	lw(' `2Now returning to Other Places');
	for (i = 0; i < 5; i++) {
		mswait(300);
		lw('`4.');
	}

	player.put();
	outhouseRecord.put();
	outhouseFile.close();
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

function say_slow(str,ms) {
	// yoinked and modified from barak.js!
	var i;
	ms = (ms == "") ? 100 : ms;
	for (i = 0; i < str.length; i++) {
		sw(str[i]);
		mswait(ms);
	}
}

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

/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*	Other Functions                                                                     */
/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

function outhouseIntro() {
	sclrscr();
	sln('');
	lln('`%'+outhouseName+' - Ver '+outhouseVersion);
	sln('');
	lln('`2An IGM for LORD by `0SETH ABLE ROBINSON');
	lln('`2Thanks to Stephen Hurd (Deuce) and other contributors');
	lln('`2for porting LORD to JS!');
	sln('');
	lln('`2Written By `%Lloyd Hannesson');
	lln('`2Original Concept By `#Robert Fogt');
	sln('');
	lln('`2Email me for support/bug reports: `%dasme@dasme.org');
	sln('');
	lln('`4Copyright (c) 1995-2023 - Lloyd Hannesson');
	sln('');
	press_a_key();
}

function outhouseMaint() {
	outhouseRecord.day = state.days;
	outhouseRecord.business = false;
	outhouseRecord.put();	
}

function outhouseInitialize() {
	var i;
	var recordFound=false;

	outhouseFile = new RecordFile(js.exec_dir+'outhouse.dat', Outhouse_Defs);
	js.on_exit('outhouseFile.locks.forEach(function(x) {outhouseFile.unLock(x); outhouseFile.file.close()});');

	if (outhouseFile.length < 1) {
		outhouseRecord = outhouseFile.new();
		outhouseRecord.lrdrecord = player.Record;
		outhouseRecord.day = state.days;
		outhouseRecord.put();		
	} else {
		
		/*
		We will have to iterate through all of the records. If we can't match the
		LoRD record ID, we'll have to create a new record. 
		*/
		for (i = 0; i < outhouseFile.length; i++) {
			outhouseRecord=outhouseFile.get(i);
			if(outhouseRecord.lrdrecord == player.Record) { 
				recordFound=true;
				break; 
			}
		}

		// If we didn't find our record, we'll have to add one here.
		if(!recordFound){
			outhouseRecord = outhouseFile.new();
			outhouseRecord.lrdrecord = player.Record;
			outhouseRecord.day = state.days;
			outhouseRecord.put();		
		}
		
		// If we do have a record and it's a new day, reset all of the booleans. 
		if(outhouseRecord.day != state.days){
			outhouseMaint();
		}
	}

}

function found_shiny() {
	var rand;
	var temp;

	press_a_key(1);
	lln('`2    As you are getting up you notice a quick flash of something shiny');
	lln('  out of the corner of your eye. Looking closer you see an item');
	lln('  inside of the hole you just sat on.');
	sln('');
	lln('    Against all common sense and everything your mother told you, you');
	lw('  make a grab for the item');

	say_slow('.....',400);

	rand = random(2);

	if (rand == 0) {
		temp = player.level * 3500;
		GoldCheck(temp);
		player.put();
		lln(' and find a pouch with `%'+pretty_int(temp)+'`2 gold!');
	} else {
		temp = player.level * 3;
		GemCheck(temp);
		player.put();
		lln(' and find a small pouch with `%'+temp+'`2 gems!');
	}

	sln('');
	lln('  `0  "I wonder who left this here?" `2you think to yourself `0"wait, I hope');
	lln('  that this was placed here and not..." `2you stop yourself and decide');
	lln('  to just take the `%WIN`2 and not worry about the `4HOW`2.');
}

function in_the_outhouse() {
	var pooped = false;
	var rand;

	if(player.forest_fights < 1) {
		sclrscr();
		sln('');
		lln('`2  You were heading towards the outhouse when you realized that you\'re');
		lln('`2  too tired. You turn around and head back to town.');
		sln('');
		lln('`0  You really didn\'t have to go that bad anyways, maybe tomorrow.');
		sln('');
		press_a_key();
	} else {
		ForestCheck(-1);
		rand=random(3);

		sclrscr();
		sln('');
		lln('  `%In the Outhouse');
		lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		
		switch (rand) {
			case 0:
				CharmCheck(2);
				lln('`2    After waiting for what seemed like hours, you finally get to the');
				lln('`2  old Outhouse door. You enter, sit down, and can now get down to business.');
				lln('`2  This place seems nicer than some rooms at the Inn. There is even a small');
				lln('`2  wash basin and mirror on the wall. Neat!');
				sln('');
				lln('`2  After doing your business, you take the time to wash up and comb your hair.');
				sln('');
				lln('`0  You look much better now.');
				sln('');
				lln('`%     YOU GAIN 2 CHARM POINTS!');
				break;
			case 1:
				DefCheck(2);
				lln('`2    After waiting for what seemed like hours, you finally get to the');
				lln('  old Outhouse door. You enter, sit down, and can now get down to business.');
				lln('  This place seems nicer than some rooms at the Inn!');
				sln('');
				lln('  While doing your business you notice a small tear in your');
				lln('  `0'+player.arm+'`2, but thankfully it looks fixable!');
				sln('');
				lln('  You grab your repair kit from your backpack and manage to perfectly');
				lln('  repair the damage!');
				sln('');
				lln('`%     YOU GAIN 2 DEFENSE POINTS!');
				break;	
			case 2:
				StrCheck(2);
				lln('`2    You enter the Outhouse, sit down and quickly get down to business.');
				lln('  To pass the time you start to sing one of your favourite drinking songs,');
				lln('  "`5Ode to the Red Dragon `#(`5Please don\'t eat me`#)`2". You must have been quite'); 
				lln('  noisy since someone starts banging on the Outhouse wall.');
				sln('');
				lln('    The vibrations of your voice, combined with the banging dislodged');
				lln('  some of the nails holding this shack together. They fall PERFECTLY');
				lln('  onto your `0'+player.weapon+'`2!');
				sln('');
				lln('    Besides looking bad-ass, they look like they will actually increase the');
				lln('  damage! Who needs to work out when you can just attach more pointy things'); 
				lln('  to your weapon!?');
				sln('');
				lln('`%     YOU GAIN 2 ...err... "STRENGTH" POINTS!');
		}

		outhouseRecord.business = true;
		outhouseRecord.put();
		player.put();		
		pooped = true;		
		
		rand = random(3);
		
		if (rand == 0) {
			sln('');
			found_shiny();
			sln('');
			press_a_key(1);
		}
		
		sln('');
		lln('`2  Satisfied, you turn and run back to the realm.');
		sln('');

		press_a_key(1);
	}

	return pooped;
}

function behind_the_trees() {
	var pooped = false;
	var ch;
	var rand;
	var yes_no;

	sclrscr();
	sln('');
	lln('  `%Behind the Trees');
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	lln('`2    You wait until nobody is looking and run towards the trees. You really');
	lln('  have to go bad, and if you don\'t do something quick you\'ll explode!');
	sln('');
	lln('`0    You hear voices in the distance but you cant tell if they are');
	lln('  coming closer or not.');
	sln('');
	lln('    `2You grab some leaves off the nearest tree and think about your options and');
	lln('  wonder if you should take a chance and try to go here?');
	sln('');
	lw('  `2Do you want chance it? [`0Y`2/`0N`2]  ');
	
	yes_no = ['Y','N'];
	do {
		ch = getkey().toUpperCase();
	}while (yes_no.indexOf(ch) === -1);

	lw('`2'+ch);
	sln('');
	sln('');

	if(ch=='Y') {
		rand = random(10);
		switch (rand) {
			case 0:
			case 1:
			case 2:
				CharmCheck(-1);
				player.put();
				pooped = true;
				sclrscr();
				sln('');
				lln('  `%Oh... oh no. Noooo.');
				lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				lln('`2    As you drop your pants, a large group of people walk by. You hear');
				lln('  laughter and look up to see the group of people pointing and snickering.');
				sln('');
				lln('  In your rush to get away you manage to `0step in the mess you made`2.');
				sln('');
				lln('    After the crowd disperses, you manage to clean yourself up a bit in');
				lln('  the nearby river. It will be a while before you\'ll be able to wash');
				lln('  your embarrassment away.');
				sln('');
				lln('`4     YOU LOSE 1 CHARM!');
				break;
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:				
				CharmCheck(1);
				player.put();
				pooped = true;
				sclrscr();
				sln('');
				lln('  `%Finally... Relief!');
				lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
				lln('`2    You make your way to the trees and find a nice well hidden bush.');
				lln('  Thankfully it seems as the nearby crowd has dispersed and you are free');
				lln('  to do your business in peace.');
				sln('');
				lln('`2    You feel better now. Man, that was close one! You have a new pep');
				lln('  in your step and it shows!');
				sln('');
				lln('`%     YOU GAIN 1 CHARM!');
		}

		outhouseRecord.business = true;
		outhouseRecord.put();

	} 
	
	sln('');
	lln('`2  You turn, and run back to the realm.');
	sln('');
	press_a_key();
	menu_done = true;

	return pooped;
}

/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*	Menus                                                                               */
/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

function mainMenu() {
	var menu_keys;
	var menu_choice;
	var ch;
	var poop_check;

	do {
		if (menu_redisplay == true) {
			sclrscr();
			sln('');
			lln('  `5.ת`#שת`2[`0The Outhouse`2]`#תש`5ת.');
			sln('');
			lln('  `2  You realize that you really do need to make a pit stop so you head');
			lln('  `2towards the outhouses. When you get there you see a `0very `2long line.');
			lln('  `2You also notice some trees a fair distance away that look deserted. If');
			lln('  `2you do decide to wait in line you\'ll lose `01`2 forest fight today. A ');
			lln('  `2small sign is nearby in the clearing.');
			sln('');
			lln('  `2What would you like to do?');
			sln('');
			lln('        `5ת`2[`0w`2]`5ת `0W`2ait in line at the outhouse.');
			lln('        `5ת`2[`0g`2]`5ת `0G`2o behind the trees.');
			lln('        `5ת`2[`0r`2]`5ת `0R`2ead the small sign.');
			lln('        `5ת`2[`0l`2]`5ת `0L`2eave, you decide to just hold it for awhile.');
			sln('');
			menu_redisplay = false;
		}

		menu_keys=['W','G','R','L','?','V'];
		menu_choice = command_prompt('The Outhouse',menu_keys);
		sln('');
		
		switch (menu_choice) {
			case 'W':
				menu_redisplay=true;
				if(outhouseRecord.business != true) {
					poop_check = in_the_outhouse();
					if(poop_check) {
						// They pooped in the outhouse so account for that
						menu_done=true;
					}
				} else {
						sclrscr();
						sln('');
						lln('`2  As much as you would like to go again, you just don\'t have anything');
						lln('`2  left to give... maybe after a meal and some strong mead.');
						sln('');
						lln('`2  You turn around and head back to town.');
						sln('');
						press_a_key();
						menu_done=true;
				}				
				break;
			case 'G':
				menu_redisplay=true;
				if(outhouseRecord.business != true) {

					poop_check = behind_the_trees();
					if(poop_check) {
						// They pooped behind the trees so account for that
						menu_done=true;
					}

				} else {
						sclrscr();
						sln('');
						lln('`2  As much as you would like to go again, you just don\'t have anything');
						lln('`2  else to give... maybe after a meal and some strong mead.');
						sln('');
						lln('`2  You turn around and head back to town.');
						sln('');
						press_a_key();
						menu_done=true;
				}	
				break;
			case 'R':
				menu_redisplay=true;
				outhouseIntro();
				break;
			case 'L':
				if(are_you_sure()) {
					menu_done = true;
					sln('');
					sln('');
					lln('  `2You realize that you don\'t have to go as bad as you thought...');
					lln('  `2You turn, and run back to the realm.');
					sln('');
					press_a_key(1);
				}
				menu_redisplay = true;
				break;
			case 'V':
				menu_redisplay=true;
				show_stats();
				break;			
			case '?':
				menu_redisplay=true;
		}
	} while (!menu_done);
}

/* -=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

function main() {
	outhouseInitialize();

	foreground(2);
	background(0);
	sclrscr();

	if(outhouseRecord.business) {
		sln('');
		lln('  `%The Outhouse');
		lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
		sln('');
		lln('`2  You were heading towards the outhouse when you realized that you\'re');
		lln('`2  too tired. You turn around and head back to town.');
		sln('');
		lln('`2  You really didn\'t have to go that bad anyways, maybe tomorrow...');
		sln('');
		press_a_key();
	} else {
		mainMenu();
	}
	exit_game();
}

if (argc == 1 && argv[0] == 'INSTALL') {
	var install = {
		desc:outhouseNameFancy
	}
	exit(0);
} else {
	main();
	exit(0);
}