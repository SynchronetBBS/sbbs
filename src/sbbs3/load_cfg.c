/* Synchronet configuration load routines (exported) */

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

#include "load_cfg.h"
#include "scfglib.h"
#include "str_util.h"
#include "nopen.h"
#include "datewrap.h"
#include "xpdatetime.h"
#include "text.h"   /* TOTAL_TEXT */
#include "readtext.h"
#include "ini_file.h"
#if defined(SBBS) && defined(USE_CRYPTLIB)
	#include "ssl.h"
#endif

static void prep_cfg(scfg_t* cfg);

int     lprintf(int level, const char *fmt, ...);   /* log output */

// Returns 0-based text string index
int get_text_num(const char* id)
{
	int i;
	if (IS_DIGIT(*id)) {
		i = atoi(id);
		if (i < 1)
			return TOTAL_TEXT;
		return i - 1;
	}
	for (i = 0; i < TOTAL_TEXT; ++i)
		if (strcmp(text_id[i], id) == 0)
			break;
	return i;
}

/****************************************************************************/
/* Initializes system and node configuration information and data variables */
/****************************************************************************/
bool load_cfg(scfg_t* cfg, char* text[], size_t total_text, bool prep, bool req_cfg, char* error, size_t maxerrlen)
{
	int   i;
	int   line = 0;
	FILE* fp;
	char  str[256];

	if (cfg->size != sizeof(scfg_t)) {
		safe_snprintf(error, maxerrlen, "cfg->size (%" PRIu32 ") != sizeof(scfg_t) (%" XP_PRIsize_t "d)"
		              , cfg->size, sizeof(scfg_t));
		return false;
	}
	if (text != NULL && total_text != TOTAL_TEXT) {
		safe_snprintf(error, maxerrlen, "total_text (%" XP_PRIsize_t "d) != TOTAL_TEXT (%d)"
		              , total_text, TOTAL_TEXT);
		return false;
	}
	if (error != NULL)
		*error = '\0';

	free_cfg(cfg);  /* free allocated config parameters */

	if (cfg->node_num < 1)
		cfg->node_num = 1;

	backslash(cfg->ctrl_dir);
	if (read_main_cfg(cfg, error, maxerrlen) == false && req_cfg)
		return false;

	if (prep)
		for (i = 0; i < cfg->sys_nodes; i++)
			prep_dir(cfg->ctrl_dir, cfg->node_path[i], sizeof(cfg->node_path[i]));

	if (cfg->sys_nodes < cfg->node_num)
		snprintf(error, maxerrlen, "%d nodes configured in %s", cfg->sys_nodes, cfg->filename);
	else {
		SAFECOPY(cfg->node_dir, cfg->node_path[cfg->node_num - 1]);
		prep_dir(cfg->ctrl_dir, cfg->node_dir, sizeof(cfg->node_dir));
		if (read_node_cfg(cfg, error, maxerrlen) == false && req_cfg)
			return false;
	}
	if (read_msgs_cfg(cfg, error, maxerrlen) == false && req_cfg)
		return false;
	if (read_file_cfg(cfg, error, maxerrlen) == false && req_cfg)
		return false;
	if (read_xtrn_cfg(cfg, error, maxerrlen) == false && req_cfg)
		return false;
	if (read_chat_cfg(cfg, error, maxerrlen) == false && req_cfg)
		return false;
	if (read_attr_cfg(cfg, error, maxerrlen) == false && req_cfg)
		return false;

	if (text != NULL) {

		/* Free existing text if allocated */
		free_text(text);

		named_string_t** str_list = NULL;
		named_string_t** substr_list = NULL;
		SAFEPRINTF(str, "%stext.ini", cfg->ctrl_dir);
		if ((fp = fnopen(NULL, str, O_RDONLY)) != NULL) {
			str_list_t       ini = iniReadFile(fp);
			fclose(fp);
			str_list = iniGetNamedStringList(ini, ROOT_SECTION);
			substr_list = iniGetNamedStringList(ini, "substr");
			iniFreeStringList(ini);
		}

		SAFEPRINTF(str, "%stext.dat", cfg->ctrl_dir);
		if ((fp = fnopen(NULL, str, O_RDONLY)) == NULL) {
			safe_snprintf(error, maxerrlen, "%d opening %s", errno, str);
			iniFreeNamedStringList(str_list);
			iniFreeNamedStringList(substr_list);
			return false;
		}
		for (i = 0; i < TOTAL_TEXT; i++)
			if ((text[i] = readtext(&line, fp, i, substr_list)) == NULL) {
				i--;
				break;
			}
		fclose(fp);

		if (i < TOTAL_TEXT) {
			safe_snprintf(error, maxerrlen, "line %d: Less than TOTAL_TEXT (%u) strings defined in %s."
			              , i
			              , TOTAL_TEXT, str);
			iniFreeNamedStringList(str_list);
			iniFreeNamedStringList(substr_list);
			return false;
		}

		for (i = 0; str_list != NULL && str_list[i] != NULL; ++i) {
			int n = get_text_num(str_list[i]->name);
			if (n >= TOTAL_TEXT) {
				safe_snprintf(error, maxerrlen, "%s text ID (%s) not recognized"
					            , str
					            , str_list[i]->name);
				continue;
			}
			free(text[n]);
			text[n] = strdup(str_list[i]->value);
		}
		iniFreeNamedStringList(str_list);
		iniFreeNamedStringList(substr_list);

		cfg->text = text;
	}

	/* Override com-port settings */
	cfg->com_base = 0xf;  /* All nodes use FOSSIL */
	cfg->com_port = 1;    /* All nodes use "COM1" */

	if (prep)
		prep_cfg(cfg);

	/* Auto-toggle daylight savings time in US time-zones */
	sys_timezone(cfg);

	return true;
}

void pathify(char* str)
{
	char* p;

	if (strchr(str, '.') == NULL) {
		REPLACE_CHARS(str, ' ', '.', p);
	} else {
		REPLACE_CHARS(str, ' ', '_', p);
	}
	REPLACE_CHARS(str, '\\', '-', p);
	REPLACE_CHARS(str, '/', '-', p);
}

void init_vdir(scfg_t* cfg, dir_t* dir)
{
	if (dir->vdir_name[0] != '\0') {
		SAFECOPY(dir->vdir, dir->vdir_name);
		return;
	}
	switch (cfg->lib[dir->lib]->vdir_name) {
		case VDIR_NAME_SHORT:
			SAFECOPY(dir->vdir, dir->sname);
			break;
		case VDIR_NAME_LONG:
			SAFECOPY(dir->vdir, dir->lname);
			break;
		default:
			SAFECOPY(dir->vdir, dir->code_suffix);
			break;
	}
	pathify(dir->vdir);
}

/****************************************************************************/
/* Prepare configuration for run-time (resolve relative paths, etc)			*/
/****************************************************************************/
void prep_cfg(scfg_t* cfg)
{
	int i;

	/* Fix-up paths */
	prep_dir(cfg->ctrl_dir, cfg->data_dir, sizeof(cfg->data_dir));
	prep_dir(cfg->ctrl_dir, cfg->logs_dir, sizeof(cfg->logs_dir));
	prep_dir(cfg->ctrl_dir, cfg->exec_dir, sizeof(cfg->exec_dir));
	prep_dir(cfg->ctrl_dir, cfg->mods_dir, sizeof(cfg->mods_dir));
	prep_dir(cfg->ctrl_dir, cfg->text_dir, sizeof(cfg->text_dir));

	prep_dir(cfg->ctrl_dir, cfg->netmail_dir, sizeof(cfg->netmail_dir));
	prep_dir(cfg->ctrl_dir, cfg->echomail_dir, sizeof(cfg->echomail_dir));

	prep_path(cfg->netmail_sem);
	prep_path(cfg->echomail_sem);
	prep_path(cfg->inetmail_sem);

	for (i = 0; i < cfg->total_subs; i++) {

		if (!cfg->sub[i]->data_dir[0])   /* no data storage path specified */
			SAFEPRINTF(cfg->sub[i]->data_dir, "%ssubs", cfg->data_dir);
		prep_dir(cfg->ctrl_dir, cfg->sub[i]->data_dir, sizeof(cfg->sub[i]->data_dir));

		/* default QWKnet tagline */
		if (!cfg->sub[i]->tagline[0])
			SAFECOPY(cfg->sub[i]->tagline, cfg->qnet_tagline);

		/* default origin line */
		if (!cfg->sub[i]->origline[0])
			SAFECOPY(cfg->sub[i]->origline, cfg->origline);

		/* A sub-board's internal code is the combination of the grp's code_prefix & the sub's code_suffix */
		SAFEPRINTF2(cfg->sub[i]->code, "%s%s"
		            , cfg->grp[cfg->sub[i]->grp]->code_prefix
		            , cfg->sub[i]->code_suffix);

		strlwr(cfg->sub[i]->code);      /* data filenames are all lowercase */

		prep_path(cfg->sub[i]->post_sem);
	}

	for (i = 0; i < cfg->total_libs; i++) {
		if (cfg->lib[i]->parent_path[0])
			prep_dir(cfg->ctrl_dir, cfg->lib[i]->parent_path, sizeof(cfg->lib[i]->parent_path));
		if ((cfg->lib[i]->misc & LIB_DIRS) == 0 || cfg->lib[i]->parent_path[0] == 0)
			continue;
		char   path[MAX_PATH + 1];
		SAFECOPY(path, cfg->lib[i]->parent_path);
		backslash(path);
		strcat(path, ALLFILES);
		glob_t g;
		if (glob(path, GLOB_MARK, NULL, &g))
			continue;
		dir_t dir = cfg->lib[i]->dir_defaults;
		dir.lib = i;
		for (uint gi = 0; gi < g.gl_pathc; gi++) {
			char* p = g.gl_pathv[gi];
			char* tp = lastchar(p);
			if (*tp != '/')
				continue;
			*tp = 0; // Remove trailing slash
			SAFECOPY(dir.path, p);
			char* dirname = getfname(dir.path);
			SAFECOPY(dir.lname, dirname);
			SAFECOPY(dir.sname, dirname);
			char code_suffix[LEN_EXTCODE + 1];
			SAFECOPY(code_suffix, dirname);
			prep_code(code_suffix, cfg->lib[i]->code_prefix);
			SAFECOPY(dir.code_suffix, code_suffix);
			init_vdir(cfg, &dir);
			int   j;
			for (j = 0; j < cfg->total_dirs; j++) {
				if (cfg->dir[j]->lib != i)
					continue;
				if (strcmp(cfg->dir[j]->path, dir.path) == 0 || strcmp(cfg->dir[j]->path, dirname) == 0 || stricmp(cfg->dir[j]->code_suffix, dir.code_suffix) == 0)
					break;
			}
			if (j < cfg->total_dirs) // duplicate
				continue;
			dir_t** new_dirs;
			if ((new_dirs = (dir_t **)realloc(cfg->dir, sizeof(dir_t *) * (cfg->total_dirs + 2))) == NULL)
				continue;
			cfg->dir  = new_dirs;
			if ((cfg->dir[cfg->total_dirs] = malloc(sizeof(dir_t))) == NULL)
				continue;
			*cfg->dir[cfg->total_dirs++] = dir;
		}
		globfree(&g);
	}

	for (i = 0; i < cfg->total_dirs; i++) {

		if (!cfg->dir[i]->data_dir[0])   /* no data storage path specified */
			SAFEPRINTF(cfg->dir[i]->data_dir, "%sdirs", cfg->data_dir);
		prep_dir(cfg->ctrl_dir, cfg->dir[i]->data_dir, sizeof(cfg->dir[i]->data_dir));

		/* A directory's internal code is the combination of the lib's code_prefix & the dir's code_suffix */
		SAFEPRINTF2(cfg->dir[i]->code, "%s%s"
		            , cfg->lib[cfg->dir[i]->lib]->code_prefix
		            , cfg->dir[i]->code_suffix);

		strlwr(cfg->dir[i]->code);      /* data filenames are all lowercase */

		if (!cfg->dir[i]->path[0])
			SAFECOPY(cfg->dir[i]->path, cfg->dir[i]->code);
		if (cfg->lib[cfg->dir[i]->lib]->parent_path[0])
			prep_dir(cfg->lib[cfg->dir[i]->lib]->parent_path, cfg->dir[i]->path, sizeof(cfg->dir[i]->path));
		else
			prep_dir(cfg->dir[i]->data_dir, cfg->dir[i]->path, sizeof(cfg->dir[i]->path));

		prep_path(cfg->dir[i]->upload_sem);
	}

	/* make data filenames are all lowercase */
	for (i = 0; i < cfg->total_shells; i++)
		strlwr(cfg->shell[i]->code);

	for (i = 0; i < cfg->total_gurus; i++)
		strlwr(cfg->guru[i]->code);

	for (i = 0; i < cfg->total_txtsecs; i++)
		strlwr(cfg->txtsec[i]->code);

	for (i = 0; i < cfg->total_xtrnsecs; i++)
		strlwr(cfg->xtrnsec[i]->code);

	for (i = 0; i < cfg->total_xtrns; i++)
	{
		strlwr(cfg->xtrn[i]->code);
		prep_dir(cfg->ctrl_dir, cfg->xtrn[i]->path, sizeof(cfg->xtrn[i]->path));
	}
	for (i = 0; i < cfg->total_events; i++) {
		strlwr(cfg->event[i]->code);    /* data filenames are all lowercase */
		prep_dir(cfg->ctrl_dir, cfg->event[i]->dir, sizeof(cfg->event[i]->dir));
	}
	for (i = 0; i < cfg->total_xedits; i++)
		strlwr(cfg->xedit[i]->code);

	cfg->prepped = true;  /* data prepared for run-time, DO NOT SAVE TO DISK! */
}

static void free_loadable_module(struct loadable_module* mod)
{
	strListFree(&mod->cmd);
	strListFree(&mod->ars);
}

static void free_fixed_event(fevent_t* event)
{
	strListFree(&event->cmd);
	FREE_AND_NULL(event->misc);
}

void free_cfg(scfg_t* cfg)
{
	if (cfg->prepped) {
		cfg->prepped = false;
	}
	free_node_cfg(cfg);
	free_main_cfg(cfg);
	free_msgs_cfg(cfg);
	free_file_cfg(cfg);
	free_chat_cfg(cfg);
	free_xtrn_cfg(cfg);

	if (cfg->text != NULL)
		free_text(cfg->text);

	free_fixed_event(&cfg->sys_newuser);
	free_fixed_event(&cfg->sys_logon);
	free_fixed_event(&cfg->sys_logout);
	free_fixed_event(&cfg->sys_daily);
	free_fixed_event(&cfg->sys_weekly);
	free_fixed_event(&cfg->sys_monthly);

	free_loadable_module(&cfg->logon_mod);
	free_loadable_module(&cfg->logoff_mod);
	free_loadable_module(&cfg->newuser_prompts_mod);
	free_loadable_module(&cfg->newuser_info_mod);
	free_loadable_module(&cfg->newuser_mod);
	free_loadable_module(&cfg->login_mod);
	free_loadable_module(&cfg->logout_mod);
	free_loadable_module(&cfg->sync_mod);
	free_loadable_module(&cfg->expire_mod);
	free_loadable_module(&cfg->emailsec_mod);
	free_loadable_module(&cfg->textsec_mod);
	free_loadable_module(&cfg->xtrnsec_mod);
	free_loadable_module(&cfg->chatsec_mod);
	free_loadable_module(&cfg->automsg_mod);
	free_loadable_module(&cfg->feedback_mod);
	free_loadable_module(&cfg->readmail_mod);
	free_loadable_module(&cfg->scanposts_mod);
	free_loadable_module(&cfg->scansubs_mod);
	free_loadable_module(&cfg->listmsgs_mod);
	free_loadable_module(&cfg->scandirs_mod);
	free_loadable_module(&cfg->listfiles_mod);
	free_loadable_module(&cfg->fileinfo_mod);
	free_loadable_module(&cfg->nodelist_mod);
	free_loadable_module(&cfg->whosonline_mod);
	free_loadable_module(&cfg->privatemsg_mod);
	free_loadable_module(&cfg->logonlist_mod);
	free_loadable_module(&cfg->userlist_mod);
	free_loadable_module(&cfg->usercfg_mod);
	free_loadable_module(&cfg->prextrn_mod);
	free_loadable_module(&cfg->postxtrn_mod);
	free_loadable_module(&cfg->tempxfer_mod);
	free_loadable_module(&cfg->batxfer_mod);
	free_loadable_module(&cfg->uselect_mod);
}

void free_text(char* text[])
{
	int i;

	if (text == NULL)
		return;

	for (i = 0; i < TOTAL_TEXT; i++) {
		FREE_AND_NULL(text[i]);
	}
}

/****************************************************************************/
/* If the directory 'path' doesn't exist, create it.                      	*/
/****************************************************************************/
int md(const char* inpath)
{
	char  path[MAX_PATH + 1];
	char* p;

	if (inpath[0] == 0)
		return EINVAL;

	SAFECOPY(path, inpath);

	/* Remove trailing '.' if present */
	p = lastchar(path);
	if (*p == '.')
		*p = '\0';

	/* Remove trailing slash if present */
	p = lastchar(path);
	if (*p == '\\' || *p == '/')
		*p = '\0';

	if (!isdir(path)) {
		if (mkpath(path) != 0) {
			int result = errno;
			if (!isdir(path)) // race condition: did another thread make the directory already?
				return result;
		}
	}

	return 0;
}

/****************************************************************************/
/* Reads in attr.ini and initializes the associated variables               */
/****************************************************************************/
bool read_attr_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char       value[INI_MAX_VALUE_LEN];
	char       path[MAX_PATH + 1];
	FILE*      fp;
	str_list_t ini;

	SAFEPRINTF(path, "%sattr.ini", cfg->ctrl_dir);
	fp = fnopen(NULL, path, O_RDONLY);

	ini = iniReadFile(fp);
	if (fp != NULL)
		fclose(fp);

	cfg->color[clr_userlow]         = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "userlow", "G", value), /* endptr: */ NULL);
	cfg->color[clr_userhigh]        = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "userhigh", "GH", value), /* endptr: */ NULL);
	cfg->color[clr_mnehigh]         = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "mnehigh", "WH", value), /* endptr: */ NULL);
	cfg->color[clr_mnelow]          = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "mnelow", "u", value), /* endptr: */ NULL);
	cfg->color[clr_mnecmd]          = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "mnecmd", "WH", value), /* endptr: */ NULL);
	cfg->color[clr_inputline]       = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "inputline", "WH4E", value), /* endptr: */ NULL);
	cfg->color[clr_err]             = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "error", "RH", value), /* endptr: */ NULL);
	cfg->color[clr_nodenum]         = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "nodenum", "WH", value), /* endptr: */ NULL);
	cfg->color[clr_nodeuser]        = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "nodeuser", "GH", value), /* endptr: */ NULL);
	cfg->color[clr_nodestatus]      = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "nodestatus", "G", value), /* endptr: */ NULL);
	cfg->color[clr_filename]        = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "filename", "BH", value), /* endptr: */ NULL);
	cfg->color[clr_filecdt]         = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "filecdt", "M", value), /* endptr: */ NULL);
	cfg->color[clr_filedesc]        = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "filedesc", "W", value), /* endptr: */ NULL);
	cfg->color[clr_filelsthdrbox]   = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "filelisthdrbox", "YH", value), /* endptr: */ NULL);
	cfg->color[clr_filelstline]     = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "filelistline", "B", value), /* endptr: */ NULL);
	cfg->color[clr_chatlocal]       = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "chatlocal", "GH", value), /* endptr: */ NULL);
	cfg->color[clr_chatremote]      = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "chatremote", "G", value), /* endptr: */ NULL);
	cfg->color[clr_multichat]       = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "multichat", "W", value), /* endptr: */ NULL);
	cfg->color[clr_external]        = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "external", "WH", value), /* endptr: */ NULL);
	cfg->color[clr_votes_full]      = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "votes_full", "WH5", value), /* endptr: */ NULL);
	cfg->color[clr_votes_empty]     = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "votes_empty", "WH", value), /* endptr: */ NULL);
	cfg->color[clr_progress_full]   = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "progress_full", "WH5", value), /* endptr: */ NULL);
	cfg->color[clr_progress_empty]  = strtoattr(cfg, iniGetString(ini, ROOT_SECTION, "progress_empty", "WH", value), /* endptr: */ NULL);

	iniGetString(ini, ROOT_SECTION, "rainbow", "WH,W,CH,C,MH,M,BH,B,YH,Y,GH,G,RH,R,KH", value);
	memset(cfg->rainbow, 0, sizeof cfg->rainbow);
	parse_attr_str_list(cfg, cfg->rainbow, LEN_RAINBOW, value);
	iniFreeStringList(ini);
	return true;
}

char* prep_dir(const char* base, char* path, size_t buflen)
{
#ifdef __unix__
	char *p;
#endif
	char  str[MAX_PATH + 1];
	char  abspath[MAX_PATH + 1];
	char  ch;

	if (!path[0])
		return path;
	if (path[0] != '\\' && path[0] != '/' && path[1] != ':') { /* Relative directory */
		ch = *lastchar(base);
		if (ch == '\\' || ch == '/')
			SAFEPRINTF2(str, "%s%s", base, path);
		else
			SAFEPRINTF3(str, "%s%c%s", base, PATH_DELIM, path);
	} else
		SAFECOPY(str, path);

#ifdef __unix__             /* Change backslashes to forward slashes on Unix */
	for (p = str; *p; p++)
		if (*p == '\\')
			*p = '/';
#endif

	backslashcolon(str);
	SAFECAT(str, ".");               /* Change C: to C:. and C:\SBBS\ to C:\SBBS\. */
	FULLPATH(abspath, str, buflen);   /* Change C:\SBBS\NODE1\..\EXEC to C:\SBBS\EXEC */
	backslash(abspath);

	strncpy(path, abspath, buflen);
	return path;
}

char* prep_path(char* path)
{
#ifdef __unix__             /* Change backslashes to forward slashes on Unix */
	char *p;

	for (p = path; *p; p++)
		if (*p == '\\')
			*p = '/';
#endif

	return path;
}

/* Prepare a string to be used as an internal code */
/* Return the usable code */
char* prep_code(char *str, const char* prefix)
{
	char tmp[1024];
	int  i, j;

	if (prefix != NULL) {  /* skip the grp/lib prefix, if specified */
		i = strlen(prefix);
		if (i && strnicmp(str, prefix, i) == 0 && strlen(str) != i)
			str += i;
	}
	for (i = j = 0; str[i] && i < sizeof(tmp); i++)
		if (str[i] > ' ' && !(str[i] & 0x80) && str[i] != '*' && str[i] != '?' && str[i] != '.'
		    && strchr(ILLEGAL_FILENAME_CHARS, str[i]) == NULL)
			tmp[j++] = toupper(str[i]);
	tmp[j] = 0;
	strcpy(str, tmp);
	if (j > LEN_CODE) {    /* Extra chars? Strip symbolic chars */
		for (i = j = 0; str[i]; i++)
			if (IS_ALPHANUMERIC(str[i]))
				tmp[j++] = str[i];
		tmp[j] = 0;
		strcpy(str, tmp);
	}
	str[LEN_CODE] = 0;
	return str;
}

/****************************************************************************/
/* Auto-toggle daylight savings time in US time-zones						*/
/* or return auto-detected local timezone as an offset from UTC.			*/
/* Does not modify cfg->sys_timezone itself, just the returned copy.		*/
/****************************************************************************/
ushort sys_timezone(scfg_t* cfg)
{
	time_t    now;
	struct tm tm;
	int16_t   sys_timezone = cfg->sys_timezone;

	if (cfg->sys_timezone == SYS_TIMEZONE_AUTO)
		return xpTimeZone_local();

	if (cfg->sys_misc & SM_AUTO_DST && SMB_TZ_HAS_DST(cfg->sys_timezone)) {
		now = time(NULL);
		if (localtime_r(&now, &tm) != NULL) {
			if (tm.tm_isdst > 0)
				sys_timezone |= DAYLIGHT;
			else if (tm.tm_isdst == 0)
				sys_timezone &= ~DAYLIGHT;
		}
	}

	return sys_timezone;
}


int smb_storage_mode(scfg_t* cfg, smb_t* smb)
{
	if (smb == NULL || smb->subnum == INVALID_SUB || (smb->status.attr & SMB_EMAIL))
		return (cfg->sys_misc & SM_FASTMAIL) ? SMB_FASTALLOC : SMB_SELFPACK;
	if (!subnum_is_valid(cfg, smb->subnum))
		return (smb->status.attr & SMB_HYPERALLOC) ? SMB_HYPERALLOC : SMB_FASTALLOC;
	if (cfg->sub[smb->subnum]->misc & SUB_HYPER) {
		smb->status.attr |= SMB_HYPERALLOC;
		return SMB_HYPERALLOC;
	}
	if (cfg->sub[smb->subnum]->misc & SUB_FAST)
		return SMB_FASTALLOC;
	return SMB_SELFPACK;
}

/* Open Synchronet Message Base and create, if necessary (e.g. first time opened) */
/* If return value is not SMB_SUCCESS, sub-board is not left open */
int smb_open_sub(scfg_t* cfg, smb_t* smb, int subnum)
{
	int         retval;
	smbstatus_t smb_status = {0};

	if (subnum != INVALID_SUB && !subnum_is_valid(cfg, subnum))
		return SMB_FAILURE;
	memset(smb, 0, sizeof(smb_t));
	if (subnum == INVALID_SUB) {
		SAFEPRINTF(smb->file, "%smail", cfg->data_dir);
		smb_status.max_crcs = cfg->mail_maxcrcs;
		smb_status.max_msgs = 0;
		smb_status.max_age  = cfg->mail_maxage;
		smb_status.attr     = SMB_EMAIL;
	} else {
		SAFEPRINTF2(smb->file, "%s%s", cfg->sub[subnum]->data_dir, cfg->sub[subnum]->code);
		smb_status.max_crcs = cfg->sub[subnum]->maxcrcs;
		smb_status.max_msgs = cfg->sub[subnum]->maxmsgs;
		smb_status.max_age  = cfg->sub[subnum]->maxage;
		smb_status.attr     = cfg->sub[subnum]->misc & SUB_HYPER ? SMB_HYPERALLOC :0;
	}
	smb->retry_time = cfg->smb_retry_time;
	if ((retval = smb_open(smb)) == SMB_SUCCESS) {
		if (smb_fgetlength(smb->shd_fp) < sizeof(smbhdr_t) + sizeof(smb->status)) {
			smb->status = smb_status;
			if ((retval = smb_create(smb)) != SMB_SUCCESS)
				smb_close(smb);
		}
		if (retval == SMB_SUCCESS)
			smb->subnum = subnum;
	}
	return retval;
}

bool smb_init_dir(scfg_t* cfg, smb_t* smb, int dirnum)
{
	if (!dirnum_is_valid(cfg, dirnum))
		return false;
	memset(smb, 0, sizeof(smb_t));
	SAFEPRINTF2(smb->file, "%s%s", cfg->dir[dirnum]->data_dir, cfg->dir[dirnum]->code);
	smb->retry_time = cfg->smb_retry_time;
	return true;
}

int smb_open_dir(scfg_t* cfg, smb_t* smb, int dirnum)
{
	int retval;

	if (!smb_init_dir(cfg, smb, dirnum))
		return SMB_FAILURE;
	if ((retval = smb_open(smb)) != SMB_SUCCESS)
		return retval;
	smb->dirnum = dirnum;
	if (filelength(fileno(smb->shd_fp)) < 1) {
		smb->status.max_files   = cfg->dir[dirnum]->maxfiles;
		smb->status.max_age     = cfg->dir[dirnum]->maxage;
		smb->status.attr        = SMB_FILE_DIRECTORY;
		if (cfg->dir[dirnum]->misc & DIR_NOHASH)
			smb->status.attr |= SMB_NOHASH;
		if ((retval = smb_create(smb)) != SMB_SUCCESS)
			smb_close(smb);
	}
	return retval;
}

int get_lang_count(scfg_t* cfg)
{
	char   path[MAX_PATH + 1];
	glob_t g;
	int    count;

	snprintf(path, sizeof path, "%stext.*.ini", cfg->ctrl_dir);
	if (glob(path, GLOB_MARK, NULL, &g) != 0)
		return 1;

	count = g.gl_pathc + 1;
	globfree(&g);
	return count;
}

str_list_t get_lang_list(scfg_t* cfg)
{
	str_list_t list = strListInit();
	char       path[MAX_PATH + 1];
	glob_t     g;
	const int  prefix_len = strlen(cfg->ctrl_dir) + 5; // strlen("text.")

	strListPush(&list, ""); // default is blank

	snprintf(path, sizeof path, "%stext.*.ini", cfg->ctrl_dir);
	if (glob(path, GLOB_MARK, NULL, &g) != 0)
		return list;

	for (size_t i = 0; i < g.gl_pathc; ++i) {
		SAFECOPY(path, g.gl_pathv[i] + prefix_len);
		char* p = strchr(path, '.');
		if (p != NULL)
			*p = '\0';
		strListPush(&list, path);
	}
	globfree(&g);
	return list;
}

str_list_t get_lang_desc_list(scfg_t* cfg, char* text[])
{
	str_list_t list = strListInit();
	char       path[MAX_PATH + 1];
	char       value[INI_MAX_VALUE_LEN];
	glob_t     g;

	strListPush(&list, text[LANG]);

	snprintf(path, sizeof path, "%stext.*.ini", cfg->ctrl_dir);
	if (glob(path, GLOB_MARK, NULL, &g) != 0)
		return list;

	for (size_t i = 0; i < g.gl_pathc; ++i) {
		FILE* fp = iniOpenFile(g.gl_pathv[i], /* for_modify */ false);
		if (fp == NULL)
			continue;
		char* p = iniReadString(fp, ROOT_SECTION, "LANG", NULL, value);
		if (p != NULL)
			strListPush(&list, p);
		iniCloseFile(fp);
	}
	globfree(&g);
	return list;
}
