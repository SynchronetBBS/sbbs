#ifndef _SYNCTERM_H_
#define _SYNCTERM_H_

#include "bbslist.h"

extern char *inpath;
extern char *syncterm_version;
void parse_url(char *url, struct bbslist *bbs);

#endif
