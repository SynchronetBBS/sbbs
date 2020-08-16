/* $Id: kingdom.js,v 1.1 2009/11/28 05:57:14 cyan Exp $

   Kingdom Gold - Ported to JavaScript by Randolph E. Sommerfeld <sysop@rrx.ca>

   A simple game based on 'Sumer Game' or 'Hamurabi'
   This software is released into the public domain.
*/

/* Constants */
const QUIT = -1;
const CR = -2;

console.clear();

printf("\r\n\r\n");
printf("\1gWelcome to \1h\1yKingdom Gold\1n\1g v0.9b, ");
printf("a simple door game for Synchronet BBS.\r\n\r\n");

printf("This game was ported by Randy E. Sommerfeld from original ");
printf("Commodore 64 software\r\n");
printf("found to be in the public domain.  Hence, this game is also ");
printf("released into the\r\n");
printf("public domain.  The original 'Kingdom' game was released in ");
printf("1973 by T. Voros\r\n");
printf("and L. Schneider for the 8K Commodore PET.  That game was ");
printf("based on a game\r\n");
printf("called 'Sumer Game', created by Richard Merrill in 1969, and ");
printf("is one of the\r\n");
printf("oldest computer games in existence.\r\n");

printf("\r\n");
printf("You can press 'Q' at any prompt to exit the game.\r\n");

printf("\r\n\r\n");

var instr=(console.noyes("\1n\1mWould you like instructions"));
if (!instr)
	instructions();

/* Globals */
citizens = random(75)+75;
acres = random(250)+250;
bushels = random(3000)+2000;
year = 0;
price = 3;
/* End Globals */

var game_over = false;

while (!game_over) {
	console.clear();
	printf("\r\n\r\n\1n\1g");

	if (!year)
		printf("In the beginning of your reign:");
	else
		printf("Report for \1h\1gYear %d\1n", year);
	printf("\r\n\r\n");

	printf("\1n\1g    Citizens in your kingdom: \1h\1y%d\r\n", citizens);
	printf("\1n\1g         Acres of land owned: \1h\1y%d\r\n", acres);
	printf("\1n\1g Bushels of grain in storage: \1h\1y%d\r\n", bushels);

	printf("\r\n");

	printf("\1n\1gThe price of land is \1h\1y%d\1n\1g ", price);
	printf("bushels per acre.\r\n");

	printf("\r\n");

	var maxacres = Math.round((bushels/price)-1);
	if (maxacres < 0)
		maxacres = 0;
	var buyacres = get_input("\1n\1mHow many acres to buy "
		+ "[\1h\1y" + maxacres + "\1n\1m]? ", maxacres);
	if (buyacres == QUIT)
		break;
	acres += buyacres;
	bushels -= buyacres * price;

	if (buyacres <= 0) {
		var sellacres = get_input("\1n\1mHow many acres to sell "
			+ "[\1h\1y" + acres + "\1n\1m]? ",acres);
		if (sellacres == QUIT)
			break;
		acres -= sellacres;
		bushels += sellacres * price;
	}

	printf("\r\n");

	if ( (buyacres > 0) || (sellacres > 0) ) {
		printf("\1n\1h\1cTransaction Results:\1n\r\n");
		printf("\1n\1g    You now own \1h\1y%d\1n\1g acres of land, and "
			,acres);
		printf("\1h\1y%d\1n\1g bushels of grain.\r\n",bushels);
		printf("\r\n");
	}

	var feed = get_input("\1n\1mHow many bushels of grain to feed to \1h\1y"
		+ citizens + "\1n\1m people [\1h\1y" + bushels + "\1n\1m]? ",bushels);
	if (feed == QUIT)
		break;
	bushels -= feed;

	var bushels_for_seeding = Math.round(bushels / 3);
	var citizens_for_seeding = Math.round(citizens / 2);
	var max_seed = citizens_for_seeding;
	if (bushels_for_seeding < citizens_for_seeding)
		max_seed = bushels_for_seeding;
	max_seed = Math.round(max_seed-1);
	if (max_seed < 0)
		max_seed = 0;
	printf("\r\n");
	seedacres = get_input("\1n\1mHow many acres do you wish to seed [\1h\1y"
		+ max_seed + "\1n\1m]? ",max_seed);
	if (seedacres == QUIT)
		break;
	bushels -= seedacres * 3;

	printf("\r\n");
	printf("\1n\1h\1cProduction Costs:\1n\r\n");
	printf("\1g    A total of \1n\1h\1y%d\1n\1g ", seedacres);
	printf("acres of land have been planted.\r\n");
	printf("\r\n");

	console.pause();

	year++;

	var arrived = 0;
	var starving = Math.round(citizens - (feed/10));
	var died = 0;

	if (starving > 0) {
		food_riots(starving);
	} else if (starving < 0) {
		arrived += food_surplus(starving);
		starving = 0;
	}

	if ((random(100) - 10) <= 0)
		died += plague();

	if ((random(100) - 10) <= 0)
		died += huns();
		
	if ((random(100) - 15) <= 0)
		arrived += border_expansion();

	citizens = citizens + arrived - died - starving;

	if (citizens <= 0) {
		console.clear();
		printf("\r\n\r\n");
		printf("\1h\1rDisaster!\1n\r\n\r\n");
		printf("\1gThe population went zip!\r\n");
		break;
	}

	var change = arrived - died - starving;

	console.clear();
	printf("\r\n\r\n\1n\1g");

	printf("\1n\1h\1cVital Statistics:\1n\r\n");
	printf("\1g    Births and Immigration: \1n\1h\1y%d\1n\r\n", arrived);
	printf("\1g          Starved to Death: \1n\1h\1y%d\1n\r\n", starving);
    printf("\1g    Died of Natural Causes: \1n\1h\1y%d\1n\r\n", died);
	printf("\r\n");
	printf("\1g             Census Change: \1n\1h");
	if (change >= 0)
		printf("\1g+");
	else
		printf("\1r");
	printf("%d\1n\r\n\r\n",change);
	console.pause();

	if ((random(100) - 10) <= 0)
		theft();

	if ((random(100) - 10) <= 0)
		earthquake();

	if ((random(100) - 15) <= 0)
		grain_shipment();

	if ((random(100) - 15) <= 0)
		drought();

	if ((random(100) - 15) <= 0)
		rain();

	var hbush = price * seedacres;
	var rats = Math.round(random(hbush/2));
	var hnet = hbush - rats;

	if (hnet > 0) {
		printf("\1n\1h\1cHarvest Results:\1n\r\n");
		printf("\1g           The Harvest Was: \1n\1h\1y%d\1n\1g ",price);
		printf("bushels per planted acre.\r\n");
		printf("\1g            For a Total of: \1n\1h\1y%d\1n\1g bushels.\r\n"
			,hbush);
		printf("\1g              Lost to Rats: \1n\1h\1y%d\1n\1g bushels.\r\n"
			,rats);
		printf("\r\n");
		printf("\1g     Total Net Harvest Was: \1n\1h\1g%d\1n\1g bushels.\r\n"
			,hnet);

		bushels += hnet;

		console.pause();
	}
}

printf("\1n\1gYour reign lasted \1h\1y%d\1n\1g year", year);
if (year != 1)
	printf("s");
printf(".\r\n\r\n");
exit();

/* End of main() */

function border_expansion() {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1gBorder Expansion\1n\r\n\r\n");

	var cit_gain = random(20)+10;
	var acre_gain = random(citizens+250)+50;

	acres += acre_gain;

	printf("\1n\1gPopulation increase! ");
	printf("You gained %d citizens and %d acres of land.\r\n"
		,cit_gain,acre_gain);

	printf("\r\n\r\n");
	console.pause();

	return(cit_gain);
}

function drought() {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1rDrought\1n\r\n\r\n");

	price = random(3)+7;

	printf("\1n\1g%d weeks of no water ",0);
	printf("and the river is dry like the moat!\r\n");

	printf("\r\n\r\n");
	console.pause();
}

function earthquake() {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1rEarthquake!\1n\r\n\r\n");

	var destroyed = Math.round(random(acres/10) + (acres/20));

	printf("\1n\1g%d acres of land destroyed.\r\n",destroyed);

	printf("\r\n\r\n");
	console.pause();
}

function food_riots(starving) {
	if ( (random(100) - (5*(starving-2))) > 0 )
		return 0; /* No Riot */

	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1rFood Riots\1n\r\n\r\n");

	var lost = Math.round(random( (starving * bushels) / (2 * citizens) ));
	bushels -= lost;

	printf("\1n\1g%d bushels of food lost.\r\n",lost);

	printf("\r\n\r\n");
	console.pause();
}

function food_surplus(starving) {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1gFood Surplus!\1n\r\n\r\n");

	var increase = Math.round(random(3 - (starving/2)));

	printf("\1gPopulation increase of \1h\1y%d\1n\1g.\r\n", increase);

	printf("\r\n\r\n");
	console.pause();

	return(increase);
}

function grain_shipment() {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1gGrain Shipment Arrives!\1n\r\n\r\n");

	var shipment = Math.round(random(100+(acres/100))+500);

	printf("The shipment contains %d bushels of grain.\r\n",shipment);

	bushels += shipment;
	price = random(2)+1;

	printf("\r\n\r\n");
	console.pause();
}

function huns() {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1rHun Pillage!\1n\r\n\r\n");

	var lost_cits = Math.round(random(citizens/5) + (citizens/5));
	var lost_bush = Math.round(random(bushels/20) + (bushels/20));
	var lost_acre = Math.round(random(acres/50));

	bushels -= lost_bush;
	acres -= lost_acre;

	printf("\1n\1g%d citizens killed, %d bushels taken, %d acres destroyed.\r\n"
		,lost_cits,lost_bush,lost_acre);

	printf("\r\n\r\n");
	console.pause();

	return(lost_cits);
}

function plague() {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1rPlague\1n\r\n\r\n");

	var died = Math.round((citizens/3) + random((citizens/2)+2));

	printf("\1n\1gIn memory of the %d citizens who died in the plague.\r\n",died);

	printf("\r\n\r\n");
	console.pause();

	return(died);
}

function rain() {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1gRain\1n\r\n\r\n");

	price = random(4)+3;

	printf("\1n\1gYour kingdom is blessed with %d days of rain.\r\n",price-3);

	printf("\r\n\r\n");
	console.pause();
}

function theft() {
	console.clear();
	printf("\r\n\r\n");

	printf("\1h\1rTheft!\1n\r\n\r\n");

	var stolen = Math.round(random(bushels/20) + (bushels/20));

	printf("\1n\1g%d bushels of grain were stolen!\r\n",stolen);

	bushels -= stolen;

	printf("\r\n\r\n");
	console.pause();
}

function get_input(str,max) {
	var valid_keys = "0123456789";
	var input_is_valid = false;
	var myinput = "";
	var myinput_int = 0;
	while (!input_is_valid) {
		printf("%s", str);
		myinput = console.getstr("", 5);
		if (myinput == "")
			return max;
		var fc = myinput[0].toUpperCase();
		if ( (fc == "Q") || (fc == "X") )
			return QUIT;
		for (k in myinput) {
			var myregexp = new RegExp('['+ myinput[k] +']', "g");
			if (!valid_keys.match(myregexp)) {
				break;
			} else if (k == myinput.length-1) {
				myinput_int = parseInt(myinput);
				if ( (myinput_int < max) && (myinput_int > -1) )
					input_is_valid = true;
			}
		}
	}
	return parseInt(myinput_int);
}

function instructions() {
	console.clear();
	printf("\r\n\r\n\1n\1g");
	printf("In this game, you control a kingdom with three major resources:");
	printf("\r\n    \1h\1yBUSHELS\1n\1g of food, used to feed your citizens ");
	printf("and barter with other kingdoms\r\n");
	printf("            to buy more acres of land.");
	printf("\r\n      \1h\1yACRES\1n\1g of land, which you will use to grow ");
	printf("bushels of food.  Land can also\r\n");
	printf("            be traded to other kingdoms for bushels.");
	printf("\r\n   \1h\1yCITIZENS\1n\1g, who work your acres of land.");
	printf("\r\n\r\n");
	printf("The object of the game is twofold: retain as many citizens and ");
	printf("resources as\r\n");
	printf("possible while making your kingdom last as long as it possibly ");
	printf("can.  Natural\r\n");
	printf("disasters, barbarian raids, fluctuating land prices, and citizen ");
	printf("revolts will\r\n");
	printf("make this somewhat difficult.\r\n");
	printf("\r\n");
	printf("Numbers to remember are that it takes a minimum of 10 bushels of ");
	printf("food to feed\r\n");
	printf("one citizen per year.  Optimally, a citizen should be fed 20 ");
	printf("bushels of food\r\n");
	printf("per year to encourage population growth.  An acre requires a ");
	printf("minimum of 3\r\n");
	printf("bushels to 'seed' properly for growth, and a single citizen is ");
	printf("capable of\r\n");
	printf("seeding 2 acres of land per year.  Starving your population will ");
	printf("cause riots.\r\n");
	printf("\r\n");
	printf("Good luck!\r\n\r\n\1p");
}

