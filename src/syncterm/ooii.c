/*
 * This code handles Operation Overkill ][ terminal codes
 */

#include <string.h>
#include <genwrap.h>
#include <xpbeep.h>
#include <ciolib.h>
#include <cterm.h>
#include <time.h>
#include "term.h"
#include "ooii.h"
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
	cterm_write(cterm, ansistr, strlen(ansistr), NULL, 0, NULL);
}

static void term_clearscreen(void)
{
	cterm_write(cterm, "\x1b[0m\x1b[2J\x1b[H", 11, NULL, 0, NULL);
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
	if(cterm->attr==attr)
		return;

	bl=attr&0x80;
	bg=(attr>>4)&0x7;
	fg=attr&0x07;
	br=attr&0x08;

	oa=cterm->attr;
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
	cterm_write(cterm, str, strlen(str), NULL, 0, NULL);
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
		cterm_write(cterm, ooii_cmenus[ooii_mode-1][fptr], strlen((char *)ooii_cmenus[ooii_mode-1][fptr])-1, NULL, 0, NULL);
	else if (codeCh>='F' && codeCh<='K')
		cterm_write(cterm, ooii_bmenus[ooii_mode-1][fptr], strlen((char *)ooii_bmenus[ooii_mode-1][fptr])-1, NULL, 0, NULL);
	else if (codeCh=='0')
		cterm_write(cterm, ooii_logon[ooii_mode-1][fptr], strlen((char *)ooii_logon[ooii_mode-1][fptr])-1, NULL, 0, NULL);

	/* We don't overwrite the status line, so we don't need to redraw it */
	/* statusLine(); */

	return;
}

static int readInText(unsigned char *codeStr) {
	unsigned char *origCodeStr=codeStr;

	switch ((char)codeStr[0]) {
		case '1':
			term_setattr(BROWN);
	    	cterm_write(cterm, "You mosey on over to the bar and take a seat on a scuffed barstool.  The\r\n", -1, NULL, 0, NULL);
	    	cterm_write(cterm, "slighty deformed keeper grunts, \"Woth ja leeke?\"  A galacticom on the top\r\n", -1, NULL, 0, NULL);
	    	cterm_write(cterm, "shelf behind him translates his jumble into \"What would you like?\"  You\r\n", -1, NULL, 0, NULL);
	    	cterm_write(cterm, "start to wonder what ever happened to the old standard human language.\r\n\r\n", -1, NULL, 0, NULL);
	    	term_setattr(LIGHTGREEN);
	    	cterm_write(cterm, "\"Give me the House Special,\" you smirk.\r\n\r\n", -1, NULL, 0, NULL);
	    	term_setattr(GREEN);
	    	cterm_write(cterm, "The bartender stares at you with one eye and then drools in agreement.  He\r\n", -1, NULL, 0, NULL);
	    	cterm_write(cterm, "vanishes from behind the bar and pops back up a few seconds later.  He\r\n", -1, NULL, 0, NULL);
	    	cterm_write(cterm, "slides you the mysterious drink as he begins to chuckle.  White smoke\r\n", -1, NULL, 0, NULL);
	    	cterm_write(cterm, "slithers from the bubbling green slime over onto the bar counter.\r\n\r\n", -1, NULL, 0, NULL);
	    	term_setattr(LIGHTGREEN);
	    	cterm_write(cterm, "\"What do I owe you?\" you inquire to the barman.  He promptly responds\r\n", -1, NULL, 0, NULL);
	    	cterm_write(cterm, "with that same smirk on his face, \"Youkla telph me.\"\r\n", -1, NULL, 0, NULL);
	    	break;
	}

	return(codeStr-origCodeStr);;
}

static void getBlock(unsigned char **codeStr, char *menuBlock)
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

static int readSmallMenu(unsigned char *codeStr) {
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
	int  yy;
	int  zz;
	unsigned char *origCodeStr=codeStr;
	char buf[256];

	switch ((char)codeStr[0]) {
		case 'a':
			term_setattr(LIGHTGREEN);
			cterm_write(cterm, "Supply Room\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);
			cterm_write(cterm, "~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);
			cterm_write(cterm, "-[A]  Armor\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[E]  Equipment\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[P]  Power Supply/Ammo\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[S]  Suits/Outfits\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[W]  Weapons\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[I]  Supply Inventory\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[C]  Check\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Back to HQ\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Supply Room>  é ", -1, NULL, 0, NULL);
			break;

		case 'b':
			term_setattr(WHITE);
			cterm_write(cterm, "Recruit Chambers\r\n", -1, NULL, 0, NULL);
			term_setattr(RED);
			cterm_write(cterm, "~~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);
			cterm_write(cterm, "-[A]  Adjust Specs\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[C]  Check Status\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[M]  Morgue Listing\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[R]  Roster Status\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[T]  Training Room\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[U]  Use Equipment\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[V]  View Recruit\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Exit to Barracks\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Chambers>  é ", -1, NULL, 0, NULL);
			break;

		case 'c':
			term_setattr(LIGHTMAGENTA);
			cterm_write(cterm, "Adjust Specifications:\r\n", -1, NULL, 0, NULL);
			term_setattr(MAGENTA);
			cterm_write(cterm, "~~~~~~~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTGREEN);
			cterm_write(cterm, "-[A]  Ansi Color On/Off\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[C]  Combat Action/Stat-Random\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[D]  Disable/Enable Ansiterm\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[E]  Expert Menus On/Off\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[F]  Fight-Text Delay Change\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[P]  Password Change\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[Q]  Quote for Combat\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[S]  Scanner Scroll/Stationary\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[T]  Terminate Yourself\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Exit\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Specs>  é ", -1, NULL, 0, NULL);
			break;

		case 'd':
			codeStr++;

			term_setattr(LIGHTMAGENTA);
			cterm_write(cterm, "Storage Bank:\r\n", -1, NULL, 0, NULL);
			term_setattr(MAGENTA);
			cterm_write(cterm, "~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(WHITE);

			cterm_write(cterm, "Crystals in Savings : ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			strcat(menuBlock,"\r\n");
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);

			cterm_write(cterm, "Crystals on Loan    : ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			strcat(menuBlock,"\r\n");
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);

			cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			cterm_write(cterm, "-[D]  Deposit\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[W]  Withdraw\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[L]  Make a Loan\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[P]  Pay off Loan\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz>0)
				cterm_write(cterm, "-[S]  Squadron Account\r\n", -1, NULL, 0, NULL);

			cterm_write(cterm, "-[T]  Transfer Crystals\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Exit to Chambers\r\n\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			term_setattr(WHITE);

			cterm_write(cterm, "You have ", -1, NULL, 0, NULL);
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
			cterm_write(cterm, " crystals onhand.\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Bank>  é ", -1, NULL, 0, NULL);
			break;

		case 'e' :
			codeStr++;

			term_setattr(YELLOW);
			cterm_write(cterm, "You are now in the Games Room, a delightful place to leisurely relax from\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "the everyday hack n' slash.   A small bar is located on the south wall.\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(WHITE);
			getBlock(&codeStr,menuBlock);
			cterm_write(cterm, "Games Left :[ ", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
			cterm_write(cterm, "\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTGREEN);

			cterm_write(cterm, "-[A]  Arm Wrestling\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[B]  Barl's Bar\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[L]  Loot Crystals\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[M]  Meat Darts\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[R]  Roach Races\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[S]  SharpShooter\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Exit to Barracks\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Games Room>  é ", -1, NULL, 0, NULL);
			break;

		case 'f' :
			codeStr++;

			term_setattr(WHITE); cterm_write(cterm, "Sentry Post\r\n", -1, NULL, 0, NULL);
			term_setattr(RED); cterm_write(cterm, "~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			getBlock(&codeStr,menuBlock);
			yy=atoi(menuBlock);
			if (yy>0) {

				cterm_write(cterm, "-[O]  Offer Guard Items\r\n\r\n", -1, NULL, 0, NULL);
				cterm_write(cterm, "-[V]  View Gate Guard\r\n", -1, NULL, 0, NULL);
				cterm_write(cterm, "-[U]  Upgrade Gate Guard\r\n", -1, NULL, 0, NULL);
			}
			else
				cterm_write(cterm, "-[R]  Requisition New Guard\r\n", -1, NULL, 0, NULL);

			cterm_write(cterm, "-[C]  Check Your Status\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Exit to Barracks\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Sentry Post>  é ", -1, NULL, 0, NULL);
			break;

		case 'g':
			codeStr++;

			term_setattr(WHITE); cterm_write(cterm, "Electronic-Mail\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTGRAY);   cterm_write(cterm, "~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);

			if (menuBlock[0]>48) {
				term_setattr(YELLOW);
				cterm_write(cterm, "-[M]  Mass Mail to Squadron\r\n\r\n", -1, NULL, 0, NULL);
			}

			term_setattr(LIGHTCYAN);
			cterm_write(cterm, "-[R]  Read E-Mail\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[S]  Send E-Mail\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Back to Comm. Post\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<E-Mail>  é ", -1, NULL, 0, NULL);
			break;

		case 'h':
			term_setattr(LIGHTCYAN);
			cterm_write(cterm, "\r\nHelp Information:\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);
			cterm_write(cterm, "~~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(WHITE);
			cterm_write(cterm, "-[1]  Scenario\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[2]  Character\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[3]  Complex\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[4]  Wastelands\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[5]  Bases\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[6]  Bulletin Info\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[7]  Logon News Info\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[0]  Exit\r\n\r\n", -1, NULL, 0, NULL);
			break;

		case 'i':
			codeStr++;

			term_setattr(LIGHTCYAN); cterm_write(cterm, "Medical Center\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);   cterm_write(cterm, "~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTGREEN);

			cterm_write(cterm, "Radiation   :[ ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz<44)
				term_setattr(WHITE);
			else
				term_setattr(LIGHTRED);
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL); cterm_write(cterm, "%\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTGREEN);
			cterm_write(cterm, "Hit Points  :[ ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz>5)
				term_setattr(WHITE);
			else
				term_setattr(LIGHTRED);

			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			cterm_write(cterm, " (", -1, NULL, 0, NULL);
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
			cterm_write(cterm, ")\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTGREEN);
			cterm_write(cterm, "Infectious  :[ ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			term_setattr(WHITE);

			cterm_write(cterm, diseases[zz], -1, NULL, 0, NULL); cterm_write(cterm, "\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);

			cterm_write(cterm, "-[M]  Medical Treatment\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[R]  Radiation Treatment\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[N]  Neutralize Disease\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[V]  Vaccinate Disease\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Back to HQ\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Medical Center>  é ", -1, NULL, 0, NULL);
			break;

		case 'j' :
			term_setattr(LIGHTMAGENTA);
			cterm_write(cterm, "Records Room\r\n", -1, NULL, 0, NULL);
			term_setattr(MAGENTA);   cterm_write(cterm, "~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTGREEN);
			cterm_write(cterm, "-[A]  Total Ability\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[B]  Bravery\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[C]  Combat Accuracy\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[E]  Experience\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[K]  Total Kills\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[O]  Owned Crystals\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[P]  Power\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[S]  Squadrons\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[W]  Wealth\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[H]  Hall of Fame\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Back to HQ\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Records>  é ", -1, NULL, 0, NULL);
			break;

		case 'k':
			codeStr++;

			term_setattr(LIGHTGREEN); cterm_write(cterm, "Supply Armor\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);  cterm_write(cterm, "~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);  term_setattr(WHITE);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			cterm_write(cterm, armors[zz], -1, NULL, 0, NULL);
			cterm_write(cterm, " / ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
			cterm_write(cterm, " Hits\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTCYAN);
			cterm_write(cterm, "-[B]  Buy Armor\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[S]  Sell Armor\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Back to Supply Room\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Supply Armor>  é ", -1, NULL, 0, NULL);
			break;

		case 'l':
			codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "Armor Types           Hits     Crystals\r\n", -1, NULL, 0, NULL); term_setattr(CYAN);
			cterm_write(cterm, "~~~~~~~~~~~           ~~~~     ~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTCYAN);

			for (zz=1;zz<5;zz++) {
				sprintf(buf,"-[%d]  - %s",zz,armors[zz]);
				strljust(buf,23,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL);

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
				cterm_write(cterm, buf, -1, NULL, 0, NULL);


				if (menuBlock[0]==48)
					sprintf(buf,"%ld",aPrice[zz]);
				else
					sprintf(buf,"%ld",aPrice[zz]*MultA);

				strrjust(buf,13,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL); cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);
			}
			break;


		case 'm' : codeStr++;

			if (codeStr[0]=='1') {
				term_setattr(LIGHTGREEN); cterm_write(cterm, "Supply Equipment\r\n", -1, NULL, 0, NULL);
				term_setattr(GREEN);  cterm_write(cterm, "~~~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			}
			else {
				term_setattr(LIGHTCYAN);
				cterm_write(cterm, "\r\n-[B]  Buy Equipment\r\n", -1, NULL, 0, NULL);
				cterm_write(cterm, "-[S]  Sell Equipment\r\n\r\n", -1, NULL, 0, NULL);
				cterm_write(cterm, "-[*]  Back to Supply Room\r\n\r\n", -1, NULL, 0, NULL);
				term_setattr(LIGHTBLUE);
				cterm_write(cterm, "-<Supply Equip>  é ", -1, NULL, 0, NULL);
			}
			break;

		case 'n' : codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "Equipment                 Crystals\r\n", -1, NULL, 0, NULL); term_setattr(CYAN);
			cterm_write(cterm, "~~~~~~~~~                 ~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTCYAN);

			for (zz=1;zz<13;zz++) {
				if (zz<=9)
					sprintf(buf,"-[%d]  - %s",zz,equips[zz]);
				else
					sprintf(buf,"-[%d] - %s",zz,equips[zz]);

				strljust(buf,28,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL);

				if (menuBlock[0]==48)
					sprintf(buf,"%ld",ePrice[zz]);
				else
					sprintf(buf,"%ld",ePrice[zz]*MultE);

				strrjust(buf,6,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL); cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);
			}

			cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);
			break;

		case 'o':
			codeStr++;
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);

			term_setattr(LIGHTGREEN); cterm_write(cterm, "Supply Suits\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);  cterm_write(cterm, "~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(WHITE);  cterm_write(cterm, "Suit : ", -1, NULL, 0, NULL);
			cterm_write(cterm, suits[zz], -1, NULL, 0, NULL);

			term_setattr(LIGHTCYAN);
			cterm_write(cterm, "\r\n\r\n-[B]  Buy a Suit\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[S]  Sell a Suit\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Back to Supply Room\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Supply Suits>  é ", -1, NULL, 0, NULL);
			break;

		case 'p' :
			codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(LIGHTBLUE); cterm_write(cterm, "Suits/Outfits               Crystals\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);  cterm_write(cterm, "~~~~~~~~~~~~~               ~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			for (zz=1;zz<3;zz++) {
				sprintf(buf,"-[%d]  - %s",zz,suits[zz]);
				strljust(buf,30,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL);

				if (menuBlock[0]==48)
					sprintf(buf,"%ld",sPrice[zz]);
				else
					sprintf(buf,"%ld",sPrice[zz]*MultS);

				strrjust(buf,6,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL); cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);
			}
			break;

		case 'q' : codeStr++;

			term_setattr(LIGHTGREEN);  cterm_write(cterm, "Supply Weapons\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);   cterm_write(cterm, "~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(WHITE);

			for (yy=1;yy<3;yy++) {
				getBlock(&codeStr,menuBlock);
				zz=atoi(menuBlock);
				cterm_write(cterm, weapons[zz], -1, NULL, 0, NULL); cterm_write(cterm, " / ", -1, NULL, 0, NULL);
				getBlock(&codeStr,menuBlock);
				zz=atoi(menuBlock);
				cterm_write(cterm, ammos[zz], -1, NULL, 0, NULL); cterm_write(cterm, " / ", -1, NULL, 0, NULL);
				getBlock(&codeStr,menuBlock);
				cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);  cterm_write(cterm, " Rds\r\n", -1, NULL, 0, NULL);
			}

			term_setattr(LIGHTCYAN);
			cterm_write(cterm, "\r\n-[B]  Buy Weapon\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[S]  Sell Weapon\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Back to Supply Room\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Supply Weapons>  é ", -1, NULL, 0, NULL);
			break;

		case 'r' : codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(LIGHTBLUE); cterm_write(cterm, "Hand-to-Hand Weapons        Crystals\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);  cterm_write(cterm, "~~~~~~~~~~~~~~~~~~~~        ~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			for (zz=1;zz<7;zz++) {
				sprintf(buf,"-[%d]  - %s",zz,weapons[zz]);
				strljust(buf,30,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL);

				if (menuBlock[0]==48)
					sprintf(buf,"%ld",wPrice[zz]);
				else
					sprintf(buf,"%ld",wPrice[zz]*MultW);

				strrjust(buf,6,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL); cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);
			}

			term_setattr(LIGHTBLUE); cterm_write(cterm, "\r\nLong-Range Weapons          Crystals\r\n", -1, NULL, 0, NULL);
			term_setattr(CYAN);  cterm_write(cterm, "~~~~~~~~~~~~~~~~~~          ~~~~~~~~\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			for (zz=7;zz<13;zz++) {
				if (zz<10)
					sprintf(buf,"-[%d]  - %s",zz,weapons[zz+1]);
				else
					sprintf(buf,"-[%d] - %s",zz,weapons[zz+1]);

				strljust(buf,30,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL);

				if (menuBlock[0]==48)
					sprintf(buf,"%ld",wPrice[zz+1]);
				else
					sprintf(buf,"%ld",wPrice[zz+1]*MultW);

				strrjust(buf,6,' ');
				cterm_write(cterm, buf, -1, NULL, 0, NULL);  cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);
			}
			break;

		case 's' : codeStr++;
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);

			term_setattr(LIGHTGREEN); cterm_write(cterm, "Squadrons Menu\r\n", -1, NULL, 0, NULL);
			term_setattr(GREEN);  cterm_write(cterm, "~~~~~~~~~~~~~~\r\n", -1, NULL, 0, NULL); term_setattr(LIGHTCYAN);

			cterm_write(cterm, "-[S]  Show Squadrons/Leaders\r\n", -1, NULL, 0, NULL);

			if (zz>0)
				cterm_write(cterm, "-[M]  Show Squadron Members\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);

			if (zz==0)
				cterm_write(cterm, "-[D]  Declare a Squadron\r\n", -1, NULL, 0, NULL);
			else
			if (zz<10)
				cterm_write(cterm, "-[Q]  Quit your Squadron\r\n", -1, NULL, 0, NULL);
			else
				cterm_write(cterm, "-[L]  Leader Options\r\n", -1, NULL, 0, NULL);

			if (zz==0)
				cterm_write(cterm, "-[R]  Recruit to a Squadron\r\n", -1, NULL, 0, NULL);

			cterm_write(cterm, "\r\n-[*]  Return to HQ\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(LIGHTBLUE); cterm_write(cterm, "-<Squadrons>  é ", -1, NULL, 0, NULL);
			break;

		case 't' : codeStr++;
			getBlock(&codeStr,menuBlock);

			term_setattr(WHITE); cterm_write(cterm, "Sighting : ", -1, NULL, 0, NULL);
			term_setattr(LIGHTMAGENTA); cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN);

			cterm_write(cterm, "\r\n\r\n-[L]  ", -1, NULL, 0, NULL);
			term_setattr(CYAN);
			cterm_write(cterm, "Long-Range Combat\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN); cterm_write(cterm, "-[H]  ", -1, NULL, 0, NULL);
			term_setattr(CYAN);
			cterm_write(cterm, "Hand-to-Hand Combat\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);

			if (!zz) {
				term_setattr(LIGHTCYAN); cterm_write(cterm, "-[R]  ", -1, NULL, 0, NULL);
				term_setattr(CYAN);   cterm_write(cterm, "Run like hell!\r\n", -1, NULL, 0, NULL);
			}

			term_setattr(LIGHTCYAN); cterm_write(cterm, "\r\n-[C]  ", -1, NULL, 0, NULL); term_setattr(MAGENTA); cterm_write(cterm, "Check Status\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN); cterm_write(cterm,   "-[V]  ", -1, NULL, 0, NULL); term_setattr(MAGENTA); cterm_write(cterm, "View Opponent\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTCYAN); cterm_write(cterm,   "-[M]  ", -1, NULL, 0, NULL); term_setattr(LIGHTGRAY); cterm_write(cterm, "Toggle Combat Modes\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);  cterm_write(cterm, "-<Combat>  é ", -1, NULL, 0, NULL);
			break;


		case 'u' : codeStr++;
			getBlock(&codeStr,menuBlock);
			strcpy(tempBlock,menuBlock);

			term_setattr(WHITE);  cterm_write(cterm, "Camp :[ ", -1, NULL, 0, NULL);
			term_setattr(LIGHTGREEN); cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
			term_setattr(WHITE);  cterm_write(cterm, " ]:\r\n", -1, NULL, 0, NULL); term_setattr(RED);

			for (zz=0;zz<(10+strlen(menuBlock));zz++) cterm_write(cterm, "~", -1, NULL, 0, NULL);
			cterm_write(cterm, "\r\n", -1, NULL, 0, NULL);

			term_setattr(CYAN);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz>0)
				cterm_write(cterm, "-[A]  Attack recruit\r\n", -1, NULL, 0, NULL);

			cterm_write(cterm, "-[L]  Leave a message\r\n\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (!zz)
				cterm_write(cterm, "-[B]  Borrow camper's items\r\n", -1, NULL, 0, NULL);
			else
				cterm_write(cterm, "-[S]  Steal camper's items\r\n", -1, NULL, 0, NULL);

			cterm_write(cterm, "-[G]  Give camper items\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[V]  View ", -1, NULL, 0, NULL);
			cterm_write(cterm, tempBlock, -1, NULL, 0, NULL);

			cterm_write(cterm, "\r\n-[C]  Check your status\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[D]  Drop your items\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[P]  Pick up items\r\n\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[*]  Leave campsite\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE); cterm_write(cterm, "-<Camp>  é ", -1, NULL, 0, NULL);
			break;

		case 'v':
			codeStr++;
			term_setattr(LIGHTGREEN); cterm_write(cterm, "Hit Points   :[ ", -1, NULL, 0, NULL); term_setattr(WHITE);
			getBlock(&codeStr,menuBlock);
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL); cterm_write(cterm, " ", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			sprintf(buf,"(%s)\r\n",menuBlock);
			cterm_write(cterm, buf, -1, NULL, 0, NULL);

			term_setattr(LIGHTGREEN); cterm_write(cterm, "Applications :[ ", -1, NULL, 0, NULL); term_setattr(WHITE);
			getBlock(&codeStr,menuBlock);
			sprintf(buf,"%s current, ",menuBlock);
			cterm_write(cterm, buf, -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			sprintf(buf,"%s total\r\n",menuBlock);
			cterm_write(cterm, buf, -1, NULL, 0, NULL);

			term_setattr(LIGHTGREEN); cterm_write(cterm, "Radiation    :[ ", -1, NULL, 0, NULL); term_setattr(WHITE);
			getBlock(&codeStr,menuBlock);
			cterm_write(cterm, menuBlock, -1, NULL, 0, NULL); cterm_write(cterm, "%\r\n\r\n", -1, NULL, 0, NULL);

			term_setattr(WHITE); cterm_write(cterm, "Also, you are ", -1, NULL, 0, NULL);
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			switch (zz) {
				case 0 :
					cterm_write(cterm, "not very hungry.\r\n", -1, NULL, 0, NULL);
					break;
				case 1 :
					cterm_write(cterm, "a little hungry.\r\n", -1, NULL, 0, NULL);
					break;
				case 2 :
					cterm_write(cterm, "hungry.\r\n", -1, NULL, 0, NULL);
					break;
				case 3 :
					cterm_write(cterm, "extremely hungry!\r\n", -1, NULL, 0, NULL);
					break;
				case 4 :
					term_setattr(LIGHTRED);
					cterm_write(cterm, "DYING ", -1, NULL, 0, NULL);
					term_setattr(WHITE);
					cterm_write(cterm, "of hunger!!\r\n", -1, NULL, 0, NULL);
					break;
			}

			term_setattr(YELLOW);
			cterm_write(cterm, "\r\n-[Q]  Quick Heal\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "-[U]  Use an Application\r\n", -1, NULL, 0, NULL);

			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			if (zz==1)
				cterm_write(cterm, "-[R]  Use Rations\r\n", -1, NULL, 0, NULL);

			cterm_write(cterm, "-[*]  Exit\r\n\r\n", -1, NULL, 0, NULL);
			term_setattr(LIGHTBLUE);
			cterm_write(cterm, "-<Medpacks>  é ", -1, NULL, 0, NULL);
			break;
	}


	return(codeStr-origCodeStr);;
}

static void checkStamp(int xx,int yy,char stampStr[20]) {  // used w/ incomingCheck
	term_gotoxy(xx+1, yy+1);
	term_setattr(LIGHTCYAN);
	cterm_write(cterm, stampStr, -1, NULL, 0, NULL);
	term_setattr(CYAN);
	cterm_write(cterm, "é", -1, NULL, 0, NULL);
	return;
}

static int incomingCheckStatus(unsigned char *codeStr) {

	int who,zz;
	char menuBlock[255];
	unsigned char *origCodeStr=codeStr;

	term_clearscreen();
	term_gotoxy(1,1);

	codeStr++;
	getBlock(&codeStr,menuBlock);
	who=atoi(menuBlock);

	term_setattr(who);
	cterm_write(cterm, "\r\nÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\r\n", -1, NULL, 0, NULL);
	term_setattr(LIGHTCYAN);
	cterm_write(cterm, "Recruit   ", -1, NULL, 0, NULL);
	term_setattr(CYAN);
	cterm_write(cterm, "é", -1, NULL, 0, NULL);
	term_setattr(who);
	term_gotoxy(34,3);
	cterm_write(cterm, " ³", -1, NULL, 0, NULL);
	term_gotoxy(1,4);
	cterm_write(cterm, "ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÏÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ", -1, NULL, 0, NULL);

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
	cterm_write(cterm, "ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ", -1, NULL, 0, NULL);

	checkStamp(0,8,"Crystals  ");
	checkStamp(27,8,"Disease    ");
	checkStamp(54,8,"Total Kills   ");
	checkStamp(0,9,"Exp. Pts  ");
	checkStamp(27,9,"Level      ");
	checkStamp(54,9,"Hunger Level  ");

	term_gotoxy(1,11);
	term_setattr(who);
	cterm_write(cterm, "ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ", -1, NULL, 0, NULL);

	checkStamp(0,11,"Weapon    ");
	checkStamp(0,12,"Weapon    ");

	term_gotoxy(1, 14);
	term_setattr(who);
	cterm_write(cterm, "ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ", -1, NULL, 0, NULL);

	checkStamp(0,14,"Armor     ");
	checkStamp(0,15,"Outfit    ");

	term_gotoxy(1,17);
	term_setattr(who);
	cterm_write(cterm, "ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ", -1, NULL, 0, NULL);

	for (zz=1;zz<4;zz++)
		checkStamp(0,16+zz,"Equipment ");

	term_gotoxy(1, 21);
	term_setattr(who);
	cterm_write(cterm, "ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ", -1, NULL, 0, NULL);

	term_gotoxy(1, 23);

	return(codeStr-origCodeStr);;
}

char * scanChar(unsigned char s, int where, int miniTrik) {
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

static int incomingMapScanner(unsigned char *codeStr)
{

	int zz,scanPtr,yy;
	unsigned char scanVals[10];       // ScanVals  : ARRAY[1..9] OF BYTE;
	char scan[30];
	char menuBlock[255];
	unsigned char *origCodeStr=codeStr;
	int  where;
	int  miniTrik;

	codeStr++;

	switch ((char)codeStr[0]) {
		case 'F' :
			term_clearscreen();
			term_gotoxy(1, 1);
			term_setattr(1);
			cterm_write(cterm, "\r\nÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, "ÖÄ", -1, NULL, 0, NULL); term_setattr(13); cterm_write(cterm, "Infrared", -1, NULL, 0, NULL); term_setattr(5);
			cterm_write(cterm, "Ä·", -1, NULL, 0, NULL); term_setattr(13); cterm_write(cterm, "     Compass      Sector Monitor     System Monitor           ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write(cterm, "³\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, "Ç          ¶     ~~~~~~~      ~~~~~~~~~~~~~~     ~~~~~~~~~~~~~~           ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write(cterm, "³\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, "Ç          ¶   [         ]    [ ]  ", -1, NULL, 0, NULL); term_setattr(13);
			cterm_write(cterm, "Items         Hit Points", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, " [:            ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write(cterm, "³\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, "Ç          ¶                  [ ]  ", -1, NULL, 0, NULL); term_setattr(13);
			cterm_write(cterm, "Campers       Radiation  ", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, "[:            ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write(cterm, "³\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "³ ", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, "ÓÄÄÄ", -1, NULL, 0, NULL); term_setattr(13); cterm_write(cterm, "scan", -1, NULL, 0, NULL); term_setattr(5);
			cterm_write(cterm, "ÄÄÄ½    ", -1, NULL, 0, NULL); term_setattr(13); cterm_write(cterm, "Time :        ", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, "[ ]  ", -1, NULL, 0, NULL);
			term_setattr(13); cterm_write(cterm, "Bases         Condition  ", -1, NULL, 0, NULL); term_setattr(5); cterm_write(cterm, "[:            ", -1, NULL, 0, NULL);
			term_setattr(1); cterm_write(cterm, "³\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ\r\n", -1, NULL, 0, NULL);
			break;

		case 'T' :
			term_gotoxy(1, 9);
			term_setattr(1);
			cterm_write(cterm, "ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "³                                                                           ³\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "³                                                                           ³\r\n", -1, NULL, 0, NULL);
			cterm_write(cterm, "ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ\r\n", -1, NULL, 0, NULL);
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
					cterm_write(cterm, scan, -1, NULL, 0, NULL);
					cterm_write(cterm, " ", -1, NULL, 0, NULL);
				}
				else
					cterm_write(cterm, scan, -1, NULL, 0, NULL);
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
						cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
						break;

					case 2 :
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock,",");
						cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
						break;

					case 3 :
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock," ");
						cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
						break;

					case 4 : 
						term_gotoxy(26,7);
						term_setattr(15);
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock," ");
						cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
						break;

					case 5 : 
						term_gotoxy(66,5);
						term_setattr(14);
						getBlock(&codeStr,menuBlock);
						strcat(menuBlock," ");
						cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
						break;

					case 6 :
						getBlock(&codeStr,menuBlock);
						cterm_write(cterm, "(", 1, NULL, 0, NULL);
						cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
						cterm_write(cterm, ")", 1, NULL, 0, NULL);
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
						cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
						break;

					case 8 :
						term_gotoxy(66,7);
						term_setattr(14);
						getBlock(&codeStr,menuBlock);
						yy=atoi(menuBlock);

						switch (yy) {
							case 0 : cterm_write(cterm, "Not Hungry", -1, NULL, 0, NULL); break;
							case 1 : cterm_write(cterm, "Bit Hungry", -1, NULL, 0, NULL); break;
							case 2 : cterm_write(cterm, "Hungry    ", -1, NULL, 0, NULL); break;
							case 3 : term_setattr(12);
								cterm_write(cterm, "HUNGRY!   ", -1, NULL, 0, NULL); break;
							case 4 : term_setattr(12+128);
								cterm_write(cterm, "STARVING!!", -1, NULL, 0, NULL); break;
						}
						break;
				}
			}

			codeStr++;
			if (codeStr[0]=='I') {
				codeStr++;
				term_gotoxy(34,5);

				if (codeStr[0]=='0')
					cterm_write(cterm, " ", -1, NULL, 0, NULL);
				else
				{
					term_setattr(12);
					cterm_write(cterm, "û", -1, NULL, 0, NULL);
				}

				codeStr++;
				codeStr++;
			}

			if (codeStr[0]=='C') {
				codeStr++;
				term_gotoxy(34,6);

				if (codeStr[0]=='0')
					cterm_write(cterm, " ", -1, NULL, 0, NULL);
				else
				{
					term_setattr(12);
					cterm_write(cterm, "û", -1, NULL, 0, NULL);
				}

				codeStr++;
				codeStr++;
			}

			if (codeStr[0]=='B') {
				codeStr++;
				term_gotoxy(34,7);

				if (codeStr[0]=='0')
					cterm_write(cterm, " ", -1, NULL, 0, NULL);
				else
				{
					term_setattr(12);
					cterm_write(cterm, "û", -1, NULL, 0, NULL);

					codeStr++;
					codeStr++;

					getBlock(&codeStr,menuBlock);
					yy=atoi(menuBlock);
					term_gotoxy(1,yy+1);

					getBlock(&codeStr,menuBlock);
					term_setattr(14);
					strcat(menuBlock," is stationed nearby.");
					cterm_write(cterm, menuBlock, -1, NULL, 0, NULL);
				}
			}
			break;


		case 'D' :
			codeStr++;
			getBlock(&codeStr,menuBlock);
			zz=atoi(menuBlock);
			term_gotoxy(3,10);
			cterm_write(cterm, "                                                                          ", -1, NULL, 0, NULL);
			term_gotoxy(3,11);
			cterm_write(cterm, "                                                                          ", -1, NULL, 0, NULL);
			term_gotoxy(3,10);

			term_setattr(11);

			switch (zz) {
				case 2 : cterm_write(cterm, "Crumbling walls of ancient cities scatter the plains, and smoke lazily", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write(cterm, "rises from the destructed relics of the past.", -1, NULL, 0, NULL);
					break;

				case 3 : cterm_write(cterm, "Coldness creeps upon you like death in this vast range of empty desert.", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write(cterm, "Harsh winds rip sand against your body like tiny shards of glass.", -1, NULL, 0, NULL);
					break;

				case 4 : cterm_write(cterm, "The black muck from the swamp slithers between your toes as you make your", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write(cterm, "way through the bubbling slime.", -1, NULL, 0, NULL);
					break;

				case 5 : cterm_write(cterm, "High upon the scorched mountains, small blackening caves can be seen.", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write(cterm, "Possibly lurking within, horrible creatures wait for your trespassing.", -1, NULL, 0, NULL);
					break;

				case 6 : cterm_write(cterm, "A mist glides along the flat terrain and summons you forward.", -1, NULL, 0, NULL);
					break;

				case 7 : ; break; // pits

				case 8 : cterm_write(cterm, "A small hole in the ground leads down into a realm of darkness.", -1, NULL, 0, NULL);
					break;

				case 9 : cterm_write(cterm, "From the pre-war years, an Airforce Base lies half buried from nuclear", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write(cterm, "ash and metallic scraps.", -1, NULL, 0, NULL);
					break;

				case 10: cterm_write(cterm, "A disabled missile silo remains intact from heavy bombing in this area.", -1, NULL, 0, NULL);
					break;

				case 11: cterm_write(cterm, "In an effortless HeXonium attempt to clean the wastelands, radiation dumps", -1, NULL, 0, NULL);
					term_gotoxy(3,11);
					cterm_write(cterm, "lay wretched with oozing radioactive substances forming.", -1, NULL, 0, NULL);
					break;

				case 12: cterm_write(cterm, "This unmarked road is littered with dead shrubs and rotting vegatation.", -1, NULL, 0, NULL);
					break;

				case 13: cterm_write(cterm, "Darkness swirls around the infamous and towering Hydrite Prison.", -1, NULL, 0, NULL);
					break;

				case 14: cterm_write(cterm, "As you look up, you notice a small hole that leads upwards.", -1, NULL, 0, NULL);
					break;

				case 15: cterm_write(cterm, "You cautiously wade through the black murky water of this shallow stream.", -1, NULL, 0, NULL);
					break;
			}
			break;

		case 'P' : codeStr++;
			term_gotoxy(3,10);
			cterm_write(cterm, "                                                                          ", -1, NULL, 0, NULL);
			term_gotoxy(3,11);
			cterm_write(cterm, "                                                                          ", -1, NULL, 0, NULL);
			term_gotoxy(3,10);
			term_setattr(11);
			cterm_write(cterm, "20 foot pit!", -1, NULL, 0, NULL);
			break;
	}

	return(codeStr-origCodeStr);;
}

/* TODO: Sound support */
static int incomingSoundVoc(unsigned char *codeStr) {
	unsigned char *origCodeStr=codeStr;

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

BOOL handle_ooii_code(unsigned char *codeStr, int *ooii_mode, unsigned char *retbuf, size_t retsize)
{
	BOOL	quit=FALSE;
	char	menuBlock[255];
	int		zz;

	if(retbuf!=NULL)
		retbuf[0]=0;

	codeStr++;	/* Skip intro char */

	for(;*codeStr && *codeStr != '|'; codeStr++) {
		if ( codeStr[0]>='A' && codeStr[0]<='Z') {
			/* This one never takes an extra char */
			readInPix(codeStr[0], *ooii_mode);
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
					readInPix(codeStr[0], *ooii_mode);
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
						getBlock(&codeStr,menuBlock);
						zz=atoi(menuBlock);
						/* Highest we support is two */
						if(zz >= MAX_OOII_MODE)
							zz=MAX_OOII_MODE-1;
						/* Old (1.22) versions don't include a number */
						if(zz < 1)
							zz=1;
						*ooii_mode=zz+1;
						if(strlen((char *)retbuf)+3 < retsize)
							sprintf((char *)retbuf, "\xaf%d|", zz);
					}
					break;
			}
		}
	}
	return(quit);
}

