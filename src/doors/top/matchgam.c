#include "top.h"

typedef struct  ////////////////!!!!!!!!!!!! Entire module
	{
    unsigned long freq[20];              // Times sq picked
    unsigned long placefreq[3][20];      // Times sq picked as pick #
    unsigned long chosen[4];             // Times pr picked
    unsigned long placechose[3][4];      // Times pr picked as pick #
    unsigned long combos[64];            // Times pr combo
    unsigned long wins[4];               // Times pr won
    unsigned long losses;                // Times lost
    unsigned long wincds[4];             // CDs pr won
    unsigned long losscds;               // CDs lost
    unsigned long winfreq[20][4];        // Times sq picked when pr won
    unsigned long lossfreq[20];          // Times sq picked as lost
    unsigned long times[20][4];          // Times pr in sq
    unsigned long seltimes[20][4];       // Times pr picked while in sq
    } match_stat_typ;

void matchdisp(char *prg, char *arr, char *pick);
void matchdone(char *prg, char *arr, char *pick);
void matchstat(char *prg, char *arr, char *pick);
void load_match_stats(match_stat_typ *mtbuf);
void save_match_stats(match_stat_typ *mtbuf);
void matchstat(char *prg, char *arr, char *pick);

char matchfreq[4] = { 7, 6, 4, 3 }; // Config maybe
char matchprize[4] = { 5, 15, 30, 60 }; // Config maybe
char *matchcol[4] = { "^B^o", "^F^p", "^E^p", "^K^p" }; // Lang maybe

void matchgame(void)
{
static char matchprog;
static char matcharr[20];
static char matchpick[3];
char wasused = 0, md, mdd, mx; //proc!

if (!stricmp(get_word(1), "SHOW"))
    {
    if (matchprog < 1 || matchprog > 3)
        {
        top_output(OUT_SCREEN, getlang("MatchGameNotOn"));
        }
	else
		{
    	matchdisp(&matchprog, matcharr, matchpick);
    	wasused = 1;
    	}
	}
if (!stricmp(get_word(1), "START"))
    {
    if (matchprog < 1 || matchprog > 3)
        {
        if (user.cybercash < 100000L)
            {
            ultoa(100000L, outnum[0], 10);
            ultoa(user.cybercash, outnum[1], 10);
            top_output(OUT_SCREEN, getlang("MatchNotEnoughCash"), outnum[0],
                       outnum[1]);
            return;
            }

        user.cybercash -= 100000L;
        save_user_data(user_rec_num, &user);
        matchpick[0] = -1; matchpick[1] = -1; matchpick[2] = -1;
        memset(matcharr, -1, 20);
        for (md = 0; md < 4; md++)
            {
            for (mdd = 0; mdd < matchfreq[md]; mdd++)
                {
                do
                    {
                    mx = xp_random(20);
                    }
                while(matcharr[mx] != -1);
                matcharr[mx] = md;
                }
            }
        matchprog = 1;
        top_output(OUT_SCREEN, getlang("MatchNewGame"));
        matchdisp(&matchprog, matcharr, matchpick);
        }
    else
        {
        top_output(OUT_SCREEN, getlang("MatchGameAlreadyOn"));
        }
    wasused = 1;
    }

if (atoi(get_word(1)) >= 1 && atoi(get_word(1)) <= 20 && !wasused)
    {
    if (matchprog < 1 || matchprog > 3)
        {
        top_output(OUT_SCREEN, getlang("MatchGameNotOn"));
        }
    else
        {
        if (atoi(get_word(1)) - 1 != matchpick[0] &&
            atoi(get_word(1)) - 1 != matchpick[1])
            {
            matchpick[matchprog - 1] = atoi(get_word(1)) - 1;
            matchprog++;
            matchdisp(&matchprog, matcharr, matchpick);
            if (matchprog == 4)
                {
                matchdone(&matchprog, matcharr, matchpick);
                }
            }
        else
            {
            top_output(OUT_SCREEN, getlang("MatchAlreadyPicked"));
            }
        }
    wasused = 1;
    }

if (!wasused)
    { // To ANSI screen!
    top_output(OUT_SCREEN, "\r\n\r\nMatchGame 3/20 Commands\r\n");
    top_output(OUT_SCREEN, "-----------------------\r\n\r\n");
    top_output(OUT_SCREEN, "SHOW  - Shows the board.\r\n");
    top_output(OUT_SCREEN, "START - Starts a new MatchGame!\r\n");
    top_output(OUT_SCREEN, "<##> - A number between 1 and 20 selects that number.\r\n");
    top_output(OUT_SCREEN, "\r\nThe object of MatchGame 3/20 is to select three of the same prize values.\r\n");
    top_output(OUT_SCREEN, "You only get three picks, so you will have to get lucky!\r\n\r\n");
    wasused = 1;
    }

}

//#pragma argsused
void matchdisp(char *prg, char *arr, char *pick)
{
XINT dd;

for (dd = 0; dd < 20; dd++)
    {
    if (dd % 5 == 0 || dd == 0)
        {
        top_output(OUT_SCREEN, getlang("MatchDispSep"));
        od_sleep(0);
        }
    if (pick[0] == dd || pick[1] == dd || pick[2] == dd)
        {
        itoa(matchprize[arr[dd]], outnum[0], 10);
        strcpy(outbuf, top_output(OUT_STRING, getlang("MatchDispPicked"),
        						  matchcol[arr[dd]], outnum[0]));
        }
    else
        {
        itoa(dd + 1, outnum[0], 10);
        strcpy(outbuf, top_output(OUT_STRING, getlang("MatchDispUnPicked"),
        						  outnum[0]));
        }
    top_output(OUT_SCREEN, outbuf);
    }
top_output(OUT_SCREEN, getlang("MatchDispBoardEnd"));

}

//#pragma argsused
void matchdone(char *prg, char *arr, char *pick)
{

if (arr[pick[0]] == arr[pick[1]] && arr[pick[1]] == arr[pick[2]])
    {
    ultoa(100000L * (long) matchprize[arr[pick[0]]], outnum[0], 10);
    top_output(OUT_SCREEN, getlang("MatchWin"), outnum[0]);
    user.cybercash += 100000L * (long) matchprize[arr[pick[0]]];
    save_user_data(user_rec_num, &user);
    }
else
    { // Change to use cfg!
    top_output(OUT_SCREEN, getlang("MatchLose"), "100000");
    }

}

//#pragma argsused
void matchstat(char *prg, char *arr, char *pick)
{
match_stat_typ mstat;

load_match_stats(&mstat);

mstat.freq[pick[0]]++;
mstat.freq[pick[1]]++;
mstat.freq[pick[2]]++;

mstat.chosen[arr[pick[0]]]++;
mstat.chosen[arr[pick[1]]]++;
mstat.chosen[arr[pick[2]]]++;

mstat.combos[(arr[pick[0]] * 16) + (arr[pick[1]] * 4) + (arr[pick[2]])]++;

mstat.placechose[0][arr[pick[0]]]++;
mstat.placechose[1][arr[pick[1]]]++;
mstat.placechose[2][arr[pick[2]]]++;

if (arr[pick[0]] == arr[pick[1]] && arr[pick[1]] == arr[pick[2]])
	{
//    mstat.wins[arr[pick
	}


}

void load_match_stats(match_stat_typ *mtbuf)
{

lseek(matchfil, 0L, SEEK_SET);
rec_locking(REC_LOCK, matchfil, 0L, sizeof(match_stat_typ));
read(matchfil, mtbuf, sizeof(match_stat_typ));

return;
}

void save_match_stats(match_stat_typ *mtbuf)
{

lseek(matchfil, 0L, SEEK_SET);
write(matchfil, mtbuf, sizeof(match_stat_typ));
rec_locking(REC_UNLOCK, matchfil, 0L, sizeof(match_stat_typ));

return;
}
