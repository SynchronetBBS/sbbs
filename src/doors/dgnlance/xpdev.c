#include <string.h>
#include <stdlib.h>		       /* RAND_MAX */
#include <time.h>		       /* time() */

#include "OpenDoor.h"		       /* BOOL */

#include "xpdev.h"

/****************************************************************************/
/* Truncates all white-space chars off end of 'str'     (needed by STRERRROR)   */
/****************************************************************************/
char           *
truncsp(char *str)
{
    size_t          i, len;
    i = len = strlen(str);

    while (i && (str[i - 1] == ' ' || str[i - 1] == '\t' || str[i - 1] == '\r' || str[i - 1] == '\n'))
	i--;
    if (i != len)
	str[i] = 0;		       /* truncate */
    return (str);
}

int
xp_random(int n)
{
    float           f;
    static BOOL     initialized;
    if (!initialized) {
	srand(time(NULL));	       /* seed random number generator */
	rand();			       /* throw away first result */
	initialized = TRUE;
    }
    if (n < 2)
	return (0);
    f = (float)rand() / (float)RAND_MAX;
    return ((int)(n * f));
}
