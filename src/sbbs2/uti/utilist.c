/* UTILIST.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "uti.h"

int main(int argc, char **argv)
{
	char str[256];
	int i,j,file;

PREPSCREEN;

printf("Synchronet UTILIST v%s\n",VER);

if(argc<2)
	exit(1);

uti_init("UTILIST",argc,argv);

if((file=nopen(argv[1],O_CREAT|O_TRUNC|O_WRONLY))==-1)
    exit(2);

for(j=0;j<total_grps;j++)
	for(i=0;i<total_subs;i++) {
		if(sub[i]->grp!=j)
			continue;
		sprintf(str,"%s\r\n%s\r\n",sub[i]->code,sub[i]->code);
		write(file,str,strlen(str));
		sprintf(str,"%s\r\n",sub[i]->lname);
		write(file,str,strlen(str)); }
close(file);
printf("\nDone.\n");
bail(0);
return(0);
}
