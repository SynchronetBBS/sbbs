#include <stdio.h>
#include <ciolib.h>
#ifdef TODO_HEADERS
#include <dos.h>
#endif

int             c_break(void){
    return (1);
}

void 
main(void)
{
    int             x = 0;
    char            stringz[2];
    directvideo = 0;
#ifdef TODO_DOSWRAP
    ctrlbrk(c_break);
#endif
    textcolor(15);
    cprintf("\n\rSmurf crack courtesy of [JuSTice]\n\r");
    textcolor(7);
    cprintf("Enter name to register under: ");

    do {
	gets(stringz);
    } while (x == 0);
}
