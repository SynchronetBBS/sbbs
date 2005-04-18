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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!		         GENERAL   */
/* REMOTE Maintenance Code: !-SIX-NINE-|                          MODULE   */
/* */
/***************************************************************************/

#include "smurfext.h"





/* +-----------------------|  +---------------+----------| |  0  | Black
 * |  |  0  | Black   |   Off    | |  1  | Blue            |  |  1  | Blue
 * |   Off    | |  2  | Green           |  |  2  | Green   |   Off    | |  3
 * | Cyan            |  |  3  | Cyan    |   Off    | |  4  | Red
 * |  |  4  | Red     |   Off    | |  5  | Magenta         |  |  5  | Magenta
 * |   Off    | |  6  | Brown           |  |  6  | Brown   |   Off    | |  7
 * | White (grey)    |  |  7  | White   |   Off    | |  8  | Bright Black
 * |  |  8  | Black   |    On    | |  9  | Bright Blue     |  |  9  | Blue
 * |    On    | |  a  | Bright Green    |  |  a  | Green   |    On    | |  b
 * | Bright Cyan     |  |  b  | Cyan    |    On    | |  c  | Bright Red
 * |  |  c  | Red     |    On    | |  d  | Bright Magenta  |  |  d  | Magenta
 * |    On    | |  e  | Yellow          |  |  e  | Brown   |    On    | |  f
 * | White (bright)  |  |  f  | White   |    On    |
 * +-----------------------+  +--------------------------+ */


void 
titlescreen(void)
{
    char            spacing[81];
    int             spacz;
#ifdef TODO_WRAPPERS
    strset(spacing, 0);
#else
    memset(spacing, 0, sizeof(spacing));
#endif
    detectwin();
    if (!statis)
	sprintf(registered_name, "UNREGISTERED");
    /* spacz=((80-(strlen(registered_name)+17))/2+1);
     * for(cyc=0;cyc<spacz;cyc++)strcat(spacing,"
     * ");od_set_colour(L_BLUE,D_BLACK); od_clr_scr();
     * od_set_attrib(SD_CYAN);  od_disp_str("\n\rYou grin as a crackling
     * sounds beneath your feet . . .\n\n\n\n\r"); od_set_attrib(SD_RED);
     * od_disp_str("                              SúMúUúRúF
     * CúOúMúBúAúT\n\n\n\r"); od_set_attrib(SD_YELLOW);od_disp_str("
     * By Severe Disorder\n\r"); textcolor(15);           cprintf    ("
     * (Laurence Manhken)\n\r"); od_set_attrib(SD_GREEN); od_printf  ("
     * %sVersion %s\n\n\r",__vspace,__version); od_set_attrib(SD_GREEN);
     * od_disp_str("                                  Copyright
     * 1993\n\n\n\r"); */
    od_send_file("smurfnew.nfo");

    od_set_colour(7, 0);
    od_printf("Registered to: ");
    od_printf("%s\n\r", registered_name);

    if (!statis) {
	od_printf(" Note that this copy is unregistered and registration is ");
	od_set_colour(L_CYAN, D_BLACK);
	od_printf("FREE.\n\r");
	sleep(2);
    } else
	od_printf(" Thank your SysOp for registering me!\n\r");
    smurf_pause();
    od_send_file("smurfver.nfo");
    smurf_pause();
    od_clr_scr();
    od_set_color(7, 0);
    od_send_file("smurfsal.nfo");
    smurf_pause();
}




void 
displaymenu(void)
{
    /* stream = fopen("SMURF.LOG", "a+");  fprintf(stream,
     * "DMAIN(%i):",od_control.user_num);fprintf(stream, "%s ...
     * ",smurfname[thisuserno]);fclose(stream); */
    od_clr_scr();
    od_set_attrib(SD_RED);
    od_disp_str("                            \nSúMúUúRúF  CúOúMúBúAúT\n\n\r");
    od_set_colour(L_MAGENTA, D_BLACK);
    od_printf("            n Combat Arena Level (1-10) ? Display Smurf Menu\n\r");
    od_printf("            A Attack Another Smurf      R Rank Other Smurfs\n\r");
    od_printf("            G Give money to Smurf       H Heal Injuries\n\r");
    od_printf("            L List Armaments            P Purchase Armaments\n\r");
    od_printf("            B Swap Abilities            C Upgrade Confinment\n\r");
    od_printf("            S Smurf Status              Z Spy On Another Smurf\n\r");
    od_printf("            + Hide All Your Money       - Get Money from Hiding Place\n\r");
    od_printf("            !AUTO Kill Character        !REROLL Reroll Character\n\r");
    od_printf("\n\r");
    od_printf("            ( Rendez Vous Menu          ) Smurfette Menu\n\r");
    od_printf("            * Civil Operations Menu     %% Secret Service\n\r");
    od_printf("            ~ Instructions **           ^ Hostage Menu\n\r");
    od_printf("            # Change Last Words         & Change Smurf Alias\n\r");
    od_printf("\n\n\r");
}
