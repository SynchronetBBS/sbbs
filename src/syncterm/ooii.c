/*
 * This code handles Operation Overkill ][ terminal codes
 */

#include <string.h>
#include <genwrap.h>
#include <xpbeep.h>
#include <ciolib.h>
#include <cterm.h>
#include <time.h>
#include "ooii_cmenus.h"
#include "ooii_bmenus.h"
#include "ooii_logons.h"
#include "ooii_sounds.h"

static unsigned char stormColors[7]={7,12,8,15,14,13,1};

static char diseases[11][14]=    // #,str length

{"None",                 // 0
 "Pneumonia",            // 1
 "Malaria",              // 2
 "Yellow Fever",         // 3
 "Hydrocism",            // 4
 "Staph",                // 5
 "Rabies",               // 6
 "Polio",                // 7
 "Black Plague",         // 8
 "Delyria",              // 9
 "Poison"};              // 10


static char weapons[27][20]=

{"ú",                  // 0
 "Steel Chain",        // 1
 "Salanger",           // 2
 "Natase",             // 3
 "TransAxe",           // 4	{  Power Pack   }
 "Electric Sword",     // 5	{ Power Pack   }
 "Tevix-Bahn",         // 6     { Cirze Supply }
 "Xendrix",            // 7	{ NOT FOR SALE!  Infinex }
 "Trilasm",            // 8	{ ZZETs        }
 "Exavator",           // 9	{ ZZETs        }
 "Raxhaven",           // 10	{ Power Pack   }
 "SanPlizer",          // 11	{ Power Pack   }
 "Devastator",         // 12	{ Cirze Supply }
 "Z-Tempest",          // 13	{ Cirze Supply }
 "Herculean",          // 14	{ NOT FOR SALE!  Infinex }
 "Trignet",            // 15	{ NOT FOR SALE!  Firenet }
 "Extinguisher",       // 16
 "Shovel",             // 17
 "Nail Gun",           // 18
 "Jack Knife",         // 19
 "Metal Shears",       // 20
 "Wall Hook",          // 21
 "Hoist",              // 22
 "Propeller",          // 23
 "Pickaxe",            // 24
 "Sledge Hammer",      // 25
 "Hydra-Jack" };       // 26



static char ammos[6][20]=

{"ú",                  // 0
 "ZZETs",              // 1	{  (Trilasm/Exavator) }
 "Power Pack",         // 2	{  (SanPlizer/RaxHaven/E-Sword/)        }
 "Cirze Supply",       // 3	{  (Z-Tempest/Devastator/Tevix-Bahn)    }
 "Infinex",            // 4	{  NOT FOR SALE!! (Herculean/Xendrix)   }
 "Firenet" };          // 5	{  NOT FOR SALE!! (Trignet)             }


static char equips[46][20]=

{"ú",                   // 0
 "Rations",             // 1
 "Hemp Rope",           // 2
 "Silencer",            // 3
 "Transmitter",         // 4
 "Summoner",            // 5
 "MedPack",             // 6
 "Thumpers",            // 7
 "Galacticom",          // 8
 "Gasmask",             // 9
 "Duroplast",           // 10
 "Aerial Scan",         // 11
 "VacPack",             // 12
 "Telegrasp",           // 13	{ NOT FOR SALE! }
 "Red Key",             // 14	{ NOT FOR SALE! }
 "Blue Key",            // 15	{ NOT FOR SALE! }
 "Green Key",           // 16	{ NOT FOR SALE! }
 "Techkit",             // 17	{ NOT FOR SALE! }
 "Rusted Wire",         // 18	{ NOT FOR SALE! }{Arming Device}
 "Clay Package",        // 19	{ NOT FOR SALE! }{Plutonium}
 "Alarm Clock",         // 20	{ NOT FOR SALE! }{Timer}
 "Broken Keypad",       // 21	{ NOT FOR SALE! }{Keypad}
 "Code Key",            // 22	{ NOT FOR SALE! }{Entry Into AFBase}
 "Alien Device",        // 23	{ NOT FOR SALE! }{Entry Into SpaceShip}
 "Trikorder",           // 24	{ NOT FOR SALE! }
 "Remote Control",      // 25	{ NOT FOR SALE! }
 "Human Skull",         // 26	{ NOT FOR SALE! }
 "Floppy Disk",         // 27	{ NOT FOR SALE! }{Entry to Computer Silo}
 "Alien Skull",         // 28	{ NOT FOR SALE! }
 "Hydrite Egg",         // 29	{ NOT FOR SALE! }
 "Lazer Key",           // 30	{ NOT FOR SALE! }{Entry thru AFB doorways }
 "Landmine",            // 31	{ For Sale Only In AirForce Base}
 "Zimmer",              // 32	{ For Sale Only In AirForce Base}
 "Re-Bar Scrap",        // 33
 "Door Hinge",          // 34
 "Broken Wrench",       // 35
 "Steel Coupling",      // 36
 "Pipe Fitting",        // 37
 "Rotted Cabling",      // 38
 "Pipe Die",            // 39
 "Chrome Pipe",         // 40
 "Gate Valve",          // 41
 "Butane Tank",         // 42
 "Drill Bit",           // 43
 "I-Bar Clamp",         // 44
 "Copper Tubing"};      // 45



static char suits[4][20]=

{"ú",                   // 0
 "Environmental Suit",  // 1
 "Combat Suit",         // 2
 "Anti-G Suit"};        // 3


static char armors[13][20]=

{"ú",                   // 0
 "Leather",             // 1
 "HeXonium",            // 2
 "Laser Vest",          // 3
 "Shezvarin",           // 4
 "Reflecto",            // 5
 "Mudrane Sheathing",   // 6
 "Breonesin Sheeting",  // 7
 "Barbed Wire",         // 8
 "Asbestos Pads",       // 9
 "Cuprid Vest",         // 10
 "Sheet Metal",         // 11
 "PolyCarbon"};         // 12


static void term_gotoxy(int x, int y)
{
	char	ansistr[32];

	sprintf(ansistr, "\x1b[%d;%dH", y, x);
	cterm_write(ansistr, strlen(ansistr), NULL, 0, NULL);
}

static void term_clearscreen(void)
{
	cterm_write("\x1b[0m\x1b[2J\x1b[H", 11, NULL, 0, NULL);
}

const int 	term_colours[8]={0,4,2,6,1,5,3,7};
static void term_setattr(int attr)
{
	char str[16];
	int fg,ofg;
	int bg,obg;
	int bl,obl;
	int br,obr;
	int oa;

	str[0]=0;
	if(cterm.attr==attr)
		return;

	bl=attr&0x80;
	bg=(attr>>4)&0x7;
	fg=attr&0x07;
	br=attr&0x08;

	oa=cterm.attr;
	obl=oa&0x80;
	obg=(oa>>4)&0x7;
	ofg=oa&0x07;
	obr=oa&0x08;

	strcpy(str,"\033[");
	if(obl!=bl) {
		if(!bl)
			strcat(str,"25;");
		else
			strcat(str,"5;");
	}
	if(br!=obr) {
		if(br)
			strcat(str,"1;");
		else
			strcat(str,"22;");
	}
	if(fg!=ofg)
		sprintf(str+strlen(str),"3%d;",term_colours[fg]);
	if(bg!=obg)
		sprintf(str+strlen(str),"4%d;",term_colours[bg]);
	str[strlen(str)-1]='m';
	cterm_write(str, strlen(str), NULL, 0, NULL);
}

static void readInPix(char codeCh, int ooii_mode) {
	int fptr;

	term_clearscreen();
	term_gotoxy(1,1);
	switch ((char)codeCh) {
		// complex pictures
		case 'A':
			fptr=0;	/* Main = 0 */
			break;
		case 'B':
			fptr=1;	/* Barracks = 1 */
			break;
		case 'C':
			fptr=3;	/* Head-quarters = 3 */	
			break;
		case 'D':
			fptr=2;	/* Communications = 2 */
			break;
		case 'E':
			fptr=4;	/* Wastelands = 4 */
			break;


		// base pictures
		case 'F':
			fptr=0;	/* Main menu = 0 */
			break;
		case 'G':
			fptr=1;	/* Barracks = 1 */
			break;
		case 'H':
			fptr=3;	/* Storage = 3 */
			break;
		case 'I':
			fptr=5;	/* Tech Centre = 5 */
			break;
		case 'J':
			fptr=2;	/* Control Room = 2 */
			break;
		case 'K':
			fptr=4;	/* Radio Room = 4 */
			break;

		// logon pictures
		case '0':
			fptr=xp_random(10);
			break;
	}

	if (codeCh>='A' && codeCh<='E')
		cterm_write(ooii_cmenus[ooii_mode-1][fptr], strlen(ooii_cmenus[ooii_mode-1][fptr])-1, NULL, 0, NULL);
	else if (codeCh>='F' && codeCh<='K')
		cterm_write(ooii_bmenus[ooii_mode-1][fptr], strlen(ooii_bmenus[ooii_mode-1][fptr])-1, NULL, 0, NULL);
	else if (codeCh=='0')
		cterm_write(ooii_logon[ooii_mode-1][fptr], strlen(ooii_logon[ooii_mode-1][fptr])-1, NULL, 0, NULL);

	/* We don't overwrite the status line, so we don't need to redraw it */
	/* statusLine(); */

	return;
}

static int readInText(char *codeStr) {
	char *origCodeStr=codeStr;

	switch ((char)codeStr[0]) {
		case '1':
			term_setattr(BROWN);
	    	cterm_write("You mosey on over to the bar and take a seat on a scuffed barstool.  The\r\n", -1, NULL, 0, NULL);
	    	cterm_write("slighty deformed keeper grunts, \"Woth ja leeke?\"  A galacticom on the top\r\n", -1, NULL, 0, NULL);
	    	cterm_write("shelf behind him translates his jumble into \"What would you like?\"  You\r\n", -1, NULL, 0, NULL);
	    	cterm_write("start to wonder what ever happened to the old standard human language.\r\n\r\n", -1, NULL, 0, NULL);
	    	term_setattr(LIGHTGREEN);
	    	cterm_write("\"Give me the House Special,\" you smirk.\r\n\r\n", -1, NULL, 0, NULL);
	    	term_setattr(GREEN);
	    	cterm_write("The bartender stares at you with one eye and then drools in agreement.  He\r\n", -1, NULL, 0, NULL);
	    	cterm_write("vanishes from behind the bar and pops back up a few seconds later.  He\r\n", -1, NULL, 0, NULL);
	    	cterm_write("slides you the mysterious drink as he begins to chuckle.  White smoke\r\n", -1, NULL, 0, NULL);
	    	cterm_write("slithers from the bubbling green slime over onto the bar counter.\r\n\r\n", -1, NULL, 0, NULL);
	    	term_setattr(LIGHTGREEN);
	    	cterm_write("\"What do I owe you?\" you inquire to the barman.  He promptly responds\r\n", -1, NULL, 0, NULL);
	    	cterm_write("with that same smirk on his face, \"Youkla telph me.\"\r\n", -1, NULL, 0, NULL);
	    	break;
	}

	return(codeStr-origCodeStr);;
}

static void getBlock(char **codeStr, char *menuBlock)
{
	menuBlock[0]=0;
	if(**codeStr=='_')
		(*codeStr)++;
	while(**codeStr != '_' && **codeStr != '|') {
		*(menuBlock++) = *((*codeStr)++);
		*menuBlock=0;
	}
}

static void strljust(char *buf, size_t len, char pad)
{
	size_t buflen=strlen(buf);

	if(buflen < len) {
		memset(buf+buflen, pad, len-buflen);
		buf[len]=0;
	}
}

static void strrjust(char *buf, size_t len, char pad)
{
	size_t buflen=strlen(buf);

	if(buflen < len) {
		memmove(buf+(len-buflen), buf, buflen+1);
		memset(buf, pad, len-buflen);
	}
}

static int readSmallMenu(char *codeStr) {
	#define MultA 2
	#define MultE 3
	#define MultS 3
	#define MultW 2

	long aPrice[5]={0,4500,14500,29000,42500};
	long ePrice[13]={0,50,200,300,500,800,1000,1500,1750,2000,3000,5000,15000};
	long sPrice[3]={0,4500,13500};
	long wPrice[15]={0,700,4250,9500,18000,32500,55750,0,7500,12000,19250,30500,41000,64750,0};

	char tempBlock[255];
	char menuBlock[255];
	int  yy,er;
	int  zz;
	char *origCodeStr=codeStr;
	char buf[256];

	switch ((char)codeStr[0]) {
		case 'a':
			term_setattr(LIGHTGREEN);
			cterm_write("Supply Room\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);
			cterm_write("~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);
			cterm_write("-[A]  Armor\r\n", -1, NULL, 0, NULL);
			cterm_write("-[E]  Equipment\r\n", -1, NULL, 0, NULL);
			cterm_write("-[P]  Power Supply/Ammo\r\n", -1, NULL, 0, NULL);
			cterm_write("-[S]  Suits/Outfits\r\n", -1, NULL, 0, NULL);
			cterm_write("-[W]  Weapons\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[I]  Supply Inventory\r\n", -1, NULL, 0, NULL);
			cterm_write("-[C]  Check\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Back to HQ\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Supply Room>  é ", -1, NULL, 0, NULL);
			break;

		case 'b':
			term_setattr(WHITE);
			cterm_write("Recruit Chambers\r\n", -1, NULL, 0, NULL);
			term_setattr(RED);
			cterm_write("~~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);
			cterm_write("-[A]  Adjust Specs\r\n", -1, NULL, 0, NULL);
			cterm_write("-[C]  Check Status\r\n", -1, NULL, 0, NULL);
			cterm_write("-[M]  Morgue Listing\r\n", -1, NULL, 0, NULL);
			cterm_write("-[R]  Roster Status\r\n", -1, NULL, 0, NULL);
			cterm_write("-[T]  Training Room\r\n", -1, NULL, 0, NULL);
			cterm_write("-[U]  Use Equipment\r\n", -1, NULL, 0, NULL);
			cterm_write("-[V]  View Recruit\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Exit to Barracks\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Chambers>  é ", -1, NULL, 0, NULL);
			break;

		case 'c':
			term_setattr(LIGHTMAGENTA);
			cterm_write("Adjust Specifications:\r\n", -1, NULL, 0, NULL);
			term_setattr(MAGENTA);
			cterm_write("~~~~~~~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTGREEN);
			cterm_write("-[A]  Ansi Color On/Off\r\n", -1, NULL, 0, NULL);
			cterm_write("-[C]  Combat Action/Stat-Random\r\n", -1, NULL, 0, NULL);
			cterm_write("-[D]  Disable/Enable Ansiterm\r\n", -1, NULL, 0, NULL);
			cterm_write("-[E]  Expert Menus On/Off\r\n", -1, NULL, 0, NULL);
			cterm_write("-[F]  Fight-Text Delay Change\r\n", -1, NULL, 0, NULL);
			cterm_write("-[P]  Password Change\r\n", -1, NULL, 0, NULL);
			cterm_write("-[Q]  Quote for Combat\r\n", -1, NULL, 0, NULL);
			cterm_write("-[S]  Scanner Scroll/Stationary\r\n", -1, NULL, 0, NULL);
			cterm_write("-[T]  Terminate Yourself\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Exit\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Specs>  é ", -1, NULL, 0, NULL);
			break;

		case 'd':
			codeStr++;

			term_setattr(LIGHTMAGENTA);
			cterm_write("Storage Bank:\r\n", -1, NULL, 0, NULL);
			term_setattr(MAGENTA);
			cterm_write("~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(WHITE);

			cterm_write("Crystals in Savings : ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			strcat(menuBlock,"\r\n");
			cterm_write(menuBlock, -1, NULL, 0, NULL);

			cterm_write("Crystals on Loan    : ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			strcat(menuBlock,"\r\n");
			cterm_write(menuBlock, -1, NULL, 0, NULL);

			cterm_write("\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			cterm_write("-[D]  Deposit\r\n", -1, NULL, 0, NULL);
			cterm_write("-[W]  Withdraw\r\n", -1, NULL, 0, NULL);
			cterm_write("-[L]  Make a Loan\r\n", -1, NULL, 0, NULL);
			cterm_write("-[P]  Pay off Loan\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz>0)
				cterm_write("-[S]  Squadron Account\r\n", -1, NULL, 0, NULL);

			cterm_write("-[T]  Transfer Crystals\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Exit to Chambers\r\n\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			term_setattr(WHITE);

			cterm_write("You have ", -1, NULL, 0, NULL);
			cterm_write(menuBlock, -1, NULL, 0, NULL);
			cterm_write(" crystals onhand.\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Bank>  é ", -1, NULL, 0, NULL);
			break;

		case 'e' :
			codeStr++;

			term_setattr(YELLOW);
			cterm_write("You are now in the Games Room, a delightful place to leisurely relax from\r\n", -1, NULL, 0, NULL);
			cterm_write("the everyday hack n' slash.   A small bar is located on the south wall.\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(WHITE);
			getBlock(&codeStr,menuBlock);
			cterm_write("Games Left :[ ", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);
			cterm_write(menuBlock, -1, NULL, 0, NULL);
			cterm_write("\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTGREEN);

			cterm_write("-[A]  Arm Wrestling\r\n", -1, NULL, 0, NULL);
			cterm_write("-[B]  Barl's Bar\r\n", -1, NULL, 0, NULL);
			cterm_write("-[L]  Loot Crystals\r\n", -1, NULL, 0, NULL);
			cterm_write("-[M]  Meat Darts\r\n", -1, NULL, 0, NULL);
			cterm_write("-[R]  Roach Races\r\n", -1, NULL, 0, NULL);
			cterm_write("-[S]  SharpShooter\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Exit to Barracks\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTBLUE);
			cterm_write("-<Games Room>  é ", -1, NULL, 0, NULL);
			break;

		case 'f' :
			codeStr++;

			term_setattr(WHITE); cterm_write("Sentry Post\r\n", -1, NULL, 0, NULL);
			term_setattr(RED); cterm_write("~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			getBlock(&codeStr,menuBlock);
			yy=atoi(menuBlock);
			if (yy>0) {

				cterm_write("-[O]  Offer Guard Items\r\n\r\n", -1, NULL, 0, NULL);
				cterm_write("-[V]  View Gate Guard\r\n", -1, NULL, 0, NULL);
				cterm_write("-[U]  Upgrade Gate Guard\r\n", -1, NULL, 0, NULL);
			}
			else
				cterm_write("-[R]  Requisition New Guard\r\n", -1, NULL, 0, NULL);

			cterm_write("-[C]  Check Your Status\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Exit to Barracks\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Sentry Post>  é ", -1, NULL, 0, NULL);
			break;

		case 'g':
			codeStr++;

			term_setattr(WHITE); cterm_write("Electronic-Mail\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTGRAY);   cterm_write("~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);

			if (menuBlock[0]>48) {
				term_setattr(YELLOW);
				cterm_write("-[M]  Mass Mail to Squadron\r\n\r\n", -1, NULL, 0, NULL);
			}

			term_setattr(LIGHTCYAN);
			cterm_write("-[R]  Read E-Mail\r\n", -1, NULL, 0, NULL);
			cterm_write("-[S]  Send E-Mail\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Back to Comm. Post\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<E-Mail>  é ", -1, NULL, 0, NULL);
			break;

		case 'h':
			term_setattr(LIGHTCYAN);
			cterm_write("\r\nHelp Information:\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);
			cterm_write("~~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(WHITE);
			cterm_write("-[1]  Scenario\r\n", -1, NULL, 0, NULL);
			cterm_write("-[2]  Character\r\n", -1, NULL, 0, NULL);
			cterm_write("-[3]  Complex\r\n", -1, NULL, 0, NULL);
			cterm_write("-[4]  Wastelands\r\n", -1, NULL, 0, NULL);
			cterm_write("-[5]  Bases\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[6]  Bulletin Info\r\n", -1, NULL, 0, NULL);
			cterm_write("-[7]  Logon News Info\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[0]  Exit\r\n\r\n", -1, NULL, 0, NULL);
			break;

		case 'i':
			codeStr++;

			term_setattr(LIGHTCYAN); cterm_write("Medical Center\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);   cterm_write("~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTGREEN);

			cterm_write("Radiation   :[ ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz<44)
				term_setattr(WHITE);
			else
				term_setattr(LIGHTRED);
			cterm_write(menuBlock, -1, NULL, 0, NULL); cterm_write("%\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTGREEN);
			cterm_write("Hit Points  :[ ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz>5)
				term_setattr(WHITE);
			else
				term_setattr(LIGHTRED);

			cterm_write(menuBlock, -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			cterm_write(" (", -1, NULL, 0, NULL);
			cterm_write(menuBlock, -1, NULL, 0, NULL);
			cterm_write(")\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTGREEN);
			cterm_write("Infectious  :[ ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			term_setattr(WHITE);

			cterm_write(diseases[zz], -1, NULL, 0, NULL); cterm_write("\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);

			cterm_write("-[M]  Medical Treatment\r\n", -1, NULL, 0, NULL);
			cterm_write("-[R]  Radiation Treatment\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[N]  Neutralize Disease\r\n", -1, NULL, 0, NULL);
			cterm_write("-[V]  Vaccinate Disease\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Back to HQ\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Medical Center>  é ", -1, NULL, 0, NULL);
			break;

		case 'j' :
			term_setattr(LIGHTMAGENTA);
			cterm_write("Records Room\r\n", -1, NULL, 0, NULL);
			term_setattr(MAGENTA);   cterm_write("~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTGREEN);
			cterm_write("-[A]  Total Ability\r\n", -1, NULL, 0, NULL);
			cterm_write("-[B]  Bravery\r\n", -1, NULL, 0, NULL);
			cterm_write("-[C]  Combat Accuracy\r\n", -1, NULL, 0, NULL);
			cterm_write("-[E]  Experience\r\n", -1, NULL, 0, NULL);
			cterm_write("-[K]  Total Kills\r\n", -1, NULL, 0, NULL);
			cterm_write("-[O]  Owned Crystals\r\n", -1, NULL, 0, NULL);
			cterm_write("-[P]  Power\r\n", -1, NULL, 0, NULL);
			cterm_write("-[S]  Squadrons\r\n", -1, NULL, 0, NULL);
			cterm_write("-[W]  Wealth\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[H]  Hall of Fame\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Back to HQ\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Records>  é ", -1, NULL, 0, NULL);
			break;

		case 'k':
			codeStr++;

			term_setattr(LIGHTGREEN); cterm_write("Supply Armor\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);  cterm_write("~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);  term_setattr(WHITE);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			cterm_write(armors[zz], -1, NULL, 0, NULL);
			cterm_write(" / ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			cterm_write(menuBlock, -1, NULL, 0, NULL);
			cterm_write(" Hits\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTCYAN);
			cterm_write("-[B]  Buy Armor\r\n", -1, NULL, 0, NULL);
			cterm_write("-[S]  Sell Armor\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Back to Supply Room\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Supply Armor>  é ", -1, NULL, 0, NULL);
			break;

		case 'l':
			codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(LIGHTBLUE);
			cterm_write("Armor Types           Hits     Crystals\r\n", -1, NULL, 0, NULL); term_setattr(CYAN);
			cterm_write("~~~~~~~~~~~           ~~~~     ~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTCYAN);

			for (zz=1;zz<5;zz++) {
				sprintf(buf,"-[%d]  - %s",zz,armors[zz]);
				strljust(buf,23,' ');
				cterm_write(buf, -1, NULL, 0, NULL);

				switch (zz) {
					case 1:
						sprintf(buf,"50");
						break;
					case 2:
						sprintf(buf,"80");
						break;
					case 3:
						sprintf(buf,"110");
						break;
					case 4:
						sprintf(buf,"150");
						break;

				}
				strrjust(buf,3,' ');
				cterm_write(buf, -1, NULL, 0, NULL);


				if (menuBlock[0]==48)
					sprintf(buf,"%ld",aPrice[zz]);
				else
					sprintf(buf,"%ld",aPrice[zz]*MultA);

				strrjust(buf,13,' ');
				cterm_write(buf, -1, NULL, 0, NULL); cterm_write("\r\n", -1, NULL, 0, NULL);
			}
			break;


		case 'm' : codeStr++;

			if (codeStr[0]=='1') {
				term_setattr(LIGHTGREEN); cterm_write("Supply Equipment\r\n", -1, NULL, 0, NULL);
				term_setattr(GREEN);  cterm_write("~~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			}
			else {
				term_setattr(LIGHTCYAN);
				cterm_write("\r\n-[B]  Buy Equipment\r\n", -1, NULL, 0, NULL);
				cterm_write("-[S]  Sell Equipment\r\n\r\n", -1, NULL, 0, NULL);
				cterm_write("-[*]  Back to Supply Room\r\n\r\n", -1, NULL, 0, NULL);
				term_setattr(LIGHTBLUE);
				cterm_write("-<Supply Equip>  é ", -1, NULL, 0, NULL);
			}
			break;

		case 'n' : codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(LIGHTBLUE);
			cterm_write("Equipment                 Crystals\r\n", -1, NULL, 0, NULL); term_setattr(CYAN);
			cterm_write("~~~~~~~~~                 ~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTCYAN);

			for (zz=1;zz<13;zz++) {
				if (zz<=9)
					sprintf(buf,"-[%d]  - %s",zz,equips[zz]);
				else
					sprintf(buf,"-[%d] - %s",zz,equips[zz]);

				strljust(buf,28,' ');
				cterm_write(buf, -1, NULL, 0, NULL);

				if (menuBlock[0]==48)
					sprintf(buf,"%ld",ePrice[zz]);
				else
					sprintf(buf,"%ld",ePrice[zz]*MultE);

				strrjust(buf,6,' ');
				cterm_write(buf, -1, NULL, 0, NULL); cterm_write("\r\n", -1, NULL, 0, NULL);
			}

			cterm_write("\r\n", -1, NULL, 0, NULL);
			break;

		case 'o':
			codeStr++;
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);

			term_setattr(LIGHTGREEN); cterm_write("Supply Suits\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);  cterm_write("~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(WHITE);  cterm_write("Suit : ", -1, NULL, 0, NULL);
			cterm_write(suits[zz], -1, NULL, 0, NULL);

			term_setattr(LIGHTCYAN);
			cterm_write("\r\n\r\n-[B]  Buy a Suit\r\n", -1, NULL, 0, NULL);
			cterm_write("-[S]  Sell a Suit\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Back to Supply Room\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Supply Suits>  é ", -1, NULL, 0, NULL);
			break;

		case 'p' :
			codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(LIGHTBLUE); cterm_write("Suits/Outfits               Crystals\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);  cterm_write("~~~~~~~~~~~~~               ~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			for (zz=1;zz<3;zz++) {
				sprintf(buf,"-[%d]  - %s",zz,suits[zz]);
				strljust(buf,30,' ');
				cterm_write(buf, -1, NULL, 0, NULL);

				if (menuBlock[0]==48)
					sprintf(buf,"%ld",sPrice[zz]);
				else
					sprintf(buf,"%ld",sPrice[zz]*MultS);

				strrjust(buf,6,' ');
				cterm_write(buf, -1, NULL, 0, NULL); cterm_write("\r\n", -1, NULL, 0, NULL);
			}
			break;

		case 'q' : codeStr++;

			term_setattr(LIGHTGREEN);  cterm_write("Supply Weapons\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);   cterm_write("~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(WHITE);

			for (yy=1;yy<3;yy++) {
				getBlock(&codeStr,menuBlock);
				zz=atoi(menuBlock);
				cterm_write(weapons[zz], -1, NULL, 0, NULL); cterm_write(" / ", -1, NULL, 0, NULL);
				getBlock(&codeStr,menuBlock);
				zz=atoi(menuBlock);
				cterm_write(ammos[zz], -1, NULL, 0, NULL); cterm_write(" / ", -1, NULL, 0, NULL);
				getBlock(&codeStr,menuBlock);
				cterm_write(menuBlock, -1, NULL, 0, NULL);  cterm_write(" Rds\r\n", -1, NULL, 0, NULL);
			}

			term_setattr(LIGHTCYAN);
			cterm_write("\r\n-[B]  Buy Weapon\r\n", -1, NULL, 0, NULL);
			cterm_write("-[S]  Sell Weapon\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Back to Supply Room\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Supply Weapons>  é ", -1, NULL, 0, NULL);
			break;

		case 'r' : codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(LIGHTBLUE); cterm_write("Hand-to-Hand Weapons        Crystals\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);  cterm_write("~~~~~~~~~~~~~~~~~~~~        ~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			for (zz=1;zz<7;zz++) {
				sprintf(buf,"-[%d]  - %s",zz,weapons[zz]);
				strljust(buf,30,' ');
				cterm_write(buf, -1, NULL, 0, NULL);

				if (menuBlock[0]==48)
					sprintf(buf,"%ld",wPrice[zz]);
				else
					sprintf(buf,"%ld",wPrice[zz]*MultW);

				strrjust(buf,6,' ');
				cterm_write(buf, -1, NULL, 0, NULL); cterm_write("\r\n", -1, NULL, 0, NULL);
			}

			term_setattr(LIGHTBLUE); cterm_write("\r\nLong-Range Weapons          Crystals\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);  cterm_write("~~~~~~~~~~~~~~~~~~          ~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			for (zz=7;zz<13;zz++) {
				if (zz<10)
					sprintf(buf,"-[%d]  - %s",zz,weapons[zz+1]);
				else
					sprintf(buf,"-[%d] - %s",zz,weapons[zz+1]);

				strljust(buf,30,' ');
				cterm_write(buf, -1, NULL, 0, NULL);

				if (menuBlock[0]==48)
					sprintf(buf,"%ld",wPrice[zz+1]);
				else
					sprintf(buf,"%ld",wPrice[zz+1]*MultW);

				strrjust(buf,6,' ');
				cterm_write(buf, -1, NULL, 0, NULL);  cterm_write("\r\n", -1, NULL, 0, NULL);
			}
			break;

		case 's' : codeStr++;
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);

			term_setattr(LIGHTGREEN); cterm_write("Squadrons Menu\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);  cterm_write("~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTCYAN);

			cterm_write("-[S]  Show Squadrons/Leaders\r\n", -1, NULL, 0, NULL);

			if (zz>0)
				cterm_write("-[M]  Show Squadron Members\r\n", -1, NULL, 0, NULL);
			cterm_write("\r\n", -1, NULL, 0, NULL);

			if (zz==0)
				cterm_write("-[D]  Declare a Squadron\r\n", -1, NULL, 0, NULL);
			else
			if (zz<10)
				cterm_write("-[Q]  Quit your Squadron\r\n", -1, NULL, 0, NULL);
			else
				cterm_write("-[L]  Leader Options\r\n", -1, NULL, 0, NULL);

			if (zz==0)
				cterm_write("-[R]  Recruit to a Squadron\r\n", -1, NULL, 0, NULL);

			cterm_write("\r\n-[*]  Return to HQ\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTBLUE); cterm_write("-<Squadrons>  é ", -1, NULL, 0, NULL);
			break;

		case 't' : codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(WHITE); cterm_write("Sighting : ", -1, NULL, 0, NULL);
			term_setattr(LIGHTMAGENTA); cterm_write(menuBlock, -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			cterm_write("\r\n\r\n-[L]  ", -1, NULL, 0, NULL);
			term_setattr(CYAN);
			cterm_write("Long-Range Combat\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN); cterm_write("-[H]  ", -1, NULL, 0, NULL);
			term_setattr(CYAN);
			cterm_write("Hand-to-Hand Combat\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);

			if (!zz) {
				term_setattr(LIGHTCYAN); cterm_write("-[R]  ", -1, NULL, 0, NULL);
				term_setattr(CYAN);   cterm_write("Run like hell!\r\n", -1, NULL, 0, NULL);
			}

			term_setattr(LIGHTCYAN); cterm_write("\r\n-[C]  ", -1, NULL, 0, NULL); term_setattr(MAGENTA); cterm_write("Check Status\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN); cterm_write(  "-[V]  ", -1, NULL, 0, NULL); term_setattr(MAGENTA); cterm_write("View Opponent\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN); cterm_write(  "-[M]  ", -1, NULL, 0, NULL); term_setattr(LIGHTGRAY); cterm_write("Toggle Combat Modes\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);  cterm_write("-<Combat>  é ", -1, NULL, 0, NULL);
			break;


		case 'u' : codeStr++;
			getBlock(&codeStr,menuBlock);
			strcpy(tempBlock,menuBlock);

			term_setattr(WHITE);  cterm_write("Camp :[ ", -1, NULL, 0, NULL);
			term_setattr(LIGHTGREEN); cterm_write(menuBlock, -1, NULL, 0, NULL);
			term_setattr(WHITE);  cterm_write(" ]:\r\n", -1, NULL, 0, NULL); term_setattr(RED);

			for (zz=0;zz<(10+strlen(menuBlock));zz++) cterm_write("~", -1, NULL, 0, NULL);
			cterm_write("\r\n", -1, NULL, 0, NULL);

			term_setattr(CYAN);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz>0)
				cterm_write("-[A]  Attack recruit\r\n", -1, NULL, 0, NULL);

			cterm_write("-[L]  Leave a message\r\n\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (!zz)
				cterm_write("-[B]  Borrow camper's items\r\n", -1, NULL, 0, NULL);
			else
				cterm_write("-[S]  Steal camper's items\r\n", -1, NULL, 0, NULL);

			cterm_write("-[G]  Give camper items\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[V]  View ", -1, NULL, 0, NULL);
			cterm_write(tempBlock, -1, NULL, 0, NULL);

			cterm_write("\r\n-[C]  Check your status\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[D]  Drop your items\r\n", -1, NULL, 0, NULL);
			cterm_write("-[P]  Pick up items\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write("-[*]  Leave campsite\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE); cterm_write("-<Camp>  é ", -1, NULL, 0, NULL);
			break;

		case 'v':
			codeStr++;
			term_setattr(LIGHTGREEN); cterm_write("Hit Points   :[ ", -1, NULL, 0, NULL); term_setattr(WHITE);
			getBlock(&codeStr,menuBlock);
			cterm_write(menuBlock, -1, NULL, 0, NULL); cterm_write(" ", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			sprintf(buf,"(%s)\r\n",menuBlock);
			cterm_write(buf, -1, NULL, 0, NULL);

			term_setattr(LIGHTGREEN); cterm_write("Applications :[ ", -1, NULL, 0, NULL); term_setattr(WHITE);
			getBlock(&codeStr,menuBlock);
			sprintf(buf,"%s current, ",menuBlock);
			cterm_write(buf, -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			sprintf(buf,"%s total\r\n",menuBlock);
			cterm_write(buf, -1, NULL, 0, NULL);

			term_setattr(LIGHTGREEN); cterm_write("Radiation    :[ ", -1, NULL, 0, NULL); term_setattr(WHITE);
			getBlock(&codeStr,menuBlock);
			cterm_write(menuBlock, -1, NULL, 0, NULL); cterm_write("%\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(WHITE); cterm_write("Also, you are ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			switch (zz) {
				case 0 :
					cterm_write("not very hungry.\r\n", -1, NULL, 0, NULL);
					break;
				case 1 :
					cterm_write("a little hungry.\r\n", -1, NULL, 0, NULL);
					break;
				case 2 :
					cterm_write("hungry.\r\n", -1, NULL, 0, NULL);
					break;
				case 3 :
					cterm_write("extremely hungry!\r\n", -1, NULL, 0, NULL);
					break;
				case 4 :
					term_setattr(LIGHTRED);
					cterm_write("DYING ", -1, NULL, 0, NULL);
					term_setattr(WHITE);
					cterm_write("of hunger!!\r\n", -1, NULL, 0, NULL);
					break;
			}

			term_setattr(YELLOW);
			cterm_write("\r\n-[Q]  Quick Heal\r\n", -1, NULL, 0, NULL);
			cterm_write("-[U]  Use an Application\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz==1)
				cterm_write("-[R]  Use Rations\r\n", -1, NULL, 0, NULL);

			cterm_write("-[*]  Exit\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write("-<Medpacks>  é ", -1, NULL, 0, NULL);
			break;
	}


	return(codeStr-origCodeStr);;
}

static void checkStamp(int xx,int yy,char stampStr[20]) {  // used w/ incomingCheck
	term_gotoxy(xx+1, yy+1);
	term_setattr(LIGHTCYAN);
	cterm_write(stampStr, -1, NULL, 0, NULL);
	term_setattr(CYAN);
	cterm_write("é", -1, NULL, 0, NULL);
	return;
}

static int incomingCheckStatus(char *codeStr) {

	int who,zz;
	char menuBlock[255];
	char *origCodeStr=codeStr;

	term_clearscreen();
	term_gotoxy(1,1);

	codeStr++;
	getBlock(&codeStr,menuBlock);
	who=atoi(menuBlock);

	term_setattr(who);
	cterm_write("\r\nÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\r\n", -1, NULL, 0, NULL);
	term_setattr(LIGHTCYAN);
	cterm_write("Recruit   ", -1, NULL, 0, NULL);
	term_setattr(CYAN);
	cterm_write("é", -1, NULL, 0, NULL);
	term_setattr(who);
	term_gotoxy(34,3);
	cterm_write(" ³", -1, NULL, 0, NULL);
	term_gotoxy(1,4);
	cterm_write("ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÏÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ", -1, NULL, 0, NULL);

	checkStamp(0,4,"Location  ");
	checkStamp(27,4,"Strength   ");
	checkStamp(54,4,"Radiation     ");
	checkStamp(0,5,"Training  ");
	checkStamp(27,5,"Dexterity  ");
	checkStamp(54,5,"Hit Accuracy  ");
	checkStamp(0,6,"Squadron  ");
	checkStamp(27,6,"Hit Pts    ");
	checkStamp(54,6,"Bravery       ");

	term_gotoxy(1, 8);
	cterm_write("ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ", -1, NULL, 0, NULL);

	checkStamp(0,8,"Crystals  ");
	checkStamp(27,8,"Disease    ");
	checkStamp(54,8,"Total Kills   ");
	checkStamp(0,9,"Exp. Pts  ");
	checkStamp(27,9,"Level      ");
	checkStamp(54,9,"Hunger Level  ");

	term_gotoxy(1,11);
	term_setattr(who);
	cterm_write("ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ", -1, NULL, 0, NULL);

	checkStamp(0,11,"Weapon    ");
	checkStamp(0,12,"Weapon    ");

	term_gotoxy(1, 14);
	term_setattr(who);
	cterm_write("ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ", -1, NULL, 0, NULL);

	checkStamp(0,14,"Armor     ");
	checkStamp(0,15,"Outfit    ");

	term_gotoxy(1,17);
	term_setattr(who);
	cterm_write("ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ", -1, NULL, 0, NULL);

	for (zz=1;zz<4;zz++)
		checkStamp(0,16+zz,"Equipment ");

	term_gotoxy(1, 21);
	term_setattr(who);
	cterm_write("ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ", -1, NULL, 0, NULL);

	term_gotoxy(1, 23);

	return(codeStr-origCodeStr);;
}

char * scanChar(unsigned char s, int where, int miniTrik) {
	char sc[30];
	unsigned char zz;
	unsigned char mbPos;

	if (!miniTrik) {
		if ((s==7) || (s>=9  && s<=11) || (s>=26 && s<=28) || (s>=31 && s<=33) || (s==38))
			return("??");
	}


	switch (s) {

		case 1 :
			if (where==4) // AFB
	    		return("²²²");
    		return("OO ");
		case 2 :
			return("ww ");
		case 3 :
			return(".. ");
		case 4 :
			return("\"\" ");
		case 5 :
			return("^^ ");
		case 6 :
			return("-- ");
		case 7 :
			return("UU ");
		case 8 :
			return("@v ");
		case 9 :
			return("®¯ ");
		case 10:
			return("ÊÊ ");
		case 11:
			return("%% ");
		case 12:
			return("II ");
		case 13:
			return("## ");
		case 14:
			return("@^ ");
		case 15:
			return("÷÷ ");
		case 16:
			return("°Ä°");
		case 17: 
			return("²±²");
		case 19:
			return("GG ");
		case 20:
			return("#_ ");
		case 21:
			return(" ");
		case 22:
			return("áá ");
		case 23:
			return("ØØ ");
		case 24:
			return(" ù ");
		case 25:
			if (where==4) // AFB
				return("óò ");
			return("É» ");
		case 26:
			return("ii ");
		case 27:
			return("èè ");
		case 28:
			return("cc ");
		case 29:
			return("@@ ");
		case 30:
			{
				time_t timer=time(NULL);
				struct tm *tblock=localtime(&timer);

				switch (tblock->tm_wday) {

	    			//   0 : no storms on Sunday!
	    			case 1 : return(")) ");
	    			case 2 : return("## ");
	    			case 3 : return("Õ¾ ");
	    			case 4 : return("// ");
	    			case 5 : return("/\\ ");
	    			case 6 : return("00 ");
				}
			}
			break;

		case 31:
			return("YY ");
		case 32:
			return(" ");
		case 33:
			return("ëë ");
		case 34:
			return("ä¯ ");
		case 35:
			return("[] ");
		case 37:
			return("** ");
		case 38:
			return(" ");
		case 39:
			return("ğğ ");
		case 99:
			return("[] ");
		default:
			return("?? ");
	}

	return("??");
}

static void setScanCol(char s) {
  switch ((char)s) {
    case '*'                     : term_setattr(4+128);  break;
    case 'G'                     : term_setattr(1);       break;
    case 'w'                     : term_setattr(2);       break;
    case '"': case '°'           : term_setattr(3);       break;
    case 'U': case '%'           : term_setattr(4);       break;

    case '^': case 'i':
    case 'á': case 'ó'           : term_setattr(6);       break;

    case 'O': case '²': case 'Q' : term_setattr(7);       break;
    case 'ä': case ' '           : term_setattr(8);       break;
    case '÷'                     : term_setattr(9);       break;
    case '-': case ''           : term_setattr(10);      break;

    case '?': case '@': case '/':
    case '': case 'Y': case 'ë' : term_setattr(12);      break;

    case '#': case 'c': case 'ğ' : term_setattr(13);      break;

    case '.': case '[': case '®':
    case 'Ê': case '!': case 'É':
    case 'Ø'                     : term_setattr(14);      break;

    case 'I': case 'è': case '' : term_setattr(15);      break;

  }

  return;
}

static int incomingMapScanner(char *codeStr)
{

	int zz,scanPtr,yy;
	unsigned char scanVals[10];       // ScanVals  : ARRAY[1..9] OF BYTE;
	char scanChIn[255];
	char scan[30];
	char menuBlock[255];
	char *origCodeStr=codeStr;
	int  where;
	int  miniTrik;

	codeStr++;

	switch ((char)codeStr[0]) {
		case 'F' :
			term_clearscreen();
			term_gotoxy(1, 1);
			term_setattr(1);
			cterm_write("\r\nÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\r\n", -1, NULL, 0, NULL);
			cterm_write("³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write("ÖÄ", -1, NULL, 0, NULL); term_setattr(13); cterm_write("Infrared", -1, NULL, 0, NULL); term_setattr(5);
			cterm_write("Ä·", -1, NULL, 0, NULL); term_setattr(13); cterm_write("     Compass      Sector Monitor     System Monitor           ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write("³\r\n", -1, NULL, 0, NULL);
			cterm_write("³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write("Ç          ¶     ~~~~~~~      ~~~~~~~~~~~~~~     ~~~~~~~~~~~~~~           ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write("³\r\n", -1, NULL, 0, NULL);
			cterm_write("³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write("Ç          ¶   [         ]    [ ]  ", -1, NULL, 0, NULL); term_setattr(13);
			cterm_write("Items         Hit Points", -1, NULL, 0, NULL); term_setattr(5); cterm_write(" [:            ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write("³\r\n", -1, NULL, 0, NULL);
			cterm_write("³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write("Ç          ¶                  [ ]  ", -1, NULL, 0, NULL); term_setattr(13);
			cterm_write("Campers       Radiation  ", -1, NULL, 0, NULL); term_setattr(5); cterm_write("[:            ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write("³\r\n", -1, NULL, 0, NULL);
			cterm_write("³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write("ÓÄÄÄ", -1, NULL, 0, NULL); term_setattr(13); cterm_write("scan", -1, NULL, 0, NULL); term_setattr(5);
			cterm_write("ÄÄÄ½    ", -1, NULL, 0, NULL); term_setattr(13); cterm_write("Time :        ", -1, NULL, 0, NULL); term_setattr(5); cterm_write("[ ]  ", -1, NULL, 0, NULL);
			term_setattr(13); cterm_write("Bases         Condition  ", -1, NULL, 0, NULL); term_setattr(5); cterm_write("[:            ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write("³\r\n", -1, NULL, 0, NULL);
			cterm_write("ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ\r\n", -1, NULL, 0, NULL);
			break;

		case 'T' :
			term_gotoxy(1, 9);
			term_setattr(1);
			cterm_write("ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\r\n", -1, NULL, 0, NULL);
			cterm_write("³                                                                           ³\r\n", -1, NULL, 0, NULL);
			cterm_write("³                                                                           ³\r\n", -1, NULL, 0, NULL);
			cterm_write("ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ\r\n", -1, NULL, 0, NULL);
			break;

		case 'M' :
			codeStr++;

			if (codeStr[0]=='A')
				where=4; // AFB
			else
				where=1; // wasteland

			codeStr++;

			if (codeStr[0]=='T')
				miniTrik=1;
			else
				miniTrik=0;

			codeStr++;

			for(scanPtr=0;scanPtr<9;scanPtr++) {
				getBlock(&codeStr,menuBlock);
				scanVals[scanPtr]=atoi(menuBlock);
			}

			for (zz=0;zz<9;zz++) {
				switch (zz) {
					case 0 : term_gotoxy(5,4);  break;
					case 1 : term_gotoxy(8,4);  break;
					case 2 : term_gotoxy(11,4); break;
					case 3 : term_gotoxy(5,5);  break;
					case 4 : term_gotoxy(8,5);  break;
					case 5 : term_gotoxy(11,5); break;
					case 6 : term_gotoxy(5,6);  break;
					case 7 : term_gotoxy(8,6);  break;
					case 8 : term_gotoxy(11,6); break;
				}

				strcpy(scan,scanChar(scanVals[zz], where, miniTrik));

				if (scanVals[zz]==30) {
					/* TODO: timer/tblock were not declared... */
					time_t timer=time(NULL);
					struct tm *tblock=localtime(&timer);
					term_setattr(stormColors[tblock->tm_wday]);
				}
				else
					setScanCol(scan[0]);

				if (strlen(scan)==2) {
					cterm_write(scan, -1, NULL, 0, NULL);
					cterm_write(" ", -1, NULL, 0, NULL);
				}
				else
					cterm_write(scan, -1, NULL, 0, NULL);
			}

			term_gotoxy(1,14);
			break;

		case 'O' :
			codeStr++;

			for (zz=1;zz<9;zz++) {

				switch (zz) {
					case 1 :
						term_gotoxy(20,5);
						term_setattr(14);
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock,",");
						cterm_write(menuBlock, -1, NULL, 0, NULL);
						break;

					case 2 :
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock,",");
						cterm_write(menuBlock, -1, NULL, 0, NULL);
						break;

					case 3 :
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock," ");
						cterm_write(menuBlock, -1, NULL, 0, NULL);
						break;

					case 4 : 
						term_gotoxy(26,7);
						term_setattr(15);
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock," ");
						cterm_write(menuBlock, -1, NULL, 0, NULL);
						break;

					case 5 : 
						term_gotoxy(66,5);
						term_setattr(14);
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock," ");
						cterm_write(menuBlock, -1, NULL, 0, NULL);
						break;

					case 6 :
						getBlock(&codeStr,menuBlock);
						cterm_write("(", 1, NULL, 0, NULL);
						cterm_write(menuBlock, -1, NULL, 0, NULL);
						cterm_write(")", 1, NULL, 0, NULL);
						break;

					case 7 :
						term_gotoxy(66,6);
						getBlock(&codeStr,menuBlock);
						yy=atoi(menuBlock);
						if (yy>=40)
							term_setattr(12);
						else
							term_setattr(14);
						strcat(menuBlock,"% ");
						cterm_write(menuBlock, -1, NULL, 0, NULL);
						break;

					case 8 :
						term_gotoxy(66,7);
						term_setattr(14);
						getBlock(&codeStr,menuBlock);
						yy=atoi(menuBlock);

						switch (yy) {
							case 0 : cterm_write("Not Hungry", -1, NULL, 0, NULL); break;
							case 1 : cterm_write("Bit Hungry", -1, NULL, 0, NULL); break;
							case 2 : cterm_write("Hungry    ", -1, NULL, 0, NULL); break;
							case 3 : term_setattr(12);
								cterm_write("HUNGRY!   ", -1, NULL, 0, NULL); break;
							case 4 : term_setattr(12+128);
								cterm_write("STARVING!!", -1, NULL, 0, NULL); break;
						}
						break;
				}
			}

			codeStr++;
			if (codeStr[0]=='I') {
				codeStr++;
				term_gotoxy(34,5);

				if (codeStr[0]=='0')
					cterm_write(" ", -1, NULL, 0, NULL);
				else
				{
					term_setattr(12);
					cterm_write("û", -1, NULL, 0, NULL);
				}

				codeStr++;
				codeStr++;
			}

			if (codeStr[0]=='C') {
				codeStr++;
				term_gotoxy(34,6);

				if (codeStr[0]=='0')
					cterm_write(" ", -1, NULL, 0, NULL);
				else
				{
					term_setattr(12);
					cterm_write("û", -1, NULL, 0, NULL);
				}

				codeStr++;
				codeStr++;
			}

			if (codeStr[0]=='B') {
				codeStr++;
				term_gotoxy(34,7);

				if (codeStr[0]=='0')
					cterm_write(" ", -1, NULL, 0, NULL);
				else
				{
					term_setattr(12);
					cterm_write("û", -1, NULL, 0, NULL);

					codeStr++;
					codeStr++;

					getBlock(&codeStr,menuBlock);
					yy=atoi(menuBlock);
					term_gotoxy(1,yy+1);

					getBlock(&codeStr,menuBlock);
					term_setattr(14);
					strcat(menuBlock," is stationed nearby.");
					cterm_write(menuBlock, -1, NULL, 0, NULL);
				}
			}
			break;


		case 'D' :
			codeStr++;
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			term_gotoxy(3,10);
			cterm_write("                                                                          ", -1, NULL, 0, NULL);
			term_gotoxy(3,11);
			cterm_write("                                                                          ", -1, NULL, 0, NULL);
			term_gotoxy(3,10);

			term_setattr(11);

			switch (zz) {
				case 2 : cterm_write("Crumbling walls of ancient cities scatter the plains, and smoke lazily", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write("rises from the destructed relics of the past.", -1, NULL, 0, NULL);
					break;

				case 3 : cterm_write("Coldness creeps upon you like death in this vast range of empty desert.", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write("Harsh winds rip sand against your body like tiny shards of glass.", -1, NULL, 0, NULL);
					break;

				case 4 : cterm_write("The black muck from the swamp slithers between your toes as you make your", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write("way through the bubbling slime.", -1, NULL, 0, NULL);
					break;

				case 5 : cterm_write("High upon the scorched mountains, small blackening caves can be seen.", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write("Possibly lurking within, horrible creatures wait for your trespassing.", -1, NULL, 0, NULL);
					break;

				case 6 : cterm_write("A mist glides along the flat terrain and summons you forward.", -1, NULL, 0, NULL);
					break;

				case 7 : ; break; // pits

				case 8 : cterm_write("A small hole in the ground leads down into a realm of darkness.", -1, NULL, 0, NULL);
					break;

				case 9 : cterm_write("From the pre-war years, an Airforce Base lies half buried from nuclear", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write("ash and metallic scraps.", -1, NULL, 0, NULL);
					break;

				case 10: cterm_write("A disabled missile silo remains intact from heavy bombing in this area.", -1, NULL, 0, NULL);
					break;

				case 11: cterm_write("In an effortless HeXonium attempt to clean the wastelands, radiation dumps", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write("lay wretched with oozing radioactive substances forming.", -1, NULL, 0, NULL);
					break;

				case 12: cterm_write("This unmarked road is littered with dead shrubs and rotting vegatation.", -1, NULL, 0, NULL);
					break;

				case 13: cterm_write("Darkness swirls around the infamous and towering Hydrite Prison.", -1, NULL, 0, NULL);
					break;

				case 14: cterm_write("As you look up, you notice a small hole that leads upwards.", -1, NULL, 0, NULL);
					break;

				case 15: cterm_write("You cautiously wade through the black murky water of this shallow stream.", -1, NULL, 0, NULL);
					break;
			}
			break;

		case 'P' : codeStr++;
			term_gotoxy(3,10);
			cterm_write("                                                                          ", -1, NULL, 0, NULL);
			term_gotoxy(3,11);
			cterm_write("                                                                          ", -1, NULL, 0, NULL);
			term_gotoxy(3,10);
			term_setattr(11);
			cterm_write("20 foot pit!", -1, NULL, 0, NULL);
			break;
	}

	return(codeStr-origCodeStr);;
}

/* TODO: Sound support */
static int incomingSoundVoc(char *codeStr) {
	char *origCodeStr=codeStr;

	codeStr++;
	switch(*codeStr) {
		case '1':
			xp_play_sample(ooii_snd_welcome, sizeof(ooii_snd_welcome), TRUE);
			break;
		case '3':
			xp_play_sample(ooii_snd_countdn, sizeof(ooii_snd_countdn), FALSE);
			/* Fallthrough */
		case '2':
			xp_play_sample(ooii_snd_explode, sizeof(ooii_snd_explode), TRUE);
			break;
		case '4':
			xp_play_sample(ooii_snd_clone, sizeof(ooii_snd_clone), TRUE);
			break;
		case '5':
			if(xp_random(2))
				xp_play_sample(ooii_snd_danger1, sizeof(ooii_snd_danger1), TRUE);
			else
				xp_play_sample(ooii_snd_danger2, sizeof(ooii_snd_danger2), TRUE);
			break;
		case '6':
			xp_play_sample(ooii_snd_steam, sizeof(ooii_snd_steam), TRUE);
			break;
		case '7':
			xp_play_sample(ooii_snd_scream1, sizeof(ooii_snd_scream1), TRUE);
			break;
		case '8':
			xp_play_sample(ooii_snd_watch, sizeof(ooii_snd_watch), TRUE);
			break;
		case '9':
			xp_play_sample(ooii_snd_levelup, sizeof(ooii_snd_levelup), TRUE);
			break;
		case 'A':
			xp_play_sample(ooii_snd_inflame, sizeof(ooii_snd_inflame), TRUE);
			break;
		case 'B':
			switch(xp_random(3)) {
				case 0:
					xp_play_sample(ooii_snd_hit1, sizeof(ooii_snd_hit1), TRUE);
					break;
				case 1:
					xp_play_sample(ooii_snd_hit2, sizeof(ooii_snd_hit2), TRUE);
					break;
				case 2:
					xp_play_sample(ooii_snd_hit3, sizeof(ooii_snd_hit3), TRUE);
					break;
			}
			break;
		case 'C':
			xp_play_sample(ooii_snd_timeisup, sizeof(ooii_snd_timeisup), TRUE);
			break;
		case 'D':
			xp_play_sample(ooii_snd_healing, sizeof(ooii_snd_healing), TRUE);
			break;
		case 'E':
			xp_play_sample(ooii_snd_lrange2, sizeof(ooii_snd_lrange2), TRUE);
			break;
		case 'F':
			xp_play_sample(ooii_snd_cackle, sizeof(ooii_snd_cackle), TRUE);
			break;
		case 'G':
			xp_play_sample(ooii_snd_teleport, sizeof(ooii_snd_teleport), TRUE);
			break;
		case 'H':
			xp_play_sample(ooii_snd_genetics, sizeof(ooii_snd_genetics), TRUE);
			break;
		case 'I':
			xp_play_sample(ooii_snd_remote, sizeof(ooii_snd_remote), TRUE);
			break;
		case 'J':
			xp_play_sample(ooii_snd_afbdoor, sizeof(ooii_snd_afbdoor), TRUE);
			break;
		case 'K':
			xp_play_sample(ooii_snd_alarm, sizeof(ooii_snd_alarm), TRUE);
			break;
		case 'L':
			xp_play_sample(ooii_snd_reverse, sizeof(ooii_snd_reverse), TRUE);
			break;
		case 'M':
			xp_play_sample(ooii_snd_aerial, sizeof(ooii_snd_aerial), TRUE);
			break;
		case 'N':
			xp_play_sample(ooii_snd_phaser, sizeof(ooii_snd_phaser), TRUE);
			break;
		case 'O':
			switch(xp_random(3)) {
				case 0:
					xp_play_sample(ooii_snd_miss1, sizeof(ooii_snd_miss1), TRUE);
					break;
				case 1:
					xp_play_sample(ooii_snd_miss2, sizeof(ooii_snd_miss2), TRUE);
					break;
				case 2:
					xp_play_sample(ooii_snd_miss3, sizeof(ooii_snd_miss3), TRUE);
					break;
			}
			break;
		case 'P':
			if(xp_random(2))
				xp_play_sample(ooii_snd_music1, sizeof(ooii_snd_music1), TRUE);
			else
				xp_play_sample(ooii_snd_music2, sizeof(ooii_snd_music2), TRUE);
			break;
		case 'Q':
			xp_play_sample(ooii_snd_device, sizeof(ooii_snd_device), TRUE);
			break;
		case 'R':
			xp_play_sample(ooii_snd_death, sizeof(ooii_snd_death), TRUE);
			break;
		case 'S':
			xp_play_sample(ooii_snd_good, sizeof(ooii_snd_good), TRUE);
			break;
		case 'T':
			xp_play_sample(ooii_snd_yahoo, sizeof(ooii_snd_yahoo), TRUE);
			break;
		case 'U':
			xp_play_sample(ooii_snd_scream2, sizeof(ooii_snd_scream2), TRUE);
			break;
		case 'V':
			xp_play_sample(ooii_snd_wap, sizeof(ooii_snd_wap), TRUE);
			break;
		case 'W':
			xp_play_sample(ooii_snd_zip, sizeof(ooii_snd_zip), TRUE);
			break;
		case 'X':
			xp_play_sample(ooii_snd_lrange3, sizeof(ooii_snd_lrange3), TRUE);
			break;
		case 'Y':
			xp_play_sample(ooii_snd_snip, sizeof(ooii_snd_snip), TRUE);
			break;
		case 'Z':
			xp_play_sample(ooii_snd_pow, sizeof(ooii_snd_pow), TRUE);
			break;
		default:
			/* LRANGE1 is unused */
			/* xp_play_sample(ooii_snd_lrange1, sizeof(ooii_snd_lrange1), TRUE); */
			break;
	}
	return(codeStr-origCodeStr);
}

BOOL handle_ooii_code(char *codeStr, int ooii_mode, char *retbuf, size_t retsize)
{
	BOOL	quit=FALSE;

	if(retbuf!=NULL)
		retbuf[0]=0;

	codeStr++;	/* Skip intro char */

	for(;*codeStr && *codeStr != '|'; codeStr++) {
		if ( codeStr[0]>='A' && codeStr[0]<='Z') {
			/* This one never takes an extra char */
			readInPix(codeStr[0], ooii_mode);
		}
		else if ( codeStr[0]>='1' && codeStr[0]<='9') {
			codeStr += readInText(codeStr);
		}
		else if ( codeStr[0]>='a' && codeStr[0]<='z') {
			codeStr += readSmallMenu(codeStr);
		}
		else {
			switch ( codeStr[0]) {
    			case '0':
					/* This one never takes an extra char */
					readInPix(codeStr[0], ooii_mode);
					break;
    			case '!'  :
					codeStr += incomingCheckStatus(codeStr);
					break;
    			case '#'  :
					codeStr += incomingMapScanner(codeStr);
					break;
    			case '$'  :
					codeStr += incomingSoundVoc(codeStr);
					break;
    			case '\\' :
					quit=TRUE;
					//quitTerm=1;
					break;
				case '?':
					if(retbuf!=NULL) {
						if(strlen(retbuf)+3 < retsize)
								strcat(retbuf,/* "\xaf""1|" */ "\xaf""2|");
					}
					break;
			}
		}
	}
	return(quit);
}

