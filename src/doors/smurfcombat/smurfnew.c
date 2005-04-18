#define DOSMODEONLY
#ifdef TODO_HEADERS
#include<process.h>
#include<dos.h>
#endif
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<ciolib.h>
#include<genwrap.h>
#include"smurfnew.h"
#include"smurfgen.h"
#include"smurfver.h"
#include"smurfsav.h"
#include"mdel.h"

void 
main(void)
{
    getdate(&d);
    noplayers = 4;
    clrscr();
    textcolor(12);
    cprintf("\n   S M U R F   C O M B A T\n\n\r");
    textcolor(10);
    cprintf("  New Game (Re-Roll) Module\n\r");
    cprintf("        %sVersion %s\n\r", __vnewsp, __vnew);
    cprintf("     By Laurence Manhken\n\r");
    textcolor(14);
    cprintf("      AutoYea in 10secs\n\n\n\r");
    textcolor(9);
    cprintf("Proceed [Yn]: ");
    textcolor(12);
    if ((stream = fopen("smurf.sgm", "r+")) == NULL)
	tcount = 999;
    else
	fclose(stream);

    do {
	tcount++;
	od_sleep(10);
    } while (!kbhit() && tcount < 1000);
    if (tcount > 999)
	timeoud = 1;
    else
	proc = getch();
    if (proc != 'Y' && proc != 'y' && timeoud != 1) {
	cprintf("Nope\n\r");
	return;
    }
    if (timeoud != 1)
	cprintf("Yea\n\n\r");
    if (timeoud == 1)
	cprintf("Yea (Forced)\n\n\r");
    stream = fopen("smurf.sgm", "w+");
    fprintf(stream, "%03i%05.5s", 0, __saveversion);
    fclose(stream);
    textcolor(13);
    cprintf("Checking Configuration: ");
    if ((stream = fopen("smurf.cfg", "r+")) == NULL) {
	stream = fopen("smurf.cfg", "w+");
	fprintf(stream, "01000311111990");
	fprintf(stream, "\n\r\n\r1. Arena Rounds/Day ###\n\r2. Turns/Day ###\n\r3. Last Ran Date DDMMYYYY\n\r");
	fclose(stream);
    } else {
	fscanf(stream, "%3s", intext);
	defrounds = atoi(intext);
	fscanf(stream, "%3s", intext);
	turnsaday = atoi(intext);
	fscanf(stream, "%8s", olddate);
	fclose(stream);
	stream = fopen("smurf.cfg", "w+");
	fprintf(stream, "%03i", defrounds);
	fprintf(stream, "%03i", turnsaday);
	fprintf(stream, "01011990");
	fprintf(stream, "\n\r\n\r1. Arena Rounds/Day\n\r2. Turns/Day\n\r3. Last Ran Date DDMMYYYY\n\r");
	fclose(stream);
    }

    od_sleep(500);
    textcolor(14);
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Reseting Variables: ");
    sleep(1);
    textcolor(14);
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Writing Computer Players: ");
    savegame();
    textcolor(14);
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Saving Save Game File: ");
    od_sleep(200);
    textcolor(14);
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Writing Name Listing: ");
    stream = fopen("smurf.scl", "w+");
    od_sleep(200);
    fprintf(stream, "NAME LISTING:)\n\r");
    fprintf(stream, "[0-CPU] #1:) GRANDPA SMURF\n\r");
    fprintf(stream, "[0-CPU] #2:) PAPA SMURF\n\r");
    fprintf(stream, "[0-CPU] #3:) SHADOW SMURF\n\r");
    fprintf(stream, "[0-CPU] #4:) REBEL SMURF\n\r");
    fclose(stream);
    textcolor(14);
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Writing Error Log File: ");
    stream = fopen("smurf.sel", "w+");
    od_sleep(200);
    fprintf(stream, "ERROR RECORDS:)\n\r");
    fprintf(stream, "LOCALDOS:) ***Ran Re-Roll program\n\r");
    fclose(stream);
    textcolor(14);
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Writing Player Logon Log File: ");
    stream = fopen("smurf.sgl", "w+");
    od_sleep(200);
    fclose(stream);
    textcolor(14);
    cprintf("Success.\n\r");

    textcolor(13);
    cprintf("Writing Personal Log Files: ");
    od_sleep(200);
    stream = fopen("smurflog.xxx", "w+");
    fclose(stream);
    mdel("SMURFLOG.*");
    textcolor(14);
    cprintf("Success.\n\r");
    textcolor(13);
    cprintf("Writing Player Hostage File: ");
    od_sleep(200);
    stream = fopen("smurf.hst", "w+");
    fprintf(stream, "255255");
    fclose(stream);
    mdel("SMURF.D0D");
    textcolor(14);
    cprintf("Success.\n\r");
}
