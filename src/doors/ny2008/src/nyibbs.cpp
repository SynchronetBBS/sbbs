#include "ny2008.h"
#undef main

tIBInfo         IBBSInfo;
extern INT16      ibbsi;
extern INT16      ibbsi_operator;
extern INT16      ibbsi_game_num;
extern char     bbs_namei[];
INT16             ibbs_send_nodelist = FALSE;
INT16             ibbs_maint_i = FALSE;
INT16             ibbs_maint_o = FALSE;
//int           ibbs_i_mail = FALSE;
//int           ibbs_operator = FALSE;
//int           ibbs_game_num = 0;
glob_t          fff;
glob_t          ff;
char		**fname;
//int           time_slice_value = 1500;
const char           *ver = "0.10";

//this is this module version which is 0.10 beta2
const char           *verinfo = "BETA 2";
//char          no_slices = FALSE;
char            single_node = FALSE;

/*
   void time_slice(void) { asm { mov ax,time_slice_value int 15 } }
*/

char
*LocationOf(char *address) {
	INT16             iCurrentSystem;

	if (IBBSInfo.paOtherSystem == NULL && IBBSInfo.nTotalSystems != 0)
		return (NULL);
	for (iCurrentSystem = 0; iCurrentSystem < IBBSInfo.nTotalSystems; ++iCurrentSystem) {
		if (strcmp(IBBSInfo.paOtherSystem[iCurrentSystem].szAddress, address) == 0) {
			return (IBBSInfo.paOtherSystem[iCurrentSystem].szSystemName);
		}
	}
	return (NULL);
}



void            AddBestPlayerInIB(char *name, DWORD points, char location[]) {
	//ffblk ffblk;
	ibbs_best_rec_type best_rec, best_rec2;
	FILE           *justfile;
	INT16             cnt = 0, len_of_list;

	strcpy(best_rec2.name, name);
	strcpy(best_rec2.location, location);
	best_rec2.points = points;


	//ch_game_d();
	if (fexist(IBBS_BESTTEN_FILENAME)) {
		justfile = ShareFileOpen(IBBS_BESTTEN_FILENAME, "r+b");
		if(justfile != NULL) {
			len_of_list = filelength(fileno(justfile)) / sizeof(ibbs_best_rec_type) + 1;
			if (len_of_list > 10)
				len_of_list = 10;

			/* check if the same record exists */
			while (cnt < 10 && ny_fread(&best_rec, sizeof(ibbs_best_rec_type), 1, justfile) == 1) {
				//printf("[%ld=%ld],", best_rec.points, points);
				//printf("[%s=%s],", best_rec.location, location);
				//printf("[%s=%s]\n", best_rec.name, name);
				if (points == best_rec.points &&
			        	strcmp(best_rec.location, location) == 0 &&
			        	strcmp(best_rec.name, name) == 0) {
					/* if it already exists quit this function */
					fclose(justfile);
					return;
				}
			}

			/* rewind the file to beginning */
			fseek(justfile, 0, SEEK_SET);

			while (cnt < 10 && ny_fread(&best_rec, sizeof(ibbs_best_rec_type), 1, justfile) == 1) {

				if (points >= best_rec.points) {
					//fseek(justfile, (INT32)cnt * sizeof(ibbs_best_rec_type), SEEK_SET);
					//ny_fwrite(&best_rec2, sizeof(ibbs_best_rec_type), 1, justfile);
					strcpy(best_rec.name, best_rec2.name);
					strcpy(best_rec.location, best_rec2.location);
					best_rec.points = best_rec2.points;
					//cnt++;
					while (cnt < len_of_list) {
						fseek(justfile, (INT32)cnt * sizeof(ibbs_best_rec_type), SEEK_SET);
						ny_fread(&best_rec2, sizeof(ibbs_best_rec_type), 1, justfile);
						fseek(justfile, (INT32)cnt * sizeof(ibbs_best_rec_type), SEEK_SET);
						ny_fwrite(&best_rec, sizeof(ibbs_best_rec_type), 1, justfile);
						strcpy(best_rec.name, best_rec2.name);
						strcpy(best_rec.location, best_rec2.location);
						best_rec.points = best_rec2.points;
						cnt++;
					}
					fclose(justfile);
					//od_control.od_ker_exec = NULL;
					return;
				}
				cnt++;

			}

			fclose(justfile);
		}
		if (cnt < 10) {
			justfile = ShareFileOpen(IBBS_BESTTEN_FILENAME, "a+b");
			if(justfile != NULL) {
				ny_fwrite(&best_rec2, sizeof(ibbs_best_rec_type), 1, justfile);
				fclose(justfile);
			}
			//od_control.od_ker_exec = NULL;
			return;
		} else {
			//od_control.od_ker_exec = NULL;
			return;
		}
	}
	justfile = ShareFileOpen(IBBS_BESTTEN_FILENAME, "wb");
	if(justfile != NULL) {
		ny_fwrite(&best_rec2, sizeof(ibbs_best_rec_type), 1, justfile);
		fclose(justfile);
	}
	//od_control.od_ker_exec = NULL;
	return;
}



void            get_bbsname(char bbsname[]) {
	INT16             cnt = 0;
	char            out[36];
	INT16             cnto = 0;

	while (bbsname[cnt] != 0 && cnto < 35) {
		if (bbsname[cnt] >= 'a' && bbsname[cnt] <= 'z')
			bbsname[cnt] -= 32;

		if (bbsname[cnt] >= 'A' && bbsname[cnt] <= 'Z') {
			out[cnto] = bbsname[cnt];
			cnto++;
		}

		cnt++;
	}
	out[cnto] = 0;
	strcpy(bbsname, out);
}

INT16             seereg(char bbsname[]) {
	return TRUE;

	/*
	   FILE *justfile;

	char kod[26]; char string[26] = "ABECEDAKURVAHLEDAPICATAKY"; int intval;
	   INT16 cnt; int bbsc; int which; int temp,temp2;

	//  ch_game_d(); justfile=ShareFileOpen(KEY_FILENAME,"rb");

	ny_fread(kod,26,1,justfile);

	fclose(justfile);


	sscanf(kod,"%02d",&intval);

	if (intval!=strlen(bbsname)) { return(0); }

	intval = kod[0] - '0';

	bbsc=0;

	which=0;

	for (cnt=2;cnt<25;cnt++) { if (bbsname[bbsc]==0) bbsc=0;

	temp2=string[cnt]+intval+bbsname[bbsc];

	//    if (kod[cnt]>'Z') kod[cnt]-=('Z'-'A'); temp =
	   (temp2-'A')/('Z'-'A');

	temp2 = temp2 - (('Z'-'A') * temp);

	if (kod[cnt]!=temp2) { return(0); }

	if (which==0) which =1; else which =0;

	intval = kod[which] - '0'; bbsc++; }

	return(TRUE);
	*/
}

void            DecodeBuffer(const char *pszSource, char *pDestBuffer, INT16 nBufferSize, INT16 skip) {
	const char     *pcSource = pszSource;
	char           *pcDest = (char *)pDestBuffer;
	INT16             iDestLocation;
	tBool           bFirstOfByte = TRUE;

	/* Search for beginning of buffer delimiter char, returning if not found */
	while (*pcSource && *pcSource != DELIMITER_CHAR)
		++pcSource;
	if (!*pcSource)
		return;

	/* Move pointer to first char after delimiter char */
	++pcSource;
	if (skip)
		pcSource += 4;

	/* Loop until destination buffer is full, delimiter char is encountered, */
	/* or end of source buffer is encountered */
	iDestLocation = 0;
	while (iDestLocation < nBufferSize && *pcSource
	        && *pcSource != DELIMITER_CHAR) {
		/* If this is a valid data character */
		if (*pcSource >= 0x40 && *pcSource <= 0x7e) {
			/* If this is first character of byte */
			if (bFirstOfByte) {
				*pcDest = *pcSource & 0x3f;

				/* Toggle bFirstOfByte */
				bFirstOfByte = FALSE;
			} else {		/* if(!bFirstOfByte) */
				*pcDest |= (*pcSource & 0x30) << 2;

				/* Increment destination */
				++iDestLocation;
				++pcDest;

				/* Toggle bFirstOfByte */
				bFirstOfByte = TRUE;
			}
		}

		/* Increment source byte pointer */
		++pcSource;
	}
}

INT16             ReadLen(const char *pszSource) {
	const char     *pcSource = pszSource;
	//char         *pcDest;

	//= (char *)pDestBuffer;
	char            size[2];
	//int           iDestLocation;
	INT16             nBufferSize = 2;
	//tBool         bFirstOfByte = TRUE;

	/* Search for beginning of buffer delimiter char, returning if not found */
	while (*pcSource && *pcSource != DELIMITER_CHAR)
		++pcSource;
	if (!*pcSource)
		return (0);

	/* Move pointer to first char after delimiter char */
	++pcSource;

	size[0] = *pcSource & 0x3f;
	pcSource++;
	size[0] |= (*pcSource & 0x30) << 2;
	pcSource++;
	size[1] = *pcSource & 0x3f;
	pcSource++;
	size[1] |= (*pcSource & 0x30) << 2;
	pcSource++;

	nBufferSize = *(INT16 *)size;
	//printf("&&&& %d &&&&\n", nBufferSize);
	return (nBufferSize);
}


/*
   void DecodeBufferR(const char *pszSource, INT16 *pDestBuffer, int
   nBufferSize) { const char *pcSource = pszSource; char *pcDest = (char
   *)pDestBuffer; //  char size[2]; int iDestLocation; //  int nBufferSize=2;
   tBool bFirstOfByte = TRUE;
 
/* Search for beginning of buffer delimiter char, returning if not found -/
   while(*pcSource && *pcSource != DELIMITER_CHAR) ++pcSource; if(!*pcSource)
   return(0);
 
/* Move pointer to first char after delimiter char -/ ++pcSource;
 
/*  size[0] = *pcSource & 0x3f; pcSource++; size[0] |= (*pcSource & 0x30) <<
   2; pcSource++; size[1] = *pcSource & 0x3f; pcSource++; size[1] |=
   (*pcSource & 0x30) << 2; pcSource++;
 
nBufferSize=*(INT16 *)size; printf("&&&& %d &&&&\n",nBufferSize);
   pDestBuffer=(INT16)malloc(nBufferSize); pcDest = (char *)(*pDestBuffer);
*/

/*
   Loop until destination buffer is full, delimiter char is encountered, -/ /*
   or end of source buffer is encountered -/ iDestLocation = 0;
 
while(iDestLocation < nBufferSize && *pcSource && *pcSource !=
   DELIMITER_CHAR) { /* If this is a valid data character -/ if(*pcSource >=
   0x40 && *pcSource <= 0x7e) { /* If this is first character of byte -/
   if(bFirstOfByte) { //      if(iDestLocation<2) { //        size =
   *pcSource & 0x3f; //        printf("#4#"); //      } else pcDest =
   *pcSource & 0x3f;
 
/* Toggle bFirstOfByte -/ bFirstOfByte = FALSE; } else { /* if(!bFirstOfByte)
   -/ //      if(iDestLocation<2) { //        size |= (*pcSource & 0x30) <<
   2; //        printf("#5#"); //      } else { pcDest |= (*pcSource & 0x30)
   << 2; ++pcDest; printf("%c",*pcDest); //      }
 
/* Increment destination -/ ++iDestLocation;
 
/* Toggle bFirstOfByte -/ bFirstOfByte = TRUE; } //  printf("#5#"); //
   if(iDestLocation==2) { //      } } /* Increment source byte pointer -/
   ++pcSource; } return(nBufferSize); }
*/

tIBResult       IBGet(tIBInfo * pInfo, char *pBuffer, INT16 nMaxBufferSize) {
	tIBResult       ToReturn;
	glob_t          DirEntry;
	DWORD           lwCurrentMsgNum;
	tMessageHeader  MessageHeader;
	char            szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	char           *pszText;
	tFidoNode       ThisNode, OtherNode;
	char          **ffname;

	/* Validate information structure */
	ToReturn = ValidateInfoStruct(pInfo);
	if (ToReturn != eSuccess)
		return (ToReturn);

	/* Get this node's address from string */
	ConvertStringToAddress(&ThisNode, pInfo->szThisNodeAddress);

	MakeFilename(pInfo->szNetmailDir, "*.msg", szFileName);

	/* Seach through each message file in the netmail directory, in no */
	/* particular order.                                               */
	if (glob(szFileName, 0, NULL, &DirEntry) == 0) {
		for(ffname=DirEntry.gl_pathv;*ffname != NULL;ffname++) {
			lwCurrentMsgNum = atol(*ffname);

			/* If able to read message */
			if (ReadMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader,
			                &pszText)) {

				/*
				   od_printf("\nREAD THROUGH MESSAGE\n");
				   od_printf("|%s|\n\r",MessageHeader.szToUserName);
				   od_printf("|%s|\n\r",pInfo->szProgName);
				   od_printf("%d:",MessageHeader.wDestZone);
				   od_printf("%d/",MessageHeader.wDestNet);
				   od_printf("%d.",MessageHeader.wDestNode);
				   od_printf("%d\n\r",MessageHeader.wDestPoint);

				od_get_answer("1");
				*/
				/* If message is for us, and hasn't be read yet */
				ConvertStringToAddress(&OtherNode, &MessageHeader.szToUserName[3]);

				if (stricmp(MessageHeader.szFromUserName, pInfo->szProgName) == 0
				        && (ThisNode.wZone == OtherNode.wZone || OtherNode.wZone == 0 || ThisNode.wZone == 0)
				        && ThisNode.wNet == OtherNode.wNet
				        && ThisNode.wNode == OtherNode.wNode
				        && ThisNode.wPoint == OtherNode.wPoint
				        && !(MessageHeader.wAttribute & ATTRIB_RECEIVED)) {

					/*
					   od_printf("\n\rUNREAD AND FOR US\n\r");
					   od_get_answer("1");
					*/

					/* Decode message text, placing information in buffer */
					DecodeBuffer(pszText, pBuffer, nMaxBufferSize, FALSE);

					/* If received messages should be deleted */
					if (pInfo->bEraseOnReceive) {
						/* Determine filename of message to erase */
						GetMessageFilename(pInfo->szNetmailDir, lwCurrentMsgNum,
						                   szFileName);

						/* Attempt to erase file */
						if (unlink(szFileName) == -1) {
							ToReturn = eGeneralFailure;
						} else {
							ToReturn = eSuccess;
						}
					}

					/* If received messages should not be deleted */
					else {	/* if(!pInfo->bEraseOnReceive) */
						/* Mark message as read */
						MessageHeader.wAttribute |= ATTRIB_RECEIVED;
						++MessageHeader.wTimesRead;

						/* Attempt to rewrite message */
						if (!WriteMessage(pInfo->szNetmailDir, lwCurrentMsgNum,
						                  &MessageHeader, pszText)) {
							ToReturn = eGeneralFailure;
						} else {
							ToReturn = eSuccess;
						}
					}

					/* Deallocate message text buffer */
					free(pszText);

					/* Return appropriate value */
					globfree(&DirEntry);
					return (ToReturn);
				}
				free(pszText);
			}
		}
		globfree(&DirEntry);
	}

	/* If no new messages were found */
	return (eNoMoreMessages);
}

tIBResult       IBGetLen(tIBInfo * pInfo, INT32 *msgnum, INT16 *nBufferLen) {
	tIBResult       ToReturn;
	glob_t          DirEntry;
	DWORD           lwCurrentMsgNum;
	tMessageHeader  MessageHeader;
	char            szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	char           *pszText;
	tFidoNode       ThisNode, OtherNode;
	//int           n;
	char	  **ffname;

	/* Validate information structure */
	ToReturn = ValidateInfoStruct(pInfo);
	if (ToReturn != eSuccess)
		return (ToReturn);

	/* Get this node's address from string */
	ConvertStringToAddress(&ThisNode, pInfo->szThisNodeAddress);

	MakeFilename(pInfo->szNetmailDir, "*.msg", szFileName);

	/* Seach through each message file in the netmail directory, in no */
	/* particular order.                                               */
	if (glob(szFileName, 0, NULL, &DirEntry) == 0) {
		for(ffname=DirEntry.gl_pathv;*fname != NULL;fname++) {
			lwCurrentMsgNum = atol(*ffname);
			*msgnum = lwCurrentMsgNum;

			/* If able to read message */
			if (ReadMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader,
			                &pszText)) {
				ConvertStringToAddress(&OtherNode, &MessageHeader.szToUserName[3]);

				if (stricmp(MessageHeader.szFromUserName, pInfo->szProgName) == 0
				        && (ThisNode.wZone == OtherNode.wZone || OtherNode.wZone == 0 || ThisNode.wZone == 0)
				        && ThisNode.wNet == OtherNode.wNet
				        && ThisNode.wNode == OtherNode.wNode
				        && ThisNode.wPoint == OtherNode.wPoint
				        && !(MessageHeader.wAttribute & ATTRIB_RECEIVED)) {

					/* Decode message text, placing information in buffer */
					*nBufferLen = ReadLen(pszText);

					free(pszText);

					/* Return appropriate value */
					globfree(&DirEntry);
					return (ToReturn);
				}
				free(pszText);
			}
		}
		globfree(&DirEntry);
	}

	/* If no new messages were found */
	return (eNoMoreMessages);
}

INT16             IBGetR(tIBInfo * pInfo, char *pBuffer, INT16 nBufferLen, long lwCurrentMsgNum) {
	//tIBResult     ToReturn;
	//struct ffblk  DirEntry;
	//DWORD         lwCurrentMsgNum;
	tMessageHeader  MessageHeader;
	char            szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	//char          numstr[FILENAME_CHARS + 2];
	char           *pszText;

	//tFidoNode ThisNode, OtherNode;
	//int           n;

	/* Validate information structure */
	//ToReturn = ValidateInfoStruct(pInfo);
	//if (ToReturn != eSuccess)	return (ToReturn);

	/* Get this node's address from string */
	//ConvertStringToAddress(&ThisNode, pInfo->szThisNodeAddress);

	//sprintf(numstr, "%d.msg", msgnum);

	//MakeFilename(pInfo->szNetmailDir, numstr, szFileName);

	/* Seach through each message file in the netmail directory, in no */
	/* particular order.                                               */
	//if (fexist(szFileName))
	//{
	//printf("@5@");
	//do
	//{
	//lwCurrentMsgNum = atol(DirEntry.ff_name);

	//printf("\n%s\n", DirEntry.ff_name);

	/* If able to read message */
	if (ReadMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader, &pszText)) {

		/*
		   ConvertStringToAddress(&OtherNode,&MessageHeader.szToUserNa
		   me[3]);

		printf("@6@");

		if(strcmp(MessageHeader.szFromUserName, pInfo->szProgName) ==
		   0 && (ThisNode.wZone == OtherNode.wZone ||
		   OtherNode.wZone==0 || ThisNode.wZone==0) && ThisNode.wNet
		   == OtherNode.wNet && ThisNode.wNode == OtherNode.wNode &&
		   ThisNode.wPoint == OtherNode.wPoint &&
		   !(MessageHeader.wAttribute & ATTRIB_RECEIVED)) {
		   printf("@7@");
		*/
		/* Decode message text, placing information in buffer */
		DecodeBuffer(pszText, pBuffer, nBufferLen, TRUE);
		//*nBufferLen = DecodeBufferR(pszText, pBuffer);

		/* If received messages should be deleted */
		if (pInfo->bEraseOnReceive) {
			/* Determine filename of message to erase */
			GetMessageFilename(pInfo->szNetmailDir, lwCurrentMsgNum, szFileName);

			/* Attempt to erase file */
			if (unlink(szFileName) == -1) {
				free(pszText);
				return (FALSE);
				//ToReturn = eGeneralFailure;
			} //
			else {
				//ToReturn = eSuccess;
				//
			}
		} else {		/* If received messages should not be deleted */
			/* if(!pInfo->bEraseOnReceive) */
			/* Mark message as read */
			MessageHeader.wAttribute |= ATTRIB_RECEIVED;
			++MessageHeader.wTimesRead;

			/* Attempt to rewrite message */
			if (!WriteMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader, pszText)) {
				free(pszText);
				return (FALSE);
				//ToReturn = eGeneralFailure;
			} //
			else {
				//ToReturn = eSuccess;
				//
			}
		}

		/* Deallocate message text buffer */
		free(pszText);

		/* Return appropriate value */
		return (TRUE);
	}
	//free(pszText);

	//} while (findnext(&DirEntry) == 0);
	//}
	//printf("@10@");

	/* If no new messages were found */
	return (FALSE);
}

/* refreshes the ib best list according to local best list */
void            RefreshBest(void) {
	best_rec_type   best_rec;
	FILE           *justfile;

	/* if best list exists */
	if (fexist(BESTTEN_FILENAME)) {
		/* open for reading */
		justfile = ShareFileOpen(BESTTEN_FILENAME, "rb");
		if(justfile != NULL) {
			while (ny_fread(&best_rec, sizeof(best_rec), 1, justfile) == 1) {
				AddBestPlayerInIB(best_rec.name, best_rec.points, IBBSInfo.szThisNodeAddress);
			}
			fclose(justfile);
		}
	}
}



int             main(int argc, char *argv[]) {
	FILE           *justfile, *njustfile;
	//char          key;
	//WORD  uintval;
	char            numstr[26];

	//scr_rec srec;
	INT16             cnt, x;

	//, intval, x;
	INT16             n;
	//struct ffblk  ffblk;

	//scr_rec rec;
	//date today;
	//date lastday;
	//char         *temp;


	//don 't believe it' s needed here no opendoors stuff used
	// strcpy(od_registered_to, "Your Name");
	//od_registration_key = 00000000000000L;

#ifdef __unix__

	srandomdev();
#else

	randomize();
#endif

	//od_control.od_disable = DIS_NAME_PROMPT;

	/*    directvideo = 0; */

	//strcpy(od_control.od_prog_name, "New York 2008 IBBS MODULE");

	//od_control.od_config_function = CustomConfigFunction;

	//od_control.od_mps = INCLUDE_MPS;
	//od_control.od_nocopyright = TRUE;
	//od_control.od_help_text2 = (char *)"  New York 2008 v0.01 WIDE BETA 3 (c) Copyright 1995 George Lebl               ";

	/*
	   od_control.od_cbefore_chat=ny_chat; //
	   od_control.od_cafter_chat=scr_res;
	   od_control.od_cbefore_shell=scr_save;
	   od_control.od_cafter_shell=scr_res;
	*/


	if (fexist(CFG_FILENAME)) {
		//printf("&|%s|&", bbs_namei);
		IBReadConfig(&IBBSInfo, CFG_FILENAME);
		//printf("&|%s|&", bbs_namei);
	}

	//printf("|%s|", bbs_namei);


	cnt = 1;
	if (argc > 1) {
		do {
			if (strnicmp(argv[cnt], "-IBBSI", 6) == 0) {
				ibbs_maint_i = TRUE;
			} else if (strnicmp(argv[cnt], "-IBBSO", 6) == 0) {
				ibbs_maint_o = TRUE;
			} else if (strnicmp(argv[cnt], "-SENDNODELIST", 13) == 0) {
				ibbs_send_nodelist = TRUE;
			} else if (strnicmp(argv[cnt], "-C", 2) == 0) {
				if (fexist(&argv[cnt][2]))
					IBReadConfig(&IBBSInfo, &argv[cnt][2]);

				//strzcpy(od_control.od_config_filename, argv[cnt], 2, 59);
				//od_control.od_config_file = INCLUDE_CONFIG_FILE;
			}
		} while ((++cnt) < argc);
	}

	//od_control.od_force_local = TRUE;
	//od_init();

	//if (no_slices == FALSE)
	//od_control.od_ker_exec = time_slice;
	//    else
	//od_control.od_ker_exec = NULL;

	//if (od_control.user_screen_length < 2)
	//od_control.user_screen_length = 24;

	//od_control.od_help_text2 = (char *)"  New York 2008 v0.10 InterBBS Module (c) 1996 George Lebl                    ";

	char           *InComing;
	char           *OutGoing;
	INT16             found = FALSE;

	//ibbs_bbs_rec_type ibbest;
	ibbs_bbs_spy_rec bbs_spy_rec;
	ibbs_mail_type  ibmail;
	user_rec        urec_i;
	ibbs_bbs_rec    bbs_rec;
	ibbs_scr_rec    ibscr_rec;
	FILE           *jfile;
	ibbs_scr_rec    ibscr_rec2;
	best_rec_type   best_rec;
	ibbs_act_rec    act_rec;
	ibbs_scr_spy_rec scr_spy_rec;
	INT16             pack_spy = FALSE;
	INT32            msgnum;
	INT16             bufferlen;

	/*
	   //  if (fexist(KEY_FILENAME)) { strcpy(numstr,bbs_namei); //
	   printf("|%s|",bbs_namei); get_bbsname(numstr); //
	   printf("|%s|",numstr); if(seereg(numstr)==FALSE) {
	   printf("\nUNREGISTERED... InterBBS DISABLED!!\n"); exit(10); }
	*/

	/*
	   } else { printf("\nUNREGISTERED... InterBBS DISABLED!!\n"); exit(10);
	   }
	*/

	if (ibbsi == TRUE) {
		//ch_game_d();
		INT16             xx;

		strcpy(IBBSInfo.szThisNodeAddress, "0:000/000");
		sprintf(IBBSInfo.szProgName, "#@NYG#%05d IBBS", ibbsi_game_num);
		strcpy(IBBSInfo.szNetmailDir, "C:/FD/NETMAIL");
		IBBSInfo.bCrash = FALSE;
		IBBSInfo.bHold = FALSE;
		IBBSInfo.bEraseOnSend = TRUE;
		IBBSInfo.bEraseOnReceive = TRUE;
		IBBSInfo.nTotalSystems = 0;
		IBBSInfo.paOtherSystem = NULL;
		IBReadConfig(&IBBSInfo, "INTERBBS.CFG");
		IBReadConfig(&IBBSInfo, NODELIST_FILENAME);


		char            szDirFileName[PATH_CHARS + 1];

		if (IBBSInfo.szNetmailDir == NULL || strlen(IBBSInfo.szNetmailDir) > PATH_CHARS) {
			printf("\n\nNETMAIL DIR NOT FOUND\n\n");
			exit(10);
		}

		//assert(IBBSInfo.szNetmailDir != NULL);
		//assert(strlen(IBBSInfo.szNetmailDir) <= PATH_CHARS);

		strcpy(szDirFileName, IBBSInfo.szNetmailDir);

		//Remove any trailing backslash from directory name
		if (szDirFileName[strlen(szDirFileName) - 1] == '\\')
			szDirFileName[strlen(szDirFileName) - 1] = '\0';


		//Return true iff file exists and it is a directory
		if (!fexist(szDirFileName) || !isdir(szDirFileName)) {
			printf("\n\nNETMAIL DIR NOT FOUND\n\n");
			exit(10);
		}

		if (IBBSInfo.nTotalSystems > 255)
			IBBSInfo.nTotalSystems = 255;

		if (ibbs_maint_i) {
			printf("\n##> Getting InterBBS Mail\n");
			sprintf(IBBSInfo.szProgName, "#@NYG#%05d MAIL", ibbsi_game_num);

			justfile = ShareFileOpen(IBBS_MAIL_INDEX, "a+b");
			if(justfile != NULL) {
				//while (IBGet(&IBBSInfo, &ibmail, sizeof(ibbs_mail_type)) == eSuccess) {

				/*
				   for(x=0;x<(sizeof(ibbs_mail_type));x++) (((char
				   *)&ibmail)+x)=0;
				*/
				while (IBGetMail(&IBBSInfo, &ibmail) == eSuccess) {

					/*
					   printf(">>>(%s|",ibmail.sender);
					   printf("%s|",ibmail.senderI);
					   printf("%s|",ibmail.node_s);
					   printf("%s|",ibmail.node_r);
					   printf("%s|",ibmail.recver);
					   printf("%s)<<<\n\n",ibmail.recverI);
					*/

					if (strcmp(ibmail.node_r, IBBSInfo.szThisNodeAddress) == 0)
						ny_fwrite(&ibmail, sizeof(ibbs_mail_type), 1, justfile);
				}
				fclose(justfile);
			}
		}

		if (ibbs_send_nodelist && ibbsi_operator) {
			//od_control.od_status_on = FALSE;
			//od_set_statusline(STATUS_NONE);
			printf("\n##> Sending Node List\n");
			sprintf(IBBSInfo.szProgName, "#@NYG#%05d NODELIST", ibbsi_game_num);
			//if (IBBSInfo.nTotalSystems <= 20)
			OutGoing = (char *)malloc(sizeof(tOtherNode) * IBBSInfo.nTotalSystems + 1);
			*OutGoing = (char)IBBSInfo.nTotalSystems;
			memcpy(OutGoing + 1, (char *)IBBSInfo.paOtherSystem, sizeof(tOtherNode) * IBBSInfo.nTotalSystems);
			if (IBSendAll(&IBBSInfo, OutGoing, sizeof(tOtherNode) * IBBSInfo.nTotalSystems + 1) != eSuccess) {
				printf("\n\nINTERBBS ERROR:Can't send the NODELIST!!!\n\n");
				sleep(4);
			}

			free(OutGoing);
		}
		if (ibbs_maint_i) {
			//od_control.od_status_on = FALSE;
			//od_set_statusline(STATUS_NONE);
			printf("\n##> Refreshing Local Best List (if there is one)\n");

			RefreshBest();

			printf("\n##> Processing Incoming Information\n");

			if (ibbsi_operator == FALSE) {
				sprintf(IBBSInfo.szProgName, "#@NYG#%05d NODELIST", ibbsi_game_num);
				InComing = (char *)malloc(sizeof(tOtherNode) * 258);
				if (InComing == NULL) {
					printf("\n\nINTERBBS ERROR:Not Enough Memory to process!\n");
					sleep(4);
					exit(12);
				}
				while (IBGet(&IBBSInfo, InComing, sizeof(tOtherNode) * 258) == eSuccess) {
					IBBSInfo.nTotalSystems = *InComing;
					InComing++;
					free(IBBSInfo.paOtherSystem);
					IBBSInfo.paOtherSystem = (tOtherNode *) malloc(sizeof(tOtherNode) * IBBSInfo.nTotalSystems);
					memcpy((char *)IBBSInfo.paOtherSystem, InComing, sizeof(tOtherNode) * IBBSInfo.nTotalSystems);
					//free(InComing);
					justfile = ShareFileOpenAR(NODELIST_FILENAME, "wt");
					if(justfile != NULL) {
						fprintf(justfile, ";Only the central system operator should change this node list!\n");
						for (x = 0; x < IBBSInfo.nTotalSystems; x++) {
							fprintf(justfile, "LinkWith        %s\n", ((tOtherNode *) InComing)[x].szAddress);
							fprintf(justfile, "LinkName        %s\n", ((tOtherNode *) InComing)[x].szSystemName);
							fprintf(justfile, "LinkLocation    %s\n\n", ((tOtherNode *) InComing)[x].szLocation);
						}
						fclose(justfile);
					}
					InComing--;
				}
				free(InComing);
			}
			//od_printf("1");
			//od_get_answer("1");

			sprintf(IBBSInfo.szProgName, "#@NYG#%05d LISTREQ", ibbsi_game_num);

			InComing = (char *)malloc(NODE_ADDRESS_CHARS + 1);

			while (IBGet(&IBBSInfo, InComing, NODE_ADDRESS_CHARS + 1) == eSuccess) {
				if (strcmp(InComing, IBBSInfo.szThisNodeAddress) != 0) {
					sprintf(IBBSInfo.szProgName, "#@NYG#%05d NAMELIST", ibbsi_game_num);
					justfile = ShareFileOpen(USER_FILENAME, "rb");
					if(justfile != NULL) {
						cnt = filelength(fileno(justfile)) / sizeof(user_rec);
						OutGoing = (char *)malloc(((25 + 36 + 1) * cnt) + NODE_ADDRESS_CHARS + 4);
						OutGoing += 2;
						x = 0;
						while (ny_fread(&urec_i, sizeof(user_rec), 1, justfile) == 1) {
							strcpy(OutGoing + 1 + ((25 + 36 + 1) * x), urec_i.name);
							strcpy(OutGoing + 1 + ((25 + 36 + 1) * x) + 25, urec_i.bbsname);
							*(OutGoing + (25 + 36 + 1) * (x + 1)) = (char)urec_i.sex;
							x++;
						}
						strcpy(OutGoing + 1 + ((25 + 36 + 1) * cnt), IBBSInfo.szThisNodeAddress);
						if (x > 0) {
							*OutGoing = cnt;
							OutGoing -= 2;
							*(INT16 *)OutGoing = ((25 + 36 + 1) * cnt) + NODE_ADDRESS_CHARS + 2;
							IBSend(&IBBSInfo, InComing, OutGoing, ((25 + 36 + 1) * cnt) + NODE_ADDRESS_CHARS + 4);
						} else {
							OutGoing -= 2;
						}
						fclose(justfile);
					}
					free(OutGoing);
					sprintf(IBBSInfo.szProgName, "#@NYG#%05d LISTREQ", ibbsi_game_num);
				}
			}
			free(InComing);
			//free(OutGoing);

			//od_printf("2");
			//od_get_answer("1");

			sprintf(IBBSInfo.szProgName, "#@NYG#%05d NEWS", ibbsi_game_num);

			newzfile_t newzfile;

			while (IBGet(&IBBSInfo, (char *)&newzfile, sizeof(newzfile)) == eSuccess) {
				//ch_game_d();
				justfile = ShareFileOpen(TODNEWS_FILENAME, "a+b");
				if(justfile != NULL) {
					ny_fwrite(&newzfile, sizeof(newzfile), 1, justfile);
					fclose(justfile);
				}
			}

			sprintf(IBBSInfo.szProgName, "#@NYG#%05d BBSINFO", ibbsi_game_num);

			//od_printf("1");
			//od_get_answer("1");
			while (IBGet(&IBBSInfo, (char *)&bbs_rec, sizeof(bbs_rec)) == eSuccess) {
				//od_printf("2");
				//od_get_answer("1");
				justfile = ShareFileOpen(IBBSSPY_FILENAME, "r+b");
				//fseek(justfile, 0, SEEK_SET);
				if (justfile == NULL) {
					justfile = ShareFileOpen(IBBSSPY_FILENAME, "wb");
					bbs_spy_rec.player_list = 0;
					bbs_spy_rec.players = 0;
					strcpy(bbs_spy_rec.node, bbs_rec.node);
					bbs_spy_rec.hi_points = bbs_rec.hi_points;
					ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile);
					fclose(justfile);
				} else {
					xx = 0;
					found = FALSE;
					while (ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
						if (strcmp(bbs_rec.node, bbs_spy_rec.node) == 0) {
							bbs_spy_rec.hi_points = bbs_rec.hi_points;
							fseek(justfile, (INT32)xx * sizeof(ibbs_bbs_spy_rec), SEEK_SET);
							ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile);
							found = TRUE;
							break;
						}
						xx++;
						fseek(justfile, (INT32)xx * sizeof(ibbs_bbs_spy_rec), SEEK_SET);
					}
					fclose(justfile);
					//od_printf("3");
					//od_get_answer("1");
					if (found == FALSE) {
						justfile = ShareFileOpen(IBBSSPY_FILENAME, "ab");
						bbs_spy_rec.player_list = 0;
						bbs_spy_rec.players = 0;
						strcpy(bbs_spy_rec.node, bbs_rec.node);
						bbs_spy_rec.hi_points = bbs_rec.hi_points;
						if(justfile != NULL) {
							ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile);
							fclose(justfile);
						}
					}
				}
			}
			//od_printf("4");
			//od_get_answer("1");

			sprintf(IBBSInfo.szProgName, "#@NYG#%05d NAMELIST", ibbsi_game_num);

			//InComing = (char *)malloc(((25 + 36 + 1) * 10) + NODE_ADDRESS_CHARS + 2);

			while (IBGetLen(&IBBSInfo, &msgnum, &bufferlen) == eSuccess) {
				InComing = (char *)malloc(bufferlen);
				IBGetR(&IBBSInfo, InComing, bufferlen, msgnum);

				cnt = *InComing;

				//printf("\nPlayers in the list: |%d|\n", cnt);
				//od_get_answer("1");
				justfile = ShareFileOpen(IBBSSPY_FILENAME, "r+b");
				if(justfile != NULL) {
					//fseek(justfile, 0, SEEK_SET);
					//printf("3");
					xx = 0;
					//printf("4");

					//printf("\n*|N#%s|\n", InComing + 1);
					//+((25 + 36 + 1) * cnt));
					//printf("*|N#%s|\n", InComing + 26);
					//+((25 + 36 + 1) * (cnt - 1)));
					//od_get_answer("1");

					while (ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
						//printf("5");
						if (strcmp(InComing + 1 + ((25 + 36 + 1) * cnt), bbs_spy_rec.node) == 0) {
							//printf("6");
							if (bbs_spy_rec.player_list != 0) {
								//printf("7");
								sprintf(numstr, SBYDBT_PREFIX".%03d", bbs_spy_rec.player_list);
								njustfile = ShareFileOpen(numstr, "wb");
								if(njustfile != NULL) {
									for (x = 0; x < cnt; x++) {
										strcpy(ibscr_rec.name, InComing + 1 + ((25 + 36 + 1) * x));
										strcpy(ibscr_rec.nameI, InComing + 1 + ((25 + 36 + 1) * x) + 25);
										ibscr_rec.sex = *(sex_type *) (InComing + (25 + 36 + 1) * (x + 1));
										//printf("\n+|%s|%s|%d|\n", ibscr_rec.name, ibscr_rec.nameI, (INT16)ibscr_rec.sex);
										//od_get_answer("1");

										ibscr_rec.level = -1;
										ny_fwrite(&ibscr_rec, sizeof(ibbs_scr_rec), 1, njustfile);
									}
									//printf("8");
									fclose(njustfile);
									//printf("9");
								}
							} else {
								bbs_spy_rec.player_list = 0;
								do {
									bbs_spy_rec.player_list++;
									sprintf(numstr, SBYDB_PREFIX".%03d", bbs_spy_rec.player_list);
								} while (fexist(numstr));
								njustfile = ShareFileOpen(numstr, "wb");
								if(njustfile != NULL)
									fclose(njustfile);
								sprintf(numstr, SBYDBT_PREFIX".%03d", bbs_spy_rec.player_list);
								njustfile = ShareFileOpen(numstr, "wb");
								if(njustfile != NULL) {
									for (x = 0; x < *InComing; x++) {
										strcpy(ibscr_rec.name, InComing + 1 + ((25 + 36 + 1) * x));
										strcpy(ibscr_rec.nameI, InComing + 1 + ((25 + 36 + 1) * x) + 25);
										ibscr_rec.level = -1;
										ibscr_rec.sex = *(sex_type *) (InComing + (25 + 36 + 1) * (x + 1));
										//od_printf("++|%s|%s|%d|", ibscr_rec.name, ibscr_rec.nameI, (INT16)ibscr_rec.sex);
										//od_get_answer("1");

										ny_fwrite(&ibscr_rec, sizeof(ibbs_scr_rec), 1, njustfile);
									}
									//printf("A");
									fclose(njustfile);
								}
								//printf("B");
								fseek(justfile, (INT32)xx * sizeof(ibbs_bbs_spy_rec), SEEK_SET);
								//printf("C");
								ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile);
							}
							break;
						}
						xx++;
						fseek(justfile, (INT32)xx * sizeof(ibbs_bbs_spy_rec), SEEK_SET);
						//printf("D");
					}
					fclose(justfile);
				}
				//printf("E");
				free(InComing);
				//printf("\nKKK\n");
				//printf("F");
			}
			//free(InComing);
			//printf("G");


			//od_printf("6");
			//od_get_answer("1");
			//printf("H");

			if(glob(SBYDBT_PREFIX".*", 0, NULL, &ff)==0) {
				for(fname=ff.gl_pathv;*fname!=NULL;fname++) {
					//printf("I");

					sscanf(*fname, SBYDBT_PREFIX".%d", &cnt);
					sprintf(numstr, SBYDB_PREFIX".%03d", cnt);
					copyfile(numstr, SBYDB_PREFIX "" TEMP_EXTENSION);
					ny_remove(numstr);

					//printf("J");
					jfile = ShareFileOpen(*fname, "rb");
					//printf("K");
					if(jfile != NULL) {

						while (ny_fread(&ibscr_rec, sizeof(ibbs_scr_rec), 1, jfile) == 1) {
							//printf("L");

							njustfile = ShareFileOpen(SBYDB_PREFIX "" TEMP_EXTENSION, "r+b");
							justfile = ShareFileOpen(numstr, "a+b");
							found = FALSE;
							//printf("M");
							x = 0;

							if(justfile!=NULL && njustfile != NULL) {
								while (ny_fread(&ibscr_rec2, sizeof(ibbs_scr_rec), 1, njustfile) == 1) {

									//printf("N");
									if (strcmp(ibscr_rec.nameI, ibscr_rec2.nameI) == 0) {
										if (ibscr_rec2.level != -66) {
											strcpy(ibscr_rec2.name, ibscr_rec.name);
											ibscr_rec2.sex = ibscr_rec.sex;
											ny_fwrite(&ibscr_rec, sizeof(ibbs_scr_rec), 1, justfile);
											fseek(njustfile, (INT32)x * sizeof(ibbs_scr_rec), SEEK_SET);
											ibscr_rec2.level = -66;
											ny_fwrite(&ibscr_rec2, sizeof(ibbs_scr_rec), 1, njustfile);
										}
										found = TRUE;
										break;
									}
									//printf("O");
									x++;
									fseek(njustfile, (INT32)x * sizeof(ibbs_scr_rec), SEEK_SET);
								}
								//printf("P");
								if (found == FALSE)
									ny_fwrite(&ibscr_rec, sizeof(ibbs_scr_rec), 1, justfile);
								fclose(njustfile);
								fclose(justfile);
							}
							//printf("Q");

						}
						//printf("R");
						fclose(jfile);
					}
					ny_remove(*fname);
					ny_remove(SBYDB_PREFIX "" TEMP_EXTENSION);
					//printf("S");
					justfile = ShareFileOpen(IBBSSPY_FILENAME, "r+b");
					//fseek(justfile, 0, SEEK_SET);
					xx = 0;
					if(justfile != NULL) {
						while (ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
							if (bbs_spy_rec.player_list == cnt && cnt != 0) {
								sprintf(numstr, SBYDB_PREFIX".%03d", bbs_spy_rec.player_list);
								njustfile = ShareFileOpen(numstr, "rb");
								if(njustfile != NULL) {
									bbs_spy_rec.players = filelength(fileno(njustfile)) / sizeof(ibbs_scr_rec);
									fclose(njustfile);
								}

								fseek(justfile, (INT32)xx * sizeof(ibbs_bbs_spy_rec), SEEK_SET);
								ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile);
								break;
							}
							xx++;
							fseek(justfile, (INT32)xx * sizeof(ibbs_bbs_spy_rec), SEEK_SET);
						}
						fclose(justfile);
					}
					//printf("T");
				}
				globfree(&ff);
			}
			//printf("U");

			//od_printf("7");
			//od_get_answer("1");

			sprintf(IBBSInfo.szProgName, "#@NYG#%05d TENBEST", ibbsi_game_num);
			//free(InComing);
			InComing = (char *)malloc(NODE_ADDRESS_CHARS + 2 + (10 * sizeof(bbs_rec)));
			while (IBGet(&IBBSInfo, InComing, NODE_ADDRESS_CHARS + 2 + (10 * sizeof(bbs_rec))) == eSuccess) {
				x = *InComing;
				//OutGoing++;
				//strcpy(OutGoing, IBBSInfo.szThisNodeAddress);
				//od_printf("A");
				//od_get_answer("1");



				InComing += NODE_ADDRESS_CHARS + 2;
				for (cnt = 0; cnt < x; cnt++) {
					//od_printf("B");
					//od_get_answer("1");
					//printf("???%s???%ld???\n", ((best_rec_type *) (InComing + (cnt * sizeof(best_rec))))->name, ((best_rec_type *) (InComing + (cnt * sizeof(best_rec))))->points);
					AddBestPlayerInIB(((best_rec_type *) (InComing + (cnt * sizeof(best_rec))))->name, ((best_rec_type *) (InComing + (cnt * sizeof(best_rec))))->points, InComing - NODE_ADDRESS_CHARS - 1);
				}
				//od_printf("C");
				//od_get_answer("1");

				InComing -= NODE_ADDRESS_CHARS + 2;
			}
			free(InComing);

			sprintf(IBBSInfo.szProgName, "#@NYG#%05d ACTIONS", ibbsi_game_num);

			//od_printf("8");
			//od_get_answer("1");

			while (IBGet(&IBBSInfo, (char *)&act_rec, sizeof(ibbs_act_rec)) == eSuccess) {
				if (act_rec.action == 0) {
					justfile = ShareFileOpen(IBBSSPY_FILENAME, "rb");
					if (justfile != NULL) {
						while (justfile != NULL && ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
							if (strcmp(bbs_spy_rec.node, act_rec.node_s) == 0) {
								if (bbs_spy_rec.player_list == 0) {
									if (strcmp(act_rec.node_s, IBBSInfo.szThisNodeAddress) != 0) {
										sprintf(IBBSInfo.szProgName, "#@NYG#%05d LISTREQ", ibbsi_game_num);
										IBSend(&IBBSInfo, act_rec.node_s, IBBSInfo.szThisNodeAddress, NODE_ADDRESS_CHARS + 1);
										sprintf(IBBSInfo.szProgName, "#@NYG#%05d ACTIONS", ibbsi_game_num);
									}
									break;
								} else {
									sprintf(numstr, SBYDB_PREFIX".%03d", bbs_spy_rec.player_list);
									fclose(justfile);
									justfile = ShareFileOpen(numstr, "r+b");
									x = 0;
									while (justfile != NULL && ny_fread(&ibscr_rec, sizeof(ibbs_scr_rec), 1, justfile) == 1) {
										if (strcmp(ibscr_rec.nameI, act_rec.name_sI) == 0) {
											strcpy(ibscr_rec.name, act_rec.name_s);
											fseek(justfile, (INT32)x * sizeof(ibbs_scr_rec), SEEK_SET);
											ny_fwrite(&ibscr_rec, sizeof(ibbs_scr_rec), 1, justfile);
											break;
										}
										x++;
										fseek(justfile, (INT32)x * sizeof(ibbs_scr_rec), SEEK_SET);
									}
									break;
								}
							}
						}
						if(justfile != NULL)
							fclose(justfile);
					} else {
						if (strcmp(act_rec.node_s, IBBSInfo.szThisNodeAddress) != 0) {
							sprintf(IBBSInfo.szProgName, "#@NYG#%05d LISTREQ", ibbsi_game_num);
							IBSend(&IBBSInfo, act_rec.node_s, IBBSInfo.szThisNodeAddress, NODE_ADDRESS_CHARS + 1);
							sprintf(IBBSInfo.szProgName, "#@NYG#%05d ACTIONS", ibbsi_game_num);
						}
					}
				} else if (act_rec.action == 1) {
					justfile = ShareFileOpen(USER_FILENAME, "rb");
					scr_spy_rec.nameI[0] = 0;
					if(justfile != NULL) {
						while (ny_fread(&urec_i, sizeof(user_rec), 1, justfile) == 1) {
							if (strcmp(urec_i.bbsname, act_rec.name_rI) == 0) {
								strcpy(scr_spy_rec.name, urec_i.name);
								strcpy(scr_spy_rec.nameI, urec_i.bbsname);
								scr_spy_rec.nation = urec_i.nation;
								scr_spy_rec.level = urec_i.level;
								scr_spy_rec.points = urec_i.points;
								scr_spy_rec.sex = urec_i.sex;
								strcpy(scr_spy_rec.node, IBBSInfo.szThisNodeAddress);
								break;
							}
						}
						fclose(justfile);
					}
					if (scr_spy_rec.nameI[0] != 0) {
						sprintf(IBBSInfo.szProgName, "#@NYG#%05d SPYINFO", ibbsi_game_num);
						IBSend(&IBBSInfo, act_rec.node_s, (char *)&scr_spy_rec, sizeof(ibbs_scr_spy_rec));
						sprintf(IBBSInfo.szProgName, "#@NYG#%05d ACTIONS", ibbsi_game_num);
					}
				}
			}

			//od_printf("9");
			//od_get_answer("1");

			sprintf(IBBSInfo.szProgName, "#@NYG#%05d SPYINFO", ibbsi_game_num);
			while (IBGet(&IBBSInfo, (char *)&scr_spy_rec, sizeof(ibbs_scr_spy_rec)) == eSuccess) {
				//od_printf("*");
				//od_get_answer("1");

				justfile = ShareFileOpen(IBBSSPY_FILENAME, "rb");
				if (justfile != NULL) {
					while (justfile != NULL && ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
						if (strcmp(bbs_spy_rec.node, scr_spy_rec.node) == 0 && bbs_spy_rec.player_list != 0) {
							sprintf(numstr, SBYDB_PREFIX".%03d", bbs_spy_rec.player_list);
							fclose(justfile);
							justfile = ShareFileOpen(numstr, "r+b");
							x = 0;
							while (justfile != NULL && ny_fread(&ibscr_rec, sizeof(ibbs_scr_rec), 1, justfile) == 1) {
								if (strcmp(ibscr_rec.nameI, scr_spy_rec.nameI) == 0) {
									strcpy(ibscr_rec.name, scr_spy_rec.name);
									ibscr_rec.nation = scr_spy_rec.nation;
									ibscr_rec.level = scr_spy_rec.level;
									ibscr_rec.points = scr_spy_rec.points;
									ibscr_rec.sex = scr_spy_rec.sex;
									fseek(justfile, (INT32)x * sizeof(ibbs_scr_rec), SEEK_SET);
									ny_fwrite(&ibscr_rec, sizeof(ibbs_scr_rec), 1, justfile);
									break;
								}
								x++;
								if(justfile != NULL)
									fseek(justfile, (INT32)x * sizeof(ibbs_scr_rec), SEEK_SET);
							}
							break;
						}
					}
					if(justfile != NULL)
						fclose(justfile);
				}
			}
			//printf("~~~");
			//od_printf("10");
			//od_get_answer("1");
		}
		if (ibbs_maint_o) {
			//od_control.od_status_on = FALSE;
			//od_set_statusline(STATUS_NONE);
			printf("\n##> Processing Outgoing Information\n");

			sprintf(IBBSInfo.szProgName, "#@NYG#%05d BBSINFO", ibbsi_game_num);
			strcpy(bbs_rec.node, IBBSInfo.szThisNodeAddress);

			justfile = ShareFileOpen(USER_FILENAME, "rb");
			if(justfile != NULL) {
				bbs_rec.hi_points = 0;
				cnt = 0;
				while (ny_fread(&urec_i, sizeof(user_rec), 1, justfile) == 1) {
					if (urec_i.points > bbs_rec.hi_points)
						bbs_rec.hi_points = urec_i.points;
				}
				fclose(justfile);
			}

			if (IBSendAll(&IBBSInfo, (char *)&bbs_rec, sizeof(ibbs_bbs_rec)) != eSuccess) {
				printf("\n\nINTERBBS ERROR:Can't send the BBSINFO!!!\n\n");
				sleep(4);
			}

			if (!fexist(SENTLIST_FILENAME)) {
				//OutGoing = (char *)malloc(((25 + 36) * 10) + NODE_ADDRESS_CHARS + 2);

				sprintf(IBBSInfo.szProgName, "#@NYG#%05d NAMELIST", ibbsi_game_num);
				justfile = ShareFileOpen(USER_FILENAME, "rb");
				if(justfile != NULL) {
					cnt = filelength(fileno(justfile)) / sizeof(user_rec);
					//printf("\n& Len of file: %ld  len of user_rec : %ld", (INT32)filelength(fileno(justfile)), (INT32)sizeof(user_rec));
					OutGoing = (char *)malloc(((25 + 36 + 1) * cnt) + NODE_ADDRESS_CHARS + 4);
					OutGoing += 2;
					x = 0;
					//strcpy(OutGoing + 1 + ((25 + 36 + 1) * cnt), IBBSInfo.szThisNodeAddress);
					while (ny_fread(&urec_i, sizeof(user_rec), 1, justfile) == 1) {
						strcpy((OutGoing + 1 + ((25 + 36 + 1) * x)), urec_i.name);
						//printf("\n[%d] - {%s} - |%s|", x, (OutGoing + 1 + ((25 + 36 + 1) * x)), urec_i.name);
						strcpy(OutGoing + 1 + ((25 + 36 + 1) * x) + 25, urec_i.bbsname);
						//printf("\n[%d] - {%s} - |%s|", x, (OutGoing + 1 + ((25 + 36 + 1) * x) + 25), urec_i.bbsname);
						*(OutGoing + (25 + 36 + 1) * (x + 1)) = (char)urec_i.sex;
						//printf("\n[%d] - {%s} - |%s|", x, (OutGoing + 1 + ((25 + 36 + 1) * x)), urec_i.name);
						x++;
					}
					strcpy(OutGoing + 1 + ((25 + 36 + 1) * cnt), IBBSInfo.szThisNodeAddress);
					if (x > 0) {
						*OutGoing = cnt;
						//printf("\nPlayers on the list: {%d}\n", cnt);
						OutGoing -= 2;
						*(INT16 *)OutGoing = ((25 + 36 + 1) * cnt) + NODE_ADDRESS_CHARS + 2;
						IBSendAll(&IBBSInfo, OutGoing, ((25 + 36 + 1) * cnt) + NODE_ADDRESS_CHARS + 4);
					} else {
						OutGoing -= 2;
					}
					fclose(justfile);
					free(OutGoing);
				}
				justfile = ShareFileOpen(SENTLIST_FILENAME, "wb");
				if(justfile != NULL)
					fclose(justfile);
			}

			if (!fexist(SENTBESTTEN_FILENAME)) {
				justfile = ShareFileOpen(BESTTEN_FILENAME, "rb");
				if (justfile != NULL) {
					//best_rec_type best_rec;

					sprintf(IBBSInfo.szProgName, "#@NYG#%05d TENBEST", ibbsi_game_num);
					OutGoing = (char *)malloc(NODE_ADDRESS_CHARS + 2 + (10 * sizeof(best_rec)));
					OutGoing++;
					strcpy(OutGoing, IBBSInfo.szThisNodeAddress);
					OutGoing += NODE_ADDRESS_CHARS + 1;
					x = 0;
					while (ny_fread(&best_rec, sizeof(best_rec), 1, justfile) == 1) {
						memcpy((OutGoing + (x * sizeof(best_rec))), &best_rec, sizeof(best_rec));
						x++;
					}
					fclose(justfile);
					OutGoing -= NODE_ADDRESS_CHARS + 2;
					*OutGoing = (char)x;
					if (IBSendAll(&IBBSInfo, OutGoing, NODE_ADDRESS_CHARS + 2 + (10 * sizeof(best_rec))) != eSuccess) {
						printf("\n\nINTERBBS ERROR:Can't send the TEN BEST LIST!!!\n\n");
						sleep(4);
					}
					free(OutGoing);
					justfile = ShareFileOpen(SENTBESTTEN_FILENAME, "wb");
					if(justfile != NULL)
						fclose(justfile);
				}
			}
			sprintf(IBBSInfo.szProgName, "#@NYG#%05d LISTREQ", ibbsi_game_num);

			if (!fexist(IBBSSPY_FILENAME)) {
				IBSendAll(&IBBSInfo, IBBSInfo.szThisNodeAddress, NODE_ADDRESS_CHARS + 1);
				justfile = ShareFileOpen(IBBSSPY_FILENAME, "wb");
				if(justfile != NULL) {
					for (x = 0; x < IBBSInfo.nTotalSystems; x++) {
						if (strcmp(IBBSInfo.paOtherSystem[x].szAddress, IBBSInfo.szThisNodeAddress) != 0) {
							strcpy(bbs_spy_rec.node, IBBSInfo.paOtherSystem[x].szAddress);
							bbs_spy_rec.hi_points = 0;
							bbs_spy_rec.players = 0;
							bbs_spy_rec.player_list = 0;
							ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile);
						}
					}
					fclose(justfile);
				}
			} else {
				justfile = ShareFileOpen(IBBSSPY_FILENAME, "r+b");
				//fseek(justfile, 0, SEEK_SET);

				pack_spy = FALSE;
				xx = 0;
				if(justfile != NULL) {
					while (ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
						found = FALSE;
						for (x = 0; x < IBBSInfo.nTotalSystems; x++) {
							if (strcmp(IBBSInfo.paOtherSystem[x].szAddress, bbs_spy_rec.node) == 0) {
								found = TRUE;
								break;
							}
						}
						if (found == FALSE) {
							bbs_spy_rec.node[0] = 0;
							fseek(justfile, (INT32)xx * sizeof(ibbs_bbs_spy_rec), SEEK_SET);
							ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile);

							pack_spy = TRUE;
						}
						xx++;
						fseek(justfile, (INT32)xx * sizeof(ibbs_bbs_spy_rec), SEEK_SET);
					}
					fclose(justfile);
				}

				if (pack_spy == TRUE) {
					copyfile(IBBSSPY_FILENAME, IBBSSPY_TEMPFILENAME);
					justfile = ShareFileOpen(IBBSSPY_TEMPFILENAME, "rb");
					njustfile = ShareFileOpen(IBBSSPY_FILENAME, "wb");
					if(justfile != NULL && njustfile != NULL) {
						while (ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
							if (bbs_spy_rec.node[0] != 0) {
								ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, njustfile);
							} else if (bbs_spy_rec.player_list != 0) {
								sprintf(numstr, SBYDB_PREFIX".%03d", bbs_spy_rec.player_list);
								ny_remove(numstr);
							}
						}
						fclose(justfile);
						fclose(njustfile);
					}
					ny_remove(IBBSSPY_TEMPFILENAME);
				}

				for (x = 0; x < IBBSInfo.nTotalSystems; x++) {
					found = FALSE;
					justfile = ShareFileOpen(IBBSSPY_FILENAME, "rb");
					if(justfile != NULL) {
						while (ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
							if (strcmp(bbs_spy_rec.node, IBBSInfo.paOtherSystem[x].szAddress) == 0) {
								found = TRUE;
								if (bbs_spy_rec.player_list == 0 && strcmp(bbs_spy_rec.node, IBBSInfo.szThisNodeAddress) != 0)
									IBSend(&IBBSInfo, bbs_spy_rec.node, IBBSInfo.szThisNodeAddress, NODE_ADDRESS_CHARS + 1);
								break;
							}
						}
						fclose(justfile);
					}
					if (found == FALSE) {
						justfile = ShareFileOpen(IBBSSPY_FILENAME, "a+b");
						strcpy(bbs_spy_rec.node, IBBSInfo.paOtherSystem[x].szAddress);
						bbs_spy_rec.hi_points = 0;
						bbs_spy_rec.players = 0;
						bbs_spy_rec.player_list = 0;
						if(justfile != NULL) {
							ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile);
							fclose(justfile);
						}
						if (strcmp(bbs_spy_rec.node, IBBSInfo.szThisNodeAddress) != 0)
							IBSend(&IBBSInfo, bbs_spy_rec.node, IBBSInfo.szThisNodeAddress, NODE_ADDRESS_CHARS + 1);
					}
				}
			}
			copyfile(IBBSSPY_FILENAME, IBBSSPY_TEMPFILENAME);
			justfile = ShareFileOpen(IBBSSPY_TEMPFILENAME, "rb");
			njustfile = ShareFileOpen(IBBSSPY_FILENAME, "wb");
			if(justfile != NULL && njustfile != NULL) {
				while (ny_fread(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, justfile) == 1) {
					for (x = 0; x < IBBSInfo.nTotalSystems; x++) {
						if (strcmp(bbs_spy_rec.node, IBBSInfo.paOtherSystem[x].szAddress) == 0) {
							ny_fwrite(&bbs_spy_rec, sizeof(ibbs_bbs_spy_rec), 1, njustfile);
							break;
						}
					}
				}
				fclose(njustfile);
				fclose(justfile);
			}
			ny_remove(IBBSSPY_TEMPFILENAME);
		}
	}
	//printf("&&&");
	exit(10);
	//printf("***");
	return (0);
}


size_t
ny_fwrite(const void *ptr, size_t size, size_t n, FILE * stream) {
	size_t          status;
	INT32            offset;

	offset = ftell(stream);

	if (single_node == FALSE && filelength(fileno(stream)) >= offset + (size * n)) {
		//offset = ftell(stream);

		lock(fileno(stream), offset, size * n)
			;
		status = fwrite(ptr, size, n, stream);
		unlock(fileno(stream), offset, size * n);
	} else {
		status = fwrite(ptr, size, n, stream);
	}
	//if (no_slices == FALSE)
	//time_slice();
	return (status);
}


size_t
ny_fread(void *ptr, size_t size, size_t n, FILE * stream) {
	size_t          status;
	INT32            offset;

	offset = ftell(stream);

	if (single_node == FALSE && filelength(fileno(stream)) >= offset + (size * n)) {
		offset = ftell(stream);

		lock(fileno(stream), offset, size * n)
			;
		status = fread(ptr, size, n, stream);
		unlock(fileno(stream), offset, size * n);
	} else {
		status = fread(ptr, size, n, stream);
	}
	//if (no_slices == FALSE)
	//time_slice();
	return (status);
}

FILE           *ShareFileOpen(const char *pszFileName, const char *pszMode) {
	FILE           *fpFile = NULL;
	time_t          StartTime = time(NULL);
	//char numstr[14];
	//ffblk ffblk;
	//ny_kernel();


	/* Attempt to open the file while there is still time remaining. */
	if              (single_node == FALSE) {

		/*
		   if(no_kernel==FALSE) { ny_kernel();
		   sprintf(numstr,"u%07d.rnk",nCurrentUserNumber); if
		   (fexist(numstr)) { no_kernel=TRUE;
		   fpFile=ShareFileOpen(numstr,"rb");
		   ny_fread(&cur_user.rank,2,1,fpFile); no_kernel=FALSE;
		   fclose(fpFile); ny_remove(numstr); wrt_sts(); } }
		*/

		while ((fpFile = _fsopen(pszFileName, pszMode, SH_DENYNO)) == NULL
		        && errno == EACCES
		        && difftime(time(NULL), StartTime) < FILE_ACCESS_MAX_WAIT) {

			/*
			   If we were unable to open the file, call od_kernal, so
			   that
			*/

			/*
			   OpenDoors can continue to respond to sysop function keys,
			   loss
			*/
			/* of connection, etc.                                            */
			//od_kernal();
		}
	} else {
		fpFile = fopen(pszFileName, pszMode);
	}

	/* Return FILE pointer for opened file, if any. */
	return (fpFile);
}

FILE           *ShareFileOpenAR(const char *pszFileName, const char *pszMode) {
	FILE           *fpFile = NULL;
	time_t          StartTime = time(NULL);
	//char numstr[14];
	//ffblk ffblk;
	//ny_kernel();


	/* Attempt to open the file while there is still time remaining. */

	if              (single_node == FALSE) {

		/*
		   if(no_kernel==FALSE) { ny_kernel();
		   sprintf(numstr,"u%07d.rnk",nCurrentUserNumber); if
		   (fexist(numstr)) { no_kernel=TRUE;
		   fpFile=ShareFileOpen(numstr,"rb");
		   ny_fread(&cur_user.rank,2,1,fpFile); no_kernel=FALSE;
		   fclose(fpFile); ny_remove(numstr); wrt_sts(); } }
		*/

		while ((fpFile = fopen(pszFileName, pszMode)) == NULL
		        && errno == EACCES
		        && difftime(time(NULL), StartTime) < FILE_ACCESS_MAX_WAIT) {

			/*
			   If we were unable to open the file, call od_kernal, so
			   that
			*/

			/*
			   OpenDoors can continue to respond to sysop function keys,
			   loss
			*/
			/* of connection, etc.                                            */
			//od_kernal();
		}
	} else {
		fpFile = fopen(pszFileName, pszMode);

	}

	/* Return FILE pointer for opened file, if any. */
	return (fpFile);
}


INT16
ny_remove(const char *pszFileName) {
	//FILE * fpFile = NULL;
	time_t          StartTime;
	//char numstr[80];
	//ffblk ffblk;
	//ny_kernel();

	if              (!glob(pszFileName, 0, NULL, &fff))
		return (0);

	for(fname=fff.gl_pathv;*fname!=NULL;fname++) {
		StartTime = time(NULL);
		/* Attempt to open the file while there is still time remaining. */
		if (single_node == FALSE) {
			while (remove
			        (*fname) == -1
			        && errno == EACCES
			        && difftime(time(NULL), StartTime) < FILE_ACCESS_MAX_WAIT) {

				/*
				   If we were unable to open the file, call od_kernal, so
				   that
				*/

				/*
				   OpenDoors can continue to respond to sysop function
				   keys, loss
				*/
				/* of connection, etc.                                            */
				//od_kernal();
			}
		} else {
			remove
				(*fname);
		}
	}
	globfree(&fff);
	return (1);
}



/* The WaitForEnter() function is used to create custom prompt */
//void
//WaitForEnter(void)
//{
/* Display prompt. */
//od_control.od_ker_exec = NULL;
//ny_line(1, 1, 0);
//ny_disp_emu("\n\r`@Smack [ENTER] to go on.");

//od_control.od_ker_exec = ny_kernel;

/* Wait for a Carriage Return or Line Feed character from the user. */
//od_get_answer("\n\r");

//debug
// od_printf("%d\n\r", nCurrentUserNumber);
//ny_get_answer("\n\r");
//    }


/*
   void ny_disp_emu(const char line[]) { int cnt;

for(cnt=0;line[cnt]!=0;cnt++) { if(line[cnt]=='`') { cnt++;

if(line[cnt]==0) return; else if(line[cnt]=='`') od_putch('`'); else
   if(line[cnt]=='v') od_printf(ver); else if(line[cnt]=='w')
   od_printf(verinfo); else if(line[cnt]=='0') od_set_attrib(0x0a); else
   if(line[cnt]=='1') od_set_attrib(0x01); else if(line[cnt]=='2')
   od_set_attrib(0x02); else if(line[cnt]=='3') od_set_attrib(0x03); else
   if(line[cnt]=='4') od_set_attrib(0x04); else if(line[cnt]=='5')
   od_set_attrib(0x05); else if(line[cnt]=='6') od_set_attrib(0x06); else
   if(line[cnt]=='7') od_set_attrib(0x07); else if(line[cnt]=='8')
   od_set_attrib(0x08); else if(line[cnt]=='9') od_set_attrib(0x09); else
   if(line[cnt]=='!') od_set_attrib(0x0b); else if(line[cnt]=='@')
   od_set_attrib(0x0c); else if(line[cnt]=='#') od_set_attrib(0x0d); else
   if(line[cnt]=='$') od_set_attrib(0x0e); else if(line[cnt]=='%')
   od_set_attrib(0x0f);

} else if (line[cnt]==9) { od_printf("        "); } else {
   od_putch(line[cnt]); } } od_kernal(); }
*/

INT16
copyfile(const char *file1, const char *file2) {
	FILE           *f1, *f2;
	char           *buffer;
	INT32            len1, x, y;
	//where, x, y;


	f1 = ShareFileOpen(file1, "rb");
	if              (f1 == NULL)
		return (0);
	f2 = ShareFileOpenAR(file2, "wb");
	if              (f2 == NULL) {
		fclose(f1);
		return (0);
	}

	buffer = (char *)malloc(10000);

	len1 = filelength(fileno(f1));
	x = len1 / 10000;

	for (y = 0; y < x; y++) {
		ny_fread(buffer, 10000, 1, f1);
		ny_fwrite(buffer, 10000, 1, f2);
	}

	y = len1 - (x * 10000);

	if (y > 0) {
		ny_fread(buffer, y, 1, f1);
		ny_fwrite(buffer, y, 1, f2);
	}

	fclose(f1);
	fclose(f2);
	free(buffer);

	return (1);
}


/* CustomConfigFunction() is called by OpenDoors to process custom */
/* configuration file keywords that                                */

/*
   void CustomConfigFunction(char *pszKeyword, char *pszOptions) {
   if(stricmp(pszKeyword, "SingleNodeOnly") == 0) { single_node=TRUE; }
   else if(stricmp(pszKeyword, "NoMultitasker") == 0) { no_slices=TRUE; }
   else if(stricmp(pszKeyword, "PollingValue") == 0) {
   sscanf(pszOptions,"%d",&time_slice_value); if (time_slice_value<0)
   time_slice_value=0; if (time_slice_value>2000) time_slice_value=2000;
   } else if(stricmp(pszKeyword, "InterBBS") == 0) { ibbs=TRUE; } else
   if(stricmp(pszKeyword, "InterBBSOperator") == 0) { ibbs_operator=TRUE;
   } else if(stricmp(pszKeyword, "InterBBSGameNumber") == 0) {
   sscanf(pszOptions,"%d",&ibbs_game_num); } }
*/

void
strzcpy(char dest[], const char src[], INT16 beg, INT16 end) {
	INT16             cnt = 0;
	do {
		dest[cnt] = src[beg];
		beg++;
		cnt++;
	} while         (cnt <= end && src[cnt] != 0);

	dest[cnt] = 0;
}
