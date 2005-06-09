#include "top.h"

char poker_loadgame(XINT gamenum, poker_game_typ *gamebuf)
    {
    char lres;

    if (((long) gamenum + 1L) * (long) sizeof(poker_game_typ) >
        filelength(pokdatafil))
        {
        return 0;
        }

    lres = lseek(pokdatafil, (long) gamenum * (long) sizeof(poker_game_typ),
                 SEEK_SET);
    if (lres == -1)
        {
        return 0;
        }
    lres = read(pokdatafil, gamebuf, sizeof(poker_game_typ));
    if (lres == -1)
        {
        return 0;
        }

    pokeretimes[gamenum] = gamebuf->eventtime;
    pokerdtimes[gamenum] = gamebuf->delaytime;
    pokerwtimes[gamenum] = gamebuf->waittime;

    return 1;
    }

char poker_savegame(XINT gamenum, poker_game_typ *gamebuf)
    {
    char sres;

    pokeretimes[gamenum] = gamebuf->eventtime;
    pokerdtimes[gamenum] = gamebuf->delaytime;
    pokerwtimes[gamenum] = gamebuf->waittime;

    if (((long) gamenum + 1L) * (long) sizeof(poker_game_typ) >
        filelength(pokdatafil))
        {
        chsize(pokdatafil,
               ((long) gamenum + 1L) * (long) sizeof(poker_game_typ));
        }

    sres = lseek(pokdatafil, (long) gamenum * (long) sizeof(poker_game_typ),
                 SEEK_SET);
    if (sres == -1)
        {
        return 0;
        }
    sres = write(pokdatafil, gamebuf, sizeof(poker_game_typ));
    if (sres == -1)
        {
        return 0;
        }

    return 1;
    }

void poker_lockgame(XINT gamenum)
    {

    rec_locking(REC_LOCK, pokdatafil,
                (long) gamenum * (long) sizeof(poker_game_typ),
                sizeof(poker_game_typ));

	pokerlockflags[gamenum] = 1;

    }

void poker_unlockgame(XINT gamenum)
    {

    rec_locking(REC_UNLOCK, pokdatafil,
                (long) gamenum * (long) sizeof(poker_game_typ),
                sizeof(poker_game_typ));

	pokerlockflags[gamenum] = 0;

    }

void poker_sendmessage(XINT smtype, char smecho, XINT smnum,
                       char smplayonly, unsigned char *smstr,
                       poker_game_typ *smgame)
    {
    XINT smd, tnn;

    for (smd = 0; smd < smgame->maxpl; smd++)
        {
        if (smplayonly)
        	{
            tnn = smgame->player[smd].node;
            }
    	else
        	{
            tnn = smgame->wanttoplay[smd];
            }
        if (tnn > -1)
            {
            if (tnn != od_control.od_node || smecho)
            	{
	            msgextradata = smnum;
    	        dispatch_message(smtype, smstr, tnn, 1, 0);
                }
            }
        }

    }

