/* $Id: dpconfig.c,v 1.6 2008/01/07 04:00:03 rswindell Exp $ */

#include "ini_file.h"
#include "ciolib.h"		/* Redefinition of main() */
#include "uifc.h"

#define MAX_PLAYERS 6
#define MAX_TABLES 25

#define COMPUTER    (1<<0)
#define PASSWORD    (1<<1)

ini_style_t ini_style = {
	/* key_len */ 15, 
	/* key_prefix */ "\t", 
	/* section_separator */ "\n", 
	/* value_separarator */NULL, 
	/* bit_separator */ NULL };

ini_style_t head_ini_style = {
	/* key_len */ 15, 
	/* key_prefix */ "", 
	/* section_separator */ "\n", 
	/* value_separarator */NULL, 
	/* bit_separator */ NULL };

struct table {
	short options;
	short ante;
	short bet_limit;
	unsigned long max_total;
};

char comp_name[41];
short num_tables;
unsigned long time_allowed;
short skim;
struct table tables[MAX_TABLES];

const char *enabled="Enabled";
const char *disabled="Disabled";

uifcapi_t	uifc;

/* Read .ini, set defaults */
void read_ini(void)
{
	FILE	*inifile;
	BOOL	tmpbool;
	int		opts;
	int		table;
	char	key_name[8];

	inifile=fopen("dpoker.ini","r");
	iniReadString(inifile,NULL,"ComputerName","King Drafus",comp_name);
	num_tables=iniReadShortInt(inifile,NULL,"Tables",5);
	if(num_tables>MAX_TABLES)
		num_tables=MAX_TABLES;
	if(num_tables<1)
		num_tables=1;
	time_allowed=iniReadInteger(inifile,NULL,"TimeAllowed",86400);
	if(time_allowed>86400 || time_allowed<0)
		time_allowed=86400;
	skim=iniReadShortInt(inifile,NULL,"Skim",10);
	if(skim<0)
		skim=0;
	if(skim>50)
		skim=50;
	/*
	 * Use MAX_TABLES here to ensure defaults are set...
	 * Delete extras before saving
	 */
	for(table=0;table<MAX_TABLES;table++) {
		sprintf(key_name,"Table%d",table+1);
		opts=0;
		opts|=(iniReadBool(inifile,key_name,"ComputerPlayer",TRUE)?COMPUTER:0);
		opts|=(iniReadBool(inifile,key_name,"Password",FALSE)?PASSWORD:0);
		tables[table].options=opts;
		tables[table].ante=iniReadShortInt(inifile,key_name,"Ante",(table+1)*50);
		if(tables[table].ante < 0)
			tables[table].ante=0;
		tables[table].bet_limit=iniReadShortInt(inifile,key_name,"BetLimit",(table+1)*100);
		if(tables[table].bet_limit < 1)
			tables[table].bet_limit=1;
		tables[table].max_total=iniReadInteger(inifile,key_name,"TableLimit",(table+1)*1000);
		if(tables[table].max_total < 1)
			tables[table].max_total=1;
	}
	if(inifile!=NULL)
		fclose(inifile);
}

void write_ini(void)
{
	FILE	*inifile;
	BOOL	tmpbool;
	int		opts;
	int		table;
	char	key_name[8];
	str_list_t inilines;

	if((inifile=fopen("dpoker.ini","r"))!=NULL) {
		inilines=iniReadFile(inifile);
		fclose(inifile);
	}
	else
		inilines=strListInit();
	iniSetString(&inilines,NULL,"ComputerName",comp_name,&head_ini_style);
	if(num_tables>MAX_TABLES)
		num_tables=MAX_TABLES;
	if(num_tables<1)
		num_tables=1;
	iniSetShortInt(&inilines,NULL,"Tables",num_tables,&head_ini_style);
	if(time_allowed>86400 || time_allowed<0)
		time_allowed=86400;
	iniSetInteger(&inilines,NULL,"TimeAllowed",time_allowed,&head_ini_style);
	if(skim<0)
		skim=0;
	if(skim>50)
		skim=50;
	iniSetShortInt(&inilines,NULL,"Skim",skim,&head_ini_style);
	for(table=0;table<num_tables;table++) {
		sprintf(key_name,"Table%d",table+1);
		if(table < num_tables) {
			iniSetBool(&inilines,key_name,"ComputerPlayer",tables[table].options&COMPUTER,&ini_style);
			iniSetBool(&inilines,key_name,"Password",tables[table].options&PASSWORD,&ini_style);
			if(tables[table].ante < 0)
				tables[table].ante=0;
			iniSetShortInt(&inilines,key_name,"Ante",tables[table].ante,&ini_style);
			if(tables[table].bet_limit < 1)
				tables[table].bet_limit=1;
			iniSetShortInt(&inilines,key_name,"BetLimit",tables[table].bet_limit,&ini_style);
			if(tables[table].max_total < 1)
				tables[table].max_total=1;
			iniSetInteger(&inilines,key_name,"TableLimit",tables[table].max_total,&ini_style);
		}
		else {
			iniRemoveSection(&inilines,key_name);
		}
	}
	if((inifile=fopen("dpoker.ini","w"))!=NULL) {
		iniWriteFile(inifile,inilines);
		fclose(inifile);
	}
	strListFreeStrings(inilines);
}

void free_opts(char **opts)
{
	int i;

	for(i=0; opts[i]!=NULL && opts[i][0]; i++) {
		free(opts[i]);
	}
	if(opts[i]!=NULL)
		free(opts[i]);
}

void edit_table(int table)
{
	char	*opts[6];
	char	str[1024];
	int 	i;
	static int opt=0;
	static int bar=0;

	while(1) {
		sprintf(str,"Computer Player      %s",tables[table].options&COMPUTER?enabled:disabled);
		opts[0]=strdup(str);
		sprintf(str,"Password Protection  %s",tables[table].options&PASSWORD?enabled:disabled);
		opts[1]=strdup(str);
		sprintf(str,"Ante                 %d",tables[table].ante);
		opts[2]=strdup(str);
		sprintf(str,"Bet Limit            %d",tables[table].bet_limit);
		opts[3]=strdup(str);
		sprintf(str,"Table Limit          %d",tables[table].max_total);
		opts[4]=strdup(str);
		opts[5]=strdup("");
		sprintf(str,"Table %d Options",table+1);
		i=uifc.list(WIN_SAV|WIN_BOT|WIN_RHT|WIN_ACT,0,0,0,&opt,&bar,str,opts);
		if(i==-1) {
			free_opts(opts);
			break;
		}
		switch(i) {
			case 0:	/* Allow Computer Plater */
				tables[table].options ^= COMPUTER;
				break;
			case 1: /* Allow user to set password */
				tables[table].options ^= PASSWORD;
				break;
			case 2:	/* Ante */
				sprintf(str,"%d",tables[table].ante);
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Ante",str,4,K_EDIT|K_NUMBER)!=-1)
					tables[table].ante=atoi(str);
				if(tables[table].ante<0)
					tables[table].ante=0;
				break;
			case 3:	/* Bet Limit */
				sprintf(str,"%d",tables[table].bet_limit);
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Bet Limit",str,4,K_EDIT|K_NUMBER)!=-1)
					tables[table].bet_limit=atoi(str);
				if(tables[table].bet_limit<1)
					tables[table].bet_limit=1;
				break;
			case 4: /* Table Limit */
				sprintf(str,"%d",tables[table].max_total);
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Table Limit",str,5,K_EDIT|K_NUMBER)!=-1)
					tables[table].max_total=atoi(str);
				if(tables[table].max_total<1)
					tables[table].max_total=1;
				break;
		}
		free_opts(opts);
	}
}

int main(int argc, char **argv)
{
	char 	*opts[6];
	char 	**topts;
	char	str[1024];
	int		opt=0;
	int		topt=0;
	int		tbar=0;
	char	*YesNo[3]={"Yes","No",""};
	int		i;

	read_ini();

    memset(&uifc,0,sizeof(uifc));
	uifc.size=sizeof(uifc);
	uifc.esc_delay=25;
	if((i=uifcini32(&uifc))!=0) {
		fprintf(stderr,"uifc library init returned error %d\n",i);
		return(-1);
	}
	if(uifc.scrn("Domain Poker Configuration")) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		uifc.bail();
	}

	topts=(char **)malloc(sizeof(char *)*(MAX_TABLES+1));
	if(topts==NULL) {
		fprintf(stderr,"MALLOC() error!\r\n");
		return(1);
	}
	while(1) {
		sprintf(str,"Computer Player's Name           %s",comp_name);
		opts[0]=strdup(str);
		sprintf(str,"Number of Tables                 %d",num_tables);
		opts[1]=strdup(str);
		sprintf(str,"Time allow playing computer      %d",time_allowed);
		opts[2]=strdup(str);
		sprintf(str,"Percentage house takes from pot  %d%%",skim);
		opts[3]=strdup(str);
		opts[4]=strdup("Table configuration...");
		opts[5]=strdup("");
		switch(uifc.list(WIN_ORG|WIN_MID|WIN_ESC|WIN_ACT,0,0,0,&opt,NULL,"Main Options",opts)) {
			case -1:	/* ESC */
				i=0;
				switch(uifc.list(WIN_MID,0,0,0,&i,NULL,"Save Changes?",YesNo)) {
					case 0:
						write_ini();
					case 1:
						uifc.bail();
						return(0);
				}
				break;
			case 0:	/* Computer Name */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Computer's Name",comp_name,sizeof(comp_name)-1,K_EDIT);
				break;
			case 1:	/* Number of tables */
				sprintf(str,"%d",num_tables);
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Number of Tables",str,2,K_EDIT|K_NUMBER)!=-1)
					num_tables=atoi(str);
				if(num_tables>MAX_TABLES)
					num_tables=MAX_TABLES;
				if(num_tables<1)
					num_tables=1;
				break;
			case 2: /* Time allowed */
				sprintf(str,"%d",time_allowed);
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Time allowed computer",str,5,K_EDIT|K_NUMBER)!=-1)
					time_allowed=atoi(str);
				if(time_allowed>86400 || time_allowed<0)
					time_allowed=86400;
				break;
			case 3:	/* Skim */
				sprintf(str,"%d",skim);
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Percentage to house",str,2,K_EDIT|K_NUMBER)!=-1)
					skim=atoi(str);
				if(skim<0)
					skim=0;
				if(skim>50)
					skim=50;
				break;
			case 4:	/* Table configuration */
				while(1) {
					for(i=0;i<num_tables;i++) {
						sprintf(str,"Table %-2d (%u/%u/%u)",i+1
							,tables[i].ante,tables[i].bet_limit,tables[i].max_total);
						topts[i]=strdup(str);
					}
					topts[i]=strdup("");
					i=uifc.list(WIN_ORG|WIN_DEL|WIN_INS|WIN_ACT|WIN_DELACT|WIN_INSACT,0,0,0,&topt,&tbar,"Table Options",topts);
					if(i==-1) {
						free_opts(topts);
						break;
					}
					if(topt==num_tables)
						i=topt|MSK_INS;
					if(i&MSK_INS) {
						if(num_tables<25) {
							num_tables++;
							/* Make this one a copy of the current one in this slot, move
							 * all up one number */
							memcpy(&tables[topt+1],&tables[topt],sizeof(tables[topt])*(MAX_TABLES-topt-1));
						}
					}
					else if(i&MSK_DEL) {
						if(num_tables>1) {
							num_tables--;
							memcpy(&tables[topt],&tables[topt+1],sizeof(tables[topt])*(MAX_TABLES-topt-1));
							tables[MAX_TABLES-1].options=COMPUTER;
							tables[MAX_TABLES-1].ante=50;
							tables[MAX_TABLES-1].bet_limit=250;
							tables[MAX_TABLES-1].max_total=5000;
						}
					}
					else {
						edit_table(topt);
					}
					free_opts(topts);
				}
				break;
		}
		free_opts(opts);
	}
	return(1);
}
