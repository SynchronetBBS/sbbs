/* mime.c */

/* Synchronet MIME functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* See RFCs 1521,2045,2046,2047,2048,2049,1590 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "sbbs.h"
#include "mailsrvr.h"
#include "base64.h"

#define SIZEOF_MIMEBOUNDARY     36

char * mimegetboundary()
{
    int i, num;
    char * boundaryString = (char *)malloc(SIZEOF_MIMEBOUNDARY + 1);

    srand(time(NULL));
    if (boundaryString == NULL) 
        return NULL;
    for (i=0;i<SIZEOF_MIMEBOUNDARY;i++) {
        num=(rand()%62);    
        if(num<10)
            num+=48;  
        else if(num>=10 && num<36)
            num+=55;
        else
            num+=61;
        boundaryString[i]=(char)num;
    }
    boundaryString[i]='\0';
    return boundaryString;
}

void mimeheaders(SOCKET socket, char * boundary)
{
    char bndline[50];
    sockprintf(socket,"MIME-Version: 1.0");
    sockprintf(socket,"Content-Type: multipart/mixed;");
    strcpy(bndline," boundary=\"");
    strcat(bndline,boundary);
    strcat(bndline,"\"");
    sockprintf(socket,bndline);
}

void mimeblurb(SOCKET socket, char * boundary)
{
    sockprintf(socket,"This is a MIME blurb. You shouldn't be seeing this!");
    sockprintf(socket,"");
}

void mimetextpartheader(SOCKET socket, char * boundary)
{
    char bndline[50];

    strcpy(bndline,"--");
    strcat(bndline,boundary);
    sockprintf(socket,bndline);
    sockprintf(socket,"Content-Type: text/plain;");
    sockprintf(socket," charset=\"iso-8859-1\"");
    sockprintf(socket,"Content-Transfer-Encoding: 7bit");
}

BOOL base64out(SOCKET socket, char * pathfile)
{
    FILE *  fp;
    char    in[57];
    char    out[77];
    int     bytesread;

    if((fp=fopen(pathfile,"rb"))==NULL) 
        return(FALSE);
    while(1) {
        bytesread=fread(in,1,57,fp);
		if((b64_encode(out,in,sizeof(out),bytesread)==NULL)
				|| !sockprintf(socket,out))  {
			fclose(fp);
			return(FALSE);
		}
        if(bytesread!=57 || feof(fp))
            break;
    }
	fclose(fp);
    sockprintf(socket,"");
	return(TRUE);
}

BOOL mimeattach(SOCKET socket, char * boundary, char * pathfile)
{
    char bndline[41];
    char * fname = getfname(pathfile);
    char * line = (char*)MALLOC(strlen(fname) + 20);

	if(line==NULL)
		return(FALSE);

    strcpy(bndline,"--");
    strcat(bndline,boundary);
    sockprintf(socket,bndline);
    sockprintf(socket,"Content-Type: application/octet-stream;");
    strcpy(line," name=\"");
    strcat(line,fname);
    strcat(line,"\"");
    sockprintf(socket,line);
    sockprintf(socket,"Content-Transfer-Encoding: base64");
    sockprintf(socket,"Content-Disposition: attachment;");
    strcpy(line," filename=\"");
    strcat(line,fname);
    strcat(line,"\"");
    sockprintf(socket,line);
    sockprintf(socket,"");
    if(!base64out(socket,pathfile))
		return(FALSE);
    sockprintf(socket,"");
    FREE(line);
	return(TRUE);
}

void endmime(SOCKET socket, char * boundary)
{
    char bndline[128];

    strcpy(bndline,"--");
    strcat(bndline,boundary);
    strcat(bndline,"--"); /* last boundary */
    sockprintf(socket,bndline);
    sockprintf(socket,"");
}
