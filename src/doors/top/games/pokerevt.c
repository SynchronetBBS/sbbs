#include "top.h"

void poker_eventkernel(XINT gamenum)
    {
    time_t timediff;
    poker_game_typ egame;
    XINT ekd, ekc;
    XINT plnum = -1;
    XINT highpl;

    timediff = difftime(time(NULL), pokeretimes[gamenum]);

    if (pokeringame[gamenum] &&
    	timediff >= (time_t) pokerdtimes[gamenum])
        { // This may be a little too disk intensive...
        poker_lockgame(gamenum);
        poker_loadgame(gamenum, &egame);
        if (egame.cycle == -1)
	        {
			assert(egame.inuse); // DANGEROUS - no unlock!
            for (ekd = 0; ekd < egame.maxpl; ekd++)
                {
                if (egame.wanttoplay[ekd] == od_control.od_node)
                    {
                    plnum = ekd;
                    break;
                    }
                }
            assert(plnum > -1); // DANGEROUS - no unlock!
            if (!egame.started[plnum])
                {
                memset(&egame.player[plnum], 0, sizeof(poker_plyr_typ));
                egame.player[plnum].node = od_control.od_node;
                if (user.cybercash >= egame.ante)
                    {
                    poker_dealhand(&egame, egame.player[plnum].cards);
                    egame.player[plnum].origcash = user.cybercash;
                    }
                highpl = poker_gethighplayer(POKPL_WAITING, &egame);
                egame.started[plnum] = 1;
                for (ekd = 0, ekc = 0; ekd <= highpl; ekd++)
                    {
                    if (egame.started[ekd])
                        {
                        ekc++;
                        }
                    }
                if (ekc == egame.wtpcount)
                    {
                    egame.plcount = egame.wtpcount;
                    egame.eventtime = time(NULL);
                    memcpy(egame.cardvals, pokerdefcardvals, POKERMAXDECK);
                    memcpy(egame.cardsuits, pokerdefcardsuits, POKERMAXDECK);
                    if (egame.rules & POKRULE_ACELOW)
                        {
                        egame.cardvals[0]  = 1;
                        egame.cardvals[13] = 1;
                        egame.cardvals[26] = 1;
                        egame.cardvals[39] = 1;
                        }
                    }
                if (user.cybercash < egame.ante)
                    {
                    egame.plcount--;
                    egame.player[plnum].node = -1;
                    }
                else
                    {
                    egame.pot += egame.ante;
                    }

                if (ekc == egame.wtpcount)
                    {
                    poker_advanceturn(gamenum, &egame);
                    }

                poker_savegame(gamenum, &egame);
                poker_unlockgame(gamenum);
                itoa(gamenum, outnum[0], 10);
                if (user.cybercash >= egame.ante)
                    {
                    user.cybercash -= egame.ante;
                    save_user_data(user_rec_num, &user);
                    }
                else
                    {
                    top_output(OUT_SCREEN, getlang("PokerTooBroke"),
                               outnum[0]);
                    }

                dispatch_message(MSG_GENERIC,
                                 top_output(OUT_STRINGNF,
                                            getlang("PokerGameStarted"),
                                                    outnum[0]),
                                 od_control.od_node, 1, 0);
                return;
                }
            }
        poker_unlockgame(gamenum);
        }

    // Wait & Toss checks

    }

