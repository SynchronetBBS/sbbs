#ifndef _SAUCE_H_
#define _SAUCE_H_

#include <stdio.h>

typedef struct {
	char            id[6];
	char            Version[2];
	char            Title[36];
	char            Author[21];
	char            Group[21];
	char            Date[9];
	long            FileSize;
	char            DataType;
	char            FileType;
	short           TInfo1, TInfo2, TInfo3, TInfo4;
	char            Comments;
	char            Flags;
	char            Filler[23];
}               SauceRecord;

extern SauceRecord     SauceDescr;
void AppendSauce(FILE * fp);
void ReadSauce(FILE * fp);
void SauceSet(void);

#endif
