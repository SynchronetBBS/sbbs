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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                          CXXvert  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                           Module  */
/* */
/***************************************************************************/

#define DOSMODEONLY
#include"smurfext.h"
/* #include"smurfcnv.h" */



void 
__CNV__main(void)
{
    /* struct date d; *//* char proc; int a=0,b=0,c=0,e=0; getdate(&d); */
    clrscr();
    /* printf("This program will convert your old Smurf Combat save game
     * files\n"); printf("from version .69a-.91 to version 1.30. If you are
     * not sure which\n"); printf("version you are converting FROM, press
     * letter X to abort. Your\n"); printf("current version number will be
     * displayed on the title screen of\n"); */
    printf("This conversion program CANNOT convert to 1.77.\n");
    printf("You must run 'SMURF RESET'\n\n");

    printf("Press any key ...");


     /* proc= */ getch();
    exit(1);
    /*
     * if(proc=='X' || proc=='x')exit(1);
     * 
     * 
     * clrscr();textcolor(12);cprintf("\n"); cprintf("   S M U R F   C O M B A
     * T\n\n\r");textcolor(10); cprintf("   69-91 Conversion Module\n\r");
     * cprintf("        %sVersion %s\n\r",__vcnvsp,__vcnv); cprintf("     By
     * Laurence Manhken\n\n\r"); textcolor(7); cprintf("[A] Version
     * 0.69a\n\r"); cprintf("[B] Version 0.69b\n\r"); cprintf("[C] Version
     * 0.69c\n\r"); cprintf("[D] Version 0.91\n\n\r"); textcolor(9);
     * cprintf("From Version [abcD]: ");textcolor(12);proc=getch();
     * if(proc=='A' || proc=='a')a=1;	else if(proc=='B' || proc=='b')b=1;
     * else if(proc=='C' || proc=='c')c=1; else e=1;
     * textcolor(9);cprintf("\n\n\rProceed?   [Yn]:
     * ");textcolor(12);proc=getch(); if(proc!='Y' &&
     * proc!='y'){cprintf("Nope\n\r");return;}cprintf("Yea\n\n\r");
     * textcolor(9);cprintf("Sure!?!?!? [Yn]: ");textcolor(12);proc=getch();
     * if(proc!='Y' &&
     * proc!='y'){cprintf("Nope\n\r");return;}cprintf("Yea\n\n\r");
     * textcolor(9);cprintf("SURE!?!?!? [Yn]: ");textcolor(12);proc=getch();
     * if(proc!='Y' &&
     * proc!='y'){cprintf("Nope\n\r");return;}cprintf("Yea\n\n\r");
     * if(a)loadgame69a(); if(b)loadgame69b(); if(c)loadgame69c();
     * if(e)loadgame91(); //if(e)loadgame100();
     * for(cyc=0;cyc<noplayers;cyc++){__morale[cyc]=3;__ettemorale[cyc]=0;}
     * savegame(); */
}
