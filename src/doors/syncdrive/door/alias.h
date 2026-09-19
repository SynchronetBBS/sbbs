/* alias.h -- a BBS alias as Test Drive's 15-character high-score name. */
#ifndef SYNCDRIVE_ALIAS_H_
#define SYNCDRIVE_ALIAS_H_

#define ALIAS_FIELD 15

void alias_sanitize(const char *in, char out[ALIAS_FIELD + 1]);

#endif
