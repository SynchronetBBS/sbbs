#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#include "doors.h"
#include "tplib.h"

short Posit;
long di;

/*
'NotAgain$ or Ch6$ (1) = Can Press Buttons?       - Non-space = NO
'                  (2) = Alive                    - Non-Space = NO
'                  (3) = Can Place a hit now?     - Non-Space = NO
'                  (4) = Has an Unfair Advantage  - Non-Space = YES!
'                  (5) = Can Look in WATER again  - Non-Space - NO
'                  (6) = Next availabe Flirt      - 1 to 6
'                  (789) = 3-digit "times laid"   - 0 to 999
'                  (10) = Times Killed since Last - 0 to 9
'                  (11) = Times escaped at L-12   - 0 to 5 '5=win
'                  (12) = Can try to fish?        - Non-space = NO
'                  (13) = Player is alive on another node non-space=yes
'                  (14) = Player can be a nuisance- Non-space = no
'                  (15) = Ansi room Mode: Space=Yes, Non-Space = no
'                  (16,17,18) = Times "simple" escaped
*/
short StatsIn = 0;
short NamesToDel = 0;

void Pauser(void)
{
	struct timeval to;
	
	to.tv_sec=2;
	to.tv_usec=0;
	select(0,NULL,NULL,NULL,&to);
}

int main(int argc, char **argv)
{
	FILE *file;
	char path[MAXPATHLEN];
	char str[256];
	char DATE[9];
	time_t currtime;
	char *uname;
	char *ualias;
	char midbuf[4];
	short t;
	char p;
	long l;

	ParseCmdLine(argc,argv);
	ReadConfig();
	ReadLinkTo();

	srandomdev();
	Ocls();
	Oprint("\r\n`%-------------------------------------------------\r\n");
	Oprint("`#Time Port Daily Event (tp-event)\r\n`5Performing daily routines:\r\n");
	Oprint("`%-------------------------------------------------\r\n");

	sprintf(path, "%sthedate.dat",DatPath);
	file=SharedOpen(path,"a",LOCK_EX);
	fputs(" \r\n",file);
	fclose(file);
	file=SharedOpen(path, "r", LOCK_SH);
	readline(str, sizeof(str), file);
	fclose(file);
	currtime=time(NULL);
	strftime(DATE, sizeof(DATE), "%D", localtime(&currtime));
	if(!strcmp(DATE,str))
	{
		file=SharedOpen(path,"w",LOCK_EX);
		fprintf(file,"%s\r\n",DATE);
		fclose(file);
	}

	Oprint("\r\n`5Running Player Maintenance\r\n");

	sprintf(path,"%snamelist.dat",DatPath);
	file=SharedOpen(path,"a", LOCK_EX);
	fclose(file);

	file=SharedOpen(path,"r", LOCK_SH);
	while(!feof(file))
	{
    	readline(str,sizeof(str), file);
		if(!str[0])
			break;
    	Posit ++;
		uname=strtok(str,"~");
		if(uname != NULL)
			ualias=strtok(NULL,"~");
		if(uname == NULL || ualias == NULL)
		{
			fprintf(stderr,"SysOp:  There is an error in the NAMELIST.DAT file\r\neach name line must be RealName ~AliasName\r\n");
			Pauser();
			exit(1);
		}
		RTrim(ualias);
		UCase(ualias);
    	OpenStats(ualias);
    	if(ch1[0])
		{
			Ch6[13]=' ';
			Ch6[0]=' ';
			Ch6[2]=' ';
			Ch6[4]=' ';
			Ch6[11]=' ';
			Ch6[14]=' ';
			ChRest[9]=' ';
			ChRest[11]=' ';
			ChRest[12]=' ';
			t=atoi(Mid(midbuf, Ch6, 16, 3));
			if(t<0)
				t=0;
			else
				if(t>998)
					t=0;
			sprintf(midbuf,"%-3d",t);
			Ch6[15]=midbuf[0];
			Ch6[16]=midbuf[1];
			Ch6[17]=midbuf[2];
			if(Ch6[5]<'1' || Ch6[5]>'6')
				Ch6[5]='1';
			p=Ch6[9]-47;
			if(p>=10)
			{
	        	SaveStrStats("DELETED2", ch1, ch2, ch3, ch4, ch5, Ch6, Ch7, Ch8, Ch9, Ch10, Ch11, Ch12, Ch13, Ch14, Ch15, Ch16, ch17, ChRest);
				sprintf(str,"`$%s `#vanished from the space-time continuum today.", ch1);
				WriteNews(str,1);
			}
			else
				Ch6[9]=p+48;
			if(Ch6[9]<='0' || Ch6[9] > '9')
				Ch6[9]='0';
			if(strlen(Ch16)<1 || Ch16[0]==' ')
				strcpy(Ch16,"Bruiser");
			sprintf(Ch13,"%d",MaxClosFight);
			sprintf(Ch14,"%d",MaxPlayFight);
			if((random()%15)==1)
			{
				strcpy(ch17, "badtimes");
				sprintf(Ch16, "R%ld N*", (random()%MaxBadTimes+1));
			}
			ChRest[0]=50+random()%3;
			if(Regis)
			{
				l=atoi(ch3)+atoi(ch3)/10;
				sprintf(ch3,"%ld",l);
			}
			if(strcmp(Ch12, DATE) && Ch6[1]=='#')
			{
            	Ch6[1]=' ';
				sprintf(Ch9,"%ld",PlayerHP[atoi(Ch11)]);
			}
			if(strcmp(Ch12, DATE) && Ch6[1] != ' ')
			{
				Ch6[1]='#';
        	}
			l=atoi(Ch15);
			if(l<0)
			{
				Ch15[0]='0';
				Ch15[1]=0;
			}
        	SaveStrStats(ch1, ch1, ch2, ch3, ch4, ch5, Ch6, Ch7, Ch8, Ch9, Ch10, Ch11, Ch12, Ch13, Ch14, Ch15, Ch16, ch17, ChRest);
		}
		else
        	SaveStrStats("DELETED2", ch1, ch2, ch3, ch4, ch5, Ch6, Ch7, Ch8, Ch9, Ch10, Ch11, Ch12, Ch13, Ch14, Ch15, Ch16, ch17, ChRest);
	}
	fclose(file);

	Oprint("`5Deleting Inactive Players `8(If any)\r\n");
	DelOldFromNameList();

	unlink(Yesterday);
	rename(Today, Yesterday);
	PickRandom(str,"daily.txt");
	WriteNews(str,1);
	ExitGame();
	return(0);
}

void UseItem(long c,short b) {}

void SaveGame(void) {}
