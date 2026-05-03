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
void fn_Host_openLocalFile(WrenVM *vm);

#endif /* WREN_BIND_FS_H */
