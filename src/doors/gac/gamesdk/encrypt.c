/* This simple program opens the text file and encrypts it to a help
data file using reverse string and swab.  Not the best encryption scheme,
but it is very fast and fairly hard to recognize. */
void command_line(void);
void HelpDecrypt( char *line);
void HelpEncrypt( char *line);

#include <stdio.h>
//#include <io.h>
#include <string.h>
#include <stdlib.h>
#define FALSE 0
#define TRUE 1
#include "genwrap.h"
#include "filewrap.h"

void command_line(void)
{

    printf("Copyright 1996, G.A.C. Computer Services\n");
    printf("Give the filename without extension as parameter 1 or give\nboth FULL filenames as parameters 1 and 2.\nIf you add on a parameter 3 of -d, it will decrpyt\n");
    printf("e.g. encrypt shephelp  will read shephelp.txt and write shephelp.dat\n");
    printf("e.g. encrypt distasc.tmp distasc.art  will read distasc.tmp and write distasc.art\n");

    return;
}

// ***THE MAIN EXECUTABLE FOR THE LORD IGM
int main(int argc, char *argv[])
{
    char inFile[13], outFile[13], line[261], test[261];
    char *p, *curp;
    //, keyword[15];
    FILE *in, *out;

    //MODIFY THESE VARIABLES FOR EACH IGM

    // Check to see if any parameters where used
    if (argc == 2)
    {
    
        sprintf(inFile, "%s.txt", argv[1]);
        sprintf(outFile, "%s.dat", argv[1]);
    }
    else if (argc == 3)
    {
    
        sprintf(inFile, "%s", argv[1]);
        sprintf(outFile, "%s", argv[2]);
    }
    else if (argc == 4)
    {
        sprintf(inFile, "%s", argv[1]);
        sprintf(outFile, "%s", argv[2]);
    }
    else
    {
        command_line();
        return(1);
    }

    // open the file and search for the string #@file then display everything
    // until we get to #@ again

    if (access(inFile, 00) != 0)
    {
        printf("ERROR: %s does not exist!\n", inFile);
        return(1);
    }

    printf("Encrypting %s to %s\n\n", inFile, outFile);

    in = fopen(inFile, "rb");
    if (in == NULL) 
    {
        printf("ERROR: Opening %s\n", inFile);
        return(1);
    }
    out = fopen(outFile, "wb");
    if (out == NULL) 
    {
        printf("ERROR: Opening %s\n", outFile);
        return(1);
    }

    fseek(in, 0, SEEK_SET);
    fseek(out, 0, SEEK_SET);

    while (fscanf( in, "%[^\r\n]\r\n", line) == 1)
    {
        // make sure the string is an even number
        if (strlen(line)%2 != 0)
            strcat(line, " ");

        // look for a word from our prompt line and the starting @#
        if (strnicmp(line, "@#", 2) == 0)
        {
            p = &line[2];
            // encrypt the line
            if(argc<4)
            	HelpEncrypt(p);
            else
                HelpDecrypt(p);
            fprintf(out, "@#%s\r\n", p);
        }
        else
        {
            
            if(argc<4)
            	HelpEncrypt(p);
            else
                HelpDecrypt(p);
            fprintf(out, "%s\r\n", line);
        }

    }

    fclose(in);
    fclose(out);
    printf("All done!\n");
    return(0);
}

void HelpEncrypt( char *line)
{
    char *curp;
    
    swab(strrev(line), line, sizeof(line));

    curp = &line[0];
    while (curp[0] != '\0')
    {

        if (curp[0] >  0 )
            curp[0] -= 128;
        else
            curp[0] += 128;
        curp++;
    }

    return;
}

void HelpDecrypt( char *line)
{
	char *curp;

	curp = &line[0];
	while (curp[0] != '\0') {
		if (curp[0] > 0 )
			curp[0] -= 128;
		else
			curp[0] += 128; //should be 127
		curp++;
	}

	swab(line, line, sizeof(line));
	strcpy(line, strrev(line));

	return;
}

