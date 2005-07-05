#ifdef SBBS
#include "sbbs.h"
static sbbs_t *sbbs=NULL;
#else
#include <stdio.h>
#endif

void con_init(void *sptr)
{
#ifdef SBBS
	sbbs=(sbbs_t *)sptr;
#endif
}

void con_write(char *str, size_t len)
{
#ifdef SBBS
    for(int i=0; i<len; i++)
        sbbs->outchar(str[i]);
#else
    fwrite(str, len, 1, stdout);
#endif
}

int con_getchar(void)
{
#ifdef SBBS
    int ch=NOINP;
    while(ch==NOINP)
        sbbs->incom(1000);
    return ch;
#else
    return(getchar());
#endif
}
