/******************************************************************************
INIT.C       Initialization procedure.

    Copyright 1993 - 2000 Paul J. Sidorsky

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

This module contains the initialization and startup code for TOP.
******************************************************************************/

#include "top.h"

/* these are completely arbitrary and occasionally stupid ToDo */
#ifdef __unix__
#define MAXDRIVE	2
#define MAXEXT		16
#endif

/* DEBUG mode variable.  This isn't in GLOBAL.C because it mostly pertains
   to initialization. */
FILE *debuglog = NULL; /* Name of the debug log. */
/* DEBUG mode function prototypes. */
void wdl(char *msg);
void _USERENTRY closedebuglog(void);

/* init() - Initializes files, variables, and the door kit.
   Parameters:  argc - Number of command line arguments.
                argv - String array of command line arguments.
   Returns:  Nothing.
*/
void init(XINT argc, char *argv[])
{
/* Temporary node number, result code, task to perform. */
XINT tnod = -1, res, todo = 0;
unsigned char valcode[11]; /* Validation code for RegKey. */
char drive[MAXDRIVE]; /* Drive where TOP.EXE is located. */
char dir[MAX_PATH]; /* Directory where TOP.EXE is located. */
char file[MAX_PATH]; /* Base file name of TOP executable (usually TOP). */
char ext[MAXEXT]; /* Extension of TOP executable (usually .EXE) */
#if !defined(__OS2__) && !defined(__WIN32__) && !defined(__unix__)
unsigned char shchk; /* Share check flag. */
#endif

/* This function is quite long and could probably be broken up into
   subfunctions for managability. */

#if !defined(__OS2__) && !defined(__WIN32__) && !defined(__unix__)
/* Check for SHARE support under DOS. */
asm {
    mov ax,01000h
    int 0x2F
    mov shchk, al
    }

/* 0xFF is returned if SHARE is installed. */
if (shchk != 0xFF)
    {
    printf("\nSHARE compatibility not found!\r\nPlease check to make sure "
           "that SHARE.EXE is loaded or that your operating\nenvironment's "
           "SHARE compatibility is turned on.\n");
    exit(207);
    }
#endif

/* Check for extended command line parameters. */
if (argc >= 3)
    {
    /* Debug mode. */
    if (!stricmp(argv[2], "DEBUG"))
        {
        /* Open the debug log. */
        debuglog = fopen("topdebug.log", "at");
        atexit(closedebuglog);
        wdl("--------------------------------------------");
        }
#ifdef __WIN32__
    /* Comm handle under Win95. */
    if (!strnicmp(argv[2], "/HANDLE:", 8))
        {
        od_control.od_open_handle = strtol(&argv[2][8], NULL, 10);
        }
#endif
    }

#if !defined(__OS2__) && !defined(__WIN32__) && !defined(__unix__)
#ifdef FORTIFY
/* Enable Fortify memory checker under DOS in debug mode. */
if (debuglog == NULL)
	{
    Fortify_Disable();
    }
#endif
#endif

/* Initialize the random number generator.  Random numbers are currently
   not used, but were used by the poker and other game modules in past. */
#ifndef __unix__
randomize();
#endif

/* Initialize the polling timer. */
lastpoll = myclock();

/* Initialize the validation code for RegKey.  This is done on a character-
   by-character basis to make the code less obvious to potential hackers
   looking on from a hex editor. */
// * Deleted *

/* Initialize door kit registration information. */
#ifdef __OS2__
// * Deleted *
#else
// * Deleted *
#endif

/* Set program name in the door kit. */
#if defined(__OS2__)
sprintf(od_control.od_prog_name, "TOP/2 v%s", ver);
#elif defined(__WIN32__)
sprintf(od_control.od_prog_name, "TOP/95 v%s", ver);
/* Additional information for Win95 About dialog. */
strcpy(od_control.od_prog_copyright,
       "Copyright 1993 - 1998 ISMWare");
strcpy(od_control.od_prog_version,
       "32-bit Windows 95/NT Version");
#else
sprintf(od_control.od_prog_name, "TOP v%s", ver);
#endif

/* General door kit settings. */
od_control.od_nocopyright = TRUE;
od_control.od_always_clear = TRUE;
#ifdef __OS2__
od_control.od_no_messages = TRUE;
#endif

/* Door kit inbound comm buffer size.  The larger size makes TOP handle
   screen dumps from users (which can occur often in a chat door) better. */
od_control.od_in_buf_size = 8192;

/* Clear the user's record number. */
user_rec_num = -1;

/* Clear the node type flags. */
localmode = 0; lanmode = 0;

/* Door kit appearance. */
od_control.mps = NO_MPS;
od_control.default_personality = PER_RA;

wdl("First variable init completed");

#ifdef __OS2__
/* Handle additional OS/2 command line parameters. */
ProcessCmdLine(argc, argv);
#endif

/* Handle extra TOP tasks. */
if (argc >= 2)
    {
    /* Cheap-ass user deletor and security editor. */
    if (!stricmp(argv[1], "EDIT"))
        {
        todo = 1;
        tnod = 1;
        localmode = 1;
        }
    /* User file packer. */
    if (!stricmp(argv[1], "PACK"))
        {
        todo = 2;
        tnod = 1;
        localmode = 1;
        }
    }

/* Get the node number from the TASK envirionment variable. */
if (getenv("TASK") != NULL)
    {
    tnod = atoi(getenv("TASK"));
    }

/* Get the node number from the command line if it's there. */
if (argc >= 2 && !todo)
    {
    tnod = atoi(argv[1]);
    }

/* Set the final node number. */
if (tnod > 0)
    {
    od_control.od_node = tnod;
    }

/* Set the default node as 1 if none has been found or specified. */
if (od_control.od_node == 0)
    {
    od_control.od_node = 1;
    }

/* We need to force this setting early as it's used by one of the file
   opening routines. */
cfg.maxretries = 1;
//cfg.maxmsglen = 255;

wdl("Command line processing completed");

/* Initialize node configuration data and word splitter buffers.  The node
   configuration loader requires the node configuration data buffer
   (obviously) and the word splitter, so these get allocated before everything
   else.*/
nodecfg = malloc(sizeof(TOP_nodecfg_typ));
word_str = malloc(256);
word_pos = calloc(128, sizeof(XINT));
word_len = calloc(128, sizeof(XINT));
wordret = malloc(256);
if (nodecfg == NULL || word_str == NULL || word_pos == NULL ||
    word_len == NULL || wordret == NULL)
    {
    printf("\nNot enough memory to load Node Configuration!\n");
    exit(202);
    }

/* Find out where TOP.EXE is. */
#ifdef __unix__
_splitpath(argv[0], drive, dir, file, ext);
#else
fnsplit(argv[0], drive, dir, file, ext);
#endif
sprintf(word_str, "%s%s", drive, dir);
verifypath(word_str);

/* Load the node configuration.  This is done first because a lot of
   information from the node configuration is needed to initialize the
   door kit. */
if (!loadnodecfg(word_str))
    {
    printf("\nCan't load Node Configuration for node %i!\n",
           od_control.od_node);
    exit(100);
    }

/* Kludge forces for local and lan nodes.  As the note suggests this should
   be handled more eloquently. */
if (nodecfg->type == NODE_LOCAL) // Won't eventually need this....
    {
    localmode = 1;
    }
if (nodecfg->type == NODE_LAN)
    {
    lanmode = 1;
    }

wdl("Node configuration read properly");

/* Under LAN or Local modes, there is no need for a drop file, carrier
   detection, or time limit. */
if (localmode || lanmode)
    {
    od_control.od_disable = DIS_INFOFILE | DIS_CARRIERDETECT | DIS_TIMEOUT;
    }

/* If one is configured, set the drop file path.  This can include the
   filename under OpenDoors specifications although it usually doesn't. */
if (nodecfg->dropfilepath[0])
    {
    strcpy(od_control.info_path, nodecfg->dropfilepath);
    }

/* Setup the configuration file processor.  Although the built-in OpenDoors
   configuration options aren't even mentioned, this seemed like the best
   place to handle the configuration information since the processing is
   built into OD. */
od_control.od_config_file = INCLUDE_CONFIG_FILE;
od_control.od_config_filename = nodecfg->cfgfile;
od_control.od_config_function = processcfg;

/* Copy communication information from the node configuration to OD. */
od_control.od_com_method = nodecfg->fossil ? COM_FOSSIL : COM_INTERNAL;
od_control.port = nodecfg->port - 1;
od_control.baud = nodecfg->speed;
od_control.od_com_address = nodecfg->portaddress;
od_control.od_com_irq = nodecfg->portirq;
od_control.od_com_no_fifo = !nodecfg->usefifo;
od_control.od_com_fifo_trigger = nodecfg->fifotrigger;
od_control.od_com_rx_buf = nodecfg->rxbufsize;
od_control.od_com_tx_buf = nodecfg->txbufsize;

wdl("OpenDoors pre-init completed");

/* Fire up OpenDoors. */
od_init();

/* Initialize the kernel timer and our custom kernel function. */
time(&lastkertime);
od_control.od_ker_exec = top_kernel;

/* Initialize a temporary user name and location under local/LAN modes so
   the status line doesn't look terrible. */
if (localmode || lanmode)
    {
    strcpy(od_control.user_name, "Unknown User");
    strcpy(od_control.user_location, "Unknown");
    }

/* Initlialize the log file.  OpenDoors' built-in logging is used. */
strcpy(od_control.od_logfile_name, nodecfg->logfile);
od_log_open();

wdl("OpenDoors init completed");

/* Initialize some door kit variables. */
od_control.od_inactivity = cfg.inactivetime;
od_control.od_clear_on_exit = FALSE;

/* For some reason long ago I thought memory handlers were needed before
   shelling to DOS and that swapping may have been crashing it.  This
   was later discovered to not only be false, but that the handlers were
   actually preventing shelling from working themselves. */
/*od_control.od_cbefore_shell = preshell;
od_control.od_cafter_shell = postshell;
od_control.od_swapping_disable = TRUE;*/

/* Override OpenDoors' rather unusual (IMO) default errorlevels.  I don't
   know many other programs (and especially doors) that return 10 on a
   normal exit! */
od_control.od_errorlevel[0] = TRUE;   /* Turn on custom errorlevels */
od_control.od_errorlevel[1] = 255;    /* Critical */
od_control.od_errorlevel[2] = 1;      /* DCD loss */
od_control.od_errorlevel[3] = 5;      /* Alt-H    */
od_control.od_errorlevel[4] = 2;      /* Time Up  */
od_control.od_errorlevel[5] = 3;      /* Inactive */
od_control.od_errorlevel[6] = 4;      /* Alt-D    */
od_control.od_errorlevel[7] = 0;      /* Normal   */

/* In case OpenDoors tried to reset the node number, we want to force it
   back to the one the user told us to use, if any. */
if (tnod > 0)
    {
    od_control.od_node = tnod;
    }
if (od_control.od_node == 0)
    {
    od_control.od_node = 1;
    }

/* Adjust some settings that we don't really need in LAN/local modes. */
if (localmode || lanmode)
    {
    /* No inactivity timer. */
    od_control.od_inactivity = 0;
    /* Since timeout is disabled the time remaining doesn't matter, but
       this makes it look like the user has a lot of time left. */
    od_control.user_timelimit = 1440;
    /* Force an update of the status line. */
    od_kernel();
    /* Turn the status line updating off. */
    od_control.od_status_on = FALSE;
    }

/* Initialize RA credit usage if enabled. */
if (cfg.usecredits)
    {
    orignumcreds = od_control.user_credit;
    }

wdl("OpenDoors post-init completed");

/* Initialize BBS interface functions if enabled. */
switch(cfg.bbstype)
    {
    case BBS_RA2: ra_init(); break;
    case BBS_MAX2: max_init(); break;
#ifdef __OS2__
    case BBS_MAX3: max_mcpinit(); break;
#endif
    case BBS_SBBS11: sbbs_init(); break;
    }

wdl("BBS function init completed");

/* Perform the memory allocations.  This is one of the few initialization
   tasks that is sub-functioned, though this was originally done because
   of the shell memory handlers described above.  Sometimes good things
   result from stupid blunders. */
res = memalloc();
/* Now that the memory has been allocated, we need the exit handler. */
od_control.od_before_exit = before_exit;
/* Abort if not enough memory. */
if (res)
    {
    printf("\nOut Of Memory!\n");
    od_log_write(top_output(OUT_STRING, getlang("LogOutOfMemory")));
    od_exit(200, FALSE);
    }

wdl("Memory allocation completed");

/* Open the files.  Again, this was sub-functioned because of the shell
   handlers. */
openfiles();

wdl("Files opened");

/* Clear all node statuses. */
memset(activenodes, 0, MAXNODES);

wdl("Memory init completed");

/* Load the language file.  The name is taken from the configuration
   information. */
if (!loadlang())
    {
    printf("\nNot enough memory to load language file!\n");
    od_log_write(top_output(OUT_STRING, getlang("LogCantLoadLang")));
    od_exit(203, FALSE);
    }

wdl("Language file loaded");

/* Load CHANNELS.CFG. */
if (!loadchan())
    {
    printf("\nCan't load channel definition list!\n");
    od_log_write(top_output(OUT_STRING, getlang("LogCantLoadChan")));
    od_exit(204, FALSE);
    }

wdl("Channel file loaded");

if (lanmode)
    {
    /* Counter, free node holder.  The choice of xz as a variable name is
       probably left over from my BASIC years, when unintelligible variable
       names were the norm. */
    XINT d, xz = -1;

    /* This section is the whole reason LAN mode exists.  It finds a free
       node if the one requested isn't available. */

    /* See which nodes are using TOP. */
    check_nodes_used(FALSE);
    /* Loop for each remaining node using the specified node as a
       starting point. */
    for (d = od_control.od_node + 1; d < MAXNODES && xz == -1; d++)
        {
        if (!activenodes[d])
            {
            /* Found a free node. */
            xz = d;
            }
        }
    if (xz == -1)
        {
        /* Didn't find a free node. */
        top_output(OUT_SCREEN, getlang("NoFreeNode"));
        top_output(OUT_SCREEN, getlang("ContactSuper"));
        od_log_write(top_output(OUT_STRING, getlang("LogNoFreeNode")));
        od_exit(101, FALSE);
        }
    /* Reset the node and reopen any node-specific files. */
    od_control.od_node = xz;
    lan_updateopenfiles();

wdl("LAN init completed");

    }
else wdl("LAN init not needed");

#ifndef __unix__
if (localmode || lanmode)
    {
    struct text_info ti; /* Screen mode information. */

    gettextinfo(&ti);
    if (ti.currmode == BW40 || ti.currmode == BW80)
        {
        /* If a monochrome screen mode (not necessarily a mono monitor) is
           detected, turn off ANSI since it isn't really needed. */
        od_control.user_ansi = FALSE;
        }
    else
        {
        /* Force ANSI on a colour screen.  Given that there is no worry
           of transmission time in local/LAN mode, it seems logical to
           turn it on. */
        od_control.user_ansi = TRUE;
        }
    /* Local RIP isn't supported by the door kit. */
    od_control.user_rip = FALSE;

wdl("Local init completed");

    }
else 
#endif
	wdl("Local init not needed");

/* Perform some final appearance work. */
od_set_personality("RemoteAccess");
trim_string(od_control.user_location, od_control.user_location, 0);
od_kernel();

wdl("OD Log init completed");

/* We don't use OD's own bulky colour strings so turn it off. */
od_control.od_colour_delimiter = '\0';

/* Reset the screen attribute. */
od_set_colour(D_GREY, D_BLACK);

/* Clear the MIX file info. */
memset(&midxfileinfo, 0, sizeof(struct file_stats_str));

/* Log the node type. */
if (localmode)
    {
    od_log_write(top_output(OUT_STRING, getlang("LogLocal")));
    }
if (lanmode)
    {
    od_log_write(top_output(OUT_STRING, getlang("LogLAN")));
    }
if (!localmode && !lanmode)
    {
    od_log_write(top_output(OUT_STRING, getlang("LogDoor")));
    }

wdl("Secondary memory init completed");

/* Execute a selected task. */
if (todo > 0)
    {
wdl("Init completed - initiating command-line-selected task");
    od_log_write(top_output(OUT_STRING, getlang("LogOneRunMode")));
    switch(todo)
        {
        /* TOP EDIT. */
        case 1: useredit(); break;
        /* TOP PACK. */
        case 2: if (argc > 3) strcpy(outbuf, argv[3]); else outbuf[0] = 0;
                if (argc > 2) strcpy(word_str, argv[2]); else word_str[0] = 0;
                userpack();
                break;
        }
    }

/* Clear all forget statuses. */
memset(forgetstatus, 0, MAXNODES);

/* Unused Poker initialization code. */
/*//|for (res = 0; res < cfg.pokermaxgames; res++)
	{
    pokerlockflags[res] = 0;
    pokeretimes[res] = 0xFFFFFFFFUL;
    pokerdtimes[res] = 0xFFFF;
    pokerwtimes[res] = 0xFFFF;
    pokeringame[res] = 0;
    }*///|

/* Initialize the channel number. */
curchannel = cfg.defaultchannel;

wdl("Tertiary memory init completed");

/* Initialize the number of action files, which is done by counting the
   words in the action file string.  todo is used in the loop because I was
   too lazy to type the entire variable name. */
numactfiles = todo = split_string(cfg.actfilestr);
// Errs from hell
/* Allocate space for all of the filenames. */
cfg.actfilenames = calloc(todo + 1, sizeof(unsigned char XFAR *));
/* Loop for each file. */
for (res = 0; res <= todo; res++)
	{
    /* Initialize the filename buffer. */
    cfg.actfilenames[res] = malloc(13);
    memset(cfg.actfilenames[res], 0, 13);
    if (res == 0)
        {
        /* List 0 is the personal action list, so copy the name from the
           language file. */
        strcpy(cfg.actfilenames[res],
               /* strupr(top_output(OUT_STRING, getlang("PersActListName")))); */
			   top_output(OUT_STRING, getlang("PersActListName")));
        }
    else
        {
        strncpy(cfg.actfilenames[res], get_word(res - 1), 8);
        }
    /* strupr(cfg.actfilenames[res]); */
	cfg.actfilenames[res];
    }

wdl("Pre-action init completed");

/* Load the action files.  Not sure why the note is there to fix the
   check but it could be because of the partially-allocated status that
   can occur if it fails. */
if (!loadactions()) // Fix this check later.
    {
    od_log_write(top_output(OUT_STRING, getlang("LogCantLoadActs")));
    od_exit(205, FALSE);
    }

wdl("Actions loaded");

/* Load CENSOR.CFG. */
if (!load_censor_words())
    {
    od_log_write(top_output(OUT_STRING, getlang("LogCantLoadCWords")));
    }

wdl("Censor Words Loaded");

/* Load spawnable programs, which was a feature removed from TOP due to
   instability. */
//load_spawn_progs(); // check return value later

/* Load BIOQUES.CFG. */
if (!loadbioquestions())
    {
    od_log_write(top_output(OUT_STRING, getlang("LogCantLoadBioQues")));
    }

/* Copy predefined BBS status types into a string array, which was how they
   were handled before the language file existed. */
for (res = 0; res < 11; res++)
    {
    sprintf(outbuf, "SBBSStatusType%i", res);
    sbbs_statustypes[res] = getlang(outbuf);
    if (res < 8)
        {
        sprintf(outbuf, "RAStatusType%i", res);
        ra_statustypes[res] = getlang(outbuf);
        }
    }

wdl("BBS Status Strings Initialized");

wdl("Initialization completed");

/* Phew!  All done! */
return;
}

/* openfiles() - Opens all major files used by TOP.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  Aborts on its own if an error occurs.
*/
void openfiles(void)
{
unsigned char tnam[512]; /* Temporary filename buffer. */
XINT res = 0; /* Result code. */

/* Open all files that TOP needs to keep open while it runs. */

/* User file. */
sprintf(tnam, "%susers.top", cfg.toppath);
userfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                  S_IREAD | S_IWRITE);
/* res holds the number of errors that occurred, which is not used right
   now but could be reported for debugging purposes. */
res += (userfil == -1);
/* Node index. */
sprintf(tnam, "%snodeidx.tch", cfg.topworkpath);
nidxfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                  S_IREAD | S_IWRITE);
res += (nidxfil == -1);
if (nidxfil != -1 && filelength(nidxfil) <
    (long) ((long) od_control.od_node + 1L) * (long) sizeof(node_idx_typ))
    {
    /* Adjust the size of the node index if it is too small. */
    chsize(nidxfil, (long) ((long) od_control.od_node + 1L) *
                   (long) sizeof(node_idx_typ));
    }
/* Active node index. */
sprintf(tnam, "%snodeidx2.tch", cfg.topworkpath);
nidx2fil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                   S_IREAD | S_IWRITE);
res += (nidx2fil == -1);
/* Message file for this node.  (Incoming messages.) */
sprintf(tnam, "%smsg%05i.tch", cfg.topworkpath, od_control.od_node);
msginfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                   S_IREAD | S_IWRITE);
res += (msginfil == -1);
/* Open BBS-specific files if needed. */
if (cfg.bbstype != BBS_UNKNOWN && bbs_call_openfiles)
    {
    res += (*bbs_call_openfiles)();
    }
/* Message index for this node.  (Incoming messages.) */
sprintf(tnam, "%smix%05i.tch", cfg.topworkpath, od_control.od_node);
midxinfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                    S_IREAD | S_IWRITE);
res += (midxinfil == -1);
/* Message change index. */
sprintf(tnam, "%schgidx.tch", cfg.topworkpath);
chgfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                 S_IREAD | S_IWRITE);
res += (chgfil == -1);
/* Adjust the size of the change index if it is too small. */
if (chgfil != -1)
    {
    chsize(chgfil, MAXNODES);
    }
/* Channel data (CMI) index. */
sprintf(tnam, "%schanidx.tch", cfg.topworkpath);
chidxfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                 S_IREAD | S_IWRITE);
res += (chidxfil == -1);
/* Games data files, not used as the games were removed. */
sprintf(tnam, "%sslotstat.top", cfg.toppath);
slotsfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                   S_IREAD | S_IWRITE);
res += (slotsfil == -1);
if (slotsfil != -1)
    {
    chsize(slotsfil, sizeof(slots_stat_typ));
    }
sprintf(tnam, "%smatchstat.top", cfg.toppath);
matchfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                   S_IREAD | S_IWRITE);
res += (slotsfil == -1);
/*//|sprintf(tnam, "%spokerdat.tch", cfg.toppath);
pokdatafil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                     S_IREAD | S_IWRITE);
res += (pokdatafil == -1);*///|
/* Biography response index. */
sprintf(tnam, "%sbigidx.top", cfg.toppath);
bioidxfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                   S_IREAD | S_IWRITE);
res += (bioidxfil == -1);
/* Adjust the size of the index if it is too small. */
if (bioidxfil != -1 &&
    filelength(bioidxfil) < (filelength(userfil) / sizeof(user_data_typ)) *
                            MAXBIOQUES * sizeof(long))
    {
    chsize(bioidxfil, (filelength(userfil) / sizeof(user_data_typ)) *
                       MAXBIOQUES * sizeof(long));
    }
/* Biography responses. */
sprintf(tnam, "%sbioresp.top", cfg.toppath);
biorespfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO,
                                    S_IREAD | S_IWRITE);
res += (biorespfil == -1);

/* Abort on error. */
if (res)
    {
    top_output(OUT_SCREEN, getlang("CantInitFiles"));
    od_exit(201, FALSE);
    }

return;
}

/* lan_updateopenfiles() - Reopen node-dependant files in LAN mode.
   Parameters:  None.
   Returns:  Nothing.
   Notes:  Aborts on its own if an error occurs.
*/
void lan_updateopenfiles(void)
{
unsigned char tnam[512]; /* Temporary filename buffer. */
XINT res = 0; /* Result code. */

/* This function opens files that use the node number in LAN mode, after
   TOP has found a free node number. */

/* Close the previously open files that are affected. */
close(msginfil); close(ipcinfil); close(midxinfil);

/* Adjust the size of the node index for the new node if it is too small. */
if (filelength(nidxfil) <
    (long) ((long) od_control.od_node + 1L) * (long) sizeof(node_idx_typ))
    {
    chsize(nidxfil, (long) ((long) od_control.od_node + 1L) *
                   (long) sizeof(node_idx_typ));
    }

/* Incoming messages. */
sprintf(tnam, "%smsg%05i.tch", cfg.topworkpath, od_control.od_node);
msginfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO, S_IREAD | S_IWRITE);
res += (msginfil == -1);
/* Reopen any BBS-specific files that need the node number. */
if (cfg.bbstype != BBS_UNKNOWN && bbs_call_updateopenfiles)
    {
    res += (*bbs_call_updateopenfiles)();
    }
/* Incoming message index. */
sprintf(tnam, "%smix%05i.tch", cfg.topworkpath, od_control.od_node);
midxinfil = sh_open(tnam, O_RDWR | O_CREAT | O_BINARY, SH_DENYNO, S_IREAD | S_IWRITE);
res += (midxinfil == -1);

/* Abort on error. */
if (res)
    {
    top_output(OUT_SCREEN, getlang("CantInitFiles"));
    od_exit(201, FALSE);
    }

return;
}

/* memalloc() - Allocate most of the memory TOP uses.
   Parameters:  None.
   Returns:  TRUE on error, FALSE if successful.
*/
char memalloc(void)
{
char mres = 0; /* Result code. */

/* See GLOBAL.C for variable descriptions.  TOP allocates all memory even
   on error, for simplicity.  The |= assignment used allows TOP to
   preserve any previously detected error while continuing the allocation. */

/* Remember to change before_exit() when adding new pointers! */
mres |= ((handles = calloc(MAXNODES, 31)) == NULL);
mres |= ((activenodes = malloc(MAXNODES)) == NULL);
mres |= ((outbuf = malloc(512)) == NULL);
mres |= ((forgetstatus = malloc(MAXNODES)) == NULL);
mres |= ((busynodes = malloc(MAXNODES)) == NULL);
mres |= ((node = malloc(sizeof(node_idx_typ))) == NULL);
mres |= ((outputstr = malloc(513)) == NULL);
mres |= ((lastcmd = malloc(256)) == NULL);
mres |= ((chan = calloc(cfg.maxchandefs, sizeof(channel_data_typ))) ==
         NULL);
mres |= ((bioques = calloc(MAXBIOQUES, sizeof(bio_ques_typ))) == NULL);
/*//|mres |= ((pokerlockflags = malloc(cfg.pokermaxgames)) == NULL);
mres |= ((pokeretimes = calloc(cfg.pokermaxgames, sizeof(time_t))) ==
         NULL);
mres |= ((pokerdtimes = calloc(cfg.pokermaxgames, sizeof(XINT))) == NULL);
mres |= ((pokerwtimes = calloc(cfg.pokermaxgames, sizeof(XINT))) == NULL);
mres |= ((pokeringame = malloc(cfg.pokermaxgames)) == NULL);*///|

return mres;
}

/* wdl() - Write a message to the DEBUG log.
   Parameters:  wdl - String to write to the log.
   Returns:  Nothing.
*/
void wdl(char *msg)
    {

    /* Only write if we're in DEBUG mode, i.e. if the debug log has been
       opened. */
    if (debuglog != NULL)
        {
        fprintf(debuglog, "%s\n", msg);
        }

    }

/* closedebuglog() - Closes the TOP DEBUG log.
   Parameters:  None.
   Returns:  Nothing.
*/
void _USERENTRY closedebuglog(void)
    {

    /* Close the debug log. */
    fclose(debuglog);

    }


