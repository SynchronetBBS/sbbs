#include "top.h"

XINT poker_getplayernum(char gpntype, XINT gpnnode, poker_game_typ *gpngame)
    {
    XINT gpnd;

    for (gpnd = 0; gpnd < gpngame->maxpl; gpnd++)
        {
        if ((gpntype == POKPL_WAITING || gpntype == POKPL_JOINED) &&
            gpngame->wanttoplay[gpnd] == gpnnode)
            {
            return gpnd;
            }
        if ((gpntype == POKPL_PLAYING || gpntype == POKPL_JOINED) &&
            gpngame->player[gpnd].node == gpnnode)
            {
            return gpnd;
            }
        }

    return -1;
    }

XINT poker_gethighplayer(char ghptype, poker_game_typ *ghpgame)
    {
    XINT ghpd;

    for (ghpd = ghpgame->maxpl - 1; ghpd >= 0; ghpd--)
        {
        if ((ghptype == POKPL_WAITING || ghptype == POKPL_JOINED) &&
            ghpgame->wanttoplay[ghpd] >= 0)
            {
            return ghpd;
            }
        if ((ghptype == POKPL_PLAYING || ghptype == POKPL_JOINED) &&
            ghpgame->player[ghpd].node >= 0)
            {
            return ghpd;
            }
        }

    return -1;
    }

void poker_displayhand(poker_game_typ *dhgame, char *dhhand)
    {
    XINT dhd;
    unsigned char ltname[41];

    top_output(OUT_SCREEN, getlang("PokerDispHandPref"));

    for (dhd = 0; dhd < dhgame->numcards; dhd++)
        {
        top_output(OUT_SCREEN, getlang("PokerDispCardPref"));

        itoa(pokerdefcardsuits[dhhand[dhd]] + 1, outnum[0], 10);
        sprintf(ltname, "CardShrtColour%s", outnum[0]);
        top_output(OUT_SCREEN, getlang(ltname));

        itoa(pokerdefcardvals[dhhand[dhd]], outnum[0], 10);
        sprintf(ltname, "CardShrtSngValue%s", outnum[0]);
        top_output(OUT_SCREEN, getlang(ltname));

        itoa(pokerdefcardsuits[dhhand[dhd]] + 1, outnum[0], 10);
        // Change A below to check for IBM ASCII pref...
        sprintf(ltname, "CardShrtSngSuit%c%s", 'A', outnum[0]);
        top_output(OUT_SCREEN, getlang(ltname));

        top_output(OUT_SCREEN, getlang("PokerDispCardSuf"));
        }

    top_output(OUT_SCREEN, getlang("PokerDispHandSuf"));

    }

void poker_endgamedisplay(poker_game_typ *egame)
    {
    XINT egd, xxq, maxnamlen; ///!!! this func!

    for (egd = 0, maxnamlen = 0; egd < MAXNODES; egd++)
        {
        if (egame->player[egd].node > -1)
            {
            if (strlen(handles[egd].string) > maxnamlen)
                {
                maxnamlen = strlen(handles[egd].string);
                }
            }
        }
    top_output(OUT_SCREEN, getlang("PokerHandsWere"));
    for (egd = 0; egd < egame->maxpl; egd++)
        {
        if (egame->player[egd].node > -1)
            {
            sprintf(outbuf, "^l%*s  ", maxnamlen,
                    handles[egame->player[egd].node].string);
            top_output(OUT_SCREEN, outbuf);
            poker_displayhand(egame, egame->player[egd].cards);
            top_output(OUT_SCREEN, " ");
            xxq = wherex();
            top_output(OUT_SCREEN,
                       poker_getshortname(egame, egame->player[egd].cards));
            od_repeat(' ', 20 - (wherex() - xxq));
            if (egame->winners[egd])
                {
                top_output(OUT_SCREEN, "  ^I^k<-- Winner!^A^h");
                }
            top_output(OUT_SCREEN, "\r\n");
            }
        }

    }
