/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "scfgdefs.h"
#include "str_util.h"
#include "load_cfg.h"
#include "smblib.h"
#include "nopen.h"
#include "crc32.h"
#include <stdarg.h>

#define DUPEFIND_VER "3.19"

char* crlf="\r\n";

void bail(int code)
{
	exit(code);
}

long lputs(char *str)
{
    char tmp[256];
    int i,j,k;

	j=strlen(str);
	for(i=k=0;i<j;i++)      /* remove CRs */
		if(str[i]==CR && str[i+1]==LF)
			continue;
		else
			tmp[k++]=str[i];
	tmp[k]=0;
	return(fputs(tmp,stderr));
}

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(const char *fmat, ...)
{
	va_list argptr;
	char sbuf[256];
	int chcount;

	va_start(argptr,fmat);
	chcount=vsprintf(sbuf,fmat,argptr);
	va_end(argptr);
	lputs(sbuf);
	return(chcount);
}

char *display_filename(scfg_t *cfg, uint dirnum, uint32_t fil_off)
{
	static char str[256];
	static smb_t smb;
	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return smb.last_error;
	smb_fseek(smb.sid_fp, (fil_off - 1) * sizeof(fileidxrec_t), SEEK_SET);
	fileidxrec_t idx;
	if(smb_fread(&smb, &idx, sizeof(idx), smb.sid_fp) != sizeof(idx)) {
		smb_close(&smb);
		return smb.last_error;
	}
	smb_close(&smb);
	SAFECOPY(str, idx.name);
	return str;
}

int main(int argc,char **argv)
{
	char str[256], *p;
	uint32_t **fcrc,*foundcrc;
	ulong total_found=0L;
	ulong g;
	uint i,j,k,h,start_lib=0,end_lib=0,found=-1;
	scfg_t cfg;

	setvbuf(stdout,NULL,_IONBF,0);

	char revision[16];
	sscanf("$Revision: 1.54 $", "%*s %s", revision);
	fprintf(stderr,"\nDUPEFIND v%s-%s (rev %s) - Synchronet Duplicate File "
		"Finder\n", DUPEFIND_VER, PLATFORM_DESC, revision);

    p = get_ctrl_dir(/* warn: */TRUE);

	if(argc>1 && (!stricmp(argv[1],"/?") || !stricmp(argv[1],"?") || !stricmp(argv[1],"-?"))) {
		fprintf(stderr,"\n");
		fprintf(stderr,"usage: %s [start] [end]\n", argv[0]);
		fprintf(stderr,"where: [start] is the starting library number to check\n");
		fprintf(stderr,"       [end]   is the final library number to check\n");
		return(0); 
	}

	memset(&cfg,0,sizeof(cfg));
	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir,p);

	if(!load_cfg(&cfg, /* text: */NULL, /* prep: */TRUE, /* node: */FALSE, str, sizeof(str))) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",str);
		return(1);
	}
	(void)chdir(cfg.ctrl_dir);

	lputs("\n");

	start_lib=0;
	end_lib=cfg.total_libs-1;
	if(argc>1)
		start_lib=end_lib=atoi(argv[1])-1;
	if(argc>2)
        end_lib=atoi(argv[2])-1;

	if((fcrc = malloc(cfg.total_dirs*sizeof(uint32_t *)))==NULL) {
		printf("Not enough memory for CRCs.\r\n");
		return(1); 
	}
	memset(fcrc, 0, cfg.total_dirs*sizeof(uint32_t *));

	for(i=0;i<cfg.total_dirs;i++) {
		if(cfg.dir[i]->lib < start_lib || cfg.dir[i]->lib > end_lib)
			continue;
		printf("Reading directory %u of %u\r",i+1,cfg.total_dirs);
		smb_t smb;
		int result = smb_open_dir(&cfg, &smb, i);
		if(result != SMB_SUCCESS) {
			fprintf(stderr, "!ERROR %d (%s) opening %s\n"
				,result, smb.last_error, smb.file);
			continue;
		}
		if(smb.status.total_files < 1) {
			smb_close(&smb);
			continue;
		}
		if((fcrc[i] = malloc((smb.status.total_files + 1) * sizeof(uint32_t)))==NULL) {
            printf("Not enough memory for CRCs.\r\n");
            return(1); 
		}
		j=0;
		fcrc[i][j++] = smb.status.total_files;
		rewind(smb.sid_fp);
		while(!feof(smb.sid_fp)) {
			fileidxrec_t idx;
			if(smb_fread(&smb, &idx, sizeof(idx), smb.sid_fp) != sizeof(idx))
				break;
			char filename[sizeof(idx.name) + 1];
			SAFECOPY(filename, idx.name);
			fcrc[i][j++]=crc32(filename, 0);
		}
		smb_close(&smb);
	}
	lputs("\n");

	foundcrc = NULL;
	for(i=0;i<cfg.total_dirs;i++) {
		if(cfg.dir[i]->lib<start_lib || cfg.dir[i]->lib>end_lib)
			continue;
		lprintf("Scanning %s %s\n",cfg.lib[cfg.dir[i]->lib]->sname,cfg.dir[i]->sname);
		if(fcrc[i] == NULL)
			continue;
		for(k=1;k<fcrc[i][0];k++) {
			for(j=i+1;j<cfg.total_dirs;j++) {
				if(cfg.dir[j]->lib<start_lib || cfg.dir[j]->lib>end_lib)
					continue;
				if(fcrc[j] == NULL)
					continue;
				for(h=1;h<fcrc[j][0];h++) {
					if(fcrc[i][k]==fcrc[j][h]) {
						if(found!=k) {
							found=k;
							for(g=0;g<total_found;g++) {
								if(foundcrc[g]==fcrc[i][k])
									break; 
							}
							if(g==total_found) {
								++total_found;
								if((foundcrc = realloc(foundcrc
									,total_found*sizeof(uint32_t)))==NULL) {
									printf("Out of memory reallocating\r\n");
									return(1); 
								} 
							}
							else
								found=0;
							printf("\n%-12s is located in : %-*s  %s\n"
								   "%-12s           and : %-*s  %s\n"
								,display_filename(&cfg,i,k)
								,LEN_GSNAME
								,cfg.lib[cfg.dir[i]->lib]->sname
								,cfg.dir[i]->sname
								,""
								,LEN_GSNAME
								,cfg.lib[cfg.dir[j]->lib]->sname
								,cfg.dir[j]->sname
								); 
						}
						else
							printf("%-12s           and : %-*s  %s\n"
								,""
								,LEN_GSNAME
								,cfg.lib[cfg.dir[j]->lib]->sname
								,cfg.dir[j]->sname
								); 
					} 
				} 
			} 
		} 
	}
	return(0);
}

