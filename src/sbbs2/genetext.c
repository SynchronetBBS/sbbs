/* GENETEXT.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Creates the file ETEXT.C which has the encrypted text strings for use in */
/* Synchronet */

#include <stdio.h>

void cvtusc(char *str)
{
    int i;

for(i=0;str[i];i++)
	if(str[i]=='_')
		str[i]=' ';
}

void main()
{
	unsigned char str[129],bits,len;
	unsigned int i,j;
    unsigned long l,m;
	FILE *in,*cout,*hout;

if((in=fopen("ETEXT.DAT","rb"))==NULL) {
	printf("can't open ETEXT.DAT\n");
	return; }

if((cout=fopen("ETEXT.C","wb"))==NULL) {
	printf("can't create ETEXT.C\n");
	return; }

if((hout=fopen("ETEXT.H","wb"))==NULL) {
	printf("can't create ETEXT.H\n");
    return; }

fprintf(cout,"/* ETEXT.C */\r\n\r\n#include \"etext.h\"\r\n\r\n");
fprintf(hout,"/* ETEXT.H */\r\n\r\n");
while(!feof(in)) {
	if(!fgets(str,127,in))
		break;
	str[strlen(str)-3]=0;	/* chop off :crlf */
	if(!str[0])
		break;
	fprintf(hout,"extern unsigned long %s[];\r\n",str);
	fprintf(cout,"unsigned long %s[]={ ",str);
	if(!fgets(str,127,in))
		break;
	str[strlen(str)-2]=0;	/* chop off crlf */
	if(!str[0])
        break;
	cvtusc(str);
	len=strlen(str);
	l=len^0x49;
	bits=7;
	for(i=0,j=1;i<len;i++) {
		m=(unsigned long)(str[i]^(i^0x2c));
		l|=(m<<bits);
		bits+=7;
		if(bits>=32) {
			fprintf(cout,"%luUL,",l);
			j++;
			l=0UL;
			bits-=32;
			if(bits)
				l=(unsigned long)(str[i]^(i^0x2c))>>(7-bits); } }
	fprintf(cout,"%luUL };\r\n",l); }
}
