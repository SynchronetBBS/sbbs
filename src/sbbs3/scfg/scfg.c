/* Synchronet configuration utility 										*/

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

#define __COLORS
#include "scfg.h"
#undef BLINK
#include "ciolib.h"
#include "git_hash.h"
#include "git_branch.h"
#include "cryptlib.h"
#include "xpdatetime.h"
#include "sbbs_ini.h"
#include "ciolib.h"

/********************/
/* Global Variables */
/********************/

scfg_t          cfg; /* Synchronet Configuration */
uifcapi_t       uifc; /* User Interface (UIFC) Library API */

bool            forcesave = false;
bool            new_install = false;
bool            run_wizard = false;
static bool     auto_save = false;
extern bool     all_msghdr;
extern bool     no_msghdr;
char **         opt;
char            tmp[256];
char            error[256];
char*           area_sort_desc[] = { "Index Position", "Long Name", "Short Name", "Internal Code Suffix", NULL };
static char     title[128];
size_t          title_len;
const char*     hostname = NULL;
int             ciolib_mode = CIOLIB_MODE_AUTO;
enum text_modes video_mode = LCD80X25;

/* convenient space-saving global constants */
const char*     nulstr = "";

char *          invalid_code =
	"`Invalid Internal Code:`\n\n"
	"Internal codes can be up to eight characters in length and can only\n"
	"contain valid DOS filename characters. The code you have entered\n"
	"contains one or more invalid characters.";

void allocfail(uint size)
{
	printf("\7Error allocating %u bytes of memory.\n", size);
	bail(1);
}

enum import_list_type determine_msg_list_type(const char* path)
{
	const char* fname = getfname(path);

	if (stricmp(fname, "areas.bbs") == 0)
		return IMPORT_LIST_TYPE_SBBSECHO_AREAS_BBS;
	if (stricmp(fname, "control.dat") == 0)
		return IMPORT_LIST_TYPE_QWK_CONTROL_DAT;
	if (stricmp(fname, "newsgroup.lst") == 0)
		return IMPORT_LIST_TYPE_NEWSGROUPS;
	if (stricmp(fname, "echostats.ini") == 0)
		return IMPORT_LIST_TYPE_ECHOSTATS;
	return IMPORT_LIST_TYPE_BACKBONE_NA;
}

uint group_num_from_name(const char* name)
{
	int u;

	for (u = 0; u < cfg.total_grps; u++)
		if (stricmp(cfg.grp[u]->sname, name) == 0)
			return u;

	return u;
}

static int sort_group = 0;

int sub_compare(const void* c1, const void* c2)
{
	sub_t* sub1 = *(sub_t**)c1;
	sub_t* sub2 = *(sub_t**)c2;

	if (sub1->grp != sort_group && sub2->grp != sort_group)
		return 0;

	if (sub1->grp != sort_group || sub2->grp != sort_group)
		return sub1->grp - sub2->grp;
	if (cfg.grp[sort_group]->sort == AREA_SORT_LNAME)
		return stricmp(sub1->lname, sub2->lname);
	if (cfg.grp[sort_group]->sort == AREA_SORT_SNAME)
		return stricmp(sub1->sname, sub2->sname);
	if (cfg.grp[sort_group]->sort == AREA_SORT_CODE)
		return stricmp(sub1->code_suffix, sub2->code_suffix);
	return sub1->subnum - sub2->subnum;
}

void sort_subs(int grpnum)
{
	sort_group = grpnum;
	qsort(cfg.sub, cfg.total_subs, sizeof(sub_t*), sub_compare);

	// Re-number the sub-boards after sorting:
	for (int i = 0; i < cfg.total_subs; ++i) {
		if (cfg.sub[i]->grp != grpnum)
			continue;
		cfg.sub[i]->subnum = i;
	}
}

static int sort_lib = 0;

int dir_compare(const void* c1, const void* c2)
{
	dir_t* dir1 = *(dir_t**)c1;
	dir_t* dir2 = *(dir_t**)c2;

	if (dir1->lib != sort_lib && dir2->lib != sort_lib)
		return 0;

	if (dir1->lib != sort_lib || dir2->lib != sort_lib)
		return dir1->lib - dir2->lib;
	if (cfg.lib[sort_lib]->sort == AREA_SORT_LNAME)
		return stricmp(dir1->lname, dir2->lname);
	if (cfg.lib[sort_lib]->sort == AREA_SORT_SNAME)
		return stricmp(dir1->sname, dir2->sname);
	if (cfg.lib[sort_lib]->sort == AREA_SORT_CODE)
		return stricmp(dir1->code_suffix, dir2->code_suffix);
	return dir1->dirnum - dir2->dirnum;
}

void sort_dirs(int libnum)
{
	sort_lib = libnum;
	qsort(cfg.dir, cfg.total_dirs, sizeof(dir_t*), dir_compare);

	// Re-number the directories after sorting:
	for (int i = 0; i < cfg.total_dirs; ++i) {
		if (cfg.dir[i]->lib != libnum)
			continue;
		cfg.dir[i]->dirnum = i;
	}
}

void toggle_flag(const char* title, uint* misc, uint flag, bool invert, const char* help)
{
	int k = ((*misc) & flag) == invert;
	if (help != NULL)
		uifc.helpbuf = (char*)help;
	k = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &k, 0, title, uifcYesNoOpts);
	if ((k == invert) && ((*misc) & flag) == 0) {
		*misc |= flag;
		uifc.changes = TRUE;
	}
	else if ((k == !invert) && ((*misc) & flag) != 0) {
		*misc &= ~flag;
		uifc.changes = TRUE;
	}
}

void wizard_msg(int page, int total, const char* text)
{
	uifc.showbuf(WIN_HLP | WIN_DYN | WIN_L2R, 2, 2, 76, 20, "Setup Wizard", text, NULL, NULL);
	if (page > 0 && page < total) {
		int x = (uifc.scrn_width / 2) + 21;
		int y = 21;
		uifc.printf(x, y, uifc.cclr | (uifc.bclr << 4),  "%*.*s", total, total
		            , "\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1\xb1");
		uifc.printf(x, y, uifc.lclr | (uifc.bclr << 4), "%*.*s", page, page
		            , "\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2\xb2");
	}
}

void cfg_wizard(void)
{
	char errormsg[MAX_PATH * 2];

	if (!load_main_cfg(&cfg, error, sizeof(error))) {
		SAFEPRINTF(errormsg, "ERROR: %s", error);
		uifc.msg(errormsg);
		return;
	}
	if (!load_msgs_cfg(&cfg, error, sizeof(error))) {
		SAFEPRINTF(errormsg, "ERROR: %s", error);
		uifc.msg(errormsg);
		return;
	}

	int    stage = 0;
	int    total = 17;
	scfg_t saved_cfg = cfg;
	do {
		switch (stage) {
			case -1:
			{
				char* opt[] = { "Abort", "Continue", NULL };
				wizard_msg(stage, total, "Do you wish to abort the Setup Wizard now?");
				if (uifc.list(WIN_SAV | WIN_L2R | WIN_NOBRDR, 0, 10, 0, NULL, NULL
				              , "Abort Setup Wizard", opt) == 0)
					stage = 100;
				break;
			}
			case __COUNTER__:
			{
				char* opt[] = { "Continue", NULL };

				wizard_msg(stage, total,
				           "                             ~    Welcome   ~\n"
				           "  _________                   .__                                __   \n"
				           " /   _____/__.__. ____   ____ |  |_________  ____   ____   _____/  |_ \n"
				           " \\_____  <   |  |/    \\_/ ___\\|  |  \\_  __ \\/  _ \\ /    \\_/ __ \\   __\\\n"
				           " /        \\___  |   |  \\  \\___|   Y  \\  | \\(  <_> )   |  \\  ___/|  |  \n"
				           "/_______  / ____|___|  /\\___  >___|  /__|   \\____/|___|  /\\___  >__| \n"
				           "        \\/\\/         \\/     \\/     \\/                  \\/     \\/     \n"
				           "\n"
				           "This wizard will take you through the configuration of the basic\n"
				           "parameters required to run a Synchronet Bulletin Board System.  All of\n"
				           "these configuration parameters may be changed later if you choose.\n"
				           "\n"
				           "Press ~ ENTER ~ to advance through the setup wizard or ~ ESC ~ to move\n"
				           "backward or abort the wizard."
				           );
				if (uifc.list(WIN_SAV | WIN_L2R | WIN_NOBRDR, 0, 18, 0, NULL, NULL, NULL, opt) == -1) {
					--stage;
					continue;
				}
				break;
			}
			case __COUNTER__:
				if (edit_sys_name(stage, total) < 1) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_operator(stage, total) < 1) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_password(stage, total) < 1) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_inetaddr(stage, total) < 1) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_id(stage, total) < 0) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_location(stage, total) < 1) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_timezone(stage, total) < 0) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_timefmt(stage, total) < 0) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_datefmt(stage, total) < 0) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_date_verbal(stage, total) < 0) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (edit_sys_newuser_policy(stage, total) < 0) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (!(cfg.sys_misc & SM_CLOSED)) {
					if (edit_sys_newuser_fback_policy(stage, total) < 0) {
						--stage;
						continue;
					}
				}
				break;
			case __COUNTER__:
				if (!(cfg.sys_misc & SM_CLOSED)) {
					if (edit_sys_alias_policy(stage, total) < 0) {
						--stage;
						continue;
					}
				}
				break;
			case __COUNTER__:
				if (edit_sys_delmsg_policy(stage, total) < 0) {
					--stage;
					continue;
				}
				break;
			case __COUNTER__:
				if (memcmp(&saved_cfg, &cfg, sizeof(cfg)) == 0) {
					uifc.scrn(title);
					uifc.msg("No configuration changes made");
					stage = -1;
					continue;
				}
				break;
			case __COUNTER__:
				wizard_msg(stage, total,
				           "`System Password Verification`\n\n"
				           "At this point you must re-enter the system password that you set earlier\n"
				           "in the configuration wizard.\n"
				           "\n"
				           "This same password will be required of you when you login to the BBS\n"
				           "with any user account that has System Operator (sysop) privileges\n"
				           "(i.e. security level 90 or higher).\n"
				           );
				char pass[sizeof(cfg.sys_pass)];
				do {
					if (uifc.input(WIN_L2R | WIN_SAV | WIN_NOBRDR, 0, 14, "SY", pass, sizeof(cfg.sys_pass) - 1, K_PASSWORD | K_UPPER) < 0)
						break;
				} while (strcmp(cfg.sys_pass, pass) != 0);
				if (strcmp(cfg.sys_pass, pass)) {
					stage = -1;
					continue;
				}
				break;
			case __COUNTER__:
				wizard_msg(stage, total,
				           "                        ~ Initial Setup Complete! ~\n"
				           "\n"
				           "You have completed the initial configuration of the basic parameters\n"
				           "required to run Synchronet - the ultimate choice in BBS software for the\n"
				           "Internet Age.\n"
				           "\n"
				           "Thank you for choosing Synchronet,\n"
				           "\n"
				           "                                               digital man (rob)\n"
				           );
				char* opts[] = { "Save Changes", "Discard Changes", NULL };
				int   sc = uifc.list(WIN_SAV | WIN_L2R | WIN_NOBRDR | WIN_ATEXIT, 0, 14, 0, NULL, NULL, NULL, opts);
				if (sc != 0) {
					if (sc == -1)
						uifc.exit_flags &= ~UIFC_XF_QUIT;
					stage = -1;
					continue;
				}
				if (strcmp(saved_cfg.sys_pass, cfg.sys_pass) != 0)
					reencrypt_keys(saved_cfg.sys_pass, cfg.sys_pass);
				cfg.new_install = new_install;
				save_main_cfg(&cfg);
				save_msgs_cfg(&cfg);
				break;
		}
		++stage;
	} while (stage < __COUNTER__);

	free_main_cfg(&cfg);
	free_msgs_cfg(&cfg);
}

void display_filename(BOOL force)
{
	static char last[MAX_PATH + 1];
	const char* fname = cfg.filename;
	if (strlen(fname) + title_len + 5 > uifc.scrn_width)
		fname = getfname(fname);
	if (force || strcmp(last, fname) != 0)
		uifc.printf(title_len + 4, 1, uifc.bclr | (uifc.cclr << 4), "%*s", uifc.scrn_width - (title_len + 5), fname);
	SAFECOPY(last, fname);
}

void set_cfg_filename(const char* hostname)
{
	if (hostname == NULL)
		sbbs_get_ini_fname(cfg.filename, cfg.ctrl_dir);
	else
		snprintf(cfg.filename, sizeof cfg.filename, "%ssbbs%s%s.ini", cfg.ctrl_dir, *hostname ? ".":"", hostname);
}

#ifdef _WIN32
	#define printf cprintf
#endif

void banner()
{
	char compiler[32];

	DESCRIBE_COMPILER(compiler);

	printf("\nSynchronet Configuration Utility (%s)  v%s%c  " COPYRIGHT_NOTICE
	       "\n", PLATFORM_DESC, VERSION, REVISION);
	printf("\nCompiled %s/%s %s %s with %s\n", GIT_BRANCH, GIT_HASH, __DATE__, __TIME__, compiler);
}

void read_scfg_ini()
{
	char path[MAX_PATH + 1];

	snprintf(path, sizeof path, "%s/scfg.ini", cfg.ctrl_dir);
	if (!fexist(path))
		snprintf(path, sizeof path, "%s/uifc.ini", cfg.ctrl_dir);
	read_uifc_ini(path, &uifc, &ciolib_mode, &video_mode);
}

int main(int argc, char **argv)
{
	char* p;
	char  errormsg[MAX_PATH * 2];
	int   i, j, main_dflt = 0, chat_dflt = 0;
	char  cfg_fname[MAX_PATH + 1];
	bool  door_mode = false;
	bool  alt_chars = false;

#if defined(_WIN32)
	cio_api.options |= CONIO_OPT_DISABLE_CLOSE;
#else
	banner();
#endif
	xp_randomize();
	cfg.size = sizeof(cfg);

	memset(&uifc, 0, sizeof(uifc));
	SAFECOPY(cfg.ctrl_dir, get_ctrl_dir(/* warn: */ true));

	read_scfg_ini();

	uifc.esc_delay = 25;

	const char* import = NULL;
	const char* grpname = NULL;
	int         grpnum = 0;
	faddr_t     faddr = {0};
	uint32_t    misc = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strncmp(argv[i], "-host=", 6) == 0) {
				hostname = argv[i] + 6;
				continue;
			}
			if (strncmp(argv[i], "-import=", 8) == 0) {
				import = argv[i] + 8;
				continue;
			}
			if (strncmp(argv[i], "-faddr=", 7) == 0) {
				faddr = atofaddr(argv[i] + 7);
				continue;
			}
			if (strncmp(argv[i], "-misc=", 6) == 0) {
				misc = strtoul(argv[i] + 6, NULL, 0);
				continue;
			}
			if (strcmp(argv[i], "-insert") == 0) {
				uifc.insert_mode = true;
				continue;
			}
			switch (argv[i][1]) {
				case 'w':
					run_wizard = true;
					break;
				case 'n':   /* Set "New Installation" flag */
					new_install = true;
					forcesave = true;
					continue;
				case 'k':   /* Keyboard only (no mouse) */
					uifc.mode |= UIFC_NOMOUSE;
					break;
				case 'm':   /* Monochrome mode */
					uifc.mode |= UIFC_MONO;
					break;
				case 'c':
					uifc.mode |= UIFC_COLOR;
					break;
				case 'u':
					umask(strtoul(argv[i] + 2, NULL, 8));
					break;
				case 'g':
					if (IS_ALPHA(argv[i][2]))
						grpname = argv[i] + 2;
					else
						grpnum = atoi(argv[i] + 2);
					break;
				case 'h':
					no_msghdr = !no_msghdr;
					break;
				case 'a':
					all_msghdr = !all_msghdr;
					break;
				case 'f':
					forcesave = true;
					break;
				case 'l':
					uifc.scrn_len = atoi(argv[i] + 2);
					break;
				case 'e':
					uifc.esc_delay = atoi(argv[i] + 2);
					break;
				case 'i':
					switch (toupper(argv[i][2])) {
						case 'A':
							ciolib_mode = CIOLIB_MODE_ANSI;
							break;
#if defined __unix__
						case 'C':
							ciolib_mode = CIOLIB_MODE_CURSES;
							break;
						case 'F':
							ciolib_mode = CIOLIB_MODE_CURSES_IBM;
							break;
						case 'X':
							ciolib_mode = CIOLIB_MODE_X;
							break;
						case 'I':
							ciolib_mode = CIOLIB_MODE_CURSES_ASCII;
							break;
#elif defined _WIN32
						case 'W':
							ciolib_mode = CIOLIB_MODE_CONIO;
							break;
						case 'G':
							switch (toupper(argv[i][3])) {
								case 0:
								case 'W':
									ciolib_mode = CIOLIB_MODE_GDI;
									break;
								case 'F':
									ciolib_mode = CIOLIB_MODE_GDI_FULLSCREEN;
									break;
							}
							break;
#endif
						case 'D':
							door_mode = true;
							break;
						default:
							goto USAGE;
					}
					break;
				case 'A':
					alt_chars = true;
					break;
				case 'v':
					video_mode = atoi(argv[i] + 2);
					break;
				case 's':
					ciolib_initial_scaling = strtod(argv[i] + 2, NULL);
					break;
				case 'y':
					auto_save = true;
					break;
				default:
USAGE:
#ifdef _WIN32
					uifc.size = sizeof(uifc);
					uifc.mode |= UIFC_NOMOUSE;
					uifc.scrn_len = 40;
					initciolib(CIOLIB_MODE_CONIO);
					uifcini32(&uifc);
					banner();
#endif

					printf("\nusage: scfg [ctrl_dir] [options]"
					       "\n\noptions:\n\n"
					       "-w                run initial setup wizard\n"
					       "-f                force save of configuration files\n"
					       "-a                update all message base status headers\n"
					       "-h                don't update message base status headers\n"
					       "-u#               set file creation permissions mask (in octal)\n"
					       "-k                keyboard mode only (no mouse support)\n"
					       "-c                force color mode\n"
					       "-m                force monochrome mode\n"
					       "-e#               set escape delay to #msec\n"
					       "-insert           enable keyboard insert mode by default\n"
					       "-import=<fname>   import a message area list file\n"
					       "-faddr=<addr>     specify your FTN address for imported subs\n"
					       "-misc=<value>     specify option flags for imported subs\n"
					       "-g#               set group number (or name) to import into\n"
					       "-host=<name>      set hostname to use for alternate sbbs.ini file\n"
					       "-iX               set interface mode to X (default=auto) where X is one of:\n"
#ifdef __unix__
					       "                   X  = X11 mode\n"
					       "                   C  = Curses mode\n"
					       "                   F  = Curses mode with forced IBM charset\n"
					       "                   I  = Curses mode with forced ASCII charset\n"
#elif defined(_WIN32)
					       "                   W  = Win32 console mode\n"
#if defined(WITH_GDI)
					       "                   G  = Win32 graphics mode\n"
					       "                   GF = Win32 graphics mode, full screen\n"
#endif // WITH_GDI
#endif // _WIN32
					       "                   A  = ANSI mode\n"
					       "                   D  = standard input/output/door mode\n"
					       "-A                use alternate (ASCII) characters for arrow symbols\n"
					       "-v#               set video mode to # (default=%u)\n"
					       "-l#               set window lines to # (default=auto-detect)\n"
					       "-s#               set window scaling factor to # (default=1.0)\n"
					       "-y                automatically save changes (don't ask)\n"
					       , video_mode
					       );
#ifdef _WIN32
					printf("\nHit a key to close...");
					getch();
#endif
					exit(0);
			}
		}
		else
			SAFECOPY(cfg.ctrl_dir, argv[i]);
	}

	if (chdir(cfg.ctrl_dir) != 0) {
		printf("!ERROR %d changing current directory to: %s\n"
		       , errno, cfg.ctrl_dir);
		exit(-1);
	}
	FULLPATH(cfg.ctrl_dir, ".", sizeof(cfg.ctrl_dir));
	backslashcolon(cfg.ctrl_dir);

	if (import != NULL && *import != 0) {
		enum { msgbase = 'M', filebase = 'F' } base = msgbase;
		char fname[MAX_PATH + 1];
		SAFECOPY(fname, import);
		p = strchr(fname, ',');
		if (p != NULL) {
			*p = 0;
			p++;
			base = toupper(*p);
		}
		FILE* fp = fopen(fname, "r");
		if (fp == NULL) {
			perror(fname);
			return EXIT_FAILURE;
		}

		printf("Reading main.ini ... ");
		if (!read_main_cfg(&cfg, error, sizeof(error))) {
			printf("ERROR: %s", error);
			return EXIT_FAILURE;
		}
		printf("\n");
		printf("Reading msgs.ini ... ");
		if (!read_msgs_cfg(&cfg, error, sizeof(error))) {
			printf("ERROR: %s", error);
			return EXIT_FAILURE;
		}
		printf("\n");

		if (grpname != NULL)
			grpnum = group_num_from_name(grpname);

		if (grpnum >= cfg.total_grps) {
			printf("!Invalid message group name specified: %s\n", grpname);
			return EXIT_FAILURE;
		}
		printf("Importing %s from %s ...", "Areas", fname);
		long ported = 0;
		long added = 0;
		switch (base) {
			case msgbase:
			{
				enum import_list_type list_type = determine_msg_list_type(fname);
				ported = import_msg_areas(list_type, fp, grpnum, 1, 99999, /* qhub: */ NULL, /* pkt_orig: */ NULL, &faddr, misc, &added);
				break;
			}
			case filebase:
			{
				fprintf(stderr, "!Not yet supported\n");
				break;
			}
		}
		fclose(fp);
		printf("\n");
		if (ported < 0)
			printf("!ERROR %ld importing areas from %s\n", ported, fname);
		else {
			printf("Imported %ld areas (%ld added) from %s\n", ported, added, fname);
			printf("Saving configuration (%u message areas) ... ", cfg.total_subs);
			write_msgs_cfg(&cfg);
			printf("done.\n");
			refresh_cfg(&cfg);
		}
		free_msgs_cfg(&cfg);
		free_main_cfg(&cfg);
		return EXIT_SUCCESS;
	}
	uifc.size = sizeof(uifc);
	if (!door_mode) {
		ciolib_initial_mode = video_mode;
		i = initciolib(ciolib_mode);
		if (i != 0) {
			printf("ciolib library init returned error %d\n", i);
			exit(1);
		}
		ciolib_settitle("Synchronet Configuration");
		i = uifcini32(&uifc);  /* curses/conio/X/ANSI */
		if (alt_chars) {
			uifc.chars->left_arrow = '<';
			uifc.chars->right_arrow = '>';
			uifc.chars->up_arrow = '^';
			uifc.chars->down_arrow = 'v';
		}
	}
	else
		i = uifcinix(&uifc); /* stdio */
	if (i != 0) {
		printf("uifc library init returned error %d\n", i);
		exit(1);
	}
	uifc.input_mode = K_TRIM; // trim all leading & trailing whitespace on string input

	if ((opt = (char **)malloc(sizeof(char *) * (MAX_OPTS + 1))) == NULL)
		allocfail(sizeof(char *) * (MAX_OPTS + 1));
	for (i = 0; i < (MAX_OPTS + 1); i++)
		if ((opt[i] = (char *)malloc(MAX_OPLN)) == NULL)
			allocfail(MAX_OPLN);

	uifc.timedisplay = display_filename;
	SAFEPRINTF2(title, "Synchronet for %s v%s", PLATFORM_DESC, VERSION);
	if (uifc.scrn(title)) {
		printf(" USCRN (len=%d) failed!\n", uifc.scrn_len + 1);
		bail(1);
	}
	title_len = strlen(title);

	SAFEPRINTF(cfg_fname, "%smain.ini", cfg.ctrl_dir);
	if (!fexist(cfg_fname)) {
		SAFEPRINTF(errormsg, "Main configuration file (%s) missing!", cfg_fname);
		uifc.msg(errormsg);
	}
	FILE* fp = iniOpenFile(cfg_fname, /* for_modify */ true);
	if (fp == NULL) {
		SAFEPRINTF2(errormsg, "Error %d opening configuration file: %s", errno, cfg_fname);
		uifc.msg(errormsg);
	} else {
		cfg.new_install = iniReadBool(fp, ROOT_SECTION, "new_install", true);
		iniCloseFile(fp);
	}
	if (run_wizard
	    || (cfg.new_install && uifc.msg("New install detected, starting Initial Setup Wizard") >= 0)) {
		cfg_wizard();
		if (run_wizard)
			bail(0);
	}
	i = 0;
	char* mopt[] = {
		"Nodes",
		"System",
		"Servers",
		"Networks",
		"File Areas",
		"File Options",
		"Chat Features",
		"Message Areas",
		"Message Options",
		"Command Shells",
		"External Programs",
		"Text File Sections",
		NULL
	};
	i = cryptInit();
	(void)i;
	while (1) {
		*cfg.filename = '\0';
		uifc.helpbuf =
			"`Main Configuration Menu:`\n"
			"\n"
			"This is the main menu of the Synchronet configuration utility (SCFG).\n"
			"From this menu, you have the following choices:\n"
			"\n"
			"    `Nodes               ` Add, delete, or configure nodes\n"
			"    `System              ` System-wide configuration options\n"
			"    `Servers             ` TCP/IP Servers and Services\n"
			"    `Networks            ` Networking configuration\n"
			"    `File Areas          ` File area configuration\n"
			"    `File Options        ` File area options\n"
			"    `Chat Features       ` Chat actions, sections, pagers, and robots\n"
			"    `Message Areas       ` Message area configuration\n"
			"    `Message Options     ` Message and e-mail options\n"
			"    `Command Shells      ` Terminal server user interface/menu modules\n"
			"    `External Programs   ` Events, editors, and online programs (doors)\n"
			"    `Text File Sections  ` Text file areas available for online viewing\n"
			"\n"
			"Use the arrow keys and ~ ENTER ~ to select an option, or ~ ESC ~ to exit.\n"
			"\n"
			"`More keys/combinations` and (`alternatives`):\n"
			"\n"
			"   ~ Ctrl-F ~    Find item in list\n"
			"   ~ Ctrl-G ~    Find next item in list\n"
			"   ~ Ctrl-U ~    Move up through list one screen-full (`PageUp`)\n"
			"   ~ Ctrl-D ~    Move down through list one screen-full (`PageDown`)\n"
			"   ~ Ctrl-B ~    Move to top of list or start of edited text (`Home`)\n"
			"   ~ Ctrl-E ~    Move to end of list or end of edited text (`End`)\n"
			"   ~ Insert ~    Insert an item (`+`) or toggle text overwrite mode\n"
			"   ~ Delete ~    Delete an item (`-`) or current character of edited text\n"
			"   ~ Ctrl-C ~    Copy item (`F5`, `Ctrl-Insert`)\n"
			"   ~ Ctrl-X ~    Cut item (`Shift-Delete`)\n"
			"   ~ Ctrl-V ~    Paste item (`F6`, `Shift-Insert`)\n"
			"   ~ Ctrl-Z ~    Display help text (`F1`, `?`)\n"
			"   ~ Backspace ~ Move back/up one menu (`ESC`) or erase previous character\n"
		;
		switch (uifc.list(WIN_ORG | WIN_MID | WIN_ESC | WIN_ACT, 0, 0, 30, &main_dflt, 0
		                  , "Configure", mopt)) {
			case 0:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				set_cfg_filename(hostname);
				node_menu();
				free_main_cfg(&cfg);
				break;
			case 1:
				if (!load_xtrn_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				sys_cfg();
				free_xtrn_cfg(&cfg);
				free_main_cfg(&cfg);
				break;
			case 2:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				set_cfg_filename(hostname);
				server_cfg();
				free_main_cfg(&cfg);
				break;
			case 3:
				net_cfg();
				break;
			case 4:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				if (!load_file_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				xfer_cfg();
				free_file_cfg(&cfg);
				free_main_cfg(&cfg);
				break;
			case 5:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				if (!load_file_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				xfer_opts();
				free_file_cfg(&cfg);
				free_main_cfg(&cfg);
				break;
			case 6:
				if (!load_chat_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				while (1) {
					i = 0;
					strcpy(opt[i++], "Artificial Gurus");
					strcpy(opt[i++], "Multinode Chat Actions");
					strcpy(opt[i++], "Multinode Chat Channels");
					strcpy(opt[i++], "External Sysop Chat Pagers");
					opt[i][0] = 0;
					uifc.helpbuf =
						"`Chat Features:`\n"
						"\n"
						"Here you may configure the real-time chat-related features of the BBS.";
					j = uifc.list(WIN_ORG | WIN_ACT | WIN_CHE, 0, 0, 0, &chat_dflt, 0
					              , "Chat Features", opt);
					if (j == -1) {
						j = save_changes(WIN_MID);
						if (j == -1)
							continue;
						if (!j) {
							save_chat_cfg(&cfg);
							refresh_cfg(&cfg);
						}
						break;
					}
					switch (j) {
						case 0:
							guru_cfg();
							break;
						case 1:
							actsets_cfg();
							break;
						case 2:
							chan_cfg();
							break;
						case 3:
							page_cfg();
							break;
					}
				}
				free_chat_cfg(&cfg);
				break;
			case 7:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				if (!load_msgs_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				msgs_cfg();
				free_msgs_cfg(&cfg);
				free_main_cfg(&cfg);
				break;
			case 8:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				if (!load_msgs_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				msg_opts();
				free_msgs_cfg(&cfg);
				free_main_cfg(&cfg);
				break;
			case 9:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				shell_cfg();
				free_main_cfg(&cfg);
				break;
			case 10:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				if (!load_xtrn_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				xprogs_cfg();
				free_xtrn_cfg(&cfg);
				free_main_cfg(&cfg);
				break;
			case 11:
				if (!load_main_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				if (!load_file_cfg(&cfg, error, sizeof(error))) {
					SAFEPRINTF(errormsg, "ERROR: %s", error);
					uifc.msg(errormsg);
					break;
				}
				txt_cfg();
				free_file_cfg(&cfg);
				free_main_cfg(&cfg);
				break;
			case -1:
				i = 0;
				uifc.helpbuf =
					"`Exit SCFG:`\n"
					"\n"
					"If you want to exit the Synchronet configuration utility, select `Yes`.\n"
					"Otherwise, select `No` or hit ~ ESC ~.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Exit SCFG", uifcYesNoOpts);
				if (!i || uifc.exit_flags & UIFC_XF_QUIT)
					bail(0);
				break;
		}
	}
}

bool load_main_cfg(scfg_t* cfg, char *error, size_t maxerrlen)
{
	uifc.pop("Reading main.ini ...");
	bool result = read_main_cfg(cfg, error, maxerrlen);
	uifc.pop(NULL);
	return result;
}

bool load_node_cfg(scfg_t* cfg, char *error, size_t maxerrlen)
{
	uifc.pop("Reading node.ini ...");
	bool result = read_node_cfg(cfg, error, maxerrlen);
	uifc.pop(NULL);
	return result;
}

bool load_msgs_cfg(scfg_t* cfg, char *error, size_t maxerrlen)
{
	uifc.pop("Reading msgs.ini ...");
	bool result = read_msgs_cfg(cfg, error, maxerrlen);
	uifc.pop(NULL);
	return result;
}

bool load_file_cfg(scfg_t* cfg, char *error, size_t maxerrlen)
{
	uifc.pop("Reading file.ini ...");
	bool result = read_file_cfg(cfg, error, maxerrlen);
	uifc.pop(NULL);
	return result;
}

bool load_chat_cfg(scfg_t* cfg, char *error, size_t maxerrlen)
{
	uifc.pop("Reading chat.ini ...");
	bool result = read_chat_cfg(cfg, error, maxerrlen);
	uifc.pop(NULL);
	return result;
}

bool load_xtrn_cfg(scfg_t* cfg, char *error, size_t maxerrlen)
{
	uifc.pop("Reading xtrn.ini ...");
	bool result = read_xtrn_cfg(cfg, error, maxerrlen);
	uifc.pop(NULL);
	return result;
}

bool save_main_cfg(scfg_t* cfg)
{
	uifc.pop("Writing main.ini ...");
	bool result = write_main_cfg(cfg);
	uifc.pop(NULL);
	return result;
}

bool save_node_cfg(scfg_t* cfg)
{
	uifc.pop("Writing node.ini ...");
	bool result = write_node_cfg(cfg);
	uifc.pop(NULL);
	return result;
}

bool save_msgs_cfg(scfg_t* cfg)
{
	uifc.pop("Writing msgs.ini ...");
	bool result = write_msgs_cfg(cfg);
	uifc.pop(NULL);
	return result;
}

bool save_file_cfg(scfg_t* cfg)
{
	uifc.pop("Writing file.ini ...");
	bool result = write_file_cfg(cfg);
	uifc.pop(NULL);
	return result;
}

bool save_chat_cfg(scfg_t* cfg)
{
	uifc.pop("Writing chat.ini ...");
	bool result = write_chat_cfg(cfg);
	uifc.pop(NULL);
	return result;
}

bool save_xtrn_cfg(scfg_t* cfg)
{
	uifc.pop("Writing xtrn.ini ...");
	bool result = write_xtrn_cfg(cfg);
	uifc.pop(NULL);
	return result;
}


/****************************************************************************/
/* Checks the uifc.changes variable. If there have been no changes, returns 2.	*/
/* If there have been changes, it prompts the user to change or not. If the */
/* user escapes the menu, returns -1, selects Yes, 0, and selects no, 1 	*/
/****************************************************************************/
int save_changes(int mode)
{
	int i = 0;

	if (!uifc.changes)
		return 2;
	if (auto_save == true) { /* -y switch used, return "Yes" */
		uifc.changes = FALSE;
		return 0;
	}
	uifc.helpbuf =
		"`Save Changes:`\n"
		"\n"
		"You have made some changes to the configuration. If you want to save\n"
		"these changes, select `Yes`. If you are positive you DO NOT want to save\n"
		"these changes, select `No`. If you are not sure and want to review the\n"
		"configuration before deciding, hit ~ ESC ~.\n"
	;
	i = uifc.list(mode | WIN_SAV | WIN_ATEXIT, 0, 0, 0, &i, 0, "Save Changes", uifcYesNoOpts);
	if (i == -1) {
		uifc.exit_flags &= ~UIFC_XF_QUIT;
	}
	else
		uifc.changes = FALSE;
	return i;
}

void txt_cfg()
{
	static int      txt_dflt, bar;
	char            str[128], code[128], done = 0;
	int             j, k;
	int             i, u;
	static txtsec_t savtxtsec;

	while (1) {
		for (i = 0; i < cfg.total_txtsecs; i++)
			snprintf(opt[i], MAX_OPLN, "%-25s", cfg.txtsec[i]->name);
		opt[i][0] = 0;
		j = WIN_ORG | WIN_ACT | WIN_CHE;
		if (cfg.total_txtsecs)
			j |= WIN_DEL | WIN_COPY | WIN_CUT;
		if (cfg.total_txtsecs < MAX_OPTS)
			j |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savtxtsec.name[0])
			j |= WIN_PASTE;
		uifc.helpbuf =
			"`Text File Sections:`\n"
			"\n"
			"This is a list of `General Text File (G-File) Sections` configured for\n"
			"your system. G-File sections are used to store text files that can be\n"
			"viewed freely by the users. Common text file section topics include\n"
			"`ANSI Artwork`, `System Information`, `Game Help Files`, and other special\n"
			"interest topics.\n"
			"\n"
			"To add a text file section, select the desired location with the arrow\n"
			"keys and hit ~ INS ~.\n"
			"\n"
			"To delete a text file section, select it and hit ~ DEL ~.\n"
			"\n"
			"To configure a text file, select it and hit ~ ENTER ~.\n"
		;
		i = uifc.list(j, 0, 0, 45, &txt_dflt, &bar, "Text File Sections", opt);
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
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf =
				"`Text Section Name:`\n"
				"\n"
				"This is the name of this text section.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Text Section Name", str, 40, K_NONE) < 1)
				continue;
			SAFECOPY(code, str);
			prep_code(code, /* prefix: */ NULL);
			uifc.helpbuf =
				"`Text Section Internal Code:`\n"
				"\n"
				"Every text file section must have its own unique internal code for\n"
				"Synchronet to reference it by. It is helpful if this code is an\n"
				"abbreviation of the name.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Text Section Internal Code", code, LEN_CODE
			               , K_EDIT | K_UPPER | K_NOSPACE) < 1)
				continue;
			if (textsec_is_valid(&cfg, gettextsec(&cfg, code))) {
				uifc.msg(strDuplicateCode);
				continue;
			}
			if (!code_ok(code)) {
				uifc.helpbuf = invalid_code;
				uifc.msg(strInvalidCode);
				uifc.helpbuf = 0;
				continue;
			}
			if ((cfg.txtsec = (txtsec_t **)realloc_or_free(cfg.txtsec
			                                               , sizeof(txtsec_t *) * (cfg.total_txtsecs + 1))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_txtsecs + 1);
				cfg.total_txtsecs = 0;
				bail(1);
				continue;
			}
			if (cfg.total_txtsecs)
				for (u = cfg.total_txtsecs; u > i; u--)
					cfg.txtsec[u] = cfg.txtsec[u - 1];
			if ((cfg.txtsec[i] = (txtsec_t *)malloc(sizeof(txtsec_t))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(txtsec_t));
				continue;
			}
			memset((txtsec_t *)cfg.txtsec[i], 0, sizeof(txtsec_t));
			SAFECOPY(cfg.txtsec[i]->name, str);
			SAFECOPY(cfg.txtsec[i]->code, code);
			cfg.total_txtsecs++;
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_CUT)
				savtxtsec = *cfg.txtsec[i];
			free(cfg.txtsec[i]);
			cfg.total_txtsecs--;
			for (j = i; j < cfg.total_txtsecs; j++)
				cfg.txtsec[j] = cfg.txtsec[j + 1];
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_COPY) {
			savtxtsec = *cfg.txtsec[i];
			continue;
		}
		if (msk == MSK_PASTE) {
			*cfg.txtsec[i] = savtxtsec;
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		i = txt_dflt;
		j = 0;
		done = 0;
		while (!done) {
			k = 0;
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Name", cfg.txtsec[i]->name);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Internal Code", cfg.txtsec[i]->code);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Access Requirements"
			         , cfg.txtsec[i]->arstr);
			opt[k][0] = 0;
			uifc_winmode_t wmode = WIN_ACT | WIN_MID | WIN_EXTKEYS;
			if (i > 0)
				wmode |= WIN_LEFTKEY;
			if (i + 1 < cfg.total_txtsecs)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 60, &j, 0, cfg.txtsec[i]->name
			                  , opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					if (i > 0)
						i--;
					break;
				case -CIO_KEY_RIGHT - 2:
					if (i + 1 < cfg.total_txtsecs)
						i++;
					break;
				case 0:
					uifc.helpbuf =
						"`Text Section Name:`\n"
						"\n"
						"This is the name of this text section.\n"
					;
					SAFECOPY(str, cfg.txtsec[i]->name);  /* save */
					if (!uifc.input(WIN_MID | WIN_SAV, 0, 10
					                , "Text File Section Name"
					                , cfg.txtsec[i]->name, sizeof(cfg.txtsec[i]->name) - 1, K_EDIT))
						SAFECOPY(cfg.txtsec[i]->name, str);
					break;
				case 1:
					SAFECOPY(str, cfg.txtsec[i]->code);
					uifc.helpbuf =
						"`Text Section Internal Code:`\n"
						"\n"
						"Every text file section must have its own unique internal code for\n"
						"Synchronet to reference it by. It is helpful if this code is an\n"
						"abbreviation of the name.\n"
					;
					if (uifc.input(WIN_MID | WIN_SAV, 0, 17, "Internal Code (unique)"
					           , str, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE | K_CHANGED | K_FIND) < 1)
						break;
					if (textsec_is_valid(&cfg, gettextsec(&cfg, str))) {
						uifc.msg(strDuplicateCode);
						break;
					}
					if (code_ok(str)) {
						SAFECOPY(cfg.txtsec[i]->code, str);
						uifc.changes = TRUE;
					}
					else {
						uifc.helpbuf = invalid_code;
						uifc.msg(strInvalidCode);
						uifc.helpbuf = 0;
					}
					break;
				case 2:
					sprintf(str, "%s Text Section", cfg.txtsec[i]->name);
					getar(str, cfg.txtsec[i]->arstr);
					break;
			}
		}
	}
}

void shell_cfg()
{
	static int     shell_dflt, shell_bar;
	char           str[128], code[128], done = 0;
	int            j, k;
	int            i;
	int            u;
	static shell_t savshell;

	while (1) {
		for (i = 0; i < cfg.total_shells; i++)
			snprintf(opt[i], MAX_OPLN, "%-25s", cfg.shell[i]->name);
		opt[i][0] = 0;
		j = WIN_ORG | WIN_ACT | WIN_CHE;
		if (cfg.total_shells)
			j |= WIN_DEL | WIN_COPY | WIN_CUT;
		if (cfg.total_shells < MAX_OPTS)
			j |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (savshell.name[0])
			j |= WIN_PASTE;
		uifc.helpbuf =
			"`Command Shells:`\n"
			"\n"
			"This is a list of `Command Shells` configured for your system.\n"
			"Command shells are modules that provide the user interface and menu\n"
			"structure for the remote users of your BBS's terminal server.\n"
			"\n"
			"To add a command shell section, select the desired location with the\n"
			"arrow keys and hit ~ INS ~.\n"
			"\n"
			"To delete a command shell, select it and hit ~ DEL ~.\n"
			"\n"
			"To configure a command shell, select it and hit ~ ENTER ~.\n"
		;
		i = uifc.list(j, 0, 0, 45, &shell_dflt, &shell_bar, "Command Shells", opt);
		if ((signed)i == -1) {
			j = save_changes(WIN_MID);
			if (j == -1)
				continue;
			if (!j) {
				cfg.new_install = new_install;
				save_main_cfg(&cfg);
				refresh_cfg(&cfg);
			}
			return;
		}
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if (msk == MSK_INS) {
			uifc.helpbuf =
				"`Command Shell Name:`\n"
				"\n"
				"This is the descriptive name of this command shell.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Command Shell Name", str, 40, K_NONE) < 1)
				continue;
			SAFECOPY(code, str);
			prep_code(code, /* prefix: */ NULL);
			uifc.helpbuf =
				"`Command Shell Internal Code:`\n"
				"\n"
				"Every command shell must have its own unique internal code for\n"
				"Synchronet to reference it by. It is helpful if this code is an\n"
				"abbreviation of the name.\n"
				"\n"
				"This code will be the base filename used to load the shell from your\n"
				"`exec` directory. e.g. A shell with an internal code of `MYBBS` would\n"
				"indicate a shell file named 'mybbs.js' or `mybbs.bin` in your `exec`\n"
				"or your `mods` directory.\n"
			;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Command Shell Internal Code", code, LEN_CODE
			               , K_EDIT | K_UPPER | K_NOSPACE) < 1)
				continue;
			if (shellnum_is_valid(&cfg, getshellnum(&cfg, code, -1))) {
				uifc.msg(strDuplicateCode);
				continue;
			}
			if (!code_ok(code)) {
				uifc.helpbuf = invalid_code;
				uifc.msg(strInvalidCode);
				uifc.helpbuf = 0;
				continue;
			}
			if ((cfg.shell = (shell_t **)realloc_or_free(cfg.shell
			                                             , sizeof(shell_t *) * (cfg.total_shells + 1))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, nulstr, cfg.total_shells + 1);
				cfg.total_shells = 0;
				bail(1);
				continue;
			}
			if (cfg.total_shells)
				for (u = cfg.total_shells; u > i; u--)
					cfg.shell[u] = cfg.shell[u - 1];
			if ((cfg.shell[i] = (shell_t *)malloc(sizeof(shell_t))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(shell_t));
				continue;
			}
			memset((shell_t *)cfg.shell[i], 0, sizeof(shell_t));
			SAFECOPY(cfg.shell[i]->name, str);
			SAFECOPY(cfg.shell[i]->code, code);
			cfg.total_shells++;
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_CUT)
				savshell = *cfg.shell[i];
			free(cfg.shell[i]);
			cfg.total_shells--;
			for (j = i; j < cfg.total_shells; j++)
				cfg.shell[j] = cfg.shell[j + 1];
			uifc.changes = TRUE;
			continue;
		}
		if (msk == MSK_COPY) {
			savshell = *cfg.shell[i];
			continue;
		}
		if (msk == MSK_PASTE) {
			*cfg.shell[i] = savshell;
			uifc.changes = TRUE;
			continue;
		}
		if (msk != 0)
			continue;
		i = shell_dflt;
		j = 0;
		done = 0;
		while (!done) {
			static int bar;
			k = 0;
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Name", cfg.shell[i]->name);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Internal Code", cfg.shell[i]->code);
			snprintf(opt[k++], MAX_OPLN, "%-27.27s%s", "Access Requirements"
			         , cfg.shell[i]->arstr);
			opt[k][0] = 0;
			uifc.helpbuf =
				"`Command Shell:`\n"
				"\n"
				"A command shell is a programmed command and menu structure that you or\n"
				"your users can use to navigate the BBS Terminal Server.  For each\n"
				"command shell, there must be an associated `.js` or `.bin` file in your\n"
				"`exec` or `mods` directory for Synchronet to execute upon user logon.\n"
				"\n"
				"Command shells may be JavaScript modules (`.js` files) in your `exec` or\n"
				"`mods` directory, created with any text editor.\n"
				"\n"
				"Legacy command shells may be created by using the Baja compiler\n"
				"to turn Baja source code (`.src`) files into binary files (`.bin`) for\n"
				"Synchronet to interpret.  See the example `.src` files in the `exec`\n"
				"directory and the documentation for the Baja compiler for more details.\n"
			;
			uifc_winmode_t wmode = WIN_ACT | WIN_MID | WIN_EXTKEYS;
			if (i > 0)
				wmode |= WIN_LEFTKEY;
			if (i + 1 < cfg.total_shells)
				wmode |= WIN_RIGHTKEY;
			switch (uifc.list(wmode, 0, 0, 60, &j, &bar, cfg.shell[i]->name
			                  , opt)) {
				case -1:
					done = 1;
					break;
				case -CIO_KEY_LEFT - 2:
					if (i > 0)
						i--;
					break;
				case -CIO_KEY_RIGHT - 2:
					if (i + 1 < cfg.total_shells)
						i++;
					break;
				case 0:
					uifc.helpbuf =
						"`Command Shell Name:`\n"
						"\n"
						"This is the descriptive name of this command shell.\n"
					;
					SAFECOPY(str, cfg.shell[i]->name);    /* save */
					if (!uifc.input(WIN_MID | WIN_SAV, 0, 10
					                , "Command Shell Name"
					                , cfg.shell[i]->name, sizeof(cfg.shell[i]->name) - 1, K_EDIT))
						SAFECOPY(cfg.shell[i]->name, str);
					break;
				case 1:
					uifc.helpbuf =
						"`WARNING:`\n"
						"\n"
						"If you change a command shell's internal code, any users that have\n"
						"chosen that shell as their default, will be reverted to the default\n"
						"command shell for new users or the first configured command shell\n"
						"for which they meet the access requirements.";
					if (uifc.deny("Changing a shell's internal code is not recommended. Edit anyway?"))
						break;
					SAFECOPY(str, cfg.shell[i]->code);
					uifc.helpbuf =
						"`Command Shell Internal Code:`\n"
						"\n"
						"Every command shell must have its own unique internal code for\n"
						"Synchronet to reference it by.\n"
						"\n"
						"This code will be the base filename of the command shell script/module.\n"
						"e.g. A shell with an internal code of `MYBBS` would correlate with a\n"
						"Baja shell file named `mybbs.bin` or a JavaScript module named `mybbs.js`\n"
						"located in your `exec` or `mods` directories.\n"
					;
					if (uifc.input(WIN_MID | WIN_SAV, 0, 17, "Internal Code (unique)"
					           , str, LEN_CODE, K_EDIT | K_UPPER | K_NOSPACE | K_CHANGED | K_FIND) < 1)
						break;
					if (shellnum_is_valid(&cfg, getshellnum(&cfg, str, -1))) {
						uifc.msg(strDuplicateCode);
						break;
					}
					if (code_ok(str)) {
						SAFECOPY(cfg.shell[i]->code, str);
						uifc.changes = TRUE;
					}
					else {
						uifc.helpbuf = invalid_code;
						uifc.msg(strInvalidCode);
						uifc.helpbuf = 0;
					}
					break;
				case 2:
					SAFEPRINTF(str, "%s Command Shell", cfg.shell[i]->name);
					getar(str, cfg.shell[i]->arstr);
					break;
			}
		}
	}
}

int whichlogic(void)
{
	int i;

	i = 0;
	strcpy(opt[0], "Greater than or Equal");
	strcpy(opt[1], "Equal");
	strcpy(opt[2], "Not Equal");
	strcpy(opt[3], "Less than");
	opt[4][0] = 0;
	uifc.helpbuf =
		"`Select Logic for Requirement:`\n"
		"\n"
		"This menu allows you to choose the type of logic evaluation to use\n"
		"in determining if the requirement is met. If, for example, the user's\n"
		"level is being evaluated and you select `Greater than or Equal` from\n"
		"this menu and set the required level to `50`, the user must have level\n"
		"`50 or higher` to meet this requirement. If you selected `Equal` from\n"
		"this menu and set the required level to `50`, the user must have level\n"
		"`50 exactly`. If you select `Not equal` and level `50`, then the user\n"
		"must have `any level BUT 50`. And if you select `Less than` from this\n"
		"menu and level `50`, the user must have a level `below 50`.\n"
	;
	i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Select Logic", opt);
	return i;
}

int whichcond(void)
{
	int i;

	i = 0;
	char* opt[] = {
		"AND (Both/All)",
	    "OR  (Either/Any)",
	    NULL
	};
	uifc.helpbuf =
		"`Select Logic for Multiple Requirements:`\n"
		"\n"
		"If you wish this new parameter to be required along with the other\n"
		"parameters, select `AND` to specify that `both` or `all` of the\n"
		"parameter requirements must be met.\n"
		"\n"
		"If you wish this new parameter to only be required if the other\n"
		"parameter requirements aren't met, select `OR` to specify that `either`\n"
		"or `any` of the parameter requirements must be met.\n"
	;
	i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Multiple Requirement Logic", opt);
	return i;
}


void getar(const char *desc, char *inar)
{
	static int curar;
	char       str[128], ar[128];
	int        i, j, len, done = 0, n;

	SAFECOPY(ar, inar);
	while (!done) {
		len = strlen(ar);
		if (len >= (LEN_ARSTR - 10)) { /* Needs to be shortened */
			str[0] = 0;
			n = strlen(ar);
			for (i = 0; i < n; i++) {       /* Shorten operators */
				if (!strncmp(ar + i, "AND", 3)) {
					SAFECAT(str, "&");
					i += 2;
				}
				else if (!strncmp(ar + i, "NOT", 3)) {
					SAFECAT(str, "!");
					i += 2;
				}
				else if (!strncmp(ar + i, "EQUAL", 5)) {
					SAFECAT(str, "=");
					i += 4;
				}
				else if (!strncmp(ar + i, "EQUALS", 6)) {
					SAFECAT(str, "=");
					i += 5;
				}
				else if (!strncmp(ar + i, "EQUAL TO", 8)) {
					SAFECAT(str, "=");
					i += 7;
				}
				else if (!strncmp(ar + i, "OR", 2)) {
					SAFECAT(str, "|");
					i += 1;
				}
				else
					strncat(str, ar + i, 1);
			}
			SAFECOPY(ar, str);
			len = strlen(ar);
		}

		if (len >= (LEN_ARSTR - 10)) {
			str[0] = 0;
			n = strlen(ar);
			for (i = 0; i < n; i++) {       /* Remove spaces from ! and = */
				if (!strncmp(ar + i, " ! ", 3)) {
					SAFECAT(str, "!");
					i += 2;
				}
				else if (!strncmp(ar + i, "= ", 2)) {
					SAFECAT(str, "=");
					i++;
				}
				else if (!strncmp(ar + i, " = ", 3)) {
					SAFECAT(str, "=");
					i += 2;
				}
				else
					strncat(str, ar + i, 1);
			}
			SAFECOPY(ar, str);
			len = strlen(ar);
		}

		if (len >= (LEN_ARSTR - 10)) {
			str[0] = 0;
			n = strlen(ar);
			for (i = 0; i < n; i++) {       /* Remove spaces from & and | */
				if (!strncmp(ar + i, " & ", 3)) {
					SAFECAT(str, " ");
					i += 2;
				}
				else if (!strncmp(ar + i, " | ", 3)) {
					SAFECAT(str, "|");
					i += 2;
				}
				else
					strncat(str, ar + i, 1);
			}
			SAFECOPY(ar, str);
			len = strlen(ar);
		}

		if (len >= (LEN_ARSTR - 10)) {            /* change week days to numbers */
			str[0] = 0;
			n = strlen(ar);
			for (i = 0; i < n; i++) {
				for (j = 0; j < 7; j++)
					if (!strnicmp(ar + i, wday[j], 3)) {
						SAFECAT(str, ultoa(j, tmp, 10));
						i += 2;
						break;
					}
				if (j == 7)
					strncat(str, ar + i, 1);
			}
			SAFECOPY(ar, str);
			len = strlen(ar);
		}

		if (len >= (LEN_ARSTR - 10)) {          /* Shorten parameters */
			str[0] = 0;
			n = strlen(ar);
			for (i = 0; i < n; i++) {
				if (!strncmp(ar + i, "AGE", 3)) {
					SAFECAT(str, "$A");
					i += 2;
				}
				else if (!strncmp(ar + i, "BPS", 3)) {
					SAFECAT(str, "$B");
					i += 2;
				}
				else if (!strncmp(ar + i, "PCR", 3)) {
					SAFECAT(str, "$P");
					i += 2;
				}
				else if (!strncmp(ar + i, "RIP", 3)) {
					SAFECAT(str, "$*");
					i += 2;
				}
				else if (!strncmp(ar + i, "SEX", 3)) {
					SAFECAT(str, "$S");
					i += 2;
				}
				else if (!strncmp(ar + i, "UDR", 3)) {
					SAFECAT(str, "$K");
					i += 2;
				}
				else if (!strncmp(ar + i, "DAY", 3)) {
					SAFECAT(str, "$W");
					i += 2;
				}
				else if (!strncmp(ar + i, "ANSI", 4)) {
					SAFECAT(str, "$[");
					i += 3;
				}
				else if (!strncmp(ar + i, "UDFR", 4)) {
					SAFECAT(str, "$D");
					i += 3;
				}
				else if (!strncmp(ar + i, "FLAG", 4)) {
					SAFECAT(str, "$F");
					i += 3;
				}
				else if (!strncmp(ar + i, "NODE", 4)) {
					SAFECAT(str, "$N");
					i += 3;
				}
				else if (!strncmp(ar + i, "NULL", 4)) {
					SAFECAT(str, "$0");
					i += 3;
				}
				else if (!strncmp(ar + i, "TIME", 4)) {
					SAFECAT(str, "$T");
					i += 3;
				}
				else if (!strncmp(ar + i, "USER", 4)) {
					SAFECAT(str, "$U");
					i += 3;
				}
				else if (!strncmp(ar + i, "REST", 4)) {
					SAFECAT(str, "$Z");
					i += 3;
				}
				else if (!strncmp(ar + i, "LOCAL", 5)) {
					SAFECAT(str, "$G");
					i += 4;
				}
				else if (!strncmp(ar + i, "LEVEL", 5)) {
					SAFECAT(str, "$L");
					i += 4;
				}
				else if (!strncmp(ar + i, "TLEFT", 5)) {
					SAFECAT(str, "$R");
					i += 4;
				}
				else if (!strncmp(ar + i, "TUSED", 5)) {
					SAFECAT(str, "$O");
					i += 4;
				}
				else if (!strncmp(ar + i, "EXPIRE", 6)) {
					SAFECAT(str, "$E");
					i += 5;
				}
				else if (!strncmp(ar + i, "CREDIT", 6)) {
					SAFECAT(str, "$C");
					i += 5;
				}
				else if (!strncmp(ar + i, "EXEMPT", 6)) {
					SAFECAT(str, "$X");
					i += 5;
				}
				else if (!strncmp(ar + i, "RANDOM", 6)) {
					SAFECAT(str, "$Q");
					i += 5;
				}
				else if (!strncmp(ar + i, "LASTON", 6)) {
					SAFECAT(str, "$Y");
					i += 5;
				}
				else if (!strncmp(ar + i, "LOGONS", 6)) {
					SAFECAT(str, "$V");
					i += 5;
				}
				else if (!strncmp(ar + i, ":00", 3)) {
					i += 2;
				}
				else
					strncat(str, ar + i, 1);
			}
			SAFECOPY(ar, str);
			len = strlen(ar);
		}
		if (len >= (LEN_ARSTR - 10)) {          /* Remove all spaces and &s */
			str[0] = 0;
			n = strlen(ar);
			for (i = 0; i < n; i++)
				if (ar[i] != ' ' && ar[i] != '&')
					strncat(str, ar + i, 1);
			SAFECOPY(ar, str);
		}
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "Requirement String (%s)", ar);
		strcpy(opt[i++], "Clear Requirements");
		strcpy(opt[i++], "Set Required Terminal Type/Capability");
		strcpy(opt[i++], "Set Required User Type (e.g. Sysop, Guest)");
		strcpy(opt[i++], "Set Required Security Level");
		strcpy(opt[i++], "Set Required Security Flag");
		strcpy(opt[i++], "Set Required Age");
		strcpy(opt[i++], "Set Required Gender");
		strcpy(opt[i++], "Set Required Post/Call Ratio (percentage)");
		strcpy(opt[i++], "Set Required Number of Credits");
		strcpy(opt[i++], "Set Required Upload/Download Byte Ratio (percentage)");
		strcpy(opt[i++], "Set Required Upload/Download File Ratio (percentage)");
		strcpy(opt[i++], "Set Required Time of Day");
		strcpy(opt[i++], "Set Required Day of Week");
		strcpy(opt[i++], "Set Required User Number");
		strcpy(opt[i++], "Set Required Time Remaining");
		opt[i][0] = 0;
		uifc.helpbuf =
			"`Access Requirements:`\n"
			"\n"
			"This menu allows you to edit the access requirement string for the\n"
			"selected feature/section of your BBS. You can edit the string\n"
			"directly (see documentation for details) or use the `Set Required...`\n"
			"options from this menu to automatically fill in the string for you.\n"
		;
		sprintf(str, "%s Requirements", desc);
		i = uifc.list(WIN_ACT | WIN_MID | WIN_SAV, 0, 0, 60, &curar, 0, str, opt);
		if (i > 1 && strlen(ar) >= (LEN_ARSTR - 10)) {
			uifc.msg("Maximum string length reached");
			continue;
		}
		switch (i) {
			case -1:
				done = 1;
				break;
			case 0:
				uifc.helpbuf =
					"Key word   Symbol      Description\n"
					"컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴\n"
					"AND          &         More than one requirement (optional)\n"
					"NOT          !         Logical negation (i.e. NOT EQUAL)\n"
					"EQUAL        =         Equality required\n"
					"OR           |         Either of two or more parameters is required\n"
					"AGE          $A        User's age (years since birth date, 0-255)\n"
					"FLAG         $F        User's flag (A-Z)\n"
					"LEVEL        $L        User's level (0-99)\n"
					"PCR          $P        User's post/call ratio (0-100)\n"
					"SEX          $S        User's sex/gender (e.g. M, F)\n"
					"TIME         $T        Time of day (HH:MM, 00:00-23:59)\n"
					"TLEFT        $R        User's time left online (minutes, 0-255)\n"
					"TUSED        $O        User's time online this call (minutes, 0-255)\n"
					"USER         $U        User's number (1-xxxx)\n"
					"ANSI         $[        ANSI Terminal in use\n"
					"COLS                   Terminal columns (e.g. 80)\n"
					"             ()        Nested requirements (mixing AND and OR logic)\n"
					"                       `... and a lot more ...`\n"
					"                       see `https://wiki.synchro.net/access:requirements`\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Requirement String", ar, LEN_ARSTR
				           , K_EDIT | K_UPPER);
				break;
			case 1:
				i = 1;
				uifc.helpbuf =
					"`Clear Requirements:`\n"
					"\n"
					"If you wish to clear the current requirement string, select `Yes`.\n"
					"Otherwise, select `No`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Are You Sure", uifcYesNoOpts);
				if (!i) {
					ar[0] = 0;
					uifc.changes = TRUE;
				}
				break;
			case 2:
				i = 0;
				snprintf(opt[i++], MAX_OPLN, "ANSI");
				snprintf(opt[i++], MAX_OPLN, "RIP");
				snprintf(opt[i++], MAX_OPLN, "PETSCII");
				snprintf(opt[i++], MAX_OPLN, "ASCII");
				snprintf(opt[i++], MAX_OPLN, "CP437");
				snprintf(opt[i++], MAX_OPLN, "UTF8");
				opt[i][0] = 0;
				i = 0;
				uifc.helpbuf =
					"`Required Terminal Flag:`\n"
					"\n"
					"You are being prompted to select a terminal type or capability flag\n"
					"to be included in this requirement evaluation.\n"
				;
				static int term_cur;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &term_cur, 0, "Select Terminal Flag", opt);
				if (i < 0)
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, opt[i]);
				uifc.changes = TRUE;
				break;
			case 3:
				i = 0;
				const char* user_type[] = { "GUEST", "SYSOP", "QNODE" };
				snprintf(opt[i++], MAX_OPLN, "Guest");
				snprintf(opt[i++], MAX_OPLN, "Sysop");
				snprintf(opt[i++], MAX_OPLN, "QWKnet Node");
				opt[i][0] = 0;
				i = 0;
				uifc.helpbuf =
					"`Required User Type:`\n"
					"\n"
					"You are being prompted to select a user category to be included in this\n"
					"requirement evaluation.\n"
					"\n"
					"`Guest`       = One and only G-restricted account on the system, usually\n"
					"`Sysop`       = Any user with security level >= 90 or temp-sysop status\n"
					"`QWKnet Node` = A QWK Network Node account\n"
				;
				static int user_type_cur;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &user_type_cur, 0, "Select User Type", opt);
				if (i < 0)
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, user_type[i]);
				uifc.changes = TRUE;
				break;
			case 4:
				i = whichlogic();
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Security Level:`\n"
					"\n"
					"You are being prompted to enter the security level to be used in this\n"
					"requirement evaluation. The valid range is 0 (zero) through 99.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Security Level", str, 2, K_NUMBER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "LEVEL ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;
			case 5:
				for (i = 0; i < 4; i++)
					snprintf(opt[i], MAX_OPLN, "Flag Set #%d", i + 1);
				opt[i][0] = 0;
				i = 0;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Select Flag Set", opt);
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Security Flag:`\n"
					"\n"
					"You are being prompted to enter the security flag to be used in this\n"
					"requirement evaluation. The valid range is A through Z.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Security Flag (A-Z)", str, 1
				           , K_UPPER | K_ALPHA);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "FLAG ");
				if (i)
					SAFECAT(ar, ultoa(i + 1, tmp, 10));
				SAFECAT(ar, str);
				break;
			case 6:
				i = whichlogic();
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Age:`\n"
					"\n"
					"You are being prompted to enter the user's age to be used in this\n"
					"requirement evaluation. The valid range is 0 through 255.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Age", str, 3, K_NUMBER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "AGE ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;
			case 7:
				str[0] = 0;
				uifc.helpbuf =
					"`Required Gender:`\n"
					"\n"
					"You are being prompted to enter the user's gender to be used in this\n"
					"requirement evaluation.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Gender (e.g. M, F, X)", str, 1
				           , K_UPPER | K_ALPHA);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "SEX ");
				SAFECAT(ar, str);
				break;
			case 8:
				i = whichlogic();
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Post/Call Ratio:`\n"
					"\n"
					"You are being prompted to enter the post/call ratio to be used in this\n"
					"requirement evaluation (percentage). The valid range is 0 through 100.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Post/Call Ratio (percentage)"
				           , str, 3, K_NUMBER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "PCR ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;
			case 9:
				i = whichlogic();
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Number of Credits:`\n"
					"\n"
					"You are being prompted to enter the number of credits (in `kilobytes`) to\n"
					"be used in this requirement evaluation. The valid range is 0 through\n"
					"65535.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Required Credits", str, 5, K_NUMBER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "CREDIT ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;
			case 10:
				i = whichlogic();
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Upload/Download Byte Ratio:`\n"
					"\n"
					"You are being prompted to enter the upload/download ratio to be used in\n"
					"this requirement evaluation (percentage). The valid range is 0 through\n"
					"100. This ratio is based on the number of `bytes` uploaded by the user\n"
					"divided by the number of bytes downloaded.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Upload/Download Byte Ratio (percentage)"
				           , str, 3, K_NUMBER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "UDR ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;
			case 11:
				i = whichlogic();
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Upload/Download File Ratio:`\n"
					"\n"
					"You are being prompted to enter the upload/download ratio to be used in\n"
					"this requirement evaluation (percentage). The valid range is 0 through\n"
					"100. This ratio is based on the number of `files` uploaded by the user\n"
					"divided by the number of files downloaded.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0
				           , "Upload/Download File Ratio (percentage)"
				           , str, 3, K_NUMBER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "UDFR ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;
			case 12:
				i = 0;
				strcpy(opt[0], "Before");
				strcpy(opt[1], "After");
				opt[2][0] = 0;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Time Relationship", opt);
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Time of Day:`\n"
					"\n"
					"You are being prompted to enter the time of day to be used in this\n"
					"requirement evaluation (24 hour HH:MM format). The valid range is 0\n"
					"through 23:59.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Time of Day (HH:MM)", str, 5, K_UPPER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "TIME ");
				if (!i)
					SAFECAT(ar, "NOT ");
				SAFECAT(ar, str);
				break;
			case 13:
				i = whichlogic();
				if (i == -1)
					break;
				uifc.helpbuf =
					"`Required Day of Week:`\n"
					"\n"
					"You are being prompted to select a day of the week as an access\n"
					"requirement value.\n"
				;
				for (n = 0; n < 7; n++)
					strcpy(opt[n], wday[n]);
				opt[n][0] = 0;
				n = 0;
				n = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &n, 0, "Select Day of Week", opt);
				if (n == -1)
					break;
				strcpy(str, wday[n]);
				strupr(str);
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "DAY ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;
			case 14:
				i = whichlogic();
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required User Number:`\n"
					"\n"
					"You are being prompted to enter the user's number to be used in this\n"
					"requirement evaluation.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "User Number", str, 5, K_NUMBER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "USER ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;

			case 15:
				i = whichlogic();
				if (i == -1)
					break;
				str[0] = 0;
				uifc.helpbuf =
					"`Required Time Remaining:`\n"
					"\n"
					"You are being prompted to enter the time remaining to be used in this\n"
					"requirement evaluation (in `minutes`). The valid range is 0 through 255.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Time Remaining (minutes)"
				           , str, 3, K_NUMBER);
				if (!str[0])
					break;
				if (ar[0]) {
					j = whichcond();
					if (j == -1)
						break;
					if (!j)
						SAFECAT(ar, " AND ");
					else
						SAFECAT(ar, " OR ");
				}
				SAFECAT(ar, "TLEFT ");
				switch (i) {
					case 1:
						SAFECAT(ar, "= ");
						break;
					case 2:
						SAFECAT(ar, "NOT = ");
						break;
					case 3:
						SAFECAT(ar, "NOT ");
						break;
				}
				SAFECAT(ar, str);
				break;
		}
	}
	sprintf(inar, "%.*s", LEN_ARSTR, ar);
}

bool code_ok(char *str)
{

	if (*str == 0)
		return false;
	if (strcspn(str, " \\/|<>*?+[]:=\";,") != strlen(str))
		return false;
	return true;
}

char random_code_char(void)
{
	char ch = (char)xp_random(36);

	if (ch < 10)
		return '0' + ch;
	else
		return 'A' + (ch - 10);
}

#ifdef __BORLANDC__
	#pragma argsused
#endif
int lprintf(int level, char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	strip_ctrl(sbuf, sbuf);
	if (uifc.msg == NULL)
		puts(sbuf);
	else
		uifc.msg(sbuf);
	return 0;
}

void* new_item(void* lp, size_t size, int index, int* total)
{
	void** list = lp;
	void** p;
	void*  item;

	if ((item = calloc(size, 1)) == NULL)
		return NULL;
	if ((p = realloc(list, size * ((*total) + 1))) == NULL) {
		free(item);
		return NULL;
	}
	list = p;
	for (int i = *total; i > index; --i)
		list[i] = list[i - 1];
	list[index] = item;
	++(*total);
	return list;
}

void bail(int code)
{
	if (code) {
		printf("\nHit enter to continue...");
		(void)getchar();
	}
	else if (forcesave) {
		FILE*              fp;
		bool               run_bbs, run_ftp, run_web, run_mail, run_services;
		global_startup_t   global_startup = { .size  = sizeof global_startup };
		bbs_startup_t      bbs_startup = { .size = sizeof bbs_startup };
		web_startup_t      web_startup = { .size = sizeof web_startup };
		ftp_startup_t      ftp_startup = { .size = sizeof ftp_startup };
		mail_startup_t     mail_startup = { .size = sizeof mail_startup};
		services_startup_t services_startup = { .size = sizeof services_startup };

		uifc.pop("Force-saving configuration files...");
		load_main_cfg(&cfg, error, sizeof(error));
		load_msgs_cfg(&cfg, error, sizeof(error));
		load_file_cfg(&cfg, error, sizeof(error));
		load_chat_cfg(&cfg, error, sizeof(error));
		load_xtrn_cfg(&cfg, error, sizeof(error));
		cfg.new_install = new_install;
		save_main_cfg(&cfg);
		save_msgs_cfg(&cfg);
		save_file_cfg(&cfg);
		save_chat_cfg(&cfg);
		save_xtrn_cfg(&cfg);

		set_cfg_filename(hostname);
		if (*cfg.filename) {
			fp = iniOpenFile(cfg.filename, /* for_modify? */ true);
			if (fp == NULL)
				uifc.msgf("Error opening %s", cfg.filename);
			else {
				if (!sbbs_read_ini(
					fp
					, cfg.filename
					, &global_startup
					, &run_bbs
					, &bbs_startup
					, &run_ftp
					, &ftp_startup
					, &run_web
					, &web_startup
					, &run_mail
					, &mail_startup
					, &run_services
					, &services_startup
					))
					uifc.msgf("Internal error reading %s", cfg.filename);
				else
					if (!sbbs_write_ini(
						fp
						, &cfg
						, &global_startup
						, run_bbs
						, &bbs_startup
						, run_ftp
						, &ftp_startup
						, run_web
						, &web_startup
						, run_mail
						, &mail_startup
						, run_services
						, &services_startup
						))
					uifc.msgf("Error writing %s", cfg.filename);
				iniCloseFile(fp);
			}
		}
		uifc.pop(NULL);
	}

	uifc.pop("Exiting");
	uifc.bail();

	cryptEnd();

	for (int i = 0; i < (MAX_OPTS + 1); ++i)
		free(opt[i]);
	free(opt);

	exit(code);
}

/****************************************************************************/
/* Error handling routine. Prints to local and remote screens the error     */
/* information, function, action, object and access and then attempts to    */
/* write the error information into the file ERROR.LOG in the text dir.     */
/****************************************************************************/
void errormsg(int line, const char* function, const char *source, const char* action, const char *object, ulong access)
{
	char scrn_buf[MAX_BFLN];
	gettext(1, 1, 80, uifc.scrn_len, scrn_buf);
	clrscr();
	printf("ERROR -     line: %d\n", line);
	printf("        function: %s\n", function);
	printf("            file: %s\n", source);
	printf("          action: %s\n", action);
	printf("          object: %s\n", object);
	printf("          access: %ld (%lx)\n", access, access);
	printf("\nHit enter to continue...");
	(void)getchar();
	puttext(1, 1, 80, uifc.scrn_len, scrn_buf);
}

/* End of SCFG.C */
