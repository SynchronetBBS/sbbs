#ifndef _CTERM_H_
#define _CTERM_H_

struct cterminal {
	int	height;
	int	width;
	int	x;
	int	y;
	char *buffer;
	int	attr;
	int save_xpos;
	int save_ypos;
	char	escbuf[1024];
	int	sequence;
	char	musicbuf[1024];
	int music;
	char *scrollback;
	int backpos;
	int backlines;
	int	xpos;
	int ypos;
};

#ifdef __cplusplus
extern "C" {
#endif

extern struct cterminal cterm;

void cterm_init(int height, int width, int xpos, int ypos, int backlines, unsigned char *scrollback);
char *cterm_write(unsigned char *buf, int buflen, char *retbuf, int retsize);
void cterm_end(void);
#ifdef __cplusplus
}
#endif

#endif
