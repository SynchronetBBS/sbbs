#ifndef WREN_BIND_FS_H
#define WREN_BIND_FS_H

/* Per-module declarations for the Directory / File / Host foreign
 * surface — pulled in by wren_bind.c so its BINDINGS table and
 * wren_bind_lookup_class can reference these functions.  Defined in
 * wren_bind_fs.c. */

#include "wren.h"

/* Foreign-class allocators / finalizers. */
void wren_directory_allocate(WrenVM *vm);
void wren_directory_finalize(void *data);
void wren_file_allocate(WrenVM *vm);
void wren_file_finalize(void *data);
void wren_file_error_allocate(WrenVM *vm);
void wren_file_error_finalize(void *data);

/* Directory instance methods. */
void fn_Directory_contains(WrenVM *vm);
void fn_Directory_list(WrenVM *vm);
void fn_Directory_create(WrenVM *vm);
void fn_Directory_createDir(WrenVM *vm);
void fn_Directory_delete(WrenVM *vm);
void fn_Directory_toString(WrenVM *vm);

/* File instance methods. */
void fn_File_open(WrenVM *vm);
void fn_File_close(WrenVM *vm);
void fn_File_readBytes_1(WrenVM *vm);
void fn_File_readBytes_2(WrenVM *vm);
void fn_File_read(WrenVM *vm);
void fn_File_writeBytes_1(WrenVM *vm);
void fn_File_writeBytes_2(WrenVM *vm);
void fn_File_write(WrenVM *vm);
void fn_File_readLine(WrenVM *vm);
void fn_File_writeLine(WrenVM *vm);
void fn_File_offset_get(WrenVM *vm);
void fn_File_offset_set(WrenVM *vm);
void fn_File_size(WrenVM *vm);
void fn_File_mtime(WrenVM *vm);
void fn_File_mtime_set(WrenVM *vm);
void fn_File_isOpen(WrenVM *vm);
void fn_File_sha1(WrenVM *vm);
void fn_File_md5(WrenVM *vm);
void fn_File_token(WrenVM *vm);
void fn_File_toString(WrenVM *vm);

/* FileError instance accessors. */
void fn_FileError_code(WrenVM *vm);
void fn_FileError_errno(WrenVM *vm);
void fn_FileError_message(WrenVM *vm);
void fn_FileError_toString(WrenVM *vm);

/* Host class methods. */
void fn_Host_cacheDirectory(WrenVM *vm);
void fn_Host_downloadDir(WrenVM *vm);
void fn_Host_uploadPath(WrenVM *vm);
void fn_Host_pickFile(WrenVM *vm);
void fn_Host_pickFiles(WrenVM *vm);
void fn_Host_pickSavePath(WrenVM *vm);
void fn_Host_openLocalFile(WrenVM *vm);

/* FileError result codes — exposed so other bindings (Capture,
 * CTerm.saveScreenshot, …) can build FileError typed-results into
 * their result slot.  Values are part of the Wren API surface and
 * MUST stay stable. */
enum file_err_code {
	FILE_ERR_OK = 0,
	FILE_ERR_OPEN_FAILED,
	FILE_ERR_WRITE_FAILED,
	FILE_ERR_STAT_FAILED,
	FILE_ERR_MMAP_FAILED,
	FILE_ERR_OOM,
	FILE_ERR_VANISHED,
	FILE_ERR_RESOLVE_FAILED,
};

/* Build a FileError into slot `slot` (used as both type-result and
 * scratch).  `errno_val == 0` when the failure didn't come from libc.
 * Never aborts.  Caller returns to the VM immediately after — the
 * slot's contents become the binding's typed-result. */
void file_build_error(WrenVM *vm, int slot, enum file_err_code code,
                      int errno_val, const char *msg);

/* Consume the write-consent of a File foreign in the given slot;
 * see wren_bind_fs.c for the contract.  Returns an owned stdio
 * FILE* on success (caller must fclose), or NULL on error (with
 * either a thrown fiber error or a FileError in slot 0).  Used by
 * Capture.start / CTerm.saveScreenshot so each does its own
 * direct stdio rather than going through fn_File_open. */
struct wren_file;
#include <stdio.h>
FILE *wren_file_consume_write(WrenVM *vm, int slot, const char **out_path);

/* Read-authorized path accessor for a File foreign in the given slot.
 * Returns the absolute path string on success (still owned by the
 * File foreign — do NOT free).  Returns NULL when the slot does
 * not hold a File, the File is dead, or it lacks read rights.  Use
 * this when a foreign needs to hand the path to a C API that takes
 * a const char * (e.g. ciolib_loadfont) but doesn't need write consent. */
const char *wren_file_read_path(WrenVM *vm, int slot);

#endif /* WREN_BIND_FS_H */
