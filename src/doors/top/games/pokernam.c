#include "top.h"

char XFAR *poker_gethandname(poker_game_typ *hgame, char *handdat)
{
char XFAR *handnam;
unsigned char val[2], suit;
unsigned XINT handval;
unsigned XINT hscore;
unsigned char vallng1[31], vallng2[31], suitlng[31], vplu[3] = "Plu";

handnam = wordret;

hscore = poker_scorehand(hgame, handdat);
val[0] = hscore & 0x000F; /* First Hand Value Parameter */
val[1] = (hscore & 0x00F0) >> 4; /* Second Hand Value Parameter */
suit = hgame->cardsuits[handdat[4]]; /* Suit Value */
handval = hscore & 0xF000; /* Hand Type */

itoa(val[0], outnum[0], 10);
sprintf(vallng1, "CardLongSngValue%s", outnum[0]);
itoa(val[1], outnum[0], 10);
sprintf(vallng2, "CardLongSngValue%s", outnum[0]);
itoa(suit + 1, outnum[0], 10);
sprintf(suitlng, "CardLongPluSuit%s", outnum[0]);

if (handval == POKHAND_HIGHCARD)
    {
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameLHighCard"),
                         getlang(vallng1));
    }
if (handval == POKHAND_ONEPAIR)
    {
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameLOnePair"),
                         getlang(vallng1));
    }
if (handval == POKHAND_TWOPAIR)
    {
    memcpy(&vallng2[8], vplu, 3);
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameLTwoPair"),
                         getlang(vallng2), getlang(vallng1));
    }
if (handval == POKHAND_THREEKIND)
    {
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameL3Kind"),
                         getlang(vallng1));
    }
if (handval == POKHAND_STRAIGHT)
    {
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameLStraight"),
                         getlang(vallng1));
    }
if (handval == POKHAND_FLUSH)
    {
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameLFlush"),
                         getlang(suitlng), getlang(vallng1));
    }
if (handval == POKHAND_FULLHOUSE)
    {
    memcpy(&vallng2[8], vplu, 3);
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameLFHouse"),
                         getlang(vallng2), getlang(vallng1));
    }
if (handval == POKHAND_FOURKIND)
    {
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameL4Kind"),
                         getlang(vallng1));
    }
if (handval == POKHAND_STRAIGHTFLUSH)
    {
    memcpy(&vallng1[8], vplu, 3);
    if (hscore != POKHAND_ROYALFLUSH)
        {
        handnam = top_output(OUT_STRINGNF, getlang("PokerNameLStrFlush"),
                             getlang(suitlng), getlang(vallng1));
        }
    else
    	{
        handnam = top_output(OUT_STRINGNF, getlang("PokerNameLRoyFlush"),
                             getlang(suitlng));
        }
    }

return handnam;
}

char XFAR *poker_getshortname(poker_game_typ *hgame, char *handdat)
{
char XFAR *handnam;
unsigned char val[2], suit;
unsigned XINT handval;
unsigned XINT hscore;
unsigned char vallng1[31], vallng2[31], suitlng[31], vplu[3] = "Plu";

handnam = wordret;

hscore = poker_scorehand(hgame, handdat);
val[0] = hscore & 0x000F; /* First Hand Value Parameter */
val[1] = (hscore & 0x00F0) >> 4; /* Second Hand Value Parameter */
suit = hgame->cardsuits[handdat[4]]; /* Suit Value */
handval = hscore & 0xF000; /* Hand Type */

itoa(val[0], outnum[0], 10);
sprintf(vallng1, "CardShrtSngValue%s", outnum[0]);
itoa(val[1], outnum[0], 10);
sprintf(vallng2, "CardShrtSngValue%s", outnum[0]);
itoa(suit, outnum[0], 10);
sprintf(suitlng, "CardShrtPluSuit%s", outnum[0]);

if (handval == POKHAND_HIGHCARD)
    {
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameSHighCard"),
                         getlang(vallng1));
    }
if (handval == POKHAND_ONEPAIR)
    {
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameSOnePair"),
                         getlang(vallng1));
    }
if (handval == POKHAND_TWOPAIR)
    {
    memcpy(&vallng2[8], vplu, 3);
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameSTwoPair"),
                         getlang(vallng2), getlang(vallng1));
    }
if (handval == POKHAND_THREEKIND)
    {
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameS3Kind"),
                         getlang(vallng1));
    }
if (handval == POKHAND_STRAIGHT)
    {
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameSStraight"),
                         getlang(vallng1));
    }
if (handval == POKHAND_FLUSH)
    {
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameSFlush"),
                         getlang(suitlng), getlang(vallng1));
    }
if (handval == POKHAND_FULLHOUSE)
    {
    memcpy(&vallng2[8], vplu, 3);
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameSFHouse"),
                         getlang(vallng2), getlang(vallng1));
    }
if (handval == POKHAND_FOURKIND)
    {
    memcpy(&vallng1[8], vplu, 3);
    handnam = top_output(OUT_STRINGNF, getlang("PokerNameS4Kind"),
                         getlang(vallng1));
    }
if (handval == POKHAND_STRAIGHTFLUSH)
    {
    memcpy(&vallng1[8], vplu, 3);
    if (hscore != POKHAND_ROYALFLUSH)
        {
        handnam = top_output(OUT_STRINGNF, getlang("PokerNameSStrFlush"),
                             getlang(suitlng), getlang(vallng1));
        }
    else
    	{
        handnam = top_output(OUT_STRINGNF, getlang("PokerNameSRoyFlush"),
                             getlang(suitlng));
        }
    }

return handnam;
}
