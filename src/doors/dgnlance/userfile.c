#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "fileutils.h"
#include "xpdev.h"
#include "dgnlnce.h"
#include "utils.h"

BOOL
loadnextuser(struct playertype * plr, FILE * infile)
{
    char            temp[81];
    if (feof(infile))
	return (FALSE);
    readline(plr->name, sizeof(plr->name), infile);
    if (feof(infile))
	return (FALSE);
    readline(plr->pseudo, sizeof(plr->pseudo), infile);
    if (feof(infile))
	return (FALSE);
    fread(&plr->sex, 1, 1, infile);
    endofline(infile);
    if (feof(infile))
	return (FALSE);
    readline(plr->gaspd, sizeof(plr->gaspd), infile);
    if (feof(infile))
	return (FALSE);
    readline(plr->laston, sizeof(plr->laston), infile);
    if (feof(infile))
	return (FALSE);
    fgets(temp, sizeof(temp), infile);
    plr->status = strtoul(temp, NULL, 10);
    if (feof(infile))
	return (FALSE);
    if (plr->status == DEAD) {
	readline(plr->killer, sizeof(plr->killer), infile);
	if (feof(infile))
	    return (FALSE);
    }
    plr->strength = readnumb(6, infile);
    plr->intelligence = readnumb(6, infile);
    plr->luck = readnumb(6, infile);
    plr->dexterity = readnumb(6, infile);
    plr->constitution = readnumb(6, infile);
    plr->charisma = readnumb(6, infile);
    plr->experience = readnumb(0, infile);
    plr->r = readnumb(1, infile);
    plr->hps = readnumb(10, infile);
    plr->weapon = readnumb(1, infile);
    plr->armour = readnumb(1, infile);
    plr->gold = readnumb(0, infile);
    plr->flights = readnumb(3, infile);
    plr->bank = readnumb(0, infile);
    plr->wins = readnumb(0, infile);
    plr->loses = readnumb(0, infile);
    plr->plus = readnumb(0, infile);
    plr->fights = readnumb(FIGHTS_PER_DAY, infile);
    plr->battles = readnumb(BATTLES_PER_DAY, infile);
    plr->vary = readfloat(supplant(), infile);
    endofline(infile);
    return (TRUE);
}
BOOL
loaduser(char *name, BOOL bbsname, struct playertype * plr)
{
    char            temp[81];
    FILE           *infile;
    infile = fopen("data/characte.lan", "rb");
    fgets(temp, sizeof(temp), infile);
    while (loadnextuser(plr, infile)) {
	if (bbsname) {
	    if (!stricmp(plr->name, name)) {
		fclose(infile);
		return (TRUE);
	    }
	} else {
	    if (!stricmp(plr->pseudo, name)) {
		fclose(infile);
		return (TRUE);
	    }
	}
    }
    fclose(infile);
    return (FALSE);
}

void
writenextuser(struct playertype * plr, FILE * outfile)
{
    fprintf(outfile, "%s\n", plr->name);
    fprintf(outfile, "%s\n", plr->pseudo);
    fprintf(outfile, "%c\n", plr->sex);
    fprintf(outfile, "%s\n", plr->gaspd);
    fprintf(outfile, "%s\n", plr->laston);
    fprintf(outfile, "%lu\n", plr->status);
    if (plr->status == DEAD)
	fprintf(outfile, "%s\n", plr->killer);
    fprintf(outfile, "%u %u %u %u %u %u %" QWORDFORMAT " %u %u %u %u %" QWORDFORMAT " %u %" QWORDFORMAT " %" QWORDFORMAT " %" QWORDFORMAT " %u %u %u %1.4f\n",
	    plr->strength, plr->intelligence,
	    plr->luck, plr->dexterity, plr->constitution,
	    plr->charisma, plr->experience,
	    plr->r, plr->hps, plr->weapon,
	    plr->armour, plr->gold,
	    plr->flights, plr->bank, plr->wins, plr->loses,
	    plr->plus, plr->fights, plr->battles, plr->vary);
}

void
saveuser(struct playertype * plr)
{
    DWORD           h, i, j;
    FILE           *outfile;
    FILE           *infile;
    char            temp[81];

    struct playertype **playerlist = NULL;
    struct playertype **ra;
    BOOL            saved = FALSE;
    infile = fopen("data/characte.lan", "rb");
    fgets(temp, sizeof(temp), infile);
    i = 0;
    while (loadnextuser(&tmpuser, infile)) {
	ra = (struct playertype **) realloc(playerlist, sizeof(plr) * (i + 1));
	if (ra == NULL)
	    exit(1);;
	playerlist = ra;
	playerlist[i] = (struct playertype *) malloc(sizeof(struct playertype));
	if (playerlist[i] == NULL)
	    exit(1);;
	if (!stricmp(tmpuser.name, plr->name)) {
	    *playerlist[i] = *plr;
	    saved = TRUE;
	} else
	    *playerlist[i] = tmpuser;
	i++;
    }
    fclose(infile);

    if (!saved) {
	ra = (struct playertype **) realloc(playerlist, sizeof(plr) * (i + 1));
	if (ra == NULL)
	    exit(1);;
	playerlist = ra;
	playerlist[i] = (struct playertype *) malloc(sizeof(struct playertype));
	if (playerlist[i] == NULL)
	    exit(1);;
	*playerlist[i] = *plr;
	i++;
    }
    for (j = 0; j < (i - 1); j++) {
	for (h = j + 1; h < i; h++) {
	    if (playerlist[h]->experience > playerlist[j]->experience) {
		tmpuser = *playerlist[h];
		*playerlist[h] = *playerlist[j];
		*playerlist[j] = tmpuser;
	    }
	}
    }
    outfile = fopen("data/characte.lan", "wb");
    fprintf(outfile, "%lu\n", i);
    for (j = 0; j < i; j++) {
	writenextuser(playerlist[j], outfile);
	free(playerlist[j]);
    }
    free(playerlist);
    fclose(outfile);
}
