/*
** by: Walter Bright via Usenet C newsgroup
**
** modified by: Bob Stout based on a recommendation by Ray Gardner
**
** modified by: David Gersic to deal with binary files
**
** There is no point in going to asm to get high speed file copies. Since it
** is inherently disk-bound, there is no sense (unless tiny code size is
** the goal). Here's a C version that you'll find is as fast as any asm code
** for files larger than a few bytes (the trick is to use large disk buffers):
*/

#include <stdio.h>
#include <stdlib.h>
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "wb_fapnd.h"

static int file_oper(const char *from, const char *to, const char *tmode)
{
	FILE *ffrom, *fto;
	size_t bufsiz;

	ffrom = _fsopen(from, "rb", _SH_DENYWR);
	if (!ffrom)
		return 1;

	fto = _fsopen(to, tmode, _SH_DENYRW);
	if (!fto)
		goto err;

	/* Use the largest buffer we can get    */

	for (bufsiz = 0x4000; bufsiz >= 128; bufsiz >>= 1) {
		void *buffer;

		buffer = malloc(bufsiz);
		if (buffer) {
			for (;;) {
				size_t n = fread(buffer, 1, bufsiz, ffrom);
				if (n == 0) {
					if (feof(ffrom)) {      /* if end of file       */
						fclose(fto);
						fclose(ffrom);
						free(buffer);
						return 0;       /* success              */
					}
					break;                  /* if error             */
				}
				if (1 != fwrite(buffer, n, 1, fto))
					break;
			}
			free(buffer);
			break;
		}
	}
	fclose(fto);
	remove(to);                               /* delete any partial file  */
err:
	fclose(ffrom);
	return 1;
}

int file_append(const char *from, const char *to)
{
	return file_oper(from, to, "ab");
}

int file_copy(const char *from, const char *to)
{
	return file_oper(from, to, "wb");
}
