#include "top.h"

char poker_checkturn(char ctstage, XINT ctnode, poker_game_typ *ctgame)
    {
    XINT ctpl = -1;

    ctpl = poker_getplayernum(POKPL_PLAYING, ctnode, ctgame);
    if (ctpl == -1)
        {
        return 0;
        }

    if (ctgame->cycle >= 0 && ctgame->turn == ctpl)
        {
        if (ctstage == POKSTG_ANYTURN)
            {
            return 1;
            }
        if (ctstage == POKSTG_ANYBET)
            {
            if (ctgame->stage == POKSTG_BET ||
                ctgame->stage == POKSTG_FINALBET)
                {
                return 1;
                }
            }
        else
            {
            if (ctgame->stage == ctstage)
                {
                return 1;
                }
            }
        }

    return 0;
    }

void poker_advanceturn(XINT atnum, poker_game_typ *atgame)
    {
    XINT atpl;
    unsigned long dchk;

    atpl = poker_gethighplayer(POKPL_PLAYING, atgame);

    if (atgame->cycle == -1)
        {
        atgame->numcalls = 0;
        atgame->cycle = 0;
        atgame->stage = 0;
        atgame->round = 0;
        atgame->turn = -1;
        }

    dchk = 0;

    TurnAdvance:

	dchk++;
    while(atgame->player[++atgame->turn].node == -1 && atgame->turn <= atpl)
        {
        atgame->turn++;
        dchk++;
        }

    if (dchk > cfg.maxnodes)
        {
        /* While this should never happen, it's here for safety. */
        // Bail out some how.  Easiest way is probably to totally deinitialize
        // the game (inuse=0) and get this player out of it (globals=0).
        return;
        }

    if (atgame->turn > atpl)
        {
        atgame->turn = -1;
        atgame->round++;
        goto TurnAdvance;
        }
    if (atgame->numcalls >= atgame->plcount ||
        atgame->cycle >= atgame->maxcycles ||
        atgame->plcount < atgame->minpl)
        {
        if (++atgame->stage == POKSTG_ENDCYCLE)
            {
            atgame->cycle++;
            atgame->stage = 0;
            }
        atgame->round = 0;
        atgame->turn = 0;
        atgame->numcalls = 0;
        if (atgame->cycle >= atgame->maxcycles ||
            atgame->plcount < atgame->minpl)
            {
            poker_sendmessage(MSG_POKER, 1, atnum, 0, "GameOver", atgame);
            return;
            }
        }
    itoa(atnum, outnum[0], 10);
    dispatch_message(MSG_GENERIC,
                     top_output(OUT_STRINGNF,
                                getlang("PokerMsgYourTurn"),
                                getlang(atgame->stage == 1 ?
                                        "Discard" : "Bet"),
                                outnum[0], atgame),
                     atgame->player[atgame->turn].node, 1, 0);

    }
