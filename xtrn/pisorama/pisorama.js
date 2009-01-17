/***************************************************************************
*Please Read!  PLEASE READ!  Please Read!  PLEASE READ!  Please READ!!@!!@ *
*                                                                          *
*                              The Piss-O-Rama                             *
*                            Version 2.0                                   *
*                                                                          *
*           By: Dave Lahti  a.k.a.  The Blue Adept                         *
*                                                                          *
*   This is the Second version of the Piss-O-Rama, a favorite among        *
* SysOps who aren't afraid to let people see a "bad word" on their board.  *
* (And I say afraid literally. It is not a challenge to their machismo,    *
* just that for some reason they can't handle the word.)This version is the*
* first to come out with Source Code. Version 1.0 was in Turbo Pascal, and *
* I ended up not liking the final product. You will find version 2 much    *
* MORE enjoyable, I hope. Feel Free to send any comments or view the latest*
* in Piss-O-Rama upadates on my board.	I hereby declare this software     *
* TOTALLY public-domain. (Especially since I used skeleton.c!)             *
*                                                                          *
*                      The Blue Demesnes BBS                               *
*                      San Francisco, California                           *
*              (415)-333-1982                                              *
*              Node 4552 of WWIVnet  (For Now....)                         *
***************************************************************************/

/*
 * Actually, this is version 3.0... now in JS rather than C
 * Translated by Stephen Hurd (aka: Deuce)
 */

var game_dir='.';
try { throw barfitty.barf(barf) } catch(e) { game_dir=e.fileName }
game_dir=game_dir.replace(/[\/\\][^\/\\]*$/,'');
game_dir=backslash(game_dir);

/* Now, the Piss-O-Rama variables... */

var	dick_size=0,	/* What kind of a punch he packs... */
	arm_strength=0,	/* Obvious, I hope */
	will_power=0,	/* How bad he wants to go */
	beers=0,		/* How many beers he has drank */
	ptsleft=50;		/* How many points left the user has */

function piss()
{
	var phases=new Array("", "New", "Quarter", "Half", "Full");
	var direcs=new Array("", "North", "South", "East", "West");
	var speeds=new Array("", "Breeze", "Gust", "Gale", "Tsunami");
	var sizes=new Array("", "Thimble", "2-Liter Bottle", "Gallon Milk-Jug", "55-Gallon Drum");
	var attempt,
		arc,			/* The Arc which the user picks */
		phase_of_moon,	/* The Tidal effects on the piss-arc */
		wind_direction,	/* Hope you don't piss in the wind! */
		wind_speed,		/* If you are, hope it ain't going fast! */
		temperature,	/* The colder it is... */
		bladder_size,	/* How big the pouch be = pressure */
		max_arc,		/* Maximum Arc based on dicksize vs
						 * armstrength */
		concentration,	/* What's on your mind? */
		playboy_factor,	/* Random event causing physical disorder */
		drunk,			/* How drunk he is (affects aim) */
		distance;		/* How far */

	for(attempt=0; attempt<=2; attempt++) {
		phase_of_moon=random(4)+1;
		wind_direction=random(4)+1;
		wind_speed=random(4)+1;
		temperature=random(100)+1;
		bladder_size=random(4)+1;
		max_arc = (arm_strength * 3) - (dick_size);
		concentration = random(100) + 1;
		playboy_factor = random(2);
		drunk = (beers / 10);

		printf("\r\n\r\n		Status Screen For Attempt #%d\r\n\r\n", attempt + 1);
		printf("Dick Size     : %-7d               Arm Strength   : %-2d\r\n",dick_size, arm_strength);
		printf("Will Power    : %-7d               Beers          : %-2d\r\n", will_power, beers);
		printf("Phase Of Moon : %-7s               Wind Direction : %-5s\r\n", phases[phase_of_moon], direcs[wind_direction]);
		printf("Temperature   : %-7d               Wind Speed     : %-7s\r\n", temperature, speeds[wind_speed]);
		printf("Concentration : %-7d               Bladder Size   : %-15s\r\n", concentration, sizes[bladder_size]);

		if (max_arc < 0)
			max_arc = 0;
		do {
			printf("\r\n\r\nEnter Arc [0-90] (Max Arc = %d) : ", max_arc);
			arc=parseInt(console.getstr('', 2));
		} while (isNaN(arc) || arc > max_arc || arc < 0);
		distance = beers * 2;
		if ((arc < 50) && (arc > 30)) {
			distance += random(20);
		} else {
			distance -= random(20);
		}
		distance += will_power + beers;
		if ((phase_of_moon == 3) || (phase_of_moon == 4)) {
			distance -= random(20);
		} else {
			distance += random(20);
		}
		switch (wind_direction) {
			case 1:			/* North */
				distance /= 5;
				break;
			case 2:			/* South */
				distance += wind_speed * 5;
				break;
			case 3:			/* East */
			case 4:			/* West */
				distance -= wind_speed * 3;
				break;
		}
		if (temperature <= 60)
			distance += (temperature / 2);
		distance += bladder_size * 2;
		distance += concentration / 10;
		if (playboy_factor) {
			write("\r\n\r\nSomeone threw a Playboy out on the field.");
			write("\r\nYou get a stiffy. -30 your distance.\r\n\r\n");
			distance -= 30;
		}
		drunk = beers * 2;
		if (drunk >= 40) {
			write("\r\n\r\nYou are really drunk, and it has affected your aim.");
			write("\r\n -30 your distance.\r\n\r\n");
			distance -= 30;
		}
		if (distance < 0)
			distance = 1;
		printf("\r\n\r\nYou Have Pissed %d meter(s). \r\n\r\n", distance);

		var f=new File(game_dir+"best.pis");
		var highscore;
		if(f.exists) {
			if(f.open("r")) {
				highscore=f.iniGetObject(null);
				f.close();
			}
		}
		else {
			highscore=new Object();
			highscore.comment="Noone has beat the high score yet.";
			highscore.oldscore=10;
			highscore.oldname="The Blue Adept";
			if(f.open("w+")) {
				f.iniSetObject(null, highscore);
				f.close();
			}
		}

		if (distance > highscore.oldscore) {
			write("\r\n\r\nYou Got The High Score!\r\n\r\n");
			write("Please Enter A Comment:\r\n");
			highscore.comment=console.getstr('',159);
			if(user.name==undefined || user.name=='') {
				write("Please Enter Your Name: ");
				highscore.oldname=console.getstr('',40);
			}
			else
				highscore.oldname=user.name;
			highscore.oldscore=distance;
			if(f.open("w+")) {
				f.iniSetObject(null, highscore);
				f.close();
			}
		}
		write("\r\n\r\nCurrent High Score : \r\n");
		printf("%s pissing in at %d meters!\r\n\r\n", highscore.oldname, highscore.oldscore);
		printf("He had This to say about his victory:\r\n%s\r\n", highscore.comment);

		console.pause();

	}
}

function edit(ask)
{
	write("[Enter #]: ");
	var ret = parseInt(console.getstr('', 3));
	if ((ret >= 0) && (ret <= ptsleft)) {
		if(isNaN(ret))
			ret=0;
		ptsleft -= ret;
		return (ret);
	} else {
		if ((ret < 0) && ((ret * -1) < ask)) {
			ptsleft += (ret * -1);
			return (ret);
		} else {
			writeln("");
			return (0);
		}
	}
}


function mainmenu()
{
	var ch;
	var ab=false;
	do {
		console.clear();
		writeln("");
		writeln("");
		write("		Piss-O-Rama - Main Menu\r\n\r\n");
		printf("                  Points Left : %d\r\n\r\n", ptsleft);
		printf("     D)ick Size      (Currently %-2d)\r\n", dick_size);
		printf("     A)rm Strength   (Currently %-2d)\r\n", arm_strength);
		printf("     W)ill Power     (Currently %-2d)\r\n", will_power);
		printf("     B)eers          (Currently %-2d)\r\n", beers);
		write("     P)iss\r\n");
		write("     I)nstructions\r\n");
		write("     Q)uit to the BBS\r\n\r\n");
		write("Your Command : ");
		ch = console.getkeys("QIPBWAD");
		switch (ch) {
		case 'Q':
			ab = true;
			break;
		case 'D':
			write("\r\n\r\nInput Integer to Add to Dick Size. (negative to subtract)\r\n");
			dick_size += edit(dick_size);
			break;
		case 'A':
			write("\r\n\r\nInput Integer to Add to Arm Strength. (negative to subtract)\r\n");
			arm_strength += edit(arm_strength);
			break;
		case 'W':
			write("\r\n\r\nInput Integer to Add to Will Power. (negative to subtract)\r\n");
			will_power += edit(will_power);
			break;
		case 'B':
			write("\r\n\r\nInput Integer to Add to Beers. (Negative to subtract)\r\n");
			beers += edit(beers);
			break;
		case 'P':
			piss();
			ab = true;
			break;
		case 'I':
			write("\r\n\r\n Instructions for Piss-O-Rama");
			write("\r\n\r\n");
			write(" Ok, in this game, the object is to piss as far as you can. There are\r\n");
			write(" several factors which can alter your performance. Four of them are\r\n");
			write(" configurable by you. They are:\r\n");
			write("\r\n");
			write(" Dick Size    -   The heavier the meat, the more punch it packs.\r\n");
			write(" Arm Strength -   If less than dick size, you won't get a good arc.\r\n");
			write(" Will Power   -   How bad you need/want to go.\r\n");
			write(" Beers        -   The more you drink, the more you gotta go. Be careful,\r\n");
			write("	           it can affect your aim.\r\n");
			write(" Arc          -   The Arc you choose to piss at.\r\n");
			writeln("");
			console.pause();
			writeln("");
			write(" Now, there are several features which are random on every try , of\r\n");
			write(" which you get three per run. They are:\r\n");
			writeln("");
			write(" Phase of the moon - The tidal effects on your urination.\r\n");
			write(" Wind Direction    - Hope you aren't pissing in the wind.\r\n");
			write(" Wind Speed        - If you are, hope it isn't blowing hard.\r\n");
			write(" Temperature       - The colder it is, the worse you have to go.\r\n");
			write(" Bladder Size      - Bigger Bladder = More Pressure.\r\n");
			write(" Concentration     - Is your mind on this, or how badly you have to go #2?\r\n");
			write(" Playboy Factor    - A Random event causing a physiological disorder.\r\n");
			write(" Drunkeness        - Depends on the Beers variable, and can affect aim.\r\n");
			writeln("");
			writeln("");
			write(" You start out with 50 points, so use them wisely.\r\n");
			writeln("");
			writeln("");
			writeln("");
			console.pause();
			break;


		}
	} while (!ab);
}

console.clear();
writeln("");
writeln("");
writeln("");
write("	             As you pull down your zipper, you enter....\r\n\r\n");
write("		          	The Piss-O-Rama!\r\n");
write("				   Version 2.0\r\n\r\n");
write("			By: The Blue Adept #1@4552\r\n\r\n\r\n");
console.pause();
mainmenu();
console.clear();
writeln("");
writeln("");
write("Thanks for Playing the Piss-O-Rama!\n\n");

