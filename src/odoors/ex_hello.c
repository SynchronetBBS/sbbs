/* EX_HELLO.C - Example of a trivial OpenDoors program. Demonstrates         */
/*              just how simple a fully functional door program can be. Also */
/*              shows all the basic elements required by any program using   */
/*              OpenDoors. See manual for instructions on how to compile     */
/*              this program.                                                */
/*                                                                           */
/*              This program shows how to do the following:                  */
/*                                                                           */
/*                 - #include the OpenDoors header file, opendoor.h.         */
/*                 - Create a mainline function that can be compiled under   */
/*                   both DOS and Windows versions of OpenDoors.             */
/*                 - How to display text on multiple lines.                  */
/*                 - How to wait for a single key to be pressed.             */
/*                 - How to properly exit a program that uses OpenDoors.     */


/* The opendoor.h file must be included by any program using OpenDoors. */
#include "OpenDoor.h"


/* The main() or WinMain() function: program execution begins here. */
#ifdef ODPLAT_WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPSTR lpszCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{

   /* Display a message. */
   od_printf("Hello world! This is a very simple OpenDoors program.\n\r");
   od_printf("Press any key to return to the BBS!\n\r");


   /* Wait for user to press a key. */
   od_get_key(TRUE);


   /* Exit door program, returning to the BBS. */
   od_exit(0, FALSE);
   return(0);
}
