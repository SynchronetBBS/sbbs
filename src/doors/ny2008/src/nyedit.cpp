//                        0 - Critical Door error (no fossil, etc.)
//                        1 - Carrier lost, user off-line
//                        2 - Sysop terminated call, user off-line
//                        3 - User time used up, user STILL ON-LINE
//                        4 - Keyboard inactivity timeout, user off-line
//                       10 - User chose to return to BBS
//                       11 - User chose to logoff, user off-line
//                       12 - Critical RAVote error


// include the header
#include "nyedit.h"

// Declare global variables
user_rec cur_user;
char str[25];
INT16 single_node=FALSE;
INT16 gamedisk,flagdisk=-1;
char gamedir[MAX_PATH],flagdir[MAX_PATH];
char c_dir_g=0;
INT16 user_num=0;
INT16 time_slice_value=1500;

void
time_slice(void) {
#if 0
	asm {
	    mov ax,time_slice_value
	    INT16 15
	}
#else
	od_sleep(1);
#endif
}

void
NoDropFile(void) {
	printf("\nNew York 2008 Player Editor - No dropfile found!\n");
	printf("To start in Local mode type:\nNYEDIT -L\n");
	exit(10);
}

extern "C" int main (int argc,char *argv[]) {
	FILE *justfile;
	char key,s_key;
	WORD uintval;
	DWORD points;
	char numstr[41];
	scr_rec srec;
	INT16 cnt;
	INT16 cntv,intval;
	//  user_rec cur_user;

	//your opendoors reg here
	/*    strcpy(od_control.od_registered_to,"[UNREGISTERED]");
	    od_control.od_registration_key=00000000000000000000000000000000; */

	//  directvideo=0;

	//  od_control.od_clear_on_exit=FALSE;
	od_control.od_no_file_func=NoDropFile;
	od_control.od_disable |= DIS_BPS_SETTING;
	od_control.od_disable |= DIS_NAME_PROMPT;

	//  od_control.od_config_function = CustomConfigFunction;
	//od_control.od_config_file = INCLUDE_CONFIG_FILE;
	//od_control.od_config_filename = "NYEDIT.CFG";


	od_control.od_config_function = CustomConfigFunction;
	if(fexist(CFG_FILENAME))
		od_control.od_config_file = INCLUDE_CONFIG_FILE;
	else
		od_control.od_config_file = NO_CONFIG_FILE;
	od_control.od_config_filename = CFG_FILENAME;

	od_control.od_mps=INCLUDE_MPS;
	od_control.od_nocopyright=TRUE;

	if(time_slice_value>0)
		od_control.od_ker_exec=time_slice;

	cnt=1;
	if(argc>1) {
		do {
			if (strnicmp(argv[cnt],"-L",2)==0) {
				od_control.od_force_local=TRUE;
				clrscr();
				textbackground(LIGHTCYAN);
				textcolor(BLUE);
				gotoxy(1,7);
				cprintf("ษอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออป");
				gotoxy(1,8);
				cprintf("บ New York 2008 Player Editor                                                 บ");
				gotoxy(1,9);
				cprintf("บ Starting in local mode input your name: ฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐฐ บ");
				gotoxy(1,10);
				cprintf("ศอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออผ");
				gotoxy(43,9);
				//      for(x=0;x<35;x++)
				//        putch('ฐ');

				//      cprintf(" บ\b\b");

				//      for(x=0;x<35;x++)
				//        putch('\b');


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
			} else if (strnicmp(argv[cnt],"-P",2)==0) {
				strzcpy(od_control.info_path,argv[cnt],2,59);
			} else if (strnicmp(argv[cnt],"-RDBPS",6)==0) {
				od_control.od_disable &=~ DIS_BPS_SETTING;
			} else if (strnicmp(argv[cnt],"-N",2)==0) {
				strzcpy(numstr,argv[cnt],2,59);
				sscanf(numstr,"%d",&intval);
				od_control.od_node=intval;
				/*      } else if (strnicmp(argv[cnt],"-DV",3)==0) {
					directvideo=1; */
			} else if (strnicmp(argv[cnt],"-C",2)==0) {
				od_control.od_config_filename=argv[cnt]+2;
				od_control.od_config_file = INCLUDE_CONFIG_FILE;
			}
			/* Was <= argc */
		} while ((++cnt)<argc);
	}

	od_printf("`bright`New York 2008 Player Editor Copyright 1995, George Lebl - All rights Reserved\n\r");
	if(!od_control.od_force_local)
		od_control.od_clear_on_exit=FALSE;
	od_control.od_disable |= DIS_INFOFILE;
	if(od_control.user_rip==TRUE) {
		od_control.user_rip=FALSE;
		od_control.user_ansi=TRUE;
	}
	getcwd(gamedir,MAX_PATH);
	gamedisk=gamedir[0] - 'A';
	strzcpy(gamedir,gamedir,2,MAX_PATH);
	if(flagdisk==-1) {
		flagdisk=gamedisk;
		strcpy(flagdir,gamedir);
	}
	//  od_control.od_ker_exec=ny_kernel;
	od_control.od_help_text2=(char *)"  New York 2008 Player Editor (c) Copyright 1995 George Lebl                  ";
	ch_flag_d();
	if (fexist(MAINTFLAG_FILENAME)) {
		od_printf("\n\r\n\r`bright red`W`red`ell the mantanace is running on some other node so please come");
		od_printf("back later, or if this is a single node bbs, notify the sysop because");
		od_printf("then he should delete the MAINT.RUN flag file....");
		WaitForEnter();
		od_exit(10,FALSE);
	}

	ch_game_d();
	if (!fexist(USER_FILENAME)) {
		od_printf("\n\r`bright`NO user File!!!!\n\r");
		od_exit(10,FALSE);
	}

	/*  if (fexist(KEY_FILENAME)) {
	    strcpy(numstr,od_control.system_name);
	    get_bbsname(numstr);
	    if(seereg(numstr)==FALSE) {
	      od_printf("\n\r\n\r`bright`UNREGISTERED... EDITOR DISABLED!!\n\r");
	      od_printf("\r\n`white`NOTE: If you have registered the game but the editor still\r\n");
	      od_printf("says it's unregistered, make sure it is reading the configuration\r\n");
	      od_printf("files the game is reading. Use the -C option, or make sure your\r\n");
	      od_printf("name and bbs name are in a file called NY2008.CFG!\n\r");
	 
	      WaitForEnter();
	      od_exit(10,FALSE);
	    }
	  } else {
	    od_printf("\n\r\n\r`bright`UNREGISTERED... EDITOR DISABLED!!\n\r");
	    od_printf("\r\n`white`NOTE: If you have registered the game but the editor still\r\n");
	    od_printf("says it's unregistered, make sure it is reading the configuration\r\n");
	    od_printf("files the game is reading. Use the -C option, or make sure your\r\n");
	    od_printf("name and bbs name are in a file called NY2008.CFG!\n\r");
	    WaitForEnter();
	    od_exit(10,FALSE);
	  }
	   */

	do {
		ch_game_d();
		justfile = ShareFileOpen(USER_FILENAME, "rb");

		/* If unable to access the user file, display an error message and */
		/* return.                                                         */
		if(justfile == NULL) {
			od_printf("Unable to access the user file.\n\r");
			WaitForEnter();
			od_exit(10,FALSE);
		}

		/* Move to appropriate location in user file for the current user's */
		/* record. */
		fseek(justfile, (INT32)user_num * sizeof(user_rec), SEEK_SET);

		/* read record from file. */
		if (ny_fread(&cur_user, sizeof(user_rec), 1, justfile) != 1) {
			user_num--;
		}

		/* Close the user file to allow other nodes to access it again. */
		fclose(justfile);



		switch(key=entry_menu())    // Get user's choice
		{
			case '[':
				user_num--;
				if (user_num<0)
					user_num=0;
				break;

			case ']':
				user_num++;
				break;

			case '4':
				if(cur_user.sex==MALE)
					cur_user.sex=FEMALE;
				else
					cur_user.sex=MALE;
				WriteCurrentUser(user_num);
				break;

			case '5':
				od_printf("\n\n\r`bright`Display Level/Minimum Points Table? (Y/N):");
				s_key=od_get_answer("YN");
				od_printf("%c\n\r",s_key);
				if(s_key=='Y') {
					od_printf("\n\n\r`bright`Level:               Min Points:`cyan`\n\r");
					od_printf("0                    0\n\r");
					od_printf("1                    500           `bright red`This table shows the minimum\n\r`cyan`");
					od_printf("2                    1020          `bright red`points required to get into\n\r`cyan`");
					od_printf("3                    2100          `bright red`a level. This editor will\n\r`cyan`");
					od_printf("4                    3700          `bright red`calculate the minimum level\n\r`cyan`");
					od_printf("5                    6100          `bright red`for the points entered.`cyan`\n\r");
					od_printf("6                    9100\n\r");
					od_printf("7                    12820\n\r");
					od_printf("8                    17300\n\r");
					od_printf("9                    22580\n\r");
					od_printf("10                   28700\n\r");
					od_printf("11                   35700\n\r");
					od_printf("12                   43620\n\r");
					od_printf("13                   52500\n\r");
					od_printf("14                   62380\n\r");
					od_printf("15                   73300\n\r");
					od_printf("16                   85300\n\r");
					od_printf("17                   98420\n\r");
					od_printf("18                   112700\n\r");
					od_printf("19                   128180\n\r");
					od_printf("20                   144900");
				}

				od_printf("\n\r`bright`Enter amount of points, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%lu",&points);
					if(points>cur_user.points) {
						set_points(points);
						WriteCurrentUser(user_num);
						SortScrFile(user_num);
					} else if(points<cur_user.points) {
						set_points(points);
						WriteCurrentUser(user_num);
						SortScrFileB(user_num);
					}
				}
				break;


			case 'M':
				od_printf("\n\r\n\r`bright`Enter amount of money at hand, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%lu",&cur_user.money);
					WriteCurrentUser(user_num);
				}
				break;

			case 'N':
				od_printf("\n\r\n\r`bright`Enter amount of money in bank, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%lu",&cur_user.bank);
					WriteCurrentUser(user_num);
				}
				break;

			case '1':
				//        od_control.od_ker_exec=NULL;
				od_printf("\n\r\n\r`bright`Enter the new identity:\n\r\n\r");
				od_printf("`bright green`1`green`. `bright green`H`green`eadbanger\n\r");
				od_printf("`bright green`2`green`. `bright green`H`green`ippie\n\r");
				od_printf("`bright green`3`green`. `bright green`B`green`ig `bright green`F`green`at `bright green`D`green`ude\n\r");
				od_printf("`bright green`4`green`. `bright green`C`green`rack `bright green`A`green`ddict\n\r");
				od_printf("`bright green`5`green`. `bright green`P`green`unk\n\r\n\r");
				od_printf("`bright`ENTER to cancel:");
				//        od_control.od_ker_exec=ny_kernel;
				s_key=od_get_answer("12345\n\r");
				od_printf("%c\n\r\n\r",key);

				if (s_key=='1')
					cur_user.nation = HEADBANGER;
				else if (s_key=='2')
					cur_user.nation = HIPPIE;
				else if (s_key=='3')
					cur_user.nation = BIG_FAT_DUDE;
				else if (s_key=='4')
					cur_user.nation = CRACK_ADDICT;
				else if (s_key=='5')
					cur_user.nation = PUNK;
				else
					break;
				set_points(cur_user.points);
				WriteCurrentUser(user_num);
				break;


			case '6':
				//      od_control.od_ker_exec=NULL;
				od_printf("\n\r\n\r`bright`Enter fights left, ENTER to abort:");
				//      od_control.od_ker_exec=ny_kernel;
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&cur_user.turns);
					WriteCurrentUser(user_num);
				}
				break;

			case '7':
				od_printf("\n\r\n\r`bright`Enter hitpoints, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%ld",&cur_user.hitpoints);

				}
				od_printf("\n\r\n\r`bright`Enter maximum hitpoints, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%ld",&cur_user.maxhitpoints);
				}
				if(cur_user.maxhitpoints<1)
					cur_user.maxhitpoints=1;
				if(cur_user.hitpoints<0)
					cur_user.hitpoints=0;
				if(cur_user.hitpoints>cur_user.maxhitpoints)
					cur_user.hitpoints=cur_user.maxhitpoints;
				WriteCurrentUser(user_num);
				break;


			case 'H':
				od_printf("\n\r\n\r`bright`Enter the drug high, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&intval);
					if(intval>100)
						intval=100;
					if(intval<0)
						intval=0;
					cur_user.drug_high=intval;
					WriteCurrentUser(user_num);
				}
				break;

			case 'O':
				od_printf("\n\r\n\r`bright`Enter hunger, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&intval);
					if(intval>100)
						intval=100;
					if(intval<0)
						intval=0;
					cur_user.hunger=intval;
					WriteCurrentUser(user_num);
				}
				break;

			case 'R':
				if (cur_user.std!=NONE) {
					od_printf("\n\r\n\r`bright`Enter the infection, ENTER to abort:");
					od_input_str(numstr,25,'0','9');
					if (numstr[0]!=0) {
						sscanf(numstr,"%d",&intval);
						if(intval>100)
							intval=100;
						if(intval<0)
							intval=0;
						cur_user.std_percent=intval;
						WriteCurrentUser(user_num);
					}
				} else {
					od_printf("\n\r\n\r`bright`Player is healthy!");
					WaitForEnter();
				}
				break;



			case '8':
				//          od_control.od_ker_exec=NULL;

				od_printf("\n\r\n\r`bright`Which weapon:\n\r\n\r");
				ny_disp_emu("`%X`2. `0H`2ands\n\r");
				ny_disp_emu("`%A`2. `0P`2epper `0S`2pray\n\r");
				ny_disp_emu("`%B`2. `0S`2harp `0S`2tick\n\r");
				ny_disp_emu("`%C`2. `0S`2crewdriver\n\r");
				ny_disp_emu("`%D`2. `0K`2nife\n\r");
				ny_disp_emu("`%E`2. `0H`2ammer\n\r");
				ny_disp_emu("`%F`2. `0C`2hain\n\r");
				ny_disp_emu("`%G`2. `0L`2ead `0P`2ipe\n\r");
				ny_disp_emu("`%H`2. `0G`2un\n\r");
				ny_disp_emu("`%I`2. `0C`2olt\n\r");
				ny_disp_emu("`%J`2. `0R`2ifle\n\r");
				ny_disp_emu("`%K`2. `0E`2lephant `0G`2un\n\r");
				ny_disp_emu("`%L`2. `0L`2aser `0G`2un\n\r");
				ny_disp_emu("`%M`2. `0N`2ail `0G`2un\n\r");
				ny_disp_emu("`%N`2. `0S`2hotgun\n\r");
				ny_disp_emu("`%O`2. `0A`2ssault Rifle\n\r");
				ny_disp_emu("`%P`2. `0M`2achine `0G`2un\n\r");
				ny_disp_emu("`%Q`2. `0P`2roton `0G`2un\n\r");
				ny_disp_emu("`%R`2. `0G`2ranade `0L`2auncher\n\r");
				WaitForEnter();
				ny_disp_emu("\n\r\n`%S`2. `0N`2eutron `0P`2haser\n\r");
				ny_disp_emu("`%T`2. `0B`2laster\n\r");
				ny_disp_emu("`%U`2. `0U`2ltrasound `0G`2un\n\r");
				ny_disp_emu("`%V`2. `0A`2tomic `0B`2omb\n\r");

				od_printf("\n\r`bright`Which one? ENTER to abort:");
				//          od_control.od_ker_exec=ny_kernel;
				s_key=od_get_answer("XABCDEFGHIJKLMNOPQRSTUV\n\r");
				od_printf("%c",s_key);

				if (s_key=='A')
					cur_user.arm=PEPPER;
				else if (s_key=='B')
					cur_user.arm=SHARP_STICK;
				else if (s_key=='C')
					cur_user.arm=SCREWDRIVER;
				else if (s_key=='D')
					cur_user.arm=KNIFE;
				else if (s_key=='E')
					cur_user.arm=HAMMER;
				else if (s_key=='F')
					cur_user.arm=CHAIN;
				else if (s_key=='G')
					cur_user.arm=LEAD_PIPE;
				else if (s_key=='H')
					cur_user.arm=GUN;
				else if (s_key=='I')
					cur_user.arm=COLT;
				else if (s_key=='J')
					cur_user.arm=RIFLE;
				else if (s_key=='K')
					cur_user.arm=ELEPHANT_GUN;
				else if (s_key=='L')
					cur_user.arm=LASER_GUN;
				else if (s_key=='M')
					cur_user.arm=NAILGUN;
				else if (s_key=='N')
					cur_user.arm=SHOTGUN;
				else if (s_key=='O')
					cur_user.arm=ASSAULT_RIFLE;
				else if (s_key=='P')
					cur_user.arm=MACHINEGUN;
				else if (s_key=='Q')
					cur_user.arm=PROTON_GUN;
				else if (s_key=='R')
					cur_user.arm=GRANADE_LAUNCHER;
				else if (s_key=='S')
					cur_user.arm=NEUTRON_PHASER;
				else if (s_key=='T')
					cur_user.arm=BLASTER;
				else if (s_key=='U')
					cur_user.arm=ULTRASOUND_GUN;
				else if (s_key=='V')
					cur_user.arm=A_BOMB;
				else if (s_key=='X')
					cur_user.arm=HANDS;
				else
					break;
				WriteCurrentUser(user_num);
				break;

			case 'I':
				if (cur_user.drug>=COKE) {
					od_printf("\n\r\n\r`bright`Enter the drug addiction, ENTER to abort:");
					od_input_str(numstr,25,'0','9');
					if (numstr[0]!=0) {
						sscanf(numstr,"%d",&intval);
						if(intval>100)
							intval=100;
						if(intval<0)
							intval=0;
						cur_user.drug_addiction=intval;
						WriteCurrentUser(user_num);
					}
				} else {
					od_printf("\n\r\n\r`bright`Player not using addictive drug!");
					WaitForEnter();
				}
				break;

			case 'J':
				if (cur_user.drug>=COKE) {
					od_printf("\n\r\n\r`bright`Enter the days since last hit, ENTER to abort:");
					od_input_str(numstr,25,'0','9');
					if (numstr[0]!=0) {
						sscanf(numstr,"%d",&intval);
						if(intval<0)
							intval=0;
						cur_user.drug_days_since=intval;
						WriteCurrentUser(user_num);
					}
				} else {
					od_printf("\n\r\n\r`bright`Player not using addictive drug!");
					WaitForEnter();
				}
				break;


			case 'A':
				od_printf("\n\r\n\r`bright`Enter days since laid, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&cur_user.since_got_laid);
					if(cur_user.since_got_laid<0)
						cur_user.since_got_laid=0;
					WriteCurrentUser(user_num);
				}
				break;

			case 'B':
				od_printf("\n\r\n\r`bright`Enter sex turns left, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&intval);
					if(intval>255)
						intval=255;
					if(intval<0)
						intval=0;
					cur_user.sex_today=intval;
					WriteCurrentUser(user_num);
				}
				break;

			case 'C':
				od_printf("\n\r\n\r`bright`Enter # of condoms, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&cur_user.condoms);
					if(cur_user.condoms<0)
						cur_user.condoms=0;
					WriteCurrentUser(user_num);
				}
				break;

			case 'D':
				od_printf("\n\r\n\r`bright`Enter # of rocks, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&cur_user.rocks);
					//        if(cur_user.rocks<0) cur_user.rocks=0;
					WriteCurrentUser(user_num);
				}
				break;

			case 'E':
				od_printf("\n\r\n\r`bright`Enter the Throwing Ability (0-100), ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&intval);
					cur_user.throwing_ability=intval;
					//        if(cur_user.throwing_ability<0) cur_user.throwing_ability=0;
					if(cur_user.throwing_ability>100)
						cur_user.throwing_ability=100;
					WriteCurrentUser(user_num);
				}
				od_printf("\n\r\n\r`bright`Enter the Kicking Ability (0-100), ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&intval);
					cur_user.kick_ability=intval;
					//        if(cur_user.kick_ability<0) cur_user.kick_ability=0;
					if(cur_user.kick_ability>100)
						cur_user.kick_ability=100;
					WriteCurrentUser(user_num);
				}
				od_printf("\n\r\n\r`bright`Enter the Punching Ability (0-100), ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&intval);
					cur_user.punch_ability=intval;
					//        if(cur_user.punch_ability<0) cur_user.punch_ability=0;
					if(cur_user.punch_ability>100)
						cur_user.punch_ability=100;
					WriteCurrentUser(user_num);
				}

				break;



			case 'G':
				od_printf("\n\r\n\r`bright`Enter # of drug hits, ENTER to abort:");
				od_input_str(numstr,25,'0','9');
				if (numstr[0]!=0) {
					sscanf(numstr,"%d",&cur_user.drug_hits);
					if(cur_user.drug_hits<0)
						cur_user.drug_hits=0;
					WriteCurrentUser(user_num);
				}
				break;




			case '3':
				od_printf("\n\r\n\r`bright`Input the new USER BBS name, ENTER to abort:");
				cntv=0;
				intval=TRUE;
				do {
					//scanf("%c",&key);
					s_key=od_get_key(TRUE);
					if(intval==TRUE) {
						if (s_key>='a' && s_key<='z')
							s_key-=32;
						intval=FALSE;
					} else if(intval==FALSE) {
						if (s_key>='A' && s_key<='Z')
							s_key+=32;
					}
					if(s_key==' ')
						intval=TRUE;
					else if(s_key=='\b') {
						if(cntv==0) {
							intval=TRUE;
							s_key=0;
						} else {
							cntv-=2;
							if(cntv>=0 && numstr[cntv]==' ')
								intval=TRUE;
							od_printf("\b \b");
							s_key=0;
						}
					}

					if(s_key!=0) {
						numstr[cntv]=s_key;
						od_putch(s_key);
					}
					cntv++;
				} while (s_key!='\n' && s_key!='\r' && cntv<35);
				numstr[cntv-1]=0;
				if (s_key!='\n' && s_key!='\r')
					dump();
				if (numstr[0]!=0) {
					strcpy(cur_user.bbsname,numstr);
					WriteCurrentUser(user_num);
				}
				/*od_input_str(numstr,35,' ',255);
				if (numstr[0]!=0) {
				  cap_names(numstr);
				  strcpy(cur_user.bbsname,numstr);

				} */
				break;

			case '2':
				od_printf("\n\r\n\r`bright`Input the new name, ENTER to abort:");
				od_input_str(numstr,25,' ',255);
				if (numstr[0]!=0) {
					strcpy(cur_user.name,numstr);
					WriteCurrentUser(user_num);
				}
				break;
			case '9':
				//      od_control.od_ker_exec=NULL;
				od_printf("\n\r`bright red`W`red`hat does the player say when he wins:\n\r|--------------------------------------|\n\r`bright green`");
				//      od_control.od_ker_exec=ny_kernel;
				od_input_str(numstr,40,' ',255);
				if (numstr[0]!=0) {
					strcpy(cur_user.say_win,numstr);
					WriteCurrentUser(user_num);
				}
				break;

			case '0':
				//      od_control.od_ker_exec=NULL;
				od_printf("\n\r`bright red`W`red`hat does the player say when he looses:\n\r|--------------------------------------|\n\r`bright green`");
				//      od_control.od_ker_exec=ny_kernel;
				od_input_str(numstr,40,' ',255);
				if (numstr[0]!=0) {
					strcpy(cur_user.say_loose,numstr);
					WriteCurrentUser(user_num);
				}
				break;

			case 'P':
				//      od_control.od_ker_exec=NULL;
				od_printf("\n\r\n\r`bright`1. `bright green`H`green`ealthy\n\r");
				od_printf("`bright`2. `bright green`C`green`raps\n\r");
				od_printf("`bright`3. `bright green`H`green`erpes\n\r");
				od_printf("`bright`4. `bright green`S`green`yphilis\n\r");
				od_printf("`bright`5. `bright green`A`green`IDS\n\r\n\r");
				od_printf("`bright`Enter the disease, ENTER to abort:");
				//      od_control.od_ker_exec=ny_kernel;
				s_key=od_get_answer("12345\n\r");
				od_putch(s_key);
				if (s_key=='1')
					cur_user.std=NONE;
				else if (s_key=='2')
					cur_user.std=CRAPS;
				else if (s_key=='3')
					cur_user.std=HERPES;
				else if (s_key=='4')
					cur_user.std=SYPHILIS;
				else if (s_key=='5')
					cur_user.std=AIDS;
				else
					break;
				WriteCurrentUser(user_num);
				break;


			case 'K':
				//      od_control.od_ker_exec=NULL;
				od_printf("\n\r\n\r`bright`1. `bright green`N`green`owhere\n\r");
				od_printf("`bright`2. `bright green`C`green`heap `bright green`M`green`otel\n\r");
				od_printf("`bright`3. `bright green`R`green`egular `bright green`H`green`otel\n\r");
				od_printf("`bright`4. `bright green`E`green`xpensive `bright green`H`green`otel\n\r\n\r");
				od_printf("`bright`Where is the user staying, ENTER to abort:");
				//      od_control.od_ker_exec=ny_kernel;
				s_key=od_get_answer("1234\n\r");
				od_putch(s_key);
				if (s_key=='1')
					cur_user.rest_where=NOWHERE;
				else if (s_key=='2')
					cur_user.rest_where=MOTEL;
				else if (s_key=='3')
					cur_user.rest_where=REG_HOTEL;
				else if (s_key=='4')
					cur_user.rest_where=EXP_HOTEL;
				else
					break;
				if(cur_user.rest_where==NOWHERE) {
					cur_user.hotel_paid_fer=0;
				} else {
					od_printf("\n\r\n\r`bright`How long is it paid for, ENTER to abort:");
					od_input_str(numstr,25,'0','9');
					if (numstr[0]!=0) {
						sscanf(numstr,"%d",&intval);
						if(intval>255)
							intval=255;
						if(intval<0)
							intval=0;
						cur_user.hotel_paid_fer=intval;
					}
				}
				WriteCurrentUser(user_num);
				break;
			case 'F':
				//      od_control.od_ker_exec=NULL;
				od_printf("\n\r\n\r`bright`1. `bright green`P`green`ot\n\r");
				od_printf("`bright`2. `bright green`H`green`ash\n\r");
				od_printf("`bright`3. `bright green`L`green`SD\n\r");
				od_printf("`bright`4. `bright green`C`green`oke\n\r");
				od_printf("`bright`5. `bright green`P`green`CP\n\r");
				od_printf("`bright`6. `bright green`H`green`eroin\n\r");
				od_printf("\n\r`bright`Select drug, ENTER to abort:");
				//      od_control.od_ker_exec=ny_kernel;
				s_key=od_get_answer("123456\n\r");
				od_putch(s_key);
				if (s_key=='1')
					cur_user.drug=POT;
				else if (s_key=='2')
					cur_user.drug=HASH;
				else if (s_key=='3')
					cur_user.drug=LSD;
				else if (s_key=='4')
					cur_user.drug=COKE;
				else if (s_key=='5')
					cur_user.drug=PCP;
				else if (s_key=='6')
					cur_user.drug=HEROIN;
				else
					break;
				if (cur_user.drug<COKE) {
					cur_user.drug_addiction=0;
					cur_user.drug_days_since=0;
				}
				WriteCurrentUser(user_num);
				break;

				/*      case '#':
					printf("\n\nPoints: ");
					scanf("%lu",&cur_user.points);
					scanf("%c",&key);
					printf("\n\nWeapon: ");
					scanf("%d",&cur_user.arm);
					scanf("%c",&key);
					WriteCurrentUser(user_num);
					break;
				 
				      case 'R':
					od_printf("\n\r\n\r`bright`Revive Player?:");
					key=od_get_answer("YN");
					if (key=='Y') {
					  cur_user.alive=ALIVE;
					  cur_user.hitpoints=cur_user.maxhitpoints;
					  WriteCurrentUser(user_num);
					}
					break;*/

			case 'L':
				if(cur_user.alive==ALIVE)
					cur_user.alive=UNCONCIOUS;
				else
					cur_user.alive=ALIVE;
				WriteCurrentUser(user_num);
				break;

			case '*':
				od_printf("\n\r\n\r`bright`New day for Player?(Y/N):");
				key=od_get_answer("YN");
				if (key=='Y') {
					cur_user.days_not_on=1;
					WriteCurrentUser(user_num);
				}
				break;

			case 'X':
				od_printf("\n\r\n\r`bright`KILL Player?(Y/N):");
				key=od_get_answer("YN");
				if (key=='Y') {
					cur_user.alive=DEAD;
					WriteCurrentUser(user_num);
				}
				break;
		}                            // Loop until quit to BBS

	} while(key!='Q');

	od_exit(10,FALSE);              // Again OpenDoors does the rest

	return(0);
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
dump(void) {
	char key;
	do {
		scanf("%c",&key);
	} while (key!='\n');
}


void
cap_names(char name[]) {
	INT16 cnt=0;
	INT16 cap=TRUE;

	while (name[cnt]!=0) {
		if (cap==TRUE) {
			if (name[cnt]>='a' && name[cnt]<='z')
				name[cnt]-=32;
		} else {
			if (name[cnt]>='A' && name[cnt]<='Z')
				name[cnt]+=32;
		}

		cnt++;

		if (name[cnt-1]==' ')
			cap=TRUE;
		else
			cap=FALSE;

	}
}



/*copy end chars  beginning from beg to dest*/
/*podobny strncpy*/
void
strzcpy(char dest[],const char src[], INT16 beg,INT16 end) {
	INT16 cnt=0;
	do {
		dest[cnt]=src[beg];
		beg++;
		cnt++;
	} while (cnt<=end && src[cnt]!=0);
	dest[cnt]=0;
}





void
ny_kernel(void) {

	ch_flag_d();
	if (fexist(MAINTFLAG_FILENAME)) {
		/* This is ugly - ToDo */
#ifndef ODPLAT_NIX
		fcloseall();
#endif
		//    od_control.od_disable|=DIS_CARRIERDETECT;
		od_printf("\n\r\n\r`bright`Please wait while maintanance runs on another node....");

		while (fexist(MAINTFLAG_FILENAME))
			;

		od_printf("\n\rThanks For Waiting....\n\r");

		//    od_control.od_disable&=~DIS_CARRIERDETECT;
	}
	ch_game_d();
}





char entry_menu(void) {
	char key;
	char numstr[15];

	od_clear_keybuffer();               // Clear any pending keys in buffer
	od_clr_scr();                       // Clear screen

	DisplayStats();

	ch_flag_d();
	sprintf(numstr,"u%07d.on",user_num);
	//   od_control.od_ker_exec=NULL;
	if(fexist(numstr)) {
		od_printf("`bright`]`red`/`bright`[`red` - Next/Previous  `bright`Q`red` - Quit\n\r");
		od_printf("`bright`This user is online and cannot be editted!");
		key= od_get_answer("[]Q");
		//     od_control.od_ker_exec=ny_kernel;
	} else {
		od_printf("`bright`]`red`/`bright`[`red` - Next/Previous `bright`X`red` - Delete `bright`*`red` - New Day `bright``red` `bright`Q`red` - Quit\n\r");
		od_printf("`bright red`P`red`ress the number/letter corresponding to what you want to change: ");
		//od_printf("`bright`R`red` - Revive Player `bright`D`red` - Delete`bright`
		// Q - Quit:");
		//     od_control.od_ker_exec=ny_kernel;
		key= od_get_answer("1234567890ABCDEFGHIJKLMNOPR[]XQ*");
	}
	return(key);
}

/*user stats display*/
void
DisplayStats(void) {
	INT16 intval;

	ny_kernel();
	//  od_control.od_ker_exec=NULL;
	od_printf("\n\r\n\r");
	od_clr_scr();
	od_printf("`bright`S`cyan`t`bright blue`ats `bright`F`cyan`o`bright blue`r `bright`L`cyan`e`bright blue`vel `white`%d `bright`1. ",(INT16)cur_user.level);
	if (cur_user.nation==HEADBANGER)
		od_printf("`bright green`H`green`eadbanger");
	else if (cur_user.nation==HIPPIE)
		od_printf("`bright green`H`green`ippie");
	else if (cur_user.nation==BIG_FAT_DUDE)
		od_printf("`bright green`B`green`ig `bright green`F`green`at `bright green`D`green`ude");
	else if (cur_user.nation==CRACK_ADDICT)
		od_printf("`bright green`C`green`rack `bright green`A`green`ddict");
	else if (cur_user.nation==PUNK)
		od_printf("`bright green`P`green`unk");
	od_printf(" `bright`2. `bright red`");
	ny_disp_emu(cur_user.name);
	od_printf("\n\r`bright`3. `white`User name:`bright`%s\n\r",cur_user.bbsname);
	//  od_printf("`blue`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
	if (cur_user.sex==MALE)
		od_printf("`bright`4. `bright red`S`red`ex: `bright green`M`green`ale  ");
	else
		od_printf("`bright`4. `bright red`S`red`ex: `bright green`F`green`emale");
	od_repeat(' ',29);
	if (cur_user.alive==ALIVE)
		od_printf("`bright`L. `bright red`S`red`tatus: `bright green`A`green`live\n\r");
	else if (cur_user.alive==DEAD)
		od_printf("`bright`L. `bright red`S`red`tatus: `bright red`DEAD\n\r");
	else
		od_printf("`bright`L. `bright red`S`red`tatus: `bright red`UNCONSIOUS\n\r");
	od_printf("`bright`5. `bright red`P`red`oints: `bright green`%-16s",D_Num(cur_user.points));
	od_repeat(' ',16);
	od_printf("`bright`M. `bright red`M`red`oney `bright red`I`red`n `bright red`H`red`and: `bright green`%s\n\r",D_Num(cur_user.money));
	od_printf("`bright`6. `bright red`F`red`ights: `bright green`%-3d", (INT16)cur_user.turns);
	od_repeat(' ',29);
	od_printf("`bright`N. `bright red`M`red`oney `bright red`I`red`n `bright red`B`red`ank: `bright green`%s\n\r",D_Num(cur_user.bank));
	od_printf("`bright`7. `bright red`H`red`it `bright red`P`red`oints: `bright green`%s ",D_Num(cur_user.hitpoints));
	intval=strlen(str);
	od_printf("`bright red`o`red`f `bright green`%-9s", D_Num(cur_user.maxhitpoints));
	od_repeat(' ',40-intval-25);
	od_printf("`bright`O. `bright red`H`red`ungry: `bright green`%d%c\n\r", (INT16)cur_user.hunger,37);
	od_printf("`bright`8. `bright red`A`red`rm: ");
	print_arm(cur_user.arm);
	od_printf("\n\r`bright`9. `bright red`W`red`inning `bright red`S`red`entence: `bright green`");
	ny_disp_emu(cur_user.say_win);
	od_printf("\n\r");
	od_printf("`bright`0. `bright red`L`red`oosing `bright red`S`red`entence: `bright green`");
	ny_disp_emu(cur_user.say_loose);
	od_printf("\n\r");
	od_printf("`bright`A. `bright red`D`red`ays `bright red`S`red`ince `bright red`G`red`ot `bright red`L`red`aid: `bright green`%-4d", cur_user.since_got_laid);
	od_repeat(' ',15);
	od_printf("`bright`P. `bright red`S`red`TD: `bright green`");
	print_disease(cur_user.std);
	od_printf("\n\r`bright`B. `bright red`S`red`ex `bright red`T`red`urns `bright red`L`red`eft: `bright green`%-4d",(INT16) cur_user.sex_today);
	od_repeat(' ',20);
	od_printf("`bright`R. `bright red`I`red`nfected: `bright green`%d%c\n\r",(INT16) cur_user.std_percent,37);
	od_printf("`bright`C. `bright red`C`red`ondoms: `bright green`%d\n\r", cur_user.condoms);
	od_printf("`bright`D. `bright red`R`red`ocks: `bright green`%d\n\r", (INT16)cur_user.rocks);
	od_printf("`bright`E. `bright red`A`red`bilities: `bright red`T`red`hrowing:`bright green`%d ", (INT16)cur_user.throwing_ability);
	od_printf("`bright red`K`red`icking: `bright green`%d ", (INT16)cur_user.kick_ability);
	od_printf("`bright red`P`red`unching: `bright green`%d\n\r", (INT16)cur_user.punch_ability);
	od_printf("`bright`F. `bright red`D`red`rug: `bright green`");
	print_drug(cur_user.drug);
	od_printf("\n\r`bright`G. `bright red`H`red`its: `bright green`%-6d\n\r",cur_user.drug_hits);
	od_printf("`bright`H. `bright red`H`red`igh: `bright green`%d%c\n\r",(INT16)cur_user.drug_high,37);
	od_printf("`bright`I. `bright red`A`red`ddicted: `bright green`%d%c\n\r",(INT16)cur_user.drug_addiction,37);
	od_printf("`bright`J. `bright red`D`red`ays `bright red`S`red`ince `bright red`L`red`ast `bright red`H`red`it: `bright green`%d\n\r",cur_user.drug_days_since);
	if (cur_user.rest_where==NOWHERE)
		od_printf("`bright`K. `bright red`S`red`taying `bright red`a`red`t: `bright green`N`green`owhere");
	else if (cur_user.rest_where==MOTEL)
		od_printf("`bright`K. `bright red`S`red`taying `bright red`a`red`t: `bright green`C`green`heap `bright green`M`green`otel");
	else if (cur_user.rest_where==REG_HOTEL)
		od_printf("`bright`K. `bright red`S`red`taying `bright red`a`red`t: `bright green`R`green`egular `bright green`H`green`otel");
	else if (cur_user.rest_where==EXP_HOTEL)
		od_printf("`bright`K. `bright red`S`red`taying `bright red`a`red`t: `bright green`E`green`xpensive `bright green`H`green`otel");
	od_printf(" `bright red`F`red`or `bright green`%d`bright red` D`red`ays\n\r",(INT16)cur_user.hotel_paid_fer);
	od_printf("`blue`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
	//  od_control.od_ker_exec=ny_kernel;
	// WaitForEnter();
}


void
print_drug(drug_type drug) {
	od_printf("`bright green`");
	if (drug==POT)
		od_printf("P`green`ot");
	else if (drug==HASH)
		od_printf("H`green`ash");
	else if (drug==LSD)
		od_printf("L`green`SD");
	else if (drug==COKE)
		od_printf("C`green`oke");
	else if (drug==PCP)
		od_printf("P`green`CP");
	else if (drug==HEROIN)
		od_printf("H`green`eroin");
}


void
set_points(DWORD raise) {       // when is time for next level      0   1    2    3    4    5    6     7     8     9     10    11    12    13    14    15    16    17     18     19      //20
	DWORD level_seal[LEVELS] = {500,1020,2100,3700,6100,9100,12820,17300,22580,28700,35700,43620,52500,62380,73300,85300,98420,112700,128180,144900};//,162900};
	/* char c_dir;

	c_dir=c_dir_g;*/


	cur_user.level=0;
	cur_user.points=raise;
	while (cur_user.level<20 && level_seal[cur_user.level]<=cur_user.points) {
		//while ( (pow( 10,((double)cur_user.level+1)/2) ) * 25 )<=cur_user.points) {
		cur_user.level++;
		if (cur_user.nation==HEADBANGER) {
			cur_user.strength=(5 * pow(1.3,cur_user.level)+.5);
			cur_user.defense=(4 * pow(1.3,cur_user.level)+.5);
		} else if (cur_user.nation==HIPPIE) {
			cur_user.strength=(4 * pow(1.3,cur_user.level)+.5);
			cur_user.defense=(4 * pow(1.3,cur_user.level)+.5);
		} else if (cur_user.nation==BIG_FAT_DUDE) {
			cur_user.strength=(3 * pow(1.3,cur_user.level)+.5);
			cur_user.defense=(6 * pow(1.3,cur_user.level)+.5);
		} else if (cur_user.nation==CRACK_ADDICT) {
			cur_user.strength=(4 * pow(1.3,cur_user.level)+.5);
			cur_user.defense=(5 * pow(1.3,cur_user.level)+.5);
		} else if (cur_user.nation==PUNK) {
			cur_user.strength=(6 * pow(1.3,cur_user.level)+.5);
			cur_user.defense=(3 * pow(1.3,cur_user.level)+.5);
		}
		cur_user.hitpoints+=.7 * cur_user.maxhitpoints;
		cur_user.maxhitpoints=(15 * pow(1.7,cur_user.level)+.5);
	}
	//   wrt_sts();
}

void
SortScrFile(INT16 usr) // pebble sorting of scorefile
{
	FILE *justfile;
	FILE *scr_file;
	FILE *fpUserFile;
	FILE *njustfile;
	INT16 crnt1,crnt2,cnt,sorted;
	scr_rec rec[2];
	user_rec urec;
	char numstr[20];

	ch_game_d();
	scr_file=ShareFileOpen(SCR_FILENAME,"r+b");
	fpUserFile=ShareFileOpen(USER_FILENAME,"r+b");
	if(scr_file != NULL && fpUserFile != NULL) {
		if (usr==user_num) {
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
		rec[crnt1].user_num=user_num;
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

					if (usr==user_num) {
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

	// sort in a certain user either current or an offline user
	ch_game_d();
	scr_file=ShareFileOpen(SCR_FILENAME,"r+b");
	fpUserFile=ShareFileOpen(USER_FILENAME,"r+b");
	if(scr_file != NULL && fpUserFile != NULL) {
		if (usr==user_num) {
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
		rec[crnt1].user_num=user_num;
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

					if (usr==user_num) {
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
print_disease(desease ill) {
	od_printf("`bright green`");
	if (ill==NONE)
		od_printf("H`green`ealthy");
	else if (ill==CRAPS)
		od_printf("C`green`rabs");
	else if (ill==HERPES)
		od_printf("H`green`erpes");
	else if (ill==SYPHILIS)
		od_printf("S`green`yphilis");
	else if (ill==AIDS)
		od_printf("A`green`IDS");
}




void
print_arm(weapon arm) {
	if (arm==HANDS)
		od_printf("`bright green`H`green`ands");
	else if (arm==PEPPER)
		od_printf("`bright green`P`green`epper `bright green`S`green`pray");
	else if (arm==KNIFE)
		od_printf("`bright green`K`green`nife");
	else if (arm==CHAIN)
		od_printf("`bright green`C`green`hain");
	else if (arm==GUN)
		od_printf("`bright green`G`green`un");
	else if (arm==RIFLE)
		od_printf("`bright green`R`green`ifle");
	else if (arm==LASER_GUN)
		od_printf("`bright green`L`green`aser `bright green`G`green`un");
	else if (arm==SHOTGUN)
		od_printf("`bright green`S`green`hotgun");
	else if (arm==MACHINEGUN)
		od_printf("`bright green`M`green`achine `bright green`G`green`un");
	else if (arm==GRANADE_LAUNCHER)
		od_printf("`bright green`G`green`ranade `bright green`L`green`auncher");
	else if (arm==BLASTER)
		od_printf("`bright green`B`green`laster");
	else if (arm==A_BOMB)
		ny_disp_emu("`0A`2tomic `0B`2omb");
	else if (arm==SHARP_STICK)
		ny_disp_emu("`0S`2harp `0S`2tick");
	else if (arm==SCREWDRIVER)
		ny_disp_emu("`0S`2crewdriver");
	else if (arm==HAMMER)
		ny_disp_emu("`0H`2ammer");
	else if (arm==LEAD_PIPE)
		ny_disp_emu("`0L`2ead `0P`2ipe");
	else if (arm==COLT)
		ny_disp_emu("`0C`2olt");
	else if (arm==ELEPHANT_GUN)
		ny_disp_emu("`0E`2lephant `0G`2un");
	else if (arm==NAILGUN)
		ny_disp_emu("`0N`2ail `0G`2un");
	else if (arm==ASSAULT_RIFLE)
		ny_disp_emu("`0A`2ssault `0R`2ifle");
	else if (arm==PROTON_GUN)
		ny_disp_emu("`0P`2roton `0G`2un");
	else if (arm==NEUTRON_PHASER)
		ny_disp_emu("`0N`2Neutron `0P`2haser");
	else if (arm==ULTRASOUND_GUN)
		ny_disp_emu("`0U`2ltrasound `0G`2un");

}


/* The WriteCurrentUser() function is called to save the information on the */
/* user who is currently using the door, to the ny2008.USR file.            */
void WriteCurrentUser(INT16 user_num) {
	FILE *fpUserFile;
	ny_kernel();

	/* Attempt to open the user file for exclusize access by this node.     */
	/* This function will wait up to the pre-set amount of time (as defined */
	/* near the beginning of this file) for access to the user file.        */
	ch_game_d();
	fpUserFile = ShareFileOpen(USER_FILENAME, "r+b");

	/* If unable to access the user file, display an error message and */
	/* return.                                                         */
	if(fpUserFile == NULL) {
		od_printf("Unable to access the user file.\n\r");
		WaitForEnter();
		return;
	}

	/* Move to appropriate location in user file for the current user's */
	/* record. */
	fseek(fpUserFile, (INT32)user_num * sizeof(user_rec), SEEK_SET);

	/* Write the new record to the file. */
	if(ny_fwrite(&cur_user, sizeof(user_rec), 1, fpUserFile) == 0) {
		/* If unable to write the record, display an error message. */
		fclose(fpUserFile);
		od_printf("Unable to update user record file.\n\r");
		WaitForEnter();
		return;
	}

	/* Close the user file to allow other nodes to access it again. */
	fclose(fpUserFile);
}







/* This function opens the specified file in the specified mode for         */
/* exculsive access by this node; while the file is open, other nodes will  */
/* be unable to open the file. This function will wait for up to the number */
/* of seconds set by FILE_ACCESS_MAX_WAIT, which is defined near the        */
/* beginning of this file.                                                  */
//FILE *ExculsiveFileOpen(char *pszFileName, char *pszMode)
//{
//   FILE *fpFile = NULL;
//   time_t StartTime = time(NULL);

/* Attempt to open the file while there is still time remaining. */
//   while((fpFile = fopen(pszFileName, pszMode)) == NULL
//      && errno == EACCES
//      && difftime(time(NULL), StartTime) < FILE_ACCESS_MAX_WAIT)
//   {
/* If we were unable to open the file, call od_kernal, so that    */
/* OpenDoors can continue to respond to sysop function keys, loss */
/* of connection, etc.                                            */
//     od_kernal();
//  }

/* Return FILE pointer for opened file, if any. */
//   return(fpFile);
//}

/* The WaitForEnter() function is used to create custom prompt */

void WaitForEnter(void) {
	/* Display prompt. */
	od_printf("\n\r`bright red`Smack [ENTER] to go on.");

	/* Wait for a Carriage Return or Line Feed character from the user. */
	od_get_answer("\n\r");
}




/* CustomConfigFunction() is called by OpenDoors to process custom */
/* configuration file keywords that                                */
void CustomConfigFunction(char *pszKeyword, char *pszOptions) {
	if(stricmp(pszKeyword, "SingleNodeOnly") == 0) {
		single_node=TRUE;
	} else if(stricmp(pszKeyword, "FlagDirectory") == 0) {
		strupr(pszOptions);

		flagdisk=(*pszOptions) - 'A';

		strzcpy(flagdir,pszOptions,2,MAX_PATH);
	} else if(stricmp(pszKeyword, "PollingValue") == 0) {
		if(time_slice_value!=0)
			sscanf(pszOptions,"%d",&time_slice_value);
	} else if(stricmp(pszKeyword, "NoMultitasker") == 0) {
		time_slice_value=0;
	}
}


void
ny_disp_emu(const char line[]) {
	INT16 cnt;

	for(cnt=0;line[cnt]!=0;cnt++) {
		if(line[cnt]=='`') {
			cnt++;
			if(line[cnt]==0)
				return;
			else if(line[cnt]=='0')
				od_printf("`bright green`");
			else if(line[cnt]=='1')
				od_printf("`blue`");
			else if(line[cnt]=='2')
				od_printf("`green`");
			else if(line[cnt]=='3')
				od_printf("`cyan`");
			else if(line[cnt]=='4')
				od_printf("`red`");
			else if(line[cnt]=='5')
				od_printf("`magenta`");
			else if(line[cnt]=='6')
				od_printf("`brown`");
			else if(line[cnt]=='7')
				od_printf("`white`");
			else if(line[cnt]=='8')
				od_printf("`bright black`");
			else if(line[cnt]=='9')
				od_printf("`bright blue`");
			else if(line[cnt]=='!')
				od_printf("`bright cyan`");
			else if(line[cnt]=='@')
				od_printf("`bright red`");
			else if(line[cnt]=='#')
				od_printf("`bright magenta`");
			else if(line[cnt]=='$')
				od_printf("`bright yellow`");
			else if(line[cnt]=='%')
				od_printf("`bright`");
		} else {
			od_putch(line[cnt]);
		}
	}
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
	 
	  char kod[26];
	  char string[26] = "ABECEDAKURVAHLEDAPICATAKY";
	  INT16 intval;
	  INT16 cnt;
	  INT16 bbsc;
	  INT16 which;
	  INT16 temp,temp2;
	 
	  ch_game_d();
	  justfile=ShareFileOpen(KEY_FILENAME,"rb");
	 
	  ny_fread(kod,26,1,justfile);
	 
	  fclose(justfile);
	 
	 
	  sscanf(kod,"%02d",&intval);
	 
	  if (intval!=strlen(bbsname)) {
	    return(0);
	  }
	 
	  intval = kod[0] - '0';
	 
	  bbsc=0;
	 
	  which=0;
	 
	  for (cnt=2;cnt<25;cnt++) {
	    if (bbsname[bbsc]==0) bbsc=0;
	 
	    temp2=string[cnt]+intval+bbsname[bbsc];
	 
	//    if (kod[cnt]>'Z') kod[cnt]-=('Z'-'A');
	    temp =  (temp2-'A')/('Z'-'A');
	 
	    temp2 = temp2 - (('Z'-'A') * temp);
	 
	    if (kod[cnt]!=temp2) {
	      return(0);
	    }
	 
	    if (which==0)
	      which =1;
	    else
	      which =0;
	 
	    intval = kod[which] - '0';
	    bbsc++;
	  }
	 
	  return(TRUE);*/
}

size_t
ny_fwrite(const void *ptr, size_t size, size_t n, FILE *stream) {
	size_t status;
	INT32 offset;
	offset=ftell(stream);

	if(single_node==FALSE && filelength(fileno(stream))>=offset+(size*n)) {
		//offset=ftell(stream);

		lock(fileno(stream),offset,size * n)
			;
		status=fwrite(ptr,size,n,stream);
		unlock(fileno(stream),offset,size * n);
	} else {
		status=fwrite(ptr,size,n,stream);
	}
	return(status);
}


size_t
ny_fread(void *ptr, size_t size, size_t n, FILE *stream) {
	size_t status;
	INT32 offset;

	if(single_node==FALSE && filelength(fileno(stream))>=offset+(size*n)) {
		offset=ftell(stream);

		lock(fileno(stream),offset,size * n)
			;
		status=fread(ptr,size,n,stream);
		unlock(fileno(stream),offset,size * n);
	} else {
		status=fread(ptr,size,n,stream);
	}
	return(status);
}




/* This function opens the specified file in the specified mode for         */
/* share access by this node; while the file is open, other nodes will      */
/* be able to open the file. This function will wait for up to the number   */
/* of seconds set by FILE_ACCESS_MAX_WAIT, which is defined near the        */
/* beginning of this file.                                                  */
FILE *ShareFileOpen(const char *pszFileName, const char *pszMode) {
	FILE *fpFile = NULL;
	time_t StartTime = time(NULL);

	/* Attempt to open the file while there is still time remaining. */
	ny_kernel();
	if(single_node==FALSE) {
		while((fpFile = _fsopen(pszFileName, pszMode,SH_DENYNO)) == NULL
		        && errno == EACCES
		        && difftime(time(NULL), StartTime) < FILE_ACCESS_MAX_WAIT) {
			/* If we were unable to open the file, call od_kernal, so that    */
			/* OpenDoors can continue to respond to sysop function keys, loss */
			/* of connection, etc.                                            */
			od_kernal();
		}
	} else {
		fpFile = fopen(pszFileName, pszMode);
	}

	/* Return FILE pointer for opened file, if any. */
	return(fpFile);
}

void
ch_game_d(void) {
	if(c_dir_g==1) {
#ifndef __unix__
		setdisk(gamedisk);
#endif

		chdir(gamedir);
		c_dir_g=0;
	}
}

void
ch_flag_d(void) {
	if(c_dir_g==0) {
#ifndef __unix__
		setdisk(flagdisk);
#endif

		chdir(flagdir);
		c_dir_g=1;
	}
}
