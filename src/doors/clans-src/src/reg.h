#ifndef THE_CLANS__REG___H
#define THE_CLANS__REG___H

#define NTRUE     581           // "True" used for reg info
#define NFALSE          0

void Jumble(char *szString);

int16_t IsRegged(char *szSysopName, char *szBBSName, char *szRegCode);

void UnregMessage(void);

void Register(void);

#endif
