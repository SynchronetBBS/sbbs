#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "xpdev.h"		       /* truncsp() */
#include "fileutils.h"

char           *
readline(char *buf, size_t size, FILE * infile)
{
    fgets(buf, size, infile);
    truncsp(buf);
    return (buf);
}

double
readfloat(float deflt, FILE * infile)
{
    char            buf[101];
    int             pos = 0;
    int             rd;
    double          ret;
    buf[0] = 0;
    while ((rd = fgetc(infile)) != EOF) {
	if (rd == '\r')
	    continue;
	if (rd == '\n') {
	    ungetc('\n', infile);
	    break;
	}
	/* Skip leading spaces */
	if (!pos && isspace(rd))
	    continue;
	if (isspace(rd))
	    break;
	buf[pos++] = rd;
    }
    if (!pos)
	return (deflt);
    buf[pos++] = 0;
    ret = strtod(buf, NULL);
    return (ret);
}

QWORD
readnumb(QWORD deflt, FILE * infile)
{
    char            buf[101];
    int             pos = 0;
    int             rd;
    QWORD           ret;
    buf[0] = 0;

    while ((rd = fgetc(infile)) != EOF) {
	if (rd == '\r')
	    continue;
	if (rd == '\n') {
	    ungetc('\n', infile);
	    break;
	}
	/* Skip leading spaces */
	if (!pos && isspace(rd))
	    continue;
	if (isspace(rd))
	    break;
	buf[pos++] = rd;
    }
    if (!pos)
	return (deflt);
    buf[pos++] = 0;
    ret = strtoull(buf, NULL, 10);
    return (ret);
}

void
endofline(FILE * infile)
{
    int             rd;
    while ((rd = fgetc(infile)) != EOF) {
	if (rd == '\n');
	break;
    }
}
