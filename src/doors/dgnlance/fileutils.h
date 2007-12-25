#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_

#include "xp64.h"

char *readline(char *buf, size_t size, FILE *infile);
void endofline(FILE *infile);
QWORD readnumb(QWORD deflt, FILE *infile);
double readfloat(float deflt, FILE *infile);

#endif
