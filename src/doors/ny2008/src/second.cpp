// include the header
#include "ny2008.h"

// include prototypes for fights
#include "fights.h"

extern unsigned _stklen;
extern glob_t fff;

extern char oneframe;
extern INT16 noevents;
extern INT16 time_slice_value;
extern INT16 no_rip_m;

extern INT16 dobad;

extern tIBInfo IBBSInfo;
extern INT16 ibbs;
extern INT16 ibbs_send_nodelist;
extern INT16 ibbs_maint_i;
extern INT16 ibbs_maint_o;
extern INT16 ibbs_operator;
extern INT16 ibbs_game_num;


// price arrays
extern DWORD gun_price[A_BOMB+1];
extern INT16 	drug_price[HEROIN+1];
extern INT16 no_kernel;
extern INT16 ibbs;
extern INT16 ibbs_operator;
extern INT16 ibbs_game_num;

// Declare global variables
extern INT16 nCurrentUserNumber,max_fights,max_drug_hits,condom_price,delete_after;
extern INT16 bank_interest;
extern char ansi_name[61],ascii_name[61];
extern INT16 do_scr_files;
extern user_rec cur_user;
extern enemy enemy_rec;
extern INT16 registered;
extern char uname[36];
extern char rec_credit[36];
extern INT16 do_maint;
extern char str[15];
extern INT16 expert;
extern INT16 rip;
extern char maint_exec[61];
extern char *t_buffer;
char *t_buffer1;
char savelevel=0;
extern INT16 single_node;
extern char *ver;
extern char *verinfo;
extern INT16 gamedisk,flagdisk;
extern char gamedir[MAX_PATH],flagdir[MAX_PATH];
extern char c_dir_g;
extern INT16 clean_mode;

extern INT16 busted_ch_bank;
extern INT16 busted_ch_food;
extern INT16 busted_ch_rape;
extern INT16 busted_ch_beggar;
extern INT16 busted_ch_car;
extern INT16 busted_ch_school;
extern INT16 busted_ch_window;
extern INT16 busted_ch_poison;
extern INT16 busted_ch_bomb;
extern INT16 success_ch_bank;
extern INT16 success_ch_food;
extern INT16 success_ch_rape;
extern INT16 success_ch_beggar;
extern INT16 success_ch_car;
extern INT16 success_ch_school;
extern INT16 success_ch_window;
extern INT16 success_ch_poison;
extern INT16 success_ch_bomb;

extern INT16 check_o_nodes;
extern INT16 no_slices;



void
get_line(char beg[],char line[], char ovr[],INT16 wrap) {
	char key;
	INT16 cnt,cnt2;
	INT16 intval;

	cnt=0;
	cnt2=0;
	cnt=strlen(beg);
	od_printf("`bright green`%s",beg);

	sprintf(line,"%s",beg);

	ovr[0]=0;

	do {
		key=od_get_key(TRUE);
		if (key!='\b') {
			if (cnt==0 && key=='/') {
				ny_line(331,0,0);
				//Command (S A C=continue)
				key=od_get_answer("SAC");
				od_putch(key);
				if (key=='S') {
					line[0]='/';
					line[1]='S';
					line[2]=0;
					od_printf("\n\r");
					return;
				} else if (key=='A') {
					line[0]='/';
					line[1]='A';
					line[2]=0;
					od_printf("\n\r");
					return;
				} else {
					cnt=0;
					od_printf("\r                          \r`bright green`");
				}
			} else if (key!='\n' && key!='\r') {
				if(wrap==TRUE || cnt<77) {
					line[cnt]=key;
					cnt++;
					od_putch(key);
				} else {
					line[cnt]=key;
					od_putch(key);
					od_putch('\b');
				}
			} else {
				od_printf("\n\r");
				line[cnt]=0;
				cnt=0;
			}
		} else {
			if (cnt>0)
				cnt--;
			od_printf("\b \b");
		}

	} while (cnt<78 && key!='\n' && key!='\r');

	if (cnt>=78) {
		while (line[cnt]!=' ' && cnt>0) {
			cnt--;
			od_printf("\b");
		}
		if (cnt==0) {
			cnt=78;
		} else {
			cnt++;
			od_clr_line();
		}
		od_printf("\n\r");
		while (cnt<78) {
			ovr[cnt2]=line[cnt];
			line[cnt]=0;
			cnt2++;
			cnt++;
		}
		line[78]=0;
		ovr[cnt2]=0;
	}
	if(dobad)
		dobadwords(line);
}






INT16
askifuser(char handle[25]) {
	char key;
	char numstr[26];


	ny_line(332,1,0);
	//Did ya mean
	if(!rip)
		ny_disp_emu(handle);
	else
		od_disp_str(ny_un_emu(handle,numstr));
	ny_line(79,0,0);
	//? (Y/N)

	key=ny_get_answer("YN");
	if(!rip)
		od_printf("%c\n\r",key);
	else
		od_disp_str("\n\r");
	if (key=='Y')
		return TRUE;
	return FALSE;
}





void
newz_ops(void) {
	char key,key_s;
	FILE *justfile;
	INT16 cnt;
	INT32 filepos;
	//	ffblk ffblk;
	INT16 nonstop=FALSE;
	newzfile_t	newzfile;

	key='T';

	ch_game_d();
	if (!fexist(YESNEWS_FILENAME) || !fexist(TODNEWS_FILENAME)) {
		justfile=ShareFileOpen(TODNEWS_FILENAME,"r+b");
		if(justfile != NULL)
			fclose(justfile);
		justfile=ShareFileOpen(YESNEWS_FILENAME,"r+b");
		if(justfile != NULL)
			fclose(justfile);
	}

	if(rip==TRUE && oneframe==FALSE) {
		od_disp_str("\n\r");
		od_send_file("frame.rip");
	}

	do {

		if(rip) {
			od_disp_str("\n\r!|e|#|#|#\n\r");
			od_send_file("frame3.rip");
		} else {
			od_printf("\n\r\n");
			ny_clr_scr();
		}

		cnt=4;
		ny_send_menu(NEWZ,"");
		//The New York Times
		//-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		ny_kernel();

		if (key=='T')
			justfile=ShareFileOpen(TODNEWS_FILENAME,"rb");
		else
			justfile=ShareFileOpen(YESNEWS_FILENAME,"rb");

		if((justfile==NULL || filelength(fileno(justfile))<sizeof(newzfile)) && key=='T') {
			ny_line(413,1,1);
			if(justfile != NULL)
				fclose(justfile);
			justfile=ShareFileOpen(TODNEWS_FILENAME,"wb");
			if(justfile != NULL)
				fclose(justfile);
		} else if(justfile == NULL || filelength(fileno(justfile))<sizeof(newzfile)) {
			ny_line(414,1,1);
			if(justfile != NULL)
				fclose(justfile);
			justfile=ShareFileOpen(YESNEWS_FILENAME,"wb");
			if(justfile != NULL)
				fclose(justfile);
		} else {
			while (justfile != NULL && ny_fread(&newzfile,sizeof(newzfile),1,justfile)==1) {
				cnt+=2;
				if (newzfile.flag==0) {
					ny_disp_emu("\n\r`@");
					ny_disp_emu(newzfile.tagline);
					od_printf("\n\r");
				} else if (newzfile.flag==1) {
					ny_disp_emu("\n\r`2");
					ny_disp_emu(newzfile.tagline);
					ny_disp_emu("\n\r`0                                                     ");
					ny_disp_emu(newzfile.name);
					od_printf("\n\r");
					cnt++;
				} else if (newzfile.flag==6) {
					ny_disp_emu("\n\r`2");
					ny_disp_emu(newzfile.tagline);
					ny_disp_emu("\n\r`0            ");
					ny_disp_emu(newzfile.name);
					ny_disp_emu(" `2from `0");
					ny_disp_emu(newzfile.name2);
					od_printf("\n\r");
					cnt++;
				} else if (newzfile.flag==2) {
					ny_disp_emu("\n\r`0");
					ny_disp_emu(newzfile.name);
					ny_disp_emu("`4 got `@BUSTED`4 today for `@");
					ny_disp_emu(newzfile.tagline);
					od_printf("\n\r");
				} else if (newzfile.flag==3) {
					ny_disp_emu("\n\r`0");
					ny_disp_emu(newzfile.name);
					ny_disp_emu("`4 ");
					ny_disp_emu(newzfile.tagline);
					od_printf("\n\r");
				} else if (newzfile.flag==4) {
					ny_disp_emu("\n\r`0");
					ny_disp_emu(newzfile.name);
					ny_disp_emu("`9 beat up `0");
					ny_disp_emu(newzfile.name2);
					ny_disp_emu("\n\r`3\"");
					ny_disp_emu(newzfile.tagline);
					ny_disp_emu("`3\" `@S`4creams `0");
					ny_disp_emu(newzfile.name);
					od_printf("\n\r");
					cnt++;
				} else if (newzfile.flag==5) {
					ny_disp_emu("\n\r`0");
					ny_disp_emu(newzfile.name);
					ny_disp_emu("`9 was beat up by `0");
					ny_disp_emu(newzfile.name2);
					ny_disp_emu("\n\r`3\"");
					ny_disp_emu(newzfile.tagline);
					ny_disp_emu("`3\" `@W`4hispers `0");
					ny_disp_emu(newzfile.name);
					ny_disp_emu(" `@I`4n `@P`4ain\n\r");
					cnt++;
				}

				if (nonstop==FALSE && (cnt+1)>=od_control.user_screen_length) {
					cnt=0;
					filepos=ftell(justfile);
					fclose(justfile);
					ny_disp_emu("`%More (Y/n/=)");
					key_s=ny_get_answer("YN=\n\r");
					od_printf("\r            \r");
					//if(key_s=='\n' || key_s=='\r') key_s='Y';
					//od_putch(key_s);
					if(key_s=='N')
						return;
					else if(key_s=='=')
						nonstop=TRUE;

					//	        WaitForEnter();


					//od_printf("\n\r");
					if (key=='T')
						justfile=ShareFileOpen(TODNEWS_FILENAME,"rb");
					else
						justfile=ShareFileOpen(YESNEWS_FILENAME,"rb");
					if(justfile != NULL)
						fseek(justfile,filepos,SEEK_SET);

				}
			}
			if(justfile != NULL)
				fclose(justfile);
		}
		if(rip)
			od_send_file("frame2.rip");
		ny_line(65,1,0);
		//T - Todays Newz Y - Yesterdays Newz [Q] - Quit

		key=ny_get_answer("TYQ\n\r");
		if (key=='\n' || key=='\r')
			key='Q';
		od_putch(key);
	} while (key!='Q');

}





void
news_post(const char line[], const char name1[], const char name2[], INT16 flag) {
	FILE *justfile;
	char c_dir;

	c_dir=c_dir_g;

	ch_game_d();
	justfile=ShareFileOpen(TODNEWS_FILENAME,"a+b");
	if(justfile != NULL) {
		ny_fwrite(line,80,1,justfile);
		ny_fwrite(name1,25,1,justfile);
		ny_fwrite(name2,36,1,justfile);
		ny_fwrite(&flag,2,1,justfile);
		fclose(justfile);
	}

	if(c_dir==1)
		ch_flag_d();
}


void
guns_ops(void) {
	char key,
	s_key;
	DWORD max;
	weapon choice;

	do {
		key = callmenu("BSYLQ?\n\r",ARMS,345,FALSE);
		while (expert>0 && key=='?') {
			expert+=10;
			key=callmenu("BSYLQ?\n\r",ARMS,345,FALSE);
			expert-=10;
		}


		if (key=='Y') {
			DisplayStats();
			WaitForEnter();
		} else if (key=='L') {           // list prices

			if(rip)
				ny_clr_scr();
			ny_line(66,2,2);
			//Here's what i got:
			ny_send_menu(WEAPONS,"",2);

			WaitForEnter();
		}
		else if (key=='B') {           // buy weapon

			if(rip)
				ny_clr_scr();
			ny_line(67,2,2);
			//Choose yer new weapon:
			ny_send_menu(WEAPONS,"",2);
			ny_line(333,1,0);
			//Which one? You got
			od_printf(D_Num(cur_user.money));
			ny_line(334,0,0);
			//to spend. (A-W, Enter=[Q]=Quit)

			s_key=ny_get_answer("ABCDEFGHIJKLMNOPRSTUVWQ\n\r");
			if (s_key=='\n' || s_key=='\r')
				s_key='Q';
			od_putch(s_key);
			if(!registered && s_key!='Q' && s_key>'J') {
				ny_disp_emu("`%\n\r\nUNREGISTERED!!!\n\r\nCannot do this!!!\n\r");
				WaitForEnter();
				s_key='Q';
			}
			if (s_key!='Q') {
				if (s_key=='A')
					choice=PEPPER;
				else if (s_key=='B')
					choice=SHARP_STICK;
				else if (s_key=='C')
					choice=SCREWDRIVER;
				else if (s_key=='D')
					choice=KNIFE;
				else if (s_key=='E')
					choice=HAMMER;
				else if (s_key=='F')
					choice=CHAIN;
				else if (s_key=='G')
					choice=LEAD_PIPE;
				else if (s_key=='H')
					choice=GUN;
				else if (s_key=='I')
					choice=COLT;
				else if (s_key=='J')
					choice=RIFLE;
				else if (s_key=='K')
					choice=ELEPHANT_GUN;
				else if (s_key=='L')
					choice=LASER_GUN;
				else if (s_key=='M')
					choice=NAILGUN;
				else if (s_key=='N')
					choice=SHOTGUN;
				else if (s_key=='O')
					choice=ASSAULT_RIFLE;
				else if (s_key=='P')
					choice=MACHINEGUN;
				else if (s_key=='R')
					choice=PROTON_GUN;
				else if (s_key=='S')
					choice=GRANADE_LAUNCHER;
				else if (s_key=='T')
					choice=NEUTRON_PHASER;
				else if (s_key=='U')
					choice=BLASTER;
				else if (s_key=='V')
					choice=ULTRASOUND_GUN;
				else if (s_key=='W') {
					choice=A_BOMB;
					if (cur_user.level<20) {
						ny_line(408,2,1);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
						goto not_old_enuf;
					}

					ny_line(68,2,1);
					//If you buy an A-Bomb you will have to fight with hands again, but who want's to
					ny_line(69,0,1);
					//fight if you can finish what you came here for ...

					WaitForEnter();
				}
				if (cur_user.arm!=HANDS) {

					ny_line(70,2,0);
					//Ya'll heffta sell yer
					print_arm(cur_user.arm);
					ny_line(335,0,0);
					//for
					od_printf(D_Num(gun_price[cur_user.arm]/2));
					ny_line(336,0,0);
					//, OK?(Y/N)

					s_key=ny_get_answer("YN");
					if(!rip)
						od_putch(s_key);
					if (s_key=='Y') {
						money_plus(gun_price[cur_user.arm]/2);
						cur_user.arm=HANDS;
						wrt_sts();

						ny_line(71,2,0);
						//Sold
						if(rip)
							od_get_answer("\n\r");

						ny_kernel();

					} else
						choice=HANDS;
				}
				if (gun_price[choice]>cur_user.money && choice!=HANDS) {

					ny_line(72,2,1);
					//Ya ain't got nuf money bro...

					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				} else if (choice!=HANDS) {

					ny_line(73,2,0);
					//Buy a
					print_arm(choice);
					ny_line(335,0,0);
					//for
					od_printf(D_Num(gun_price[choice]));
					ny_line(336,0,0);
					//?(Y/N)

					s_key=ny_get_answer("YN");
					if(!rip)
						od_putch(s_key);
					if (s_key=='Y') {
						cur_user.arm=choice;
						money_minus(gun_price[choice]);
						wrt_sts();

						ny_line(74,2,0);
						//Ya now got the
						print_arm(choice);
						ny_line(75,0,1);
						//so go destroy something...

						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");

					}
				}
not_old_enuf:
				;
			}
		} else if (key=='S') {   // Sell guns
			if(rip)
				no_rip_m=1;
			if (cur_user.arm==HANDS) {

				ny_line(76,2,1);
				//You ain't got nothing to sell!

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {

				ny_line(77,2,0);
				//Sell yer
				print_arm(cur_user.arm);
				ny_line(78,0,0);
				//for
				od_printf(D_Num(gun_price[cur_user.arm]/2));
				ny_line(79,0,0);
				//?(Y/N)

				s_key=ny_get_answer("YN");
				if(!rip)
					od_putch(s_key);
				if (s_key=='Y') {
					money_plus(gun_price[cur_user.arm]/2);
					cur_user.arm=HANDS;
					wrt_sts();
					ny_line(71,1,1);
					//Sold
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
take_drug(void) {
	if(rip)
		no_rip_m=1;
	if (cur_user.drug_hits<=0) {   // check if got some drugs
		ny_line(80,2,1);
		//You got no hits man...
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
	} else if (cur_user.drug_high==100) {
		ny_line(81,2,1);
		//Yer already 100% stoned!
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
	} else { // ok to take a drug
		ny_line(82,2,1);
		//You take a hit of yer favorite drug...
		if(rip)
			od_get_answer("\n\r");
		cur_user.drug_hits--;       //decrease user hits
		points_raise((DWORD)(12*(cur_user.drug+1))); //raise points for taking drugs
		cur_user.drug_high+=25;   //25% more high
		if (cur_user.drug_high>100)
			cur_user.drug_high=100;
		ny_line(83,0,0);
		//You are now
		od_printf("%d",(INT16)cur_user.drug_high,37);
		ny_line(84,0,1);
		if(rip)
			od_get_answer("\n\r");
		//% high
		wrt_sts();
		if (cur_user.drug>=COKE) {
			cur_user.drug_addiction+=10;
			cur_user.drug_days_since=0;
			wrt_sts();
			if (cur_user.drug_addiction>=100) { //overdosed
				cur_user.drug_addiction=100;
				Die(1);
			}
			ny_line(85,0,1);
			//Yer drug addiction went up by 10%
			if(rip)
				od_get_answer("\n\r");
		}
		if(!rip)
			WaitForEnter();
	}
}







void
healing_ops(void) {
	char key,
	s_key;
	INT16  hit_diff,
	howmuch;
	INT32 intval;

	/*drug rehab prices for different levels*/
	/* 0  1   2   3   4   5   6   7   8   9   10  11   12   13   14   15   16   17   18   19   20*/
	//int drug_rehab_price[LEVELS] ={80,110,150,270,360,420,530,620,750,890,950,1100,1410,1730,2050,2600,3700,4800,6310,8130,10000};

	do {
		key = callmenu("HDCYQ?\n\r",HEALING,346,FALSE);
		while (expert>0 && key=='?') {
			expert+=10;
			key=callmenu("HDCYQ?\n\r",HEALING,346,FALSE);
			expert-=10;
		}


		if (key=='Y') {
			DisplayStats();
			WaitForEnter();
		} else if (key=='H') {
			heal_wounds();
		} else if (key=='C') {
			INT16 std_diff;

			if (cur_user.std==NONE) {

				ny_line(86,2,1);
				//Yer not infected ... (yet) ...

				if(!rip)
					WaitForEnter();
				else {
					od_get_answer("\n\r");
					no_rip_m=1;
				}
			} else {
				std_diff=cur_user.std_percent;
				intval=pow(1.6,cur_user.level)*cur_user.std;

				ny_line(87,2,0);
				//It costs
				od_printf(D_Num(intval));//%s
				ny_line(88,0,0);
				// to cure 1%
				if(rip)
					od_get_answer("\n\r");

				ny_line(89,1,0);
				//You can heal
				od_printf(D_Num(std_diff));//%s
				ny_line(90,0,0);
				//% for
				od_printf(D_Num(std_diff*intval));
				if(rip) {
					od_disp_str("::^M@OK))|#|#|#\n\r");
					od_get_answer("\n\r");
				}

				if ((std_diff*intval)>cur_user.money) {
					std_diff=cur_user.money/(intval);
					ny_line(91,1,0);
					//You can only afford to cure
					od_printf("%s`red`%c!",D_Num(std_diff),37);
					if(rip) {
						od_disp_str("::^M@OK))|#|#|#\n\r");
						od_get_answer("\n\r");
					}
				}
				if(rip) {
					od_send_file("input.rip");
				}

				ny_line(92,2,0);
				//So how much ya want healed (Enter=[
				od_printf(D_Num(std_diff));
				ny_line(93,0,0);
				//], M=max
				od_printf(D_Num(std_diff));
				ny_disp_emu("`2):");

				howmuch=get_val((DWORD)std_diff,(DWORD)std_diff);
				cur_user.std_percent-=howmuch;
				money_minus(howmuch * (cur_user.level+1) * cur_user.std);
				if(!rip) {
					ny_disp_emu("\n\r`@");
				} else {
					ny_disp_emu("\n\r!|10000");
				}
				od_printf(D_Num(howmuch));

				ny_line(94,0,1);
				//% cured!

				wrt_sts();

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		}
		else if (key=='D') {
			if(rip)
				no_rip_m=1;
			if (cur_user.drug<COKE || cur_user.drug_addiction==0) {

				ny_line(95,2,1);
				//Yer not addicted or anything...

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {

				ny_line(96,2,0);

				od_printf(D_Num((DWORD)(500*pow(1.9,cur_user.level))));
				ny_line(97,0,0);
				// for loosing 1/3 of addiction, do it (Y/N)

				s_key=ny_get_answer("YN");
				od_putch(s_key);
				if (s_key=='Y') {
					if (cur_user.money<500*pow(1.9,cur_user.level)) {

						ny_line(98,2,1);

						//		  od_printf("\n\r\n`bright`You can't afford it!\n\r");
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else {
						money_minus(500*pow(1.9,cur_user.level));
						cur_user.drug_addiction=cur_user.drug_addiction*2/3;
						wrt_sts();
						news_post("was in drug rehab today",cur_user.name,"",3);

						ny_line(99,2,1);

						//		  od_printf("\n\r\n`bright green`Y`green`ou feel so much better......\n\r");
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					}
				}
			}
		}
	} while (key!='Q');
}

void
heal_wounds() {
	INT32 hit_diff;
	DWORD howmuch;

	if (cur_user.hitpoints==cur_user.maxhitpoints) {

		ny_line(100,2,1);
		//    od_printf("\n\r\n`bright red`Y`red`er not hurt ... (yet) ...\n\r");

		if(!rip)
			WaitForEnter();
		else {
			od_get_answer("\n\r");
			no_rip_m=1;
		}
		return;
	}
	hit_diff=cur_user.maxhitpoints-cur_user.hitpoints;

	ny_line(101,2,0);
	//od_printf("\n\r\n`bright red`Y`red`ou can heal `bright red`
	od_printf(D_Num(hit_diff));
	ny_line(78,0,0);
	//  od_printf(" for `bright red`
	od_printf(D_Num(hit_diff*(INT32)(cur_user.level/2.0 + 1)));
	if(rip) {
		od_disp_str("::^M@OK))|#|#|#\n\t");
		od_get_answer("\n\r");
	}

	if ((INT32)(hit_diff * (INT16)(cur_user.level/2.0 + 1))>cur_user.money) {
		hit_diff=cur_user.money/(INT16)(cur_user.level/2.0 + 1);
		ny_line(102,1,0);
		//od_printf("\n\r`bright red`Y`red`ou can only afford to heal `bright red`
		od_printf(D_Num(hit_diff));
		ny_line(103,0,0);
		if(rip)
			od_get_answer("\n\r");
		//`red` hitpoints!
	}
	if(rip) {
		od_send_file("input.rip");
	}

	ny_line(104,2,0);
	//  od_printf("\n\r\n`bright green`S`green`o how much ya want healed (`bright blue`Enter`green`=`bright green`[
	od_printf(D_Num(hit_diff));
	ny_line(93,0,0);
	//]`datk geen`, `bright blue`M`green`=max `bright green`%s`green`):",D_Num(hit_diff),str);
	od_printf(D_Num(hit_diff));
	ny_disp_emu("`2):");

	howmuch=get_val((DWORD)hit_diff,(DWORD)hit_diff);
	cur_user.hitpoints+=howmuch;
	money_minus(howmuch * (INT16)(cur_user.level/2.0 + 1));

	if(!rip)
		ny_disp_emu("\n\r`@");
	else
		od_disp_str("\n\r!|10000((*");
	od_printf(D_Num(howmuch));
	ny_line(105,0,1);
	//   H`red`itpoints were healed!\n\r",D_Num(howmuch));

	wrt_sts();
	if(!rip)
		WaitForEnter();
	else
		od_get_answer("\n\r");

}


void
food_ops(void) {
	char key,
	s_key;
	INT32 longval,intval;
	INT16 chance;

	do {
		key=callmenu("GESYQ?\n\r",FOOD,347,FALSE);
		while (expert>0 && key=='?') {
			expert+=10;
			key=callmenu("GESYQ?\n\r",FOOD,347,FALSE);
			expert-=10;
		}


		if (key=='Y') {
			DisplayStats();
			WaitForEnter();
		} else if (key=='G') {
			if(rip)
				no_rip_m=1;
			if (cur_user.hunger==0) {

				ny_line(106,2,1);
				//		    od_printf("\n\r\n`bright green`Y`green`er not hungry....\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {
				intval=80 + (50*pow(1.4,(double)cur_user.level));

				ny_line(107,2,0);
				//od_printf("\n\r\n`bright green`A`green`s you enter a restaurant you look at the prices...`bright green`
				od_printf(D_Num(intval));
				ny_line(108,0,0);

				//`green` ..., do it (`bright green`Y`green`/`bright green`N`green`)",D_Num(intval));
				s_key=ny_get_answer("YN");
				if(!rip)
					od_putch(s_key);
				if (s_key=='Y') {
					if (cur_user.money<intval) {

						ny_line(109,2,1);

						//od_printf("\n\r\n`bright`Well you ain't got enuf cash for that....\n\r");
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else {
						money_minus(intval);
						cur_user.hitpoints=cur_user.maxhitpoints;
						if (cur_user.hunger>=30)
							cur_user.hunger-=30;
						else
							cur_user.hunger=0;
						wrt_sts();
						ny_line(110,2,1);
						//od_printf("\n\r\n`bright red`Y`red`ou feel so refreshed yer hitpoints maxed out....\n\r");
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					}
				}
			}
		} else if (key=='E') {
			if(rip)
				no_rip_m=1;
			if (cur_user.hunger==0) {
				ny_line(106,2,1);
				//		    od_printf("\n\r\n`bright green`Y`green`er not hungry....\n\r");
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {
				ny_line(111,2,1);
				if(rip)
					od_get_answer("\n\r");
				ny_line(112,0,1);
				//od_printf("\n\r\n`bright red`T`red`hough not feeling that hungry anymore, you feel a bit worse and\n\rloose 1/3 of yer hitpoints...\n\r");
				cur_user.hitpoints=cur_user.hitpoints*2/3;
				if (cur_user.hunger>=25)
					cur_user.hunger-=25;
				else
					cur_user.hunger=0;

				wrt_sts();
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		} else if (key=='S') {
			if(rip)
				no_rip_m=1;
			if (cur_user.level==0) {

				ny_line(113,2,1);
				//		    od_printf("\n\r\n`bright`Level 0 players can't steal....\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {

				ny_line(114,2,1);
				//od_printf("\n\r\n`bright red`Y`red`ou try to steel some food from a supermarket...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				chance=xp_random(100);
				if (chance<=busted_ch_food) {

					od_printf("\n\r\n");
					ny_send_menu(BUSTED,"");

					//		      od_printf("\n\r\n`bright`BUSTED!!!! .... well the police beat the hell out of you .....\n\rWhy don't you try again tomorrow...\n\r");
					//		      od_printf("Ya lost 2%c of yer points!\n\r",37);
					cur_user.alive=UNCONCIOUS;
					points_loose(cur_user.points*.02);
					news_post("stealing food",cur_user.name,"",2);
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					od_exit(10,FALSE);
				} else {
					chance=xp_random(100);
					if (chance<=success_ch_food) {
						intval=xp_random(cur_user.hunger+2);
						if (cur_user.hunger<intval)
							intval= cur_user.hunger;

						ny_line(115,2,0);
						//od_printf("\n\r\n`bright red`Y`red`ou just stole enough so yer hunger went down by %d
						od_printf("%d%c",intval,37);

						cur_user.hunger-=intval;
						points_raise((INT32)25*cur_user.level);
						if(!rip) {
							od_disp_str("...\n\r");
							WaitForEnter();
						} else {
							od_disp_str("::^M@OK))|#|#|#\n\r");
							od_get_answer("\n\r");
						}
					} else {
						intval=2 * cur_user.level * DrgPtsCoef();

						ny_line(116,2,1);
						if(rip)
							od_get_answer("\n\r");

						ny_line(117,0,0);
						//		        od_printf("\n\r\n`bright red`Y`red`ou did not manage to steal anything but did not get busted...\n\r`bright red`Y`red`our points went up by %s\n\r",D_Num(intval));
						od_printf("%s",D_Num(intval));

						points_raise((INT32)13*cur_user.level);
						if(!rip) {
							od_disp_str("\n\r");
							WaitForEnter();
						} else {
							od_disp_str("::^M@OK))|#|#|#\n\r");
							od_get_answer("\n\r");
						}
					}
				}
			}
		}
	} while (key!='Q');
}


void
get_laid_ops(void) {
	char key;
	INT32 longval;
	INT32 intval;
	INT16 chance;
	char hand[25];
	char numstr[26];
	char line[80],ovr[80];
	FILE *justfile;
	FILE *msg_file;
	scr_rec urec;
	INT16 unum,ret,cnt;
	mail_idx_type mail_idx;
	INT32 fillen;


	do {
		key=callmenu("GSBYRPMQ?\n\r",SEX,348,FALSE);
		while (expert>0 && key=='?') {
			expert+=10;
			key=callmenu("GSBYRPMQ?\n\r",SEX,348,FALSE);
			expert-=10;
		}


		if (key=='Y') {
			DisplayStats();
			WaitForEnter();
		} else if (key=='R') {
			if(rip)
				no_rip_m=1;
			if (cur_user.sex_today<=0) {

				ny_line(118,2,1);
				//		    od_printf("\n\r\n`bright`You already used up all your sex turns today ...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {

				ny_line(119,2,1);
				//		    od_printf("\n\r\n`bright red`Y`red`ou look for a victim...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				chance=xp_random(100);
				if (chance<=busted_ch_rape) {

					od_printf("\n\r\n");
					ny_send_menu(BUSTED,"");
					//od_printf("\n\r\n`bright`BUSTED!!!! .... well the police beat the hell out of you .....\n\rWhy don't you try again tomorrow...\n\r");
					//od_printf("Ya lost 2%c of yer points!\n\r",37);

					cur_user.alive=UNCONCIOUS;
					points_loose(cur_user.points*.02);
					news_post("rape",cur_user.name,"",2);
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					od_exit(10,FALSE);
				} else {
					chance=xp_random(100);
					if (chance<=success_ch_rape) {

						if (cur_user.sex==MALE)
							ny_line(120,2,1);// od_printf("\n\r\n`bright red`Y`red`ou raped some lady and ran away ... `bright red`G`red`ood job ...\n\r");
						else
							ny_line(121,2,1); //od_printf("\n\r\n`bright red`Y`red`ou raped some guy and ran away ... `bright red`G`red`ood job ...\n\r");

						cur_user.since_got_laid=0;
						cur_user.sex_today--;
						illness();
						points_raise((INT32)40*(cur_user.level+1));
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else {

						ny_line(122,2,1);
						//		        od_printf("\n\r\n`bright red`Y`red`ou couldn't find any victim...\n\r");

						cur_user.sex_today--;
						wrt_sts();
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					}
				}
			}
		} else if (key=='G') {
			if(rip)
				no_rip_m=1;
			if (cur_user.sex_today<=0) {

				ny_line(118,2,1);
				//		    od_printf("\n\r\n`bright`You already used up all your sex turns today ...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {
				chance=xp_random(22);
				if (chance<=cur_user.level) {

					if (cur_user.sex==MALE)
						ny_line(123,2,1);// od_printf("\n\r\n`bright red`Y`red`ou scored a pretty nice chick ... `bright red`G`red`ood job ...\n\r");
					else
						ny_line(124,2,1); //od_printf("\n\r\n`bright red`Y`red`ou scored some cool guy ... `bright red`G`red`ood job ...\n\r");

					cur_user.since_got_laid=0;
					cur_user.sex_today--;
					illness();
					points_raise((INT32)50*(cur_user.level+1));
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");

				} else {

					ny_line(125,2,1);
					//		      od_printf("\n\r\n`bright red`Y`red`ou couldn't find anybody who'd like you...\n\r");
					if(rip)
						od_get_answer("\n\r");

					ny_line(126,0,1);
					//		      od_printf("`bright red`T`red`he sex turns went down anyway...\n\r");

					cur_user.sex_today--;
					wrt_sts;
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				}
			}
		}
		else if (key=='P') {
			if(rip)
				no_rip_m=1;
			if (cur_user.sex_today<=0) {

				ny_line(118,2,1);
				//		    od_printf("\n\r\n`bright`You already used up all your sex turns today ...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");

			} else {
				intval=75+(50*cur_user.level);

				ny_line(127,2,0);
				//od_printf("\n\r\n`bright red`Y`red`ou can get a hooker fer
				od_printf(D_Num(intval));
				ny_line(128,0,0);
				// ... (`bright red`Y`red`/`bright red`N`red`)",D_Num(intval));

				key=ny_get_answer("YN");
				if(!rip)
					od_printf("%c\n\r\n",key);
				else
					od_disp_str("\n\r");
				if (key=='Y') {
					if(intval>cur_user.money) {

						ny_line(252,0,1);
						//you can afford it man

						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else {

						ny_line(129,0,1);
						//		        od_printf("`bright red`O`red`k there you go...\n\r");

						cur_user.since_got_laid=0;
						cur_user.sex_today--;
						money_minus(intval);
						illness();
						points_raise((INT32)25*(cur_user.level+1));
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					}
				}
			}
		} else if (key=='S') {
			if (cur_user.sex_today<=0) {

				ny_line(118,2,1);
				//		    od_printf("\n\r\n`bright`You already used up all your sex turns today ...\n\r");

				if(!rip)
					WaitForEnter();
				else {
					od_get_answer("\n\r");
					no_rip_m=1;
				}

			} else {

				if (cur_user.sex==MALE)
					ny_line(130,2,0); //od_printf("\n\r\n`bright red`Y`red`ou wanna list all the female players (`bright red`Y`red`/`bright red`N`red`)");
				else
					ny_line(131,2,0); //od_printf("\n\r\n`bright red`Y`red`ou wanna list all the male players (`bright red`Y`red`/`bright red`N`red`)");

				key=ny_get_answer("YN");
				if(!rip)
					od_printf("%c\n\r",key);
				else
					od_disp_str("\n\r  \b\b");
				if (key=='Y') {
					if (cur_user.sex==MALE)
						ListPlayersS(FEMALE);
					else
						ListPlayersS(MALE);
					od_printf("\n\r");
				}
				if(rip)
					od_send_file("tframe3.rip");

				ny_line(132,1,0);
				//		    od_printf("\n\r\n`bright red`W`red`ho ya wanna screw (`bright red`full`red` or `bright red`partial`red` name):`bright green`");

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
								//time_slice();
							} while ((strzcmp(hand,ny_un_emu(urec.name,numstr)) || urec.sex==cur_user.sex) && ret==1);
							fclose(justfile);
						}
					} while (ret==1 && askifuser(urec.name)==FALSE);
				}
				if (ret!=1) {

					ny_line(133,1,1);
					if(rip)
						od_get_answer("\n\r");

					//od_printf("\n\r`bright red`G`red`ot no idea who you mean ...");

					//  WaitForEnter();
				} else if (hand[0]!=0) {

					user_rec u2rec;

					ch_game_d();
					justfile=ShareFileOpen(USER_FILENAME,"rb");
					if(justfile != NULL) {
						fseek(justfile,sizeof(user_rec) * (INT32)urec.user_num,SEEK_SET);
						ny_fread(&u2rec,sizeof(user_rec),1,justfile);
						fclose(justfile);
					}

					ny_line(134,1,1);
					//		      od_printf("\n\r`bright red`H`red`ow you gonna ask? (`bright red`/s`red`=save `bright red`/a`red`=abort):\n\r");

					ch_flag_d();
					sprintf(numstr,"u%07d.tmg",nCurrentUserNumber);
					justfile=ShareFileOpen(numstr,"wb");
					if(justfile != NULL) {
						cnt= -1;
						ovr[0]=0;
						do {
							cnt++;
							get_line(ovr,line,ovr,TRUE);
							ny_fwrite(&line,80,1,justfile);
						} while ((line[0]!='/' && (line[1]!='s' || line[1]!='S')) && (line[0]!='/' && (line[1]!='a' || line[1]!='A')));
						fclose(justfile);
					}
					if (line[1]=='s' || line[1]=='S') {
						ny_line(135,0,1);
						//			od_printf("`bright red`S`red`aving...\n\r");
						mail_idx.length=cnt;
						strcpy(mail_idx.sender,cur_user.name);
						strcpy(mail_idx.recver,urec.name);
						strcpy(mail_idx.senderI,cur_user.bbsname);
						strcpy(mail_idx.recverI,u2rec.bbsname);
						mail_idx.afterquote=0;
						mail_idx.deleted=FALSE;
						mail_idx.sender_sex=cur_user.sex;
						mail_idx.flirt=1;
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
								//time_slice();
							}
							fclose(msg_file);
							fclose(justfile);
						}
						ch_flag_d();
						ny_remove(numstr);
						//			sprintf(numstr,"del u%07d.tmg");
						//			system(numstr);
						ch_game_d();
						msg_file=ShareFileOpen(MAIL_INDEX,"a+b");
						if(msg_file != NULL) {
							ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,msg_file);
							fclose(msg_file);
						}
						cur_user.sex_today--;
						if(single_node==FALSE && urec.online==TRUE) {
							ch_flag_d();
							sprintf(numstr,"u%07d.omg",urec.user_num);
							char omg[2];
							omg[0]=27;
							omg[1]=0;
							justfile=ShareFileOpen(numstr,"a+b");
							if(justfile != NULL) {
								ny_fwrite(&omg,51,1,justfile);
								ny_fwrite(&cur_user.name,25,1,justfile);
								fclose(justfile);
							}
						}
						if(rip)
							od_get_answer("\n\r");
					} else {
						ny_line(136,0,1);
						if(rip)
							od_get_answer("\n\r");

						//od_printf("\b\b`bright red`A`red`borted...\n\r");
						//sprintf(numstr,"del u%07d.tmg");
						//system(numstr);
						ch_flag_d();
						ny_remove(numstr);
					}
				}
				if(!rip)
					WaitForEnter();
			}
		}
		else if (key=='M') {
			if(registered==FALSE) {
				ny_disp_emu("`%\n\r\nUNREGISTERED!!!\n\r\nCannot do this!!!\n\r");
				WaitForEnter();
			} else if (cur_user.sex_today<=0) {
				if(rip)
					no_rip_m=1;

				ny_line(118,2,1);
				//od_printf("\n\r\n`bright`You already used up all your sex turns today ...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");

			} else {
				if(rip)
					no_rip_m=1;

				ny_line(137,2,1);
				if(rip)
					od_get_answer("\n\r");
				//		    od_printf("\n\r\n`bright red`W`red`ell it aint the real thing, but at least you don't feel\n\r");
				ny_line(138,0,1);
				//		    od_printf("that you didn't have REAL sex in such a long time, and you can't get sick\n\rlike this.\n\r");

				cur_user.since_got_laid/=2;
				cur_user.sex_today--;
				points_raise((INT32)12*(cur_user.level+1));
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		} else if (key=='B') {
			if(registered==FALSE) {
				ny_disp_emu("`%\n\r\nUNREGISTERED!!!\n\r\nCannot do this!!!\n\r");
				WaitForEnter();
			} else {

				ny_line(139,2,0);
				//od_printf("\n\r\n`bright green`Y`green`ou got `bright green`
				od_printf("%d",cur_user.condoms);
				ny_line(140,0,2);
				//		    `green` condoms.\n\r\n
				if(rip)
					od_get_answer("\n\r");

				if(rip) {
					od_send_file("input.rip");
				}

				longval=cur_user.money/condom_price;
				if (longval+cur_user.condoms>INT_MAX)
					longval=INT_MAX-cur_user.condoms;
				ny_line(141,0,0);
				//od_printf("`bright green`H`green`ow much ya want? (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`
				od_printf(D_Num(longval));
				ny_disp_emu("`2):");

				intval = get_val((DWORD)0,(DWORD)longval);
				cur_user.condoms+=intval;
				money_minus(intval * condom_price);
				wrt_sts();

				ny_line(142,1,0);
				//od_printf("\n\r`bright red`O`red`k you now got `bright red`
				od_printf("%d",cur_user.condoms);
				ny_line(143,0,1);

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		}

	} while (key!='Q');
}


void
disp_fig_stats(void) {
	od_printf("`cyan`Hitpoints: `bright green`%s  ",D_Num(cur_user.hitpoints));
	od_printf("`cyan`Fights Left: `bright green`%d  ",(INT16)cur_user.turns);
	od_printf("`cyan`Points: `bright green`%s  ",D_Num(cur_user.points));
	od_printf("`cyan`Money: `bright green`%s\n\r",D_Num(cur_user.money));
}





void
illness(void)  // std's and stuff
{
	INT16 intval;
	desease ill;

	ill = (desease)(xp_random(AIDS-cur_user.std+2)+cur_user.std-1);
	if (ill>AIDS)
		ill=AIDS;

	if (ill>=cur_user.std && ill!=NONE) {
		cur_user.std=ill;
		if (cur_user.condoms>0) {

			ny_line(144,1,1);
			//      od_printf("\n\r`bright red`Y`red`ou used a condom and got only 1/2 infected...\n\r");

			if(rip)
				od_get_answer("\n\r");
			cur_user.condoms--;
			cur_user.std_percent+=ill*2;
		} else {
			cur_user.std_percent+=ill*4;
		}

		ny_line(145,1,0);
		//od_printf("\n\r`bright red`Y`red`ou are now `bright green`
		od_printf("%d",(INT16)cur_user.std_percent);
		ny_line(146,0,0);
		//    `red`%c infected with `brigh green`",(INT16)cur_user.std_percent,37);
		print_disease(ill);
		wrt_sts();
		if(rip) {
			od_disp_str("::^M@OK))|#|#|#\n\r");
			od_get_answer("\n\r");
		} else
			od_printf("\n\r");


		if (cur_user.std_percent>=100) {
			if(!rip)
				WaitForEnter();
			Die(3);
		}
	}
}





void
illness(desease ill, INT16 inf, INT16 rape)  // std's and stuff from players
{
	//  printf("\n\n%d\n\n",inf);
	//  WaitForEnter();

	if(ill>AIDS)
		ill=AIDS;
	if(cur_user.std>AIDS)
		cur_user.std=AIDS;
	if (ill>=cur_user.std && ill!=NONE) {
		cur_user.std=ill;
		if (cur_user.condoms>0 && rape==FALSE) {
			if(inf<4)
				inf=4;

			ny_line(144,1,1);
			//      od_printf("\n\r`bright red`Y`red`ou used a condom and got only 1/2 infected...\n\r");

			if(rip)
				od_get_answer("\n\r");
			cur_user.condoms--;
			cur_user.std_percent+=inf/4;
		} else {
			if(inf<2)
				inf=2;
			cur_user.std_percent+=inf/2;
		}


		//od_printf("\n\r`bright red`Y`red`ou are now `bright green`%d`red`%c infected with `brigh green`",(INT16)cur_user.std_percent,37);
		ny_line(145,1,0);
		od_printf("%d",(INT16)cur_user.std_percent,37);
		ny_line(146,0,0);
		print_disease(ill);
		wrt_sts();
		if(rip) {
			od_disp_str("::^M@OK))|#|#|#\n\r");
			od_get_answer("\n\r");
		} else
			od_printf("\n\r");


		if (cur_user.std_percent>=100) {
			if(!rip)
				WaitForEnter();
			Die(3);
		}
	}
}



void
money_ops(void) {
	char key;
	DWORD intval;
	INT16 chance;
	DWORD med;
	char hand[25];
	char omg[51];
	char numstr[26];
	char line[80];
	FILE *justfile;
	FILE *msg_file;
	scr_rec urec;
	user_rec u2rec;
	INT16 unum,ret;
	mail_idx_type mail_idx;
	INT32 fillen;


	do {
		key=callmenu("DWGSYQ?\n\r",BANK,349,FALSE);
		while (expert>0 && key=='?') {
			expert+=10;
			key=callmenu("DWGSYQ?\n\r",BANK,349,FALSE);
			expert-=10;
		}

		if(key == 'S') {
			if(rip)
				od_send_file("texti.rip");
			ny_line(416,2,0);
			od_input_str(hand,24,' ',255);
			ny_un_emu(hand);
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
							//time_slice();
						} while ((strzcmp(hand,ny_un_emu(urec.name,numstr)) || urec.user_num==nCurrentUserNumber) && ret==1);
						fclose(justfile);
					}
				} while (ret==1 && askifuser(urec.name)==FALSE);
			}
			if (ret!=1) {
				ny_line(133,1,1);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else if (hand[0]!=0) {
				ch_game_d();
				justfile=ShareFileOpen(USER_FILENAME,"rb");
				if(justfile != NULL) {
					fseek(justfile,sizeof(user_rec) * (INT32)urec.user_num,SEEK_SET);
					ny_fread(&u2rec,sizeof(user_rec),1,justfile);
					fclose(justfile);
				}
				if(rip) {
					od_send_file("input.rip");
				}
				ny_line(417,1,0);
				od_printf(D_Num(cur_user.money));
				ny_disp_emu("`2):");
				intval=get_val((DWORD)0,cur_user.money);
				if (intval>0) {
					cur_user.money-=intval;
					wrt_sts();
					ny_line(418,1,1);
					mail_idx.flirt=1002;
					mail_idx.length=1;
					strcpy(mail_idx.sender,cur_user.name);
					strcpy(mail_idx.senderI,cur_user.bbsname);
					strcpy(mail_idx.recver,urec.name);
					strcpy(mail_idx.recverI,u2rec.bbsname);
					mail_idx.afterquote=0;
					mail_idx.deleted=FALSE;
					mail_idx.sender_sex=cur_user.sex;
					mail_idx.ill=cur_user.std;
					mail_idx.inf=cur_user.std_percent;
					ch_game_d();
					msg_file=ShareFileOpen(MAIL_FILENAME,"a+b");
					if(msg_file != NULL) {
						fillen=filelength(fileno(msg_file));
						fillen/=80;
						mail_idx.location=fillen;
						ny_fwrite(&intval,80,1,msg_file);
						fclose(msg_file);
					}
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
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				}
			}
		} else if (key == 'D') { //deposit in bank

			med=ULONG_MAX-cur_user.money;
			if (med<=cur_user.bank)
				intval=med;
			else
				intval=cur_user.money;

			if(rip) {
				od_send_file("input.rip");
			}

			ny_line(147,2,0);
			//od_printf("\n\r\n`bright green`H`green`ow much ya want to deposit? (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`
			od_printf(D_Num(intval));
			ny_disp_emu("`2):");


			intval=get_val((DWORD)0,intval);
			cur_user.money-=intval;
			cur_user.bank+=intval;
			wrt_sts();

			ny_line(148,1,0);
			//od_printf("\n\r`bright red`Y`red`ou got `bright red`
			od_printf(D_Num(cur_user.bank));
			ny_line(149,0,1);
			//`red` in the bank...\n\r",D_Num(cur_user.bank));

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
		} else if (key=='W') { ///widthraw from bank

			med=ULONG_MAX-cur_user.bank;
			if (med<=cur_user.money)
				intval=med;
			else
				intval=cur_user.bank;
			if(rip) {
				od_send_file("input.rip");
			}

			ny_line(150,2,0);
			//od_printf("\n\r\n`bright green`H`green`ow much ya want to widthraw? (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`
			od_printf(D_Num(intval));
			ny_disp_emu("`2):");

			intval=get_val((DWORD)0,intval);
			cur_user.bank-=intval;
			cur_user.money+=intval;
			wrt_sts();

			ny_line(148,1,0);
			//od_printf("\n\r`bright red`Y`red`ou got `bright red`
			od_printf(D_Num(cur_user.money));
			ny_line(151,0,1);
			//`red` on ya...\n\r",D_Num(cur_user.money));

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
		} else if (key=='Y') {   //display stats
			DisplayStats();
			WaitForEnter();
		} else if (key=='G') {  //steal money
			if(rip)
				no_rip_m=1;
			if (cur_user.level==0) {

				ny_line(113,2,1);
				//od_printf("\n\r\n`bright`Level 0 players can't steall....\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {

				ny_line(152,2,1);
				//od_printf("\n\r\n`bright red`Y`red`ou try to steel some money from the bank...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				chance=xp_random(100);
				if (chance<=busted_ch_bank) {

					od_printf("\n\r\n");
					ny_send_menu(BUSTED,"");
					//od_printf("\n\r\n`bright`BUSTED!!!! .... well the police beat the hell out of you .....\n\rWhy don't you try again tomorrow...\n\r");
					//od_printf("Ya lost 2%c of yer points!\n\r",37);

					cur_user.alive=UNCONCIOUS;
					points_loose(cur_user.points*.02);
					news_post("robbing bank",cur_user.name,"",2);
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					od_exit(10,FALSE);
				} else {
					chance=xp_random(100);
					if (chance<=success_ch_bank) {
						intval=xp_random(cur_user.level*700);

						ny_line(153,2,0);
						//od_printf("\n\r\n`bright red`Y`red`ou just stole
						od_printf(D_Num(intval));
						ny_line(154,0,1);
						// without getting busted, good job...\n\r",intval);

						money_plus(intval);
						points_raise((INT32)55*cur_user.level);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else {
						intval=5*cur_user.level*DrgPtsCoef();

						ny_line(155,2,1);
						if(rip)
							od_get_answer("\n\r");
						//od_printf("\n\r\n`bright red`Y`red`ou did not manage to steal anything but did not get busted...\n\r`bright red`Y`red`our points went up by
						ny_line(117,0,0);
						od_printf("%s\n\r",D_Num(intval));

						points_raise((INT32)20*cur_user.level);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					}
				}
			}
		}
	} while (key!='Q');
}

void
drug_ops(void) {
	char s_key,
	t_key,
	f_key,
	inputs[25];
	INT32  max,
	quant;

	do {
		s_key=callmenu("BSUYCQ?\n\r",DRUGS,350,FALSE);
		while (expert>0 && s_key=='?') {
			expert+=10;
			s_key=callmenu("BSUYCQ?\n\r",DRUGS,350,FALSE);
			expert-=10;
		}

		if (s_key=='Y') {
			DisplayStats();
			WaitForEnter();
		} else if (s_key=='B') {           // buy drugs

			ny_line(156,2,0);
			//od_printf("\n\r\n`bright green`Y`green`er drug of choice is: `bright green`");
			print_drug(cur_user.drug);
			if(rip) {
				od_disp_str("::^M@OK))|#|#|#\n\r");
				od_get_answer("\n\r");
			}


			ny_line(157,2,0);
			//od_printf("\n\r\n`bright green`T`green`he price is `bright green`
			od_printf("%s",D_Num((INT32)(drug_price[cur_user.drug]*(cur_user.level+1))));
			if(!rip)
				od_printf("\n\r\n");
			else {
				od_disp_str("::^M@OK))|#|#|#\n\r");
				od_get_answer("\n\r");
			}
			max=cur_user.money/(drug_price[cur_user.drug]*(cur_user.level+1));
			if ((max+cur_user.drug_hits)>INT_MAX)
				max=INT_MAX-cur_user.drug_hits;
			if ((cur_user.drug_hits+max)>max_drug_hits)
				max=max_drug_hits-cur_user.drug_hits;
			if(rip) {
				od_send_file("input.rip");
			}
			ny_line(158,0,0);
			//od_printf("`bright green`H`green`ow much ya want? (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`
			od_printf("%d",max);
			ny_disp_emu("`2):");


			quant=get_val((DWORD)0,(DWORD)max);

			cur_user.drug_hits+=quant;
			money_minus(quant*(drug_price[cur_user.drug]*(cur_user.level+1)));
			//			    points_raise((DWORD)2*(cur_user.level+1));

			ny_line(159,1,0);
			//			    od_printf("\n\r`bright red`O`red`k you now have `bright red`
			od_printf("%d",cur_user.drug_hits);
			ny_line(160,0,1);
			//`red` hits!\n\r",cur_user.drug_hits);

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
		} else if (s_key=='S') {   // Sell drugs

			ny_line(156,2,0);
			//od_printf("\n\r\n`bright green`Y`green`er drug of choice is: `bright green`");
			print_drug(cur_user.drug);

			if(rip) {
				od_disp_str("::^M@OK))|#|#|#\n\r");
				od_get_answer("\n\r");
			}

			ny_line(161,2,0);
			//od_printf("\n\r\n`bright green`Y`green`a can sell it for `bright green`
			od_printf("%s",D_Num((INT32)(drug_price[cur_user.drug]*(cur_user.level+1))/2));
			if(!rip)
				od_printf("\n\r\n");
			else {
				od_disp_str("::^M@OK))|#|#|#\n\r");
				od_get_answer("\n\r");
			}

			max=cur_user.drug_hits;

			if(rip) {
				od_send_file("input.rip");
			}


			ny_line(162,0,0);
			//od_printf("`bright green`H`green`ow much ya wanna sell? (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`
			od_printf("%d",max);
			ny_disp_emu("`2):");


			quant=get_val((DWORD)0,(DWORD)max);

			cur_user.drug_hits-=quant;
			money_plus(quant*((drug_price[cur_user.drug]*(cur_user.level+1))/2));
			//			    points_raise((DWORD)quant*(cur_user.level+1));

			ny_line(159,1,0);
			od_printf("%d",cur_user.drug_hits);
			ny_line(160,0,1);
			//od_printf("\n\r`bright red`O`red`k you now have `bright red`%d`red` hits!\n\r",cur_user.drug_hits);

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
		} else if (s_key=='C') {      //change drug
			if(rip)
				no_rip_m=1;
			if (cur_user.drug>=COKE && cur_user.drug_addiction>0) {

				ny_line(163,2,1);
				//od_printf("\n\r\n`bright`You are addicted to another drug.... First you heffta get off of that...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else if (cur_user.drug_high>0) {
				ny_line(415,2,1);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {

				//if(rip) ny_clr_scr();
				ny_line(164,2,2);
				//od_printf("\n\r\n`bright red`W`red`hich drug you wanna switch to...\n\r\n`bright green`");
				if(!rip)
					ny_send_menu(CH_DRUG,"");
				/*			      if (cur_user.drug!=POT) od_printf("1`green`. `bright green`P`green`ot\n\r");
							      if (cur_user.drug!=HASH) od_printf("`bright green`2`green`. `bright green`H`green`ash\n\r");
							      if (cur_user.drug!=LSD) od_printf("`bright green`3`green`. `bright green`L`green`SD\n\r");
							      if (cur_user.drug!=COKE) od_printf("`bright green`4`green`. `bright green`C`green`oke\n\r");
							      if (cur_user.drug!=PCP) od_printf("`bright green`5`green`. `bright green`P`green`CP\n\r");
							      if (cur_user.drug!=HEROIN) od_printf("`bright green`6`green`. `bright green`H`green`eroin\n\r\n");*/

				if(!rip)
					ny_line(165,0,0);
				//od_printf("`bright red`S`red`o....(`bright green`[0]`red`=quit):");

				no_kernel=TRUE;
				t_key=ny_get_answer("0123456\n\r");
				no_kernel=FALSE;
				if (t_key=='\n' || t_key=='\r')
					t_key='0';
				if(!rip)
					od_putch(t_key);
				if (t_key!='0') {

					if(cur_user.drug_hits>0) {
						ny_line(166,2,0);

						//od_printf("`bright red`Y`red`ou'll heffta sell all the hits of yer old drug! Do it?(`bright red`Y`red`/`bright red`N`red`)");

						f_key=ny_get_answer("YN");
						if(!rip)
							od_putch(f_key);
					} else {
						f_key='Y';
					}
					if (f_key!='N') {
						money_plus(cur_user.drug_hits*(drug_price[cur_user.drug]*(cur_user.level+1)));
						cur_user.drug_hits=0;

						ny_line(167,2,0);
						//od_printf("`bright red`Y`red`er drug of choice set to: `bright green`");
						if (t_key=='1')
							print_drug(cur_user.drug=POT);
						else if (t_key=='2')
							print_drug(cur_user.drug=HASH);
						else if (t_key=='3')
							print_drug(cur_user.drug=LSD);
						else if (t_key=='4')
							print_drug(cur_user.drug=COKE);
						else if (t_key=='5')
							print_drug(cur_user.drug=PCP);
						else if (t_key=='6')
							print_drug(cur_user.drug=HEROIN);

						wrt_sts();
						if(!rip) {
							od_printf("\n\r");
							WaitForEnter();
						} else {
							od_disp_str("::^M@OK))|#|#|#\n\r");
							od_get_answer("\n\r");
						}
						/*	} else {
						od_printf("\n\r");
						WaitForEnter();*/
					}
				}
			}
		} else if (s_key=='U') {             //use drugs
			take_drug();
		}
	} while (s_key!='Q');
}




void
money_plus(DWORD howmuch) {
	DWORD med;

	med=ULONG_MAX-howmuch;
	if (med<=cur_user.money)
		cur_user.money=ULONG_MAX;
	else
		cur_user.money+=howmuch;
	od_control.od_update_status_now=TRUE;
}





void
money_minus(DWORD howmuch) {
	if (howmuch>=cur_user.money)
		cur_user.money=0;
	else
		cur_user.money-=howmuch;
	od_control.od_update_status_now=TRUE;
}


char
ny_get_answer(const char *string) {
	INT16 len, cnt=0;
	time_t t,t2;
	char key,doker=TRUE;


	len=strlen(string);
	t=time(NULL);
	t2=t;
	if(strcmp(string,"YN")!=0 && no_kernel==FALSE) {
		game_events();
		doker=TRUE;
		fig_ker();
	} else
		doker=FALSE;

	while (1) {
		key=od_get_key(FALSE);
		while (key==0) {
			t=time(NULL);
			if(t>=t2+check_o_nodes) {
				if(doker)
					fig_ker();
				t2=t;
			}

			key=od_get_key(FALSE);
			od_sleep(0);
		}
		cnt=-1;
		while (cnt++<len) {
			if (string[cnt]==key) {
				if(doker)
					fig_ker();
				return key;
			}
		}
		cnt=-1;
		if (key>='a' && key<='z')
			key-=32;
		while (cnt++<len) {
			if (string[cnt]==key) {
				if(doker)
					fig_ker();
				return key;
			}
		}
	}
}

void
game_events(void) {
	INT16 intval;
	INT32 intval2;
	char key;

	if(noevents)
		return;

	intval=xp_random(100);

	if(intval!=50)
		return;

	if(!rip)
		scr_save();

	od_printf("\n\r\n\r");
	if(!rip)
		ny_clr_scr();

	intval=xp_random(4)+1;
	ny_line(183,0,2);
	if(rip)
		od_get_answer("\n\r");

	if(intval==1) {
		if(rip) {
			od_disp_str("\n\r!|10000((*Ya hear a voice ...::^M@OK))|#|#|#\n\r");
			od_get_answer("\n\r");
		}
		if(registered==FALSE) {
			ny_line(421,0,1);
		} else {
			intval=xp_random(3)+1;
			ny_line(421+intval,0,1);
		}
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
	} else if(intval==2) {
		ny_line(184,0,1);
		//    od_printf("`bright red`Y`red`a find a rich new car ...\n\r");
		if(rip)
			od_get_answer("\n\r");
		ny_line(185,0,0);
		//    od_printf("`bright red`S`red`mash it? (`bright red`Y`red`/`bright red`N`red`)");

		key=ny_get_answer("YN");

		if(!rip)
			od_printf("%c\n\r\n",key);
		else
			od_disp_str("\n\r");
		if (key=='N') {
			ny_line(189,0,1);
			//      od_printf("`bright red`Y`red`a consider the risks and decide it ain't a good idea\n\r");

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			goto donewith;
		}

		intval=xp_random(100);
		if (intval<=busted_ch_car) {

			//      od_printf("\n\r\n");
			ny_send_menu(BUSTED,"");
			//od_printf("\n\r\n`bright white`BUSTED!!!! .... well the police beat the hell out of you .....");
			//od_printf("\n\rWhy don't you try again tomorrow...\n\r");
			//od_printf("Ya lost 2%c of yer points!\n\r",37);

			cur_user.alive=UNCONCIOUS;
			points_loose(cur_user.points*.02);
			news_post("busting up a car",cur_user.name,"",2);
			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			od_exit(10,FALSE);
		} else {
			intval=xp_random(100);
			if(intval<=success_ch_car) {

				ny_line(192,0,1);
				//od_printf("\n\r\n`bright red`Y`red`ou totally smashed that car .... a very nice job...\n\r");
				if(rip)
					od_get_answer("\n\r");
				ny_line(117,0,0);
				od_printf("%s",D_Num((INT32)(25*cur_user.level+25) * (INT32)DrgPtsCoef()));

				points_raise((INT32)25 * cur_user.level+25);
				if(!rip) {
					od_disp_str("\n\r");
					WaitForEnter();
				} else {
					od_disp_str("::^M@OK))|#|#|#\n\r");
					od_get_answer("\n\r");
				}
				goto donewith;
			} else {

				ny_line(406,0,1);

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				goto donewith;
			}
		}
	} else if(intval==3) {
		ny_line(425,0,0);
		key=ny_get_answer("YN");
		if(!rip)
			od_printf("%c\n\r\n",key);
		else
			od_disp_str("\n\r");
		if(key=='Y') {
			intval=xp_random(2)+1;
			if(intval==1) {
				intval2=randomf(40*pow(1.5,(double)cur_user.level));
				ny_line(426,0,0);
				od_printf(D_Num(intval));
				ny_line(427,0,0);
				key=ny_get_answer("YN");
				if(!rip)
					od_printf("%c\n\r\n",key);
				else
					od_disp_str("\n\r");
				if (key=='N') {
					ny_line(189,0,1);
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					goto donewith;
				}
				intval=xp_random(3);
				if (intval==0) {
					ny_line(430,0,1);
					if(rip)
						od_get_answer("\n\r");
					ny_line(431,0,1);
					money_minus(cur_user.money);
					cur_user.hitpoints=1;
					wrt_sts();
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					goto donewith;
				}
				ny_line(429,0,1);
				money_plus(intval2);
				points_raise((10 * cur_user.level) + 10);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else {
				ny_line(428,0,0);
				key=ny_get_answer("YN");
				if(!rip)
					od_printf("%c\n\r\n",key);
				else
					od_disp_str("\n\r");
				if (key=='N') {
					ny_line(189,0,1);
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					goto donewith;
				}
				intval=xp_random(5);
				if (intval==0) {
					ny_line(430,0,1);
					if(rip)
						od_get_answer("\n\r");
					ny_line(431,0,1);
					money_minus(cur_user.money);
					cur_user.hitpoints=1;
					wrt_sts();
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					goto donewith;
				}
				ny_line(429,0,1);
				if (cur_user.drug_hits<32767)
					cur_user.drug_hits++;
				points_raise((5 * cur_user.level) + 5);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		}
	} else {
		ny_line(432,0,1);
		if (cur_user.condoms<32767)
			cur_user.condoms++;
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
	}
donewith:
	if(!rip)
		scr_res();
}

DWORD    //value input system
get_val(DWORD def, DWORD max) {
	char input_s[30];
	DWORD intval=0;
	INT16 cnt=0;


uplneZnova:
	;
	sprintf(input_s,"%29.29s","");
	//	strset(input_s,' ');
	input_s[0]= od_get_answer("0123456789M>\n\r");
	if (input_s[0]=='M' || input_s[0]=='>') {
		od_printf("%lu\n\r",max);
		return max;
	} else if (input_s[0]=='\n' || input_s[0]=='\r') {
		od_printf("%lu\n\r",def);
		return def;
	}
	od_printf("%c",input_s[0]);

	cnt=0;
	while (1) {
		if (cnt<29)
			cnt++;
		else
			od_printf("\b");
		input_s[cnt]=od_get_answer("0123456789>\n\r\b");
		if(input_s[cnt]=='>') {
			return max;
		} else if (input_s[cnt]=='\n' || input_s[cnt]=='\r') {
			input_s[cnt]=' ';
			sscanf(input_s,"%lu",&intval);
			if (intval>max) {
				do {
					od_printf("\b \b");
					cnt--;
				} while (cnt>0);
				intval=max;
				od_printf("%lu",max);
			}
			od_printf("\n\r");
			break;
		}
		od_printf("%c",input_s[cnt]);
		if (input_s[cnt]=='\b') {
			od_printf(" \b");
			input_s[cnt]=' ';
			cnt--;
			input_s[cnt]=' ';
			cnt--;
			if (cnt == -1)
				break;
		}
	}
	if (cnt == -1)
		goto uplneZnova;
	return intval;
}





INT16
CheckForHandle(char handle[25]) {
	FILE *fpUserFile;
	INT16	user_num,
	ret_val;
	char numstr[25],numstr2[25];
	user_rec urec;
	scr_rec srec;

	ny_un_emu(handle,numstr);
	ret_val=FALSE;
	ch_game_d();
	fpUserFile = ShareFileOpen(SCR_FILENAME, "rb");
	if(fpUserFile == NULL) {
		fpUserFile = ShareFileOpen(USER_FILENAME, "rb");
		if(fpUserFile == NULL) {

			if(rip)
				ny_clr_scr();
			od_printf("\n\r\nUnable to access the user files ... Exitting ...");
			//
			od_exit(12,FALSE);
		}
		/* Begin with the current user record number set to 0. */
		user_num = 0;
		/* Loop for each record in the file */
		while(ny_fread(&urec, sizeof(user_rec), 1, fpUserFile) == 1) {
			/* If name in record matches the current user name ... */
			if(stricmp(ny_un_emu(urec.name,numstr2), numstr) == 0) {
				/* ... then record that we have found the user's record, */
				ret_val = TRUE;
				ny_line(168,1,1);
				//	od_printf("\n\r\n`bright`Sorry that name is already in use ... choose a different one...\n\r");
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				/* and exit the loop. */
				break;
			}
			/* Move user record number to next user record. */
			user_num++;
			//      time_slice();
		}
		fclose(fpUserFile);
		return ret_val;
	} else {
		/* Begin with the current user record number set to 0. */
		user_num = 0;
		/* Loop for each record in the file */
		while(ny_fread(&srec, sizeof(scr_rec), 1, fpUserFile) == 1) {
			/* If name in record matches the current user name ... */
			if(stricmp(ny_un_emu(srec.name,numstr2), numstr) == 0) {
				/* ... then record that we have found the user's record, */
				ret_val = TRUE;
				ny_line(168,1,1);
				//	od_printf("\n\r\n`bright`Sorry that name is already in use ... choose a different one...\n\r");
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				/* and exit the loop. */
				break;
			}
			/* Move user record number to next user record. */
			user_num++;
		}
		fclose(fpUserFile);
		return ret_val;


	}
}






/* The ReadOrAddCurrentUser() function is used by NY2008 to search the    */
/* NY2008 user file for the record containing information on the user who */
/* is currently using the door. If this is the first time that the user   */
/* has used this door, then their record will not exist in the user file. */
/* In this case, this function will add a new record for the current      */
/* user. This function returns TRUE on success, or FALSE on failure.      */
INT16 ReadOrAddCurrentUser(void) {
	FILE *fpUserFile;
	FILE *scr_file;
	INT16 bGotUser = FALSE;
	//   ffblk ffblk;
	char key;
	char handle[25];
	scr_rec scr_user;

	/* Attempt to open the user file for exclusize access by this node.     */
	/* This function will wait up to the pre-set amount of time (as defined */
	/* near the beginning of this file) for access to the user file.        */

	nCurrentUserNumber=0;
	ch_game_d();
	if (fexist(USER_FILENAME)) {
		fpUserFile = ShareFileOpen(USER_FILENAME, "a+b");


		/* If unable to open user file, return with failure. */
		if(fpUserFile == NULL)
			return FALSE;

		/* Begin with the current user record number set to 0. */
		fseek(fpUserFile, (INT32)0, SEEK_SET);
		//  nCurrentUserNumber = 0; nahore se to ted dela^^^^^

		/* Loop for each record in the file */
		while(ny_fread(&cur_user, sizeof(user_rec), 1, fpUserFile) == 1) {
			/* If name in record matches the current user name ... */
			if(strcmp(cur_user.bbsname, od_control.user_name) == 0) {
				/* ... then record that we have found the user's record, */
				bGotUser = TRUE;

				/* and exit the loop. */
				break;
			}

			/* Move user record number to next user record. */
			nCurrentUserNumber++;
			od_kernal();
		}

		/* Close the user file to allow other nodes to access it. */
		fclose(fpUserFile);
	} //else {

	// }
	/* If the user was not found in the file, attempt to add them as a */
	/* new user if the user file is not already full.                  */
	if(!bGotUser && nCurrentUserNumber < MAX_USERS) {
		od_printf("\n\r\n");
		ny_clr_scr();
		//
		ny_send_menu(NEW,"");

		/*      od_printf("\n\r`bright red`W`red`elcome `bright red`T`red`o `bright red`N`red`ew `bright red`Y`red`ork `bright red`2008\n\r\n");
		      od_printf("`bright green`T`green`his game is quite self explanatory... `bright green`T`green`hough here is yer mission...\n\r");
		      od_printf("`bright green`Y`green`ou are the child of terrorism, your grandfathers could only dream of what you\n\rare about to do ");
		      od_printf("...`bright green`B`green`low up `bright red`N`red`ew `bright red`Y`red`ork `green`... \n\r");
		      od_printf("`bright green`F`green`or this you will need an `bright green`ATOMIC BOMB`green`...\n\r\n");
		      od_printf("`bright`THIS IS THE BETA VERSION (still might have bugs I guess)... \n\r");
		      od_printf("`bright red`H`red`ere are a few tips: (read the instructions anywayz!)\n\r");
		      od_printf("`bright red`I`red`f you get drug addiction of an addictive drug to 100%c you die and heffta start\n\r",37);
		      od_printf("over. `bright red`S`red`ame with hunger and sexual deseases ... so watch those percentages.\n\r");
		      od_printf("`bright red`A`red`lso read instructions on the efects of drugs ...\n\r");
		      od_printf("`bright red`O`red`ne more tip, if you don't have sex fer a while, your attack\n\r");
		      od_printf("force goes down. `bright red`K`red`eep it in mind!\n\r");
		      od_printf("`bright red`C`red`olors can be used in messages and in the stuff you say or namez!\n\r\n");*/



		ny_line(169,0,0);
		//      ny_disp_emu("`9T`1his game contains violence, sex, drugs... `9Y`1a sure ya can handle that?(`9Y`1/`9N`1)");
		//
		key=ny_get_answer("YN");
		od_putch(key);
		if (key=='N') {
			if(!rip)
				ny_clr_scr();
			ny_line(170,2,2);
			//od_printf("\n\r`bright red`N`red`ot everybody was born with a vicious mind....\n\r");
			od_exit(10,FALSE);
		}
AskAgain:
		;

		do {

			od_printf("\n\r\n");
			ny_clr_scr();
			if(rip)
				od_send_file("texti.rip");
			ny_line(171,1,2);
			//	  od_printf("\n\r`bright red`N`red`ow that we got through that, I'll heffta ask ya a few questions...\n\r\n");
			ny_line(172,0,0);
			//od_printf("`bright green`W`green`hat is yer name: ");
			//
			od_input_str(handle,24,' ',255);
			trim(handle);

			//	  WaitForEnter();

		} while ((nCurrentUserNumber!=0 && CheckForHandle(handle)!=FALSE) || handle[0]==0);



		//	  WaitForEnter();

		strcpy(cur_user.name, handle);
		//

		//	  WaitForEnter();
		//
		ny_line(173,1,0);
		//
		//	  od_printf("\n\r\n`bright red`W`red`hat is your sex (`bright red`M`red`/`bright red`F`red`):");

		//	  WaitForEnter();


		key=od_get_answer("MF");
		//
		if(!rip)
			od_printf("%c\n\r\n",key);
		else
			od_disp_str("\n\r");
		//

		if (key=='M')
			cur_user.sex = MALE;
		else
			cur_user.sex = FEMALE;

		if(!rip)
			ny_send_menu(NATION,"");
		/*	  od_printf("\n\r\n`bright red`W`red`hat are you....\n\r\n");
			  od_printf("`bright green`1`green`. `bright green`H`green`eadbanger\n\r");
			  od_printf("`bright green`2`green`. `bright green`H`green`ippie\n\r");
			  od_printf("`bright green`3`green`. `bright green`B`green`ig `bright green`F`green`at `bright green`D`green`ude\n\r");
			  od_printf("`bright green`4`green`. `bright green`C`green`rack `bright green`A`green`ddict\n\r");
			  od_printf("`bright green`5`green`. `bright green`P`green`unk\n\r\n");*/
		ny_line(174,0,0);
		//od_printf("`bright red`W`red`ell? :");
		//
		key=od_get_answer("12345");
		//
		if(!rip)
			od_printf("%c\n\r\n",key);
		else
			od_disp_str("\n\r\n");
		//

		switch(key) {
			case '1':
				cur_user.nation = HEADBANGER;
				break;
			case '2':
				cur_user.nation = HIPPIE;
				break;
			case '3':
				cur_user.nation = BIG_FAT_DUDE;
				break;
			case '4':
				cur_user.nation = CRACK_ADDICT;
				break;
			case '5':
				cur_user.nation = PUNK;
				break;
		}

		ny_send_menu(NEW_WIN,"");
		//What do you say when you win:
		//|--------------------------------------|
		od_input_str(cur_user.say_win,40,' ',255);

		od_printf("\n\r\n");
		ny_send_menu(NEW_LOOSE,"");
		//What do you say when you get yer ass kicked:
		//|--------------------------------------|
		od_input_str(cur_user.say_loose,40,' ',255);


		ny_line(175,2,0);
		//	  od_printf("\n\r\n`bright blue`D`blue`id ya enter all that stuff correctly?(`bright blue`Y`blue`/`bright blue`N`blue`)");
		key=ny_get_answer("YN");
		if(!rip)
			od_putch(key);
		if (key=='N')
			goto AskAgain;

		strcpy(cur_user.bbsname, od_control.user_name);

		/*tady se potrebuje initializovat user*/
		//
		ny_line(176,2,2);
		//	od_printf("`bright blue`\n\r\nResetting User Record....\n\r\n");

		cur_user.level=0;
		cur_user.points=0;
		cur_user.alive=ALIVE;
		cur_user.turns=max_fights;
		cur_user.hunger=0;
		cur_user.hitpoints=cur_user.maxhitpoints=15;

		if (cur_user.nation==HEADBANGER) {
			cur_user.strength=5;
			cur_user.defense=4;
			cur_user.throwing_ability=10;
			cur_user.punch_ability=10;
			cur_user.kick_ability=20;
		} else if (cur_user.nation==HIPPIE) {
			cur_user.strength=4;
			cur_user.defense=4;
			cur_user.throwing_ability=20;
			cur_user.punch_ability=10;
			cur_user.kick_ability=10;
		} else if (cur_user.nation==BIG_FAT_DUDE) {
			cur_user.strength=3;
			cur_user.defense=6;
			cur_user.throwing_ability=4;
			cur_user.punch_ability=18;
			cur_user.kick_ability=18;
		} else if (cur_user.nation==CRACK_ADDICT) {
			cur_user.strength=4;
			cur_user.defense=5;
			cur_user.throwing_ability=14;
			cur_user.punch_ability=13;
			cur_user.kick_ability=13;
		} else if (cur_user.nation==PUNK) {
			cur_user.strength=6;
			cur_user.defense=3;
			cur_user.throwing_ability=11;
			cur_user.punch_ability=20;
			cur_user.kick_ability=11;
		}

		cur_user.days_not_on=0;
		cur_user.money=75;
		cur_user.bank=0;
		cur_user.arm=HANDS;
		cur_user.condoms=0;
		cur_user.since_got_laid=0;
		cur_user.sex_today=1;
		cur_user.std=NONE;
		cur_user.std_percent=0;
		cur_user.drug=POT;
		cur_user.drug_hits=0;
		cur_user.drug_addiction=0;
		cur_user.drug_high=0;
		cur_user.drug_days_since=0;
		cur_user.rank=nCurrentUserNumber;
		cur_user.rest_where=NOWHERE;
		cur_user.hotel_paid_fer=0;
		cur_user.days_in_hospital=0;
		cur_user.rocks=0;
		cur_user.InterBBSMoves=2;
		cur_user.res1=0;
		cur_user.res2=0;
		//	cur_user.res3=0;
		//	cur_user.res4=0;

		strcpy(scr_user.name,cur_user.name);
		scr_user.nation=cur_user.nation;
		scr_user.level=0;
		scr_user.points=0;
		scr_user.alive=ALIVE;
		scr_user.sex=cur_user.sex;
		scr_user.user_num=nCurrentUserNumber;


		ny_line(177,0,2);
		//od_printf("`bright blue`\n\r\nWriting to user file....\n\r\n");
		//      if (bGotUser && cur_user.alive==DEAD && do_maint==FALSE)
		ch_game_d();
		fpUserFile = ShareFileOpen(USER_FILENAME, "a+b");
		scr_file = ShareFileOpen(SCR_FILENAME, "a+b");


		if(fpUserFile != NULL && scr_file != NULL) {

			/* Write the new record to the file. */
			if(ny_fwrite(&cur_user, sizeof(user_rec), 1, fpUserFile) == 1) {
				/* If write succeeded, record that we now have a valid user record. */
				bGotUser = TRUE;
			}
			ny_fwrite(&scr_user, sizeof(scr_rec), 1, scr_file);
			fclose(scr_file);
			fclose(fpUserFile);
		}
		ny_remove(SENTLIST_FILENAME);
	}

	/* Return, indciating whether or not a valid user record now exists for */
	/* the user that is currently online.                                   */
	return bGotUser;
}



/* The WriteCurrentUser() function is called to save the information on the */
/* user who is currently using the door, to the ny2008.USR file.            */
void WriteCurrentUser(void) {
	FILE *fpUserFile;

	/* Attempt to open the user file for exclusize access by this node.     */
	/* This function will wait up to the pre-set amount of time (as defined */
	/* near the beginning of this file) for access to the user file.        */
	ch_game_d();
	fpUserFile = ShareFileOpen(USER_FILENAME, "r+b");

	/* If unable to access the user file, display an error message and */
	/* return.                                                         */
	if(fpUserFile == NULL) {
		if(rip)
			ny_clr_scr();
		od_printf("Unable to access the user file.\n\r");
		WaitForEnter();
		return;
	}

	/* Move to appropriate location in user file for the current user's */
	/* record. */
	fseek(fpUserFile, (INT32)nCurrentUserNumber * sizeof(user_rec), SEEK_SET);

	/* Write the new record to the file. */
	if(ny_fwrite(&cur_user, sizeof(user_rec), 1, fpUserFile) == 0) {
		/* If unable to write the record, display an error message. */
		fclose(fpUserFile);
		if(rip)
			ny_clr_scr();
		od_printf("Unable to update your user record file.\n\r");
		WaitForEnter();
		return;
	}

	/* Close the user file to allow other nodes to access it again. */
	fclose(fpUserFile);
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
	if(!no_slices)
		time_slice();
	return status;
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
	if(!no_slices)
		time_slice();
	return status;
}

/* This function opens the specified file in the specified mode for         */
/* share access by this node; while the file is open, other nodes will      */
/* be able to open the file. This function will wait for up to the number   */
/* of seconds set by FILE_ACCESS_MAX_WAIT, which is defined near the        */
/* beginning of this file.                                                  */
FILE *ShareFileOpen(const char *pszFileName, const char *pszMode) {
	FILE *fpFile = NULL;
	time_t StartTime = time(NULL);
	char numstr[14];
	//   ffblk ffblk;
	//   ny_kernel();


	/* Attempt to open the file while there is still time remaining. */
	if(single_node==FALSE) {
		if(no_kernel==FALSE) {
			ny_kernel();
			sprintf(numstr,"u%07d.rnk",nCurrentUserNumber);
			if (fexist(numstr)) {
				no_kernel=TRUE;
				fpFile=ShareFileOpen(numstr,"rb");
				ny_fread(&cur_user.rank,2,1,fpFile);
				no_kernel=FALSE;
				fclose(fpFile);
				ny_remove(numstr);
				wrt_sts();
			}
		}

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
	return fpFile;
}


FILE *ShareFileOpenAR(const char *pszFileName, const char *pszMode) {
	FILE *fpFile = NULL;
	time_t StartTime = time(NULL);
	char numstr[14];
	//   ffblk ffblk;
	//   ny_kernel();


	/* Attempt to open the file while there is still time remaining. */

	if(single_node==FALSE) {

		if(no_kernel==FALSE) {
			ny_kernel();
			sprintf(numstr,"u%07d.rnk",nCurrentUserNumber);
			if (fexist(numstr)) {
				no_kernel=TRUE;
				fpFile=ShareFileOpen(numstr,"rb");
				ny_fread(&cur_user.rank,2,1,fpFile);
				no_kernel=FALSE;
				fclose(fpFile);
				ny_remove(numstr);
				wrt_sts();
			}
		}

		while((fpFile = fopen(pszFileName, pszMode)) == NULL
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
	return fpFile;
}


INT16
ny_remove(const char *pszFileName) {
	//   FILE *fpFile = NULL;
	time_t StartTime;
	//   char numstr[80];
	//   ffblk ffblk;
	//   ny_kernel();
	char **fname;

	/*  if(findfirst(pszFileName,&fff,0)!=0) return 0; */
	if(glob(pszFileName,0,NULL,&fff)!=0)
		return 0;

	for(fname=fff.gl_pathv;*fname!=NULL;fname++) {
		StartTime = time(NULL);
		/* Attempt to open the file while there is still time remaining. */
		if(single_node==FALSE) {
			while(remove
			        (*fname) == -1
			        && errno == EACCES
			        && difftime(time(NULL), StartTime) < FILE_ACCESS_MAX_WAIT) {
				/* If we were unable to open the file, call od_kernal, so that    */
				/* OpenDoors can continue to respond to sysop function keys, loss */
				/* of connection, etc.                                            */
				od_kernal();
			}
		} else {
			remove
				(*fname);
		}
	};

	return 1;
}



/* The WaitForEnter() function is used to create custom prompt */
void
WaitForEnter(void) {
	/* Display prompt. */

	ny_line(1,1,0);
	//   ny_disp_emu("\n\r`@Smack [ENTER] to go on.");



	/* Wait for a Carriage Return or Line Feed character from the user. */
	ny_get_answer("\n\r");

	//debug
	//   od_printf("%d\n\r",nCurrentUserNumber);
	//   ny_get_answer("\n\r");
}




/* CustomConfigFunction() is called by OpenDoors to process custom */
/* configuration file keywords that                                */
void CustomConfigFunction(char *pszKeyword, char *pszOptions) {
	if(stricmp(pszKeyword, "BustedChanceBank") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_bank);
		if (busted_ch_bank<1)
			busted_ch_bank=1;
		if (busted_ch_bank>90)
			busted_ch_bank=90;
	} else if(stricmp(pszKeyword, "BustedChanceFood") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_food);
		if (busted_ch_food<1)
			busted_ch_food=1;
		if (busted_ch_food>90)
			busted_ch_food=90;
	} else if(stricmp(pszKeyword, "BustedChanceRape") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_rape);
		if (busted_ch_rape<1)
			busted_ch_rape=1;
		if (busted_ch_rape>90)
			busted_ch_rape=90;
	} else if(stricmp(pszKeyword, "BustedChanceBeggar") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_beggar);
		if (busted_ch_beggar<1)
			busted_ch_beggar=1;
		if (busted_ch_beggar>90)
			busted_ch_beggar=90;
	} else if(stricmp(pszKeyword, "BustedChanceCar") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_car);
		if (busted_ch_car<1)
			busted_ch_car=1;
		if (busted_ch_car>90)
			busted_ch_car=90;
	} else if(stricmp(pszKeyword, "BustedChanceSchool") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_school);
		if (busted_ch_school<1)
			busted_ch_school=1;
		if (busted_ch_school>90)
			busted_ch_school=90;
	} else if(stricmp(pszKeyword, "BustedChanceWindow") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_window);
		if (busted_ch_window<1)
			busted_ch_window=1;
		if (busted_ch_window>90)
			busted_ch_window=90;
	} else if(stricmp(pszKeyword, "BustedChancePoison") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_poison);
		if (busted_ch_poison<1)
			busted_ch_poison=1;
		if (busted_ch_poison>90)
			busted_ch_poison=90;
	} else if(stricmp(pszKeyword, "BustedChanceBomb") == 0) {
		sscanf(pszOptions,"%d",&busted_ch_bomb);
		if (busted_ch_bomb<1)
			busted_ch_bomb=1;
		if (busted_ch_bomb>90)
			busted_ch_bomb=90;
	} else if(stricmp(pszKeyword, "SuccessChanceBank") == 0) {
		sscanf(pszOptions,"%d",&success_ch_bank);
		if (success_ch_bank<1)
			success_ch_bank=1;
		if (success_ch_bank>100)
			success_ch_bank=100;
	} else if(stricmp(pszKeyword, "SuccessChanceFood") == 0) {
		sscanf(pszOptions,"%d",&success_ch_food);
		if (success_ch_food<1)
			success_ch_food=1;
		if (success_ch_food>100)
			success_ch_food=100;
	} else if(stricmp(pszKeyword, "SuccessChanceRape") == 0) {
		sscanf(pszOptions,"%d",&success_ch_rape);
		if (success_ch_rape<1)
			success_ch_rape=1;
		if (success_ch_rape>100)
			success_ch_rape=100;
	} else if(stricmp(pszKeyword, "SuccessChanceBeggar") == 0) {
		sscanf(pszOptions,"%d",&success_ch_beggar);
		if (success_ch_beggar<1)
			success_ch_beggar=1;
		if (success_ch_beggar>100)
			success_ch_beggar=100;
	} else if(stricmp(pszKeyword, "SuccessChanceCar") == 0) {
		sscanf(pszOptions,"%d",&success_ch_car);
		if (success_ch_car<1)
			success_ch_car=1;
		if (success_ch_car>100)
			success_ch_car=100;
	} else if(stricmp(pszKeyword, "SuccessChanceSchool") == 0) {
		sscanf(pszOptions,"%d",&success_ch_school);
		if (success_ch_school<1)
			success_ch_school=1;
		if (success_ch_school>100)
			success_ch_school=100;
	} else if(stricmp(pszKeyword, "SuccessChanceWindow") == 0) {
		sscanf(pszOptions,"%d",&success_ch_window);
		if (success_ch_window<1)
			success_ch_window=1;
		if (success_ch_window>100)
			success_ch_window=100;
	} else if(stricmp(pszKeyword, "SuccessChancePoison") == 0) {
		sscanf(pszOptions,"%d",&success_ch_poison);
		if (success_ch_poison<1)
			success_ch_poison=1;
		if (success_ch_poison>100)
			success_ch_poison=100;
	} else if(stricmp(pszKeyword, "SuccessChanceBomb") == 0) {
		sscanf(pszOptions,"%d",&success_ch_bomb);
		if (success_ch_bomb<1)
			success_ch_bomb=1;
		if (success_ch_bomb>100)
			success_ch_bomb=100;
	} else if(stricmp(pszKeyword, "FightsPerDay") == 0) {
		sscanf(pszOptions,"%d",&max_fights);
		if (max_fights<0)
			max_fights=0;
		if (max_fights>800)
			max_fights=800;
	} else if(stricmp(pszKeyword, "MaintExec") == 0) {
		strcpy(maint_exec,pszOptions);
	} else if(stricmp(pszKeyword, "RegCreditTo") == 0) {
		strncpy(rec_credit,pszOptions,35);
	} else if(stricmp(pszKeyword, "SingleNodeOnly") == 0) {
		single_node=TRUE;
	} else if(stricmp(pszKeyword, "NoMultitasker") == 0) {
		no_slices=TRUE;
	} else if(stricmp(pszKeyword, "DeleteAfter") == 0) {
		sscanf(pszOptions,"%d",&delete_after);
		if (delete_after<5)
			delete_after=5;
		if (delete_after>800)
			delete_after=800;
	} else if(stricmp(pszKeyword, "PollingValue") == 0) {
		sscanf(pszOptions,"%d",&time_slice_value);
		if (time_slice_value<0)
			time_slice_value=0;
		if (time_slice_value>2000)
			time_slice_value=2000;
	} else if(stricmp(pszKeyword, "CheckFlagsEvery") == 0) {
		sscanf(pszOptions,"%d",&check_o_nodes);
		if (check_o_nodes<1)
			check_o_nodes=1;
		if (check_o_nodes>60)
			check_o_nodes=60;
	} else if(stricmp(pszKeyword, "BankInterest") == 0) {
		sscanf(pszOptions,"%d",&bank_interest);
		if (bank_interest<0)
			bank_interest=0;
		if (bank_interest>100)
			bank_interest=100;
	} else if(stricmp(pszKeyword, "AnsiScores") == 0) {
		strcpy(ansi_name,pszOptions);
	} else if(stricmp(pszKeyword, "AsciiScores") == 0) {
		strcpy(ascii_name,pszOptions);
	} else if(stricmp(pszKeyword, "NoScoreFiles") == 0) {
		do_scr_files=FALSE;
	} else if(stricmp(pszKeyword, "FlagDirectory") == 0) {
		strupr(pszOptions);

		flagdisk=(*pszOptions) - 'A';

		strzcpy(flagdir,pszOptions,2,MAX_PATH);
	} else if(stricmp(pszKeyword, "InterBBS") == 0) {
		ibbs=TRUE;
	} else if(stricmp(pszKeyword, "InterBBSOperator") == 0) {
		ibbs_operator=TRUE;
	} else if(stricmp(pszKeyword, "InterBBSGameNumber") == 0) {
		sscanf(pszOptions,"%d",&ibbs_game_num);
	}

}

void
nyr_disp_emu(char line[]) {
	INT16 cnt,intval;
	FILE *justfile;

	for(cnt=0;line[cnt]!=0;cnt++) {
		if(line[cnt]=='`') {
			cnt++;
			if(line[cnt]==0)
				return;
			else if(line[cnt]=='`')
				od_putch('`');
			else if(line[cnt]=='v')
				od_printf(ver);
			else if(line[cnt]=='w')
				od_printf(verinfo);
			else if(line[cnt]=='r') {
				if(registered==TRUE) {
					od_printf("%ce%cistere%c to %s",'R','g','d',od_control.system_name);
				} else {
					od_printf("%cN%cE%cISTERE%c",'U','R','G','D');
				}
			} else if(line[cnt]=='d') {
				ch_game_d();
				justfile=ShareFileOpen(GAMEDAY_FILENAME,"rb");
				if(justfile != NULL) {
					ny_fread(&intval,2,1,justfile);
					fclose(justfile);
				}
				od_printf("%d",intval);
			} else if(line[cnt]=='h')
				od_printf("%s",D_Num(cur_user.hitpoints));
			else if(line[cnt]=='m')
				od_printf("%s",D_Num(cur_user.money));
			else if(line[cnt]=='f')
				od_printf("%d",(INT16)cur_user.turns);
			else if(line[cnt]=='p')
				od_printf("%s",D_Num(cur_user.points));
		} else if (line[cnt]==9) {
			od_printf("        ");
		} else {
			od_putch(line[cnt]);
		}
	}
}



void
ny_disp_emu(const char line[]) {
	INT16 cnt;
	//  if(rip) {
	//    od_control.user_rip=FALSE;

	for(cnt=0;line[cnt]!=0;cnt++) {
		if(line[cnt]=='`') {
			cnt++;
			if(!rip) {
				if(line[cnt]==0)
					return;
				else if(line[cnt]=='`')
					od_putch('`');
				else if(line[cnt]=='v')
					od_printf(ver);
				else if(line[cnt]=='w')
					od_printf(verinfo);
				else if(line[cnt]=='0')
					od_set_attrib(0x0a);
				else if(line[cnt]=='1')
					od_set_attrib(0x01);
				else if(line[cnt]=='2')
					od_set_attrib(0x02);
				else if(line[cnt]=='3')
					od_set_attrib(0x03);
				else if(line[cnt]=='4')
					od_set_attrib(0x04);
				else if(line[cnt]=='5')
					od_set_attrib(0x05);
				else if(line[cnt]=='6')
					od_set_attrib(0x06);
				else if(line[cnt]=='7')
					od_set_attrib(0x07);
				else if(line[cnt]=='8')
					od_set_attrib(0x08);
				else if(line[cnt]=='9')
					od_set_attrib(0x09);
				else if(line[cnt]=='!')
					od_set_attrib(0x0b);
				else if(line[cnt]=='@')
					od_set_attrib(0x0c);
				else if(line[cnt]=='#')
					od_set_attrib(0x0d);
				else if(line[cnt]=='$')
					od_set_attrib(0x0e);
				else if(line[cnt]=='%')
					od_set_attrib(0x0f);
			} else {
				if(line[cnt]==0)
					return;
				else if(line[cnt]=='`')
					od_putch('`');
				else if(line[cnt]=='v')
					od_printf(ver);
				else if(line[cnt]=='w')
					od_printf(verinfo);
				else if(line[cnt]=='0')
					od_disp_emu("[1;32m",TRUE);
				else if(line[cnt]=='1')
					od_disp_emu("[0;34m",TRUE);
				else if(line[cnt]=='2')
					od_disp_emu("[0;32m",TRUE);
				else if(line[cnt]=='3')
					od_disp_emu("[0;36m",TRUE);
				else if(line[cnt]=='4')
					od_disp_emu("[0;31m",TRUE);
				else if(line[cnt]=='5')
					od_disp_emu("[0;35m",TRUE);
				else if(line[cnt]=='6')
					od_disp_emu("[0;33m",TRUE);
				else if(line[cnt]=='7')
					od_disp_emu("[0;37m",TRUE);
				else if(line[cnt]=='8')
					od_disp_emu("[1;30m",TRUE);
				else if(line[cnt]=='9')
					od_disp_emu("[1;34m",TRUE);
				else if(line[cnt]=='!')
					od_disp_emu("[1;36m",TRUE);
				else if(line[cnt]=='@')
					od_disp_emu("[1;31m",TRUE);
				else if(line[cnt]=='#')
					od_disp_emu("[1;35m",TRUE);
				else if(line[cnt]=='$')
					od_disp_emu("[1;33m",TRUE);
				else if(line[cnt]=='%')
					od_disp_emu("[1;37m",TRUE);
			}
		} else if (line[cnt]==9) {
			od_printf("        ");
		} else {
			od_putch(line[cnt]);
		}
	}
	//  if(rip) od_control.user_rip=TRUE;
	od_kernal();
}


void
ny_disp_emu(const char line[],INT16 min) {
	INT16 cnt;
	INT16 len;

	len=0;

	//od_control.user_rip=FALSE;

	for(cnt=0;line[cnt]!=0 && len<min;cnt++) {
		if(line[cnt]=='`') {
			cnt++;
			if(!rip) {
				if(line[cnt]==0)
					return;
				else if(line[cnt]=='`') {
					od_putch('`');
					len++;
				} else if(line[cnt]=='v') {
					od_printf(ver);
					len+=(strlen(ver) - 1);
				} else if(line[cnt]=='w') {
					od_printf(verinfo);
					len+=(strlen(verinfo) - 1);
				} else if(line[cnt]=='0')
					od_set_attrib(0x0a);
				else if(line[cnt]=='1')
					od_set_attrib(0x01);
				else if(line[cnt]=='2')
					od_set_attrib(0x02);
				else if(line[cnt]=='3')
					od_set_attrib(0x03);
				else if(line[cnt]=='4')
					od_set_attrib(0x04);
				else if(line[cnt]=='5')
					od_set_attrib(0x05);
				else if(line[cnt]=='6')
					od_set_attrib(0x06);
				else if(line[cnt]=='7')
					od_set_attrib(0x07);
				else if(line[cnt]=='8')
					od_set_attrib(0x08);
				else if(line[cnt]=='9')
					od_set_attrib(0x09);
				else if(line[cnt]=='!')
					od_set_attrib(0x0b);
				else if(line[cnt]=='@')
					od_set_attrib(0x0c);
				else if(line[cnt]=='#')
					od_set_attrib(0x0d);
				else if(line[cnt]=='$')
					od_set_attrib(0x0e);
				else if(line[cnt]=='%')
					od_set_attrib(0x0f);
			} else {
				if(line[cnt]==0)
					return;
				else if(line[cnt]=='`') {
					od_putch('`');
					len++;
				} else if(line[cnt]=='v') {
					od_printf(ver);
					len+=(strlen(ver) - 1);
				} else if(line[cnt]=='w') {
					od_printf(verinfo);
					len+=(strlen(verinfo) - 1);
				} else if(line[cnt]=='0')
					od_disp_emu("[1;32m",TRUE);
				else if(line[cnt]=='1')
					od_disp_emu("[0;34m",TRUE);
				else if(line[cnt]=='2')
					od_disp_emu("[0;32m",TRUE);
				else if(line[cnt]=='3')
					od_disp_emu("[0;36m",TRUE);
				else if(line[cnt]=='4')
					od_disp_emu("[0;31m",TRUE);
				else if(line[cnt]=='5')
					od_disp_emu("[0;35m",TRUE);
				else if(line[cnt]=='6')
					od_disp_emu("[0;33m",TRUE);
				else if(line[cnt]=='7')
					od_disp_emu("[0;37m",TRUE);
				else if(line[cnt]=='8')
					od_disp_emu("[1;30m",TRUE);
				else if(line[cnt]=='9')
					od_disp_emu("[1;34m",TRUE);
				else if(line[cnt]=='!')
					od_disp_emu("[1;36m",TRUE);
				else if(line[cnt]=='@')
					od_disp_emu("[1;31m",TRUE);
				else if(line[cnt]=='#')
					od_disp_emu("[1;35m",TRUE);
				else if(line[cnt]=='$')
					od_disp_emu("[1;33m",TRUE);
				else if(line[cnt]=='%')
					od_disp_emu("[1;37m",TRUE);
			}
		} else if (line[cnt]==9) {
			len+=8;
			if (min-len>=8)
				od_printf("        ");
			else
				od_repeat(' ',min-len);
		} else {
			len++;
			od_putch(line[cnt]);
		}
	}
	//  if(rip) od_control.user_rip=TRUE;
	od_repeat(' ',min-len);
}


char
*ny_un_emu(char line[]) {
	INT16 cnt;
	INT16 len;
	char *out;

	len=strlen(line);

	out=(char *)malloc(len);

	len=0;

	for(cnt=0;line[cnt]!=0;cnt++) {
		if(line[cnt]=='`') {
			cnt++;
			if(line[cnt]==0) {
				out[len]=0;
				return out;
			} else if(line[cnt]=='`') {
				out[len]='`';
				len++;
			} else if(line[cnt]=='v') {
				out[len]=0;
				strcat(out,ver);
				len+=strlen(ver);
			} else if(line[cnt]=='w') {
				out[len]=0;
				strcat(out,verinfo);
				len+=strlen(verinfo);
			}

		} else {
			out[len]=line[cnt];
			len++;
		}
	}
	out[len]=0;
	strcpy(line,out);
	free(out);
	return line;
}

char
*ny_un_emu(char line[],char out[]) {
	INT16 cnt;
	INT16 len;

	len=0;

	for(cnt=0;line[cnt]!=0;cnt++) {
		if(line[cnt]=='`') {
			cnt++;
			if(line[cnt]==0) {
				out[len]=0;
				return out;
			} else if(line[cnt]=='`') {
				out[len]='`';
				len++;
			} else if(line[cnt]=='v') {
				out[len]=0;
				strcat(out,ver);
				len+=strlen(ver);
			} else if(line[cnt]=='w') {
				out[len]=0;
				strcat(out,verinfo);
				len+=strlen(verinfo);
			}

		} else {
			out[len]=line[cnt];
			len++;
		}
	}
	out[len]=0;
	return out;
}


void
ny_disp_emu_file(FILE *ans_phile,FILE *asc_phile,char line[],INT16 min) {
	INT16 cnt;
	INT16 len;

	len=0;

	for(cnt=0;line[cnt]!=0 && len<min;cnt++) {
		if(line[cnt]=='`') {
			cnt++;
			if(line[cnt]==0) {
				while(min>len) {
					fprintf(asc_phile," ");
					fprintf(ans_phile," ");
					len++;
				}
				return;
			} else if(line[cnt]=='`') {
				fprintf(asc_phile,"`");
				fprintf(ans_phile,"`");
			} else if(line[cnt]=='v') {
				fprintf(asc_phile,ver);
				fprintf(ans_phile,ver);
				len+=(strlen(ver) - 1);
			} else if(line[cnt]=='w') {
				fprintf(asc_phile,verinfo);
				fprintf(ans_phile,verinfo);
				len+=(strlen(verinfo) - 1);
			} else if(line[cnt]=='0')
				fprintf(ans_phile,"[1;32m");
			else if(line[cnt]=='1')
				fprintf(ans_phile,"[0;34m");
			else if(line[cnt]=='2')
				fprintf(ans_phile,"[0;32m");
			else if(line[cnt]=='3')
				fprintf(ans_phile,"[0;36m");
			else if(line[cnt]=='4')
				fprintf(ans_phile,"[0;31m");
			else if(line[cnt]=='5')
				fprintf(ans_phile,"[0;35m");
			else if(line[cnt]=='6')
				fprintf(ans_phile,"[0;33m");
			else if(line[cnt]=='7')
				fprintf(ans_phile,"[0;37m");
			else if(line[cnt]=='8')
				fprintf(ans_phile,"[1;30m");
			else if(line[cnt]=='9')
				fprintf(ans_phile,"[1;34m");
			else if(line[cnt]=='!')
				fprintf(ans_phile,"[1;36m");
			else if(line[cnt]=='@')
				fprintf(ans_phile,"[1;31m");
			else if(line[cnt]=='#')
				fprintf(ans_phile,"[1;35m");
			else if(line[cnt]=='$')
				fprintf(ans_phile,"[1;33m");
			else if(line[cnt]=='%')
				fprintf(ans_phile,"[1;37m");
		} else {
			len++;
			fprintf(asc_phile,"%c",line[cnt]);
			fprintf(ans_phile,"%c",line[cnt]);
		}
	}
	while(min>len) {
		fprintf(asc_phile," ");
		fprintf(ans_phile," ");
		len++;
	}
}

void
scr_save(void) {
	if(savelevel==0) {
		if(rip) {
			od_disp_str("\n\r!|10000$SAVEALL$|#|#|#\n\r");
		} else {
			t_buffer=(char *)malloc(4004);
			if(t_buffer!=NULL)
				od_save_screen(t_buffer);
		}
		savelevel=1;
	} else if(savelevel==1) {
		if(rip) {
			od_disp_str("\n\r!|10000$SAVE0$|#|#|#\n\r");
		} else {
			t_buffer1=(char *)malloc(4004);
			if(t_buffer1!=NULL)
				od_save_screen(t_buffer1);
		}
		savelevel=2;
	} else {
		savelevel++;
	}
}

void
scr_res(void) {
	if(savelevel==1) {
		if(rip) {
			od_disp_str("\n\r!|10000$RESTOREALL$|#|#|#\n\r  \b\b");
		} else {
			if(t_buffer!=NULL) {
				od_restore_screen(t_buffer);
				free(t_buffer);
			}
		}
		savelevel=0;
	} else if(savelevel==2) {
		if(rip) {
			od_disp_str("\n\r!|10000$RESTORE0$|#|#|#\n\r  \b\b");
		} else {
			if(t_buffer1!=NULL) {
				od_restore_screen(t_buffer1);
				free(t_buffer1);
			}
		}
		savelevel=1;
	} else if(savelevel>2) {
		savelevel--;
	}
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


void
ny_clr_scr(void) {
	if(rip) {
		od_control.user_rip=FALSE;
		od_disp_str("\n\r!|*|#|#|#\n\r");
		od_clr_scr();
		od_control.user_rip=TRUE;
		od_printf("\r\r");
		od_kernal();
	} else {
		od_clr_scr();
	}
	// if(rip) od_disp_str("\n\r\n\r");
}

INT16
ibbs_bbs_list(void) {
	INT16 cnt,cnt2,mine;
	INT16 nonstop=FALSE;
	char numstr[4],key;

	if(ibbs==FALSE || IBBSInfo.paOtherSystem==NULL || IBBSInfo.nTotalSystems==0)
		return -1;

	if(rip) {
		od_send_file("frame.rip");
		od_send_file("frame3.rip");
	} else {
		od_disp_str("\n\r\n\r");
		ny_clr_scr();
	}

	ny_send_menu(LIST_IB_SYS,"");

	cnt2=5;
	for(cnt=0;cnt<IBBSInfo.nTotalSystems;cnt++) {
		if(strcmp(IBBSInfo.szThisNodeAddress,IBBSInfo.paOtherSystem[cnt].szAddress)==0) {
			mine=cnt;
		} else {
			od_printf("`bright`%d - `bright green`%s `dark green`%.32s\n\r",cnt,IBBSInfo.paOtherSystem[cnt].szSystemName,IBBSInfo.paOtherSystem[cnt].szLocation);
			cnt2++;
			if (nonstop==FALSE && cnt2%od_control.user_screen_length==0) {
				ny_disp_emu("`%More (Y/n/=)");
				key=ny_get_answer("YN=\n\r");
				od_printf("\r            \r");
				cnt2=2;
				if(key=='N')
					break;
				else if(key=='=')
					nonstop=TRUE;
			}
		}
	}

	if(rip)
		od_send_file("frame.rip");

	do {
		ny_line(448,1,0);
		od_input_str(numstr,3,'0','9');
		if(numstr[0]==0)
			return -1;
		sscanf(numstr,"%d",&cnt);
		od_disp_str("\n\r");
	} while(cnt>=IBBSInfo.nTotalSystems || cnt==mine);

	return cnt;
}

void
ibbs_bbs_scores(void) {
	INT16 cnt,cnt2;
	INT16 nonstop=FALSE;
	ibbs_bbs_spy_rec bbs_spy_rec;
	scr_rec srec;
	INT32 filepos;
	FILE *justfile,*jfile;
	char key;

	if(ibbs==FALSE || IBBSInfo.paOtherSystem==NULL || IBBSInfo.nTotalSystems==0)
		return;


	justfile=ShareFileOpen(IBBSSPY_FILENAME,"rb");
	if(justfile==NULL) {
		ny_line(450,2,1);
		WaitForEnter();
		return;
	}

	if(rip) {
		od_send_file("frame.rip");
		od_send_file("frame3.rip");
	} else {
		od_disp_str("\n\r\n\r");
		ny_clr_scr();
	}

	ny_send_menu(LIST_IB_SYS,"");
	ny_line(453,0,1);

	cnt2=5;
	while(ny_fread(&bbs_spy_rec,sizeof(ibbs_bbs_spy_rec),1,justfile)==1) {
		for(cnt=0;cnt<IBBSInfo.nTotalSystems;cnt++) {
			if(strcmp(bbs_spy_rec.node,IBBSInfo.paOtherSystem[cnt].szAddress)==0) {
				if(strcmp(bbs_spy_rec.node,IBBSInfo.szThisNodeAddress)==0) {
					jfile=ShareFileOpen(SCR_FILENAME,"rb");
					if(justfile != NULL) {
						ny_fread(&srec,sizeof(scr_rec),1,jfile);
						fclose(jfile);
					}
					od_printf("`bright green`%-40s `dark green`%s\n\r",IBBSInfo.paOtherSystem[cnt].szSystemName,D_Num(srec.points));
				} else {
					od_printf("`bright green`%-40s `dark green`%s\n\r",IBBSInfo.paOtherSystem[cnt].szSystemName,D_Num(bbs_spy_rec.hi_points));
					break;
				}
			}
		}
		cnt2++;
		if (nonstop==FALSE && cnt2%od_control.user_screen_length==0) {
			filepos=ftell(justfile);
			fclose(justfile);
			ny_disp_emu("`%More (Y/n/=)");
			key=ny_get_answer("YN=\n\r");
			od_printf("\r            \r");
			cnt2=1;
			justfile=ShareFileOpen(IBBSSPY_FILENAME,"rb");
			if(justfile == NULL) {
				ny_line(450,2,1);
				WaitForEnter();
				return;
			}
			fseek(justfile,filepos,SEEK_SET);
			if(key=='N')
				break;
			else if(key=='=')
				nonstop=TRUE;
		}
	}
	fclose(justfile);
	if(rip)
		od_send_file("frame1.rip");
	WaitForEnter();
}


void
ibbs_bbs_name(INT16 bbs,INT16 sex,INT16 nochoice,char nameI[],INT16 *dbn,INT16 *pn) {
	FILE *justfile;
	ibbs_bbs_spy_rec bbs_spy_rec;
	ibbs_scr_rec ibscr_rec;
	INT16 cnt,nonstop=FALSE;
	INT16 cnt2;
	char numstr[26];
	INT32 filepos;
	char key;
	nameI[0]=0;

	*dbn=0;
	*pn=-1;

	justfile=ShareFileOpen(IBBSSPY_FILENAME,"rb");
	if(justfile==NULL) {
		ny_line(450,0,1);
		WaitForEnter();
		return;
	}

	if(rip) {
		od_send_file("frame.rip");
		od_send_file("frame3.rip");
	} else {
		od_disp_str("\n\r\n\r");
		ny_clr_scr();
	}

	ny_line(452,1,0);
	od_disp_str(IBBSInfo.paOtherSystem[bbs].szSystemName);
	ny_line(451,2,1);

	while(ny_fread(&bbs_spy_rec,sizeof(ibbs_bbs_spy_rec),1,justfile)==1) {
		if(strcmp(IBBSInfo.paOtherSystem[bbs].szAddress,bbs_spy_rec.node)==0) {
			if(bbs_spy_rec.player_list!=0) {
				sprintf(numstr,SBYDB_PREFIX".%03d",bbs_spy_rec.player_list);
				*dbn=bbs_spy_rec.player_list;
				break;
			} else {
				ny_line(450,0,1);
				WaitForEnter();
				return;
			}
		}
	}
	fclose(justfile);

	justfile=ShareFileOpen(numstr,"rb");
	if(justfile==NULL) {
		ny_line(450,0,1);
		WaitForEnter();
		return;
	}
	cnt2=4;

	if(nochoice) {
		ny_line(457,0,1);
		cnt2++;
	}

	cnt=0;
	while(ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile)==1) {
		if(sex==0 || (sex==1 && ibscr_rec.sex==FEMALE) || (sex==2 && ibscr_rec.sex==MALE)) {
			if(nochoice==FALSE) {
				od_printf("`bright`%d - `bright red`",cnt);
				ny_disp_emu(ibscr_rec.name);
				od_disp_str("\n\r");
			} else {
				if(ibscr_rec.level>=0) {
					od_set_attrib(0x0a);
					od_printf("%-2d   `bright red`",ibscr_rec.level);
					ny_disp_emu(ibscr_rec.name,25);
					od_set_attrib(0x0a);
					od_printf(" %-20s ",D_Num(ibscr_rec.points));
					if (ibscr_rec.sex==MALE)
						ny_disp_emu("`$M  ");
					else
						ny_disp_emu("`$F  ");
					od_set_attrib(0x09);
					switch(ibscr_rec.nation) {
						case HEADBANGER:
							od_printf("HEADBANGER   ");
							break;
						case HIPPIE:
							od_printf("HIPPIE       ");
							break;
						case BIG_FAT_DUDE:
							od_printf("BIG FAT DUDE ");
							break;
						case CRACK_ADDICT:
							od_printf("CRACK ADDICT ");
							break;
						case PUNK:
							od_printf("PUNK         ");
							break;
					}
				} else {
					ny_disp_emu("`@     ");
					ny_disp_emu(ibscr_rec.name,25);
					ny_disp_emu(" `%No Spy Info Available");
				}
				od_disp_str("\n\r");
			}
			cnt2++;
			if (nonstop==FALSE && cnt2%od_control.user_screen_length==0) {
				filepos=ftell(justfile);
				fclose(justfile);
				ny_disp_emu("`%More (Y/n/=)");
				key=ny_get_answer("YN=\n\r");
				od_printf("\r            \r");
				cnt2=2;
				justfile=ShareFileOpen(numstr,"rb");
				if(justfile==NULL) {
					ny_line(450,0,1);
					WaitForEnter();
					return;
				}
				fseek(justfile,filepos,SEEK_SET);
				if(key=='N')
					break;
				else if(key=='=')
					nonstop=TRUE;
			}
		}
		cnt++;
	}
	fclose(justfile);

	if(nochoice==FALSE) {
		if(rip)
			od_send_file("frame.rip");
		do {
			ny_line(449,1,0);
			od_input_str(numstr,3,'0','9');
			if(numstr[0]==0)
				return;
			sscanf(numstr,"%d",&cnt);
			od_disp_str("\n\r");
			if(sex>0 && cnt<bbs_spy_rec.players) {
				justfile=ShareFileOpen(numstr,"rb");
				if(justfile==NULL) {
					ny_line(450,0,1);
					WaitForEnter();
					return;
				}
				fseek(justfile,cnt*sizeof(ibbs_scr_rec),SEEK_SET);
				ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
				strcpy(nameI,ibscr_rec.nameI);
				fclose(justfile);
				if(sex==1 && ibscr_rec.sex==MALE)
					cnt=bbs_spy_rec.players;
				else if(sex==2 && ibscr_rec.sex==FEMALE)
					cnt=bbs_spy_rec.players;
			}
		} while(cnt>=bbs_spy_rec.players);
		justfile=ShareFileOpen(numstr,"rb");
		if(justfile==NULL) {
			ny_line(450,0,1);
			WaitForEnter();
			return;
		}
		fseek(justfile,cnt*sizeof(ibbs_scr_rec),SEEK_SET);
		ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
		strcpy(nameI,ibscr_rec.nameI);
		fclose(justfile);
		*pn=cnt;
	} else {
		if(rip)
			od_send_file("frame1.rip");
		WaitForEnter();
	}
}

void
ibbs_ops(void) {
	char key;
	char hand[36];
	char omg[51];
	char numstr[26];
	//	char node_r[NODE_ADDRESS_CHARS + 1];
	char line[80],ovr[80];
	FILE *justfile;
	FILE *msg_file;
	scr_rec urec;
	user_rec u2rec;
	INT16 unum,ret,cnt;
	ibbs_mail_type ibmail;
	ibbs_scr_rec ibscr_rec;
	INT32 fillen;
	INT16 dbn,pn,intval;
	ibbs_act_rec act_rec;
	DWORD money;
	//	desease ill;
	newzfile_t	newzfile;

	ovr[0]=0;
	line[0]=0;

	do {
		key=callmenu("LSMBHIPDXAYQ?\n\r",IBBS_MENU,454,FALSE);
		while (expert>0 && key=='?') {
			expert+=10;
			key=callmenu("LSMBHIPDXAYQ?\n\r",IBBS_MENU,454,FALSE);
			expert-=10;
		}

		if (key=='P') {
			if(cur_user.InterBBSMoves<=0) {
				ny_line(455,2,1);
				if(rip) {
					od_get_answer("\n\r");
					no_rip_m=1;
				} else
					WaitForEnter();
			} else {
				wrt_sts();
				intval=ibbs_bbs_list();
				if(intval>-1) {
					ibbs_bbs_name(intval,0,FALSE,hand,&dbn,&pn);
					if(hand[0]!=0) {
						cur_user.InterBBSMoves--;
						sprintf(numstr,SBYDB_PREFIX".%03d",dbn);
						justfile=ShareFileOpen(numstr,"rb");
						if(justfile!=NULL) {
							fseek(justfile,pn*sizeof(ibbs_scr_rec),SEEK_SET);
							ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
							fclose(justfile);
						}

						strcpy(act_rec.name_rI,ibscr_rec.nameI);
						strcpy(act_rec.name_sI,cur_user.bbsname);
						strcpy(act_rec.node_s,IBBSInfo.szThisNodeAddress);
						act_rec.action=1;
						sprintf(IBBSInfo.szProgName, "#@NYG#%05d ACTIONS",ibbs_game_num);
						IBSend(&IBBSInfo,IBBSInfo.paOtherSystem[intval].szAddress,(char *)&act_rec,sizeof(ibbs_act_rec));
						ny_line(456,0,1);
						if(rip)
							od_get_answer("\n\r");
						else
							WaitForEnter();
					}
				}
			}
		} else if (key=='Y') {
			DisplayStats();
			WaitForEnter();
		} else if (key=='L') {
			ibbs_bbs_scores();
		} else if (key=='S') {
			intval=ibbs_bbs_list();
			if(intval>-1) {
				ibbs_bbs_name(intval,0,FALSE,hand,&dbn,&pn);
				if(hand[0]!=0) {
					sprintf(numstr,SBYDB_PREFIX".%03d",dbn);
					justfile=ShareFileOpen(numstr,"rb");
					if(justfile!=NULL) {
						fseek(justfile,pn*sizeof(ibbs_scr_rec),SEEK_SET);
						ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
						fclose(justfile);
					}

					ibmail.flirt=0;
					if (ibscr_rec.sex!=cur_user.sex && cur_user.sex_today>0) {

						ny_line(328,1,0);
						//	          od_printf("\n\r`bright red`A`red`sk `bright red`
						if(!rip)
							ny_disp_emu(ibscr_rec.name);
						else
							od_disp_str(ny_un_emu(ibscr_rec.name,numstr));
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

					/*
					ny_line(330,1,0);
					//od_printf("\n\r`bright red`Q`red`uote message? (`bright red`Y`red`/`bright red`N`red`)");

					key=ny_get_answer("YN");
					if(!rip)
					od_printf("%c\n\r",key);
					else
					od_disp_str("\n\r");

					if(key=='Y') {
					ibmail.quote_length=ibmail.length;
					for(cnt=0;cnt<10;cnt++)
					strcpy(ibmail.quote_lines[cnt],ibmail.lines[cnt]);
					} else {
					ibmail.quote_length=0;
					}*/
					ibmail.quote_length=0;
					ny_line(446,1,1);
					//od_printf("\n\r`bright red`O`red`k type yer message now (`bright red`/s`red`=save `bright red`/a`red`=abort):\n\r");
					cnt=-1;
					ovr[0]=0;
					do {
						cnt++;
						if(cnt<9) {
							get_line(ovr,ibmail.lines[cnt],ovr,TRUE);
						} else {
							get_line(ovr,ibmail.lines[cnt],ovr,FALSE);
							cnt++;
						}
					} while ((ibmail.lines[cnt][0]!='/' && ibmail.lines[cnt][1]!='S') && (ibmail.lines[cnt][0]!='/' && ibmail.lines[cnt][1]!='A') && cnt<9);

					if (ibmail.lines[cnt][1]=='s' || ibmail.lines[cnt][1]=='S' || cnt==9) {
						ny_line(135,0,1);
						//	          od_printf("\b\b`bright red`S`red`aving...\n\r");
						//cnt+=mail_idx.afterquote;
						ibmail.length=cnt;
						strcpy(ibmail.recver,ibscr_rec.name);
						strcpy(ibmail.sender,cur_user.name);
						strcpy(ibmail.node_r,IBBSInfo.paOtherSystem[intval].szAddress);
						strcpy(ibmail.node_s,IBBSInfo.szThisNodeAddress);
						strcpy(ibmail.recverI,ibscr_rec.nameI);
						strcpy(ibmail.senderI,cur_user.bbsname);
						ibmail.deleted=FALSE;
						ibmail.sender_sex=cur_user.sex;
						ibmail.ill=cur_user.std;
						ibmail.inf=cur_user.std_percent;
						sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);

						IBSendMail(&IBBSInfo,&ibmail);

						//IBSend(&IBBSInfo,IBBSInfo.paOtherSystem[intval].szAddress,&ibmail,sizeof(ibbs_mail_type)-((20-ibmail.length)*81));


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
		} else if (key=='X') {
			intval=ibbs_bbs_list();
			if(intval>-1) {
				if(cur_user.sex==MALE)
					ibbs_bbs_name(intval,1,FALSE,hand,&dbn,&pn);
				else
					ibbs_bbs_name(intval,2,FALSE,hand,&dbn,&pn);

				if(hand[0]!=0) {
					sprintf(numstr,SBYDB_PREFIX".%03d",dbn);
					justfile=ShareFileOpen(numstr,"rb");
					if(justfile!=NULL) {
						fseek(justfile,pn*sizeof(ibbs_scr_rec),SEEK_SET);
						ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
						fclose(justfile);
					}

					ibmail.flirt=0;
					if (ibscr_rec.sex!=cur_user.sex && cur_user.sex_today>0) {
						ibmail.flirt=1;
						cur_user.sex_today--;
						ibmail.quote_length=0;
						ny_line(134,1,1);

						cnt=-1;
						ovr[0]=0;
						do {
							cnt++;
							if(cnt<9) {
								get_line(ovr,ibmail.lines[cnt],ovr,TRUE);
							} else {
								get_line(ovr,ibmail.lines[cnt],ovr,FALSE);
								cnt++;
							}
						} while ((ibmail.lines[cnt][0]!='/' && ibmail.lines[cnt][1]!='S') && (ibmail.lines[cnt][0]!='/' && ibmail.lines[cnt][1]!='A') && cnt<9);

						if (ibmail.lines[cnt][1]=='s' || ibmail.lines[cnt][1]=='S' || cnt==9) {
							ny_line(135,0,1);
							//	            od_printf("\b\b`bright red`S`red`aving...\n\r");
							//cnt+=mail_idx.afterquote;
							ibmail.length=cnt;
							strcpy(ibmail.recver,ibscr_rec.name);
							strcpy(ibmail.sender,cur_user.name);
							strcpy(ibmail.node_r,IBBSInfo.paOtherSystem[intval].szAddress);
							strcpy(ibmail.node_s,IBBSInfo.szThisNodeAddress);
							strcpy(ibmail.recverI,ibscr_rec.nameI);
							strcpy(ibmail.senderI,cur_user.bbsname);
							ibmail.deleted=FALSE;
							ibmail.sender_sex=cur_user.sex;
							ibmail.ill=cur_user.std;
							ibmail.inf=cur_user.std_percent;
							sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);

							IBSendMail(&IBBSInfo,&ibmail);

							//IBSend(&IBBSInfo,IBBSInfo.paOtherSystem[intval].szAddress,&ibmail,sizeof(ibbs_mail_type)-((20-ibmail.length)*81));


						} else {
							ny_line(136,0,1);
							cur_user.sex_today++;
						}

						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else if (cur_user.sex_today<=0) {

						ny_line(118,1,1);
						//od_printf("\n\r\n\r`bright`You already used up all your sex turns today ...\n\r");

						WaitForEnter();
						od_printf("\n\r");
					}
				}
			}
		} else if (key=='M') {
			if(cur_user.InterBBSMoves<=0) {
				ny_line(455,2,1);
				if(rip) {
					od_get_answer("\n\r");
					no_rip_m=1;
				} else
					WaitForEnter();
			} else {
				intval=ibbs_bbs_list();
				if(intval>-1) {
					ibbs_bbs_name(intval,0,FALSE,hand,&dbn,&pn);
					if(hand[0]!=0) {
						cur_user.InterBBSMoves--;
						sprintf(numstr,SBYDB_PREFIX".%03d",dbn);
						justfile=ShareFileOpen(numstr,"rb");
						if(justfile!=NULL) {
							fseek(justfile,pn*sizeof(ibbs_scr_rec),SEEK_SET);
							ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
							fclose(justfile);
						}

						ibmail.flirt=1002;
						ibmail.quote_length=0;

						if(rip) {
							od_send_file("input.rip");
							od_disp_str("\n\r");
						}
						ny_line(417,0,0);
						od_printf(D_Num(cur_user.money));
						ny_disp_emu("`2):");
						money=get_val((DWORD)0,cur_user.money);

						*(DWORD *)ibmail.lines[0]=money;

						if(money!=0) {

							ny_line(135,1,1);
							//	            od_printf("\b\b`bright red`S`red`aving...\n\r");
							//cnt+=mail_idx.afterquote;
							ibmail.length=1;
							strcpy(ibmail.recver,ibscr_rec.name);
							strcpy(ibmail.sender,cur_user.name);
							strcpy(ibmail.node_r,IBBSInfo.paOtherSystem[intval].szAddress);
							strcpy(ibmail.node_s,IBBSInfo.szThisNodeAddress);
							strcpy(ibmail.recverI,ibscr_rec.nameI);
							strcpy(ibmail.senderI,cur_user.bbsname);
							ibmail.deleted=FALSE;
							ibmail.sender_sex=cur_user.sex;
							ibmail.ill=cur_user.std;
							ibmail.inf=cur_user.std_percent;
							sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);

							IBSendMail(&IBBSInfo,&ibmail);

							//IBSend(&IBBSInfo,IBBSInfo.paOtherSystem[intval].szAddress,&ibmail,sizeof(ibbs_mail_type)-((20-ibmail.length)*81));

							if(!rip)
								WaitForEnter();
							else
								od_get_answer("\n\r");
						} else {
							cur_user.InterBBSMoves++;
						}
					}
				}
			}
		} else if (key=='A') {

			if(rip)
				od_send_file("texti.rip");
			ny_line(309,2,1);
			//Say what:
			ny_line(310,0,1);
			//|-----------------------------------------------------------------------------|

			od_input_str(line,79,' ',255);
			if(line[0]!=0) {
				if(dobad)
					dobadwords(line);
				news_post(line,cur_user.name,"",1);
				strcpy(newzfile.tagline,line);
				strcpy(newzfile.name,cur_user.name);
				strncpy(newzfile.name2,LocationOf(IBBSInfo.szThisNodeAddress),35);
				newzfile.name2[35]=0;
				newzfile.flag=6;
				sprintf(IBBSInfo.szProgName, "#@NYG#%05d NEWS",ibbs_game_num);
				IBSendAll(&IBBSInfo,(char *)&newzfile,sizeof(newzfile));
				ny_line(311,2,1);
				//Posted...
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			}
		} else if (key=='B') {
			if(cur_user.InterBBSMoves<=0) {
				ny_line(455,2,1);
				if(rip) {
					od_get_answer("\n\r");
					no_rip_m=1;
				} else
					WaitForEnter();
			} else {
				money=15 + (15*pow(1.7,(double)cur_user.level));
				ny_line(467,2,0);
				od_printf("%s",D_Num(money));
				ny_line(468,0,0);
				key=ny_get_answer("YN");
				if(key=='Y') {
					if(money>cur_user.money) {
						ny_line(252,2,1);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else {
						intval=ibbs_bbs_list();
						if(intval>-1) {
							ibbs_bbs_name(intval,0,FALSE,hand,&dbn,&pn);
							if(hand[0]!=0) {
								cur_user.InterBBSMoves--;
								money_minus(money);
								sprintf(numstr,SBYDB_PREFIX".%03d",dbn);
								justfile=ShareFileOpen(numstr,"rb");
								if(justfile!=NULL) {
									fseek(justfile,pn*sizeof(ibbs_scr_rec),SEEK_SET);
									ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
									fclose(justfile);
								}

								ibmail.flirt=1005;
								ibmail.quote_length=0;

								ny_line(135,0,1);
								//	              od_printf("\b\b`bright red`S`red`aving...\n\r");
								//cnt+=mail_idx.afterquote;
								ibmail.length=1;
								strcpy(ibmail.recver,ibscr_rec.name);
								strcpy(ibmail.sender,cur_user.name);
								strcpy(ibmail.node_r,IBBSInfo.paOtherSystem[intval].szAddress);
								strcpy(ibmail.node_s,IBBSInfo.szThisNodeAddress);
								strcpy(ibmail.recverI,ibscr_rec.nameI);
								strcpy(ibmail.senderI,cur_user.bbsname);
								ibmail.deleted=FALSE;
								ibmail.sender_sex=cur_user.sex;
								ibmail.ill=cur_user.std;
								ibmail.inf=cur_user.std_percent;
								sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);

								IBSendMail(&IBBSInfo,&ibmail);

								//IBSend(&IBBSInfo,IBBSInfo.paOtherSystem[intval].szAddress,&ibmail,sizeof(ibbs_mail_type)-((20-ibmail.length)*81));

								if(!rip)
									WaitForEnter();
								else
									od_get_answer("\n\r");
							}
						}
					}
				}
			}
		} else if (key=='H') {
			if(cur_user.InterBBSMoves<=0) {
				ny_line(455,2,1);
				if(rip) {
					od_get_answer("\n\r");
					no_rip_m=1;
				} else
					WaitForEnter();
			} else {

				od_disp_str("\n\n\r");

				if(!rip)
					ny_send_menu(HITMEN,"");

				ny_line(470,0,0);

				no_kernel=TRUE;
				key=ny_get_answer("123456Q");
				no_kernel=FALSE;
				if(key!='Q') {
					intval=key-'1'+1;
					money=(15 + (15*pow(1.7,(double)cur_user.level))) * intval;
					ny_line(469,2,0);
					od_printf("%s",D_Num(money));
					ny_line(468,0,0);
					key=ny_get_answer("YN");
				}
				if(key=='Y') {
					if(money>cur_user.money) {
						ny_line(252,2,1);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else {
						*(INT16 *)ibmail.lines[0]=intval-1;
						if(intval==1)
							strcpy(ibmail.lines[1],"George");
						else if(intval==2)
							strcpy(ibmail.lines[1],"Gang Member");
						else if(intval==3)
							strcpy(ibmail.lines[1],"Mafia Guy");
						else if(intval==4)
							strcpy(ibmail.lines[1],"Lloyd The Bloody");
						else if(intval==5)
							strcpy(ibmail.lines[1],"David Koresh");
						else if(intval==6)
							strcpy(ibmail.lines[1],"Masked Fella");
						intval=ibbs_bbs_list();
						if(intval>-1) {
							ibbs_bbs_name(intval,0,FALSE,hand,&dbn,&pn);
							if(hand[0]!=0) {
								cur_user.InterBBSMoves--;
								money_minus(money);
								sprintf(numstr,SBYDB_PREFIX".%03d",dbn);
								justfile=ShareFileOpen(numstr,"rb");
								if(justfile!=NULL) {
									fseek(justfile,pn*sizeof(ibbs_scr_rec),SEEK_SET);
									ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
									fclose(justfile);
								}

								ibmail.flirt=1004;
								ibmail.quote_length=0;

								ny_line(135,2,1);
								//	              od_printf("\b\b`bright red`S`red`aving...\n\r");
								//cnt+=mail_idx.afterquote;
								ibmail.length=2;
								strcpy(ibmail.recver,ibscr_rec.name);
								strcpy(ibmail.sender,cur_user.name);
								strcpy(ibmail.node_r,IBBSInfo.paOtherSystem[intval].szAddress);
								strcpy(ibmail.node_s,IBBSInfo.szThisNodeAddress);
								strcpy(ibmail.recverI,ibscr_rec.nameI);
								strcpy(ibmail.senderI,cur_user.bbsname);
								ibmail.deleted=FALSE;
								ibmail.sender_sex=cur_user.sex;
								ibmail.ill=cur_user.std;
								ibmail.inf=cur_user.std_percent;
								sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);

								IBSendMail(&IBBSInfo,&ibmail);

								//IBSend(&IBBSInfo,IBBSInfo.paOtherSystem[intval].szAddress,&ibmail,sizeof(ibbs_mail_type)-((20-ibmail.length)*81));

								if(!rip)
									WaitForEnter();
								else
									od_get_answer("\n\r");
							}
						}
					}
				}
			}
		} else if (key=='I') {
			if(cur_user.InterBBSMoves<=0) {
				ny_line(455,2,1);
				if(rip) {
					od_get_answer("\n\r");
					no_rip_m=1;
				} else
					WaitForEnter();
			} else {
				money=15 + (15*pow(1.7,(double)cur_user.level));
				ny_line(471,2,0);
				od_printf("%s",D_Num(money));
				ny_line(468,0,0);
				key=ny_get_answer("YN");
				if(key=='Y') {
					if(money>cur_user.money) {
						ny_line(252,2,1);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
					} else {
						intval=ibbs_bbs_list();
						if(intval>-1) {
							ibbs_bbs_name(intval,0,FALSE,hand,&dbn,&pn);
							if(hand[0]!=0) {
								cur_user.InterBBSMoves--;
								money_minus(money);
								sprintf(numstr,SBYDB_PREFIX".%03d",dbn);
								justfile=ShareFileOpen(numstr,"rb");
								if(justfile!=NULL) {
									fseek(justfile,pn*sizeof(ibbs_scr_rec),SEEK_SET);
									ny_fread(&ibscr_rec,sizeof(ibbs_scr_rec),1,justfile);
									fclose(justfile);
								}

								ibmail.flirt=1003;
								ibmail.quote_length=0;

								ny_line(135,2,1);
								//	              od_printf("\b\b`bright red`S`red`aving...\n\r");
								//cnt+=mail_idx.afterquote;
								ibmail.length=1;
								strcpy(ibmail.recver,ibscr_rec.name);
								strcpy(ibmail.sender,cur_user.name);
								strcpy(ibmail.node_r,IBBSInfo.paOtherSystem[intval].szAddress);
								strcpy(ibmail.node_s,IBBSInfo.szThisNodeAddress);
								strcpy(ibmail.recverI,ibscr_rec.nameI);
								strcpy(ibmail.senderI,cur_user.bbsname);
								ibmail.deleted=FALSE;
								ibmail.sender_sex=cur_user.sex;
								ibmail.ill=(desease)(xp_random((INT16)AIDS)+1);
								ibmail.inf=xp_random(10)+2;
								sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL",ibbs_game_num);

								IBSendMail(&IBBSInfo,&ibmail);

								//IBSend(&IBBSInfo,IBBSInfo.paOtherSystem[intval].szAddress,&ibmail,sizeof(ibbs_mail_type)-((20-ibmail.length)*81));

								if(!rip)
									WaitForEnter();
								else
									od_get_answer("\n\r");
							}
						}
					}
				}
			}
		} else if (key=='D') {
			intval=ibbs_bbs_list();
			if(intval>-1) {
				ibbs_bbs_name(intval,0,TRUE,hand,&dbn,&pn);
			}
		}
	} while (key!='Q');
}
