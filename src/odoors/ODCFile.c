/* OpenDoors Online Software Programming Toolkit
 * (C) Copyright 1991 - 1999 by Brian Pirie.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *        File: ODCFile.c
 *
 * Description: Implements the configuration file sub-system.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Nov 11, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 01, 1996  6.00  BP   Added DisableDTR and NoDTRDisable.
 *              Jan 19, 1996  6.00  BP   Display error if config file not found
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "OpenDoor.h"
#include "ODStr.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODInEx.h"
#include "ODUtil.h"


/* Internal private variables */
static WORD awTimeVal[3];
static BYTE btTimeNumVals;


/* Local functions. */
static WORD ODCfgGetWordDecimal(char *pszConfigText);
static DWORD ODCfgGetDWordDecimal(char *pszConfigText);
static WORD ODCfgGetWordHex(char *pszConfigText);
static void ODCfgGetNextTime(char **ppchConfigText);
static BOOL ODCfgIsTrue(char *pszConfigText);


/* ----------------------------------------------------------------------------
 * ODConfigInit()
 *
 * Called to perform OpenDoors initialization when the configuration file
 * system is being used. This function is called from the normal od_init(),
 * and also uses the normal od_init() to perform base initialization after
 * the configuration file has been read, but before certain configuration
 * settings are set in od_control.
 *
 * Parameters: none
 *
 *     Return: void
 */
ODAPIDEF void ODCALL ODConfigInit(void)
{
   void (*custom_line_function)(char *keyword, char *options)
      = od_control.config_function;
   char *pchConfigText;
   WORD wCurrent;
   INT nConfigOption;
   BOOL bConfigFileRequired = TRUE;
   static FILE *pfConfigFile;
   static FILE *pfCustomDropFile = NULL;
   static char szConfigLine[257];
   static char szToken[33];
   static char szTempString[256];
   static char szWorkDir[80];
   static BOOL bWorkDirSet = FALSE;
   static time_t nUnixTime;
   static struct tm *TimeBlock;
   static INT16 nPageStart;
   static INT16 nPageEnd;
   static BOOL bPageSet = FALSE;
   static BOOL bInactivitySet = FALSE;
   static INT16 nInactivity;
   static char *pszWork;
   static BOOL bPageLengthSet = FALSE;
   static BYTE btPageLength;
   static char *apszFileNames[1];

   bIsCallbackActive = TRUE;

   nUnixTime = time(NULL);
   TimeBlock = localtime(&nUnixTime);

   /* Use default configuration file filename if none has been specified. */
   if(od_control.od_config_filename == NULL)
   {
      od_control.od_config_filename = "door.cfg";
      bConfigFileRequired = FALSE;
   }

   if((pfConfigFile = fopen(od_control.od_config_filename, "rt")) == NULL)
   {
      if(strchr(od_control.od_config_filename, DIRSEP) != NULL
         || strchr(od_control.od_config_filename, ':') != NULL)
      {
         wCurrent = strlen(od_control.od_config_filename);
         pchConfigText = (char *)od_control.od_config_filename + (wCurrent - 1);
         while(wCurrent > 0)
         {
            if(*pchConfigText == DIRSEP || *pchConfigText == ':')
            {
               strcpy(szConfigLine, (char *)pchConfigText + 1);
               pfConfigFile = fopen(szConfigLine, "rt");
               break;
            }

            --pchConfigText;
            --wCurrent;
         }
      }
      else
      {
         strcpy(szConfigLine, od_control.od_config_filename);
      }
   }

   /* If we were able to open the configuration file. */
   if(pfConfigFile != NULL)
   {
      /* Get configuration file strings in upper case. */
      for(wCurrent = 0; wCurrent < TEXT_SIZE; ++wCurrent)
      {
         strupr(od_config_text[wCurrent]);
      }
      for(wCurrent = 0; wCurrent < LINES_SIZE; ++wCurrent)
      {
         strupr(od_config_lines[wCurrent]);
      }

      for(;;)
      {
         /* Read the next line from the configuration file. */
         if(fgets(szConfigLine, 257, pfConfigFile) == NULL) break;

         /* Ignore all of line after comments or CR/LF char. */
         pchConfigText = (char *)szConfigLine;
         while(*pchConfigText)
         {
            if(*pchConfigText == '\n' || *pchConfigText == '\r'
               || *pchConfigText == ';')
            {
               *pchConfigText = '\0';
               break;
            }
            ++pchConfigText;
         }

         /* Search for beginning of first token on line. */
         pchConfigText = (char *)szConfigLine;
         while(*pchConfigText
            && (*pchConfigText == ' ' || *pchConfigText == '\t'))
         {
            ++pchConfigText;
         }
         if(!*pchConfigText) continue;

         /* Get first token from line. */
         wCurrent = 0;
         while(*pchConfigText
            && !(*pchConfigText == ' ' || *pchConfigText == '\t'))
         {
            if(wCurrent < 32) szToken[wCurrent++] = *pchConfigText;
            ++pchConfigText;
         }
         if(wCurrent <= 32)
         {
            szToken[wCurrent] = '\0';
         }
         else
         {
            szToken[32] = '\0';
         }
         strupr(szToken);

         /* Find beginning of configuration option parameters */
         while(*pchConfigText && (*pchConfigText == ' '
            || *pchConfigText == '\t'))
         {
            ++pchConfigText;
         }

         /* Trim trailing spaces from setting string. */
         for(wCurrent = strlen(pchConfigText) - 1; wCurrent > 0; --wCurrent)
         {
            if(pchConfigText[wCurrent] == ' '
               || pchConfigText[wCurrent] == '\t')
            {
               pchConfigText[wCurrent] = '\0';
            }
            else
            {
               break;
            }
         }


         for(wCurrent = 0; wCurrent < TEXT_SIZE; ++wCurrent)
         {
            if(strcmp(szToken, od_config_text[wCurrent]) == 0)
            {
               switch(wCurrent)
               {
                  case 0:
                     wODNodeNumber = ODCfgGetWordDecimal(pchConfigText);
                     break;

                  case 1:
                     strcpy(od_control.info_path,pchConfigText);
                     break;

                  case 2:
                     if(pchConfigText[strlen(pchConfigText) - 1] == DIRSEP
                        && pchConfigText[strlen(pchConfigText) - 2] != ':'
                        && strlen(pchConfigText) > 1)
                     {
                        pchConfigText[strlen(pchConfigText) - 1] = '\0';
                     }

                     szOriginalDir = (char *)malloc(256);
                     if(szOriginalDir != NULL)
                     {
                        ODDirGetCurrent(szOriginalDir, 256);
                     }

                     strcpy(szWorkDir, pchConfigText);
                     bWorkDirSet = TRUE;
                     break;

                  case 3:
                     strcpy(od_control.od_logfile_name, pchConfigText);
                     break;

                  case 4:
                     od_control.od_logfile_disable = TRUE;
                     break;

                  case 5:
                  case 6:
                  case 7:
                  case 8:
                  case 9:
                  case 10:
                  case 11:
                      if((wCurrent - 5) == (WORD)TimeBlock->tm_wday)
                      {
                         ODCfgGetNextTime((char **)&pchConfigText);
                         nPageStart = awTimeVal[0] * 60 + awTimeVal[1];
                         ODCfgGetNextTime((char **)&pchConfigText);
                         nPageEnd = awTimeVal[0] * 60 + awTimeVal[1];
                         bPageSet = TRUE;
                      }
                     break;

                  case 12:
                     od_control.od_maxtime = ODCfgGetWordDecimal(pchConfigText);
                     break;

                  case 13:
                     bSysopNameSet = TRUE;
                     strncpy((char *)&szForcedSysopName, pchConfigText, 39);
                     szForcedSysopName[39] = '\0';
                     break;

                  case 14:
                     bSystemNameSet = TRUE;
                     strncpy((char *)&szForcedSystemName, pchConfigText, 39);
                     szForcedSystemName[39] = '\0';
                     break;

                  case 15:
                     od_control.od_swapping_disable = TRUE;
                     break;

                  case 16:
                     strncpy(od_control.od_swapping_path, pchConfigText, 79);
                     od_control.od_swapping_path[79] = '\0';
                     break;

                  case 17:
                     od_control.od_swapping_noems = TRUE;
                     break;

                  case 18:
                     dwForcedBPS = ODCfgGetDWordDecimal(pchConfigText);
                     break;

                  case 19:
                     nForcedPort = ODCfgGetWordDecimal(pchConfigText);
                     break;

                  case 20:
                     if(pfCustomDropFile == NULL && !od_control.od_force_local)
                     {
                        apszFileNames[0] = (char *)pchConfigText;
                        if(ODSearchForDropFile(apszFileNames, 1, szTempString,
                           NULL) != -1)
                        {
                           if((pfCustomDropFile = fopen(szTempString, "rt"))
                              != NULL)
                           {
                              od_control.od_info_type = CUSTOM;
                              od_control.user_attribute = 0x06;
                              od_control.user_screen_length = 23;
                              od_control.user_ansi = TRUE;
                              od_control.user_rip = FALSE;
                              od_control.user_avatar = FALSE;
                              od_control.od_page_pausing = TRUE;
                              od_control.od_page_len = 15;
                              od_control.user_timelimit = 0;
                              strcpy(od_control.user_name, "Unknown User");
                              strcpy(od_control.user_location,
                                 "Unknown Location");
                              od_control.user_security = 1;
                           }
                        }
                     }
                     break;

                  case 21:
                     if(pfCustomDropFile != NULL)
                     {
                        if(fgets(szTempString, 255, pfCustomDropFile)!=NULL)
                        {
                           if(szTempString[strlen(szTempString) - 1] == '\n')
                           {
                              szTempString[strlen(szTempString) - 1] = '\0';
                           }
                           else
                           {
                              INT ch;
                              do
                              {
                                 ch = fgetc(pfCustomDropFile);
                              } while(ch != '\n' && ch != EOF);
                           }
                           if(szTempString[strlen(szTempString) - 1] == '\r')
                           {
                              szTempString[strlen(szTempString) - 1] = '\0';
                           }

                           strupr(pchConfigText);

                           for(nConfigOption = 0; nConfigOption < LINES_SIZE;
                              ++nConfigOption)
                           {
                              if(strcmp(pchConfigText,
                                 od_config_lines[nConfigOption]) == 0)
                              {
                                 switch(nConfigOption)
                                 {
                                    case 1:
                                       od_control.port =
                                          ODCfgGetWordDecimal(szTempString) - 1;
                                       break;

                                    case 2:
                                       od_control.port =
                                          ODCfgGetWordDecimal(szTempString);
                                       break;

                                    case 3:
                                       od_control.baud =
                                          ODCfgGetWordDecimal(szTempString);
                                       break;

                                    case 4:
                                       if(ODCfgIsTrue(szTempString))
                                       {
#ifdef ODPLAT_NIX
                                          od_control.baud = 1;
#else
                                          od_control.baud = 0;
#endif
                                       }
                                       break;

                                    case 5:
                                    case 6:
                                       ODStringToName(szTempString);
                                       strncpy(od_control.user_name,
                                          szTempString, 34);
                                       od_control.user_name[34] = '\0';
                                       break;

                                    case 7:
                                       strcat(od_control.user_name, " ");
                                       ODStringToName(szTempString);
                                       strncat(od_control.user_name,
                                          szTempString,
                                          35 - strlen(od_control.user_name));
                                       od_control.user_name[35] = '\0';
                                       break;

                                    case 8:
                                       ODStringToName(szTempString);
                                       strncpy(od_control.user_handle,
                                          szTempString, 35);
                                       od_control.user_handle[35] = '\0';
                                       break;

                                    case 9:
                                       pszWork = (char *)szTempString;
                                       ODCfgGetNextTime((char **)&pszWork);
                                       od_control.user_timelimit += 
                                          (awTimeVal[0] * 60);
                                       break;

                                    case 10:
                                       pszWork = (char *)szTempString;
                                       ODCfgGetNextTime((char **)&pszWork);
                                       if(btTimeNumVals <= 1)
                                       {
                                          od_control.user_timelimit +=
                                             awTimeVal[0];
                                       }
                                       else
                                       {
                                          od_control.user_timelimit +=
                                             awTimeVal[1] + (awTimeVal[0] * 60);
                                       }
                                       break;

                                    case 11:
                                       pszWork = (char *)szTempString;
                                       ODCfgGetNextTime((char **)&pszWork);
                                       if(btTimeNumVals <= 1)
                                       {
                                          od_control.user_timelimit +=
                                             awTimeVal[0] / 60;
                                       }
                                       else if(btTimeNumVals == 2)
                                       {
                                          od_control.user_timelimit +=
                                             (awTimeVal[1] / 60) + awTimeVal[0];
                                       }
                                       else
                                       {
                                          od_control.user_timelimit +=
                                             (awTimeVal[2] / 60) + awTimeVal[1]
                                             + (awTimeVal[0] * 60);
                                       }
                                       break;

                                    case 12:
                                       od_control.user_ansi =
                                          ODCfgIsTrue(szTempString);
                                       break;

                                    case 13:
                                       od_control.user_avatar =
                                          ODCfgIsTrue(szTempString);
                                       break;

                                    case 14:
                                       od_control.od_page_pausing =
                                          ODCfgIsTrue(szTempString);
                                       break;

                                    case 15:
                                       od_control.user_screen_length =
                                          ODCfgGetWordDecimal(szTempString);
                                       break;

                                    case 16:
                                       if(ODCfgIsTrue(szTempString))
                                        {
                                           od_control.user_attribute |= 0x02;
                                        }
                                       else
                                        {
                                           od_control.user_attribute &=~ 0x02;
                                        }
                                       break;

                                    case 17:
                                       od_control.user_security =
                                          ODCfgGetWordDecimal(szTempString);
                                       break;

                                    case 18:
                                       ODStringToName(szTempString);
                                       strncpy(od_control.user_location,
                                          szTempString, 25);
                                       od_control.user_location[25] = '\0';
                                       break;

                                    case 19:
                                       wODNodeNumber =
                                          ODCfgGetWordDecimal(szTempString);
                                       break;

                                    case 20:
                                    case 21:
                                       ODStringToName(szTempString);
                                       strncpy(od_control.sysop_name,
                                          szTempString, 38);
                                       od_control.sysop_name[38] = '\0';
                                       break;

                                    case 22:
                                       strcat(od_control.sysop_name, " ");
                                       ODStringToName(szTempString);
                                       strncat(od_control.sysop_name,
                                          szTempString,
                                          39 - strlen(od_control.system_name));
                                       od_control.sysop_name[39] = '\0';
                                       break;

                                    case 23:
                                       strncpy(od_control.system_name,
                                          szTempString, 39);
                                       od_control.system_name[39] = '\0';
                                       break;

                                    case 24:
                                       od_control.user_rip =
                                          ODCfgIsTrue(szTempString);
                                 }
                              }
                           }
                        }
                     }
                     break;

                  case 22:
                     bInactivitySet = TRUE;
                     nInactivity = ODCfgGetWordDecimal(pchConfigText);
                     if(nInactivity < 0) nInactivity = 0;
                     break;

                  case 23:
                     btPageLength = (BYTE)ODCfgGetWordDecimal(pchConfigText);
                     bPageLengthSet = TRUE;
                     break;

                  case 24:
                     od_control.od_chat_color2 = 
                        od_color_config(pchConfigText);
                     break;

                  case 25:
                     od_control.od_chat_color1 =
                        od_color_config(pchConfigText);
                     break;

                  case 26:
                     od_control.od_list_title_col =
                        od_color_config(pchConfigText);
                     break;

                  case 27:
                     od_control.od_list_name_col =
                        od_color_config(pchConfigText);
                     break;

                  case 28:
                     od_control.od_list_size_col = 
                        od_color_config(pchConfigText);
                     break;

                  case 29:
                     od_control.od_list_comment_col =
                        od_color_config(pchConfigText);
                     break;

                  case 30:
                     od_control.od_list_offline_col =
                        od_color_config(pchConfigText);
                     break;

                  case 31:
                     strncpy(szDesiredPersonality, pchConfigText, 32);
                     szDesiredPersonality[32] = '\0';
                     break;

                  case 32:
                     /* "NoFossil" */
                     od_control.od_no_fossil = TRUE;
                     break;

                  case 33:
                     /* "PortAddress" */
                     od_control.od_com_address = ODCfgGetWordHex(pchConfigText);
                     break;

                  case 34:
                     /* "PortIRQ" */
                     od_control.od_com_irq =
                        (char)ODCfgGetWordDecimal(pchConfigText);
                     break;

                  case 35:
                     /* "ReceiveBuffer" */
                     od_control.od_com_rx_buf =
                        ODCfgGetWordDecimal(pchConfigText);
                     break;

                  case 36:
                     /* "TransmitBuffer" */
                     od_control.od_com_tx_buf =
                        ODCfgGetWordDecimal(pchConfigText);
                     break;

                  case 37:
                     /* "PagePromptColour" */
                     od_control.od_continue_col =
                        od_color_config(pchConfigText);
                     break;

                  case 38:
                     /* "LocalMode" */
                     od_control.od_force_local = TRUE;
                     break;

                  case 39:
                     /* "PopupMenuTitleColour" */
                     od_control.od_menu_title_col =
                        od_color_config(pchConfigText);
                     break;

                  case 40:
                     /* "PopupMenuBorderColour" */
                     od_control.od_menu_border_col =
                        od_color_config(pchConfigText);
                     break;

                  case 41:
                     /* "PopupMenuTextColour" */
                     od_control.od_menu_text_col =
                        od_color_config(pchConfigText);
                     break;

                  case 42:
                     /* "PopupMenuKeyColour" */
                     od_control.od_menu_key_col =
                        od_color_config(pchConfigText);
                     break;

                  case 43:
                     /* "PopupMenuHighlightColour" */
                     od_control.od_menu_highlight_col =
                        od_color_config(pchConfigText);
                     break;

                  case 44:
                     /* "PopupMenuHighKeyColour" */
                     od_control.od_menu_highkey_col = 
                        od_color_config(pchConfigText);
                     break;

                  case 45:
                     /* "NoFIFO" */
                     od_control.od_com_no_fifo = TRUE;
                     break;

                  case 46:
                     /* "FIFOTriggerSize" */
                     od_control.od_com_fifo_trigger =
                        (BYTE)ODCfgGetWordDecimal(pchConfigText);
                     break;

                  case 47:
                     /* "DisableDTR" */
                     ODStringCopy(od_control.od_disable_dtr, pchConfigText,
                        sizeof(od_control.od_disable_dtr));
                     break;

                  case 48:
                     /* "NoDTRDisable" */
                     od_control.od_disable |= DIS_DTR_DISABLE;
                     break;
               }
            }
         }

         /* Check if command is a programmer customized option. */
         if(wCurrent >= TEXT_SIZE && custom_line_function != NULL)
         {
            (*custom_line_function)((char *)&szToken, pchConfigText);
         }
      }

      /* Close the configuration file. */
      fclose(pfConfigFile);}
   else
   {
      if(bConfigFileRequired)
      {
         od_control.od_error = ERR_FILEOPEN;
         ODInitError("Unable to access configuration file.");
         exit(od_control.od_errorlevel[1]);
      }
   }

   /* Close custom door info file */
   if(pfCustomDropFile != NULL)
   {
      fclose(pfCustomDropFile);
   }

   bIsCallbackActive = FALSE;

   /* Carry out normal OpenDoors initialization. */
   bCalledFromConfig = TRUE;
   od_init();
   bCalledFromConfig = FALSE;

   /* Update any settings that need to be updated. */
   if(bPageSet)
   {
      od_control.od_pagestartmin = nPageStart;
      od_control.od_pageendmin = nPageEnd;
   }

   if(bInactivitySet && nInactivity != 0)
   {
      od_control.od_inactivity = nInactivity;
   }

   if(bSysopNameSet)
   {
      strcpy((char *)&od_control.sysop_name, (char *)&szForcedSysopName);
   }

   if(bSystemNameSet)
   {
      strcpy((char *)&od_control.system_name, (char *)&szForcedSystemName);
   }

   if(bPageLengthSet)
   {
      od_control.od_page_len = btPageLength;
   }

   if(bWorkDirSet)
   {
      ODDirChangeCurrent(szWorkDir);
   }
}


/* ----------------------------------------------------------------------------
 * ODCfgGetWordDecimal()                               *** PRIVATE FUNCTION ***
 *
 * Obtains the value of the next decimal number in the provided string, in the
 * form of a WORD (16 bit value).
 *
 * Parameters: pszConfigText - String to examine.
 *
 *     Return: The first number obtained from the string.
 */
static WORD ODCfgGetWordDecimal(char *pszConfigText)
{
   ASSERT(pszConfigText != NULL);

   /* Skip any initial non-numerical characters. */
   while(*pszConfigText && (*pszConfigText < '0' || *pszConfigText > '9'))
   {
      ++pszConfigText;
   }

   /* Return value of number. */
   return(atoi(pszConfigText));
}


/* ----------------------------------------------------------------------------
 * ODCfgGetDWordDecimal()                              *** PRIVATE FUNCTION ***
 *
 * Obtains the value of the next decimal number in the provided string, in the
 * form of a DWORD (32 bit value).
 *
 * Parameters: pszConfigText - String to examine.
 *
 *     Return: The first number obtained from the string.
 */
static DWORD ODCfgGetDWordDecimal(char *pszConfigText)
{                                
   ASSERT(pszConfigText != NULL);

   /* Skip any initial non-numerical characters. */
   while(*pszConfigText && (*pszConfigText < '0' || *pszConfigText > '9'))
   {
      ++pszConfigText;
   }

   /* Return value of number. */
   return(atol(pszConfigText));
}


/* ----------------------------------------------------------------------------
 * ODCfgGetWordHex()                                   *** PRIVATE FUNCTION ***
 *
 * Obtains the value of the next hexidecimal number in the provided string, in
 * the form of a WORD (16 bit value).
 *
 * Parameters: pszConfigText - String to examine.
 *
 *     Return: The first number obtained from the string.
 */
static WORD ODCfgGetWordHex(char *pszConfigText)
{
   WORD wToReturn;

   ASSERT(pszConfigText != NULL);

   /* Skip any initial non-hexidecimal characters. */
   while(*pszConfigText && (*pszConfigText < '0' || *pszConfigText > '9')
      && (toupper(*pszConfigText) < 'A' || toupper(*pszConfigText) > 'F'))
   {
      ++pszConfigText;
   }

   sscanf(pszConfigText, "%x", &wToReturn);

   return(wToReturn);
}


/* ----------------------------------------------------------------------------
 * ODCfgGetNextTime()                                  *** PRIVATE FUNCTION ***
 *
 * Obtains the next time from a string, updating the string pointer to point to
 * the position in the string after the end of the time. The time information
 * is stored in the btTimeNumVals and awTimeVal private global variables.
 *
 * Parameters: ppchConfigText - Pointer to character pointer to the string,
 *                              which is to be updated.
 *
 *     Return: void
 */
static void ODCfgGetNextTime(char **ppchConfigText)
{
   char *pchConfigText = (char *)(*ppchConfigText);

   ASSERT(ppchConfigText != NULL);
   ASSERT(*ppchConfigText != NULL);

   btTimeNumVals = 0;
   awTimeVal[0] = 0;
   awTimeVal[1] = 0;
   awTimeVal[2] = 0;


   while(*pchConfigText && (*pchConfigText == ' ' || *pchConfigText == '\t'))
   {
      ++pchConfigText;
   }


   while(*pchConfigText && btTimeNumVals < 3)
   {
      if(*pchConfigText < '0' || *pchConfigText > '9') break;
      awTimeVal[btTimeNumVals++] = atoi(pchConfigText);
      while(*pchConfigText && *pchConfigText >= '0' && *pchConfigText <= '9')
      {
         ++pchConfigText;
      }
      if(*pchConfigText == ':' || *pchConfigText == '.' || *pchConfigText == ','
         || *pchConfigText == ';')
      {
         ++pchConfigText;
      }
   }

   *ppchConfigText = (char *)pchConfigText;
}


/* ----------------------------------------------------------------------------
 * ODCfgIsTrue()                                       *** PRIVATE FUNCTION ***
 *
 * Determines whether the specified string represents a TRUE or FALSE value.
 * For example "Yes", "TRUE", "Y" and "1" all represent TRUE values, while
 * "No", "FALSE", "N" and "0" all represent FALSE values.
 *
 * Parameters: pszConfigText - String to examine.
 *
 *     Return: The Boolean value represented by the string.
 */
static BOOL ODCfgIsTrue(char *pszConfigText)
{
   ASSERT(pszConfigText != NULL);

   while(*pszConfigText && (*pszConfigText == ' ' || *pszConfigText == '\t'))
   {
      ++pszConfigText;
   }

   switch(*pszConfigText)
   {
      case '1':
      case 't':
      case 'T':
      case 'y':
      case 'Y':
      case 'g':
      case 'G':
         return(TRUE);
   }

   return(FALSE);
}
