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

#include "scfg.h"
#include "ciolib.h" // CIO_KEY_*
#include <stdbool.h>

#define CUT_LIBNUM  USHRT_MAX
#define ADDFILES_HELP "Help adding files to directories, see `http://wiki.synchro.net/faq:files`"

static const char* strDuplicateLibName = "Library already exists with that name!";

void dir_defaults_cfg(int libnum);

char*        file_sort_desc[] = {
	"Name Ascending (case-insensitive)",
	"Name Descending (case-insensitive)",
	"Date Ascending",
	"Date Descending",
	"Name Ascending (case-sensitive)",
	"Name Descending (case-sensitive)",
	"Size Ascending",
	"Size Descending",
	"None (unsorted natural index order)",
	NULL
};

char*        vdir_name_desc[] = {
	"Internal Code Suffix",
	"Short Name",
	"Long Name",
	NULL
};

static char* up_pct_help =
	"`Percentage of Credits to Credit Uploader on Upload:`\n"
	"\n"
	"This is the percentage of a file's credit value that is given to users\n"
	"when they upload files.  Most often, this value will be set to `100` to\n"
	"give full credit value (100%) for uploads.\n"
	"\n"
	"If you want uploaders to receive no credits upon upload, set this value\n"
	"to `0`.\n"
;

static char* dn_pct_help =
	"`Percentage of Credits to Credit Uploader on Download:`\n"
	"\n"
	"This is the percentage of a file's credit value that is given to users\n"
	"who upload a file that is later downloaded by another user.  This is an\n"
	"award type system where more popular files will generate more credits\n"
	"for the uploader.\n"
	"\n"
	"If you do not want uploaders to receive credit when files they upload\n"
	"are later downloaded, set this value to `0`.\n"
;

static char* max_age_help =
	"`Maximum Age of Files:`\n"
	"\n"
	"This value is the maximum number of days that files will be kept in\n"
	"the directory based on the date the file was uploaded or last\n"
	"downloaded (If the `Purge by Last Download` toggle option is used).\n"
	"\n"
	"The Synchronet file base maintenance program (`delfiles`) must be used\n"
	"to automatically remove files based on age.\n"
;

static char* max_files_help =
	"`Maximum Number of Files:`\n"
	"\n"
	"This value is the maximum number of files allowed in this directory.\n"
	"\n"
	"The value `0` indicates an unlimited number of files will be allowed."
	"\n"
	ADDFILES_HELP
;

static char* file_ext_help =
	"`File Extensions Allowed:`\n"
	"\n"
	"This option allows you to limit the types of files uploaded to this\n"
	"directory.  This is a list of file extensions that are allowed, each\n"
	"separated by a comma (Example: `ZIP,EXE`).  If this option is left\n"
	"blank, all file extensions will be allowed to be uploaded.\n"
	"\n"
	ADDFILES_HELP
;

static char* data_dir_help =
	"`Data Directory:`\n"
	"\n"
	"Use this if you wish to place the data directory for this directory\n"
	"on another drive or in another directory besides the default setting.\n"
	"\n"
	ADDFILES_HELP
;

static char* upload_sem_help =
	"`Upload Semaphore File:`\n"
	"\n"
	"This is a filename that will be used as a semaphore (signal) to your\n"
	"FidoNet software that new files are ready to be hatched for export.\n"
	"\n"
	"`Command line specifiers may be included in the semaphore filename.`\n"
	SCFG_CMDLINE_SPEC_HELP
;

static char* sort_help =
	"`Sort Value and Direction:`\n"
	"\n"
	"This option allows you to determine the sort value and direction for\n"
	"the display of file listings.\n"
	"\n"
	"The dates available for sorting are the file import/upload date/time.\n"
	"\n"
	"The natural (and thus fastest) sort order is `None`, which is normally\n"
	"the order in which the files were added/imported into the filebase."
;

static bool new_dir(unsigned new_dirnum, unsigned libnum)
{
	dir_t* new_directory = malloc(sizeof(dir_t));
	if (new_directory == NULL) {
		errormsg(WHERE, ERR_ALLOC, "directory", sizeof(*new_directory));
		return false;
	}
	*new_directory = cfg.lib[libnum]->dir_defaults;
	new_directory->lib = libnum;

	dir_t** new_dir_list;
	if ((new_dir_list = (dir_t **)realloc(cfg.dir, sizeof(dir_t *) * (cfg.total_dirs + 1))) == NULL) {
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

static int next_dirnum(dir_t* dir)
{
	for (int i = dir->dirnum + 1; i < cfg.total_dirs; i++) {
		if (cfg.dir[i]->lib == dir->lib)
			return i;
	}
	return dir->dirnum;
}

static int prev_dirnum(dir_t* dir)
{
	for (int i = dir->dirnum - 1; i >= 0; i--) {
		if (cfg.dir[i]->lib == dir->lib)
			return i;
	}
	return dir->dirnum;
}

static bool code_prefix_exists(const char* prefix)
{
	int i;

	for (i = 0; i < cfg.total_libs; i++)
		if (cfg.lib[i]->code_prefix[0] && stricmp(cfg.lib[i]->code_prefix, prefix) == 0)
			return true;
	return false;
}

static bool new_lib(int new_libnum)
{
	lib_t* new_library = malloc(sizeof(lib_t));
	if (new_library == NULL) {
		errormsg(WHERE, ERR_ALLOC, "library", sizeof(lib_t));
		return false;
	}
	memset(new_library, 0, sizeof(*new_library));
	new_library->dir_defaults.misc = DEFAULT_DIR_OPTIONS;
	new_library->dir_defaults.up_pct = cfg.cdt_up_pct;
	new_library->dir_defaults.dn_pct = cfg.cdt_dn_pct;

	lib_t** new_lib_list;
	if ((new_lib_list = (lib_t **)realloc(cfg.lib, sizeof(lib_t *) * (cfg.total_libs + 1))) == NULL) {
		free(new_library);
		errormsg(WHERE, ERR_ALLOC, "library list", cfg.total_libs + 1);
		return false;
	}
	cfg.lib = new_lib_list;

	for (int u = cfg.total_libs; u > new_libnum; u--)
		cfg.lib[u] = cfg.lib[u - 1];

	if (new_libnum != cfg.total_libs) { /* Inserting library? Renumber (higher) existing libs */
		for (int j = 0; j < cfg.total_dirs; j++) {
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
	char     path[MAX_PATH + 1];
	char*    p;
	glob_t   g;
	unsigned gi;

	SAFECOPY(path, dir);
	backslash(path);
	SAFECAT(path, ALLFILES);

	glob(path, GLOB_MARK, NULL, &g);
	for (gi = 0; gi < g.gl_pathc; gi++) {
		if (*lastchar(g.gl_pathv[gi]) == '/') {
			if (include_empty_dirs ||
			    getdirsize(g.gl_pathv[gi], /* include_subdirs */ FALSE, /* subdir_only */ FALSE) > 0) {
				SAFECOPY(path, g.gl_pathv[gi] + strlen(parent));
				p = lastchar(path);
				if (IS_PATH_DELIM(*p))
					*p = 0;
				fprintf(fp, "%s\n", path);
			}
			if (max_depth == 0 || depth + 1 < max_depth) {
				append_dir_list(parent, g.gl_pathv[gi], fp, depth + 1, max_depth, include_empty_dirs);
			}
		}
	}
	globfree(&g);
}

BOOL create_raw_dir_list(char* list_file, const char* parent)
{
	char  path[MAX_PATH + 1];
	char  fname[MAX_PATH + 1] = "dirs.raw";
	char* p;
	int   k = 0;
	bool  include_empty_dirs;
	FILE* fp;

	if (parent == NULL) {
		SAFECOPY(path, list_file);
		if ((p = getfname(path)) == NULL)
			return FALSE;
		SAFECOPY(fname, p);
		*p = 0;
		parent = path;
	}

	k = 1;
	k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, "Include Empty Directories", uifcYesNoOpts);
	if (k < 0)
		return FALSE;
	include_empty_dirs = (k == 0);
	k = 0;
	k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, "Recursive Directory Search", uifcYesNoOpts);
	if (k < 0)
		return FALSE;
	if ((fp = fopen(list_file, "w")) == NULL) {
		strcpy(list_file, fname);
		if ((fp = fopen(list_file, "w")) == NULL) {
			uifc.msgf("Create Failure (%u): %s", errno, list_file);
			return FALSE;
		}
	}

	uifc.pop("Scanning Directories...");
	append_dir_list(parent, parent, fp, /* depth: */ 0, /* max_depth: */ k, include_empty_dirs);
	uifc.pop(NULL);
	fclose(fp);
	return TRUE;
}

int dirs_in_lib(int libnum)
{
	int total = 0;

	for (int u = 0; u < cfg.total_dirs; u++)
		if (cfg.dir[u]->lib == libnum)
			total++;
	return total;
}

static bool permutate_sname(char* name)
{
	const char* set = " _-:.;/+|*=";

	for(const char* s = set; *(s + 1) != '\0'; ++s) {
		for(char* p = name; *p != '\0'; ++p) {
			if (*p != *s)
				continue;
			*p = *(s + 1);
			return true;
		}
	}
	return false;
}

static char* find_last_fit(char* p, size_t maxlen)
{
	size_t len;

	if (p == NULL)
		return p;
	/* skip first sub-dir(s) */
	char* tp = p;
	while (strlen(tp) > maxlen) {
		FIND_CHAR(tp, '/');
		SKIP_CHAR(tp, '/');
	}
	if (*tp != '\0')
		p = tp;
	if ((len = strlen(p)) > maxlen)
		p += len - maxlen;
	FIND_ALPHANUMERIC(p);
	return p;
}

static bool get_parent(char* parent, bool required)
{
	char path[LEN_DIR + 2];

	SAFECOPY(path, parent);
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Parent Directory", path, LEN_DIR, K_EDIT) < required)
		return false;
	if (*path != '\0' && !getdircase(path)) {
		uifc.msgf("Directory doesn't exist: %s", path);
		if (required)
			return false;
	}
	strlcpy(parent, path, LEN_DIR);
	return true;
}

void xfer_cfg()
{
	static int   libs_dflt, libs_bar, dflt, bar;
	char         str[256], done = 0, *p;
	char         path[MAX_PATH + 1];
	char         tmp_code[MAX_PATH + 1];
	int          file, j, k, q;
	int          i;
	long         ported, added;
	static lib_t savlib;
	dir_t        tmpdir;
	FILE *       stream;

	char*        lib_long_name_help =
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
		"Code prefixes may contain up to 16 legal filename characters.\n"
		"\n"
		"Code prefixes should be unique among the file libraries on the system.\n"
		"\n"
		"Changing a library's code prefix implicitly changes all the internal\n"
		"code of the directories within the library, so change this value with\n"
		"caution.\n"
	;

	enum dirlist_type {
		DIRLIST_CDROM,
		DIRLIST_FIDO,
		DIRLIST_RAW
	};

	while (1) {
		for (i = 0; i < cfg.total_libs && i < MAX_OPTS; i++)
			snprintf(opt[i], MAX_OPLN, "%-*s %5u", LEN_GLNAME, cfg.lib[i]->lname, dirs_in_lib(i));
		opt[i][0] = 0;
		j = WIN_ACT | WIN_CHE | WIN_ORG;
		if (cfg.total_libs)
			j |= WIN_DEL | WIN_COPY | WIN_CUT | WIN_DELACT;
		if (cfg.total_libs < MAX_OPTS)
			j |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savlib.sname[0])
			j |= WIN_PASTE | WIN_PASTEXTR;
		uifc.helpbuf =
			"`File Libraries:`\n"
			"\n"
			"This is a listing of file libraries for your BBS.   File Libraries are\n"
			"used to logically separate your file `directories` into groups.  Every\n"
			"directory (database of files available for transfer) must be a child of\n"
			"a file library.\n"
			"\n"
			"To add a library, select the desired position with the arrow keys and\n"
			"hit ~ INS ~.\n"
			"\n"
			"To delete a library, select it with the arrow keys and hit ~ DEL ~.\n"
			"\n"
			"To configure a library, select it with the arrow keys and hit ~ ENTER ~.\n"
			"\n"
			ADDFILES_HELP
		;
		i = uifc.list(j, 0, 0, 0, &libs_dflt, &libs_bar, "File Libraries                                         Directories", opt);
		if ((signed)i == -1) {
			j = save_changes(WIN_MID);
			if (j == -1)
				continue;
			if (!j) {
				save_file_cfg(&cfg);
				refresh_cfg(&cfg);
			}
			return;
		}
		int libnum = i & MSK_OFF;
		int msk = i & MSK_ON;
		if (msk == MSK_INS) {
			char long_name[LEN_GLNAME + 1];
			uifc.helpbuf = lib_long_name_help;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Library Long Name", long_name, LEN_GLNAME, K_NONE) < 1)
				continue;

			char short_name[LEN_GSNAME + 1];
			SAFECOPY(short_name, long_name);
			uifc.helpbuf = lib_short_name_help;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Library Short Name", short_name, LEN_GSNAME, K_EDIT) < 1)
				continue;
			if (libnum_is_valid(&cfg, getlibnum_from_name(&cfg, short_name))) {
				uifc.msg(strDuplicateLibName);
				continue;
			}
			char code_prefix[LEN_EXTCODE + 1];    /* purposely extra-long */
			SAFECOPY(code_prefix, short_name);
			prep_code(code_prefix, NULL);
			if (strlen(code_prefix) < LEN_CODE)
				SAFECAT(code_prefix, "_");
			uifc.helpbuf = lib_code_prefix_help;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Internal Code Prefix", code_prefix, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE) < 0)
				continue;
			if (code_prefix_exists(code_prefix)) {
				uifc.msg(strDuplicateCodePrefix);
				continue;
			}
			if (code_prefix[0] != 0 && !code_ok(code_prefix)) {
				uifc.helpbuf = invalid_code;
				uifc.msg(strInvalidCodePrefix);
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
		if (msk == MSK_DEL || msk == MSK_CUT) {
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
							uifc.pop("Deleting %s/%s ...", tmp, str);
							delfiles(tmp, str, /* keep: */ 0);
							uifc.pop(NULL);
						}
					}
				}
			}
			if (msk == MSK_CUT)
				savlib = *cfg.lib[libnum];
			free(cfg.lib[libnum]);
			if (msk == MSK_DEL) {
				for (j = 0; j < cfg.total_dirs;) {
					if (cfg.dir[j]->lib == libnum) {
						free(cfg.dir[j]);
						cfg.total_dirs--;
						k = j;
						while (k < cfg.total_dirs) {
							cfg.dir[k] = cfg.dir[k + 1];
							k++;
						}
					}
					else
						j++;
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
				cfg.lib[i] = cfg.lib[i + 1];
			uifc.changes = 1;
			continue;
		}
		if (msk == MSK_COPY) {
			savlib = *cfg.lib[libnum];
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
			uifc.changes = 1;
			continue;
		}
		done = 0;
		while (!done) {
			j = 0;
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Long Name", cfg.lib[libnum]->lname);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Short Name", cfg.lib[libnum]->sname);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Internal Code Prefix", cfg.lib[libnum]->code_prefix);
			if (cfg.lib[libnum]->parent_path[0] == '\0')
				snprintf(str, sizeof str, "[%sdirs/]", cfg.data_dir);
			else
				SAFECOPY(str, cfg.lib[libnum]->parent_path);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Parent Directory", str);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Access Requirements"
			         , cfg.lib[libnum]->arstr);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Upload Requirements"
			         , cfg.lib[libnum]->ul_arstr);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Download Requirements"
			         , cfg.lib[libnum]->dl_arstr);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Operator Requirements"
			         , cfg.lib[libnum]->op_arstr);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Exemption Requirements"
			         , cfg.lib[libnum]->ex_arstr);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Auto-Add Sub-directories"
			         , cfg.lib[libnum]->parent_path[0] ? (cfg.lib[libnum]->misc & LIB_DIRS ? "Yes":"No") : "N/A");
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Sort Library By Directory", area_sort_desc[cfg.lib[libnum]->sort]);
			snprintf(opt[j++], MAX_OPLN, "%-27.27s%s", "Virtual Sub-directories", vdir_name_desc[cfg.lib[libnum]->vdir_name]);
			strcpy(opt[j++], "Export Areas...");
			strcpy(opt[j++], "Import Areas...");
			strcpy(opt[j++], "Directory Defaults...");
			strcpy(opt[j++], "File Directories...");
			opt[j][0] = 0;
			sprintf(str, "%s Library", cfg.lib[libnum]->sname);
			uifc.helpbuf =
				"`File Library Configuration:`\n"
				"\n"
				"This menu allows you to configure the security requirements for access\n"
				"to this file library.  You can also add, delete, and configure the\n"
				"directories of this library by selecting the `File Directories...` option.\n"
				"\n"
				"The left and right arrow keys may be used to cycle through file\n"
				"libraries.\n"
				"\n"
				ADDFILES_HELP
			;
			uifc_winmode_t wmode = WIN_BOT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
			if (libnum > 0)
				wmode |= WIN_LEFTKEY;
			if (libnum + 1 < cfg.total_libs)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 72, &dflt, &bar, str, opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					if (libnum > 0)
						libnum--;
					break;
				case -CIO_KEY_RIGHT - 2:
					if (libnum + 1 < cfg.total_libs)
						libnum++;
					break;
				case __COUNTER__:
					uifc.helpbuf = lib_long_name_help;
					SAFECOPY(str, cfg.lib[libnum]->lname);
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Name to use for Listings"
					               , str, LEN_GLNAME, K_EDIT) > 0)
						SAFECOPY(cfg.lib[libnum]->lname, str);
					break;
				case __COUNTER__:
					uifc.helpbuf = lib_short_name_help;
					SAFECOPY(str, cfg.lib[libnum]->sname);
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Name to use for Prompts"
					               , str, LEN_GSNAME, K_EDIT | K_CHANGED) > 0) {
						if (libnum_is_valid(&cfg, getlibnum_from_name(&cfg, str)))
							uifc.msg(strDuplicateLibName);
						else
							SAFECOPY(cfg.lib[libnum]->sname, str);
					}
					break;
				case __COUNTER__:
				{
					char code_prefix[LEN_CODE + 1];
					SAFECOPY(code_prefix, cfg.lib[libnum]->code_prefix);
					uifc.helpbuf = lib_code_prefix_help;
					if (uifc.input(WIN_MID | WIN_SAV, 0, 17, "Internal Code Prefix"
					               , code_prefix, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE | K_CHANGED) < 0)
						continue;
					if (code_prefix_exists(code_prefix))
						uifc.msg(strDuplicateCodePrefix);
					else if (code_prefix[0] == 0 || code_ok(code_prefix)) {
						SAFECOPY(cfg.lib[libnum]->code_prefix, code_prefix);
						for (j = 0; j < cfg.total_dirs; j++) {
							if (cfg.dir[j]->lib == libnum)
								cfg.dir[j]->cfg_modified = true;
						}
					} else {
						uifc.helpbuf = invalid_code;
						uifc.msg(strInvalidCodePrefix);
					}
					break;
				}
				case __COUNTER__:
					uifc.helpbuf =
						"`Parent Directory:`\n"
						"\n"
						"This an optional path to be used as the default physical \"parent\"\n"
						"for logical directories contained within this library.  This path\n"
						"will be automatically prepended to each directory's `Actual File Path`\n"
						"(when relative) to create a full/absolute physical storage path for\n"
						"files in that directory.\n"
						"\n"
						"This option is convenient for adding libraries with many directories\n"
						"that share a common parent directory (e.g. CD-ROMs) and gives you the\n"
						"option of easily changing the common parent path later, if desired.\n"
						"\n"
						"The library's `Parent Directory` is not used for any directories of\n"
						"the library that have been configured with an absolute (not relative)\n"
						"`Actual File Path`."
					;
					get_parent(cfg.lib[libnum]->parent_path, /* required: */ false);
					break;
				case __COUNTER__:
					sprintf(str, "%s Library Access", cfg.lib[libnum]->sname);
					getar(str, cfg.lib[libnum]->arstr);
					break;
				case __COUNTER__:
					sprintf(str, "%s Library Upload", cfg.lib[libnum]->sname);
					getar(str, cfg.lib[libnum]->ul_arstr);
					break;
				case __COUNTER__:
					sprintf(str, "%s Library Download", cfg.lib[libnum]->sname);
					getar(str, cfg.lib[libnum]->dl_arstr);
					break;
				case __COUNTER__:
					sprintf(str, "%s Library Operator", cfg.lib[libnum]->sname);
					getar(str, cfg.lib[libnum]->op_arstr);
					break;
				case __COUNTER__:
					sprintf(str, "%s Library Exemption", cfg.lib[libnum]->sname);
					getar(str, cfg.lib[libnum]->ex_arstr);
					break;
				case __COUNTER__:
					uifc.helpbuf =
						"`Auto-Add Sub-directories:`\n"
						"\n"
						"When a Library's parent directory has been defined, sub-directories of\n"
						"that parent directory may be automatically added as directories of the\n"
						"library when this option is set to `Yes`.\n"
						"\n"
						"The directory's names and code suffix are derived from the name of the\n"
						"sub-directory.\n"
						"\n"
						"~ This is an experimental feature. ~"
					;
					if (!isdir(cfg.lib[libnum]->parent_path)) {
						uifc.msg("A valid parent directory must be specified to use this feature");
						break;
					}
					j = (cfg.lib[libnum]->misc & LIB_DIRS) ? 0 : 1;
					j = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &j, 0
					              , "Automatically Add Sub-directories of Parent Directory"
					              , uifcYesNoOpts);
					if (j == 0 && (cfg.lib[libnum]->misc & LIB_DIRS) == 0) {
						cfg.lib[libnum]->misc |= LIB_DIRS;
						uifc.changes = true;
					} else if (j == 1 && (cfg.lib[libnum]->misc & LIB_DIRS) != 0) {
						cfg.lib[libnum]->misc &= ~LIB_DIRS;
						uifc.changes = true;
					}
					break;
				case __COUNTER__:
					uifc.helpbuf = "`Sort Library By Directory:`\n"
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
					j = cfg.lib[libnum]->sort;
					j = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &j, 0, "Sort Library By Directory", area_sort_desc);
					if (j >= 0) {
						cfg.lib[libnum]->sort = j;
						sort_dirs(libnum);
						uifc.changes = TRUE;
					}
					break;
				case __COUNTER__:
					uifc.helpbuf = "`Source of Virtual Sub-directory Names:`\n"
					               "\n"
					               "File areas accessed via the FTP server or Web server are represented\n"
					               "by a virtual path.  This setting determines the source of the\n"
					               "sub-directory portion of the virtual paths used to represent directories\n"
					               "within this file library.\n"
					;
					j = cfg.lib[libnum]->vdir_name;
					j = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &j, 0, "Source of Virtual Sub-directory Names", vdir_name_desc);
					if (j >= 0) {
						cfg.lib[libnum]->vdir_name = j;
						uifc.changes = TRUE;
					}
					break;
				case __COUNTER__:
	#define DIRS_CDR_HELP_TEXT      "`DIRS.TXT` is a text file containing a list of directory names and\n" \
			"descriptions (one per line) included on CD-ROMs.\n" \
			"A file of this format is sometimes named `DIRS.WIN` or `00_INDEX.TXT`.\n"
	#define FILEGATE_ZXX_HELP_TEXT  "`FILEGATE.ZXX` is a plain text file in the old RAID/FILEBONE.NA format\n" \
			"which describes a list of file areas connected across a File\n" \
			"Distribution Network (e.g. Fidonet).\n"
					k = 0;
					ported = 0;
					q = uifc.changes;
					strcpy(opt[k++], "CD-ROM    DIRS.TXT");
					strcpy(opt[k++], "FidoNet   FILEGATE.ZXX");
					opt[k][0] = 0;
					uifc.helpbuf =
						"`Export Area File Format:`\n"
						"\n"
						"This menu allows you to choose the format of the area file you wish to\n"
						"export to.\n"
						"\n"
						DIRS_CDR_HELP_TEXT
						"\n"
						FILEGATE_ZXX_HELP_TEXT
					;
					static int export_cur;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &export_cur, 0
					              , "Export Area File Format", opt);
					if (k == -1)
						break;
					if (k == DIRLIST_CDROM) {
						SAFECOPY(str, cfg.lib[libnum]->parent_path);
						backslash(str);
						SAFECAT(str, "DIRS.TXT");
					}
					else if (k == DIRLIST_FIDO)
						snprintf(str, sizeof str, "FILEGATE.ZXX");
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Filename"
					               , str, sizeof(str) - 1, K_EDIT) <= 0) {
						uifc.changes = q;
						break;
					}
					if (fexist(str)) {
						strcpy(opt[0], "Overwrite");
						strcpy(opt[1], "Append");
						opt[2][0] = 0;
						j = 0;
						j = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &j, 0
						              , "File Exists", opt);
						if (j == -1)
							break;
						if (j == 0)
							j = O_WRONLY | O_TRUNC;
						else
							j = O_WRONLY | O_APPEND;
					}
					else
						j = O_WRONLY | O_CREAT;
					if ((stream = fnopen(&file, str, j | O_TEXT)) == NULL) {
						uifc.msg("Open Failure");
						break;
					}
					uifc.pop("Exporting Areas...");
					for (j = 0; j < cfg.total_dirs; j++) {
						if (cfg.dir[j]->lib != libnum)
							continue;
						ported++;
						if (k == DIRLIST_CDROM)
							fprintf(stream, "%-30s %s\n", cfg.dir[j]->path, cfg.dir[j]->lname);
						else if (k == DIRLIST_FIDO)
							fprintf(stream, "Area %-*s  0     !      %s\n"
							        , FIDO_AREATAG_LEN
							        , dir_area_tag(&cfg, cfg.dir[j], str, sizeof(str)), cfg.dir[j]->lname);
					}
					fclose(stream);
					uifc.pop(NULL);
					sprintf(str, "%lu File Areas Exported Successfully", ported);
					uifc.msg(str);
					uifc.changes = q;
					break;

				case __COUNTER__:
					ported = added = 0;
					k = 0;
					uifc.helpbuf =
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
						DIRS_CDR_HELP_TEXT
						"\n"
						FILEGATE_ZXX_HELP_TEXT
					;
					strcpy(opt[k++], "CD-ROM    DIRS.TXT, DIRS.WIN, 00_INDEX.TXT");
					strcpy(opt[k++], "FidoNet   FILEGATE.ZXX");
					strcpy(opt[k++], "Raw       DIRS.RAW");
					strcpy(opt[k++], "Directory Listing...");
					opt[k][0] = 0;
					static int import_cur;
					k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &import_cur, 0
					              , "Import Area File Format", opt);
					if (k == -1)
						break;
					char parent_buf[MAX_PATH + 1];
					SAFECOPY(parent_buf, cfg.lib[libnum]->parent_path);
					char* parent = parent_buf;
					if (!get_parent(parent, /* required: */ true))
						break;
					if (cfg.lib[libnum]->parent_path[0] == '\0'
						|| paths_are_same(parent, cfg.lib[libnum]->parent_path)) {
						SAFECOPY(cfg.lib[libnum]->parent_path, parent);
						parent = cfg.lib[libnum]->parent_path;
					}
					// 'parent' should be properly (back)slash-terminated at this point
					bool chk_dir_exist = true;
					if (k == DIRLIST_CDROM) {
						snprintf(str, sizeof str, "%sDIRS.WIN", parent);
						if (!fexistcase(str))
							snprintf(str, sizeof str, "%sDIRS.TXT", parent);
						if (!fexistcase(str))
							snprintf(str, sizeof str, "%s00_INDEX.TXT", parent);
					}
					else if (k == DIRLIST_FIDO) {
						sprintf(str, "FILEGATE.ZXX");
						chk_dir_exist = false;
					} else {
						snprintf(str, sizeof str, "%sdirs.raw", parent);
					}
					if (k > DIRLIST_RAW) {
						if (!create_raw_dir_list(str, parent))
							break;
						chk_dir_exist = false;
					} else {
						filename_prompt:
						if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Filename", str, sizeof(str) - 1, K_EDIT) <= 0)
							break;
						if (!fexistcase(str)) {
							if (k == DIRLIST_RAW) {
								if (uifc.confirm("File doesn't exist, create it?")) {
									create_raw_dir_list(str, parent);
									chk_dir_exist = false;
								} else
									goto filename_prompt;
							}
							else  {
								uifc.msgf("File does not exist: %s", str);
								goto filename_prompt;
							}
						}
					}
					if ((stream = fnopen(&file, str, O_RDONLY)) == NULL) {
						uifc.msgf("Error %d opening %s", errno, str);
						break;
					}
					if (chk_dir_exist)
						chk_dir_exist = uifc.confirm("Check for each directory's existence");
					uifc.pop("Importing Areas...");
					char duplicate_code[LEN_CODE + 1] = "";
					uint duplicate_codes = 0;   // consecutive duplicate codes
					bool prompt_on_dupe = true;
					int dir_count = dirs_in_lib(libnum);

					while (!feof(stream) && dir_count + added < MAX_OPTS) {
						if (!fgets(str, sizeof(str), stream))
							break;
						truncsp(str);
						p = str;
						SKIP_WHITESPACE(p);
						if (*p == '\0')
							continue;

						tmpdir = cfg.lib[libnum]->dir_defaults;

						if (k >= DIRLIST_RAW) { /* raw */
							int len = strlen(p);
							if (len > LEN_DIR)
								continue;
							SAFECOPY(tmp_code, p);
							SAFECOPY(tmpdir.path, p);
							/* skip first sub-dir(s) */
							char* tp = find_last_fit(p, LEN_SLNAME);
							if (*tp == '\0')
								SAFECOPY(tmpdir.lname, p);
							else
								SAFECOPY(tmpdir.lname, tp);
							tp = find_last_fit(p, LEN_SSNAME);
							if (*tp == '\0')
								SAFECOPY(tmpdir.sname, p);
							else
								SAFECOPY(tmpdir.sname, tp);
						}
						else if (k == DIRLIST_FIDO) {
							if (strnicmp(p, "AREA ", 5))
								continue;
							p += 5;
							while (*p && *p <= ' ') p++;
							SAFECOPY(tmp_code, p);
							truncstr(tmp_code, " \t");
							while (*p > ' ') p++;       /* Skip areaname */
							while (*p && *p <= ' ') p++; /* Skip space */
							while (*p > ' ') p++;       /* Skip level */
							while (*p && *p <= ' ') p++; /* Skip space */
							while (*p > ' ') p++;       /* Skip flags */
							while (*p && *p <= ' ') p++; /* Skip space */
							SAFECOPY(tmpdir.sname, tmp_code);
							SAFECOPY(tmpdir.area_tag, tmp_code);
							SAFECOPY(tmpdir.lname, p);
						}
						else if (k == DIRLIST_CDROM) { // CD-ROM DIRS.TXT (DIRS.WIN) format
							while (*p == '/' || *p == '\\') p++;
							char* tp = p + 1;
							FIND_WHITESPACE(tp);
							if (*tp != '\0') {
								*tp = '\0';
								tp++;
								FIND_ALPHANUMERIC(tp);
							}
							replace_chars(p, '\\', '/');
							if (*lastchar(p) == '/')
								*lastchar(p) = '\0';
							SAFECOPY(tmp_code, getfname(p));
							SAFECOPY(tmpdir.path, p);
							SAFECOPY(tmpdir.lname, *tp == '\0' ? p : tp);
							tp = find_last_fit(p, LEN_SSNAME);
							if (*tp == '\0')
								SAFECOPY(tmpdir.sname, p);
							else
								SAFECOPY(tmpdir.sname, tp);
						}

						if (tmpdir.lname[0] == 0)
							SAFECOPY(tmpdir.lname, tmp_code);
						if (tmpdir.sname[0] == 0)
							SAFECOPY(tmpdir.sname, tmp_code);

						SAFECOPY(tmpdir.code_suffix, prep_code(tmp_code, cfg.lib[libnum]->code_prefix));

						snprintf(path, sizeof path, "%s%s", parent, tmpdir.path);
#ifdef _WIN32
						REPLACE_CHARS(path, '/', '\\', p);
#endif
						if (chk_dir_exist && !getdircase(path)) {
							if(!uifc.confirm("%s is not a directory. Continue?", path))
								break;
							continue;
						}
						if (parent == cfg.lib[libnum]->parent_path)
							SAFECOPY(tmpdir.path, path + strlen(parent));
						else
							SAFECOPY(tmpdir.path, path);
						int attempts = 0;   // attempts to generate a unique internal code
						if (stricmp(tmpdir.code_suffix, duplicate_code) == 0)
							attempts = ++duplicate_codes;
						else
							duplicate_codes = 0;
						bool dupe_sname = false;
						for (j = 0; j < cfg.total_dirs && attempts < (36 * 36 * 36); j++) {
							if (cfg.dir[j]->lib == libnum) { /* same lib */
								if (tmpdir.path[0]
								    && strcmp(cfg.dir[j]->path, tmpdir.path) == 0)  /* same path? overwrite the dir entry */
									break;
								dupe_sname = stricmp(cfg.dir[j]->sname, tmpdir.sname) == 0;
								if (dupe_sname) {
									if (!permutate_sname(tmpdir.sname))
										break;
									j = 0;
									++attempts;
									continue;
								}
							} else {
								if ((cfg.lib[libnum]->code_prefix[0] || cfg.lib[cfg.dir[j]->lib]->code_prefix[0]))
									continue;
							}
							if (stricmp(cfg.dir[j]->code_suffix, tmpdir.code_suffix) == 0) {
								if (attempts == 0)
									SAFECOPY(duplicate_code, tmpdir.code_suffix);
								int code_len = strlen(tmpdir.code_suffix);
								if (code_len < 1)
									break;
								tmpdir.code_suffix[code_len - 1] = random_code_char();
								if (attempts > 10 && code_len > 1)
									tmpdir.code_suffix[code_len - 2] = random_code_char();
								if (attempts > 100 && code_len > 2)
									tmpdir.code_suffix[code_len - 3] = random_code_char();
								j = 0;
								attempts++;
							}
						}
						if (attempts >= (36 * 36 * 36))
							break;
						if (j == cfg.total_dirs) {

							if ((cfg.dir = (dir_t **)realloc_or_free(cfg.dir
							                                         , sizeof(dir_t *) * (cfg.total_dirs + 1))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, "dir", cfg.total_dirs + 1);
								cfg.total_dirs = 0;
								bail(1);
								break;
							}

							if ((cfg.dir[j] = (dir_t *)malloc(sizeof(dir_t)))
							    == NULL) {
								errormsg(WHERE, ERR_ALLOC, "dir", sizeof(dir_t));
								break;
							}
							*cfg.dir[j] = cfg.lib[libnum]->dir_defaults;
							added++;
						} else if (prompt_on_dupe) {
							if (dupe_sname) {
								if (!uifc.confirm("Duplicate dir name '%s' detected. Continue?", cfg.dir[j]->sname))
									break;
							} else
								if (!uifc.confirm("Duplicate dir code '%s' detected. Continue?", cfg.dir[j]->code_suffix))
									break;
							prompt_on_dupe = uifc.confirm("Continue to notify/prompt for each duplicate found?");
						}
						if (k == DIRLIST_FIDO) {
							SAFECOPY(cfg.dir[j]->code_suffix, tmpdir.code_suffix);
							SAFECOPY(cfg.dir[j]->sname, tmpdir.sname);
							SAFECOPY(cfg.dir[j]->lname, tmpdir.lname);
						} else
							memcpy(cfg.dir[j], &tmpdir, sizeof(dir_t));
						cfg.dir[j]->lib = libnum;
						cfg.dir[j]->dirnum = j;
						if (j == cfg.total_dirs) {
							cfg.dir[j]->misc = tmpdir.misc;
							cfg.total_dirs++;
						}
						++ported;
						uifc.changes = 1;
					}
					fclose(stream);
					if (ported && cfg.lib[libnum]->sort)
						sort_dirs(libnum);
					uifc.pop(NULL);
					sprintf(str, "%lu File Areas Imported Successfully (%lu added)", ported, added);
					uifc.msg(str);
					break;

				case __COUNTER__:
					dir_defaults_cfg(libnum);
					break;

				case __COUNTER__:
					dir_cfg(libnum);
					break;
			}
		}
	}
}

void dir_toggle_options(dir_t* dir)
{
	static int dflt, bar;
	char       str[128];

	while (1) {
		int n = 0;
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Check for File Existence"
		         , dir->misc & DIR_FCHK ? "Yes":"No");
		strcpy(str, "Slow Media Device");
		if (dir->seqdev) {
			SAFEPRINTF(tmp, " #%u", dir->seqdev);
			SAFECAT(str, tmp);
		}
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", str
		         , dir->seqdev ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Force Content Ratings"
		         , dir->misc & DIR_RATE ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Upload Date in Listings"
		         , dir->misc & DIR_ULDATE ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Multiple File Numberings"
		         , dir->misc & DIR_MULT ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Search for Duplicates"
		         , dir->misc & DIR_DUPES ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Search for New Files"
		         , dir->misc & DIR_NOSCAN ? "No":"Yes");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Search for Auto-ADDFILES"
		         , dir->misc & DIR_NOAUTO ? "No":"Yes");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Import FILE_ID.DIZ"
		         , dir->misc & DIR_DIZ ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Free Downloads"
		         , dir->misc & DIR_FREE ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Free Download Time"
		         , dir->misc & DIR_TFREE ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Deduct Upload Time"
		         , dir->misc & DIR_ULTIME ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Credit Uploads"
		         , dir->misc & DIR_CDTUL ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Credit Downloads"
		         , dir->misc & DIR_CDTDL ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Credit with Minutes"
		         , dir->misc & DIR_CDTMIN ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Download Notifications"
		         , dir->misc & DIR_QUIET ? "No":"Yes");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Anonymous Uploads"
		         , dir->misc & DIR_ANON ? dir->misc & DIR_AONLY
		    ? "Only":"Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Purge by Last Download"
		         , dir->misc & DIR_SINCEDL ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Mark Moved Files as New"
		         , dir->misc & DIR_MOVENEW ? "Yes":"No");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Include Transfers In Stats"
		         , dir->misc & DIR_NOSTAT ? "No":"Yes");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Calculate/Store Hash of Files"
		         , dir->misc & DIR_NOHASH ? "No":"Yes");
		snprintf(opt[n++], MAX_OPLN, "%-30.30s%s", "Allow File Tagging"
		         , dir->misc & DIR_FILETAGS ? "Yes" : "No");
		opt[n][0] = 0;
		uifc.helpbuf =
			"`Directory Toggle Options:`\n"
			"\n"
			"This is the toggle options menu for the selected file directory.\n"
			"\n"
			"The available options from this menu can all be toggled between two or\n"
			"more states, such as `Yes` and `No`.\n"
		;
		n = uifc.list(WIN_ACT | WIN_SAV | WIN_RHT | WIN_BOT, 0, 0, 0, &dflt, &bar, "Toggle Options", opt);
		if (n == -1)
			break;
		switch (n) {
			case 0:
				n = dir->misc & DIR_FCHK ? 0:1;
				uifc.helpbuf =
					"`Check for File Existence When Listing:`\n"
					"\n"
					"If you want the actual existence of files to be verified while listing\n"
					"directories and to have the current file size and modification time\n"
					"reported when listing files, set this value to `Yes`.\n"
					"\n"
					"Directories with files located on CD-ROM or other slow media should have\n"
					"this option set to `No`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Check for File Existence When Listing", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_FCHK)) {
					dir->misc |= DIR_FCHK;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_FCHK)) {
					dir->misc &= ~DIR_FCHK;
					uifc.changes = 1;
				}
				break;
			case 1:
				n = dir->seqdev ? 0:1;
				uifc.helpbuf =
					"`Slow Media Device:`\n"
					"\n"
					"If this directory contains files located on CD-ROM or other slow media\n"
					"device, you should set this option to `Yes`.  Each slow media device on\n"
					"your system should have a unique `Device Number`.  If you only have one\n"
					"slow media device, then this number should be set to `1`.\n"
					"\n"
					"`CD-ROM multi-disk changers` are considered `one` device and all the\n"
					"directories on all the CD-ROMs in each changer should be set to the same\n"
					"device number.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Slow Media Device"
				              , uifcYesNoOpts);
				if (n == 0) {
					if (!dir->seqdev) {
						uifc.changes = 1;
						strcpy(str, "1");
					}
					else
						sprintf(str, "%u", dir->seqdev);
					uifc.input(WIN_MID | WIN_SAV, 0, 0
					           , "Device Number"
					           , str, 2, K_EDIT | K_NUMBER);
					dir->seqdev = atoi(str);
				}
				else if (n == 1 && dir->seqdev) {
					dir->seqdev = 0;
					uifc.changes = 1;
				}
				break;
			case 2:
				n = dir->misc & DIR_RATE ? 0:1;
				uifc.helpbuf =
					"`Force Content Ratings in Descriptions:`\n"
					"\n"
					"If you would like all uploads to this directory to be prompted for\n"
					"content rating (G, R, or X), set this value to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Force Content Ratings in Descriptions", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_RATE)) {
					dir->misc |= DIR_RATE;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_RATE)) {
					dir->misc &= ~DIR_RATE;
					uifc.changes = 1;
				}
				break;
			case 3:
				n = dir->misc & DIR_ULDATE ? 0:1;
				uifc.helpbuf =
					"`Include Upload Date in File Descriptions:`\n"
					"\n"
					"If you wish the upload date of each file in this directory to be\n"
					"automatically included in the file description, set this option to\n"
					"`Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Include Upload Date in Descriptions", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_ULDATE)) {
					dir->misc |= DIR_ULDATE;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_ULDATE)) {
					dir->misc &= ~DIR_ULDATE;
					uifc.changes = 1;
				}
				break;
			case 4:
				n = dir->misc & DIR_MULT ? 0:1;
				uifc.helpbuf =
					"`Ask for Multiple File Numberings:`\n"
					"\n"
					"If you would like uploads to this directory to be prompted for multiple\n"
					"file (disk) numbers, set this value to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Ask for Multiple File Numberings", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_MULT)) {
					dir->misc |= DIR_MULT;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_MULT)) {
					dir->misc &= ~DIR_MULT;
					uifc.changes = 1;
				}
				break;
			case 5:
				n = dir->misc & DIR_DUPES ? 0:1;
				uifc.helpbuf =
					"`Search Directory for Duplicate Filenames:`\n"
					"\n"
					"If you would like to have this directory searched for duplicate\n"
					"filenames when a user attempts to upload a file, set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Search for Duplicate Filenames", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_DUPES)) {
					dir->misc |= DIR_DUPES;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_DUPES)) {
					dir->misc &= ~DIR_DUPES;
					uifc.changes = 1;
				}
				break;
			case 6:
				n = dir->misc & DIR_NOSCAN ? 1:0;
				uifc.helpbuf =
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
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Search for New files", uifcYesNoOpts);
				if (n == 0 && dir->misc & DIR_NOSCAN) {
					dir->misc &= ~DIR_NOSCAN;
					uifc.changes = 1;
				}
				else if (n == 1 && !(dir->misc & DIR_NOSCAN)) {
					dir->misc |= DIR_NOSCAN;
					uifc.changes = 1;
				}
				break;
			case 7:
				n = dir->misc & DIR_NOAUTO ? 1:0;
				uifc.helpbuf =
					"`Search Directory for Auto-ADDFILES:`\n"
					"\n"
					"If you would like to have this directory searched for a file list to\n"
					"import automatically when using the `ADDFILES *` (Auto-ADD) feature,\n"
					"set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Search for Auto-ADDFILES", uifcYesNoOpts);
				if (n == 0 && dir->misc & DIR_NOAUTO) {
					dir->misc &= ~DIR_NOAUTO;
					uifc.changes = 1;
				}
				else if (n == 1 && !(dir->misc & DIR_NOAUTO)) {
					dir->misc |= DIR_NOAUTO;
					uifc.changes = 1;
				}
				break;
			case 8:
				n = dir->misc & DIR_DIZ ? 0:1;
				uifc.helpbuf =
					"`Import FILE_ID.DIZ and DESC.SDI Descriptions:`\n"
					"\n"
					"If you would like archived descriptions (`FILE_ID.DIZ` and `DESC.SDI`)\n"
					"of uploaded files to be automatically imported as the extended\n"
					"description, set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Import FILE_ID.DIZ and DESC.SDI", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_DIZ)) {
					dir->misc |= DIR_DIZ;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_DIZ)) {
					dir->misc &= ~DIR_DIZ;
					uifc.changes = 1;
				}
				break;
			case 9:
				n = dir->misc & DIR_FREE ? 0:1;
				uifc.helpbuf =
					"`Downloads are Free:`\n"
					"\n"
					"If you would like all downloads from this directory to be free (cost\n"
					"no credits), set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Downloads are Free", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_FREE)) {
					dir->misc |= DIR_FREE;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_FREE)) {
					dir->misc &= ~DIR_FREE;
					uifc.changes = 1;
				}
				break;
			case 10:
				n = dir->misc & DIR_TFREE ? 0:1;
				uifc.helpbuf =
					"`Free Download Time:`\n"
					"\n"
					"If you would like all downloads from this directory to not subtract\n"
					"time from the user, set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Free Download Time", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_TFREE)) {
					dir->misc |= DIR_TFREE;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_TFREE)) {
					dir->misc &= ~DIR_TFREE;
					uifc.changes = 1;
				}
				break;
			case 11:
				n = dir->misc & DIR_ULTIME ? 0:1;
				uifc.helpbuf =
					"`Deduct Upload Time:`\n"
					"\n"
					"If you would like all uploads to this directory to have the time spent\n"
					"uploading subtracted from their time online, set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Deduct Upload Time", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_ULTIME)) {
					dir->misc |= DIR_ULTIME;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_ULTIME)) {
					dir->misc &= ~DIR_ULTIME;
					uifc.changes = 1;
				}
				break;
			case 12:
				n = dir->misc & DIR_CDTUL ? 0:1;
				uifc.helpbuf =
					"`Give Credit for Uploads:`\n"
					"\n"
					"If you want users who upload to this directory to get credit for their\n"
					"initial upload, set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Give Credit for Uploads", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_CDTUL)) {
					dir->misc |= DIR_CDTUL;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_CDTUL)) {
					dir->misc &= ~DIR_CDTUL;
					uifc.changes = 1;
				}
				break;
			case 13:
				n = dir->misc & DIR_CDTDL ? 0:1;
				uifc.helpbuf =
					"`Give Uploader Credit for Downloads:`\n"
					"\n"
					"If you want users who upload to this directory to get credit when their\n"
					"files are downloaded, set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Give Uploader Credit for Downloads", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_CDTDL)) {
					dir->misc |= DIR_CDTDL;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_CDTDL)) {
					dir->misc &= ~DIR_CDTDL;
					uifc.changes = 1;
				}
				break;
			case 14:
				n = dir->misc & DIR_CDTMIN ? 0:1;
				uifc.helpbuf =
					"`Credit Uploader with Minutes instead of Credits:`\n"
					"\n"
					"If you wish to give the uploader of files to this directory minutes,\n"
					"instead of credits, set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Credit Uploader with Minutes", uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_CDTMIN)) {
					dir->misc |= DIR_CDTMIN;
					uifc.changes = 1;
				}
				else if (n == 1 && dir->misc & DIR_CDTMIN) {
					dir->misc &= ~DIR_CDTMIN;
					uifc.changes = 1;
				}
				break;
			case 15:
				n = dir->misc & DIR_QUIET ? 1:0;
				uifc.helpbuf =
					"`Send Download Notifications:`\n"
					"\n"
					"If you wish the BBS to send download notification messages to the\n"
					"uploader of a file to this directory, set this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Send Download Notifications", uifcYesNoOpts);
				if (n == 1 && !(dir->misc & DIR_QUIET)) {
					dir->misc |= DIR_QUIET;
					uifc.changes = 1;
				} else if (n == 0 && dir->misc & DIR_QUIET) {
					dir->misc &= ~DIR_QUIET;
					uifc.changes = 1;
				}
				break;
			case 16:
				n = dir->misc & DIR_ANON ? (dir->misc & DIR_AONLY ? 2:0):1;
				strcpy(opt[0], "Yes");
				strcpy(opt[1], "No");
				strcpy(opt[2], "Only");
				opt[3][0] = 0;
				uifc.helpbuf =
					"`Allow Anonymous Uploads:`\n"
					"\n"
					"If you want users with the `A` exemption to be able to upload anonymously\n"
					"to this directory, set this option to `Yes`.  If you want all uploads to\n"
					"this directory to be forced anonymous, set this option to `Only`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Allow Anonymous Uploads", opt);
				if (n == 0 && (dir->misc & (DIR_ANON | DIR_AONLY))
				    != DIR_ANON) {
					dir->misc |= DIR_ANON;
					dir->misc &= ~DIR_AONLY;
					uifc.changes = 1;
				}
				else if (n == 1 && dir->misc & (DIR_ANON | DIR_AONLY)) {
					dir->misc &= ~(DIR_ANON | DIR_AONLY);
					uifc.changes = 1;
				}
				else if (n == 2 && (dir->misc & (DIR_ANON | DIR_AONLY))
				         != (DIR_ANON | DIR_AONLY)) {
					dir->misc |= (DIR_ANON | DIR_AONLY);
					uifc.changes = 1;
				}
				break;
			case 17:
				n = dir->misc & DIR_SINCEDL ? 0:1;
				uifc.helpbuf =
					"`Purge Files Based on Date of Last Download:`\n"
					"\n"
					"Using the Synchronet file base maintenance utility (`delfiles`), you can\n"
					"have files removed based on the number of days since last downloaded\n"
					"rather than the number of days since the file was uploaded (default),\n"
					"by setting this option to `Yes`.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Purge Files Based on Date of Last Download"
				              , uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_SINCEDL)) {
					dir->misc |= DIR_SINCEDL;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_SINCEDL)) {
					dir->misc &= ~DIR_SINCEDL;
					uifc.changes = 1;
				}
				break;
			case 18:
				n = dir->misc & DIR_MOVENEW ? 0:1;
				uifc.helpbuf =
					"`Mark Moved Files as New:`\n"
					"\n"
					"If this option is set to `Yes`, then all files moved `from` this directory\n"
					"will have their upload date changed to the current date so the file will\n"
					"appear in users' new-file scans again.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Mark Moved Files as New"
				              , uifcYesNoOpts);
				if (n == 0 && !(dir->misc & DIR_MOVENEW)) {
					dir->misc |= DIR_MOVENEW;
					uifc.changes = 1;
				}
				else if (n == 1 && (dir->misc & DIR_MOVENEW)) {
					dir->misc &= ~DIR_MOVENEW;
					uifc.changes = 1;
				}
				break;
			case 19:
				n = dir->misc & DIR_NOSTAT ? 1:0;
				uifc.helpbuf =
					"`Include Transfers In System Statistics:`\n"
					"\n"
					"If this option is set to ~Yes~, then all files uploaded to or downloaded\n"
					"from this directory will be included in the system's daily and\n"
					"cumulative statistics.\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Include Transfers In System Statistics"
				              , uifcYesNoOpts);
				if (n == 1 && !(dir->misc & DIR_NOSTAT)) {
					dir->misc |= DIR_NOSTAT;
					uifc.changes = 1;
				} else if (n == 0 && dir->misc & DIR_NOSTAT) {
					dir->misc &= ~DIR_NOSTAT;
					uifc.changes = 1;
				}
				break;
			case 20:
				n = dir->misc & DIR_NOHASH ? 1:0;
				uifc.helpbuf =
					"`Calculate/Store Hashes of Files:`\n"
					"\n"
					"Set to ~Yes~ to calculate and store the hashes of file contents when\n"
					"adding files to this file base.\n"
					"\n"
					"The hashes (CRC-16, CRC-32, MD5, and SHA-1) are useful for detecting\n"
					"duplicate files (i.e. and rejecting them) as well as allowing the\n"
					"confirmation of data integrity for the downloaders of files."
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Calculate/Store Hashes of Files"
				              , uifcYesNoOpts);
				if (n == 0 && dir->misc & DIR_NOHASH) {
					dir->misc &= ~DIR_NOHASH;
					dir->cfg_modified = true;
					uifc.changes = 1;
				} else if (n == 1 && !(dir->misc & DIR_NOHASH)) {
					dir->misc |= DIR_NOHASH;
					dir->cfg_modified = true;
					uifc.changes = 1;
				}
				break;
			case 21:
				n = (dir->misc & DIR_FILETAGS) ? 0:1;
				uifc.helpbuf =
					"`Allow Addition of Tags to Files:`\n"
					"\n"
				;
				n = uifc.list(WIN_SAV | WIN_MID, 0, 0, 0, &n, 0
				              , "Allow Addition of Tags to Files", uifcYesNoOpts);
				if (n == -1)
					break;
				if (!n && !(dir->misc & DIR_FILETAGS)) {
					uifc.changes = TRUE;
					dir->misc |= DIR_FILETAGS;
					break;
				}
				if (n == 1 && (dir->misc & DIR_FILETAGS)) {
					uifc.changes = TRUE;
					dir->misc &= ~DIR_FILETAGS;
				}
				break;
		}
	}
}

void dir_cfg(int libnum)
{
	static int   dflt, bar, adv_dflt, opt_dflt;
	char         str[128], str2[128], code[128], path[MAX_PATH + 1], done = 0;
	char         data_dir[MAX_PATH + 1];
	int          j, n;
	int          i, dirnum[MAX_OPTS + 1];
	static dir_t savdir;

	char*        dir_long_name_help =
		"`Directory Long Name:`\n"
		"\n"
		"This is a description of the file directory which is displayed in all\n"
		"directory listings.\n"
	;
	char* dir_short_name_help =
		"`Directory Short Name:`\n"
		"\n"
		"This is a short description of the file directory which is displayed at\n"
		"the file transfer prompt.  Some short names have specially-defined\n"
		"meaning:\n"
		"\n"
		"~Temp~\n"
		"    This short name is reserved for `temporary archive transfers`.\n"
		"\n"
		"~Offline~\n"
		"    This short name specifies that this directory is to hold\n"
		"    `offline files` for the selected library and is treated a bit\n"
		"    differently than other directories: it will not be included\n"
		"    in new-file-scans and will be the default location of files that\n"
		"    are removed or moved.  A directory with this short name should\n"
		"    have its required upload level set to 90 (Sysop) and its access\n"
		"    requirements set the same as the library.\n"
		"    There can only be one directory per library with this short name.\n"
		"\n"
		"~Sysop~\n"
		"    This short name specifies that this directory will be the\n"
		"    destination for `files uploaded to the sysop` by users (e.g. with\n"
		"    the `Z` command from the file transfer menu).  There should only be\n"
		"    one directory with this short name and it should belong to the most\n"
		"    accessible library.  A directory with this short name should have\n"
		"    its required access level set to 90 (Sysop) and its upload\n"
		"    requirements set the same as its library.\n"
		"\n"
		"~User~\n"
		"    This short name specifies that this directory will be used for\n"
		"    `user to user file transfers`.  There should only be one directory\n"
		"    with this short name and it should belong to the most accessible\n"
		"    library.  User to user file transfers allow users to upload files\n"
		"    to other users with the `/U` file transfer menu command.\n"
		"    Users download files sent to them with the `/D` file transfer\n"
		"    menu command.  A directory with this short name should have\n"
		"    its required access level set to 90 and its upload requirements\n"
		"    set the same as the library or with whatever requirements the sysop\n"
		"    wishes to require of users to use the user to user file transfer\n"
		"    facilities.\n"
		"\n"
		"~Uploads~\n"
		"    This short name specifies that this directory will be used\n"
		"    as the default destination of user's `uploaded files` (i.e. if a user\n"
		"    attempts to upload to a directory where they do not have upload\n"
		"    permission or `blind-upload` files not in their batch upload queue).\n"
		"    If you wish `all` uploads to automatically go into `this` directory,\n"
		"    set the upload requirements for all `other` directories to level 90.\n"
		"    If you do not want users to be able to see the files in this\n"
		"    directory or download them until they are moved by the sysop to a\n"
		"    more accessible directory, set the access requirements for this\n"
		"    directory to level 90 (Sysop) or higher.\n"
	;
	char* dir_code_help =
		"`Directory Internal Code Suffix:`\n"
		"\n"
		"Every directory must have its own unique code for Synchronet to refer to\n"
		"it internally.  This code should be descriptive of the directory's\n"
		"contents, usually an abbreviation of the directory's name.\n"
		"\n"
		"`Note:` The Internal Code (displayed) is the complete internal code\n"
		"constructed from the file library's `Code Prefix` (if present) and the\n"
		"directory's `Code Suffix`.\n"
		"\n"
		"Changing a directory's internal code (suffix or prefix) changes the\n"
		"underlying database filenames used for that file area, so change these\n"
		"values with caution."
	;
	char* dir_actual_path_help =
		"`Actual File Path:`\n"
		"\n"
		"This is the physical storage path for files uploaded-to and available\n"
		"for download-from this logical directory.\n"
		"\n"
		"If this setting is blank, the directory's internal-code (lower-cased)\n"
		"is used as the default directory name.\n"
		"\n"
		"If this value is not a full/absolute path, the parent directory will be\n"
		"either the library's `Parent Directory` (if set) or the data directory\n"
		"(e.g. ../data/dirs/)\n"
		"\n"
		ADDFILES_HELP
	;

	SAFECOPY(cfg.lib[libnum]->vdir, cfg.lib[libnum]->sname);
	pathify(cfg.lib[libnum]->vdir);

	while (1) {
		if (uifc.changes && cfg.lib[libnum]->sort)
			sort_dirs(libnum);
		for (i = 0, j = 0; i < cfg.total_dirs && j < MAX_OPTS; i++) {
			if (cfg.dir[i]->lib != libnum)
				continue;
			char* name = cfg.dir[i]->lname;
			switch (cfg.lib[libnum]->sort) {
				case AREA_SORT_SNAME:
					name = cfg.dir[i]->sname;
					break;
				case AREA_SORT_CODE:
					name = cfg.dir[i]->code_suffix;
					break;
				default:    /* Defeat stupid GCC warning */
					break;
			}
			strcpy(opt[j], name);
			dirnum[j++] = i;
		}
		dirnum[j] = cfg.total_dirs;
		opt[j][0] = 0;
		sprintf(str, "%s Directories (%u)", cfg.lib[libnum]->sname, j);
		int mode = WIN_SAV | WIN_ACT | WIN_RHT;
		if (j)
			mode |= WIN_DEL | WIN_COPY | WIN_CUT | WIN_DELACT;
		if (j < MAX_OPTS)
			mode |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savdir.sname[0])
			mode |= WIN_PASTE | WIN_PASTEXTR;
		uifc.helpbuf =
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
			"\n"
			ADDFILES_HELP
		;
		i = uifc.list(mode, 0, 0, 0, &dflt, &bar, str, opt);
		if ((signed)i == -1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf = dir_long_name_help;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Directory Long Name", str, LEN_SLNAME
			               , K_NONE) < 1)
				continue;
			sprintf(str2, "%.*s", LEN_SSNAME, str);
			uifc.helpbuf = dir_short_name_help;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Directory Short Name", str2, LEN_SSNAME
			               , K_EDIT) < 1)
				continue;
			SAFECOPY(code, str2);
			prep_code(code, /* prefix: */ NULL);
			uifc.helpbuf = dir_code_help;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Directory Internal Code Suffix", code, LEN_CODE
			               , K_EDIT | K_UPPER | K_NOSPACE) < 1)
				continue;
			SAFEPRINTF2(tmp, "%s%s", cfg.lib[libnum]->code_prefix, code);
			if (getdirnum(&cfg, tmp) >= 0) {
				uifc.msg(strDuplicateCode);
				continue;
			}
			if (!code_ok(code)) {
				uifc.helpbuf = invalid_code;
				uifc.msg(strInvalidCode);
				uifc.helpbuf = NULL;
				continue;
			}
			SAFECOPY(path, code);
			strlwr(path);
			uifc.helpbuf = dir_actual_path_help;
			uifc.input(WIN_MID | WIN_SAV, 0, 0, "Actual File Path (directory)", path, LEN_DIR, K_EDIT);

			if (!new_dir(dirnum[i], libnum))
				continue;

			if (stricmp(str2, "OFFLINE") == 0)
				cfg.dir[dirnum[i]]->misc = 0;
			SAFECOPY(cfg.dir[dirnum[i]]->code_suffix, code);
			SAFECOPY(cfg.dir[dirnum[i]]->lname, str);
			SAFECOPY(cfg.dir[dirnum[i]]->sname, str2);
			SAFECOPY(cfg.dir[dirnum[i]]->path, path);
			uifc.changes = 1;
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
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
					if (j == 0) {
						uifc.pop("Deleting ...");
						delfiles(data_dir, str, /* keep: */ 0);
						uifc.pop(NULL);
					}
				}
			}
			if (msk == MSK_CUT)
				savdir = *cfg.dir[dirnum[i]];
			free(cfg.dir[dirnum[i]]);
			cfg.total_dirs--;
			for (j = dirnum[i]; j < cfg.total_dirs; j++)
				cfg.dir[j] = cfg.dir[j + 1];
			uifc.changes = 1;
			continue;
		}
		if (msk == MSK_COPY) {
			savdir = *cfg.dir[dirnum[i]];
			continue;
		}
		if (msk == MSK_PASTE) {
			if (!new_dir(dirnum[i], libnum))
				continue;
			*cfg.dir[dirnum[i]] = savdir;
			cfg.dir[dirnum[i]]->lib = libnum;
			uifc.changes = 1;
			continue;
		}
		i = dirnum[dflt];
		j = 0;
		done = 0;
		while (!done) {
			n = 0;
			char area_tag[sizeof(cfg.dir[i]->area_tag) + 2];
			if (cfg.dir[i]->area_tag[0])
				SAFECOPY(area_tag, cfg.dir[i]->area_tag);
			else
				SAFEPRINTF(area_tag, "[%s]", dir_area_tag(&cfg, cfg.dir[i], tmp, sizeof(tmp)));
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Long Name", cfg.dir[i]->lname);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Short Name", cfg.dir[i]->sname);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s%s", "Internal Code"
			         , cfg.lib[cfg.dir[i]->lib]->code_prefix, cfg.dir[i]->code_suffix);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "FidoNet Area Tag", area_tag);
			if (cfg.dir[i]->vdir_name[0] != '\0')
				snprintf(path, sizeof path, "/%s/%s", cfg.lib[cfg.dir[i]->lib]->vdir, cfg.dir[i]->vdir_name);
			else {
				init_vdir(&cfg, cfg.dir[i]);
				snprintf(path, sizeof path, "[/%s/%s]", cfg.lib[cfg.dir[i]->lib]->vdir, cfg.dir[i]->vdir);
			}
			if (cfg.dir[i]->vshortcut[0] != '\0')
				snprintf(str, sizeof str, "/%s -> %s", cfg.dir[i]->vshortcut, path);
			else
				strlcpy(str, path, sizeof str);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Virtual File Path", str);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Access Requirements"
			         , cfg.dir[i]->arstr);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Upload Requirements"
			         , cfg.dir[i]->ul_arstr);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Download Requirements"
			         , cfg.dir[i]->dl_arstr);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Operator Requirements"
			         , cfg.dir[i]->op_arstr);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Exemption Requirements"
			         , cfg.dir[i]->ex_arstr);
			SAFECOPY(path, cfg.dir[i]->path);
			if (!path[0]) {
				SAFEPRINTF2(path, "%s%s", cfg.lib[cfg.dir[i]->lib]->code_prefix, cfg.dir[i]->code_suffix);
				strlwr(path);
			}
			if (cfg.lib[cfg.dir[i]->lib]->parent_path[0])
				prep_dir(cfg.lib[cfg.dir[i]->lib]->parent_path, path, sizeof(path));
			else {
				if (!cfg.dir[i]->data_dir[0])
					SAFEPRINTF(data_dir, "%sdirs", cfg.data_dir);
				else
					SAFECOPY(data_dir, cfg.dir[i]->data_dir);
				prep_dir(data_dir, path, sizeof(path));
			}
			if (paths_are_same(path, cfg.dir[i]->path))
				SAFECOPY(str, cfg.dir[i]->path);
			else
				SAFEPRINTF(str, "[%s]", path);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Actual File Path", str);
			if (cfg.dir[i]->maxfiles)
				sprintf(str, "%u", cfg.dir[i]->maxfiles);
			else
				SAFECOPY(str, "Unlimited");
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Maximum Number of Files"
			         , str);
			if (cfg.dir[i]->maxage)
				sprintf(str, "Enabled (%u days old)", cfg.dir[i]->maxage);
			else
				strcpy(str, "Disabled");
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Purge by Age", str);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%u%%", "Credit on Upload"
			         , cfg.dir[i]->up_pct);
			snprintf(opt[n++], MAX_OPLN, "%-27.27s%u%%", "Credit on Download"
			         , cfg.dir[i]->dn_pct);
			strcpy(opt[n++], "Toggle Options...");
			strcpy(opt[n++], "Advanced Options...");
			opt[n][0] = 0;
			sprintf(str, "%s Directory", cfg.dir[i]->sname);
			uifc.helpbuf =
				"`Directory Configuration:`\n"
				"\n"
				"This menu allows you to configure the individual selected directory.\n"
				"Options with a trailing `...` provide a sub-menu of more options.\n"
				"\n"
				"The left and right arrow keys may be used to cycle through file\n"
				"directories.\n"
				"\n"
				ADDFILES_HELP
			;
			uifc_winmode_t wmode = WIN_SAV | WIN_ACT | WIN_L2R | WIN_BOT | WIN_EXTKEYS;
			int            prev = prev_dirnum(cfg.dir[i]);
			int            next = next_dirnum(cfg.dir[i]);
			if (prev != i)
				wmode |= WIN_LEFTKEY;
			if (next != i)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 72, &opt_dflt, 0, str, opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					i = prev;
					break;
				case -CIO_KEY_RIGHT - 2:
					i = next;
					break;
				case 0:
					uifc.helpbuf = dir_long_name_help;
					SAFECOPY(str, cfg.dir[i]->lname);
					if (uifc.input(WIN_L2R | WIN_SAV, 0, 17, "Name to use for Listings"
					               , str, LEN_SLNAME, K_EDIT) > 0)
						SAFECOPY(cfg.dir[i]->lname, str);
					break;
				case 1:
					uifc.helpbuf = dir_short_name_help;
					SAFECOPY(str, cfg.dir[i]->sname);
					if (uifc.input(WIN_L2R | WIN_SAV, 0, 17, "Name to use for Prompts"
					               , str, LEN_SSNAME, K_EDIT) > 0)
						SAFECOPY(cfg.dir[i]->sname, str);
					break;
				case 2:
					uifc.helpbuf = dir_code_help;
					SAFECOPY(str, cfg.dir[i]->code_suffix);
					if (uifc.input(WIN_L2R | WIN_SAV, 0, 17, "Internal Code Suffix (unique)"
					           , str, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE | K_CHANGED) < 1)
						break;
					SAFEPRINTF2(tmp, "%s%s", cfg.lib[cfg.dir[i]->lib]->code_prefix, str);
					if (getdirnum(&cfg, tmp) >= 0)
						uifc.msg(strDuplicateCode);
					else if (code_ok(str)) {
						SAFECOPY(cfg.dir[i]->code_suffix, str);
						cfg.dir[i]->cfg_modified = true;
					}
					else {
						uifc.helpbuf = invalid_code;
						uifc.msg(strInvalidCode);
						uifc.helpbuf = NULL;
					}
					break;
				case 3:
					uifc.helpbuf =
						"`FidoNet Area Tag:`\n"
						"\n"
						"This field may be used to specify the FidoNet-style `Echo/Area Tag` for\n"
						"this file area.  If no tag name is configured here, a tag name will be\n"
						"automatically generated from the Directory's `Short Name`.\n"
						"\n"
						"This tag may ~ not ~ contain spaces."
					;
					SAFECOPY(str, cfg.dir[i]->area_tag);
					if (uifc.input(WIN_L2R | WIN_SAV, 0, 17, "FidoNet File Echo Area Tag"
					               , str, sizeof(cfg.dir[i]->area_tag) - 1, K_EDIT | K_UPPER | K_NOSPACE) > 0)
						SAFECOPY(cfg.dir[i]->area_tag, str);
					break;
				case 4:
					uifc.helpbuf =
						"`Virtual Sub-directory:`\n"
						"\n"
						"You may use this setting to override the auto-generated virtual\n"
						"sub-directory name as generated by the `Virtual Sub-directories`\n"
						"naming convention configured for the parent File Library.\n"
						"\n"
						"If no sub-directory name is specified here, then one is automatically\n"
						"generated and displayed within `[`brackets`]` for your information.\n"
						"\n"
					;
					SAFECOPY(str, cfg.dir[i]->vdir_name);
					if (uifc.input(WIN_L2R | WIN_SAV, 0, 17, "Virtual Sub-directory"
					               , str, sizeof(cfg.dir[i]->vdir_name) - 1, K_EDIT | K_NOSPACE) < 0)
						break;
					if (strchr(str, '/') != NULL || strchr(str, '\\') != NULL) {
						uifc.msg("Invalid virtual directory");
						break;
					}
					SAFECOPY(cfg.dir[i]->vdir_name, str);

					uifc.helpbuf =
						"`Virtual Shortcut:`\n"
						"\n"
						"You may use this setting to set an optional virtual shortcut to this\n"
						"directory that will appear to users in the root directory of the\n"
						"Synchronet Web and FTP servers.\n"
						"\n"
						"Setting a virtual shortcut here creates an alias to the directory, so\n"
						"you will `not` need an equivalent alias in the `ftpalias.cfg` or\n"
						"`web_alias.ini` files.\n"
					;
					SAFECOPY(str, cfg.dir[i]->vshortcut);
					if (uifc.input(WIN_L2R | WIN_SAV, 0, 17, "Virtual Shortcut"
					               , str, sizeof(cfg.dir[i]->vshortcut) - 1, K_EDIT | K_NOSPACE) < 0)
						break;
					if (strchr(str, '/') != NULL || strchr(str, '\\') != NULL) {
						uifc.msg("Invalid virtual shortcut");
						break;
					}
					SAFECOPY(cfg.dir[i]->vshortcut, str);
					break;
				case 5:
					sprintf(str, "%s Access", cfg.dir[i]->sname);
					getar(str, cfg.dir[i]->arstr);
					break;
				case 6:
					sprintf(str, "%s Upload", cfg.dir[i]->sname);
					getar(str, cfg.dir[i]->ul_arstr);
					break;
				case 7:
					sprintf(str, "%s Download", cfg.dir[i]->sname);
					getar(str, cfg.dir[i]->dl_arstr);
					break;
				case 8:
					sprintf(str, "%s Operator", cfg.dir[i]->sname);
					getar(str, cfg.dir[i]->op_arstr);
					break;
				case 9:
					sprintf(str, "%s Exemption", cfg.dir[i]->sname);
					getar(str, cfg.dir[i]->ex_arstr);
					break;
				case 10:
					uifc.helpbuf = dir_actual_path_help;
					uifc.input(WIN_L2R | WIN_SAV, 0, 17, "Actual File Path (directory)"
					           , cfg.dir[i]->path, sizeof(cfg.dir[i]->path) - 1, K_EDIT);
					break;
				case 11:
					uifc.helpbuf = max_files_help;
					sprintf(str, "%u", cfg.dir[i]->maxfiles);
					uifc.input(WIN_L2R | WIN_SAV, 0, 17, "Maximum Number of Files (0=Unlimited)"
					           , str, 5, K_EDIT | K_NUMBER);
					cfg.dir[i]->maxfiles = atoi(str);
					cfg.dir[i]->cfg_modified = true;
					break;
				case 12:
					sprintf(str, "%u", cfg.dir[i]->maxage);
					uifc.helpbuf = max_age_help;
					uifc.input(WIN_MID | WIN_SAV, 0, 17, "Maximum Age of Files (in days)"
					           , str, 5, K_EDIT | K_NUMBER);
					cfg.dir[i]->maxage = atoi(str);
					cfg.dir[i]->cfg_modified = true;
					break;
				case 13:
					uifc.helpbuf = up_pct_help;
					uifc.input(WIN_MID | WIN_SAV, 0, 0
					           , "Percentage of Credits to Credit Uploader on Upload"
					           , ultoa(cfg.dir[i]->up_pct, tmp, 10), 4, K_EDIT | K_NUMBER);
					cfg.dir[i]->up_pct = atoi(tmp);
					break;
				case 14:
					uifc.helpbuf = dn_pct_help;
					uifc.input(WIN_MID | WIN_SAV, 0, 0
					           , "Percentage of Credits to Credit Uploader on Download"
					           , ultoa(cfg.dir[i]->dn_pct, tmp, 10), 4, K_EDIT | K_NUMBER);
					cfg.dir[i]->dn_pct = atoi(tmp);
					break;
				case 15:
					dir_toggle_options(cfg.dir[i]);
					break;
				case 16:
					while (1) {
						n = 0;
						snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Extensions Allowed"
						         , cfg.dir[i]->exts);
						if (!cfg.dir[i]->data_dir[0])
							sprintf(str, "[%sdirs/]", cfg.data_dir);
						else
							strcpy(str, cfg.dir[i]->data_dir);
						snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Data Directory"
						         , str);
						snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Upload Semaphore File"
						         , cfg.dir[i]->upload_sem);
						snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Sort Value and Direction"
						         , file_sort_desc[cfg.dir[i]->sort]);
						snprintf(opt[n++], MAX_OPLN, "%-27.27sNow %u / Was %u", "Directory Index", i, cfg.dir[i]->dirnum);
						opt[n][0] = 0;
						uifc.helpbuf =
							"`Directory Advanced Options:`\n"
							"\n"
							"This is the advanced options menu for the selected file directory.\n"
						;
						n = uifc.list(WIN_ACT | WIN_SAV | WIN_RHT | WIN_BOT, 3, 4, 66, &adv_dflt, 0
						              , "Advanced Options", opt);
						if (n == -1)
							break;
						switch (n) {
							case 0:
								uifc.helpbuf = file_ext_help;
								uifc.input(WIN_L2R | WIN_SAV, 0, 17
								           , "File Extensions Allowed"
								           , cfg.dir[i]->exts, sizeof(cfg.dir[i]->exts) - 1, K_EDIT);
								break;
							case 1:
								uifc.helpbuf = data_dir_help;
								uifc.input(WIN_MID | WIN_SAV, 0, 17, "Data"
								           , cfg.dir[i]->data_dir, sizeof(cfg.dir[i]->data_dir) - 1, K_EDIT);
								break;
							case 2:
								uifc.helpbuf = upload_sem_help;
								uifc.input(WIN_MID | WIN_SAV, 0, 17, "Upload Semaphore"
								           , cfg.dir[i]->upload_sem, sizeof(cfg.dir[i]->upload_sem) - 1, K_EDIT);
								break;
							case 3:
								n = cfg.dir[i]->sort;
								uifc.helpbuf = sort_help;
								n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
								              , "Sort Value and Direction", file_sort_desc);
								if (n >= 0 && cfg.dir[i]->sort != n) {
									cfg.dir[i]->sort = n;
									uifc.changes = TRUE;
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

void dir_defaults_cfg(int libnum)
{
	static int dflt;
	char       str[128];
	dir_t*     dir = &cfg.lib[libnum]->dir_defaults;

	while (1) {
		int n = 0;
		if (dir->maxfiles)
			sprintf(str, "%u", dir->maxfiles);
		else
			SAFECOPY(str, "Unlimited");
		snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Maximum Number of Files"
		         , str);
		if (dir->maxage)
			sprintf(str, "Enabled (%u days old)", dir->maxage);
		else
			strcpy(str, "Disabled");
		snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Purge by Age", str);
		snprintf(opt[n++], MAX_OPLN, "%-27.27s%u%%", "Credit on Upload"
		         , dir->up_pct);
		snprintf(opt[n++], MAX_OPLN, "%-27.27s%u%%", "Credit on Download"
		         , dir->dn_pct);
		snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Extensions Allowed"
		         , dir->exts);
		if (!dir->data_dir[0])
			sprintf(str, "[%sdirs/]", cfg.data_dir);
		else
			strcpy(str, dir->data_dir);
		snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Data Directory"
		         , str);
		snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Upload Semaphore File"
		         , dir->upload_sem);
		snprintf(opt[n++], MAX_OPLN, "%-27.27s%s", "Sort Value and Direction"
		         , file_sort_desc[dir->sort]);
		strcpy(opt[n++], "Toggle Options...");
		strcpy(opt[n++], "Clone Settings");
		opt[n][0] = 0;
		uifc.helpbuf =
			"`Directory Default Settings:`\n"
			"\n"
			"This menu allows you to configure the default settings for new file\n"
			"directories created in this file library.\n"
		;
		uifc_winmode_t wmode = WIN_SAV | WIN_ACT | WIN_L2R | WIN_BOT | WIN_EXTKEYS;
		switch (uifc.list(wmode, 0, 0, 72, &dflt, 0, "Directory Default Settings", opt)) {
			case -1:
				return;
			case 0:
				uifc.helpbuf = max_files_help;
				sprintf(str, "%u", dir->maxfiles);
				uifc.input(WIN_MID | WIN_SAV, 0, 17, "Maximum Number of Files (0=Unlimited)"
				           , str, 5, K_EDIT | K_NUMBER);
				dir->maxfiles = atoi(str);
				break;
			case 1:
				sprintf(str, "%u", dir->maxage);
				uifc.helpbuf = max_age_help;
				uifc.input(WIN_MID | WIN_SAV, 0, 17, "Maximum Age of Files (in days)"
				           , str, 5, K_EDIT | K_NUMBER);
				dir->maxage = atoi(str);
				break;
			case 2:
				uifc.helpbuf = up_pct_help;
				uifc.input(WIN_MID | WIN_SAV, 0, 0
				           , "Percentage of Credits to Credit Uploader on Upload"
				           , ultoa(dir->up_pct, tmp, 10), 4, K_EDIT | K_NUMBER);
				dir->up_pct = atoi(tmp);
				break;
			case 3:
				uifc.helpbuf = dn_pct_help;
				uifc.input(WIN_MID | WIN_SAV, 0, 0
				           , "Percentage of Credits to Credit Uploader on Download"
				           , ultoa(dir->dn_pct, tmp, 10), 4, K_EDIT | K_NUMBER);
				dir->dn_pct = atoi(tmp);
				break;
			case 4:
				uifc.helpbuf = file_ext_help;
				uifc.input(WIN_MID | WIN_SAV, 0, 17
				           , "File Extensions Allowed"
				           , dir->exts, sizeof(dir->exts) - 1, K_EDIT);
				break;
			case 5:
				uifc.helpbuf = data_dir_help;
				uifc.input(WIN_MID | WIN_SAV, 0, 17, "Data"
				           , dir->data_dir, sizeof(dir->data_dir) - 1, K_EDIT);
				break;
			case 6:
				uifc.helpbuf = upload_sem_help;
				uifc.input(WIN_MID | WIN_SAV, 0, 17, "Upload Semaphore"
				           , dir->upload_sem, sizeof(dir->upload_sem) - 1, K_EDIT);
				break;
			case 7:
				n = dir->sort;
				uifc.helpbuf = sort_help;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0
				              , "Sort Value and Direction", file_sort_desc);
				if (n >= 0 && dir->sort != n) {
					dir->sort = n;
					uifc.changes = TRUE;
				}
				break;
			case 8:
				dir_toggle_options(dir);
				break;
			case 9: /* clone options */
				uifc.helpbuf =
					"`Clone Directory Default Settings:`\n"
					"\n"
					"If you want to clone these default directory settings into all\n"
					"directories of this library, select `Yes`.\n"
					"\n"
				;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, NULL, 0
				              , "Clone Default Settings into All of Library"
				              , uifcYesNoOpts);
				if (n == 0) {
					for (int j = 0; j < cfg.total_dirs; j++) {
						if (cfg.dir[j]->lib != libnum)
							continue;
						uifc.changes = 1;
						cfg.dir[j]->misc        = dir->misc;
						cfg.dir[j]->maxfiles    = dir->maxfiles;
						cfg.dir[j]->maxage      = dir->maxage;
						cfg.dir[j]->seqdev      = dir->seqdev;
						cfg.dir[j]->sort        = dir->sort;
						SAFECOPY(cfg.dir[j]->exts, dir->exts);
						SAFECOPY(cfg.dir[j]->data_dir, dir->data_dir);
						SAFECOPY(cfg.dir[j]->upload_sem, dir->upload_sem);
					}
				}
				break;
		}
	}
}
