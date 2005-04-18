#include<OpenDoor.h>
#ifdef TODO_HEADERS
#include<dos.h>
#endif
#include<stdio.h>
#include<string.h>
void            bp(char *registration_string, unsigned int security_code);
unsigned long   rcount = 0;
extern char     registeredxx;
extern char     registered_name[201];
extern void     notdone(void);
extern void     mustregister(void);
extern void     registerme(void);
extern int      statis, cyc;
extern void     nl(void);
extern void     smurf_pause(void);
