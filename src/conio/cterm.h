#ifndef _TERM_H_
#define _TERM_H_

#ifdef __cplusplus
extern "C" {
#endif
void cterm_init(int height, int width, int xpos, int ypos, int backlines, unsigned char *scrollback);
char *cterm_write(unsigned char *buf, int buflen, char *retbuf, int retsize);
void cterm_end(void);
#ifdef __cplusplus
}
#endif

#endif
