#ifndef _TERM_H_
#define _TERM_H_

struct terminal {
	int	height;
	int	width;
	int	x;
	int	y;
	char *buffer;
	int	xpos;
	int	ypos;
	int	attr;
	int save_xpos;
	int save_ypos;
	char	escbuf[1024];
	int	sequence;
	int music;
	char *scrollback;
	int backpos;
};

extern struct terminal term;

void doterm(void);

#endif
