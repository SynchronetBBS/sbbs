#ifndef _MISCFUNCTIONS_H_
#define _MISCFUNCTIONS_H_

void CopyScreen(int p1, int p2);
void SaveScreen(void);
void InsLine(void);
void DelLine(void);
void InsCol(void);
void DelCol(void);
void Colors(unsigned char col);
void CoolWrite(int x, int y, char *a);
void CodeWrite(int x, int y, char *a);
unsigned char SelectColor(void);
unsigned char QuickColor(void);
char translate(char a);
char *inputfield(char *Str, int length, int x1, int y);
void about(void);
int bufprintf(char *buf, int attr, char *fmat, ...);

#endif
