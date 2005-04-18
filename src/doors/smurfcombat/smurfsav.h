void 
savegame(void)
{
    int             thp;
    stream = fopen("smurf.sgm", "w+");
    fprintf(stream, "%03i", noplayers);
    fprintf(stream, "%05.5s", __saveversion);
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

    } fclose(stream);
}














void 
loadgame(void)
{
    char            intext[81], revtext[10], outtext[81] = "";
    int             inputint;
    stream = fopen("smurf.sgm", "r+");
    if(stream == NULL) {
	stream = fopen("smurf.sgm", "w+");
	fprintf(stream,"%03i%05.5s",0 , __saveversion);
	fseek(stream, 0, SEEK_SET);
    }
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
	smurfmoney[cyc] = atof(intext);/* if(smurfmoney[cyc]<0)smurfmoney[cyc]
				        * =0; */
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
















#ifndef DOSMODEONLY
void 
detectversion(void)
{
    char            intext[6];
    stream = fopen("smurf.sgm", "r+");
    if(stream == NULL) {
	stream = fopen("smurf.sgm", "w+");
	fprintf(stream,"%03i%05.5s",0 , __saveversion);
	fseek(stream, 0, SEEK_SET);
    }
    cyc = 0;
    fscanf(stream, "%3s", intext);     /* noplayers=atoi(intext); */
    fscanf(stream, "%5s", intext);     /* save version */
    fclose(stream);
    if (intext[0] == 'w')
	wongame();
    if (strcmp(intext, __saveversion) != 0)
	__mess(10);
}
#endif


void 
detectsave(void)
{
    if ((stream = fopen("smurf.sgm", "r+")) == NULL)
	__NEW__main();
    /* spawnl(P_WAIT,"SMURFnew",NULL); else fclose(stream); */
}

void 
detectwin(void)
{
    if ((stream = fopen("smurfdat.d0e", "r+")) != NULL) {
	fclose(stream);
	wongame();
    }
}
