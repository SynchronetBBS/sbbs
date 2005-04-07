#ifndef _GREGEDIT_H
#define _GREGEDIT_H

#include "OpenDoor.h"

//#include "netmail.h"
#include <string.h>
//#include <mem.h>

//INT16 del_line(INT16 line, INT16 end );
//INT16 ins_line(INT16 line, INT16 end);
INT16 erase(INT16 n);   // 1/97 removed far
 /* pass how wide the editor should be and the maximum line number */
INT16 gac_lined(INT16 y, INT16 last); // 1/97 removed far


#define WIDOFF  (sizeof("99: "))    /* size of line number prompt */
#define SS  80          /* maximum size of string     */
#ifndef SUB
#define SUB 26   /* control-Z */
#endif
#ifndef VT
#define VT  11   /* control-K */
#endif
#ifndef VT
#define VT  11   /* control-K */
#endif
// #define ETX  3    /* control-C */
#ifndef ETX
#define ETX 0x03    /* control-C */
#endif
// #define CR   13   /* Enter/Return */
#ifndef CR
#define CR  0x0d   /* Enter/Return */
#endif
#ifndef DEL
#define DEL 127  /* Delete    */
#endif
// #define BS   8    /* BackSpace */
#ifndef BS
#define BS  0x08    /* BackSpace */
#endif
// #define TAB  9    /* Tab       */
#ifndef TAB
#define TAB 0x09    /* Tab       */
#endif
#ifndef CAN
#define CAN 127  /* Cancel (DEL) */
#endif


#endif

