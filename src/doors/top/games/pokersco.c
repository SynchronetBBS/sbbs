#include "top.h"

XINT poker_findwinners(XINT fwnode, unsigned XINT *fwwin,
                       poker_game_typ *fwgame)
    {
    XINT fwd;
    unsigned long handscores[POKERMAXPLAYERS], maxscore = 0;
    unsigned long handrawscores[POKERMAXPLAYERS], maxrawscore = 0;
    unsigned long winchk = 0;
    XINT fwpl = -1;

    // Need checksum mods...

    fwpl = poker_getplayernum(POKPL_PLAYING, fwnode, fwgame);
    if (fwpl == -1)
        {
        top_output(OUT_SCREEN, getlang("PokerCantFindPlyr"));
        return 0;
        }

    memset(fwgame->winners, 0, fwgame->maxpl);
    *fwwin = 0;

    for (fwd = 0; fwd < fwgame->maxpl; fwd++)
        {
        handscores[fwd] = 0;
        handrawscores[fwd] = 0;
        if (fwgame->player[fwd].node > -1)
            {
            handscores[fwd] = poker_scorehand(fwgame,
                                  fwgame->player[fwd].cards);
            handrawscores[fwd] = poker_rawscorehand(fwgame,
                                     fwgame->player[fwd].cards);
            if (handscores[fwd] > maxscore)
                {
                maxscore = handscores[fwd];
                maxrawscore = handrawscores[fwd];
                }
            if (handscores[fwd] == maxscore &&
                handrawscores[fwd] > maxrawscore)
                {
                maxrawscore = handrawscores[fwd];
                }
            }
        }

    for (fwd = 0; fwd < fwgame->maxpl; fwd++)
        {
        if (fwgame->player[fwd].node > -1)
            {
            if (handscores[fwd] == maxscore &&
                handrawscores[fwd] == maxrawscore)
                {
                fwgame->winners[fwd] = 1;
                (*fwwin)++;
                }
            }
        }

    if (fwgame->winners[fwpl])
        {
        *fwwin += 0x0100;
        }

    return winchk;
    }

#define ssuit(xss)  pokerdefcardsuits[shand[xss]]
#define sval(xsv)   pokerdefcardvals[shand[xsv]]

// Delete this when 6-7 card mods are made.
#pragma argsused
unsigned XINT poker_scorehand(poker_game_typ *sgame, char *shand)
{
unsigned XINT scoreret, sd, se;
char basehand[3] = { 0,0,0 }, pairvals[10], pnum;

poker_sorthand(sgame, shand);

// Major mods for 7Stud, naturally.
// I'm leaving this hardcoded at 5 'til I get a plan for working with more.

// Mods for Jokers/wilds, though they will be easy.

for (sd = 0, pnum = 0; sd < 5; sd++)
	{
    for (se = sd + 1; se < 5; se++)
		{
		if (sval(sd) == sval(se))
			{
            basehand[0]++;
            pairvals[pnum++] = sval(se);
            }
        }
    }

scoreret = POKHAND_HIGHCARD + sval(4);
if (basehand[0] == 1)
	{
    scoreret = POKHAND_ONEPAIR + pairvals[0];
    }
if (basehand[0] == 2)
	{
    scoreret = POKHAND_TWOPAIR + (pairvals[1] * 0x10) + pairvals[0];
    }
if (basehand[0] == 3)
	{
    scoreret = POKHAND_THREEKIND + pairvals[2];
    }
if ((sval(1) == sval(0) + 1) && (sval(2) == sval(0) + 2) &&
	(sval(3) == sval(0) + 3) && (sval(4) == sval(0) + 4))
    {
    basehand[1]++;
    scoreret = POKHAND_STRAIGHT + sval(4);
    }
if ((ssuit(0) == ssuit(1)) && (ssuit(1) == ssuit(2)) &&
	(ssuit(2) == ssuit(3)) && (ssuit(3) == ssuit(4)))
	{
    basehand[2]++;
    scoreret = POKHAND_FLUSH + sval(4);
    }
if (basehand[0] == 4)
	{
    if (sval(1) == sval(2))
    	{
        scoreret = POKHAND_FULLHOUSE + (sval(2) * 0x10) + sval(3);
        }
    else
    	{
        scoreret = POKHAND_FULLHOUSE + (sval(2) * 0x10) + sval(1);
        }
    }
if (basehand[0] == 6)
	{
    scoreret = POKHAND_FOURKIND + pairvals[5];
    }
if (basehand[1] == 1 && basehand[2] == 1)
	{
    scoreret = POKHAND_STRAIGHTFLUSH + sval(4);
    }

return scoreret;
}

unsigned long poker_rawscorehand(poker_game_typ *sgame, char *shand)
{
XINT prshd;
unsigned long scoreret = 0;

poker_sorthand(sgame, shand);

for (prshd = 0; prshd < sgame->numcards; prshd++)
	{
    scoreret += ((unsigned long) sval(prshd) <<
                 ((unsigned long) prshd * 4L));
	}

return scoreret;
}

void poker_sorthand(poker_game_typ *sgame, char *shand)
{
char tmpsort[POKERMAXCARDS];
char sortused[POKERMAXCARDS];
unsigned XINT highcardval;
XINT sort1, sort2;
char highcardnum;

memset(tmpsort, -1, POKERMAXCARDS);
memset(sortused, 0, POKERMAXCARDS);

for (sort1 = sgame->numcards - 1; sort1 >= 0; sort1--)
	{
    highcardval = 0;
    highcardnum = -1;
    for (sort2 = 0; sort2 < sgame->numcards; sort2++)
    	{
        if (!sortused[sort2])
        	{
            unsigned XINT tmpval;

            tmpval = (sval(sort2) * 0x10) + ssuit(sort2);
            if (tmpval >= highcardval)
            	{
                highcardval = tmpval;
                highcardnum = sort2;
                }
            }
        }
    sortused[highcardnum] = 1;
    tmpsort[sort1] = shand[highcardnum];
    }

memcpy(shand, tmpsort, POKERMAXCARDS);

return;
}
