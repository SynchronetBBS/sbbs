/*
 * MyOpen ADT
 */

struct PACKED FileHeader {
	FILE *fp;     // used only when reading the file in
	char szFileName[30];
	long lStart, lEnd, lFileSize;
} PACKED;

void MyOpen(char *szFileName, char *szMode, struct FileHeader *FileHeader);
/*
 * This function opens the file specified and sees if it is a .PAKfile
 * file or a regular DOS file.
 */

void EncryptWrite(void *Data, long DataSize, FILE *fp, char XorValue);
_INT16 EncryptRead(void *Data, long DataSize, FILE *fp, char XorValue);


#define EXTRABYTES      10L
