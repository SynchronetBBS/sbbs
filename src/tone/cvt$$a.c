/* CVT$$A*/

/* Converts OS/2 tune editor files to tone format */

#include <stdio.h>
#include <stdlib.h>

void main()
{
	char str[128]
		,*note="abcdefgabcdefgabcdefgabcdefg"
		,*flat="GABCDEFGABCDEFGABCDEFGABCDEFG";
	unsigned int  bt=200,o,n,t,i;

while(1) {
	fprintf(stderr,"top\n");
	if(!fgets(str,81,stdin))
		break;
	if(str[1]==':') {
		printf("%s",str+1);
		continue; }
	if(str[2]!=' ') {
		str[4]=0;
		bt=atoi(str);
		continue; }
	t=atoi(str);
	if(!t)
		continue;
	fprintf(stderr,"rest\n");
	if(t>=10) {
		if(t==10)
			t=bt*4;
		else if(t==11)
			t=bt*2;
		else if(t==12)
			t=bt;
		else if(t==13)
			t=bt/2;
		else if(t==14)
			t=bt/4;
		else if(t==14)
			t=bt/8;
		printf("r %u\n",t);
		continue; }
	fprintf(stderr,"time\n");
	t=(bt*4)/t;
	i=atoi(str+3);
	o=2;
	fprintf(stderr,"note\n");
	if(i<=20) {
		n=note[20-i];
		if(20-i)
			o+=((20-i)/7); }
	else if(i<=41) { /* flats */
		n=flat[41-i];
		if(41-i)
			o+=((41-i)/7); }
	else {
		n=toupper(note[62-i]);
		if(62-i)
			o+=((62-i)/7); }
	if(toupper(n)=='A' || toupper(n)=='B' || n=='G')
		o--;
	printf("%c%u %u\n",n,o,t); }
}

