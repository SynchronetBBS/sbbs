#ifndef _UIFCINIT_H_
#define _UIFCINIT_H_

#include <uifc.h>

extern	uifcapi_t uifc; /* User Interface (UIFC) Library API */
extern int uifc_initialized;
int	init_uifc(void);
void uifcbail(void);
void uifcmsg(char *msg, char *helpbuf);

#endif
