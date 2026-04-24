#ifndef _FILEPICK_H_
#define _FILEPICK_H_

#include "dirwrap.h"
#include "uifc.h"

/*
 * filepick() — single-file (or single-directory) browser.
 * filepick_multi() — multi-file browser; user tags any number of files
 *   from any number of directories, then OK confirms the whole batch.
 *
 * Both populate fp->files (count) and fp->selected (heap array of full paths)
 * and must be released with filepick_free().
 *
 * Return value:
 *   -1  error or escape (fp unchanged)
 *    0  user cancelled (fp unchanged)
 *   >0  number of selected paths in fp->selected (always 1 for filepick(),
 *       1 or more for filepick_multi())
 */

/* opts bit flags */
#define UIFC_FP_MSKNOCHG    (1 << 0)              /* Don't allow user to change the file mask */
#define UIFC_FP_MSKCASE     (1 << 1)              /* Make the mask case-sensitive */
#define UIFC_FP_UNIXSORT    (1 << 2)              /* Sort case-sensitively (default is case-insensitive) */
#define UIFC_FP_DIRSEL      (1 << 3)              /* Select a directory instead of a file */
#define UIFC_FP_PATHEXIST   (1 << 4)              /* Selected path must exist */
#define UIFC_FP_FILEEXIST   (1 << 5)              /* Selected file must exist */
#define UIFC_FP_SHOWHIDDEN  (1 << 6)              /* Include dot-files in directory and file lists */

/* The following can NOT be combined with multi-select (filepick_multi returns -1 if set) */
#define UIFC_FP_ALLOWENTRY  (1 << 8)              /* Allow user to type a file/path name */
#define UIFC_FP_OVERPROMPT  (1 << 9)              /* Prompt "Overwrite?" if file exists */
#define UIFC_FP_CREATPROMPT (1 << 10)             /* Prompt "Create?" if file does not exist */

struct file_pick {
	int    files;
	char **selected;
};

int filepick      (uifcapi_t *api, const char *title, struct file_pick *fp,
                   const char *initial_dir, const char *default_mask, int opts);
int filepick_multi(uifcapi_t *api, const char *title, struct file_pick *fp,
                   const char *initial_dir, const char *default_mask, int opts);
int filepick_free (struct file_pick *fp);

#endif
