/***************************************************************************/
/* */
/* sss                       fff     ccc                 b               */
/* s   s                     f   f   c   c                b               */
/* s                         f       c                    b               */
/* sss  mmm mmm  u   u r rr  fff    c      ooo  mmm mmm   bbb  aaa  ttt  */
/* s m  m   m u   u rr   f       c     o   o m  m   m b   b a  a  t   */
/* s   s m  m   m u  uu r    f       c   c o   o m  m   m b   b aaaa  t   */
/* sss  m  m   m  uu u r    f        ccc   ooo  m  m   m  bbb  a  a  t   */
/* */
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                          DXXRoll  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                           Module  */
/* */
/***************************************************************************/







/* #define DOSMODEONLY */
/* #include"smurfver.h" */
/* #include"smurfgen.h" */
#include"smurfext.h"
/* #include"smurfdef.h" */
/* #include"smurfdat.h" */
/* #include"smurfsav.h" */
#include"smurfbak.h"

int             lgain[50] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
float           gain[50] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

unsigned long   freecost;
FILE           *logstream;
char            in2[40], senform[81];

void 
__DAY__checklevel(void)
{
    int             getthis;
    if (smurfexper[cyc] > defexper[smurflevel[cyc]]) {
	smurflevel[cyc]++;
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfstr[cyc]++;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfspd[cyc]++;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfint[cyc]++;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfcon[cyc]++;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfbra[cyc]++;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfchr[cyc]++;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfstr[cyc] += 6;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfspd[cyc] += 6;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfint[cyc] += 6;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfcon[cyc] += 6;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfbra[cyc] += 6;
	}
	getthis = xp_random(8);
	if (getthis < 3) {
	    smurfchr[cyc] += 6;
	}
	getthis = ((smurfcon[cyc] * .80) + (xp_random(smurfcon[cyc] / 10)));
	smurfhpm[cyc] = smurfhpm[cyc] + getthis;
	smurfhp[cyc] = smurfhpm[cyc];
	lgain[cyc]++;
    }
}

void 
__DAY__main(void)
{
    char            newdate[10], olddate[10];
    int             turnsaday, defrounds, mod;
    struct date     d;
    char            intext[81];
    int             ef = 0, gf = 0;
    int             hostage[1000], holder[1000], hcount;
    getdate(&d);

#ifdef TODO_LOCAL_DISPLAY
    textcolor(12);
    cprintf("\n   S M U R F   C O M B A T\n\n\r");
    textcolor(10);
    cprintf("  Maintenance (24hr) Module\n\r");
    cprintf("        %sVersion %s\n\r", __vdaysp, __vday);
    cprintf("     By Laurence Manhken\n\r");
    textcolor(14);
    cprintf("        Daily Routine\n\n\n\r");
    textcolor(9);
    cprintf("Proceed [Yn]: ");
    od_sleep(200);
    textcolor(12);
    cprintf("Yea (Automatic)\n\n\r");
    textcolor(13);
    cprintf("Reading Configuration: ");
    od_sleep(200);
    textcolor(14);
    cprintf("Success.\n\r");
#endif
    sprintf(newdate, "%02d%02d%04d", d.da_day, d.da_mon, d.da_year);
    stream = fopen("smurf.cfg", "r+");
    fscanf(stream, "%3s", intext);
    defrounds = atoi(intext);
    fscanf(stream, "%3s", intext);
    turnsaday = atoi(intext);
    fscanf(stream, "%8s", olddate);
    fclose(stream);
    stream = fopen("smurf.cfg", "w+");
    fprintf(stream, "%03i", defrounds);
    fprintf(stream, "%03i", turnsaday);
    fprintf(stream, "%8s", newdate);
    fprintf(stream, "\n\r\n\r1. Arena Rounds/Day ###\n\r2. Turns/Day ###\n\r3. Last Ran Date DDMMYYYY\n\r");
    fclose(stream);
#ifdef TODO_LOCAL_DISPLAY
    textcolor(13);
    cprintf("Loading Smurf Players: ");
    od_sleep(200);
    textcolor(14);
#endif
    loadgame();
    backgame();
#ifdef TODO_LOCAL_DISPLAY
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Loading Hostage Intellegence: ");
    od_sleep(200);
    textcolor(14);
    for (cyc = 0; cyc < noplayers; cyc++) {
	od_sleep(50);
	cprintf(".");
    }
    cprintf("Success.\n\r");
#endif

    stream = fopen("smurf.hst", "r+");
    cyc = 0;
    hcount = 0;
    do {
	fscanf(stream, "%3s%3s", intext, in2);
	hostage[cyc] = atoi(intext);
	holder[cyc] = atoi(in2);
	cyc++;
	hcount++;
    } while (feof(stream) == 0);
    fclose(stream);

    for (cyc2 = 0; cyc2 < noplayers; cyc2++) {
	numhost[cyc2] = 0;
	for (cyc = 0; cyc < hcount; cyc++)
	    if (holder[cyc] == thisuserno) {
		numhost[cyc2]++;
	    }
    }

#ifdef TODO_LOCAL_DISPLAY
    textcolor(13);
    cprintf("Reseting Variables: ");
#endif

    stream = fopen("smurf.hst", "a+");
    fclose(stream);
    stream = fopen("smurf.hst", "r+");
    cyc = 0;
    hcount = 0;
    do {
	fscanf(stream, "%3s%3s", intext, in2);
	hostage[cyc] = atoi(intext);
	holder[cyc] = atoi(in2);
	cyc++;
	hcount++;
    } while (feof(stream) == 0);
    fclose(stream);

    stream = fopen("smurf.hst", "w+");
    fprintf(stream, "255255");
    for (cyc = 0; cyc < hcount; cyc++)
	if (hostage[cyc] != 255 && holder[cyc] != 255)
	    fprintf(stream, "%03d%03d", hostage[cyc], holder[cyc]);
    fclose(stream);

    stream = fopen("smurf.hst", "r+");
    cyc = 0;
    hcount = 0;
    do {
	fscanf(stream, "%3s%3s", intext, in2);
	hostage[cyc] = atoi(intext);
	holder[cyc] = atoi(in2);
	cyc++;
	hcount++;
    } while (feof(stream) == 0);
    fclose(stream);

    sleep(1);
#ifdef TODO_LOCAL_DISPLAY
    textcolor(14);
    for (cyc = 0; cyc < noplayers; cyc++) {
	od_sleep(50);
	cprintf(".");
	smurfturns[cyc] = turnsaday;
    }
    cprintf("Success.\n\r");

    textcolor(13);
    cprintf("Animating Cpu Players: ");
    od_sleep(100);
    textcolor(14);
#endif
    for (cyc = 0; cyc < noplayers; cyc++) {
	if (realnumb[cyc] == 0) {

	    for (cyc2 = 0; cyc2 < hcount; cyc2++)
		if (hostage[cyc2] == cyc) {
		    if (smurflevel[hostage[cyc2]] > 0)
			freecost = 1000;
		    if (smurflevel[hostage[cyc2]] > 5)
			freecost = 20000;
		    if (smurflevel[hostage[cyc2]] > 10)
			freecost = 50000;
		    if (smurflevel[hostage[cyc2]] > 20)
			freecost = 100000;
		    if (smurfmoney[hostage[cyc2]] < freecost)
			smurfmoney[hostage[cyc2]] = freecost;
		    smurfmoney[hostage[cyc2]] -= freecost;
		    smurfmoney[holder[cyc2]] += freecost;
		    /* if(smurfmoney[hostage[cyc2]]<1)smurfmoney[cyc2]=0; */
		    sprintf(senform, "smurflog.%03d", holder[cyc2]);
		    logstream = fopen(senform, "a+");
		    fprintf(logstream, "%s payed %lu for freedom!\n\r", smurfname[hostage[cyc2]], freecost);
		    fclose(logstream);
		    hostage[cyc2] = 255;
		    holder[cyc2] = 255;
#ifdef TODO_LOCAL_DISPLAY
		    od_sleep(200);
		    cprintf("$");
#endif
		}
#ifdef TODO_LOCAL_DISPLAY
	    od_sleep(200);
	    cprintf("@");
#endif
	    for (cyc3 = 0; cyc3 < turnsaday; cyc3++) {
		mod = smurflevel[cyc] / 2;
		if (mod < 1)
		    mod = 1;
		for (cyc2 = 0; cyc2 < defrounds; cyc2++) {
		    gf = xp_random(2 * 100 * (mod)) + 69 * mod;
		    ef = xp_random(2 * 100 * (mod)) + 69 * mod;
		    smurfmoney[cyc] = (smurfmoney[cyc] + gf);
		    smurfexper[cyc] = (smurfexper[cyc] + ef);
		    gain[cyc] = gain[cyc] + ef;
		}
		__DAY__checklevel();
		__DAY__checklevel();
	    }
	} else {
#ifdef TODO_LOCAL_DISPLAY
	    od_sleep(50);
	    cprintf(".");
#endif
	}
    }
#ifdef TODO_LOCAL_DISPLAY
    cprintf("Success.\n\r");

    textcolor(13);
    cprintf("Saving Player Game File: ");
#endif
    stream = fopen("smurf.hst", "w+");
    fprintf(stream, "255255");
    for (cyc = 0; cyc < hcount; cyc++)
	if (hostage[cyc] != 255 && holder[cyc] != 255)
	    fprintf(stream, "%03d%03d", hostage[cyc], holder[cyc]);
    fclose(stream);

    stream = fopen("smurf.hst", "r+");
    cyc = 0;
    hcount = 0;
    do {
	fscanf(stream, "%3s%3s", intext, in2);
	hostage[cyc] = atoi(intext);
	holder[cyc] = atoi(in2);
	cyc++;
	hcount++;
    } while (feof(stream) == 0);
    fclose(stream);

#ifdef TODO_LOCAL_DISPLAY
    od_sleep(200);			       /* stream = fopen("smurf.log",
				        * "a+");fprintf(stream, "SPAWN:)
				        * Logon Maintenance.\n\r");
				        * fclose(stream); */
    textcolor(14);
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Writing Today Log File: ");
#endif
    stream = fopen("smurf.new", "w+");
#ifdef TODO_LOCAL_DISPLAY
    od_sleep(200);
#endif
    for (cyc = 0; cyc < noplayers; cyc++) {
	if (realnumb[cyc] == 0) {
	    if (gain[cyc] > 0)
		fprintf(stream, "%s gained %.0f experience\n\r", smurfname[cyc], gain[cyc]);
	    if (lgain[cyc] > 1)
		fprintf(stream, "%s gained %i levels!\n\r", smurfname[cyc], lgain[cyc]);
	    if (lgain[cyc] == 1)
		fprintf(stream, "%s gained a level!\n\r", smurfname[cyc]);
	}
    }
    fclose(stream);
    savegame();
#ifdef TODO_LOCAL_DISPLAY
    textcolor(14);
    cprintf("Success.\n\r");	       /* stream = fopen("smurf.log",
				        * "a+");fprintf(stream, "SPAWN:)
				        * Disconnect Maintenance.\n\r");
				        * fclose(stream); */
#endif
}
