//                        0 - Critical Door error (no fossil, etc.)
//                        1 - Carrier lost, user off-line
//                        2 - Sysop terminated call, user off-line
//                        3 - User time used up, user STILL ON-LINE
//                        4 - Keyboard inactivity timeout, user off-line
//                       10 - User chose to return to BBS
//                       11 - User chose to logoff, user off-line
//                       12 - Critical NY2008 error


// include the header
#include "ny2008.h"

// include prototypes for fights
#include "fights.h"

unsigned _stklen=20000U;

// price arrays
DWORD    gun_price[ULTRASOUND_GUN+1] =  {
                                            0,
                                            70,
                                            1500,
                                            7000,
                                            30000,
                                            130000,
                                            500000,
                                            2000000,
                                            8000000,
                                            30000000,
                                            130000000,
                                            500000000,
                                            300,
                                            600,
                                            3000,
                                            12000,
                                            70000,
                                            300000,
                                            900000,
                                            5500000,
                                            15000000,
                                            80000000,
                                            150000000};
INT16     drug_price[HEROIN+1] = {10, 25,  50, 100, 150, 150};



// Declare global variables
INT16 nCurrentUserNumber;
INT16 forced_maint=FALSE;
INT16 temp_len_stor;
INT16 temp_rip_stor;
INT16 temp_ans_stor;
INT16 temp_avt_stor;
user_rec cur_user;
enemy enemy_rec;
char uname[36];
INT16 do_maint=TRUE;
char str[15];
INT16 reg_check=345;
INT16 expert=1;
INT16 no_rip_m=0;
INT16 rip=FALSE;
char *t_buffer=NULL;
INT16 registered=786;
const char *ver="1.01";
const char *verinfo="";
INT16 gamedisk,flagdisk=-1;
INT16 *regptr;
char gamedir[MAX_PATH],flagdir[MAX_PATH];
char c_dir_g=0;
INT16 no_wrt_sts=FALSE;
INT16 no_kernel=TRUE;
INT16 no_slices=FALSE;
INT16 autodetect=TRUE;
INT16 max_fights=20;
INT16 max_drug_hits=32767;
INT16 condom_price=5;
INT16 delete_after=15;
INT16 bank_interest=5;
char ansi_name[MAX_PATH]=ANSI_SCORES_FILENAME;
char ascii_name[MAX_PATH]=ASCII_SCORES_FILENAME;
INT16 do_scr_files=TRUE;
char rec_credit[36];
char maint_exec[MAX_PATH]="";
INT16 single_node=FALSE;
INT16 clean_mode=FALSE;

char *badwords[50];
INT16 badwordscnt=0;
INT16 dobad=FALSE;

INT16 check_o_nodes=2;

INT16 busted_ch_bank   =33;
INT16 busted_ch_food   =33;
INT16 busted_ch_rape   =33;
INT16 busted_ch_beggar =20;
INT16 busted_ch_car    =33;
INT16 busted_ch_school =33;
INT16 busted_ch_window =20;
INT16 busted_ch_poison =25;
INT16 busted_ch_bomb   =25;
INT16 success_ch_bank  =50;
INT16 success_ch_food  =50;
INT16 success_ch_rape  =50;
INT16 success_ch_beggar=50;
INT16 success_ch_car   =65;
INT16 success_ch_school=50;
INT16 success_ch_window=95;
INT16 success_ch_poison=33;
INT16 success_ch_bomb  =33;

char oneframe=FALSE;
INT16 reset=FALSE;
INT16 noevents=TRUE;
INT16 time_slice_value=1500;

tIBInfo IBBSInfo;
INT16 ibbs=FALSE;
INT16 ibbs_send_nodelist=FALSE;
INT16 ibbs_maint_i=FALSE;
INT16 ibbs_maint_o=FALSE;
INT16 ibbs_i_mail=FALSE;
INT16 ibbs_operator=FALSE;
INT16 ibbs_game_num=0;

glob_t fff;
glob_t ff;
char	**fname;

/*
 * Date Stuff for *nix
 */
#ifdef __unix__
#include <time.h>
struct date {
	INT16 da_year;
	char  da_day;
	char  da_mon;
};

void getdate(struct date *nyd)  {
	time_t tim;
	struct tm *dte;

	tim=time(NULL);
	dte=localtime(&tim);
	nyd->da_year=dte->tm_year+1900;
	nyd->da_day=dte->tm_mday;
	nyd->da_mon=dte->tm_mon+1;
}
#endif


void
time_slice(void) {
#if 0	/* ToDo */
	asm {
	    mov ax,time_slice_value
	    INT16 15
	}
#else
	od_sleep(1);
#endif
}

INT16
copyfile(const char *file1,const char *file2) {
	FILE *f1,*f2;
	char *buffer;
	INT32 len1,where,x,y;


	f1=ShareFileOpen(file1,"rb");
	if(f1==NULL)
		return 0;
	f2=ShareFileOpenAR(file2,"wb");
	if(f2==NULL) {
		fclose(f1);
		return 0;
	}

	buffer=(char *)malloc(10000);

	len1=filelength(fileno(f1));
	x=len1/10000;

	for(y=0;y<x;y++) {
		ny_fread(buffer,10000,1,f1);
		ny_fwrite(buffer,10000,1,f2);
	}

	y=len1-(x*10000);

	if(y>0) {
		ny_fread(buffer,y,1,f1);
		ny_fwrite(buffer,y,1,f2);
	}

	fclose(f1);
	fclose(f2);
	free(buffer);

	return 1;
}

void fullscreen_chat(void);

void
ny_chat(void) {
	scr_save();
	od_control.od_chat_active=FALSE;
	fullscreen_chat();
	scr_res();
}

void
NoDropFile(void) {
	printf("\nNew York 2008 %s %s - No dropfile found!\n",ver,verinfo);
	printf("To start in Local mode type:\nNY2008 -L\n");
	exit(10);
}

void
trim(char *numstr) {
	INT16 x;
	for(x=strlen(numstr)-1;numstr[x]==' ' && x>=0;x--)
		od_kernal;
	numstr[x+1]=0;
	strrev(numstr);
	for(x=strlen(numstr)-1;numstr[x]==' ' && x>=0;x--)
		od_kernal;
	numstr[x+1]=0;
	strrev(numstr);
}

void
loadbadwords(void) {
	FILE *fp;

	char *temp;

	badwordscnt=0;

	fp=ShareFileOpenAR(BADWORDS_FILENAME,"rt");
	
	if(fp==NULL)
		return;

	INT32 len=filelength(fileno(fp));

	if(len>4000) {
		ny_disp_emu("\n\r\n`%BADWORDS FILE TOO BIG!\n\r\n");
		fclose(fp);
		return;
	}

	temp=(char *)malloc(len+1);
	if(temp==0) {
		ny_disp_emu("\n\r\n`%Not Enough memory for BADWORDS.TXT\n\r\n");
		fclose(fp);
		return;
	}

	for(INT16 x=0;fscanf(fp,"%s",temp)!=EOF;x++) {

		badwords[x]=(char *)malloc(strlen(temp)+1);
		if(badwords[x]==0) {
			ny_disp_emu("\n\r\n`%Not Enough memory for BADWORDS.TXT\n\r\n");
			fclose(fp);
			free(temp);
			return;
		}
		strupr(temp);
		strcpy(badwords[x],temp);
		badwordscnt=x+1;
	}

	free(temp);
	fclose(fp);
}

extern "C" int
main(int argc,char *argv[]) {
	FILE *justfile,*njustfile;
	char key;
	WORD uintval;
	char numstr[26];
	scr_rec srec;
	INT16 cnt,intval,x;
	scr_rec rec;
	date today;
	date lastday;

	//put your opendoors reg here
	strcpy(od_registered_to,"Your Name");
	od_registration_key=00000000;

#ifdef __unix__

	srandomdev();
#else

	randomize();
#endif

	od_control.od_no_file_func=NoDropFile;
	od_control.od_disable = DIS_BPS_SETTING | DIS_NAME_PROMPT;

	od_control.od_num_keys=1;
	od_control.od_hot_key[0]=26624;
	od_control.od_hot_function[0]=UserStatus;

	od_control.od_default_rip_win=TRUE;

	od_add_personality("NY2008",1,23,ny_pers);

	od_control.od_default_personality=ny_pers;

	regptr=&registered;

	//  directvideo=0;

	strcpy(od_control.od_prog_name,"New York 2008");

	rec_credit[0]=0;

	od_control.od_config_function = CustomConfigFunction;
	if(fexist(CFG_FILENAME))
		od_control.od_config_file = INCLUDE_CONFIG_FILE;
	else
		od_control.od_config_file = NO_CONFIG_FILE;
	od_control.od_config_filename = CFG_FILENAME;

	od_control.od_mps=INCLUDE_MPS;
	od_control.od_nocopyright=TRUE;
	od_control.od_cbefore_chat=ny_chat;
	od_control.od_cbefore_shell=scr_save;
	od_control.od_cafter_shell=scr_res;


	cnt=1;
	if(argc>1) {
		do {
			if (strnicmp(argv[cnt],"RESET",5)==0) {
				od_control.od_force_local=TRUE;
				reset=TRUE;
			} else if (strnicmp(argv[cnt],"-IBBSM",6)==0) {
				ibbs_i_mail=TRUE;
				od_control.od_force_local=TRUE;
			} else if (strnicmp(argv[cnt],"-RIP",4)==0) {
				rip=166;
			} else if (strnicmp(argv[cnt],"-L",2)==0) {
				od_control.od_force_local=TRUE;
				clrscr();
				textbackground(LIGHTCYAN);
				textcolor(BLUE);
				gotoxy(1,7);
				cprintf("ษอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออป");
				gotoxy(1,8);
				cprintf("บ New York 2008 v%-5s %-20s                                   บ",ver,verinfo);
				gotoxy(1,9);
				cprintf("บ Starting in local mode input your name: ฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐ บ");
				gotoxy(1,10);
				cprintf("ศอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออผ");
				gotoxy(43,9);

				INT16 cntv=0;
				intval=TRUE;
				do {
					if(cntv>=35) {
						cntv=34;
						putch('\b');
						key=getch();
						if(key=='\n' || key=='\r')
							cntv=35;
					} else {
						key=getch();
					}

					if(key==27) {
						gotoxy(1,9);
						cprintf("บ Starting in local mode input your name: ฐ Canceled ... ฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐ บ");
						gotoxy(1,12);
						exit(10);
					}

					if(intval==TRUE) {
						if (key>='a' && key<='z')
							key-=32;
						intval=FALSE;
					} else if(intval==FALSE) {
						if (key>='A' && key<='Z')
							key+=32;
					}
					if(key==' ') {
						putch('ฐ');
						key=0;
						intval=TRUE;
					} else if(key=='\b') {
						if(cntv==0) {
							intval=TRUE;
							key=0;
							cntv--;
						} else {
							cntv-=2;
							if(cntv>=0 && od_control.user_name[cntv]==' ')
								intval=TRUE;
							if(cntv==32)
								cprintf("\bฐฐ\b\b");
							else
								cprintf("\bฐ\b");
							key=0;
						}
					}

					if(key!=0) {
						od_control.user_name[cntv]=key;
						putch(key);
					}
					cntv++;
				} while (key!='\n' && key!='\r');
				od_control.user_name[cntv-1]=0;

				trim(od_control.user_name);

			} else if (strnicmp(argv[cnt],"-NAD",4)==0) {
				autodetect=FALSE;
			} else if (strnicmp(argv[cnt],"-P",2)==0) {
				strzcpy(od_control.info_path,argv[cnt],2,61);
			} else if (strnicmp(argv[cnt],"-RDBPS",6)==0) {
				od_control.od_disable &=~ DIS_BPS_SETTING;
			} else if (strnicmp(argv[cnt],"-CL",3)==0) {
				clean_mode=TRUE;
				/*      } else if (strnicmp(argv[cnt],"-DV",3)==0) {
					directvideo=1; */
			} else if (strnicmp(argv[cnt],"-NM",3)==0) {
				do_maint=FALSE;
			} else if (strnicmp(argv[cnt],"-CR",3)==0) {
				od_control.od_force_local=TRUE;
				od_init();
				od_control.od_status_on=FALSE;
				od_set_statusline(STATUS_NONE);
				getcwd(gamedir,MAX_PATH);
				gamedisk=gamedir[0] - 'A';
				strzcpy(gamedir,gamedir,2,MAX_PATH);
				if(flagdisk==-1) {
					flagdisk=gamedisk;
					strcpy(flagdir,gamedir);
				}
				ch_game_d();
				registered=TRUE;
				/*        if (fexist(KEY_FILENAME)) {
				strcpy(numstr,od_control.system_name);
				get_bbsname(numstr);
				registered=seereg(numstr);
				}*/
				ny_line(-1,0,0);
				CrashRecovery();
				od_exit(10,FALSE);
			} else if (strnicmp(argv[cnt],"-MM",3)==0) {
				od_control.od_force_local=TRUE;
				od_init();
				od_control.od_status_on=FALSE;
				od_set_statusline(STATUS_NONE);
				getcwd(gamedir,MAX_PATH);
				gamedisk=gamedir[0] - 'A';
				strzcpy(gamedir,gamedir,2,MAX_PATH);
				if(flagdisk==-1) {
					flagdisk=gamedisk;
					strcpy(flagdir,gamedir);
				}
				ch_game_d();
				registered=TRUE;
				/*      if (fexist(KEY_FILENAME)) {
					  strcpy(numstr,od_control.system_name);
					  get_bbsname(numstr);
					  registered=seereg(numstr);
					}*/
				ny_line(-1,0,0);
				forced_maint=TRUE;
				Maintanance();
				od_exit(10,FALSE);

			} else if (strnicmp(argv[cnt],"-M",2)==0) {
				od_control.od_force_local=TRUE;
				od_init();
				od_control.od_status_on=FALSE;
				od_set_statusline(STATUS_NONE);
				getcwd(gamedir,MAX_PATH);
				gamedisk=gamedir[0] - 'A';
				strzcpy(gamedir,gamedir,2,MAX_PATH);
				if(flagdisk==-1) {
					flagdisk=gamedisk;
					strcpy(flagdir,gamedir);
				}
				ch_game_d();
				registered=TRUE;
				/*      if (fexist(KEY_FILENAME)) {
					  strcpy(numstr,od_control.system_name);
					  get_bbsname(numstr);
					  registered=seereg(numstr);
					}*/
				ny_line(-1,0,0);
				Maintanance();
				od_exit(10,FALSE);
			} else if (strnicmp(argv[cnt],"-N",2)==0) {
				strzcpy(numstr,argv[cnt],2,59);
				sscanf(numstr,"%d",&intval);
				od_control.od_node=intval;
			} else if (strnicmp(argv[cnt],"-C",2)==0) {
				od_control.od_config_filename=argv[cnt]+2;
				od_control.od_config_file = INCLUDE_CONFIG_FILE;
			}
		} while ((++cnt)<argc);
	}

	nCurrentUserNumber=MAX_USERS+1;

	od_init();
	if(!od_control.od_force_local)
		od_control.od_clear_on_exit=FALSE;

	getcwd(gamedir,MAX_PATH);
	gamedisk=gamedir[0] - 'A';
	strzcpy(gamedir,gamedir,2,MAX_PATH);
	if(flagdisk==-1) {
		flagdisk=gamedisk;
		strcpy(flagdir,gamedir);
	}


	od_control.od_disable |= DIS_INFOFILE;

	if(no_slices==FALSE)
		od_control.od_ker_exec=time_slice;
	else
		od_control.od_ker_exec=NULL;

	if(od_control.user_screen_length<2)
		od_control.user_screen_length=24;

	od_control.od_help_text2=(char *)"  New York 2008 v1.01 (c) 1995-97 George Lebl   Alt-[F1]=NY2008 Status Bar    ";

	temp_rip_stor=od_control.user_rip;
	temp_ans_stor=od_control.user_ansi;
	temp_avt_stor=od_control.user_avatar;

	if(rip!=166) {
		if(autodetect==TRUE)
			od_autodetect(DETECT_NORMAL);

		rip=od_control.user_rip;
		if(rip) {
			temp_len_stor=od_control.user_screen_length;
			od_control.user_screen_length=35;
		}
	} else {
		rip=TRUE;
		od_control.user_rip=TRUE;
		temp_len_stor=od_control.user_screen_length;
		od_control.user_screen_length=35;
	}

	char *InComing;
	char *OutGoing;
	ibbs_mail_type ibmail;

	registered=TRUE;
	/*  if (fexist(KEY_FILENAME)) {
	    strcpy(numstr,od_control.system_name);
	    get_bbsname(numstr);
	    registered=seereg(numstr);
	  }*/

	if(!registered)
		ibbs=FALSE;

	if(ibbs==TRUE) {
		ch_game_d();
		INT16 xx;
		strcpy(IBBSInfo.szThisNodeAddress, "0:000/000");
		sprintf(IBBSInfo.szProgName, "#@NYG#%05d IBBS",ibbs_game_num);
		strcpy(IBBSInfo.szNetmailDir, "C:/FD/NETMAIL");
		IBBSInfo.bCrash = FALSE;
		IBBSInfo.bHold = FALSE;
		IBBSInfo.bEraseOnSend = TRUE;
		IBBSInfo.bEraseOnReceive = TRUE;
		IBBSInfo.nTotalSystems = 0;
		IBBSInfo.paOtherSystem = NULL;
		IBReadConfig(&IBBSInfo, "INTERBBS.CFG");
		IBReadConfig(&IBBSInfo, NODELIST_FILENAME);

		char szDirFileName[PATH_CHARS + 1];

		if(IBBSInfo.szNetmailDir==NULL || strlen(IBBSInfo.szNetmailDir)>PATH_CHARS) {
			od_printf("\n\r\nNETMAIL DIR NOT FOUND\n\r\n");
			od_exit(10,FALSE);
		}

		strcpy(szDirFileName, IBBSInfo.szNetmailDir);

		/* Remove any trailing backslash from directory name */
		if(szDirFileName[strlen(szDirFileName) - 1] == '\\')
			szDirFileName[strlen(szDirFileName) - 1] = '\0';


		/* Return true iff file exists and it is a directory */
		if(!fexist(szDirFileName) || !isdir(szDirFileName)) {
			od_printf("\n\r\nNETMAIL DIR NOT FOUND\n\r\n");
			od_exit(10,FALSE);
		}

		if(IBBSInfo.nTotalSystems>255)
			IBBSInfo.nTotalSystems=255;

		if(ibbs_i_mail) {
			ny_disp_emu("\n\r`0##> Getting InterBBS Mail\n\r");
			sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);

			justfile=ShareFileOpen(IBBS_MAIL_INDEX,"a+b");
			if(justfile != NULL) {
				while(IBGetMail(&IBBSInfo,&ibmail) == eSuccess) {

					if(strcmp(ibmail.node_r,IBBSInfo.szThisNodeAddress)==0)
						ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
				}
				fclose(justfile);
			}
		}
	}

	if(rip) {
		od_control.user_ansi=TRUE;
		od_disp_str("\n\r!|*|10000$SBAROFF$$HKEYOFF$|#|#|#\n\r");
		od_control.od_no_time=(char *)"\n\r!|10000((Sorry, you have used up all your time for today::@OK))|#|#|#\n\r";
		od_control.od_time_warning=(char *)"\n\r!|10000((Warning, only %d minute(s) remaining today!::@OK))|#|#|#\n\r";
		od_control.od_inactivity_warning=(char *)"\n\r!|10000((Warning, inactivity timeout in %d minute(s)!::@OK))|#|#|#\n\r";
		od_control.od_inactivity_timeout=(char *)"\n\r!|10000((Inactivity Timeout!::@OK))|#|#|#\n\r";
	}


	if(reset==TRUE) {
		od_control.od_status_on=FALSE;
		od_set_statusline(STATUS_NONE);
		od_clr_scr();
		ny_disp_emu("`@N`4ew `@Y`4ork `@2008 `%RESET `@U`4tility");
		ny_disp_emu("\n\n\r`2Not all options may aply to you. If you are not in an `0InterBBS`2 game");
		ny_disp_emu("\n\rand just wish to `0RESET`2 your game choose 1.");

		ny_disp_emu("\n\n\r`%1 `4- `@R`4eset `@T`4he `@G`4ame `@O`4nly `2(You will stay in an InterBBS game)");
		ny_disp_emu("\n\r`%2 `4- `@G`4et `@O`4ut `@O`4f `@InterBBS `2(The game will `0NOT `2be RESET)");
		ny_disp_emu("\n\r`%Q `4- `@Q`4uit `@T`4his `2(No RESETs)");

		ny_disp_emu("\n\n\r`@Y`4our `@C`4hoice: `0");
		key=od_get_answer("12Q");
		od_printf("%c",(char)key);
		if(key=='1') {
			ny_disp_emu("\n\n\r`@T`4he current game `%WILL `4be lost`%!");
			ny_disp_emu("\n\n\r`@A`4re you sure you want to `%RESET `4(`@Y`4/`@N`4):");
			key=od_get_answer("YN");

			od_printf("%c\n\n\r",(char)key);

			if(key=='Y') {
				ny_disp_emu("`%Ignore any error messages following this\n\n\r");
				ch_flag_d();

				ny_remove("u00?????.*");
				ny_remove("mnu*.dat");
				ny_remove("evt*.dat");
				ny_remove("n00?????.*");
				ny_remove(MAINTFLAG_FILENAME);

				ch_game_d();

				ny_remove(USER_FILENAME);
				ny_remove(SCR_FILENAME);
				ny_remove(LASTMAINT_FILENAME);
				ny_remove(GAMEDAY_FILENAME);
				ny_remove(MAIL_FILENAME);
				ny_remove(MAIL_INDEX);
				ny_remove(SENTBESTTEN_FILENAME);
				ny_remove(SENTLIST_FILENAME);
			}
		} else if(key=='2') {
			ny_disp_emu("\n\n\r`@Y`4ou will be taken out of this InterBBS game!");
			ny_disp_emu("\n\r`@M`4ake sure the operator bbs of this game has taken you out of the nodelist!");
			ny_disp_emu("\n\r`@A`4nd turn the InterBBS keyword off in your .cfg file!");
			ny_disp_emu("\n\n\r`@A`4re you sure you want to `%Get Local Only? `4(`@Y`4/`@N`4):");
			key=od_get_answer("YN");

			od_printf("%c\n\n\r",(char)key);

			if(key=='Y') {
				ny_disp_emu("`%Ignore any error messages following this\n\n\r");
				ch_game_d();

				ny_remove(NODELIST_FILENAME);
				ny_remove(SBYDB_PREFIX".*");
				ny_remove(IBBSSPY_FILENAME);
				ny_remove(IBBSSPY_TEMPFILENAME);
				ny_remove(IBBS_MAIL_INDEX);
				ny_remove(IBBS_BESTTEN_FILENAME);
				ny_remove(SBYDBT_PREFIX".*");
				ny_remove(SENTBESTTEN_FILENAME);
				ny_remove(SENTLIST_FILENAME);
			}
		}
		od_exit(10,FALSE);
	}

	registered=TRUE;
	/*  if (fexist(KEY_FILENAME)) {
	    strcpy(numstr,od_control.system_name);
	    get_bbsname(numstr);
	    registered=seereg(numstr);
	  }*/

	ny_line(-1,0,0);
	ny_stat_line(-1,0,0);

	//load bad wordlist
	if(fexist(BADWORDS_FILENAME)) {
		loadbadwords();
		dobad=TRUE;
	}

	//  char test1[]="shit man this fucking thing is fucking done !assfucked";
	//  dobadwords(test1);
	//  od_printf("\n\r\n%s\n\r\n",test1);
	//  WaitForEnter();

	ny_line(267,1,0);

	if (rip==FALSE) {
		ny_line(268,1,0);
		//(Xpert mode recomended for slow modems)
		ny_line(269,1,0);
		//Wanna start in Xpert, Regular or Novice mode (X/[R]/N)
		key=od_get_answer("XRN\n\r");
		if (key=='\n' || key=='\r')
			key='R';
		if (key=='N')
			expert=0;
		if (key=='R')
			expert=1;
		if (key=='X')
			expert=2;
		od_printf("%c\n\r\n\r",key);

		if(*regptr==TRUE && reg_check==784)
			od_exit(10,TRUE);


		if (expert<2) {
			ny_clr_scr();
			od_send_file("intro");
			ny_line(1,1,0);
			//Smack [ENTER] to go on.
			od_get_answer("\n\r");
		}
	} else {
		expert=0;
		od_printf("\n\r\n\r");
		ny_clr_scr();
		od_send_file("intro");
		ny_line(1,1,0);
		od_get_answer("\n\r");
	}
	if(rip)
		ny_clr_scr();
	ny_line(2,2,1);
	//Getting the menu text file pointers...
	ny_get_index();

	if(*regptr==TRUE && reg_check==784)
		od_exit(10,TRUE);


	if(ibbs)
		DisplayBestIB();
	else
		DisplayBest();

	if (registered==FALSE) {
		max_fights       =20;
		delete_after     =15;
		busted_ch_bank   =33;
		busted_ch_food   =33;
		busted_ch_rape   =33;
		busted_ch_beggar =20;
		busted_ch_car    =33;
		busted_ch_school =33;
		busted_ch_window =20;
		busted_ch_poison =25;
		busted_ch_bomb   =25;
		success_ch_bank  =50;
		success_ch_food  =50;
		success_ch_rape  =50;
		success_ch_beggar=50;
		success_ch_car   =65;
		success_ch_school=50;
		success_ch_window=95;
		success_ch_poison=33;
		success_ch_bomb  =33;
	}


	ch_flag_d();
	if (single_node==FALSE && fexist(MAINTFLAG_FILENAME)) {
		od_printf("\n\r\n\r");
		if(rip)
			ny_clr_scr();
		ny_send_menu(MAINT_RUN,"");
		/*    od_printf("\n\r\n\r`bright red`W`red`ell the mantanace is running on some other node so please come");
		    od_printf("back later, or\n\rif it is a one node bbs or the sysop's running the maintanance in the nightly\n\rbatch file ");
		    od_printf("then he should delete the MAINT.RUN flag file....");*/
		WaitForEnter();
		od_exit(10,FALSE);
	}
	//  if (single_node==TRUE && fexist(".RUN")) {


	ch_game_d();
	if (!fexist(USER_FILENAME)) { // player first in play run maint after he logs in
		if(!ReadOrAddCurrentUser()) {
			/* If unable to obtain a user record for the current user, then exit */
			/* the door after displaying an error message.                       */
			if(rip)
				ny_clr_scr();
			od_printf("Unable to access user file. File may be locked or full.\n\r");
			WaitForEnter();
			od_exit(1, FALSE);
		}

		//  strcpy(uname,od_control.user_name);
		//  sprintf(od_control.user_name,"%s (%s)",cur_user.name,uname);
		strcpy(rec.name,cur_user.name);
		rec.nation=cur_user.nation;
		rec.level=cur_user.level;
		rec.points=cur_user.points;
		rec.alive=cur_user.alive;
		rec.sex=cur_user.sex;
		rec.user_num=nCurrentUserNumber;
		rec.online=FALSE;

		ch_game_d();
		justfile=ShareFileOpen(SCR_FILENAME,"wb");
		if(justfile != NULL) {
			ny_fwrite(&rec, sizeof(scr_rec), 1, justfile);
			fclose(justfile);
		}

		date today;

		getdate(&today);

		ch_game_d();
		justfile=ShareFileOpen(LASTMAINT_FILENAME,"wb");
		if(justfile != NULL) {
			ny_fwrite(&today, sizeof(date), 1, justfile);
			fclose(justfile);
		}
		justfile=ShareFileOpen(GAMEDAY_FILENAME,"wb");
		if(justfile != NULL) {
			intval=1;
			ny_fwrite(&intval,2,1,justfile);
			fclose(justfile);
		}
	} else {
		/*Run Maintanance if it hasn't been run yet...*/
		if (do_maint==TRUE) {
			od_control.od_disable |= DIS_CARRIERDETECT;
			//      od_set_statusline(STATUS_NONE);
			Maintanance();
			//      od_set_statusline(STATUS_NORMAL);
			od_control.od_disable &=~ DIS_CARRIERDETECT;
			if(!ReadOrAddCurrentUser()) {
				/* If unable to obtain a user record for the current user, then exit */
				/* the door after displaying an error message.                       */
				if(rip)
					ny_clr_scr();
				od_printf("Unable to access user file. File may be locked or full.\n\r");
				WaitForEnter();
				od_exit(1, FALSE);
			}
		} else {
			if(!ReadOrAddCurrentUser()) {
				/* If unable to obtain a user record for the current user, then exit */
				/* the door after displaying an error message.                       */
				if(rip)
					ny_clr_scr();
				od_printf("Unable to access user file. File may be locked or full.\n\r");
				WaitForEnter();
				od_exit(1, FALSE);
			}
			ch_game_d();
			if (fexist(LASTMAINT_FILENAME)) {
				justfile = ShareFileOpen(LASTMAINT_FILENAME, "rb");
				if(justfile != NULL) {
					ny_fread(&lastday, sizeof(date), 1, justfile);
					fclose(justfile);
				}
				if (lastday.da_year!=today.da_year || lastday.da_mon!=today.da_mon || lastday.da_day!=today.da_day) {
					ch_flag_d();
					sprintf(numstr,"u%07d.tdp",nCurrentUserNumber);
					if(!fexist(numstr)) {
						cur_user.days_not_on++;

						getdate(&today);

						justfile=ShareFileOpen(numstr,"wb");
						if(justfile != NULL) {
							ny_fwrite(&today,sizeof(date),1,justfile);
							fclose(justfile);
						}
					} else {
						justfile=ShareFileOpen(numstr,"rb");
						if(justfile != NULL) {
							ny_fread(&lastday,sizeof(date),1,justfile);
							fclose(justfile);
						}
						getdate(&today);

						if (lastday.da_year!=today.da_year || lastday.da_mon!=today.da_mon || lastday.da_day!=today.da_day) {
							cur_user.days_not_on++;
							justfile=ShareFileOpen(numstr,"wb");
							if(justfile != NULL) {
								ny_fwrite(&today,sizeof(date),1,justfile);
								fclose(justfile);
							}
						}
					}
				}
			} else {
				ch_flag_d();
				sprintf(numstr,"u%07d.tdp",nCurrentUserNumber);
				if(!fexist(numstr)) {
					cur_user.days_not_on++;
					/*      justfile=ShareFileOpen(numstr,"w");
					 fclose(justfile);*/

					getdate(&today);

					justfile=ShareFileOpen(numstr,"wb");
					if(justfile != NULL) {
						ny_fwrite(&today,sizeof(date),1,justfile);
						fclose(justfile);
					}
				} else {
					justfile=ShareFileOpen(numstr,"rb");
					if(justfile != NULL) {
						ny_fread(&lastday,sizeof(date),1,justfile);
						fclose(justfile);
					}

					getdate(&today);

					if (lastday.da_year!=today.da_year || lastday.da_mon!=today.da_mon || lastday.da_day!=today.da_day) {
						cur_user.days_not_on++;
						justfile=ShareFileOpen(numstr,"wb");
						if(justfile != NULL) {
							ny_fwrite(&today,sizeof(date),1,justfile);
							fclose(justfile);
						}
					}
				}
			}
		}

		if(*regptr==TRUE && reg_check==784)
			od_exit(10,TRUE);


		if (single_node==FALSE) {
			ch_flag_d();
			sprintf(numstr, "u%07d.bfa",nCurrentUserNumber);
			if (fexist(numstr)) {
				justfile=ShareFileOpen(numstr,"rb");
				if(justfile != NULL) {
					ny_fread(&intval,2,1,justfile);
					fclose(justfile);
				}
				sprintf(numstr,"u%07d.on",intval);
				if (fexist(numstr)) {
					ny_line(402,2,1);
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					od_exit(10,FALSE);
				} else {
					sprintf(numstr,"u%07d.bfa",nCurrentUserNumber);
					ny_remove(numstr);
				}
			}
		}
	}

	//reset users flags
	ch_game_d();
	sprintf(numstr,"u%07d.inf",nCurrentUserNumber);
	if (fexist(numstr))
		ny_remove(numstr);
	sprintf(numstr,"n%07d.sts",od_control.od_node);
	if (fexist(numstr))
		ny_remove(numstr);
	sprintf(numstr,"n%07d.stt",od_control.od_node);
	if (fexist(numstr))
		ny_remove(numstr);


	od_control.od_before_exit=exit_ops;
	no_kernel=FALSE;

	strcpy(uname,od_control.user_name);
	sprintf(od_control.user_name,"%s (%s)",ny_un_emu(cur_user.name,numstr),uname);
	od_control.od_update_status_now=TRUE;
	od_kernal();

	//  od_set_statusline(STATUS_NONE);
	//  od_set_statusline(STATUS_NORMAL);


	do {
		switch(key=entry_menu())    // Get user's choice
		{
			case 'N':
				newz_ops();
				break;

			case 'E':          // If user hit E key

				if(*regptr==TRUE && reg_check==784)
					od_exit(10,TRUE);

				/*user wuz disabled today so come back tomorrow*/
				if (cur_user.days_not_on==0 && cur_user.alive==UNCONCIOUS) {
					ny_line(401,2,1);
					//        od_printf("\n\r\n\r`bright`You heffta wait until tomorrow to play again!\n\r");
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					od_exit(10,FALSE);
				}

				/*user died today*/
				if (cur_user.alive==DEAD) {
					ny_line(400,2,1);
					//        od_printf("\n\r\n\r`bright`Dead people are not allowed to play, come back tomorrow to start over!\n\r");
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					od_exit(10,FALSE);
				}

				/*user not played today so perform his daily functions*/
				if (cur_user.days_not_on>0) {
					cur_user.hitpoints=cur_user.maxhitpoints;
					cur_user.InterBBSMoves=2;
					cur_user.unhq=1;
					cur_user.poison=1;
					cur_user.drug_high=0;
					cur_user.turns=max_fights;
					if (cur_user.std_percent>4)
						cur_user.std_percent-=4;
					else {
						cur_user.std_percent=0;
						cur_user.std=NONE;
					}

					if(cur_user.std>AIDS)
						cur_user.std=AIDS;
					//        if(cur_user.std<NONE) cur_user.std=NONE;
					cur_user.hunger+=10;
					cur_user.sex_today=cur_user.level+1;
					cur_user.since_got_laid++;
					if (cur_user.hunger>100)
						cur_user.hunger=100;
					if (cur_user.drug_addiction>10) {
						cur_user.drug_addiction-=10;
						cur_user.drug_days_since++;
					} else {
						cur_user.drug_addiction=0;
						cur_user.drug_days_since=0;
					}

					//        od_printf("debug5");
					//cur_user.std_percent-=10;
					//if (cur_user.std_percent<=0) cur_user.std_percent=0;
					/*user wuz beaten  last time, get him back on street*/
					if (cur_user.alive==UNCONCIOUS) {

						//          od_printf("\n\r\n\r");
						//          ny_send_menu(WAKE_UP,"");

						//           od_printf("debug6");

						ny_line(403,1,1);
						//          od_printf("\n\r\n\r`bright red`Y`red`er feeling better as you wake up from a comma...\n\r\n\r");
						cur_user.alive=ALIVE;
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else if (cur_user.days_not_on>0) { //user gets 10 points for not getting his ass kicked
						ny_line(398,1,1);
						//          od_printf("\n\r\n\r`bright red`Y`red`ou get 10 points for not having yer ass kicked...\n\r\n\r");
						points_raise((DWORD)10);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");

					}

				}

				/*reset days not on variable....so user won't get deleted*/
				cur_user.days_not_on=0;

				if(*regptr==TRUE && reg_check==784)
					od_exit(10,TRUE);

				if (single_node==FALSE) {
					/*create status file for user*/
					ch_flag_d();
					sprintf(numstr,"u%07d.sts",nCurrentUserNumber);
					justfile = ShareFileOpen(numstr, "wb");
					if(justfile != NULL) {
						ny_fwrite(&cur_user,sizeof(user_rec),1,justfile);
						fclose(justfile);
					}

					/*send message to all online users*/
					if (glob("u???????.on",0,NULL,&ff)==0) {
						for(fname=ff.gl_pathv;*fname != NULL;fname++) {
							strcpy(numstr,*fname);
							numstr[10]='m';
							numstr[11]='g';
							numstr[12]=0;
							justfile=ShareFileOpen(numstr,"a+b");
							numstr[0]=27;
							numstr[1]=10;
							if(justfile != NULL) {
								ny_fwrite(&numstr,51,1,justfile);
								ny_fwrite(&cur_user.name,25,1,justfile);
								fclose(justfile);
							}
						}
						globfree(&ff);
					}
					/*User is now on-line!*/
					sprintf(numstr,"u%07d.on",nCurrentUserNumber);
					justfile = ShareFileOpen(numstr, "a+b");
					if(justfile != NULL)
						fclose(justfile);

				}/* else {
					  justfile = ShareFileOpen("INUSE.FLG", "a+b");
					  fclose(justfile);
					}  */

				/*enable online flag in the scr file*/
				//      ch_game_d();
				points_raise(0);
				/*justfile = ShareFileOpen(SCR_FILENAME,"r+b");
				fseek(justfile, (INT32)cur_user.rank * sizeof(scr_rec),SEEK_SET);
				ny_fread(&srec,sizeof(scr_rec),1,justfile);
				srec.online=TRUE;
				fseek(justfile, (INT32)cur_user.rank * sizeof(scr_rec),SEEK_SET);
				ny_fwrite(&srec,sizeof(scr_rec),1,justfile);
				fclose(justfile);*/

				/*reset days not on variable....so user won't get deleted*/
				cur_user.days_not_on=0;

				/*store cur parms.....*/
				wrt_sts();

				oneframe=TRUE;
				/*display users stats*/
				if(rip) {
					od_disp_str("\n\r");
					od_send_file("frame.rip");
					od_send_file("frame1.rip");
				}
				DisplayStats();
				WaitForEnter();

				/*display who's online*/
				WhosOnline();

				/*display newzfiles*/
				newz_ops();

				oneframe=FALSE;

				if(reg_check==784 && *regptr==TRUE)
					registered=FALSE;

				/*mail read....*/
				//       od_printf("\n\r\n\r`bright red`R`red`ead yer mail ...(`bright red`[Y]`red`/`bright red`N`red`)");
				//        key=ny_get_answer("YN\n\r");
				//      if (key=='\n' || key=='\r') key='Y';
				//      od_printf("%c\n\r",key);
				//      if (key=='Y')

				ny_line(3,2,1);
				//      printf("\n\r1\n\r");
				//      od_printf("\n\r\n\r`bright red`R`red`eading mail...\n\r");
				read_mail();
				if(ibbs)
					read_ibmail();

				//      printf("\n\r13\n\r");

				/*here goes the streets of ny*/
				EnterStreet();

				/*quit game after returning from there*/
				key='Q';
				break;

			case 'L':
				ListPlayers();
				break;
			case 'Y':
				DisplayStats();
				WaitForEnter();
				break;
		}                            // Loop until quit to BBS
	} while(key!='Q');

	od_exit(10,FALSE);              // Again OpenDoors does the rest

	return 0;
}


void
UserStatus(void) {
	od_set_personality("NY2008");
}

void
set_wildcat(void) {
	od_set_personality("Wildcat");
}

void
ny_pers(unsigned char message) {
#ifndef ODPLAT_NIX
	INT16 x,y;
	char numstr[25];


	if(message==PEROP_INITIALIZE) {
		//    od_set_personality("Standard");
		od_control.key_status[0]=15104;
		od_control.key_status[1]=15360;
		od_control.key_status[2]=15616;
		od_control.key_status[3]=15872;
		od_control.key_status[4]=16128;
		od_control.key_status[5]=16384;
		od_control.key_status[6]=17152;
		od_control.key_status[7]=17408;
		od_control.od_hot_key[0]=16640;
		od_control.od_hot_function[0]=set_wildcat;
		od_control.key_chat=11776;
		od_control.key_dosshell=9216;
		od_control.key_drop2bbs=8192;
		od_control.key_hangup=8960;
		od_control.key_keyboardoff=9472;
		od_control.key_lesstime=20480;
		od_control.key_lockout=9728;
		od_control.key_moretime=18432;
		od_control.key_sysopnext=12544;
	} else if(message==PEROP_DEINITIALIZE) {
		od_control.od_hot_key[0]=26624;
		od_control.od_hot_function[0]=UserStatus;
	} else if(message==PEROP_DISPLAY1 || message==PEROP_UPDATE1) {
		x=wherex();
		y=wherey();
		textbackground(BLUE);

		textcolor(LIGHTRED);

		gotoxy(1,24);
		clreol();
		ny_un_emu(cur_user.name,numstr);
		cprintf("  %-46s",numstr);
		textcolor(WHITE);
		cprintf(" New York 2008 ");

		textcolor(LIGHTBLUE);
		cprintf(" by Franz  ");

		textcolor(BLACK);
		cprintf("%s\n\r",ver);

		clreol();
		textcolor(LIGHTBLUE);
		cprintf("                [F2]=Player Status [F3]=Personalities [F9]=Help");
		gotoxy(x,y);

	} else if(message==PEROP_DISPLAY2 || message==PEROP_UPDATE2) {
		x=wherex();
		y=wherey();
		textbackground(BLUE);
		textcolor(WHITE);

		gotoxy(1,24);
		clreol();
		ny_un_emu(cur_user.name,numstr);
		textcolor(LIGHTRED);
		cprintf(" %-25s ",numstr);
		textcolor(WHITE);
		cprintf("Level: ");
		textcolor(LIGHTRED);
		cprintf("%-2d   ",(INT16)cur_user.level);

		textcolor(LIGHTBLUE);
		switch(cur_user.nation) {
			case HEADBANGER:
				cprintf("Headbanger     ");
				break;
			case HIPPIE:
				cprintf("Hippie         ");
				break;
			case BIG_FAT_DUDE:
				cprintf("Big Fat Dude   ");
				break;
			case CRACK_ADDICT:
				cprintf("Crack Addict   ");
				break;
			case PUNK:
				cprintf("Punk           ");
				break;
		}

		textcolor(WHITE);
		cprintf("Sex: ");
		textcolor(LIGHTRED);
		if(cur_user.sex==MALE)
			cprintf("Male");
		else
			cprintf("Female");

		cprintf("\n\r");

		clreol();
		textcolor(WHITE);
		cprintf(" Points: ");
		textcolor(LIGHTRED);
		cprintf("%-16s  ",D_Num(cur_user.points));
		textcolor(WHITE);
		cprintf("Money: ");
		textcolor(LIGHTRED);
		cprintf("%-16s",D_Num(cur_user.money));

		gotoxy(x,y);
	} else if(message==PEROP_DISPLAY3 || message==PEROP_UPDATE3) {
		x=wherex();
		y=wherey();
		textbackground(BLUE);
		textcolor(WHITE);

		gotoxy(1,24);
		clreol();
		cprintf("  These are the personalities NY2008 supports:\n\r");

		clreol();
		textcolor(LIGHTBLUE);
		cprintf("  [F4]=Opendoors Standard [F5]=PCBoard [F6]=Remote Access [F7]=Wildcat ");
		gotoxy(x,y);
	} else if(message==PEROP_DISPLAY4) {
		od_set_personality("Standard");
	} else if(message==PEROP_DISPLAY5) {
		od_set_personality("PCBoard");
	} else if(message==PEROP_DISPLAY6) {
		od_set_personality("RemoteAccess");
	} else if(message==PEROP_DISPLAY7 || message==PEROP_UPDATE7) {
		x=wherex();
		y=wherey();
		textbackground(BLUE);
		textcolor(WHITE);

		gotoxy(1,24);
		clreol();
		cprintf("%s",od_control.od_help_text);

		clreol();
		textcolor(LIGHTBLUE);
		cprintf("  [F1]=Main Status Bar [F2]=User Stats [F3]=Personalities");

		gotoxy(x,y);
	}
#endif
}

void
dump(void) {
	char key;
	do {
		scanf("%c",&key);
	} while (key!='\n');
}


void
get_bbsname(char bbsname[]) {
	INT16 cnt=0;
	char out[36];
	INT16 cnto=0;

	while (bbsname[cnt]!=0 && cnto<35) {
		if (bbsname[cnt]>='a' && bbsname[cnt]<='z')
			bbsname[cnt]-=32;

		if (bbsname[cnt]>='A' && bbsname[cnt]<='Z') {
			out[cnto]=bbsname[cnt];
			cnto++;
		}

		cnt++;
	}
	out[cnto]=0;
	strcpy(bbsname,out);
}



INT16
seereg(char bbsname[]) {
	return TRUE;
	/*  FILE *justfile;
	  char string4[]="PICATAKY";
	  char kod[26];
	  char string[26] = "ABECEDA";
	  INT16 intval;
	  INT16 cnt;
	  INT16 bbsc;
	  INT16 which;
	  INT16 temp,temp2;
	  INT16 ass=0;
	  char string3[]="HLEDA";
	  char string2[]="KURVA";
	 
	  ass++;
	 
	  strcat(string,string2);
	 
	  ass++;
	 
	  ch_game_d();
	 
	  ass++;
	 
	  justfile=ShareFileOpen(KEY_FILENAME,"rb");
	 
	  ass++;
	 
	  ny_fread(kod,26,1,justfile);
	 
	  if(string[6]=='$') {
	    printf("hehe");
	    od_exit(15,TRUE);
	  }
	 
	  ass++;
	 
	  fclose(justfile);
	 
	  ass++;
	 
	  strcat(string,string3);
	 
	  string3[0]='j';
	 
	  ass++;
	 
	  string2[2]='#';
	 
	  sscanf(kod,"%02d",&intval);
	 
	  ass++;
	 
	  strcat(string,string4);
	 
	 
	  if (intval!=strlen(bbsname)) {
	    string[5]=0;
	    ass=0;
	    string[2]='%';
	    string2[2]='k';
	    string3[4]=3;
	    ass=string[8]+string2[3];
	    ass=string[5] * string[3] * string[2];
	    goto ending;
	  }
	 
	  ass++;
	 
	  intval = kod[0] - '0';
	 
	  ass++;
	 
	  bbsc=0;
	 
	  ass++;
	 
	  which=0;
	 
	  ass++;
	 
	  for (cnt=2;cnt<25;cnt++) {
	    if (bbsname[bbsc]==0) bbsc=0;
	 
	    if(string[4]=='(')
	      string[4]='5';
	 
	 
	    temp2=string[cnt]+intval+bbsname[bbsc];
	 
	//    if (kod[cnt]>'Z') kod[cnt]-=('Z'-'A');
	    temp =  (temp2-'A')/('Z'-'A');
	 
	    temp2 = temp2 - (('Z'-'A') * temp);
	 
	    if(ass!=12) {
	      ass=-8;
	      string[333]=0;
	      string2[333]=0;
	      ass=0;
	      goto ending;
	    }
	 
	    if (kod[cnt]!=temp2) {
	      ass=0;
	      string2[2]=2;
	      string3[2]=2;
	      goto ending;
	    }
	 
	    if (which==0)
	      which =1;
	    else
	      which =0;
	 
	    if(string2[4]=='(')
	      string2[4]='5';
	 
	    intval = kod[which] - '0';
	    bbsc++;
	  }
	 
	  ass++;
	 
	  ending:
	 
	  if (ass==13)
	    ass=TRUE;
	 
	  if(ass==0)
	    reg_check=784;
	 
	  return ass;*/
}






/*copy end chars  beginning from beg to dest*/
/*similiar to strncpy*/
void
strzcpy(char dest[],const char src[], INT16 beg,INT16 end) {
	INT16 cnt=0;
	do
		dest[cnt++]=src[beg++];
	while (cnt<=end && src[cnt]!=0);
	dest[cnt]='\0';
}





void
ny_kernel(void) {
	if(no_kernel || single_node)
		return;

	od_kernal();

	no_kernel=TRUE;

	char c_dir;
	//  ffblk ff;


	c_dir=c_dir_g;

	// if (single_node==FALSE) {
	ch_flag_d();

	//  bdosptr(0x4E,(void *)&ff,0);
	//fexist(MAINTFLAG_FILENAME)
	if (fexist(MAINTFLAG_FILENAME)) {
		FILE *justfile;
		static char numstr[35],numstr2[14];
		INT16 intval,
		INT16used,
		battled_user = -1;

		//if(0) {
		/* This is terrible - ToDo */
#ifndef ODPLAT_NIX

		fcloseall();
#endif

		scr_save();
		od_control.od_disable |= DIS_CARRIERDETECT;
		ny_line(4,2,0);
		//Please wait while maintanance runs on another node...

		while (fexist(MAINTFLAG_FILENAME)) {
			od_kernal();
			od_sleep(1);
		}

		ny_line(5,1,1);
		//    od_printf("\n\rThanks For Waiting....\n\r");

		cur_user.days_not_on=1;
		if (cur_user.rest_where!=NOWHERE) {
			if (cur_user.hotel_paid_fer>0)
				cur_user.hotel_paid_fer--;
			if (cur_user.hotel_paid_fer==0) {
				ny_line(6,1,1);
				//      od_printf("\n\r`bright`You were kicked out of the hotel\n\r");
				cur_user.rest_where=NOWHERE;
			}
		}

		sprintf(numstr,"u%07d.bfo",nCurrentUserNumber);
		if (fexist(numstr)) {
			justfile=ShareFileOpen(numstr,"rb");
			if(justfile != NULL) {
				ny_fread(&battled_user,2,1,justfile);
				fclose(justfile);
			}
			ny_remove(numstr);
		}

		sprintf(numstr,"u%07d.cng",nCurrentUserNumber);
		if (fexist(numstr)) {
			justfile=ShareFileOpen(numstr,"rb");
			if(justfile != NULL) {
				ny_fread(&intval,2,1,justfile);
				fclose(justfile);
			}
			sprintf(numstr,"u%07d.fgg",nCurrentUserNumber);
			if (fexist(numstr)) {

				sprintf(numstr,"u%07d.fgg",intval);
				justfile = ShareFileOpen(numstr,"wb");
				if(justfile != NULL)
					fclose(justfile);
				sprintf(numstr,"u%07d.atk",nCurrentUserNumber);
				sprintf(numstr2,"u%07d.atk",intval);
				rename(numstr,numstr2);
			}

			sprintf(numstr,"u%07d.chl",nCurrentUserNumber);
			sprintf(numstr2,"u%07d.chl",intval);
			rename(numstr,numstr2);

			/*clean up*/
			sprintf(numstr,"u%07d.*",nCurrentUserNumber);
			ny_remove(numstr);

			sprintf(numstr,"u%07d.fgc",nCurrentUserNumber);
			justfile = ShareFileOpen(numstr, "wb");
			if(justfile != NULL) {
				ny_fwrite(&intval,2,1,justfile);
				fclose(justfile);
			}

			nCurrentUserNumber=intval;

			/* tady se musi zmenit vsechnt flagy......*/
			sprintf(numstr,"u%07d.sts",nCurrentUserNumber);
			justfile = ShareFileOpen(numstr, "wb");
			if(justfile != NULL) {
				ny_fwrite(&cur_user,sizeof(user_rec),1,justfile);
				fclose(justfile);
			}

			sprintf(numstr,"u%07d.on",nCurrentUserNumber);
			justfile = ShareFileOpen(numstr, "a+b");
			if(justfile != NULL)
				fclose(justfile);

		}
		if(battled_user>=0) {
			sprintf(numstr,"u%07d.bfa",battled_user);
			justfile=ShareFileOpen(numstr,"wb");
			if(justfile != NULL) {
				ny_fwrite(&nCurrentUserNumber,2,1,justfile);
				fclose(justfile);
			}
			sprintf(numstr,"u%07d.bfo",nCurrentUserNumber);
			justfile=ShareFileOpen(numstr,"wb");
			if(justfile != NULL) {
				ny_fwrite(&battled_user,2,1,justfile);
				fclose(justfile);
			}
		}

		// user is in a battle with an offline user
		//    wrt_sts();
		od_control.od_disable &=~ DIS_CARRIERDETECT;
		sprintf(numstr,"u%07d.rnk",nCurrentUserNumber);
		if (fexist(numstr)) {
			justfile=ShareFileOpen(numstr,"rb");
			if(justfile != NULL) {
				ny_fread(&cur_user.rank,2,1,justfile);
				fclose(justfile);
			}
			ny_remove(numstr);
			//       sprintf(numstr,"del u%07d.rnk",nCurrentUserNumber);
			//       system(numstr);
			//      wrt_sts();
		}
		if(no_wrt_sts==FALSE) {
			//      intval=no_kernel;
			//      no_kernel=FALSE;
			wrt_sts();
			//      no_kernel=intval;
		}

		scr_res();
	}

	if(c_dir==0)
		ch_game_d();

	no_kernel=FALSE;
}


void    //the lower kernel .... can use inputs and only done when
fig_ker(void) //waiting for user not in middle of calcs or whatever
{             //used for fights and messages and online sex
	od_kernal();

	if(no_kernel || single_node)
		return;

	char c_dir;

	char numstr[21],omg[51],nam[25],type = 0;
	FILE *justfile;
	INT16 intval;

	ny_kernel();

	no_kernel=TRUE;
	c_dir=c_dir_g;

	//if (single_node==FALSE) {
	//if u get an online message
	ch_flag_d();
	sprintf(numstr,"u%07d.omg",nCurrentUserNumber);
	if (fexist(numstr)) {

		if(!rip)
			scr_save(); //try to store the screen
		//if(rip) ny_clr_scr();
		if(!rip)
			ny_line(7,2,1);
		//    od_printf("\n\r\n\r`bright`You get a message!\n\r");
		justfile=ShareFileOpen(numstr,"rb");
		if(justfile != NULL) {
			do {
				intval=ny_fread(&omg,51,1,justfile);
				ny_fread(&nam,25,1,justfile);

				if(omg[0]==27 && intval==1 ) {
					if(rip) {
						ny_line(7,2,1);
						od_get_answer("\n\r");
					}
					if(omg[1]<10) {
						type=1;

						if(!rip) {
							ny_disp_emu("`0");
							ny_disp_emu(nam);
						} else {
							ny_un_emu(nam);
							od_printf("\n\r!|10000((%s",nam);
						}

						ny_line(357 + omg[1],0,1);
						if(rip)
							od_get_answer("\n\r");


					} else {

						type=2;
						if(!rip) {
							ny_disp_emu("`0");
							ny_disp_emu(nam);
						} else {
							ny_un_emu(nam);
							od_printf("\n\r!|10000((%s",nam);
						}
						ny_line(360 + omg[1]-10,0,1);
						if(rip)
							od_get_answer("\n\r");

					}
				} else {

					if (intval==1) {
						if(rip) {
							scr_save();
							od_disp_str("\n\r");
							od_send_file("texti.rip");
							ny_disp_emu("`%You get a message!\n\r");
						}

						ny_disp_emu("`0");
						ny_disp_emu(omg);
						if (nam[0]!=0) {
							ny_line(8,0,0);
							//          ny_disp_emu("   `9F`1rom: `@");
							ny_disp_emu(nam);
							od_printf("\n\r");
						}
					}

				}
			} while (intval==1);
			fclose(justfile);
		}
		ny_remove(numstr);
		//sprintf(numstr,"del u%07d.omg",nCurrentUserNumber);
		//system(numstr);

		if(type==1) {
			ny_line(359,0,0);
			type=od_get_answer("YN\n\r");
			if(type=='\n' || type=='\r')
				type='Y';
			if(!rip)
				od_printf("%c\n\r",type);
			else
				od_disp_str("\n\r");
			if(type=='Y')
				read_mail();
			ch_flag_d();
		} else if(rip==FALSE || type==0) {
			ny_line(1,1,0);
			od_kernal();
			od_get_answer("\n\r");
		}

		if(rip==FALSE || type==0)
			scr_res(); //try to restore the screen
	}

	ch_flag_d();
	sprintf(numstr,"u%07d.chl",nCurrentUserNumber);
	if (fexist(numstr)) {
		justfile=ShareFileOpen(numstr,"rb");
		if(justfile != NULL) {
			ny_fread(&intval,2,1,justfile);
			fclose(justfile);
		}
		scr_save(); //try to store the screen
		online_fight(&nCurrentUserNumber,&cur_user,intval);
		scr_res(); //try to restore the screen
	}

	if(c_dir==0)
		ch_game_d();
	no_kernel=FALSE;
}



INT32
randomf(INT32 max) {
	INT32 fin,
	ovrflw;

	//  randomize();

	fin=xp_random(32767) * xp_random(32767);

	if (fin<0)
		fin *= -1;

	ovrflw=fin/max;

	fin=fin-(ovrflw*max);

	return fin;
}


void //write current user stats to temp file
wrt_sts(void) {
	char  numstr[20];
	FILE *justfile;
	FILE *scr_file;
	scr_rec rec;
	char c_dir;
	//  ffblk ffblk;

	c_dir=c_dir_g;

	if(c_dir==1)
		ch_game_d();

	no_wrt_sts=TRUE;
	ny_kernel();
	no_wrt_sts=FALSE;

	od_control.od_disable |= DIS_CARRIERDETECT;

	if(cur_user.std>AIDS)
		cur_user.std=AIDS;
	//  if(cur_user.std<NONE) cur_user.std=NONE;


	if(cur_user.alive==UNCONCIOUS)
		cur_user.days_in_hospital=0;

	if (single_node==FALSE) {
		ch_flag_d();
		sprintf(numstr,"u%07d.rnk",nCurrentUserNumber);
		if (fexist(numstr)) {
			justfile=ShareFileOpen(numstr,"rb");
			if(justfile != NULL) {
				ny_fread(&cur_user.rank,2,1,justfile);
				fclose(justfile);
			}
			ny_remove(numstr);
			//      sprintf(numstr,"del u%07d.rnk",nCurrentUserNumber);
			//      system(numstr);
		}
		sprintf(numstr,"u%07d.sts",nCurrentUserNumber);
		justfile = ShareFileOpen(numstr, "w+b");
		if(justfile != NULL) {
			ny_fwrite(&cur_user,sizeof(user_rec),1,justfile);
			fclose(justfile);
		}
	} else {
		ch_game_d();
		WriteCurrentUser();
		//    justfile = fopen(USER_FILENAME, "r+b");
		//    fseek(justfile,nCurrentUserNumber * sizeof(user_rec),SEEK_SET);
		//    fwrite(&cur_user,sizeof(user_rec),1,justfile);
		//    fclose(justfile);
	}
	strcpy(rec.name,cur_user.name);
	rec.nation=cur_user.nation;
	rec.level=cur_user.level;
	rec.points=cur_user.points;
	rec.alive=cur_user.alive;
	rec.sex=cur_user.sex;
	rec.user_num=nCurrentUserNumber;
	rec.online=TRUE;

	ch_game_d();
	scr_file=ShareFileOpen(SCR_FILENAME,"r+b");
	if(justfile != NULL) {
		fseek(scr_file, (INT32)cur_user.rank * sizeof(scr_rec), SEEK_SET);
		ny_fwrite(&rec, sizeof(scr_rec), 1, scr_file);
		fclose(scr_file);
	}

	od_control.od_disable &=~ DIS_CARRIERDETECT;
	if(c_dir==1)
		ch_flag_d();
	else
		ch_game_d();
}




void
exit_ops(void) {
	FILE* justfile;
	char numstr[20];
	scr_rec srec;
	//      ffblk ffblk;
	INT16 intval;

	wrt_sts();

	if(rip) {
		ny_clr_scr();
		od_disp_str("\n\r!|10000$SBARON$$HKEYON$|#|#|#\n\r");
		od_control.user_screen_length=temp_len_stor;
		od_control.user_rip=temp_rip_stor;
		od_control.user_ansi=temp_avt_stor;
		od_control.user_avatar=temp_ans_stor;
	}

	if (cur_user.hunger>=100) { //user died of hunger today
		cur_user.hunger=0;
		Die(2);
	}


	/*Print return message*/

	if (registered) {
		ny_disp_emu("\n\r\n\r`@R`4eturning you to `@");
		ny_disp_emu(od_control.system_name);
		ny_disp_emu("`4...");
	} else {
		ny_disp_emu("\n\r\n\r`@R`4eturning you to `%UNREGISTERED BBS`4...\n\r");
	}

	//od_set_statusline(STATUS_NONE);

	strcpy(od_control.user_name,uname);

	/*user offline, delete temp files and flags*/
	ch_flag_d();
	sprintf(numstr,"mnu%d.dat",od_control.od_node);
	ny_remove(numstr);
	sprintf(numstr,"fev%d.dat",od_control.od_node);
	ny_remove(numstr);

	sprintf(numstr,"u%07d.bfo",nCurrentUserNumber);
	if (fexist(numstr)) {
		justfile=ShareFileOpen(numstr,"rb");
		if(justfile != NULL) {
			ny_fread(&intval,2,1,justfile);
			fclose(justfile);
		}
		sprintf(numstr,"u%07d.bfa",intval);
		ny_remove(numstr);
	}

	sprintf(numstr,"u%07d.swp",nCurrentUserNumber);
	if (fexist(numstr)) {
		justfile=ShareFileOpen(numstr,"rb");
		if(justfile != NULL) {
			ny_fread(&cur_user.arm,2,1,justfile);
			fclose(justfile);
		}
		ny_remove(numstr);
	}

	sprintf(numstr,"u%07d.*",nCurrentUserNumber);
	if (fexist(numstr)) {
		//  sprintf(numstr,"u%07d.*",nCurrentUserNumber);
		ny_remove(numstr);
	}

	/*write stats to usr file*/
	WriteCurrentUser();


	if(single_node==FALSE) {
		ch_flag_d();
		/*send message to all online users*/
		if (glob("u???????.on",0,NULL,&ff)==0) {
			for(fname=ff.gl_pathv;*fname != NULL;fname++) {
				strcpy(numstr,*fname);
				numstr[10]='m';
				numstr[11]='g';
				numstr[12]=0;
				justfile=ShareFileOpen(numstr,"a+b");
				numstr[0]=27;
				numstr[1]=11;
				if(justfile != NULL) {
					ny_fwrite(&numstr,51,1,justfile);
					ny_fwrite(&cur_user.name,25,1,justfile);
					fclose(justfile);
				}
			}
			globfree(&ff);
		}
	}
	strcpy(srec.name,cur_user.name);
	srec.nation=cur_user.nation;
	srec.level=cur_user.level;
	srec.points=cur_user.points;
	srec.alive=cur_user.alive;
	srec.sex=cur_user.sex;
	srec.user_num=nCurrentUserNumber;
	srec.online=FALSE;

	/*disable online flag in the scr file*/
	ch_game_d();
	justfile = ShareFileOpen(SCR_FILENAME,"r+b");
	if(justfile != NULL) {
		fseek(justfile, (INT32)cur_user.rank * sizeof(scr_rec),SEEK_SET);
		ny_fwrite(&srec,sizeof(scr_rec),1,justfile);
		fclose(justfile);
	}

	/*create score files*/
	if (do_scr_files==TRUE) {
		ny_line(9,2,0);
		MakeFiles();
		ny_line(10,0,1);
	}

	for(INT16 i=0;i<badwordscnt;i++)
		free(badwords[i]);
}



void
Maintanance(void) {
	FILE *fpUserFile;
	FILE *scr_file;
	FILE *justfile;
	FILE *njustfile;
	FILE *delfile;
	char numstr[45],numstr2[14];
	scr_rec scr_user;
	user_rec urec;
	INT16 intval;
	INT16 user_num=0;
	INT16 user_num_w=0;
	date    today,
	lastday;
	//      struct ffblk ffblk;


	if(registered==FALSE)
		bank_interest=0;

	if(rip)
		ny_clr_scr();

	ny_line(11,2,1);
	//  od_printf("\n\r\n\r`cyan`### Checking for maintanance running\n\r");

	ch_flag_d();
	if (!fexist(MAINTFLAG_FILENAME)) {

		no_kernel=TRUE;


		if(forced_maint==FALSE) {
			ny_line(12,0,1);
			//        od_printf("### Checking for last maintanance\n\r");

			getdate(&today);

			ch_game_d();
			if (fexist(LASTMAINT_FILENAME)) {
				fpUserFile = ShareFileOpen(LASTMAINT_FILENAME, "r+b");
				if(justfile != NULL) {
					ny_fread(&lastday, sizeof(date), 1, fpUserFile);
					if (lastday.da_year==today.da_year && lastday.da_mon==today.da_mon && lastday.da_day==today.da_day) {
						fclose(fpUserFile);
						return;
					}

					/*        if (do_maint==FALSE) {
					    	sprintf(numstr,"u%07d.nmt",nCurrentUserNumber);
					    	if(!fexist(numstr))
					    	  cur_user.days_not_on++;
					    	fclose(fpUserFile);
					    	return;
						  }*/
					ch_flag_d();
					justfile = ShareFileOpen(MAINTFLAG_FILENAME, "a+b");
					if(justfile != NULL)
						fclose(justfile);
					fseek(fpUserFile, (INT32)0, SEEK_SET);
					ny_fwrite(&today, sizeof(date), 1, fpUserFile);
					fclose(fpUserFile);
				}
			} else {
				/*        if (do_maint==FALSE) {
					    cur_user.days_not_on+=1;
					    return;
					  }*/
				ch_flag_d();
				justfile = ShareFileOpen(MAINTFLAG_FILENAME, "wb");
				if(justfile != NULL)
					fclose(justfile);
				ch_game_d();
				fpUserFile = ShareFileOpen(LASTMAINT_FILENAME, "wb");
				if(fpUserFile != NULL) {
					ny_fwrite(&today, sizeof(date), 1, fpUserFile);
					fclose(fpUserFile);
				}
			}
		} else {
			ch_flag_d();
			justfile = ShareFileOpen(MAINTFLAG_FILENAME, "a+b");
			if(justfile != NULL)
				fclose(justfile);
			getdate(&today);
			ch_game_d();
			fpUserFile = ShareFileOpen(LASTMAINT_FILENAME, "wb");
			if(fpUserFile != NULL) {
				ny_fwrite(&today, sizeof(date), 1, fpUserFile);
				fclose(fpUserFile);
			}
		}
		ch_game_d();
		if (fexist(GAMEDAY_FILENAME)) {
			justfile=ShareFileOpen(GAMEDAY_FILENAME,"r+b");
			if(justfile != NULL) {
				ny_fread(&intval,2,1,justfile);
				intval++;
				fseek(justfile,0,SEEK_SET);
				ny_fwrite(&intval,2,1,justfile);
				fclose(justfile);
			}
		} else {
			ch_game_d();
			justfile=ShareFileOpen(GAMEDAY_FILENAME,"wb");
			if(justfile != NULL) {
				intval=1;
				ny_fwrite(&intval,2,1,justfile);
				fclose(justfile);
			}
		}

		ny_line(13,0,1);
		//      od_printf("### Running daily maintance\n\r");
		//get rid of all the played today files ....
		ch_flag_d();
		if(fexist("*.tdp"))
			ny_remove("*.tdp");

		ch_game_d();
		if (fexist(YESNEWS_FILENAME))
			ny_remove(YESNEWS_FILENAME);
		if (fexist(TODNEWS_FILENAME))
			copyfile(TODNEWS_FILENAME,YESNEWS_FILENAME);
		//        ny_remove(TODNEWS_FILENAME);
		//    }
		fpUserFile = ShareFileOpen(TODNEWS_FILENAME, "w+b");
		if(fpUserFile != NULL)
			fclose(fpUserFile);
		fpUserFile = ShareFileOpen(YESNEWS_FILENAME, "a+b");
		if(fpUserFile != NULL)
			fclose(fpUserFile);


		/* Begin with the current user record number set to 0. */
		user_num = 0;
		user_num_w = 0;

		/*delete all change files cuz they're not needed and will mess up*/
		ch_flag_d();
		if (fexist("*.cng"))
			ny_remove("*.cng");

		/*if fast_mail_index founddelete it*/
		//      if (fexist("fastmail.idx")) ny_remove("fastmail.idx");

		/*delete scores index file*/
		ch_game_d();
		//  if (fexist(SCR_FILENAME)) ny_remove(SCR_FILENAME);

		//      if (fexist(USER_BACK_FILENAME)) ny_remove(USER_BACK_FILENAME);
		copyfile(USER_FILENAME,USER_BACK_FILENAME);
		//    ny_remove(USER_FILENAME);
		scr_file = ShareFileOpen(SCR_FILENAME, "wb");
		justfile = ShareFileOpen(USER_BACK_FILENAME, "rb");
		fpUserFile = ShareFileOpen(USER_FILENAME, "wb");

		/* Loop for each record in the file */
		ny_line(14,0,1);
		//      od_printf("### Packing and updating user file and creating scorefile\n\r");
		if(scr_file !=NULL && justfile != NULL && fpUserFile != NULL) {
			while(scr_file != NULL && ny_fread(&urec, sizeof(user_rec), 1, justfile) == 1) {
				if (((++urec.days_not_on)<=delete_after) && urec.alive!=DEAD) {
					urec.rank=user_num_w;

					urec.bank *= 1.0+ (bank_interest/100.0);

					if (urec.alive==UNCONCIOUS && (urec.days_in_hospital++)>=2 ) {
						urec.alive=ALIVE;
						urec.hitpoints=urec.maxhitpoints;
						urec.days_in_hospital=0;
						od_printf("### %s kicked out of the hospital\n\r",ny_un_emu(urec.name,numstr));
					}
					if (urec.rest_where!=NOWHERE) {
						if (urec.hotel_paid_fer==0) {
							urec.rest_where=NOWHERE;
							od_printf("### %s was kicked out of the hotel\n\r",ny_un_emu(urec.name,numstr));
						} else {
							urec.hotel_paid_fer--;
						}
					}
					fseek(fpUserFile, (INT32)user_num_w * sizeof(user_rec), SEEK_SET);
					ny_fwrite(&urec, sizeof(user_rec), 1, fpUserFile);
					//check if on-line
					if (single_node==FALSE) {
						ch_flag_d();
						sprintf(numstr, "u%07d.bfa",user_num); //user in a battle
						if (fexist(numstr)) {
							if (user_num!=user_num_w) {
								njustfile = ShareFileOpen(numstr, "rb");
								if(njustfile != NULL) {
									ny_fread(&intval,2,1,njustfile);
									fclose(njustfile);
								}
								ny_remove(numstr);
								sprintf(numstr, "u%07d.bfo",intval);
								if(njustfile != NULL) {
									njustfile = ShareFileOpen(numstr, "wb");
									ny_fwrite(&user_num_w,2,1,njustfile);
									fclose(njustfile);
								}
								//sprintf(numstr, "del u%07d.bfa",user_num);
								//system(numstr); //see above!
							}
						}
						sprintf(numstr,"u%07d.on",user_num);
						if (fexist(numstr)) {
							scr_user.online=TRUE;
							sprintf(numstr,"u%07d.sts",user_num);
							njustfile = ShareFileOpen(numstr, "r+b");
							if(njustfile != NULL) {
								ny_fread(&urec,sizeof(user_rec),1,njustfile);
								fclose(njustfile);
							}
							if (user_num!=user_num_w) {
								ch_game_d();
								sprintf(numstr,"u%07d.inf",user_num);
								if (fexist(numstr)) {
									sprintf(numstr,"u%07d.inf",user_num);
									sprintf(numstr2,"u%07d.inf",user_num_w);
									rename(numstr,numstr2);
								}
								ch_flag_d();
								sprintf(numstr,"u%07d.cng",user_num);
								njustfile = ShareFileOpen(numstr, "a+b");
								if(njustfile != NULL) {
									ny_fwrite(&user_num_w,2,1,njustfile);
									fclose(njustfile);
								}
							}
						} else {
							scr_user.online=FALSE;
						}
					} else {
						scr_user.online=FALSE;
					}



					strcpy(scr_user.name,urec.name);
					scr_user.nation=urec.nation;
					scr_user.level=urec.level;
					scr_user.points=urec.points;
					scr_user.alive=urec.alive;
					scr_user.sex=urec.sex;
					scr_user.user_num=user_num_w;


					ny_fwrite(&scr_user, sizeof(scr_rec), 1, scr_file);

					user_num_w++;
				} else {
					od_printf("### Deleting (%s)\n\r",ny_un_emu(urec.name,numstr));
					ny_remove(SENTLIST_FILENAME);
					/*ch_game_d();
					delfile=ShareFileOpen(DELUSER_FILENAME,"a+b");
					ny_fwrite(&urec.bbsname,36,1,delfile);
					fclose(delfile);*/
				}

				/* Move user record number to next user record. */
				user_num++;
			}
			fclose(fpUserFile);
			fclose(justfile);
			fclose(scr_file);
		}
		ny_line(15,0,1);
		//      od_printf("### User file done...\n\r");
		ny_line(16,0,1);
		//      od_printf("### Sorting SCR file...\n\r");
		SortScrFile(-1,user_num_w);
		if (single_node==FALSE)
			ChangeOnlineRanks();
		ny_line(17,0,1);
		//      od_printf("### Checking for mail index file...\n\r");
		ch_game_d();
		if (fexist(MAIL_INDEX)) {

			mail_idx_type mail_idx;
			//        long fileposr,fileposw;
			INT32 mlinew,mliner;
			INT32 cnt;
			char line[80];
			FILE *rdonly1;
			FILE *rdonly2;
			ch_game_d();
			//if (fexist(DELUSER_FILENAME)) {
			cnt=0;
			ny_line(18,0,1);
			//          od_printf("### Packing mail index file...\n\r");
			justfile=ShareFileOpen(MAIL_INDEX,"r+b");
			fpUserFile=ShareFileOpen(USER_FILENAME,"rb");
			ny_line(19,0,1);
			//          od_printf("### Deleting messages for dead users...\n\r");
			if(justfile != NULL && fpUserFile != NULL) {
				while (ny_fread(&mail_idx,sizeof(mail_idx_type),1,justfile)==1) {
					fseek(fpUserFile,(INT32)0,SEEK_SET);
					mail_idx.deleted=TRUE;
					while(ny_fread(&urec,sizeof(user_rec),1,fpUserFile)==1) {
						if (strcmp(urec.bbsname,mail_idx.recverI)==0) {
							mail_idx.deleted=FALSE;
							break;
						}
					}
					if(mail_idx.deleted==TRUE) {
						fseek(justfile,(INT32)cnt*sizeof(mail_idx_type),SEEK_SET);
						ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
					}
					cnt++;
				}
				fclose(justfile);
				fclose(fpUserFile);
			}
			//ny_remove(DELUSER_FILENAME);
			//}
			ch_game_d();
			copyfile(MAIL_INDEX,MAIL_BACKUP_INDEX);
			copyfile(MAIL_FILENAME,MAIL_BACKUP_FILENAME);
			//        ny_remove(MAIL_INDEX);
			//      ny_remove(MAIL_FILENAME);

			rdonly1=ShareFileOpen(MAIL_BACKUP_INDEX,"rb");
			rdonly2=ShareFileOpen(MAIL_BACKUP_FILENAME,"rb");
			justfile=ShareFileOpen(MAIL_INDEX,"wb");
			njustfile=ShareFileOpen(MAIL_FILENAME,"wb");
			mlinew=0;
			mliner=0;
			//        fileposr=0;
			//        fileposw=0;
			ny_line(20,0,1);
			//        od_printf("### Deleting read messages...\n\r");
			if(rdonly1 != NULL && rdonly2 != NULL && justfile != NULL && njustfile != NULL) {
				while (ny_fread(&mail_idx,sizeof(mail_idx_type),1,rdonly1)==1) {
					if (mail_idx.deleted==FALSE) {
						mail_idx.location=mlinew;
						//            fseek(justfile,fileposw,SEEK_SET);
						ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
						cnt=0;

						fseek(rdonly2,mliner*80,SEEK_SET);
						while (cnt<mail_idx.length) {
							ny_fread(&line,80,1,rdonly2);
							//              fseek(njustfile,mlinew,SEEK_SET);
							ny_fwrite(&line,80,1,njustfile);
							cnt++;
						}
						mlinew+=mail_idx.length;
						//            fileposw+=sizeof(mail_idx_type);
					}// else {
					//  od_printf(".");
					//}
					mliner+=mail_idx.length;
					//          fileposr+=sizeof(mail_idx_type);
					//          fseek(justfile,fileposr,SEEK_SET);
				}
				fclose(justfile);
				fclose(njustfile);
				fclose(rdonly1);
				fclose(rdonly2);
			}
			//od_printf("\n\r");
			ny_line(21,0,1);
			//        od_printf("### Mail file done...\n\r");
		}
		if (fexist(IBBS_MAIL_INDEX)) {
			ibbs_mail_type ibmail;
			//        long fileposr,fileposw;
			//        long mlinew,mliner;
			INT32 cnt;
			//        char line[80];
			FILE *rdonly1;
			//        FILE *rdonly2;
			ch_game_d();
			//if (fexist(DELUSER_FILENAME)) {
			cnt=0;
			ny_line(447,0,1);
			//          od_printf("### Packing IBBS mail index file...\n\r");
			justfile=ShareFileOpen(IBBS_MAIL_INDEX,"r+b");
			fpUserFile=ShareFileOpen(USER_FILENAME,"rb");
			ny_line(19,0,1);
			//          od_printf("### Deleting messages for dead users...\n\r");
			if(justfile != NULL && fpUserFile != NULL) {
				while (ny_fread(&ibmail,sizeof(ibbs_mail_type),1,justfile)==1) {
					fseek(fpUserFile,(INT32)0,SEEK_SET);
					ibmail.deleted=TRUE;
					while(ny_fread(&urec,sizeof(user_rec),1,fpUserFile)==1) {
						if (strcmp(urec.bbsname,ibmail.recverI)==0) {
							ibmail.deleted=FALSE;
							break;
						}
					}
					if(ibmail.deleted==TRUE) {
						fseek(justfile,(INT32)cnt*sizeof(ibbs_mail_type),SEEK_SET);
						ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
					}
					cnt++;
				}
				fclose(justfile);
				fclose(fpUserFile);
			}
			//ny_remove(DELUSER_FILENAME);
			//}
			ch_game_d();
			copyfile(IBBS_MAIL_INDEX,IBBS_BACKUP_MAIL_INDEX);
			//        ny_remove(IBBS_MAIL_INDEX);

			rdonly1=ShareFileOpen(IBBS_BACKUP_MAIL_INDEX,"rb");
			justfile=ShareFileOpen(IBBS_MAIL_INDEX,"wb");
			ny_line(20,0,1);
			//        od_printf("### Deleting read messages...\n\r");
			if(rdonly1 != NULL && justfile != NULL) {
				while (ny_fread(&ibmail,sizeof(ibbs_mail_type),1,rdonly1)==1) {
					if (ibmail.deleted==FALSE) {

						ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
					}
				}
				fclose(justfile);
				fclose(rdonly1);
			}
			ny_line(21,0,1);
			//        od_printf("### Mail file done...\n\r");
		}


		ny_line(22,0,1);
		//      od_printf("### Cleaning up...\n\r");
		ch_game_d();
		if (maint_exec[0]!=0) {
			ny_line(23,0,1);
			//        od_printf("### Executing custom maintanance program\n\r");
			od_spawn(maint_exec);
		}
		if (fexist(TRDMAINT_FILENAME)) {
			ny_line(23,0,1);
			//        od_printf("### Executing custom maintanance program\n\r");
			CreateDropFile(FALSE);
			od_spawn(TRDMAINT_FILENAME);
			ny_remove(TRDMAINT_LIST_FILENAME);
		}

		ch_flag_d();
		ny_remove(MAINTFLAG_FILENAME);
		no_kernel=FALSE;
		ny_remove("nyerr~~~.~~~");
	}
}




void
ChangeOnlineRanks(void) {
	if (glob("u???????.on",0,NULL,&ff)) {
		FILE *justfile;
		char numstr[20];
		INT16 intval;
		user_rec urec;
		ch_flag_d();
		for(fname=ff.gl_pathv;*fname != NULL;fname++) {
			strcpy(numstr,*fname);
			numstr[0]='0';
			numstr[8]=0;
			sscanf(numstr,"%d",&intval);
			ch_game_d();
			justfile=ShareFileOpen(USER_FILENAME,"rb");
			if(justfile != NULL) {
				fseek(justfile,(INT32)intval*sizeof(user_rec),SEEK_SET);
				ny_fread(&urec,sizeof(user_rec),1,justfile);
				fclose(justfile);
			}
			strcpy(numstr,*fname);
			numstr[9]='r';
			numstr[10]='n';
			numstr[11]='k';
			numstr[12]=0;
			ch_flag_d();
			justfile=ShareFileOpen(numstr,"wb");
			if(justfile != NULL) {
				ny_fwrite(&urec.rank,2,1,justfile);
				fclose(justfile);
			}
		}
	}
}



void
CrashRecovery(void) {
	FILE *fpUserFile;
	FILE *scr_file;
	FILE *justfile;
	FILE *bakfile;
	char numstr[36];
	scr_rec scr_user;
	user_rec urec;
	INT16 intval;
	INT16 user_num=0;
	//  struct ffblk ffblk;

	no_kernel=TRUE;

	ny_line(24,0,1);
	//  od_printf("`cyan`### Running Crash Recovery\n\r");

	/* Begin with the current user record number set to 0. */
	user_num = 0;

	/*delete scores index file*/
	ch_game_d();
	if (fexist(SCR_FILENAME))
		ny_remove(SCR_FILENAME);

	/*delete maintanance running flag file*/
	ch_flag_d();
	if (fexist(MAINTFLAG_FILENAME))
		ny_remove(MAINTFLAG_FILENAME);
	/*delete all change files cuz they're not needed and will mess up*/
	if (fexist("*.cng"))
		ny_remove("*.cng");

	ch_game_d();
	scr_file = ShareFileOpen(SCR_FILENAME, "wb");
	copyfile(USER_FILENAME,USER_BACK_FILENAME);
	//  ny_remove(USER_FILENAME);
	fpUserFile = ShareFileOpen(USER_FILENAME, "wb");
	bakfile = ShareFileOpen(USER_BACK_FILENAME, "rb");

	/* Loop for each record in the file */
	ny_line(25,0,1);
	//  od_printf("### Updating (no maintanance) user file and creating scorefile\n\r");
	ch_flag_d();
	if(scr_file != NULL && fpUserFile != NULL && bakfile != NULL) {
		while(ny_fread(&urec, sizeof(user_rec), 1, bakfile) == 1 && user_num<=MAX_USERS) {
			//    ch_flag_d();
			sprintf(numstr, "u%07d.on",user_num);
			if (fexist(numstr)) {
				sprintf(numstr, "u%07d.sts",user_num);
				if (fexist(numstr)) {
					justfile = ShareFileOpen(numstr, "rb");
					if(justfile != NULL) {
						if(ny_fread(&urec, sizeof(user_rec), 1, justfile)!=1) {
							fseek(bakfile, (INT32)user_num * sizeof(user_rec), SEEK_SET);
							ny_fread(&urec, sizeof(user_rec), 1, bakfile);
						}
						fclose(justfile);
					}
				}
			}
			sprintf(numstr, "u%07d.*",user_num);
			if (fexist(numstr)) {
				//      ny_remove(numstr);
				sprintf(numstr, "u%07d.*",user_num);
				ny_remove(numstr);
			}
			urec.rank=user_num;
			//    fseek(fpUserFile, (INT32)user_num * sizeof(user_rec), SEEK_SET);
			ny_fwrite(&urec, sizeof(user_rec), 1, fpUserFile);

			//offline records all
			scr_user.online=FALSE;
			strcpy(scr_user.name,urec.name);
			scr_user.nation=urec.nation;
			scr_user.level=urec.level;
			scr_user.points=urec.points;
			scr_user.alive=urec.alive;
			scr_user.sex=urec.sex;
			scr_user.user_num=user_num;
			ny_fwrite(&scr_user, sizeof(scr_rec), 1, scr_file);

			/* Move user record number to next user record. */
			user_num++;
		}
		fclose(fpUserFile);
		fclose(scr_file);
		fclose(bakfile);
	}
	ny_line(26,0,1);
	//  od_printf("### User file done...\n\r");
	ny_line(27,0,1);
	//  od_printf("### Sorting new SCR file...\n\r");
	SortScrFile(-1,user_num);
	ny_remove("mnu*.dat");
	ny_remove("fev*.dat");
	ny_remove("n???????.sts");
	ny_remove("n???????.stt");
	//  ny_remove("nyerr~~~.~~~");
	no_kernel=FALSE;
}




void
SortScrFile(INT16 usr,INT16 max) // pebble sorting of scorefile
{
	FILE *justfile;
	FILE *scr_file;
	FILE *fpUserFile;
	FILE *njustfile;
	INT16 crnt1,crnt2,cnt,sorted;
	scr_rec rec[2];
	user_rec urec;
	char numstr[20];
	//  ffblk ffblk;

	if (usr==-1) {
		ch_game_d();
		scr_file=ShareFileOpenAR(SCR_FILENAME,"r+b");
		fpUserFile=ShareFileOpenAR(USER_FILENAME,"r+b");
		if(scr_file != NULL && fpUserFile != NULL) {
			do {
				cnt=1;
				sorted=TRUE;


				while (cnt<max) {
					fseek(scr_file, (INT32)(cnt-1) * sizeof(scr_rec), SEEK_SET);
					ny_fread(&rec[0], sizeof(scr_rec), 1, scr_file);


					fseek(scr_file, (INT32)cnt * sizeof(scr_rec), SEEK_SET);
					if (ny_fread(&rec[1], sizeof(scr_rec), 1, scr_file)!=1) {
						/* This is terrible - ToDo */
	#ifndef ODPLAT_NIX
						fcloseall();
	#endif

						return;
					}

					if (rec[0].points<rec[1].points) { // switch records

						sorted=FALSE; // run another round
						fseek(fpUserFile, (INT32)rec[0].user_num * sizeof(user_rec), SEEK_SET);
						ny_fread(&urec,sizeof(user_rec),1,fpUserFile);
						urec.rank=cnt;

						fseek(fpUserFile, (INT32)rec[0].user_num * sizeof(user_rec), SEEK_SET);
						ny_fwrite(&urec,sizeof(user_rec),1,fpUserFile);

						fseek(fpUserFile, (INT32)rec[1].user_num * sizeof(user_rec), SEEK_SET);
						ny_fread(&urec,sizeof(user_rec),1,fpUserFile);
						urec.rank=cnt-1;

						fseek(fpUserFile, (INT32)rec[1].user_num * sizeof(user_rec), SEEK_SET);
						ny_fwrite(&urec,sizeof(user_rec),1,fpUserFile);

						do
							fseek(scr_file, (INT32)(cnt-1) * sizeof(scr_rec), SEEK_SET);
						while (ny_fwrite(&rec[1],sizeof(scr_rec),1,scr_file)!=1);

						do
							fseek(scr_file, (INT32)cnt * sizeof(scr_rec), SEEK_SET);
						while (ny_fwrite(&rec[0],sizeof(scr_rec),1,scr_file)!=1);

					}
					cnt++;
				}
				fseek(fpUserFile, (INT32)0, SEEK_SET);
				fseek(scr_file, (INT32)0, SEEK_SET);
				max--;
			} while (sorted==FALSE);
			fclose(scr_file);
			fclose(fpUserFile);
		}
	} else { // sort in a certain user either current or an offline user
		ch_game_d();
		// if(no_kernel==TRUE) {
		scr_file=ShareFileOpenAR(SCR_FILENAME,"r+b");
		fpUserFile=ShareFileOpenAR(USER_FILENAME,"r+b");
		if(scr_file != NULL && fpUserFile != NULL) {
			/*    } else {
		    	  no_kernel=TRUE;
		    	  scr_file=ShareFileOpenAR(SCR_FILENAME,"r+b");
		    	  fpUserFile=ShareFileOpenAR(USER_FILENAME,"r+b");
		    	  no_kernel=FALSE;
		    	}*/

			if (usr==nCurrentUserNumber) {
				cnt=cur_user.rank;
			} else {
				fseek(fpUserFile, (INT32)usr * sizeof(user_rec), SEEK_SET);
				ny_fread(&urec, sizeof(user_rec), 1, fpUserFile);
				cnt=urec.rank;
			}
			//od_printf("\n\r\n\r%d\n\r\n\r",cnt);

			crnt1=0;
			crnt2=1;

			strcpy(rec[crnt1].name,cur_user.name);
			rec[crnt1].nation=cur_user.nation;
			rec[crnt1].level=cur_user.level;
			rec[crnt1].points=cur_user.points;
			rec[crnt1].alive=cur_user.alive;
			rec[crnt1].sex=cur_user.sex;
			rec[crnt1].user_num=nCurrentUserNumber;
			rec[crnt1].online=TRUE;

			fseek(scr_file, (INT32)cur_user.rank * sizeof(scr_rec), SEEK_SET);
			ny_fwrite(&rec[crnt1], sizeof(scr_rec), 1, scr_file);

			if (cnt>0) {
				ch_flag_d();
				do {
					sorted=TRUE;

					fseek(scr_file, (INT32)(cnt-1) * sizeof(scr_rec), SEEK_SET);
					ny_fread(&rec[crnt1], sizeof(scr_rec), 1, scr_file);

					fseek(scr_file, (INT32)cnt * sizeof(scr_rec), SEEK_SET);
					ny_fread(&rec[crnt2], sizeof(scr_rec), 1, scr_file);

					if (rec[crnt1].points<rec[crnt2].points) { // switch records

						sorted=FALSE; // run another round

						sprintf(numstr,"u%07d.on",rec[crnt1].user_num);
						if (single_node==FALSE && fexist(numstr)) {
							sprintf(numstr,"u%07d.rnk",rec[crnt1].user_num);
							njustfile = ShareFileOpen(numstr, "wb");
							if(njustfile != NULL) {
								ny_fwrite(&cnt,2,1,njustfile);
								fclose(njustfile);
							}
						} else {
							fseek(fpUserFile, (INT32)rec[crnt1].user_num * sizeof(user_rec), SEEK_SET);
							ny_fread(&urec,sizeof(user_rec),1,fpUserFile);
							urec.rank=cnt;
							fseek(fpUserFile, (INT32)rec[crnt1].user_num * sizeof(user_rec), SEEK_SET);
							ny_fwrite(&urec,sizeof(user_rec),1,fpUserFile);
						}

						if (usr==nCurrentUserNumber) {
							cur_user.rank=cnt-1;
						} else {
							sprintf(numstr,"u%07d.on",rec[crnt2].user_num);
							if (single_node==FALSE && fexist(numstr)) {
								sprintf(numstr,"u%07d.rnk",rec[crnt2].user_num);
								njustfile = ShareFileOpen(numstr, "wb");
								if(njustfile != NULL) {
									cnt--;
									ny_fwrite(&cnt,2,1,njustfile);
									cnt++;
									fclose(njustfile);
								}
							} else {
								fseek(fpUserFile, (INT32)rec[crnt2].user_num * sizeof(user_rec), SEEK_SET);
								ny_fread(&urec,sizeof(user_rec),1,fpUserFile);
								urec.rank=cnt-1;
								fseek(fpUserFile, (INT32)rec[crnt2].user_num * sizeof(user_rec), SEEK_SET);
								ny_fwrite(&urec,sizeof(user_rec),1,fpUserFile);
							}
						}

						fseek(scr_file, (INT32)(cnt-1) * sizeof(scr_rec), SEEK_SET);
						ny_fwrite(&rec[crnt2],sizeof(scr_rec),1,scr_file);
						fseek(scr_file, (INT32)cnt * sizeof(scr_rec), SEEK_SET);
						ny_fwrite(&rec[crnt1],sizeof(scr_rec),1,scr_file);

					}
					cnt--;
				} while (sorted==FALSE && cnt>0);
			}

			fclose(scr_file);
			fclose(fpUserFile);
		}
	}
}



void
SortScrFileB(INT16 usr) // pebble sorting of scorefile
{
	FILE *justfile;
	FILE *scr_file;
	FILE *fpUserFile;
	FILE *njustfile;
	INT16 crnt1,crnt2,cnt,sorted,cont;
	scr_rec rec[2];
	user_rec urec;
	char numstr[20];
	//  ffblk ffblk;

	// sort in a certain user either current or an offline user
	ch_game_d();
	scr_file=ShareFileOpenAR(SCR_FILENAME,"r+b");
	fpUserFile=ShareFileOpenAR(USER_FILENAME,"r+b");
	if(scr_file != NULL && fpUserFile != NULL) {
		if (usr==nCurrentUserNumber) {
			cnt=cur_user.rank;
		} else {
			fseek(fpUserFile, (INT32)usr * sizeof(user_rec), SEEK_SET);
			ny_fread(&urec, sizeof(user_rec), 1, fpUserFile);
			cnt=urec.rank;
		}
		//od_printf("\n\r\n\r%d\n\r\n\r",cnt);

		crnt1=0;
		crnt2=1;

		strcpy(rec[crnt1].name,cur_user.name);
		rec[crnt1].nation=cur_user.nation;
		rec[crnt1].level=cur_user.level;
		rec[crnt1].points=cur_user.points;
		rec[crnt1].alive=cur_user.alive;
		rec[crnt1].sex=cur_user.sex;
		rec[crnt1].user_num=nCurrentUserNumber;
		rec[crnt1].online=TRUE;


		fseek(scr_file, (INT32)cur_user.rank * sizeof(scr_rec), SEEK_SET);
		ny_fwrite(&rec[crnt1], sizeof(scr_rec), 1, scr_file);
		cont=filelength(fileno(scr_file))/sizeof(scr_rec);

		if (cnt<(cont-1)) {
			ch_flag_d();
			do {
				sorted=TRUE;

				fseek(scr_file, (INT32)(cnt+1) * sizeof(scr_rec), SEEK_SET);
				ny_fread(&rec[crnt1], sizeof(scr_rec), 1, scr_file);

				fseek(scr_file, (INT32)cnt * sizeof(scr_rec), SEEK_SET);
				ny_fread(&rec[crnt2], sizeof(scr_rec), 1, scr_file);
				if (rec[crnt1].points>rec[crnt2].points) { // switch records

					sorted=FALSE; // run another round

					sprintf(numstr,"u%07d.on",rec[crnt1].user_num);
					if (single_node==FALSE && fexist(numstr)) {
						sprintf(numstr,"u%07d.rnk",rec[crnt1].user_num);
						njustfile = ShareFileOpen(numstr, "wb");
						if(njustfile != NULL) {
							ny_fwrite(&cnt,2,1,njustfile);
							fclose(njustfile);
						}
					} else {
						fseek(fpUserFile, (INT32)rec[crnt1].user_num * sizeof(user_rec), SEEK_SET);
						ny_fread(&urec,sizeof(user_rec),1,fpUserFile);
						urec.rank=cnt;
						fseek(fpUserFile, (INT32)rec[crnt1].user_num * sizeof(user_rec), SEEK_SET);
						ny_fwrite(&urec,sizeof(user_rec),1,fpUserFile);
					}

					if (usr==nCurrentUserNumber) {
						cur_user.rank=cnt+1;
					} else {
						sprintf(numstr,"u%07d.on",rec[crnt2].user_num);
						if (single_node==FALSE && fexist(numstr)) {
							sprintf(numstr,"u%07d.rnk",rec[crnt2].user_num);
							njustfile = ShareFileOpen(numstr, "wb");
							if(njustfile != NULL) {
								cnt++;
								ny_fwrite(&cnt,2,1,njustfile);
								cnt--;
								fclose(njustfile);
							}
						} else {
							fseek(fpUserFile, (INT32)rec[crnt2].user_num * sizeof(user_rec), SEEK_SET);
							ny_fread(&urec,sizeof(user_rec),1,fpUserFile);
							urec.rank=cnt+1;
							fseek(fpUserFile, (INT32)rec[crnt2].user_num * sizeof(user_rec), SEEK_SET);
							ny_fwrite(&urec,sizeof(user_rec),1,fpUserFile);
						}
					}

					fseek(scr_file, (INT32)(cnt+1) * sizeof(scr_rec), SEEK_SET);
					ny_fwrite(&rec[crnt2],sizeof(scr_rec),1,scr_file);
					fseek(scr_file, (INT32)cnt * sizeof(scr_rec), SEEK_SET);
					ny_fwrite(&rec[crnt1],sizeof(scr_rec),1,scr_file);

				}
				cnt++;
			} while (sorted==FALSE && cnt<(cont-1));
		}
		fclose(scr_file);
		fclose(fpUserFile);
	}
}



void
change_info(void) {
	char key;
	char numstr[25],numstr2[25],numstr3[25];


	do {
		od_printf("\n\r\n\r");
		ny_clr_scr();

		ny_line(28,0,2);
		//    ny_disp_emu("`@C`4hange `@Y`4er `@I`4nfo\n\r\n\r");
		ny_line(29,0,0);
		//    ny_disp_emu("`%1 `4- `@Y`4er name: `0");
		ny_disp_emu(cur_user.name);
		ny_line(30,1,0);
		//    ny_disp_emu("\n\r`%2 `4- `@W`4hen you win ya say: `0");
		ny_disp_emu(cur_user.say_win);
		ny_line(31,1,0);
		//    ny_disp_emu("\n\r`%3 `4- `@W`4hen you loose ya say: `0");
		ny_disp_emu(cur_user.say_loose);
		ny_line(32,1,1);
		//    ny_disp_emu("\n\r`%Q `4- `@R`4eturn to Central Park\n\r");

		ny_line(33,1,0);
		//    ny_disp_emu("\n\r`9W`1hich ya wanna change? (`91`1,`92`1,`93`1,`9Q`1`)");

		key=ny_get_answer("123Q");

		od_printf("%c\n\r\n\r",key);
		if (key=='1') {
			do {

				od_printf("\n\r");
				if(rip)
					od_send_file("texti.rip");
				ny_send_menu(NEW_NAME,"");
				//      ny_disp_emu("\n\r`@I`4nput yer new name: (`@ENTER`4=abort\n\r|------------------------|\n\r`0");

				od_input_str(numstr,24,' ',255);
			} while (numstr[0]!=0 && stricmp(ny_un_emu(numstr,numstr2),ny_un_emu(cur_user.name,numstr3))!=0 && CheckForHandle(numstr)!=FALSE);
			trim(numstr);
			if (numstr[0]!=0) {
				strcpy(cur_user.name,numstr);
				wrt_sts();
				sprintf(od_control.user_name,"%s (%s)",ny_un_emu(cur_user.name,numstr),uname);
				ibbs_act_rec act_rec;
				act_rec.action=0;
				strcpy(act_rec.name_sI,cur_user.bbsname);
				strcpy(act_rec.name_s,cur_user.name);
				if(ibbs)
					IBSendAll(&IBBSInfo,(char *)&act_rec,sizeof(ibbs_act_rec));
				od_control.od_update_status_now=TRUE;
			}
		}
		if (key=='2') {

			od_printf("\n\r");
			if(rip)
				od_send_file("texti.rip");
			ny_send_menu(NEW_WIN,"");
			//      ny_disp_emu("\n\r`@W`4hat do you say when you win:\n\r|--------------------------------------|\n\r`0");

			od_input_str(cur_user.say_win,40,' ',255);
			wrt_sts();
		} else if (key=='3') {

			od_printf("\n\r");
			if(rip)
				od_send_file("texti.rip");
			ny_send_menu(NEW_LOOSE,"");

			//      od_printf("\n\r\n\r`@W`4hat do you say when you get yer ass kicked:\n\r|--------------------------------------|`0\n\r");

			od_input_str(cur_user.say_loose,40,' ',255);
			wrt_sts();
		}
	} while (key!='Q');
}



char entry_menu(void) {
	char key;
	char allowed[]="ELYNQ\n\r";
	FILE *justfile;
	INT16 intval;
	static int unreg_sign=TRUE;
	char numstr[100];


	key=0;



	od_clear_keybuffer();               // Clear any pending keys in buffer
	od_printf("\n\r\n\r");
	ny_clr_scr();                       // Clear screen

	ny_send_menu(ENTRY_1,"");

	/*   ny_disp_emu("\n\r`@N`4ew `@Y`4ork `@2008\n\r\n\r");
	//   ny_kernel();
	   ny_disp_emu("`2By `0Franz\n\r\n\r");//`bright green`Doom`green` helped\n\r\n\r");
	//   ny_kernel();
	   ny_disp_emu("`4Version `@0.01 `%BETA 9\n\r");
	//   ny_kernel();
	   ny_disp_emu("`1Copyright 1995, George Lebl\n\r");*/

	if(!rip) {
		//od_printf("`blue`Thx fer the help Martin ...\n\r");
		if(clean_mode==TRUE)
			ny_disp_emu("`@C`4lean mode `@ON\n\r");
		//   else if(clean_mode==666)
		//     ny_disp_emu("`@X`4tra Fucking Dirty mode `@ON\n\r");
		if (registered==TRUE) {
			ny_disp_emu("`@FREEWARE");
			/*     ny_disp_emu("`@REGISTERED `4to ");
			     ny_disp_emu(od_control.system_name);
			     if(rec_credit[0]!=0) {
			       ny_disp_emu("`@!\n\r");
			       ny_line(405,0,0);//`@T`4hanks to `0");
			       ny_disp_emu(rec_credit);
			     } */
			ny_disp_emu("`@!\n\r");
		} else {
			if(unreg_sign==TRUE) {
				ny_disp_emu("`%UNREGISTERED!\n\rPausing For 5 Seconds `#.");
				unreg_sign=FALSE;
				sleep(1);
				ny_kernel();
				od_printf(".");
				sleep(1);
				ny_kernel();
				od_printf(".");
				sleep(1);
				ny_kernel();
				od_printf(".");
				sleep(1);
				ny_kernel();
				od_printf(".");
				ny_kernel();
				sleep(1);
				od_printf("\r");
			} else {
				ny_disp_emu("`%UNREGISTERED!\n\r");
				ny_line(404,0,1);
			}
		}
		ny_kernel();
		ch_game_d();
		justfile=ShareFileOpen(GAMEDAY_FILENAME,"rb");
		if(justfile != NULL) {
			ny_fread(&intval,2,1,justfile);
			fclose(justfile);
		}
		ny_line(34,0,0);
		//   ny_disp_emu("`$T`6his game has been running for `0");
		od_printf("%d",intval);
		ny_line(35,0,1);
		//   ny_disp_emu("`6 days!\n\r");

		key=ny_send_menu(ENTRY_2,allowed);
	}

	if(!rip) {
		ny_line(36,0,0);
		//Enter Yer Command (
		od_printf("%d ",od_control.caller_timelimit);
		ny_line(37,0,0);
	}
	//   ny_disp_emu("`1mins)`9>");
	//   entry_after:;


	if(key==0) {
		if(rip)
			od_disp_str("\n\r!|10000$HKEYON$|#|#|#\n\r");
		key= ny_get_answer(allowed);
		if(rip)
			od_disp_str("\n\r!|10000$HKEYOFF$|#|#|#\n\r");
	}
	if(!rip)
		od_printf("%c\n\r",key);
	else
		od_disp_str("\n\r");

	return key;
}











/*user stats display*/
void
DisplayStats(void) {
	INT16 intval;

	ny_kernel();
	wrt_sts(); //just in case write the stats file

	if(oneframe==FALSE || rip==FALSE) {
		od_printf("\n\r\n\r");
		ny_clr_scr();
		if(rip)
			od_send_file("frame1.rip");
	} else {
		od_disp_str("\n\r!|e|#|#|#\n\r  \b\b");
	}
	//ny_line(38,0,0);
	ny_stat_line(0,0,0);
	//  ny_disp_emu("`%S`!t`9ats `%F`!o`9r `%L`!e`9vel `%");
	od_printf("%d ",(INT16)cur_user.level);
	switch (cur_user.nation) {
		case HEADBANGER:
			ny_stat_line(1,0,0);
			break;
		case HIPPIE:
			ny_stat_line(2,0,0);
			break;
		case BIG_FAT_DUDE:
			ny_stat_line(3,0,0);
			break;
		case CRACK_ADDICT:
			ny_stat_line(4,0,0);
			break;
		case PUNK:
			ny_stat_line(5,0,0);
			break;
	}
	ny_disp_emu(" `@");
	ny_disp_emu(cur_user.name);
	//  od_printf("\n\r");
	//  ny_send_menu(BLUE_LINE,"");
	//  ny_line(399,1,1);
	ny_stat_line(6,1,1);
	// ny_disp_emu("\n\r`1-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
	//ny_line(39,0,0);
	ny_stat_line(7,0,0);
	//  ny_disp_emu("`@S`4ex: `0");
	if (cur_user.sex==MALE)
		ny_stat_line(8,0,0);
	else
		ny_stat_line(9,0,0);

	od_repeat(' ',29);
	//ny_line(270,0,0);
	ny_stat_line(10,0,0);
	//ny_disp_emu("`@S`4tatus: `@");
	if (cur_user.alive==ALIVE)
		//ny_line(369,0,1);
		ny_stat_line(11,0,1);
	//ny_disp_emu("`0A`2live\n\r");
	else if (cur_user.alive==DEAD)
		//ny_line(370,0,1);
		ny_stat_line(12,0,1);
	//ny_disp_emu("DEAD\n\r");
	else
		//ny_line(371,0,1);
		ny_stat_line(13,0,1);
	//ny_disp_emu("UNCONSIOUS\n\r");
	//ny_line(271,0,0);
	ny_stat_line(14,0,0);
	//  od_printf("`bright red`P`red`oints: `bright green`
	od_printf("%-16s",D_Num(cur_user.points));
	od_repeat(' ',16);
	//ny_line(272,0,0);
	ny_stat_line(15,0,0);
	//  od_printf("`bright red`M`red`oney `bright red`I`red`n `bright red`H`red`and: `bright green`
	od_printf("%s\n\r",D_Num(cur_user.money));
	//ny_line(273,0,0);
	ny_stat_line(16,0,0);
	//  od_printf("`bright red`F`red`ights: `bright green`
	od_printf("%-3d", (INT16)cur_user.turns);
	od_repeat(' ',29);
	//ny_line(274,0,0);
	ny_stat_line(17,0,0);
	//  od_printf("`bright red`M`red`oney `bright red`I`red`n `bright red`B`red`ank: `bright green`
	od_printf("%s\n\r",D_Num(cur_user.bank));
	//ny_line(275,0,0);
	ny_stat_line(18,0,0);
	//  od_printf("`bright red`H`red`it `bright red`P`red`oints: `bright green`
	od_printf("%s ",D_Num(cur_user.hitpoints));
	intval=strlen(str);
	//ny_line(276,0,0);
	ny_stat_line(19,0,0);
	//  od_printf("`bright red`o`red`f `bright green`
	od_printf("%-9s", D_Num(cur_user.maxhitpoints));
	od_repeat(' ',40-intval-25);
	//ny_line(277,0,0);
	ny_stat_line(20,0,0);
	//  od_printf("`bright red`H`red`ungry: `bright green`
	od_printf("%d%c\n\r", (INT16)cur_user.hunger,37);
	//ny_line(278,0,0);
	ny_stat_line(21,0,0);
	//  ny_disp_emu("`@A`4rm: ");
	print_arm(cur_user.arm);
	//ny_line(279,1,0);
	ny_stat_line(22,1,0);
	//  ny_disp_emu("\n\r`@W`4inning `@S`4entence: `0");
	ny_disp_emu(cur_user.say_win);
	//ny_line(280,1,0);
	ny_stat_line(23,1,0);
	//  ny_disp_emu("\n\r`@L`4oosing `@S`4entence: `0");
	ny_disp_emu(cur_user.say_loose);
	//ny_line(281,1,0);
	ny_stat_line(24,1,0);
	//  od_printf("\n\r`bright red`D`red`ays `bright red`S`red`ince `bright red`G`red`ot `bright red`L`red`aid: `bright green`
	od_printf("%-4d", (INT16)cur_user.since_got_laid);
	od_repeat(' ',15);
	//ny_line(282,0,0);
	ny_stat_line(25,0,0);
	//  ny_disp_emu("`@S`4TD: `0");
	print_disease(cur_user.std);
	//ny_line(283,1,0);
	ny_stat_line(26,1,0);
	//  od_printf("\n\r`bright red`S`red`ex `bright red`T`red`urns `bright red`L`red`eft: `bright green`
	od_printf("%-4d", (INT16)cur_user.sex_today);
	od_repeat(' ',20);
	//ny_line(284,0,0);
	ny_stat_line(27,0,0);
	//  od_printf("`bright red`I`red`nfected: `bright green`
	od_printf("%d%c\n\r", (INT16)cur_user.std_percent,37);
	//ny_line(285,0,0);
	ny_stat_line(28,0,0);
	//  od_printf("`bright red`C`red`ondoms: `bright green`
	od_printf("%d\n\r", (INT16)cur_user.condoms);
	//ny_line(433,0,0);
	ny_stat_line(29,0,0);
	od_printf("%d\n\r", (INT16)cur_user.rocks);
	//ny_line(434,0,0);
	ny_stat_line(30,0,0);
	od_printf("%d ", (INT16)cur_user.throwing_ability);
	//ny_line(435,0,0);
	ny_stat_line(31,0,0);
	od_printf("%d ", (INT16)cur_user.kick_ability);
	//ny_line(436,0,0);
	ny_stat_line(32,0,0);
	od_printf("%d\n\r", (INT16)cur_user.punch_ability);
	//ny_line(286,0,0);
	ny_stat_line(33,0,0);
	//  ny_disp_emu("`@D`4rug: `0");
	print_drug(cur_user.drug);
	//ny_line(287,1,0);
	ny_stat_line(34,1,0);
	//  od_printf("\n\r`bright red`H`red`its: `bright green`
	od_printf("%-6d\n\r",(INT16)cur_user.drug_hits);
	//ny_line(288,0,0);
	ny_stat_line(35,0,0);
	//  od_printf("`bright red`H`red`igh: `bright green`
	od_printf("%d%c\n\r",(INT16)cur_user.drug_high,37);
	if (cur_user.drug>=COKE) {
		//ny_line(289,0,0);
		ny_stat_line(36,0,0);
		//    od_printf("`bright red`A`red`ddicted: `bright green`
		od_printf("%d%c\n\r", (INT16)cur_user.drug_addiction,37);
		if (cur_user.drug_addiction>0) {
			//ny_line(290,0,0);
			ny_stat_line(37,0,0);
			//od_printf("`bright red`D`red`ays `bright red`S`red`ince `bright red`L`red`ast `bright red`H`red`it: `bright green`
			od_printf("%d\n\r",(INT16)cur_user.drug_days_since);
		}
	}
	if(cur_user.rest_where!=NOWHERE) {
		//ny_line(291,0,0);
		ny_stat_line(38,0,0);
		//  ny_disp_emu("`@S`4taying `@a`4t: `0");
		if (cur_user.rest_where==MOTEL)
			ny_stat_line(39,0,0);// ny_line(372,0,0); //ny_disp_emu("C`2heap `0M`2otel");
		else if (cur_user.rest_where==REG_HOTEL)
			ny_stat_line(40,0,0); //ny_line(373,0,0);//ny_disp_emu("R`2egular `0H`2otel");
		else if (cur_user.rest_where==EXP_HOTEL)
			ny_stat_line(41,0,0);//ny_line(374,0,0);//ny_disp_emu("E`2xpensive `0H`2otel");
		//ny_line(292,0,0);
		ny_stat_line(42,0,0);
		//    ny_disp_emu(" `@F`4or `0");
		od_printf("%d",(INT16)cur_user.hotel_paid_fer);
		//ny_line(293,0,1);
		ny_stat_line(43,0,1);
		//    ny_disp_emu("`@ D`4ays\n\r");
	}
	//  ny_send_menu(BLUE_LINE,"");
	//ny_line(399,0,1);
	ny_stat_line(6,0,1);
	//  ny_disp_emu("`1-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");

	//  WaitForEnter();
}






void   //die of 1=drugs 2=hunger 3=std 4=suicide
Die(INT16 diecode) {
	cur_user.alive=DEAD;
	wrt_sts();

	od_printf("\n\r\n\r");
	ny_clr_scr();
	ny_line(40,0,2);
	//  ny_disp_emu("\n\r\n\r`@W`4ell it's end of the road for ya....yer `%DEAD`4!");
	ny_line(41,0,0);
	//  ny_disp_emu("\n\r\n\r`0Y`2ou died of: `0");
	switch (diecode) {
		case 1:
			ny_line(42,0,2);
			ny_line(43,0,0);
			//    ny_disp_emu("Drug Overdose\n\r\n\r`@Y`4a should consider cutting down on those drugs next time...");
			news_post("died of drug overdose",cur_user.name,"",3);
			break;
		case 2:
			ny_line(44,0,2);
			ny_line(45,0,0);
			//    ny_disp_emu("Hunger\n\r\n\r`@Y`4a should consider eating more next time...");
			news_post("died of hunger",cur_user.name,"",3);
			break;
		case 3:
			ny_line(46,0,2);
			if(cur_user.sex==MALE)
				ny_line(47,0,0);
			else
				ny_line(48,0,0);

			news_post("died of sexual desease",cur_user.name,"",3);
			break;

		case 4:
			ny_line(49,0,2);
			ny_line(50,0,0);
			//    ny_disp_emu("Suicide\n\r\n\r`@Y`4a should talk to somebody before you get that down ...next time...");
			news_post("comitted suicide",cur_user.name,"",3);
			break;
	}

	ny_line(51,2,1);
	//  od_printf("\n\r\n\r`bright`But you can still start all over tomorrow and be the best!\n\r");
	WaitForEnter();
	od_exit(10,FALSE);
}



void
print_drug(drug_type drug) {
	switch(drug) {
		case POT:
			ny_line(375,0,0);
			break;     // Pot
		case HASH:
			ny_line(376,0,0);
			break;    // Hash
		case LSD:
			ny_line(377,0,0);
			break;     // LSD
		case COKE:
			ny_line(378,0,0);
			break;    // Coke
		case PCP:
			ny_line(379,0,0);
			break;     // PCP
		case HEROIN:
			ny_line(380,0,0);
			break;  // Heroin
	}
}


void
print_disease(desease ill) {
	switch (ill) {
		case NONE:
			ny_line(381,0,0);
			break;              // Healthy
		case CRAPS:
			ny_line(382,0,0);
			break;             // Crabs
		case HERPES:
			ny_line(383,0,0);
			break;            // Herpes
		case SYPHILIS:
			ny_line(384,0,0);
			break;          // Syphilis
		case AIDS:
			ny_line(385,0,0);
			break;              // AIDS
	}
}




void
print_arm(weapon arm) {
	switch (arm) {
		case HANDS:
			ny_line(386,0,0);
			break;
		case PEPPER:
			ny_line(387,0,0);
			break;
		case SHARP_STICK:
			ny_line(471,0,0);
			break;
		case SCREWDRIVER:
			ny_line(472,0,0);
			break;
		case KNIFE:
			ny_line(388,0,0);
			break;
		case HAMMER:
			ny_line(473,0,0);
			break;
		case CHAIN:
			ny_line(389,0,0);
			break;
		case LEAD_PIPE:
			ny_line(474,0,0);
			break;
		case GUN:
			ny_line(390,0,0);
			break;
		case COLT:
			ny_line(475,0,0);
			break;
		case RIFLE:
			ny_line(391,0,0);
			break;
		case ELEPHANT_GUN:
			ny_line(476,0,0);
			break;
		case LASER_GUN:
			ny_line(392,0,0);
			break;
		case NAILGUN:
			ny_line(477,0,0);
			break;
		case SHOTGUN:
			ny_line(393,0,0);
			break;
		case ASSAULT_RIFLE:
			ny_line(478,0,0);
			break;
		case MACHINEGUN:
			ny_line(394,0,0);
			break;
		case PROTON_GUN:
			ny_line(479,0,0);
			break;
		case GRANADE_LAUNCHER:
			ny_line(395,0,0);
			break;
		case NEUTRON_PHASER:
			ny_line(480,0,0);
			break;
		case BLASTER:
			ny_line(396,0,0);
			break;
		case ULTRASOUND_GUN:
			ny_line(481,0,0);
			break;
		case A_BOMB:
			ny_line(397,0,0);
			break;
	}
}

/*current player listing*/
void
MakeFiles(void) {
	FILE *scr_file;
	FILE *ascii_file;
	FILE *ansi_file;
	scr_rec user_scr;
	INT32 filepos;
	INT16 cnt;

	//od_printf("\n\r\n\r");
	//ny_clr_scr();
	ch_game_d();
	ansi_file=ShareFileOpen(ansi_name,"w");
	ascii_file=ShareFileOpen(ascii_name,"w");
	if(ansi_file != NULL && ascii_file != NULL) {
		fprintf(ansi_file,"[0;1;32mP[0;32mlayer [1;32mL[0;32misting [1;32mF[0;32mor [1;31mN[0;31mew [1;31mY[0;31mork [1;31m2008\n\n");
		fprintf(ascii_file,"Player Listing For New York 2008\n\n");
		fprintf(ansi_file,"[0;32m-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
		fprintf(ascii_file,"-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
		if ((scr_file=ShareFileOpen(SCR_FILENAME,"rb"))!=NULL) {
			cnt=1;
			fprintf(ansi_file,"[1;36mRank: Lvl: Name:                     Points:              S: T:\n");
			fprintf(ascii_file,"Rank: Lvl: Name:                     Points:              S: T:\n");
			while (ny_fread(&user_scr, sizeof(scr_rec), 1, scr_file) == 1) {
				fprintf(ansi_file,"[1;34m%-5d [1;32m%-2d   [1;31m",cnt,user_scr.level);
				fprintf(ascii_file,"%-5d %-2d   ",cnt,user_scr.level);
				ny_disp_emu_file(ansi_file,ascii_file,user_scr.name,25);
				fprintf(ansi_file," [1;32m%-20s ",D_Num(user_scr.points));
				fprintf(ascii_file," %-20s ",D_Num(user_scr.points));
				if (user_scr.sex==MALE) {
					fprintf(ansi_file,"[1;33mM  ");
					fprintf(ascii_file,"M  ");
				} else {
					fprintf(ansi_file,"[1;33mF  ");
					fprintf(ascii_file,"F  ");
				}

				switch(user_scr.nation) {
					case HEADBANGER:
						fprintf(ansi_file,"[1;34mHEADBANGER    ");
						fprintf(ascii_file,"HEADBANGER   ");
						break;
					case HIPPIE:
						fprintf(ansi_file,"[1;34mHIPPIE       ");
						fprintf(ascii_file,"HIPPIE       ");
						break;
					case BIG_FAT_DUDE:
						fprintf(ansi_file,"[1;34mBIG FAT DUDE ");
						fprintf(ascii_file,"BIG FAT DUDE ");
						break;
					case CRACK_ADDICT:
						fprintf(ansi_file,"[1;34mCRACK ADDICT ");
						fprintf(ascii_file,"CRACK ADDICT ");
						break;
					case PUNK:
						fprintf(ansi_file,"[1;34mPUNK         ");
						fprintf(ascii_file,"PUNK         ");
						break;
				}

				fprintf(ansi_file,"\n");
				fprintf(ascii_file,"\n");
				cnt++;
			}
			fclose(scr_file);
		}
		fclose(ansi_file);
		fclose(ascii_file);
	}
}


void
PlInfo(scr_rec *user_scr, INT16 cnt) {
	od_printf("`bright blue`%-5d `bright green`%-2d   `bright red`",cnt,user_scr->level);
	ny_disp_emu(user_scr->name,25);
	od_set_attrib(0x0a);
	od_printf(" %-20s ",D_Num(user_scr->points));
	if (user_scr->sex==MALE)
		ny_disp_emu("`$M  ");
	else
		ny_disp_emu("`$F  ");
	od_set_attrib(0x09);
	switch (user_scr->nation) {
		case HEADBANGER:
			od_disp_str("HEADBANGER   ");
			break;
		case HIPPIE:
			od_disp_str("HIPPIE       ");
			break;
		case BIG_FAT_DUDE:
			od_disp_str("BIG FAT DUDE ");
			break;
		case CRACK_ADDICT:
			od_disp_str("CRACK ADDICT ");
			break;
		case PUNK:
			od_disp_str("PUNK         ");
			break;
	}
	if (user_scr->online==TRUE)
		ny_disp_emu("`%ON");
	od_disp_str("\n\r");
}



/*current player listing*/
void
ListPlayers(void) {
	FILE *scr_file;
	scr_rec user_scr;
	INT32 filepos;
	INT16 cnt,cnt2;
	char key;
	INT16 nonstop=FALSE;

	od_printf("\n\r\n\r");
	ny_clr_scr();


	if(rip) {
		od_send_file("frame.rip");
		od_send_file("frame3.rip");
	}


	cnt=1;
	cnt2=5;
	ny_send_menu(LIST,"");
	/*      od_printf("`bright green`P`green`layer `bright green`L`green`isting `bright green`F`green`or `bright red`N`red`ew `bright red`Y`red`ork `bright red`2008\n\r\n\r");
		od_printf("`green`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
		od_printf("`cyan`Rank: Lvl: Name:                     Points:              S: T:\n\r");*/
	ch_game_d();
	if ((scr_file=ShareFileOpen(SCR_FILENAME,"rb"))!=NULL) {
		//cnt=1;
		while (scr_file != NULL && ny_fread(&user_scr, sizeof(scr_rec), 1, scr_file) == 1) {
			PlInfo(&user_scr,cnt);

			cnt++;
			cnt2++;
			if (scr_file != NULL && nonstop==FALSE && cnt2%od_control.user_screen_length==0) {
				filepos=ftell(scr_file);
				fclose(scr_file);

				ny_disp_emu("`%More (Y/n/=)");
				key=ny_get_answer("YN=\n\r");
				od_disp_str("\r            \r");
				//if(key=='\n' || key=='\r') key='Y';
				//od_putch(key);
				cnt2=1;
				if(key=='N')
					return;
				else if(key=='=')
					nonstop=TRUE;


				//od_printf("\n\r");
				scr_file=ShareFileOpen(SCR_FILENAME,"rb");
				if(scr_file != NULL)
					fseek(scr_file,filepos,SEEK_SET);
			}
		}
		if(scr_file != NULL)
			fclose(scr_file);

		if(rip)
			od_send_file("frame1.rip");
		WaitForEnter();
	}
}


/*current player listing of a certain sex*/
void
ListPlayersS(sex_type psex) {
	FILE *scr_file;
	scr_rec user_scr;
	INT32 filepos;
	INT16 cnt,rnk;
	INT16 nonstop=FALSE;
	char key;

	od_printf("\n\r\n\r");
	ny_clr_scr();

	if(rip) {
		od_send_file("frame.rip");
		od_send_file("frame3.rip");
	}
	ny_send_menu(LIST,"");
	/*      od_printf("`bright green`P`green`layer `bright green`L`green`isting `bright green`F`green`or `bright red`N`red`ew `bright red`Y`red`ork `bright red`2008\n\r\n\r");
		od_printf("`green`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
		od_printf("`cyan`Rank: Lvl: Name:                     Points:              S: T:\n\r");*/
	ch_game_d();
	if ((scr_file=ShareFileOpen(SCR_FILENAME,"rb"))!=NULL) {
		cnt=5;
		rnk=1;

		while (scr_file != NULL && ny_fread(&user_scr, sizeof(scr_rec), 1, scr_file) == 1) {
			if (user_scr.sex==psex) {
				PlInfo(&user_scr,cnt);

				cnt++;
				if (scr_file != NULL && nonstop==FALSE && cnt%od_control.user_screen_length==0) {
					filepos=ftell(scr_file);
					fclose(scr_file);


					ny_disp_emu("`%More (Y/n/=)");
					key=ny_get_answer("YN=\n\r");
					od_disp_str("\r            \r");
					//if(key=='\n' || key=='\r') key='Y';
					//od_putch(key);
					cnt=1;
					if(key=='N')
						return;
					else if(key=='=')
						nonstop=TRUE;

					//              WaitForEnter();

					//od_printf("\n\r");
					scr_file=ShareFileOpen(SCR_FILENAME,"rb");
					if(scr_file != NULL)
						fseek(scr_file,filepos,SEEK_SET);
				}
			}
			rnk++;
		}
		if(scr_file != NULL)
			fclose(scr_file);

		if(rip)
			od_send_file("frame1.rip");
		WaitForEnter();
	}
}




/*current alive player listing*/
void
ListPlayersA() {
	FILE *scr_file;
	scr_rec user_scr;
	INT32 filepos;
	INT16 cnt,rnk;
	INT16 nonstop=FALSE;
	char key;


	od_printf("\n\r\n\r");
	ny_clr_scr();
	if(rip)
		od_send_file("frame.rip");
	if(rip)
		od_send_file("frame3.rip");
	ny_send_menu(CONSIOUS,"");
	/*      od_printf("`bright green`C`green`onsious `bright green`P`green`layer `bright green`L`green`isting `bright green`F`green`or `bright red`N`red`ew `bright red`Y`red`ork `bright red`2008\n\r\n\r");
		od_printf("`green`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
		od_printf("`cyan`Rank: Lvl: Name:                     Points:              S: T:\n\r");*/
	ch_game_d();
	if ((scr_file=ShareFileOpen(SCR_FILENAME,"rb"))!=NULL) {
		cnt=5;
		rnk=1;
		while (scr_file != NULL && ny_fread(&user_scr, sizeof(scr_rec), 1, scr_file) == 1) {
			if (user_scr.alive==ALIVE) {

				PlInfo(&user_scr,cnt);


				cnt++;
				if (scr_file != NULL && nonstop==FALSE && cnt%od_control.user_screen_length==0) {
					filepos=ftell(scr_file);
					fclose(scr_file);

					ny_disp_emu("`%More (Y/n/=)");
					key=ny_get_answer("YN=\n\r");
					od_printf("\r            \r");
					//if(key=='\n' || key=='\r') key='Y';
					//od_putch(key);
					cnt=1;
					if(key=='N')
						return;
					else if(key=='=')
						nonstop=TRUE;

					//              WaitForEnter();

					//od_printf("\n\r\n\r");
					scr_file=ShareFileOpen(SCR_FILENAME,"rb");
					if(scr_file != NULL)
						fseek(scr_file,filepos,SEEK_SET);
				}
			}
			rnk++;
		}
		if(scr_file != NULL)
			fclose(scr_file);

		if(rip)
			od_send_file("frame1.rip");
		WaitForEnter();
	}
}





/*online player listing*/
void
WhosOnline(void) {
	FILE *scr_file;
	scr_rec user_scr;
	INT32 filepos;
	INT16 cnt,rnk;
	INT16 nonstop=FALSE;
	char key;


	if(oneframe==FALSE || rip==FALSE) {
		od_printf("\n\r\n");
		ny_clr_scr();
		if(rip)
			od_send_file("frame.rip");
		if(rip)
			od_send_file("frame3.rip");
	} else {
		od_disp_str("\n\r!|e\n\r");
		od_send_file("frame3.rip");
	}

	ny_send_menu(ONLINE,"");

	/*  od_printf("`bright green`O`green`nline `bright green`P`green`layer `bright green`L`green`isting `bright green`F`green`or `bright red`N`red`ew `bright red`Y`red`ork `bright red`2008\n\r\n\r");
	  od_printf("`green`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
	  od_printf("`cyan`Rank: Lvl: Name:                     Points:              S: T:\n\r");*/
	ch_game_d();
	if ((scr_file=ShareFileOpen(SCR_FILENAME,"rb"))!=NULL) {
		cnt=5;
		rnk=1;
		while (scr_file != NULL && ny_fread(&user_scr, sizeof(scr_rec), 1, scr_file) == 1) {
			if (user_scr.online==TRUE) {
				PlInfo(&user_scr,cnt);

				cnt++;
				if (nonstop==FALSE && cnt%od_control.user_screen_length==0) {
					filepos=ftell(scr_file);
					fclose(scr_file);

					ny_disp_emu("`%More (Y/n/=)");
					key=ny_get_answer("YN=\n\r");
					od_printf("\r            \r");
					//if(key=='\n' || key=='\r') key='Y';
					//od_putch(key);
					cnt=1;
					if(key=='N')
						return;
					else if(key=='=')
						nonstop=TRUE;

					//        WaitForEnter();

					//od_printf("\n\r");
					scr_file=ShareFileOpen(SCR_FILENAME,"rb");
					if(scr_file != NULL)
						fseek(scr_file,filepos,SEEK_SET);
				}
			}
			rnk++;
		}
		if(scr_file != NULL)
			fclose(scr_file);

		if(rip)
			od_send_file("frame1.rip");
		WaitForEnter();
	}
}


void
EnterStreet(void) {
	char key;
	//      int intval;

	//      printf("\n\r14\n\r");
	read_IGMs();
	//      printf("\n\r20\n\r");
	read_fight_IGMs();
	//      printf("\n\rEND DEBUG!\n\r");
	noevents=FALSE;

	//if conversion from v0.05a or lower
	if(cur_user.throwing_ability==0) {
		cur_user.rocks=2;
		switch(cur_user.nation) {
			case HEADBANGER:
				cur_user.throwing_ability=10 + (cur_user.level*1.33);
				cur_user.punch_ability=10 + (cur_user.level*1.33);
				cur_user.kick_ability=20 + (cur_user.level*1.33);
				break;
			case HIPPIE:
				cur_user.throwing_ability=20 + (cur_user.level*1.33);
				cur_user.punch_ability=10 + (cur_user.level*1.33);
				cur_user.kick_ability=10 + (cur_user.level*1.33);
				break;
			case BIG_FAT_DUDE:
				cur_user.throwing_ability=4 + (cur_user.level*1.33);
				cur_user.punch_ability=18 + (cur_user.level*1.33);
				cur_user.kick_ability=18 + (cur_user.level*1.33);
				break;
			case CRACK_ADDICT:
				cur_user.throwing_ability=14 + (cur_user.level*1.33);
				cur_user.punch_ability=13 + (cur_user.level*1.33);
				cur_user.kick_ability=13 + (cur_user.level*1.33);
				break;
			case PUNK:
				cur_user.throwing_ability=11 + (cur_user.level*1.33);
				cur_user.punch_ability=20 + (cur_user.level*1.33);
				cur_user.kick_ability=11 + (cur_user.level*1.33);
				break;
		}
	}

	if (cur_user.hunger==100) {   //user dying of hunger
		od_printf("\n\r\n\r");
		ny_clr_scr();
		//if(rip) od_send_file("frame.rip");

		//od_printf("`bright red`B`red`oy yer dying of hunger... If you don't eat today yer gonna die!\n\r");
		ny_line(53,0,1);
		//        ny_disp_emu("`@B`4oy yer dying of hunger... If you don't eat today yer gonna die!\n\r");

		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		food_ops();
	}
	else if (cur_user.hunger>=50) {  //user 50% or more hungry....cmon eat
		od_printf("\n\r\n\r");
		ny_clr_scr();

		//if(rip) od_send_file("frame.rip");
		//        od_printf("`bright red`B`red`oy yer hungry....you should eat something not to starve...\n\r");
		ny_line(52,0,1);
		//        ny_disp_emu("`@B`4oy yer hungry....you should eat something not to starve...\n\r");

		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		food_ops();
	}

	if(expert==3)
		expert=1;
	do {
		//key=central_park(); //display menu and get the key pressed
		if(!ibbs)
			key=callmenu("SCPFDAGUMIXEBHYWLRNOKQ!?\n\r",CENTRAL_PARK,342,FALSE);
		else
			key=callmenu("SCPFDAGUMIXEBHYWLRNO*KQ!?\n\r",CENTRAL_PARK_IB,342,FALSE);
		while (expert>0 && key=='?') {
			expert+=20;
			if(!ibbs)
				key=callmenu("SCPFDAGUMIXEBHYWLRNOKQ!?\n\r",CENTRAL_PARK,342,FALSE);
			else
				key=callmenu("SCPFDAGUMIXEBHYWLRNO*KQ!?\n\r",CENTRAL_PARK_IB,342,FALSE);
			expert-=20;
		}

		//go to the right ops function
		switch (key) {
			case 'D':               //buy sell use drugs
				if(expert==3)
					expert=1;
				drug_ops();
				if(expert==3)
					expert=1;
				break;
			case 'B':        //money and stuff...bank...
				if(expert==3)
					expert=1;
				money_ops();
				if(expert==3)
					expert=1;
				break;
			case 'R':         //rest somewhere.....
				if(expert==3)
					expert=1;
				rest_ops();
				if(expert==3)
					expert=1;
				break;
			case 'Y':       // display user stats
				DisplayStats();
				WaitForEnter();
				break;
			case 'L':         // list active players
				ListPlayers();
				break;
			case 'O':         // IGM (other stuff)
				if(expert==3)
					expert=1;
				IGM_ops();
				if(expert==3)
					expert=1;
				break;
			case 'M':         /// mail
				if(expert==3)
					expert=1;
				mail_ops();
				if(expert==3)
					expert=1;
				break;
			case 'W':         // Who's Online
				WhosOnline();
				break;
			case 'G':         // get laid.....
				if(expert==3)
					expert=1;
				get_laid_ops();
				if(expert==3)
					expert=1;
				break;
			case 'F':          // eat some food
				if(expert==3)
					expert=1;
				food_ops();
				if(expert==3)
					expert=1;
				break;
			case 'A':          //weapon store
				if(expert==3)
					expert=1;
				guns_ops();
				if(expert==3)
					expert=1;
				break;
			case '*':
				if (ibbs) {          //InterBBS
					if(expert==3)
						expert=1;
					ibbs_ops();
					if(expert==3)
						expert=1;
				}
				break;
			case 'H':         //healing (drug rehab, heal woulnds, cure std's)
				if(expert==3)
					expert=1;
				healing_ops();
				if(expert==3)
					expert=1;
				break;
			case 'E':
				if(expert==3)
					expert=1;
				evil_ops(&cur_user);
				if(expert==3)
					expert=1;
				break;
			case 'S':             //street fights
				if(expert==3)
					expert=1;
				fight_ops(&cur_user);
				if(expert==3)
					expert=1;
				break;
			case 'K':             //suicide
				ny_line(54,2,0);
				key=ny_get_answer("YN");
				if(!rip)
					od_printf("%c\n\r",key);
				else if(key=='Y') {
					od_disp_str("\n\r!|10000((*REALLY want to DIE?::y@Yes,n@No))|#|#|#\n\r");
					key=od_get_answer("YN");
				}
				if (key=='Y')
					Die(4);
				else if(rip)
					no_rip_m=1;
				break;
			case 'I':
				if(registered==FALSE) {
					ny_disp_emu("`%\n\r\n\rUNREGISTERED!!!\n\r\n\rCannot do this!!!\n\r");
					WaitForEnter();
				} else {
					if(expert==3)
						expert=1;
					change_info();
					if(expert==3)
						expert=1;
				}
				break;
			case '!':               //instructions
				od_printf("\n\r\n\r");
				ny_clr_scr();
				ch_game_d();
				if(rip)
					od_send_file("frame.rip");
				if(rip)
					od_send_file("frame3.rip");
				if(clean_mode==TRUE)
					ny_send_file("nyinstrc.asc");
				else
					ny_send_file("nyinstr.asc");
				if(rip)
					od_send_file("frame1.rip");
				WaitForEnter();
				break;
			case 'C': // copfights
				if(registered==FALSE) {
					ny_disp_emu("`%\n\r\n\rUNREGISTERED!!!\n\r\n\rCannot do this!!!\n\r");
					WaitForEnter();
				} else {
					if(expert==3)
						expert=1;
					copfight_ops(&cur_user);
					if(expert==3)
						expert=1;
				}
				break;
			case 'P': // Player fights
				if(expert==3)
					expert=1;
				p_fight_ops(&cur_user,&nCurrentUserNumber);
				if(expert==3)
					expert=1;
				break;
			case 'N':  //newz
				newz_ops();
				break;
			case 'X':
				if(expert==3)
					expert=1;
				if(rip) {
					expert=0;
				} else {
					expert+=1;//expert;
					if(expert>2)
						expert=0;
					if (expert==2) { //expert
						ny_line(55,2,1);
					} else if(expert==1) { //regular
						ny_line(56,2,1);
					} else { //novice
						ny_line(341,2,1);
					}
					WaitForEnter();
				}
				break;
			case 'Q':
				ny_line(57,2,0);
				key=ny_get_answer("YN");
				if(!rip)
					od_printf("%c\n\r",key);
				else
					od_disp_str("\n\r");
				if (key=='Y')
					key='Q';
				else if(!rip)
					no_rip_m=1;
				break;
			case 'U':           //use a-bomb to win game
				//only if user got one
				if (cur_user.arm!=A_BOMB) {
					ny_line(58,2,1);
					if(!rip)
						WaitForEnter();
					else {
						od_get_answer("\n\r");
						no_rip_m=1;
					}
				} else {
					ny_line(59,2,0);
					//                  od_printf("`bright red`\n\r\n\rR`red`eally do it? (`bright red`Y`red`/`bright red`N`red`)");
					key=ny_get_answer("YN");
					if(!rip)
						od_printf("%c\n\r",key);
					else
						od_disp_str("\n\r");
					if (key=='Y') {
						cur_user.alive=DEAD;
						wrt_sts();
						od_printf("\n\r\n\r");
						ny_clr_scr();
						od_send_file("win");
						ny_line(1,1,0);
						od_get_answer("\n\r");

						od_printf("\n\r\n\r");
						ny_clr_scr();

						ny_line(295,2,0);
						//                    od_printf("\n\r\n\r`bright`YOU WON THE GAME!!!!! With
						od_printf(D_Num(cur_user.points));
						ny_line(296,0,2);
						ny_send_menu(WIN,"");
						/*  od_printf("\n\r\n\r\n\r`white`Of course blowing up New York with an A-Bomb did wonders to your body\n\r");
						    od_printf("basically what I'm saying you died... but you completed yer mission ...\n\r");
						    od_printf("just think of all the people that died with you ...\n\r\n\r");*/
						news_post("`%WON `4THE GAME .... `@LOOSERS ....",cur_user.name,"",3);
						AddBestPlayer();
						if(ibbs)
							AddBestPlayerIB();
						ny_line(1,1,0);
						od_get_answer("\n\r");
						od_exit(10,FALSE);
					}
				}
				break;
		}
	} while (key!='Q');
}




char *D_Num(INT16 num) {
	char temp[8];
	INT16 cnt,cnt2,len,sign=0;
	if (num<0) {
		sign=1;
		str[0]='-';
	}

	sprintf(temp,"%d",num);

	len=strlen(temp);

	if (len<=4) {
		strcpy(str,temp);
		return str;
	}

	cnt2=sign;
	for(cnt=sign;cnt<len-1;cnt++) {
		str[cnt2]=temp[cnt];
		if ( ( (INT16)((len-cnt-.00099)/3) *3) == (len-cnt-1) ) {
			cnt2++;
			str[cnt2]=',';
		}
		cnt2++;
	}
	str[cnt2]=temp[cnt];
	str[cnt2+1]=0;
	return str;
}



char *D_Num(INT32 num) {
	char temp[15];
	INT16 cnt,cnt2,len,sign=0;
	if (num<0) {
		sign=1;
		str[0]='-';
	}

	sprintf(temp,"%ld",num);

	len=strlen(temp);

	if (len<=4) {
		strcpy(str,temp);
		return str;
	}

	cnt2=sign;
	for(cnt=sign;cnt<len-1;cnt++) {
		str[cnt2]=temp[cnt];
		if ( ((INT16)((len-cnt-.00099)/3)*3) == (len-cnt-1) ) {
			cnt2++;
			str[cnt2]=',';
		}
		cnt2++;
	}
	str[cnt2]=temp[cnt];
	str[cnt2+1]=0;

	return str;
}

char *D_Num(DWORD num) {
	char temp[14];
	INT16 cnt,cnt2,len;

	sprintf(temp,"%lu",num);

	len=strlen(temp);

	if (len<=4) {
		strcpy(str,temp);
		return str;
	}

	cnt2=0;
	for(cnt=0;cnt<len-1;cnt++) {
		str[cnt2]=temp[cnt];
		if ( ((INT16)((len-cnt-.00099)/3)*3) == (len-cnt-1) ) {
			cnt2++;
			str[cnt2]=',';
		}
		cnt2++;
	}
	str[cnt2]=temp[cnt];
	str[cnt2+1]=0;

	return str;
}






void
points_raise(DWORD raise) {       // when is time for next level      0   1    2    3    4    5    6     7     8     9     10    11    12    13    14    15    16    17     18     19      //20
	DWORD level_seal[LEVELS] = {500,1020,2100,3700,6100,9100,12820,17300,22580,28700,35700,43620,52500,62380,73300,85300,98420,112700,128180,144900};//,162900};
	char c_dir,key;

	c_dir=c_dir_g;

	cur_user.points+=raise * DrgPtsCoef();


	if(cur_user.points>30000 && registered==FALSE) {
		if(rip) {
			scr_save();
			ny_clr_scr();
			od_send_file("frame.rip");
			od_send_file("frame1.rip");
		}

		ny_disp_emu("`%\n\r\n\rUNREGISTERED!!!\n\r\n\r");
		ny_disp_emu("You reached 30,000 points, and this game is `$UNREGISTERED`%!\n\r");
		ny_disp_emu("This nag screen will appear to you every time your points raise\n\r");
		ny_disp_emu("And you can't get into level 10, until the game is registered\n\r");
		ny_disp_emu("Otherwise nothing will happen, please tell the sysop to register the game!\n\r");
		WaitForEnter();
		if(rip)
			scr_res();
	}


	if(registered==FALSE && cur_user.level>9)
		cur_user.level=9;



	while (cur_user.level<20 && level_seal[cur_user.level]<=cur_user.points) {
		//while ( (pow( 10,((double)cur_user.level+1)/2) ) * 25 )<=cur_user.points) {
		if(cur_user.level>=9 && registered==FALSE)
			break;

		cur_user.level++;
		switch(cur_user.nation) {
			case HEADBANGER:
				cur_user.strength=(5 * pow(1.3,cur_user.level)+.5);
				cur_user.defense=(4 * pow(1.3,cur_user.level)+.5);
				break;
			case HIPPIE:
				cur_user.strength=(4 * pow(1.3,cur_user.level)+.5);
				cur_user.defense=(4 * pow(1.3,cur_user.level)+.5);
				break;
			case BIG_FAT_DUDE:
				cur_user.strength=(3 * pow(1.3,cur_user.level)+.5);
				cur_user.defense=(6 * pow(1.3,cur_user.level)+.5);
				break;
			case CRACK_ADDICT:
				cur_user.strength=(4 * pow(1.3,cur_user.level)+.5);
				cur_user.defense=(5 * pow(1.3,cur_user.level)+.5);
				break;
			case PUNK:
				cur_user.strength=(6 * pow(1.3,cur_user.level)+.5);
				cur_user.defense=(3 * pow(1.3,cur_user.level)+.5);
				break;
		}
		cur_user.hitpoints+=.7 * cur_user.maxhitpoints;
		cur_user.maxhitpoints=(15 * pow(1.7,cur_user.level)+.5);

		ny_line(60,2,0);
		//Yer level has been raised to level
		if(!rip) {
			od_printf("%d`red`!\n\r",(INT16)cur_user.level);
			ny_line(437,1,0);
			key=od_get_answer("TKP");
			od_printf("%c\n\r",key);
		} else {
			od_printf("%d\\!::^M",(INT16)cur_user.level);
			od_putch('@');
			od_printf("OK))|#|#|#\n\r");
			od_get_answer("\n\r");
			ny_line(437,1,1);
			key=od_get_answer("TKP");
		}
		switch(key) {
			case 'T':
				cur_user.throwing_ability+=4;
				break;
			case 'K':
				cur_user.kick_ability+=4;
				break;
			case 'P':
				cur_user.punch_ability+=4;
				break;
		}

		cur_user.sex_today++;
	}


	wrt_sts();

	SortScrFile(nCurrentUserNumber,0);

	if(c_dir==0)
		ch_game_d();
	else
		ch_flag_d();

	od_control.od_update_status_now=TRUE;
}


void
points_loose(DWORD lost) {
	char c_dir;

	c_dir=c_dir_g;
	if (cur_user.points<lost)
		cur_user.points=0;
	else
		cur_user.points-=lost;

	wrt_sts();
	SortScrFileB(nCurrentUserNumber);
	if(c_dir==0)
		ch_game_d();
	else
		ch_flag_d();
	od_control.od_update_status_now=TRUE;
}


double
DrgPtsCoef(void) {
	if (cur_user.nation!=HIPPIE) {
		if (cur_user.drug==POT)
			return (cur_user.drug_high/1000.0) + 1.0;
		if (cur_user.drug==HASH)
			return (cur_user.drug_high/500.0) + 1.0;
		if (cur_user.drug==LSD)
			return (cur_user.drug_high/250.0) + 1.0;
		return 1.0;
	}
	if (cur_user.drug==POT)
		return (cur_user.drug_high/900.0) + 1.0;
	if (cur_user.drug==HASH)
		return (cur_user.drug_high/450.0) + 1.0;
	if (cur_user.drug==LSD)
		return (cur_user.drug_high/225.0) + 1.0;
	return 1.01;

}

char
callmenu(const char allowed[],menu_t menu,INT16 menu_line,char figst) {
	char key;

	od_clear_keybuffer();

	if(no_rip_m!=1 || !rip) {

		od_printf("\n\r\n\r");
		if (expert!=2 && expert!=3)
			ny_clr_scr();

		if (expert==2 || expert==3) {
			ny_line(menu_line,0,1);
			od_printf(allowed);
			ny_line(36,0,0);
			od_printf("%d ",od_control.caller_timelimit);
			ny_line(37,0,0);
			//od_printf("`bright green`%s`bright blue`E`blue`nter `bright blue`Y`blue`er `bright blue`C`blue`ommand (%d mins)`bright blue`>",allowed,od_control.caller_timelimit);
			if(rip)
				od_disp_str("\n\r!|10000$HKEYON$|#|#|#\n\r");
			key=ny_get_answer(allowed);
			if(rip)
				od_disp_str("\n\r!|10000$HKEYOFF$|#|#|#\n\r");
		} else {
			if(figst && !rip) {
				ny_line(menu_line,0,2);
				disp_fig_stats();
			}
			key=ny_send_menu(menu,allowed);

			if(!rip) {
				ny_line(36,0,0);
				od_printf("%d ",od_control.caller_timelimit);
				ny_line(37,0,0);
			}

			if (key==0) {
				if(rip)
					od_disp_str("\n\r!|10000$HKEYON$|#|#|#\n\r");
				key= ny_get_answer(allowed);
				if(rip)
					od_disp_str("\n\r!|10000$HKEYOFF$|#|#|#\n\r");
			}

		}
	} else {
		no_rip_m=0;
		od_disp_str("\n\r!|10000$HKEYON$|#|#|#\n\r");
		key=ny_get_answer(allowed);
		od_disp_str("\n\r!|10000$HKEYOFF$|#|#|#\n\r");
	}
	if (menu!=CENTRAL_PARK && menu!=CENTRAL_PARK_IB && (key=='\n' || key=='\r'))
		key='Q';
	if(!rip)
		od_putch(key);
	if(expert==1)
		expert=3;
	return key;
}




void
rest_ops(void) {
	char key,s_key;
	DWORD intval;
	DWORD max;

	do {
		key=callmenu("CERYQ?\n\r",REST,343,FALSE);
		while (expert>0 && key=='?') {
			expert+=10;
			key=callmenu("CERYQ?\n\r",REST,343,FALSE);
			expert-=10;
		}


		if (key=='Y') {
			DisplayStats();
			WaitForEnter();
		} else if (key=='C') {
			if (cur_user.rest_where!=NOWHERE && cur_user.rest_where!=MOTEL) {

				ny_line(61,2,0);
				//            od_printf("\n\r\n\r`bright red`Y`red`er already staying at: `bright green`");
				//if (cur_user.rest_where==MOTEL) od_printf("C`green`heap `bright green`M`green`otel");
				if(!rip) {
					if (cur_user.rest_where==REG_HOTEL)
						od_printf("R`green`egular `bright green`H`green`otel");
					else if (cur_user.rest_where==EXP_HOTEL)
						od_printf("E`green`xpensive `bright green`H`green`otel");
				} else {
					if (cur_user.rest_where==REG_HOTEL)
						od_printf(" Regular Hotel::@OK))|#|#|#\n\r");
					else if (cur_user.rest_where==EXP_HOTEL)
						od_printf(" Expensive Hotel::@OK))|#|#|#\n\r");
				}

				ny_line(62,2,0);
				//            od_printf("\n\r`bright red`W`red`anna check outta there (no money back)? (`bright red`Y`red`/`bright red`N`red`)");

				s_key=ny_get_answer("YN");
				od_putch(s_key);
				if (s_key=='Y')
					goto Cheap_motel_ok;
				if(rip)
					no_rip_m=1;
			} else {
Cheap_motel_ok:
				;
				intval=(cur_user.level*20)+20;

				ny_line(297,2,0);
				//od_printf("\n\r\n\r`bright red`I`red`t will cost ya `bright red`
				od_printf(D_Num(intval));
				ny_line(298,0,0);
				//            `red` per day.",intval);// still wanna do it(`bright red`Y`red`/`bright red`N`red`)",intval);
				max=cur_user.money/intval;
				if (max>255)
					max=255;
				if(rip)
					od_get_answer("\n\r");
				ny_line(299,2,0);
				//            od_printf("\n\r`bright red`Y`red`ou can afford `bright red`
				od_printf(D_Num(max));
				ny_line(300,0,2);
				if(rip) {
					od_get_answer("\n\r");
					//          `red` days!\n\r",D_Num(max));
					od_send_file("input.rip");
				}
				ny_line(301,0,0);
				//od_printf("`bright green`H`green`ow mady days ya wanna stay here? (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`
				od_printf(D_Num(max));
				ny_disp_emu("`2):");

				max=get_val((DWORD)0,(DWORD)max);
				if (max==0) {
					//if (intval>cur_user.money) {
					//  if (cur_user.rest_where==NOWHERE) {
					//    od_printf("`bright red`Y`red`a cant afford that...\n\r");
					//  } else {

					if (cur_user.rest_where!=NOWHERE) {
						ny_line(63,1,0);
						//od_printf("`bright red`Y`red`e're still staying at: ");
						if(!rip) {
							if (cur_user.rest_where==MOTEL)
								od_printf("C`green`heap `bright green`M`green`otel");
							else if (cur_user.rest_where==REG_HOTEL)
								od_printf("R`green`egular `bright green`H`green`otel");
							else if (cur_user.rest_where==EXP_HOTEL)
								od_printf("E`green`xpensive `bright green`H`green`otel");
						} else {
							if (cur_user.rest_where==MOTEL)
								od_printf("Cheap Motel::@OK))|#|#|#");
							else if (cur_user.rest_where==REG_HOTEL)
								od_printf(" Regular Hotel::@OK))|#|#|#\n\r");
							else if (cur_user.rest_where==EXP_HOTEL)
								od_printf(" Expensive Hotel::@OK))|#|#|#\n\r");
						}

						od_printf("\n\r");
						if (rip==FALSE)
							WaitForEnter();
					}
				} else {

					ny_line(64,1,1);
					//od_printf("`bright red`O`red`k dude you got it...\n\r");
					money_minus(intval*max);
					if (cur_user.rest_where==MOTEL)
						cur_user.hotel_paid_fer+=max;
					else
						cur_user.hotel_paid_fer=max;
					cur_user.rest_where=MOTEL;

					od_exit(10,FALSE);
				}
			}
		} else if (key=='R') {
			if (cur_user.rest_where!=NOWHERE && cur_user.rest_where!=REG_HOTEL) {

				ny_line(61,2,0);
				//od_printf("\n\r\n\r`bright red`Y`red`er already staying at: ");
				if(!rip) {
					if (cur_user.rest_where==MOTEL)
						od_printf("C`green`heap `bright green`M`green`otel");
					else if (cur_user.rest_where==EXP_HOTEL)
						od_printf("E`green`xpensive `bright green`H`green`otel");
				} else {
					if (cur_user.rest_where==MOTEL)
						od_printf("Cheap Motel::@OK))|#|#|#");
					else if (cur_user.rest_where==EXP_HOTEL)
						od_printf(" Expensive Hotel::@OK))|#|#|#\n\r");
				}
				ny_line(62,2,0);
				//od_printf(" `bright red`W`red`anna check outta there (no money back)? (`bright red`Y`red`/`bright red`N`red`)");

				s_key=ny_get_answer("YN");
				od_putch(s_key);
				if (s_key=='Y')
					goto Reg_hotel_ok;
				if(rip)
					no_rip_m=1;
			} else {
Reg_hotel_ok:
				;
				intval=(cur_user.level*40)+40;

				ny_line(297,2,0);
				od_printf(D_Num(intval));
				ny_line(298,0,0);
				//            od_printf("\n\r\n\r`bright red`I`red`t will cost ya `bright red`%d`red` per day.",intval);// still wanna do it(`bright red`Y`red`/`bright red`N`red`)",intval);
				max=cur_user.money/intval;
				if(rip)
					od_get_answer("\n\r");
				if (max>255)
					max=255;
				ny_line(299,2,0);
				od_printf(D_Num(max));
				ny_line(300,0,2);
				//            od_printf("\n\r`bright red`Y`red`ou can afford `bright red`%s`red` days!\n\r",D_Num(max));
				if(rip) {
					od_get_answer("\n\r");
					od_send_file("input.rip");
				}
				ny_line(301,0,0);
				od_printf(D_Num(max));
				ny_disp_emu("`2):");
				//od_printf("`bright green`H`green`ow mady days ya wanna stay here? (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`%d`green`):",max);

				max=get_val((DWORD)0,(DWORD)max);
				if (max==0) {
					//if (intval>cur_user.money) {
					//  if (cur_user.rest_where==NOWHERE) {
					//    od_printf("`bright red`Y`red`a cant afford that...\n\r");
					//  } else {

					if (cur_user.rest_where!=NOWHERE) {
						ny_line(63,1,0);
						//od_printf("`bright red`Y`red`e're still staying at: ");
						if(!rip) {
							if (cur_user.rest_where==MOTEL)
								od_printf("C`green`heap `bright green`M`green`otel");
							else if (cur_user.rest_where==REG_HOTEL)
								od_printf("R`green`egular `bright green`H`green`otel");
							else if (cur_user.rest_where==EXP_HOTEL)
								od_printf("E`green`xpensive `bright green`H`green`otel");
						} else {
							if (cur_user.rest_where==MOTEL)
								od_printf("Cheap Motel::@OK))|#|#|#");
							else if (cur_user.rest_where==REG_HOTEL)
								od_printf(" Regular Hotel::@OK))|#|#|#\n\r");
							else if (cur_user.rest_where==EXP_HOTEL)
								od_printf(" Expensive Hotel::@OK))|#|#|#\n\r");
						}


						od_printf("\n\r");
						if (rip==FALSE)
							WaitForEnter();
					}
				} else {

					ny_line(64,1,1);
					//od_printf("`bright red`O`red`k dude you got it...\n\r");
					money_minus(intval*max);
					if (cur_user.rest_where==REG_HOTEL)
						cur_user.hotel_paid_fer+=max;
					else
						cur_user.hotel_paid_fer=max;
					//cur_user.rest_where=REG_HOTEL;
					//cur_user.hotel_paid_fer=max;
					od_exit(10,FALSE);
				}
			}
		} else if (key=='E') {
			if (cur_user.rest_where!=NOWHERE && cur_user.rest_where!=EXP_HOTEL) {

				ny_line(61,2,0);
				//od_printf("\n\r\n\r`bright red`Y`red`er already staying at: ");
				if(!rip) {
					if (cur_user.rest_where==MOTEL)
						od_printf("C`green`heap `bright green`M`green`otel");
					else if (cur_user.rest_where==REG_HOTEL)
						od_printf("R`green`egular `bright green`H`green`otel");
				} else {
					if (cur_user.rest_where==MOTEL)
						od_printf("Cheap Motel::@OK))|#|#|#");
					else if (cur_user.rest_where==REG_HOTEL)
						od_printf(" Regular Hotel::@OK))|#|#|#\n\r");
				}
				ny_line(62,2,0);
				//od_printf(" `bright red`W`red`anna check outta there (no money back)? (`bright red`Y`red`/`bright red`N`red`)");

				s_key=ny_get_answer("YN");
				od_putch(s_key);
				if (s_key=='Y')
					goto Exp_hotel_ok;
				if(rip)
					no_rip_m=1;
			} else {
Exp_hotel_ok:
				;
				intval=(cur_user.level*110)+110;

				ny_line(297,2,0);
				od_printf(D_Num(intval));
				ny_line(298,0,0);
				//od_printf("\n\r\n\r`bright red`I`red`t will cost ya `bright red`%d`red` per day.",intval);// still wanna do it(`bright red`Y`red`/`bright red`N`red`)",intval);
				max=cur_user.money/intval;
				if(rip)
					od_get_answer("\n\r");
				if (max>255)
					max=255;
				ny_line(299,2,0);
				od_printf(D_Num(max));
				ny_line(300,0,2);
				if(rip) {
					od_get_answer("\n\r");
					//od_printf("\n\r`bright red`Y`red`ou can afford `bright red`%s`red` days!\n\r",D_Num(max));
					od_send_file("input.rip");
				}

				ny_line(301,0,0);
				od_printf(D_Num(max));
				ny_disp_emu("`2):");
				//od_printf("`bright green`H`green`ow mady days ya wanna stay here? (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`%d`green`):",max);

				max=get_val((DWORD)0,(DWORD)max);
				if (max==0) {
					//if (intval>cur_user.money) {
					//  if (cur_user.rest_where==NOWHERE) {
					//    od_printf("`bright red`Y`red`a cant afford that...\n\r");
					//  } else {

					if (cur_user.rest_where!=NOWHERE) {
						ny_line(63,1,0);
						//od_printf("`bright red`Y`red`e're still staying at: ");
						if(!rip) {
							if (cur_user.rest_where==MOTEL)
								od_printf("C`green`heap `bright green`M`green`otel");
							else if (cur_user.rest_where==REG_HOTEL)
								od_printf("R`green`egular `bright green`H`green`otel");
							else if (cur_user.rest_where==EXP_HOTEL)
								od_printf("E`green`xpensive `bright green`H`green`otel");
						} else {
							if (cur_user.rest_where==MOTEL)
								od_printf("Cheap Motel::@OK))|#|#|#");
							else if (cur_user.rest_where==REG_HOTEL)
								od_printf(" Regular Hotel::@OK))|#|#|#\n\r");
							else if (cur_user.rest_where==EXP_HOTEL)
								od_printf(" Expensive Hotel::@OK))|#|#|#\n\r");
						}
						od_printf("\n\r");

						if (rip==FALSE)
							WaitForEnter();
					}
				} else {

					ny_line(64,1,1);
					//od_printf("`bright red`O`red`k dude you got it...\n\r");
					money_minus(intval*max);
					if (cur_user.rest_where==EXP_HOTEL)
						cur_user.hotel_paid_fer+=max;
					else
						cur_user.hotel_paid_fer=max;
					//cur_user.rest_where=EXP_HOTEL;
					//cur_user.hotel_paid_fer=max;
					od_exit(10,FALSE);
				}
			}
		}
	} while (key!='Q');
}



void
DisplayBest(void) {
	//  ffblk ffblk;
	best_rec_type best_rec;
	FILE *justfile;
	INT16 cnt;

	ch_game_d();
	if (fexist(BESTTEN_FILENAME)) {
		justfile=ShareFileOpen(BESTTEN_FILENAME,"rb");
		if(justfile != NULL) {
			od_printf("\n\r\n\r");
			ny_clr_scr();
			if(rip)
				od_send_file("frame.rip");
			if(rip)
				od_send_file("frame1.rip");
			ny_send_menu(TEN_BEST,"");
			cnt=1;
			/*    od_printf("\n\r`bright red`T`red`en `bright red`B`red`est ... `bright red`W`red`inners\n\r\n\r");
		    	od_printf("`bright blue`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
		    	od_printf("`cyan`Rank:    Name:                     Points:");*/
			while(ny_fread(&best_rec,sizeof(best_rec),1,justfile)==1) {
				od_printf("`bright red`%-2d       `bright green`",cnt);
				ny_disp_emu(best_rec.name,25);
				od_printf(" `bright red`%s\n\r",D_Num(best_rec.points));
				cnt++;
			}
			//    od_printf("\n\r");
			ny_line(399,0,1);
			//ny_send_menu(BLUE_LINE,"");
			//od_printf("\n\r`bright blue`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
			fclose(justfile);
		}
		ny_line(1,1,0);
		//    ny_disp_emu("\n\r`@Smack [ENTER] to go on.");
		od_get_answer("\n\r");
	}
}

void
DisplayBestIB(void) {
	//  ffblk ffblk;
	ibbs_best_rec_type best_rec;
	FILE *justfile;
	INT16 cnt;

	ch_game_d();
	if (fexist(IBBS_BESTTEN_FILENAME)) {
		justfile=ShareFileOpen(IBBS_BESTTEN_FILENAME,"rb");
		if(justfile != NULL) {
			od_printf("\n\r\n\r");
			ny_clr_scr();
			if(rip)
				od_send_file("frame.rip");
			if(rip)
				od_send_file("frame1.rip");
			ny_send_menu(TEN_BEST_IBBS,"");
			cnt=1;
			/*    od_printf("\n\r`bright red`T`red`en `bright red`B`red`est ... `bright red`W`red`inners\n\r\n\r");
		    	od_printf("`bright blue`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
		    	od_printf("`cyan`Rank:    Name:                     Points:");*/
			while(ny_fread(&best_rec,sizeof(best_rec),1,justfile)==1) {
				//findmeagain
				//      od_printf("`bright red`%-2d       `bright green`",cnt);
				od_set_attrib(0x0a);
				ny_disp_emu(best_rec.name,25);
				od_printf(" `bright red`%-13s ",D_Num(best_rec.points));
				od_set_attrib(0x02);
				ny_disp_emu(LocationOf(best_rec.location));
				od_disp_str("\n\r");
				cnt++;
			}
			//    od_printf("\n\r");
			ny_line(399,0,1);
			//ny_send_menu(BLUE_LINE,"");
			//od_printf("\n\r`bright blue`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
			fclose(justfile);
		}
		ny_line(1,1,0);
		//    ny_disp_emu("\n\r`@Smack [ENTER] to go on.");
		od_get_answer("\n\r");
	}
}



void
AddBestPlayer(void) {
	//  ffblk ffblk;
	best_rec_type best_rec,best_rec2;
	FILE *justfile;
	INT16 cnt=0,len_of_list;
	strcpy(best_rec2.name,cur_user.name);
	best_rec2.points=cur_user.points;

	ch_game_d();

	ny_remove(SENTBESTTEN_FILENAME);

	if (fexist(BESTTEN_FILENAME)) {
		justfile=ShareFileOpen(BESTTEN_FILENAME,"r+b");
		if(justfile != NULL) {
			len_of_list=filelength(fileno(justfile))/sizeof(best_rec_type) +1;
			if (len_of_list>10)
				len_of_list=10;
		}

		while (justfile != NULL && cnt<10 && ny_fread(&best_rec,sizeof(best_rec_type),1,justfile)==1) {
			if (cur_user.points>=best_rec.points) {
				//fseek(justfile,(INT32)cnt*sizeof(best_rec_type),SEEK_SET);
				//ny_fwrite(&best_rec2,sizeof(best_rec_type),1,justfile);
				strcpy(best_rec.name,best_rec2.name);
				best_rec.points=best_rec2.points;
				//cnt++;
				while (cnt<len_of_list) {
					fseek(justfile,(INT32)cnt*sizeof(best_rec_type),SEEK_SET);
					ny_fread(&best_rec2,sizeof(best_rec_type),1,justfile);
					fseek(justfile,(INT32)cnt*sizeof(best_rec_type),SEEK_SET);
					ny_fwrite(&best_rec,sizeof(best_rec_type),1,justfile);
					strcpy(best_rec.name,best_rec2.name);
					best_rec.points=best_rec2.points;
					cnt++;
				}
				fclose(justfile);

				if(ibbs==FALSE) {
					ny_line(302,2,1);
					//od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r");

					//        WaitForEnter();
					if(rip)
						od_get_answer("\n\r");
				}
				return;
			}
			cnt++;
		}
		if(justfile != NULL)
			fclose(justfile);
		if (cnt<10) {
			justfile=ShareFileOpen(BESTTEN_FILENAME,"a+b");
			if(justfile != NULL) {
				ny_fwrite(&best_rec2,sizeof(best_rec_type),1,justfile);
				fclose(justfile);
			}

			if(ibbs==FALSE) {
				ny_line(302,2,1);
				//od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r");

				if(rip)
					od_get_answer("\n\r");
			}
			return;
		} else {

			if(ibbs==FALSE) {
				ny_line(303,2,1);
				//od_printf("\n\r\n\r`bright red`Y`red`ou didn't make the `bright`BEST`red` list.\n\r`bright red`T`red`here were better.\n\r");

				//    WaitForEnter();
				if(rip)
					od_get_answer("\n\r");
			}
			return;
		}
	}
	justfile=ShareFileOpen(BESTTEN_FILENAME,"wb");
	if(justfile != NULL) {
		ny_fwrite(&best_rec2,sizeof(best_rec_type),1,justfile);
		fclose(justfile);
	}

	if(ibbs==FALSE) {
		ny_line(302,2,1);
		//    od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r
		if(rip)
			od_get_answer("\n\r");
		ny_line(304,0,1);
		if(rip)
			od_get_answer("\n\r");
	}
	//`bright red`A`red`ctually ya were the first to do it.\n\r");

	// WaitForEnter();
	return;
}

void
AddBestPlayerIB(void) {
	//  ffblk ffblk;
	ibbs_best_rec_type best_rec,best_rec2;
	FILE *justfile;
	INT16 cnt=0,len_of_list;
	strcpy(best_rec2.name,cur_user.name);
	strcpy(best_rec.location,IBBSInfo.szThisNodeAddress);
	best_rec2.points=cur_user.points;


	ch_game_d();
	if (fexist(IBBS_BESTTEN_FILENAME)) {
		justfile=ShareFileOpen(IBBS_BESTTEN_FILENAME,"r+b");
		if(justfile != NULL) {
			len_of_list=filelength(fileno(justfile))/sizeof(ibbs_best_rec_type) +1;
			if (len_of_list>10)
				len_of_list=10;
		}

		/*check if the same record exists*/
		while (justfile != NULL && cnt<10 && ny_fread(&best_rec,sizeof(ibbs_best_rec_type),1,justfile)==1) {
			if (cur_user.points==best_rec.points &&
			        strcmp(best_rec.location,IBBSInfo.szThisNodeAddress)==0 &&
			        strcmp(best_rec.name,cur_user.name)==0) {
				/*if it already exists quit this function*/
				fclose(justfile);
				return;
			}
		}

		/*rewind the file to beginning*/
		if(justfile != NULL)
			fseek(justfile,0,SEEK_SET);

		while (justfile != NULL && cnt<10 && ny_fread(&best_rec,sizeof(ibbs_best_rec_type),1,justfile)==1) {
			if (cur_user.points>=best_rec.points) {
				//fseek(justfile,(INT32)cnt*sizeof(best_rec_type),SEEK_SET);
				//ny_fwrite(&best_rec2,sizeof(best_rec_type),1,justfile);
				strcpy(best_rec.location,best_rec2.location);
				strcpy(best_rec.name,best_rec2.name);
				best_rec.points=best_rec2.points;
				//cnt++;
				while (cnt<len_of_list) {
					fseek(justfile,(INT32)cnt*sizeof(ibbs_best_rec_type),SEEK_SET);
					ny_fread(&best_rec2,sizeof(ibbs_best_rec_type),1,justfile);
					fseek(justfile,(INT32)cnt*sizeof(ibbs_best_rec_type),SEEK_SET);
					ny_fwrite(&best_rec,sizeof(ibbs_best_rec_type),1,justfile);
					strcpy(best_rec.location,best_rec2.location);
					strcpy(best_rec.name,best_rec2.name);
					best_rec.points=best_rec2.points;
					cnt++;
				}
				fclose(justfile);

				ny_line(302,2,1);
				//od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r");

				//      WaitForEnter();
				if(rip)
					od_get_answer("\n\r");
				return;
			}
			cnt++;
		}
		if(justfile != NULL)
			fclose(justfile);
		if (cnt<10) {
			justfile=ShareFileOpen(IBBS_BESTTEN_FILENAME,"a+b");
			if(justfile != NULL) {
				ny_fwrite(&best_rec2,sizeof(ibbs_best_rec_type),1,justfile);
				fclose(justfile);
			}

			ny_line(302,2,1);
			//od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r");

			if(rip)
				od_get_answer("\n\r");
			return;
		} else {

			ny_line(303,2,1);
			//od_printf("\n\r\n\r`bright red`Y`red`ou didn't make the `bright`BEST`red` list.\n\r`bright red`T`red`here were better.\n\r");

			//  WaitForEnter();
			if(rip)
				od_get_answer("\n\r");
			return;
		}
	}
	justfile=ShareFileOpen(IBBS_BESTTEN_FILENAME,"wb");
	if(justfile != NULL) {
		ny_fwrite(&best_rec2,sizeof(ibbs_best_rec_type),1,justfile);
		fclose(justfile);
	}

	ny_line(302,2,1);
	//  od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r
	if(rip)
		od_get_answer("\n\r");
	ny_line(304,0,1);
	if(rip)
		od_get_answer("\n\r");
	//`bright red`A`red`ctually ya were the first to do it.\n\r");

	// WaitForEnter();
	return;
}


void
AddBestPlayerInIB(char *name,DWORD points) {
	//  ffblk ffblk;
	best_rec_type best_rec,best_rec2;
	FILE *justfile;
	INT16 cnt=0,len_of_list;
	strcpy(best_rec2.name,name);
	best_rec2.points=points;


	ch_game_d();
	if (fexist(IBBS_BESTTEN_FILENAME)) {
		justfile=ShareFileOpen(IBBS_BESTTEN_FILENAME,"r+b");
		if(justfile != NULL) {
			len_of_list=filelength(fileno(justfile))/sizeof(best_rec_type) +1;
			if (len_of_list>10)
				len_of_list=10;
		}

		while (justfile != NULL && cnt<10 && ny_fread(&best_rec,sizeof(best_rec_type),1,justfile)==1) {

			if (points>=best_rec.points) {
				//fseek(justfile,(INT32)cnt*sizeof(best_rec_type),SEEK_SET);
				//ny_fwrite(&best_rec2,sizeof(best_rec_type),1,justfile);
				strcpy(best_rec.name,best_rec2.name);
				best_rec.points=best_rec2.points;
				//cnt++;
				while (cnt<len_of_list) {
					fseek(justfile,(INT32)cnt*sizeof(best_rec_type),SEEK_SET);
					ny_fread(&best_rec2,sizeof(best_rec_type),1,justfile);
					fseek(justfile,(INT32)cnt*sizeof(best_rec_type),SEEK_SET);
					ny_fwrite(&best_rec,sizeof(best_rec_type),1,justfile);
					strcpy(best_rec.name,best_rec2.name);
					best_rec.points=best_rec2.points;
					cnt++;
				}
				fclose(justfile);

				if(ibbs_maint_i==FALSE) {
					ny_line(302,2,1);
					//od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r");

					//        WaitForEnter();
					if(rip)
						od_get_answer("\n\r");
				}
				return;
			}
			cnt++;

		}

		if(justfile != NULL)
			fclose(justfile);
		if (cnt<10) {
			justfile=ShareFileOpen(IBBS_BESTTEN_FILENAME,"a+b");
			if(justfile != NULL) {
				ny_fwrite(&best_rec2,sizeof(best_rec_type),1,justfile);
				fclose(justfile);
			}

			if(ibbs_maint_i==FALSE) {
				ny_line(302,2,1);
				//od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r");

				if(rip)
					od_get_answer("\n\r");
			}
			return;
		} else {

			if(ibbs_maint_i==FALSE) {
				ny_line(303,2,1);
				//od_printf("\n\r\n\r`bright red`Y`red`ou didn't make the `bright`BEST`red` list.\n\r`bright red`T`red`here were better.\n\r");

				//    WaitForEnter();
				if(rip)
					od_get_answer("\n\r");
			}
			return;
		}
	}
	justfile=ShareFileOpen(IBBS_BESTTEN_FILENAME,"wb");
	if(justfile != NULL) {
		ny_fwrite(&best_rec2,sizeof(best_rec_type),1,justfile);
		fclose(justfile);
	}

	if(ibbs_maint_i==FALSE) {
		ny_line(302,2,1);
		//    od_printf("\n\r\n\r`bright red`Y`red`ou made the `bright`BEST`red` list.\n\r
		if(rip)
			od_get_answer("\n\r");
		ny_line(304,0,1);
		//Actually ya were the first to do it.
		if(rip)
			od_get_answer("\n\r");
	}
	return;
}


INT16
strzcmp(const char str1[], const char str2[]) {
	char temp1[40],
	temp2[40];

	strcpy(temp1,str1);
	strcpy(temp2,str2);
	strlwr(temp1);
	strlwr(temp2);

	//  if (strstr(temp1,temp2)!=NULL) return 0; //found
	if (strstr(temp2,temp1)!=NULL)
		return 0;

	return 1;
}



void
mail_ops(void) {
	char key;
	char hand[25];
	char omg[51];
	char numstr[26];
	char line[80],ovr[80];
	FILE *justfile;
	FILE *msg_file;
	scr_rec urec;
	user_rec u2rec;
	INT16 unum,ret,cnt;
	mail_idx_type mail_idx;
	INT32 fillen;

	ovr[0]=0;
	line[0]=0;

	do {
		key=callmenu("SAROCQ?\n\r",MAIL,344,FALSE);
		while (expert>0 && key=='?') {
			expert+=10;
			key=callmenu("SAROCQ?\n\r",MAIL,344,FALSE);
			expert-=10;
		}


		if (key=='Y') {
			DisplayStats();
			WaitForEnter();
		} else if (key=='C') {

			od_printf("\n\r\n\r");
			if(rip) {
				od_send_file("frame.rip");
				od_send_file("frame1.rip");
			}
			ny_send_menu(COLORS_HELP,"");
			/*          od_printf("\n\r\n\r`bright red`S`red`o ya'd like to use colors, huh?\n\r\n\r");
				    od_printf("`bright red`W`red`ell its easy. `bright red`T`red`hey can be used in mail and the stuff you say.\n\r\n\r");
				    od_printf("`bright red`Y`red`ou simply type the `bright`");
				    od_putch('`');
				    od_printf("`red` character and one of the following:\n\r");
				    ny_disp_emu("`11`22`33`44`55`66`77`88`99`00`!!`@@`##`$$`%%");
				    od_printf("\n\r\n\r`bright`");
				    od_putch('`');
				    od_printf("$Colors ");
				    od_putch('`');
				    od_printf("0RULE\n\r\n\rWould look like:\n\r\n\r");
				    ny_disp_emu("`$Colors `0RULE");
				    ny_disp_emu("\n\r\n\r`@N`4OTE: `@S`4ame as in `@LORD\n\r");*/

			WaitForEnter();

		} else if (key=='S') {

			mail_idx.flirt=0;

			if(rip)
				od_send_file("tframe2.rip");
			ny_line(305,2,0);
			//od_printf("\n\r\n\r`bright red`W`red`ho's mailbox do ya wanna fill (`bright red`full`red` or `bright red`partial`red` name):`bright green`");

			od_input_str(hand,24,' ',255);
			ny_un_emu(hand);
			//od_printf("\n\r");
			unum=0;
			ret=1;
			if (hand[0]!=0) {
				do {
					ch_game_d();
					justfile=ShareFileOpen(SCR_FILENAME,"rb");
					if(justfile != NULL) {
						fseek(justfile,sizeof(scr_rec) * (INT32)unum,SEEK_SET);
						do {
							ret=ny_fread(&urec,sizeof(scr_rec),1,justfile);
							unum++;
						} while ((strzcmp(hand,ny_un_emu(urec.name,numstr)) || urec.user_num==nCurrentUserNumber) && ret==1);
						fclose(justfile);
					}
				} while (ret==1 && askifuser(urec.name)==FALSE);
			}
			if (ret!=1) {

				ny_line(133,1,1);
				//            od_printf("\n\r`bright red`G`red`ot no idea who you mean ...");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else if (hand[0]!=0) {
				if (urec.sex!=cur_user.sex && cur_user.sex_today>0) {

					ny_line(306,1,0);
					//              ny_disp_emu("\n\r`@A`4sk `@");
					if(!rip)
						ny_disp_emu(urec.name);
					else
						od_disp_str(ny_un_emu(urec.name,numstr));
					ny_line(307,0,0);
					//              ny_disp_emu("`4 to have sex with you?(`@Y`4/`@N`4)");

					key=ny_get_answer("YN");
					if(!rip)
						od_printf("%c\n\r",key);
					else
						od_disp_str("\n\r");
					if (key=='Y') {
						if (cur_user.sex_today<=0) {

							ny_line(118,1,1);
							//od_printf("\n\r\n\r`bright`You already used up all your sex turns today ...\n\r");

							if(!rip)
								WaitForEnter();
							else
								od_get_answer("\n\r");
							od_printf("\n\r");
						} else {
							mail_idx.flirt=1;
							cur_user.sex_today--;
						}
					}
				}
				ch_game_d();
				justfile=ShareFileOpen(USER_FILENAME,"rb");
				if(justfile != NULL) {
					fseek(justfile,sizeof(user_rec) * (INT32)urec.user_num,SEEK_SET);
					ny_fread(&u2rec,sizeof(user_rec),1,justfile);
					fclose(justfile);
				}

				ny_line(308,1,1);
				//od_printf("\n\r`bright red`O`red`k type yer message now (`bright red`/s`red`=save `bright red`/a`red`=abort):\n\r");

				ch_flag_d();
				sprintf(numstr,"u%07d.tmg", nCurrentUserNumber);
				justfile=ShareFileOpen(numstr,"wb");
				if(justfile != NULL) {
					cnt= -1;
					do {
						cnt++;
						get_line(ovr,line,ovr,TRUE);
						ny_fwrite(&line,80,1,justfile);
					} while ((line[0]!='/' && (line[1]!='s' || line[1]!='S')) && (line[0]!='/' && (line[1]!='a' || line[1]!='A')));
					fclose(justfile);
				}

				if ((line[1]=='s' || line[1]=='S') && (cnt>0 || mail_idx.flirt==1)) {
					ny_line(135,0,1);
					//od_printf("\b\b`bright red`S`red`aving...\n\r");
					mail_idx.length=cnt;
					strcpy(mail_idx.sender,cur_user.name);
					strcpy(mail_idx.senderI,cur_user.bbsname);
					strcpy(mail_idx.recver,urec.name);
					strcpy(mail_idx.recverI,u2rec.bbsname);
					mail_idx.afterquote=0;
					mail_idx.deleted=FALSE;
					mail_idx.sender_sex=cur_user.sex;
					mail_idx.ill=cur_user.std;
					mail_idx.inf=cur_user.std_percent;
					ch_flag_d();
					justfile=ShareFileOpen(numstr,"rb");
					ch_game_d();
					msg_file=ShareFileOpen(MAIL_FILENAME,"a+b");
					if(justfile != NULL && msg_file != NULL) {
						fillen=filelength(fileno(msg_file));
						fillen/=80;
						mail_idx.location=fillen;
						while ((cnt--)>0) {
							ny_fread(&line,80,1,justfile);
							ny_fwrite(&line,80,1,msg_file);
						}
						fclose(msg_file);
						fclose(justfile);
					}
					//sprintf(numstr,"del u%07d.tmg");
					//system(numstr);
					ch_flag_d();
					ny_remove(numstr);
					ch_game_d();
					msg_file=ShareFileOpen(MAIL_INDEX,"a+b");
					if(msg_file != NULL) {
						ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,msg_file);
						fclose(msg_file);
					}
					if(single_node==FALSE && urec.online==TRUE) {
						ch_flag_d();
						sprintf(numstr,"u%07d.omg",urec.user_num);
						omg[0]=27;
						omg[1]=0;
						justfile=ShareFileOpen(numstr,"a+b");
						if(justfile != NULL) {
							ny_fwrite(&omg,51,1,justfile);
							ny_fwrite(&cur_user.name,25,1,justfile);
							fclose(justfile);
						}
					}
				} else {
					ny_line(136,0,1);
					//              od_printf("\b\b`bright red`A`red`borted...\n\r");
					//sprintf(numstr,"del u%07d.tmg");
					//system(numstr);
					if(mail_idx.flirt==1)
						cur_user.sex_today++;
					ch_flag_d();
					ny_remove(numstr);
				}
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		} else if (key=='R') {
			read_mail();
			if(ibbs)
				read_ibmail();
		} else if (key=='A') {

			if(rip)
				od_send_file("texti.rip");
			ny_line(309,2,1);
			//        od_printf("\n\r\n\r`bright red`S`red`ay what:\n\r");
			ny_line(310,0,1);
			//        od_printf("|-----------------------------------------------------------------------------|\n\r`bright green`");

			od_input_str(line,79,' ',255);
			if(line[0]!=0) {
				if(dobad)
					dobadwords(line);
				news_post(line,cur_user.name,"",1);
				ny_line(311,2,1);
				//            od_printf("\n\r\n\r`bright red`P`red`osted...\n\r");
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		} else if (key=='O') {
			if (single_node==TRUE) {
				//This is a Single Node only Game!!!!
				ny_line(338,2,1);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {

				if(rip)
					od_send_file("texti.rip");
				ny_line(312,2,0);
				//Who do ya want to bother (full or partial):

				od_input_str(hand,24,' ',255);
				ny_un_emu(hand);
				od_printf("\n\r");
				unum=0;
				if (hand[0]!=0) {
					do {
						ch_game_d();
						justfile=ShareFileOpen(SCR_FILENAME,"rb");
						if(justfile != NULL) {
							fseek(justfile,sizeof(scr_rec) * (INT32)unum,SEEK_SET);
ask_online_again:
							;
							do {
								ret=ny_fread(&urec,sizeof(scr_rec),1,justfile);
								unum++;
							} while (strzcmp(hand,ny_un_emu(urec.name,numstr)) && ret==1);
							if (urec.online==FALSE && ret==1)
								goto ask_online_again;
							fclose(justfile);
						}
					} while (ret==1 && askifuser(urec.name)==FALSE);
				}
				unum--;
				if (ret!=1) {

					ny_line(133,0,1);
					//Got no idea who you mean ...

					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				} else if (hand[0]!=0) {

					ny_line(313,2,0);
					ny_line(314,1,1);
					//Ok type what u wanna say now:
					//+------------------------------------------------+

					od_input_str(omg,50,' ',255);
					if(dobad)
						dobadwords(omg);

					ch_flag_d();
					sprintf(numstr,"u%07d.omg",urec.user_num);
					justfile=ShareFileOpen(numstr,"a+b");
					if(justfile != NULL) {
						ny_fwrite(&omg,51,1,justfile);
						ny_fwrite(&cur_user.name,25,1,justfile);
						fclose(justfile);
					}
					ny_line(315,2,1);
					//Sent!
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				}
			}
		}
	} while (key!='Q');
}







void
read_mail(void) {
	FILE *justfile;
	FILE *njustfile;
	INT32 fillen;
	INT32 filepos;
	mail_idx_type mail_idx;
	INT16 intval;
	char numstr[36];
	char key;
	char ovr[80],line[80];
	INT16 cnt,cnt2;
	INT16 nonstop;
	user_rec urec;
	char omg[2];
	INT16 unum;
	DWORD mon, med;

	filepos=0;

	if(rip)
		od_send_file("tframe1.rip");

	do {
		ch_game_d();
		justfile=ShareFileOpen(MAIL_INDEX,"rb");
		if(justfile != NULL) {
			fseek(justfile,filepos,SEEK_SET);
			intval=ny_fread(&mail_idx,sizeof(mail_idx_type),1,justfile);

			ny_line(316,2,0);
			//Searching ...
			while (intval==1 && (strcmp(mail_idx.recverI,cur_user.bbsname)!=0 || mail_idx.deleted==TRUE))
				intval=ny_fread(&mail_idx,sizeof(mail_idx_type),1,justfile);
			filepos=ftell(justfile);
			fclose(justfile);
		}
		if (intval==1) {
			nonstop=FALSE;

			ny_line(317,2,0);
			//Mail from:
			ny_disp_emu(mail_idx.sender);
			ny_line(318,0,0);
			//To:
			ny_disp_emu(mail_idx.recver);
			od_printf("\n\r");
			cnt=0;
			cnt2=2;
			if (mail_idx.flirt<=998) {
				ch_game_d();
				justfile=ShareFileOpen(MAIL_FILENAME,"rb");
				if(justfile != NULL) {
					fseek(justfile,(INT32)mail_idx.location * 80,SEEK_SET);
					od_printf("`green`");
					while (justfile != NULL && cnt<mail_idx.length) {
						ny_fread(&line,80,1,justfile);
						if (cnt<mail_idx.afterquote) {
							ny_disp_emu("`9>`2");
							ny_disp_emu(line);
							od_printf("\n\r");
						} else {
							od_set_attrib(0x0a); //bright green
							ny_disp_emu(line);
							od_printf("\n\r");
						}
						cnt++;
						cnt2++;
						if (nonstop==FALSE && cnt2%od_control.user_screen_length==0) {
							fclose(justfile);

							ny_disp_emu("`%More (Y/n/=)");
							key=ny_get_answer("YN=\n\r");
							ny_disp_emu("\r            \r");
							cnt2=1;
							if(key=='N')
								return;
							else if(key=='=')
								nonstop=TRUE;

							ch_game_d();
							justfile=ShareFileOpen(MAIL_FILENAME,"rb");
							if(justfile != NULL)
								fseek(justfile,(INT32)(mail_idx.location+cnt) * 80,SEEK_SET);
						}
					}
					if(justfile != NULL)
						fclose(justfile);
				}

				if (mail_idx.flirt==1) {

					ny_disp_emu("\n\r`@");
					ny_disp_emu(mail_idx.sender);
					ny_line(319,0,0);
					//wants to have sex with ya Ok?
					ny_line(320,1,0);
					//[O]=OK (sex) R=reply D=delete I=ignore >

					key=ny_get_answer("ORDI\n\r");
					if (key=='\n' || key=='\r')
						key='O';
				} else {

					ny_line(321,1,0);
					//R=reply [D]=delete I=ignore >

					key=ny_get_answer("RDI\n\r");
					if (key=='\n' || key=='\r')
						key='D';
				}
				od_printf("%c\n\r",key);
			} else if (mail_idx.flirt==999) {

				ny_disp_emu("\n\r`@");
				ny_disp_emu(mail_idx.sender);
				ny_line(322,0,1);
				//agreed and had sex with ya ...

				cur_user.since_got_laid=0;
				points_raise((INT32)50*(cur_user.level+1));
				illness(mail_idx.ill,mail_idx.inf);
				ch_game_d();
				justfile=ShareFileOpen(MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(mail_idx_type),SEEK_SET);
					mail_idx.deleted=TRUE;
					ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
					fclose(justfile);
				}
				WaitForEnter();
				key=-1;
			} else if (mail_idx.flirt==1000) {

				ny_disp_emu("\n\r`@");
				ny_disp_emu(mail_idx.sender);
				ny_line(323,0,1);
				//raped you!!

				illness(mail_idx.ill,mail_idx.inf);
				ch_game_d();
				justfile=ShareFileOpen(MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(mail_idx_type),SEEK_SET);
					mail_idx.deleted=TRUE;
					ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
					fclose(justfile);
				}
				WaitForEnter();
				key=-1;
			} else if (mail_idx.flirt==1001) {

				ny_disp_emu("\n\r`@");
				ny_disp_emu(mail_idx.sender);
				ny_line(324,0,1);
				//defeated you!!

				ch_game_d();
				justfile=ShareFileOpen(MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(mail_idx_type),SEEK_SET);
					mail_idx.deleted=TRUE;
					ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
					fclose(justfile);
				}
				WaitForEnter();
				key=-1;
			} else if (mail_idx.flirt==1002) {
				ch_game_d();
				justfile=ShareFileOpen(MAIL_FILENAME,"rb");
				if(justfile != NULL) {
					fseek(justfile,(INT32)mail_idx.location * 80,SEEK_SET);
					fread(&mon,sizeof(DWORD),1,justfile);
					fclose(justfile);
				}

				med=ULONG_MAX-mon;
				if (med<=cur_user.money)
					mon=med;


				ny_disp_emu("\n\r`@");
				ny_disp_emu(mail_idx.sender);
				ny_line(419,0,0);
				od_printf("%s",D_Num(mon));
				ny_line(420,0,1);

				cur_user.money+=mon;

				wrt_sts();

				justfile=ShareFileOpen(MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(mail_idx_type),SEEK_SET);
					mail_idx.deleted=TRUE;
					ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
					fclose(justfile);
				}
				WaitForEnter();
				key=-1;
			}

			if (key=='D' || key=='R') {
				if (key=='D')
					ny_line(325,1,2);
				//Deleting...
				ch_game_d();
				justfile=ShareFileOpen(MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(mail_idx_type),SEEK_SET);
					mail_idx.deleted=TRUE;
					ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
					fclose(justfile);
				}
				if(key=='D' && rip==TRUE)
					od_get_answer("\n\r");
			}
			if (key=='O') {
				/*sex with playeres goes here*/
				if (cur_user.sex_today<=0) {

					ny_line(118,1,1);
					//You already used up all your sex turns today ...
					ny_line(326,0,1);
					//Try again tomorrow....

					WaitForEnter();
				} else {
					ch_game_d();
					justfile=ShareFileOpen(MAIL_INDEX,"r+b");
					if(justfile != NULL) {
						fseek(justfile,filepos-sizeof(mail_idx_type),SEEK_SET);
						mail_idx.deleted=TRUE;
						ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
						fclose(justfile);
					}

					ny_line(327,2,0);
					//You just had sex with
					ny_disp_emu(mail_idx.sender);
					ny_disp_emu("`4 ...\n\r");

					cur_user.since_got_laid=0;
					cur_user.sex_today--;
					points_raise((INT32)50*(cur_user.level+1));

					desease ill;
					INT16 inf;

					inf=mail_idx.inf;
					ill=mail_idx.ill;

					strcpy(mail_idx.recver,mail_idx.sender);
					strcpy(mail_idx.sender,cur_user.name);
					strcpy(mail_idx.recverI,mail_idx.senderI);
					strcpy(mail_idx.senderI,cur_user.bbsname);
					mail_idx.flirt=999;
					mail_idx.deleted=FALSE;
					mail_idx.location=0;
					mail_idx.length=0;
					mail_idx.afterquote=0;
					mail_idx.ill=cur_user.std;
					mail_idx.inf=cur_user.std_percent;
					mail_idx.sender_sex=cur_user.sex;
					ch_game_d();
					justfile=ShareFileOpen(MAIL_INDEX,"a+b");
					if(justfile != NULL) {
						ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
						fclose(justfile);
					}
					if(single_node==FALSE) {

						unum=0;

						ch_game_d();
						justfile=ShareFileOpen(USER_FILENAME,"rb");
						if(justfile != NULL) {
							while(justfile != NULL && ny_fread(&urec,sizeof(user_rec),1,justfile)==1) {
								if(strcmp(urec.bbsname,mail_idx.recverI)==0) {
									fclose(justfile);
									ch_flag_d();
									sprintf(numstr,"u%07d.on",unum);
									if(fexist(numstr)) {
										sprintf(numstr,"u%07d.omg",unum);
										omg[0]=27;
										omg[1]=1;
										justfile=ShareFileOpen(numstr,"a+b");
										if(justfile != NULL) {
											ny_fwrite(&omg,51,1,justfile);
											ny_fwrite(&cur_user.name,25,1,justfile);
											fclose(justfile);
										}
									}
									goto found_the_guy;
								}
								unum++;
							}
							if(justfile != NULL)
								fclose(justfile);
						}
found_the_guy:
						;
					}
					illness(ill,inf);
					WaitForEnter();
				}
			}
			if (key=='R') {
				mail_idx.flirt=0;
				if (mail_idx.sender_sex!=cur_user.sex && cur_user.sex_today>0) {

					ny_line(328,1,0);
					//Ask
					if(!rip)
						ny_disp_emu(mail_idx.sender);
					else
						od_disp_str(ny_un_emu(mail_idx.sender,numstr));
					ny_line(329,0,0);
					//to have sex with you?(Y/N)

					key=ny_get_answer("YN");
					if(!rip)
						od_printf("%c\n\r",key);
					else
						od_disp_str("\n\r");
					if (key=='Y') {
						if (cur_user.sex_today<=0) {

							ny_line(118,1,1);
							//You already used up all your sex turns today ...

							WaitForEnter();
							od_printf("\n\r");
						} else {
							mail_idx.flirt=1;
							cur_user.sex_today--;
						}
					}
				}

				ny_line(330,1,0);
				//Quote message? (Y/N)

				key=ny_get_answer("YN");
				if(!rip)
					od_printf("%c\n\r",key);
				else
					od_disp_str("\n\r");
				ch_flag_d();
				sprintf(numstr,"u%07d.tmg",nCurrentUserNumber);
				njustfile=ShareFileOpen(numstr,"wb");
				if(njustfile != NULL) {
					ny_line(308,1,1);
					//Ok type yer message now (/s=save /a=abort):
					if (key=='N') {
						mail_idx.afterquote=0;
					} else {
						ch_game_d();
						justfile=ShareFileOpen(MAIL_FILENAME,"rb");
						if(justfile != NULL) {
							cnt= mail_idx.afterquote;
							fseek(justfile,(INT32)(mail_idx.location+cnt) * 80,SEEK_SET);
							while (cnt<mail_idx.length) {
								ny_fread(&line,80,1,justfile);
								ny_fwrite(&line,80,1,njustfile);
								ny_disp_emu("`9>`2");
								ny_disp_emu(line);
								ny_disp_emu("\n\r");
								cnt++;
							}
							fclose(justfile);
						}
						mail_idx.afterquote=cnt-mail_idx.afterquote;
					}

					cnt=-1;
					ovr[0]=0;
					do {
						cnt++;
						get_line(ovr,line,ovr,TRUE);
						ny_fwrite(&line,80,1,njustfile);
					} while ((line[0]!='/' && line[1]!='S') && (line[0]!='/' && line[1]!='A'));
					fclose(njustfile);
				}
				if (line[1]=='s' || line[1]=='S') {
					ny_line(135,0,1);
					//        od_printf("\b\b`bright red`S`red`aving...\n\r");
					cnt+=mail_idx.afterquote;
					mail_idx.length=cnt;
					strcpy(mail_idx.recver,mail_idx.sender);
					strcpy(mail_idx.sender,cur_user.name);
					strcpy(mail_idx.recverI,mail_idx.senderI);
					strcpy(mail_idx.senderI,cur_user.bbsname);
					mail_idx.deleted=FALSE;
					mail_idx.sender_sex=cur_user.sex;
					mail_idx.ill=cur_user.std;
					mail_idx.inf=cur_user.std_percent;
					ch_flag_d();
					njustfile=ShareFileOpen(numstr,"rb");
					ch_game_d();
					justfile=ShareFileOpen(MAIL_FILENAME,"a+b");
					if(njustfile != NULL && justfile != NULL) {
						fillen=filelength(fileno(justfile));
						fillen/=80;
						mail_idx.location=fillen;
						while ((cnt--)>0) {
							ny_fread(&line,80,1,njustfile);
							ny_fwrite(&line,80,1,justfile);
						}
						fclose(justfile);
						fclose(njustfile);
					}
					//sprintf(numstr,"del u%07d.tmg");
					//system(numstr);
					ch_flag_d();
					ny_remove(numstr);
					ch_game_d();
					justfile=ShareFileOpen(MAIL_INDEX,"a+b");
					if(justfile != NULL) {
						ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
						fclose(justfile);
					}

					if(single_node==FALSE) {
						//          scr_rec srec;

						unum=0;

						ch_game_d();
						justfile=ShareFileOpen(USER_FILENAME,"rb");
						if(justfile != NULL) {
							while(justfile != NULL && ny_fread(&urec,sizeof(user_rec),1,justfile)==1) {
								if(strcmp(urec.bbsname,mail_idx.recverI)==0) {
									fclose(justfile);
									ch_flag_d();
									sprintf(numstr,"u%07d.on",unum);
									if(fexist(numstr)) {
										sprintf(numstr,"u%07d.omg",unum);
										omg[0]=27;
										omg[1]=1;
										justfile=ShareFileOpen(numstr,"a+b");
										if(justfile != NULL) {
											ny_fwrite(&omg,51,1,justfile);
											ny_fwrite(&cur_user.name,25,1,justfile);
											fclose(justfile);
										}
									}
									goto found_the_guy_2;
								}
								unum++;
							}
							if(justfile != NULL)
								fclose(justfile);
						}
found_the_guy_2:
						;
					}
				} else {
					ny_line(136,0,1);
					//od_printf("\b\b`bright red`A`red`borted...\n\r");
					//sprintf(numstr,"del u%07d.tmg");
					//system(numstr);
					ch_flag_d();
					ny_remove(numstr);
					if(mail_idx.flirt==1)
						cur_user.sex_today++;
				}
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		}
	} while (intval==1);
	//  printf("\n\r12\n\r");
}

char
*LocationOf(char *address) {
	INT16 iCurrentSystem;
	if(IBBSInfo.paOtherSystem == NULL && IBBSInfo.nTotalSystems != 0)
		return NULL;
	for(iCurrentSystem = 0; iCurrentSystem < IBBSInfo.nTotalSystems;++iCurrentSystem) {
		if(strcmp(IBBSInfo.paOtherSystem[iCurrentSystem].szAddress,address)==0) {
			return IBBSInfo.paOtherSystem[iCurrentSystem].szSystemName;
		}
	}
	return NULL;
}

void
read_ibmail(void) {
	FILE *justfile;
	//  FILE *njustfile;
	//  long fillen;
	INT32 filepos;
	ibbs_mail_type ibmail;
	INT16 intval;
	char numstr[36];
	char key;
	char ovr[80];//,line[80];
	INT16 cnt;//,cnt2;
	//char turnround[4]={'-','\\','|','/'};
	//  int nonstop;
	//  user_rec urec;
	//  char omg[2];
	//  int unum;
	//  ffblk ffblk;
	INT16 num;
	DWORD mon, med;

	filepos=0;

	//  if(rip) od_send_file("tframe1.rip");

	ch_game_d();
	if(!fexist(IBBS_MAIL_INDEX))
		return;

	do {
		ch_game_d();
		justfile=ShareFileOpen(IBBS_MAIL_INDEX,"rb");
		if(justfile != NULL) {
			fseek(justfile,filepos,SEEK_SET);
			intval=ny_fread(&ibmail,sizeof(ibbs_mail_type),1,justfile);
			ny_line(316,2,0);
			while (intval==1 && (strcmp(ibmail.recverI,cur_user.bbsname)!=0 || ibmail.deleted==TRUE))
				intval=ny_fread(&ibmail,sizeof(ibbs_mail_type),1,justfile);
			filepos=ftell(justfile);
			fclose(justfile);
		}
		if (intval==1) {
			//      nonstop=FALSE;
			ny_line(317,2,0);
			ny_disp_emu(ibmail.sender);
			od_printf(" `dark green`(%s)",LocationOf(ibmail.node_s));
			ny_line(318,0,0);
			//ny_disp_emu("  `@T`4o: `0");
			ny_disp_emu(ibmail.recver);
			od_printf("\n\r");
			cnt=0;
			//      cnt2=2;
			if (ibmail.flirt<=998) {
				od_printf("`green`");
				while (cnt<ibmail.quote_length && cnt<10) {
					ny_disp_emu("`9>`2");
					ny_disp_emu(ibmail.lines[cnt]);
					od_printf("\n\r");
					cnt++;
				}
				cnt=0;
				while (cnt<ibmail.length && cnt<10) {
					//od_printf("`bright green`");
					od_set_attrib(0x0a);
					ny_disp_emu(ibmail.lines[cnt+ibmail.quote_length]);
					od_printf("\n\r");
					cnt++;
				}
				if (ibmail.flirt==1) {

					ny_disp_emu("\n\r`@");
					ny_disp_emu(ibmail.sender);
					//      od_printf("\n\r`bright red`%s`red`
					ny_line(319,0,0);
					//        wants to have sex with ya Ok?",mail_idx.sender);
					ny_line(320,1,0);
					//od_printf("\n\r`bright blue`[O]`red`=OK (sex) `bright blue`R`red`=reply `bright blue`D`red`=delete `bright blue`I`red`=ignore `bright blue`>"); //S`red`=stop `bright blue` >");

					key=ny_get_answer("ORDI\n\r");
					if (key=='\n' || key=='\r')
						key='O';
				} else {

					ny_line(321,1,0);
					//        od_printf("\n\r`bright blue`R`red`=reply `bright blue`[D]`red`=delete `bright blue`I`red`=ignore `bright blue`>"); //S`red`=stop `bright blue`>");

					key=ny_get_answer("RDI\n\r");
					if (key=='\n' || key=='\r')
						key='D';
				}
				od_printf("%c\n\r",key);
			} else if (ibmail.flirt==999) {

				ny_disp_emu("\n\r`@");
				ny_disp_emu(ibmail.sender);
				od_printf(" `dark green`(%s)",LocationOf(ibmail.node_s));
				ny_line(322,0,1);
				//      ny_disp_emu("`4 agreed and had sex with ya ...\n\r");

				cur_user.since_got_laid=0;
				points_raise((INT32)50*(cur_user.level+1));
				illness(ibmail.ill,ibmail.inf);
				ch_game_d();
				justfile=ShareFileOpen(IBBS_MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(ibbs_mail_type),SEEK_SET);
					ibmail.deleted=TRUE;
					ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
					fclose(justfile);
				}
				WaitForEnter();
			} else if (ibmail.flirt==1003) {

				if(xp_random(2)==0) {
					if(xp_random(2)==0) {
						ny_disp_emu("\n\r`@");
						ny_disp_emu(ibmail.sender);
						od_printf(" `dark green`(%s)",LocationOf(ibmail.node_s));
						ny_line(458,0,1);
					} else {
						ny_line(459,1,1);
					}
					illness(ibmail.ill,ibmail.inf);
				} else {
					ny_line(460,1,1);
				}
				ch_game_d();
				justfile=ShareFileOpen(IBBS_MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(ibbs_mail_type),SEEK_SET);
					ibmail.deleted=TRUE;
					ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
					fclose(justfile);
				}
				WaitForEnter();
			} else if (ibmail.flirt==1005) {

				if(xp_random(3)==0) {
					if(xp_random(2)==0) {
						ny_disp_emu("\n\r`@");
						ny_disp_emu(ibmail.sender);
						od_printf(" `dark green`(%s)",LocationOf(ibmail.node_s));
						ny_line(463,0,1);
					} else {
						ny_line(464,1,1);
					}
					//        illness(ibmail.ill,ibmail.inf);
					cur_user.hitpoints*=2.0/3.0;
					;
					ny_line(466,1,1);
				} else {
					ny_line(465,1,1);
				}
				ch_game_d();
				justfile=ShareFileOpen(IBBS_MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(ibbs_mail_type),SEEK_SET);
					ibmail.deleted=TRUE;
					ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
					fclose(justfile);
				}
				WaitForEnter();
			} else if (ibmail.flirt==1004) {
				ny_disp_emu("\n\r`@");
				ny_disp_emu(ibmail.sender);
				od_printf(" `dark green`(%s)",LocationOf(ibmail.node_s));
				ny_line(461,0,2);
				ny_line(462,0,1);

				WaitForEnter();

				ch_game_d();
				justfile=ShareFileOpen(IBBS_MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(ibbs_mail_type),SEEK_SET);
					ibmail.deleted=TRUE;
					ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
					fclose(justfile);
				}
				num=*(INT16 *)ibmail.lines[0];

				any_attack_ops(&cur_user,"`@H`4ired `@H`4itman `@F`4ight",ibmail.lines[1],
				               (INT32)xp_random(pow(1.72,(double)cur_user.level+1)*10)+pow(1.7,(double)cur_user.level+1)*10,
				               (INT32)xp_random(pow(1.31,(double)cur_user.level+1)*4)+pow(1.3,(double)cur_user.level+1)*(18.0+num)/10.0,
				               (INT32)xp_random(pow(1.31,(double)cur_user.level+1)*4)+pow(1.3,(double)cur_user.level+1)*(18.0+num)/10.0,
				               (weapon)cur_user.level);

			} else if (ibmail.flirt==1002) {
				mon=*((DWORD *)ibmail.lines[0]);

				med=ULONG_MAX-mon;
				if (med<=cur_user.money)
					mon=med;

				ny_disp_emu("\n\r`@");
				ny_disp_emu(ibmail.sender);
				od_printf(" `dark green`(%s)",LocationOf(ibmail.node_s));
				ny_line(419,0,0);
				od_printf("%s",D_Num(mon));
				ny_line(420,0,1);

				cur_user.money+=mon;

				wrt_sts();

				justfile=ShareFileOpen(IBBS_MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(ibbs_mail_type),SEEK_SET);
					ibmail.deleted=TRUE;
					ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
					fclose(justfile);
				}
				WaitForEnter();
			}

			//    if (key=='S') break;
			if (key=='D' || key=='R') {
				if (key=='D')
					ny_line(325,1,2); //od_printf("\n\r`bright red`D`red`eleting...\n\r\n\r");
				ch_game_d();
				justfile=ShareFileOpen(IBBS_MAIL_INDEX,"r+b");
				if(justfile != NULL) {
					fseek(justfile,filepos-sizeof(ibbs_mail_type),SEEK_SET);
					ibmail.deleted=TRUE;
					//      od_printf("}>}%d{<{",(INT16)ibmail.deleted);
					//      filepos=0;
					ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
					fclose(justfile);
				}

				if(key=='D' && rip==TRUE)
					od_get_answer("\n\r");
			}
			if (key=='O') {
				/*sex with playeres goes here*/
				if (cur_user.sex_today<=0) {

					ny_line(118,1,1);
					//od_printf("\n\r\n\r`bright`You already used up all your sex turns today ...\n\r");
					ny_line(326,0,1);
					//od_printf("\n\rTry again tomorrow....\n\r");

					WaitForEnter();
				} else {
					ch_game_d();
					justfile=ShareFileOpen(IBBS_MAIL_INDEX,"r+b");
					if(justfile != NULL) {
						fseek(justfile,filepos-sizeof(ibbs_mail_type),SEEK_SET);
						ibmail.deleted=TRUE;
						ny_fwrite(&ibmail,sizeof(ibbs_mail_type),1,justfile);
						fclose(justfile);
					}

					ny_line(327,2,0);
					//od_printf("\n\r\n\r`bright red`Y`red`ou just had sex with `bright red`
					ny_disp_emu(ibmail.sender);
					od_printf(" `dark green`(%s)",LocationOf(ibmail.node_s));
					ny_disp_emu("`4 ...\n\r");

					cur_user.since_got_laid=0;
					cur_user.sex_today--;
					points_raise((INT32)50*(cur_user.level+1));

					desease ill;
					INT16 inf;

					inf=ibmail.inf;
					ill=ibmail.ill;

					strcpy(ibmail.recver,ibmail.sender);
					strcpy(ibmail.sender,cur_user.name);
					strcpy(ibmail.node_r,ibmail.node_s);
					strcpy(ibmail.node_s,IBBSInfo.szThisNodeAddress);
					strcpy(ibmail.recverI,ibmail.senderI);
					strcpy(ibmail.senderI,cur_user.bbsname);
					ibmail.flirt=999;
					ibmail.deleted=FALSE;
					//        ibmail.location=0;
					ibmail.length=0;
					ibmail.quote_length=0;
					ibmail.ill=cur_user.std;
					ibmail.inf=cur_user.std_percent;
					ibmail.sender_sex=cur_user.sex;
					sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);
					IBSendMail(&IBBSInfo,&ibmail);
					//IBSend(&IBBSInfo,ibmail.node_r,&ibmail,sizeof(ibbs_mail_type)-(20*81));
					illness(ill,inf);
					WaitForEnter();
				}
			}
			if (key=='R') {
				ibmail.flirt=0;
				if (ibmail.sender_sex!=cur_user.sex && cur_user.sex_today>0) {

					ny_line(328,1,0);
					//        od_printf("\n\r`bright red`A`red`sk `bright red`
					if(!rip)
						ny_disp_emu(ibmail.sender);
					else
						od_disp_str(ny_un_emu(ibmail.sender,numstr));
					ny_line(329,0,0);
					//`red` to have sex with you?(`bright red`Y`red`/`bright red`N`red`)",mail_idx.sender);

					key=ny_get_answer("YN");
					if(!rip)
						od_printf("%c\n\r",key);
					else
						od_disp_str("\n\r");
					if (key=='Y') {
						if (cur_user.sex_today<=0) {

							ny_line(118,1,1);
							//od_printf("\n\r\n\r`bright`You already used up all your sex turns today ...\n\r");

							WaitForEnter();
							od_printf("\n\r");
						} else {
							ibmail.flirt=1;
							cur_user.sex_today--;
						}
					}
				}

				ny_line(330,1,0);
				//od_printf("\n\r`bright red`Q`red`uote message? (`bright red`Y`red`/`bright red`N`red`)");

				key=ny_get_answer("YN");
				if(!rip)
					od_printf("%c\n\r",key);
				else
					od_disp_str("\n\r");

				if(key=='Y') {
					if(ibmail.quote_length>0) {
						for(cnt=0;cnt<ibmail.length;cnt++)
							strcpy(ibmail.lines[cnt],ibmail.lines[cnt+ibmail.quote_length]);
						ibmail.quote_length=ibmail.length;
					} else {
						ibmail.quote_length=ibmail.length;
					}
					while (cnt<ibmail.length) {
						ny_disp_emu("`9>`2");
						ny_disp_emu(ibmail.lines[cnt]);
						ny_disp_emu("\n\r");
						cnt++;
					}
				} else {
					ibmail.quote_length=0;
				}
				ny_line(446,1,1);
				//od_printf("\n\r`bright red`O`red`k type yer message now (`bright red`/s`red`=save `bright red`/a`red`=abort):\n\r");
				cnt=-1;
				ovr[0]=0;
				do {
					cnt++;
					if(cnt<9)
						get_line(ovr,ibmail.lines[cnt+ibmail.quote_length],ovr,TRUE);
					else
						get_line(ovr,ibmail.lines[cnt+ibmail.quote_length],ovr,FALSE);
				} while ((ibmail.lines[cnt+ibmail.quote_length][0]!='/' &&
				          ibmail.lines[cnt+ibmail.quote_length][1]!='S') &&
				         (ibmail.lines[cnt+ibmail.quote_length][0]!='/' &&
				          ibmail.lines[cnt+ibmail.quote_length][1]!='A') &&
				         cnt<9);

				if (ibmail.lines[cnt+ibmail.quote_length][1]=='s' || ibmail.lines[cnt+ibmail.quote_length][1]=='S' || cnt==9) {
					ny_line(135,0,1);
					//        od_printf("\b\b`bright red`S`red`aving...\n\r");
					//cnt+=mail_idx.afterquote;
					ibmail.length=cnt;
					strcpy(ibmail.recver,ibmail.sender);
					strcpy(ibmail.sender,cur_user.name);
					strcpy(ibmail.node_r,ibmail.node_s);
					strcpy(ibmail.node_s,IBBSInfo.szThisNodeAddress);
					strcpy(ibmail.recverI,ibmail.senderI);
					strcpy(ibmail.senderI,cur_user.bbsname);
					ibmail.deleted=FALSE;
					ibmail.sender_sex=cur_user.sex;
					ibmail.ill=cur_user.std;
					ibmail.inf=cur_user.std_percent;
					sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);
					IBSendMail(&IBBSInfo,&ibmail);
					//IBSend(&IBBSInfo,ibmail.node_r,&ibmail,sizeof(ibbs_mail_type)-((20-ibmail.length-ibmail.quote_length)*81));
					//        od_printf(">>%s<<",ibmail.node_r);
				} else {
					ny_line(136,0,1);
					if(ibmail.flirt==1)
						cur_user.sex_today++;
				}
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		}
	} while (intval==1);
}

void
dobadwords(char *s) {
	if(!dobad || badwordscnt==0)
		return;

	char badchars[]="@!#$&?@%*";
	INT16 c;
	INT16 len=strlen(s);
	INT16 tryagain=FALSE;

	char *sm=(char *)malloc(len+1);
	if(sm==0)
		return;

	char *p,*pm;

	do {
		strcpy(sm,s);
		strupr(sm);
		tryagain=FALSE;

		for(INT16 i=0;i<badwordscnt;i++) {
			od_kernal();
			p=sm;
			while((p=strstr(p,badwords[i]))!=0) {
				od_kernal();
				if(p==sm || *(p-1)<'A' || *(p-1)>'Z') {
					c=0;
					pm=(p-sm+s);
					while(*pm!=0 && c<strlen(badwords[i])) {
						*pm=badchars[xp_random(9)];
						pm++;
						c++;
					}
					tryagain=TRUE;
				}
				if(++p==NULL)
					break;
			}
		}
	} while(tryagain);
	free(sm);
}
