/***************************************************************************/
/* */
/* sss                       fff     ccc                 b               */
/* s   s                     f   f   c   c                b               */
/* s                         f       c                    b               */
/* sss  mmm mmm  u   u r rr  fff    c      ooo  mmm mmm   bbb  aaa  ttt  */
/* s m  m   m u   u rr   f       c     o   o m  m   m b   b a  a  t   */
/* s   s m  m   m u  uu r    f       c   c o   o m  m   m b   b aaaa  t   */
/* sss  m  m   m  uu u r    f        ccc   ooo  m  m   m  bbb  a  a  t   */
/* */
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                          WinGame  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                           Module  */
/* */
/***************************************************************************/

#include "genwrap.h"
#include "datewrap.h"
#include "smurfext.h"





void 
writeD0E(void)
{
    stream = fopen("smirfdat.d0e", "w+");
    fprintf(stream, "%s", smurfname[thisuserno]);
    fclose(stream);
}

void 
wingame(void)
{
    struct date     d;
    getdate(&d);
    od_clr_scr();
    od_control.user_screen_length = 999;
    od_send_file("smurfdat.d0a");
    smurf_pause();
    od_send_file("smurfdat.d0b");
    sleep(5);
    od_send_file("smurfdat.d0c");
    sleep(5);
    od_send_file("smurfdat.d0d");
    smurf_pause();
    writeD0E();
    sprintf(__saveversion, "w0%02dw", d.da_mon + 1);
    savegame();
    od_send_file("smurfdat.d0z");
    smurf_pause();
    od_exit(10, FALSE);
}

void 
wongame(void)
{
    char            intext[4];
    struct date     d;
    getdate(&d);
    stream = fopen("smurf.sgm", "r+");
    fscanf(stream, "%3s", intext);     /* noplayers=atoi(intext); */
    fscanf(stream, "%2s", intext);     /* save blanks */
    fscanf(stream, "%2s", intext);     /* save month */
    fclose(stream);
    if (d.da_mon > atoi(intext) || d.da_mon < atoi(intext) - 1) {
	od_clr_scr();
	od_printf("Rerolling game, please hold...");
	__NEW__main();
	__DAY__main();
	return;
    }
    od_set_colour(15, 0);
    od_clr_scr();
    nl();
    nl();
    od_printf("The game has been won, but there will be other lands and\n\r");
    od_printf("and other worlds to be conquered...\n\n\r");
    od_printf("Until then?\n\r");
    od_printf("Ask any authorized member of management to reroll\n\r");
    od_set_colour(L_RED, D_BLACK);
    od_printf("Smurf Combat...\n\n\r");
    od_set_colour(L_GREEN, D_BLACK);
    od_send_file("smurfdat.d0e");
    od_printf("... ");
    od_set_colour(D_GREEN, D_BLACK);
    od_printf("some heros will never be forgotten...\n\n\n\r");
    smurf_pause();
    od_send_file("smurfdat.d0z");
    smurf_pause();
    od_send_file("smurfdat.d0m");
    smurf_pause();
    od_exit(10, FALSE);
}
