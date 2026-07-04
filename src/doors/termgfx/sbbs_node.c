// sbbs_node.c -- see sbbs_node.h. Door-native reads of Synchronet's node.dab +
// user/name.dat and writes of the per-node page file, with no scfg_t/load_cfg:
// the ctrl dir is $SBBSCTRL and the data dir is derived from the door's -home.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nodedefs.h"      // node_t, enum node_status/node_action, NODE_* bits
#include "dirwrap.h"       // xpdev: fexist, MKDIR (defines MAX_PATH, not PATH_MAX)
#include "filewrap.h"      // xpdev: lock/unlock
#include "sbbs_node.h"

#ifndef PATH_MAX           // POSIX defines it via <sys/param.h>; MSVC does not
#define PATH_MAX 1024      // (xpdev's portable macro is MAX_PATH) -- match syncdoom.c
#endif

#ifndef LEN_ALIAS
#define LEN_ALIAS 25       // src/sbbs3/sbbsdefs.h -- alias length in user/name.dat
#endif
#define NAMEDAT_RECLEN (LEN_ALIAS + 2)   // name.dat fixed record size (27)
#define SBBS_PATHLEN   (PATH_MAX + 32)    // dir (PATH_MAX) + a short filename suffix

static char g_ctrl[PATH_MAX];   // ctrl dir (trailing sep) -- node.dab
static char g_data[PATH_MAX];   // data dir (trailing sep) -- user/name.dat, msgs/
static int  g_ok = 0;

// Strip a trailing separator, then the last path component, leaving a trailing sep.
static void path_pop(char *p)
{
	size_t n = strlen(p);

	while (n && (p[n - 1] == '/' || p[n - 1] == '\\'))
		p[--n] = '\0';
	while (n && p[n - 1] != '/' && p[n - 1] != '\\')
		p[--n] = '\0';
}

// Append a path separator if there isn't one.
static void ensure_sep(char *p, size_t sz)
{
	size_t n = strlen(p);

	if (n && p[n - 1] != '/' && p[n - 1] != '\\' && n + 1 < sz) {
		p[n]     = '/';
		p[n + 1] = '\0';
	}
}

int sbbs_node_init(const char *home)
{
	const char *ctrl = getenv("SBBSCTRL");
	const char *data = getenv("SBBSDATA");
	char        nodedab[SBBS_PATHLEN];

	g_ok = 0;
	g_ctrl[0] = g_data[0] = '\0';
	if (ctrl == NULL || *ctrl == '\0')
		return 0;
	snprintf(g_ctrl, sizeof(g_ctrl), "%s", ctrl);            // ctrl dir (node.dab)
	ensure_sep(g_ctrl, sizeof(g_ctrl));

	// data dir (user/name.dat, msgs/): $SBBSDATA -- the BBS sets it for externals.
	// Fall back to stripping user/####/doom/ off the door's -home if it's unset.
	if (data != NULL && *data) {
		snprintf(g_data, sizeof(g_data), "%s", data);
		ensure_sep(g_data, sizeof(g_data));
	} else if (home != NULL && *home) {
		snprintf(g_data, sizeof(g_data), "%s", home);
		path_pop(g_data);                                   // doom
		path_pop(g_data);                                   // ####
		path_pop(g_data);                                   // user -> <data>/
	}
	if (g_data[0] == '\0')
		return 0;

	snprintf(nodedab, sizeof(nodedab), "%snode.dab", g_ctrl);
	if (!fexist(nodedab))                                    // confirm a real ctrl dir
		return 0;
	g_ok = 1;
	return 1;
}

int sbbs_my_node(void)
{
	const char *nnum = getenv("SBBSNNUM");                   // BBS sets the node number
	const char *node;
	int         n;

	if (nnum != NULL && (n = atoi(nnum)) > 0)
		return n;
	// Fall back to the trailing digits of $SBBSNODE (the node dir, e.g. .../node3).
	if ((node = getenv("SBBSNODE")) != NULL) {
		const char *p = node + strlen(node);
		while (p > node && (p[-1] == '/' || p[-1] == '\\'))
			p--;
		const char *end = p;
		while (p > node && p[-1] >= '0' && p[-1] <= '9')
			p--;
		if (p < end && (n = atoi(p)) > 0)
			return n;
	}
	return 0;
}

int sbbs_node_available(void) { return g_ok; }

int sbbs_list_nodes(sbbs_node_info_t *out, int max, int selfnode)
{
	char   path[SBBS_PATHLEN];
	FILE * f;
	node_t node;
	int    count = 0, num = 0;

	if (!g_ok || out == NULL || max <= 0)
		return 0;
	snprintf(path, sizeof(path), "%snode.dab", g_ctrl);
	if ((f = fopen(path, "rb")) == NULL)
		return 0;
	while (count < max && fread(&node, sizeof(node), 1, f) == 1) {
		num++;
		if (num == selfnode)
			continue;
		if (node.status != NODE_INUSE && node.status != NODE_QUIET)
			continue;
		out[count].number     = num;
		out[count].useron     = node.useron;
		out[count].anon       = (node.misc & NODE_ANON) != 0;
		out[count].action     = node.action;
		out[count].aux        = node.aux;
		out[count].ext        = (node.misc & NODE_EXT) != 0;
		out[count].connection = node.connection;
		count++;
	}
	fclose(f);
	return count;
}

const char *sbbs_username(int usernum, char *buf, size_t bufsz)
{
	char   path[SBBS_PATHLEN];
	FILE * f;
	char   rec[LEN_ALIAS];
	int    i;

	if (buf != NULL && bufsz)
		buf[0] = '\0';
	if (!g_ok || buf == NULL || bufsz < 2 || usernum < 1)
		return buf;
	snprintf(path, sizeof(path), "%suser/name.dat", g_data);
	if ((f = fopen(path, "rb")) == NULL)
		return buf;
	if (fseek(f, (long)(usernum - 1) * NAMEDAT_RECLEN, SEEK_SET) == 0
	    && fread(rec, 1, sizeof(rec), f) == sizeof(rec)) {
		for (i = 0; i < (int)sizeof(rec) && rec[i] != '\x03'; i++)
			;
		if (i == 0)
			snprintf(buf, bufsz, "DELETED USER");
		else {
			if ((size_t)i >= bufsz)
				i = (int)bufsz - 1;
			memcpy(buf, rec, (size_t)i);
			buf[i] = '\0';
		}
	}
	fclose(f);
	return buf;
}

// Synchronet's standard activity wording (nodestr()'s built-in English defaults,
// userdat.c). We can't read the sysop's text.dat overrides without scfg_t, and a
// couple (sysop/guru names, the running door's name) need state we don't load, so
// those degrade to a generic phrase -- the wording still matches the standard.
const char *sbbs_action_str(const sbbs_node_info_t *node, char *buf, size_t bufsz)
{
	switch (node->action) {
		case NODE_MAIN:   return "at main menu";
		case NODE_RMSG:   return "reading messages";
		case NODE_RMAL:   return "reading mail";
		case NODE_RSML:   return "reading sent mail";
		case NODE_RTXT:   return "reading text files";
		case NODE_PMSG:   return "posting message";
		case NODE_SMAL:   return "sending mail";
		case NODE_AMSG:   return "posting auto-message";
		case NODE_XTRN:   return node->aux ? "running external program" : "at external program menu";
		case NODE_DFLT:   return "changing defaults";
		case NODE_XFER:   return "at transfer menu";
		case NODE_RFSD:   snprintf(buf, bufsz, "retrieving from device #%d", node->aux); return buf;
		case NODE_DLNG:   return "downloading";
		case NODE_ULNG:   return "uploading";
		case NODE_BXFR:   return "transferring bidirectional";
		case NODE_LFIL:   return "listing files";
		case NODE_LOGN:   return "logging on";
		case NODE_LCHT:   return "chatting with the sysop";
		case NODE_MCHT:
			if (node->aux) {
				snprintf(buf, bufsz, "in multinode chat channel %d", node->aux & 0xff);
				return buf;
			}
			return "in multinode global chat channel";
		case NODE_PAGE:   snprintf(buf, bufsz, "paging node %u for private chat", (unsigned)node->aux); return buf;
		case NODE_PCHT:
			if (node->aux) {
				snprintf(buf, bufsz, "in private chat with node %u", (unsigned)node->aux);
				return buf;
			}
			return "in local chat";
		case NODE_GCHT:   return "chatting with the guru";
		case NODE_CHAT:   return "in chat section";
		case NODE_TQWK:   return "transferring QWK packet";
		case NODE_SYSP:   return "performing sysop activities";
		case NODE_CUSTOM: return "performing custom action";
		default:          snprintf(buf, bufsz, "unknown action %d", node->action); return buf;
	}
}

const char *sbbs_action_abbr(int action)
{
	switch (action) {
		case NODE_MAIN:                                 return "main menu";
		case NODE_RMSG: case NODE_RMAL: case NODE_RSML: return "reading msgs";
		case NODE_PMSG: case NODE_SMAL: case NODE_AMSG: return "writing msg";
		case NODE_RTXT:                                 return "reading text";
		case NODE_XTRN:                                 return "running xtrn";
		case NODE_DFLT:                                 return "configuring";
		case NODE_XFER: case NODE_LFIL:                 return "file menu";
		case NODE_DLNG:                                 return "downloading";
		case NODE_ULNG:                                 return "uploading";
		case NODE_BXFR: case NODE_TQWK: case NODE_RFSD: return "transferring";
		case NODE_LOGN:                                 return "logging on";
		case NODE_LCHT: case NODE_MCHT: case NODE_GCHT:
		case NODE_CHAT: case NODE_PCHT:                 return "chatting";
		case NODE_PAGE:                                 return "paging";
		case NODE_SYSP:                                 return "sysop activity";
		default:                                        return "online";
	}
}

int sbbs_page_node(int target, int from_node, const char *from_alias, const char *text)
{
	char   path[SBBS_PATHLEN], ndp[SBBS_PATHLEN];
	FILE * f;
	node_t node;
	long   off;

	if (!g_ok || target < 1)
		return 0;
	off = (long)(target - 1) * (long)sizeof(node_t);
	snprintf(ndp, sizeof(ndp), "%snode.dab", g_ctrl);

	// Target must be in use and not paging-disabled.
	if ((f = fopen(ndp, "rb")) == NULL)
		return 0;
	if (fseek(f, off, SEEK_SET) != 0 || fread(&node, sizeof(node), 1, f) != 1) {
		fclose(f);
		return 0;
	}
	fclose(f);
	if ((node.status != NODE_INUSE && node.status != NODE_QUIET) || (node.misc & NODE_POFF))
		return 0;

	// Append the page text (leading BEL beeps the target; Ctrl-A codes color it).
	snprintf(path, sizeof(path), "%smsgs", g_data);
	(void)MKDIR(path);
	snprintf(path, sizeof(path), "%smsgs/n%3.3u.msg", g_data, (unsigned)target);
	if ((f = fopen(path, "ab")) == NULL)
		return 0;
	// Ctrl-A attribute codes use octal \001 -- "\x01c" would be the single hex
	// escape 0x1C, since 'c' is a hex digit (the cyan code was being eaten).
	fprintf(f, "\007\r\n\001n\001h\001cNode %d\001n: \001c%s\001n sent you a message:\r\n%s\r\n",
	        from_node, from_alias ? from_alias : "?", text ? text : "");
	fclose(f);

	// Raise NODE_NMSG on the target's record (locked read-modify-write of the one
	// 15-byte record, matching how sbbs itself guards node.dab writes).
	if ((f = fopen(ndp, "r+b")) != NULL) {
		node_t nd;
		lock(fileno(f), off, (long)sizeof(node_t));
		if (fseek(f, off, SEEK_SET) == 0 && fread(&nd, sizeof(nd), 1, f) == 1
		    && !(nd.misc & NODE_NMSG)) {
			nd.misc |= NODE_NMSG;
			fseek(f, off, SEEK_SET);
			fwrite(&nd, sizeof(nd), 1, f);
			fflush(f);
		}
		unlock(fileno(f), off, (long)sizeof(node_t));
		fclose(f);
	}
	return 1;
}

int sbbs_recv_nmsg(int mynode, char *buf, size_t bufsz)
{
	char   path[SBBS_PATHLEN], ndp[SBBS_PATHLEN];
	FILE * f;
	long   sz;
	size_t rd = 0;

	if (!g_ok || mynode < 1 || buf == NULL || bufsz < 1)
		return 0;
	if (buf != NULL)
		buf[0] = '\0';

	// Read + clear our own node message file. Lock it across the read+truncate so
	// a concurrent page (append) isn't lost.
	snprintf(path, sizeof(path), "%smsgs/n%3.3u.msg", g_data, (unsigned)mynode);
	if ((f = fopen(path, "r+b")) == NULL)
		return 0;
	lock(fileno(f), 0, (long)bufsz);
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	if (sz > 0) {
		fseek(f, 0, SEEK_SET);
		rd = fread(buf, 1, (sz < (long)bufsz - 1) ? (size_t)sz : bufsz - 1, f);
		buf[rd] = '\0';
		chsize(fileno(f), 0);                  // consumed -> truncate
	}
	unlock(fileno(f), 0, (long)bufsz);
	fclose(f);
	if (rd == 0)
		return 0;

	// Clear NODE_NMSG on our own record (locked RMW), matching getnmsg().
	snprintf(ndp, sizeof(ndp), "%snode.dab", g_ctrl);
	if ((f = fopen(ndp, "r+b")) != NULL) {
		node_t nd;
		long   off = (long)(mynode - 1) * (long)sizeof(node_t);
		lock(fileno(f), off, (long)sizeof(node_t));
		if (fseek(f, off, SEEK_SET) == 0 && fread(&nd, sizeof(nd), 1, f) == 1
		    && (nd.misc & NODE_NMSG)) {
			nd.misc &= ~NODE_NMSG;
			fseek(f, off, SEEK_SET);
			fwrite(&nd, sizeof(nd), 1, f);
			fflush(f);
		}
		unlock(fileno(f), off, (long)sizeof(node_t));
		fclose(f);
	}
	return (int)rd;
}

// Set this node's free-text who's-online status (node.exb + the NODE_EXT misc bit),
// shown in place of the standard action wording ("running external program") in the
// BBS who's-online list and in our own Ctrl-U list. Pass a short activity phrase;
// "" or NULL clears it back to the normal action text. The BBS rewrites node.exb
// from the action whenever it next updates this node (e.g. when the door exits), so
// this holds only while the door is running -- exactly the window we want.
void sbbs_node_set_ext(const char *text)
{
	char   path[SBBS_PATHLEN], rec[128];
	FILE * f;
	int    mynode = sbbs_my_node();
	int    want   = (text != NULL && text[0] != '\0');
	long   off;

	if (!g_ok || mynode < 1)
		return;

	// Our 128-byte record in node.exb (null-padded). Only create the file if it's
	// somehow absent -- never truncate an existing one (it holds every node's text).
	memset(rec, 0, sizeof(rec));
	if (text != NULL)
		strncpy(rec, text, sizeof(rec) - 1);
	snprintf(path, sizeof(path), "%snode.exb", g_ctrl);
	if ((f = fopen(path, "r+b")) == NULL)
		f = fopen(path, "w+b");
	if (f != NULL) {
		off = (long)(mynode - 1) * 128;
		if (fseek(f, off, SEEK_SET) == 0)
			fwrite(rec, sizeof(rec), 1, f);
		fclose(f);
	}

	// Set/clear NODE_EXT on our own node.dab record (locked RMW, as in sbbs).
	snprintf(path, sizeof(path), "%snode.dab", g_ctrl);
	if ((f = fopen(path, "r+b")) != NULL) {
		node_t nd;
		off = (long)(mynode - 1) * (long)sizeof(node_t);
		lock(fileno(f), off, (long)sizeof(node_t));
		if (fseek(f, off, SEEK_SET) == 0 && fread(&nd, sizeof(nd), 1, f) == 1) {
			uint16_t before = nd.misc;
			if (want)
				nd.misc |= NODE_EXT;
			else
				nd.misc &= ~NODE_EXT;
			if (nd.misc != before) {
				fseek(f, off, SEEK_SET);
				fwrite(&nd, sizeof(nd), 1, f);
				fflush(f);
			}
		}
		unlock(fileno(f), off, (long)sizeof(node_t));
		fclose(f);
	}
}

// Read node `num`'s free-text status from node.exb into buf (>= 128 bytes). Returns
// buf; empty string if the node has none. Used to show NODE_EXT nodes in Ctrl-U.
const char *sbbs_node_ext(int num, char *buf, size_t bufsz)
{
	char  path[SBBS_PATHLEN], rec[128];
	FILE *f;

	if (buf == NULL || bufsz < 1)
		return "";
	buf[0] = '\0';
	if (!g_ok || num < 1)
		return buf;
	snprintf(path, sizeof(path), "%snode.exb", g_ctrl);
	if ((f = fopen(path, "rb")) == NULL)
		return buf;
	if (fseek(f, (long)(num - 1) * 128, SEEK_SET) == 0 && fread(rec, 1, sizeof(rec), f) == sizeof(rec)) {
		rec[sizeof(rec) - 1] = '\0';
		strncpy(buf, rec, bufsz - 1);
		buf[bufsz - 1] = '\0';
	}
	fclose(f);
	return buf;
}
