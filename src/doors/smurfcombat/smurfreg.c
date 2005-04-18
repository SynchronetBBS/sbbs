/***************************************************************************/
/* */
/* sss                       fff     ccc                 b               */
/* s   s                     f   f   c   c                b               */
/* s                         f       c                    b               */
/* sss  mmm mmm  u   u r rr  fff    c      ooo  mmm mmm   bbb  aaa  ttt  */
/* s m  m   m u   u rr   f       c     o   o m  m   m b   b a  a  t   */
/* s   s m  m   m u  uu r    f       c   c o   o m  m   m b   b aaaa  t   */
/* sss  m  m   m  uu u r    f        ccc   ooo  m  m   m  bbb  a  a  t   */
/* */
/* SYSOP Maintenance Code: !-S-Y-S-O-P-!                         REGISTA   */
/* REMOTE Maintenance Code: !-SIX-NINE-|                         MODULE    */
/* */
/***************************************************************************/
/* SMURF.nnn (Personal) SMURF.Dnn (Documentation)                          */
/* !  Standard Operation Codes                                             */
/* !# Extended Testing Codes, Value Increment                              */
/* !@ Extended Testing Codes, Smurf Increment                              */
/* // Specialized To-Local Codes                                           */
/***************************************************************************/

#include<ciolib.h>
#include<stdlib.h>
#include"smurfreg.h"

void 
__REG__main(void)
{
    char            code[40], name[80], cod;
    FILE           *stream;
    printf("\n\r");

#if TODO_REGISTRATION
    printf("Enter Registration Code: ");
    gets(code);
#endif
    printf("Enter Registration Name: ");
    gets(name);
    /* printf("Enter Intermediate Code: "); cin >> name[0];cod=atoi(name[0]);
     * if(cod>0)for(cyc=0;cyc<cod;cyc++){ printf("Name %d:
     * ",cyc);scanf("%39s", name[cyc]);} fprintf(stream,"%s",name[0]);
     * if(cod>1)for(cyc=1;cyc<cod;cyc++)    fprintf(stream," %s",name[cyc]); */
    stream = fopen("smurfreg.nfo", "w+");
    fprintf(stream, "%s", name);
#if TODO_REGISTRATION
    fprintf(stream, "\n\r%s\n\r", code);
#endif
    fclose(stream);
}

void 
notdone(void)
{
    mustregister();
    if (statis) {
	od_set_colour(L_BLUE, D_BLACK);
	od_printf("\n\rSorry, but this function is not complete as of.\n\n\r");
    }
}

void 
mustregister(void)
{
    if (!statis) {
	od_clr_scr();
	od_printf("Sorry to interrupt your fun like this but your Sysop didn't\n\r");
	od_printf("register this version of SMURF COMBAT yet!               \n\n\r");
	od_printf("In order for this function to be orperative, your Sysop has\n\r");
	od_printf("to pay the one time, VERY low registration cost of NOTHING!!!\n\n\r");
	od_printf("Consider the advantages,  plain out no more waits,  and ALL\n\r");
	od_printf("the functions work 100 percent!\n\n\r");
	/* sleep(5); */
	smurf_pause();
    }
}

void 
registerme(void)
{
    if (!statis) {
	od_clr_scr();
	nl();
	nl();
	od_set_colour(15, 0);
	od_printf("Sorry to interrupt your fun like this but your Sysop didn't\n\r");
	od_printf("register this version of SMURF COMBAT yet! OHMYGOD! So, I'd\n\r");
	od_printf("like to take .21 minutes to remind you that it took 10,000+\n\r");
	od_printf("minutes of day-in-day-out work to complete this game.\n\n\r");
	od_set_colour(11, 0);
	od_set_colour(10, 0);
	od_printf("\n\n\rThanks for your time, I hope you appreciate mine.\n\n\r");
	/* sleep(2); */
    }
}

void 
checkkey(void)
{				       /* Main program function */
    FILE           *fp;		       /* File pointer for REGISTER.KEY file */
    unsigned long   supplied_key;      /* Key supplied by user */
    unsigned long   correct_key;       /* Correct registration key */
    if ((fp = fopen("smurfreg.nfo", "r")) != NULL) {	/* Try to open file *//* I
							 * f successful */
	fgets(registered_name, 200, fp);	/* read name from file */
	if (registered_name[strlen(registered_name) - 1] == '\n')
	    registered_name[strlen(registered_name) - 1] = '\0';
	if (registered_name[strlen(registered_name) - 1] == '\r')
	    registered_name[strlen(registered_name) - 1] = '\0';
	if (registered_name[strlen(registered_name) - 1] == '\n')
	    registered_name[strlen(registered_name) - 1] = '\0';

#if TODO_REGISTRATION
	fscanf(fp, "%lu", &supplied_key);	/* read key from file */

	fclose(fp);		       /* Close file */

	bp(registered_name, 909);      /* Calculate correct key */
	correct_key = rcount;

	if (correct_key == supplied_key) {	/* Compare correct & supplied
	     * keys *//* If they are identical */
	    registeredxx = 1;	       /* Then switch program into registered
				        * mode */
	}
#else
	registeredxx = 1;
#endif
    }
    if (registeredxx == 1)
	statis = 1;		       /* If registered mode */
#if 0
     /* {                                  /* Display registration information */ */
    /* printf("This program is registered to: %s\n",registered_name); */
    /* } */
    /* else if(registeredxx==0)                       /* If not in registered
	        mode */ */
	     /* {                                  /* Display unregistered information */ */
    /* printf("This program is UNREGISTERED!!!\n"); */
    /* }smurf_pause(); */
#endif
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
