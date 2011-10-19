
/*  Copyright 1993 - 2000 Paul J. Sidorsky

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
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <OpenDoor.h>

#include <dirwrap.h>
#include <datewrap.h>
#include <genwrap.h>
#include <filewrap.h>

#ifdef __OS2__
#include "reginit.h"
#define wherex() od_wherex()
#define wherey() od_wherey()
#undef random
#define random(xxx) od_random(xxx)
#define myclock() clock()
#endif

#if defined(__OS2__) || defined(__WIN32__) || defined(__unix__)
#define XFAR
#define XINT short int
typedef struct _date
    {
    XINT          da_year;        /* Year - 1980 */
    char          da_day;     /* Day of the month */
    char          da_mon;     /* Month (1 = Jan) */
    } XDATE;
#else
#define XFAR far
#define XINT int
#define LIBENTRY
#define LIBDATA
typedef struct date XDATE;
#endif

#if !defined(__OS2__) && !defined(__WIN32__)
#define _USERENTRY
#endif

#ifdef __OS2__
#include "mcp.h"
#endif

#include "max2ipc.h"
#include "topcfg.h"

#if !defined(__OS2__) && !defined(__WIN32__) && !defined(__unix__)
#define FORTIFY
#include "\bc45\fort\fortify.h"
#endif

#define oddnum(nnnn) ((nnnn % 2) == 1)
#define evennum(nnnn) (((nnnn % 2) == 0) || (nnnn == 0))
#define get_word_char(wordnum, charpos) (word_str[word_pos[wordnum] + charpos])

#define verifypath(vpath) backslash(vpath)

#define stripcr(vvvv) if (vvvv[strlen(vvvv) - 1] == 10) vvvv[strlen(vvvv) - 1] = 0

#define MAXNODES       ((long) cfg.maxnodes)
#define MAXSTRLEN      255L
#define MAXWORDS       (XINT) (MAXSTRLEN / 2)
#define MAXACTIONS     ((long) cfg.maxactions)
#define MAXRETRIES     ((long) cfg.maxretries)
#define MAXPWTRIES     ((long) cfg.maxpwtries)
#define RETRYINTERVAL  ((float) cfg.retrydelay / 10.0)
#define MAXOPENRETRIES 1024L
#define POLLTIMEDELAY  ((float) cfg.polldelay / 10.0)
#define MAXCHANSPECNODES 100
#define MAXBIOQUES	   100L
#define BUSYCHANNEL    0xFFFFFFFFUL
#define MAXCHANNEL     0xFFFFFFFEUL
#define MAXASCII       (cfg.allowhighascii ? 254 : 127)

#define MSG_NULL               0  /* Null message (does nothing). */
#define MSG_TEXT               1  /* Regular chat text. */
#define MSG_ENTRY              2  /* User has entered the pub. */
#define MSG_EXIT               3  /* User has left the pub. */
#define MSG_INEDITOR           4  /* User has entered the profile editor. */
#define MSG_OUTEDITOR          5  /* User has left the profile editor. */
#define MSG_WHISPER            6  /* Private message to you. */
#define MSG_GA                 7  /* General action. */
#define MSG_GA2                8  /* Posessive general action. */
#define MSG_ACTIONSIN          9  /* Singular normal action. */
#define MSG_ACTIONPLU         10  /* Plural normal action. */
#define MSG_TLKTYPSIN         11  /* Singular talktype action. */
#define MSG_TLKTYPPLU         12  /* Plural talktype action (AKA TT msg). */
#define MSG_ACTPLUSEC         13  /* Secret action done to you. */
#define MSG_HANDLECHG         14  /* User has changed his/her handle. */
#define MSG_BIOCHG            15  /* User has changed his/her Biography. */
#define MSG_SEXCHG            16  /* User has changed genders. */
#define MSG_ACTIONCHG         17  /* Reserved. (Usr edit pub act.) */
#define MSG_PERACTCHG         18  /* User has changed personal actions. */
#define MSG_TOSSOUT           19  /* User has abruptly left the pub. */
#define MSG_FORGET            20  /* User has forgotten you. */
#define MSG_REMEMBER          21  /* User has remembered you. */
#define MSG_LINKUP            22  /* TOPLink has been initiated. */
#define MSG_LINKTEXT          23  /* TOPLink chat text. */
#define MSG_UNLINK            24  /* TOPLink has been cut. */
#define MSG_EXMSGCHG          25  /* User has changed entry/exit messages. */
#define MSG_TOSSED            26  /* Sysop has tossed you back to BBS. */
#define MSG_ZAPPED            27  /* Sysop has zapped you off system. */
#define MSG_DELETED           28  /* Reserved. (Sys deleted you.) */
#define MSG_CLEARNODE         29  /* Sysop cleared a node. */
#define MSG_SYSGIVECD         30  /* Reserved. (Sys gave you CDs.) */
#define MSG_SYSTAKECD         31  /* Reserved. (Sys took CDs from you.) */
#define MSG_SYSSETCD          32  /* Reserved. (Sys changed your CDs.) */
#define MSG_TRANSCD           33  /* Reserved. (Usr trans'd CDs to you.) */
#define MSG_EXTCOMMAND        34  /* Reserved. (Usr spawned ext prog. */
#define MSG_GENERIC           35  /* Sent as is (with code processing). */
#define MSG_INCHANNEL         36  /* User has entered your channel. */
#define MSG_OUTCHANNEL        37  /* User has left your channel. */
#define MSG_KILLCRASH         38  /* Node cleared automatically by TOP. */
#define MSG_DIRECTED          39  /* Directed message. */
#define MSG_SYSINEDITOR       40  /* Reserved. (Sys enter syst editor.) */
#define MSG_SYSOUTEDITOR      41  /* Reserved. (Sys left syst editor.) */
#define MSG_SYSUSEREDITED     42  /* Reserved. (Sys edit you.) */
#define MSG_SYSCFGEDITED      43  /* Reserved. (Sys edit syst config.) */
#define MSG_SYSLANGEDITED     44  /* Reserved. (Sys edit lang file.) */
#define MSG_SYSSETSEC         45  /* Sysop has changed your security. */
#define MSG_INPRIVCHAT        46  /* User entered private chat. */
#define MSG_OUTPRIVCHAT       47  /* User returned from private chat. */
#define MSG_WANTSPRIVCHAT     48  /* User wants private chat with you. */
#define MSG_DENYPRIVCHAT      49  /* User has denied your chat request. */
#define MSG_CHANTOPICCHG      50  /* Channel's topic has been changed. */
#define MSG_CHANBANNED        51  /* User was banned from a channel. */
#define MSG_CHANUNBANNED      52  /* User's ban from channel was lifted. */
#define MSG_CHANINVITED       53  /* User has been invited to a channel. */
#define MSG_CHANUNINVITED     54  /* User's invite to channel cancelled. */
#define MSG_CHANMODCHG        55  /* User appointed new channel moderator. */
#define MSG_FORCERESCAN       56  /* Forces rescan of NODEIDX* files. */
#define MSG_INBIOEDITOR       57  /* User entered Biography editor. */
#define MSG_OUTBIOEDITOR      58  /* User left Biography editor. */

#define ALLOW_ME            0x01
#define ALLOW_YOU           0x02
#define ALLOW_SEX           0x04
#define ALLOW_POS           0x08
#define ALLOW_SLF           0x10
#define ALLOW_HESHE         0x20

#define ERR_CANTOPEN           0
#define ERR_CANTREAD           1
#define ERR_CANTWRITE          2
#define ERR_CANTACCESS         3
#define ERR_CANTCLOSE          4

#define FGT_FORGOTTEN       0x01
#define FGT_HASFORGOTME     0x02

#define NO_LTRIM            0x01
#define NO_RTRIM            0x02

#define TASK_NONE           0x0000
#define TASK_EDITING        0x0001
#define TASK_AFK            0x0002
#define TASK_QUIET          0x0004
#define TASK_SYSCHAT        0x0008
#define TASK_SHELL          0x0010
#define TASK_TYPING         0x0020
#define TASK_PRIVATECHAT    0x0040
#define TASK_WINDOWEDCHAT   0x0080
#define TASK_SYSOPEDITING   0x0100

#define PREF1_ECHOACTIONS   0x01
#define PREF1_ECHOTALKTYP   0x02
#define PREF1_ECHOTALKMSG   0x04
#define PREF1_MSGSENT       0x08
#define PREF1_ECHOGA        0x10
#define PREF1_TALKMSGSENT   0x20
#define PREF1_DUALWINDOW    0x40
#define PREF1_BLKWHILETYP   0x80

#define PREF2_SINGLECHAR    0x01
#define PREF2_CHANLISTED    0x02
#define PREF2_ECHOOWNTEXT   0x04
#define PREF2_ACTIONSOFF    0x08

#define REC_LOCK               0
#define REC_UNLOCK             1

#define OUT_SCREEN             0
#define OUT_LOCAL              1
#define OUT_STRING             2
#define OUT_STRINGNF           3    /* String, no filtering. */
#define OUT_EMULATE            4    /* Same as mode 0 but od_emulate(). */

#define SCRN_NONE           0x00
#define SCRN_CLS            0x01
#define SCRN_RESETCOL       0x02
#define SCRN_RESET          0x03
#define SCRN_WAITKEY        0x04
#define SCRN_ALL            0xFF

#define NEX_HANDLE        0x0001
#define NEX_REALNAME      0x0002
#define NEX_NODE          0x0004
#define NEX_SPEED         0x0008
#define NEX_LOCATION      0x0010
#define NEX_STATUS        0x0020
#define NEX_PAGESTAT      0x0040
#define NEX_GENDER        0x0080
#define NEX_NUMCALLS      0x0100

#define EINP_NOFIELD        0x00
#define EINP_STRING         0x01
#define EINP_HEX            0x02
#define EINP_NUMBER         0x04
#define EINP_NUMBERCHAR     0x0C
#define EINP_NUMBERINT      0x14
#define EINP_NUMBERLONG     0x1C
#define EINP_UNSIGNED       0x80
#define EINP_NUMBERBYTE     0x8C
#define EINP_NUMBERWORD     0x94
#define EINP_NUMBERDWORD    0x9C

#define ACT_NORMAL             0
#define ACT_TALKTYPE           1

#define BEEP_NONE           0x00
#define BEEP_REMOTE         0x01
#define BEEP_LOCAL          0x02
#define BEEP_LAN            0x04
#define BEEP_ALL            0xFF

#define CHAN_DELETED           0
#define CHAN_OPEN              1
#define CHAN_CLOSED            2

#define CMI_OKAY               0
#define CMIERR_BANNED          1
#define CMIERR_NOINV           2
#define CMIERR_NOTFOUND        3
#define CMIERR_FULL            4
#define CMIERR_FILEIO        255

#define BIORESP_NUMERIC		   0
#define BIORESP_STRING		   1

#define UFIND_USECFG           0
#define UFIND_REALNAME         1
#define UFIND_HANDLE           2

typedef struct stringarray1_str
	{
    unsigned char XFAR string[11];
    } stringarray1;

typedef struct stringarray2_str
	{
    unsigned char XFAR string[31];
    } stringarray2;

typedef struct user_data_str
	{
    unsigned char realname[41];
    unsigned char handle[31];
    unsigned char unusedspace[61];
    struct pers_act_typ
    	{
        XINT          type;
        unsigned char verb[11];
	    unsigned char response[61];
    	unsigned char singular[61];
	    unsigned char plural[61];
        } persact[4];
    XDATE         last_use;
    XINT          gender;
    char          datefmt;
    char          timefmt;
    unsigned char pref1;
    unsigned char pref2;
    unsigned char pref3;
    unsigned char password[16];
    unsigned char age;
    unsigned char emessage[81];
    unsigned char xmessage[81];
    unsigned long cybercash;
    unsigned XINT security;
    unsigned char defchantopic[71];
    char          donebio;
    char reserved[2048 - 1185];
    } user_data_typ;

typedef struct node_idx_str
	{
    unsigned XINT structlength;
    unsigned char handle[31];
    unsigned char realname[41];
    unsigned XINT speed;
    unsigned char location[31];
    XINT          gender;
    char          quiet;
    unsigned XINT task;
    time_t        lastaccess;
    unsigned long channel;
    char          channellisted;
    unsigned XINT security;
    char          actions;
    } node_idx_typ;

typedef struct msg_str
	{
    unsigned XINT structlength;
    XINT          type;
    XINT          from;
    XINT          doneto;
    XINT          gender;
    unsigned char handle[31];
    unsigned char string[256]; /////////////Change To Dynamic!
    unsigned long channel;
    unsigned XINT minsec;  /* Minimum Security to recv msg. */
	unsigned XINT maxsec;  /* Maximum Security to recv msg. */
    long          data1;
    } msg_typ;

typedef struct slots_stat_str
	{
    unsigned long pulls;
    unsigned long spent;
    unsigned long hits[10];
    unsigned long totals[10];
    } slots_stat_typ;

typedef struct lang_text_str
    {
    unsigned char idtag[31];
    XINT          length;
    unsigned char XFAR *string; /* Points to a malloc()ed buffer in memory */
    } lang_text_typ;

typedef struct bbsnodedata_str
	{
    unsigned char handle[31];
    unsigned char realname[41];
    XINT          node;
    long          speed;
    unsigned char location[31];
    unsigned char statdesc[81];
    char          quiet;
    char          gender;
    char          hidden;
    unsigned XINT numcalls;
    unsigned char infobyte;

    unsigned char attribs1;
    unsigned XINT existbits;
	} bbsnodedata_typ;

typedef struct editor_field_str
	{
    char enabled;
    unsigned char inptype;
    char noinput;
    void *varbuffer;
    unsigned char *title;
    unsigned char *fmtstring;
    XINT titlerow, titlecol;
    XINT fieldrow, fieldcol;
    unsigned char titleattrib;
    unsigned char tihiltattrib;
    unsigned char tidisattrib;
    unsigned char normalattrib;
    unsigned char highltattrib;
    unsigned char bgchar;
    unsigned XINT flags;
    long minval, maxval;
    XINT minlen, maxlen;
    char (*verifyfunc)(void *fieldinf);
    void (*noinputfunc)(unsigned char *buffer, void *fieldinf);
    } editor_field_typ;

typedef struct channel_data_str
    { // This is too memory consuming - strings need to be dynamic.
    unsigned long channel;
    unsigned char topic[71];
    unsigned char joinaliases[31];
    unsigned XINT minsec;
    unsigned XINT maxsec;
    unsigned XINT modsec;
    char          listed;
    } channel_data_typ;

typedef struct action_file_str
    {
    unsigned long datstart; /* Location of first action's data. */
    unsigned long txtstart; /* Location of first action's text. */
    unsigned char name[31];
    unsigned XINT minsec;
    unsigned XINT maxsec;
    unsigned long minchannel;
    unsigned long maxchannel;
    unsigned long numactions;
    } action_file_typ;

typedef struct action_data_str
    {
    char          type;   /* -1 = deleted */
    unsigned char verb[11];
    unsigned long textofs; /* From txtstart */
    unsigned XINT responselen; /* 0 = N/A */
    unsigned XINT singularlen; /* 0 = N/A */
    unsigned XINT plurallen;  /* 0 = N/A */
    } action_data_typ;

typedef struct action_ptr_str
    {
    unsigned char *responsetext;
    unsigned char *singulartext;
    unsigned char *pluraltext;
    } action_ptr_typ;

typedef struct action_rec_str
    {
    action_data_typ data;
    action_ptr_typ ptrs;
    } action_rec_typ;

typedef struct chan_idx_str
    {
    unsigned XINT structlength;
    unsigned long channum;
    char          type;
    XINT          usercount;
    unsigned char topic[71];
    XINT          modnode;
    /* specnodes: banned for open channels, invited for closed. */
    XINT          specnode[MAXCHANSPECNODES];
    } chan_idx_typ;

typedef struct spawn_prog_str
    {
    unsigned char name[41];
    unsigned char cmds[81];
    unsigned char cmdline[129];
    } spawn_prog_typ;

typedef struct censor_word_str
    {
    char wholeword;
    XINT level;
    char *word;
    char *changeto;
    } censor_word_typ;

typedef struct bio_ques_str
    {
    XINT number;
    unsigned char field[31];
    char resptype;
    long minval;
    long maxval;
    } bio_ques_typ;

typedef struct bio_resp_str
    {
    unsigned char response[71];
    unsigned char reserved[128 - 71];
    } bio_resp_typ;

#define RA_HIDDEN    0x01
#define RA_WANTCHAT  0x02
#define RA_RESERVED  0x04
#define RA_NODISTURB 0x08
#define RA_READY     0x40

typedef struct RA_USERON_str
	{
    unsigned char name[36];      /*   0 */
    unsigned char handle[36];    /*  36 */
    unsigned char line;          /*  72 */
    unsigned XINT baud;          /*  73 */
    unsigned char city[26];      /*  75 */
    unsigned char status;        /* 101 */
    unsigned char attribute;     /* 102 */
    unsigned char statdesc[11];  /* 103 */
    unsigned char freespace[98]; /* 114 */
    unsigned XINT numcalls;      /* 212 */ /* Total = 214 */
    } RA_USERON_typ;

#define EZY_NODISTURB 0x01

typedef struct EZY_ONLINE_str
    {
    unsigned char name[36];
    unsigned char alias[36];
    unsigned char status;
    unsigned char attribute;
    long          baud;
    unsigned char location[26];
    } EZY_ONLINE_typ;

#define SBBS_INUSE        0x01
#define SBBS_NODISTURB    0x02
#define SBBS_WAITING      0x04
#define SBBS_HIDDEN       0x08

typedef struct SBBS_USERON_str
	{
    unsigned char name[36];
    unsigned char attribute;
    unsigned char status;
    unsigned XINT baud;
    unsigned char city[26];
	unsigned char infobyte;
    unsigned char freespace[9];
    } SBBS_USERON_typ;

typedef struct file_stats_str
	{
	time_t fdate;
	long fsize;
	} file_stats_typ;

//|#include "toppoker.h"

#ifndef __WIN32__
extern int main(XINT ac, char *av[]);
#else
extern int pascal WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                          LPSTR lpszCmdParam, int nCmdShow);
#endif
extern void init(XINT argc, char *argv[]);
extern void show_file(char *name, unsigned char tasks);
extern void search_for_user(void);
extern char load_user_data(XINT recnum, user_data_typ *buffer);
extern char save_user_data(XINT recnum, user_data_typ *buffer);
extern void make_new_user(XINT recnum);
extern void minihelp(void);
extern void whos_in_pub(void);
extern void get_user_input(void);
extern void main_loop(void);
extern void process_input(unsigned char *string);
extern XINT calc_days_ago(XDATE *lastdate);
extern void register_node(void);
extern void unregister_node(char errabort);
extern void dispatch_message(XINT type, unsigned char *string, XINT tonode,
                             XINT priv, XINT echo);
extern void write_message(XINT outnode, msg_typ *msgbuf);
extern void check_nodes_used(char fatal);
extern XINT process_messages(char delname);
extern char get_node_data(XINT recnum, node_idx_typ *nodebuf);
extern XINT split_string(unsigned char *line);
extern XINT find_node_from_name(char *newstring, char *handle,
							   char *oldstring);
/*extern XINT find_max_score(XINT *check, XINT *thescore);*/
extern void show_time(void);
extern void quit_top(void);
extern void cmdline_help(void);
extern void profile_editor(void);
extern void save_node_data(XINT recnum, node_idx_typ *nodebuf);
extern void lookup(char *string);
extern void _USERENTRY before_exit(void);
extern void fixname(unsigned char *newname, unsigned char *oldname);
extern char check_dupe_handles(char *string);
extern unsigned char *top_output(char outmode, unsigned char *string, ...);
extern void errorabort(XINT code, char *info);
extern void filter_string(char *newstring, char *oldstring);
extern void get_password(char *buffer, XINT len);
extern void trim_string(char *newstring, char *oldstring,
						unsigned char nobits);
extern char iswhite(unsigned char ch);
extern void welcomescreen(void);
extern void newuserscreen(void);
extern XINT unbuild_pascal_string(XINT maxclen, unsigned char *buffer);
extern XINT build_pascal_string(unsigned char *buffer);
extern XINT closefile(FILE *thefil);
extern unsigned char *make_lok_name(XINT type);
extern XINT rec_locking(char op, XINT hand, long ofs, long len);
extern void openfiles(void);
extern void lan_updateopenfiles(void);
extern XINT sh_open(char *nam, XINT acc, XINT sh, XINT mod);
FILE XFAR *openfile(char *name, char *mode, char errabort);
extern void dofree(void XFAR *pfree);
extern void closefiles(void);
extern void preshell(void);
extern void postshell(void);
extern char memalloc(void);
extern void memfree(void);
extern void useredit(void);
extern void userpack(void);
extern void slots_game(void);
extern void load_slots_stats(slots_stat_typ *slbuf);
extern void save_slots_stats(slots_stat_typ *slbuf);
extern char XFAR *get_word(XINT wordnum);
extern void quit_top_screen(void);
extern void sysop_proc(void);
extern void change_proc(void);
extern char loadlang(void);
extern void outwordwrap(XINT omode, XINT *wwwc, char *www);
extern unsigned long outchecksuffix(unsigned char *strstart);
extern char checkcmdmatch(unsigned char *instring, unsigned char *cmdstring);
extern void matchgame(void);
#ifdef __OS2__
extern void LIBENTRY processcfg(char *keyword, char *options);
#else
extern void processcfg(char *keyword, char *options);
#endif
extern char seektruth(unsigned char *seekstr);
extern XINT loadnodecfg(unsigned char *exepath);
extern XINT splitnodecfgline(unsigned char *ncline);
extern unsigned char *getlang(unsigned char *langname);
extern unsigned char getlangchar(unsigned char *langname, XINT charnum);
extern void scan_pub(void);
extern void top_kernel(void);
extern void delprompt(char dname);
extern void bbs_useron(char endpause);
extern void bbs_page(XINT nodenum, unsigned char *pagebuf);
extern XINT editor(XINT numfields, XINT defaultfield,
				  editor_field_typ *fielddata);
extern void usereditor(void);
extern XINT userpicker(XINT startuser);
extern char loadchan(void);
extern void list_channels(void);
extern unsigned char *channelname(unsigned long chnum);
extern void channelsummary(void);
extern unsigned long checkchannelname(unsigned char *chnam);
extern long findchannelrec(unsigned long fchan);
extern void bbs_useronhdr(bbsnodedata_typ *ndata);
extern char moreprompt(void);
extern char loadactions(void);
extern char action_check(XINT awords);
extern void action_do(unsigned XINT lnum, unsigned XINT anum,
               XINT dowords, unsigned char *dostr);
extern void action_list(XINT alwords);
extern void action_proc(XINT awords);
extern void help_proc(XINT helpwds);
extern void privatechat(XINT pcnode);
extern void get_extension(char *ename, char *ebuf);
extern char loadpersactions(void);
extern char cmi_join(unsigned long chnum);
extern char cmi_unjoin(unsigned long chnum);
extern void cmi_adduser(void);
extern void cmi_subuser(void);
extern char cmi_load(unsigned long chnum);
extern char cmi_save(void);
extern char cmi_loadraw(XINT rec);
extern char cmi_saveraw(XINT rec);
extern char cmi_make(unsigned long chnum);
extern char cmi_lock(XINT rec);
extern char cmi_unlock(XINT rec);
extern char cmi_check(void);
extern char cmi_setspec(XINT node, char status);
//extern char load_spawn_progs(void);
//extern char check_spawn_cmds(XINT cmdwds);
extern void fixcredits(void);
extern void mod_proc(void);
extern void show_helpfile(unsigned char *hlpname);
extern char cmi_busy(void);
extern char cmi_unbusy(void);
extern char validatekey(byte *vkstr, byte *vkcode, word seed1, word seed2);
extern dword generatekey(byte *gkstr, word gks1, word gks2);
extern char load_censor_words(void);
extern char censorinput(char *str);
extern char loadbioquestions(void);
extern char displaybio(unsigned char *uname);
extern char writebioresponse(XINT qnum, bio_resp_typ *br);
extern char readbioresponse(XINT qnum, bio_resp_typ *br);
extern char getbioresponse(XINT rec, bio_resp_typ *br);
extern char biogetall(void);
extern void bioeditor(void);
extern XINT find_user_from_name(unsigned char *uname, user_data_typ *ubuf,
                                XINT nuse);
extern void userlist(void);
extern void sendtimemsgs(char *string);
extern XINT sh_unlink(char *filnam);
extern char biocheckalldone(void);

#ifdef __OS2__
extern void ProcessCmdLine(XINT ac, char *av[]);
#endif

extern void ra_init(void);
extern char ra_loaduseron(XINT nodenum, bbsnodedata_typ *userdata);
extern char ra_saveuseron(XINT nodenum, bbsnodedata_typ *userdata);
extern XINT ra_processmsgs(void);
extern char ra_page(XINT nodenum, unsigned char *pagebuf);
extern void ra_setexistbits(bbsnodedata_typ *userdata);
extern void ra_login(void);
extern void ra_logout(void);
extern XINT ra_openfiles(void);
extern XINT ra_updateopenfiles(void);
extern char ra_longpageeditor(XINT nodenum, unsigned char *pagebuf);

extern void max_init(void);
extern char max_loadipcstatus(XINT nodenum, bbsnodedata_typ *userdata);
extern char max_saveipcstatus(XINT nodenum, bbsnodedata_typ *userdata);
extern XINT max_processipcmsgs(void);
extern char max_page(XINT nodenum, unsigned char *pagebuf);
extern char max_writeipcmsg(XINT nodenum, struct _cdat *msginf,
		                    char *message);
extern void max_setexistbits(bbsnodedata_typ *userdata);
extern void max_login(void);
extern void max_logout(void);
extern XINT max_openfiles(void); /* Also doubles as updateopenfiles call. */
extern char max_shortpageeditor(XINT nodenum, unsigned char *pagebuf);
extern void max_showmeccastring(unsigned char *str);

#ifdef __OS2__
extern void max_mcpinit(void);
extern char max_loadmcpstatus(XINT nodenum, bbsnodedata_typ *userdata);
extern char max_savemcpstatus(XINT nodenum, bbsnodedata_typ *userdata);
extern XINT max_processmcpmsgs(void);
extern char max_mcppage(XINT nodenum, unsigned char *pagebuf);
extern void max_mcplogin(void);
extern void max_mcplogout(void);
extern XINT max_mcpopenpipe(void); /* Also doubles as update call. */
#endif

extern void sbbs_init(void);
extern char sbbs_loaduseron(XINT nodenum, bbsnodedata_typ *userdata);
extern char sbbs_saveuseron(XINT nodenum, bbsnodedata_typ *userdata);
extern void sbbs_setexistbits(bbsnodedata_typ *userdata);

#ifndef __OS2__
extern clock_t myclock(void);
#endif

extern TOP_config_typ cfg;
extern XINT user_rec_num;
extern user_data_typ user;
extern char XFAR *word_str;
extern XINT XFAR *word_pos;
extern XINT XFAR *word_len;
extern stringarray2 XFAR *handles;
extern unsigned char XFAR *activenodes;
extern clock_t lastpoll;
extern unsigned char XFAR *ra_statustypes[];
extern unsigned char XFAR *outbuf;
extern char isregistered;
extern char node_added;
extern unsigned char XFAR *forgetstatus;
extern XINT lastsendtonode;
extern char localmode, lanmode;
extern char XFAR *busynodes;
extern struct file_stats_str ipcfildat;
extern XINT cfgfil, userfil, nidxfil, nidx2fil, msginfil, msgoutfil,
		   useronfil, ipcinfil, ipcoutfil, rapagefil,
           midxinfil, midxoutfil, chgfil, chidxfil, //|, pokdatafil,
		   biorespfil, bioidxfil;
extern XINT slotsfil;
extern struct file_stats_str midxfileinfo;
extern unsigned long slotsbet;
extern XINT lowestnode;
//extern unsigned char XFAR *langtext;
//extern unsigned long XFAR *langstart;
extern lang_text_typ XFAR **langptrs;
extern XINT usedcmdlen;
extern XINT nodecfgfil;
extern unsigned long curchannel;
extern unsigned char *wordret;
extern long numlang;
extern node_idx_typ *node;
extern unsigned char *outputstr;
extern XINT matchfil;
extern TOP_nodecfg_typ *nodecfg;
extern unsigned char XFAR *sbbs_statustypes[];
extern char outproclang;
extern char outproccol;
extern char outprocact;
extern unsigned XINT outdefattrib;
extern unsigned char *lastcmd;
extern unsigned char ver[16];
extern unsigned char rev[11];
extern channel_data_typ *chan;
extern long msgextradata;
//|extern clock_t lastpokerpoll;
extern char numactfiles;
extern action_file_typ **actfiles;
extern action_rec_typ ***actptrs;
extern XINT ystore, xstore;
extern char redrawwindows;
extern XINT privchatin, privchatout;
extern chan_idx_typ cmibuf;
extern XINT cmirec;
extern time_t lastkertime;
extern spawn_prog_typ *spawnprog;
extern XINT numspawnprog;
extern unsigned XINT msgminsec, msgmaxsec;
extern unsigned long cmiprevchan;
extern long orignumcreds;
extern char onflag;
extern char msgsendglobal;
extern censor_word_typ *cwords;
extern XINT numcensorwords;
extern XINT censorwarningshigh;
extern XINT censorwarningslow;
extern time_t lastcensorwarnhigh;
extern time_t lastcensorwarnlow;
extern bio_ques_typ *bioques;
extern XINT numbioques;
extern char blocktimemsgs;
extern char forcefix;

// extern unsigned char XFAR *argptrs[9];
extern unsigned char XFAR outnum[9][12];

extern char (XFAR *bbs_call_loaduseron)(XINT nodenum,
									   bbsnodedata_typ *userdata);
extern char (XFAR *bbs_call_saveuseron)(XINT nodenum,
									   bbsnodedata_typ *userdata);
extern XINT (XFAR *bbs_call_processmsgs)(void);
extern char (XFAR *bbs_call_page)(XINT nodenum, unsigned char *pagebuf);
extern void (XFAR *bbs_call_setexistbits)(bbsnodedata_typ *userdata);
extern void (XFAR *bbs_call_login)(void);
extern void (XFAR *bbs_call_logout)(void);
extern XINT (XFAR *bbs_call_openfiles)(void);
extern XINT (XFAR *bbs_call_updateopenfiles)(void);
extern char (XFAR *bbs_call_pageedit)(XINT nodenum, unsigned char *pagebuf);

#ifdef __OS2__
extern HPIPE hpMCP;
#endif

extern unsigned char *bbsnames[BBSTYPES];
extern unsigned char *nodetypes[3];

extern unsigned char XFAR *card_names[14];
extern unsigned char XFAR *card_suitnames[4];
extern unsigned char XFAR card_symbols[14];
extern unsigned char XFAR card_suits[8];

/* OpenDoors undocumented variable. */
extern unsigned XINT _forced_bps;
