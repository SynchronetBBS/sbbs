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
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                         MAIN      */
/* REMOTE Maintenance Code: !-SIX-NINE-|                         MODULE    */
/* */
/***************************************************************************/

#include"sockwrap.h"
#include"smurfdef.h"
#include"smurfdat.h"
#include"smurfver.h"

int             nada = 0;
int             registered = FALSE;

void 
protov(void)
{
    od_clr_scr();
    od_set_color(11, 0);
    od_printf("!********** S");
    od_set_color(3, 0);
    od_printf("MURF ");
    od_set_color(11, 0);
    od_printf("C");
    od_set_color(3, 0);
    od_printf("OMBAT ");
    od_set_color(11, 0);
    od_printf("v%s ", __version);
    od_set_color(11, 0);
    od_printf("**********!\n\r");
    od_set_color(15, 0);
    od_printf("(");
    od_set_color(7, 0);
    od_printf("C");
    od_set_color(15, 0);
    od_printf(")");
    od_set_color(7, 0);
    od_printf("opyright 1993");
    od_set_color(15, 0);
    od_printf(",");
    od_set_color(7, 0);
    od_printf("1994 ");
    od_set_color(15, 0);
    od_printf("L");
    od_set_color(7, 0);
    od_printf("aurence ");
    od_set_color(15, 0);
    od_printf("M");
    od_set_color(7, 0);
    od_printf("aar\n\n\r");

    od_set_color(7,0); od_printf("Program:    ");
      
     if(statis) od_printf("Registered\n\r"); else
     od_printf("Unregistered\n\r");
      
     od_printf("Interbbs:   n/a (Local Game Only)\n\r");
     
     od_printf("Synchronet: n/a (Project Discontinued)\n\n\r");

/*
    od_set_color(7, 0);
    od_printf("This version is intended to be run SysOps who wish to participate in the M.E.\n\r");
    od_printf("game testing program. You will be automatically listed as an OFFICIAL site in\n\r");
    od_printf("our next release upon report of ANY program malfunction.    Simply report the\n\r");
    od_printf("nature and location of the error. Not only will your BBS be advertised nation\n\r");
    od_printf("wide, but you will be able to enjoy error free games!\n\r");
*/

    od_set_color(15, 0);
    od_printf("Laurence Maar (909)861-1616.   1415 Valeview Dr.   Diamond Bar, CA 91765-4337\n\n\r");

    /* stream = fopen("smurf.log", "a+"); */
    /* fprintf(stream,
     * "\n\n\rLOGON(%i):",od_control.user_num);fprintf(stream, "%s ...
     * ",realname[thisuserno]);fprintf(stream,
     * "%s\n\r",smurfname[thisuserno]); */
    /* fclose(stream); */

    od_set_color(12, 0);
    od_printf("컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴\n\r");
    nada = 1;
}







#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPSTR lpszCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
    int             stat;
    char           *sp;
#ifdef _WIN32
int     argc;
char*   argv[100];

    argv[0] = "market.exe";
    argv[1] = strtok(lpszCmdLine, " \t");
    for (argc = 1; argv[argc];) {
        argc++;
        argv[argc] = strtok(NULL, " \t");
    }
#endif

    registered = TRUE;
    directvideo = 0;
    if (argc > 1 && strnicmp(argv[1], "REGISTER", 8) == 0) {
	__REG__main();
	exit(0);
    }
    if (argc > 1 && strnicmp(argv[1], "RESET", 5) == 0) {
	__NEW__main();
	exit(0);
    }
    if (argc > 1 && strnicmp(argv[1], "SETUP", 5) == 0) {
	__SET__main();
	exit(0);
    }
    if (argc > 1 && strnicmp(argv[1], "CNV", 3) == 0) {
	__CNV__main();
	exit(0);
    }
    if (argc > 1) {
	int arg=1;

	if(strnicmp(argv[arg], "LOCAL", 5) == 0) {
		od_control.od_force_local=TRUE;
		arg++;
	}
	if(argc > arg) {
		strcpy(od_control.info_path,argv[arg]);
		if(isdir(argv[arg]))
			strcat(od_control.info_path, DIRSEP_STR);
		arg++;
	}
	if(argc > arg) {
		int type,len,got;
		SOCKET sock;
#ifdef _WIN32
		WSADATA crap;
		WSAStartup(0x0202, &crap);
#endif

		od_control.od_open_handle=atoi(argv[arg]);
		sock=od_control.od_open_handle;
		len=sizeof(type);
		if((got=getsockopt(sock, SOL_SOCKET, SO_TYPE, (void *)&type, &len))==0 && type==SOCK_STREAM)
			od_control.od_use_socket=TRUE;
	}
    }
#ifdef TODO_LOCAL_DISPLAY
    __mess(5);
#endif
    od_init();
#ifdef TODO_STATUS_LINE
    od_control.od_status_on = FALSE;
    od_set_statusline(STATUS_NONE);
#endif
#ifdef TODO_LOCAL_DISPLAY
    window(1, 1, 80, 25);
    clrscr();
#endif
    od_control.od_clear_on_exit = FALSE;
    od_control.od_nocopyright = TRUE;
    checkkey();
    if (od_control.user_num < 1)
	od_control.user_num = 1;
    if (od_control.user_num == 1)
	sprintf(od_control.user_name, "System Operator");
#ifdef TODO_LOCAL_DISPLAY
    window(1, 1, 80, 25);
    _wscroll = 0;
    textbackground(1);
    textcolor(15);
    gotoxy(1, 25);
    cprintf(" %-20s � TR  2.xx  %s � (C)opyright 1993,1994 Laurence Maar ", od_control.user_name, __version);
    window(1, 1, 80, 24);
    _wscroll = 1;
#endif
    titlescreen();
    loadgame();
    checkstatus();
    protov();
    displaystatus();
    getcommand();
    od_exit(10, FALSE);
    return(1);
}

void 
getcommand(void)
{
    char            bbsin4[10];
    int             numba;
    do {
#ifdef TODO_LOCAL_DISPLAY
	textcolor(7);
#endif
	od_set_colour(D_GREY, D_BLACK);
	od_printf("Command (?=Help) :");
	bbsinkey = od_get_key(TRUE);
	od_set_colour(D_CYAN, D_BLACK);
	showexit = 0;
	if (bbsinkey == '!' && enterpointer == 0) {
	    enterpointer = 1;
	    what();
	} else if (bbsinkey == 'A' && enterpointer == 1) {
	    enterpointer = 2;
	    what();
	} else if (bbsinkey == 'U' && enterpointer == 2) {
	    enterpointer = 3;
	    what();
	} else if (bbsinkey == 'T' && enterpointer == 3) {
	    enterpointer = 4;
	    what();
	} else if (bbsinkey == 'O' && enterpointer == 4) {
	    enterpointer = 0;
	    what();
	    automate();
	} else if (bbsinkey == 'R' && enterpointer == 1) {
	    enterpointer = 12;
	    what();
	} else if (bbsinkey == 'E' && enterpointer == 12) {
	    enterpointer = 13;
	    what();
	} else if (bbsinkey == 'R' && enterpointer == 13) {
	    enterpointer = 14;
	    what();
	} else if (bbsinkey == 'O' && enterpointer == 14) {
	    enterpointer = 15;
	    what();
	} else if (bbsinkey == 'L' && enterpointer == 15) {
	    enterpointer = 16;
	    what();
	} else if (bbsinkey == 'L' && enterpointer == 16) {
	    enterpointer = 0;
	    what();
	    newplayer(2);
#ifdef ALLOW_CHEATING
	} else if (bbsinkey == '$' && enterpointer == 1) {
	    what();
	    smurfmoney[thisuserno] += 1000000;
#endif
	} else if (bbsinkey == 'S' && enterpointer == 1 && od_control.user_num == 1) {
	    enterpointer = 0;
	    maint(2);
	} else {
	    enterpointer = 0;

	    /* if(bbsinkey=='#' &&
	     * enterpointer==1){enterpointer=34;what();}else if(bbsinkey=='@'
	     * && enterpointer==1){enterpointer=64;what();}else
	     * if(bbsinkey=='/' &&
	     * enterpointer==0){enterpointer=69;what();}else if(bbsinkey=='/'
	     * && enterpointer==69){enterpointer=0;tolocal();}else
	     * if(bbsinkey=='1' &&
	     * enterpointer==1){enterpointer=0;wongame();}else
	     * if(bbsinkey=='2' &&
	     * enterpointer==1){enterpointer=0;wingame();}else
	     * if(bbsinkey=='Z' && enterpointer==64){what();od_printf("No.
	     * Players:
	     * ");od_input_str(bbsin4,2,'0','9');numba=atoi(bbsin4);noplayers=
	     * numba;}else if(bbsinkey=='S' &&
	     * enterpointer==64){what();smurfstr[thisuserno]++;}else
	     * if(bbsinkey=='D' &&
	     * enterpointer==64){what();smurfspd[thisuserno]++;}else
	     * if(bbsinkey=='I' &&
	     * enterpointer==64){what();smurfint[thisuserno]++;}else
	     * if(bbsinkey=='R' &&
	     * enterpointer==64){what();smurfcon[thisuserno]++;}else
	     * if(bbsinkey=='B' &&
	     * enterpointer==64){what();smurfbra[thisuserno]++;}else
	     * if(bbsinkey=='C' &&
	     * enterpointer==64){what();smurfchr[thisuserno]++;}else
	     * if(bbsinkey=='P' &&
	     * enterpointer==64){what();smurfhpm[thisuserno]=smurfhpm[thisuser
	     * no]+5;}else if(bbsinkey=='X' &&
	     * enterpointer==64){userlist(1);od_printf("Change To Who:
	     * ");od_input_str(bbsin4,2,'0','9');savegame();thisuserno=(atoi(b
	     * bsin4))-1;}else if(bbsinkey=='+' &&
	     * enterpointer==34){what();smurfturns[thisuserno]++;}else
	     * if(bbsinkey=='0' &&
	     * enterpointer==34){what();smurflose[thisuserno]=0;}else
	     * if(bbsinkey=='Z' &&
	     * enterpointer==34){enterpointer=0;what();hcount=1;writehostage()
	     * ;}else if(bbsinkey=='$' &&
	     * enterpointer==34){what();smurfmoney[thisuserno]+=100000;}else
	     * if(bbsinkey=='^' &&
	     * enterpointer==1){enterpointer=2;what();}else if(bbsinkey=='('
	     * && enterpointer==2){enterpointer=3;what();}else
	     * if(bbsinkey=='|' &&
	     * enterpointer==3){enterpointer=0;maint(1);}else
	     * if(bbsinkey=='S' && enterpointer==1 &&
	     * od_control.user_num==1){enterpointer=0;maint(2);}else{enterpoin
	     * ter=0; if(bbsinkey=='Y' &&
	     * enterpointer==5){enterpointer=6;what();}else if(bbsinkey=='S'
	     * && enterpointer==6){enterpointer=7;what();}else
	     * if(bbsinkey=='O' &&
	     * enterpointer==7){enterpointer=8;what();}else if(bbsinkey=='P'
	     * && enterpointer==8){enterpointer=9;what();}else
	     * if(bbsinkey=='!' &&
	     * enterpointer==9){enterpointer=0;maint(2);}else{enterpointer=0; */

	    switch (bbsinkey) {
		case '%':
		    service();
		    break;
		case '~':
		    stealmoney();
		    break;
		case '^':
		    hostagemenu();
		    break;
		case '&':
		    changename();
		    break;
		case '#':
		    changelast();
		    break;
		case ')':
		    ettemenu();
		    break;
		case 'A':
		case 'a':
		    userarena();
		    break;
		case '*':
		    inciterevolt();
		    break;
		case '(':
		    rendezvous();
		    break;
		case 'G':
		case 'g':
		    givemoney();
		    break;
		case 'H':
		case 'h':
		    heal();
		    break;
		case 'B':
		case 'b':
		    abilities();
		    break;
		case 'Z':
		case 'z':
		    spy();
		    break;
		case '?':
		    displaymenu();
		    break;
		case '+':
		    smurfbankd[thisuserno] = smurfbankd[thisuserno] + smurfmoney[thisuserno];
		    smurfmoney[thisuserno] = 0;
		    nl();
		    od_printf("You now have %.0f in your hiding place.\n\n\r", smurfbankd[thisuserno]);
		    break;
		case '-':
		    smurfmoney[thisuserno] = smurfmoney[thisuserno] + smurfbankd[thisuserno];
		    smurfbankd[thisuserno] = 0;
		    nl();
		    od_printf("You now have %.0f on hand.\n\n\r", smurfmoney[thisuserno]);
		    break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '0':
		    arena();
		    break;
		case 'R':
		case 'r':
		    userlist(0);
		    break;
		case 'L':
		case 'l':
		    itemlist();
		    break;
		case 'P':
		case 'p':
		case 'W':
		case 'w':
		    buyitem();
		    od_printf("\n\n\r");
		    break;
		case 'C':
		case 'c':
		case 'U':
		case 'u':
		    buyconf();
		    od_printf("\n\n\r");
		    break;
		case 'S':
		case 's':
		    inuser = thisuserno;
		    displaystatus();
		    break;
		case 'Q':
		case 'q':
		    savegame();
		    od_set_colour(D_CYAN, D_BLACK);
		    od_printf("\n\rLeave the village? ");
		    bbsinkey = od_get_key(TRUE);
		    od_set_colour(L_CYAN, D_BLACK);
		    if (bbsinkey == 'Y' || bbsinkey == 'y') {
			od_printf("Yea\n\n\r");
			writeSGL();
			lastquote();
			bbsexit = 1;
			od_control.user_screen_length = 23;
			break;
		    } else
			od_printf("Nope\n\n\r");
		    break;
		default:
		    what();
		    enterpointer = 0;
	    }
	}
    } while (bbsexit != 1 && smurfexper[thisuserno] < 100000000);
    if (smurfexper[thisuserno] >= 100000000)
	wingame();
    od_exit(10, FALSE);
}

void 
displaystatus(void)
{
    char            nom[80];
    int             thostcount = 0;
    /* stream = fopen("smurf.log", "a+"); */
    /* fprintf(stream, "STATZ(%i):",od_control.user_num);fprintf(stream, "%s
     * ... ",smurfname[thisuserno]);  fclose(stream); */
    if (smurfsex[inuser] == 001)
	sprintf(nom, "%s (M)", smurfname[inuser]);
    else
	sprintf(nom, "%s (F)", smurfname[inuser]);
    if (smurfsex[thisuserno] != 001 && smurfsex[thisuserno] != 002) {
	asksex();
	savegame();
    }
    if (!nada)
	od_clr_scr();
    else
	nada = 0;
    od_set_colour(D_GREY, D_BLACK);
    if (smurfhp[inuser] < 1)
	sprintf(power, "0(%i)", smurfhpm[inuser]);
    else
	sprintf(power, "%i(%i)", smurfhp[inuser], smurfhpm[inuser]);
    if (smurfettelevel[inuser] == 0)
	sprintf(ette, "None (#0)");
    else if (smurflevel[inuser] > smurflevel[thisuserno] + 39)
	sprintf(ette, ">Unknown<");
    else
	sprintf(ette, "%s (#%i)", smurfettename[inuser], smurfettelevel[inuser]);
    if (smurflevel[inuser] > smurflevel[thisuserno] + 19)
	sprintf(weap, ">Unknown<");
    else
	sprintf(weap, "%s (#%i)", smurfweap[inuser], smurfweapno[inuser] + 1);
    if (smurflevel[inuser] > smurflevel[thisuserno] + 19)
	sprintf(armr, ">Unknown<");
    else
	sprintf(armr, "%s (#%i)", smurfarmr[inuser], smurfarmrno[inuser] + 1);
    if (smurflevel[inuser] > smurflevel[thisuserno] + 9)
	sprintf(conf, ">Unknown<");
    else
	sprintf(conf, "%s (#%i)", smurfconf[inuser], smurfconfno[inuser] + 1);

    counthostages();
    thostcount = numhost[inuser];
    if (thostcount == 0)
	sprintf(host, "None");
    else
	sprintf(host, "%i", thostcount);
    if (smurflevel[inuser] > smurflevel[thisuserno] + 19)
	sprintf(host, "?");
    else if (displayreal == 1) {
	od_printf("%s (#%i)\n\r", realname[inuser], realnumb[inuser]);
    }
    od_printf("%-30sLevel: %-10i              Wins/Losses: %i/%i\r\n", nom, smurflevel[inuser], smurfwin[inuser], smurflose[inuser]);
    od_printf("Power: %-12s           Experience: %-10.0f         Needed: %-10.0f\n\r", power, smurfexper[inuser], defexper[smurflevel[inuser]]);
    od_printf("MITe:  %-4i                   SPeeD: %-4i                    INTel: %-4i\n\r", smurfstr[inuser], smurfspd[inuser], smurfint[inuser]);
    od_printf("REGen: %-4i                   BRAve: %-4i                    CHaRm: %-4i\n\r", smurfcon[inuser], smurfbra[inuser], smurfchr[inuser]);
    od_printf("Weap:  %-23sArmor: %-24sTurns Left: %i\n\r", weap, armr, smurfturns[inuser]);
    od_printf("Ette:  %-22s Confinment: %-18s Hostages: %s\n\r", ette, conf, host);
    od_printf("Gold:  %-14.0f         Hiding Place: %-14.0f   Fights Left: %i/%i\n\n\r", smurfmoney[inuser], smurfbankd[inuser], roundsleft, smurffights[inuser]);
    savegame();
    od_set_colour(L_CYAN, D_BLACK);
    if (__morale[inuser] > 1)
	od_printf("Morale: %s\n\n\r", _morale[__morale[inuser]]);
    if (__morale[inuser] < 2)
	od_printf("Morale: %s%s\n\n\r", _morale[__morale[inuser]], dataref2[smurfsex[inuser]]);
    if (inuser == thisuserno)
	checklevel();
}









void 
checklevel(void)
{
    int             getthis, oke = 1;
    do {
	if (smurfexper[thisuserno] > defexper[smurflevel[thisuserno]]) {
	    od_set_colour(L_YELLOW, D_BLACK);
	    smurflevel[thisuserno]++;
	    od_printf("You are now level %i!        \n\r", smurflevel[thisuserno]);
	    od_set_colour(D_CYAN, D_BLACK);
	    getthis = xp_random(8);
	    if (getthis < 3) {
		od_printf("Might increased!             \n\r");
		smurfstr[thisuserno]++;
	    }
	    getthis = xp_random(8);
	    if (getthis < 3) {
		od_printf("Speed increased!             \n\r");
		smurfspd[thisuserno]++;
	    }
	    getthis = xp_random(8);
	    if (getthis < 3) {
		od_printf("Intelligence increased!      \n\r");
		smurfint[thisuserno]++;
	    }
	    getthis = xp_random(8);
	    if (getthis < 3) {
		od_printf("Regeneration increased!      \n\r");
		smurfcon[thisuserno]++;
	    }
	    getthis = xp_random(8);
	    if (getthis < 3) {
		od_printf("Bravery increased!           \n\r");
		smurfbra[thisuserno]++;
	    }
	    getthis = xp_random(8);
	    if (getthis < 3) {
		od_printf("Charm increased!             \n\r");
		smurfchr[thisuserno]++;
	    }
	    od_set_colour(L_CYAN, D_BLACK);
	    getthis = xp_random(40);
	    if (getthis == 0) {
		od_printf("Hit Points soared!           \n\r");
		smurfhpm[thisuserno] += 10;
	    } else
		od_printf("Hit Points increased!        \n\r");
	    getthis = xp_random(40);
	    if (getthis == 0) {
		od_printf("Might jumped up seven!       \n\r");
		smurfstr[thisuserno] += 6;
	    }
	    getthis = xp_random(40);
	    if (getthis == 0) {
		od_printf("Speed jumped up seven!       \n\r");
		smurfspd[thisuserno] += 6;
	    }
	    getthis = xp_random(40);
	    if (getthis == 0) {
		od_printf("Intelligence jumped up seven!\n\r");
		smurfint[thisuserno] += 6;
	    }
	    getthis = xp_random(40);
	    if (getthis == 0) {
		od_printf("Regeneration jumped up seven!\n\r");
		smurfcon[thisuserno] += 6;
	    }
	    getthis = xp_random(40);
	    if (getthis == 0) {
		od_printf("Bravery jumped up seven!     \n\r");
		smurfbra[thisuserno] += 6;
	    }
	    getthis = xp_random(40);
	    if (getthis == 0) {
		od_printf("Charm jumped up seven!       \n\r");
		smurfchr[thisuserno] += 6;
	    }
	    getthis = xp_random(40);
	    if (getthis == 0)
		increasemorale();
	    getthis = (smurfcon[thisuserno] / 2 + xp_random(smurfcon[thisuserno] / 5));
	    smurfhpm[thisuserno] = smurfhpm[thisuserno] + getthis;
	    smurfhp[thisuserno] = smurfhpm[thisuserno];
	    nl();
	    sprintf(tempenform, "%s gained a level!\n\r", smurfname[thisuserno]);
	    newshit(tempenform);
	} else
	    oke = 0;
    } while (oke);
}








void            checkstatus(void){
    char            logname[13], intext[5], in2[5];
    time_t          t;
    struct date     d;
    char            olddate[10], newdate[10];
    getdate(&d);
    sprintf(newdate, "%02d%02d%04d", d.da_day, d.da_mon, d.da_year);
    stream = fopen("smurf.hst", "a+");
    fclose(stream);
    stream = fopen("smurf.hst", "r+");
    cyc = 0;
    do {
	fscanf(stream, "%3s%3s", intext, in2);
	hostage[cyc] = atoi(intext);
	holder[cyc] = atoi(in2);
	cyc++;
	hcount++;
    } while (feof(stream) == 0);
    fclose(stream);

    detectsave();
    detectwin();
    detectversion();

    if ((stream = fopen("smurf.cfg", "r+")) == NULL) {
	stream = fopen("smurf.cfg", "w+");
	fprintf(stream, "01000311111990");
	fprintf(stream, "\n\r\n\r1. Arena Rounds/Day ###\n\r2. Turns/Day ###\n\r3. Last Ran Date DDMMYYYY\n\r");
	fclose(stream);
    }
    stream = fopen("smurf.cfg", "r+");
    fscanf(stream, "%3s", intext);
    defrounds = atoi(intext);
    fscanf(stream, "%3s", intext);
    turnsaday = atoi(intext);
    fscanf(stream, "%8s", olddate);
    fclose(stream);

    stream = fopen("smurf.cfg", "w+");
    fprintf(stream, "%03i", defrounds);
    fprintf(stream, "%03i", turnsaday);
    fprintf(stream, "%8s", newdate);
    fprintf(stream, "\n\r\n\r1. Arena Rounds/Day\n\r2. Turns/Day\n\r3. Last Ran Date DDMMYYYY\n\r");
    fclose(stream);

    if (strcmp(olddate, newdate) != 0) {
	od_clr_scr();
	od_set_colour(L_BLUE, D_BLACK);
	od_printf("Doing daily maintenance . . .\n\r");
	od_printf("Spawning Smurf Combat Day . . .\n\r");
	od_printf("Standing By . . .\n\r");
	/* stream = fopen("SMURF.LOG", "a+"); */
	/* fprintf(stream,
	 * "\n\n\r------------------------------------------------------------
	 * ------------------\n\n\r"); */
	/* fprintf(stream, "SPAWN:) [SMURF DAY] Ran Daily Maintenance
	 * Successfully!\n\r"); */
	/* fclose(stream); */
	__DAY__main();
	/* if(spawnl(P_WAIT,"SMURFDAY",NULL)==-1){od_printf("Unable to locate
	 * program!!!\n\r");stream = fopen("SMURF.LOG", "a+");fprintf(stream,
	 * "SPAWN(%i): Daily Maintenance
	 * *****FAILED!*****",od_control.user_num);fclose(stream);loadgame();}
	 * else { */
	loadgame();
	od_printf("Success!\n\r");
	smurf_pause();
    }
    smurfturns[thisuserno]--;
#ifdef TODO_WRAPPERS
    randomize();
#else
    srand(time(NULL));
#endif
    thisuserno = 255;

    for (cyc = 0; cyc < noplayers; cyc++) {
	if (realnumb[cyc] == od_control.user_num) {
	    thisuserno = cyc;
	    inuser = cyc;
	}
    }

    killeduser();
    if (thisuserno == 255)
	newplayer(1);
    smurfturns[thisuserno]--;
    torturepoints = 10;
    if (statis)
	vpoints = 25;
    else
	vpoints = 10;
    etpoints = smurfettelevel[thisuserno] * 10;
    if (etpoints > 25)
	etpoints = 25;

    srand((unsigned)time(&t));
    smurffights[thisuserno] = 3;
    roundsleft = defrounds;

    if (smurfturns[thisuserno] < 0) {
	smurfturns[thisuserno] = 0;
	smurffights[thisuserno] = 0;
	roundsleft = 0;
    }
    sprintf(logname, "smurf.%03i", thisuserno);
    od_control.user_screen_length = 23;
    __mess(3);			       /* Update Bulletins */
    __mess(1);			       /* Smurf Times */
    __mess(2);			       /* Personal Information */
    __mess(4);			       /* Hostage Information */
    od_control.user_screen_length = 999;
    counthostages();

    for (cyc = 0; cyc < hcount; cyc++)
	if (hostage[cyc] == thisuserno)
	    phostage();

    /* for(cyc=0;cyc<hcount;cyc++){if(holder[cyc]==thisuserno &&
     * roundsleft>0)hostagemenu();} */
}
