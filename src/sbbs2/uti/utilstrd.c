/* UTILSTRD.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "uti.h"

#define READ	1
#define WRITE	2

int main(int argc, char **argv)
{
	char	*p,str[256],name[256],mode=0;
	int 	i,file;
	uint	usernumber;
	time_t	ptr;
	FILE	*stream;

PREPSCREEN;

printf("Synchronet UTILSTRD v%s\n",VER);

if(argc<4)
	exit(1);

if(!stricmp(argv[1],"READ"))
    mode=READ;
else if(!stricmp(argv[1],"WRITE"))
    mode=WRITE;
if(!mode)
    exit(1);

uti_init("UTILSTRD",argc,argv);

if(mode==READ) {
	if((file=nopen(argv[2],O_CREAT|O_TRUNC|O_WRONLY))==-1)
		exit(2); }
else {
	if((file=nopen(argv[2],O_RDONLY))==-1)
		exit(2); }

if((stream=fdopen(file,"wb"))==NULL)
	exit(2);


strcpy(name,argv[3]);		/* build the user name */
for(i=4;i<argc;i++) {
	strcat(name," ");
	strcat(name,argv[i]); }

sprintf(str,"%sUSER\\NAME.DAT",data_dir);
if((file=nopen(str,O_RDONLY))==-1)
	exit(8);

usernumber=1;
while(!eof(file)) {
	read(file,str,LEN_ALIAS+2);
	str[25]=0;
	p=strchr(str,3);
	if(p) *p=0;
	if(!stricmp(str,name))
		break;
	usernumber++; }
if(stricmp(str,name)) {
	printf("Username '%s' not found.\n",name);
	exit(9); }
close(file);

sprintf(str,"%sUSER\\PTRS\\%4.4u.IXB",data_dir,usernumber);
if((file=nopen(str,mode==READ ? O_RDONLY : O_WRONLY|O_CREAT))==-1)
	exit(10);
for(i=0;i<total_subs;i++) {
	lseek(file,((long)sub[i]->ptridx)*10L,SEEK_SET);
	if(mode==READ) {
		read(file,&ptr,4);
		fprintf(stream,"%lu\r\n",ptr); }
	else {
		fgets(str,81,stream);
		ptr=atol(str);
		write(file,&ptr,4); } }
close(file);
printf("\nDone.\n");
bail(0);
return(0);
}

