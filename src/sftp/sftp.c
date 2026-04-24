/*
 * sftp.c — single translation unit for the entire SFTP library.
 *
 * The library is a codec + two users (client and server) of it.  Building
 * the five logical source files as one TU lets the compiler see every
 * internal function and flatten the build/send/parse chain — there is no
 * cross-TU call overhead and non-public symbols stay file-local.
 *
 * Files assembled here, in dependency order:
 *
 *   sftp_str.c        str helpers (public: sftp_alloc_str / strdup / …)
 *   sftp_attr.c       file-attribute codec (public: sftp_fattr_* / …)
 *   sftp_pkt.c        low-level packet codec (sftp_get32 / append32 / …)
 *   sftp_common.c     state-independent helpers shared by client + server
 *                     (extension table, appendheader)
 *   sftp_client.c     the blocking SFTP client (sftpc_*)
 *   sftp_server.c     the server dispatcher (sftps_*)
 *
 * The old cross-TU shim pattern (one-line wrappers like `get32(state)`
 * → `sftp_get32(state->rxp)`) is gone — callers invoke the underlying
 * primitives directly.
 */

#include <assert.h>
#include <genwrap.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <threadwrap.h>
#include <time.h>
#include <xpendian.h>
#include <xpprintf.h>
#include <str_list.h>

#include "sftp.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

#include "sftp_str.c"
#include "sftp_pkt.c"
#include "sftp_attr.c"
#include "sftp_common.c"
#include "sftp_client.c"
#include "sftp_server.c"
