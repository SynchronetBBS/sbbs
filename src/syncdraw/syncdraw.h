#ifndef _SYNCDRAW_H_
#define _SYNCDRAW_H_

#define UNDOPage 0
#define MaxLines 1000

extern unsigned char   Screen[4][MaxLines+1][160];
extern unsigned char   tabs[80];
extern unsigned char   CursorPos[80];
extern int             CursorX, CursorY, FirstLine, x, y, LastLine;
extern unsigned char   Attribute, FontMode, Undo, FontTyped,
                		InsertMode, EliteMode, SaveSauce,
		                ActiveCharset, ActivePage;
extern unsigned char   FullScreen;
extern unsigned short  DrawMode;
extern char           *stri[20];
extern char           *CharSet[12];
extern int				Dragging;

void		Statusline(void);
void		CursorHandling(int ch);
void		CursorCheck(void);
void		ShowScreen(int a1, int a2);
int			SelectFont(void);
void		SelectOutline(void);
void		putchacter(unsigned char c);
unsigned char	asciitable(void);
int			SelectCharSet(void);

#endif
