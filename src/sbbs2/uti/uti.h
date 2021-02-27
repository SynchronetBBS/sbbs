/* UTI.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

extern char scrnbuf[4000];
extern struct text_info txtinfo;
extern int logfile;

#define PREPSCREEN gettext(1,1,80,25,scrnbuf); gettextinfo(&txtinfo); \
	textattr(LIGHTGRAY); clrscr()

#define VER "2.30"

void uti_init(char *name,int argc, char **argv);
