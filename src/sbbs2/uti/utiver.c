/* UTIVER.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "uti.h"

int main(int argc, char **argv)
{
	char str[1024];
	int file;

sprintf(str,"2\r\nSynchronet UTI Driver v%s - "
			"Developed 1995-1997 Rob Swindell\r\n",VER);
if(argc<2)
	exit(1);
if((file=open(argv[1],O_WRONLY|O_CREAT|O_TRUNC,S_IWRITE))==-1)
	exit(2);
write(file,str,strlen(str));
close(file);
return(0);
}
