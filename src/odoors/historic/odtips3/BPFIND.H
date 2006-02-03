/* MSC / BC compatible findfirst()/findnext() definitions. */

#ifdef __TURBOC__
#include <dir.h>
#include <dos.h>
#else
#include <dos.h>
struct ffblk
{
   char ff_reserved[21];
   char ff_attrib;
   unsigned ff_ftime;
   unsigned ff_fdate;
   long ff_fsize;
   char ff_name[13];
}
#define findfirst(p, f, a)  _dos_findfirst(p, (struct _find_t *)f, a)
#define findnext(f)         _dos_findnext((struct _find_t *)f)
#define FA_RDONLY           _A_RDONLY
#define FA_HIDDEN           _A_HIDDEN
#define FA_SYSTEM           _A_SYSTEM
#define FA_LABEL            _A_VOLID
#define FA_DIREC            _A_SUBDIR
#define FA_ARCH             _A_ARCH
#endif
