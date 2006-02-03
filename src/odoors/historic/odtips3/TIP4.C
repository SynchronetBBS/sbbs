#include "opendoor.h"
#include "cmdline.h"

int main(int argc, char *argv[])
{
   /* Set function to be called if no door information file is found. */
   od_control.od_no_file_func = NoDoorFileHandler;

   /* Call command-line processing function before first call to any */
   /* OpenDoors function! */
   ParseStandardCommandLine(argc, argv);

   /* Proceed with door normally. */
   od_printf("Hello World!\n\r\n\r");
   od_printf("Press [Enter] to return to BBS.\n\r");
   od_get_answer("\n\r");

   return(0);
}
