#ifndef _XPDEV_H_
#define _XPDEV_H_

#define SAFECOPY(dst, src) sprintf(dst, "%.*s", (int)sizeof(dst) - 1, src)
#if defined(__unix__)
#if !defined(stricmp)
#define stricmp(x,y)            strcasecmp(x,y)
#define strnicmp(x,y,z)         strncasecmp(x,y,z)
#endif
#endif

char *truncsp(char *str);
int xp_random(int n);

#endif
