#ifndef THE_CLANS__GUM___H
#define THE_CLANS__GUM___H

extern long bytes_in, bytes_out;

void decode(FILE *input, FILE *output, void(*kputs)(const char *));
int encode(FILE *input, FILE *output);
void ClearAll(void);

#endif
