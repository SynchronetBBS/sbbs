/* This simple program opens the text file and encrypts it to a help
data file using reverse string and swab.  Not the best encryption scheme,
but it is very fast and fairly hard to recognize. */
void command_line(void);
void HelpEncrypt( char *line);

#include <stdio.h>
//#include <io.h>
#include <string.h>
#include <stdlib.h>
#define FALSE 0
#define TRUE 1
#include "genwrap.h"

void command_line(void)
{

    printf("Copyright 1996, G.A.C. Computer Services\n");
    printf("Give the filename without extension as parameter 1 or give\nboth FULL filenames as parameters 1 and 2.\n");
    printf("e.g. ENCRYPT SHEPHELP  will read SHEPHELP.TXT and write SHEPHELP.DAT\n");
    printf("e.g. ENCRYPT DISTASC.TMP DISTASC.ART  will read DISTASC.TMP and write DISTASC.ART\n");

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
    
        sprintf(inFile, "%s.TXT", argv[1]);
        sprintf(outFile, "%s.DAT", argv[1]);
    }
    else if (argc == 3)
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

    in = fopen(inFile, "rt");
    if (in == NULL) 
    {
        printf("ERROR: Opening %s\n", inFile);
        return(1);
    }
    out = fopen(outFile, "wt");
    if (out == NULL) 
    {
        printf("ERROR: Opening %s\n", outFile);
        return(1);
    }

    fseek(in, 0, SEEK_SET);
    fseek(out, 0, SEEK_SET);

    while (fscanf( in, "%[^\n]\n", line) == 1)
    {
        // make sure the string is an even number
        if (strlen(line)%2 != 0)
            strcat(line, " ");

        // look for a word from our prompt line and the starting @#
        if (strnicmp(line, "@#", 2) == 0)
        {
            p = &line[2];
            // encrypt the line
            HelpEncrypt(p);            
            fprintf(out, "@#%s\n", p);
        }
        else
        {
            
            HelpEncrypt(line);            
            fprintf(out, "%s\n", line);
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

