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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                            SETUP  */
/* REMOTE Maintenance Code: !-SIX-NINE-|                           MODULE  */
/* */
/***************************************************************************/


#ifdef TODO_HEADERS
/* Setup 1.00    */
#include<dir.h>
/* Setup 1.00    */
#include<io.h>
/* Setup 1.00    */
#include<process.h>
#endif
/* Setup 1.00    */
#include<stdio.h>
/* Setup 1.00    */
#include<stdlib.h>
/* Setup 1.00    */
#include<ciolib.h>
/* Setup 1.00    */
#include<fcntl.h>
/* Setup 1.00    */
#include<string.h>
/* Setup 1.00    */
#include<sys/stat.h>
/* Setup 1.00    */
#include<genwrap.h>
/* Setup 1.00    */
#include<filewrap.h>
#include<dirwrap.h>
 /* Setup 1.00    */ char _fname[15];
 /* Setup 1.00    */ typedef struct {
     /* Setup 1.00    */ unsigned char parameter[11];
     /* Setup 1.00    */ char directory[10][90];
     /* Setup 1.00    */ 
}               configrec;
 /* Setup 1.00    */ configrec config;
 /* Setup 1.00    */ char *yesno[2] = {"No", "Yes"};
 /* Setup 1.00    */ void 
__errormessage(int x, char y[15])
{
     /* Setup 1.00    */ printf("Error #%d with file %s", x, y);
    exit(0);
}
 /* Setup 1.00    */ void 
__loadconfiguration(void)
 /* Initiate      */ 
{
     /* Initiate      */ int __userhandle, bytes;
     /* Open Save     */ sprintf(_fname, "bolivia.cfg");
     /* Open Save     */ if ((__userhandle = open(_fname, O_RDONLY | O_BINARY, S_IWRITE | S_IREAD)) == -1)
	__errormessage(1, _fname);
     /* Open Save     */ if ((bytes = read(__userhandle, (void *)&config, sizeof(configrec))) == -1)
	__errormessage(2, _fname);
     /* Close Up      */ close(__userhandle);
     /* Terminate     */ 
}
 /* Setup 1.00    */ void 
__saveconfiguration(void)
 /* Initiate      */ 
{
     /* Initiate      */ int __userhandle, bytes;
     /* Open Save     */ sprintf(_fname, "bolivia.cfg");
     /* Open Save     */ if ((__userhandle = open(_fname, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE)) == -1)
	__errormessage(1, _fname);
     /* Open Save     */ if ((bytes = write(__userhandle, (void *)&config, sizeof(configrec))) != sizeof(configrec))
	__errormessage(3, _fname);
     /* Close Up      */ close(__userhandle);
     /* Terminate     */ 
}
 /* Setup 1.00    */ void 
dline(void)
{
     /* Terminate     */ cprintf("\r                                                                           \r");
}
 /* Setup 1.00    */ void 
dnl(void)
{
     /* Terminate     */ cprintf("\r                                                                           \n\r");
}
 /* Setup 1.00    */ void 
__window(int x1, int x2, int c1, int c2)
 /* Initiate      */ 
{
    int             cyc;
     /* Initiate      */ textcolor(c1);
     /* Initiate      */ textbackground(c2);
     /* Initiate      */ for (cyc = x1; cyc < x2; cyc++) {
	 /* Initiate      */ gotoxy(1, cyc);
	cprintf("บ");
	 /* Initiate      */ gotoxy(80, cyc);
	cprintf("บ");
	 /* Initiate      */ 
    }
     /* Initiate      */ for (cyc = x1; cyc < x2; cyc++) {
	 /* Initiate      */ gotoxy(2, cyc);
	cprintf("                                                                              ");
	 /* Initiate      */ 
    }
     /* Initiate      */ gotoxy(2, x1);
    cprintf("ออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ");
     /* Initiate      */ gotoxy(2, x2);
    cprintf("ออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ");
     /* Initiate      */ gotoxy(1, x1);
    cprintf("ษ");
     /* Initiate      */ gotoxy(1, x2);
    cprintf("ศ");
     /* Initiate      */ gotoxy(80, x1);
    cprintf("ป");
     /* Initiate      */ gotoxy(80, x2);
    cprintf("ผ");
     /* Initiate      */ 
}




 /* Setup 1.00    */ void 
__SET__main(void)
{
     /* Setup 1.00    */ char path[80], pathname[160], path2[80], path3[3],
                    type;
    FILE           *stream;
     /* Setup 1.00    */ textbackground(0);
    clrscr();
     /* Setup 1.00    */ __window(1, 7, 14, 1);
     /* Setup 1.00    */ gotoxy(5, 3);
    cprintf("                        Maartian Enterprises");
     /* Setup 1.00    */ gotoxy(5, 5);
    cprintf("                   Standardized Setup Program 1.0");
     /* Setup 1.00    */ __window(21, 23, 14, 1);
     /* Setup 1.00    */ gotoxy(3, 22);
    cprintf("(C) Copyright 1993 - Laurence Maar - No Fee May Be Charged For This Software");
     /* Setup 1.00    */ window(1, 10, 80, 18);
     /* Setup 1.00    */ gotoxy(1, 1);
    textbackground(0);
     /* Setup 1.00    */ textcolor(10);
    cprintf("Enter Directory Containing BBS Program (No Trailing Backslash) \n\r");
     /* Setup 1.00    */ textcolor(2);
    cprintf("C:\\BBS                                                        \n\r");
     /* Setup 1.00    */ textcolor(15);
    dline();
    cgets(path);
     /* Setup 1.00    */ textcolor(10);
    cprintf("\n\rEnter Directory Containing THIS Program (No Trailing Backslash)\n\r");
     /* Setup 1.00    */ textcolor(2);
    cprintf("C:\\BBS\\SMURF                                                 \n\r");
     /* Setup 1.00    */ textcolor(15);
    dline();
    cgets(path2);
     /* Setup 1.00    */ textcolor(11);
    textcolor(7);
     /* Setup 1.00    */ clrscr();
     /* Setup 1.00    */ cprintf("\n\rSelect Door Information File Format\n\n\r");
     /* Setup 1.00    */ cprintf("[1] CHAIN.TXT    - Apex/Telegard/WWiV\n\r");
     /* Setup 1.00    */ cprintf("[2] DOOR.SYS     - Apex/GAP/RemoteAccess/Spitfire v3/TAG/Telegard/Wildcat v3\n\r");
     /* Setup 1.00    */ cprintf("[3] DORINFOx.DEF - Apex/QBBS/RBBS/RemoteAccess/Telegard\n\r");
     /* Setup 1.00    */ cprintf("[4] SFDOORS.DAT  - Apex/Spitfire/Telegard/TriTel\n\r");
     /* Setup 1.00    */ cprintf("[5] CALLINFO.BBS - Apex/Telegard/Wildcat [A]/Wildcat [B]\n\n\r");
     /* Setup 1.00    */ textcolor(15);
    dline();
    cgets(path3);
    type = atoi(path3);
     /* Setup 1.00    */ textcolor(11);
    textcolor(7);
     /* Setup 1.00    */ window(1, 1, 80, 25);
    clrscr();
     /* Setup 1.00    */ chdir(path);
     /* Setup 1.00    */ sprintf(pathname, "smurf.bat", path);
     /* Setup 1.00    */ stream = fopen(pathname, "w+");
     /* Setup 1.00    */ fprintf(stream, "@echo off\n\r");
     /* Setup 1.00    */ fprintf(stream, "echo The Fossil Driver BNU is included with this program.\n\r", path2);
     /* Setup 1.00    */ fprintf(stream, "echo The author of Smurf Combat encourages that you register\n\r", path2);
     /* Setup 1.00    */ fprintf(stream, "echo BNU if you already havn't done so. This program will\n\r", path2);
     /* Setup 1.00    */ fprintf(stream, "echo not function without a fossil driver such as BNU or X00.                             \n\r", path2);
     /* Setup 1.00    */ fprintf(stream, "echo BNU is (C) 1989 by David Nugent & Unique Computing Ptd Ltd                           \n\r", path2);
     /* Setup 1.00    */ fprintf(stream, "cd %s\n\r", path2);
     /* Setup 1.00    */ if (type == 1)
	fprintf(stream, "copy %s\\CHAIN.TXT\n\r", path2);
     /* Setup 1.00    */ if (type == 2)
	fprintf(stream, "copy %s\\DOOR.SYS\n\r", path2);
     /* Setup 1.00    */ if (type == 3)
	fprintf(stream, "copy %s\\DORINFO*.DEF\n\r", path2);
     /* Setup 1.00    */ if (type == 4)
	fprintf(stream, "copy %s\\SFDOORS.DAT\n\r", path2);
     /* Setup 1.00    */ if (type == 5)
	fprintf(stream, "copy %s\\CALLINFO.BBS\n\r", path2);
     /* Setup 1.00    */ fprintf(stream, "BNU\n\r");
     /* Setup 1.00    */ fprintf(stream, "SMURF %%1 %%2 %%3 %%4 %%5 %%6 %%7 %%8 %%9\n\r");
     /* Setup 1.00    */ fprintf(stream, "cd %s\n\r", path);
     /* Setup 1.00    */ fclose(stream);
     /* Setup 1.00    */ chdir(path2);
     /* Setup 1.00    */ 
}
