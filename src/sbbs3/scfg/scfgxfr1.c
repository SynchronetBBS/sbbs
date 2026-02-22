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

char* testable_files_help =
	"`Testable File Types:`\n"
	"\n"
	"These file types each have a command line that will be executed to test\n"
	"the integrity of a file upon upload. These file types are specified by\n"
	"file `extension` (suffix) and if one file extension is listed more than\n"
	"once, each command line will be executed. A file extension of '`*`'\n"
	"matches `all` files.\n"
	"\n"
	"The configured command lines must return an error code of 0 (no error)\n"
	"to indicate that the file has passed each test. This method of file\n"
	"testing upon upload is also known as an upload processing event.\n"
	"\n"
	"In the command line, `%f` represents the path to the file to be tested\n"
	"while `%s` represents the path to the `sbbsfile.des` file that contains the\n"
	"file's description (which may be modified by the testing command line).\n"
	"\n"
	"This test can do more than just test the file; it can perform any\n"
	"function that the sysop wishes, such as adding comments to an archived\n"
	"file, or extracting an archive and performing a virus scan. It's even\n"
	"possible for a file tester to rename a file or change its description\n"
	"by modifying the contents of the `sbbsfile.nam` or `sbbsfile.des` files in\n"
	"the node directory.\n"
	"\n"
	"While the command line is executing, an optional text string may be\n"
	"displayed to the user. This `working string` can be set uniquely\n"
	"for each testable file type.\n"
;

extern char* native_help;
extern char* native_opt;

bool get_new_prot(prot_t* prot)
{
	char str[2];
	memset(prot, 0, sizeof *prot);
	prot->misc = PROT_NATIVE;
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Mnemonic (Command Key)"
				, str, 1, K_UPPER) != 1)
		return false;
	prot->mnemonic = str[0];
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Protocol Name", prot->name, sizeof prot->name, K_TRIM) <= 0)
		return false;
	uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
	return uifc.input(WIN_MID | WIN_SAV, 0, 0, "Upload Command Line", prot->ulcmd, sizeof(prot->ulcmd), K_TRIM) > 0
		&& uifc.input(WIN_MID | WIN_SAV, 0, 0, "Download Command Line", prot->dlcmd, sizeof(prot->dlcmd), K_TRIM) > 0;
}

bool get_new_fextr(fextr_t* fextr)
{
	memset(fextr, 0, sizeof *fextr);
	fextr->ex_mode = EX_NATIVE;
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "File Extension (e.g. zip or *)", fextr->ext, MAX_FILEEXT_LEN, K_UPPER | K_TRIM) <= 0)
		return false;
	uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
	return uifc.input(WIN_MID | WIN_SAV, 0, 0, "Command Line", fextr->cmd, LEN_CMD, K_TRIM) > 0;
}

bool get_new_fview(fview_t* fview)
{
	memset(fview, 0, sizeof *fview);
	fview->ex_mode = EX_NATIVE;
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "File Extension (e.g. txt, zip or *)", fview->ext, MAX_FILEEXT_LEN, K_UPPER | K_TRIM) <= 0)
		return false;
	uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
	return uifc.input(WIN_MID | WIN_SAV, 0, 0, "Command Line", fview->cmd, LEN_CMD, K_TRIM) > 0;
}

bool get_new_ftest(ftest_t* ftest)
{
	memset(ftest, 0, sizeof *ftest);
	ftest->ex_mode = EX_NATIVE;
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "File Extension (e.g. zip or *)", ftest->ext, MAX_FILEEXT_LEN, K_UPPER | K_TRIM) < 1)
		return false;
	uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
	return uifc.input(WIN_MID | WIN_SAV, 0, 0, "Command Line", ftest->cmd, LEN_CMD, K_TRIM) > 0;
}

bool get_new_fcomp(fcomp_t* fcomp)
{
	memset(fcomp, 0, sizeof *fcomp);
	fcomp->ex_mode = EX_NATIVE;
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "File Extension (e.g. zip or *)", fcomp->ext, MAX_FILEEXT_LEN, K_UPPER | K_TRIM) < 1)
		return false;
	uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
	return uifc.input(WIN_MID | WIN_SAV, 0, 0, "Command Line", fcomp->cmd, LEN_CMD, K_TRIM) > 0;
}

bool get_new_dlevent(dlevent_t* dlevent)
{
	memset(dlevent, 0, sizeof *dlevent);
	dlevent->ex_mode = EX_NATIVE;
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "File Extension (e.g. zip or *)", dlevent->ext, MAX_FILEEXT_LEN, K_UPPER | K_TRIM) < 1)
		return false;
	uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
	return uifc.input(WIN_MID | WIN_SAV, 0, 0, "Command Line", dlevent->cmd, LEN_CMD, K_TRIM) > 0;
}

void xfer_opts()
{
	char             str[128], done;
	int              i, j, l;
	static int       xfr_dflt;
	static int       fextr_dflt;
	static int       fextr_bar;
	static int       fextr_opt;
	static int       fview_dflt;
	static int       fview_bar;
	static int       fview_opt;
	static int       ftest_dflt;
	static int       ftest_bar;
	static int       ftest_opt;
	static int       fcomp_dflt;
	static int       fcomp_bar;
	static int       fcomp_opt;
	static int       prot_dflt;
	static int       prot_bar;
	static int       prot_opt;
	static int       dlevent_dflt;
	static int       dlevent_bar;
	static int       dlevent_opt;
	static fextr_t   savfextr;
	static fview_t   savfview;
	static ftest_t   savftest;
	static fcomp_t   savfcomp;
	static prot_t    savprot;
	static dlevent_t savdlevent;

	while (1) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Min Bytes Free Disk Space"
		         , byte_count_to_str(cfg.min_dspace, str, sizeof(str)));
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%u", "Max Files in Batch UL Queue"
		         , cfg.max_batup);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%u", "Max Files in Batch DL Queue"
		         , cfg.max_batdn);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%u", "Max Users in User Transfers"
		         , cfg.max_userxfer);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%u%%", "Default Credit on Upload"
		         , cfg.cdt_up_pct);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%u%%", "Default Credit on Download"
		         , cfg.cdt_dn_pct);
		if (cfg.leech_pct)
			snprintf(str, sizeof str, "%u%% after %u seconds"
			         , cfg.leech_pct, cfg.leech_sec);
		else
			SAFECOPY(str, "<disabled>");
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Leech Protocol Detection", str);
		if (cfg.file_misc & FM_SAFEST)
			SAFECOPY(str, "Safest Subset");
		else
			SAFEPRINTF2(str, "Most %s, %scluding Spaces"
			            , cfg.file_misc & FM_EXASCII ? "CP437" : "ASCII"
			            , cfg.file_misc & FM_SPACES ? "In" : "Ex");
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%u characters", "Allowed Filename Length", cfg.filename_maxlen);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Allowed Filename Characters", str);
		snprintf(opt[i++], MAX_OPLN, "%-33.33s%s", "Supported Archive Formats"
		                , strListCombine(cfg.supported_archive_formats, str, sizeof str, ", "));
		snprintf(opt[i++], MAX_OPLN, "Viewable Files...");
		snprintf(opt[i++], MAX_OPLN, "Testable Files...");
		snprintf(opt[i++], MAX_OPLN, "Download Events...");
		snprintf(opt[i++], MAX_OPLN, "Extractable Files...");
		snprintf(opt[i++], MAX_OPLN, "Compressible Files...");
		snprintf(opt[i++], MAX_OPLN, "Transfer Protocols...");
		opt[i][0] = 0;
		uifc.helpbuf =
			"`File Options:`\n"
			"\n"
			"This menu has options and sub-menus that pertain specifically to the\n"
			"file transfer section of the BBS.\n"
		;
		switch (uifc.list(WIN_ORG | WIN_ACT | WIN_CHE, 0, 0, 72, &xfr_dflt, 0
		                  , "File Options", opt)) {
			case -1:
				i = save_changes(WIN_MID);
				if (i == -1)
					break;
				if (!i) {
					save_file_cfg(&cfg);
					refresh_cfg(&cfg);
				}
				return;
			case __COUNTER__:
				uifc.helpbuf =
					"`Minimum Byte Free Disk Space to Allow Uploads:`\n"
					"\n"
					"This is the minimum free space in a file directory to allow user\n"
					"uploads.\n"
				;
				byte_count_to_str(cfg.min_dspace, str, sizeof(str));
				if (uifc.input(WIN_MID, 0, 0
				               , "Minimum Bytes Free Disk Space to Allow Uploads"
				               , str, 10, K_UPPER | K_EDIT) > 0)
					cfg.min_dspace = parse_byte_count(str, 1);
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`Maximum Files in Batch Upload Queue:`\n"
					"\n"
					"This is the maximum number of files that can be placed in the batch\n"
					"upload queue.\n"
				;
				uifc.input(WIN_MID, 0, 0, "Maximum Files in Batch Upload Queue"
				           , ultoa(cfg.max_batup, tmp, 10), 5, K_EDIT | K_NUMBER);
				cfg.max_batup = atoi(tmp);
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`Maximum Files in Batch Download Queue:`\n"
					"\n"
					"This is the maximum number of files that can be placed in the batch\n"
					"download queue.\n"
				;
				uifc.input(WIN_MID, 0, 0, "Maximum Files in Batch Download Queue"
				           , ultoa(cfg.max_batdn, tmp, 10), 5, K_EDIT | K_NUMBER);
				cfg.max_batdn = atoi(tmp);
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`Maximum Destination Users in User to User Transfers:`\n"
					"\n"
					"This is the maximum number of users allowed in the destination user list\n"
					"of a user to user file upload.\n"
				;
				uifc.input(WIN_MID, 0, 0
				           , "Maximum Destination Users in User to User Transfers"
				           , ultoa(cfg.max_userxfer, tmp, 10), 5, K_EDIT | K_NUMBER);
				cfg.max_userxfer = atoi(tmp);
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`Default Percentage of Credits to Credit Uploader on Upload:`\n"
					"\n"
					"This is the default value that will be used for this setting (under\n"
					"`Directory Defaults...`) when new File Libraries are created.\n"
				;
				uifc.input(WIN_MID, 0, 0
				           , "Default Percentage of Credits to Credit Uploader on Upload"
				           , ultoa(cfg.cdt_up_pct, tmp, 10), 4, K_EDIT | K_NUMBER);
				cfg.cdt_up_pct = atoi(tmp);
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`Default Percentage of Credits to Credit Uploader on Download:`\n"
					"\n"
					"This is the default value that will be used for this setting (under\n"
					"`Directory Defaults...`) when new File Libraries are created.\n"
				;
				uifc.input(WIN_MID, 0, 0
				           , "Default Percentage of Credits to Credit Uploader on Download"
				           , ultoa(cfg.cdt_dn_pct, tmp, 10), 4, K_EDIT | K_NUMBER);
				cfg.cdt_dn_pct = atoi(tmp);
				break;

			case __COUNTER__:
				uifc.helpbuf =
					"`Leech Protocol Detection Percentage:`\n"
					"\n"
					"This value is the sensitivity of the leech protocol detection feature of\n"
					"Synchronet. If the transfer is apparently unsuccessful, but the transfer\n"
					"time was at least this percentage of the estimated transfer time (based\n"
					"on the estimated CPS of the connection result code), then a leech\n"
					"protocol error is issued and the user's leech download counter is\n"
					"incremented. Setting this value to `0` disables leech protocol detection.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0
				           , "Leech Protocol Detection Percentage (0=Disabled)"
				           , ultoa(cfg.leech_pct, tmp, 10), 3, K_EDIT | K_NUMBER);
				cfg.leech_pct = atoi(tmp);
				if (!cfg.leech_pct)
					break;
				uifc.helpbuf =
					"`Leech Protocol Minimum Time (in Seconds):`\n"
					"\n"
					"This option allows you to adjust the sensitivity of the leech protocol\n"
					"detection feature. This value is the minimum length of transfer time\n"
					"(in seconds) that must elapse before an aborted transfer will be\n"
					"considered a possible leech attempt.\n"
				;
				uifc.input(WIN_MID, 0, 0
				           , "Leech Protocol Minimum Time (in Seconds)"
				           , ultoa(cfg.leech_sec, tmp, 10), 3, K_EDIT | K_NUMBER);
				cfg.leech_sec = atoi(tmp);
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`Maximum Uploaded Filename Length Allowed:`\n"
					"\n"
					"This value is the maximum length of filenames allowed to be uploaded.\n"
					"\n"
					"Only 64 characters of filenames are indexed (searchable) and sometimes\n"
					"(depending on the terminal) only 64 or fewer characters of a filename\n"
					"may be displayed to a user. For these reasons, `64` characters is a\n"
					"reasonable maximum filename length to enforce (and thus, the default).\n"
					"\n"
					"The absolute maximum filename length supported is `65535` characters."
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0
				           , "Maximum Uploaded Filename Length Allowed (in characters)"
				           , ultoa(cfg.filename_maxlen, tmp, 10), 5, K_EDIT | K_NUMBER);
				cfg.filename_maxlen = atoi(tmp);
				break;
			case __COUNTER__:   /* Uploaded Filename characters allowed */
				i = 0;
				snprintf(opt[i++], MAX_OPLN,  "Safest Subset Only (A-Z, a-z, 0-9, -, _, and .)");
				snprintf(opt[i++], MAX_OPLN,  "Most ASCII Characters, Excluding Spaces");
				snprintf(opt[i++], MAX_OPLN,  "Most ASCII Characters, Including Spaces");
				snprintf(opt[i++], MAX_OPLN,  "Most CP437 Characters, Excluding Spaces");
				snprintf(opt[i++], MAX_OPLN,  "Most CP437 Characters, Including Spaces");
				opt[i][0] = '\0';
				if (cfg.file_misc & FM_SAFEST)
					j = 0;
				else {
					j = 1;
					if (cfg.file_misc & FM_EXASCII)
						j = 3;
					if (cfg.file_misc & FM_SPACES)
						j++;
				}
				uifc.helpbuf =
					"`Allowed Characters in Uploaded Filenames:`\n"
					"\n"
					"Here you can control which characters will be allowed in the names of\n"
					"files uploaded by users (assuming you allow file uploads at all).\n"
					"\n"
					"The `Safest` (most compatible) filename characters to allow are:\n"
					"`" SAFEST_FILENAME_CHARS "`\n"
					"\n"
					"`Spaces` may be allowed in filenames, but may cause issues with some file\n"
					"transfer protocol drivers or clients (older/MS-DOS software).\n"
					"\n"
					"Filenames that are most often troublesome (including those in your\n"
					"`text/file.can` file) are `always disallowed`:\n"
					"    Filenames beginning with dash (`-`)\n"
					"    Filenames beginning or ending in space\n"
					"    Filenames beginning or ending in period (`.`)\n"
					"    Filenames containing consecutive periods (`..`)\n"
					"    Filenames containing illegal characters (`" ILLEGAL_FILENAME_CHARS "`)\n"
					"    Filenames containing control characters (ASCII 0-31 and 127)\n"
				;
				i = uifc.list(WIN_RHT, 0, 0, 0, &j, NULL, "Allowed Characters in Uploaded Filenames", opt);
				switch (i) {
					case 0:
						if (cfg.file_misc != FM_SAFEST) {
							cfg.file_misc |= FM_SAFEST;
							cfg.file_misc &= ~(FM_EXASCII | FM_SPACES);
							uifc.changes = TRUE;
						}
						break;
					case 1:
						if (cfg.file_misc) {
							cfg.file_misc &= ~(FM_SAFEST | FM_SPACES | FM_EXASCII);
							uifc.changes = TRUE;
						}
						break;
					case 2:
						if (cfg.file_misc != FM_SPACES) {
							cfg.file_misc &= ~(FM_SAFEST | FM_EXASCII);
							cfg.file_misc |= FM_SPACES;
							uifc.changes = TRUE;
						}
						break;
					case 3:
						if (cfg.file_misc != FM_EXASCII) {
							cfg.file_misc &= ~(FM_SAFEST | FM_SPACES);
							cfg.file_misc |= FM_EXASCII;
							uifc.changes = TRUE;
						}
						break;
					case 4:
						if (cfg.file_misc != (FM_EXASCII | FM_SPACES)) {
							cfg.file_misc &= ~(FM_SAFEST);
							cfg.file_misc |= FM_EXASCII | FM_SPACES;
							uifc.changes = TRUE;
						}
						break;
				}
				break;
			case __COUNTER__:   /* Supported Arhive types */
				uifc.helpbuf =
					"`Supported Archive Formats:`\n"
					"\n"
					"This is a list of archive file types (extensions/suffixes, without the\n"
					"'.') that are supported by libarchive for the creation of QWK/REP\n"
					"packets and temporary archives of files for users to download.\n"
					"\n"
					"By default, the supported archive formats are: `zip, 7z, tgz`.\n"
					"\n"
					"Synchronet uses `libarchive` for internal archive file support\n"
					"(see `http://libarchive.org/` for details).  Depending on the version of\n"
					"libarchive, build options, and OS/system configuration, more archive\n"
					"formats than the default list may be supported.\n"
					"\n"
					"If you want additional formats to be available for your users and you've\n"
					"confirmed system compatibility (e.g. created and extracted archives\n"
					"of the desired types using `archive.js`), you can add the standard file\n"
					"extensions for those additional archive formats to this list.\n"
					"\n"
					"This list has no impact on the types of files allowed to be uploaded\n"
					"into file areas or the ability to view, test, or extract archives of\n"
					"any type.\n"
					;
		        strListCombine(cfg.supported_archive_formats, str, sizeof str, ", ");
				uifc.input(WIN_MID | WIN_SAV, 0 , 0, "Supported Archive Formats", str, sizeof str - 1, K_EDIT);
				strListFree(&cfg.supported_archive_formats);
				cfg.supported_archive_formats = strListSplit(NULL, str, ", ");
				break;
			case __COUNTER__:   /* Viewable file types */
				while (1) {
					for (i = 0; i < cfg.total_fviews && i < MAX_OPTS; i++)
						snprintf(opt[i], MAX_OPLN, "%-*s %s", MAX_FILEEXT_LEN, cfg.fview[i]->ext, cfg.fview[i]->cmd);
					opt[i][0] = 0;
					i = WIN_RHT | WIN_ACT | WIN_SAV;  /* save cause size can change */
					if (cfg.total_fviews < MAX_OPTS)
						i |= WIN_INS | WIN_INSACT | WIN_XTR;
					if (cfg.total_fviews)
						i |= WIN_DEL | WIN_COPY | WIN_CUT;
					if (savfview.cmd[0])
						i |= WIN_PASTE | WIN_PASTEXTR;
					uifc.helpbuf =
						"`Viewable File Types:`\n"
						"\n"
						"This is a list of file types (extensions) that have content information\n"
						"that can be viewed on the Terminal Server through the execution of an\n"
						"external program or script.\n"
						"\n"
						"The file types/extensions are case insensitive and may contain wildcard\n"
						"characters (i.e. `?` or `*`).\n"
						"\n"
						"Specify the filename argument on the command-line with `%s` or `%f`."
					;
					i = uifc.list(i, 0, 0, 50, &fview_dflt, &fview_bar, "Viewable File Types", opt);
					if (i == -1)
						break;
					int msk = i & MSK_ON;
					i &= MSK_OFF;
					if (msk == MSK_DEL || msk == MSK_CUT) {
						if (msk == MSK_CUT)
							savfview = *cfg.fview[i];
						free(cfg.fview[i]);
						cfg.total_fviews--;
						while (i < cfg.total_fviews) {
							cfg.fview[i] = cfg.fview[i + 1];
							i++;
						}
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_INS) {
						if ((cfg.fview = (fview_t **)realloc_or_free(cfg.fview
						                                             , sizeof(fview_t *) * (cfg.total_fviews + 1))) == NULL) {
							errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_fviews + 1);
							cfg.total_fviews = 0;
							bail(1);
							continue;
						}
						fview_t new_fview;
						if (!get_new_fview(&new_fview))
							continue;
						if (!cfg.total_fviews) {
							if ((cfg.fview[0] = (fview_t *)malloc(
									 sizeof(fview_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(fview_t));
								continue;
							}
							*cfg.fview[0] = new_fview;
						}
						else {
							for (j = cfg.total_fviews; j > i; j--)
								cfg.fview[j] = cfg.fview[j - 1];
							if ((cfg.fview[i] = (fview_t *)malloc(
									 sizeof(fview_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(fview_t));
								continue;
							}
							*cfg.fview[i] = new_fview;
						}
						cfg.total_fviews++;
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_COPY) {
						savfview = *cfg.fview[i];
						continue;
					}
					if (msk == MSK_PASTE) {
						if ((cfg.fview = (fview_t**)new_item(cfg.fview, sizeof(fview_t), i, &cfg.total_fviews)) == NULL) {
							errormsg(WHERE, ERR_ALLOC, "fviews", sizeof(fview_t) * (cfg.total_fviews + 1));
							cfg.total_fviews = 0;
							bail(1);
						}
						*cfg.fview[i] = savfview;
						uifc.changes = TRUE;
						continue;
					}
					if (msk != 0)
						continue;
					done = 0;
					while (!done) {
						j = 0;
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "File Extension"
						         , cfg.fview[i]->ext);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Command Line"
						         , cfg.fview[i]->cmd);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", native_opt
						         , cfg.fview[i]->ex_mode & EX_NATIVE ? "Yes" : "No");
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Access Requirements"
						         , cfg.fview[i]->arstr);
						opt[j][0] = 0;
						uifc_winmode_t wmode = WIN_RHT | WIN_BOT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
						if (i > 0)
							wmode |= WIN_LEFTKEY;
						if (i + 1 < cfg.total_fviews)
							wmode |= WIN_RIGHTKEY;
						switch (uifc.list(wmode, 0, 0, 0, &fview_opt, 0
						                  , "Viewable File Type", opt)) {
							case -1:
								done = 1;
								break;
							case -CIO_KEY_LEFT - 2:
								if (i > 0)
									i--;
								break;
							case -CIO_KEY_RIGHT - 2:
								if (i + 1 < cfg.total_fviews)
									i++;
								break;
							case 0:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Viewable File Type Extension"
								           , cfg.fview[i]->ext, sizeof(cfg.fview[i]->ext) - 1, K_EDIT);
								break;
							case 1:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.fview[i]->cmd, sizeof(cfg.fview[i]->cmd) - 1, K_EDIT);
								break;
							case 2:
								toggle_flag(native_opt, &cfg.fview[i]->ex_mode, EX_NATIVE, false, native_help);
								break;
							case 3:
								snprintf(str, sizeof str, "Viewable File Type %s", cfg.fview[i]->ext);
								getar(str, cfg.fview[i]->arstr);
								break;
						}
					}
				}
				break;
			case __COUNTER__:    /* Testable file types */
				while (1) {
					for (i = 0; i < cfg.total_ftests && i < MAX_OPTS; i++)
						snprintf(opt[i], MAX_OPLN, "%-*s %s", MAX_FILEEXT_LEN, cfg.ftest[i]->ext, cfg.ftest[i]->cmd);
					opt[i][0] = 0;
					i = WIN_RHT | WIN_ACT | WIN_SAV;  /* save cause size can change */
					if (cfg.total_ftests < MAX_OPTS)
						i |= WIN_INS | WIN_INSACT | WIN_XTR;
					if (cfg.total_ftests)
						i |= WIN_DEL | WIN_COPY | WIN_CUT;
					if (savftest.cmd[0])
						i |= WIN_PASTE | WIN_PASTEXTR;
					uifc.helpbuf = testable_files_help;
					i = uifc.list(i, 0, 0, 50, &ftest_dflt, &ftest_bar, "Testable File Types", opt);
					if (i == -1)
						break;
					int msk = i & MSK_ON;
					i &= MSK_OFF;
					if (msk == MSK_DEL || msk == MSK_CUT) {
						if (msk == MSK_CUT)
							savftest = *cfg.ftest[i];
						free(cfg.ftest[i]);
						cfg.total_ftests--;
						while (i < cfg.total_ftests) {
							cfg.ftest[i] = cfg.ftest[i + 1];
							i++;
						}
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_INS) {
						if ((cfg.ftest = (ftest_t **)realloc_or_free(cfg.ftest
						                                             , sizeof(ftest_t *) * (cfg.total_ftests + 1))) == NULL) {
							errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_ftests + 1);
							cfg.total_ftests = 0;
							bail(1);
							continue;
						}
						ftest_t new_ftest;
						if (!get_new_ftest(&new_ftest))
							continue;
						if (!cfg.total_ftests) {
							if ((cfg.ftest[0] = (ftest_t *)malloc(
									 sizeof(ftest_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(ftest_t));
								continue;
							}
							*cfg.ftest[0] = new_ftest;
						}
						else {

							for (j = cfg.total_ftests; j > i; j--)
								cfg.ftest[j] = cfg.ftest[j - 1];
							if ((cfg.ftest[i] = (ftest_t *)malloc(
									 sizeof(ftest_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(ftest_t));
								continue;
							}
							*cfg.ftest[i] = new_ftest;
						}
						cfg.total_ftests++;
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_COPY) {
						savftest = *cfg.ftest[i];
						continue;
					}
					if (msk == MSK_PASTE) {
						if ((cfg.ftest = (ftest_t**)new_item(cfg.ftest, sizeof(ftest_t), i, &cfg.total_ftests)) == NULL) {
							errormsg(WHERE, ERR_ALLOC, "ftests", sizeof(ftest_t) * (cfg.total_ftests + 1));
							cfg.total_ftests = 0;
							bail(1);
						}
						*cfg.ftest[i] = savftest;
						uifc.changes = TRUE;
						continue;
					}
					if (msk != 0)
						continue;
					done = 0;
					while (!done) {
						j = 0;
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "File Extension"
						         , cfg.ftest[i]->ext);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Command Line"
						         , cfg.ftest[i]->cmd);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", native_opt
						         , cfg.ftest[i]->ex_mode & EX_NATIVE ? "Yes" : "No");
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Working String"
						         , cfg.ftest[i]->workstr);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Access Requirements"
						         , cfg.ftest[i]->arstr);
						opt[j][0] = 0;
						uifc.helpbuf = testable_files_help;
						uifc_winmode_t wmode = WIN_RHT | WIN_BOT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
						if (i > 0)
							wmode |= WIN_LEFTKEY;
						if (i + 1 < cfg.total_ftests)
							wmode |= WIN_RIGHTKEY;
						switch (uifc.list(wmode, 0, 0, 0, &ftest_opt, 0
						                  , "Testable File Type", opt)) {
							case -1:
								done = 1;
								break;
							case -CIO_KEY_LEFT - 2:
								if (i > 0)
									i--;
								break;
							case -CIO_KEY_RIGHT - 2:
								if (i + 1 < cfg.total_ftests)
									i++;
								break;
							case 0:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Testable File Type Extension"
								           , cfg.ftest[i]->ext, sizeof(cfg.ftest[i]->ext) - 1, K_EDIT);
								break;
							case 1:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.ftest[i]->cmd, sizeof(cfg.ftest[i]->cmd) - 1, K_EDIT);
								break;
							case 2:
								toggle_flag(native_opt, &cfg.ftest[i]->ex_mode, EX_NATIVE, false, native_help);
								break;
							case 3:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Working String"
								           , cfg.ftest[i]->workstr, sizeof(cfg.ftest[i]->workstr) - 1, K_EDIT | K_MSG);
								break;
							case 4:
								snprintf(str, sizeof str, "Testable File Type %s", cfg.ftest[i]->ext);
								getar(str, cfg.ftest[i]->arstr);
								break;
						}
					}
				}
				break;
			case __COUNTER__:    /* Download Events */
				while (1) {
					for (i = 0; i < cfg.total_dlevents && i < MAX_OPTS; i++)
						snprintf(opt[i], MAX_OPLN, "%-*s %s", MAX_FILEEXT_LEN, cfg.dlevent[i]->ext, cfg.dlevent[i]->cmd);
					opt[i][0] = 0;
					i = WIN_RHT | WIN_ACT | WIN_SAV;  /* save cause size can change */
					if (cfg.total_dlevents < MAX_OPTS)
						i |= WIN_INS | WIN_INSACT | WIN_XTR;
					if (cfg.total_dlevents)
						i |= WIN_DEL | WIN_COPY | WIN_CUT;
					if (savdlevent.cmd[0])
						i |= WIN_PASTE | WIN_PASTEXTR;
					uifc.helpbuf =
						"`Download Events:`\n"
						"\n"
						"This is a list of file types that will have a command line executed to\n"
						"perform an event upon their download (e.g. trigger a download event).\n"
						"\n"
						"The file types are specified by `extension` and if one file extension\n"
						"is listed more than once, each command line will be executed. The\n"
						"command lines must return a error level of 0 (no error) in order\n"
						"for the file to pass the test. This test or event, can do more than\n"
						"just test the file, it can perform any function that the sysop wishes.\n"
						"Such as adding comments to an archived file, or extracting an archive\n"
						"and performing a virus scan. While the external program is executing,\n"
						"a text string is displayed to the user. This `working string` can be set\n"
						"for each file type and command line listed.\n"
					;
					i = uifc.list(i, 0, 0, 50, &dlevent_dflt, &dlevent_bar, "Download Events", opt);
					if (i == -1)
						break;
					int msk = i & MSK_ON;
					i &= MSK_OFF;
					if (msk == MSK_DEL || msk == MSK_CUT) {
						if (msk == MSK_CUT)
							savdlevent = *cfg.dlevent[i];
						free(cfg.dlevent[i]);
						cfg.total_dlevents--;
						while (i < cfg.total_dlevents) {
							cfg.dlevent[i] = cfg.dlevent[i + 1];
							i++;
						}
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_INS) {
						if ((cfg.dlevent = (dlevent_t **)realloc_or_free(cfg.dlevent
						                                                 , sizeof(dlevent_t *) * (cfg.total_dlevents + 1))) == NULL) {
							errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_dlevents + 1);
							cfg.total_dlevents = 0;
							bail(1);
							continue;
						}
						dlevent_t new_dlevent;
						if (!get_new_dlevent(&new_dlevent))
							continue;
						if (!cfg.total_dlevents) {
							if ((cfg.dlevent[0] = (dlevent_t *)malloc(
									 sizeof(dlevent_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(dlevent_t));
								continue;
							}
							*cfg.dlevent[0] = new_dlevent;
						}
						else {

							for (j = cfg.total_dlevents; j > i; j--)
								cfg.dlevent[j] = cfg.dlevent[j - 1];
							if ((cfg.dlevent[i] = (dlevent_t *)malloc(
									 sizeof(dlevent_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(dlevent_t));
								continue;
							}
							*cfg.dlevent[i] = new_dlevent;
						}
						cfg.total_dlevents++;
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_COPY) {
						savdlevent = *cfg.dlevent[i];
						continue;
					}
					if (msk == MSK_PASTE) {
						if ((cfg.dlevent = (dlevent_t**)new_item(cfg.dlevent, sizeof(dlevent_t), i, &cfg.total_dlevents)) == NULL) {
							errormsg(WHERE, ERR_ALLOC, "dlevents", sizeof(dlevent_t) * (cfg.total_dlevents + 1));
							cfg.total_dlevents = 0;
							bail(1);
						}
						*cfg.dlevent[i] = savdlevent;
						uifc.changes = TRUE;
						continue;
					}
					if (msk != 0)
						continue;
					done = 0;
					while (!done) {
						j = 0;
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "File Extension"
						         , cfg.dlevent[i]->ext);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Command Line"
						         , cfg.dlevent[i]->cmd);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", native_opt
						         , cfg.dlevent[i]->ex_mode & EX_NATIVE ? "Yes" : "No");
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Working String"
						         , cfg.dlevent[i]->workstr);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Access Requirements"
						         , cfg.dlevent[i]->arstr);
						opt[j][0] = 0;
						uifc_winmode_t wmode = WIN_RHT | WIN_BOT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
						if (i > 0)
							wmode |= WIN_LEFTKEY;
						if (i + 1 < cfg.total_dlevents)
							wmode |= WIN_RIGHTKEY;
						switch (uifc.list(wmode, 0, 0, 0, &dlevent_opt, 0
						                  , "Download Event", opt)) {
							case -1:
								done = 1;
								break;
							case -CIO_KEY_LEFT - 2:
								if (i > 0)
									i--;
								break;
							case -CIO_KEY_RIGHT - 2:
								if (i + 1 < cfg.total_dlevents)
									i++;
								break;
							case 0:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Download Event Extension"
								           , cfg.dlevent[i]->ext, sizeof(cfg.dlevent[i]->ext) - 1, K_EDIT);
								break;
							case 1:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.dlevent[i]->cmd, sizeof(cfg.dlevent[i]->cmd) - 1, K_EDIT);
								break;
							case 2:
								toggle_flag(native_opt, &cfg.dlevent[i]->ex_mode, EX_NATIVE, false, native_help);
								break;
							case 3:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Working String"
								           , cfg.dlevent[i]->workstr, sizeof(cfg.dlevent[i]->workstr) - 1, K_EDIT | K_MSG);
								break;
							case 4:
								snprintf(str, sizeof str, "Download Event %s", cfg.dlevent[i]->ext);
								getar(str, cfg.dlevent[i]->arstr);
								break;
						}
					}
				}
				break;
			case __COUNTER__:    /* Extractable file types */
				while (1) {
					for (i = 0; i < cfg.total_fextrs && i < MAX_OPTS; i++)
						snprintf(opt[i], MAX_OPLN, "%-*s %s"
						         , MAX_FILEEXT_LEN, cfg.fextr[i]->ext, cfg.fextr[i]->cmd);
					opt[i][0] = 0;
					i = WIN_RHT | WIN_ACT | WIN_SAV;  /* save cause size can change */
					if (cfg.total_fextrs < MAX_OPTS)
						i |= WIN_INS | WIN_INSACT | WIN_XTR;
					if (cfg.total_fextrs)
						i |= WIN_DEL | WIN_COPY | WIN_CUT;
					if (savfextr.cmd[0])
						i |= WIN_PASTE | WIN_PASTEXTR;
					uifc.helpbuf =
						"`Extractable File Types:`\n"
						"\n"
						"List of external extraction methods available by file type/extension.\n"
						"\n"
						"Support for extracting archives of common formats (i.e. `zip`, etc.)\n"
						"is built-into Synchronet (requires no external program), however may be\n"
						"extended via external archive/extraction programs configured here."
					;
					i = uifc.list(i, 0, 0, 50, &fextr_dflt, &fextr_bar, "Extractable File Types", opt);
					if (i == -1)
						break;
					int msk = i & MSK_ON;
					i &= MSK_OFF;
					if (msk == MSK_DEL || msk == MSK_CUT) {
						if (msk == MSK_CUT)
							savfextr = *cfg.fextr[i];
						free(cfg.fextr[i]);
						cfg.total_fextrs--;
						while (i < cfg.total_fextrs) {
							cfg.fextr[i] = cfg.fextr[i + 1];
							i++;
						}
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_INS) {
						if ((cfg.fextr = (fextr_t **)realloc_or_free(cfg.fextr
						                                             , sizeof(fextr_t *) * (cfg.total_fextrs + 1))) == NULL) {
							errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_fextrs + 1);
							cfg.total_fextrs = 0;
							bail(1);
							continue;
						}
						fextr_t new_fextr;
						if (!get_new_fextr(&new_fextr))
							continue;
						if (!cfg.total_fextrs) {
							if ((cfg.fextr[0] = (fextr_t *)malloc(
									 sizeof(fextr_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(fextr_t));
								continue;
							}
							*cfg.fextr[0] = new_fextr;
						}
						else {

							for (j = cfg.total_fextrs; j > i; j--)
								cfg.fextr[j] = cfg.fextr[j - 1];
							if ((cfg.fextr[i] = (fextr_t *)malloc(
									 sizeof(fextr_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(fextr_t));
								continue;
							}
							*cfg.fextr[i] = new_fextr;
						}
						cfg.total_fextrs++;
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_COPY) {
						savfextr = *cfg.fextr[i];
						continue;
					}
					if (msk == MSK_PASTE) {
						if ((cfg.fextr = (fextr_t**)new_item(cfg.fextr, sizeof(fextr_t), i, &cfg.total_fextrs)) == NULL) {
							errormsg(WHERE, ERR_ALLOC, "fextrs", sizeof(fextr_t) * (cfg.total_fextrs + 1));
							cfg.total_fextrs = 0;
							bail(1);
						}
						*cfg.fextr[i] = savfextr;
						uifc.changes = TRUE;
						continue;
					}
					if (msk != 0)
						continue;
					done = 0;
					while (!done) {
						j = 0;
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "File Extension"
						         , cfg.fextr[i]->ext);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Command Line"
						         , cfg.fextr[i]->cmd);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", native_opt
						         , cfg.fextr[i]->ex_mode & EX_NATIVE ? "Yes" : "No");
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Access Requirements"
						         , cfg.fextr[i]->arstr);
						opt[j][0] = 0;
						uifc_winmode_t wmode = WIN_RHT | WIN_BOT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
						if (i > 0)
							wmode |= WIN_LEFTKEY;
						if (i + 1 < cfg.total_fextrs)
							wmode |= WIN_RIGHTKEY;
						switch (uifc.list(wmode, 0, 0, 0, &fextr_opt, 0
						                  , "Extractable File Type", opt)) {
							case -1:
								done = 1;
								break;
							case -CIO_KEY_LEFT - 2:
								if (i > 0)
									i--;
								break;
							case -CIO_KEY_RIGHT - 2:
								if (i + 1 < cfg.total_fextrs)
									i++;
								break;
							case 0:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Extractable File Type Extension"
								           , cfg.fextr[i]->ext, sizeof(cfg.fextr[i]->ext) - 1, K_EDIT);
								break;
							case 1:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.fextr[i]->cmd, sizeof(cfg.fextr[i]->cmd) - 1, K_EDIT);
								break;
							case 2:
								toggle_flag(native_opt, &cfg.fextr[i]->ex_mode, EX_NATIVE, false, native_help);
								break;
							case 3:
								snprintf(str, sizeof str, "Extractable File Type %s", cfg.fextr[i]->ext);
								getar(str, cfg.fextr[i]->arstr);
								break;
						}
					}
				}
				break;
			case __COUNTER__:    /* Compressible file types */
				while (1) {
					for (i = 0; i < cfg.total_fcomps && i < MAX_OPTS; i++)
						snprintf(opt[i], MAX_OPLN, "%-*s %s", MAX_FILEEXT_LEN, cfg.fcomp[i]->ext, cfg.fcomp[i]->cmd);
					opt[i][0] = 0;
					i = WIN_RHT | WIN_ACT | WIN_SAV;  /* save cause size can change */
					if (cfg.total_fcomps < MAX_OPTS)
						i |= WIN_INS | WIN_INSACT | WIN_XTR;
					if (cfg.total_fcomps)
						i |= WIN_DEL | WIN_COPY | WIN_CUT;
					if (savfcomp.cmd[0])
						i |= WIN_PASTE | WIN_PASTEXTR;
					uifc.helpbuf =
						"`Compressible File Types:`\n"
						"\n"
						"List of external compression methods available by file type/extension.\n"
						"\n"
						"Support for creating archives of common formats (i.e. `zip`, `7z`, `tgz`)\n"
						"is built-into Synchronet (requires no external program), however may be\n"
						"extended via external compression/archive programs configured here."
					;
					i = uifc.list(i, 0, 0, 50, &fcomp_dflt, &fcomp_bar, "Compressible File Types", opt);
					if (i == -1)
						break;
					int msk = i & MSK_ON;
					i &= MSK_OFF;
					if (msk == MSK_DEL || msk == MSK_CUT) {
						if (msk == MSK_CUT)
							savfcomp = *cfg.fcomp[i];
						free(cfg.fcomp[i]);
						cfg.total_fcomps--;
						while (i < cfg.total_fcomps) {
							cfg.fcomp[i] = cfg.fcomp[i + 1];
							i++;
						}
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_INS) {
						if ((cfg.fcomp = (fcomp_t **)realloc_or_free(cfg.fcomp
						                                             , sizeof(fcomp_t *) * (cfg.total_fcomps + 1))) == NULL) {
							errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_fcomps + 1);
							cfg.total_fcomps = 0;
							bail(1);
							continue;
						}
						fcomp_t new_fcomp;
						if (!get_new_fcomp(&new_fcomp))
							continue;
						if (!cfg.total_fcomps) {
							if ((cfg.fcomp[0] = (fcomp_t *)malloc(
									 sizeof(fcomp_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(fcomp_t));
								continue;
							}
							*cfg.fcomp[0] = new_fcomp;
						}
						else {
							for (j = cfg.total_fcomps; j > i; j--)
								cfg.fcomp[j] = cfg.fcomp[j - 1];
							if ((cfg.fcomp[i] = (fcomp_t *)malloc(
									 sizeof(fcomp_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(fcomp_t));
								continue;
							}
							*cfg.fcomp[i] = new_fcomp;
						}
						cfg.total_fcomps++;
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_COPY) {
						savfcomp = *cfg.fcomp[i];
						continue;
					}
					if (msk == MSK_PASTE) {
						if ((cfg.fcomp = (fcomp_t**)new_item(cfg.fcomp, sizeof(fcomp_t), i, &cfg.total_fcomps)) == NULL) {
							errormsg(WHERE, ERR_ALLOC, "fcomps", sizeof(fcomp_t) * (cfg.total_fcomps + 1));
							cfg.total_fcomps = 0;
							bail(1);
						}
						*cfg.fcomp[i] = savfcomp;
						uifc.changes = TRUE;
						continue;
					}
					if (msk != 0)
						continue;
					done = 0;
					while (!done) {
						j = 0;
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "File Extension"
						         , cfg.fcomp[i]->ext);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Command Line"
						         , cfg.fcomp[i]->cmd);
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", native_opt
						         , cfg.fcomp[i]->ex_mode & EX_NATIVE ? "Yes" : "No");
						snprintf(opt[j++], MAX_OPLN, "%-22s%s", "Access Requirements"
						         , cfg.fcomp[i]->arstr);
						opt[j][0] = 0;
						uifc_winmode_t wmode = WIN_RHT | WIN_BOT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
						if (i > 0)
							wmode |= WIN_LEFTKEY;
						if (i + 1 < cfg.total_fcomps)
							wmode |= WIN_RIGHTKEY;
						switch (uifc.list(wmode, 0, 0, 0, &fcomp_opt, 0
						                  , "Compressible File Type", opt)) {
							case -1:
								done = 1;
								break;
							case -CIO_KEY_LEFT - 2:
								if (i > 0)
									i--;
								break;
							case -CIO_KEY_RIGHT - 2:
								if (i + 1 < cfg.total_fcomps)
									i++;
								break;
							case 0:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Compressible File Type Extension"
								           , cfg.fcomp[i]->ext, sizeof(cfg.fcomp[i]->ext) - 1, K_EDIT);
								break;
							case 1:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.fcomp[i]->cmd, sizeof(cfg.fcomp[i]->cmd) - 1, K_EDIT);
								break;
							case 2:
								toggle_flag(native_opt, &cfg.fcomp[i]->ex_mode, EX_NATIVE, false, native_help);
								break;
							case 3:
								snprintf(str, sizeof str, "Compressible File Type %s", cfg.fcomp[i]->ext);
								getar(str, cfg.fcomp[i]->arstr);
								break;
						}
					}
				}
				break;
			case __COUNTER__:   /* Transfer protocols */
				while (1) {
					for (i = 0; i < cfg.total_prots && i < MAX_OPTS; i++)
						snprintf(opt[i], MAX_OPLN, "%c  %s"
						         , cfg.prot[i]->mnemonic, cfg.prot[i]->name);
					opt[i][0] = 0;
					uifc_winmode_t wmode = WIN_RHT | WIN_ACT | WIN_SAV; /* WIN_SAV because size can change */
					if (cfg.total_prots < MAX_OPTS)
						wmode |= WIN_INS | WIN_INSACT | WIN_XTR;
					if (cfg.total_prots)
						wmode |= WIN_DEL | WIN_COPY | WIN_CUT;
					if (savprot.mnemonic)
						wmode |= WIN_PASTE | WIN_PASTEXTR;
					uifc.helpbuf =
						"`File Transfer Protocols:`\n"
						"\n"
						"This is a list of file transfer protocols that can be used to transfer\n"
						"files either to or from a remote user. For each protocol, you can\n"
						"specify the mnemonic (hot-key) to use to specify that protocol, the\n"
						"command line to use for uploads, downloads, batch uploads, batch\n"
						"downloads, support of DSZLOG, and (for *nix only) if it uses socket\n"
						"I/O or the more common stdio.\n"
						"\n"
						"If the protocol doesn't support a certain method of transfer, or you\n"
						"don't wish it to be available for a certain method of transfer, leave\n"
						"the command line for that method blank.\n"
					;
					i = uifc.list(wmode, 0, 0, 34, &prot_dflt, &prot_bar, "File Transfer Protocols", opt);
					if (i == -1)
						break;
					int msk = i & MSK_ON;
					i &= MSK_OFF;
					if (msk == MSK_DEL || msk == MSK_CUT) {
						if (msk == MSK_CUT)
							savprot = *cfg.prot[i];
						free(cfg.prot[i]);
						cfg.total_prots--;
						while (i < cfg.total_prots) {
							cfg.prot[i] = cfg.prot[i + 1];
							i++;
						}
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_INS) {
						if ((cfg.prot = (prot_t **)realloc(cfg.prot
						                                   , sizeof(prot_t *) * (cfg.total_prots + 1))) == NULL) {
							errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_prots + 1);
							cfg.total_prots = 0;
							bail(1);
							continue;
						}
						prot_t new_prot;
						if (!get_new_prot(&new_prot))
							continue;
						if (!cfg.total_prots) {
							if ((cfg.prot[0] = (prot_t *)malloc(
									 sizeof(prot_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(prot_t));
								continue;
							}
							*cfg.prot[0] = new_prot;
						} else {
							for (j = cfg.total_prots; j > i; j--)
								cfg.prot[j] = cfg.prot[j - 1];
							if ((cfg.prot[i] = (prot_t *)malloc(
									 sizeof(prot_t))) == NULL) {
								errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(prot_t));
								continue;
							}
							*cfg.prot[i] = new_prot;
						}
						cfg.total_prots++;
						uifc.changes = TRUE;
						continue;
					}
					if (msk == MSK_COPY) {
						savprot = *cfg.prot[i];
						continue;
					}
					if (msk == MSK_PASTE) {
						if ((cfg.prot = (prot_t**)new_item(cfg.prot, sizeof(prot_t), i, &cfg.total_prots)) == NULL) {
							errormsg(WHERE, ERR_ALLOC, "prots", sizeof(prot_t) * (cfg.total_prots + 1));
							cfg.total_prots = 0;
							bail(1);
						}
						*cfg.prot[i] = savprot;
						uifc.changes = TRUE;
						continue;
					}
					if (msk != 0)
						continue;
					done = 0;
					while (!done) {
						j = 0;
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%c", "Mnemonic (Command Key)"
						         , cfg.prot[i]->mnemonic);
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s", "Protocol Name"
						         , cfg.prot[i]->name);
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s", "Access Requirements"
						         , cfg.prot[i]->arstr);
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s", "Upload Command Line"
						         , cfg.prot[i]->ulcmd);
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s", "Download Command Line"
						         , cfg.prot[i]->dlcmd);
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s", "Batch Upload Command Line"
						         , cfg.prot[i]->batulcmd);
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s", "Batch Download Command Line"
						         , cfg.prot[i]->batdlcmd);
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s",   native_opt
						         , cfg.prot[i]->misc & PROT_NATIVE ? "Yes" : "No");
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s",   "Supports DSZLOG"
						         , cfg.prot[i]->misc & PROT_DSZLOG ? "Yes":"No");
						snprintf(opt[j++], MAX_OPLN, "%-30.30s%s",   "Socket I/O"
						         , cfg.prot[i]->misc & PROT_SOCKET ? "Yes":"No");
						opt[j][0] = 0;
						wmode = WIN_RHT | WIN_BOT | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
						if (i > 0)
							wmode |= WIN_LEFTKEY;
						if (i + 1 < cfg.total_prots)
							wmode |= WIN_RIGHTKEY;
						switch (uifc.list(wmode, 0, 0, 70, &prot_opt, 0
						                  , "File Transfer Protocol", opt)) {
							case -1:
								done = 1;
								break;
							case -CIO_KEY_LEFT - 2:
								if (i > 0)
									i--;
								break;
							case -CIO_KEY_RIGHT - 2:
								if (i + 1 < cfg.total_prots)
									i++;
								break;
							case 0:
								str[0] = cfg.prot[i]->mnemonic;
								str[1] = 0;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Mnemonic (Command Key)"
								           , str, 1, K_UPPER | K_EDIT);
								if (str[0])
									cfg.prot[i]->mnemonic = str[0];
								break;
							case 1:
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Protocol Name"
								           , cfg.prot[i]->name, sizeof(cfg.prot[i]->name) - 1, K_EDIT);
								break;
							case 2:
								snprintf(str, sizeof str, "Protocol %s", cfg.prot[i]->name);
								getar(str, cfg.prot[i]->arstr);
								break;
							case 3:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.prot[i]->ulcmd, sizeof(cfg.prot[i]->ulcmd) - 1, K_EDIT);
								break;
							case 4:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.prot[i]->dlcmd, sizeof(cfg.prot[i]->dlcmd) - 1, K_EDIT);
								break;
							case 5:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.prot[i]->batulcmd, sizeof(cfg.prot[i]->batulcmd) - 1, K_EDIT);
								break;
							case 6:
								uifc.helpbuf = SCFG_CMDLINE_PREFIX_HELP SCFG_CMDLINE_SPEC_HELP;
								uifc.input(WIN_MID | WIN_SAV, 0, 0
								           , "Command"
								           , cfg.prot[i]->batdlcmd, sizeof(cfg.prot[i]->batdlcmd) - 1, K_EDIT);
								break;
							case 7:
								toggle_flag(native_opt, &cfg.prot[i]->misc, PROT_NATIVE, false, native_help);
								break;
							case 8:
								l = cfg.prot[i]->misc & PROT_DSZLOG ? 0:1;
								l = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &l, 0
								              , "Uses DSZLOG", uifcYesNoOpts);
								if ((l == 0 && !(cfg.prot[i]->misc & PROT_DSZLOG))
								    || (l == 1 && cfg.prot[i]->misc & PROT_DSZLOG)) {
									cfg.prot[i]->misc ^= PROT_DSZLOG;
									uifc.changes = TRUE;
								}
								break;
							case 9:
								l = cfg.prot[i]->misc & PROT_SOCKET ? 0:1l;
								l = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &l, 0
								              , "Uses Socket I/O", uifcYesNoOpts);
								if ((l == 0 && !(cfg.prot[i]->misc & PROT_SOCKET))
								    || (l == 1 && cfg.prot[i]->misc & PROT_SOCKET)) {
									cfg.prot[i]->misc ^= PROT_SOCKET;
									uifc.changes = TRUE;
								}
								break;
						}
					}
				}
				break;
		}
	}
}
