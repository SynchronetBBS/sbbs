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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                          MESSAGE  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                           MODULE  */
/* */
/***************************************************************************/

#include<datewrap.h>
#include"smurfext.h"





void 
__mess(int x)
{
    char            pch, logname[13], intext[5], in2[5];
    time_t          t;
    struct date     d;
    char            olddate[10], newdate[10];
    strcpy(logname, "smurflog.000");
    getdate(&d);
    sprintf(newdate, "%02d%02d%04d", d.da_day, d.da_mon, d.da_year);
    if (x == 1) {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("SMURF TIMES %d/%d/%d\n\n\r", d.da_day, d.da_mon, d.da_year);
	od_set_colour(L_BLUE, D_BLACK);
	od_send_file("smurf.new");
	smurf_pause();
	stream = fopen(logname, "a+");
	fclose(stream);
    }
    if (x == 2) {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("PERSONAL SMURF LOG AS OF %d/%d/%d\n\n\r", d.da_day, d.da_mon, d.da_year);
	od_set_colour(L_BLUE, D_BLACK);
	od_send_file(logname);
	smurf_pause();
	stream = fopen(logname, "w+");
	fclose(stream);
    }
    if (x == 3) {
	od_clr_scr();
	od_set_colour(7, 0);
	od_send_file("smurfdat.d0k");
	smurf_pause();
    }
    if (x == 13) {
	od_clr_scr();
	od_send_file("smurfdat.d04");
    }
    if (x == 4) {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("HOSTAGE INFORMATION\n\n\r");
	od_set_colour(L_BLUE, D_BLACK);
	for (cyc2 = 0; cyc2 < hcount; cyc2++)
	    if (hostage[cyc2] != 255 && hostage[cyc2] < noplayers)
		od_printf("%s is a hostage of %s with %i%% chance of escape.\n\r", smurfname[hostage[cyc2]], smurfname[holder[cyc2]], 100 - (smurfconfno[holder[cyc2]] + 1) * 8);
	smurf_pause();
    }
    if (x == 5) {
	clrscr();
	printf("Smurf Combat Local Echo Info:\n\n");
	printf("      SMURF SETUP    : Initial Game Setup\n");
	printf("      SMURF RESET    : Reroll Current Game\n");
	printf("      SMURF SYSOP    : Local SysOp Menu\n");
	/* printf("      SMURF CNV      : Convert Save Game Type\n"); */
	printf("      SMURF REGISTER : Register Game\n");
	printf("      Door Info File : CHAIN.TXT/DOOR.SYS etc must be in current dir\n\n\n");
	sleep(2);
    }
    if (x == 10) {
	od_clr_scr();
	od_set_colour(L_RED, D_BLACK);
	od_printf("*** WARNING! ***\n\n\r");
	od_set_colour(D_CYAN, D_BLACK);
	od_printf("The version of the save file is obsolete.\n\r");
	od_printf("Tell your SYSOP to run ");
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("'SMURF RESET'\n\n\r");
	smurf_pause();
	od_exit(0, FALSE);
    }
}
