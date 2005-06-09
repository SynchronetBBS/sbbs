#include "top.h"

void slots_game(void) ///////!!!!! Massively.
{//proc
slots_stat_typ sstat;

char wasused = 0;

if (!stricmp(get_word(1), "BET"))
	{
    if (strtoul(get_word(2), NULL, 10) < 1L ||
        strtoul(get_word(2), NULL, 10) > 500000L)
    	{
        if (!get_word_char(2, 0))
            {
            sprintf(outbuf, "\r\n^nCurrent Slots bet:  ^l%lu^n.\r\n",
            		slotsbet);
            top_output(OUT_SCREEN, outbuf);
            }
        else
        	{
            top_output(OUT_SCREEN, "\r\n^mInvalid bet!  Bets must be between ^p1^m "
                       "and ^p500000^m.\r\n");
            top_output(OUT_SCREEN, "Sample bet command:  ^kSLOTS BET 150\r\n");
    	    }
        }
    else
    	{
        slotsbet = strtoul(get_word(2), NULL, 10);
        sprintf(outbuf, "\r\n^nSlots bet set to ^l%lu^n.\r\n",
				slotsbet);
        top_output(OUT_SCREEN, outbuf);
        }
    wasused = 1;
    }
if (!stricmp(get_word(1), "PULL"))
	{
    XINT sd, st;
    char snum[5], allok = 1, wintype;
    XINT spincalc, numtimes, pullcount;
    unsigned long payoff;

    if (get_word_char(2, 0))
        {
        numtimes = atoi(get_word(2));
        }
    else
    	{
        numtimes = 1;
        }

    if (numtimes < 1 || numtimes > 10)
    	{
        top_output(OUT_SCREEN, "\r\n^mInvalid number of pulls!  You may only pull "
				   "the lever ^p1^m to ^p10^m\r\ntimes per PULL "
				   "command.\r\n");
        allok = 0;
        }

    for (pullcount = 0; pullcount < numtimes && allok; pullcount++)
    	{
        if (slotsbet < 1L || slotsbet > 500000L)
    		{
            top_output(OUT_SCREEN, "\r\n^mYou must set a bet first!\r\n");
	        allok = 0;
    	    }
	    if (user.cybercash < slotsbet)
    	    {
            sprintf(outbuf, "\r\n^mNot enough CyberCash left!  "
				    "(Bet = ^l%lu^m, CyberCash = ^l%lu^m)\r\n",
	                slotsbet, user.cybercash);
            top_output(OUT_SCREEN, outbuf);
        	allok = 0;
	        }

	    if (allok)
    		{
        	user.cybercash -= slotsbet;
            top_output(OUT_SCREEN, "\r\n\r\n^pThe Lucky 7 Slot Machine "
					   "spins...     ^k");
            if (od_control.baud == 0)
            	{
                st = 38400 / 10 / 10;
            	}
            else
            	{
                st = od_control.baud / 10 / 10;
                }
            for (sd = 0; sd < st; sd++)
    			{
                od_sleep(0);
		        snum[0] = xp_random(10); snum[1] = xp_random(10);
				snum[2] = xp_random(10); snum[3] = xp_random(10);
				snum[4] = xp_random(10);
                sprintf(outbuf, "\b\b\b\b\b%i%i%i%i%i", snum[0], snum[1],
						snum[2], snum[3], snum[4]);
                top_output(OUT_SCREEN, outbuf);
		        }
            top_output(OUT_SCREEN, "\r\n\r\n");
            wintype = 0;

        	if (snum[0] == snum[1] || snum[3] == snum[4])
        		{
                wintype = 1;
    	        }
        	if (snum[0] == 7)
            	{
                wintype = 2;
    	        }
        	if (((snum[0] == snum[1]) && (snum[1] == snum[2])) ||
        		((snum[2] == snum[3]) && (snum[3] == snum[4])))
	            {
                wintype = 3;
        	    }
        	if (snum[0] == 7 && snum[1] == 7)
            	{
                wintype = 4;
    	        }
	        if (((snum[0] == snum[1]) && (snum[1] == snum[2]) &&
				(snum[2] == snum[3])) || ((snum[4] == snum[3]) &&
				(snum[3] == snum[2]) &&	(snum[2] == snum[1])))
            	{
                wintype = 5;
    	        }
	        if (snum[0] == 7 && snum[1] == 7 && snum[2] == 7)
    	        {
                wintype = 6;
            	}
        	if ((snum[0] == snum[1]) && (snum[1] == snum[2]) &&
				(snum[2] == snum[3]) && (snum[3] == snum[4]))
	            {
                wintype = 7;
        	    }
        	if (snum[0] == 7 && snum[1] == 7 && snum[2] == 7 && snum[3] == 7)
            	{
                wintype = 8;
    	        }
	        if (snum[0] == 7 && snum[1] == 7 && snum[2] == 7 &&
				snum[3] == 7 && snum[4] == 7)
        	    {
                wintype = 9;
	            }

            switch(wintype)
        		{
                case 0:
                    payoff = slotsbet;
                    sprintf(outbuf, "^mSorry, you lost your bet of "
							"^l%lu^m CyberDollar%s.\r\n", slotsbet,
							slotsbet == 1 ? "" : "s");
                    top_output(OUT_SCREEN, outbuf);
                    break;
                case 1:
                    payoff = slotsbet * 2L;
                    top_output(OUT_SCREEN, "^kYou got Two Of A Kind!!^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n "
							"CyberDollar%s.\r\n", payoff,
							payoff == 1 ? "" : "s");
                    top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
                case 2:
                    // Change the rest to check at the end for s or no s
                    payoff = slotsbet * 3L;
                    top_output(OUT_SCREEN, "^kYou got One Lucky 7!^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n CyberDollars."
    						"\r\n", payoff);
                    top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
                case 3:
                    payoff = slotsbet * 7L;
                    top_output(OUT_SCREEN, "^I^kYou got Three Of A Kind!!!^A^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n CyberDollars."
    						"\r\n", payoff);
                    top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
                case 4:
                    payoff = slotsbet * 10L;
                    top_output(OUT_SCREEN, "^I^kYou got Two Lucky 7s!!!!^A^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n CyberDollars."
    						"\r\n", payoff);
                    top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
                case 5:
                    payoff = slotsbet * 35L;
                    top_output(OUT_SCREEN, "^I^kYou got Four Of A Kind!!!!!^A^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n CyberDollars."
    						"\r\n", payoff);
                    top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
                case 6:
                    payoff = slotsbet * 77L;
                    top_output(OUT_SCREEN, "^I^kYou got Three Lucky 7s!!!!!!!^A^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n CyberDollars."
    						"\r\n", payoff);
                    top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
                case 7:
                    payoff = slotsbet * 500L;
                    top_output(OUT_SCREEN, "^I^kYou got Five Of A "
							   "Kind!!!!!!!!!!!!^A^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n CyberDollars."
    						"\r\n", payoff);
                    top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
                case 8:
                    payoff = slotsbet * 777L;
                    top_output(OUT_SCREEN, "^I^kYou got Four Lucky "
							   "7s!!!!!!!!!!^A^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n CyberDollars."
    						"\r\n", payoff);
                        top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
                case 9:
                    payoff = slotsbet * 7777L;
                    top_output(OUT_SCREEN, "^I^k**************************************^A^k\r\n");
                    top_output(OUT_SCREEN, "^I^k* You got the JACKPOT!!!!!!!!!!!!!!! *^A^k\r\n");
                    top_output(OUT_SCREEN, "^I^k**************************************^A^n\r\n");
                    sprintf(outbuf, "The payoff was ^l%lu^n CyberDollars."
    						"\r\n", payoff);
                    top_output(OUT_SCREEN, outbuf);
                    user.cybercash += payoff;
                    break;
    			}
            save_user_data(user_rec_num, &user);
            sprintf(outbuf, "\r\n^jYou now have ^o%lu^j CyberDollars."
    				"\r\n\r\n", user.cybercash);
            top_output(OUT_SCREEN, outbuf);
            load_slots_stats(&sstat);
            sstat.pulls++;
            sstat.spent += slotsbet;
            sstat.hits[wintype]++;
            sstat.totals[wintype] += payoff;
            save_slots_stats(&sstat);
        	}
    	}
	wasused = 1;
    }
if (!stricmp(get_word(1), "ODDS"))
	{
    top_output(OUT_SCREEN, "\r\n\r\n            ^oLucky 7 Slots ^h- ^pOdds and Payoff Table\r\n");
    top_output(OUT_SCREEN, "^c+------------------+---------------+-------------+-----------+\r\n");
    top_output(OUT_SCREEN, "^c| ^mCombination Name ^c| ^mExample       ^c| ^mOdds        ^c| ^mPayoff    ^c|\r\n");
    top_output(OUT_SCREEN, "^c+------------------+---------------+-------------+-----------+\r\n");
    top_output(OUT_SCREEN, "^c| ^nTwo Of A Kind    ^c| ^pnnxxx / xxxnn ^c| ^o1 ^jin ^o5      ^c| ^l   2 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c| ^nOne Lucky 7      ^c| ^p  7 x x x x   ^c| ^o1 ^jin ^o10     ^c| ^l   3 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c| ^nThree Of A Kind  ^c| ^pnnnxx / xxnnn ^c| ^o1 ^jin ^o50     ^c| ^l   7 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c| ^nTwo Lucky 7s     ^c| ^p  7 7 x x x   ^c| ^o1 ^jin ^o100    ^c| ^l  10 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c| ^nFour Of A Kind   ^c| ^pnnnnx / xnnnn ^c| ^o1 ^jin ^o500    ^c| ^l  35 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c| ^nThree Lucky 7s   ^c| ^p  7 7 7 x x   ^c| ^o1 ^jin ^o1000   ^c| ^l  77 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c| ^nFive Of A Kind   ^c| ^p  n n n n n   ^c| ^o1 ^jin ^o10000  ^c| ^l 500 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c| ^nFour Lucky 7s    ^c| ^p  7 7 7 7 x   ^c| ^o1 ^jin ^o10000  ^c| ^l 777 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c| ^nLucky 7 JACKPOT! ^c| ^p  7 7 7 7 7   ^c| ^o1 ^jin ^o100000 ^c| ^l7777 ^nto ^l1 ^c|\r\n");
    top_output(OUT_SCREEN, "^c+------------------+---------------+-------------+-----------+\r\n");
    top_output(OUT_SCREEN, "^c| ^nOverall          ^c| ^i- - - - - - - ^c| ^o1 ^jin ^o3.0011 ^c| ^i- - - - - ^c|\r\n");
    top_output(OUT_SCREEN, "^c+------------------+---------------+-------------+-----------+\r\n\r\n");
    wasused = 1;
	}
if (!stricmp(get_word(1), "STATS"))
	{
    unsigned long tval1, tval2;
    long tval3;
    char ssd;

    load_slots_stats(&sstat); save_slots_stats(&sstat);
    sprintf(outbuf, "\r\n\r\n                 ^oLucky 7 Slots ^h- ^pStatistics\r\n"); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c+------------------+-------------+-------------------------+\r\n"); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^mCategory         ^c| ^mOccurrences ^c| ^mCyberCash Totals        ^c|\r\n"); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c+------------------+-------------+-------------------------+\r\n"); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nNo Prize         ^c| ^l%11lu ^c| ^o%11lu ^j(Collected) ^c|\r\n", sstat.hits[0], sstat.totals[0]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nTwo Of A Kind    ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[1], sstat.totals[1]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nOne Lucky 7      ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[2], sstat.totals[2]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nThree Of A Kind  ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[3], sstat.totals[3]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nTwo Lucky 7s     ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[4], sstat.totals[4]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nFour Of A Kind   ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[5], sstat.totals[5]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nThree Lucky 7s   ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[6], sstat.totals[6]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nFive Of A Kind   ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[7], sstat.totals[7]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nFour Lucky 7s    ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[8], sstat.totals[8]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nLucky 7 JACKPOT! ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", sstat.hits[9], sstat.totals[9]); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c+------------------+-------------+-------------------------+\r\n"); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c| ^nTotal  -  Wagers ^c| ^l%11lu ^c| ^o%11lu ^j(Wagered)   ^c|\r\n", sstat.pulls, sstat.spent); top_output(OUT_SCREEN, outbuf);
    for (tval1 = 0, tval2 = 0, ssd = 1; ssd < 10; tval1 += sstat.hits[ssd],
		 tval2 += sstat.totals[ssd], ssd++);
    sprintf(outbuf, "^c|           ^nPrizes ^c| ^l%11lu ^c| ^o%11lu ^j(Awarded)   ^c|\r\n", tval1, tval2); top_output(OUT_SCREEN, outbuf);
    tval3 = sstat.spent - tval2;
    sprintf(outbuf, "^c|           ^nProfit ^c|             ^c| ^o%11li ^j(Profit)    ^c|\r\n", tval3); top_output(OUT_SCREEN, outbuf);
    sprintf(outbuf, "^c+------------------+-------------+-------------------------+\r\n\r\n"); top_output(OUT_SCREEN, outbuf);
    wasused = 1;
	}

if (!wasused)
	{
    top_output(OUT_SCREEN, "\r\n\r\n^oLucky 7 Slots ^h- ^pCommands\r\n");
    top_output(OUT_SCREEN, "^c------------------------\r\n\r\n");
    top_output(OUT_SCREEN, "^lHELP     ^g- ^mThis help screen.\r\n");
    top_output(OUT_SCREEN, "^lBET xxxx ^g- ^mChange bet to ^oxxxx^m (^k1^m to ^k500000^m).  Omit ^oxxxx^m to show current bet.\r\n");
    top_output(OUT_SCREEN, "^lPULL xx  ^g- ^mPull the lever ^oxx^m times (^k1^m to ^k10^m).  Omit ^oxx^m to pull just once.\r\n");
    top_output(OUT_SCREEN, "^lODDS     ^g- ^mShow odds and payoff table.\r\n");
    top_output(OUT_SCREEN, "^lSTATS    ^g- ^mShow cumulative Lucky 7 Statistics.\r\n");
    top_output(OUT_SCREEN, "\r\n^nTo use a command, type ^lSLOTS^n followed by a "
			   "space and then the command.\r\n\r\n");
    }

return;
}

void load_slots_stats(slots_stat_typ *slbuf)
{

lseek(slotsfil, 0L, SEEK_SET);
rec_locking(REC_LOCK, slotsfil, 0L, sizeof(slots_stat_typ));
read(slotsfil, slbuf, sizeof(slots_stat_typ));

return;
}

void save_slots_stats(slots_stat_typ *slbuf)
{

lseek(slotsfil, 0L, SEEK_SET);
write(slotsfil, slbuf, sizeof(slots_stat_typ));
rec_locking(REC_UNLOCK, slotsfil, 0L, sizeof(slots_stat_typ));

return;
}
