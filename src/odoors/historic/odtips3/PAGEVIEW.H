/* pageview.h - Include file for PagedViewer() function. */

/* Manifest constant. */
#define NO_LINE -1

/* Boolean definitions. */
#ifndef BOOL
typedef int BOOL;
#endif

/* Prototype for the function. */
int PagedViewer(
   int nInitialLine,
   int nTotalLines,
   void (*pDisplayCallback)(int nLine, void *pCallbackData),
   void *pCallbackData,
   BOOL bAllowSelection,
   char *pszTitle,
   int nPageSize);
