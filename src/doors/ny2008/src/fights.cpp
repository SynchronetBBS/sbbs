
#include "ny2008.h"
#include "fights.h"

extern unsigned _stklen;

INT32 menu_index[END];
//extern INT16 fast_mail;
extern INT16 no_rip_m;
extern INT16 expert;
extern INT16 single_node;
extern char c_dir_g;
extern INT16 no_forrest_IGM;

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
extern INT16 clean_mode;
extern INT16 nCurrentUserNumber;
extern user_rec cur_user;
extern enemy enemy_rec;
extern char uname[36];
extern INT16 do_maint;
extern char str[15];
extern INT16 expert;
extern INT16 rip;
extern char *t_buffer;
extern char *ver;
extern char *verinfo;
extern INT16 gamedisk,flagdisk;
extern char gamedir[MAX_PATH],flagdir[MAX_PATH];
extern char c_dir_g;
extern INT16 no_wrt_sts;
extern INT16 no_kernel;


//extern struct door_info;


/*
void	fight_ops(void);
char	fight();
void	attack_ops(void);
char	attack();
INT16     attack_points();
INT16	monster_hit();
INT16	xp_random();
void	attack_sequence();
  */

void any_attack_ops(user_rec *user_on, const char fight_name[], const char en_name[], INT32 en_hitpoints, INT32 en_strength, INT32 en_defense, weapon en_arm) {
	INT32 hit_s; //the attacking stregth
	INT32 def_s;
	INT32 en_hit_s;
	INT32 en_def_s;

	FILE *justfile;
	//enemy_idx eidx;
	//	INT16 first,last;//,moneis;
	INT32 intval;
	//	enemy erec;
	char key;

	wrt_sts();

	od_printf("\n\r\n");

	ny_clr_scr();


	ny_line(201,0,0);
	//	You meet
	ny_disp_emu(en_name);
	ny_line(202,0,2);
	//	 ...
	ny_line(203,0,0);
	//	H`4e got a bad lokin' `0");
	print_arm(en_arm);


	//	INT32 enhitp;
	//	enhitp=en_hitpoints;

	do {

bam_af:

		//	  ny_line(204,3,2);
		//	  ny_disp_emu("\n\r\n\n\r`@S`4treet `@F`4ight\n\r\n`0");
		od_disp_str("\n\n\n\r");
		ny_disp_emu(fight_name);
		od_disp_str("\n\n\r");
		od_set_attrib(0x0a);
		ny_disp_emu(en_name);
		ny_line(205,0,0);
		//	  ny_disp_emu("'s `@H`4itpoints:");
		od_printf("%s",D_Num(en_hitpoints));
		ny_line(206,1,0);
		//	  ny_disp_emu("\n\r`@Y`4er `@H`4itpoints: ");
		od_printf("%s\n\r\n",D_Num(user_on->hitpoints));
		/*	  ny_disp_emu("\n\r\n`@[A] `4- `@A`4ttack\n\r");
			  ny_disp_emu("`@[T] `4- `@T`4ake `@D`4rug `@A`4nd `@A`4ttack\n\r");
			  ny_disp_emu("`@[G] `4- `@G`4et `@O`4utta `@H`4ere\n\r");
			  ny_disp_emu("`@[Y] `4- `@Y`4er `@S`4tats\n\r");*/
		ny_send_menu(ATTACK,"");
		ny_line(207,1,0);
		//ny_disp_emu("\n\r`@W`4hat ya gonna do? (`@[A] T G Y`4)");

		key=od_get_answer("ATKPRGY\n\r");
		if (key=='\n' || key=='\r')
			key='A';

		od_putch(key);

		if(key=='R' && user_on->rocks==0) {
			ny_line(445,2,1);
			WaitForEnter();
			goto bam_af;
		}


		if (key == 'T') {  // Take drugs
			take_drug();
			no_rip_m=0;
			key='A';
		}
		if (key=='A' || key=='K' || key=='P' || key=='R') {
			// when atacking
			//	    randomize();
			if(key=='K') {
				hit_s=xp_random(150 - user_on->kick_ability);
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->kick_ability/100.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(439,2,0);
				} else {
					hit_s=0;
					ny_line(440,2,0);
				}
			} else if(key=='P') {
				hit_s=xp_random(140 - user_on->punch_ability);
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+1,1.2)*6.0*(user_on->punch_ability/90.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(441,2,0);
				} else {
					hit_s=0;
					ny_line(442,2,0);
				}
			} else if(key=='R') {
				hit_s=xp_random(145 - user_on->throwing_ability);
				user_on->rocks--;
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->throwing_ability/110.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(443,2,0);
				} else {
					hit_s=0;
					ny_line(444,2,0);
				}
			}

attack_again_a:
			;
			if(key=='A')
				hit_s = (INT32)user_on->strength * (what_arm_force(user_on->arm)) * (double)((xp_random(80)+90)/100.0);
			// If he is on drugs
			if(user_on->drug_high > 0)
				hit_s = (hit_s * what_drug_force_a(user_on->drug,user_on->drug_high));

			//	    hit_s*=pow(1.1,user_on->level);

			hit_s*=(100.0-(user_on->since_got_laid*2))/100.0;
			if (user_on->drug>=COKE)
				hit_s-=xp_random(user_on->drug_days_since * pow((double)user_on->drug_addiction,1.2))/60;
			if (hit_s<0)
				hit_s=0;

			en_hit_s = en_strength * (what_arm_force(en_arm)) * (double)((xp_random(75)+60)/100.0);

			def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
			def_s*=pow(1.2,user_on->level);
			def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
			if (user_on->drug>=COKE)
				def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
			if (def_s<0)
				def_s=0;

			en_def_s = en_defense * (double)((xp_random(75)+50)/100.0);

			def_s/=2;
			en_def_s/=2;

			en_hit_s-=def_s;
			if (en_hit_s<0)
				en_hit_s=0;

			hit_s-=en_def_s;
			if (hit_s<0)
				hit_s=0;

			if (hit_s==0 && en_hit_s==0 && key=='A')
				goto attack_again_a;

			en_hitpoints-=hit_s;


			ny_line(208,2,0);
			//	    od_printf("\n\r\n`bright red`Y`red`a kick `bright green`");
			ny_disp_emu(en_name);
			ny_line(209,0,0);
			//	    od_printf("'s `red`ass fer `bright green`
			od_printf(D_Num(hit_s));
			ny_line(210,0,0);
			//	    %s `red`damage",D_Num(hit_s));


			if (en_hitpoints>0) {

				ny_disp_emu("\n\r\n`0");
				ny_disp_emu(en_name);

				ny_line(211,0,0);
				//od_printf(" `red`kicks yer ass fer `bright green`
				od_printf(D_Num(en_hit_s));
				ny_line(210,0,0);
				user_on->hitpoints-=en_hit_s;
				wrt_sts();
				if (user_on->hitpoints<=0) {
					od_printf("\n\r\n");
					ny_send_menu(ASS_KICKED,"");
					/*		od_printf("You had yer ass kicked ... oh well that happens\n\rCome back tomorrow to get revenge ...\n\r");
							od_printf("You lost all the money on ya...");
							od_printf("\n\rAnd 2%c of yer points\n\r",37);*/
					news_post(user_on->say_loose,user_on->name,en_name,5);
					user_on->money=0;
					user_on->alive=UNCONCIOUS;
					points_loose(user_on->points*.02);

					WaitForEnter();
					od_exit(10,FALSE);
				}

			}
		} else if (key=='G') {
			intval=xp_random(2);
			if (intval==0) {

				ny_line(212,2,1);
				//od_printf("\n\r\n`bright red`Y`red`a got away ...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				return;
			} else {

				ny_line(213,2,1);
				//od_printf("\n\r\n`bright red`Y`red`e couldn't find a way outta this ...\n\r");

				//	      randomize();
				en_hit_s = en_strength * (what_arm_force(en_arm)) * (double)((xp_random(75)+75)/100.0);
				def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
				def_s*=pow(1.2,user_on->level);
				def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
				if (user_on->drug>=COKE)
					def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
				if (def_s<0)
					def_s=0;
				def_s/=2;
				en_hit_s-=def_s;
				if (en_hit_s<0)
					en_hit_s=0;


				ny_disp_emu("\n\r\n`0");
				ny_disp_emu(en_name);
				ny_line(211,0,0);
				//od_printf(" `red`kicks yer ass fer `bright green`
				od_printf(D_Num(en_hit_s));
				ny_line(210,0,0);

				user_on->hitpoints-=en_hit_s;
				wrt_sts();
				if (user_on->hitpoints<=0) {
					//od_printf("\n\r\n`bright`");
					od_printf("\n\r\n");
					ny_send_menu(ASS_KICKED,"");

					/*od_printf("You had yer ass kicked ... oh well that happens\n\rCome back tomorrow to get revenge ...\n\r");
					od_printf("You lost all the money on ya...");
					od_printf("\n\rAnd 2%c of yer points\n\r",37);*/


					news_post(user_on->say_loose,user_on->name,en_name,5);
					user_on->alive=UNCONCIOUS;
					user_on->money=0;
					points_loose(user_on->points*.02);
					//wrt_sts();
					WaitForEnter();
					od_exit(10,FALSE);
				}

			}
		} else {
			DisplayStats();
			WaitForEnter();
		}
	} while (en_hitpoints>0);
	//	od_printf("\n\r\n%ld\n\r\n",intval);
	//	intval=(INT32)enhitp*.2+ randomf((INT32)enhitp*.2);
	//	od_printf("\n\r\n%ld\n\r\n",intval);
	/*	intval=((INT32)user_on->level*.55 + 1)*((INT32)enhitp*.5+ randomf((INT32)enhitp*.5))+(INT32)(3*(user_on->level+1));
		if ((INT32)intval<(INT32)enhitp) intval=(INT32)enhitp;
		money_plus((DWORD)intval);
		wrt_sts();*/


	ny_line(214,2,0);
	//	od_printf("\n\r\n`bright red`Y`red`ou kicked `bright green`");
	ny_disp_emu(en_name);
	ny_line(215,0,1);
	//	od_printf("'s `red`ass ...\n\r\n");
	//	ny_line(216,0,0);
	//	od_printf("`bright red`Y`red`a find `bright red`
	//	od_printf(D_Num(intval));
	//	ny_line(217,0,0);
	//	`red` bucks ",D_Num(intval));
	//	intval=randomf((user_on->level+1)*12)+3;
	//	od_printf(D_Num((DWORD)intval * (DWORD)DrgPtsCoef()));
	//	ny_line(218,0,1);
	//	`red` points!\n\r",D_Num((DWOPRD)intval * (DWORD)DrgPtsCoef()));
	//	points_raise(intval);

	WaitForEnter();
} // end of function attack_ops

/*char
fight(INT16 exp)
{
	char allowed[]="LHYTQ?\n\r";
 
	char key;  // Menu choice
 
 
	od_clear_keybuffer();
 
       if(no_rip_m!=1){
	od_printf("\n\r\n");
	if (exp!=2 && exp!=3) ny_clr_scr();            // Prepare virgin screen
 
	if ((exp==2 || exp==3) && rip==FALSE) {
//	  ny_dispod_printf("\n\r`bright red`S`red`treet `bright red`F`red`ights\n\r");
	  ny_line(353,0,1);
	  //ny_disp_emu("\n\r`@S`4treet `@F`4ights\n\r`0");
	  od_printf(allowed);
	  ny_line(36,0,0);
	  od_printf("%d ",od_control.caller_timelimit);
	  ny_line(37,0,0);
 
	  //od_printf("`bright green`%s`bright blue`E`blue`nter `bright blue`Y`blue`er `bright blue`C`blue`ommand (%d mins)`bright blue`>",allowed,od_control.caller_timelimit);
	  if(rip) od_disp_str("\n\r!|10000$HKEYON$|#|#|#\n\r");
	  key=ny_get_answer(allowed);
	  if(rip) od_disp_str("\n\r!|10000$HKEYOFF$|#|#|#\n\r");
	} else {// if((key = od_hotkey_menu("SFIGHT", allowed, TRUE)) == 0) {
 
	// Print menu and command line
	if(!rip) {
	  ny_line(353,1,2);
	  //ny_disp_emu("\n\r`@S`4treet `@F`4ights\n\r\n");
	  disp_fig_stats();
	}
	key=ny_send_menu(S_FIG,allowed);
 
 
 
/*	od_printf("`green`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
 
	key=od_get_key(FALSE);
	if (key>='a' && key<='z') key-=32;
	if (key!=0 && strchr(allowed,key)!=NULL) goto s_f_after;
	od_printf("`bright blue`So you wanna kick some ass ....\n\r\n");
	key=od_get_key(FALSE);
	if (key>='a' && key<='z') key-=32;
	if (key!=0 && strchr(allowed,key)!=NULL) goto s_f_after;
	od_printf("     `bright red`L`red` - Look for Ass to kick  `bright red`H`red` - Heal wounds\n\r");
	key=od_get_key(FALSE);
	if (key>='a' && key<='z') key-=32;
	if (key!=0 && strchr(allowed,key)!=NULL) goto s_f_after;
	od_printf("     `bright red`Y`red` - Your Stats            `bright red`T`red` - Take a drug\n\r");
	key=od_get_key(FALSE);
	if (key>='a' && key<='z') key-=32;
	if (key!=0 && strchr(allowed,key)!=NULL) goto s_f_after;
	od_printf("     `bright red`Q`red` - Return to Central Park\n\r\n");
	key=od_get_key(FALSE);
	if (key>='a' && key<='z') key-=32;
	if (key!=0 && strchr(allowed,key)!=NULL) goto s_f_after;-/
 
//	ny_disp_emu("`9E`1nter `9Y`1er `9C`1ommand (`9");
 //	od_printf("%d ",od_control.caller_timelimit);
   //	ny_disp_emu("`1mins)`9>");
	if(!rip){
	  ny_line(36,0,0);
	  od_printf("%d ",od_control.caller_timelimit);
	  ny_line(37,0,0);
	}
 
//	s_f_after:;
 
	if (key==0) {
	  if(rip) od_disp_str("\n\r!|10000$HKEYON$|#|#|#\n\r");
	  key = ny_get_answer(allowed);
	  if(rip) od_disp_str("\n\r!|10000$HKEYOFF$|#|#|#\n\r");
	}
	}
 
       } else {
	no_rip_m=0;
	if(rip) od_disp_str("\n\r!|10000$HKEYON$|#|#|#\n\r");
	key=ny_get_answer(allowed);
	if(rip) od_disp_str("\n\r!|10000$HKEYOFF$|#|#|#\n\r");
       }
 
	// If enter or line feed, quit the menu -- default choice
	if (key=='\n' || key=='\r') key='Q';
 
	// Print the choice
	if(!rip) od_putch(key);
	if(expert==1) expert=3;
 
	// return the choice
	return key;
} // End of fight menu function
*/





void
fight_ops(user_rec *cur_user)  //This function operates the fights with monsters
{

	char key;  // Menu choice
	INT16 intval;

	//user_rec cur_user; // The User Record Variable
	//cur_user *user_on;  // declare allias for the user record

	do {
		//key = fight(expert);  //Getting the result of menu choice
		key=callmenu("LHYTQ?\n\r",S_FIG,353,TRUE);
		while (key=='?') {
			expert+=10;
			key=callmenu("LHYTQ?\n\r",S_FIG,353,TRUE);
			expert-=10;
		}



		if (key == 'L') {
			attack_ops(cur_user);
		} // End of if look
		else if (key == 'T') {  // Take drugs
			take_drug();
		} // End of take drug
		else if (key == 'H') {
			ny_line(199,2,0);
			if(rip)
				od_get_answer("\n\r");
			//od_printf("\n\r\n`bright red`Y`red`a enter the hospital ...\n\r");
			heal_wounds();
		} // End of healing
		else if (key == 'Y') {
			DisplayStats();
			WaitForEnter();
		} // End of viewing the stats

	} while (key != 'Q');   // Do until quit encountered
}   // end of fight_ops function



void attack_ops(user_rec *user_on) {
	INT32 hit_s; //the attacking stregth
	INT32 def_s;
	INT32 en_hit_s;
	INT32 en_def_s;

	FILE *justfile;
	//enemy_idx eidx;
	INT16 first,last;//,moneis;
	INT32 intval;
	enemy erec;
	char key;

	if (user_on->turns <= 0) {

		ny_line(200,2,1);
		//od_printf("\n\r\n`bright red`Y`red`a ain't got no fights left ... come back tomorrow\n\r");


		if(!rip)
			WaitForEnter();
		else {
			od_get_answer("\n\r");
			no_rip_m=1;
		}
		return;
	}

	if(expert==3)
		expert=1;

	intval=event_gen(user_on);
	if (intval==1) {
		if(rip)
			no_rip_m=1;
		return;
	}
	if (intval==2) {
		if(rip)
			no_rip_m=1;
		user_on->turns--;
		return;
	}

	user_on->turns--;
	wrt_sts();

	//readin the index
	ch_game_d();
	justfile=ShareFileOpen(ENEMY_INDEX,"rb");
	if(justfile != NULL) {
		fseek(justfile,(INT32)user_on->level*2,SEEK_SET);
		ny_fread(&first,2,1,justfile);
		fseek(justfile,(INT32)user_on->level*2 + 42,SEEK_SET);
		ny_fread(&last,2,1,justfile);

		fclose(justfile);
	}

	intval=xp_random(last-first+1)+first;

	justfile=ShareFileOpen(ENEMY_FILENAME,"rb");
	if(justfile != NULL) {
		fseek(justfile,(INT32)intval*sizeof(enemy),SEEK_SET);
		ny_fread(&erec,sizeof(enemy),1,justfile);
		fclose(justfile);
	}


	od_printf("\n\r\n");

	ny_clr_scr();


	ny_line(201,0,0);
	//	You meet
	ny_disp_emu(erec.name);
	ny_line(202,0,2);
	//	 ...
	ny_line(203,0,0);
	//	H`4e got a bad lokin' `0");
	print_arm(erec.arm);


	INT32 enhitp;
	enhitp=erec.hitpoints;

	do {

bam_nf:

		ny_line(204,3,2);
		//	  ny_disp_emu("\n\r\n\n\r`@S`4treet `@F`4ight\n\r\n`0");
		ny_disp_emu(erec.name);
		ny_line(205,0,0);
		//	  ny_disp_emu("'s `@H`4itpoints:");
		od_printf("%s",D_Num(erec.hitpoints));
		ny_line(206,1,0);
		//	  ny_disp_emu("\n\r`@Y`4er `@H`4itpoints: ");
		od_printf("%s\n\r\n",D_Num(user_on->hitpoints));
		/*	  ny_disp_emu("\n\r\n`@[A] `4- `@A`4ttack\n\r");
			  ny_disp_emu("`@[T] `4- `@T`4ake `@D`4rug `@A`4nd `@A`4ttack\n\r");
			  ny_disp_emu("`@[G] `4- `@G`4et `@O`4utta `@H`4ere\n\r");
			  ny_disp_emu("`@[Y] `4- `@Y`4er `@S`4tats\n\r");*/
		ny_send_menu(ATTACK,"");
		ny_line(207,1,0);
		//ny_disp_emu("\n\r`@W`4hat ya gonna do? (`@[A] T G Y`4)");

		key=od_get_answer("ATKPRGY\n\r");
		if (key=='\n' || key=='\r')
			key='A';

		od_putch(key);

		if(key=='R' && user_on->rocks==0) {
			ny_line(445,2,1);
			WaitForEnter();
			goto bam_nf;
		}


		if (key == 'T') {  // Take drugs
			take_drug();
			no_rip_m=0;
			key='A';
		}
		if (key=='A' || key=='K' || key=='P' || key=='R') {
			// when atacking
			//	    randomize();
			if(key=='K') {
				hit_s=xp_random(150 - user_on->kick_ability);
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->kick_ability/100.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(439,2,0);
				} else {
					hit_s=0;
					ny_line(440,2,0);
				}
			} else if(key=='P') {
				hit_s=xp_random(140 - user_on->punch_ability);
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+1,1.2)*6.0*(user_on->punch_ability/90.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(441,2,0);
				} else {
					hit_s=0;
					ny_line(442,2,0);
				}
			} else if(key=='R') {
				hit_s=xp_random(145 - user_on->throwing_ability);
				user_on->rocks--;
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->throwing_ability/110.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(443,2,0);
				} else {
					hit_s=0;
					ny_line(444,2,0);
				}
			}

attack_again_s:
			;
			if(key=='A')
				hit_s = (INT32)user_on->strength * (what_arm_force(user_on->arm)) * (double)((xp_random(80)+90)/100.0);
			// If he is on drugs
			if(user_on->drug_high > 0)
				hit_s = (hit_s * what_drug_force_a(user_on->drug,user_on->drug_high));

			//	    hit_s*=pow(1.1,user_on->level);

			hit_s*=(100.0-(user_on->since_got_laid*2))/100.0;
			if (user_on->drug>=COKE)
				hit_s-=xp_random(user_on->drug_days_since * pow((double)user_on->drug_addiction,1.2))/60;
			if (hit_s<0)
				hit_s=0;

			en_hit_s = erec.strength * (what_arm_force(erec.arm)) * (double)((xp_random(75)+60)/100.0);

			def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
			def_s*=pow(1.2,user_on->level);
			def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
			if (user_on->drug>=COKE)
				def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
			if (def_s<0)
				def_s=0;

			en_def_s = erec.defense * (double)((xp_random(75)+50)/100.0);

			def_s/=2;
			en_def_s/=2;

			en_hit_s-=def_s;
			if (en_hit_s<0)
				en_hit_s=0;

			hit_s-=en_def_s;
			if (hit_s<0)
				hit_s=0;

			if (hit_s==0 && en_hit_s==0 && key=='A')
				goto attack_again_s;

			erec.hitpoints-=hit_s;


			ny_line(208,2,0);
			//	    od_printf("\n\r\n`bright red`Y`red`a kick `bright green`");
			ny_disp_emu(erec.name);
			ny_line(209,0,0);
			//	    od_printf("'s `red`ass fer `bright green`
			od_printf(D_Num(hit_s));
			ny_line(210,0,0);
			//	    %s `red`damage",D_Num(hit_s));


			if (erec.hitpoints>0) {

				ny_disp_emu("\n\r\n`0");
				ny_disp_emu(erec.name);

				ny_line(211,0,0);
				//od_printf(" `red`kicks yer ass fer `bright green`
				od_printf(D_Num(en_hit_s));
				ny_line(210,0,0);
				user_on->hitpoints-=en_hit_s;
				wrt_sts();
				if (user_on->hitpoints<=0) {
					od_printf("\n\r\n");
					ny_send_menu(ASS_KICKED,"");
					/*		od_printf("You had yer ass kicked ... oh well that happens\n\rCome back tomorrow to get revenge ...\n\r");
							od_printf("You lost all the money on ya...");
							od_printf("\n\rAnd 2%c of yer points\n\r",37);*/
					news_post(user_on->say_loose,user_on->name,erec.name,5);
					user_on->money=0;
					user_on->alive=UNCONCIOUS;
					points_loose(user_on->points*.02);

					WaitForEnter();
					od_exit(10,FALSE);
				}

			}
		} else if (key=='G') {
			intval=xp_random(2);
			if (intval==0) {

				ny_line(212,2,1);
				//od_printf("\n\r\n`bright red`Y`red`a got away ...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				return;
			} else {

				ny_line(213,2,1);
				//od_printf("\n\r\n`bright red`Y`red`e couldn't find a way outta this ...\n\r");

				//	      randomize();
				en_hit_s = erec.strength * (what_arm_force(erec.arm)) * (double)((xp_random(75)+75)/100.0);
				def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
				def_s*=pow(1.2,user_on->level);
				def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
				if (user_on->drug>=COKE)
					def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
				if (def_s<0)
					def_s=0;
				def_s/=2;
				en_hit_s-=def_s;
				if (en_hit_s<0)
					en_hit_s=0;


				ny_disp_emu("\n\r\n`0");
				ny_disp_emu(erec.name);
				ny_line(211,0,0);
				//od_printf(" `red`kicks yer ass fer `bright green`
				od_printf(D_Num(en_hit_s));
				ny_line(210,0,0);

				user_on->hitpoints-=en_hit_s;
				wrt_sts();
				if (user_on->hitpoints<=0) {
					//od_printf("\n\r\n`bright`");
					od_printf("\n\r\n");
					ny_send_menu(ASS_KICKED,"");

					/*od_printf("You had yer ass kicked ... oh well that happens\n\rCome back tomorrow to get revenge ...\n\r");
					od_printf("You lost all the money on ya...");
					od_printf("\n\rAnd 2%c of yer points\n\r",37);*/


					news_post(user_on->say_loose,user_on->name,erec.name,5);
					user_on->alive=UNCONCIOUS;
					user_on->money=0;
					points_loose(user_on->points*.02);
					//wrt_sts();
					WaitForEnter();
					od_exit(10,FALSE);
				}

			}
		} else {
			DisplayStats();
			WaitForEnter();
		}
	} while (erec.hitpoints>0);
	//	od_printf("\n\r\n%ld\n\r\n",intval);
	//	intval=(INT32)enhitp*.2+ randomf((INT32)enhitp*.2);
	//	od_printf("\n\r\n%ld\n\r\n",intval);
	intval=((INT32)user_on->level*.55 + 1)*((INT32)enhitp*.5+ randomf((INT32)enhitp*.5))+(INT32)(3*(user_on->level+1));
	if ((INT32)intval<(INT32)enhitp)
		intval=(INT32)enhitp;
	money_plus((DWORD)intval);
	wrt_sts();


	ny_line(214,2,0);
	//	od_printf("\n\r\n`bright red`Y`red`ou kicked `bright green`");
	ny_disp_emu(erec.name);
	ny_line(215,0,2);
	//	od_printf("'s `red`ass ...\n\r\n");
	ny_line(216,0,0);
	//	od_printf("`bright red`Y`red`a find `bright red`
	od_printf(D_Num(intval));
	ny_line(217,0,0);
	//	`red` bucks ",D_Num(intval));
	intval=randomf((user_on->level+1)*12)+3;
	od_printf(D_Num((DWORD)intval * (DWORD)DrgPtsCoef()));
	ny_line(218,0,1);
	//	`red` points!\n\r",D_Num((DWORD)intval * (DWORD)DrgPtsCoef()));
	points_raise(intval);

	WaitForEnter();
} // end of function attack_ops

void
fgc(INT16 *enm_num, INT16 *user_num) {
	char numstr[21];
	INT16 intval;
	//  ffblk ffblk;
	FILE *justfile;

	sprintf(numstr,"u%07d.fgc",*user_num);
	if (fexist(numstr)) {
		justfile=ShareFileOpen(numstr,"rb");
		if(justfile != NULL) {
			ny_fread(enm_num,2,1,justfile);
			fclose(justfile);
		}
	}
}


void
sfflg(INT16 user_num,char numstr[], INT16 enm_num, user_rec *erec) {
	ch_flag_d();
	FILE *justfile;

	sprintf(numstr,"u%07d.fgg",user_num); //set fight flag
	justfile = ShareFileOpen(numstr, "wb");
	if(justfile != NULL)
		fclose(justfile);

	//readin the enemy record
	sprintf(numstr,"u%07d.sts",enm_num);
	justfile=ShareFileOpen(numstr,"rb");
	if(justfile != NULL) {
		ny_fread(erec,sizeof(user_rec),1,justfile);
		fclose(justfile);
	}
}

INT16
fig_m(user_rec *erec, user_rec *user_on, INT16 *enm_num, INT16 *user_num) {
	char key;
	DWORD intval;
	char numstr[25];
	char numstr2[25];
	FILE *justfile;
	INT32 hit_s,def_s,en_hit_s;
	time_t t,t2;



	do {

bam_ofm:
		ny_line(221,3,0);
		//      od_printf("\n\r\n\n\r`bright red`O`red`nline `bright red`P`red`layer `bright red`F`red`ight");
		ny_disp_emu("\n\r\n`0");
		ny_disp_emu(erec->name);
		ny_line(205,0,0);
		//      od_printf("'s `bright red`H`red`itpoints: %s",
		od_printf(D_Num(erec->hitpoints));
		ny_line(206,1,0);
		//      od_printf("\n\r`bright red`Y`red`er `bright red`H`red`itpoints: %s",
		od_printf(D_Num(user_on->hitpoints));
		od_printf("\n\r\n");
		/*      od_printf("\n\r\n`bright red`[A] `red`- `bright red`A`red`ttack\n\r");
		      od_printf("`bright red`[T] `red`- `bright red`T`red`ake `bright red`D`red`rug `bright red`A`red`nd `bright red`A`red`ttack\n\r");
		      od_printf("`bright red`[G] `red`- `bright red`G`red`et `bright red`O`red`utta `bright red`H`red`ere\n\r");
		      od_printf("`bright red`[Y] `red`- `bright red`Y`red`er `bright red`S`red`tats\n\r");
		      od_printf("\n\r`bright red`W`red`hat ya gonna do? (`bright red`[A] G Y`red`)");*/
		ny_send_menu(ATTACK,"");
		ny_line(207,1,0);
		//      ny_disp_emu("\n\r`@W`4hat ya gonna do? (`@[A] T G Y`4)");


		key=od_get_answer("ATKPRGY\n\r");
		if (key=='\n' || key=='\r')
			key='A';
		od_putch(key);
		fgc(enm_num,user_num);
		if (key=='Y') {
			DisplayStats();
			ny_line(1,1,0);
			od_get_answer("\n\r");
		}
	} while (key=='Y');

	if(key=='R' && user_on->rocks==0) {
		ny_line(445,2,1);
		ny_line(1,1,0);
		od_get_answer("\n\r");
		goto bam_ofm;
	}


	if (key == 'T') {  // Take drugs
		take_drug();
		no_rip_m=0;
		key='A';
	}
	if (key=='A' || key=='K' || key=='P' || key=='R') {
		// when atacking
		//   randomize();
		if(key=='K') {
			hit_s=xp_random(150 - user_on->kick_ability);
			if(hit_s<35) {
				hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->kick_ability/100.0)) * (double)((xp_random(80)+90)/100.0);
				ny_line(439,2,0);
			} else {
				hit_s=0;
				ny_line(440,2,0);
			}
		} else if(key=='P') {
			hit_s=xp_random(140 - user_on->punch_ability);
			if(hit_s<35) {
				hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->punch_ability/90.0)) * (double)((xp_random(80)+90)/100.0);
				ny_line(441,2,0);
			} else {
				hit_s=0;
				ny_line(442,2,0);
			}
		} else if(key=='R') {
			hit_s=xp_random(145 - user_on->throwing_ability);
			user_on->rocks--;
			if(hit_s<35) {
				hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->throwing_ability/110.0)) * (double)((xp_random(80)+90)/100.0);
				ny_line(443,2,0);
			} else {
				hit_s=0;
				ny_line(444,2,0);
			}
		}


		/*if (key == 'T') {  // Take drugs
		  take_drug();
		  no_rip_m=0;
		  key='A';
		}
		if (key=='A') {*/
		// when atacking
		//      randomize();
		if(key=='A')
			hit_s = user_on->strength * (what_arm_force(user_on->arm)) * (double)((xp_random(80)+80)/100.0);
		// If he is on drugs
		if(user_on->drug_high > 0)
			hit_s = (hit_s * what_drug_force_a(user_on->drug,user_on->drug_high));

		//      hit_s*=pow(1.1,user_on->level);

		hit_s*=(100.0-(user_on->since_got_laid*2))/100.0;
		if (user_on->drug>=COKE)
			hit_s-=xp_random(user_on->drug_days_since * pow((double)user_on->drug_addiction,1.2))/60;
		if (hit_s<0)
			hit_s=0;

		def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
		def_s*=pow(1.2,user_on->level);
		def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
		if (user_on->drug>=COKE)
			def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
		if (def_s<0)
			def_s=0;

		def_s/=2;

		sprintf(numstr,"u%07d.atk",*enm_num);
		justfile = ShareFileOpen(numstr, "wb");
		if(justfile != NULL) {
			ny_fwrite(&hit_s,4,1,justfile);
			fclose(justfile);
		}
		sprintf(numstr,"u%07d.atk",*enm_num);
		sprintf(numstr2,"u%07d.on",*enm_num);
		ny_line(222,2,1);
		//od_printf("\n\r\n`bright red`W`red`aiting....\n\r");
		t=time(NULL);
		t2=t;

		while (fexist(numstr) && fexist(numstr2)) {
			fgc(enm_num,user_num);
			sprintf(numstr,"u%07d.atk",*enm_num);
			sprintf(numstr2,"u%07d.on",*enm_num);
			while(t==t2) {
				t=time(NULL);
				od_kernal();
			}
			t2=t;
			od_kernal();
		}

		if (!fexist(numstr2)) {

			ny_line(223,2,1);
			if(rip)
				od_get_answer("\n\r");
			ny_line(224,0,1);
			//od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");

			ny_remove(numstr);
			//sprintf(numstr,"del u%07d.atk",*enm_num);
			//system(numstr);
			sprintf(numstr,"u%07d.fgg",*user_num);
			ny_remove(numstr);
			if(!rip)
				ny_line(1,1,0);
			od_get_answer("\n\r");
			return TRUE;
		}

		intval=erec->hitpoints;

		sprintf(numstr,"u%07d.sts",*enm_num);
		justfile=ShareFileOpen(numstr,"rb");
		if(justfile != NULL) {
			ny_fread(erec,sizeof(user_rec),1,justfile);
			fclose(justfile);
		}

		intval -= erec->hitpoints;

		fgc(enm_num,user_num);


		ny_line(208,2,0);
		//od_printf("\n\r\n`bright red`Y`red`a kick `bright green`");
		ny_disp_emu(erec->name);
		ny_line(209,0,0);
		od_printf(D_Num(intval));
		ny_line(210,0,0);

	} else if (key=='G') {
		intval=xp_random(2);
		if (intval==0) {

			ny_line(212,2,1);
			//	 od_printf("\n\r\n`bright red`Y`red`a got away ...\n\r");

			sprintf(numstr,"u%07d.atk",*enm_num);
			justfile = ShareFileOpen(numstr, "wb");
			if(justfile != NULL) {
				intval=-1; //user got away
				ny_fwrite(&intval,4,1,justfile);
				fclose(justfile);
			}
			sprintf(numstr,"u%07d.fgg",*user_num);
			ny_remove(numstr);
			if(!rip)
				ny_line(1,1,0);
			od_get_answer("\n\r");
			return TRUE;
		} else {

			ny_line(213,2,1);
			//	 od_printf("\n\r\n`bright red`Y`red`e couldn't find a way outta this ...\n\r");

			sprintf(numstr,"u%07d.atk",*enm_num);
			justfile = ShareFileOpen(numstr, "wb");
			if(justfile != NULL) {
				intval=-2; ///user couldn't get away
				ny_fwrite(&intval,4,1,justfile);
				fclose(justfile);
			}
			sprintf(numstr,"u%07d.atk",*enm_num);
			sprintf(numstr2,"u%07d.on",*enm_num);

			ny_line(222,2,1);
			//od_printf("\n\r\n`bright red`W`red`aiting....\n\r");

			t=time(NULL);
			t2=t;
			while (fexist(numstr) && fexist(numstr2)) {
				fgc(enm_num,user_num);
				sprintf(numstr,"u%07d.atk",*enm_num);
				sprintf(numstr2,"u%07d.on",*enm_num);
				while(t==t2) {
					t=time(NULL);
					od_kernal();
				}
				t2=t;
				od_kernal();
			}

			if (!fexist(numstr2)) {

				ny_line(223,2,1);
				if(rip)
					od_get_answer("\n\r");
				ny_line(224,0,1);
				//od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");

				//	   sprintf(numstr,"del u%07d.atk",enm_num);
				//	   system(numstr);
				ny_remove(numstr);
				sprintf(numstr,"u%07d.fgg",*user_num);
				ny_remove(numstr);
				if(!rip)
					ny_line(1,1,0);
				od_get_answer("\n\r");
				return TRUE;
			}
		}
	}
	return FALSE;

}

INT16
o_checks(INT16 *enm_num,INT16 *user_num, user_rec *user_on, user_rec *erec) {
	DWORD intval;
	FILE *justfile;
	char numstr[25];
	char numstr2[25];
	INT16 rape;
	INT32 hit_s,def_s,en_hit_s;
	time_t t,t2;
	char key;
	mail_idx_type mail_idx;


	sprintf(numstr,"u%07d.atk",*user_num);

	ny_line(225,2,1);
	//      od_printf("\n\r\n`bright red`W`red`aiting fer enemy response....\n\r");


	t=time(NULL);
	t2=t;
	sprintf(numstr2,"u%07d.on",*enm_num);
	while (!fexist(numstr) && fexist(numstr2)) {
		fgc(enm_num,user_num);
		sprintf(numstr2,"u%07d.on",*enm_num);
		sprintf(numstr,"u%07d.atk",*user_num);
		while(t==t2) {
			t=time(NULL);
			od_kernal();
		}
		t2=t;
		od_kernal();
	}
	fgc(enm_num,user_num);
	if (!fexist(numstr2)) {

		ny_line(223,2,1);
		if(rip)
			od_get_answer("\n\r");
		ny_line(224,0,1);
		//	od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");

		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		if(!rip)
			ny_line(1,1,0);
		od_get_answer("\n\r");
		return TRUE;
	}

	//read atack file
	sprintf(numstr,"u%07d.atk",*user_num);
	justfile = ShareFileOpen(numstr, "rb");
	if(justfile != NULL) {
		ny_fread(&en_hit_s,4,1,justfile);
		fclose(justfile);
	}

	if (en_hit_s==-1) {

		ny_line(226,2,1);
		//	od_printf("\n\r\n`bright red`Y`red`er enemy ran away in fear...\n\r");

		ny_remove(numstr);
		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		//sprintf(numstr,"del u%07d.atk",*user_num);
		//system(numstr); //see above!
		if(!rip)
			ny_line(1,1,0);
		od_get_answer("\n\r");
		return TRUE;
	}
	if (en_hit_s==-9) {

		ny_line(227,2,2);
		//	od_printf("\n\r\n`bright red`Y`red`a `bright white`WON`red`!\n\r\n");
		sprintf(numstr,"u%07d.atk",*user_num);
		justfile = ShareFileOpen(numstr, "rb");
		if(justfile != NULL) {
			ny_fread(&en_hit_s,4,1,justfile);
			ny_fread(&intval,4,1,justfile);

			fclose(justfile);
		}

		money_plus(intval);

		ny_line(216,0,0);
		//	od_printf("`bright red`Y`red`a find `bright red`
		od_printf(D_Num(intval));
		ny_line(217,0,0);
		intval=.08*erec->points;
		od_printf(D_Num((DWORD)intval * (DWORD)DrgPtsCoef()));
		ny_line(218,0,1);
		points_raise(intval);

		ny_kernel();


		//no_kernel=TRUE;
		wrt_sts();
		//no_kernel=FALSE;

		if (erec->arm>user_on->arm) {

			ny_line(228,2,0);
			//	  od_printf("\n\r\n`bright red`D`red`o ya wanna swap weapons with`bright green` ");
			if(!rip)
				ny_disp_emu(erec->name);
			else
				od_disp_str(ny_un_emu(erec->name,numstr));
			ny_line(79,0,0);
			//ny_disp_emu("`4? (`@Y`4/`@N`4)");

			key=od_get_answer("YN");
			if(!rip)
				od_printf("%c\n\r",key);
			else
				od_disp_str("\n\r");

			if (key=='Y') {

				weapon tarm;

				tarm=erec->arm;
				erec->arm=user_on->arm;
				user_on->arm=tarm;
				sprintf(numstr,"u%07d.on",*enm_num);
				if (fexist(numstr)) {
					sprintf(numstr,"u%07d.swp",*enm_num);
					justfile=ShareFileOpen(numstr,"wb");
					if(justfile != NULL) {
						ny_fwrite(&(erec->arm),2,1,justfile);
						fclose(justfile);
					}
				} else {
					tarm=erec->arm;
					ch_game_d();
					justfile=ShareFileOpen(USER_FILENAME,"r+b");
					if(justfile != NULL) {
						fseek(justfile,*enm_num * sizeof(user_rec),SEEK_SET);
						ny_fread(erec,sizeof(user_rec),1,justfile);
						erec->arm=tarm;
						fseek(justfile,*enm_num * sizeof(user_rec),SEEK_SET);
						ny_fwrite(erec,sizeof(user_rec),1,justfile);
						fclose(justfile);
					}
					ch_flag_d();
				}

				ny_line(229,2,0);
				//	    od_printf("\n\r`bright red`D`red`one! `bright red`Y`red`ou now got the `bright green`");

				print_arm(user_on->arm);
				if(rip) {
					od_disp_str("::^M@OK))|#|#|#\n\r\n");
					od_get_answer("\n\r");
				}
				od_printf("\n\r\n");

			}
		}

		if (erec->sex!=user_on->sex && clean_mode==FALSE && user_on->sex_today>0) {

			ny_line(230,2,0);
			//	  od_printf("\n\r\n`bright red`D`red`o ya wanna rape `bright green`");
			if(!rip)
				ny_disp_emu(erec->name);
			else
				od_disp_str(ny_un_emu(erec->name,numstr));
			ny_line(79,0,0);
			//	  ny_disp_emu("`4? (`@Y`4/`@N`4)");

			key=od_get_answer("YN");
			if(!rip)
				od_printf("%c\n\r",key);
			else
				od_disp_str("\n\r");
			if (key=='Y') {
				if (user_on->sex_today<=0) {

					ny_line(118,2,1);
					//od_printf("\n\r\n`bright white`You already used up all your sex turns today ...\n\r");

					if(rip)
						od_get_answer("\n\r");
					sprintf(numstr,"u%07d.on",*enm_num);
					if (fexist(numstr)) {
						sprintf(numstr,"u%07d.kik",*enm_num);
						justfile=ShareFileOpen(numstr,"wb");
						if(justfile != NULL) {
							rape=-1;
							ny_fwrite(&rape,2,2,justfile);
							fclose(justfile);
						}
					}
					//	      WaitForEnter();
				} else {
					sprintf(numstr,"u%07d.on",*enm_num);
					if (fexist(numstr)) {
						sprintf(numstr,"u%07d.kik",*enm_num);
						justfile=ShareFileOpen(numstr,"wb");
						if(justfile != NULL) {
							ny_fwrite(&user_on->std,2,1,justfile);
							ny_fwrite(&user_on->std_percent,2,1,justfile);
							fclose(justfile);
						}
					} else {
						strcpy(mail_idx.recver,erec->name);
						strcpy(mail_idx.sender,user_on->name);
						strcpy(mail_idx.recverI,erec->bbsname);
						strcpy(mail_idx.senderI,user_on->bbsname);
						mail_idx.flirt=1000;
						mail_idx.deleted=FALSE;
						mail_idx.location=0;
						mail_idx.length=0;
						mail_idx.afterquote=0;
						mail_idx.ill=user_on->std;
						mail_idx.inf=user_on->std_percent;
						mail_idx.sender_sex=user_on->sex;
						ch_game_d();
						justfile=ShareFileOpen(MAIL_INDEX,"a+b");
						if(justfile != NULL) {
							ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
							fclose(justfile);
						}
						ch_flag_d();
					}
					user_on->since_got_laid=0;
					user_on->sex_today--;
					ny_line(339,1,0);
					if(rip)
						ny_disp_emu(erec->name);
					else
						od_disp_str(ny_un_emu(erec->name,numstr));
					ny_line(340,0,1);
					if(rip)
						od_get_answer("\n\r");
					illness(erec->std,erec->std_percent);
					points_raise((INT32)35*(user_on->level+1));

					//	      char omg[51];

					// sprintf(omg,"%s raped you after beating you ...",user_on->name);
				}
			} else {
				sprintf(numstr,"u%07d.on",*enm_num);
				if (fexist(numstr)) {
					sprintf(numstr,"u%07d.kik",*enm_num);
					justfile=ShareFileOpen(numstr,"wb");
					if(justfile != NULL) {
						rape=-1;
						ny_fwrite(&rape,2,2,justfile);
						fclose(justfile);
					}
				}
			}
		}

		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);

		sprintf(numstr,"u%07d.atk",*user_num);
		ny_remove(numstr);

		wrt_sts();

		if(!rip) {
			ny_line(1,1,0);
			od_get_answer("\n\r");
		}
		return TRUE;
	}
	if (en_hit_s!=-2) {
		en_hit_s-=def_s;
		if (en_hit_s<0)
			en_hit_s=0;

		user_on->hitpoints-=en_hit_s;
		wrt_sts();


		ny_disp_emu("\n\r\n`0");
		ny_disp_emu(erec->name);
		ny_line(211,0,0);
		//	od_printf(" `red`kicks yer ass fer `bright green`
		od_printf(D_Num(en_hit_s));
		ny_line(210,0,0);


		if (user_on->hitpoints <= 0) {
			sprintf(numstr,"u%07d.atk",*user_num);
			ny_remove(numstr);
			sprintf(numstr,"u%07d.atk",*enm_num);
			justfile = ShareFileOpen(numstr, "wb");
			if(justfile != NULL) {
				intval=-9; //user lost
				ny_fwrite(&intval,4,1,justfile);
				intval=user_on->money; //users money
				ny_fwrite(&intval,4,1,justfile);
				fclose(justfile);
			}

			od_printf("\n\r\n");
			ny_send_menu(ASS_KICKED_O,"");
			/*	  od_printf("\n\r\n`bright red`Y`red`a `bright white`LOST`red`!!!!\n\r");
				  od_printf("`bright red`C`red`ome back tomorrow to get revenge\n\r");
				  od_printf("`bright red`8%c`red` of yer points lost\n\r",37);*/

			user_on->alive=UNCONCIOUS;
			user_on->money=0;
			points_loose(user_on->points*.08);
			//wrt_sts();

			if (erec->sex!=user_on->sex) {
				sprintf(numstr,"u%07d.kik",*user_num);
				sprintf(numstr2,"u%07d.on",*enm_num);

				ny_line(225,2,1);
				//	    od_printf("\n\r\n`bright red`W`red`aiting fer enemy response....\n\r");

				t=time(NULL);
				t2=t;
				while (!fexist(numstr) && fexist(numstr2)) {
					fgc(enm_num,user_num);
					sprintf(numstr2,"u%07d.on",*enm_num);
					sprintf(numstr,"u%07d.kik",*user_num);
					while(t==t2) {
						t=time(NULL);
						od_kernal();
					}
					t2=t;
					od_kernal();
				}
				if (fexist(numstr)) {
					INT16 ill;
					INT16 inf;

					justfile=ShareFileOpen(numstr,"rb");
					if(justfile != NULL) {
						ny_fread(&ill,2,1,justfile);
						ny_fread(&inf,2,1,justfile);
						fclose(justfile);
					}

					ny_remove(numstr);
					//sprintf(numstr,"del u%07d.kik",*user_num);
					//system(numstr);

					if (ill!=1) {
						illness((desease)ill,inf);


						ny_disp_emu("\n\r`0");
						ny_disp_emu(erec->name);
						ny_line(232,0,1);
						//		od_printf(" `red`raped ya!\n\r");

					}
				}
			}


			fig_ker();
			if(!rip)
				ny_line(1,1,0);
			od_get_answer("\n\r");

			od_exit(10,FALSE);
		}

		sprintf(numstr,"u%07d.atk",*user_num);
		ny_remove(numstr);
	} else {

		od_printf("\n\r\n`0");
		ny_disp_emu(erec->name);
		ny_line(231,2,0);
		//	od_printf("\n\r\n`bright green`%s`red` tried to get away but could not ...");

	}
	return FALSE;
}



void
online_fight_a(INT16 *user_num, user_rec *user_on, INT16 enm_num) {
	//  mail_idx_type mail_idx;
	char numstr[25];
	char numstr2[25];
	//  DWORD intval;
	INT16 rape;
	user_rec erec;
	//  ffblk ffblk;
	FILE *justfile;
	char key;
	//  long hit_s,def_s,en_hit_s;
	//  time_t t,t2;

	sfflg(*user_num,numstr,enm_num,&erec);

	/*  ch_flag_d();
	 
	  sprintf(numstr,"u%07d.fgg",*user_num); //set fight flag
	  justfile = ShareFileOpen(numstr, "wb");
	  fclose(justfile);
	 
	  //readin the enemy record
	  sprintf(numstr,"u%07d.sts",enm_num);
	  justfile=ShareFileOpen(numstr,"rb");
	  ny_fread(&erec,sizeof(user_rec),1,justfile);
	  fclose(justfile);*/

	sprintf(numstr,"u%07d.chl",enm_num); //set fight flag
	justfile = ShareFileOpen(numstr, "wb");
	if(justfile != NULL) {
		ny_fwrite(user_num,2,1,justfile);
		fclose(justfile);
	}
	sprintf(numstr2,"u%07d.on",enm_num);

	ny_line(219,2,1);
	//  od_printf("\n\r\n`bright red`L`red`ookin fer yer enemy ....\n\r");

	key=od_get_key(FALSE);
	while ((fexist(numstr)) && (fexist(numstr2)) && key!='S' && key!='s') {
		key=od_get_key(FALSE);
		fgc(&enm_num,user_num);
		//    sprintf(numstr2,"u%07d.on",enm_num);
		//    sprintf(numstr,"u%07d.chl",enm_num);
		od_kernal();
		sleep(0);
	}

	if (!fexist(numstr2)) {

		ny_line(220,2,1);
		//od_printf("\n\r\n`bright red`U`red`ser is not online anymore!\n\r");

		//    sprintf(numstr,"del u%07d.chl",enm_num);
		//    system(numstr);
		ny_remove(numstr);
		sprintf(numstr,"u%07d.fgg",enm_num);
		ny_remove(numstr);
		if(!rip)
			ny_line(1,1,0);
		od_get_answer("\n\r");
		return;
	}

	if (key=='S' || key=='s') {

		ny_line(337,2,1);
		//You stopped looking ...

		//sprintf(numstr,"del u%07d.chl",enm_num);
		//system(numstr);
		ny_remove(numstr);
		sprintf(numstr,"u%07d.fgg",enm_num);
		ny_remove(numstr);
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		return;
	}



	do {
		if(fig_m(&erec,user_on,&enm_num,user_num))
			return;
		if(o_checks(&enm_num,user_num,user_on,&erec))
			return;
		/*   do {
		 
		    bam_of1:
		    ny_line(221,3,0);
		//    od_printf("\n\r\n\n\r`bright red`O`red`nline `bright red`P`red`layer `bright red`F`red`ight");
		    ny_disp_emu("\n\r\n`0");
		    ny_disp_emu(erec.name);
		    ny_line(205,0,0);
		//    od_printf("'s `bright red`H`red`itpoints: `0
		    od_printf(D_Num(erec.hitpoints));
		    ny_line(206,1,0);
		//    od_printf("\n\r`bright red`Y`red`er `bright red`H`red`itpoints:
		    od_printf("%s\n\r\n",D_Num(user_on->hitpoints));
		/*    od_printf("\n\r\n`bright red`[A] `red`- `bright red`A`red`ttack\n\r");
		    od_printf("`bright red`[T] `red`- `bright red`T`red`ake `bright red`D`red`rug `bright red`A`red`nd `bright red`A`red`ttack\n\r");
		    od_printf("`bright red`[G] `red`- `bright red`G`red`et `bright red`O`red`utta `bright red`H`red`ere\n\r");
		    od_printf("`bright red`[Y] `red`- `bright red`Y`red`er `bright red`S`red`tats\n\r");
		    od_printf("\n\r`bright red`W`red`hat ya gonna do? (`bright red`[A] G Y`red`)");     -/
		    ny_send_menu(ATTACK,"");
		    ny_line(207,1,0);
		    //ny_disp_emu("\n\r`@W`4hat ya gonna do? (`@[A] T G Y`4)");
		 
		 
		    key=od_get_answer("ATKPRGY\n\r");
		    if (key=='\n' || key=='\r') key='A';
		    od_putch(key);
		    fgc(&enm_num,user_num);
		    if (key=='Y') {
		      DisplayStats();
		      WaitForEnter();
		    }
		   } while (key=='Y');
		 
		   if(key=='R' && user_on->rocks==0) {
		     ny_line(445,2,1);
		     WaitForEnter();
		     goto bam_of1;
		   }
		 
		 
		   if (key == 'T') {  // Take drugs
		     take_drug();
		     no_rip_m=0;
		     key='A';
		   }*/
		/*if (key=='A' || key=='K' || key=='P' || key=='R') {
		  // when atacking
		//   randomize();
		  if(key=='K') {
		    hit_s=xp_random(150 - user_on->kick_ability);
		    if(hit_s<35) {
		hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->kick_ability/100.0)) * (double)((xp_random(80)+90)/100.0);
		ny_line(439,2,0);
		    } else {
		hit_s=0;
		ny_line(440,2,0);
		    }
		  } else if(key=='P') {
		    hit_s=xp_random(140 - user_on->punch_ability);
		    if(hit_s<35) {
		hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->punch_ability/90.0)) * (double)((xp_random(80)+90)/100.0);
		ny_line(441,2,0);
		    } else {
		hit_s=0;
		ny_line(442,2,0);
		    }
		  } else if(key=='R') {
		    hit_s=xp_random(145 - user_on->throwing_ability);
		    user_on->rocks--;
		    if(hit_s<35) {
		hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->throwing_ability/110.0)) * (double)((xp_random(80)+90)/100.0);
		ny_line(443,2,0);
		    } else {
		hit_s=0;
		ny_line(444,2,0);
		    }
		  }

		/*  if (key == 'T') {  // Take drugs
		   take_drug();
		   no_rip_m=0;
		   key='A';
		 }
		 if (key=='A') {-/
		   // when atacking
		//      randomize();
		   if(key=='A')
		hit_s = user_on->strength * (what_arm_force(user_on->arm)) * (double)((xp_random(80)+80)/100.0);
		 // If he is on drugs
		   if(user_on->drug_high > 0)
		hit_s = (hit_s * what_drug_force_a(user_on->drug,user_on->drug_high));

		//      hit_s*=pow(1.1,user_on->level);

		   hit_s*=(100.0-(user_on->since_got_laid*2))/100.0;
		   if (user_on->drug>=COKE) hit_s-=xp_random(user_on->drug_days_since * pow((double)user_on->drug_addiction,1.2))/60;
		   if (hit_s<0) hit_s=0;

		   def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
		   def_s*=pow(1.2,user_on->level);
		   def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
		   if (user_on->drug>=COKE) def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
		   if (def_s<0) def_s=0;

		   def_s/=2;

		   sprintf(numstr,"u%07d.atk",enm_num);
		   justfile = ShareFileOpen(numstr, "wb");
		   ny_fwrite(&hit_s,4,1,justfile);
		   fclose(justfile);
		   sprintf(numstr,"u%07d.atk",enm_num);
		   sprintf(numstr2,"u%07d.on",enm_num);
		   ny_line(222,2,1);
		//      od_printf("\n\r\n`bright red`W`red`aiting....\n\r");
		   t=time(NULL);
		   t2=t;

		   while ((fexist(numstr)) && (fexist(numstr2))){
		fgc(&enm_num,user_num);
		sprintf(numstr,"u%07d.atk",enm_num);
		sprintf(numstr2,"u%07d.on",enm_num);
		while(t==t2) {
		t=time(NULL);
		od_kernal();
		}
		t2=t;
		od_kernal();
		   }

		   if (!fexist(numstr2)) {

		ny_line(223,2,1);
		if(rip) od_get_answer("\n\r");
		ny_line(224,0,1);
		//	od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");

		//sprintf(numstr,"del u%07d.atk",enm_num);
		//system(numstr);
		ny_remove(numstr);
		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		if(!rip) ny_line(1,1,0);
		od_get_answer("\n\r");
		return;
		   }

		   intval=erec.hitpoints;

		   sprintf(numstr,"u%07d.sts",enm_num);
		   justfile=ShareFileOpen(numstr,"rb");
		   ny_fread(&erec,sizeof(user_rec),1,justfile);
		   fclose(justfile);

		   intval -= erec.hitpoints;

		   fgc(&enm_num,user_num);


		   ny_line(208,2,0);
		//      od_printf("\n\r\n`bright red`Y`red`a kick `bright green`");
		   ny_disp_emu(erec.name);
		   ny_line(209,0,0);
		   //od_printf("'s `red`ass fer `bright green`%d `red`damage",
		   od_printf(D_Num(intval));
		   ny_line(210,0,0);


		  } else if (key=='G') {
		    intval=xp_random(2);
		    if (intval==0) {

		ny_line(212,2,1);
		//od_printf("\n\r\n`bright red`Y`red`a got away ...\n\r");

		sprintf(numstr,"u%07d.atk",enm_num);
		justfile = ShareFileOpen(numstr, "wb");
		intval=-1; //user got away
		ny_fwrite(&intval,4,1,justfile);
		fclose(justfile);
		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		if(!rip)
		 WaitForEnter();
		else
		 od_get_answer("\n\r");
		return;
		    } else {

		ny_line(213,2,1);
		//	 od_printf("\n\r\n`bright red`Y`red`e couldn't find a way outta this ...\n\r");

		sprintf(numstr,"u%07d.atk",enm_num);
		justfile = ShareFileOpen(numstr, "wb");
		intval=-2; ///user couldn't get away
		ny_fwrite(&intval,4,1,justfile);
		fclose(justfile);
		sprintf(numstr,"u%07d.atk",enm_num);
		sprintf(numstr2,"u%07d.on",enm_num);
		ny_line(222,2,1);

		t=time(NULL);
		t2=t;

		//od_printf("\n\r\n`bright red`W`red`aiting....\n\r");
		while ((fexist(numstr)) && (fexist(numstr2))){
		 fgc(&enm_num,user_num);
		 sprintf(numstr,"u%07d.atk",enm_num);
		 sprintf(numstr2,"u%07d.on",enm_num);
		 while(t==t2) {
		   t=time(NULL);
		   od_kernal();
		 }
		 t2=t;

		 od_kernal();
		}

		if (!fexist(numstr2)) {

		 ny_line(223,2,1);
		 if(rip) od_get_answer("\n\r");
		 ny_line(224,0,1);
		 //od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");

		//	   sprintf(numstr,"del u%07d.atk",enm_num);
		//	   system(numstr);
		 ny_remove(numstr);
		 sprintf(numstr,"u%07d.fgg",*user_num);
		 ny_remove(numstr);
		 if(!rip)
		   WaitForEnter();
		 else
		   od_get_answer("\n\r");
		 return;
		}
		    }
		  }*/// else {
		// DisplayStats();
		//}

		/*      sprintf(numstr,"u%07d.atk",*user_num);
		 
		      ny_line(225,2,1);
		//      od_printf("\n\r\n`bright red`W`red`aiting fer enemy response....\n\r");
		 
		 
		      t=time(NULL);
		      t2=t;
		      while (!fexist(numstr) && fexist(numstr2)) {
			fgc(&enm_num,user_num);
			sprintf(numstr2,"u%07d.on",enm_num);
			sprintf(numstr,"u%07d.atk",*user_num);
			while(t==t2) {
			  t=time(NULL);
			  od_kernal();
			}
			t2=t;
			od_kernal();
		      }
		      fgc(&enm_num,user_num);
		      if (!fexist(numstr2)) {
		 
			ny_line(223,2,1);
			if(rip) od_get_answer("\n\r");
			ny_line(224,0,1);
		//	od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");
		 
			sprintf(numstr,"u%07d.fgg",*user_num);
			ny_remove(numstr);
			if(!rip)
			  WaitForEnter();
			else
			  od_get_answer("\n\r");
			return;
		      }
		 
		      //read atack file
		      sprintf(numstr,"u%07d.atk",*user_num);
		      justfile = ShareFileOpen(numstr, "rb");
		      ny_fread(&en_hit_s,4,1,justfile);
		      fclose(justfile);
		 
		      if (en_hit_s==-1) {
		 
			ny_line(226,2,1);
		//	od_printf("\n\r\n`bright red`Y`red`er enemy ran away in fear...\n\r");
		 
			ny_remove(numstr);
			sprintf(numstr,"u%07d.fgg",*user_num);
			ny_remove(numstr);
			//sprintf(numstr,"del u%07d.atk",*user_num);
			//system(numstr); //see above!
			if(!rip)
			  WaitForEnter();
			else
			  od_get_answer("\n\r");
			return;
		      }
		      if (en_hit_s==-9) {
		 
			ny_line(227,2,2);
		//	od_printf("\n\r\n`bright red`Y`red`a `bright white`WON`red`!\n\r\n");
			sprintf(numstr,"u%07d.atk",*user_num);
			justfile = ShareFileOpen(numstr, "rb");
		 
			ny_fread(&en_hit_s,4,1,justfile);
			ny_fread(&intval,4,1,justfile);
		 
			fclose(justfile);
		 
			money_plus(intval);
		 
			ny_line(216,0,0);
		//	od_printf("`bright red`Y`red`a find `bright red`
			od_printf(D_Num(intval));
			ny_line(217,0,0);
			intval=.08*erec.points;
			od_printf(D_Num((DWORD)intval * (DWORD)DrgPtsCoef()));
			ny_line(218,0,1);
			points_raise(intval);
		 
			ny_kernel();
		 
		 
			//no_kernel=TRUE;
			wrt_sts();
			//no_kernel=FALSE;
		 
			if (erec.arm>user_on->arm) {
		 
			  ny_line(228,2,0);
		//	  od_printf("\n\r\n`bright red`D`red`o ya wanna swap weapons with`bright green` ");
			  if(!rip)
			    ny_disp_emu(erec.name);
			  else
			    od_disp_str(ny_un_emu(erec.name,numstr));
			  ny_line(79,0,0);
			  //ny_disp_emu("`4? (`@Y`4/`@N`4)");
		 
			  key=od_get_answer("YN");
			  if(!rip)
			    od_printf("%c\n\r",key);
			  else
			    od_disp_str("\n\r");
		 
			  if (key=='Y') {
		 
			    weapon tarm;
		 
			    tarm=erec.arm;
			    erec.arm=user_on->arm;
			    user_on->arm=tarm;
			    sprintf(numstr,"u%07d.on",enm_num);
			    if (fexist(numstr)) {
			      sprintf(numstr,"u%07d.swp",enm_num);
			      justfile=ShareFileOpen(numstr,"wb");
			      ny_fwrite(&erec.arm,2,1,justfile);
			      fclose(justfile);
			    } else {
			      tarm=erec.arm;
			      ch_game_d();
			      justfile=ShareFileOpen(USER_FILENAME,"r+b");
			      fseek(justfile,enm_num*sizeof(user_rec),SEEK_SET);
			      ny_fread(&erec,sizeof(user_rec),1,justfile);
			      erec.arm=tarm;
			      fseek(justfile,enm_num*sizeof(user_rec),SEEK_SET);
			      ny_fwrite(&erec,sizeof(user_rec),1,justfile);
			      fclose(justfile);
			      ch_flag_d();
			    }
		 
			    ny_line(229,2,0);
		//	    od_printf("\n\r`bright red`D`red`one! `bright red`Y`red`ou now got the `bright green`");
		 
			    print_arm(user_on->arm);
			    if(rip) {
			      od_disp_str("::^M@OK))|#|#|#\n\r\n");
			      od_get_answer("\n\r");
			    }
			    od_printf("\n\r\n");
		 
			  }
			}
		 
			if (erec.sex!=user_on->sex && clean_mode==FALSE && user_on->sex_today>0) {
		 
			  ny_line(230,2,0);
		//	  od_printf("\n\r\n`bright red`D`red`o ya wanna rape `bright green`");
			  if(!rip)
			    ny_disp_emu(erec.name);
			  else
			    od_disp_str(ny_un_emu(erec.name,numstr));
			  ny_line(79,0,0);
		//	  ny_disp_emu("`4? (`@Y`4/`@N`4)");
		 
			  key=od_get_answer("YN");
			  if(!rip)
			    od_printf("%c\n\r",key);
			  else
			    od_disp_str("\n\r");
			  if (key=='Y') {
			    if (user_on->sex_today<=0) {
		 
			      ny_line(118,2,1);
			      //od_printf("\n\r\n`bright white`You already used up all your sex turns today ...\n\r");
		 
			      if(rip)
				od_get_answer("\n\r");
			      sprintf(numstr,"u%07d.on",enm_num);
			      if (fexist(numstr)) {
				sprintf(numstr,"u%07d.kik",enm_num);
				justfile=ShareFileOpen(numstr,"wb");
				rape=-1;
				ny_fwrite(&rape,2,2,justfile);
				fclose(justfile);
			      }
		//	      WaitForEnter();
			    } else {
			      sprintf(numstr,"u%07d.on",enm_num);
			      if (fexist(numstr)) {
				sprintf(numstr,"u%07d.kik",enm_num);
				justfile=ShareFileOpen(numstr,"wb");
				ny_fwrite(&user_on->std,2,1,justfile);
				ny_fwrite(&user_on->std_percent,2,1,justfile);
				fclose(justfile);
			      } else {
				strcpy(mail_idx.recver,erec.name);
				strcpy(mail_idx.sender,user_on->name);
				strcpy(mail_idx.recverI,erec.bbsname);
				strcpy(mail_idx.senderI,user_on->bbsname);
				mail_idx.flirt=1000;
				mail_idx.deleted=FALSE;
				mail_idx.location=0;
				mail_idx.length=0;
				mail_idx.afterquote=0;
				mail_idx.ill=user_on->std;
				mail_idx.inf=user_on->std_percent;
				mail_idx.sender_sex=user_on->sex;
				ch_game_d();
				justfile=ShareFileOpen(MAIL_INDEX,"a+b");
				ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
				fclose(justfile);
				ch_flag_d();
			      }
			      user_on->since_got_laid=0;
			      user_on->sex_today--;
			      ny_line(339,1,0);
			      if(rip)
				ny_disp_emu(erec.name);
			      else
				od_disp_str(ny_un_emu(erec.name,numstr));
			      ny_line(340,0,1);
			      if(rip)
				od_get_answer("\n\r");
			      illness(erec.std,erec.std_percent);
			      points_raise((INT32)35*(user_on->level+1));
		 
		//	      char omg[51];
		 
			     // sprintf(omg,"%s raped you after beating you ...",user_on->name);
			    }
			  } else {
			    sprintf(numstr,"u%07d.on",enm_num);
			    if (fexist(numstr)) {
			      sprintf(numstr,"u%07d.kik",enm_num);
			      justfile=ShareFileOpen(numstr,"wb");
			      rape=-1;
			      ny_fwrite(&rape,2,2,justfile);
			      fclose(justfile);
			    }
			  }
			}
		 
			sprintf(numstr,"u%07d.fgg",*user_num);
			ny_remove(numstr);
		 
			sprintf(numstr,"u%07d.atk",*user_num);
			ny_remove(numstr);
		 
			wrt_sts();
		 
			if(!rip) WaitForEnter();
			return;
		      }
		      if (en_hit_s!=-2) {
			en_hit_s-=def_s;
			if (en_hit_s<0) en_hit_s=0;
		 
			user_on->hitpoints-=en_hit_s;
			wrt_sts();
		 
		 
			ny_disp_emu("\n\r\n`0");
			ny_disp_emu(erec.name);
			ny_line(211,0,0);
		//	od_printf(" `red`kicks yer ass fer `bright green`
			od_printf(D_Num(en_hit_s));
			ny_line(210,0,0);
		 
		 
			if (user_on->hitpoints <= 0) {
			  sprintf(numstr,"u%07d.atk",*user_num);
			  ny_remove(numstr);
			  sprintf(numstr,"u%07d.atk",enm_num);
			  justfile = ShareFileOpen(numstr, "wb");
			  intval=-9; //user lost
			  ny_fwrite(&intval,4,1,justfile);
			  intval=user_on->money; //users money
			  ny_fwrite(&intval,4,1,justfile);
			  fclose(justfile);
		 
			  od_printf("\n\r\n");
			  ny_send_menu(ASS_KICKED_O,"");
		/*	  od_printf("\n\r\n`bright red`Y`red`a `bright white`LOST`red`!!!!\n\r");
			  od_printf("`bright red`C`red`ome back tomorrow to get revenge\n\r");
			  od_printf("`bright red`8%c`red` of yer points lost\n\r",37);-/
		 
			  user_on->alive=UNCONCIOUS;
			  user_on->money=0;
			  points_loose(user_on->points*.08);
			  //wrt_sts();
		 
			  if (erec.sex!=user_on->sex) {
			    sprintf(numstr,"u%07d.kik",*user_num);
			    sprintf(numstr2,"u%07d.on",enm_num);
		 
			    ny_line(225,2,1);
		//	    od_printf("\n\r\n`bright red`W`red`aiting fer enemy response....\n\r");
		 
			    t=time(NULL);
			    t2=t;
			    while (!fexist(numstr) && fexist(numstr2)) {
			      fgc(&enm_num,user_num);
			      sprintf(numstr2,"u%07d.on",enm_num);
			      sprintf(numstr,"u%07d.kik",*user_num);
			      while(t==t2) {
				t=time(NULL);
				od_kernal();
			      }
			      t2=t;
			      od_kernal();
			    }
			    if (fexist(numstr)) {
			      INT16 ill;
			      INT16 inf;
		 
			      justfile=ShareFileOpen(numstr,"rb");
			      ny_fread(&ill,2,1,justfile);
			      ny_fread(&inf,2,1,justfile);
			      fclose(justfile);
		 
			      ny_remove(numstr);
			      //sprintf(numstr,"del u%07d.kik",*user_num);
			      //system(numstr);
		 
			      if (ill!=1) {
				illness((desease)ill,inf);
		 
		 
				ny_disp_emu("\n\r`0");
				ny_disp_emu(erec.name);
				ny_line(232,0,1);
		//		od_printf(" `red`raped ya!\n\r");
		 
			      }
			    }
			  }
		 
		 
			  fig_ker();
			  WaitForEnter();
			  od_exit(10,FALSE);
			}
		 
			sprintf(numstr,"u%07d.atk",*user_num);
			ny_remove(numstr);
		      } else {
		 
			od_printf("\n\r\n`0");
			ny_disp_emu(erec.name);
			ny_line(231,2,0);
		//	od_printf("\n\r\n`bright green`%s`red` tried to get away but could not ...");
		 
		      }*/

	} while (1);
}




void
online_fight(INT16 *user_num, user_rec *user_on, INT16 enm_num) {
	//  mail_idx_type mail_idx;
	//  INT16 rape;
	char numstr[25];
	//  char numstr2[25];
	//  DWORD intval;
	user_rec erec;
	//  ffblk ffblk;
	//  FILE *justfile;
	//  char key;
	//  long hit_s,def_s,en_hit_s;
	//  time_t t,t2;

	sfflg(*user_num,numstr,enm_num,&erec);

	ny_disp_emu("\n\r\n`@");
	ny_disp_emu(erec.name);
	ny_line(294,0,1);

	sprintf(numstr,"u%07d.chl",*user_num);
	ny_remove(numstr);
	do {
		if(o_checks(&enm_num,user_num,user_on,&erec))
			return;
		if(fig_m(&erec,user_on,&enm_num,user_num))
			return;


		/*    sprintf(numstr,"u%07d.atk",*user_num);
		    sprintf(numstr2,"u%07d.on",enm_num);

		    ny_line(225,2,1);
		//      od_printf("\n\r\n`bright red`W`red`aiting fer enemy response....\n\r");

		    t=time(NULL);
		    t2=t;
		    while (!fexist(numstr) && fexist(numstr2)) {
		fgc(&enm_num,user_num);
		sprintf(numstr2,"u%07d.on",enm_num);
		sprintf(numstr,"u%07d.atk",*user_num);
		while(t==t2) {
		 t=time(NULL);
		 od_kernal();
		}
		t2=t;
		od_kernal();
		    }

		//      fgc(&enm_num,user_num);
		    if (!fexist(numstr2)) {

		ny_line(223,2,1);
		if(rip) od_get_answer("\n\r");
		ny_line(224,0,1);
		//	od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");

		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		if(!rip) ny_line(1,1,0);
		od_get_answer("\n\r");
		return;
		    }

		    //read atack file
		    sprintf(numstr,"u%07d.atk",*user_num);
		    justfile = ShareFileOpen(numstr, "rb");
		    ny_fread(&en_hit_s,4,1,justfile);
		    fclose(justfile);

		    if (en_hit_s==-1) {

		ny_line(226,2,1);
		//	od_printf("\n\r\n`bright red`Y`red`er enemy ran away in fear...\n\r");

		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		sprintf(numstr,"u%07d.atk",*user_num);
		ny_remove(numstr);
		if(!rip) ny_line(1,1,0);
		od_get_answer("\n\r");
		return;
		    }
		    if (en_hit_s==-9) {

		ny_line(227,2,2);
		//	od_printf("\n\r\n`bright red`Y`red`a `bright white`WON`red`!\n\r\n");
		sprintf(numstr,"u%07d.atk",*user_num);
		justfile = ShareFileOpen(numstr, "rb");
		ny_fread(&en_hit_s,4,1,justfile);
		ny_fread(&intval,4,1,justfile);
		fclose(justfile);
		money_plus(intval);
		ny_line(216,0,0);
		od_printf(D_Num(intval));
		intval=.08*erec.points;
		ny_line(217,0,0);
		od_printf(D_Num((DWORD)intval * (DWORD)DrgPtsCoef()));
		ny_line(218,0,1);
		points_raise(intval);

		wrt_sts();
		if (erec.arm>user_on->arm) {

		 ny_line(228,2,0);
		//	  od_printf("\n\r\n`bright red`D`red`o ya wanna swap weapons with`bright green` ");
		 ny_disp_emu(erec.name);
		 ny_line(79,0,0);
		 //ny_disp_emu("`4? (`@Y`4/`@N`4)");

		 key=od_get_answer("YN");
		 if(!rip)
		   od_printf("%c\n\r",key);
		 else
		   od_disp_str("\n\r");
		 if (key=='Y') {

		   weapon tarm;

		   tarm=erec.arm;
		   erec.arm=user_on->arm;
		   user_on->arm=tarm;
		   sprintf(numstr,"u%07d.on",enm_num);
		   if (fexist(numstr)) {
		     sprintf(numstr,"u%07d.swp",enm_num);
		     justfile=ShareFileOpen(numstr,"wb");
		     ny_fwrite(&erec.arm,2,1,justfile);
		     fclose(justfile);
		   } else {
		     tarm=erec.arm;
		     ch_game_d();
		     justfile=ShareFileOpen(USER_FILENAME,"r+b");
		     fseek(justfile,enm_num*sizeof(user_rec),SEEK_SET);
		     ny_fread(&erec,sizeof(user_rec),1,justfile);
		     erec.arm=tarm;
		     fseek(justfile,enm_num*sizeof(user_rec),SEEK_SET);
		     ny_fwrite(&erec,sizeof(user_rec),1,justfile);
		     fclose(justfile);
		     ch_flag_d();
		   }

		   ny_line(229,2,0);
		//	    od_printf("\n\r`bright red`D`red`one! `bright red`Y`red`ou now got the `bright green`");

		   print_arm(user_on->arm);
		   if(rip) {
		     od_disp_str("::^M@OK))|#|#|#\n\r\n");
		     od_get_answer("\n\r");
		   }
		   od_printf("\n\r\n");

		 }
		}

		if (erec.sex!=user_on->sex && clean_mode==FALSE && user_on->sex_today>0) {

		 ny_line(230,2,0);
		//	  od_printf("\n\r\n`bright red`D`red`o ya wanna rape `bright green`");
		 ny_disp_emu(erec.name);
		 ny_line(79,0,0);
		//	  ny_disp_emu("`4? (`@Y`4/`@N`4)");

		 key=od_get_answer("YN");
		 if(!rip)
		   od_printf("%c\n\r",key);
		 else
		   od_disp_str("\n\r");
		 if (key=='Y') {
		   if (user_on->sex_today<=0) {

		     ny_line(118,2,1);
		//	      od_printf("\n\r\n`bright white`You already used up all your sex turns today ...\n\r");

		     if(rip)
		od_get_answer("\n\r");
		     sprintf(numstr,"u%07d.on",enm_num);
		     if (fexist(numstr)) {
		sprintf(numstr,"u%07d.kik",enm_num);
		justfile=ShareFileOpen(numstr,"wb");
		rape=-1;
		ny_fwrite(&rape,2,2,justfile);
		fclose(justfile);
		     }
		//	      WaitForEnter();
		   } else {
		     sprintf(numstr,"u%07d.on",enm_num);
		     if (fexist(numstr)) {
		sprintf(numstr,"u%07d.kik",enm_num);
		justfile=ShareFileOpen(numstr,"wb");
		ny_fwrite(&user_on->std,2,1,justfile);
		ny_fwrite(&user_on->std_percent,2,1,justfile);
		fclose(justfile);
		     } else {
		strcpy(mail_idx.recver,erec.name);
		strcpy(mail_idx.sender,user_on->name);
		strcpy(mail_idx.recverI,erec.bbsname);
		strcpy(mail_idx.senderI,user_on->bbsname);
		mail_idx.flirt=1000;
		mail_idx.deleted=FALSE;
		mail_idx.location=0;
		mail_idx.length=0;
		mail_idx.afterquote=0;
		mail_idx.ill=user_on->std;
		mail_idx.inf=user_on->std_percent;
		mail_idx.sender_sex=user_on->sex;
		ch_game_d();
		justfile=ShareFileOpen(MAIL_INDEX,"a+b");
		ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
		fclose(justfile);
		ch_flag_d();
		     }

		     user_on->since_got_laid=0;
		     user_on->sex_today--;
		     ny_line(339,1,0);
		     if(!rip)
		ny_disp_emu(erec.name);
		     else
		od_disp_str(ny_un_emu(erec.name,numstr));
		     ny_line(340,0,1);
		     if(rip)
		od_get_answer("\n\r");

		     illness(erec.std,erec.std_percent);
		     points_raise((INT32)35*(user_on->level+1));
		   }
		 } else {
		   sprintf(numstr,"u%07d.on",enm_num);
		   if (fexist(numstr)) {
		     sprintf(numstr,"u%07d.kik",enm_num);
		     justfile=ShareFileOpen(numstr,"wb");
		     rape=-1;
		     ny_fwrite(&rape,2,2,justfile);
		     fclose(justfile);
		   }
		 }
		}
		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		sprintf(numstr,"u%07d.atk",*user_num);
		ny_remove(numstr);
		if(!rip) {
		 ny_line(1,1,0);
		 od_get_answer("\n\r");
		}
		return;
		    }
		    if (en_hit_s!=-2) {
		en_hit_s-=def_s;
		if (en_hit_s<0) en_hit_s=0;

		user_on->hitpoints-=en_hit_s;
		wrt_sts();


		ny_disp_emu("\n\r\n`0");
		ny_disp_emu(erec.name);
		ny_line(211,0,0);
		od_printf(D_Num(en_hit_s));
		ny_line(210,0,0);


		if (user_on->hitpoints <= 0) {
		 sprintf(numstr,"u%07d.atk",*user_num);
		 ny_remove(numstr);
		 sprintf(numstr,"u%07d.atk",enm_num);
		 justfile = ShareFileOpen(numstr, "wb");
		 intval=-9; //user lost
		 ny_fwrite(&intval,4,1,justfile);
		 intval=user_on->money; //users money
		 ny_fwrite(&intval,4,1,justfile);
		 fclose(justfile);

		 od_printf("\n\r\n");
		 ny_send_menu(ASS_KICKED_O,"");
		 /*`bright red`Y`red`a `bright white`LOST`red`!!!!\n\r");
		 od_printf("`bright red`C`red`ome back tomorrow to get revenge\n\r");
		 od_printf("`bright red`8%c`red` of yer points lost\n\r",37);-/

		 user_on->alive=UNCONCIOUS;
		 user_on->money=0;
		 points_loose(user_on->points*.08);
		 //wrt_sts();



		 if (erec.sex!=user_on->sex) {
		   sprintf(numstr,"u%07d.kik",*user_num);
		   sprintf(numstr2,"u%07d.on",enm_num);

		   ny_line(225,2,1);
		//	    od_printf("\n\r\n`bright red`W`red`aiting fer enemy response....\n\r");

		   t=time(NULL);
		   t2=t;
		   while (!fexist(numstr) && fexist(numstr2)) {
		     fgc(&enm_num,user_num);
		     sprintf(numstr2,"u%07d.on",enm_num);
		     sprintf(numstr,"u%07d.kik",*user_num);
		     while(t==t2) {
		t=time(NULL);
		od_kernal();
		     }
		     t2=t;
		     od_kernal();
		   }
		   if (fexist(numstr)) {
		     INT16 ill;
		     INT16 inf;

		     justfile=ShareFileOpen(numstr,"rb");
		     ny_fread(&ill,2,1,justfile);
		     ny_fread(&inf,2,1,justfile);
		     fclose(justfile);

		     sprintf(numstr,"u%07d.kik",*user_num);
		     ny_remove(numstr);

		     if (ill!=-1) {
		illness((desease)ill,inf);

		ny_disp_emu("\n\r`0");
		ny_disp_emu(erec.name);
		ny_line(232,0,1);
		//		od_printf(" `red`raped ya!\n\r");

		     }
		   }
		 }
		 ny_line(1,1,0);
		 od_get_answer("\n\r");
		 od_exit(10,FALSE);
		}

		sprintf(numstr,"u%07d.atk",*user_num);
		ny_remove(numstr);
		    } else {

		od_printf("\n\r\n`0");
		ny_disp_emu(erec.name);
		ny_line(231,0,0);
		//	od_printf("\n\r\n`bright green`%s`red` tried to get away but could not ...");

		    }*/


		/*do {

		  bam_of2:
		  ny_line(221,3,0);
		//      od_printf("\n\r\n\n\r`bright red`O`red`nline `bright red`P`red`layer `bright red`F`red`ight");
		  ny_disp_emu("\n\r\n`0");
		  ny_disp_emu(erec.name);
		  ny_line(205,0,0);
		//      od_printf("'s `bright red`H`red`itpoints: %s",
		  od_printf(D_Num(erec.hitpoints));
		  ny_line(206,1,0);
		//      od_printf("\n\r`bright red`Y`red`er `bright red`H`red`itpoints: %s",
		od_printf(D_Num(user_on->hitpoints));
		  od_printf("\n\r\n");
		/*      od_printf("\n\r\n`bright red`[A] `red`- `bright red`A`red`ttack\n\r");
		  od_printf("`bright red`[T] `red`- `bright red`T`red`ake `bright red`D`red`rug `bright red`A`red`nd `bright red`A`red`ttack\n\r");
		  od_printf("`bright red`[G] `red`- `bright red`G`red`et `bright red`O`red`utta `bright red`H`red`ere\n\r");
		  od_printf("`bright red`[Y] `red`- `bright red`Y`red`er `bright red`S`red`tats\n\r");
		  od_printf("\n\r`bright red`W`red`hat ya gonna do? (`bright red`[A] G Y`red`)");-/
		  ny_send_menu(ATTACK,"");
		  ny_line(207,1,0);
		//      ny_disp_emu("\n\r`@W`4hat ya gonna do? (`@[A] T G Y`4)");


		  key=od_get_answer("ATKPRGY\n\r");
		  if (key=='\n' || key=='\r') key='A';
		  od_putch(key);
		  fgc(&enm_num,user_num);
		  if (key=='Y') {
		DisplayStats();
		ny_line(1,1,0);
		od_get_answer("\n\r");
		  }
		} while (key=='Y');

		if(key=='R' && user_on->rocks==0) {
		 ny_line(445,2,1);
		 WaitForEnter();
		 goto bam_of2;
		}


		if (key == 'T') {  // Take drugs
		 take_drug();
		 no_rip_m=0;
		 key='A';
		}
		if (key=='A' || key=='K' || key=='P' || key=='R') {
		 // when atacking
		//   randomize();
		 if(key=='K') {
		   hit_s=xp_random(150 - user_on->kick_ability);
		   if(hit_s<35) {
		hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->kick_ability/100.0)) * (double)((xp_random(80)+90)/100.0);
		ny_line(439,2,0);
		   } else {
		hit_s=0;
		ny_line(440,2,0);
		   }
		 } else if(key=='P') {
		   hit_s=xp_random(140 - user_on->punch_ability);
		   if(hit_s<35) {
		hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->punch_ability/90.0)) * (double)((xp_random(80)+90)/100.0);
		ny_line(441,2,0);
		   } else {
		hit_s=0;
		ny_line(442,2,0);
		   }
		 } else if(key=='R') {
		   hit_s=xp_random(145 - user_on->throwing_ability);
		   user_on->rocks--;
		   if(hit_s<35) {
		hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->throwing_ability/110.0)) * (double)((xp_random(80)+90)/100.0);
		ny_line(443,2,0);
		   } else {
		hit_s=0;
		ny_line(444,2,0);
		   }
		 }


		/*if (key == 'T') {  // Take drugs
		  take_drug();
		  no_rip_m=0;
		  key='A';
		}
		if (key=='A') {-/
		  // when atacking
		//      randomize();
		  if(key=='A')
		hit_s = user_on->strength * (what_arm_force(user_on->arm)) * (double)((xp_random(80)+80)/100.0);
		// If he is on drugs
		  if(user_on->drug_high > 0)
		hit_s = (hit_s * what_drug_force_a(user_on->drug,user_on->drug_high));

		//      hit_s*=pow(1.1,user_on->level);

		  hit_s*=(100.0-(user_on->since_got_laid*2))/100.0;
		  if (user_on->drug>=COKE) hit_s-=xp_random(user_on->drug_days_since * pow((double)user_on->drug_addiction,1.2))/60;
		  if (hit_s<0) hit_s=0;

		  def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
		  def_s*=pow(1.2,user_on->level);
		  def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
		  if (user_on->drug>=COKE) def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
		  if (def_s<0) def_s=0;

		  def_s/=2;

		  sprintf(numstr,"u%07d.atk",enm_num);
		  justfile = ShareFileOpen(numstr, "wb");
		  ny_fwrite(&hit_s,4,1,justfile);
		  fclose(justfile);
		  sprintf(numstr,"u%07d.atk",enm_num);
		  sprintf(numstr2,"u%07d.on",enm_num);
		  ny_line(222,2,1);
		  //od_printf("\n\r\n`bright red`W`red`aiting....\n\r");
		  t=time(NULL);
		  t2=t;

		  while ((fexist(numstr)) && (fexist(numstr2))){
		fgc(&enm_num,user_num);
		sprintf(numstr,"u%07d.atk",enm_num);
		sprintf(numstr2,"u%07d.on",enm_num);
		while(t==t2) {
		t=time(NULL);
		od_kernal();
		}
		t2=t;
		od_kernal();
		  }

		  if (!fexist(numstr2)) {

		ny_line(223,2,1);
		if(rip) od_get_answer("\n\r");
		ny_line(224,0,1);
		//od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");

		ny_remove(numstr);
		//sprintf(numstr,"del u%07d.atk",enm_num);
		//system(numstr);
		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		if(!rip) ny_line(1,1,0);
		od_get_answer("\n\r");
		return;
		  }

		  intval=erec.hitpoints;

		  sprintf(numstr,"u%07d.sts",enm_num);
		  justfile=ShareFileOpen(numstr,"rb");
		  ny_fread(&erec,sizeof(user_rec),1,justfile);
		  fclose(justfile);

		  intval -= erec.hitpoints;

		  fgc(&enm_num,user_num);


		  ny_line(208,2,0);
		  //od_printf("\n\r\n`bright red`Y`red`a kick `bright green`");
		  ny_disp_emu(erec.name);
		  ny_line(209,0,0);
		  od_printf(D_Num(intval));
		  ny_line(210,0,0);

		 } else if (key=='G') {
		   intval=xp_random(2);
		   if (intval==0) {

		ny_line(212,2,1);
		//	 od_printf("\n\r\n`bright red`Y`red`a got away ...\n\r");

		sprintf(numstr,"u%07d.atk",enm_num);
		justfile = ShareFileOpen(numstr, "wb");
		intval=-1; //user got away
		ny_fwrite(&intval,4,1,justfile);
		fclose(justfile);
		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		if(!rip) ny_line(1,1,0);
		od_get_answer("\n\r");
		return;
		   } else {

		ny_line(213,2,1);
		//	 od_printf("\n\r\n`bright red`Y`red`e couldn't find a way outta this ...\n\r");

		sprintf(numstr,"u%07d.atk",enm_num);
		justfile = ShareFileOpen(numstr, "wb");
		intval=-2; ///user couldn't get away
		ny_fwrite(&intval,4,1,justfile);
		fclose(justfile);
		sprintf(numstr,"u%07d.atk",enm_num);
		sprintf(numstr2,"u%07d.on",enm_num);

		ny_line(222,2,1);
		//od_printf("\n\r\n`bright red`W`red`aiting....\n\r");

		t=time(NULL);
		t2=t;
		while ((fexist(numstr)) && (fexist(numstr2))){
		fgc(&enm_num,user_num);
		sprintf(numstr,"u%07d.atk",enm_num);
		sprintf(numstr2,"u%07d.on",enm_num);
		while(t==t2) {
		  t=time(NULL);
		  od_kernal();
		}
		t2=t;
		od_kernal();
		}

		if (!fexist(numstr2)) {

		ny_line(223,2,1);
		if(rip) od_get_answer("\n\r");
		ny_line(224,0,1);
		//od_printf("\n\r\n`bright red`Y`red`er enemy droped carrier on ya!\n\r`bright red`G`red`o and kill him offline\n\r");

		//	   sprintf(numstr,"del u%07d.atk",enm_num);
		//	   system(numstr);
		ny_remove(numstr);
		sprintf(numstr,"u%07d.fgg",*user_num);
		ny_remove(numstr);
		if(!rip) ny_line(1,1,0);
		od_get_answer("\n\r");
		return;
		}
		   }
		 }*/// else {
		// DisplayStats();
		//}
	} while (1);
}





void
p_fight_ops(user_rec *cur_user,INT16 *user_num)  //This function operates the fights with players
{

	char key;  // Menu choice

	//user_rec cur_user; // The User Record Variable
	//cur_user *user_on;  // declare allias for the user record

	do {
		//key = p_fight(expert);  //Getting the result of menu choice
		key=callmenu("KYLHTQ?\n\r",P_FIG,354,TRUE);
		while (key=='?') {
			expert+=10;
			key=callmenu("KYLHTQ?\n\r",P_FIG,354,TRUE);
			expert-=10;
		}

		if (key == 'K') {
			p_attack_ops(cur_user,user_num);
		} // End of if look
		else if (key == 'T') {  // Take drugs
			take_drug();
		} // End of take drug
		else if (key == 'H') {
			ny_line(199,2,0);
			//		  od_printf("\n\r\n`bright red`Y`red`a enter the hospital ...\n\r");
			if(rip)
				od_get_answer("\n\r");
			heal_wounds();
		} // End of healing
		else if (key == 'Y') {
			DisplayStats();
			WaitForEnter();
		} // End of viewing the stats
		else if (key=='L') {
			ListPlayersA();
		}

	} while (key != 'Q');   // Do until quit encountered
}   // end of fight_ops function




void p_attack_ops(user_rec *user_on,INT16 *nCurrentUserNumber) {
	INT32 hit_s; //the attacking stregth
	INT32 def_s;
	INT32 en_hit_s;
	INT32 en_def_s;
	//  ffblk ffblk;
	char hand[25];
	char numstr[25];
	INT16 unum,ret;
	FILE *justfile;
	FILE *scr_file;
	//enemy_idx eidx;
	INT32 intval,first,last,moneis;
	scr_rec urec;
	user_rec erec;
	mail_idx_type mail_idx;
	//	enemy erec;
	char key;

	if(rip) {
		od_disp_str("\n\r");
		od_send_file("texti.rip");
	}


	ny_line(233,2,0);
	//  od_printf("\n\r\n`bright red`W`red`ho ya don't like (`bright red`full`red` or `bright red`partial`red` name):`bright green`");

	od_input_str(hand,24,' ',255);
	ny_un_emu(hand);
	od_printf("\n\r");
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
					od_kernal();
				} while ( ((unum++)==user_on->rank ||
				           strzcmp(hand,ny_un_emu(urec.name,numstr)) ||
				           urec.alive!=ALIVE) &&
				          ret==1);
				fclose(justfile);
			}
		} while (ret==1 && askifuser(urec.name)==FALSE);
	}
	if (ret!=1) {

		ny_line(133,1,1);
		//od_printf("\n\r`bright red`G`red`ot no idea who you mean ...");

		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		return;
	} else if (single_node==FALSE && urec.online==TRUE) {
		ch_flag_d();
		sprintf(numstr,"u%07d.fgg",urec.user_num);
		if (fexist(numstr)) {
			justfile=ShareFileOpen(numstr,"rb");
			if(justfile != NULL) {
				ny_fread(&intval,2,1,justfile);
				fclose(justfile);
			}
			sprintf(numstr,"u%07d.on",intval);
			if (fexist(numstr)) {

				ny_line(234,2,1);
				//      od_printf("\n\r\n`bright red`T`red`he user is already being fought!\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				return;
			} else {
				sprintf(numstr,"u%07d.fgg",urec.user_num);
				ny_remove(numstr);
			}
		}
		if(expert==3)
			expert=1;
		online_fight_a(nCurrentUserNumber,user_on,urec.user_num);
		return;
	} else if (hand[0]==0)
		return;

	if (user_on->turns <= 0) {

		ny_line(200,2,1);
		//od_printf("\n\r\n`bright red`Y`red`a ain't got no fights left ... come back tomorrow\n\r");

		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		return;
	}

	if(expert==3)
		expert=1;

	if(single_node==FALSE) {
		ch_flag_d();
		sprintf(numstr,"u%07d.bfa",urec.user_num);
		if (fexist(numstr)) {
			justfile=ShareFileOpen(numstr,"rb");
			if(justfile != NULL) {
				ny_fread(&intval,2,1,justfile);
				fclose(justfile);
			}
			sprintf(numstr,"u%07d.on",intval);
			if (fexist(numstr)) {

				ny_line(234,2,1);
				//      od_printf("\n\r\n`bright red`T`red`he user is already being fought!\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				return;
			} else {
				sprintf(numstr,"u%07d.bfa",urec.user_num);
				ny_remove(numstr);
			}
		}

		ch_game_d();
		sprintf(numstr,"u%07d.inf",urec.user_num);
		if (fexist(numstr)) {

			ny_line(235,2,1);
			//    od_printf("\n\r\n`bright red`T`red`he player is somewhere where you can' find him!\n\r");
			//    od_printf(" `red`(Other stuff)\n\r");

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			return;
		}
		ch_flag_d();

		sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
		justfile = ShareFileOpen(numstr, "wb");
		if(justfile != NULL) {
			ny_fwrite(&urec.user_num,2,1,justfile);
			fclose(justfile);
		}

		sprintf(numstr,"u%07d.bfa",urec.user_num);
		justfile = ShareFileOpen(numstr, "wb");
		if(justfile != NULL) {
			ny_fwrite(nCurrentUserNumber,2,1,justfile);
			fclose(justfile);
		}
	}

	//readin the enemy record
	ch_game_d();
	justfile=ShareFileOpen(USER_FILENAME,"rb");
	if(justfile != NULL) {
		fseek(justfile,(INT32)urec.user_num * sizeof(user_rec),SEEK_SET);
		ny_fread(&erec,sizeof(user_rec),1,justfile);
		fclose(justfile);
	}

	// WaitForEnter();

	if (erec.rest_where==MOTEL) {
		intval=100*pow(1.4,(double)user_on->level+1);

		ny_line(236,2,0);
		//    od_printf("\n\r\n`bright red`I`red`t will cost ya `bright green`
		od_printf(D_Num(intval));
		ny_line(237,0,0);
		//    `red` to get keys to his room ... Do it?(`bright red`Y`red`/`bright red`N`red`)",intval);

		key=ny_get_answer("YN");
		if(!rip)
			od_printf("%c\n\r",key);
		else
			od_disp_str("\n\r");

		if (key=='N') {
			if(single_node==FALSE) {
				ch_flag_d();
				sprintf(numstr,"u%07d.bfa",urec.user_num);
				ny_remove(numstr);
				sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
				//	if (fexist(numstr)) {
				ny_remove(numstr);
				//	  sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
				//	  system(numstr);
				//	}
			}
			return;
		}

		if (user_on->money < intval) {

			ny_line(238,2,1);
			//      od_printf("\n\r\n`bright white`Not nuff money man ...\n\r");

			if(single_node==FALSE) {
				ch_flag_d();
				sprintf(numstr,"u%07d.bfa",urec.user_num);
				ny_remove(numstr);
				sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
				//	if (fexist(numstr)) {
				ny_remove(numstr);
				//	  sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
				//	  system(numstr);
				//	}
			}
			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			return;
		}
		money_minus(intval);
	} else if (erec.rest_where==REG_HOTEL) {
		intval=300*pow(1.4,(double)user_on->level+1);

		ny_line(236,2,0);
		od_printf(D_Num(intval));
		ny_line(237,0,0);
		//od_printf("\n\r\n`bright red`I`red`t will cost ya `bright green`%d`red` to get keys to his room ... Do it?(`bright red`Y`red`/`bright red`N`red`)",intval);


		key=ny_get_answer("YN");
		if(!rip)
			od_printf("%c\n\r",key);
		else
			od_disp_str("\n\r");

		if (key=='N') {
			if(single_node==FALSE) {
				ch_flag_d();
				sprintf(numstr,"u%07d.bfa",urec.user_num);
				ny_remove(numstr);
				sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
				//	if (fexist(numstr)) {
				ny_remove(numstr);
				//	  sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
				//	  system(numstr);
				//	}
			}
			return;
		}

		if (user_on->money < intval) {

			ny_line(238,2,1);
			//od_printf("\n\r\n`bright white`Not nuff money man ...\n\r");

			if(single_node==FALSE) {
				ch_flag_d();
				sprintf(numstr,"u%07d.bfa",urec.user_num);
				ny_remove(numstr);
				sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
				//	if (fexist(numstr)) {
				ny_remove(numstr);
				//	  sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
				//	  system(numstr);
				//	}
			}
			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			return;
		}
		money_minus(intval);
	} else if (erec.rest_where==EXP_HOTEL) {
		intval=700*pow(1.4,(double)user_on->level+1);

		ny_line(236,2,0);
		od_printf(D_Num(intval));
		ny_line(237,0,0);

		//od_printf("\n\r\n`bright red`I`red`t will cost ya `bright green`%d`red` to get keys to his room ... Do it?(`bright red`Y`red`/`bright red`N`red`)",intval);

		key=ny_get_answer("YN");
		if(!rip)
			od_printf("%c\n\r",key);
		else
			od_disp_str("\n\r");

		if (key=='N') {
			if(single_node==FALSE) {
				ch_flag_d();
				sprintf(numstr,"u%07d.bfa",urec.user_num);
				ny_remove(numstr);
				sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
				//	if (fexist(numstr)) {
				ny_remove(numstr);
				//	  sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
				//	  system(numstr);
				//	}
			}
			return;
		}

		if (user_on->money < intval) {

			//od_printf("\n\r\n`bright white`Not nuff money man ...\n\r");

			if(single_node==FALSE) {
				ch_flag_d();
				sprintf(numstr,"u%07d.bfa",urec.user_num);
				ny_remove(numstr);
				sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
				//	if (fexist(numstr)) {
				ny_remove(numstr);
				//	  sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
				//	  system(numstr);
				//	}
			}
			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			return;
		}
		money_minus(intval);
	}
	user_on->turns--;

	//  WaitForEnter();
	wrt_sts();
	//  WaitForEnter();

	od_printf("\n\r\n");
	ny_clr_scr();


	ny_line(201,0,0);
	//	  od_printf("`bright red`Y`red`ou meet `bright green`");
	ny_disp_emu(erec.name);
	ny_line(202,0,2);
	//	  od_printf(" `red`...\n\r\n");
	ny_line(203,0,0);
	//	  od_printf("`bright red`H`red`e got a bad lokin' `bright green`");
	print_arm(erec.arm);
	ny_line(239,2,0);
	//	  od_printf("\n\r\n`bright red`S`red`till wanna do it?(`bright red`Y`red`/`bright red`N`red`)");

	key=ny_get_answer("YN");
	od_printf("%c\n\r",key);

	if (key=='N') {
		if(single_node==FALSE) {
			ch_flag_d();
			sprintf(numstr,"u%07d.bfa",urec.user_num);
			ny_remove(numstr);
			sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
			//	      if (fexist(numstr)) {
			ny_remove(numstr);
			//		sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
			//		system(numstr);
			//	      }
		}
		return;
	}


	do {

bam_pf:
		ny_line(240,3,0);
		//	    od_printf("\n\r\n\n\r`bright red`P`red`layer `bright red`F`red`ight");
		ny_disp_emu("\n\r\n`0");
		ny_disp_emu(erec.name);
		ny_line(205,0,0);
		od_printf(D_Num(erec.hitpoints));
		ny_line(206,1,0);
		od_printf("%s\n\r\n",D_Num(user_on->hitpoints));
		/*	    od_printf("\n\r\n`bright red`[A] `red`- `bright red`A`red`ttack\n\r");
			    od_printf("`bright red`[T] `red`- `bright red`T`red`ake `bright red`D`red`rug `bright red`A`red`nd `bright red`A`red`ttack\n\r");
			    od_printf("`bright red`[G] `red`- `bright red`G`red`et `bright red`O`red`utta `bright red`H`red`ere\n\r");
			    od_printf("`bright red`[Y] `red`- `bright red`Y`red`er `bright red`S`red`tats\n\r");
			    od_printf("\n\r`bright red`W`red`hat ya gonna do? (`bright red`[A] G Y`red`)");*/
		ny_send_menu(ATTACK,"");
		ny_line(207,1,0);
		//	    ny_disp_emu("\n\r`@W`4hat ya gonna do? (`@[A] T G Y`4)");


		key=od_get_answer("ATKPRGY\n\r");
		if (key=='\n' || key=='\r')
			key='A';

		od_putch(key);

		if(key=='R' && user_on->rocks==0) {
			ny_line(445,2,1);
			WaitForEnter();
			goto bam_pf;
		}


		if (key == 'T') {  // Take drugs
			take_drug();
			no_rip_m=0;
			key='A';
		}
		if (key=='A' || key=='K' || key=='P' || key=='R') {
			// when atacking
			//	      randomize();
			if(key=='K') {
				hit_s=xp_random(150 - user_on->kick_ability);
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->kick_ability/100.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(439,2,0);
				} else {
					hit_s=0;
					ny_line(440,2,0);
				}
			} else if(key=='P') {
				hit_s=xp_random(140 - user_on->punch_ability);
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->punch_ability/90.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(441,2,0);
				} else {
					hit_s=0;
					ny_line(442,2,0);
				}
			} else if(key=='R') {
				hit_s=xp_random(145 - user_on->throwing_ability);
				user_on->rocks--;
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->throwing_ability/110.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(443,2,0);
				} else {
					hit_s=0;
					ny_line(444,2,0);
				}
			}

			/*if (key == 'T') {  // Take drugs
			  take_drug();
			  no_rip_m=0;
			  key='A';
			}
			if (key=='A') {*/
			// when atacking
			//	      randomize();
attack_again_p:
			if(key=='A')
				hit_s = user_on->strength * (what_arm_force(user_on->arm)) * (double)((xp_random(80)+80)/100.0);
			// If he is on drugs
			if(user_on->drug_high > 0)
				hit_s = (hit_s * what_drug_force_a(user_on->drug,user_on->drug_high));

			//	    hit_s*=pow(1.1,user_on->level);

			hit_s*=(100.0-(user_on->since_got_laid*2))/100.0;
			if (user_on->drug>=COKE)
				hit_s-=xp_random(user_on->drug_days_since * pow((double)user_on->drug_addiction,1.2))/60;
			if (hit_s<0)
				hit_s=0;

			en_hit_s = erec.strength * (what_arm_force(erec.arm)) * (double)((xp_random(70)+60)/100.0);
			//	    en_hit_s*=pow(1.1,erec.level);

			en_hit_s*=(100.0-(erec.since_got_laid*2))/100.0;
			if (erec.drug>=COKE)
				en_hit_s-=xp_random(erec.drug_days_since * pow((double)erec.drug_addiction,1.2))/60;
			if (en_hit_s<0)
				en_hit_s=0;

			def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
			def_s*=pow(1.2,user_on->level);
			def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
			if (user_on->drug>=COKE)
				def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
			if (def_s<0)
				def_s=0;

			en_def_s = erec.defense * (double)((xp_random(80)+120)/100.0);
			en_def_s*=pow(1.2,erec.level);
			en_def_s*=(100.0-(erec.since_got_laid*2))/100.0;
			if (erec.drug>=COKE)
				en_def_s-=xp_random(erec.drug_days_since * pow(erec.drug_addiction,1.2))/60;
			if (en_def_s<0)
				en_def_s=0;

			def_s/=2;
			en_def_s/=2;

			en_hit_s-=def_s;
			if (en_hit_s<0)
				en_hit_s=0;

			hit_s-=en_def_s;
			if (hit_s<0)
				hit_s=0;

			if (hit_s==0 && en_hit_s==0 && key=='A')
				goto attack_again_p;

			erec.hitpoints-=hit_s;


			ny_line(208,2,0);
			//	    od_printf("\n\r\n`bright red`Y`red`a kick `bright green`");
			ny_disp_emu(erec.name);
			ny_line(209,0,0);
			od_printf(D_Num(hit_s));
			ny_line(210,0,0);


			if (erec.hitpoints>0) {

				ny_disp_emu("\n\r\n`0");
				ny_disp_emu(erec.name);
				ny_line(211,0,0);
				od_printf(D_Num(en_hit_s));
				ny_line(210,0,0);


				user_on->hitpoints-=en_hit_s;
				wrt_sts();
				if (user_on->hitpoints<=0) {

					od_printf("\n\r\n");
					ny_send_menu(ASS_KICKED_P,"");
					//		od_printf("\n\r\n`bright white`You had yer ass kicked ... oh well that happens\n\rCome back tomorrow to get revenge ...\n\rYa lost 8%c of yer points\n\r",37);

					news_post(user_on->say_loose,user_on->name,erec.name,5);
					user_on->alive=UNCONCIOUS;
					user_on->money=0;
					points_loose(user_on->points*.08);
					//wrt_sts();
					if(single_node==FALSE) {
						ch_flag_d();
						sprintf(numstr,"u%07d.bfa",urec.user_num);
						ny_remove(numstr);
						sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
						//		  if (fexist(numstr)) {
						ny_remove(numstr);
						//		    sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
						//		    system(numstr);
						//		  }
					}
					WaitForEnter();
					od_exit(10,FALSE);
				}
			}
		} else if (key=='G') {
			intval=xp_random(2);
			if (intval==0) {

				ny_line(212,2,1);
				//od_printf("\n\r\n`bright red`Y`red`a got away ...\n\r");

				if(single_node==FALSE) {
					ch_flag_d();
					sprintf(numstr,"u%07d.bfa",urec.user_num);
					ny_remove(numstr);
					sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
					//		if (fexist(numstr)) {
					ny_remove(numstr);
					//		  sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
					//		  system(numstr);
					//}
				}
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				return;
			} else {

				ny_line(213,2,1);
				//	      od_printf("\n\r\n`bright red`Y`red`e couldn't find a way outta this ...\n\r");

				//	      randomize();
				en_hit_s = erec.strength * (what_arm_force(erec.arm)) * (double)((xp_random(80)+80)/100.0);
				//	      en_hit_s*=pow(1.1,erec.level);

				en_hit_s*=(100.0-(erec.since_got_laid*2))/100.0;
				if (erec.drug>=COKE)
					en_hit_s-=xp_random(erec.drug_days_since * pow((double)erec.drug_addiction,1.2))/60;
				if (en_hit_s<0)
					en_hit_s=0;

				def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
				def_s*=pow(1.2,user_on->level);
				def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
				if (user_on->drug>=COKE)
					def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
				if (def_s<0)
					def_s=0;

				def_s/=2;
				en_hit_s-=def_s;
				if (en_hit_s<0)
					en_hit_s=0;

				ny_disp_emu("\n\r\n`0");
				ny_disp_emu(erec.name);
				ny_line(211,0,0);
				od_printf(D_Num(en_hit_s));
				ny_line(210,0,0);


				user_on->hitpoints-=en_hit_s;
				wrt_sts();
				if (user_on->hitpoints<=0) {

					od_printf("\n\r\n");
					ny_send_menu(ASS_KICKED_P,"");

					//od_printf("\n\r\n`bright white`You had yer ass kicked ... oh well that happens\n\rCome back tomorrow to get revenge ...\n\rYa lost 8%c of yer points\n\r",37);

					news_post(user_on->say_loose,user_on->name,erec.name,5);
					user_on->alive=UNCONCIOUS;
					user_on->money=0;
					points_loose(user_on->points*.08);
					//wrt_sts();
					if(single_node==FALSE) {
						ch_flag_d();
						sprintf(numstr,"u%07d.bfa",urec.user_num);
						ny_remove(numstr);
						sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
						//		  if (fexist(numstr)) {
						ny_remove(numstr);
						//		    sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
						//		    system(numstr);
						//}
					}
					WaitForEnter();
					od_exit(10,FALSE);
				}
			}
		} else {
			DisplayStats();
			WaitForEnter();
		}
	} while (erec.hitpoints>0);


	ny_line(214,2,0);
	//od_printf("\n\r\n`bright red`Y`red`ou kicked `bright green`");
	ny_disp_emu(erec.name);
	ny_line(215,0,2);
	//od_printf("'s `red`ass ...\n\r\n");
	money_plus(erec.money);
	ny_line(216,0,0);
	od_printf(D_Num(erec.money));
	intval=.08*erec.points;
	ny_line(217,0,0);
	od_printf(D_Num((DWORD)intval * (DWORD)DrgPtsCoef()));
	ny_line(218,0,1);

	points_raise(intval);




	erec.money=0;

	erec.alive=UNCONCIOUS;
	urec.alive=UNCONCIOUS;

	if (erec.arm>user_on->arm) {

		ny_line(228,2,0);
		//	  od_printf("\n\r\n`bright red`D`red`o ya wanna swap weapons with`bright green` ");
		if(!rip)
			ny_disp_emu(erec.name);
		else
			od_disp_str(ny_un_emu(erec.name,numstr));
		ny_line(79,0,0);
		//	  ny_disp_emu("`4? (`@Y`4/`@N`4)");

		key=od_get_answer("YN");
		if(!rip)
			od_printf("%c\n\r",key);
		else
			od_disp_str("\n\r");
		if (key=='Y') {

			weapon tarm;

			tarm=erec.arm;
			erec.arm=user_on->arm;
			user_on->arm=tarm;

			ny_line(229,1,0);
			//	    od_printf("\n\r`bright red`D`red`one! `bright red`Y`red`ou now got the `bright green`");

			print_arm(user_on->arm);
			if(rip) {
				od_disp_str("::^M@OK))|#|#|#\n\r\n");
				od_get_answer("\n\r");
			}
			od_printf("\n\r\n");
		}
	}




	/*	strcpy(urec.name,user_on.name);
		rec.nation=cur_user.nation;
		rec.level=cur_user.level;
		rec.points=cur_user.points;
		rec.alive=cur_user.alive;
		rec.sex=cur_user.sex;
		rec.user_num=nCurrentUserNumber;
		rec.online=TRUE;*/

	if(single_node==FALSE) {
		ch_flag_d();
		sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);

		if (fexist(numstr)) {
			justfile=ShareFileOpen(numstr,"rb");
			if(justfile != NULL) {
				ny_fread(&urec.user_num,2,1,justfile);
				fclose(justfile);
			}

			ny_remove(numstr);

			//	    sprintf(numstr,"del u%07d.bfo",*nCurrentUserNumber);
			//	    system(numstr);
		}
	}

	erec.points*=.92;
	urec.points*=.92;

	SortScrFileB(urec.user_num);

	user_rec serec;

	ch_game_d();

	justfile=ShareFileOpen(USER_FILENAME,"rb");
	if(justfile != NULL) {
		fseek(justfile,(INT32)urec.user_num*sizeof(user_rec),SEEK_SET);
		ny_fread(&serec,sizeof(user_rec),1,justfile);
		fclose(justfile);
	}


	scr_file=ShareFileOpen(SCR_FILENAME,"r+b");
	if(justfile != NULL) {
		fseek(scr_file, (INT32)serec.rank * sizeof(scr_rec), SEEK_SET);
		ny_fwrite(&urec, sizeof(scr_rec), 1, scr_file);
		fclose(scr_file);
	}

	erec.rank=serec.rank;



	justfile=ShareFileOpen(USER_FILENAME,"r+b");
	if(justfile != NULL) {
		fseek(justfile,(INT32)urec.user_num*sizeof(user_rec),SEEK_SET);
		ny_fwrite(&erec,sizeof(user_rec),1,justfile);
		fclose(justfile);
	}


	//
	//	od_printf("\n\r\n`bright red`Y`red`ou kicked `bright green`%s's `red`ass ...\n\r",erec.name);
	//

	strcpy(mail_idx.recver,erec.name);
	strcpy(mail_idx.sender,user_on->name);
	strcpy(mail_idx.recverI,erec.bbsname);
	strcpy(mail_idx.senderI,user_on->bbsname);
	mail_idx.flirt=1001;
	mail_idx.deleted=FALSE;
	mail_idx.location=0;
	mail_idx.length=0;
	mail_idx.afterquote=0;
	mail_idx.ill=user_on->std;
	mail_idx.inf=user_on->std_percent;
	mail_idx.sender_sex=user_on->sex;
	ch_game_d();

	justfile=ShareFileOpen(MAIL_INDEX,"a+b");
	if(justfile != NULL) {
		ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
		fclose(justfile);
	}



	if (erec.sex!=user_on->sex && clean_mode==FALSE && user_on->sex_today>0) {

		ny_line(230,2,0);
		//	  od_printf("\n\r\n`bright red`D`red`o ya wanna rape `bright green`");
		ny_disp_emu(erec.name);
		ny_line(79,0,0);
		//	  ny_disp_emu("`4? (`@Y`4/`@N`4)");

		key=od_get_answer("YN");
		if(!rip)
			od_printf("%c\n\r",key);
		else
			od_disp_str("\n\r");
		if (key=='Y') {
			if (user_on->sex_today<=0) {

				ny_line(118,2,1);
				//	      od_printf("\n\r\n`bright white`You already used up all your sex turns today ...\n\r");

			} else {
				strcpy(mail_idx.recver,erec.name);
				strcpy(mail_idx.sender,user_on->name);
				strcpy(mail_idx.recverI,erec.bbsname);
				strcpy(mail_idx.senderI,user_on->bbsname);
				mail_idx.flirt=1000;
				mail_idx.deleted=FALSE;
				mail_idx.location=0;
				mail_idx.length=0;
				mail_idx.afterquote=0;
				mail_idx.ill=user_on->std;
				mail_idx.inf=user_on->std_percent;
				mail_idx.sender_sex=user_on->sex;
				ch_game_d();
				justfile=ShareFileOpen(MAIL_INDEX,"a+b");
				if(justfile != NULL) {
					ny_fwrite(&mail_idx,sizeof(mail_idx_type),1,justfile);
					fclose(justfile);
				}
				user_on->sex_today--;
				user_on->since_got_laid=0;
				ny_line(339,1,0);
				if(rip)
					ny_disp_emu(erec.name);
				else
					od_disp_str(ny_un_emu(erec.name,numstr));
				ny_line(340,0,1);
				if(rip)
					od_get_answer("\n\r");

				illness(erec.std,erec.std_percent);
				points_raise((INT32)35*(user_on->level+1));

			}
		}
	}

	news_post(user_on->say_win,user_on->name,erec.name,4);
	if(single_node==FALSE) {
		ch_flag_d();

		sprintf(numstr,"u%07d.bfa",urec.user_num);
		ny_remove(numstr);


		sprintf(numstr,"u%07d.bfo",*nCurrentUserNumber);
		ny_remove(numstr);
	}

	if(!rip)
		WaitForEnter();
} // end of function p_attack_ops

void evil_ops(user_rec *cur_user) {
	char key;
	INT32 chance,intval;

	do {
		key=callmenu("SBPDRWQ?\n\r",EVIL_STUFF,355,FALSE);
		while (key=='?') {
			expert+=10;
			key=callmenu("SBPDRWQ?\n\r",EVIL_STUFF,355,FALSE);
			expert-=10;
		}

		if (key=='S') {  //steel money from beggars
			if(rip)
				no_rip_m=1;

			ny_line(241,2,1);
			//      od_printf("\n\r\n`bright red`Y`red`ou find some blind beggar and try to take some of his money...\n\r");

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			chance=xp_random(100);
			if (chance<=busted_ch_beggar) {

				od_printf("\n\r\n");
				ny_send_menu(BUSTED,"");
				//od_printf("\n\r\n`bright white`BUSTED!!!! .... well the police beat the hell out of you .....");
				//od_printf("\n\rWhy don't you try again tomorrow...\n\r");
				//od_printf("Ya lost 2%c of yer points!\n\r",37);

				cur_user->alive=UNCONCIOUS;
				points_loose(cur_user->points*.02);
				news_post("stealing from beggars",cur_user->name,"",2);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				od_exit(10,FALSE);
			} else {
				chance=xp_random(100);
				if (chance<=success_ch_beggar) {
					intval=xp_random((cur_user->level+1)*30);

					ny_line(242,2,0);
					//	  od_printf("\n\r\n`bright red`Y`red`ou just stole
					od_printf(D_Num(intval));
					ny_line(243,0,1);
					//	  from the poor bastard, good job...\n\r",intval);

					money_plus(intval);
					points_raise((INT32)20*cur_user->level+15);
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				} else {

					ny_line(244,2,1);
					//od_printf("\n\r\n`bright red`T`red`he poor bastard was broke...\n\r");

					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				}
			}
		}
		if (key=='D') {  //destroy a car
			if(rip)
				no_rip_m=1;

			ny_line(184,2,1);
			//od_printf("\n\r\n`bright red`Y`red`ou find a nice car...\n\r");

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			chance=xp_random(100);
			if (chance<=busted_ch_car) {

				od_printf("\n\r\n");
				ny_send_menu(BUSTED,"");
				//od_printf("\n\r\n`bright white`BUSTED!!!! .... well the police beat the hell out of you .....");
				//od_printf("\n\rWhy don't you try again tomorrow...\n\r");
				//od_printf("Ya lost 2%c of yer points!\n\r",37);

				cur_user->alive=UNCONCIOUS;
				points_loose(cur_user->points*.02);
				news_post("busting up a car",cur_user->name,"",2);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				od_exit(10,FALSE);
			} else {
				chance=xp_random(100);
				if(chance<=success_ch_car) {

					ny_line(192,2,1);
					//	  od_printf("\n\r\n`bright red`Y`red`ou totally smashed that car .... a very nice job...\n\r");
					if(rip)
						od_get_answer("\n\r");
					ny_line(117,0,0);
					od_printf("%s",D_Num((INT32)(25*cur_user->level+25) * (INT32)DrgPtsCoef()));

					points_raise((INT32)25*cur_user->level+25);
					if(!rip) {
						WaitForEnter();
						od_disp_str("\n\r");
					} else {
						od_disp_str("::^M@OK))|#|#|#\n\r");
						od_get_answer("\n\r");
					}
				} else {

					ny_line(406,2,1);

					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				}
			}
		}
		if (key=='B') {  //burn a school down
			if(rip)
				no_rip_m=1;

			ny_line(245,2,1);
			//      od_printf("\n\r\n`bright red`Y`red`ou find a school...that's the easy part...\n\r");

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			chance=xp_random(100);
			if (chance<=busted_ch_school) {

				od_printf("\n\r\n");
				ny_send_menu(BUSTED,"");
				//od_printf("\n\r\n`bright white`BUSTED!!!! .... well the police beat the hell out of you .....");
				//od_printf("\n\rWhy don't you try again tomorrow...\n\r");
				//od_printf("Ya lost 2%c of yer points!\n\r",37);

				cur_user->alive=UNCONCIOUS;
				points_loose(cur_user->points*.02);
				news_post("burning down a school",cur_user->name,"",2);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				od_exit(10,FALSE);
			} else {
				chance=xp_random(100);
				if (chance<=success_ch_school) {

					ny_line(246,2,1);
					if(rip)
						od_get_answer("\n\r");
					//	  od_printf("\n\r\n`bright red`I`red`t looks lovely as the school buildings crumble into ashes ...\n\r");
					ny_line(117,0,0);
					od_printf("%s",D_Num((INT32)(50*cur_user->level+40) * (INT32)DrgPtsCoef()));

					points_raise((INT32)50*cur_user->level+40);
					news_post("`@A`4 school burned to ashes ... `%Police `4say clear case of arsen","","",0);
					if(!rip) {
						od_disp_str("\n\r");
						WaitForEnter();
					} else {
						od_disp_str("::^M@OK))|#|#|#\n\r");
						od_get_answer("\n\r");
					}
				} else {

					ny_line(247,2,1);
					//od_printf("\n\r\n`bright red`S`red`chools don't burn very well... all matches are gone...\n\r");

					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				}
			}
		}
		if (key=='R') {  //break a window
			if(rip)
				no_rip_m=1;

			ny_line(248,2,1);
			//od_printf("\n\r\n`bright red`Y`red`ou pick up a rock and...\n\r");

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			chance=xp_random(100);
			if (chance<=busted_ch_window) {

				od_printf("\n\r\n");
				ny_send_menu(BUSTED,"");
				//od_printf("\n\r\n`bright white`BUSTED!!!! .... well the police beat the hell out of you .....");
				//od_printf("\n\rWhy don't you try again tomorrow...\n\r");
				//od_printf("Ya lost 2%c of yer points!\n\r",37);

				cur_user->alive=UNCONCIOUS;
				points_loose(cur_user->points*.02);
				news_post("breaking windows",cur_user->name,"",2);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				od_exit(10,FALSE);
			} else {
				chance=xp_random(100);
				if(chance<=success_ch_window) {

					ny_line(249,2,1);
					if(rip)
						od_get_answer("\n\r");
					//	  od_printf("\n\r\n`bright red`B`red`reaking glass sounds so good...\n\r");
					ny_line(117,0,0);
					od_printf("%s",D_Num((INT32)(8*cur_user->level+8) * (INT32)DrgPtsCoef()));

					points_raise((INT32)8 * cur_user->level + 8);
					if(!rip) {
						od_disp_str("\n\r");
						WaitForEnter();
					} else {
						od_disp_str("::^M@OK))|#|#|#\n\r");
						od_get_answer("\n\r");
					}
				} else {

					ny_line(407,2,1);

					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				}
			}
		}
		if (key=='P') {  //poison
			if(rip)
				no_rip_m=1;
			intval=pow(1.6,cur_user->level) * 500;

			ny_line(250,2,0);
			od_printf(D_Num(intval));
			//      od_printf("\n\r\n`bright red`P`red`oisoning water will cost ya `bright red`%s`red` ... Do it(`bright red`Y`red`/`bright red`N`red`)",D_Num(intval));
			ny_line(251,0,0);

			key=ny_get_answer("YN");
			if(!rip)
				od_putch(key);
			if (key=='Y' && cur_user->poison<=0) {
				ny_line(182,2,1);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else if (key=='Y') {
				if (cur_user->money < intval) {

					ny_line(252,2,1);
					//	  od_printf("\n\r\n`bright red`Y`red`a cant afford it man...\n\r");

					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				} else {
					money_minus(intval);

					ny_line(253,2,1);
					//od_printf("\n\r\n`bright red`Y`red`ou buy enough poison and dump it into the water...\n\r");

					cur_user->poison--;
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					chance=xp_random(100);
					if (chance<=busted_ch_poison) {

						od_printf("\n\r\n");
						ny_send_menu(BUSTED,"");
						//od_printf("\n\r\n`bright white`BUSTED!!!! .... well the police beat the hell out of you .....");
						//od_printf("\n\rWhy don't you try again tomorrow...\n\r");
						//od_printf("Ya lost 2%c of yer points!\n\r",37);

						cur_user->alive=UNCONCIOUS;
						points_loose(cur_user->points*.02);
						news_post("poisoning water",cur_user->name,"",2);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
						od_exit(10,FALSE);
					} else {
						chance=xp_random(100);
						if (chance<=success_ch_poison) {

							ny_line(254,2,1);
							if(rip)
								od_get_answer("\n\r");
							//od_printf("\n\r\n`bright red`W`red`ow lotsa dead folks ... It worked ...\n\r");
							ny_line(117,0,0);
							od_printf("%s",D_Num((INT32)(80*cur_user->level+60) * (INT32)DrgPtsCoef()));

							points_raise((INT32)80*cur_user->level+60);
							news_post("`@W`4ater was poisoned today ... `%nobody was caught","","",0);
							if(!rip) {
								od_disp_str("\n\r");
								WaitForEnter();
							} else {
								od_disp_str("::^M@OK))|#|#|#\n\r");
								od_get_answer("\n\r");
							}
						} else {

							ny_line(255,2,1);
							//od_printf("\n\r\n`bright red`T`red`he poison didn't work ... The guy ripped you off ...\n\r");

							if(!rip)
								WaitForEnter();
							else
								od_get_answer("\n\r");
						}
					}
				}
			}
		}
		if (key=='U') {  //UNHQ bombing
			if(rip)
				no_rip_m=1;
			intval=pow(1.6,cur_user->level) * 1000;

			ny_line(256,2,0);
			od_printf(D_Num(intval));
			ny_line(251,0,0);
			//      od_printf("\n\r\n`bright red`E`red`xplosives will cost ya `bright red`%s`red` ... Do it(`bright red`Y`red`/`bright red`N`red`)",D_Num(intval));

			key=ny_get_answer("YN");
			if(!rip)
				od_putch(key);
			if (key=='Y' && cur_user->unhq<=0) {
				ny_line(181,2,1);
				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
			} else if (key=='Y') {
				if (cur_user->money < intval) {

					ny_line(252,2,1);
					//od_printf("\n\r\n`bright red`Y`red`a cant afford it man...\n\r");

					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
				} else {
					money_minus(intval);

					ny_line(257,2,1);
					//	  od_printf("\n\r\n`bright red`Y`red`ou buy nuff explosives and ...\n\r");

					cur_user->unhq--;
					if(!rip)
						WaitForEnter();
					else
						od_get_answer("\n\r");
					chance=xp_random(100);
					if (chance<=busted_ch_bomb) {

						od_printf("\n\r\n");
						ny_send_menu(BUSTED,"");
						//od_printf("\n\r\n`bright white`BUSTED!!!! .... well the police beat the hell out of you .....");
						//od_printf("\n\rWhy don't you try again tomorrow...\n\r");
						//od_printf("Ya lost 2%c of yer points!\n\r",37);

						cur_user->alive=UNCONCIOUS;
						points_loose(cur_user->points*.02);
						news_post("United Nations HQ bombing",cur_user->name,"",2);
						if(!rip)
							WaitForEnter();
						else
							od_get_answer("\n\r");
						od_exit(10,FALSE);
					} else {
						chance=xp_random(100);
						if (chance<=success_ch_bomb) {

							ny_line(258,2,1);
							if(rip)
								od_get_answer("\n\r");
							//	      od_printf("\n\r\n`bright red`W`red`ow ... it didn't fall but almost ...\n\r");
							ny_line(117,0,0);
							od_printf("%s",D_Num((INT32)(120*cur_user->level+100) * (INT32)DrgPtsCoef()));

							points_raise((INT32)120*cur_user->level+100);
							news_post("`%United Nations HQ`4 was `%bombed `4today ... 10 suspects brought in","","",0);
							if(!rip) {
								od_disp_str("\n\r");
								WaitForEnter();
							} else {
								od_disp_str("::^M@OK))|#|#|#\n\r");
								od_get_answer("\n\r");
							}
						} else {

							ny_line(259,2,1);
							//od_printf("\n\r\n`bright red`T`red`he police found yer explosives ... Bad Luck ...\n\r");
							news_post("`@E`4xplosives found in the `%United Nations HQ `4today ...","","",0);

							if(!rip)
								WaitForEnter();
							else
								od_get_answer("\n\r");
						}
					}
				}
			}
		}
	} while (key!='Q');
} // end of function evil ops


void
copfight_ops(user_rec *cur_user)  //This function operates the fights with monsters
{

	char key;  // Menu choice

	do {

		key=callmenu("LYHTQ?\n\r",C_FIG,356,TRUE);
		//		key = copfight(expert);  //Getting the result of menu choice
		while (key=='?') {
			expert+=10;
			key=callmenu("LYHTQ?\n\r",C_FIG,356,TRUE);
			expert-=10;
		}

		if (key == 'L') {
			copattack_ops(cur_user);
		} // End of if look
		else if (key == 'T') {  // Take drugs
			take_drug();
		} // End of take drug
		else if (key == 'H') {
			ny_line(199,2,0);
			//Ya enter the hospital ...
			if(rip)
				od_get_answer("\n\r");
			heal_wounds();
		} // End of healing
		else if (key == 'Y') {
			DisplayStats();
			WaitForEnter();
		} // End of viewing the stats

	} while (key != 'Q');   // Do until quit encountered
}   // end of fight_ops function



void
print_cop(INT16 num) {
	od_set_attrib(0x0a);
	if (num==0)
		od_printf("Museum Nightguard");
	else if (num==1)
		od_printf("School Security Guy");
	else if (num==2)
		od_printf("Old Sergant");
	else if (num==3)
		od_printf("Regular Street Cop");
	else if (num==4)
		od_printf("Motocycle Cop");
	else if (num==5)
		od_printf("Detective");
	else if (num==6)
		od_printf("Gang Unit Member");
	else if (num==7)
		od_printf("Special Assigment Member");
	else if (num==8)
		od_printf("Anti Terrorist Unit Guy");
	else if (num==9)
		od_printf("SuperCop");
}



char
cop_list(void) {
	char key;

	if(!rip) {
		od_printf("\n\r\n");

		ny_clr_scr();


		ny_send_menu(COPS,"");
	}

	/*  od_printf("`bright red`W`red`hich one ya wanna attack:\n\r\n");
	  od_printf("`bright white`0`red`. `bright green`Museum Nightguard\n\r");
	  od_printf("`bright white`1`red`. `bright green`School Security Guy\n\r");
	  od_printf("`bright white`2`red`. `bright green`Old Sergant\n\r");
	  od_printf("`bright white`3`red`. `bright green`Regular Street Cop\n\r");
	  od_printf("`bright white`4`red`. `bright green`Motocycle Cop\n\r");
	  od_printf("`bright white`5`red`. `bright green`Detective\n\r");
	  od_printf("`bright white`6`red`. `bright green`Gang Unit Member\n\r");
	  od_printf("`bright white`7`red`. `bright green`Special Assigment Member\n\r");
	  od_printf("`bright white`8`red`. `bright green`Anti Terrorist Unit Guy\n\r");
	  od_printf("`bright white`9`red`. `bright green`SuperCop\n\r\n");*/
	ny_line(260,0,0);
	//ny_disp_emu("`@S`4ooo... (`@0`4-`@9 [Q]`4=quit)");

	key=ny_get_answer("0123456789Q\n\r");
	if (key=='\n' || key=='\r')
		key='Q';

	if(!rip)
		od_putch(key);

	return key;
}

void copattack_ops(user_rec *user_on) {
	char key;
	INT16 num;
	INT32 intval,hitpoints,strength,defense;
	INT32 hit_s,en_hit_s,def_s,en_def_s;

	if (user_on->turns<=0) {

		ny_line(200,2,1);
		//od_printf("\n\r\n`bright red`You got no fights left today ...\n\r");

		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		return;
	}

	if(expert==3)
		expert=1;

	intval=event_gen(user_on);
	if (intval==1) {
		if(rip)
			no_rip_m=1;
		return;
	}
	if (intval==2) {
		if(rip)
			no_rip_m=1;
		user_on->turns--;
		return;
	}


	key=cop_list();

	if (key=='Q') {
		if(rip)
			no_rip_m=1;
		return;
	}

	num= key - 48;

	if ( (num*2) > user_on->level) {

		ny_line(261,2,1);
		//od_printf("\n\r\n`bright white`You cannot attack this cop in your level...\n\r");

		if(!rip)
			WaitForEnter();
		else {
			od_get_answer("\n\r");
			no_rip_m=1;
		}
		return;
	}

	od_printf("\n\r\n");
	ny_clr_scr();



	ny_line(201,0,0);
	//  od_printf("`bright red`Y`red`ou find ");
	print_cop(num);
	ny_line(202,0,2);

	ny_line(203,0,0);
	//  od_printf("`bright red`H`red`e got a bad lokin' `bright green`");
	print_arm((weapon)num);


	hitpoints=xp_random(pow(1.7,(double)num*2+1)*10 )+pow(1.7,(double)num*2+1)*10;
	strength=xp_random(pow(1.3,(double)num*2+1)*4)+pow(1.3,(double)num*2+1)*1.8;
	defense=xp_random(pow(1.3,(double)num*2+1)*4)+pow(1.3,(double)num*2+1)*1.8;

	INT32 enhitp;
	enhitp=hitpoints;

	user_on->turns--;
	wrt_sts();

	do {

bam_cf:
		ny_line(262,3,2);
		//    od_printf("\n\r\n\n\r`bright red`C`red`op `bright red`F`red`ight");
		//    od_printf("\n\r\n");
		print_cop(num);
		ny_line(205,0,0);
		od_printf(D_Num(hitpoints));
		ny_line(206,1,0);
		od_printf("%s\n\r\n",D_Num(user_on->hitpoints));
		/*    od_printf("\n\r\n`bright red`[A] `red`- `bright red`A`red`ttack\n\r");
		    od_printf("`bright red`[T] `red`- `bright red`T`red`ake `bright red`D`red`rug `bright red`A`red`nd `bright red`A`red`ttack\n\r");
		    od_printf("`bright red`[G] `red`- `bright red`G`red`et `bright red`O`red`utta `bright red`H`red`ere\n\r");
		    od_printf("`bright red`[Y] `red`- `bright red`Y`red`er `bright red`S`red`tats\n\r");
		    od_printf("\n\r`bright red`W`red`hat ya gonna do? (`bright red`[A] G Y`red`)");*/
		ny_send_menu(ATTACK,"");
		ny_line(207,1,0);
		//    ny_disp_emu("\n\r`@W`4hat ya gonna do? (`@[A] T G Y`4)");


		key=od_get_answer("ATKPRGY\n\r");
		if (key=='\n' || key=='\r')
			key='A';

		od_putch(key);

		if(key=='R' && user_on->rocks==0) {
			ny_line(445,2,1);
			WaitForEnter();
			goto bam_cf;
		}


		if (key == 'T') {  // Take drugs
			take_drug();
			no_rip_m=0;
			key='A';
		}
		if (key=='A' || key=='K' || key=='P' || key=='R') {
			// when atacking
			//	      randomize();
			if(key=='K') {
				hit_s=xp_random(150 - user_on->kick_ability);
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->kick_ability/100.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(439,2,0);
				} else {
					hit_s=0;
					ny_line(440,2,0);
				}
			} else if(key=='P') {
				hit_s=xp_random(140 - user_on->punch_ability);
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->punch_ability/90.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(441,2,0);
				} else {
					hit_s=0;
					ny_line(442,2,0);
				}
			} else if(key=='R') {
				hit_s=xp_random(145 - user_on->throwing_ability);
				user_on->rocks--;
				if(hit_s<35) {
					hit_s = (INT32)user_on->strength * (pow((user_on->level/2)+2,1.2)*6.0*(user_on->throwing_ability/110.0)) * (double)((xp_random(80)+90)/100.0);
					ny_line(443,2,0);
				} else {
					hit_s=0;
					ny_line(444,2,0);
				}
			}


			/*if (key == 'T') {  // Take drugs
			  take_drug();
			  no_rip_m=0;
			  key='A';
			}
			if (key=='A') {*/
			// when atacking

			//      randomize();
attack_again_c:
			;
			if(key=='A')
				hit_s = user_on->strength * (what_arm_force(user_on->arm)) * (double)((xp_random(80)+80)/100.0);
			// If he is on drugs
			if(user_on->drug_high > 0)
				hit_s = (hit_s * what_drug_force_a(user_on->drug,user_on->drug_high));

			//      hit_s*=pow(1.1,user_on->level);

			hit_s*=(100.0-(user_on->since_got_laid*2))/100.0;
			if (user_on->drug>=COKE)
				hit_s-=xp_random(user_on->drug_days_since * pow((double)user_on->drug_addiction,1.2))/60;
			if (hit_s<0)
				hit_s=0;

			en_hit_s = strength * (what_arm_force((weapon)num)) * (double)((xp_random(75)+75)/100.0);

			def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
			def_s*=pow(1.2,user_on->level);
			def_s*=(100.0 - (user_on->since_got_laid*2))/100.0;
			if (user_on->drug>=COKE)
				def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
			if (def_s<0)
				def_s=0;

			en_def_s = defense * (double)((xp_random(80)+50)/100.0);

			def_s/=2;
			en_def_s/=2;

			en_hit_s-=def_s;
			if (en_hit_s<0)
				en_hit_s=0;

			hit_s-=en_def_s;
			if (hit_s<0)
				hit_s=0;

			if (hit_s==0 && en_hit_s==0 && key=='A')
				goto attack_again_c;

			hitpoints-=hit_s;


			ny_line(208,2,0);
			//      od_printf("\n\r\n`bright red`Y`red`a kick ");
			print_cop(num);
			ny_line(209,0,0);
			od_printf(D_Num(hit_s));
			ny_line(210,0,0);


			if (hitpoints>0) {

				od_printf("\n\r\n");
				print_cop(num);
				ny_line(211,0,0);
				od_printf(D_Num(en_hit_s));
				ny_line(210,0,0);
				user_on->hitpoints-=en_hit_s;
				wrt_sts();
				if (user_on->hitpoints<=0) {
					od_printf("\n\r\n");
					ny_send_menu(ASS_KICKED,"");

					/*	  od_printf("\n\r\n`bright white`You had yer ass kicked ... oh well that happens\n\r");
						  od_printf("Come back tomorrow to get revenge ...\n\r");
						  od_printf("You lost all the money on ya...\n\r");
						  od_printf("And 2%c of yer points\n\r",37);*/

					news_post(user_on->say_loose,user_on->name,"A Cop",5);
					user_on->money=0;
					user_on->alive=UNCONCIOUS;
					points_loose(user_on->points*.02);
					//wrt_sts();
					WaitForEnter();
					od_exit(10,FALSE);
				}

			}
		} else if (key=='G') {
			intval=xp_random(2);
			if (intval==0) {

				ny_line(212,2,1);
				//od_printf("\n\r\n`bright red`Y`red`a got away ...\n\r");

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				return;
			} else {

				ny_line(213,2,1);
				//od_printf("\n\r\n`bright red`Y`red`e couldn't find a way outta this ...\n\r");

				//	randomize();
				en_hit_s = strength * (what_arm_force((weapon)num)) * (double)((xp_random(75)+75)/100.0);
				def_s = user_on->defense * what_drug_force_d(user_on->drug,user_on->drug_high) * (double)((xp_random(80)+140)/100.0);
				def_s*=pow(1.2,user_on->level);
				def_s*=(100.0-(user_on->since_got_laid*2))/100.0;
				if (user_on->drug>=COKE)
					def_s-=xp_random(user_on->drug_days_since * pow(user_on->drug_addiction,1.2))/60;
				if (def_s<0)
					def_s=0;
				def_s/=2;
				en_hit_s-=def_s;
				if (en_hit_s<0)
					en_hit_s=0;


				od_printf("\n\r\n");
				print_cop(num);
				ny_line(211,0,0);
				od_printf(D_Num(en_hit_s));
				ny_line(210,0,0);


				user_on->hitpoints-=en_hit_s;
				if (user_on->hitpoints<=0) {

					od_printf("\n\r\n");
					ny_send_menu(ASS_KICKED,"");

					/*od_printf("\n\r\n`bright white`You had yer ass kicked ... oh well that happens\n\r");
					od_printf("Come back tomorrow to get revenge ...\n\r");
					od_printf("You lost all the money on ya...\n\r");
					od_printf("And 2%c of yer points\n\r",37);*/
					news_post(user_on->say_loose,user_on->name,"A Cop",5);
					user_on->alive=UNCONCIOUS;
					user_on->money=0;
					points_loose(user_on->points*.02);
					//	  wrt_sts();

					WaitForEnter();
					od_exit(10,FALSE);
				}
			}
		} else {
			DisplayStats();
			WaitForEnter();
		}
	} while (hitpoints>0);

	//points_raise((DWORD)xp_random(((num+1)*2)*12)+3);
	intval=(num+1)*(1.1)*(randomf(enhitp*.5)+enhitp*.5)+(4*(num+1));
	if (intval<enhitp)
		intval=enhitp;
	money_plus(intval);

	//od_printf("\n\r%lu",user_on->money);
	//money_plus((user_on->level+1)*xp_random((enhitp*2))+(20*(user_on->level+1)));
	//od_printf("\n\r%lu",user_on->money);
	//user_on->money+=xp_random((enhitp*(1.05+(.15/((user_on->level+1)/5.0)))))+(18*(user_on->level+1));
	//user_on->money+=xp_random((enhitp*(.75+(.75/(user_on->level+1)))))+(18*(user_on->level+1));

	ny_line(214,2,0);
	//od_printf("\n\r\n`bright red`Y`red`ou kicked ");
	print_cop(num);
	//  od_printf("'s `red`ass ...\n\r\n");
	ny_line(215,0,2);
	ny_line(216,0,0);
	od_printf(D_Num(intval));
	ny_line(217,0,0);
	intval=xp_random(((num+1)*2)*12)+3;
	od_printf(D_Num((DWORD)intval * (DWORD)DrgPtsCoef()));
	ny_line(218,0,1);
	points_raise(intval);

	WaitForEnter();


} // End of function cop attack ops

double
what_drug_force_a(drug_type droga,INT16 high) // Gives the factor for drug in use for attack
{
	if(droga == POT)
		return (1.0-(high/800.0));
	else if(droga == HASH)
		return (1.0-(high/650.0));
	else if(droga == LSD)
		return (1.0-(high/500.0));
	else if(droga == COKE)
		return ((.5*(high/100.0))+1);
	else if(droga == PCP)
		return ((high/100.0)+1);
	else
		return 1;
} // End of function what drug force a

double
what_drug_force_d(drug_type droga,INT16 high) // Gives the factor for drug in use fer defense
{
	if(droga == POT)
		return (1.0-(high/800.0));
	else if(droga == HASH)
		return (1.0-(high/650.0));
	else if(droga == LSD)
		return (1.0-(high/500.0));
	else if(droga == COKE)
		return (.5*(high/100.0)+1);
	else if(droga == PCP)
		return 1;
	else
		return ((high/100.0)+1);
} // End of function what drug force d


double what_arm_force(weapon zbran) // Gives the multiplication of strengh in from weapon
{

	if(zbran == HANDS)
		return 1.0;
	else if(zbran == PEPPER)
		return 1.5;
	else if(zbran == SHARP_STICK)
		return 1.6;
	else if(zbran == SCREWDRIVER)
		return 1.7;
	else if(zbran == KNIFE)
		return 2.0;
	else if(zbran == HAMMER)
		return 3.0;
	else if(zbran == CHAIN)
		return 4.0;
	else if(zbran == LEAD_PIPE)
		return 5.5;
	else if(zbran == GUN)
		return 7.0;
	else if(zbran == COLT)
		return 8.5;
	else if(zbran == RIFLE)
		return 11.0;
	else if(zbran == ELEPHANT_GUN)
		return 13.3;
	else if(zbran == LASER_GUN)
		return 16.0;
	else if(zbran == NAILGUN)
		return 19.0;
	else if(zbran == SHOTGUN)
		return 25.0;
	else if(zbran == ASSAULT_RIFLE)
		return 33.3;
	else if(zbran == MACHINEGUN)
		return 40.0;
	else if(zbran == PROTON_GUN)
		return 54.0;
	else if(zbran == GRANADE_LAUNCHER)
		return 70.0;
	else if(zbran == NEUTRON_PHASER)
		return 95.0;
	else if(zbran == BLASTER)
		return 120.0;
	else if(zbran == ULTRASOUND_GUN)
		return 127.0;
	else
		return 1.0;

} // End of function what arm force

INT16
event_gen(user_rec *user_on) {
	INT16 chance;
	INT32 intval;
	char key;
	//  char numstr[14];
	//  ffblk ff;

	//randomize();

	ch_game_d();
	if(no_forrest_IGM==FALSE) {
		//    ch_game_d();
		intval=xp_random(9);
		if (intval>1) {
			//no event
			return 0;
		} else if (intval==0) {
			//find an igm
			forrest_IGM();
			return 1;
		}
	} else {
		//    ch_game_d();
		intval=xp_random(7);
		if (intval>0) {
			//no event
			return 0;
		}
	}

	intval=xp_random(6)+1;

	if(intval==1) {

		od_printf("\n\r\n");
		if(!rip)
			ny_clr_scr();
		ny_line(183,0,2);
		//od_printf("`bright`Stuff happens......\n\r\n");
		if(rip)
			od_get_answer("\n\r");
		ny_line(186,0,1);
		//    od_printf("`bright red`Y`red`a find an open car ...\n\r");
		if(rip)
			od_get_answer("\n\r");
		ny_line(187,0,1);
		//    od_printf("`bright red`T`red`here is money inside ...\n\r");
		if(rip)
			od_get_answer("\n\r");
		ny_line(188,0,0);
		//    od_printf("`bright red`T`red`ake the money? (`bright red`Y`red`/`bright red`N`red`)");

		key=ny_get_answer("YN");

		if(!rip)
			od_printf("%c\n\r\n",key);
		else
			od_disp_str("\n\n\r");
		if (key=='N') {
			ny_line(189,0,1);
			//      od_printf("`bright red`Y`red`a consider the risks and decide it ain't a good idea\n\r");

			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			return 1;
		}

		intval=xp_random(3);
		if (intval==0) {

			ny_line(190,0,1);
			if(rip)
				od_get_answer("\n\r");
			//      od_printf("`bright red`W`red`ow, the guy that owns the car came back and kicked yer ass!\n\r");
			ny_line(191,0,1);
			//      od_printf("`bright red`Y`red`a lost almost all yer hitpoints and 1/2 the money on ya!\n\r");

			money_minus(.5*user_on->money);
			user_on->hitpoints=1;
			wrt_sts();
			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			return 1;
		}

		intval=randomf(40*pow(1.5,(double)user_on->level));
		ny_line(263,0,0);
		//    od_printf("`bright red`W`red`ow ya got `bright red`
		od_printf(D_Num(intval));
		ny_line(264,0,1);
		//    `red` from that car ...\n\r",D_Num(intval));

		money_plus(intval);
		points_raise((10 * user_on->level) + 10);
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		return 1;
	}
	if(intval==2) {

		od_printf("\n\r\n");
		if(!rip)
			ny_clr_scr();
		ny_line(183,0,2);
		if(rip)
			od_get_answer("\n\r");
		//    od_printf("`bright`Stuff happens......\n\r\n");
		ny_line(184,0,1);
		if(rip)
			od_get_answer("\n\r");
		//    od_printf("`bright red`Y`red`a find a rich new car ...\n\r");
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
			return 1;
		}

		chance=xp_random(100);
		if (chance<=busted_ch_car) {

			ny_send_menu(BUSTED,"");
			//BUSTED!!!! .... well the police beat the hell out of you .....
			//Why don't you try again tomorrow...
			//Ya lost 2% of yer points!

			user_on->alive=UNCONCIOUS;
			points_loose(user_on->points*.02);
			news_post("busting up a car",user_on->name,"",2);
			if(!rip)
				WaitForEnter();
			else
				od_get_answer("\n\r");
			od_exit(10,FALSE);
		} else {
			chance=xp_random(100);
			if(chance<=success_ch_car) {

				ny_line(192,0,1);
				if(rip)
					od_get_answer("\n\r");
				//od_printf("\n\r\n`bright red`Y`red`ou totally smashed that car .... a very nice job...\n\r");
				ny_line(117,0,0);
				od_printf("%s",D_Num((INT32)(25*user_on->level+25) * (INT32)DrgPtsCoef()));

				points_raise((INT32)25 * user_on->level+25);
				if(!rip) {
					od_disp_str("\n\r");
					WaitForEnter();
				} else {
					od_disp_str("::^M@OK))|#|#|#\n\r");
					od_get_answer("\n\r");
				}
				return 1;
			} else {

				ny_line(406,0,1);

				if(!rip)
					WaitForEnter();
				else
					od_get_answer("\n\r");
				return 1;
			}
		}
	}
	if(intval==3) {

		od_printf("\n\r\n");
		if(!rip)
			ny_clr_scr();
		ny_line(183,0,2);
		if(rip)
			od_get_answer("\n\r");
		//od_printf("`bright`Stuff happens......\n\r\n");
		ny_line(193,0,1);
		if(rip)
			od_get_answer("\n\r");
		//    od_printf("`bright red`Y`red`a find a bunch of hippies ...\n\r");
		ny_line(194,0,1);
		if(rip)
			od_get_answer("\n\r");
		//    od_printf("`bright red`Y`red`a smoke some dope with them ...\n\r");
		ny_line(195,0,1);
		if(rip)
			od_get_answer("\n\r");
		//    od_printf("`bright red`T`red`hey give ya hit of yer favorite drug ...\n\r");
		ny_line(196,0,1);
		//    od_printf("`bright red`A`red`nd yer hitpoints max out!\n\r");

		if (user_on->drug_hits<32767)
			user_on->drug_hits++;
		user_on->hitpoints=user_on->maxhitpoints;
		points_raise((10 * user_on->level) + 10);
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		return 1;
	}
	if(intval==4) {

		od_printf("\n\r\n");
		if(!rip)
			ny_clr_scr();
		intval=randomf(pow(1.5,(user_on->level+1))*10);
		ny_line(183,0,2);
		if(rip)
			od_get_answer("\n\r");
		//od_printf("`bright`Stuff happens......\n\r\n");
		ny_line(265,0,0);
		od_printf(D_Num(intval));
		ny_line(266,0,1);

		money_plus(intval);
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		return 1;
	}
	if(intval==5) {

		od_printf("\n\r\n");
		if(!rip)
			ny_clr_scr();
		intval=xp_random(10 * (user_on->level+1))+2;
		ny_line(183,0,2);
		if(rip)
			od_get_answer("\n\r");
		//od_printf("`bright`Stuff happens......\n\r\n");
		ny_line(197,0,1);
		if(rip)
			od_get_answer("\n\r");
		//    od_printf("`bright red`Y`red`a fell inside a hole ...\n\r");
		ny_line(198,0,1);
		//od_printf("`bright red`Y`red`a lost 1/3 of yer hitpoints ...\n\r");

		user_on->hitpoints*=2.0/3.0;
		wrt_sts();
		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");
		return 1;
	}
	if(intval==6) {
		od_printf("\n\r\n");
		if(!rip)
			ny_clr_scr();
		intval=xp_random(14)+2;
		if((intval + user_on->rocks) > 250)
			intval=250 - user_on->rocks;
		ny_line(183,0,2);
		//Stuff happens...
		if(rip)
			od_get_answer("\n\r");
		ny_line(265,0,0);
		od_printf("%d",intval);
		ny_line(438,0,1);

		user_on->rocks+=intval;

		if(!rip)
			WaitForEnter();
		else
			od_get_answer("\n\r");

		return 1;
	}

	return 0;
}


INT16
len_format(char str[]) {
	INT16 len,
	cnt;

	len=0;
	cnt=0;

	while(str[cnt]!=0) {
		if(str[cnt]!='`') {
			cnt++;
			len++;
		} else {
			if(str[++cnt]!=0)
				cnt++;
		}
	}
	return len;
}

void
ny_send_file(const char filename[]) {
	char line[124];
	FILE *phile;
	INT16 cnt;
	INT16 nonstop=FALSE;
	char key;
	char *keyp;

	phile=ShareFileOpen(filename,"rb");
	if(phile=NULL)
		return;

	cnt=2;

	while (fgets(line,120,phile)!=NULL) {
		if((keyp=strchr(line,'\r'))!=NULL) {
			*keyp='\r';
			*(keyp+1)='\n';
			*(keyp+2)='\0';
		}
		ny_disp_emu(line);
		if (nonstop==FALSE) {
			cnt++;
			if(len_format(line)>80)
				cnt++;

			if (cnt>od_control.user_screen_length) {

				ny_disp_emu("`%More (Y/n/=)");
				key=ny_get_answer("YN=\n\r");
				od_printf("\r            \r");
				if(key=='N') {
					fclose(phile);
					return;
				}
				if(key=='=')
					nonstop=TRUE;

				cnt=2;
				//od_printf("\n\r");
			}
		}
	}
	fclose(phile);
	//  WaitForEnter();
}


void
ny_get_index(void) {
	char line[124];
	FILE *phile;
	char numstr[26];

	ch_game_d();

	if(rip) {
		if(clean_mode==FALSE)
			phile=ShareFileOpen("menus.rip","rb");
		else
			phile=ShareFileOpen("menusc.rip","rb");
	} else {
		if(clean_mode==FALSE)
			phile=ShareFileOpen("menus.dat","rb");
		//  else if(clean_mode==666)
		//    phile=ShareFileOpen("MENUSXD.DAT","rb");
		else
			phile=ShareFileOpen("menusc.dat","rb");
	}
	if(phile==NULL)
		return;

	while (fgets(line,120,phile)!=NULL) {
		sscanf(line,"%s",numstr);

		if(strcmp(numstr,"@CENTRAL_PARK")==0)
			menu_index[(INT16)CENTRAL_PARK]=ftell(phile);
		else if(strcmp(numstr,"@CENTRAL_PARK_IB")==0)
			menu_index[(INT16)CENTRAL_PARK_IB]=ftell(phile);
		else if(strcmp(numstr,"@EVIL_STUFF")==0)
			menu_index[(INT16)EVIL_STUFF]=ftell(phile);
		else if(strcmp(numstr,"@BANK")==0)
			menu_index[(INT16)BANK]=ftell(phile);
		else if(strcmp(numstr,"@HEALING")==0)
			menu_index[(INT16)HEALING]=ftell(phile);
		else if(strcmp(numstr,"@FOOD")==0)
			menu_index[(INT16)FOOD]=ftell(phile);
		else if(strcmp(numstr,"@DRUGS")==0)
			menu_index[(INT16)DRUGS]=ftell(phile);
		else if(strcmp(numstr,"@ARMS")==0)
			menu_index[(INT16)ARMS]=ftell(phile);
		else if(strcmp(numstr,"@SEX")==0)
			menu_index[(INT16)SEX]=ftell(phile);
		else if(strcmp(numstr,"@MAIL")==0)
			menu_index[(INT16)MAIL]=ftell(phile);
		else if(strcmp(numstr,"@REST")==0)
			menu_index[(INT16)REST]=ftell(phile);
		else if(strcmp(numstr,"@P_FIG")==0)
			menu_index[(INT16)P_FIG]=ftell(phile);
		else if(strcmp(numstr,"@C_FIG")==0)
			menu_index[(INT16)C_FIG]=ftell(phile);
		else if(strcmp(numstr,"@S_FIG")==0)
			menu_index[(INT16)S_FIG]=ftell(phile);
		else if(strcmp(numstr,"@ENTRY_1")==0)
			menu_index[(INT16)ENTRY_1]=ftell(phile);
		else if(strcmp(numstr,"@ENTRY_2")==0)
			menu_index[(INT16)ENTRY_2]=ftell(phile);
		else if(strcmp(numstr,"@ONLINE")==0)
			menu_index[(INT16)ONLINE]=ftell(phile);
		else if(strcmp(numstr,"@NEWZ")==0)
			menu_index[(INT16)NEWZ]=ftell(phile);
		else if(strcmp(numstr,"@LIST")==0)
			menu_index[(INT16)LIST]=ftell(phile);
		else if(strcmp(numstr,"@CONSIOUS")==0)
			menu_index[(INT16)CONSIOUS]=ftell(phile);
		else if(strcmp(numstr,"@ATTACK")==0)
			menu_index[(INT16)ATTACK]=ftell(phile);
		else if(strcmp(numstr,"@WIN")==0)
			menu_index[(INT16)WIN]=ftell(phile);
		else if(strcmp(numstr,"@MAINT_RUN")==0)
			menu_index[(INT16)MAINT_RUN]=ftell(phile);
		else if(strcmp(numstr,"@WEAPONS")==0)
			menu_index[(INT16)WEAPONS]=ftell(phile);
		else if(strcmp(numstr,"@COPS")==0)
			menu_index[(INT16)COPS]=ftell(phile);
		else if(strcmp(numstr,"@NEW")==0)
			menu_index[(INT16)NEW]=ftell(phile);
		else if(strcmp(numstr,"@NATION")==0)
			menu_index[(INT16)NATION]=ftell(phile);
		//    else if(strcmp(numstr,"@BLUE_LINE")==0)
		//      menu_index[(INT16)BLUE_LINE]=ftell(phile);
		else if(strcmp(numstr,"@OTHER")==0)
			menu_index[(INT16)OTHER]=ftell(phile);
		//    else if(strcmp(numstr,"@DEAD_OUT")==0)
		//      menu_index[(INT16)DEAD_OUT]=ftell(phile);
		//    else if(strcmp(numstr,"@BEATEN_OUT")==0)
		//      menu_index[(INT16)BEATEN_OUT]=ftell(phile);
		//    else if(strcmp(numstr,"@FOUGHT_OUT")==0)
		//      menu_index[(INT16)FOUGHT_OUT]=ftell(phile);
		//    else if(strcmp(numstr,"@WAKE_UP")==0)
		//      menu_index[(INT16)WAKE_UP]=ftell(phile);
		//    else if(strcmp(numstr,"@BEEN_ALIVE")==0)
		//      menu_index[(INT16)BEEN_ALIVE]=ftell(phile);
		else if(strcmp(numstr,"@NEW_NAME")==0)
			menu_index[(INT16)NEW_NAME]=ftell(phile);
		else if(strcmp(numstr,"@NEW_WIN")==0)
			menu_index[(INT16)NEW_WIN]=ftell(phile);
		else if(strcmp(numstr,"@NEW_LOOSE")==0)
			menu_index[(INT16)NEW_LOOSE]=ftell(phile);
		else if(strcmp(numstr,"@TEN_BEST")==0)
			menu_index[(INT16)TEN_BEST]=ftell(phile);
		else if(strcmp(numstr,"@BUSTED")==0)
			menu_index[(INT16)BUSTED]=ftell(phile);
		else if(strcmp(numstr,"@ASS_KICKED")==0)
			menu_index[(INT16)ASS_KICKED]=ftell(phile);
		else if(strcmp(numstr,"@ASS_KICKED_P")==0)
			menu_index[(INT16)ASS_KICKED_P]=ftell(phile);
		else if(strcmp(numstr,"@ASS_KICKED_O")==0)
			menu_index[(INT16)ASS_KICKED_O]=ftell(phile);
		else if(strcmp(numstr,"@COLORS_HELP")==0)
			menu_index[(INT16)COLORS_HELP]=ftell(phile);
		else if(strcmp(numstr,"@CH_DRUG")==0)
			menu_index[(INT16)CH_DRUG]=ftell(phile);
		else if(strcmp(numstr,"@LIST_IB_SYS")==0)
			menu_index[(INT16)LIST_IB_SYS]=ftell(phile);
		else if(strcmp(numstr,"@IBBS_MENU")==0)
			menu_index[(INT16)IBBS_MENU]=ftell(phile);
		else if(strcmp(numstr,"@HITMEN")==0)
			menu_index[(INT16)HITMEN]=ftell(phile);
		else if(strcmp(numstr,"@TEN_BEST_IBBS")==0)
			menu_index[(INT16)TEN_BEST_IBBS]=ftell(phile);
	}
	fclose(phile);
}




char
ny_send_menu(menu_t menu,const char allowed[],INT16 onscreen) {
	char line[124];
	FILE *phile;
	INT16 cnt;
	INT16 nonstop=FALSE;
	char key=0;
	char *keyp;
	char c_dir;
	char menu_type;

	menu_type=0;

	c_dir=c_dir_g;

	if(c_dir==1)
		ch_game_d();

	if(rip) {
		if(clean_mode==FALSE)
			phile=ShareFileOpen("menus.rip","rb");
		else
			phile=ShareFileOpen("menusc.rip","rb");
	} else {
		if(clean_mode==FALSE)
			phile=ShareFileOpen("menus.dat","rb");
		//  else if(clean_mode==666)
		//    phile=ShareFileOpen("MENUSXD.DAT","rb");
		else
			phile=ShareFileOpen("menusc.dat","rb");
	}
	if(phile==0)
		return(0);

	fseek(phile,menu_index[(INT16)menu],SEEK_SET);

	cnt=2+onscreen;

	while (fgets(line,120,phile)!=NULL && line[0]!='@') {
		if((cnt-onscreen)==2 && rip && line[0]=='$' && line[1]=='R' && line[2]=='I' && line[3]=='P' && line[4]=='$') {
			menu_type=1;
			od_disp_str("\n\r");
		} else {
			if(!rip) {
				key=od_get_key(FALSE);
				if (key>='a' && key<='z')
					key-=32;
				if (key!=0 && strchr(allowed,key)!=NULL) {
					fclose(phile);
					if(c_dir==1)
						ch_flag_d();
					return key;
				}
			}

			if((keyp=strchr(line,'\r'))!=NULL) {
				*keyp='\r';
				*(keyp+1)='\n';
				*(keyp+2)='\0';
			}

			if(line[0]=='\0')
				od_printf("\n\r");
			else
				if(menu_type==0)
					ny_disp_emu(line);
				else
					nyr_disp_emu(line);

			if (nonstop==FALSE && menu_type==0) {
				cnt++;
				if(len_format(line)>80)
					cnt++;

				if (cnt>od_control.user_screen_length) {

					ny_disp_emu("`%More (Y/n/=)");
					key=ny_get_answer("YN=\n\r");
					od_printf("\r            \r");
					if(key=='N') {
						fclose(phile);
						if(c_dir==1)
							ch_flag_d();
						return 0;
					}
					if(key=='=')
						nonstop=TRUE;

					cnt=2;

				}
			}
		}
	}
	fclose(phile);
	if(c_dir==1)
		ch_flag_d();
	return 0;
}

INT32 fpoint[15];

void
ny_stat_line(INT16 line,INT16 before,INT16 after) {
	char c_dir;
	INT16 cnt;
	static char *lines[44];
	FILE *phile;
	char string[302];
	//  char *keyp;
	INT16 x;

	if(line==-1) {
		c_dir=c_dir_g;

		if(c_dir==1)
			ch_game_d();

		if(rip) {
			if(clean_mode==FALSE)
				phile=ShareFileOpen("lines.rip","rb");
			//  else if(clean_mode==666)
			//    phile=ShareFileOpen("LINESXD.RIP","rb");
			else
				phile=ShareFileOpen("linesc.rip","rb");
		} else {
			if(clean_mode==FALSE)
				phile=ShareFileOpen("lines.dat","rb");
			//  else if(clean_mode==666)
			//    phile=ShareFileOpen("LINESXD.DAT","rb");
			else
				phile=ShareFileOpen("linesc.dat","rb");
		}
		if(phile==NULL)
			return;
		ny_read_stat_line(38,string,phile);
		lines[0]=(char *)malloc(strlen(string)+1);
		strcpy(lines[0],string);
		ny_read_stat_line(362,string,phile);
		lines[1]=(char *)malloc(strlen(string)+1);
		strcpy(lines[1],string);
		ny_read_stat_line(363,string,phile);
		lines[2]=(char *)malloc(strlen(string)+1);
		strcpy(lines[2],string);

		ny_read_stat_line(364,string,phile);
		lines[3]=(char *)malloc(strlen(string)+1);
		strcpy(lines[3],string);

		ny_read_stat_line(365,string,phile);
		lines[4]=(char *)malloc(strlen(string)+1);
		strcpy(lines[4],string);

		ny_read_stat_line(366,string,phile);
		lines[5]=(char *)malloc(strlen(string)+1);
		strcpy(lines[5],string);

		ny_read_stat_line(399,string,phile);
		lines[6]=(char *)malloc(strlen(string)+1);
		strcpy(lines[6],string);

		ny_read_stat_line(39,string,phile);
		lines[7]=(char *)malloc(strlen(string)+1);
		strcpy(lines[7],string);

		ny_read_stat_line(367,string,phile);
		lines[8]=(char *)malloc(strlen(string)+1);
		strcpy(lines[8],string);

		ny_read_stat_line(368,string,phile);
		lines[9]=(char *)malloc(strlen(string)+1);
		strcpy(lines[9],string);

		ny_read_stat_line(270,string,phile);
		lines[10]=(char *)malloc(strlen(string)+1);
		strcpy(lines[10],string);

		ny_read_stat_line(369,string,phile);
		lines[11]=(char *)malloc(strlen(string)+1);
		strcpy(lines[11],string);

		ny_read_stat_line(370,string,phile);
		lines[12]=(char *)malloc(strlen(string)+1);
		strcpy(lines[12],string);

		ny_read_stat_line(371,string,phile);
		lines[13]=(char *)malloc(strlen(string)+1);
		strcpy(lines[13],string);

		for(x=271;x<286;x++) {
			ny_read_stat_line(x,string,phile);
			lines[x-257]=(char *)malloc(strlen(string)+1);
			strcpy(lines[x-257],string);
		}
		ny_read_stat_line(433,string,phile);
		lines[29]=(char *)malloc(strlen(string)+1);
		strcpy(lines[29],string);

		ny_read_stat_line(434,string,phile);
		lines[30]=(char *)malloc(strlen(string)+1);
		strcpy(lines[30],string);

		ny_read_stat_line(435,string,phile);
		lines[31]=(char *)malloc(strlen(string)+1);
		strcpy(lines[31],string);

		ny_read_stat_line(436,string,phile);
		lines[32]=(char *)malloc(strlen(string)+1);
		strcpy(lines[32],string);

		for(x=286;x<292;x++) {
			ny_read_stat_line(x,string,phile);
			lines[x-253]=(char *)malloc(strlen(string)+1);
			strcpy(lines[x-253],string);
		}
		ny_read_stat_line(372,string,phile);
		lines[39]=(char *)malloc(strlen(string)+1);
		strcpy(lines[39],string);

		ny_read_stat_line(373,string,phile);
		lines[40]=(char *)malloc(strlen(string)+1);
		strcpy(lines[40],string);

		ny_read_stat_line(374,string,phile);
		lines[41]=(char *)malloc(strlen(string)+1);
		strcpy(lines[41],string);

		ny_read_stat_line(292,string,phile);
		lines[42]=(char *)malloc(strlen(string)+1);
		strcpy(lines[42],string);

		ny_read_stat_line(293,string,phile);
		lines[43]=(char *)malloc(strlen(string)+1);
		strcpy(lines[43],string);

		//    printf("\n%d\n\r",lines[42]);
		//    printf("\n%s\n\r",lines[42]);

		fclose(phile);
		if(c_dir==1)
			ch_flag_d();
		//    od_get_answer("K");
		return;
	}
	for(cnt=0;cnt<before;cnt++)
		od_printf("\n\r");

	ny_disp_emu((char *)lines[line]);
	//  printf("\n%s|%d\n\r",lines[0],line);
	//od_putch('|');

	for(cnt=0;cnt<after;cnt++)
		od_printf("\n\r");
}


void
ny_read_stat_line(INT16 line,char *string, FILE *phile) {
	INT16 cnt2,cnt;
	//  FILE *phile;
	char *keyp;

	cnt2=line/50;
	line-=50*cnt2;
	fseek(phile,fpoint[cnt2],SEEK_SET);
	if(cnt2>0)
		fgets(string,300,phile);
	for(cnt=0;cnt<line;cnt++)
		fgets(string,300,phile);

	if((keyp=strchr(string,'\r'))!=NULL)
		*keyp='\0';

	//    printf("\n%d\n\r",*string);
	//    printf("\n%s",string);
	//    scanf("%c",keyp);
}

void
ny_line(INT16 line,INT16 before,INT16 after) {
	char lines[302];
	FILE *phile;
	INT16 cnt,cnt2;
	char *keyp;
	//  static long fpoint[10];
	char c_dir;

	c_dir=c_dir_g;

	if(c_dir==1)
		ch_game_d();

	if(rip) {
		if(clean_mode==FALSE)
			phile=ShareFileOpen("lines.rip","rb");
		//  else if(clean_mode==666)
		//    phile=ShareFileOpen("LINESXD.RIP","rb");
		else
			phile=ShareFileOpen("linesc.rip","rb");
	} else {
		if(clean_mode==FALSE)
			phile=ShareFileOpen("lines.dat","rb");
		//  else if(clean_mode==666)
		//    phile=ShareFileOpen("LINESXD.DAT","rb");
		else
			phile=ShareFileOpen("linesc.dat","rb");
	}

	if(phile==NULL)
		return;

	if(line == -1) {
		cnt2=1;
		fpoint[0]=0;
		while (fgets(lines,300,phile)!=NULL) {
			for(cnt=1;fgets(lines,300,phile)!=NULL && cnt<49;cnt++)
				fpoint[cnt2]=ftell(phile);
			cnt2++;
		}
		fclose(phile);
		if(c_dir==1)
			ch_flag_d();
		return;
	}

	cnt2=line/50;

	line-=50*cnt2;

	//  line++;

	fseek(phile,fpoint[cnt2],SEEK_SET);

	if(cnt2>0)
		fgets(lines,300,phile);
	for(cnt=0;cnt<line;cnt++)
		fgets(lines,300,phile);

	fclose(phile);

	if((keyp=strchr(lines,'\r'))!=NULL)
		*keyp='\0';

	for(cnt=0;cnt<before;cnt++)
		od_printf("\n\r");

	/*  if(before==0 && rip==TRUE && lines[0]=='!')
	    od_printf("\n\r");*/


	ny_disp_emu(lines);

	for(cnt=0;cnt<after;cnt++)
		od_printf("\n\r");

	/*  if(after==0 && rip==TRUE && lines[0]=='!')
	    od_printf("\n\r");*/

	if(c_dir==1)
		ch_flag_d();
	return;
}
