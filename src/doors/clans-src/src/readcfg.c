#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __unix__
# include <share.h>
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "parsing.h"
#include "readcfg.h"

struct config Config;

// Someone else needs to provide these...
void CheckMem(void *Test);
void System_Error(char *szErrorMsg);

#define MAX_CONFIG_WORDS          17

char *papszConfigKeyWords[MAX_CONFIG_WORDS] = {
	"SysopName",
	"BBSName",
	"UseLog",
	"ScoreANSI",
	"ScoreASCII",
	"Node",
	"DropDirectory",
	"UseFOSSIL",
	"SerialPortAddr",
	"SerialPortIRQ",
	"BBSId",
	"NetmailDir",
	"InboundDir",
	"MailerType",
	"InterBBS",
	"bangshangalang\xff\xff",
	"OutputSemaphore"
};

void AddInboundDir(const char *dir)
{
	char **new = realloc(Config.szInboundDirs, sizeof(Config.szInboundDirs[0]) * (Config.NumInboundDirs + 1));
	size_t dirlen = strlen(dir);
	bool addSlash = false;
	CheckMem(new);
	Config.szInboundDirs = new;
	if (dirlen && dir[dirlen - 1] != '\\' && dir[dirlen - 1] != '/')
		addSlash = true;
	Config.szInboundDirs[Config.NumInboundDirs] = malloc(dirlen + addSlash + 1);
	CheckMem(Config.szInboundDirs[Config.NumInboundDirs]);
	memcpy(Config.szInboundDirs[Config.NumInboundDirs], dir, dirlen);
	if (addSlash)
		Config.szInboundDirs[Config.NumInboundDirs][dirlen++] = '/';
	Config.szInboundDirs[Config.NumInboundDirs][dirlen] = 0;
	Config.NumInboundDirs++;
}

bool Config_Init(int16_t Node, struct NodeData *(*getNodeData)(int))
/*
 * Loads data from .CFG file into Config.
 *
 */
{
	FILE *fpConfigFile;
	char szConfigName[40], szConfigLine[255];
	char *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	int16_t iKeyWord, iCurrentNode = 1;
	struct NodeData *currNode = NULL;

	// --- Set defaults
	strlcpy(szConfigName, "clans.cfg", sizeof(szConfigName));
	strlcpy(Config.szScoreFile[0], "scores.asc", sizeof(Config.szScoreFile[0]));
	strlcpy(Config.szScoreFile[1], "scores.ans", sizeof(Config.szScoreFile[1]));

	fpConfigFile = _fsopen(szConfigName, "rt", SH_DENYWR);
	if (!fpConfigFile) {
		/* file not found! error */
		return false;
	}

	for (;;) {
		/* read in a line */
		if (fgets(szConfigLine, 255, fpConfigFile) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szConfigLine;
		ParseLine(pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;

		GetToken(pcCurrentPos, szToken);

		/* Loop through list of keywords */
		for (iKeyWord = 0; iKeyWord < MAX_CONFIG_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (stricmp(szToken, papszConfigKeyWords[iKeyWord]) == 0) {
				/* Process config token */
				switch (iKeyWord) {
					case 0 :  /* sysopname */
						strlcpy(Config.szSysopName, pcCurrentPos, sizeof(Config.szSysopName));
						break;
					case 1 :  /* bbsname */
						strlcpy(Config.szBBSName, pcCurrentPos, sizeof(Config.szBBSName));
						break;
					case 2 :  /* use log? */
						if (stricmp(pcCurrentPos, "Yes") == 0)
							Config.UseLog = true;
						break;
					case 3 :  /* ansi file */
						strlcpy(Config.szScoreFile[1], pcCurrentPos, sizeof(Config.szScoreFile[1]));
						break;
					case 4 :  /* ascii file */
						strlcpy(Config.szScoreFile[0], pcCurrentPos, sizeof(Config.szScoreFile[0]));
						break;
					case 5 :  /* node = ? */
						iCurrentNode = atoi(pcCurrentPos);
						if (getNodeData)
							currNode = getNodeData(iCurrentNode);
						break;
					case 6 :  /* dropdirectory = ? */
						if (getNodeData)
							strlcpy(currNode->dropDir, pcCurrentPos, sizeof(currNode->dropDir));
						if (Node == iCurrentNode) {
							Config.pszInfoPath = strdup(pcCurrentPos);
						}
						break;
					case 7 :  /* usefossil */
						if (currNode) {
							if (stricmp(pcCurrentPos, "No") == 0)
								currNode->fossil = false;
							else
								currNode->fossil = true;
						}
						if (Node == iCurrentNode) {
							if (stricmp(pcCurrentPos, "No") == 0) {
								/* do not use fossil */
								Config.NoFossil = true;
							}
						}
						break;
					case 8 :  /* serial port addr */
						if (currNode)
							currNode->addr = atoi(pcCurrentPos);
						if (Node == iCurrentNode) {
							//printf("Not yet used\n");
						}
						break;
					case 9 :  /* serial port irq */
						if (currNode)
                                                        currNode->irq = atoi(pcCurrentPos);
						if (Node == iCurrentNode) {
							if (stricmp(pcCurrentPos, "Default") != 0)
								Config.ComIRQ = atoi(pcCurrentPos);
						}
						break;
					case 10 : /* BBS Id */
						Config.BBSID = atoi(pcCurrentPos);
						break;
					case 11 : /* netmail dir */
						strlcpy(Config.szNetmailDir, pcCurrentPos, sizeof(Config.szNetmailDir));

						/* remove '\' if last char is it */
						if (Config.szNetmailDir [ strlen(Config.szNetmailDir) - 1] == '\\' || Config.szNetmailDir [strlen(Config.szNetmailDir) - 1] == '/')
							Config.szNetmailDir [ strlen(Config.szNetmailDir) - 1] = 0;
						break;
					case 12 : /* inbound dir */
						AddInboundDir(pcCurrentPos);
						break;
					case 13 : /* mailer type */
						if (stricmp(pcCurrentPos, "BINKLEY") == 0)
							Config.MailerType = MAIL_BINKLEY;
						else
							Config.MailerType = MAIL_OTHER;
						break;
					case 14 : /* in a league? */
						Config.InterBBS = true;
						break;
					case 15 : /* regcode */
						if (*pcCurrentPos)
							strlcpy(Config.szRegcode, pcCurrentPos, sizeof(Config.szRegcode));
						break;
					case 16 : /* IBBS output semaphore */
						strlcpy(Config.szOutputSem, pcCurrentPos, sizeof(Config.szOutputSem));
						break;
				}
			}
		}
	}

	fclose(fpConfigFile);
	Config.Initialized = true;
	return true;
}

void Config_Close(void)
/*
 * Shuts down config's mem.
 *
 */
{
	int16_t i;
	for (i = 0; i < Config.NumInboundDirs; i++)
		free(Config.szInboundDirs[i]);
	free(Config.pszInfoPath);
}
