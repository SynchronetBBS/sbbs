/*
**  Header file for portable file functions
*/

#ifndef THE_CLANS__SNIPFILE___H
#define THE_CLANS__SNIPFILE___H

#include <stdio.h>

int32_t   flength(char *fname);                          /* Ansiflen.C     */
FILE * cant(char *fname, char *fmode);                /* Ferrorf.C      */
int16_t    fcompare(const char *fnam1, const char *fnam2);/* Fcompare.C     */
int32_t   fcopy(char *dest, char *source);               /* Fcopy.C        */
int32_t   ffsearch(FILE *fp, const char *pattern,
				const size_t size, int16_t N);               /* Srchfile.C     */
int32_t   rfsearch(FILE *fp, const char *pattern,
				const size_t size, int16_t N);               /* Srchfile.C     */
void   show_text_file(char *txt);                     /* Textmod.C      */
int    file_copy(char *from, char *to);               /* Wb_Fcopy.C     */
int    file_append(char *from, char *to);             /* Wb_Fapnd.C     */

#endif /* THE_CLANS__SNIPFILE___H */
