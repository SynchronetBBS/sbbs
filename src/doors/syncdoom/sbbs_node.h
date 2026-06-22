// sbbs_node.h -- door-native access to Synchronet's online-node list and the
// inter-node page mechanism, for syncdoom's in-game Ctrl-U (who's online) and
// Ctrl-P (page a node). No SCFG/scfg_t load: the ctrl dir comes from $SBBSCTRL
// (where node.dab lives) and the data dir is derived from the door's -home
// (<data>/user/####/doom/ -> <data>/, where user/name.dat and msgs/ live).
//
// Only sbbs_node.c pulls in the Synchronet headers; the door includes just this.

#ifndef SBBS_NODE_H_
#define SBBS_NODE_H_

#include <stddef.h>

typedef struct {
	int      number;        // 1-based node number
	int      useron;        // user number signed on
	int      anon;          // non-zero: anonymous (hide the alias)
	int      action;        // enum node_action (what they're doing)
	int      aux;           // node aux word (channel/node#/etc. for some actions)
	unsigned connection;    // connection type/rate
} sbbs_node_info_t;

// Locate the ctrl + data dirs ($SBBSCTRL and the door's -home). Call once at
// startup. Returns 1 if a usable BBS context was found (node.dab present), else
// 0 -- the Ctrl-U/Ctrl-P features should then stay disabled. 'home' is the -home
// value the door was launched with (may be empty).
int sbbs_node_init(const char *home);

// Was a usable BBS context found by sbbs_node_init()?
int sbbs_node_available(void);

// This door's own node number (from $SBBSNNUM, else the trailing digits of
// $SBBSNODE), or 0 if unknown. Use to exclude self from the list and as the page
// "from" node.
int sbbs_my_node(void);

// Snapshot the in-use nodes into out[] (up to max), skipping 'selfnode' (pass 0
// to include all). Returns the count written.
int sbbs_list_nodes(sbbs_node_info_t *out, int max, int selfnode);

// Resolve a user number to its alias in buf (>= 26 bytes recommended). Returns
// buf. Deleted/empty users -> "DELETED USER"; on error -> "".
const char *sbbs_username(int usernum, char *buf, size_t bufsz);

// Synchronet's standard activity phrase for a node (the lowercase wording from
// nodestr()'s built-in defaults, e.g. "at main menu", "downloading", "running
// external program"). Parameterized ones format into buf; returns a string to
// print after "Node N: <alias> is ...".
const char *sbbs_action_str(const sbbs_node_info_t *node, char *buf, size_t bufsz);

// A terse abbreviation of a node action ("xtrn", "main", "dl", ...) for compact
// one-line lists where the full sbbs_action_str() wording won't fit.
const char *sbbs_action_abbr(int action);

// Page 'target' node: append a BEL-prefixed message to <data>/msgs/n###.msg and
// raise the target's NODE_NMSG flag so it displays. from_node/from_alias name the
// sender. Returns 1 on success; 0 if the target isn't in use, has paging off, or
// on error.
int sbbs_page_node(int target, int from_node, const char *from_alias, const char *text);

// Consume THIS node's pending node message (an incoming page): read it into buf,
// truncate the message file, and clear our NODE_NMSG flag (like getnmsg). Returns
// the byte count (0 = nothing waiting). The text may carry Synchronet Ctrl-A
// attribute codes -- the caller strips them, since the door emits raw.
int sbbs_recv_nmsg(int mynode, char *buf, size_t bufsz);

#endif /* SBBS_NODE_H_ */
