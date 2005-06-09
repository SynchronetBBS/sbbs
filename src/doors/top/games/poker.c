#include "top.h"

void poker_game(XINT pgwords) //!! cmd words need lang
    { ////proc
    poker_game_typ tgame;
    XINT tgnum;
    char override = 0;
    XINT pgd;
    char bettype = 0;
    unsigned long tmpbet;
    XINT plnum;

    if (!stricmp(get_word(1), "START"))
        {
        tgnum = poker_startnewgame(&tgame);
        itoa(tgnum, outnum[0], 10);
        top_output(OUT_SCREEN, getlang("PokerYouStartGame"), outnum[0]);
        dispatch_message(MSG_GENERIC, top_output(OUT_STRINGNF,
                                                 getlang("PokerMsgStartGame"),
                                                 user.handle, outnum[0]),
                         -1, 0, 0);
        pokermaingame = tgnum;
        pokeringame[tgnum] = 1;
        return;
        }
    if (!stricmp(get_word(1), "JOIN"))
        {
        tgnum = atoi(get_word(2)); // Maybe take POKER x JOIN as well
        if (tgnum < 0 || !isdigit(get_word_char(2, 0)))
            {
            top_output(OUT_SCREEN, getlang("PokerBadGameNum"));
            return;
            }
        else
            {
            if (!poker_joingame(tgnum, &tgame))
                {
                return;
                }
            }

        itoa(tgnum, outnum[0], 10);
        top_output(OUT_SCREEN, getlang("PokerYouJoinGame"), outnum[0]);
        poker_sendmessage(MSG_GENERIC, 0, tgnum, 0,
                          top_output(OUT_STRINGNF,
                                     getlang("PokerMsgJoinGame"),
                                     user.handle, outnum[0]),
                          &tgame);
        pokermaingame = tgnum;
        pokeringame[tgnum] = 1;
        return;
        }
    if (!stricmp(get_word(1), "QUIT"))
        {
        tgnum = atoi(get_word(2)); // Maybe take POKER x QUIT as well
        if (tgnum < 0 || !isdigit(get_word_char(2, 0)))
            {
            top_output(OUT_SCREEN, getlang("PokerBadGameNum"));
            }
        else
            {
            poker_lockgame(tgnum);
            poker_loadgame(tgnum, &tgame);
            if (poker_checkturn(POKSTG_ANYTURN, od_control.od_node, &tgame))
                {
                poker_foldplayer(od_control.od_node, tgnum, &tgame);
                }

            plnum = poker_getplayernum(POKPL_WAITING, od_control.od_node,
                                       &tgame);
            tgame.wanttoplay[plnum] = -1;
            if (--tgame.wtpcount < 1)
                {
                tgame.inuse = 0;
                }
            poker_savegame(tgnum, &tgame);
            poker_unlockgame(tgnum);
            pokeringame[tgnum] = 0;

            itoa(tgnum, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("PokerYouQuitGame"), outnum[0]);
            poker_sendmessage(MSG_GENERIC, 1, tgnum, 0,
                              top_output(OUT_STRINGNF,
                                         getlang("PokerMsgQuitGame"),
                                         user.handle, outnum[0]),
                              &tgame);
            }
        return;
        }
    if (!stricmp(get_word(1), "MAIN"))
        {
        tgnum = atoi(get_word(2)); // Maybe take POKER x MAIN as well
        if (tgnum < 0 || !isdigit(get_word_char(2, 0)))
            {
            top_output(OUT_SCREEN, getlang("PokerBadGameNum"));
            }
        else
            {
            pokermaingame = tgnum;
            itoa(pokermaingame, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("PokerMainGameSet"), outnum[0]);
            }
        return;
        }

    if (!isdigit(get_word_char(1, 0)))
        {
        for (pgd = pgwords; pgd > 1; pgd--)
            {
            word_pos[pgd] = word_pos[pgd - 1];
            word_len[pgd] = word_len[pgd - 1];
            }
        override = 1;
        pgwords++;
        }

    if (override && pokermaingame < 0)
        {
        top_output(OUT_SCREEN, getlang("PokerNoMainGame"));
        return;
        }
    if (!override)
        {
        tgnum = atoi(get_word(1));
        }
    else
        {
        tgnum = pokermaingame;
        }

    if (!stricmp(get_word(2), "TURN"))
        {
        poker_lockgame(tgnum);
        poker_loadgame(tgnum, &tgame);
        poker_unlockgame(tgnum);

        // Add bet/discard show support!!
        if (tgame.turn < 0)
        	{
            itoa(tgnum, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("PokerGameNotOn"), outnum[0]);
            }
		else
        	{
	        top_output(OUT_SCREEN, getlang("PokerWhosTurn"),
    	               handles[tgame.player[tgame.turn].node].string,
                       getlang(tgame.stage == 1 ? "Discard" : "Bet"));
            }
        return;
        }
    if (!stricmp(get_word(2), "POT"))
        {
        poker_lockgame(tgnum);
        poker_loadgame(tgnum, &tgame);
        poker_unlockgame(tgnum);

        if (tgame.turn < 0)
        	{
            itoa(tgnum, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("PokerGameNotOn"), outnum[0]);
            }
		else
        	{
	        ultoa(tgame.pot, outnum[0], 10);
    	    top_output(OUT_SCREEN, getlang("PokerShowPot"), outnum[0],
        	           getlang(tgame.pot == 1 ? "CDSingular" : "CDPlural"));
            }
        return;
        }
    if (!stricmp(get_word(2), "HAND"))
        {
        poker_lockgame(tgnum);
        poker_loadgame(tgnum, &tgame);
        poker_unlockgame(tgnum);

        plnum = poker_getplayernum(POKPL_PLAYING, od_control.od_node,
                                   &tgame);
        if (tgame.turn < 0)
        	{
			itoa(tgnum, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("PokerGameNotOn"), outnum[0]);
            return;
            }
        if (plnum == -1)
            {
        	itoa(tgnum, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("PokerNotInGame"), outnum[0]);
            }
        else
            {
            top_output(OUT_SCREEN, getlang("PokerHandPrefix"));
            poker_displayhand(&tgame, tgame.player[plnum].cards);
            top_output(OUT_SCREEN, getlang("PokerHandSuffix"));
            top_output(OUT_SCREEN,
                       poker_gethandname(&tgame, tgame.player[plnum].cards));
            top_output(OUT_SCREEN, getlang("PokerHandNameSuf"));
            }

        return;
        }
    if (!stricmp(get_word(2), "SCAN"))
        {
        // Add scanning after tests.
        }

    if (!stricmp(get_word(2), "BET"))
        {
        bettype = 1;
        }
    if (!stricmp(get_word(2), "CALL"))
        {
        bettype = 2;
        }
    if (!stricmp(get_word(2), "RAISE"))
        {
        bettype = 3;
        }
    if (bettype > 0)
        {
        poker_lockgame(tgnum);
        poker_loadgame(tgnum, &tgame);

        if (!get_word_char(3, 0) && bettype != 2)
            {
            poker_unlockgame(tgnum);
            top_output(OUT_SCREEN, getlang("PokerNoBet"),
                       bettype == 1 ? "BET" : "RAISE");
            return;
            }

        if (!poker_checkturn(POKSTG_ANYBET, od_control.od_node, &tgame))
            {
            poker_unlockgame(tgnum);
            itoa(tgnum, outnum[0], 10);
            // Add bet/discard show support!!
            top_output(OUT_SCREEN, getlang("PokerNotYourTurn"),
                       getlang("Bet"), outnum[0]);

            return;
            }

        plnum = poker_getplayernum(POKPL_PLAYING, od_control.od_node,
                                   &tgame);

        tmpbet = 0;
        if (bettype == 2 || bettype == 3)
            {
            tmpbet = tgame.highbet - tgame.player[plnum].totalbet;
            }
        if (bettype == 1 || bettype == 3)
            {
            tmpbet += strtoul(get_word(3), NULL, 10);
            }

        if (tmpbet > user.cybercash)
            {
            poker_unlockgame(tgnum);
            ultoa(tmpbet, outnum[0], 10);
            ultoa(user.cybercash, outnum[1], 10);
            top_output(OUT_SCREEN, getlang("PokerNotEnoughCash"), outnum[0],
                       outnum[1]);
            return;
            }

        if (tgame.player[plnum].totalbet + tmpbet < tgame.highbet)
            {
            poker_unlockgame(tgnum);
            ultoa(tmpbet, outnum[0], 10);
            ultoa(tgame.highbet - tgame.player[plnum].totalbet,
                  outnum[1], 10);
            top_output(OUT_SCREEN, getlang("PokerBetTooLow"), outnum[0],
                       outnum[1]);
            return;
            }

        if ((((tgame.player[plnum].totalbet + tmpbet) - tgame.highbet <
            tgame.minbet) || ((tgame.player[plnum].totalbet + tmpbet) -
            tgame.highbet > tgame.maxbet)) &&
            ((tgame.player[plnum].totalbet + tmpbet) - tgame.highbet != 0))
            {
            poker_unlockgame(tgnum);
            ultoa(tmpbet, outnum[0], 10);
            ultoa(tgame.minbet, outnum[1], 10);
            ultoa(tgame.maxbet, outnum[2], 10);
            top_output(OUT_SCREEN, getlang("PokerInvalidBet"), outnum[0],
                       outnum[1], outnum[2]);
            return;
            }

        if (tgame.maxrounds > -1 &&
        	tmpbet > tgame.highbet - tgame.player[plnum].totalbet &&
            tgame.round >= tgame.maxrounds)
            {
            poker_unlockgame(tgnum);
            top_output(OUT_SCREEN, getlang("PokerNoMoreRaise"));
            return;
            }

        ultoa(tgame.highbet - tgame.player[plnum].totalbet, outnum[7], 10);
        ultoa(tmpbet - (tgame.highbet - tgame.player[plnum].totalbet),
                        outnum[8], 10);

        if (tmpbet > tgame.highbet - tgame.player[plnum].totalbet)
            {
            tgame.numcalls = 0;
            }

        tgame.player[plnum].totalbet += tmpbet;
        tgame.highbet = tgame.player[plnum].totalbet;
        tgame.pot += tmpbet;
        tgame.numcalls++;

        poker_advanceturn(tgnum, &tgame);

        poker_savegame(tgnum, &tgame);
        poker_unlockgame(tgnum);

        if (tmpbet == tgame.highbet - tgame.player[plnum].totalbet)
            {
            top_output(OUT_SCREEN, getlang("PokerYouCall"), outnum[7],
                       getlang(strtoul(outnum[7], NULL, 10) == 1 ?
                       "CDSingular" : "CDPlural"));
            poker_sendmessage(MSG_GENERIC, 0, tgnum, 0,
                              top_output(OUT_STRINGNF,
                                         getlang("PokerMsgCall"),
                                         outnum[0], user.handle, outnum[7],
                                         getlang(strtoul(outnum[7], NULL, 10) == 1 ?
                                         "CDSingular" : "CDPlural")),
                              &tgame);
            }
        else
            {
            top_output(OUT_SCREEN, getlang("PokerYouRaise"), outnum[7],
                       getlang(strtoul(outnum[7], NULL, 10) == 1 ?
                       "CDSingular" : "CDPlural"), outnum[8],
                       getlang(strtoul(outnum[8], NULL, 10) == 1 ?
                       "CDSingular" : "CDPlural"));
            poker_sendmessage(MSG_GENERIC, 0, tgnum, 0,
                              top_output(OUT_STRINGNF,
                                         getlang("PokerMsgRaise"),
                                         outnum[0], user.handle, outnum[7],
                                         getlang(strtoul(outnum[7], NULL, 10) == 1 ?
                                         "CDSingular" : "CDPlural"), outnum[8],
                                         getlang(strtoul(outnum[8], NULL, 10) == 1 ?
                                         "CDSingular" : "CDPlural")),
                              &tgame);
            }

        return;
        }

    if (!stricmp(get_word(2), "DISCARD"))
        {
        bettype = -1;
        }
    if (!stricmp(get_word(2), "STAND"))
        {
        bettype = -2;
        }
    if (bettype < 0)
        {
        unsigned char tlang1[31], tlang2[31];
        XINT dcount = 0;

        poker_lockgame(tgnum);
        poker_loadgame(tgnum, &tgame);

        if (!poker_checkturn(POKSTG_DISCARD, od_control.od_node, &tgame))
            {
            poker_unlockgame(tgnum);
            itoa(tgnum, outnum[0], 10);
            // Add bet/discard show support!!
            top_output(OUT_SCREEN, getlang("PokerNotYourTurn"),
                       getlang("Discard"), outnum[0]);
            return;
            }

        plnum = poker_getplayernum(POKPL_PLAYING, od_control.od_node,
                                   &tgame);

        if (bettype == -1)
            {
            for (pgd = 0; pgd < tgame.numcards; pgd++)
                { // Max discard limit in future.
                if (strchr(&word_str[word_pos[3]], pgd + '1'))
                    {
                    if (tgame.player[plnum].cards[pgd] > -1)
                        {
                        tgame.deck[tgame.player[plnum].cards[pgd]] = 2;
                        tgame.player[plnum].cards[pgd] = -1;
                        }
                    poker_dealcard(&tgame,
                    			   &tgame.player[plnum].cards[pgd]);

                    dcount++;
                    }
                }
            poker_sorthand(&tgame, tgame.player[plnum].cards);
            }

        tgame.numcalls++;

        poker_advanceturn(tgnum, &tgame);

        poker_savegame(tgnum, &tgame);
        poker_unlockgame(tgnum);

        itoa(dcount, outnum[8], 10);
        top_output(OUT_SCREEN, getlang("PokerYouDiscard"), outnum[8],
                   getlang(dcount == 1 ?
                           "PokerDiscSingular" : "PokerDiscPlural")),
        poker_sendmessage(MSG_GENERIC, 0, tgnum, 0,
                          top_output(OUT_STRINGNF, getlang("PokerMsgDiscard"),
                                     outnum[0], user.handle, outnum[8],
                                     getlang(dcount == 1 ?
                                             "PokerDiscSingular" :
                                             "PokerDiscPlural")),
                          &tgame);

        if (bettype == -1)
            {
            for (pgd = 0; pgd < tgame.numcards; pgd++)
                { // Max discard limit in future.
                if (strchr(&word_str[word_pos[3]], pgd + '1'))
                    {
                    itoa(tgame.cardvals[tgame.player[plnum].cards[pgd]],
                         outnum[0], 10);
                    sprintf(tlang1, "CardLongSingValue%s", outnum[0]);
                    itoa(tgame.cardsuits[tgame.player[plnum].cards[pgd]] + 1,
                         outnum[0], 10);
                    sprintf(tlang2, "CardLongPluSuit%s", outnum[0]);
                    top_output(OUT_SCREEN, getlang("PokerYouAreDealt"),
                               getlang(tlang1), getlang(tlang2));
                    }
                }
            }

        return;
        }

     if (!stricmp(get_word(2), "FOLD"))
        {
        poker_lockgame(tgnum);
        poker_loadgame(tgnum, &tgame);
        itoa(tgnum, outnum[0], 10);
        if (poker_getplayernum(POKPL_PLAYING, od_control.od_node,
			&tgame) < 0)
            {
            poker_unlockgame(tgnum);
            top_output(OUT_SCREEN, getlang("PokerNotPlaying"), outnum[0]);
            return;
            }
        poker_foldplayer(od_control.od_node, tgnum, &tgame);
        poker_savegame(tgnum, &tgame);
        poker_unlockgame(tgnum);
        top_output(OUT_SCREEN, getlang("PokerYouFold"));
        poker_sendmessage(MSG_GENERIC, 1, tgnum, 0,
                          top_output(OUT_STRINGNF,
                                     getlang("PokerMsgFold"),
                                     outnum[0], user.handle,
                                     getlang(user.gender == 1 ?
                                             "Her" : "His")),
                          &tgame);
        }

    }
