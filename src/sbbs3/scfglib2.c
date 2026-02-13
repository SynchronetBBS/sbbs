/* Synchronet configuration library routines */

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

#include "scfglib.h"
#include "nopen.h"
#include "ars_defs.h"
#include "load_cfg.h"
#include "ini_file.h"

static void read_dir_defaults_cfg(scfg_t* cfg, str_list_t ini, dir_t* dir)
{
	char value[INI_MAX_VALUE_LEN];

	SAFECOPY(dir->data_dir, iniGetString(ini, NULL, "data_dir", "", value));
	SAFECOPY(dir->upload_sem, iniGetString(ini, NULL, "upload_sem", "", value));
	SAFECOPY(dir->exts, iniGetString(ini, NULL, "extensions", "", value));

	dir->maxfiles = iniGetUInteger(ini, NULL, "max_files", 0);
	dir->misc = iniGetInt32(ini, NULL, "settings",  DEFAULT_DIR_OPTIONS);
	dir->seqdev = iniGetUInteger(ini, NULL, "seq_dev", 0);
	dir->sort = iniGetUInteger(ini, NULL, "sort", 0);
	dir->maxage = iniGetUInt16(ini, NULL, "max_age", 0);
	dir->up_pct = iniGetUInt16(ini, NULL, "upload_credit_pct", cfg->cdt_up_pct);
	dir->dn_pct = iniGetUInt16(ini, NULL, "download_credit_pct", cfg->cdt_dn_pct);
}

/****************************************************************************/
/* Reads in file.ini and initializes the associated variables				*/
/****************************************************************************/
bool read_file_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char        errstr[256];
	char        str[128];
	FILE*       fp;
	str_list_t  ini;
	char        value[INI_MAX_VALUE_LEN];

	const char* fname = "file.ini";
	SAFEPRINTF2(cfg->filename, "%s%s", cfg->ctrl_dir, fname);
	if ((fp = fnopen(NULL, cfg->filename, O_RDONLY)) == NULL) {
		safe_snprintf(error, maxerrlen, "%d (%s) opening %s", errno, safe_strerror(errno, errstr, sizeof(errstr)), cfg->filename);
		return false;
	}
	ini = iniReadFile(fp);
	fclose(fp);

	cfg->min_dspace = iniGetBytes(ini, ROOT_SECTION, "min_dspace", 1, 100 * 1024 * 1024);
	cfg->max_batup = iniGetUInt16(ini, ROOT_SECTION, "max_batup", 0);
	cfg->max_batdn = iniGetUInt16(ini, ROOT_SECTION, "max_batdn", 0);
	cfg->max_userxfer = iniGetUInt16(ini, ROOT_SECTION, "max_userxfer", 0);
	cfg->cdt_up_pct = iniGetUInt16(ini, ROOT_SECTION, "upload_credit_pct", 0);
	cfg->cdt_dn_pct = iniGetUInt16(ini, ROOT_SECTION, "download_credit_pct", 0);
	cfg->leech_pct = iniGetUInt16(ini, ROOT_SECTION, "leech_pct", 0);
	cfg->leech_sec = iniGetUInt16(ini, ROOT_SECTION, "leech_sec", 0);
	cfg->file_misc = iniGetInt32(ini, ROOT_SECTION, "settings", 0);
	cfg->filename_maxlen = iniGetIntInRange(ini, ROOT_SECTION, "filename_maxlen", 8, SMB_FILEIDX_NAMELEN, UINT16_MAX);
	SAFECOPY(str, iniGetString(ini, ROOT_SECTION, "supported_archive_formats", "zip,7z,tgz", value));
	cfg->supported_archive_formats = strListSplit(NULL, str, " ,");

	named_str_list_t** sections = iniParseSections(ini);

	/**************************/
	/* Extractable File Types */
	/**************************/

	str_list_t fextr_list = iniGetParsedSectionList(sections, "extractor:");
	cfg->total_fextrs = strListCount(fextr_list);

	if ((cfg->fextr = (fextr_t **)malloc(sizeof(fextr_t *) * cfg->total_fextrs)) == NULL)
		return allocerr(error, maxerrlen, fname, "fextrs", sizeof(fextr_t*) * cfg->total_fextrs);

	for (int i = 0; i < cfg->total_fextrs; i++) {
		if ((cfg->fextr[i] = (fextr_t *)malloc(sizeof(fextr_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "fextr", sizeof(fextr_t));
		str_list_t section = iniGetParsedSection(sections, fextr_list[i], /* cut: */ true);
		memset(cfg->fextr[i], 0, sizeof(fextr_t));
		SAFECOPY(cfg->fextr[i]->ext, iniGetString(section, NULL, "extension", "", value));
		SAFECOPY(cfg->fextr[i]->cmd, iniGetString(section, NULL, "cmd", "", value));
		SAFECOPY(cfg->fextr[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->fextr[i]->arstr, cfg, cfg->fextr[i]->ar);
		cfg->fextr[i]->ex_mode = iniGetUInt32(section, NULL, "ex_mode", 0);
	}
	iniFreeStringList(fextr_list);

	/***************************/
	/* Compressible File Types */
	/***************************/

	str_list_t fcomp_list = iniGetParsedSectionList(sections, "compressor:");
	cfg->total_fcomps = strListCount(fcomp_list);

	if ((cfg->fcomp = (fcomp_t **)malloc(sizeof(fcomp_t *) * cfg->total_fcomps)) == NULL)
		return allocerr(error, maxerrlen, fname, "fcomps", sizeof(fcomp_t*) * cfg->total_fcomps);

	for (int i = 0; i < cfg->total_fcomps; i++) {
		if ((cfg->fcomp[i] = (fcomp_t *)malloc(sizeof(fcomp_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "fcomp", sizeof(fcomp_t));
		str_list_t section = iniGetParsedSection(sections, fcomp_list[i], /* cut: */ true);
		memset(cfg->fcomp[i], 0, sizeof(fcomp_t));
		SAFECOPY(cfg->fcomp[i]->ext, iniGetString(section, NULL, "extension", "", value));
		SAFECOPY(cfg->fcomp[i]->cmd, iniGetString(section, NULL, "cmd", "", value));
		SAFECOPY(cfg->fcomp[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->fcomp[i]->arstr, cfg, cfg->fcomp[i]->ar);
		cfg->fcomp[i]->ex_mode = iniGetUInt32(section, NULL, "ex_mode", 0);
	}
	iniFreeStringList(fcomp_list);

	/***********************/
	/* Viewable File Types */
	/***********************/

	str_list_t fview_list = iniGetParsedSectionList(sections, "viewer:");
	cfg->total_fviews = strListCount(fview_list);

	if ((cfg->fview = (fview_t **)malloc(sizeof(fview_t *) * cfg->total_fviews)) == NULL)
		return allocerr(error, maxerrlen, fname, "fviews", sizeof(fview_t*) * cfg->total_fviews);

	for (int i = 0; i < cfg->total_fviews; i++) {
		if ((cfg->fview[i] = (fview_t *)malloc(sizeof(fview_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "fname", sizeof(fview_t));
		str_list_t section = iniGetParsedSection(sections, fview_list[i], /* cut: */ true);
		memset(cfg->fview[i], 0, sizeof(fview_t));
		SAFECOPY(cfg->fview[i]->ext, iniGetString(section, NULL, "extension", "", value));
		SAFECOPY(cfg->fview[i]->cmd, iniGetString(section, NULL, "cmd", "", value));
		SAFECOPY(cfg->fview[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->fview[i]->arstr, cfg, cfg->fview[i]->ar);
		cfg->fview[i]->ex_mode = iniGetUInt32(section, NULL, "ex_mode", EX_STDIO | EX_SH);
	}
	iniFreeStringList(fview_list);

	/***********************/
	/* Testable File Types */
	/***********************/

	str_list_t ftest_list = iniGetParsedSectionList(sections, "tester:");
	cfg->total_ftests = strListCount(ftest_list);

	if ((cfg->ftest = (ftest_t **)malloc(sizeof(ftest_t *) * cfg->total_ftests)) == NULL)
		return allocerr(error, maxerrlen, fname, "ftests", sizeof(ftest_t*) * cfg->total_ftests);

	for (int i = 0; i < cfg->total_ftests; i++) {
		if ((cfg->ftest[i] = (ftest_t *)malloc(sizeof(ftest_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "ftest", sizeof(ftest_t));
		str_list_t section = iniGetParsedSection(sections, ftest_list[i], /* cut: */ true);
		memset(cfg->ftest[i], 0, sizeof(ftest_t));
		SAFECOPY(cfg->ftest[i]->ext, iniGetString(section, NULL, "extension", "", value));
		SAFECOPY(cfg->ftest[i]->cmd, iniGetString(section, NULL, "cmd", "", value));
		SAFECOPY(cfg->ftest[i]->workstr, iniGetString(section, NULL, "working", "", value));
		SAFECOPY(cfg->ftest[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->ftest[i]->arstr, cfg, cfg->ftest[i]->ar);
		cfg->ftest[i]->ex_mode = iniGetUInt32(section, NULL, "ex_mode", 0);
	}
	iniFreeStringList(ftest_list);

	/*******************/
	/* Download events */
	/*******************/

	str_list_t dlevent_list = iniGetParsedSectionList(sections, "dlevent:");
	cfg->total_dlevents = strListCount(dlevent_list);

	if ((cfg->dlevent = (dlevent_t **)malloc(sizeof(dlevent_t *) * cfg->total_dlevents)) == NULL)
		return allocerr(error, maxerrlen, fname, "dlevents", sizeof(dlevent_t*) * cfg->total_dlevents);

	for (int i = 0; i < cfg->total_dlevents; i++) {
		if ((cfg->dlevent[i] = (dlevent_t *)malloc(sizeof(dlevent_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "dlevent", sizeof(dlevent_t));
		str_list_t section = iniGetParsedSection(sections, dlevent_list[i], /* cut: */ true);
		memset(cfg->dlevent[i], 0, sizeof(dlevent_t));
		SAFECOPY(cfg->dlevent[i]->ext, iniGetString(section, NULL, "extension", "", value));
		SAFECOPY(cfg->dlevent[i]->cmd, iniGetString(section, NULL, "cmd", "", value));
		SAFECOPY(cfg->dlevent[i]->workstr, iniGetString(section, NULL, "working", "", value));
		SAFECOPY(cfg->dlevent[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->dlevent[i]->arstr, cfg, cfg->dlevent[i]->ar);
		cfg->dlevent[i]->ex_mode = iniGetUInt32(section, NULL, "ex_mode", 0);
	}
	iniFreeStringList(dlevent_list);

	/***************************/
	/* File Transfer Protocols */
	/***************************/

	str_list_t prot_list = iniGetParsedSectionList(sections, "protocol:");
	cfg->total_prots = strListCount(prot_list);

	if ((cfg->prot = (prot_t **)malloc(sizeof(prot_t *) * cfg->total_prots)) == NULL)
		return allocerr(error, maxerrlen, fname, "prots", sizeof(prot_t*) * cfg->total_prots);

	for (int i = 0; i < cfg->total_prots; i++) {
		if ((cfg->prot[i] = (prot_t *)malloc(sizeof(prot_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "prot", sizeof(prot_t));
		str_list_t section = iniGetParsedSection(sections, prot_list[i], /* cut: */ true);
		memset(cfg->prot[i], 0, sizeof(prot_t));

		cfg->prot[i]->mnemonic = *iniGetString(section, NULL, "key", "", value);
		SAFECOPY(cfg->prot[i]->name, iniGetString(section, NULL, "name", "", value));
		SAFECOPY(cfg->prot[i]->ulcmd, iniGetString(section, NULL, "ulcmd", "", value));
		SAFECOPY(cfg->prot[i]->dlcmd, iniGetString(section, NULL, "dlcmd", "", value));
		SAFECOPY(cfg->prot[i]->batulcmd, iniGetString(section, NULL, "batulcmd", "", value));
		SAFECOPY(cfg->prot[i]->batdlcmd, iniGetString(section, NULL, "batdlcmd", "", value));
		cfg->prot[i]->misc = iniGetInt32(section, NULL, "settings", 0);
		SAFECOPY(cfg->prot[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->prot[i]->arstr, cfg, cfg->prot[i]->ar);
	}
	iniFreeStringList(prot_list);

	/******************/
	/* File Libraries */
	/******************/

	str_list_t lib_list = iniGetParsedSectionList(sections, "lib:");
	cfg->total_libs = strListCount(lib_list);

	if ((cfg->lib = (lib_t **)malloc(sizeof(lib_t *) * cfg->total_libs)) == NULL)
		return allocerr(error, maxerrlen, fname, "libs", sizeof(lib_t *) * cfg->total_libs);

	for (int i = 0; i < cfg->total_libs; i++) {
		char*      name = lib_list[i];
		if ((cfg->lib[i] = (lib_t *)malloc(sizeof(lib_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "lib", sizeof(lib_t));
		str_list_t section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->lib[i], 0, sizeof(lib_t));
		cfg->lib[i]->offline_dir = INVALID_DIR;
		SAFECOPY(cfg->lib[i]->sname, name + 4);
		SAFECOPY(cfg->lib[i]->lname, iniGetString(section, NULL, "description", name + 4, value));
		SAFECOPY(cfg->lib[i]->code_prefix, iniGetString(section, NULL, "code_prefix", "", value));
		SAFECOPY(cfg->lib[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		SAFECOPY(cfg->lib[i]->ul_arstr, iniGetString(section, NULL, "upload_ars", "", value));
		SAFECOPY(cfg->lib[i]->dl_arstr, iniGetString(section, NULL, "download_ars", "", value));
		SAFECOPY(cfg->lib[i]->op_arstr, iniGetString(section, NULL, "operator_ars", "", value));
		SAFECOPY(cfg->lib[i]->ex_arstr, iniGetString(section, NULL, "exempt_ars", "", value));

		SAFECOPY(cfg->lib[i]->vdir, cfg->lib[i]->sname);
		pathify(cfg->lib[i]->vdir);

		arstr(NULL, cfg->lib[i]->arstr, cfg, cfg->lib[i]->ar);
		arstr(NULL, cfg->lib[i]->ul_arstr, cfg, cfg->lib[i]->ul_ar);
		arstr(NULL, cfg->lib[i]->dl_arstr, cfg, cfg->lib[i]->dl_ar);
		arstr(NULL, cfg->lib[i]->op_arstr, cfg, cfg->lib[i]->op_ar);
		arstr(NULL, cfg->lib[i]->ex_arstr, cfg, cfg->lib[i]->ex_ar);

		SAFECOPY(cfg->lib[i]->parent_path, iniGetString(section, NULL, "parent_path", "", value));
		cfg->lib[i]->sort = iniGetInteger(section, NULL, "sort", 0);
		cfg->lib[i]->misc = iniGetInt32(section, NULL, "settings", 0);
		cfg->lib[i]->vdir_name = iniGetUInteger(section, NULL, "vdir_name", 0);

		char dir_defaults[128];
		SAFEPRINTF(dir_defaults, "dir_defaults:%s", cfg->lib[i]->sname);
		section = iniGetParsedSection(sections, dir_defaults, /* cut: */ true);
		read_dir_defaults_cfg(cfg, section, &cfg->lib[i]->dir_defaults);
	}
	iniFreeStringList(lib_list);

	/********************/
	/* File Directories */
	/********************/

	cfg->sysop_dir = cfg->user_dir = cfg->upload_dir = INVALID_DIR;
	str_list_t dir_list = iniGetParsedSectionList(sections, "dir:");
	cfg->total_dirs = strListCount(dir_list);

	if ((cfg->dir = (dir_t **)malloc(sizeof(dir_t *) * (cfg->total_dirs + 1))) == NULL)
		return allocerr(error, maxerrlen, fname, "dirs", sizeof(dir_t *) * (cfg->total_dirs + 1));

	cfg->total_dirs = 0;
	for (uint i = 0; dir_list[i] != NULL; i++) {
		char        lib[INI_MAX_VALUE_LEN];
		const char* name = dir_list[i];
		SAFECOPY(lib, name + 4);
		char*       p = strrchr(lib, ':');
		if (p == NULL)
			continue;
		*p = '\0';
		char*       code = p + 1;
		int         libnum = getlibnum_from_name(cfg, lib);
		if (!libnum_is_valid(cfg, libnum))
			continue;

		if ((cfg->dir[i] = (dir_t *)malloc(sizeof(dir_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "dir", sizeof(dir_t));
		str_list_t section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->dir[i], 0, sizeof(dir_t));
		SAFECOPY(cfg->dir[i]->code_suffix, code);

		cfg->dir[i]->dirnum = i;
		cfg->dir[i]->lib = libnum;

		SAFECOPY(cfg->dir[i]->lname, iniGetString(section, NULL, "description", code, value));
		SAFECOPY(cfg->dir[i]->sname, iniGetString(section, NULL, "name", code, value));

		if (!stricmp(cfg->dir[i]->sname, "SYSOP"))            /* Sysop upload directory */
			cfg->sysop_dir = i;
		else if (!stricmp(cfg->dir[i]->sname, "USER"))        /* User to User xfer dir */
			cfg->user_dir = i;
		else if (!stricmp(cfg->dir[i]->sname, "UPLOADS"))  /* Upload directory */
			cfg->upload_dir = i;
		else if (!stricmp(cfg->dir[i]->sname, "OFFLINE")) /* Offline files dir */
			cfg->lib[cfg->dir[i]->lib]->offline_dir = i;

		SAFECOPY(cfg->dir[i]->vdir_name, iniGetString(section, NULL, "vdir", "", value));
		init_vdir(cfg, cfg->dir[i]);
		SAFECOPY(cfg->dir[i]->vshortcut, iniGetString(section, NULL, "vshortcut", "", value));

		SAFECOPY(cfg->dir[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		SAFECOPY(cfg->dir[i]->ul_arstr, iniGetString(section, NULL, "upload_ars", "", value));
		SAFECOPY(cfg->dir[i]->dl_arstr, iniGetString(section, NULL, "download_ars", "", value));
		SAFECOPY(cfg->dir[i]->op_arstr, iniGetString(section, NULL, "operator_ars", "", value));
		SAFECOPY(cfg->dir[i]->ex_arstr, iniGetString(section, NULL, "exempt_ars", "", value));

		arstr(NULL, cfg->dir[i]->arstr, cfg, cfg->dir[i]->ar);
		arstr(NULL, cfg->dir[i]->ul_arstr, cfg, cfg->dir[i]->ul_ar);
		arstr(NULL, cfg->dir[i]->dl_arstr, cfg, cfg->dir[i]->dl_ar);
		arstr(NULL, cfg->dir[i]->op_arstr, cfg, cfg->dir[i]->op_ar);
		arstr(NULL, cfg->dir[i]->ex_arstr, cfg, cfg->dir[i]->ex_ar);

		SAFECOPY(cfg->dir[i]->path, iniGetString(section, NULL, "path", "", value));
		SAFECOPY(cfg->dir[i]->area_tag, iniGetString(section, NULL, "area_tag", "", value));

		read_dir_defaults_cfg(cfg, section, cfg->dir[i]);
		++cfg->total_dirs;
	}
	iniFreeStringList(dir_list);

	/**********************/
	/* Text File Sections */
	/**********************/

	str_list_t sec_list = iniGetParsedSectionList(sections, "text:");
	cfg->total_txtsecs = strListCount(sec_list);

	if ((cfg->txtsec = (txtsec_t **)malloc(sizeof(txtsec_t *) * cfg->total_txtsecs)) == NULL)
		return allocerr(error, maxerrlen, fname, "txtsecs", sizeof(txtsec_t *) * cfg->total_txtsecs);

	for (int i = 0; i < cfg->total_txtsecs; i++) {
		const char* name = sec_list[i];
		if ((cfg->txtsec[i] = (txtsec_t *)malloc(sizeof(txtsec_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "txtsec", sizeof(txtsec_t));
		str_list_t  section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->txtsec[i], 0, sizeof(txtsec_t));

		SAFECOPY(cfg->txtsec[i]->code, name + 5);
		SAFECOPY(cfg->txtsec[i]->name, iniGetString(section, NULL, "name", name + 5, value));
		SAFECOPY(cfg->txtsec[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->txtsec[i]->arstr, cfg, cfg->txtsec[i]->ar);
	}
	iniFreeParsedSections(sections);
	iniFreeStringList(sec_list);
	iniFreeStringList(ini);

	return true;
}

/****************************************************************************/
/* Reads in xtrn.ini and initializes the associated variables				*/
/****************************************************************************/
bool read_xtrn_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char        errstr[256];
	FILE*       fp;
	str_list_t  ini;
	char        value[INI_MAX_VALUE_LEN];

	const char* fname = "xtrn.ini";
	SAFEPRINTF2(cfg->filename, "%s%s", cfg->ctrl_dir, fname);
	if ((fp = fnopen(NULL, cfg->filename, O_RDONLY)) == NULL) {
		safe_snprintf(error, maxerrlen, "%d (%s) opening %s", errno, safe_strerror(errno, errstr, sizeof(errstr)), cfg->filename);
		return false;
	}
	ini = iniReadFile(fp);
	fclose(fp);

	named_str_list_t** sections = iniParseSections(ini);

	/********************/
	/* External Editors */
	/********************/

	str_list_t list = iniGetParsedSectionList(sections, "editor:");
	cfg->total_xedits = strListCount(list);

	if ((cfg->xedit = (xedit_t **)malloc(sizeof(xedit_t *) * cfg->total_xedits)) == NULL)
		return allocerr(error, maxerrlen, fname, "xedits", sizeof(xedit_t *) * cfg->total_xedits);

	for (int i = 0; i < cfg->total_xedits; i++) {
		const char* name = list[i];
		if ((cfg->xedit[i] = (xedit_t *)malloc(sizeof(xedit_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "xedit", sizeof(xedit_t));
		str_list_t  section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->xedit[i], 0, sizeof(xedit_t));
		SAFECOPY(cfg->xedit[i]->code, name + 7);
		SAFECOPY(cfg->xedit[i]->name, iniGetString(section, NULL, "name", name + 7, value));
		SAFECOPY(cfg->xedit[i]->rcmd, iniGetString(section, NULL, "cmd", "", value));

		cfg->xedit[i]->misc = iniGetInt32(section, NULL, "settings", 0);
		SAFECOPY(cfg->xedit[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->xedit[i]->arstr, cfg, cfg->xedit[i]->ar);

		cfg->xedit[i]->type = (uint8_t)iniGetUInteger(section, NULL, "type", 0);
		cfg->xedit[i]->soft_cr = iniGetUInteger(section, NULL, "soft_cr", (cfg->xedit[i]->misc & QUICKBBS) ? XEDIT_SOFT_CR_EXPAND : XEDIT_SOFT_CR_RETAIN);
		cfg->xedit[i]->quotewrap_cols = iniGetUInteger(section, NULL, "quotewrap_cols", 0);
	}
	iniFreeStringList(list);

	/*****************************/
	/* External Program Sections */
	/*****************************/

	list = iniGetParsedSectionList(sections, "sec:");
	cfg->total_xtrnsecs = strListCount(list);

	if ((cfg->xtrnsec = (xtrnsec_t **)malloc(sizeof(xtrnsec_t *) * cfg->total_xtrnsecs)) == NULL)
		return allocerr(error, maxerrlen, fname, "xtrnsecs", sizeof(xtrnsec_t *) * cfg->total_xtrnsecs);

	for (int i = 0; i < cfg->total_xtrnsecs; i++) {
		const char* name = list[i];
		if ((cfg->xtrnsec[i] = (xtrnsec_t *)malloc(sizeof(xtrnsec_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "xtrnsec", sizeof(xtrnsec_t));
		str_list_t  section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->xtrnsec[i], 0, sizeof(xtrnsec_t));
		SAFECOPY(cfg->xtrnsec[i]->code, name + 4);
		SAFECOPY(cfg->xtrnsec[i]->name, iniGetString(section, NULL, "name", name + 4, value));
		SAFECOPY(cfg->xtrnsec[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->xtrnsec[i]->arstr, cfg, cfg->xtrnsec[i]->ar);
	}
	iniFreeStringList(list);

	/*********************/
	/* External Programs */
	/*********************/

	list = iniGetParsedSectionList(sections, "prog:");
	cfg->total_xtrns = strListCount(list);

	if ((cfg->xtrn = (xtrn_t **)malloc(sizeof(xtrn_t *) * cfg->total_xtrns)) == NULL)
		return allocerr(error, maxerrlen, fname, "xtrns", sizeof(xtrn_t *) * cfg->total_xtrns);

	cfg->total_xtrns = 0;
	for (uint i = 0; list[i] != NULL; i++) {
		char        sec[INI_MAX_VALUE_LEN];
		const char* name = list[i];
		SAFECOPY(sec, name + 5);
		char*       p = strchr(sec, ':');
		if (p == NULL)
			continue;
		*p = '\0';
		char*       code = p + 1;
		int         secnum = getxtrnsec(cfg, sec);
		if (!xtrnsec_is_valid(cfg, secnum))
			continue;

		if ((cfg->xtrn[i] = (xtrn_t *)malloc(sizeof(xtrn_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "xtrn", sizeof(xtrn_t));
		str_list_t section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->xtrn[i], 0, sizeof(xtrn_t));
		cfg->xtrn[i]->sec = secnum;

		SAFECOPY(cfg->xtrn[i]->name, iniGetString(section, NULL, "name", code, value));
		SAFECOPY(cfg->xtrn[i]->code, code);
		SAFECOPY(cfg->xtrn[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		SAFECOPY(cfg->xtrn[i]->run_arstr, iniGetString(section, NULL, "execution_ars", "", value));
		arstr(NULL, cfg->xtrn[i]->arstr, cfg, cfg->xtrn[i]->ar);
		arstr(NULL, cfg->xtrn[i]->run_arstr, cfg, cfg->xtrn[i]->run_ar);

		cfg->xtrn[i]->type = (uint8_t)iniGetUInteger(section, NULL, "type", 0);
		cfg->xtrn[i]->misc = iniGetInt32(section, NULL, "settings", 0);
		cfg->xtrn[i]->event = (uint8_t)iniGetUInteger(section, NULL, "event", 0);
		cfg->xtrn[i]->cost = iniGetInt32(section, NULL, "cost", 0);
		SAFECOPY(cfg->xtrn[i]->cmd, iniGetString(section, NULL, "cmd", "", value));
		SAFECOPY(cfg->xtrn[i]->clean, iniGetString(section, NULL, "clean_cmd", "", value));
		SAFECOPY(cfg->xtrn[i]->path, iniGetString(section, NULL, "startup_dir", "", value));
		cfg->xtrn[i]->textra = (uint8_t)iniGetUInteger(section, NULL, "textra", 0);
		cfg->xtrn[i]->maxtime = (uint8_t)iniGetUInteger(section, NULL, "max_time", 0);
		cfg->xtrn[i]->max_inactivity = (uint)iniGetDuration(section, NULL, "max_inactivity", 0);
		++cfg->total_xtrns;
	}
	iniFreeStringList(list);

	/****************/
	/* Timed Events */
	/****************/

	list = iniGetParsedSectionList(sections, "event:");
	cfg->total_events = strListCount(list);

	if ((cfg->event = (event_t **)malloc(sizeof(event_t *) * cfg->total_events)) == NULL)
		return allocerr(error, maxerrlen, fname, "events", sizeof(event_t *) * cfg->total_events);

	for (int i = 0; i < cfg->total_events; i++) {
		const char* name = list[i];
		if ((cfg->event[i] = (event_t *)malloc(sizeof(event_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "event", sizeof(event_t));
		memset(cfg->event[i], 0, sizeof(event_t));
		str_list_t  section = iniGetParsedSection(sections, name, /* cut: */ true);

		SAFECOPY(cfg->event[i]->code, name + 6);
		SAFECOPY(cfg->event[i]->cmd, iniGetString(section, NULL, "cmd", "", value));
		SAFECOPY(cfg->event[i]->xtrn, iniGetString(section, NULL, "xtrn", "", value));
		cfg->event[i]->days = (uint8_t)iniGetUInteger(section, NULL, "days", 0);
		cfg->event[i]->time = iniGetUInteger(section, NULL, "time", 0);
		cfg->event[i]->node = iniGetUInteger(section, NULL, "node_num", 0);
		cfg->event[i]->misc = iniGetInt32(section, NULL, "settings", 0);
		SAFECOPY(cfg->event[i]->dir, iniGetString(section, NULL, "startup_dir", "", value));
		cfg->event[i]->freq = iniGetUInt16(section, NULL, "freq", 0);
		cfg->event[i]->mdays = iniGetUInt32(section, NULL, "mdays", 0);
		cfg->event[i]->months = iniGetUInt16(section, NULL, "months", 0);
		cfg->event[i]->errlevel = (uint8_t)iniGetUInteger(section, NULL, "errlevel", LOG_ERR);

		// You can't require exclusion *and* not specify which node/instance will execute the event
		if (cfg->event[i]->node == NODE_ANY)
			cfg->event[i]->misc &= ~EVENT_EXCL;
	}
	iniFreeStringList(list);

	/************************************/
	/* Native (not MS-DOS) Program list */
	/************************************/

	list = iniGetParsedSectionList(sections, "native:");
	cfg->total_natvpgms = strListCount(list);

	if ((cfg->natvpgm = (natvpgm_t **)malloc(sizeof(natvpgm_t *) * cfg->total_natvpgms)) == NULL)
		return allocerr(error, maxerrlen, fname, "natvpgms", sizeof(natvpgm_t *) * cfg->total_natvpgms);

	for (int i = 0; i < cfg->total_natvpgms; i++) {
		const char* name = list[i];
		if ((cfg->natvpgm[i] = (natvpgm_t *)malloc(sizeof(natvpgm_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "natvpgm", sizeof(natvpgm_t));
		memset(cfg->natvpgm[i], 0, sizeof(natvpgm_t));
		SAFECOPY(cfg->natvpgm[i]->name, name + 7);
	}
	iniFreeStringList(list);

	/*******************/
	/* Global Hot Keys */
	/*******************/

	list = iniGetParsedSectionList(sections, "hotkey:");
	cfg->total_hotkeys = strListCount(list);

	if ((cfg->hotkey = (hotkey_t **)malloc(sizeof(hotkey_t *) * cfg->total_hotkeys)) == NULL)
		return allocerr(error, maxerrlen, fname, "hotkeys", sizeof(hotkey_t *) * cfg->total_hotkeys);

	for (int i = 0; i < cfg->total_hotkeys; i++) {
		const char* section = list[i];
		if ((cfg->hotkey[i] = (hotkey_t *)malloc(sizeof(hotkey_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "hotkey", sizeof(hotkey_t));
		memset(cfg->hotkey[i], 0, sizeof(hotkey_t));

		cfg->hotkey[i]->key = atoi(list[i] + 7);
		SAFECOPY(cfg->hotkey[i]->cmd, iniGetString(ini, section, "cmd", "", value));
	}
	iniFreeStringList(list);

	/************************************/
	/* External Program-related Toggles */
	/************************************/
	cfg->xtrn_misc = iniGetInt32(ini, ROOT_SECTION, "settings", 0);

	iniFreeParsedSections(sections);
	iniFreeStringList(ini);

	return true;
}

/****************************************************************************/
/* Reads in chat.ini and initializes the associated variables				*/
/****************************************************************************/
bool read_chat_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char        errstr[256];
	FILE*       fp;
	str_list_t  ini;
	char        value[INI_MAX_VALUE_LEN];

	const char* fname = "chat.ini";
	SAFEPRINTF2(cfg->filename, "%s%s", cfg->ctrl_dir, fname);
	if ((fp = fnopen(NULL, cfg->filename, O_RDONLY)) == NULL) {
		safe_snprintf(error, maxerrlen, "%d (%s) opening %s", errno, safe_strerror(errno, errstr, sizeof(errstr)), cfg->filename);
		return false;
	}
	ini = iniReadFile(fp);
	fclose(fp);

	named_str_list_t** sections = iniParseSections(ini);

	/*********/
	/* Gurus */
	/*********/

	str_list_t list = iniGetParsedSectionList(sections, "guru:");
	cfg->total_gurus = strListCount(list);

	if ((cfg->guru = (guru_t **)malloc(sizeof(guru_t *) * cfg->total_gurus)) == NULL)
		return allocerr(error, maxerrlen, fname, "gurus", sizeof(guru_t *) * cfg->total_gurus);

	for (int i = 0; i < cfg->total_gurus; i++) {
		const char* name = list[i];
		if ((cfg->guru[i] = (guru_t *)malloc(sizeof(guru_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "guru", sizeof(guru_t));
		str_list_t  section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->guru[i], 0, sizeof(guru_t));

		SAFECOPY(cfg->guru[i]->name, iniGetString(section, NULL, "name", name + 5, value));
		SAFECOPY(cfg->guru[i]->code, name + 5);

		SAFECOPY(cfg->guru[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->guru[i]->arstr, cfg, cfg->guru[i]->ar);
	}
	iniFreeStringList(list);

	/********************/
	/* Chat Action Sets */
	/********************/

	list = iniGetParsedSectionList(sections, "actions:");
	cfg->total_actsets = strListCount(list);

	if ((cfg->actset = (actset_t **)malloc(sizeof(actset_t *) * cfg->total_actsets)) == NULL)
		return allocerr(error, maxerrlen, fname, "actsets", sizeof(actset_t *) * cfg->total_actsets);

	cfg->total_chatacts = 0;
	for (int i = 0; i < cfg->total_actsets; i++) {
		const char* name = list[i];
		if ((cfg->actset[i] = (actset_t *)malloc(sizeof(actset_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "actset", sizeof(actset_t));
		str_list_t  section = iniGetParsedSection(sections, name, /* cut: */ true);
		SAFECOPY(cfg->actset[i]->name, name + 8);
		str_list_t  act_list = iniGetKeyList(section, NULL);
		for (uint j = 0; act_list != NULL && act_list[j] != NULL; j++) {
			chatact_t** np = realloc(cfg->chatact, sizeof(chatact_t *) * (cfg->total_chatacts + 1));
			if (np == NULL)
				return allocerr(error, maxerrlen, fname, "chatacts", sizeof(chatact_t *) * (cfg->total_chatacts + 1));
			cfg->chatact = np;
			cfg->chatact[cfg->total_chatacts] = malloc(sizeof(chatact_t));
			chatact_t* act = cfg->chatact[cfg->total_chatacts];
			if (act == NULL)
				return allocerr(error, maxerrlen, fname, "chatact", sizeof(chatact_t));
			cfg->total_chatacts++;
			act->actset = i;
			SAFECOPY(act->cmd, act_list[j]);
			SAFECOPY(act->out, iniGetString(section, NULL, act_list[j], "", value));
		}
		iniFreeStringList(act_list);
	}
	iniFreeStringList(list);

	/***************************/
	/* Multinode Chat Channels */
	/***************************/

	list = iniGetParsedSectionList(sections, "chan:");
	cfg->total_chans = strListCount(list);

	if ((cfg->chan = (chan_t **)malloc(sizeof(chan_t *) * cfg->total_chans)) == NULL)
		return allocerr(error, maxerrlen, fname, "chans", sizeof(chan_t *) * cfg->total_chans);

	for (int i = 0; i < cfg->total_chans; i++) {
		const char* name = list[i];
		if ((cfg->chan[i] = (chan_t *)malloc(sizeof(chan_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "chan", sizeof(chan_t));
		str_list_t  section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->chan[i], 0, sizeof(chan_t));

		cfg->chan[i]->actset = getchatactset(cfg, iniGetString(section, NULL, "actions", "", value));
		SAFECOPY(cfg->chan[i]->name, iniGetString(section, NULL, "name", "", value));

		SAFECOPY(cfg->chan[i]->code, name + 5);

		SAFECOPY(cfg->chan[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->chan[i]->arstr, cfg, cfg->chan[i]->ar);

		cfg->chan[i]->cost = iniGetUInt32(section, NULL, "cost", 0);
		cfg->chan[i]->guru = getgurunum(cfg, iniGetString(section, NULL, "guru", "", value));
		cfg->chan[i]->misc = iniGetUInt32(section, NULL, "settings", 0);
	}
	iniFreeStringList(list);

	/***************/
	/* Chat Pagers */
	/***************/

	list = iniGetParsedSectionList(sections, "pager:");
	cfg->total_pages = strListCount(list);

	if ((cfg->page = (page_t **)malloc(sizeof(page_t *) * cfg->total_pages)) == NULL)
		return allocerr(error, maxerrlen, fname, "pages", sizeof(page_t *) * cfg->total_pages);

	for (int i = 0; i < cfg->total_pages; i++) {
		const char* name = list[i];
		if ((cfg->page[i] = (page_t *)malloc(sizeof(page_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "page", sizeof(page_t));
		str_list_t  section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->page[i], 0, sizeof(page_t));

		SAFECOPY(cfg->page[i]->cmd, iniGetString(section, NULL, "cmd", "", value));

		SAFECOPY(cfg->page[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->page[i]->arstr, cfg, cfg->page[i]->ar);

		cfg->page[i]->misc = iniGetInt32(section, NULL, "settings", 0);
	}
	iniFreeParsedSections(sections);
	iniFreeStringList(list);
	iniFreeStringList(ini);

	return true;
}

/****************************************************************************/
/* Read one line of up to 256 characters from the file pointed to by		*/
/* 'stream' and put upto 'maxlen' number of character into 'outstr' and     */
/* truncate spaces off end of 'outstr'.                                     */
/****************************************************************************/
char *readline(long *offset, char *outstr, int maxlen, FILE *instream)
{
	char str[257];

	if (fgets(str, 256, instream) == NULL)
		return NULL;
	sprintf(outstr, "%.*s", maxlen, str);
	truncsp(outstr);
	(*offset) += maxlen;
	return outstr;
}

/****************************************************************************/
/* Turns char string of flags into a uint32_t								*/
/****************************************************************************/
uint32_t aftou32(const char *str)
{
	char     ch;
	size_t   c = 0;
	uint32_t l = 0;

	while (str[c]) {
		ch = toupper(str[c]);
		if (ch >= 'A' && ch <= 'Z')
			l |= FLAG(ch);
		c++;
	}
	return l;
}

/*****************************************************************************/
/* Converts a long into an ASCII Flag string (A-Z) that represents bits 0-25 */
/*****************************************************************************/
char *u32toaf(uint32_t l, char *str)
{
	int c = 0;

	while (c < 26) {
		if (l & (1 << c))
			str[c] = 'A' + c;
		else
			str[c] = ' ';
		c++;
	}
	str[c] = 0;
	return str;
}

/****************************************************************************/
/* Returns the actual attribute code from a string of ATTR characters       */
/* Ignores any Ctrl-A characters in the string (as attrstr() used to)       */
/****************************************************************************/
uint strtoattr(scfg_t* cfg, const char *str, char** endptr)
{
	int   atr;
	ulong l = 0;

	atr = LIGHTGRAY;
	while (str[l]) {
		switch (toupper(str[l])) {
			case CTRL_A:
				break;
			case 'H':   /* High intensity */
				atr |= HIGH;
				break;
			case 'I':   /* Blink */
				atr |= BLINK;
				break;
			case 'E':   /* iCE color */
				atr |= BG_BRIGHT;
				break;
			case 'K':   /* Black */
				atr = (atr & 0xf8) | BLACK;
				break;
			case 'W':   /* White */
				atr = (atr & 0xf8) | LIGHTGRAY;
				break;
			case 'R':
				atr = (uchar)((atr & 0xf8) | RED);
				break;
			case 'G':
				atr = (uchar)((atr & 0xf8) | GREEN);
				break;
			case 'Y':   /* Yellow */
				atr = (uchar)((atr & 0xf8) | BROWN);
				break;
			case 'B':
				atr = (uchar)((atr & 0xf8) | BLUE);
				break;
			case 'M':
				atr = (uchar)((atr & 0xf8) | MAGENTA);
				break;
			case 'C':
				atr = (uchar)((atr & 0xf8) | CYAN);
				break;
			case '0':   /* Black Background */
				atr = (uchar)(atr & 0x8f);
				break;
			case '1':   /* Red Background */
				atr = (uchar)((atr & 0x8f) | BG_RED);
				break;
			case '2':   /* Green Background */
				atr = (uchar)((atr & 0x8f) | BG_GREEN);
				break;
			case '3':   /* Yellow Background */
				atr = (uchar)((atr & 0x8f) | BG_BROWN);
				break;
			case '4':   /* Blue Background */
				atr = (uchar)((atr & 0x8f) | BG_BLUE);
				break;
			case '5':   /* Magenta Background */
				atr = (uchar)((atr & 0x8f) | BG_MAGENTA);
				break;
			case '6':   /* Cyan Background */
				atr = (uchar)((atr & 0x8f) | BG_CYAN);
				break;
			case '7':   /* White Background */
				atr = (uchar)((atr & 0x8f) | BG_LIGHTGRAY);
				break;
			case 'U':	/* User Theme */
				atr = cfg->color[str[l] == 'u' ? clr_userlow : clr_userhigh];
				break;
			default:
				if (endptr != NULL)
					*endptr = (char*)str + l;
				return atr;
		}
		l++;
	}
	if (endptr != NULL)
		*endptr = (char*)str + l;
	return atr;
}

void parse_attr_str_list(scfg_t* cfg, uint* list, int max, const char* str)
{
	char* endptr = NULL;
	for (int i = 0; i < max && *str != '\0'; ++i) {
		list[i] = strtoattr(cfg, str, &endptr);
		if (*endptr == '\0')
			break;
		str = endptr + 1;
	}
}

void free_file_cfg(scfg_t* cfg)
{
	int i;

	strListFree(&cfg->supported_archive_formats);
	if (cfg->fextr != NULL) {
		for (i = 0; i < cfg->total_fextrs; i++) {
			FREE_AND_NULL(cfg->fextr[i]);
		}
		FREE_AND_NULL(cfg->fextr);
	}
	cfg->total_fextrs = 0;

	if (cfg->fcomp != NULL) {
		for (i = 0; i < cfg->total_fcomps; i++) {
			FREE_AND_NULL(cfg->fcomp[i]);
		}
		FREE_AND_NULL(cfg->fcomp);
	}
	cfg->total_fcomps = 0;

	if (cfg->fview != NULL) {
		for (i = 0; i < cfg->total_fviews; i++) {
			FREE_AND_NULL(cfg->fview[i]);
		}
		FREE_AND_NULL(cfg->fview);
	}
	cfg->total_fviews = 0;

	if (cfg->ftest != NULL) {
		for (i = 0; i < cfg->total_ftests; i++) {
			FREE_AND_NULL(cfg->ftest[i]);
		}
		FREE_AND_NULL(cfg->ftest);
	}
	cfg->total_ftests = 0;

	if (cfg->dlevent != NULL) {
		for (i = 0; i < cfg->total_dlevents; i++) {
			FREE_AND_NULL(cfg->dlevent[i]);
		}
		FREE_AND_NULL(cfg->dlevent);
	}
	cfg->total_dlevents = 0;

	if (cfg->prot != NULL) {
		for (i = 0; i < cfg->total_prots; i++) {
			FREE_AND_NULL(cfg->prot[i]);
		}
		FREE_AND_NULL(cfg->prot);
	}
	cfg->total_prots = 0;

	if (cfg->lib != NULL) {
		for (i = 0; i < cfg->total_libs; i++) {
			FREE_AND_NULL(cfg->lib[i]);
		}
		FREE_AND_NULL(cfg->lib);
	}
	cfg->total_libs = 0;

	if (cfg->dir != NULL) {
		for (i = 0; i < cfg->total_dirs; i++) {
			FREE_AND_NULL(cfg->dir[i]);
		}
		FREE_AND_NULL(cfg->dir);
	}
	cfg->total_dirs = 0;

	if (cfg->txtsec != NULL) {
		for (i = 0; i < cfg->total_txtsecs; i++) {
			FREE_AND_NULL(cfg->txtsec[i]);
		}
		FREE_AND_NULL(cfg->txtsec);
	}
	cfg->total_txtsecs = 0;
}

void free_chat_cfg(scfg_t* cfg)
{
	int i;

	if (cfg->actset != NULL) {
		for (i = 0; i < cfg->total_actsets; i++) {
			FREE_AND_NULL(cfg->actset[i]);
		}
		FREE_AND_NULL(cfg->actset);
	}
	cfg->total_actsets = 0;

	if (cfg->chatact != NULL) {
		for (i = 0; i < cfg->total_chatacts; i++) {
			FREE_AND_NULL(cfg->chatact[i]);
		}
		FREE_AND_NULL(cfg->chatact);
	}
	cfg->total_chatacts = 0;

	if (cfg->chan != NULL) {
		for (i = 0; i < cfg->total_chans; i++) {
			FREE_AND_NULL(cfg->chan[i]);
		}
		FREE_AND_NULL(cfg->chan);
	}
	cfg->total_chans = 0;

	if (cfg->guru != NULL) {
		for (i = 0; i < cfg->total_gurus; i++) {
			FREE_AND_NULL(cfg->guru[i]);
		}
		FREE_AND_NULL(cfg->guru);
	}
	cfg->total_gurus = 0;

	if (cfg->page != NULL) {
		for (i = 0; i < cfg->total_pages; i++) {
			FREE_AND_NULL(cfg->page[i]);
		}
		FREE_AND_NULL(cfg->page);
	}
	cfg->total_pages = 0;

}

void free_xtrn_cfg(scfg_t* cfg)
{
	int i;

	if (cfg->swap != NULL) {
		for (i = 0; i < cfg->total_swaps; i++) {
			FREE_AND_NULL(cfg->swap[i]);
		}
		FREE_AND_NULL(cfg->swap);
	}
	cfg->total_swaps = 0;

	if (cfg->xedit != NULL) {
		for (i = 0; i < cfg->total_xedits; i++) {
			FREE_AND_NULL(cfg->xedit[i]);
		}
		FREE_AND_NULL(cfg->xedit);
	}
	cfg->total_xedits = 0;

	if (cfg->xtrnsec != NULL) {
		for (i = 0; i < cfg->total_xtrnsecs; i++) {
			FREE_AND_NULL(cfg->xtrnsec[i]);
		}
		FREE_AND_NULL(cfg->xtrnsec);
	}
	cfg->total_xtrnsecs = 0;

	if (cfg->xtrn != NULL) {
		for (i = 0; i < cfg->total_xtrns; i++) {
			FREE_AND_NULL(cfg->xtrn[i]);
		}
		FREE_AND_NULL(cfg->xtrn);
	}
	cfg->total_xtrns = 0;

	if (cfg->event != NULL) {
		for (i = 0; i < cfg->total_events; i++) {
			FREE_AND_NULL(cfg->event[i]);
		}
		FREE_AND_NULL(cfg->event);
	}
	cfg->total_events = 0;

	if (cfg->natvpgm != NULL) {
		for (i = 0; i < cfg->total_natvpgms; i++) {
			FREE_AND_NULL(cfg->natvpgm[i]);
		}
		FREE_AND_NULL(cfg->natvpgm);
	}
	cfg->total_natvpgms = 0;

	if (cfg->hotkey != NULL) {
		for (i = 0; i < cfg->total_hotkeys; ++i)
			free(cfg->hotkey[i]);
		FREE_AND_NULL(cfg->hotkey);
	}
	cfg->total_hotkeys = 0;
}
