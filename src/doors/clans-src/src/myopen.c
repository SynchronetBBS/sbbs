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
#ifndef __unix__
# include <share.h>
# include <dos.h>
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "defines.h"
#include "language.h"
#include "myopen.h"
#include "system.h"
#include "video.h"

uint8_t serBuf[4096];
bool erRet;

static void cipher(void *, void *, size_t, unsigned char);

void MyOpen(char *szFileName, char *szMode, struct FileHeader *FileHeader)
/*
 * This function opens the file specified and sees if it is a .PAKfile
 * file or a regular DOS file.
 */
{
	FILE *fp;
	bool FoundFile = false;
	char szPakFileName[PATH_SIZE], szModFileName[PATH_SIZE], *pc;
	uint8_t fhBuf[BUF_SIZE_FileHeader];
	long lTemp;

	FileHeader->fp = NULL;
	strlcpy(szModFileName, szFileName, sizeof(szModFileName));

	if (szFileName[0] == '@') {
		// different .PAK file
		strlcpy(szPakFileName, &szFileName[1], sizeof(szPakFileName));

		// find the '/' char and put a null there

		pc = szPakFileName;

		while (*pc) {
			if (*pc == '/') {
				strlcpy(szModFileName, pc, sizeof(szModFileName));

				*pc = 0;
				break;
			}
			pc++;
		}

	}
	else {
		strlcpy(szPakFileName, "clans.pak", sizeof(szPakFileName));
	}

	// if file starts with / then it is in pakfile
	FileHeader->fp = NULL;
	if (szModFileName[0] == '/') {
		// look for the file
		// fp = _fsopen(szPakFileName, "rb", _SH_DENYWR);
		fp = fopen(szPakFileName, "rb");

		if (! fp)  {
			System_Error("Can't open CLANS.PAK file.\n");
			return;
		}

		for (;;) {
			if (fread(fhBuf, sizeof(fhBuf), 1, fp) == 0)
				break;
			s_FileHeader_d(fhBuf, sizeof(fhBuf), FileHeader);

			// od_printf("read in %s\n\r", FileHeader->szFileName);

			if (strcasecmp(FileHeader->szFileName, szModFileName) == 0) {
				FoundFile = true;
				break;
			}
			else  // go onto next file
				fseek(fp, FileHeader->lFileSize, SEEK_CUR);
		}

		// found file and at start of it, close file, reopen with szMode
		fclose(fp);

		if (FoundFile) {
			FileHeader->fp = fopen(szPakFileName, szMode);
			if (FileHeader->fp == NULL) {
				System_Error("Unable to open clans.pak");
				return;
			}
			fseek(FileHeader->fp, FileHeader->lStart, SEEK_SET);
		}
		else {
			// open file from dos
			fp = _fsopen(szModFileName, szMode, _SH_DENYWR);
			if (!fp) return;

			// read in file stats
			FileHeader->lStart = 0;

			fseek(fp, 0L, SEEK_END);
			lTemp = ftell(fp);
			if (lTemp < 0 || lTemp > INT32_MAX)
				System_Error("Invalid position in clans.pak");
			FileHeader->lEnd = FileHeader->lFileSize = (int32_t)lTemp;
			fseek(fp, 0L, SEEK_SET);

			FileHeader->fp = fp;
		}
		return;
	}
	else {
		// open file from dos
		fp = _fsopen(szModFileName, szMode, _SH_DENYWR);
		if (!fp)
			return;

		// read in file stats
		FileHeader->lStart = 0;

		fseek(fp, 0L, SEEK_END);
		lTemp = ftell(fp);
		if (lTemp < 0 || lTemp > INT32_MAX)
			System_Error("Invalid position in file");
		FileHeader->lEnd = FileHeader->lFileSize = (int32_t)lTemp;
		fseek(fp, 0L, SEEK_SET);

		FileHeader->fp = fp;

		return;
	}
}

bool EncryptWrite(void *Data, size_t DataSize, FILE *fp, char XorValue)
{
	if (!DataSize) {
		System_Error("EncryptWrite() called with 0 bytes\n");
	}

	if (!Data) {
		System_Error("EncryptWrite() called with NULL Data\n");
	}

	if (!XorValue) {
		System_Error("EncryptWrite() called with bad key\n");
	}

	/*  -- Removed the original Encrypt() function for simplicity
	    Encrypt(EncryptedData, (char *)Data, DataSize, XorValue);*/
	cipher(Data, Data, DataSize, (unsigned char)XorValue);

	return (fwrite(Data, DataSize, 1, fp) == 1);
}

bool EncryptRead(void *Data, size_t DataSize, FILE *fp, char XorValue)
{
	size_t Result;

	if (!DataSize) {
		System_Error("EncryptRead() called with 0 bytes\n");
	}

	if (!Data) {
		System_Error("EncryptRead() called with NULL Data\n");
	}

	if (!XorValue) {
		System_Error("EncryptRead() called with bad key\n");
	}

	Result = fread(Data, DataSize, 1, fp);

	/*  -- Removed the original Decrypt() function for simplicity
	    Decrypt((char *)Data, EncryptedData, DataSize, XorValue);*/
	if (Result == 1)
		cipher(Data, Data, DataSize, (unsigned char)XorValue);

	return Result == 1;
}

/* -- Replacement Encrypt/Decrypt function, reduces redundant code */
static void cipher(void *dest, void *src, size_t len, unsigned char xor_val)
{
	if (!dest || !src || !len || !xor_val)
		System_Error("Invalid Parameters to cipher");

	while (len--) {
		*(unsigned char *)dest = *(unsigned char *)src ^ xor_val;
		dest = (char *)dest + 1;
		src = (char *)src + 1;
	}
}
