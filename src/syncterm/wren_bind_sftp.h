#ifndef WREN_BIND_SFTP_H
#define WREN_BIND_SFTP_H

/* Per-module declarations for the SFTP foreign surface — pulled in
 * by wren_bind.c so its BINDINGS table and wren_bind_lookup_class
 * can reference these functions.  Defined in wren_bind_sftp.c. */

#include "wren.h"

/* Foreign-class allocators / finalizers. */
void wren_sftp_entry_allocate(WrenVM *vm);
void wren_sftp_entry_finalize(void *data);
void wren_sftp_stat_allocate(WrenVM *vm);
void wren_sftp_stat_finalize(void *data);
void wren_sftp_handle_allocate(WrenVM *vm);
void wren_sftp_handle_finalize(void *data);
void wren_sftp_error_allocate(WrenVM *vm);
void wren_sftp_error_finalize(void *data);

/* SFTP class — synchronous getters and parking ops. */
void fn_SFTP_available(WrenVM *vm);
void fn_SFTP_pubdir(WrenVM *vm);
void fn_SFTP_realpath(WrenVM *vm);
void fn_SFTP_stat(WrenVM *vm);
void fn_SFTP_opendir(WrenVM *vm);
void fn_SFTP_readdir(WrenVM *vm);
void fn_SFTP_close(WrenVM *vm);
void fn_SFTP_open(WrenVM *vm);
void fn_SFTP_read(WrenVM *vm);
void fn_SFTP_write(WrenVM *vm);
void fn_SFTP_mkdir(WrenVM *vm);
void fn_SFTP_rmdir(WrenVM *vm);
void fn_SFTP_remove(WrenVM *vm);
void fn_SFTP_rename(WrenVM *vm);

/* SFTPEntry instance accessors. */
void fn_SFTPEntry_name(WrenVM *vm);
void fn_SFTPEntry_longname(WrenVM *vm);
void fn_SFTPEntry_size(WrenVM *vm);
void fn_SFTPEntry_mtime(WrenVM *vm);
void fn_SFTPEntry_isDir(WrenVM *vm);
void fn_SFTPEntry_hash(WrenVM *vm);
void fn_SFTPEntry_toString(WrenVM *vm);

/* SFTPStat instance accessors. */
void fn_SFTPStat_size(WrenVM *vm);
void fn_SFTPStat_mtime(WrenVM *vm);
void fn_SFTPStat_atime(WrenVM *vm);
void fn_SFTPStat_mode(WrenVM *vm);
void fn_SFTPStat_uid(WrenVM *vm);
void fn_SFTPStat_gid(WrenVM *vm);
void fn_SFTPStat_toString(WrenVM *vm);

/* SFTPError instance accessors. */
void fn_SFTPError_code(WrenVM *vm);
void fn_SFTPError_message(WrenVM *vm);
void fn_SFTPError_isTransient(WrenVM *vm);
void fn_SFTPError_serverStatus(WrenVM *vm);
void fn_SFTPError_toString(WrenVM *vm);

#endif /* WREN_BIND_SFTP_H */
