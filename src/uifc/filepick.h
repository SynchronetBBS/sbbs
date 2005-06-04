#ifndef _FILEPICK_H_
#define _FILEPICK_H_

#include "dirwrap.h"
#include "uifc.h"

#define UIFC_FP_MSKNOCHG	(1<<0)				/* Don't allow user to change the file mask */
#define	UIFC_FP_MSKCASE		(1<<1)				/* Make the mask case-sensitive */
#define UIFC_FP_UNIXSORT	(1<<2)				/* ToDo: Sort upper-case first, then lower-case */
#define UIFC_FP_DIRSEL		(1<<3)				/* ToDo: Select directory, not file */
#define UIFC_FP_PATHEXIST	(1<<4)				/* Do not allow selection of non-existant paths */
#define UIFC_FP_FILEEXIST	(1<<5)				/* File must exist */
#define UIFC_FP_SHOWHIDDEN	(1<<6)				/* Show hidden files */
#define UIFC_FP_MULTI		((1<<7)|UIFC_FP_PATHEXIST|UIFC_FP_FILEEXIST)
												/* ToDo: Allow multiple files to be chosen */

/* The following can NOT be used with multi selects (Returns an error) */
#define UIFC_FP_ALLOWENTRY	(1<<8)				/* Allow user to type a file/path name */
#define UIFC_FP_OVERPROMPT	(1<<9)				/* Prompt "Overwrite?" if file exists */
#define UIFC_FP_CREATPROMPT	(1<<10)				/* Prompt "Create?" if file does not exist */

struct file_pick {
	int		files;
	char	**selected;
};

int filepick(uifcapi_t *api, char *title, struct file_pick *, char *initial_dir, char *default_mask, int opts);
int filepick_free(struct file_pick *);

#endif
