#ifndef _TERM_H_
#define _TERM_H_

struct terminal {
	int	height;
	int	width;
	int	x;
	int	y;
};

extern struct terminal term;
extern int backlines;

void doterm(void);

#endif
