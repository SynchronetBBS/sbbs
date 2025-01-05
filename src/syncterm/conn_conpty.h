#ifndef CONN_CONPTY_H
#define CONN_CONPTY_H
#ifdef _WIN32

// We need to #include windows.h for NTDDI_VERSION to be rightified.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if NTDDI_VERSION >= 0x0A000006

#define HAS_CONPTY

int conpty_connect(struct bbslist *bbs);
int conpty_close(void);

#endif
#endif
#endif
