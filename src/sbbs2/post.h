/* POST.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "gen_defs.h"


typedef struct {                        /* Message data */
	ulong	offset, 					/* Offset to header (in bytes) */
			number; 					/* Number of message */
	ushort	to, 						/* CRC-16 of to username */
			from,						/* CRC-16 of from username */
			subj;						/* CRC-16 of subject */
			} post_t;

#ifdef SBBS
post_t HUGE16 *loadposts(ulong *posts, uint subnum, ulong ptr, uint mode);
#else
ulong loadposts(post_t HUGE16 **post, uint subnum, ulong ptr, uint mode);
#endif
int  searchposts(uint subnum, post_t HUGE16 *post, long start, long msgs
     ,char *search);
void showposts_toyou(post_t HUGE16 *post, ulong start, ulong posts);
