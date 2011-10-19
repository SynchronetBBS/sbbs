// include the header
#include "ny2008.h"

// include prototypes for fights
#include "fights.h"

extern unsigned _stklen;

// price arrays
extern INT16 no_rip_m;
extern DWORD gun_price[A_BOMB+1];
extern INT16    drug_price[HEROIN+1];
//extern INT16 fast_mail;
INT16 no_forrest_IGM=FALSE;

// Declare global variables
extern INT16 nCurrentUserNumber,max_fights,max_drug_hits,condom_price,delete_after;
extern INT16 bank_interest;
extern char ansi_name[61],ascii_name[61];
extern INT16 do_scr_files;
extern user_rec cur_user;
extern enemy enemy_rec;
extern char uname[36];
extern char rec_credit[36];
extern INT16 do_maint;
extern char str[15];
extern INT16 expert;
extern INT16 rip;
extern char maint_exec[61];
extern char *t_buffer;
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

void
read_IGMs(void) {
        FILE *tparty;
        FILE *menuthing;
        INT16 ok,x2,x3;
        char *key, kkk;
        INT16 intval;
        char numstr[14];
        char exenam[251];
        char temp[31];

        /*  printf("\n}{Stack Length=%u}{\n",_stklen);
          scanf("%c",&kkk);*/

        //  od_printf("\r\n\nreading...\n\n");

        //  printf("\n\r15\n\r");

        ch_game_d();
        //  printf("\n\r16\n\r");
        tparty=ShareFileOpen(TRDPARTY_FILENAME,"rt");
        //  printf("\n\r17\n\r");
        if(tparty==NULL) {
                //    no_forrest_IGM=TRUE;
                return;
        }
        //  printf("\n\r18\n\r");
        ch_flag_d();
        sprintf(numstr,"mnu%d.dat",od_control.od_node);
        menuthing=ShareFileOpen(numstr,"wb");
        if(menuthing==NULL)
                return;
        ch_game_d();

        ok=0;
        while(tparty!=NULL && ok!=2 && fgets(temp,30,tparty)!=NULL) {
                //    od_printf("\n\n1111...\n\n");
                ok=0;
                if((key=strchr(temp,'\n'))!=0)
                        *key=0;
                else
                        while (kkk!='\n')
                                fscanf(tparty,"%c",&kkk);
                if((key=strchr(temp,';'))!=0)
                        *key=0;
                if (temp[0]!=0) {
                        //      od_printf("\n\n2222...\n\n");
                        while (ok==0) {
                                if (fgets(exenam,250,tparty)!=NULL) {
                                        if((key=strchr(exenam,'\n'))!=0)
                                                *key=0;
                                        else
                                                while (kkk!='\n')
                                                        fscanf(tparty,"%c",&kkk);
                                        if((key=strchr(exenam,';'))!=0)
                                                *key=0;
                                        if (exenam[0]!=0)
                                                ok=1;
                                        if((key=strchr(exenam,'{'))!=0) {
                                                for(x2=0;x2<=30 && *(key+x2+1)!='}' && *(key+x2+1)!=0;x2++) {
                                                        numstr[x2]= *(key+x2+1);
                                                        od_kernel();
                                                }
                                                numstr[x2]=0;
                                                x3=x2;
                                                x2++;
                                                if(*(key+x2)==0)
                                                        *(key+x2+1)=0;
                                                while(*(key+x2+1)!=0) {
                                                        *(key+x2-x3-1)= *(key+x2+1);
                                                        x2++;
                                                        od_kernel();
                                                }
                                                *(key+x2-x3-1)=0;
                                                sscanf(numstr,"%d",&intval);
                                                if(od_control.user_security<intval)
                                                        ok=3;
                                        }
                                } else {
                                        ok=2;
                                }
                        }
                        //      od_printf("\n\n1111 <%d>...\n\n",ok);
                        if (ok==1) {
                                //      od_printf("\n\n4444...\n\n");
                                ny_fwrite(temp,31,1,menuthing);
                                ny_fwrite(exenam,251,1,menuthing);
                        }
                }
        }
        //  WaitForEnter();
        fclose(tparty);
        fclose(menuthing);
}


void
read_fight_IGMs(void) {
        FILE *tparty;
        FILE *menuthing;
        INT16 ok,x2,x3;
        char *key, kkk;
        INT16 intval;
        char numstr[14];
        char exenam[251];
        char temp[31];

        //  od_printf("\r\n\nreading...\n\n");

        //  printf("\n\r20\n\r");
        ch_game_d();
        //  printf("\n\r21\n\r");
        tparty=ShareFileOpen(TRDEVENT_FILENAME,"rt");
        //  printf("\n\r22\n\r");
        if(tparty==NULL) {
                no_forrest_IGM=TRUE;
                return;
        }
        //  printf("\n\r23\n\r");
        ch_flag_d();
        sprintf(numstr,"fev%d.dat",od_control.od_node);
        menuthing=ShareFileOpen(numstr,"wb");
        if(menuthing==NULL)
                return;
        ch_game_d();

        ok=0;
        while(tparty!=NULL && ok!=2 && fgets(temp,30,tparty)!=NULL) {
                //    od_printf("\n\n1111...\n\n");
                ok=0;
                if((key=strchr(temp,'\n'))!=0)
                        *key=0;
                else
                        while (kkk!='\n')
                                fscanf(tparty,"%c",&kkk);
                if((key=strchr(temp,';'))!=0)
                        *key=0;
                if (temp[0]!=0) {
                        //      od_printf("\n\n2222...\n\n");
                        while (ok==0) {
                                if (fgets(exenam,250,tparty)!=NULL) {
                                        if((key=strchr(exenam,'\n'))!=0)
                                                *key=0;
                                        else
                                                while (kkk!='\n')
                                                        fscanf(tparty,"%c",&kkk);
                                        if((key=strchr(exenam,';'))!=0)
                                                *key=0;
                                        if (exenam[0]!=0)
                                                ok=1;
                                        if((key=strchr(exenam,'{'))!=0) {
                                                for(x2=0;x2<=30 && *(key+x2+1)!='}' && *(key+x2+1)!=0;x2++) {
                                                        numstr[x2]= *(key+x2+1);
                                                        od_kernel();
                                                }
                                                numstr[x2]=0;
                                                x3=x2;
                                                x2++;
                                                if(*(key+x2)==0)
                                                        *(key+x2+1)=0;
                                                while(*(key+x2+1)!=0) {
                                                        *(key+x2-x3-1)= *(key+x2+1);
                                                        x2++;
                                                        od_kernel();
                                                }
                                                *(key+x2-x3-1)=0;
                                                sscanf(numstr,"%d",&intval);
                                                if(od_control.user_security<intval)
                                                        ok=3;
                                        }
                                } else {
                                        ok=2;
                                }
                        }
                        //      od_printf("\n\n1111 <%d>...\n\n",ok);
                        if (ok==1) {
                                //      od_printf("\n\n4444...\n\n");
                                ny_fwrite(temp,31,1,menuthing);
                                ny_fwrite(exenam,251,1,menuthing);
                        }
                }
        }
        //  WaitForEnter();
        fclose(tparty);
        fclose(menuthing);
}



void
IGM(char exenamr[]) {
        char temp[31];
        INT16 x,x1,found_file;
        //      FILE *tparty;
        FILE *menuthing;
        //      char kkk;
        //      char *key;
        INT32 intval;
        char numstr[14];
        char exenam[251];
        INT32 maxnum;
        INT16 ok;
        static long where=0;

all_over_igm:
        ;
        //od_control.od_ker_exec=NULL;
        od_clear_keybuffer();
        od_printf("\n\r\n");
        if (expert!=2 && expert!=3)
                ny_clr_scr();

        if (expert==2 || expert==3) {
                ny_line(351,0,1);
                //        od_printf("\n\r`bright red`O`red`ther `bright red`S`red`tuff\n\r");
                ny_line(352,0,1);
                //        od_printf("`bright green`? = list  Q = Quit\n\r`bright blue`E`blue`nter `bright blue`Y`blue`er `bright blue`C`blue`ommand (%d mins)`bright blue`>",od_control.caller_timelimit);
                ny_line(36,0,0);
                od_printf("%d ",od_control.caller_timelimit);
                ny_line(37,0,0);
                maxnum=20;
                ch_flag_d();
                sprintf(numstr,"mnu%d.dat",od_control.od_node);
                menuthing=ShareFileOpen(numstr,"rb");
                if(menuthing==NULL)
                        return;
                if((where+(282*20))>=filelength(fileno(menuthing))) {
                        maxnum=(filelength(fileno(menuthing))-where)/(INT32)282;
                } else {
                        ok=1;
                }
                fclose(menuthing);
                ch_game_d();
        } else {
list_igm:
                ;
                found_file=FALSE;
                if(od_send_file("other") == TRUE)
                        found_file=TRUE;
                //od_control.od_ker_exec=NULL;
                //od_send_file("igm");
                if(found_file==FALSE)
                        ny_send_menu(OTHER,"");

                /*        ny_disp_emu("\n\r`@O`red`ther `bright red`S`red`tuff\n\r\n");
                          od_printf("`green`-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\r");
                          od_printf("`bright blue`You go and look for something less boring ...\n\r\n");*/

                ch_flag_d();
                sprintf(numstr,"mnu%d.dat",od_control.od_node);
                menuthing=ShareFileOpen(numstr,"rb");
                if(menuthing==NULL)
                        return;
                ch_game_d();

                if(found_file==FALSE) {
                        fseek(menuthing,where,SEEK_SET);
                        x=1;
                        while(menuthing!=NULL && x<=20 && ny_fread(temp,31,1,menuthing)!=0) {
                                ny_fread(exenam,251,1,menuthing);
                                if(found_file==FALSE) {
                                        od_printf(" `bright red`%-2d`red` - ",x);
                                        ny_disp_emu(temp,30);
                                        od_printf("  ");
                                        if ((INT16)(x/2) * 2 == x )
                                                od_printf("\n\r");
                                }
                                x++;
                        }
                        maxnum=x-1;
                        if ((INT16)(x/2) * 2 == x )
                                od_printf("\n\r");
                        if (x==1)
                                ny_line(178,0,1);
                        od_printf("\n\r");
                        if(where>0)
                                ny_line(409,0,0);
                        ok=0;
                        if(filelength(fileno(menuthing))>ftell(menuthing)) {
                                ny_line(410,0,0);
                                ok=1;
                        }
                        ny_line(179,0,2);
                } else {
                        maxnum=filelength(fileno(menuthing))/(INT32)282;
                        od_printf("\n\r");
                }
                fclose(menuthing);

                ny_line(36,0,0);
                od_printf("%d ",od_control.caller_timelimit);
                ny_line(37,0,0);
        }
        //od_control.od_ker_exec=ny_kernel;

        x1=0;
        do {
                if(x1==0)
                        numstr[0]=ny_get_answer("0123456789QPN?\b\n\r");
                else
                        numstr[x1]=ny_get_answer("0123456789\b\n\r");
                x1++;
                if(numstr[x1-1]=='\b') {
                        if(x1>1) {
                                od_printf("\b \b");
                                x1-=2;
                        } else {
                                x1--;
                        }
                } else {
                        if(numstr[x1-1]!='\r')
                                od_putch(numstr[x1-1]);
                }
        } while(numstr[x1-1]!='\n' &&
                numstr[x1-1]!='\r' &&
                numstr[0]!='Q' &&
                numstr[0]!='P' &&
                numstr[0]!='N' &&
                numstr[0]!='?' &&
                x1<10);

        //      printf("%ld",maxnum);

        numstr[x1]=0;
        //
        //      printf("\n%s",numstr);

        //      od_input_str(numstr,2,' ',127);
        if(expert==1)
                expert=3;
        if (numstr[0]==0) {
                exenamr[0]=0;
                return;
        }
        if (numstr[0]=='?') {
                od_printf("\n\r\n");
                ny_clr_scr();
                goto list_igm;
        }
        if (numstr[0]=='Q') {
                exenamr[0]=0;
                return;
        }

        if (found_file==FALSE &&
                ok==1 &&
                numstr[0]=='N') {
                if(expert==3)
                        expert=1;
                where+=282*20;
                goto all_over_igm;
        }
        if (found_file==FALSE &&
                where>0 &&
                numstr[0]=='P') {
                if(expert==3)
                        expert=1;
                where-=282*20;
                goto all_over_igm;
        }

        sscanf(numstr,"%ld",&intval);

        if (intval==0 || intval>maxnum) {
                goto all_over_igm;
        }


        ch_flag_d();
        sprintf(numstr,"mnu%d.dat",od_control.od_node);
        menuthing=ShareFileOpen(numstr,"rb");
        if(menuthing==NULL)
                return;
        ch_game_d();

        fseek(menuthing,where+((intval-1)*282)+31,SEEK_SET);
        ny_fread(exenamr,251,1,menuthing);
        //      printf("%s",exenamr);
        fclose(menuthing);

        return;
}



void
forrest_IGM() {
        char exenamr[251];
        char temp[31];
        char key;
        FILE *menuthing;
        INT16 intval;
        char numstr[14];
        INT32 maxnum;
        //od_control.od_ker_exec=NULL;
        no_rip_m=0;
        od_clear_keybuffer();
        od_printf("\n\r\n");
        ny_clr_scr();
        ch_flag_d();
        sprintf(numstr,"fev%d.dat",od_control.od_node);
        menuthing=ShareFileOpen(numstr,"rb");
        if(menuthing==NULL)
                return;
        maxnum=filelength(fileno(menuthing))/(INT32)282;
        intval=xp_random((INT16)maxnum);
        fseek(menuthing,intval*282,SEEK_SET);
        ny_fread(temp,31,1,menuthing);
        ny_fread(exenamr,251,1,menuthing);
        fclose(menuthing);
        ch_game_d();

        if(rip==FALSE) {
                ny_line(411,0,2);
                ny_disp_emu(temp);
        } else {
                ny_line(411,1,0);
                od_disp_str(ny_un_emu(temp));
                od_disp_str("::^M@OK))|#|#|#\n\r");
                od_get_answer("\n\r");
        }
        ny_line(412,2,0);
        key=ny_get_answer("YN");
        if(rip==FALSE)
                od_printf("%c\n\r\n",key);
        else
                od_disp_str("\n\r\n");


        if(key=='Y')
                call_IGM(exenamr);

        return;
}




//      od_putch(key);

//      return(key);


void
IGM_ops(void) {
        char exenam[251];
        do {
                ch_game_d();
                IGM(exenam);
                if(exenam[0]!=0)
                        call_IGM(exenam);
        } while(exenam[0]!=0);

}

void
CreateDropFile(INT16 all) {
        char numstr[25];
        FILE *justfile;

        if(all==TRUE)
                sprintf(numstr,"u%07d.inf",nCurrentUserNumber);
        else
                strcpy(numstr,TRDMAINT_LIST_FILENAME);
        justfile=ShareFileOpen(numstr,"w+t");
        if(justfile==NULL)
                return;
        fprintf(justfile,"%s\n",od_control.info_path);
        fprintf(justfile,"%d\n",od_control.caller_timelimit);
        fprintf(justfile,"%d\n",od_control.port);
        fprintf(justfile,"%lu\n",od_control.baud);
        if(od_control.user_avatar!=FALSE) {
                fprintf(justfile,"AVATAR");
        } else if(od_control.user_ansi!=FALSE || rip==TRUE) {
                fprintf(justfile,"ANSI");
        } else {
                fprintf(justfile,"ASCII");
        }
        fprintf(justfile,"\n%s\n",od_control.user_location);
        if(od_control.od_com_method==COM_FOSSIL)
                fprintf(justfile,"FOSSIL\n");
        else {
                fprintf(justfile,"NOFOSSIL\n");
                fprintf(justfile,"%d\n",od_control.od_com_address); //
                fprintf(justfile,"%d\n",(INT16)od_control.od_com_irq);
                if (od_control.od_com_no_fifo==TRUE)
                        fprintf(justfile,"NOFIFO\n");
                else
                        fprintf(justfile,"FIFO\n");
                fprintf(justfile,"%d\n",(INT16)od_control.od_com_fifo_trigger);
                fprintf(justfile,"%u\n",(INT16)od_control.od_com_rx_buf);
                fprintf(justfile,"%u\n",(INT16)od_control.od_com_tx_buf);
        }
        fclose(justfile);

        if(all==TRUE) {
                sprintf(numstr,"n%07d.sts",od_control.od_node);
                justfile=ShareFileOpen(numstr,"wb");
                if(justfile != NULL) {
                        ny_fwrite(&cur_user,sizeof(user_rec),1,justfile);
                        fclose(justfile);
                }

                sprintf(numstr,"n%07d.stt",od_control.od_node);
                justfile=ShareFileOpen(numstr,"wt");
                if(justfile != NULL) {
                        fprintf(justfile,"%s\n",cur_user.bbsname);     //the BBS name of the user
                        fprintf(justfile,"%s\n",cur_user.name);        //the name of the character
                        fprintf(justfile,"%s\n",cur_user.say_win);     //what the user says when he wins
                        fprintf(justfile,"%s\n",cur_user.say_loose);   // "    "    "   "    "   "  looses
                        fprintf(justfile,"%d\n",cur_user.rank);          //user rank
                        fprintf(justfile,"%d\n",cur_user.days_not_on);     //days the user has been inactive
                        fprintf(justfile,"%d\n",cur_user.strength);        //attacking strenght of the user
                        fprintf(justfile,"%d\n",cur_user.defense);         //defensive strenght
                        fprintf(justfile,"%d\n",cur_user.condoms);         //condoms user has
                        fprintf(justfile,"%d\n",cur_user.since_got_laid);  //days since the user last got laid
                        fprintf(justfile,"%d\n",cur_user.drug_hits);       //the hist that the user has
                        fprintf(justfile,"%d\n",cur_user.drug_days_since); //if addicted how long the user
                        //has not used the drug
                        fprintf(justfile,"%ld\n",cur_user.hitpoints);       //users hitpoints
                        fprintf(justfile,"%ld\n",cur_user.maxhitpoints);    //maximum of the users hitpoints
                        fprintf(justfile,"%lu\n",cur_user.points);          //users points
                        fprintf(justfile,"%lu\n",cur_user.money);           //money in hand
                        fprintf(justfile,"%lu\n",cur_user.bank);            //money in bank
                        fprintf(justfile,"%d\n",(INT16)cur_user.level);           //user level
                        fprintf(justfile,"%d\n",(INT16)cur_user.turns);           //fight the user has left today
                        fprintf(justfile,"%d\n",(INT16)cur_user.hunger);          // % of hunger
                        fprintf(justfile,"%d\n",(INT16)cur_user.sex_today);       //sex turns left today
                        fprintf(justfile,"%d\n",(INT16)cur_user.std_percent);     // % of current std
                        fprintf(justfile,"%d\n",(INT16)cur_user.drug_addiction);  // % of drug addiction
                        fprintf(justfile,"%d\n",(INT16)cur_user.drug_high);       // % of how "high" the player is
                        fprintf(justfile,"%d\n",(INT16)cur_user.hotel_paid_fer);  //for how many more days the hotel
                        //is paid for
                        fprintf(justfile,"%d\n",(INT16)cur_user.days_in_hospital);//how many days has the use been
                        //in hospital

                        if(cur_user.alive==ALIVE)
                                fprintf(justfile,"ALIVE\n");
                        else
                                fprintf(justfile,"UNCONSIOUS\n");
                        if(cur_user.sex==MALE)
                                fprintf(justfile,"M\n");
                        else
                                fprintf(justfile,"F\n");

                        if(cur_user.nation==PUNK)
                                fprintf(justfile,"PUNK\n");
                        else if(cur_user.nation==HEADBANGER)
                                fprintf(justfile,"HEADBANGER\n");
                        else if(cur_user.nation==BIG_FAT_DUDE)
                                fprintf(justfile,"BIG FAT DUDE\n");
                        else if(cur_user.nation==CRACK_ADDICT)
                                fprintf(justfile,"CRACK ADDICT\n");
                        else
                                fprintf(justfile,"HIPPIE\n");
                        fprintf(justfile,"%d\n",(INT16)cur_user.arm);
                        if(cur_user.std==CRAPS)
                                fprintf(justfile,"CRAPS\n");
                        else if(cur_user.std==HERPES)
                                fprintf(justfile,"HERPES\n");
                        else if(cur_user.std==SYPHILIS)
                                fprintf(justfile,"SYPHILIS\n");
                        else if(cur_user.std==AIDS)
                                fprintf(justfile,"AIDS\n");
                        else
                                fprintf(justfile,"NONE\n");

                        if(cur_user.drug==POT)
                                fprintf(justfile,"POT\n");
                        else if(cur_user.drug==HASH)
                                fprintf(justfile,"HASH\n");
                        else if(cur_user.drug==LSD)
                                fprintf(justfile,"LSD\n");
                        else if(cur_user.drug==COKE)
                                fprintf(justfile,"COKE\n");
                        else if(cur_user.drug==PCP)
                                fprintf(justfile,"PCP\n");
                        else
                                fprintf(justfile,"HEROIN\n");


                        if(cur_user.rest_where==NOWHERE)
                                fprintf(justfile,"NOWHERE\n");
                        else if(cur_user.rest_where==MOTEL)
                                fprintf(justfile,"MOTEL\n");
                        else if(cur_user.rest_where==REG_HOTEL)
                                fprintf(justfile,"REGULAR HOTEL\n");
                        else
                                fprintf(justfile,"EXPENSIVE HOTEL\n");

                        fprintf(justfile,"%d\n",(INT16)cur_user.unhq);
                        fprintf(justfile,"%d\n",(INT16)cur_user.poison);
                        fprintf(justfile,"%d\n",(INT16)cur_user.rocks);
                        fprintf(justfile,"%d\n",(INT16)cur_user.throwing_ability);
                        fprintf(justfile,"%d\n",(INT16)cur_user.punch_ability);
                        fprintf(justfile,"%d\n",(INT16)cur_user.kick_ability);
                        fprintf(justfile,"%d\n",(INT16)cur_user.InterBBSMoves);

                        /*reserved for future use 3 bytes reset to 0
                                char            res1;
                                INT16           res2;*/

                        fclose(justfile);
                }
        }
}

void
call_IGM(char exenam[]) {
        //  char exenam[251];
        char exenam2[81];
        char numstr[40];
        char bbs_name[40];
        FILE *justfile;
        DWORD p_before;
        DWORD p_diff;
        INT16 x,x2;
        char *key;
        //  ffblk ffblk;
        INT16 intval;
        INT16 rankt,inact;



        ny_line(180,2,1);
        p_before=cur_user.points;
        CreateDropFile(TRUE);
        if(exenam[0]=='*') {
                for(x=1;exenam[x]!=0;x++) {
                        exenam[x-1]=exenam[x];
                }
                exenam[x-1]=0;

                for(x=0;exenam[x]!=' '&& exenam[x]!=0 && x<251;x++) {
                        exenam2[x]=exenam[x];
                }
                exenam2[x]=0;

                for(;exenam2[x]!='\\' && x>=0;x--)
                        ;

                exenam2[x+1]=0;
                if(x>2)
                        exenam2[x]=0;

                if(exenam2[0]!=0) {
#if !defined(__unix__) && !defined(__WATCOMC__)
                        if(exenam2[0]>='a' && exenam2[0]<='z')
                                setdisk(exenam2[0] - 'a');
                        else
                                setdisk(exenam2[0] - 'A');
#endif

                        chdir(exenam2);
                }
        }
        sprintf(numstr," -U%d -N%d",nCurrentUserNumber,od_control.od_node);
        strcat(exenam,numstr);
        if (od_control.baud==0)
                strcat(exenam," -L");
        //od_control.od_ker_exec=NULL;
        rankt=cur_user.rank;
        inact=cur_user.days_not_on;
        strcpy(bbs_name,cur_user.bbsname);
        od_spawn(exenam);
        od_control.od_update_status_now=TRUE;
        sprintf(numstr,"n%07d.sts",od_control.od_node);
        c_dir_g=1;
        ch_game_d();

        if(fexist(numstr)) {
                justfile=ShareFileOpen(numstr,"rb");
                if(justfile != NULL) {
                        ny_fread(&cur_user,sizeof(user_rec),1,justfile);
                        fclose(justfile);
                }
                ny_remove(numstr);
                cur_user.rank=rankt;
                cur_user.days_not_on=inact;
                strcpy(cur_user.bbsname,bbs_name);
                wrt_sts();
        } else {
                sprintf(numstr,"n%07d.stt",od_control.od_node);
                if(fexist(numstr)) {
                        justfile=ShareFileOpen(numstr,"rb");
                        if(justfile != NULL) {

                                /*        fgets(cur_user.bbsname,36,justfile);     //the BBS name of the user
                                  cur_user.bbsname[35]=0;
                                  if((key=strchr(cur_user.bbsname,'\n'))!=NULL)
                                    *key=0;
                                  if((key=strchr(cur_user.bbsname,'\r'))!=NULL)
                                    *key=0;*/

                                fgets(bbs_name,36,justfile); /*BBS name not read in no more*/

                                fgets(cur_user.name,25,justfile);     //the BBS name of the user
                                cur_user.name[24]=0;
                                if((key=strchr(cur_user.name,'\n'))!=NULL)
                                        *key=0;
                                if((key=strchr(cur_user.name,'\r'))!=NULL)
                                        *key=0;


                                fgets(cur_user.say_win,41,justfile);     //the BBS name of the user
                                cur_user.say_win[40]=0;
                                if((key=strchr(cur_user.say_win,'\n'))!=NULL)
                                        *key=0;
                                if((key=strchr(cur_user.say_win,'\r'))!=NULL)
                                        *key=0;


                                fgets(cur_user.say_loose,41,justfile);     //the BBS name of the user
                                cur_user.say_loose[40]=0;
                                if((key=strchr(cur_user.say_loose,'\n'))!=NULL)
                                        *key=0;
                                if((key=strchr(cur_user.say_loose,'\r'))!=NULL)
                                        *key=0;


                                fgets(numstr,30,justfile);
                                //sscanf(numstr,"%d",&cur_user.rank);            //user rank
                                fgets(numstr,30,justfile);
                                //sscanf(numstr,"%d",&cur_user.days_not_on);     //days the user has been inactive
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&cur_user.strength);        //attacking strenght of the user
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&cur_user.defense);         //defensive strenght
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&cur_user.condoms);         //condoms user has
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&cur_user.since_got_laid);  //days since the user last got laid
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&cur_user.drug_hits);       //the hist that the user has
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&cur_user.drug_days_since); //if addicted how long the user
                                //has not used the drug
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%ld",&cur_user.hitpoints);       //users hitpoints
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%ld",&cur_user.maxhitpoints);    //maximum of the users hitpoints
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%lu",&cur_user.points);          //users points
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%lu",&cur_user.money);           //money in hand
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%lu",&cur_user.bank);            //money in bank
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.level=intval;           //user level
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.turns=intval;           //fight the user has left today
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.hunger=intval;          // % of hunger
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.sex_today=intval;       //sex turns left today
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.std_percent=intval;     // % of current std
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.drug_addiction=intval;  // % of drug addiction
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.drug_high=intval;       // % of how "high" the player is
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.hotel_paid_fer=intval;  //for how many more days the hotel
                                //is paid for
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.days_in_hospital=intval;//how many days has the use been
                                //in hospital

                                fgets(numstr,30,justfile);
                                if(strzcmp("ALIVE",numstr)==0)
                                        cur_user.alive=ALIVE;
                                else if(strzcmp("DEAD",numstr)==0)
                                        cur_user.alive=DEAD;
                                else if(strzcmp("UNCONSIOUS",numstr)==0)
                                        cur_user.alive=UNCONCIOUS;


                                fgets(numstr,30,justfile);
                                if(strzcmp("M",numstr)==0)
                                        cur_user.sex=MALE;
                                else if(strzcmp("F",numstr)==0)
                                        cur_user.sex=FEMALE;


                                fgets(numstr,30,justfile);
                                if(strzcmp("PUNK",numstr)==0)
                                        cur_user.nation=PUNK;
                                else if(strzcmp("HEADBANGER",numstr)==0)
                                        cur_user.nation=HEADBANGER;
                                else if(strzcmp("BIG FAT DUDE",numstr)==0)
                                        cur_user.nation=BIG_FAT_DUDE;
                                else if(strzcmp("CRACK ADDICT",numstr)==0)
                                        cur_user.nation=CRACK_ADDICT;
                                else if(strzcmp("HIPPIE",numstr)==0)
                                        cur_user.nation=HIPPIE;

                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.arm=(weapon)intval;


                                fgets(numstr,30,justfile);
                                if(strzcmp("CRAPS",numstr)==0)
                                        cur_user.std=CRAPS;
                                else if(strzcmp("HERPES",numstr)==0)
                                        cur_user.std=HERPES;
                                else if(strzcmp("SYPHILIS",numstr)==0)
                                        cur_user.std=SYPHILIS;
                                else if(strzcmp("AIDS",numstr)==0)
                                        cur_user.std=AIDS;
                                else if(strzcmp("NONE",numstr)==0)
                                        cur_user.std=NONE;


                                fgets(numstr,30,justfile);
                                if(strzcmp("POT",numstr)==0)
                                        cur_user.drug=POT;
                                else if(strzcmp("HASH",numstr)==0)
                                        cur_user.drug=HASH;
                                else if(strzcmp("LSD",numstr)==0)
                                        cur_user.drug=LSD;
                                else if(strzcmp("COKE",numstr)==0)
                                        cur_user.drug=COKE;
                                else if(strzcmp("PCP",numstr)==0)
                                        cur_user.drug=PCP;
                                else if(strzcmp("HEROIN",numstr)==0)
                                        cur_user.drug=HEROIN;



                                fgets(numstr,30,justfile);
                                if(strzcmp("MOTEL",numstr)==0)
                                        cur_user.rest_where=MOTEL;
                                else if(strzcmp("REGULAR HOTEL",numstr)==0)
                                        cur_user.rest_where=REG_HOTEL;
                                else if(strzcmp("EXPENSIVE HOTEL",numstr)==0)
                                        cur_user.rest_where=EXP_HOTEL;
                                else if(strzcmp("NOWHERE",numstr)==0)
                                        cur_user.rest_where=NOWHERE;

                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.unhq=intval;
                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.poison=intval;

                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.rocks=intval;

                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.throwing_ability=intval;

                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.punch_ability=intval;

                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.kick_ability=intval;

                                fgets(numstr,30,justfile);
                                sscanf(numstr,"%d",&intval);
                                cur_user.InterBBSMoves=intval;

                                fclose(justfile);
                        }
                        sprintf(numstr,"n%07d.stt",od_control.od_node);
                        ny_remove(numstr);
                }
        }

        sprintf(numstr,"u%07d.inf",nCurrentUserNumber);
        ny_remove(numstr);
        if(p_before>cur_user.points) {
                p_diff=p_before-cur_user.points;
                cur_user.points=p_before;
                points_loose(p_diff);
        } else if (p_before<cur_user.points) {
                p_diff=cur_user.points-p_before;
                cur_user.points=p_before;
                points_raise(p_diff);
        }
        wrt_sts();
        if(cur_user.alive!=DEAD && cur_user.drug_addiction>=100)
                Die(1);
        if(cur_user.alive!=DEAD && cur_user.hunger>=100)
                Die(2);
        if(cur_user.alive!=DEAD && cur_user.std_percent>=100)
                Die(3);
        ny_kernel();
        //od_control.od_ker_exec=ny_kernel;
        if (cur_user.alive!=ALIVE)
                od_exit(10,FALSE);
}
