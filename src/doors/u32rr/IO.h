#ifndef _IO_H_
#define _IO_H_

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

extern const char *black;
extern const char *blue;
extern const char *green;
extern const char *cyan;
extern const char *red;
extern const char *magenta;
extern const char *brown;
extern const char *lgray;
extern const char *dgray;
extern const char *lblue;
extern const char *lgreen;
extern const char *lcyan;
extern const char *lred;
extern const char *lmagenta;
extern const char *yellow;
extern const char *white;
extern const char *blread;

extern int lines;
extern int cols;

void colorcode(const char *color);
void outstr(const char *str);
void halt(void);
void nl(void);
void d(const char *str, ...);
void f(const char *color, const char *fmt, ...);
void upause(void);
char gchar(void);
void clr(void);
void menu2(const char *str);
void menu(const char *str);
bool confirm(const char *prompt, char dflt);
void dbs(int);
long get_a_number(long min, long max, long dflt);
long get_number(long min, long max);

#define D(...)		d(__VA_ARGS__, NULL)
#define DL(...)		d(__VA_ARGS__, NULL); nl()
#define F(...)		f(__VA_ARGS__, NULL)

#endif
