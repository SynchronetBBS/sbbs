'use strict';

function catchup()
{
	while (dk.console.waitkey(0)) {
		dk.console.getkey();
	}
}

function read_direction()
{
	var ch = dk.console.getkey();

	switch(ch) {
		case '8':
		case 'KEY_UP':
			return 'U';
		case '4':
		case 'KEY_LEFT':
			return 'L';
		case '2':
		case 'KEY_DOWN':
			return 'D';
		case '6':
		case 'KEY_RIGHT':
			return 'R';
		case 'q':
		case 'Q':
			return 'Q';
		case ' ':
			return ' ';
		default:
			return '|';
	}
}

function check_move(x, y)
{
	if (x > 57) {
		return false;
	}
	if (y > 21) {
		return false;
	}
	if (x < 16) {
		return false;
	}
	if (y < 4) {
		return false;
	}
	return true;
}

function run()
{
	var old_gold = player.gold;
	var time = 30;
	var over = false;
	var you_x = random(41) + 16;
	var you_y = random(17) + 4;
	var bar_x = random(41) + 16;
	var bar_y = random(17) + 4;
	var you_old_x = you_x;
	var you_old_y = you_y;
	var bar_old_x = bar_x;
	var bar_old_y = bar_y;
	var old_time = (new Date()).valueOf() + 500;
	var j;
	var gold = [];
	var tmp;
	var stole = 0;
	var num;
	var cur_time;
	var ch;

	function important(x, y) {
		var i;

		for (i = 0; i < gold.length; i++) {
			if (gold[i].x === x && gold[i].y === y) {
				return i;
			}
		}
		return -1;
	}

	for (j = 0; j < 10; j++) {
		tmp = {x:random(41)+16, y:random(17)+4};
		gold.push(tmp);
		dk.console.gotoxy(tmp.x - 1, tmp.y - 1);
		lw('`r6`%\xec`r0');
	}
	dk.console.gotoxy(you_x - 1, you_y - 1);
	lw('`r6`%\xea`r0');
	dk.console.gotoxy(bar_x - 1, bar_y - 1);
	lw('`r6`%B`r0');
	dk.console.gotoxy(1, 7);
	lw('`0READY...');
	dk.console.gotoxy(1, 8);
	mswait(1000);
	lw('`0SET...');
	mswait(1000);
	lw('`4GO!');
	mswait(400);
	do {
		ch = read_direction();
		if (ch === 'L') {
			if (check_move(you_x - 1, you_y)) {
				you_x -= 1;
			}
		}
		else if (ch === 'R') {
			if (check_move(you_x + 1, you_y)) {
				you_x += 1;
			}
		}
		else if (ch === 'U') {
			if (check_move(you_x, you_y - 1)) {
				you_y -= 1;
			}
		}
		else if (ch === 'D') {
			if (check_move(you_x, you_y + 1)) {
				you_y += 1;
			}
		}
		else if (ch === 'Q') {
			over = true;
		}
		if (you_old_x != you_x || you_old_y != you_y) {
			dk.console.gotoxy(you_x - 1, you_y - 1);
			lw('`r6`%\xea`r0');
			num = important(you_x, you_y);
			if (num >= 0) {
				gold[num].x = 0;
				gold[num].y = 0;
				player.gold += player.level * player.level * 100;
				stole += 1;
				dk.console.gotoxy(0, 7);
				lw('`r0`0Gold: `%'+pretty_int(player.gold));
				dk.console.gotoxy(0, 8);
				lw('`0Time: `%'+pretty_int(time)+'  ');
			}
			dk.console.gotoxy(you_old_x - 1,you_old_y - 1);
			lw('`r6 `r0');
		}
		you_old_x = you_x;
		you_old_y = you_y;
		if ((new Date()).valueOf() > old_time) {
			old_time += 500;
			if (you_x > bar_x && check_move(bar_x+1,bar_y)) {
				bar_x += 1;
			}
			if (you_x < bar_x && check_move(bar_x-1,bar_y)) {
				bar_x -= 1;
			}
			if (you_y > bar_y && check_move(bar_x,bar_y+1)) {
				bar_y += 1;
			}
			if (you_y < bar_y && check_move(bar_x,bar_y-1)) {
				bar_y -= 1;
			}
			dk.console.gotoxy(0, 7);
			lw('`r0`0Gold: `%'+pretty_int(player.gold));
			dk.console.gotoxy(0, 8);
			lw('`0Time: `%'+pretty_int(time)+'  ');
			time--;
		}
		if (bar_old_x !== bar_x || bar_old_y !== bar_y) {
			dk.console.gotoxy(bar_x - 1, bar_y - 1);
			lw('`r6`%B`r0');
			dk.console.gotoxy(bar_old_x - 1, bar_old_y - 1);
			num = important(bar_old_x, bar_old_y);
			if (num >= 0) {
				lw('`r6\xec`r0');
			}
			else {
				lw('`r6 `r0');
			}
		}
		bar_old_x = bar_x;
		bar_old_y = bar_y;
		if (bar_old_x === you_x && bar_old_y === you_y) {
			dk.console.gotoxy(bar_x - 4, bar_y - 1);
			lw('`)Splat!');
			mswait(1000);
			lln('`r0`c  `%YOU ARE DEFEATED.');
			lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			lln('  `2Barak laughs as warm blood flows down your cheek.');
			if (old_gold != player.gold) {
				sln('  He savagely takes back the gold you stole from him.');
			}
			player.gold = old_gold;
			sln('  Maybe next time?');
			sln('');
			lln('  `4YOU FEEL AWFULLY WEAK.');
			sln('');
			player.hp = 1;
			catchup();
			return;
		}
		if (time < 0) {
			dk.console.gotoxy(you_x - 4, you_y - 1);
			lw('`0YAHOO!');
			mswait(1000);
			lln('`r0`c  `%YOU PUT BARAK TO SHAME!');
			lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			lln('  `2Barak curses as you nimbly dance away from his');
			sln('  knife.  Not only did you live, you also stole ');
			lln('  `0'+pretty_int(player.gold - old_gold)+'`2 from his house!');
			sln('');
			if (player.clss === 3) {
				if (stole === 10) {
					lln('  `%FOR FANTASTIC THIEVING, YOU GET AN EXTRA '+pretty_int(200 * player.level * player.level));
					player.gold += 200 * player.level * player.level;
					sln('');
				}
			}
			lln('  `%YOU HEAD HOME, IN GOOD HUMOR.');
			sln('');
			catchup();
			return;
		}
	} while (!over);
}

function start_fight()
{
	var i;
	var mc = morechk;

	morechk = false;
	lln('`c                      `%HAVING FUN AT BARAK\'S');
	sln('');
	for (i = 0; i < 18; i++) {
		lln('`r0               `r6                                          `r0');
	}
	sln('');
	lln('    `0(`2Use the keypad`0, `2arrow keys or `%Ctrl`0-`2S`0,`2E`0,`2D`0,`2X keys to run like hell!`0)');
	run();
	morechk = mc;
	more_nomail();
}

function sugar()
{
	var ch;

	if (player.sex === 'M') {
		lln('  `0"You want sugar?!  Go give a few gems to `#Violet`0, she\'ll give');
		sln('  you some sugar!  Har!"');
	}
	else {
		lln('  `0"You want sugar?!  Go give a few gems to `%Seth Able`0, he\'ll give');
		sln('  you some sugar!  Har!"');
	}
	sln('');
	lln('  `2(`0Y`2)ou animal!  How dare you! Prepare to fight!');
	lln('  `2(`0L`2)augh loudly at Baraks lame humor.');
	sln('');
	lw('  `2Your choice?  [`0Y`2] :`%');
	ch = getkey().toUpperCase();
	sln(ch);
	sln('');
	if (ch === 'L') {
		lln('  `2You giggle uncontrollably.  Barak looks pleased as hell.');
		sln('');
		more_nomail();
		lln('  `0"You know kid?  You\'re ok.  Here is a little somethun for ya."');
		sln('');
		lln('  `%BARAK TOSSES YOU A GEM!');
		player.gem++;
		player.put();
		sln('');
		sln('  You trot back home in triumph.');
		sln('');
		more_nomail();
		return;
	}
	lln('  `2Barak looks quite upset.');
	sln('');
	more_nomail();
	lln('  `2He then pulls out a bigass knife.');
	sln('');
	more_nomail();
	lln('  `2He then proceeds to chase you around the house with it.');
	sln('');
	more_nomail();
	start_fight();
}

function say_slow(str)
{
	var i;

	for (i = 0; i < str.length; i++) {
		sw(str[i]);
		mswait(100);
	}
}

function say_slow2(str)
{
	var i;

	for (i = 0; i < str.length; i++) {
		sw(str[i]);
		mswait(10);
	}
}

function hair_end(times_hit, shots_left) {
	var num_end;


	lln('`c                            `%EPILOGUE');
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-');
	lln('  `2The battle is over.');
	sln('');
	if (times_hit > 0 && times_hit < 5) {
		lln('  You struck the hair `0'+pretty_int(times_hit)+'`2 times.');
	}
	if (times_hit === 0) {
		lln('  `2The old woman laughs at you.  `#"You are the worst shot I have');
		sln('  ever seen, fool!  Go practice with the hair on your back.  Begone."');
		sln('');
		if (player.sex === 'M') {
			lln('  `4The old women flashes you!  `2You gag reflexes take over as you');
			lln('  gape at her `4crusty saggers`2.');
			sln('');
		}
		lln('  `%YOU TRUDGE HOME IN DEFEAT - YOU FEEL HAIRABLE.');
		player.high_spirits = false;
		sln('');
		more_nomail();
		return;
	}
	if (times_hit === 5) {
		lln('  `2You totally `%DESTROYED `2the hair!');
		sln('');
		if (shots_left > 0) {
			lln('  `0You even had `%'+pretty_int(shots_left)+' `2tries left!');
		}
	}
	sln('');
	more_nomail();
	if (times_hit < 5) {
		lln('  `#"Not bad!  Here is your reward." `2the wrinkled woman cackles.');
	}
	else {
		lln('  `#"Incredible!  Here is your reward."`2 the saggy one exclaims.');
	}
	lln('  `0She waves her hair strangely.');
	sln('');
	if (times_hit > 4) {
		num_end = (times_hit+shots_left) * (10*player.level);
	}
	else {
		num_end = (times_hit) * (5*player.level);
	}
	num_end *= player.level;
	player.exp += num_end;
	if (player.exp > 220020020) {
		player.exp = 220020020;
	}
	lln('  `%YOU GET '+pretty_int(num_end)+' EXPERIENCE.');
	sln('');
	if (times_hit === 5) {
		lln('  `%YOUR STRENTH IS RAISED BY ONE!');
		player.str += 1;
		sln('');
		// DIFF: The original returned here...
	}
	more_nomail();
	player.high_spirits = true;
	lln('  `0You travel home in an incredible mood.');
}

function fly()
{
	var j;
	var tries;
	var time;
	var old_time2;
	var old_time;
	var old_time1;
	var cur_time;
	var wz;
	var wy;
	// DIFF: forward1 was uninitialized...
	var forward1 = !(random(2) === 0);
	var wep_char;
	var times_hit;
	var speed;
	var ch;

	wep_char = 'I';
	lln('`c                     `%A HAIRY PREDICAMENT');
	sln('');
	for (j = 0; j < 10; j++) {
		lln('`r0                     `r6                   `r0');
	}
	lln('`r0                     `r6         `#o         `r0');
	lln('`r0                     `r6        `#<`5\xdb`#>        `r0');
	lln('`r0                     `r6        `#/\'>        `r0');
	sln('');
	tries = 10;
	time = 40;
	dk.console.gotoxy(24,18);
	lw('`0Tries Left: `%'+pretty_int(tries)+'  ');
	dk.console.gotoxy(24,19);
	lw('`0Time  Left: `%'+pretty_int(time)+'  ');
	speed = random(10);
	dk.console.gotoxy(0,22);
	lln('              `0(Press space to take your best shot)          ');
	old_time = (new Date()).valueOf();
	old_time1 = old_time;
	old_time = old_time + 500;
	old_time1 = old_time1 + speed * 10;
	wz = 23;
	wy = 5;
	dk.console.gotoxy(wz-1,wy-1);
	lln('`r6\xf7\xf7');
	times_hit = 0;
	j = 1;

	do {
		ch = read_direction();
		if (tries > 0 && ch === ' ') {
			tries -= 1;
			j = 14;
			old_time2 = cur_time+100;
			dk.console.gotoxy(24,18);
			lw('`0`r0Tries Left: `%'+pretty_int(tries)+' ');
		}
		if (cur_time > old_time2 && j > 4) {
			if (j !== 14) {
				dk.console.gotoxy(29,j);
				lw('`r6 ');
			}
			dk.console.gotoxy(29,j - 1);
			lw('`r6'+wep_char);
			j -= 1;
			if (j === 4 && wz > 28 && wz < 31) {
				times_hit += 1;
				dk.console.gotoxy(0,22);
				lw('`r0');
				if (times_hit === 1) {
					lw('              `0You hit the thing!  It wobbles a little. `2<KEY>             ');
				}
				if (times_hit === 2) {
					lw('              `0Nice shot!  The wig falters a bit. `2<KEY>                  ');
				}
				if (times_hit === 3) {
					lw('              `0Direct hit!  The hair piece is limping around! `2<KEY>           ');
				}
				if (times_hit === 4) {
					lw('              `0You knock some hairs off - It\'s almost dead! `2<KEY>               ');
				}
				if (times_hit === 5) {
					lw('              `0Beautiful shot.  The wig stops moving.  `2<KEY>               ');
				}
				ch = getkey();
				if (times_hit === 5) {
					hair_end(times_hit,tries);
					return;
				}
				speed = random(10);
				dk.console.gotoxy(0,22);
				lln('`r0              `0(Press space to take your best shot)               ');
			}
			old_time2 = cur_time+100;
		}
		if (time < 1) {
			dk.console.gotoxy(0,22);
			lln('`r0              `%YOU ARE OUT OF TIME!                     ');
			mswait(2000);
			hair_end(times_hit, tries);
			return;
		}
		if (tries < 1 && j < 5) {
			dk.console.gotoxy(0,22);
			lln('`r0              `%YOU ARE TOO TIRED TO THROW AGAIN!        ');
			mswait(2000);
			hair_end(times_hit,tries);
			return;
		}
		cur_time = (new Date()).valueOf();
		if (cur_time > old_time) {
			old_time = cur_time;
			old_time += 500;
			time -= 1;
			dk.console.gotoxy(25,20);
			lw('`0`r0Time  Left: `%'+pretty_int(time)+'  ');
		}
		if (cur_time > old_time1) {
			old_time1 = cur_time+speed * 10;
			dk.console.gotoxy(wz-1,wy-1);
			lw('`r6  ');
			if (forward1) {
				wz += 1;
				if (wz === 39) {
					forward1 = false;
				}
			}
			else {
				wz -= 1;
				if (wz < 24) {
					forward1 = true;
				}
			}
			dk.console.gotoxy(wz-1,wy-1);
			lw('`r6`$\xf7\xf7');
		}
	} while (time >= 0);
}

function beard()
{
	var man;
	var ch;

	sln('');
	lln('  `2Barak\'s face falls. `0"You don\'t like my beard?"');
	sln('');
	lln('  `%"No, I definitely do not." `2you assure him.');
	sln('');
	lln('  `2A large tear wells up in one if his eyes.');
	sln('');
	more_nomail();
	lln('  `2Just then, an old women pops up behind him!');
	sln('');
	lw('`0  ');
	say_slow('"MOTHER!"');
	lln('  `2Barak screams.  `0"Get back into the basement!"');
	sln('');
	more_nomail();
	man = (player.sex === 'M' ? 'man' : 'woman');
	lln('  `#"I will not, boy! - That young '+man+' just insulted your beard!"');
	sln('');
	lln('  `2You scowl at the hag.  `%"I\'m just being honest with your boy, ma\'am."');
	sln('');
	lln('  `#"I cannot tell if you are being serious or not!  Are you willing to');
	sln('  let me test your skills?"');
	sln('');
	lln('  `2(`0A`2)gree with the hag');
	lln('  `2(`0T`2)ell her to shove it');
	sln('');
	lw('  `2You decide to ... [`0A`2] :`%');
	ch = getkey().toUpperCase();
	if (ch !== 'T') {
		ch = 'A';
	}
	sln(ch);
	sln('');
	if (ch === 'T') {
		player.hp = 1;
		lln('  `%"Forget it, ancient one.  Your boy looks like an ogre." `2you');
		sln('  taunt.');
		sln('');
		more_nomail();
		man = (player.sex === 'M' ? '"YOU BASTARD!"' : '"YOU BITCH!"');
		lw('  `#');
		say_slow(man);
		lln('  `2screams the old woman, spittle forming at her mouth. ');
		sln('  She then plucks off her hair - only a few strands of white adorn');
		sln('  her bald head!');
		sln('');
		more_nomail();
		lln('  `4SHE `)THROWS `4HER HAIR AT YOU!');
		sln('');
		more_nomail();
		lln('`c  `2Time has passed.  Hours have passed - you rub your sore head');
		sln('  wondering what happened.');
		sln('');
		more_nomail();
		lln('  `2YOU TRUDGE HOME WEAK AND DEJECTEDLY.');
		sln('');
		return
	}
	if (ch === 'A') {
		player.hp = 1;
		lln('  `%"Lets do this thing, antique." `2you challenge.');
		sln('');
		lln('  `2Her wig rises from her head as if by `#magic`2!');
		sln('');
		lln('  `%IT FLOATS WILDLY AROUND THE ROOM!');
		sln('');
		more_nomail();
		lln('  `2You grip your `0'+player.weapon+' `2tightly, and prepare to');
		sln('  take down the hurling hair piece.');
		sln('');
		more_nomail();
		fly();
		return;
	}
}

function wait()
{
	mswait(1000);
	sw('.');
}

function history()
{
	if (random(2) == 0) {
		lln('  `%"The Way Things Were" `2by `0Master Turgon`2.');
		sln('');
		sln('  Our town has gone to pot.  Things used to be so nice - Children');
		sln('  used to play on the street.  Now they huddle together under their');
		lln('  beds and whisper stories about the dreaded `4Dragon`2. ');
		sln('');
		lln('  I remember when my own daughter, `#Violet`2 (my but she\'s grown) used');
		sln('  laugh and play outside.  (now she seems to play inside more often now)');
		sln('');
		sln('  Many have asked, why don\'t *I* hunt the dragon?  The answer is');
		sln('  simple.  I\'m not expendable - like all these new warriors.  Someone');
		sln('  must stay behind and teach the others. (Also, someone\'s got to run');
		sln('  my training center, right?!)');
		sln('');
		more_nomail();
		sln('  Am I a coward?  Maybe.  But I am getting along in years.  We need a');
		sln('  younger hero.');
		sln('');
		sln('                      ** THE END **');
		sln('');
		more_nomail();
	}
	else {
		lln('  `%"The Story Of The Gods" `2by `0Master Turgon`2.');
		sln('');
		sln('  Even in this day and age many of us are religious.  Many of us');
		sln('  still believe in God.  Some say it is a male deity, others say');
		lln('  female.  I believe the latter.  Her name is said to be `%Jennie`2.');
		sln('');
		sln('  Once a man named Nalyd Yakcm screamed the devine ones named in the');
		sln('  forest.  He came back to the village telling people she appeared to');
		sln('  him.  Is it true?  None can say.');
		sln('');
		sln('                      ** THE END **');
		sln('');
		more_nomail();
	}
}

function skill()
{
	if (player.clss === 1) {
		lln('  `%"The Art Of Swordfighting" `2by Aragorn.');
		sln('');
		sln('  Swing it good, swing it hard - and try to avoid blows to your');
		sln('  groin area.');
		if (player.skillw < 40) {
			player.skillw += 1;
		}
	}
	if (player.clss === 2) {
		lln('  `%"The Art Of Being Mystical" `2by Atsuko Sensei.');
		sln('');
		sln('  Never fully explain yourself, and well - thats pretty much');
		sln('  it.');
		if (player.skillm < 40) {
			player.skillm += 1;
		}
	}
	if (player.clss === 3) {
		lln('  `%"The Art Of Thievery" `2by Chance.');
		sln('');
		sln('  Sellect your targets carefully.  Don\'t steal from level');
		sln('  12 people - being beheaded isn\'t particularly fun.');
		if (player.skillt < 40) {
			player.skillt += 1;
		}
	}
}

function newspaper()
{
	switch (random(9)) {
		case 0:
			lln('  `2You read a clipping from `06 `2years ago.');
			sln('');
			lln('      `2-=-=-=-=`%DIVORCE ROCKS NATION!`2=-=-=-=-');
			lln('  `2Sweet hearts `0Seth Able `2and `#Violet `2are');
			lln('  divorced!  `0"They were a troubled couple" `2reports');
			sln('  a close friend.  No one really knows why the breakup');
			sln('  occurred.');
			break;
		case 1:
			lln('  `2You read a clipping from `02`2 years ago.');
			sln('');
			lln('      `2-=-=-=-=`%CHILD FOUND!`2=-=-=-=-');
			lln('  `2Tiny angelic faced `#Lee Wren`2 was found today.  Her');
			sln('  life was barely saved by the healers - thanks to The');
			lln('  Old Man who brought her in.  `0"Usually it\'s the old ');
			lln('  man that needs saving." `2a bystander comments.');
			break;
		case 2:
			lln('  `2You read a clipping from `07`2 years ago.');
			sln('');
			lln('      `2-=-=-=-=`%TROUBLE IN PARADISE!`2=-=-=-=-');
			lln('  `2A fight errupted in the Able home as newly weds `0Seth`2 ');
			lln('  and `#Violet`2 had a squabble.  Over what?  `0"A big dumb egg"');
			lln('  `2a family friend informed.  Now, the relationship as well');
			sln('  as the egg is cracked as ever.');
			break;
		case 3:
			lln('  `2You read a clipping from `013`2 years ago.');
			sln('');
			lln('      `2-=-=-=-=`%GRAND OPENING!`2=-=-=-=-');
			lln('  `2Are you scrawny and have arms like toothpicks?  Come enroll at');
			lln('  `%Turgon\'s Warrior Training`2.  Head trainer `0Turgon `2GURANTEE\'S');
			lln('  `2you\'ll be kicking butt in two weeks flat. (`0Women trained too!`2)');
			break;
		case 4:
			lln('  `2You read a clipping from `08`2 years ago.');
			sln('');
			lln('      `2-=-=-=-=`%NEW TRAINER HIRED!`2=-=-=-=-');
			lln('  `2Recently a local boy known as Barak was hired as a level 2');
			lln('  master at Turgon\'s Warrior Training.  `0"We needed somebody');
			lln('  fast and couldn\'t be picky."`2 commented Turgon.');
			break;
		case 5:
			lln('  `2You read a clipping from `09`2 years ago.');
			sln('');
			lln('      `2-=-=-=-=`%AN UNPOPULAR HAUNT IS CLOSED`2=-=-=-=-');
			lln('  `%"King Arthur\'s House Of Sex" `2was closed today.  It seems');
			lln('  the owner just wasn\'t getting the business. `0"People seemed to');
			lln('  prefer good looking girls.  Next time I\'ll listen."`2 he commented');
			break;
		case 6:
			lln('  `2You read a clipping from `09`2 years ago.');
			sln('');
			lln('      `2-=-=-=-=`%A NEW PLACE OPENS!`2=-=-=-=-');
			lln('  `%"Abdul\'s Armour" `2had it\'s grand opening today.  It seems the');
			lln('  young lady who owns it used to be a minstrel. `0"People were always');
			sln('  fighting over me and getting hurt.  I saw the need for better');
			lln('  armour."`2 she commented.');
			break;
		case 7:
			lln('  `2You read a clipping from `09`2 years ago.');
			sln('');
			lln('      `2-=-=-=-=`%THIEF ESCAPES!`2=-=-=-=-');
			lln('  `2A young man known as `%"Chance"`2 `2escaped jail today.  Many believe');
			sln('  this hard to find individual to be a ringleader of a guild of thieves.');
			lln('  `0"His father was a Master Thief - Young `%Chance`2 will become one also."');
			lln('  `2comments Turgon.');
			break;
		case 8:
			lln('  `2You read a clipping from `02`2 months ago.');
			sln('');
			lln('      `2-=-=-=-=`%Healers Blow It!`2=-=-=-=-');
			lln('  `2The level one master `%Halder`2 seems to have convinced the healers to');
			sln('  create a wieght gaining potion for him.  Instead, the faulty formula');
			lln('  shrunk his twig and berries.  `%Halder`2 was heard begging `0"Please don\'t');
			sln('  print this!"');
			break;
	}
}

var x1 = 0;
var y1 = 0;
function draw_man(x, y)
{
	if (x1 !== 0) {
		dk.console.gotoxy(x1, y1 - 1);
		lw('`r0 ');
		dk.console.gotoxy(x1 - 1, y1);
		sw('   ');
		dk.console.gotoxy(x1 - 1,y1 + 1);
		sw('   ');
	}
	dk.console.gotoxy(x, y - 1);
	lw('`r0`#o');
	dk.console.gotoxy(x - 1, y);
	lw('`#<`5\xdb`#>');
	dk.console.gotoxy(x - 1,y + 1);
	lw('`#/\'>');
	x1 = x;
	y1 = y;
}

function chest()
{
	var chest = [true, true, true, true, true, true];
	var x = 21;
	var y = 7;
	var bad_one = random(6);
	var cur_chest;
	var n1;
	var j;
	var chests_opened = 0;
	var ch;

	lln('`c                           `0** `%THE BASEMENT `0**');
	sln('');
	lln('                 `r1`!\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc`r0                       `r1\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc`r0');
	lln('                 `r1\xdb\xdb\xdb\xdb\xdb\xdb\xdb\xdb`r0                       `r1\xdb\xdb\xdb\xdb\xdb\xdb\xdb\xdb`r0');
	sln('');
	sln('');
	sln('');
	lln('                 `r1`!\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc`r0                       `r1\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc`r0');
	lln('                 `r1\xdb\xdb\xdb\xdb\xdb\xdb\xdb\xdb`r0                       `r1\xdb\xdb\xdb\xdb\xdb\xdb\xdb\xdb`r0');
	sln('');
	sln('');
	sln('');
	lln('                 `r1`!\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc`r0                       `r1\xdc\xdc\xdc\xdc\xdc\xdc\xdc\xdc`r0');
	lln('                 `r1\xdb\xdb\xdb\xdb\xdb\xdb\xdb\xdb`r0                       `r1\xdb\xdb\xdb\xdb\xdb\xdb\xdb\xdb`r0');

	dk.console.gotoxy(0, 19);
	lln('  `0(`2Use Arrow Keys`0, `2Num Pad or `%Ctrl`0-`2S`0,`2E`0,`2D`0,`2X`0 to move. `%Space `2to open chests`0)');

	draw_man(x, y);
	do {
		ch = read_direction();
		if (ch === 'R') {
			if (x < 51) {
				x += 1;
				draw_man(x, y);
			}
		}
		if (ch === 'L') {
			if (x > 21) {
				x -= 1;
				draw_man(x, y);
			}
		}
		if (ch === 'U') {
			if (y > 7) {
				y -= 1;
				draw_man(x, y);
			}
		}
		if (ch === 'D') {
			if (y < 17) {
				y += 1;
				draw_man(x, y);
			}
		}
		if (ch === ' ') {
			cur_chest = -1;
			if (x === 21 && y === 7) {
				cur_chest = 0;
			}
			if (x === 21 && y === 12) {
				cur_chest = 1;
			}
			if (x === 21 && y === 17) {
				cur_chest = 2;
			}
			if (x === 51 && y === 7) {
				cur_chest = 3;
			}
			if (x === 51 && y === 12) {
				cur_chest = 4;
			}
			if (x === 51 && y === 17) {
				cur_chest = 5;
			}
			if (cur_chest === -1) {
				dk.console.gotoxy(0, 19);
				lw('  `0Are you trying to open air?                                            ');
				continue;
			}
			else if (chest[cur_chest] === false) {
				dk.console.gotoxy(0, 19);
				lln('  `0The chest is empty.  (`2Hmm - Maybe \'cuz you already opened it?!`0)');
				more_nomail();
				dk.console.gotoxy(0, 19);
				lw('  `0Barak seems to be looking the other way.                             ');
			}
			else {
				dk.console.gotoxy(0, 19);
				lw('  `0You sneakily open a chest up while Barak isn\'t looking...               ');
				if (cur_chest < 3) {
					dk.console.gotoxy(x-4,y-3);
				}
				else {
					dk.console.gotoxy(x-3,y-3);
				}
				lw('`r1`!\xdf\xdf\xdf\xdf\xdf\xdf\xdf\xdf`r0');
				dk.console.gotoxy(0, 20);
				lw('  `2');
				say_slow('YOU FIND...');
				lw('`%');
				if (bad_one === cur_chest) {
					lw('`4');
					say_slow2('BARAK\'S CRAZY MOTHER!');
					sln('');
					sln('');
					more_nomail();
					lln('`c                   `%**   `4THE JIG IS UP.  `%**');
					lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
					lln('  `#"You thieving little puke!" `2Barak\'s mother screams at you.');
					lln('  `2Bits of foam bubble through her teeth in her frenzy.');
					sln('');
					more_nomail();
					if (chests_opened > 1) {
						lln('  At least you got away with opening `%'+pretty_int(chests_opened)+'`2 chests!');
					}
					else if(chests_opened === 0) {
						sln('  You didn\'t steal one thing - Rotten luck today.');
					}
					else {
						sln('  You only stole one thing - Sort of rotten luck today.');
					}
					sln('');
					sln('  At her command, Barak throws you out!');
					sln('');
					if (chests_opened > 1) {
						lln('  `0You trudge back to town `2- `%Victorious`0!');
					}
					else if (chests_opened === 1) {
						lln('  `0You trudge back to town.  Not especially proud of yourself.');
					}
					else {
						lln('  `4You crawl back to down in total defeat.');
					}
					sln('');
					more_nomail();
					return;
				}
				chest[cur_chest] = false;
				chests_opened += 1;
				if (random(2) == 0) {
					say_slow2('A GEM!');
					player.gem += 1;
				}
				else {
					n1 = 20 * player.level;
					n1 *= player.level;
					n1 = random(n1 * player.level) + 1;
					say_slow2('A POUCH WITH '+pretty_int(n1)+' GOLD IN IT!');
					player.gold += n1;
					if (player.gold > 2100100100) {
						player.gold = 2100100100;
					}
				}
				sln('');
				more_nomail();
				if (chests_opened === 5) {
					lln('`c                    `%**  `0EXELLENT!  `%**');
					lln('`2-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
					lln('  `2You figure now would be a good time to leave - You have a');
					lln('  `2feeling something awfully putrid is in that last chance...');
					sln('');
					lln('  `0"Leaving already, friend '+player.name+'`0?" `2Barak asks');
					sln('  disappointedly.');
					sln('');
					lln('  `2You barely supress laughing at loud.  `%"Uh, yeah... Been a long day."');
					sln('');
					lln('  `0BARAK GIVES YOU SOME ULTRA ALE FOR HELPING HIM CLEAN UP!');
					player.hp = player.hp_max + parseInt(player.hp_max / 4, 10);
					sln('');
					lln('  `0You trudge back to town `2- `%FEELING WONDERFUL`0!');
					more_nomail();
					return;
				}
				dk.console.gotoxy(0, 20);
				dk.console.cleareol();
				dk.console.gotoxy(0, 19);
				dk.console.cleareol();
				switch (random(6)) {
					case 0:
						lw('  `0Barak looks occupied with studying his \'Playmaid\' collection...');
						break;
					case 1:
						lw('  `0Barak seems busy scratching himself in a corner...');
						break;
					case 2:
						lw('  `0Now seems like a good time to steal something...');
						break;
					case 3:
						lw('  `0You smile - Barak is totally absorbed in chasing a rat around...');
						break;
					case 4:
						lw('  `0Barak looks busy arranging his severed heads...');
						break;
					case 5:
						lw('  `0Barak is busy amusing himself by making faces in a mirror.');
						break;
				}
			}
		}
	} while (ch !== 'Q');
	more_nomail();
}

function shoot()
{
	var r;
	var ch;

	lln('`c  `%Chatting With Barak');
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	lln('  `0"Shoot the breeze?" `2Barak asks, obviously puzzled.');
	sln('');
	lln('  `2(`0C`2)an I read some of your books?');
	lln('  `2(`0W`2)ant to play a game?');
	sln('');
	lw('  `2You decide to say... [`0W`2] :`%');
	ch = getkey().toUpperCase();
	sln(ch);
	sln('');
	if (ch === 'C') {
		lln('  `0"Books?!  BOOKS?!  You know I can\'t read!" `2Barak shouts, tears');
		sln('  streaming out of his eyes.');
		sln('');
		more_nomail();
		lln('  `2(`0L`2)augh like hell at poor Barak.');
		lln('  `2(`0O`2)ffer to read him a story.');
		sln('');
		lw('  `2You decide to ... [`0O`2] :`%');
		ch = getkey().toUpperCase();
		sln(ch);
		if (ch !== 'L') {
			r = random(3) + 1;
			lln('  `0"You will?" `2Barak pitifully, wiping his nose.  `0"Will');
			lln('  you read this to me?"');
			lln('');
			lw('  `2Barak shows you a book of.');
			wait();
			wait();
			wait();
			if (r === 1) {
				lln('  `%History`2.');
			}
			if (r === 2) {
				lln('  `%Newspaper Clippings`2.');
			}
			if (r === 3) {
				if (player.clss === 1) {
					lln('`0Fighting`2.');
				}
				if (player.clss === 2) {
					lln('`#Magic Use`2.');
				}
				if (player.clss === 3) {
					lln('`1Dirty Deeds`2.');
				}
			}
			sln('');
			lln('  `2You are non-plussed, but agree to read it.');
			sln('');
			more_nomail();
			lln('`c  `%Story time with Barak');
			lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
			if (r === 1) {
				history();
			}
			if (r === 2) {
				newspaper();
			}
			if (r === 3) {
				skill();
			}
			sln('');
			lln('  `2You put down the book.  `0"Please, '+player.name+'`0!  Read more!"');
			lln('  `2Barak whines.');
			sln('');
			more_nomail();
			lln('  `2You smile.  `%"Nah, I gotta go.  See you later."');
			sln('');
			more_nomail();
		}
		else {
			lw('  `2You can\'t stop yourself from bellowing out in laughter.  Barak\'s');
			sln('   face falls.  Then turns to stone.');
			sln('');
			more_nomail();
			sln('  He then pulls out an Able\'s Sword!');
			sln('');
			more_nomail();
			sln('  Barak hunts you down like a dog.');
			sln('');
			more_nomail();
			start_fight();
		}
		return;
	}
	lln('  `0"Game?  Ok - Uh, want to play \'let\'s clean out the basement\'?"');
	lln('  `2Barak asks slyly.');
	sln('');
	lln('  `2(`0O`2)k, uh, that sounds like a really fun game.');
	lln('  `2(`0F`2)orget it.  I\'m not that stupid.');
	sln('');
	lw('  `2You decide to ... [`0O`2] :`%');
	ch = getkey().toUpperCase();
	if (ch !== 'F') {
		ch = 'O';
	}
	sln(ch);
	sln('');
	if (ch === 'O') {
		lln('  `2Barak looks overjoyed.  You smile at his simplicity, and get');
		lln('  ready to pocket a few things for yourself in this little \'cleanup\'.');
		sln('');
		more_nomail();
		chest();
		return;
	}
	lln('  `0"You stupid brat!" `2screams Barak in a fit of rage.  `0"Get out');
	sln('  my house!"');
	sln('');
	lln('  `2You wonder if helping out would have been that bad of an idea...');
	sln('');
	lln('  `%YOU TRUDGE HOME, FEELING LIKE A LOSER.');
	sln('');
	more_nomail();
}

function knock()
{
	var ch;

	lln('`c  `%Visiting Old Friends');
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	lln('  `%Barak opens the door!');
	sln('');
	lln('  `0"Whadaya ya want, kid?" `2Barak asks harshly.');
	sln('');
	lln('  `2(`0J`2)ust wanted to shoot the breeze, friend!');
	lln('  `2(`0C`2)an I borrow a cup of sugar, neighbor?');
	lln('  `2(`0Y`2)our beard went out of style centuries ago.');
	sln('');
	lw('  `2You decide to say... [`0J`2] :`%');
	ch = getkey().toUpperCase();
	sln(ch);
	sln('');
	if (ch === 'C') {
		sugar();
		return;
	}
	if (ch === 'Y') {
		beard();
		return;
	}
	shoot();
}

function walk_in()
{
	var ch;

	lln('`c  `%Uh oh...');
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	lln('  `2You saunter in like you own the place.  Barak');
	sln('  stares at you in wonder as you help yourself to');
	sln('  some meat and cheese sitting on the table.');
	sln('');
	lln('  `0"You insolent pubby!  You will die for this."`2 the ');
	sln('  bearded man growls.');
	sln('');
	lln('  `2(`0A`2)ppologize and leave him be');
	lln('  `2(`0K`2)ick him in the shin and have a good laugh');
	sln('');
	lw('  `2You decide to... [`0A`2] :`%');
	ch = getkey().toUpperCase();
	if (ch !== 'K') {
		ch = 'A';
	}
	sln('');
	sln('');
	if (ch === 'A') {
		lln('  `%"I\'m uh.. sorry.. I thought no one was home," `2you');
		sln('  finish lamely.');
		if (player.cha > 1) {
			player.cha -= 1;
		}
		player.put();
		sln('');
		lln('  `0"You stupid fool!" `2 Barak screams.  He then gives');
		sln('  you a severe throttling to your face and ears.');
		sln('');
		more_nomail();
		lln('  `2You are then thrown out of his house, landing in a rather');
		sln('  large pile of cow dung.');
		sln('');
		lln('  `%(THE SMELL IS OVERWELMING, YOU LOSE 1 CHARM)');
		sln('');
		more_nomail();
	}
	if (ch === 'K') {
		lln('  `2You kick him a good one!');
		sln('');
		if (player.level < 3) {
			lln('  `2Barak laughs at your puny attempt.');
			lln('');
			player.cha -= 1;
			more_nomail();
			sln('  He grabs you by your throat and lifts you off the ground.');
			lln('  `0"You fool.  I am the level two master - And you have never bested');
			sln('  me.  How do you expect to do so now?"');
			sln('');
			more_nomail();
			sln('  `2You are then thrown out of his house, landing in a rather');
			sln('  large pile of cow dung.');
			sln('');
			lln('  `%(THE SMELL IS OVERWELMING, YOU LOSE 1 CHARM)');
		}
		else {
			lln('  `%He screams in pain!');
			sln('');
			more_nomail();
			lln('  `2You help yourself to another chunk of bread, and');
			sln('  laugh so hard at Barak small pieces fly out of');
			sln('  your mouth and pummel him.');
			sln('');
			more_nomail();
			lln('  `0"No more!" `2Barak shrieks in a rather high pitched voice.');
			sln('');
			lln('  You laugh.  `%"Give me your most valuable possesion, you hairy');
			lln('  fool." `2you demand.');
			sln('');
			lln('  `0"Alright!  I\'ll give you a flask of my Ultra Ale, damnit!"');
			sln('');
			more_nomail();
			sln('  `2You snatch up this \'Ultra Ale\' and drain it in one swig.');
			sln('');
			lln('  `%YOU FEEL INVICINCIBLE!');
			sln('');
			lln('  `2You feel you\'ve done enough Barak taunting for now and head home.');
			player.hp = player.hp_max + parseInt(player.hp_max / 4, 10);
		}
		sln('');
		more_nomail();
	}
}

var Barak_Defs = [
	{
		prop:'day',
		type:'SignedInteger',
		def:-1
	},
	{
		prop:'can_play',
		type:'Array:150:Boolean',
		def:eval('var aret = []; while(aret.length < 150) aret.push(true); aret;')
	}
];

function run_maint(b)
{
	var i;

	for (i = 0; i < b.can_play.length; i++) {
		b.can_play[i] = true;
	}
	b.day = state.days;
	b.put();
}

var bs;
function main()
{
	'use strict';
	var ch;
	var b;
	var i;

	foreground(2);
	background(0);

	// TODO: Some sort of install/uninstall thing...

	lln('`r0`0`2`c  `%Visiting A Friend');
	lln('`0-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-');
	if (!dk.console.ansi) {
		sln('  NOTE:  The \'arcade\' sequences in this IGM *REQUIRE* ANSI terminal');
		sln('  support.  Things will look out of wack in your current settings.');
		sln('  You can switch to ANSI inside of LORD by pressing 3 from the main');
		sln('  menu.  You just better hope your terminal supports it...');
		sln('');
	}

	bs = new RecordFile(js.exec_dir+'barak.dat', Barak_Defs);
	js.on_exit('bs.locks.forEach(function(x) {bs.unLock(x); bs.file.close()});');
	if (bs.length < 1) {
		b = bs.new();
	}
	else {
		b = bs.get(0);
	}
	if (b.day != state.days) {
		run_maint(b);
	}
	if(!b.can_play[player.Record] || player.forest_fights < 1) {
		lln('  `2You like Barak and all - But you feel a might too weary to');
		sln('  make the trip.  Maybe tomorrow.');
		sln('');
		more_nomail();
		exit(0);
	}
	b.can_play[player.Record] = false;
	b.put();

	// TODO: Not range checked or anything...
	player.forest_fights--;
	lln('  `2Feeling a might lonely, you decide to pay a visit to a');
	sln('  dear friend.  It\'s no short journey and you are quite');
	sln('  tired when you arrive.');
	sln('');
	lln('  `2(`0K`2)nock on the door');
	lln('  `2(`0W`2)alk in like you own the place');
	lln('  `2(`0H`2)ead back to town');
	sln('');
	lw('  `2You decide to... [`0K`2] :`%');
	ch = getkey().toUpperCase();
	if ('WHK'.indexOf(ch) == -1) {
		ch = 'K';
	}
	sln(ch);
	sln('');
	if (ch === 'H') {
		lln('  `2You decide maybe you should have called first - and trudge back');
		sln('  home.');
		sln('');
		more_nomail();
		exit(0);
	}
	if (ch === 'K') {
		knock();
	}
	if (ch == 'W') {
		walk_in();
	}
}

main();
exit(0);
