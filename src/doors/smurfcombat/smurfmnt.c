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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                         MAINTENT  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                         MODULE    */
/* */
/***************************************************************************/

#include "smurfext.h"
#include "mdel.h"




void 
maint(int level)
{
    char            inputline[80], x = 1;
    char           *leveln[3] = {"Alien", "Author", "SysOp"};
    int             cyc4;
    /* stream = fopen("SMURF.LOG", "a+");  fprintf(stream,
     * "MAINT(%i):",od_control.user_num);  fprintf(stream, "%s ...
     * ",smurfname[thisuserno]); */
    /* fclose(stream); */
    do {
	od_clr_scr();
#ifdef TODO_LOCAL_DISPLAY
	gotoxy(1, 20);
	printf("Local ECho Only: Press Alt-C To Chat!");
	gotoxy(1, 1);
#endif
	od_set_colour(L_YELLOW, D_BLACK);
	od_printf("\n\rLevel %d (%s) ", level, leveln[level]);
	od_set_colour(L_RED, D_BLACK);
	od_printf("maintenance signing in...\n\n\r");
	od_set_colour(L_GREEN, D_BLACK);
	od_printf("[1] Cycle thru Status of All Players\n\r");
	od_printf("[2] Cycle thru and Rename Players\n\r");
#ifdef TODO_LOCAL_DISPLAY
	od_printf("[3] Reroll Current Game\n\r");
#endif
	if (level == 1) {
	    od_set_colour(L_CYAN, D_BLACK);
	    od_printf("[A] Display: Smurf Player Records\n\r");
	    od_printf("[B] Display: Smurf Log Records\n\r");
	    od_printf("[C] Display: Smurf Save Game File\n\r");
	    od_printf("[Z] Purge:   Smurf Record Files\n\r");
	}
	od_printf("[Q] Return to Previous Mode\n\r");
	od_printf("[*] Full Player Information Mode\n\r");
	od_set_colour(D_GREY, D_BLACK);
	od_printf("\n\rCommand [Official Release] :");
	bbsinkey = od_get_key(TRUE);
	od_set_colour(D_CYAN, D_BLACK);
	switch (bbsinkey) {
	    case '1':
		for (cyc4 = 0; cyc4 < noplayers; cyc4++) {
		    inuser = cyc4;
		    displaystatus();
		    smurf_pause();
		} break;
	    case '2':
		for (cyc4 = 0; cyc4 < noplayers; cyc4++) {
		    inuser = cyc4;
		    displaystatus();
		    od_set_colour(L_BLUE, D_BLACK);
		    od_printf("Change Name [yN]: ");
		    bbsinkey = od_get_key(TRUE);
		    if (bbsinkey == 'Y' || bbsinkey == 'y')
			if (realnumb[cyc] != 0) {
			    od_printf("Yea\n\n\r");
		    getname:;
			    od_set_colour(L_YELLOW, D_BLACK);
			    od_printf(": ");
			    od_set_attrib(SD_GREEN);
			    od_input_str(inputline, 28, 32, 167);
			    sprintf(smurfname[cyc], "%s", strupr(inputline));
			    for (cyc2 = 0; cyc2 + 1 < noplayers; cyc2++) {
				od_set_attrib(SD_GREEN);
				if (strcmp(smurfname[cyc2], smurfname[cyc]) == 0 && smurfname[cyc2] != smurfname[cyc]) {
				    od_set_colour(L_CYAN, D_BLACK);
				    od_printf("Same name found in rebel roster!!!\n\n\n\r");
				    goto getname;
				}
			    }
			    od_set_attrib(SD_CYAN);
			    od_printf("Is this cool? ");
			    bbsinkey = od_get_key(TRUE);
			    od_set_colour(D_CYAN, D_BLACK);
			    if (bbsinkey == 'Y' || bbsinkey == 'y') {
				od_printf("Yea\n\r");
			    } else {
				od_printf("Nope\n\r");
				nl();
				goto getname;
			    }
			} else {
			    od_set_colour(L_BLUE, D_BLACK);
			    od_printf("Can't edit a computer player!\n\r");
			    nl();
			    smurf_pause();
		    } else {
			od_set_colour(L_BLUE, D_BLACK);
			od_printf("Nope\n\r");
			nl();
		    }
		}
		break;
#ifdef TODO_LOCAL_DISPLAY
	    case '3':
		__NEW__main();
		smurf_pause();
		od_exit(10, FALSE);
		break;
#endif
	    case 'A':
	    case 'a':
		if (level == 1) {
		    od_clr_scr();
		    od_printf("Press something...\n\r");
		    od_get_key(TRUE);
		    od_send_file("smurflog.nfo");
		    od_printf("\n\n\rDone.");
		    od_get_key(TRUE);
		} break;
	    case 'B':
	    case 'b':
		if (level == 1) {
		    od_clr_scr();
		    od_printf("Press something...\n\r");
		    od_get_key(TRUE);
		    od_send_file("smurf.log");
		    od_printf("\n\n\rDone.");
		    od_get_key(TRUE);
		} break;
	    case 'C':
	    case 'c':
		if (level == 1) {
		    od_clr_scr();
		    od_printf("Press something...\n\r");
		    od_get_key(TRUE);
		    od_send_file("smurf.sgm");
		    od_printf("\n\n\rDone.");
		    od_get_key(TRUE);
		} break;
	    case 'Z':
	    case 'z':
		if (level == 1) {
		    mdel("*.SEL");
		    mdel("*.LOG");
		    mdel("*.SGL");
		    mdel("*.SCL");
		} break;
	    case '*':
		od_set_colour(D_CYAN, D_BLACK);
		od_printf("Real names now displayed at status!\n\n\r");
		displayreal = 1;
		break;
	    default:
		od_clr_scr();
		od_printf("\n\n\rReturning to Game ...\n\n\r");
		return;
	} od_printf("\n\n\r");
    } while (x);
}
