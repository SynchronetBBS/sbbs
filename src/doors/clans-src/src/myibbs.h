#ifndef THE_CLANS__MYIBBS___H
#define THE_CLANS__MYIBBS___H

#include "interbbs.h"

void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource);
tIBResult IBSendFileAttach(tIBInfo *pInfo, char *pszDestNode, const char *pszFileName);

#endif
