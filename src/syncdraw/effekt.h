#ifndef _EFFEKT_H_
#define _EFFEKT_H_

typedef struct teffekt {
	int             Effekt;
	int             Colortable[5][10];
}               teffekt;

extern teffekt         effect;
void draweffect(int x1, int y1, int x2, int y2);
void select_effekt(void);

#endif
