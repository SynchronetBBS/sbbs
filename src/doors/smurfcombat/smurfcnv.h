/* /* 69a -- 69a -- 69a */

void 
loadgame69a(void)
{
    char            intext[81], revtext[10], outtext[81] = "";
    int             inputint;
    stream = fopen("smurf.sgm", "r+");
    cyc = 0;
    fscanf(stream, "%3s", intext);
    noplayers = atoi(intext);

    for (cyc = 0; cyc < noplayers; cyc++) {
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(realname[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	realnumb[cyc] = atoi(intext);
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfname[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 80; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurftext[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfweap[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfarmr[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfettename[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	smurfettelevel[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfweapno[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfarmrno[cyc] = atoi(intext);
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfconf[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	smurfconfno[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhost[cyc][0] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhost[cyc][0] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhost[cyc][0] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurflevel[cyc] = atoi(intext);
	fscanf(stream, "%20s", intext);
	smurfexper[cyc] = atof(intext);
	fscanf(stream, "%20s", intext);
	smurfmoney[cyc] = atof(intext);
	fscanf(stream, "%20s", intext);
	smurfbankd[cyc] = atof(intext);
	fscanf(stream, "%10s", intext);
	smurffights[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfwin[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurflose[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhp[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhpm[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfstr[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfspd[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfint[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfcon[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfbra[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfchr[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfturns[cyc] = atoi(intext);
    } fclose(stream);
}













/* 69b -- 69b -- 69b */

void 
loadgame69b(void)
{
    char            intext[81], revtext[10], outtext[81] = "";
    int             inputint;
    stream = fopen("smurf.sgm", "r+");
    cyc = 0;
    fscanf(stream, "%3s", intext);
    noplayers = atoi(intext);
    for (cyc = 0; cyc < noplayers; cyc++) {
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(realname[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	realnumb[cyc] = atoi(intext);
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfname[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 80; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurftext[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfweap[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfarmr[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfettename[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	smurfettelevel[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfweapno[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfarmrno[cyc] = atoi(intext);
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfconf[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	smurfconfno[cyc] = atoi(intext);
	 /* 69b */ for (cyc3 = 0; cyc3 < 10; cyc3++) {
	    fscanf(stream, "%10s", intext);
	    smurfhost[cyc][cyc3] = atoi(intext);
	}
	/* fscanf(stream,"%10s",intext);smurfhost1[cyc]=atoi(intext); //69b */
	/* fscanf(stream,"%10s",intext);smurfhost2[cyc]=atoi(intext); //69b */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	fscanf(stream, "%10s", intext);
	smurflevel[cyc] = atoi(intext);
	fscanf(stream, "%20s", intext);
	smurfexper[cyc] = atof(intext);
	fscanf(stream, "%20s", intext);
	smurfmoney[cyc] = atof(intext);
	fscanf(stream, "%20s", intext);
	smurfbankd[cyc] = atof(intext);
	fscanf(stream, "%10s", intext);
	smurffights[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfwin[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurflose[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhp[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhpm[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfstr[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfspd[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfint[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfcon[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfbra[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfchr[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfturns[cyc] = atoi(intext);

	for (cyc3 = 0; cyc3 < 6; cyc3++) {
	     /* 69b */ fscanf(stream, "%3s", intext);
	    smurfspcweapon[cyc][cyc3] = atoi(intext);
	     /* 69b */ fscanf(stream, "%3s", intext);
	    smurfqtyweapon[cyc][cyc3] = atoi(intext);
	}

    } fclose(stream);
}





/* 69c -- 69c -- 69c */

void 
loadgame69c(void)
{
    char            intext[81], revtext[10], outtext[81] = "";
    int             inputint;
    stream = fopen("smurf.sgm", "r+");
    cyc = 0;
    fscanf(stream, "%3s", intext);
    noplayers = atoi(intext);
    for (cyc = 0; cyc < noplayers; cyc++) {
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(realname[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	realnumb[cyc] = atoi(intext);
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfname[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 80; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurftext[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfweap[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfarmr[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfettename[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	smurfettelevel[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfweapno[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfarmrno[cyc] = atoi(intext);
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfconf[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	smurfconfno[cyc] = atoi(intext);
	 /* 69b */ for (cyc3 = 0; cyc3 < 10; cyc3++) {
	    fscanf(stream, "%10s", intext);
	    smurfhost[cyc][cyc3] = atoi(intext);
	}
	/* fscanf(stream,"%10s",intext);smurfhost1[cyc]=atoi(intext); //69b */
	/* fscanf(stream,"%10s",intext);smurfhost2[cyc]=atoi(intext); //69b */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	fscanf(stream, "%10s", intext);
	smurflevel[cyc] = atoi(intext);
	fscanf(stream, "%20s", intext);
	smurfexper[cyc] = atof(intext);
	fscanf(stream, "%20s", intext);
	smurfmoney[cyc] = atof(intext);
	fscanf(stream, "%20s", intext);
	smurfbankd[cyc] = atof(intext);
	fscanf(stream, "%10s", intext);
	smurffights[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfwin[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurflose[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhp[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhpm[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfstr[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfspd[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfint[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfcon[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfbra[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfchr[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfturns[cyc] = atoi(intext);

	for (cyc3 = 0; cyc3 < 6; cyc3++) {
	     /* 69b */ fscanf(stream, "%3s", intext);
	    smurfspcweapon[cyc][cyc3] = atoi(intext);
	     /* 69b */ fscanf(stream, "%3s", intext);
	    smurfqtyweapon[cyc][cyc3] = atoi(intext);
	}
	 /* 69c */ fscanf(stream, "%3s", intext);
	smurfsex[cyc] = atoi(intext);
    } fclose(stream);
}





/* 91 -- 91 -- 91 */

void 
loadgame91(void)
{
    char            intext[81], revtext[10], outtext[81] = "";
    int             inputint;
    stream = fopen("smurf.sgm", "r+");
    cyc = 0;
    fscanf(stream, "%3s", intext);
    noplayers = atoi(intext);
    fscanf(stream, "%5s", intext);     /* save version */

    for (cyc = 0; cyc < noplayers; cyc++) {

	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(realname[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	realnumb[cyc] = atoi(intext);
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfname[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 80; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurftext[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfweap[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfarmr[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfettename[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	smurfettelevel[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfweapno[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfarmrno[cyc] = atoi(intext);
	for (cyc2 = 0; cyc2 < 40; cyc2++) {
	    fscanf(stream, "%3s", intext);
	    inputint = atoi(intext);
	    sprintf(revtext, "%c", inputint);
	    strcat(outtext, revtext);
	}
	sprintf(smurfconf[cyc], "%s", outtext);
	sprintf(outtext, "\0");
	fscanf(stream, "%10s", intext);
	smurfconfno[cyc] = atoi(intext);
	 /* 69b */ for (cyc3 = 0; cyc3 < 10; cyc3++) {
	    fscanf(stream, "%10s", intext);
	    smurfhost[cyc][cyc3] = atoi(intext);
	}
	/* fscanf(stream,"%10s",intext);smurfhost1[cyc]=atoi(intext); //69b */
	/* fscanf(stream,"%10s",intext);smurfhost2[cyc]=atoi(intext); //69b */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	/* 69a fscanf(stream,"%10s",intext);smurfhost[cyc][0]=atoi(intext); */
	fscanf(stream, "%10s", intext);
	smurflevel[cyc] = atoi(intext);
	fscanf(stream, "%20s", intext);
	smurfexper[cyc] = atof(intext);
	fscanf(stream, "%20s", intext);
	smurfmoney[cyc] = atof(intext);
	fscanf(stream, "%20s", intext);
	smurfbankd[cyc] = atof(intext);
	fscanf(stream, "%10s", intext);
	smurffights[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfwin[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurflose[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhp[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfhpm[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfstr[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfspd[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfint[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfcon[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfbra[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfchr[cyc] = atoi(intext);
	fscanf(stream, "%10s", intext);
	smurfturns[cyc] = atoi(intext);

	 /* 91 */ fscanf(stream, "%3s", intext);
	__morale[cyc] = atoi(intext);
	 /* 91 */ fscanf(stream, "%3s", intext);
	__ettemorale[cyc] = atoi(intext);
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
	for (cyc3 = 0; cyc3 < 5; cyc3++) {
	     /* 69b */ fscanf(stream, "%3s", intext);
	    smurfspcweapon[cyc][cyc3] = atoi(intext);
	     /* 69b */ fscanf(stream, "%3s", intext);
	    smurfqtyweapon[cyc][cyc3] = atoi(intext);
	}
	 /* 69c */ fscanf(stream, "%3s", intext);
	smurfsex[cyc] = atoi(intext);

    } fclose(stream);
}
*/
