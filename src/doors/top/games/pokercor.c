#include "top.h"

void poker_core(XINT msggam, byte *msgstr)
    {
    poker_game_typ cgame;
    XINT cpl = -1;
    char pproc = 0;
    XINT cd, cfin = 0;
	unsigned XINT numwin;

    msgextradata = 0;

    if (!stricmp(msgstr, "GameOver"))
        {
        poker_lockgame(msggam);
        poker_loadgame(msggam, &cgame);

        cpl = poker_getplayernum(POKPL_JOINED, od_control.od_node, &cgame);
        if (cpl == -1)
            {
            poker_unlockgame(msggam);
            itoa(msggam, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("PokerBadGameOver"), outnum[0]);
            return;
            }

        cpl = poker_getplayernum(POKPL_PLAYING, od_control.od_node, &cgame);
        if (cpl >= 0)
        	{
	        cgame.player[cpl].winchksum =
    	        poker_findwinners(od_control.od_node, &numwin, &cgame);

        	cgame.finished[cpl] = 1;

	        for (cd = 0; cd < cgame.maxpl; cd++)
    	        {
        	    if (cgame.finished[cd])
            	    {
                	cfin++;
	                }
    	        }

	        if (cfin == cgame.plcount)
    	        {
        	    pproc = 1;
            	}

	        poker_savegame(msggam, &cgame);
        	}

        poker_unlockgame(msggam);

        if (pproc)
            {
            msgextradata = 1;
            }

        return;
        }
    if (!stricmp(msgstr, "Finished"))
        {
        unsigned long tpot = 0;

        poker_lockgame(msggam);
        poker_loadgame(msggam, &cgame);

        cpl = poker_getplayernum(POKPL_JOINED, od_control.od_node, &cgame);
        if (cpl < 0)
            { // Chg this section to let folded/broke plyrs watch
            poker_unlockgame(msggam);
            itoa(msggam, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("PokerBadFinish"), outnum[0]);
            return;
            }

        cpl = poker_getplayernum(POKPL_PLAYING, od_control.od_node, &cgame);
        if (cpl >= 0)
	        {
//            unsigned long xtrachk = 0; (chksum check for later)

            tpot = cgame.pot;
            if (cpl >= 0)
                {
                // Compare all checksums (func)
                // if problem, restore cash & wipe game.  Display error too.

                cgame.finished[cpl] = 2;

                for (cd = 0; cd < cgame.maxpl; cd++)
                    {
                    if (cgame.finished[cd] == 2)
                        {
                        cfin++;
                        }
                    }

                if (cfin == cgame.plcount)
                    {
                    cgame.eventtime = time(NULL);
                    poker_resetgame(&cgame);
                    poker_sendmessage(MSG_POKER, 0, msggam, 0, "DoRescan",
                                      &cgame);
                    }

                poker_savegame(msggam, &cgame);
                }
            }
        poker_unlockgame(msggam);

        // Match checksums with winner check below too if playing.

   		poker_findwinners(od_control.od_node, &numwin, &cgame);
        if (poker_getplayernum(POKPL_PLAYING, od_control.od_node,
                               &cgame) < 0)
            {
            numwin = 0;
            }

    	// Abort & wipe like above if chk from above doesn't match.

        itoa(msggam, outnum[0], 10);
        top_output(OUT_SCREEN, getlang("PokerGameEnded"), outnum[0]);
        poker_endgamedisplay(&cgame);
        ultoa(tpot, outnum[0], 10);
        top_output(OUT_SCREEN, getlang("PokerPotWas"), outnum[0],
                   tpot == 1 ?
                   getlang("CDSingular") : getlang("CDPlural"));
        if (pokeringame[msggam] &&
            poker_getplayernum(POKPL_PLAYING, od_control.od_node,
                               &cgame) >= 0)
            {
            if (numwin & 0x0100)
                {
                unsigned long wintmp;

                wintmp = (tpot / ((unsigned long) numwin - 0x0100UL));
                ultoa(wintmp, outnum[0], 10);
                top_output(OUT_SCREEN, getlang("PokerWinAmount"), outnum[0],
                           getlang(wintmp == 1 ? "CDSingular" : "CDPlural"));
                user.cybercash += wintmp;

                save_user_data(user_rec_num, &user);
                }
            else
                {
                top_output(OUT_SCREEN, getlang("PokerYouLose"));
                }
            }

        return;
        }
    if (!stricmp(msgstr, "DoRescan"))
        {
        poker_lockgame(msggam);
        poker_loadgame(msggam, &cgame);
        poker_unlockgame(msggam);
        }

    }
