/*

The Clans BBS Door Game
Copyright (C) 1997-2002 Allen Ussher

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
 * "File opening" calls.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <share.h>
#include <dos.h>
#endif

#include "defines.h"
#include "myopen.h"
#include "video.h"
#include "language.h"
#include "system.h"

void cipher(void *, void *, size_t, unsigned char);

void MyOpen(char *szFileName, char *szMode, struct FileHeader *FileHeader)
/*
 * This function opens the file specified and sees if it is a .PAKfile
 * file or a regular DOS file.
 */
{
	FILE *fp;
	BOOL FoundFile = FALSE;
	char szPakFileName[50], szModFileName[50], *pc;

	strcpy(szModFileName, szFileName);

	if (szFileName[0] == '@') {
		// different .PAK file
		strcpy(szPakFileName, &szFileName[1]);

		// find the '/' char and put a null there

		pc = szPakFileName;

		while (*pc) {
			if (*pc == '/') {
				strcpy(szModFileName, pc);

				*pc = 0;
				break;
			}
			pc++;
		}

	}
	else {
		strcpy(szPakFileName, "clans.pak");
	}

	// if file starts with / then it is in pakfile
	FileHeader->fp = NULL;
	if (szModFileName[0] == '/') {
		// look for the file
		// fp = _fsopen(szPakFileName, "rb", SH_DENYWR);
		fp = fopen(szPakFileName, "rb");

		if (! fp)  {
			System_Error("Can't open CLANS.PAK file.\n");
		}

		for (;;) {
			if (fread(FileHeader, sizeof(struct FileHeader), 1, fp) == 0)
				break;

			// od_printf("read in %s\n\r", FileHeader->szFileName);

			if (stricmp(FileHeader->szFileName, szModFileName) == 0) {
				FoundFile = TRUE;
				break;
			}
			else  // go onto next file
				fseek(fp, FileHeader->lFileSize, SEEK_CUR);
		}

		// found file and at start of it, close file, reopen with szMode
		fclose(fp);

		if (FoundFile) {
			FileHeader->fp = fopen(szPakFileName, szMode);
			fseek(FileHeader->fp, FileHeader->lStart, SEEK_SET);
		}
		else {
			// open file from dos
			fp = _fsopen(szModFileName, szMode, SH_DENYWR);
			if (!fp) return;

			// read in file stats
			FileHeader->lStart = 0;

			fseek(fp, 0L, SEEK_END);
			FileHeader->lEnd = FileHeader->lFileSize = ftell(fp);
			fseek(fp, 0L, SEEK_SET);

			FileHeader->fp = fp;
		}
		return;
	}
	else {
		// open file from dos
		fp = _fsopen(szModFileName, szMode, SH_DENYWR);
		if (!fp) return;

		// read in file stats
		FileHeader->lStart = 0;

		fseek(fp, 0L, SEEK_END);
		FileHeader->lEnd = FileHeader->lFileSize = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		FileHeader->fp = fp;

		return;
	}
}

void EncryptWrite(void *Data, long DataSize, FILE *fp, char XorValue)
{
	char *EncryptedData;

	if (DataSize == 0) {
		System_Error("EncryptWrite() called with 0 bytes\n");
	}

	//printf("EncryptWrite(): Data size is %d\n", (_INT16)DataSize);
	EncryptedData = malloc((_INT16)DataSize);
	CheckMem(EncryptedData);

	/*  -- Removed the original Encrypt() function for simplicity
	    Encrypt(EncryptedData, (char *)Data, DataSize, XorValue);*/
	cipher(EncryptedData, Data, DataSize, (unsigned char)XorValue);

	fwrite(EncryptedData, (_INT16)DataSize, 1, fp);

	free(EncryptedData);
}

_INT16 EncryptRead(void *Data, long DataSize, FILE *fp, char XorValue)
{
	char *EncryptedData;
	_INT16 Result;

	if (DataSize == 0) {
		System_Error("EncryptRead() called with 0 bytes\n");
	}

	EncryptedData = malloc(DataSize);
	CheckMem(EncryptedData);

	Result = fread(EncryptedData, 1, DataSize, fp);

	/*  -- Removed the original Decrypt() function for simplicity
	    Decrypt((char *)Data, EncryptedData, DataSize, XorValue);*/
	if (Result)
		cipher(Data, EncryptedData, DataSize, (unsigned char)XorValue);

	free(EncryptedData);

	return Result;
}

/* -- Replacement Encrypt/Decrypt function, reduces redundant code */
void cipher(void *dest, void *src, size_t len, unsigned char xor_val)
{
	if (!dest || !src || !len || !xor_val)
		System_Error("Invalid Parameters to cipher");

	while (len--) {
		*(char *)dest = *(char *)src ^ xor_val;
		dest = (char *)dest + 1;
		src = (char *)src + 1;
	}
}
