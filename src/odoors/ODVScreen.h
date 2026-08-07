/* Private virtual session-screen support.  This is not part of the
 * OpenDoors public interface. */
#ifndef _INC_ODVSCREEN
#define _INC_ODVSCREEN

#include "OpenDoor.h"

typedef struct
{
   INT winleft;
   INT wintop;
   INT winright;
   INT winbottom;
   BYTE attribute;
   INT curx;
   INT cury;
   BOOL scrolling;
} tODVScreenInfo;

void ODSessionScreenInitialize(INT nMinimumWidth, INT nMinimumHeight);
void ODSessionScreenShutdown(void);
BOOL ODSessionScreenAvailable(void);
INT ODSessionScreenError(void);
INT ODSessionScreenWidth(void);
INT ODSessionScreenHeight(void);

void ODSessionScreenGetInfo(tODVScreenInfo *pInfo);
void ODSessionScreenSetBoundary(INT nLeft, INT nTop, INT nRight, INT nBottom);
void ODSessionScreenSetCursorPos(INT nColumn, INT nRow);
void ODSessionScreenSetAttribute(BYTE btAttribute);
void ODSessionScreenEnableScrolling(BOOL bEnable);
void ODSessionScreenDisplayChar(unsigned char chToOutput);
void ODSessionScreenDisplayBuffer(const char *pBuffer, INT nCharsToDisplay);
void ODSessionScreenDisplayString(const char *pszString);
void ODSessionScreenClear(void);
void ODSessionScreenClearToEndOfLine(void);
BOOL ODSessionScreenGetText(INT nLeft, INT nTop, INT nRight, INT nBottom,
   void *pBuffer);
BOOL ODSessionScreenPutText(INT nLeft, INT nTop, INT nRight, INT nBottom,
   const void *pBuffer);
BOOL ODSessionScreenCopyText(INT nLeft, INT nTop, INT nRight, INT nBottom,
   INT nDestColumn, INT nDestRow);

void ODSessionScreenPresent(void);
void ODSessionScreenBeginEmulation(void);
void ODSessionScreenEndEmulation(void);
BOOL ODSessionScreenIsEmulating(void);

DWORD ODSessionScreenSnapshotSize(void);
BOOL ODSessionScreenSave(void *pBuffer, DWORD dwBufferSize);
BOOL ODSessionScreenRestore(const void *pBuffer, DWORD dwBufferSize);

#endif /* _INC_ODVSCREEN */
