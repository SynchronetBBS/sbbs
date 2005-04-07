/* Code for DisplayImbedded() function to display imbedded   */
/* .ASC/.ANS/.AVT/.RIP files.                                */

/* Include string.h for strlen() prototype. */
#include <string.h>

/* Include opendoors.h for OpenDoors definitions. */
#include "OpenDoor.h"

/* Include display.h for prototype of DisplayImbedded() function. */
#include "display.h"

/* Implementation of DisplayImbedded() function. */
void DisplayImbedded(
   char *pszAscii,
   char *pszAnsi,
   char *pszAvatar,
   char *pszRip)
{
   /* If RIP graphics is available and a RIP screen was specified ... */
   if(pszRip && od_control.user_rip)
   {
      /* Send RIP screen to remote system. */
      od_disp(pszRip, strlen(pszRip), FALSE);

      /* Proceed to send another screen to the local system only. */
      if(pszAvatar)
      {
         od_disp_emu(pszAvatar, FALSE);
      }
      else if(pszAnsi)
      {
         od_disp_emu(pszAnsi, FALSE);
      }
      else if(pszAscii)
      {
         od_disp_emu(pszAscii, FALSE);
      }
   }

   /* If RIP graphics is not available, display the best screen that has */
   /* been provided and that the user's system supports.                 */
   else if(pszAvatar && od_control.user_avatar)
   {
      /* Send screen to both local and remote systems. */
      od_disp_emu(pszAvatar, TRUE);
   }
   else if(pszAnsi && od_control.user_ansi)
   {
      /* Send screen to both local and remote systems. */
      od_disp_emu(pszAnsi, TRUE);
   }
   else if(pszAscii)
   {
      /* Send screen to both local and remote systems. */
      od_disp_emu(pszAscii, TRUE);
   }
}
