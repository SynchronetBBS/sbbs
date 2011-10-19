/*
**  Header file for portable file functions
*/

#ifndef SNIPFILE__H
#define SNIPFILE__H

#include <stdio.h>

long   flength(char *fname);                          /* Ansiflen.C     */
FILE * cant(char *fname, char *fmode);                /* Ferrorf.C      */
_INT16    fcompare(const char *fnam1, const char *fnam2);/* Fcompare.C     */
long   fcopy(char *dest, char *source);               /* Fcopy.C        */
long   ffsearch(FILE *fp, const char *pattern,
				const size_t size, _INT16 N);               /* Srchfile.C     */
long   rfsearch(FILE *fp, const char *pattern,
				const size_t size, _INT16 N);               /* Srchfile.C     */
void   show_text_file(char *txt);                     /* Textmod.C      */
int    file_copy(char *from, char *to);               /* Wb_Fcopy.C     */
int    file_append(char *from, char *to);             /* Wb_Fapnd.C     */

#endif /* SNIPFILE__H */
