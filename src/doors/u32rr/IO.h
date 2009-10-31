#ifndef _IO_H_
#define _IO_H_

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

extern const char *black;	// 0
extern const char *blue;	// 1
extern const char *green;	// 2
extern const char *cyan;	// 3
extern const char *red;		// 4
extern const char *magenta;	// 5
extern const char *brown;	// 6
extern const char *lgray;	// 7
extern const char *dgray;	// 8
extern const char *lblue;	// 9
extern const char *lgreen;	// 10
extern const char *lcyan;	// 11
extern const char *lred;	// 12
extern const char *lmagenta;// 13
extern const char *yellow;	// 14
extern const char *white;	// 15
extern const char *blread;	// 99

extern int lines;
extern int cols;

void colorcode(const char *color);
void outstr(const char *str);
void halt(void);
void normal_exit(void);
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
#define DL(...)		({ d(__VA_ARGS__, NULL); nl(); })
#define F(...)		f(__VA_ARGS__, NULL)

#endif
