/* MAKEHELP.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Makes SCFG.HLP for Synchronet configuration program */

#include <stdio.h>
#include <alloc.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

void main(void)
{
	char *files[]={ "SCFG.C"
				,"SCFGSYS.C"
				,"SCFGMSG.C"
				,"SCFGSUB.C"
				,"SCFGNODE.C"
				,"SCFGCHAT.C"
				,"SCFGXFR1.C"
				,"SCFGXFR2.C"
				,"SCFGNET.C"
				,"SCFGXTRN.C"
				,NULL };
	char str[256],tmp[256];
	int i,j,k,line,ixb;
	long l;
	FILE *stream,*out;

if((out=fopen("SCFGHELP.DAT","wb"))==NULL) {
	printf("error opening SCFGHELP.DAT\r\n");
	return; }

if((ixb=open("SCFGHELP.IXB",O_WRONLY|O_CREAT|O_BINARY,S_IWRITE|S_IREAD))==-1) {
	printf("error opening SCFGHELP.IXB\r\n");
	return; }

for(i=0;files[i];i++) {
	if((stream=fopen(files[i],"rb"))==NULL) {
		printf("error opening %s\r\n",files[i]);
		return; }
	printf("\r\n%s ",files[i]);
	line=0;
	while(!feof(stream)) {
		if(!fgets(str,128,stream))
			break;
		line++;
		if(strstr(str,"SETHELP(WHERE);")) {
			l=ftell(out);
			write(ixb,files[i],12);
			write(ixb,&line,2);
			write(ixb,&l,4);
			fgets(str,128,stream);	 /* skip start comment */
			line++;
			while(!feof(stream)) {
				if(!fgets(str,128,stream))
					break;
				if(strchr(str,9)) { /* Tab expansion */
					strcpy(tmp,str);
					k=strlen(tmp);
					for(j=l=0;j<k;j++) {
						if(tmp[j]==9) {
							str[l++]=32;
							while(l%4)
								str[l++]=32; }
						else
							str[l++]=tmp[j]; }
					str[l]=0; }
				line++;
				if(strstr(str,"*/"))    /* end comment */
					break;
				fputs(str,out); }
			fputc(0,out); } }
	fclose(stream); }
fclose(out);
printf("\n");
}
