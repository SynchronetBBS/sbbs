//Include the header file
#include "jackpot.h"

//fuction prototypes
DWORD get_val(DWORD def, DWORD max);
//ny2008 value entering system

void strzcpy(char dest[],const char src[], INT16 beg,INT16 end);
//copy characters from a middle of a string

char *ny_un_emu(const char line[],char out[]);
//remove color codes from line[] and out put to out[]

int main(int argc,char *argv[]);
// main fuction

int main(int argc,char *argv[]) {
	FILE *file_handle;          //file handle
	char key;                   //single character for menu choice
	char *char_in_string;       //character pointer used in terminating strings
	WORD money;         //money that was bet
	DWORD jackpot;      //the jackpot
	user_rec urec;              //user record
	WORD chance;        //the chance variable ... will store a random num
	WORD max_bet;       //maximum bet
	WORD set_ch=20000;  //average winable jackpot
	INT16 intval;                 //temporary int variable
	char tmp_str_1[61],tmp_str_2[35];//temporary strings
	char dfile1[74],dfile2[74]; //dropfile names
	INT16 ans_t,avt_t;            //ANSI?,AVATAR? Boolean values
	INT16 x,x2;                   //for loop counters
	char path[60];              //path to drop files
	INT16 user_num;               //user number


	//registration (enter you opendoors reg key here)
	strcpy(od_registered_to,"Nobody");
	od_registration_key=0000000000000;

	//do not use direct video writes
	// directvideo=0;

	//include config file
	od_control.od_config_file = INCLUDE_CONFIG_FILE;
	od_control.od_config_filename = "jackpot.cfg";

	//include multiple personality system
	od_control.od_mps=INCLUDE_MPS;
	od_control.od_nocopyright=TRUE;

	//there was not enough arguments passed
	if(argc<4) {
		printf("\n\r\n\rJACKPOT:Not enough arguments\n\r\n\r");
		exit(12);
	}

	//argument #1 is the path to the drop files
	strcpy(path,argv[1]);

	//get the second argument
	x=2;

	//read all arguments and act accordingly
	do {

		//game is local (NY2008's generic argument)
		if (strnicmp(argv[x],"-L",2)==0) {
			od_control.od_force_local=TRUE;

			//node number (NY2008's generic argument)
		} else if (strnicmp(argv[x],"-N",2)==0) {
			strzcpy(tmp_str_1,argv[x],2,59);
			sscanf(tmp_str_1,"%d",&intval);
			od_control.od_node=intval;

			//average jackpot winable size
		} else if (strnicmp(argv[x],"-J",2)==0) {
			strzcpy(tmp_str_1,argv[x],2,59);
			sscanf(tmp_str_1,"%u",&set_ch);

			// user number (NY2008's generic argument)
		} else if (strnicmp(argv[x],"-U",2)==0) {
			strzcpy(tmp_str_1,argv[x],2,59);
			sscanf(tmp_str_1,"%d",&user_num);

			//use direct writes
		} else if (strnicmp(argv[x],"-DV",3)==0) {
			// directvideo=1;

			//use a different .CFG file
		} else if (strnicmp(argv[x],"-C",2)==0) {
			strzcpy(od_control.od_config_filename,argv[x],2,59);
		}

		//loop for all arguments
	} while ((++x)<=argc);

	//do not read any drop files (will be read manually this is for odoors)
	od_control.od_disable|=DIS_INFOFILE;

	//get length of path and check weather it includes a trailing slash
	// if not then add it
	x=strlen(path);
	if (path[x-1]!='\\') {
		path[x]='\\';
		path[x+1]=0;
	}

	//create drop file names
	sprintf(dfile1,"%su%07d.inf",path,user_num);
	sprintf(dfile2,"%sn%07d.sts",path,od_control.od_node);

	//if one or both are not found exit
	if(!fexist(dfile1) && !fexist(dfile2)) {
		printf("\n\r\n\rJACKPOT:Game drop file(s) not found!\n\r\n\r");
		exit(12);
	}

	//open .inf file
	file_handle=fopen(dfile1,"rt");

	//get first line (bbs dropfile path)
	fgets(od_control.info_path,59,file_handle);
	//get rid of the \n char in the string and make it 0
	if((char_in_string=strchr(od_control.info_path,'\n'))!=0)
		*char_in_string=0;

	//get second line (time limit)
	fgets(tmp_str_1,30,file_handle);
	//read in the value
	sscanf(tmp_str_1,"%d",&od_control.caller_timelimit);

	//comport
	fgets(tmp_str_1,30,file_handle);
	sscanf(tmp_str_1,"%d",&od_control.port);

	//baud rate
	fgets(tmp_str_1,30,file_handle);
	sscanf(tmp_str_1,"%lu",&od_control.baud);

	//terminal emulation
	fgets(tmp_str_1,30,file_handle);
	//get rid of the \n char in the string and make it 0
	if((char_in_string=strchr(tmp_str_1,'\n'))!=0)
		*char_in_string=0;

	//set avt_t and ans_t according to tmp_str_1
	//ansi
	if(strcmp(tmp_str_1,"ANSI")==0) {
		ans_t=TRUE;
		avt_t=FALSE;
		//avatar
	} else if(strcmp(tmp_str_1,"AVATAR")==0) {
		ans_t=TRUE;
		avt_t=TRUE;
		//ascii
	} else {
		ans_t=FALSE;
		avt_t=FALSE;
	}

	//get players location
	fgets(od_control.user_location,25,file_handle);
	//get rid of the \n char in the string and make it 0
	if((char_in_string=strchr(od_control.user_location,'\n'))!=0)
		*char_in_string=0;

	//is fossil used?
	fgets(tmp_str_1,30,file_handle);
	//get rid of the \n char in the string and make it 0
	if((char_in_string=strchr(tmp_str_1,'\n'))!=0)
		*char_in_string=0;

	//fossil used
	if(strcmp(tmp_str_1,"FOSSIL")==0) {
		od_control.od_com_method=COM_FOSSIL;

		//fossil not used read the comm settings
	} else {
		od_control.od_com_method=COM_INTERNAL;

		//get com address
		fgets(tmp_str_1,30,file_handle);
		sscanf(tmp_str_1,"%d",&od_control.od_com_address);

		//get irq
		fgets(tmp_str_1,30,file_handle);
		sscanf(tmp_str_1,"%d",&intval);
		od_control.od_com_irq=intval;

		//use FIFO?
		fgets(tmp_str_1,30,file_handle);
		//get rid of the \n char in the string and make it 0
		if((char_in_string=strchr(tmp_str_1,'\n'))!=0)
			*char_in_string=0;

		//no fifo
		if(strcmp(tmp_str_1,"NOFIFO")==0)
			od_control.od_com_no_fifo=TRUE;

		//use fifo
		else
			od_control.od_com_no_fifo=FALSE;

		//fifo trigger size
		fgets(tmp_str_1,30,file_handle);
		sscanf(tmp_str_1,"%d",&intval);
		od_control.od_com_fifo_trigger=intval;

		//recieve buffer
		fgets(tmp_str_1,30,file_handle);
		sscanf(tmp_str_1,"%u",&od_control.od_com_rx_buf);

		//transmit buffer
		fgets(tmp_str_1,30,file_handle);
		sscanf(tmp_str_1,"%u",&od_control.od_com_tx_buf);
	}

	//close the .inf file
	fclose(file_handle);

	//open the .sts file
	file_handle=fopen(dfile2,"rb");

	//read in the user record
	fread(&urec,sizeof(user_rec),1,file_handle);

	//close the .sts file
	fclose(file_handle);

	//destroy emulation codes and put it in tmp_str_2
	ny_un_emu(urec.name,tmp_str_2);

	//store this in user name so that the handle will display first and
	//bbsname second. If you read any bbs drop files be sure to change it
	//back before you exit.
	sprintf(tmp_str_1,"%s (%s)",tmp_str_2,urec.bbsname);
	strcpy(od_control.user_name,tmp_str_1);

	//display the copyright message (it won't really show but it will be there)
	od_printf("`bright`New York 2008 Jackpot IGM, FREEWARE!\n\r(c) Copyright 1995, George Lebl - All rights Reserved\n\r");

	//se the ansi and avatar values
	od_control.user_ansi=ans_t;
	od_control.user_avatar=avt_t;

	//check if another node is using the IGM
	if(!fexist("jackpot.usd")) {
		file_handle=fopen("jackpot.usd","wt");
		fclose(file_handle);

		//if it is exit
	} else {
		od_printf("\n\r`bright red`T`red`he IGM is used by another node!\n\r`bright red`T`red`his IGM is not multinode capable!\n\r`bright red`C`red`ome back later!\n\r\n\r`bright red`Smack [Enter] to countinue:");
		od_get_answer("\n\r");
		od_exit(10,FALSE);
	}

	//check if the dat file is there
	//if not create it and make the jackpot 100
	if(!fexist("jackpot.dat")) {
		jackpot=100;
		file_handle=fopen("jackpot.dat","wt");
		fprintf(file_handle,"100");
		fclose(file_handle);

		//if yes read in the value
	} else {
		file_handle=fopen("jackpot.dat","rt");
		fscanf(file_handle,"%lu",&jackpot);
		fclose(file_handle);
	}

	//this is where the main menu is drawn
main_menu:

	//if the user would not have clear clearing it will push the menu a bit down
	od_printf("\n\r\n\r");

	//clear screen
	od_clr_scr();

	//display the menu
	od_printf("`bright red`J`red`ackpot IGM for `bright green`NY2008`red`!\n\r\n\r");
	od_printf("`bright white`The jackpot is: `flashing bright red`%lu\n\r\n\r",jackpot);
	od_printf("`bright red`B `red`- `bright red`B`red`et\n\r");
	od_printf("`bright red`Q `red`- `bright red`Q`red`uit\n\r\n\r");
	od_printf("`bright blue`E`blue`nter `bright blue`Y`blue`er `bright blue`C`blue`ommand (`bright blue`%d `blue`mins)`bright blue`>`bright`",od_control.caller_timelimit);

	//get the user response
	key=od_get_answer("BQ");

	//display it
	od_putch(key);

	//if user chose to quit .. quit
	if(key=='Q') {
		od_printf("\n\r\n\r`bright red`L`red`eaving the `flashing bright red`JACKPOT\n\r\n\r");

		//the game is not used anymore
		remove
			("jackpot.usd");
		od_exit(10,FALSE);

		//if user is betting
	} else {

		//warn about no money
		if(urec.money==0)
			od_printf("\n\r\n\r`bright red`Y`red`a ain't got no money on hand!");

		//because the highest bet is half the average jackpot win size
		//make it that unless the user's got less money
		if(urec.money>(INT16)(set_ch/2))
			chance=(INT16)(set_ch/2);
		else
			chance=urec.money;

		//display prompt
		od_printf("\n\r\n\r`bright green`H`green`ow much`green` (`bright blue`Enter`green`=`bright green`[0]`green`, `bright blue`M`green`=max `bright green`%u`green`):",chance);

		//use the ny value entering system to get the value
		money=get_val(0,chance);

		//if user bet 0 goto mainmenu
		if(money==0)
			goto main_menu;

		//seed the random number generator
#ifdef ODPLAT_NIX

		srandomdev();
#else

		randomize();
#endif

		//make it look like there's something going on
		od_printf("\n\r\n\r`bright red`I`red`t's turning - `bright`");

		//turn around 3 digit numbers for a while
		for(x=0;x<20;x++) {
			od_printf("%3d\b\b\b",xp_random(1000));
			for(x2=0;x2<200;x2++)
				od_kernel();
		}

		// add the money betted to jackpot
		jackpot+=money;

		//take off users money that he bet
		urec.money-=money;

		//set the chance of winning
		chance=(WORD)xp_random(set_ch/2) * 2;

		//check if user won
		if(chance<money) {

			//put WON over the 3 digit numbers
			od_printf("WON");

			//take a 10% tax
			jackpot=(jackpot*90)/100;

			//display that user had won
			od_printf("\n\r\n\r`bright red`Y`red`ou have won %lu!\n\r`bright red`Y`red`ou had to pay 10\% tax though!",jackpot);

			/*****this routine adds money but checks for overflow*****/
			/**/    DWORD med;                                     /**/
			/**/                                                   /**/
			/**/    med=ULONG_MAX-jackpot;                         /**/
			/**/    if (med<=urec.money)                           /**/
			/**/      urec.money=ULONG_MAX;                        /**/
			/**/    else                                           /**/
			/**/      urec.money+=jackpot;                         /**/
			/*********************************************************/

			//display the enter prompt
			od_printf("\n\r\n\r`bright red`Smack [Enter] to continue:");

			//wait for enter
			od_get_answer("\n\r");

			//open the jackpot dat file
			file_handle=fopen("jackpot.dat","wt");

			//put a hundred in
			fprintf(file_handle,"100");

			//close the file
			fclose(file_handle);

			//display leaving message
			od_printf("\n\r\n\r`bright red`L`red`eaving the `flashing bright red`JACKPOT\n\r\n\r");

			//others can use the igm
			remove
				("jackpot.usd");

			//write out the user record into the .sts file for NY2008 to read
			file_handle=fopen(dfile2,"wb");
			fwrite(&urec,sizeof(user_rec),1,file_handle);
			fclose(file_handle);

			//exit
			od_exit(10,FALSE);

			//if user lost
		} else {

			//put LOST over the 3 digit numbers
			od_printf("LOST");

			//display message
			od_printf("\n\r\n\r`bright red`Y`red`ou have `bright`LOST!");
			od_printf("\n\r\n\r`bright red`Smack [Enter] to continue:");

			//wait for enter
			od_get_answer("\n\r");

			//write the jackpot into the data file
			file_handle=fopen("jackpot.dat","wt");
			fprintf(file_handle,"%lu",jackpot);
			fclose(file_handle);

			//exitting message
			od_printf("\n\r\n\r`bright red`L`red`eaving the `flashing bright red`JACKPOT\n\r\n\r");

			//others can use the igm
			remove
				("jackpot.usd");

			//write out the .sts for ny2008 to read
			file_handle=fopen(dfile2,"wb");
			fwrite(&urec,sizeof(user_rec),1,file_handle);
			fclose(file_handle);

			//exit
			od_exit(10,FALSE);
		}
	}
	return(0);
}

DWORD    //value input system
get_val(DWORD def, DWORD max) {
	//this function gets a number from the user just like ny2008 does
	//max is the maximim value user can enter
	//def is the default value if user presses enter

	char input_s[30];         //input string
	DWORD intval=0;   //temp value
	INT16 cnt=0;                //loop counter


Again:                    //do again if neccesary

	//set input string to spaces
	memset(input_s,' ',strlen(input_s));

	//get first digit
	input_s[0]= od_get_answer("0123456789M\n\r");

	//user wants maximum
	if (input_s[0]=='M') {
		od_printf("%lu\n\r",max);
		return(max);
	}

	//user wants default
	else if (input_s[0]=='\n' || input_s[0]=='\r') {
		od_printf("%lu\n\r",def);
		return(def);
	}

	//print out the digit
	od_printf("%c",input_s[0]);

	//get all other digits
	cnt=0;
	while (1) {
		//only 30 digits allowed
		if (cnt<29)
			cnt++;
		else
			od_printf("\b");

		//get a digit
		input_s[cnt]=od_get_answer("0123456789\n\r\b");

		//if enter that is the value we want
		if (input_s[cnt]=='\n' || input_s[cnt]=='\r') {
			input_s[cnt]=' ';
			sscanf(input_s,"%lu",&intval);

			//the value entered was more than the max value so truncate it
			if (intval>max) {
				do {
					od_printf("\b \b");
					cnt--;
				} while (cnt>0);
				intval=max;
				od_printf("%lu",max);
			}

			//print an enter
			od_printf("\n\r");

			//get out of the loop
			break;
		}

		//print the digit
		od_printf("%c",input_s[cnt]);

		//it was backspace
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
	//if all deleted goto the beginning
	if (cnt == -1)
		goto Again;

	//return the value
	return(intval);
}

/*copy end chars beginning from beg to dest*/
/*similiar to strncpy*/
void
strzcpy(char dest[],const char src[], INT16 beg,INT16 end) {
	//dest is the source, src is the cource

	INT16 cnt=0; //loop counter

	//begin loop
	do {
		//copy
		dest[cnt]=src[beg];
		beg++;
		cnt++;
		//until end characters are copied
	} while (cnt<=end && src[cnt]!=0);

	//end the string
	dest[cnt]=0;
}

//remove the ny color codes from a string
char
*ny_un_emu(const char line[],char out[]) {
	//line is input, out is output

	INT16 cnt;   //real pos
	INT16 len;   //pos in out

	len=0; //set out pos to 0

	//loop for every char in line
	for(cnt=0;line[cnt]!=0;cnt++) {

		//if '`' found remove color code
		if(line[cnt]=='`') {
			cnt++;

			//end of string
			if(line[cnt]==0) {
				out[len]=0;
				return(out);
				//not a color code but a single '`'
			} else if(line[cnt]=='`') {
				out[len]='`';
				len++;
			}

			//'`' not found
		} else {
			out[len]=line[cnt];
			len++;
		}
	}

	//end the string
	out[len]=0;

	//return the pointer to out
	return(out);
}
