#ifndef _V3_MCI_H_
#define _V3_MCI_H_

size_t commafmt_s32(char* buf, int bufsize, s32 N);
size_t commafmt_u32(char* buf, int bufsize, u32 N);

void text(char *lnum);
void mcigettext(char *lnum, char *outtext);
void mcimacros(char *ttxt);
void mcicolor(char *ttxt, int ansi);
void mcihidden(char *ttxt);
void mcifuncts(char *ttxt);
char *strrepl(char *Str, size_t BufSiz, const char *OldStr, const char *NewStr);
void mci(char *fmt, ...);
void mciEseries(char *ttxt);
void stripCR(char *ttxt);
int mcistrlen(char *ttxt);
void mciAnd(char *ttxt);

// argument writing functions
void char_2A(int numa, char chr);
void str_2A(int numa, char *ttxt);
void s16_2A(int numa, s16 num);
void u16_2A(int numa, u16 num);
void s32_2A(int numa, s32 lnum);
void u32_2A(int numa, u32 lnum);

void s32_2C(int numa, s32 lnum);
void u32_2C(int numa, u32 lnum);

#endif

