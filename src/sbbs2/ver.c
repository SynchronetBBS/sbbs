/* VER.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <dos.h>
#include "sbbs.h"
#include "riolib.h"
#include "riodefs.h"
#include "etext.h"

extern uint inDV;
extern uint asmrev;
extern uint emshandle;
extern char emsver;

char *compile_time=__TIME__,*compile_date=__DATE__;

char *decrypt(ulong [], char *str);

void ver()
{
	char str[128],tmp[128];
	int i;

i=rioctl(FIFOCTL);
CRLF;
#if defined(__OS2__)
strcpy(str,decrypt(VersionNoticeOS2,0));
#elif defined(__WIN32__)
strcpy(str,decrypt(VersionNoticeW32,0));
#else
strcpy(str,decrypt(VersionNoticeDOS,0));
#endif
center(str);
CRLF;

sprintf(str,"Revision %c%s %s %.5s  "
#ifdef __FLAT__
	"RIOLIB %u.%02u"
#else
	"RCIOL %u"
#endif
	"  SMBLIB %s  BCC %X.%02X"
	,REVISION,BETA,compile_date,compile_time
#ifdef __FLAT__
	,rioctl(GVERS)/100,rioctl(GVERS)%100
#else
	,rioctl(GVERS)
#endif
	,smb_lib_ver()
	,__BORLANDC__>>8
	,__BORLANDC__&0xff);
center(str);
CRLF;

center(decrypt(CopyrightAddress,0));
CRLF;

#if defined(__OS2__)

sprintf(str,"OS/2 %u.%u (%u.%u)",_osmajor/10,_osminor/10,_osmajor,_osminor);

#elif defined(__WIN32__)

sprintf(str,"Win32 %u.%02u",_osmajor,_osminor);

#else	/* DOS */

sprintf(str,"DOS %u.%02u",_osmajor,_osminor);
if(inDV) {
	sprintf(tmp,"   DESQview %u.%02u",inDV>>8,inDV&0xff);
	strcat(str,tmp); }
if(emsver) {
	sprintf(tmp,"   EMS %u.%u",(emsver&0xf0)>>4,emsver&0xf);
	strcat(str,tmp);
    if(emshandle!=0xffff)
		strcat(str," (overlay)"); }

#endif

if(i&0xc0) {
	strcat(str,"   16550 UART");
	if(i&0xc)
		strcat(str," FIFO"); }
#if DEBUG
	i=open("NODE.CFG",0);
	bprintf("   Files (%d)",i);
	close(i);
#endif
center(str);
}
