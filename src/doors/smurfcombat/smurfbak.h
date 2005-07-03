#include<datewrap.h>
void 
backgame(void)
{
    char            backname[20];
    struct date     d;
    int             thp;
    getdate(&d);
    sprintf(backname, "smurf.s%02.2d", d.da_day);
    stream = fopen(backname, "w+");
    fprintf(stream, "%03i", noplayers);
    for (cyc = 0; cyc < noplayers; cyc++) {
	if (smurfturns[cyc] < 0)
	    smurfturns[cyc] = 0;
	if (smurffights[cyc] < 0)
	    smurffights[cyc] = 0;
	thp = smurfhpm[cyc];
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", realname[cyc][cyc2]);
	fprintf(stream, "%010i", realnumb[cyc]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfname[cyc][cyc2]);
	for (cyc2 = 0; cyc2 < 80; cyc2++)
	    fprintf(stream, "%03i", smurftext[cyc][cyc2]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfweap[cyc][cyc2]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfarmr[cyc][cyc2]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfettename[cyc][cyc2]);
	fprintf(stream, "%010i", smurfettelevel[cyc]);
	fprintf(stream, "%010i", smurfweapno[cyc]);
	fprintf(stream, "%010i", smurfarmrno[cyc]);
	for (cyc3 = 0; cyc3 < 40; cyc3++)
	    fprintf(stream, "%03i", smurfconf[cyc][cyc3]);
	fprintf(stream, "%010i", smurfconfno[cyc]);
	for (cyc3 = 0; cyc3 < 10; cyc3++) {
	     /* 69d */ fprintf(stream, "%010i", smurfhost[cyc][cyc3]);
	}

	/* 69a     fprintf(stream, "%010i",smurfhost[cyc]);fprintf(stream,
	 * "%010i",smurfhost1[cyc]);fprintf(stream, "%010i",smurfhost2[cyc]); */
	fprintf(stream, "%010i", smurflevel[cyc]);
	fprintf(stream, "%020.0f", smurfexper[cyc]);
	fprintf(stream, "%020.0f", smurfmoney[cyc]);
	fprintf(stream, "%020.0f", smurfbankd[cyc]);
	fprintf(stream, "%010i", smurffights[cyc]);
	fprintf(stream, "%010i", smurfwin[cyc]);
	fprintf(stream, "%010i", smurflose[cyc]);
	fprintf(stream, "%010i", thp);
	fprintf(stream, "%010i", smurfhpm[cyc]);
	fprintf(stream, "%010i", smurfstr[cyc]);
	fprintf(stream, "%010i", smurfspd[cyc]);
	fprintf(stream, "%010i", smurfint[cyc]);
	fprintf(stream, "%010i", smurfcon[cyc]);
	fprintf(stream, "%010i", smurfbra[cyc]);
	fprintf(stream, "%010i", smurfchr[cyc]);
	fprintf(stream, "%010i", smurfturns[cyc]);

	 /* 91 */ if (__morale[cyc] >= 13) {
	    __morale[cyc] = 13;
	}
	 /* 91 */ if (__ettemorale[cyc] >= 999) {
	    __ettemorale[cyc] = 999;
	}
	 /* 91 */ if (__morale[cyc] < 1) {
	    __morale[cyc] = 0;
	}
	 /* 91 */ if (__ettemorale[cyc] < 1) {
	    __ettemorale[cyc] = 0;
	}
	 /* 91 */ fprintf(stream, "%03i%03i", __morale[cyc], __ettemorale[cyc]);

	for (cyc3 = 0; cyc3 < 5; cyc3++) {
	     /* 69b */ fprintf(stream, "%03i%03i", smurfspcweapon[cyc][cyc3], smurfqtyweapon[cyc][cyc3]);	/* 111222 - Special Weap
														 * Number / Qty */
	}
	 /* 69c */ fprintf(stream, "%03i", smurfsex[cyc]);	/* 000/001/002
								 * Nil/Mal/Fem */

    }
    /* ToDo: Figure out why this was here and what it was trying to do. */
    /* fclose(stream); */
    fprintf(stream, "%03i", noplayers);
    for (cyc = 0; cyc < noplayers; cyc++) {
#ifdef TODO_LOCAL_DISPLAY
	od_sleep(50);
	cprintf(".");
#endif
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", realname[cyc][cyc2]);
	fprintf(stream, "%010i", realnumb[cyc]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfname[cyc][cyc2]);
	for (cyc2 = 0; cyc2 < 80; cyc2++)
	    fprintf(stream, "%03i", smurftext[cyc][cyc2]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfweap[cyc][cyc2]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfarmr[cyc][cyc2]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfettename[cyc][cyc2]);
	fprintf(stream, "%010i", smurfettelevel[cyc]);
	fprintf(stream, "%010i", smurfweapno[cyc]);
	fprintf(stream, "%010i", smurfarmrno[cyc]);
	for (cyc2 = 0; cyc2 < 40; cyc2++)
	    fprintf(stream, "%03i", smurfconf[cyc][cyc2]);
	fprintf(stream, "%010i", smurfconfno[cyc]);
	for (cyc3 = 0; cyc3 < 10; cyc3++) {
	     /* 69d */ fprintf(stream, "%010i", smurfhost[cyc][cyc3]);
	}
	/* 69a     fprintf(stream, "%010i",smurfhost[cyc]);fprintf(stream,
	 * "%010i",smurfhost1[cyc]);fprintf(stream, "%010i",smurfhost2[cyc]); */
	fprintf(stream, "%010i", smurflevel[cyc]);
	fprintf(stream, "%020.0f", smurfexper[cyc]);
	fprintf(stream, "%020.0f", smurfmoney[cyc]);
	fprintf(stream, "%020.0f", smurfbankd[cyc]);
	fprintf(stream, "%010i", smurffights[cyc]);
	fprintf(stream, "%010i", smurfwin[cyc]);
	fprintf(stream, "%010i", smurflose[cyc]);
	fprintf(stream, "%010i", smurfhp[cyc]);
	fprintf(stream, "%010i", smurfhpm[cyc]);
	fprintf(stream, "%010i", smurfstr[cyc]);
	fprintf(stream, "%010i", smurfspd[cyc]);
	fprintf(stream, "%010i", smurfint[cyc]);
	fprintf(stream, "%010i", smurfcon[cyc]);
	fprintf(stream, "%010i", smurfbra[cyc]);
	fprintf(stream, "%010i", smurfchr[cyc]);
	fprintf(stream, "%010i", smurfturns[cyc]);

	for (cyc3 = 0; cyc3 < 6; cyc3++) {
	     /* 69b */ fprintf(stream, "%03i%03i", smurfspcweapon[cyc][cyc3], smurfqtyweapon[cyc][cyc3]);	/* 111222 - Special Weap
														 * Number / Qty */
	}

    } fclose(stream);
    stream = fopen("smurf.log", "a+");
    fprintf(stream, "\n\rSPAWN:) System Backup\n\r");
    fprintf(stream, "SPAWN:) System Save Game\n\r");
    fclose(stream);
}
