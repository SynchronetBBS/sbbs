#ifndef CONN_CONPTY_H
#define CONN_CONPTY_H
#ifdef _WIN32
#if NTDDI_VERSION >= 0x0A000006

#define HAS_CONPTY

int conpty_connect(struct bbslist *bbs);
int conpty_close(void);

#endif
#endif
#endif
