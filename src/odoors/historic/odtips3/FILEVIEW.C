/* fileview.c - File viewing door that demonstrates the use of the    */
/*              PagedViewer() function. This door can be setup to     */
/*              to display a single text file, or any file from an    */
/*              entire directory of text files. The program accepts   */
/*              a single command-line argument, which if present,     */
/*              specifies the filename or wildcard of the files to    */
/*              display. If this argument is not present, all files   */
/*              in the current directory will be available for        */
/*              viewing. If there is more than one possible file to   */
/*              be displayed, this program will display a list of     */
/*              files that the user can choose from to display. If    */
/*              there is only one possible file, that file is         */
/*              is displayed. This program uses PagedViewer() for two */
/*              seperate uses - the list of available files, and for  */
/*              viewing the file itself.                              */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bpfind.h"
#include "opendoor.h"
#include "pageview.h"


/* Configurable constants. */
#define FILENAME_SIZE        75
#define PATH_CHARS           (FILENAME_SIZE - 13)
#define LINE_SIZE            80
#define ARRAY_GROW_SIZE      20


/* Global variables. */
int nTotalFiles = 0;
int nFileArraySize = 0;
char *paszFileArray = NULL;

FILE *pfCurrentFile;
int nTotalLines = 0;
int nLineArraySize = 0;
long *palLineOffset = NULL;


/* Function prototypes. */
void AddFilesMatching(char *pszFileSpec);
char *GetFilename(int nIndex);
int AddFilename(char *pszFilename);
void GetDirOnly(char *pszOutDirName, const char *pszInPathName);
int DirExists(const char *pszDirName);
void BuildPath(char *pszOut, char *pszPath, char *pszFilename);
void FreeFileList(void);
void DisplayFileName(int nLine, void *pCallbackData);
void DisplayFile(char *pszFilename);
void DisplayFileLine(int nLine, void *pCallbackData);
int AddOffsetToArray(long lOffset);
void FreeLineArray(void);


/* Program execution begins here. */
int main(int nArgCount, char *papszArgument[])
{
   int nArg;
   int nChoice;

   od_init();

   /* Get file specifiction from command-line, if any. */
   if(nArgCount >= 2)
   {
      for(nArg = 1; nArg < nArgCount; ++nArg)
      {
         AddFilesMatching(papszArgument[nArg]);
      }
   }

   /* If there are no command-line parameters, use *.* */
   else
   {
      AddFilesMatching("*.*");
   }

   /* If there are no matching files, display error. */
   if(nTotalFiles == 0)
   {
      od_printf("No files were found.\n\r\n\r");
      od_printf("Press [Enter] to continue.\n\r");
      od_get_answer("\n\r");
      return(0);
   }

   /* If only one file was found, then display it. */
   else if(nTotalFiles == 1)
   {
      DisplayFile(GetFilename(0));
   }

   /* If more than one file was found, allow user to choose file */
   /* to display.                                                */
   else
   {
      /* Loop until user chooses to quit. */
      nChoice = 0;
      for(;;)
      {
         /* Get user's selection. */
         nChoice = PagedViewer(nChoice, nTotalFiles, DisplayFileName,
            NULL, TRUE, "Choose A File To Display", 19);

         /* If user chose to quit, then exit door. */
         if(nChoice == NO_LINE) break;

         /* Otherwise, display the file that the user chose. */
         DisplayFile(GetFilename(nChoice));
      }
   }

   FreeFileList();

   return(0);
}


void AddFilesMatching(char *pszFileSpec)
{
   struct ffblk DirEntry;
   int bNoMoreFiles;
   char szDirName[PATH_CHARS + 1];
   char szFileName[FILENAME_SIZE];

   /* Check that file specification is not too long. */
   if(strlen(pszFileSpec) > PATH_CHARS)
   {
      return;
   }

   /* Get directory name from path. */
   GetDirOnly(szDirName, pszFileSpec);

   bNoMoreFiles = findfirst(pszFileSpec, &DirEntry, FA_RDONLY);
   while(!bNoMoreFiles)
   {
      BuildPath(szFileName, szDirName, DirEntry.ff_name);

      AddFilename(szFileName);

      bNoMoreFiles = findnext(&DirEntry);
   }
}


void GetDirOnly(char *pszOutDirName, const char *pszInPathName)
{
   char *pchBackslashChar;

   /* Default dir name is entire path. */
   strcpy(pszOutDirName, pszInPathName);

   /* If there is a backslash in the string. */
   pchBackslashChar = strrchr(pszOutDirName, '\\');
   if(pchBackslashChar != NULL)
   {
      /* Remove all character beginning at last backslash from path. */
      *pchBackslashChar = '\0';
   }
   else
   {
      /* If there is no backslash in the filename, then the dir name */
      /* is empty.                                                   */
      pszOutDirName[0] = '\0';
   }
}


void BuildPath(char *pszOut, char *pszPath, char *pszFilename)
{
   /* Copy path to output filename. */
   strcpy(pszOut, pszPath);

   /* Ensure there is a trailing backslash. */
   if(strlen(pszOut) > 0 && pszOut[strlen(pszOut) - 1] != '\\')
   {
      strcat(pszOut, "\\");
   }

   /* Append base filename. */
   strcat(pszOut, pszFilename);
}


char *GetFilename(int nIndex)
{
   return(paszFileArray + (nIndex * FILENAME_SIZE));
}


int AddFilename(char *pszFilename)
{
   int nNewArraySize;
   char *paszNewArray;
   char *pszNewString;

   /* If array is full, then try to grow it. */
   if(nTotalFiles == nFileArraySize)
   {
      nNewArraySize = nFileArraySize + ARRAY_GROW_SIZE;
      if((paszNewArray =
         realloc(paszFileArray, nNewArraySize * FILENAME_SIZE)) == NULL)
      {
         return(FALSE);
      }
      nFileArraySize = nNewArraySize;
      paszFileArray = paszNewArray;
   }

   /* Get address to place new string at, while incrementing total number */
   /* of filenames.                                                       */
   pszNewString = GetFilename(nTotalFiles++);

   /* Copy up to the maximum number of filename characters to the string. */
   strncpy(pszNewString, pszFilename, FILENAME_SIZE - 1);
   pszNewString[FILENAME_SIZE - 1] = '\0';

   return(TRUE);
}


void FreeFileList(void)
{
   if(nFileArraySize > 0)
   {
      free(paszFileArray);
      nFileArraySize = 0;
      nTotalFiles = 0;
      paszFileArray = NULL;
   }
}


void DisplayFileName(int nLine, void *pCallbackData)
{
   (void)pCallbackData;

   od_printf(GetFilename(nLine));
}


void DisplayFile(char *pszFilename)
{
   char szLine[LINE_SIZE];
   long lnOffset;

   /* Clear the screen. */
   od_clr_scr();

   /* Attempt to open the file. */
   pfCurrentFile = fopen(pszFilename, "r");
   if(pfCurrentFile == NULL)
   {
      od_printf("Unable to open file.\n\r\n\r");
      od_printf("Press [Enter] to continue.\n\r");
      od_get_answer("\n\r");
      return;
   }

   /* Get file offsets of each line and total line count from file. */
   for(;;)
   {
      lnOffset = fTell(pfCurrentFile);

      if(fgets(szLine, LINE_SIZE, pfCurrentFile) == NULL) break;

      AddOffsetToArray(lnOffset);
   }

   /* Use PagedViewer() to view the file. */
   PagedViewer(0, nTotalLines, DisplayFileLine, NULL, FALSE, NULL, 21);

   /* Deallocate array of line offsets. */
   FreeLineArray();

   /* Close the file. */
   fclose(pfCurrentFile);
}


void DisplayFileLine(int nLine, void *pCallbackData)
{
   char szLine[LINE_SIZE];
   long lnTargetOffset = palLineOffset[nLine];
   int nLineLen;

   (void)pCallbackData;

   /* Move to proper offset in file. */
   if(lnTargetOffset != ftell(pfCurrentFile))
   {
      fseek(pfCurrentFile, lnTargetOffset, SEEK_SET);
   }

   /* Get line from line. */
   if(fgets(szLine, LINE_SIZE, pfCurrentFile) != NULL)
   {
      /* Remote any trailing CR/LF sequence from line. */
      nLineLen = strlen(szLine);
      while(nLineLen > 0
         && (szLine[nLineLen - 1] == '\r' || szLine[nLineLen - 1] == '\n'))
      {
         szLine[--nLineLen] = '\0';
      }

      /* Display the line on the screen. */
      od_disp_str(szLine);
   }
}


int AddOffsetToArray(long lOffset)
{
   long *palNewArray;
   int nNewArraySize;

   /* If array is full, then grow it. */
   if(nTotalLines == nLineArraySize)
   {
      nNewArraySize = nLineArraySize + ARRAY_GROW_SIZE;

      if((palNewArray =
         realloc(palLineOffset, nNewArraySize * sizeof(long))) == NULL)
      {
         return(FALSE);
      }

      nLineArraySize = nNewArraySize;
      palLineOffset = palNewArray;
   }

   palLineOffset[nTotalLines++] = lOffset;

   return(TRUE);
}


void FreeLineArray(void)
{
   if(nLineArraySize > 0)
   {
      nTotalLines = 0;
      nLineArraySize = 0;
      free(palLineOffset);
      palLineOffset = NULL;
   }
}
  