/*****************************************************************************
 *
 * File ..................: v3_io.c
 * Purpose ...............: File I/O routines
 * Last modification date : 30-Sept-2000
 *
 *****************************************************************************
 * Copyright (C) 1999-2000
 *
 * Darryl Perry		FIDO:		1:211/105
 * Sacramento, CA.
 * USA
 *
 * This file is part of Virtual BBS.
 *
 * This Game is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Virtual BBS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Virutal BBS; see the file COPYING.  If not, write to the
 * Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include <fcntl.h>
#include <filewrap.h>
#include "vbbs.h"
#include "v3_defs.h"
#include "v3_io.h"
#include "v3_mci.h"
#include "v3_basic.h"
#ifdef FIXME
#include "mx_file.h"
#endif

int fexist(char *fname)
{
	FILE *fptr;
	int x;

	fptr=fopen(fname,"rb");
	if(fptr==NULL)	x=FALSE;
	else{
		x=TRUE;
		fclose(fptr);
		}
	return(x);
}

int countplyrs()
{
	int count = 0;
	// build the filename
	char path[_MAX_PATH];
	sprintf(path,"%susers.vb3",gamedir);
	// open the file
	FILE* fptr = fopen(path, "rb+");
	if (fptr)
	{
		fseek(fptr, 0L, SEEK_END);
		count = ftell(fptr) / sizeof(tUserRecord);
		fclose(fptr);
	}

	return count;
}

int saveplyr(tUserRecord *user, int usernum)
{
	FILE *fptr;
	int x=FALSE;
	char path[_MAX_PATH];

	sprintf(path,"%susers.vb3",gamedir);
	fptr=fopen(path,"rb+");
	if(fptr==NULL){
		fptr=fopen(path,"wb");
		x=fwrite(user,sizeof(tUserRecord),1,fptr);
		}
	else{
		if(fseek(fptr,(long)usernum*sizeof(tUserRecord),SEEK_SET)==0)
			x=fwrite(user,sizeof(tUserRecord),1,fptr);
		}
	fclose(fptr);
	return(x);
}

int readplyr(tUserRecord* user, int usernum)
{
	int rval = TRUE;
	char path[FILENAME_MAX];
	FILE* fptr;

	sprintf(path,"%susers.vb3",gamedir);
	fptr = fopen(path,"rb+");
	if (fptr)
	{
		if (fseek(fptr, (long)usernum*sizeof(tUserRecord), SEEK_SET) == 0)
			rval = fread(user, sizeof(tUserRecord), 1, fptr);
		else
			rval = FALSE;
		fclose(fptr);
	}
	else
	{
		rval = FALSE;
		od_printf("\r\nERROR: Unable to Read User Record!\r\n");
		memset(user, 0, sizeof(tUserRecord));
	}

	return rval;
}

int readapd()
{
	FILE *fptr;
	char ttxt[21];
	char path[_MAX_PATH];

	sprintf(path,"%sapd.dat",gamedir);
	fptr=fopen(path,"rt");
	if(fptr==NULL)
		return(100);
	else
		{
		fgets(ttxt,20,fptr);
		fclose(fptr);
		}
	return(atoi(ttxt));
}

void read_vtext(int vnum)
{
	FILE *fptr;
	int  count=0,actidx=0,actval=0;
	char string[90];
	char path[_MAX_PATH],*x;

	sprintf(path,"%smsgsv.dat",gamedir);
	if((fptr=fopen(path,"rt"))==NULL)
		return;
	else{
		while(fgets(string,81,fptr)!=NULL){
			if(string[0]=='!' && string[1]=='@' && string[2]=='#'){
				count++;
				if(count==vnum){
					fgets(string,81,fptr);
					actidx=atoi(string);
					fgets(string,81,fptr);
					actval=atoi(string);
					fgets(string,81,fptr);
					do{
						mci("\r`0E");
						mci(string);
						x=fgets(string,81,fptr);
						}while(x!=NULL && string[0]!='!' && string[1]!='@' && string[2]!='#');
					}
				}
			}
		}
	fclose(fptr);
	mci("\r");
	act_change(actidx,0,0);
	act_change(actval,0,1);
}

int count_vtext()
{
	FILE *fptr;
	int  count=0;
	char string[90];
	char path[_MAX_PATH];

	// try to open the virus data files
	sprintf(path,"%smsgsv.dat",gamedir);
	fptr = fopen(path, "rb");
	// display an error if unsuccessful
	if (!fptr)
	{
		od_set_color(L_RED, D_BLACK);
		od_printf("WARNING: unable to open: %s\n", path);
		return 0;
	}
	// count the strings in the file
	while (fgets(string,81,fptr) != NULL)
	{
		if(string[0]=='!' && string[1]=='@' && string[2]=='#')
			count++;
	}
	// close the data file
	fclose(fptr);

	return(count);
}

void read_actions(int actno)
{
	FILE *fptr;
	int  count=0,actidx=0,actval=0,done=FALSE;
	char string[90];
	char path[_MAX_PATH];
	int  next_line = 0;

	sprintf(path,"%smsgsa.dat",gamedir);
	fptr = fopen(path, "rt");
	if (fptr)
	{
		while(fgets(string,81,fptr) != NULL && !done )
		{
			if(strcmp(string,":END") == 0)
				done = TRUE;
			if(string[0] == '!' && string[1] == '@' && string[2] == '#' && !done)
			{
				count++;
				if (count == actno)
				{
					fgets(string,81,fptr);
					actidx = atoi(string);
					fgets(string,81,fptr);
					actval = atoi(string);
					fgets(string,81,fptr);
					// grab next line
					next_line = -1;
					while (next_line)
					{
						// display what we got
						stripCR(string);
						mci("`0F");
						mci(string);
						mci("~SM");
						// grab a new line
						fgets(string,81,fptr);
						// check it out
						if ((string[0] == '!' && string[1] == '@' && string[2] == '#') ||
							(string[0] == ':' && string[1] == 'E' && string[2] == 'N' && string[3] == 'D'))
							next_line = 0;
					}
				}
			}
		}
		fclose(fptr);
		mci("~SM");
		act_change(actidx, actval, 1);
	}
}

int count_actions()
{
	FILE *fptr;
	int  count=0;
	char string[90];
	char path[_MAX_PATH];

	sprintf(path,"%smsgsa.dat",gamedir);
	if ((fptr=fopen(path,"rb"))==NULL)
		return 0;
	else
	{
		while (fgets(string,81,fptr)!=NULL)
		{
			if (string[0]=='!' && string[1]=='@' && string[2]=='#')
				count++;
		}
	}

	fclose(fptr);

	return(count);
}

int count_virus()
{
	FILE *fptr;
	int  count = 0;
	char string[90];
	char path[_MAX_PATH];

	sprintf(path,"%smsgsv.dat",gamedir);
	if((fptr=fopen(path,"rb"))==NULL)
		return 0;
	else
		while(fgets(string,81,fptr)!=NULL)
			if(string[0]=='!' && string[1]=='@' && string[2]=='#')
				count++;
	fclose(fptr);
	return(count);
}

int count_cpu()
{
	FILE *fptr;
	int l1;
	char ttxt[81];
	char path[_MAX_PATH];

	sprintf(path,"%scpu.dat",gamedir);
	fptr=fopen(path,"rt");
	if(fptr==NULL){
		return(MAX_CPU);
		}
	else
		{
		l1=0;
		while(fgets(ttxt,80,fptr)!=NULL){
			stripCR(ttxt);
			strcpy(computer[l1].cpuname,ttxt);
			computer[l1].cost=((l1*l1)*100)+(l1*10);
			l1++;
			}
		if(l1>19)	l1=19;
		}
	return(l1);

}

int count_msgsr()
{
	FILE *fptr;
	int  count=0;
	char string[90];
	char path[_MAX_PATH];

	sprintf(path,"%smsgsr.dat",gamedir);
	if((fptr=fopen(path,"rb"))==NULL)
		return 0;
	else
		while(fgets(string,81,fptr)!=NULL)
			if(string[0]=='!' && string[1]=='@' && string[2]=='#')
				count++;
	fclose(fptr);
	return(count);
}

void read_msgsr(int actno)
{
	FILE *fptr;
	int  count=0;
	char string[91];
	char path[_MAX_PATH];

	sprintf(path,"%smsgsr.dat",gamedir);
	if((fptr=fopen(path,"rt"))==NULL){
		mci("~SMUnable to open msgsr.dat~SM");
		return;
		}
	else{
		while(fgets(string,91,fptr)!=NULL){
			if(strcmp(string,":END")==0)	break;
			if(string[0]=='!' && string[1]=='@' && string[2]=='#'){
				count++;
				if(count==actno){
					fgets(string,91,fptr);
					fgets(string,91,fptr);
					fgets(string,91,fptr);
					do{
						stripCR(string);
						mci(string);
						fgets(string,91,fptr);
						}while(string[0]!='!' && string[1]!='@' && string[2]!='#');
					}
				}
			}
		}
	fclose(fptr);
}

void viruslist()
{
	FILE *fptr;
	int  count,k;
	char path[_MAX_PATH];

	sprintf(path,"%svirus.dat",gamedir);

    k = rand_num(vlcnt);

    if((fptr=fopen(path,"rt"))==NULL) {
        nl();
		od_printf("Error opening VIRUS.DAT");nl();
        nl();
        return;
        }
	else {
        count=0;
		while(fgets(TS,61,fptr)!=NULL && count!=k)
			count++;
		stripCR(TS);
		str_2A(1,TS);
		text("0875");
		}
	fclose(fptr);
}

int vlcount()
{
	FILE *fptr;
	int  count=0;
	char path[_MAX_PATH];

	sprintf(path,"%svirus.dat",gamedir);

    if((fptr=fopen(path,"rt"))==NULL)
		return -1;
	else{
		while(fgets(TS,81,fptr)!=NULL)
			count++;
		}
	fclose(fptr);
    return(count);
}

void writemail(char *mfile, char *msg)
{
	FILE *fptr;
	fptr = fopen(mfile,"at");
	if (fptr == NULL)
	{
		fptr = fopen(mfile,"wt");
		if (fptr == NULL)
		{
			mci("~SM~SMERROR saving mail.");
			return;
		}
	}
	fputs(msg,fptr);
	fclose(fptr);
}

void basedir(char *retstr)
{
	int i;
	for(i=strlen(retstr)-1;i>0;i--){
		if(retstr[i]=='/'){
			retstr[i+1]='\0';
			return;
			}
		}
}

int readmaildat(mailrec *mr, int letter)
{
	FILE *fptr;
	int x=FALSE;
	char path[_MAX_PATH];

	sprintf(path,"%smail.vb3",gamedir);
	fptr=fopen(path,"rb+");
	if(fptr!=NULL) {
		if(fseek(fptr,(long)letter*sizeof(mailrec),SEEK_SET)==0)
			x=fread(mr,sizeof(mailrec),1,fptr);
		fclose(fptr);
		}

	return(x);
}

int savemaildat(mailrec *mr, int letter)
{
	FILE *fptr;
	int x=FALSE;
	char path[_MAX_PATH];

	sprintf(path,"%smail.vb3",gamedir);
	fptr=fopen(path,"rb+");
	if(fptr==NULL){
		fptr=fopen(path,"wb");
		x=fwrite(mr,sizeof(mailrec),1,fptr);
		}
	else{
		if(fseek(fptr,(long)letter*sizeof(mailrec),SEEK_SET)==0)
			x=fwrite(mr,sizeof(mailrec),1,fptr);
		}
	fclose(fptr);
	return(x);
}

#ifdef FIXME
mx_file gLockFile;
#else
int	gLockFile;
#endif

int vbbs_io_lock_game(const char* fullname)
{
	char path[FILENAME_MAX];

	// open our lock file
	sprintf(path, "%slock.vb3", gamedir);
	if ((gLockFile=open(path,O_RDWR|O_CREAT|O_BINARY,S_IREAD|S_IWRITE)) < 0)
		return -1;
	// attempt to lock it
	if (lock(gLockFile, 0, 256))
	{
		close(gLockFile);
		return -1;
	}
	// write the user rec
	char username[256];
	sprintf(username, "%s", fullname);
	write(gLockFile, &username, 256);

	return 0;
}

void vbbs_io_unlock_game()
{
	// clear username
	char username[256];
	memset(username, 0, 256);
	lseek(gLockFile, 0, SEEK_SET);
	write(gLockFile, &username, 256);
	// unlock the file
	unlock(gLockFile, 0, 256);
	// close it
	close(gLockFile);
}
