//
//  unix_compat.h
//  Duke3D
//
//  Based on macos_compat.h
//  Copyright Wed, Jul 31, 2013, Juan Manuel Borges Caño (GPLv3+)
//

#ifndef Duke3D_unix_compat_h
#define Duke3D_unix_compat_h

//#define BYTE_ORDER LITTLE_ENDIAN
#define PLATFORM_SUPPORTS_SDL

#include <stdlib.h>

#define kmalloc(x) malloc(x)
#define kkmalloc(x) malloc(x)
#define kfree(x) free(x)
#define kkfree(x) free(x)

#ifdef FP_OFF
#undef FP_OFF
#endif

// Horrible horrible macro: Watcom allowed memory pointer to be cast
// to a 32bits integer. The code is unfortunately stuffed with this :( !
// SyncDuke 64-bit patch: widen to intptr_t so the pointer round-trip uses
// ((void*)FP_OFF(ptr), e.g. initfastcolorlookup's colhere/colhead clear and
// the palookup pointer builds) survive on LP64 instead of truncating to 32
// bits and crashing. Sites that store the result into an int32_t global
// (the 3D render inner loops: palookupoffs/asm3) still truncate -- those are
// a deeper 32-bit assumption handled separately. No-op on a 32-bit build.
#include <stdint.h>
#define FP_OFF(x) ((intptr_t) (x))

#ifndef max
#define max(x, y)  (((x) > (y)) ? (x) : (y))
#endif

#ifndef min
#define min(x, y)  (((x) < (y)) ? (x) : (y))
#endif

#include <inttypes.h>
#define __int64 int64_t

#define O_BINARY 0

#define UDP_NETWORKING 1

#define PLATFORM_UNIX 1

/*
#define SOL_IP SOL_SOCKET
#define IP_RECVERR  SO_BROADCAST
*/

#define stricmp strcasecmp
#define strcmpi strcasecmp

#define S_IREAD S_IRUSR
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#define USER_DUMMY_NETWORK 1

#endif
