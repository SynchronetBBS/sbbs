#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<ciolib.h>
#include"smurfver.h"

void            bp(char *registration_string, unsigned int security_code);
unsigned long   rcount = 0;
main()
{
    char            registered;
    char            registered_name[201];
    char            temp_string[201];
    char            registration_string[201];
    char            registration_verification[201];
    unsigned int    security_code;
    char            tempstring[201];
    unsigned int    security_verification;
    clrscr();
    textcolor(12);
    cprintf("\n   S M U R F   C O M B A T\n\n\r");
    textcolor(10);
    cprintf("  Registration No. Module\n\r");
    cprintf("        %sVersion %s\n\r", __vkeysp, __vkey);
    cprintf("     By Laurence Manhken\n\n\n\r");
    textcolor(9);
    cprintf(" Smurf Combat est 909 ou 12345\n\n\r");

    textcolor(11);
    cprintf(" Programme Code : ");
    cgets(temp_string);
    security_code = atoi(temp_string);
    cprintf("        Repetez : ");
    cgets(temp_string);
    security_verification = atoi(temp_string);
    if (security_code != security_verification) {
	printf("\nCodes ne match pas!\n");
	return (1);
    }
    textcolor(11);
    cprintf(" Individule Nom : ");
    cgets(registration_string);
    cprintf("        Repetez : ");
    cgets(registration_verification);
    if (strcmp(registration_string, registration_verification) != 0) {
	printf("\nStrings ne match pas!\n");
	return (1);
    }
    bp(registration_string, security_code);
    textcolor(9);
    cprintf("\n Individule Nom : [%s]\n\r", registration_string);
    cprintf("       Serialle : [%lu]\n\n\r", rcount);

}



void 
bp(char *registration_string, unsigned int security_code)
{
    char            tempstring[201];
    int             cyc;
#ifdef TODO_WRAPPERS
    strset(tempstring, 0);
#else
    memset(tempstring, 0, sizeof(tempstring));
#endif
    sprintf(tempstring, "%s", registration_string);
    for (cyc = 0; cyc < strlen(tempstring); cyc++) {
	rcount = rcount + tempstring[cyc] * security_code;
    }
}
