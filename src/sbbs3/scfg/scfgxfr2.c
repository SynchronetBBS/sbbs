/* $Id: scfgxfr2.c,v 1.62 2019/08/12 06:21:28 rswindell Exp $ */

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
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "scfg.h"
#include <stdbool.h>

#define DEFAULT_DIR_OPTIONS (DIR_FCHK|DIR_MULT|DIR_DUPES|DIR_CDTUL|DIR_CDTDL|DIR_DIZ)
#define CUT_LIBNUM	USHRT_MAX

static bool new_dir(unsigned new_dirnum, unsigned libnum)
{
	dir_t* new_directory = malloc(sizeof(dir_t));
	if (new_directory == NULL) {
		errormsg(WHERE, ERR_ALLOC, "directory", sizeof(*new_directory));
		return false;
	}
	memset(new_directory, 0, sizeof(*new_directory));
	new_directory->lib = libnum;
	new_directory->maxfiles = MAX_FILES;
	new_directory->misc = DEFAULT_DIR_OPTIONS;
	new_directory->up_pct = cfg.cdt_up_pct;
	new_directory->dn_pct = cfg.cdt_dn_pct;

	/* Use last dir in lib (if exists) as a template for new dirs */
	for (unsigned u = 0; u < cfg.total_dirs; u++) {
		if(cfg.dir[u]->lib == libnum) {
			*new_directory = *cfg.dir[u];
			new_directory->misc &= ~DIR_TEMPLATE;
			if(cfg.dir[u]->misc & DIR_TEMPLATE)	/* Use this dir (not last) if marked as template */
				break;
		}
	}

	dir_t** new_dir_list;
	if ((new_dir_list = (dir_t **)realloc(cfg.dir, sizeof(dir_t *)*(cfg.total_dirs + 1))) == NULL) {
		free(new_directory);
		errormsg(WHERE, ERR_ALLOC, "directory list", cfg.total_dirs + 1);
		return false;
	}
	cfg.dir = new_dir_list;

	/* Move higher numbered dirs (for inserting) */
	for (unsigned u = cfg.total_dirs; u > new_dirnum; u--)
		cfg.dir[u] = cfg.dir[u - 1];

	new_directory->dirnum = new_dirnum;
	cfg.dir[new_dirnum] = new_directory;
	cfg.total_dirs++;
	return true;
}

static bool new_lib(unsigned new_libnum)
{
	lib_t* new_library = malloc(sizeof(lib_t));
	if (new_library == NULL) {
		errormsg(WHERE, ERR_ALLOC, "library", sizeof(lib_t));
		return false;
	}
	memset(new_library, 0, sizeof(*new_library));

	lib_t** new_lib_list;
	if ((new_lib_list = (lib_t **)realloc(cfg.lib, sizeof(lib_t *)*(cfg.total_libs + 1))) == NULL) {
		free(new_library);
		errormsg(WHERE, ERR_ALLOC, "library list", cfg.total_libs + 1);
		return false;
	}
	cfg.lib = new_lib_list;

	for (unsigned u = cfg.total_libs; u > new_libnum; u--)
		cfg.lib[u] = cfg.lib[u - 1];
	
	if (new_libnum != cfg.total_libs) {	/* Inserting library? Renumber (higher) existing libs */
		for (unsigned j = 0; j < cfg.total_dirs; j++) {
			if (cfg.dir[j]->lib >= new_libnum && cfg.dir[j]->lib != CUT_LIBNUM)
				cfg.dir[j]->lib++;
		}
	}
	cfg.lib[new_libnum] = new_library;
	cfg.total_libs++;
	return true;
}

static void append_dir_list(const char* parent, const char* dir, FILE* fp, int depth, int max_depth, bool include_empty_dirs)
{
	char		path[MAX_PATH+1];
	char*		p;
	glob_t		g;
	unsigned	gi;

	SAFECOPY(path,dir);
	backslash(path);
	strcat(path,ALLFILES);

	glob(path,GLOB_MARK,NULL,&g);
   	for(gi=0;gi<g.gl_pathc;gi++) {
		if(*lastchar(g.gl_pathv[gi])=='/') {
			if(include_empty_dirs ||
				getdirsize(g.gl_pathv[gi], /* include_subdirs */ FALSE, /* subdir_only */FALSE) > 0) {
				SAFECOPY(path,g.gl_pathv[gi]+strlen(parent));
				p=lastchar(path);
				if(IS_PATH_DELIM(*p))
					*p=0;
				fprintf(fp,"%s\n",path);
			}
			if(max_depth==0 || depth+1 < max_depth) {
				append_dir_list(parent, g.gl_pathv[gi], fp, depth+1, max_depth, include_empty_dirs);
			}
		}
	}
	globfree(&g);
}

BOOL create_raw_dir_list(const char* list_file)
{
	char		path[MAX_PATH+1];
	char*		p;
	int			k=0;
	bool		include_empty_dirs;
	FILE*		fp;

	SAFECOPY(path, list_file);
	if((p=getfname(path))!=NULL)
		*p=0;
	if(uifc.input(WIN_MID|WIN_SAV,0,0,"Parent Directory",path,sizeof(path)-1
		,K_EDIT)<1)
		return(FALSE);
	k=1;
	k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Include Empty Directories",uifcYesNoOpts);
	if(k<0)
		return(FALSE);
	include_empty_dirs = (k == 0);
	k=0;
	k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Recursive Directory Search",uifcYesNoOpts);
	if(k<0)
		return(FALSE);
	if((fp=fopen(list_file,"w"))==NULL) {
		SAFEPRINTF2(path,"Create Failure (%u): %s", errno, list_file);
		uifc.msg(path);
		return(FALSE); 
	}
	backslash(path);
	uifc.pop("Scanning Directories...");
	append_dir_list(path, path, fp, /* depth: */0, /* max_depth: */k, include_empty_dirs);
	uifc.pop(NULL);
	fclose(fp);
	return(TRUE);
}

unsigned dirs_in_lib(unsigned libnum)
{
	unsigned total = 0;

	for(unsigned u = 0; u < cfg.total_dirs; u++)
		if(cfg.dir[u]->lib == libnum)
			total++;
	return total;
}

void xfer_cfg()
{
	static int libs_dflt,libs_bar,dflt;
	char str[256], done=0, *p;
	char tmp_code[MAX_PATH+1];
	int file,j,k,q;
	uint i;
	long ported,added;
	static lib_t savlib;
	dir_t tmpdir;
	FILE *stream;

	char* lib_long_name_help =
		"`Library Long Name:`\n"
		"\n"
		"This is a description of the file library which is displayed when a\n"
		"user of the system uses the `/*` command from the file transfer menu.\n"
		;
	char* lib_short_name_help =
		"`Library Short Name:`\n"
		"\n"
		"This is a short description of the file library which is used for the\n"
		"file transfer menu prompt.\n"
		;
	char* lib_code_prefix_help =
		"`Internal Code Prefix:`\n"
		"\n"
		"This is an `optional`, but highly recommended `code prefix` used to help\n"
		"generate `unique` internal codes for the directories in this file library.\n"
		"When code prefixes are used, directory `internal codes` will be\n"
		"constructed from a combination of the prefix and the specified code\n"
		"suffix for each directory.\n"
		"\n"
		"Code prefixes may contain up to 8 legal filename characters.\n"
		"\n"
		"Code prefixes should be unique among the file libraries on the system.\n"
		"\n"
		"Changing a library's code prefix implicitly changes all the internal\n"
		"code of the directories within the library, so change this value with\n"
		"caution.\n"
		;

	while(1) {
		for(i=0;i<cfg.total_libs && i<MAX_OPTS;i++)
			sprintf(opt[i],"%-*s %5u", LEN_GLNAME, cfg.lib[i]->lname, dirs_in_lib(i));
		opt[i][0]=0;
		j=WIN_ACT|WIN_CHE|WIN_ORG;
		if(cfg.total_libs)
			j|=WIN_DEL|WIN_COPY|WIN_CUT|WIN_DELACT;
		if(cfg.total_libs<MAX_OPTS)
			j|=WIN_INS|WIN_INSACT|WIN_XTR;
		if(savlib.sname[0])
			j|=WIN_PASTE | WIN_PASTEXTR;
		uifc.helpbuf=
			"`File Libraries:`\n"
			"\n"
			"This is a listing of file libraries for your BBS. File Libraries are\n"
			"used to logically separate your file `directories` into groups. Every\n"
			"directory belongs to a file library.\n"
			"\n"
			"One popular use for file libraries is to separate CD-ROM and hard disk\n"
			"directories. One might have an `Uploads` file library that contains\n"
			"uploads to the hard disk directories and also have a `PC-SIG` file\n"
			"library that contains directories from a PC-SIG CD-ROM. Some sysops\n"
			"separate directories into more specific areas such as `Main`, `Graphics`,\n"
			"or `Adult`. If you have many directories that have a common subject\n"
			"denominator, you may want to have a separate file library for those\n"
			"directories for a more organized file structure.\n"
		;
		i=uifc.list(j,0,0,0,&libs_dflt,&libs_bar,"File Libraries                     Directories", opt);
		if((signed)i==-1) {
			j=save_changes(WIN_MID);
			if(j==-1)
				continue;
			if(!j) {
				save_file_cfg(&cfg,backup_level);
				refresh_cfg(&cfg);
			}
			return;
		}
		int libnum = i & MSK_OFF;
		int msk = i & MSK_ON;
		if(msk == MSK_INS) {
			char long_name[LEN_GLNAME+1];
			strcpy(long_name,"Main");
			uifc.helpbuf=lib_long_name_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Library Long Name",long_name,LEN_GLNAME,K_EDIT) < 1)
				continue;

			char short_name[LEN_GSNAME+1];
			SAFECOPY(short_name, long_name);
			uifc.helpbuf=lib_short_name_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Library Short Name",short_name,LEN_GSNAME, K_EDIT)<1)
				continue;

			char code_prefix[LEN_GSNAME+1];	/* purposely extra-long */
			SAFECOPY(code_prefix, short_name);
			prep_code(code_prefix, NULL);
			if(strlen(code_prefix) < LEN_CODE)
				strcat(code_prefix, "_");
			uifc.helpbuf=lib_code_prefix_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Internal Code Prefix", code_prefix, LEN_CODE, K_EDIT|K_UPPER) < 0)
				continue;
			if (code_prefix[0] != 0 && !code_ok(code_prefix)) {
				uifc.helpbuf=invalid_code;
				uifc.msg("Invalid Code Prefix");
				continue;
			}

			if (!new_lib(libnum))
				continue;
			SAFECOPY(cfg.lib[libnum]->lname, long_name);
			SAFECOPY(cfg.lib[libnum]->sname, short_name);
			SAFECOPY(cfg.lib[libnum]->code_prefix, code_prefix);
			uifc.changes = TRUE;
			continue;
		}
		if(msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_DEL) {
				uifc.helpbuf =
					"`Delete All Data in Library:`\n"
					"\n"
					"If you wish to delete the database files for all directories in this\n"
					"library, select `Yes`.\n"
					;
				j = 1;
				j = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &j, 0
					, "Delete All Library Data Files", uifcYesNoOpts);
				if (j == -1)
					continue;
				if (j == 0) {
					for (j = 0; j < cfg.total_dirs; j++) {
						if (cfg.dir[j]->lib == libnum) {
							sprintf(str, "%s%s.*"
								, cfg.lib[cfg.dir[j]->lib]->code_prefix
								, cfg.dir[j]->code_suffix);
							strlwr(str);
							if (!cfg.dir[j]->data_dir[0])
								sprintf(tmp, "%sdirs/", cfg.data_dir);
							else
								strcpy(tmp, cfg.dir[j]->data_dir);
							delfiles(tmp, str, /* keep: */0);
						}
					}
				}
			}
			if(msk == MSK_CUT)
				savlib = *cfg.lib[libnum];
			free(cfg.lib[libnum]);
			if (msk == MSK_DEL) {
				for(j=0;j<cfg.total_dirs;) {
					if(cfg.dir[j]->lib==libnum) {
						free(cfg.dir[j]);
						cfg.total_dirs--;
						k=j;
						while(k<cfg.total_dirs) {
							cfg.dir[k]=cfg.dir[k+1];
							k++;
						}
					}
					else j++;
				}
				for (j = 0; j < cfg.total_dirs; j++) {
					if (cfg.dir[j]->lib > libnum)
						cfg.dir[j]->lib--;
				}
			}
			else { /* CUT */
				for (j = 0; j < cfg.total_dirs; j++) {
					if (cfg.dir[j]->lib > libnum)
						cfg.dir[j]->lib--;
					else if (cfg.dir[j]->lib == libnum)
						cfg.dir[j]->lib = CUT_LIBNUM;
				}
			}
			cfg.total_libs--;
			for (i = libnum; i < cfg.total_libs; i++)
				cfg.lib[i] = cfg.lib[i+1];
			uifc.changes=1;
			continue;
		}
		if(msk == MSK_COPY) {
			savlib=*cfg.lib[libnum];
			continue;
		}
		if (msk == MSK_PASTE) {
			if (!new_lib(libnum))
				continue;
			/* Restore previously cut directories to newly-pasted library */
			for (i = 0; i < cfg.total_dirs; i++)
				if (cfg.dir[i]->lib == CUT_LIBNUM)
					cfg.dir[i]->lib = libnum;
			*cfg.lib[libnum] = savlib;
			uifc.changes=1;
			continue;
		}
		done=0;
		while(!done) {
			j=0;
			sprintf(opt[j++],"%-27.27s%s","Long Name",cfg.lib[i]->lname);
			sprintf(opt[j++],"%-27.27s%s","Short Name",cfg.lib[i]->sname);
			sprintf(opt[j++],"%-27.27s%s","Internal Code Prefix",cfg.lib[i]->code_prefix);
			sprintf(opt[j++],"%-27.27s%.40s","Parent Directory"
				,cfg.lib[i]->parent_path);
			sprintf(opt[j++],"%-27.27s%.40s","Access Requirements"
				,cfg.lib[i]->arstr);
			sprintf(opt[j++],"%-27.27s%.40s","Access to Sub-directories"
				,cfg.lib[i]->misc&LIB_DIRS ? "Yes":"No");
			sprintf(opt[j++],"%-27.27s%s","Sort Library By Directory", area_sort_desc[cfg.lib[i]->sort]);
			strcpy(opt[j++],"Clone Options");
			strcpy(opt[j++],"Export Areas...");
			strcpy(opt[j++],"Import Areas...");
			strcpy(opt[j++],"File Directories...");
			opt[j][0]=0;
			sprintf(str,"%s Library",cfg.lib[i]->sname);
			uifc.helpbuf=
				"`File Library Configuration:`\n"
				"\n"
				"This menu allows you to configure the security requirements for access\n"
				"to this file library. You can also add, delete, and configure the\n"
				"directories of this library by selecting the `File Directories...` option.\n"
			;
			switch(uifc.list(WIN_ACT,6,4,60,&dflt,0,str,opt)) {
				case -1:
					done=1;
					break;
				case __COUNTER__:
					uifc.helpbuf=lib_long_name_help;
					SAFECOPY(str, cfg.lib[i]->lname);
					if(uifc.input(WIN_MID|WIN_SAV,0,0,"Name to use for Listings"
						,str,LEN_GLNAME,K_EDIT) > 0)
						SAFECOPY(cfg.lib[i]->lname,str);
					break;
				case __COUNTER__:
					uifc.helpbuf=lib_short_name_help;
					SAFECOPY(str, cfg.lib[i]->sname);
					if(uifc.input(WIN_MID|WIN_SAV,0,0,"Name to use for Prompts"
						,str,LEN_GSNAME,K_EDIT) > 0)
						SAFECOPY(cfg.lib[i]->sname,str);
					break;
				case __COUNTER__:
				{
					char code_prefix[LEN_CODE+1];
					SAFECOPY(code_prefix, cfg.lib[i]->code_prefix);
					uifc.helpbuf=lib_code_prefix_help;
					if(uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code Prefix"
						,code_prefix,LEN_CODE,K_EDIT|K_UPPER) < 0)
						continue;
					if(code_prefix[0] == 0 || code_ok(code_prefix)) {
						SAFECOPY(cfg.lib[i]->code_prefix, code_prefix);
					} else {
						uifc.helpbuf = invalid_code;
						uifc.msg("Invalid Code Prefix");
					}
					break;
				}
				case __COUNTER__:
					uifc.helpbuf=
						"`Parent Directory:`\n"
						"\n"
						"This an optional path to be used as the physical \"parent\" directory for \n"
						"all logical directories in this library. This parent directory will be\n"
						"used in combination with each directory's `Transfer File Path` to create\n"
						"the full physical storage path for files in each directory.\n"
						"\n"
						"This option is convenient for adding libraries with many directories\n"
						"that share a common parent directory (e.g. CD-ROMs) and gives you the\n"
						"option of easily changing the common parent directory location later, if\n"
						"desired.\n"
						"\n"
						"The parent directory is not used for directories with a full/absolute\n"
						"`Transfer File Path` configured."
					;
					uifc.input(WIN_MID|WIN_SAV,0,0,"Parent Directory"
						,cfg.lib[i]->parent_path,sizeof(cfg.lib[i]->parent_path)-1,K_EDIT);
					break;
				case __COUNTER__:
					sprintf(str,"%s Library",cfg.lib[i]->sname);
					getar(str,cfg.lib[i]->arstr);
					break;
				case __COUNTER__:
					uifc.helpbuf=
						"`Access to Sub-directories:`\n"
						"\n"
					;
					j=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Allow Access to Sub-directories of Parent Directory"
						,uifcYesNoOpts);
					if(j==0 && (cfg.lib[i]->misc&LIB_DIRS) == 0) {
						cfg.lib[i]->misc|=LIB_DIRS;
						uifc.changes = true;
					} else if(j==1 && (cfg.lib[i]->misc&LIB_DIRS) != 0) {
						cfg.lib[i]->misc &= ~LIB_DIRS;
						uifc.changes = true;
					}
					break;
				case __COUNTER__:
					uifc.helpbuf="`Sort Library By Directory:`\n"
						"\n"
						"Normally, the directories within a file library are listed in the order\n"
						"that the sysop created them or a logical order determined by the sysop.\n"
						"\n"
						"Optionally, you can have the library sorted by directory `Long Name`,\n"
						"`Short Name`, or `Internal Code`.\n"
						"\n"
						"The library will be automatically re-sorted when new directories\n"
						"are added via `SCFG` or when existing directories are modified.\n"
						;
					j = cfg.lib[i]->sort;
					j = uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &j, 0, "Sort Library By Directory", area_sort_desc);
					if(j >= 0) {
						cfg.lib[i]->sort = j;
						sort_dirs(i);
						uifc.changes = TRUE;
					}
					break;
				case __COUNTER__: /* clone options */
					j=0;
					uifc.helpbuf=
						"`Clone Directory Options:`\n"
						"\n"
						"If you want to clone the options of the template directory of this\n"
						"library into all directories of this library, select `Yes`.\n"
						"\n"
						"The options cloned are upload requirements, download requirements,\n"
						"operator requirements, exempted user requirements, toggle options,\n"
						"maximum number of files, allowed file extensions, default file\n"
						"extension, and sort type.\n"
					;
					j=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Clone Options of Template Directory into All of Library"
						,uifcYesNoOpts);
					if(j==0) {
						dir_t* template = NULL;
						for(j=0;j<cfg.total_dirs;j++) {
							if(cfg.dir[j]->lib == i && cfg.dir[j]->misc&DIR_TEMPLATE) {
								template = cfg.dir[j];
								break;
							}
						}
						for(j=0;j<cfg.total_dirs;j++) {
							if(cfg.dir[j]->lib != i)
								continue;
							if(template == NULL)
								template = cfg.dir[j];
							else if(cfg.dir[j] != template) {
								uifc.changes=1;
								cfg.dir[j]->misc = template->misc;
								cfg.dir[j]->misc &= ~DIR_TEMPLATE;
								strcpy(cfg.dir[j]->ul_arstr		, template->ul_arstr);
								strcpy(cfg.dir[j]->dl_arstr		, template->dl_arstr);
								strcpy(cfg.dir[j]->op_arstr		, template->op_arstr);
								strcpy(cfg.dir[j]->ex_arstr		, template->ex_arstr);
								strcpy(cfg.dir[j]->exts			, template->exts);
								strcpy(cfg.dir[j]->data_dir		, template->data_dir);
								strcpy(cfg.dir[j]->upload_sem	, template->upload_sem);
								cfg.dir[j]->maxfiles			= template->maxfiles;
								cfg.dir[j]->maxage				= template->maxage;
								cfg.dir[j]->up_pct				= template->up_pct;
								cfg.dir[j]->dn_pct				= template->dn_pct;
								cfg.dir[j]->seqdev				= template->seqdev;
								cfg.dir[j]->sort				= template->sort;
							}
						}
					}
					break;
	#define DIRS_TXT_HELP_TEXT		"`DIRS.TXT` is a plain text file that includes all of the Synchronet\n" \
									"configuration field values for each directory in the library.\n"
	#define FILEGATE_ZXX_HELP_TEXT	"`FILEGATE.ZXX` is a plain text file in the old RAID/FILEBONE.NA format\n" \
									"which describes a list of file areas connected across a File\n" \
									"Distribution Network (e.g. Fidonet)."
				case __COUNTER__:
					k=0;
					ported=0;
					q=uifc.changes;
					strcpy(opt[k++],"DIRS.TXT     (Synchronet)");
					strcpy(opt[k++],"FILEGATE.ZXX (Fido)");
					opt[k][0]=0;
					uifc.helpbuf=
						"`Export Area File Format:`\n"
						"\n"
						"This menu allows you to choose the format of the area file you wish to\n"
						"export to.\n"
						"\n"
						DIRS_TXT_HELP_TEXT
						"\n"
						FILEGATE_ZXX_HELP_TEXT
					;
					k=0;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
						,"Export Area File Format",opt);
					if(k==-1)
						break;
					if(k==0)
						sprintf(str,"%sDIRS.TXT",cfg.ctrl_dir);
					else if(k==1)
						sprintf(str,"FILEGATE.ZXX");
					if(uifc.input(WIN_MID|WIN_SAV,0,0,"Filename"
						,str,sizeof(str)-1,K_EDIT)<=0) {
						uifc.changes=q;
						break;
					}
					if(fexist(str)) {
						strcpy(opt[0],"Overwrite");
						strcpy(opt[1],"Append");
						opt[2][0]=0;
						j=0;
						j=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
							,"File Exists",opt);
						if(j==-1)
							break;
						if(j==0) j=O_WRONLY|O_TRUNC;
						else	 j=O_WRONLY|O_APPEND;
					}
					else
						j=O_WRONLY|O_CREAT;
					if((stream=fnopen(&file,str,j|O_TEXT))==NULL) {
						uifc.msg("Open Failure");
						break;
					}
					uifc.pop("Exporting Areas...");
					for(j=0;j<cfg.total_dirs;j++) {
						if(cfg.dir[j]->lib!=i)
							continue;
						ported++;
						if(k==1) {
							fprintf(stream,"Area %-8s  0     !      %s\n"
								,cfg.dir[j]->code_suffix,cfg.dir[j]->lname);
							continue;
						}
						fprintf(stream,"%s\n%s\n%s\n%s\n%s\n%s\n"
								"%s\n%s\n"
							,cfg.dir[j]->lname
							,cfg.dir[j]->sname
							,cfg.dir[j]->code_suffix
							,cfg.dir[j]->data_dir
							,cfg.dir[j]->arstr
							,cfg.dir[j]->ul_arstr
							,cfg.dir[j]->dl_arstr
							,cfg.dir[j]->op_arstr
							);
						fprintf(stream,"%s\n%s\n%u\n%s\n%"PRIX32"\n%u\n"
								"%u\n"
							,cfg.dir[j]->path
							,cfg.dir[j]->upload_sem
							,cfg.dir[j]->maxfiles
							,cfg.dir[j]->exts
							,cfg.dir[j]->misc
							,cfg.dir[j]->seqdev
							,cfg.dir[j]->sort
							);
						fprintf(stream,"%s\n%u\n%u\n%u\n"
							,cfg.dir[j]->ex_arstr
							,cfg.dir[j]->maxage
							,cfg.dir[j]->up_pct
							,cfg.dir[j]->dn_pct
							);
						fprintf(stream,"***END-OF-DIR***\n\n");
					}
					fclose(stream);
					uifc.pop(NULL);
					sprintf(str,"%lu File Areas Exported Successfully",ported);
					uifc.msg(str);
					uifc.changes=q;
					break;

				case __COUNTER__:
					ported=added=0;
					k=0;
					uifc.helpbuf=
						"`Import Area File Format:`\n"
						"\n"
						"This menu allows you to choose the format of the area file you wish to\n"
						"import into the current file library.\n"
						"\n"
						"A \"raw\" directory listing is a text file with sub-directory names only.\n"
						"\n"
						"A \"raw\" directory listing can be created in Windows with the following\n"
						"command: `dir /on /ad /b > dirs.raw`\n"
						"\n"
						"The `Directory Listing...` option will automatically generate and import\n"
						"the raw directory listing for you.\n"
						"\n"
						DIRS_TXT_HELP_TEXT
						"\n"
						FILEGATE_ZXX_HELP_TEXT
					;
					strcpy(opt[k++],"DIRS.TXT     (Synchronet)");
					strcpy(opt[k++],"FILEGATE.ZXX (Fido)");
					strcpy(opt[k++],"DIRS.RAW     (Raw)");
					strcpy(opt[k++],"Directory Listing...");
					opt[k][0]=0;
					k=0;
					k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
						,"Import Area File Format",opt);
					if(k==-1)
						break;
					if(k==0)
						sprintf(str,"%sDIRS.TXT",cfg.ctrl_dir);
					else if(k==1)
						sprintf(str,"FILEGATE.ZXX");
					else {
						strcpy(str,cfg.lib[i]->parent_path);
						backslash(str);
						strcat(str,"dirs.raw");
					}
					if(k==3) {
						if(!create_raw_dir_list(str))
							break;
					} else {
						if(uifc.input(WIN_MID|WIN_SAV,0,0,"Filename"
							,str,sizeof(str)-1,K_EDIT)<=0)
							break;
						if(k==2 && !fexistcase(str)) {
							j=0;
							if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
								,"File doesn't exist, create it?",uifcYesNoOpts)==0)
								create_raw_dir_list(str);
						}
					}
					if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
						uifc.msg("Open Failure");
						break; 
					}
					uifc.pop("Importing Areas...");
					char duplicate_code[LEN_CODE+1]="";
					uint duplicate_codes = 0;	// consecutive duplicate codes
					while(!feof(stream) && cfg.total_dirs < MAX_DIRS) {
						if(!fgets(str,sizeof(str),stream)) break;
						truncsp(str);
						if(!str[0])
							continue;
						memset(&tmpdir,0,sizeof(dir_t));
						tmpdir.misc=DEFAULT_DIR_OPTIONS;
						tmpdir.maxfiles=MAX_FILES;
						tmpdir.up_pct=cfg.cdt_up_pct;
						tmpdir.dn_pct=cfg.cdt_dn_pct; 

						p=str;
						while(*p && *p<=' ') p++;

						if(k>=2) { /* raw */
							int len = strlen(p);
							if(len > LEN_DIR)
								continue;
							SAFECOPY(tmp_code,p);
							SAFECOPY(tmpdir.path,p);
							/* skip first sub-dir(s) */
							char* tp = p;
							while((len=strlen(tp)) > LEN_SLNAME) {
								FIND_CHAR(tp, '/');
								SKIP_CHAR(tp, '/');
							}
							if(*tp != 0)
								p = tp;
							if((len=strlen(p)) > LEN_SLNAME)
								p += len - LEN_SLNAME;
							SAFECOPY(tmpdir.lname,p);
							/* skip first sub-dir(s) */
							tp = p;
							while((len=strlen(tp)) > LEN_SSNAME) {
								FIND_CHAR(tp, '/');
								SKIP_CHAR(tp, '/');
							}
							if(*tp != 0)
								p = tp;
							if((len=strlen(p)) > LEN_SSNAME)
								p += len - LEN_SSNAME;
							SAFECOPY(tmpdir.sname,p);
							ported++;
						}
						else if(k==1) {
							if(strnicmp(p,"AREA ",5))
								continue;
							p+=5;
							while(*p && *p<=' ') p++;
							SAFECOPY(tmp_code,p);
							truncstr(tmp_code," \t");
							while(*p>' ') p++;			/* Skip areaname */
							while(*p && *p<=' ') p++;	/* Skip space */
							while(*p>' ') p++;			/* Skip level */
							while(*p && *p<=' ') p++;	/* Skip space */
							while(*p>' ') p++;			/* Skip flags */
							while(*p && *p<=' ') p++;	/* Skip space */
							SAFECOPY(tmpdir.sname,tmp_code); 
							SAFECOPY(tmpdir.lname,p); 
							ported++;
						}
						else {
							sprintf(tmpdir.lname,"%.*s",LEN_SLNAME,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.sname,"%.*s",LEN_SSNAME,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							SAFECOPY(tmp_code,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.data_dir,"%.*s",LEN_DIR,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.arstr,"%.*s",LEN_ARSTR,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.ul_arstr,"%.*s",LEN_ARSTR,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.dl_arstr,"%.*s",LEN_ARSTR,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.op_arstr,"%.*s",LEN_ARSTR,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.path,"%.*s",LEN_DIR,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.upload_sem,"%.*s",LEN_DIR,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							tmpdir.maxfiles=atoi(str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.exts,"%.*s",40,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							tmpdir.misc=ahtoul(str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							tmpdir.seqdev=atoi(str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							tmpdir.sort=atoi(str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							sprintf(tmpdir.ex_arstr,"%.*s",LEN_ARSTR,str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							tmpdir.maxage=atoi(str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							tmpdir.up_pct=atoi(str);
							if(!fgets(str,sizeof(str),stream)) break;
							truncsp(str);
							tmpdir.dn_pct=atoi(str);

							ported++;
							while(!feof(stream)
								&& strcmp(str,"***END-OF-DIR***")) {
								if(!fgets(str,sizeof(str),stream)) break;
								truncsp(str); 
							} 
						}

						if(tmpdir.lname[0] == 0)
							SAFECOPY(tmpdir.lname, tmp_code);
						if(tmpdir.sname[0] == 0)
							SAFECOPY(tmpdir.sname, tmp_code);

						SAFECOPY(tmpdir.code_suffix, prep_code(tmp_code,cfg.lib[i]->code_prefix));

						int dupes = 0;
						int attempts = 0;	// attempts to generate a unique internal code
						if(stricmp(tmpdir.code_suffix, duplicate_code) == 0)
							attempts = ++duplicate_codes;
						else
							duplicate_codes = 0;
						for(j=0; j < cfg.total_dirs && attempts < (36*36*36); j++) {
							if(cfg.dir[j]->lib == i) {	/* same lib */
								if(tmpdir.path[0]
									&& strcmp(cfg.dir[j]->path, tmpdir.path) == 0)	/* same path? overwrite the dir entry */
									break;
								if(stricmp(cfg.dir[j]->sname, tmpdir.sname) == 0)
									break;
							} else {
								if((cfg.lib[i]->code_prefix[0] || cfg.lib[cfg.dir[j]->lib]->code_prefix[0]))
									continue;
							}
							if(stricmp(cfg.dir[j]->code_suffix,tmpdir.code_suffix) == 0) {
								if(k < 1)	/* dirs.txt import (don't modify internal code) */
									break;
								if(attempts == 0)
									SAFECOPY(duplicate_code, tmpdir.code_suffix);
								int code_len = strlen(tmpdir.code_suffix);
								if(code_len < 1)
									break;
								tmpdir.code_suffix[code_len-1] = random_code_char();
								if(attempts > 10 && code_len > 1)
									tmpdir.code_suffix[code_len-2] = random_code_char();
								if(attempts > 100 && code_len > 2)
									tmpdir.code_suffix[code_len-3] = random_code_char();
								j=0;
								attempts++;
							}
						}
						if(attempts >= (36*36*36))
							break;
						if(j==cfg.total_dirs) {

							if((cfg.dir=(dir_t **)realloc(cfg.dir
								,sizeof(dir_t *)*(cfg.total_dirs+1)))==NULL) {
								errormsg(WHERE,ERR_ALLOC,"dir",cfg.total_dirs+1);
								cfg.total_dirs=0;
								bail(1);
								break;
							}

							if((cfg.dir[j]=(dir_t *)malloc(sizeof(dir_t)))
								==NULL) {
								errormsg(WHERE,ERR_ALLOC,"dir",sizeof(dir_t));
								break;
							}
							memset(cfg.dir[j],0,sizeof(dir_t)); 
							cfg.dir[j]->dirnum = j;
							added++;
						} else
							dupes++;
						if(k==1) {
							SAFECOPY(cfg.dir[j]->code_suffix,tmpdir.code_suffix);
							SAFECOPY(cfg.dir[j]->sname,tmpdir.sname);
							SAFECOPY(cfg.dir[j]->lname,tmpdir.lname);
							if(j==cfg.total_dirs) {
								cfg.dir[j]->maxfiles=MAX_FILES;
								cfg.dir[j]->up_pct=cfg.cdt_up_pct;
								cfg.dir[j]->dn_pct=cfg.cdt_dn_pct; 
							}
						} else
							memcpy(cfg.dir[j],&tmpdir,sizeof(dir_t));
						cfg.dir[j]->lib=i;
						if(j==cfg.total_dirs) {
							cfg.dir[j]->misc=tmpdir.misc;
							cfg.total_dirs++; 
						}
						uifc.changes=1; 
					}
					fclose(stream);
					if(ported && cfg.lib[i]->sort)
						sort_dirs(i);
					uifc.pop(NULL);
					sprintf(str,"%lu File Areas Imported Successfully (%lu added)",ported, added);
					uifc.msg(str);
					break;

				case __COUNTER__:
					dir_cfg(libnum);
					break;
			}
		}
	}
}

void dir_cfg(uint libnum)
{
	static int dflt,bar,tog_dflt,tog_bar,adv_dflt,opt_dflt;
	char str[128],str2[128],code[128],path[MAX_PATH+1],done=0;
	char data_dir[MAX_PATH+1];
	int j,n;
	uint i,dirnum[MAX_OPTS+1];
	static dir_t savdir;

	char* dir_long_name_help =
		"`Directory Long Name:`\n"
		"\n"
		"This is a description of the file directory which is displayed in all\n"
		"directory listings.\n"
		;
	char* dir_short_name_help =
		"`Directory Short Name:`\n"
		"\n"
		"This is a short description of the file directory which is displayed at\n"
		"the file transfer prompt.\n"
		;
	char* dir_code_help =
		"`Directory Internal Code Suffix:`\n"
		"\n"
		"Every directory must have its own unique code for Synchronet to refer to\n"
		"it internally. This code should be descriptive of the directory's\n"
		"contents, usually an abbreviation of the directory's name.\n"
		"\n"
		"`Note:` The internal code is constructed from the file library's code prefix\n"
		"(if present) and the directory's code suffix.\n"
		;
	char* dir_transfer_path_help =
		"`Transfer File Path:`\n"
		"\n"
		"This is the default storage path for files uploaded-to and available for\n"
		"download-from this directory.\n"
		"\n"
		"If this setting is blank, the internal-code (lower-cased) is used as the\n"
		"default directory name.\n"
		"\n"
		"If this value is not a full/absolute path, the parent directory will be\n"
		"either the library's `Parent Directory` (if set) or the data directory\n"
		"(e.g. ../data/dirs)\n"
		"\n"
		"This path can be overridden on a per-file basis using `Alternate File\n"
		"Paths`.\n"
		;

	while(1) {
		if(uifc.changes && cfg.lib[libnum]->sort)
			sort_dirs(libnum);
		int maxlen = 0;
		for(i=0,j=0;i<cfg.total_dirs && j<MAX_OPTS;i++) {
			if(cfg.dir[i]->lib != libnum)
				continue;
			char* name = cfg.dir[i]->lname;
			int name_len = LEN_SLNAME;
			switch(cfg.lib[libnum]->sort) {
				case AREA_SORT_SNAME:
					name = cfg.dir[i]->sname;
					name_len = LEN_SSNAME;
					break;
				case AREA_SORT_CODE:
					name = cfg.dir[i]->code_suffix;
					name_len = LEN_CODE;
					break;
				default:	/* Defeat stupid GCC warning */
					break;
			}
			sprintf(str, "%-*s %c", name_len, name, cfg.dir[i]->misc&DIR_TEMPLATE ? '*' : ' ');
			truncsp(str);
			int len = sprintf(opt[j], "%s", str);
			if(len > maxlen)
				maxlen = len;
			dirnum[j++]=i;
		}
		dirnum[j]=cfg.total_dirs;
		opt[j][0]=0;
		sprintf(str,"%s Directories (%u)",cfg.lib[libnum]->sname, j);
		int mode = WIN_SAV|WIN_ACT|WIN_RHT;
		if(j)
			mode |= WIN_DEL|WIN_COPY|WIN_CUT|WIN_DELACT;
		if(j<MAX_OPTS)
			mode |= WIN_INS|WIN_INSACT|WIN_XTR;
		if(savdir.sname[0])
			mode |= WIN_PASTE | WIN_PASTEXTR;
		uifc.helpbuf=
			"`File Directories:`\n"
			"\n"
			"This is a list of file directories that have been configured for the\n"
			"selected file library.\n"
			"\n"
			"To add a directory, select the desired position with the arrow keys and\n"
			"hit ~ INS ~.\n"
			"\n"
			"To delete a directory, select it with the arrow keys and hit ~ DEL ~.\n"
			"\n"
			"To configure a directory, select it with the arrow keys and hit ~ ENTER ~.\n"
		;
		i = uifc.list(mode, 0, 0, maxlen=5, &dflt, &bar, str, opt);
		if((signed)i==-1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if(msk == MSK_INS) {
			strcpy(str,"My Brand-New File Directory");
			uifc.helpbuf=dir_long_name_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Directory Long Name",str,LEN_SLNAME
				,K_EDIT)<1)
				continue;
			sprintf(str2,"%.*s",LEN_SSNAME,str);
			uifc.helpbuf=dir_short_name_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Directory Short Name",str2,LEN_SSNAME
				,K_EDIT)<1)
				continue;
			SAFECOPY(code,str2);
			prep_code(code,/* prefix: */NULL);
			uifc.helpbuf=dir_code_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Directory Internal Code Suffix",code,LEN_CODE
				,K_EDIT|K_UPPER)<1)
				continue;
			if(!code_ok(code)) {
				uifc.helpbuf=invalid_code;
				uifc.msg("Invalid Code");
				uifc.helpbuf=0;
				continue;
			}
			SAFECOPY(path,code);
			strlwr(path);
			uifc.helpbuf = dir_transfer_path_help;
			uifc.input(WIN_MID|WIN_SAV,0,0,"File Transfer Path", path, LEN_DIR,K_EDIT);

			if (!new_dir(dirnum[i], libnum))
				continue;

			if(stricmp(str2,"OFFLINE") == 0)
				cfg.dir[dirnum[i]]->misc = 0;
			SAFECOPY(cfg.dir[dirnum[i]]->code_suffix,code);
			SAFECOPY(cfg.dir[dirnum[i]]->lname,str);
			SAFECOPY(cfg.dir[dirnum[i]]->sname,str2);
			SAFECOPY(cfg.dir[dirnum[i]]->path,path);
			uifc.changes=1;
			continue;
		}
		if(msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_DEL) {
				uifc.helpbuf =
					"`Delete Directory Data Files:`\n"
					"\n"
					"If you want to delete all the database files for this directory,\n"
					"select `Yes`.\n"
					;
				j = 1;
				SAFEPRINTF2(str, "%s%s.*"
					, cfg.lib[cfg.dir[dirnum[i]]->lib]->code_prefix
					, cfg.dir[dirnum[i]]->code_suffix);
				strlwr(str);
				if (!cfg.dir[dirnum[i]]->data_dir[0])
					SAFEPRINTF(data_dir, "%sdirs/", cfg.data_dir);
				else
					SAFECOPY(data_dir, cfg.dir[dirnum[i]]->data_dir);
				SAFEPRINTF2(path, "%s%s", data_dir, str);
				if (fexist(path)) {
					SAFEPRINTF(str2, "Delete %s", path);
					j = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &j, 0
						, str2, uifcYesNoOpts);
					if (j == -1)
						continue;
					if (j == 0)
						delfiles(data_dir, str, /* keep: */0);
				}
			}
			if(msk == MSK_CUT)
				savdir = *cfg.dir[dirnum[i]];
			free(cfg.dir[dirnum[i]]);
			cfg.total_dirs--;
			for(j=dirnum[i];j<cfg.total_dirs;j++)
				cfg.dir[j]=cfg.dir[j+1];
			uifc.changes=1;
			continue;
		}
		if(msk == MSK_COPY) {
			savdir=*cfg.dir[dirnum[i]];
			continue;
		}
		if (msk == MSK_PASTE) {
			if (!new_dir(dirnum[i], libnum))
				continue;
			*cfg.dir[dirnum[i]]=savdir;
			cfg.dir[dirnum[i]]->lib=libnum;
			uifc.changes=1;
			continue;
		}
		i=dirnum[dflt];
		j=0;
		done=0;
		while(!done) {
			n=0;
			sprintf(opt[n++],"%-27.27s%s","Long Name",cfg.dir[i]->lname);
			sprintf(opt[n++],"%-27.27s%s","Short Name",cfg.dir[i]->sname);
			sprintf(opt[n++],"%-27.27s%s%s","Internal Code"
				,cfg.lib[cfg.dir[i]->lib]->code_prefix, cfg.dir[i]->code_suffix);
			sprintf(opt[n++],"%-27.27s%.40s","Access Requirements"
				,cfg.dir[i]->arstr);
			sprintf(opt[n++],"%-27.27s%.40s","Upload Requirements"
				,cfg.dir[i]->ul_arstr);
			sprintf(opt[n++],"%-27.27s%.40s","Download Requirements"
				,cfg.dir[i]->dl_arstr);
			sprintf(opt[n++],"%-27.27s%.40s","Operator Requirements"
				,cfg.dir[i]->op_arstr);
			sprintf(opt[n++],"%-27.27s%.40s","Exemption Requirements"
				,cfg.dir[i]->ex_arstr);
			SAFECOPY(path, cfg.dir[i]->path);
			if(!path[0]) {
				SAFEPRINTF2(path, "%s%s", cfg.lib[cfg.dir[i]->lib]->code_prefix, cfg.dir[i]->code_suffix);
				strlwr(path);
			}
			if(cfg.lib[cfg.dir[i]->lib]->parent_path[0])
				prep_dir(cfg.lib[cfg.dir[i]->lib]->parent_path, path, sizeof(path));
			else {
				if (!cfg.dir[i]->data_dir[0])
					SAFEPRINTF(data_dir, "%sdirs", cfg.data_dir);
				else
					SAFECOPY(data_dir, cfg.dir[i]->data_dir);
				prep_dir(data_dir, path, sizeof(path));
			}
			if(strcmp(path, cfg.dir[i]->path) == 0)
				SAFECOPY(str, path);
			else
				SAFEPRINTF(str, "[%s]", path);
			sprintf(opt[n++],"%-27.27s%.40s","Transfer File Path"
				,str);
			sprintf(opt[n++],"%-27.27s%u","Maximum Number of Files"
				,cfg.dir[i]->maxfiles);
			if(cfg.dir[i]->maxage)
				sprintf(str,"Enabled (%u days old)",cfg.dir[i]->maxage);
			else
				strcpy(str,"Disabled");
			sprintf(opt[n++],"%-27.27s%s","Purge by Age",str);
			sprintf(opt[n++],"%-27.27s%u%%","Credit on Upload"
				,cfg.dir[i]->up_pct);
			sprintf(opt[n++],"%-27.27s%u%%","Credit on Download"
				,cfg.dir[i]->dn_pct);
			strcpy(opt[n++],"Toggle Options...");
			strcpy(opt[n++],"Advanced Options...");
			opt[n][0]=0;
			sprintf(str,"%s Directory",cfg.dir[i]->sname);
			uifc.helpbuf=
				"`Directory Configuration:`\n"
				"\n"
				"This menu allows you to configure the individual selected directory.\n"
				"Options with a trailing `...` provide a sub-menu of more options.\n"
			;
			switch(uifc.list(WIN_SAV|WIN_ACT|WIN_RHT|WIN_BOT
				,0,0,0,&opt_dflt,0,str,opt)) {
				case -1:
					done=1;
					break;
				case 0:
					uifc.helpbuf=dir_long_name_help;
					SAFECOPY(str, cfg.dir[i]->lname);
					if(uifc.input(WIN_L2R|WIN_SAV,0,17,"Name to use for Listings"
						,str,LEN_SLNAME,K_EDIT) > 0)
						SAFECOPY(cfg.dir[i]->lname, str);
					break;
				case 1:
					uifc.helpbuf=dir_short_name_help;
					SAFECOPY(str, cfg.dir[i]->sname);
					if(uifc.input(WIN_L2R|WIN_SAV,0,17,"Name to use for Prompts"
						,str,LEN_SSNAME,K_EDIT) > 0)
						SAFECOPY(cfg.dir[i]->sname, str);
					break;
				case 2:
					uifc.helpbuf=dir_code_help;
					SAFECOPY(str,cfg.dir[i]->code_suffix);
					uifc.input(WIN_L2R|WIN_SAV,0,17,"Internal Code Suffix (unique)"
						,str,LEN_CODE,K_EDIT|K_UPPER);
					if(code_ok(str))
						SAFECOPY(cfg.dir[i]->code_suffix,str);
					else {
						uifc.helpbuf=invalid_code;
						uifc.msg("Invalid Code");
						uifc.helpbuf=0; 
					}
					break;
				case 3:
					sprintf(str,"%s Access",cfg.dir[i]->sname);
					getar(str,cfg.dir[i]->arstr);
					break;
				case 4:
					sprintf(str,"%s Upload",cfg.dir[i]->sname);
					getar(str,cfg.dir[i]->ul_arstr);
					break;
				case 5:
					sprintf(str,"%s Download",cfg.dir[i]->sname);
					getar(str,cfg.dir[i]->dl_arstr);
					break;
				case 6:
					sprintf(str,"%s Operator",cfg.dir[i]->sname);
					getar(str,cfg.dir[i]->op_arstr);
					break;
				case 7:
					sprintf(str,"%s Exemption",cfg.dir[i]->sname);
					getar(str,cfg.dir[i]->ex_arstr);
					break;
				case 8:
					uifc.helpbuf = dir_transfer_path_help;
					uifc.input(WIN_L2R|WIN_SAV,0,17,"Transfer File Path"
						,cfg.dir[i]->path,sizeof(cfg.dir[i]->path)-1,K_EDIT);
					break;
				case 9:
					uifc.helpbuf=
						"`Maximum Number of Files:`\n"
						"\n"
						"This value is the maximum number of files allowed in this directory.\n"
					;
					sprintf(str,"%u",cfg.dir[i]->maxfiles);
					uifc.input(WIN_L2R|WIN_SAV,0,17,"Maximum Number of Files"
						,str,5,K_EDIT|K_NUMBER);
					n=atoi(str);
					if(n>MAX_FILES) {
						sprintf(str,"Maximum Files is %u",MAX_FILES);
						uifc.msg(str);
					}
					else
						cfg.dir[i]->maxfiles=n;
					break;
				case 10:
					sprintf(str,"%u",cfg.dir[i]->maxage);
					uifc.helpbuf=
						"`Maximum Age of Files:`\n"
						"\n"
						"This value is the maximum number of days that files will be kept in\n"
						"the directory based on the date the file was uploaded or last\n"
						"downloaded (If the `Purge by Last Download` toggle option is used).\n"
						"\n"
						"The Synchronet file base maintenance program (`DELFILES`) must be used\n"
						"to automatically remove files based on age.\n"
					;
					uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Age of Files (in days)"
						,str,5,K_EDIT|K_NUMBER);
					cfg.dir[i]->maxage=atoi(str);
					break;
				case 11:
	uifc.helpbuf=
		"`Percentage of Credits to Credit Uploader on Upload:`\n"
		"\n"
		"This is the percentage of a file's credit value that is given to users\n"
		"when they upload files. Most often, this value will be set to `100` to\n"
		"give full credit value (100%) for uploads.\n"
		"\n"
		"If you want uploaders to receive no credits upon upload, set this value\n"
		"to `0`.\n"
	;
					uifc.input(WIN_MID|WIN_SAV,0,0
						,"Percentage of Credits to Credit Uploader on Upload"
						,ultoa(cfg.dir[i]->up_pct,tmp,10),4,K_EDIT|K_NUMBER);
					cfg.dir[i]->up_pct=atoi(tmp);
					break;
				case 12:
	uifc.helpbuf=
		"`Percentage of Credits to Credit Uploader on Download:`\n"
		"\n"
		"This is the percentage of a file's credit value that is given to users\n"
		"who upload a file that is later downloaded by another user. This is an\n"
		"award type system where more popular files will generate more credits\n"
		"for the uploader.\n"
		"\n"
		"If you do not want uploaders to receive credit when files they upload\n"
		"are later downloaded, set this value to `0`.\n"
	;
					uifc.input(WIN_MID|WIN_SAV,0,0
						,"Percentage of Credits to Credit Uploader on Download"
						,ultoa(cfg.dir[i]->dn_pct,tmp,10),4,K_EDIT|K_NUMBER);
					cfg.dir[i]->dn_pct=atoi(tmp);
					break;
				case 13:
					while(1) {
						n=0;
						sprintf(opt[n++],"%-30.30s%s","Check for File Existence"
							,cfg.dir[i]->misc&DIR_FCHK ? "Yes":"No");
						strcpy(str,"Slow Media Device");
						if(cfg.dir[i]->seqdev) {
							sprintf(tmp," #%u",cfg.dir[i]->seqdev);
							strcat(str,tmp);
						}
						sprintf(opt[n++],"%-30.30s%s",str
							,cfg.dir[i]->seqdev ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Force Content Ratings"
							,cfg.dir[i]->misc&DIR_RATE ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Upload Date in Listings"
							,cfg.dir[i]->misc&DIR_ULDATE ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Multiple File Numberings"
							,cfg.dir[i]->misc&DIR_MULT ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Search for Duplicates"
							,cfg.dir[i]->misc&DIR_DUPES ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Search for New Files"
							,cfg.dir[i]->misc&DIR_NOSCAN ? "No":"Yes");
						sprintf(opt[n++],"%-30.30s%s","Search for Auto-ADDFILES"
							,cfg.dir[i]->misc&DIR_NOAUTO ? "No":"Yes");
						sprintf(opt[n++],"%-30.30s%s","Import FILE_ID.DIZ"
							,cfg.dir[i]->misc&DIR_DIZ ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Free Downloads"
							,cfg.dir[i]->misc&DIR_FREE ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Free Download Time"
							,cfg.dir[i]->misc&DIR_TFREE ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Deduct Upload Time"
							,cfg.dir[i]->misc&DIR_ULTIME ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Credit Uploads"
							,cfg.dir[i]->misc&DIR_CDTUL ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Credit Downloads"
							,cfg.dir[i]->misc&DIR_CDTDL ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Credit with Minutes"
							,cfg.dir[i]->misc&DIR_CDTMIN ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Download Notifications"
							,cfg.dir[i]->misc&DIR_QUIET ? "No":"Yes");
						sprintf(opt[n++],"%-30.30s%s","Anonymous Uploads"
							,cfg.dir[i]->misc&DIR_ANON ? cfg.dir[i]->misc&DIR_AONLY
							? "Only":"Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Purge by Last Download"
							,cfg.dir[i]->misc&DIR_SINCEDL ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Mark Moved Files as New"
							,cfg.dir[i]->misc&DIR_MOVENEW ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Include Transfers In Stats"
							,cfg.dir[i]->misc&DIR_NOSTAT ? "No":"Yes");
						sprintf(opt[n++],"%-30.30s%s","Access Files not in Database"
							,cfg.dir[i]->misc&DIR_FILES ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Template for New Directories"
							,cfg.dir[i]->misc&DIR_TEMPLATE ? "Yes" : "No");
						opt[n][0]=0;
						uifc.helpbuf=
							"`Directory Toggle Options:`\n"
							"\n"
							"This is the toggle options menu for the selected file directory.\n"
							"\n"
							"The available options from this menu can all be toggled between two or\n"
							"more states, such as `Yes` and `No`.\n"
						;
						n=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,2,0,&tog_dflt
							,&tog_bar,"Toggle Options",opt);
						if(n==-1)
							break;
						switch(n) {
							case 0:
								n=cfg.dir[i]->misc&DIR_FCHK ? 0:1;
								uifc.helpbuf=
									"`Check for File Existence When Listing:`\n"
									"\n"
									"If you want the actual existence of files to be verified while listing\n"
									"directories, set this value to `Yes`.\n"
									"\n"
									"Directories with files located on CD-ROM or other slow media should have\n"
									"this option set to `No`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Check for File Existence When Listing",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_FCHK)) {
									cfg.dir[i]->misc|=DIR_FCHK;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_FCHK)) {
									cfg.dir[i]->misc&=~DIR_FCHK;
									uifc.changes=1;
								}
								break;
							case 1:
								n=cfg.dir[i]->seqdev ? 0:1;
								uifc.helpbuf=
									"`Slow Media Device:`\n"
									"\n"
									"If this directory contains files located on CD-ROM or other slow media\n"
									"device, you should set this option to `Yes`. Each slow media device on\n"
									"your system should have a unique `Device Number`. If you only have one\n"
									"slow media device, then this number should be set to `1`.\n"
									"\n"
									"`CD-ROM multi-disk changers` are considered `one` device and all the\n"
									"directories on all the CD-ROMs in each changer should be set to the same\n"
									"device number.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Slow Media Device"
									,uifcYesNoOpts);
								if(n==0) {
									if(!cfg.dir[i]->seqdev) {
										uifc.changes=1;
										strcpy(str,"1");
									}
									else
										sprintf(str,"%u",cfg.dir[i]->seqdev);
									uifc.input(WIN_MID|WIN_SAV,0,0
										,"Device Number"
										,str,2,K_EDIT|K_UPPER);
									cfg.dir[i]->seqdev=atoi(str);
								}
								else if(n==1 && cfg.dir[i]->seqdev) {
									cfg.dir[i]->seqdev=0;
									uifc.changes=1;
								}
								break;
							case 2:
								n=cfg.dir[i]->misc&DIR_RATE ? 0:1;
								uifc.helpbuf=
									"`Force Content Ratings in Descriptions:`\n"
									"\n"
									"If you would like all uploads to this directory to be prompted for\n"
									"content rating (G, R, or X), set this value to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Force Content Ratings in Descriptions",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_RATE)) {
									cfg.dir[i]->misc|=DIR_RATE;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_RATE)) {
									cfg.dir[i]->misc&=~DIR_RATE;
									uifc.changes=1;
								}
								break;
							case 3:
								n=cfg.dir[i]->misc&DIR_ULDATE ? 0:1;
								uifc.helpbuf=
									"`Include Upload Date in File Descriptions:`\n"
									"\n"
									"If you wish the upload date of each file in this directory to be\n"
									"automatically included in the file description, set this option to\n"
									"`Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Include Upload Date in Descriptions",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_ULDATE)) {
									cfg.dir[i]->misc|=DIR_ULDATE;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_ULDATE)) {
									cfg.dir[i]->misc&=~DIR_ULDATE;
									uifc.changes=1;
								}
								break;
							case 4:
								n=cfg.dir[i]->misc&DIR_MULT ? 0:1;
								uifc.helpbuf=
									"`Ask for Multiple File Numberings:`\n"
									"\n"
									"If you would like uploads to this directory to be prompted for multiple\n"
									"file (disk) numbers, set this value to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Ask for Multiple File Numberings",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_MULT)) {
									cfg.dir[i]->misc|=DIR_MULT;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_MULT)) {
									cfg.dir[i]->misc&=~DIR_MULT;
									uifc.changes=1;
								}
								break;
							case 5:
								n=cfg.dir[i]->misc&DIR_DUPES ? 0:1;
								uifc.helpbuf=
									"`Search Directory for Duplicate Filenames:`\n"
									"\n"
									"If you would like to have this directory searched for duplicate\n"
									"filenames when a user attempts to upload a file, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Search for Duplicate Filenames",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_DUPES)) {
									cfg.dir[i]->misc|=DIR_DUPES;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_DUPES)) {
									cfg.dir[i]->misc&=~DIR_DUPES;
									uifc.changes=1;
								}
								break;
							case 6:
								n=cfg.dir[i]->misc&DIR_NOSCAN ? 1:0;
								uifc.helpbuf=
									"`Search Directory for New Files:`\n"
									"\n"
									"If you would like to have this directory searched for newly uploaded\n"
									"files when a user scans `All` libraries for new files, set this option to\n"
									"`Yes`.\n"
									"\n"
									"If this directory is located on `CD-ROM` or other read only media\n"
									"(where uploads are unlikely to occur), it will improve new file scans\n"
									"if this option is set to `No`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Search for New files",uifcYesNoOpts);
								if(n==0 && cfg.dir[i]->misc&DIR_NOSCAN) {
									cfg.dir[i]->misc&=~DIR_NOSCAN;
									uifc.changes=1;
								}
								else if(n==1 && !(cfg.dir[i]->misc&DIR_NOSCAN)) {
									cfg.dir[i]->misc|=DIR_NOSCAN;
									uifc.changes=1;
								}
								break;
							case 7:
								n=cfg.dir[i]->misc&DIR_NOAUTO ? 1:0;
								uifc.helpbuf=
									"`Search Directory for Auto-ADDFILES:`\n"
									"\n"
									"If you would like to have this directory searched for a file list to\n"
									"import automatically when using the `ADDFILES *` (Auto-ADD) feature,\n"
									"set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Search for Auto-ADDFILES",uifcYesNoOpts);
								if(n==0 && cfg.dir[i]->misc&DIR_NOAUTO) {
									cfg.dir[i]->misc&=~DIR_NOAUTO;
									uifc.changes=1;
								}
								else if(n==1 && !(cfg.dir[i]->misc&DIR_NOAUTO)) {
									cfg.dir[i]->misc|=DIR_NOAUTO;
									uifc.changes=1;
								}
								break;
							case 8:
								n=cfg.dir[i]->misc&DIR_DIZ ? 0:1;
								uifc.helpbuf=
									"`Import FILE_ID.DIZ and DESC.SDI Descriptions:`\n"
									"\n"
									"If you would like archived descriptions (`FILE_ID.DIZ` and `DESC.SDI`)\n"
									"of uploaded files to be automatically imported as the extended\n"
									"description, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Import FILE_ID.DIZ and DESC.SDI",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_DIZ)) {
									cfg.dir[i]->misc|=DIR_DIZ;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_DIZ)) {
									cfg.dir[i]->misc&=~DIR_DIZ;
									uifc.changes=1;
								}
								break;
							case 9:
								n=cfg.dir[i]->misc&DIR_FREE ? 0:1;
								uifc.helpbuf=
									"`Downloads are Free:`\n"
									"\n"
									"If you would like all downloads from this directory to be free (cost\n"
									"no credits), set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Downloads are Free",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_FREE)) {
									cfg.dir[i]->misc|=DIR_FREE;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_FREE)) {
									cfg.dir[i]->misc&=~DIR_FREE;
									uifc.changes=1;
								}
								break;
							case 10:
								n=cfg.dir[i]->misc&DIR_TFREE ? 0:1;
								uifc.helpbuf=
									"`Free Download Time:`\n"
									"\n"
									"If you would like all downloads from this directory to not subtract\n"
									"time from the user, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Free Download Time",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_TFREE)) {
									cfg.dir[i]->misc|=DIR_TFREE;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_TFREE)) {
									cfg.dir[i]->misc&=~DIR_TFREE;
									uifc.changes=1;
								}
								break;
							case 11:
								n=cfg.dir[i]->misc&DIR_ULTIME ? 0:1;
								uifc.helpbuf=
									"`Deduct Upload Time:`\n"
									"\n"
									"If you would like all uploads to this directory to have the time spent\n"
									"uploading subtracted from their time online, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Deduct Upload Time",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_ULTIME)) {
									cfg.dir[i]->misc|=DIR_ULTIME;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_ULTIME)) {
									cfg.dir[i]->misc&=~DIR_ULTIME;
									uifc.changes=1;
								}
								break;
							case 12:
								n=cfg.dir[i]->misc&DIR_CDTUL ? 0:1;
								uifc.helpbuf=
									"`Give Credit for Uploads:`\n"
									"\n"
									"If you want users who upload to this directory to get credit for their\n"
									"initial upload, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Give Credit for Uploads",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_CDTUL)) {
									cfg.dir[i]->misc|=DIR_CDTUL;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_CDTUL)) {
									cfg.dir[i]->misc&=~DIR_CDTUL;
									uifc.changes=1;
								}
								break;
							case 13:
								n=cfg.dir[i]->misc&DIR_CDTDL ? 0:1;
								uifc.helpbuf=
									"`Give Uploader Credit for Downloads:`\n"
									"\n"
									"If you want users who upload to this directory to get credit when their\n"
									"files are downloaded, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Give Uploader Credit for Downloads",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_CDTDL)) {
									cfg.dir[i]->misc|=DIR_CDTDL;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_CDTDL)) {
									cfg.dir[i]->misc&=~DIR_CDTDL;
									uifc.changes=1;
								}
								break;
							case 14:
								n=cfg.dir[i]->misc&DIR_CDTMIN ? 0:1;
								uifc.helpbuf=
									"`Credit Uploader with Minutes instead of Credits:`\n"
									"\n"
									"If you wish to give the uploader of files to this directory minutes,\n"
									"instead of credits, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Credit Uploader with Minutes",uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_CDTMIN)) {
									cfg.dir[i]->misc|=DIR_CDTMIN;
									uifc.changes=1;
								}
								else if(n==1 && cfg.dir[i]->misc&DIR_CDTMIN){
									cfg.dir[i]->misc&=~DIR_CDTMIN;
									uifc.changes=1;
								}
								break;
							case 15:
								n=cfg.dir[i]->misc&DIR_QUIET ? 1:0;
								uifc.helpbuf=
									"`Send Download Notifications:`\n"
									"\n"
									"If you wish the BBS to send download notification messages to the\n"
									"uploader of a file to this directory, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Send Download Notifications",uifcYesNoOpts);
								if(n==1 && !(cfg.dir[i]->misc&DIR_QUIET)) {
									cfg.dir[i]->misc|=DIR_QUIET;
									uifc.changes=1; 
								} else if(n==0 && cfg.dir[i]->misc&DIR_QUIET){
									cfg.dir[i]->misc&=~DIR_QUIET;
									uifc.changes=1; 
								}
								break;


							case 16:
								n=cfg.dir[i]->misc&DIR_ANON ? (cfg.dir[i]->misc&DIR_AONLY ? 2:0):1;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								strcpy(opt[2],"Only");
								opt[3][0]=0;
								uifc.helpbuf=
									"`Allow Anonymous Uploads:`\n"
									"\n"
									"If you want users with the `A` exemption to be able to upload anonymously\n"
									"to this directory, set this option to `Yes`. If you want all uploads to\n"
									"this directory to be forced anonymous, set this option to `Only`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Allow Anonymous Uploads",opt);
								if(n==0 && (cfg.dir[i]->misc&(DIR_ANON|DIR_AONLY))
									!=DIR_ANON) {
									cfg.dir[i]->misc|=DIR_ANON;
									cfg.dir[i]->misc&=~DIR_AONLY;
									uifc.changes=1;
								}
								else if(n==1 && cfg.dir[i]->misc&(DIR_ANON|DIR_AONLY)){
									cfg.dir[i]->misc&=~(DIR_ANON|DIR_AONLY);
									uifc.changes=1;
								}
								else if(n==2 && (cfg.dir[i]->misc&(DIR_ANON|DIR_AONLY))
									!=(DIR_ANON|DIR_AONLY)) {
									cfg.dir[i]->misc|=(DIR_ANON|DIR_AONLY);
									uifc.changes=1;
								}
								break;
							case 17:
								n=cfg.dir[i]->misc&DIR_SINCEDL ? 0:1;
								uifc.helpbuf=
									"`Purge Files Based on Date of Last Download:`\n"
									"\n"
									"Using the Synchronet file base maintenance utility (`DELFILES`), you can\n"
									"have files removed based on the number of days since last downloaded\n"
									"rather than the number of days since the file was uploaded (default),\n"
									"by setting this option to `Yes`.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Purge Files Based on Date of Last Download"
									,uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_SINCEDL)) {
									cfg.dir[i]->misc|=DIR_SINCEDL;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_SINCEDL)) {
									cfg.dir[i]->misc&=~DIR_SINCEDL;
									uifc.changes=1;
								}
								break;
							case 18:
								n=cfg.dir[i]->misc&DIR_MOVENEW ? 0:1;
								uifc.helpbuf=
									"`Mark Moved Files as New:`\n"
									"\n"
									"If this option is set to `Yes`, then all files moved `from` this directory\n"
									"will have their upload date changed to the current date so the file will\n"
									"appear in users' new-file scans again.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Mark Moved Files as New"
									,uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_MOVENEW)) {
									cfg.dir[i]->misc|=DIR_MOVENEW;
									uifc.changes=1;
								}
								else if(n==1 && (cfg.dir[i]->misc&DIR_MOVENEW)) {
									cfg.dir[i]->misc&=~DIR_MOVENEW;
									uifc.changes=1;
								}
								break;
							case 19:
								n=cfg.dir[i]->misc&DIR_NOSTAT ? 1:0;
								uifc.helpbuf=
									"`Include Transfers In System Statistics:`\n"
									"\n"
									"If this option is set to ~Yes~, then all files uploaded to or downloaded\n"
									"from this directory will be included in the system's daily and\n"
									"cumulative statistics.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Include Transfers In System Statistics"
									,uifcYesNoOpts);
								if(n==1 && !(cfg.dir[i]->misc&DIR_NOSTAT)) {
									cfg.dir[i]->misc|=DIR_NOSTAT;
									uifc.changes=1; 
								} else if(n==0 && cfg.dir[i]->misc&DIR_NOSTAT){
									cfg.dir[i]->misc&=~DIR_NOSTAT;
									uifc.changes=1; 
								}
								break;
							case 20:
								n=cfg.dir[i]->misc&DIR_FILES ? 0:1;
								uifc.helpbuf=
									"`Allow Access to Files Not in Database:`\n"
									"\n"
									"If this option is set to ~Yes~, then all files in this directory's\n"
									"`Transfer File Path` will be visible/downloadable by users with access to\n"
									"this directory.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Allow Access to Files Not in Database"
									,uifcYesNoOpts);
								if(n==0 && !(cfg.dir[i]->misc&DIR_FILES)) {
									cfg.dir[i]->misc|=DIR_FILES;
									uifc.changes=1; 
								} else if(n==1 && cfg.dir[i]->misc&DIR_FILES){
									cfg.dir[i]->misc&=~DIR_FILES;
									uifc.changes=1; 
								}
								break;
							case 21:
								n=(cfg.dir[i]->misc&DIR_TEMPLATE) ? 0:1;
								uifc.helpbuf=
									"`Use this Directory as a Template for New Dirs:`\n"
									"\n"
									"If you want this directory's options / settings to be used as the\n"
									"template for newly-created or cloned directories in this file library,\n"
									"set this option to `Yes`.\n"
									"\n"
									"If multiple directories have this option enabled, only the first will be\n"
									"used as the template."
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Use this Directory as a Template for New Dirs",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.dir[i]->misc&DIR_TEMPLATE)) {
									uifc.changes = TRUE;
									cfg.dir[i]->misc|=DIR_TEMPLATE;
									break; 
								}
								if(n==1 && cfg.dir[i]->misc&DIR_TEMPLATE) {
									uifc.changes = TRUE;
									cfg.dir[i]->misc&=~DIR_TEMPLATE; 
								}
								break;
						} 
					}
					break;
			case 14:
				while(1) {
					n=0;
					sprintf(opt[n++],"%-27.27s%s","Extensions Allowed"
						,cfg.dir[i]->exts);
					if(!cfg.dir[i]->data_dir[0])
						sprintf(str,"[%sdirs/]",cfg.data_dir);
					else
						strcpy(str,cfg.dir[i]->data_dir);
					sprintf(opt[n++],"%-27.27s%.40s","Data Directory"
						,str);
					sprintf(opt[n++],"%-27.27s%.40s","Upload Semaphore File"
						,cfg.dir[i]->upload_sem);
					sprintf(opt[n++],"%-27.27s%s","Sort Value and Direction"
						, cfg.dir[i]->sort==SORT_NAME_A ? "Name Ascending"
						: cfg.dir[i]->sort==SORT_NAME_D ? "Name Descending"
						: cfg.dir[i]->sort==SORT_DATE_A ? "Date Ascending"
						: "Date Descending");
					sprintf(opt[n++],"%-27.27sNow %u / Was %u","Directory Index", i, cfg.dir[i]->dirnum);
					opt[n][0]=0;
					uifc.helpbuf=
						"`Directory Advanced Options:`\n"
						"\n"
						"This is the advanced options menu for the selected file directory.\n"
					;
						n=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,4,60,&adv_dflt,0
							,"Advanced Options",opt);
						if(n==-1)
							break;
						switch(n) {
							case 0:
								uifc.helpbuf=
									"`File Extensions Allowed:`\n"
									"\n"
									"This option allows you to limit the types of files uploaded to this\n"
									"directory. This is a list of file extensions that are allowed, each\n"
									"separated by a comma (Example: `ZIP,EXE`). If this option is left\n"
									"blank, all file extensions will be allowed to be uploaded.\n"
								;
								uifc.input(WIN_L2R|WIN_SAV,0,17
									,"File Extensions Allowed"
									,cfg.dir[i]->exts,sizeof(cfg.dir[i]->exts)-1,K_EDIT);
								break;
							case 1:
								uifc.helpbuf=
									"`Data Directory:`\n"
									"\n"
									"Use this if you wish to place the data directory for this directory\n"
									"on another drive or in another directory besides the default setting.\n"
								;
								uifc.input(WIN_MID|WIN_SAV,0,17,"Data"
									,cfg.dir[i]->data_dir,sizeof(cfg.dir[i]->data_dir)-1,K_EDIT);
								break;
							case 2:
								uifc.helpbuf=
									"`Upload Semaphore File:`\n"
									"\n"
									"This is a filename that will be used as a semaphore (signal) to your\n"
									"FidoNet front-end that new files are ready to be hatched for export.\n"
								;
								uifc.input(WIN_MID|WIN_SAV,0,17,"Upload Semaphore"
									,cfg.dir[i]->upload_sem,sizeof(cfg.dir[i]->upload_sem)-1,K_EDIT);
								break;
							case 3:
								n=0;
								strcpy(opt[0],"Name Ascending");
								strcpy(opt[1],"Name Descending");
								strcpy(opt[2],"Date Ascending");
								strcpy(opt[3],"Date Descending");
								opt[4][0]=0;
								uifc.helpbuf=
									"`Sort Value and Direction:`\n"
									"\n"
									"This option allows you to determine the sort value and direction. Files\n"
									"that are uploaded are automatically sorted by filename or upload date,\n"
									"ascending or descending. If you change the sort value or direction after\n"
									"a directory already has files in it, use the sysop transfer menu `;RESORT`\n"
									"command to resort the directory with the new sort parameters.\n"
								;
								n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0
									,"Sort Value and Direction",opt);
								if(n==0 && cfg.dir[i]->sort!=SORT_NAME_A) {
									cfg.dir[i]->sort=SORT_NAME_A;
									uifc.changes=1;
								}
								else if(n==1 && cfg.dir[i]->sort!=SORT_NAME_D) {
									cfg.dir[i]->sort=SORT_NAME_D;
									uifc.changes=1;
								}
								else if(n==2 && cfg.dir[i]->sort!=SORT_DATE_A) {
									cfg.dir[i]->sort=SORT_DATE_A;
									uifc.changes=1;
								}
								else if(n==3 && cfg.dir[i]->sort!=SORT_DATE_D) {
									cfg.dir[i]->sort=SORT_DATE_D;
									uifc.changes=1;
								}
								break; 
							case 4:
								uifc.msg("This value cannot be changed.");
								break;
						} 
					}
				break;
			} 
		} 
	}
}
