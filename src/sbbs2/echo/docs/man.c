/* MAN.C */

/* Synchronet docs convert to paged format for printing */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>
#include <io.h>
#include <alloc.h>

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && (unsigned char)str[c-1]<=32) c--;
str[c]=0;
}

int main(int argc, char **argv)
{
	char str[256],str2[256],infile[128],outfile[128],idxfile[128]
		,*invbuf,*outvbuf,*idxvbuf,*tempvbuf,*p,*p2
		,chap[128]="SBBSecho"
		,pagetag[250][9]
		,idxtag[512][9];
	int i,j,page=1,line=0,pagetags=0,idxtags=0;
	long pageidx[250],idx[512],l;
	FILE *in, *out, *index, *tempfile;

if(argc<3) {
	printf("usage: man infile outfile indexfile\r\n");
	exit(1); }

strcpy(infile,argv[1]);
strcpy(outfile,argv[2]);
strcpy(idxfile,argv[3]);

if((in=fopen(infile,"rb"))==NULL) {
	printf("error opening %s\r\n",infile);
	exit(1); }

if((out=fopen(outfile,"wb"))==NULL) {
	printf("error opening %s\r\n",outfile);
    exit(1); }

//if((index=fopen(idxfile,"rb"))==NULL) {
//	  printf("error opening %s\r\n",idxfile);
//	  exit(1); }

//if((tempfile=fopen("TEMPFILE","w+b"))==NULL) {
//	  printf("error opening TEMPFILE\r\n");
//	  exit(1); }

if((invbuf=(char *)malloc(8*1024))==NULL) {
	printf("error allocating input buffer\n");
	exit(1); }

if((outvbuf=(char *)malloc(8*1024))==NULL) {
	printf("error allocating output buffer\n");
    exit(1); }

//if((idxvbuf=(char *)malloc(8*1024))==NULL) {
//	  printf("error allocating index buffer\n");
//	  exit(1); }

//if((tempvbuf=(char *)malloc(8*1024))==NULL) {
//	  printf("error allocating temp buffer\n");
//	  exit(1); }

setvbuf(in,invbuf,_IOFBF,8*1024);
setvbuf(out,outvbuf,_IOFBF,8*1024);
//setvbuf(index,idxvbuf,_IOFBF,8*1024);
//setvbuf(tempfile,tempvbuf,_IOFBF,8*1024);

//while(!ferror(index) && !feof(index)) {
//	  p=fgets(str,128,index);
//	  fputs(str,tempfile);
//	  while((p2=strstr(str,"@@"))!=NULL) {
//		  p2[0]=32;
//		  p2[1]=32;
//		  sprintf(idxtag[idxtags],"%-.8s",p2+2);
//		  idx[idxtags++]=ftell(index)-strlen(p2); } }

while(!ferror(in) && !feof(in)) {
	p=fgets(str,128,in);
	if(str[0]=='&' && str[1]=='&') {
		truncsp(str+2);
		strcpy(chap,str+2);
		continue; }
	if(str[0]=='$' && str[1]=='$') {
		truncsp(str+2);

//		  for(i=0;i<idxtags;i++) {
//			  if(!stricmp(str+2,idxtag[i])) {
//				  fseek(tempfile,idx[i],SEEK_SET);
//				  sprintf(str2,"%-10d",page);
//				  fprintf(tempfile,"%s",str2); } }

		for(i=0;i<pagetags;i++)
			if(!stricmp(str+2,pagetag[i]))
				break;
		if(i<pagetags) {
			l=ftell(out);
			fseek(out,pageidx[i]-1,SEEK_SET);
			sprintf(str,"%10d",page);
			for(i=0;i<10;i++)
				if(str[i]==0x20)
					str[i]='.';
			fprintf(out,"%s",str);
			fseek(out,l,SEEK_SET); }
		continue; }

	line++;
	if(!p)
		break;
	if(!p || str[0]==0xc || line==55) {
		while(line<55) {
			fprintf(out,"\r\n");
			line++; }
		line=1;
		if(page!=1) 	/* no tag on page 1 */
			fprintf(out,"\r\n___________________________________________"
				"____________________________________\r\n"
				"%-35s %3u %39s"
				,"SBBSecho",page,chap);
		fprintf(out,"\r\n\xc\r\n\r\n");
		page++; }

	if((p2=strstr(str,"@@"))!=NULL) {
        sprintf(pagetag[pagetags],"%-.8s",p2+2);
        pageidx[pagetags++]=ftell(out)+(p2-str)+1; }

	if(!p)
		break;
	if(str[0]!=0xc)
		fprintf(out,"%s",str);
	else
		line=0;
		}

//fseek(tempfile,0L,SEEK_SET);
//while(!ferror(tempfile) && !feof(tempfile)) {
//	  j=0;
//	  p=fgets(str2,128,tempfile);
//	  for(i=0;i<strlen(str2);i++) {
//		  str[j++]=str2[i];
//		  if(str2[i]==',') {
//			  i++;
//			  str[j++]=str2[i++];
//			  while(1) {
//				  if(str2[i]>32 && isdigit(str2[i]))
//					  str[j++]=str2[i++];
//				  else
//					  break; }
//			  while(str2[i++]==32);
//			  i-=2; } }
//	  str[j]=0;

//	  line++;
//	  if(!p || str[0]==0xc || line==55) {
//		  while(line<55) {
//			  fprintf(out,"\r\n");
//			  line++; }
//		  line=1;
//		  if(page!=1)	  /* no tag on page 1 */
//			  fprintf(out,"\r\n\t___________________________________________"
//				  "____________________________________\r\n"
//				  "\t%-35s %3u %39s"
//				  ,"Synchronet",page,"Index");
//		  fprintf(out,"\r\n\xc\r\n\r\n");
//		  page++; }

//	  if(!p)
//		  break;
//	  if(str[0]!=0xc)
//		  fprintf(out,"\t%s",str);
//	else
//		  line=0;
//		  }
return(0);
}


