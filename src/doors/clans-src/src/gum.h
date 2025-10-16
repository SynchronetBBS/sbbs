#ifndef GUM_H
#define GUM_H

extern long bytes_in, bytes_out;

void decode(FILE *input, FILE *output, void(*kputs)(char *));
int encode(FILE *input, FILE *output);
void ClearAll(void);

#endif
