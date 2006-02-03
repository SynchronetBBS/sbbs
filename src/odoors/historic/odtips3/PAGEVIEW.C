/* pageview.c - Implementation of the PagedViewer() system. */

#include <string.h>

#include "opendoor.h"
#include "pageview.h"

char bTitleColor = 0x0c;
char bTitleLineColor = 0x04;
char bNumberColor = 0x0a;
char bTextColor = 0x02;
char bPromptColor = 0x0f;

int PagedViewer(
   int nInitialLine,                      /* Zero-based initial line number. */
   int nTotalLines,                       /* Total line count. */
   void (*pDisplayCallback)(int nLine, void *pData),
   void *pCallbackData,                   /* Data to pass to callback func. */
   BOOL bAllowSelection,                  /* TRUE if selection is permitted. */
   char *pszTitle,                        /* Title string, or NULL. */
   int nPageSize)                         /* # of lines to display per page. */
{
   int nCurrentPage = 0;
   int nScreenLine;
   int nAbsoluteLine;
   char chPressed;
   char bCanPageDown;
   char bCanPageUp;

   /* Determine current page from initial line number, if specified. */
   if(nInitialLine != NO_LINE)
   {
      nCurrentPage = nInitialLine / nPageSize;
   }

   /* Loop until user makes a selection, or chooses to quit. */
   for(;;)
   {
      /* Display the current page. */

      /* Clear the screen */
      od_printf("\n\r");
      od_clr_scr();

      /* If a title has been specified, then display it. */
      if(pszTitle != NULL)
      {
         od_set_attrib(bTitleColor);
         od_repeat(' ', (80 - strlen(pszTitle)) / 2);
         od_disp_str(pszTitle);
         od_printf("\n\r");
         od_set_attrib(bTitleLineColor);
         if(od_control.user_ansi || od_control.user_avatar)
         {
            od_repeat(196, 79);
         }
         else
         {
            od_repeat('-', 79);
         }
         od_printf("\n\r");
      }

      /* Display the lines on this page. */
      nAbsoluteLine = nCurrentPage * nPageSize;
      nScreenLine = 0;
      while(nScreenLine < nPageSize && nAbsoluteLine < nTotalLines)
      {
         /* If selection is permitted, display an identifier for each line. */
         if(bAllowSelection)
         {
            od_set_attrib(bNumberColor);
            if(nScreenLine < 9)
            {
               od_printf("%d. ", nScreenLine + 1);
            }
            else
            {
               od_printf("%c. ", 'A' + (nScreenLine - 9));
            }
         }

         /* Display the line itself. */
         od_set_attrib(bTextColor);
         (*pDisplayCallback)(nAbsoluteLine, pCallbackData);
         od_printf("\n\r");

         /* Move to next line. */
         nScreenLine++;
         nAbsoluteLine++;
      }

      /* Determine whether user can page up or down from this page. */
      bCanPageDown = nCurrentPage < (nTotalLines - 1) / nPageSize;
      bCanPageUp = nCurrentPage > 0;

      /* Display prompt at bottom of screen. */
      od_set_attrib(bPromptColor);
      od_printf("\n\r[Page %d of %d]  ", nCurrentPage + 1,
         ((nTotalLines - 1) / nPageSize) + 1);
      if(bAllowSelection)
      {
         od_printf("Choose an option or");
      }
      else
      {
         od_printf("Available options:");
      }
      if(bCanPageDown)
      {
         od_printf(" [N]ext page,");
      }
      if(bCanPageUp)
      {
         od_printf(" [P]revious page,");
      }
      od_printf(" [Q]uit.");

      /* Loop until the user makes a valid choice. */
      for(;;)
      {
         /* Get key from user */
         chPressed = toupper(od_get_key(TRUE));

         if(chPressed == 'Q')
         {
            /* If user chooses to quit, then return without a selection. */
            od_printf("\n\r");
            return(NO_LINE);
         }

         else if(chPressed == 'P' && bCanPageUp)
         {
            /* Move to previous page and redraw screen. */
            --nCurrentPage;
            break;
         }

         else if(chPressed == 'N' && bCanPageDown)
         {
            /* Move to next page and redraw screen. */
            ++nCurrentPage;
            break;
         }

         else if(bAllowSelection
                 && (
                         (chPressed >= '1' && chPressed <= '9')
                      || (chPressed >= 'A' && chPressed <= 'M')
                    )
                )
         {
            /* If user pressed a possible line key, and selection is */
            /* enabled, try translating character to a line number.  */
            if(chPressed >= '1' && chPressed <= '9')
            {
               nScreenLine = chPressed - '1';
            }
            else
            {
               nScreenLine = 9 + (chPressed - 'A');
            }

            /* Calculate absolute line number. */
            nAbsoluteLine = nScreenLine + (nCurrentPage * nPageSize);

            /* If selected line is within range, then return selected line */
            /* number.                                                     */
            if(nScreenLine < nPageSize && nAbsoluteLine < nTotalLines)
            {
               od_printf("\n\r");
               return(nAbsoluteLine);
            }
         }
      }
   }
}
