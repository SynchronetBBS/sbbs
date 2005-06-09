#include "top.h"

XINT poker_startnewgame(poker_game_typ *sgame)
    {
    XINT std, sgres;
    XINT newgamenum = -1;

    for (std = 0; std < cfg.pokermaxgames; std++)
        {
        poker_lockgame(std);
        sgame->inuse = 0;
        sgres = poker_loadgame(std, sgame);

        if (!sgame->inuse || !sgres)
            {
            pokermaingame = newgamenum = std;
            poker_initnew(sgame);
            poker_savegame(std, sgame);
            poker_unlockgame(std);
            break;
            }
        poker_unlockgame(std);
        }

    return newgamenum;
    }

void poker_initnew(poker_game_typ *newgame)
    {
    XINT ind, ine;

    memset(newgame, 0, sizeof(poker_game_typ));

    for (ind = 0; ind < POKERMAXPLAYERS; ind++)
        {
		newgame->wanttoplay[ind] = -1;
		newgame->started[ind] = 0;
        newgame->finished[ind] = 0;
        newgame->winners[ind] = 0;
        newgame->player[ind].node = -1;
        for (ine = 0; ine < POKERMAXCARDS; ine++)
            {
            newgame->player[ind].cards[ine] = -1;
            }
        }

    newgame->inuse = 1;

    newgame->dealer = od_control.od_node;

    newgame->rules = POKRULE_ACEHIGH;

    newgame->minbet = cfg.pokerminbet; // Cfg! (def)
    newgame->maxbet = cfg.pokermaxbet; // Cfg! (def)
    newgame->ante = cfg.pokerante; // Cfg! (def)

    newgame->maxrounds = -1; // Cfg!
    newgame->maxcycles = 1; // Cfg! (def)

    newgame->minpl = 2; // Cfg!
    newgame->maxpl = 8; // Cfg!
    newgame->minai = 0; // Cfg!
    newgame->maxai = 0; // Cfg!

    newgame->wildcards[13] = 1; // Cfg!
    newgame->decksize = 52;
    if (newgame->rules & POKRULE_USEJOKERS)
        {
        newgame->decksize += 2;
        }
    newgame->numcards = 5; // Cfg! (via game type)

    newgame->delaytime = 20; // Cfg!

    newgame->wanttoplay[0] = od_control.od_node;
    newgame->wtpcount = 1;

    newgame->eventtime = time(NULL);

    poker_resetgame(newgame);

    }

void poker_resetgame(poker_game_typ *rgame)
    {
    XINT rgd;

	for (rgd = 0; rgd < POKERMAXPLAYERS; rgd++)
    	{
        rgame->player[rgd].totalbet = 0;
        }

    rgame->highbet = 0;
    rgame->pot = 0;

    rgame->cycle = -1;
    rgame->stage = -1;
    rgame->round = -1;
    rgame->turn = -1;

    rgame->numcalls = 0;

	memset(rgame->started, 0, POKERMAXPLAYERS);
    memset(rgame->finished, 0, POKERMAXPLAYERS);

    }

void poker_dealhand(poker_game_typ *dhgame, char *cardbuf)
    {
    XINT deald;

    for (deald = 0; deald < dhgame->numcards; deald++)
        {
        poker_dealcard(dhgame, &cardbuf[deald]);
        }

    poker_sorthand(dhgame, cardbuf);

    }

void poker_dealcard(poker_game_typ *dgame, char *dcard)
    {
    char dct, dcd;

    for (dcd = 0, dct = 0; dcd < dgame->decksize; dcd++)
        {
        if (!dgame->deck[dcd])
            {
            dct++;
            }
        }

    if (dct == dgame->decksize)
        {
        poker_shuffle(dgame);
        }

    do
        {
        dct = random(dgame->decksize);
        }
    while (dgame->deck[dct]);

    *dcard = dct;
    dgame->deck[dct] = 1;

    }

void poker_shuffle(poker_game_typ *shufgame)
    {
    XINT shd;

    for (shd = 0; shd < shufgame->decksize; shd++)
        {
        if (shufgame->deck[shd] == 2)
            {
            shufgame->deck[shd] = 0;
            }
        }

    }

char poker_joingame(XINT gamenum, poker_game_typ *jgame)
    {
    XINT jgd, newplnum = -1;

    poker_lockgame(gamenum);
    poker_loadgame(gamenum, jgame);

    for (jgd = 0; jgd < jgame->maxpl; jgd++)
        {
        if (jgame->wanttoplay[jgd] == -1)
            {
            newplnum = jgd;
            break;
            }
        }

    if (newplnum == -1)
        {
        poker_unlockgame(gamenum);
        itoa(gamenum, outnum[0], 10);
        top_output(OUT_SCREEN, getlang("PokerTooManyPeople"), outnum[0]);
        return 0;
        }

    jgame->wanttoplay[jgd] = od_control.od_node;
    jgame->wtpcount++;

    poker_savegame(gamenum, jgame);
    poker_unlockgame(gamenum);

    return 1;
    }

void poker_foldplayer(XINT fpnode, XINT fpnum, poker_game_typ *fpgame)
    {
    XINT foldd, fppl = -1;

    fppl = poker_getplayernum(POKPL_PLAYING, fpnode, fpgame);
    if (fppl == -1)
        {
        return;
        }

    for (foldd = 0; foldd < fpgame->numcards; foldd++)
        {
        if (fpgame->player[fppl].cards[foldd] > -1)
            {
            fpgame->deck[fpgame->player[fppl].cards[foldd]] = 2;
            fpgame->player[fppl].cards[foldd] = -1;
            }
        }
    fpgame->player[fppl].node = -1;

    (fpgame->plcount)--;

    if (fpgame->turn == fppl)
        {
        poker_advanceturn(fpnum, fpgame);
        }

    }

