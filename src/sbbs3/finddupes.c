/* finddupes.c */

/* Search for (and optionally delete) duplicate files in multiple	*/
/* directories based on size and either MD5 or CRC-32 "chksums".	*/

#include <stdio.h>
#include <time.h>

#include "dirwrap.h"
#ifdef USE_MD5
	#include "md5.h"
#else	/* CRC-32 */
	#include "crc32.h"
#endif

typedef struct {
	char	path[MAX_PATH+1];
	long	length;
	time_t	date;
#ifdef USE_MD5
	BYTE	chksum[MD5_DIGEST_SIZE];
#else
	ulong	chksum;
#endif
} file_t;

file_t* file;
ulong	file_count=0;

int fchksum(const char* fname, long length,
#ifdef USE_MD5
			BYTE*
#else
			ulong*
#endif
			chksum)
{
	BYTE* buf=NULL;
	FILE* fp;

	if((fp=fopen(fname,"rb"))==NULL) {
		perror(fname);
		return(-1);
	}
	
	if(length && (buf=malloc(length))==NULL) {
		printf("!Error allocating %ld bytes of memory for %s\n"
			,length,fname);
		fclose(fp);
		return(-1);
	}

	if(fread(buf,sizeof(BYTE),length,fp) != length) {
		perror(fname);
		fclose(fp);
		FREE_AND_NULL(buf);
		return(-1);
	}

	fclose(fp);
#ifdef USE_MD5
	MD5_calc(chksum, buf, length);
#else
	*chksum = crc32(buf, length);
#endif
	FREE_AND_NULL(buf);
	return(0);
}

char* timestr(void)
{
	char* p;
	time_t t=time(NULL);
	p=ctime(&t);
	p[19]=0;		/* chop off year and \n */
	return(p+4);	/* skip day-of-week */
}
	
int searchdir(const char* path, BOOL recursive, ulong compare_bytes)
{
	DIR* dir;
	struct dirent* ent;
	file_t* fp;
	char fpath[MAX_PATH+1];

	printf("%s begin searching %s\n",timestr(), path);
	if((dir = opendir(path))==NULL) {
		perror(path);
		return(1);
	}

	while((ent = readdir(dir))!=NULL) {
		if(kbhit())
			break;
		if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0)
			continue;
		strcpy(fpath,path);
		backslash(fpath);
		strcat(fpath,ent->d_name);
		if(isdir(fpath)) {
			if(recursive)
				searchdir(fpath, recursive, compare_bytes);
			continue;
		}

		file=realloc(file,sizeof(file_t)*(file_count+1));
		if(file==NULL) {
			printf("!Error allocating %lu bytes\n",sizeof(file_t)*(file_count+1));
			exit(1);
		}
		fp=&file[file_count];
		memset(fp,0,sizeof(file_t));
		strcpy(fp->path,fpath);
		fp->date=fdate(fp->path);
		if((fp->length=flength(fp->path))==-1) {
			printf("!Failed to get length of %s\n",fp->path);
			continue;
		}
		if(compare_bytes && fp->length > compare_bytes)
			fp->length = compare_bytes;
		if(fchksum(fp->path, fp->length,
#ifdef USE_MD5
			fp->chksum
#else
			&fp->chksum
#endif
			))
			continue;
		file_count++;
		printf("%lu\r", file_count);
	}

	closedir(dir);
	printf("%s done searching %s\n",timestr(), path);
	return(0);
}

int compare_files(const file_t *f1, const file_t *f2 )
{
	int result;
	
	/* Sort first by size (descending) */
	if((result = f2->length - f1->length) != 0)
		return(result);

	/* Then by chksum (ascending) */
	if((result = memcmp(&f1->chksum, &f2->chksum, sizeof(f1->chksum))) != 0)
		return(result);

	/* Then by date (descending) */
	return(f2->date - f1->date);
}

int main(int argc, char** argv)
{
	char hex[32];
	int i;
	ulong fsize;
	ulong dupe_count=0;
	ulong del_files=0;
	ulong del_bytes=0;
	ulong compare_bytes=0;
	BOOL recursive=FALSE;
	BOOL del_dupes=FALSE;
	BOOL dir_specified=FALSE;

	for(i=1;i<argc;i++) {
		if(!stricmp(argv[i],"-d"))
			del_dupes=TRUE;
		else if(!stricmp(argv[i],"-r"))
			recursive=TRUE;
		else if(!stricmp(argv[i],"-b") && i<argc+1)
			compare_bytes=atoi(argv[++i]);
		else if(!stricmp(argv[i],"-k") && i<argc+1)
			compare_bytes=atoi(argv[++i])*1024;
		else if(argv[i][0]=='-') {
			printf("%s [[-opt] [-opt] [...]] [[path] [path] [...]]\n", argv[0]);
			printf("-r\t search directories recursively\n");
			printf("-d\t delete duplicate files found\n");
			printf("-b n\t compare up to n bytes of each file\n");
			printf("-k n\t compare up to n kilobytes each of file\n");
			exit(0);
		}
		else {
			dir_specified=TRUE;
			searchdir(argv[i], recursive, compare_bytes);
		}
	}
	if(!dir_specified)
		searchdir(".", recursive, compare_bytes);

	if(!file_count) {
		printf("no files.\n");
		return(0);
	}

	printf("%s begin sorting (%lu files)\n", timestr(), file_count);
	qsort(file,file_count,sizeof(file_t),compare_files);
	printf("%s end sorting\n", timestr());

	printf("%s comparing (%lu files)\n", timestr(), file_count);

	for(i=0;i<file_count-1;i++) {
		if(file[i].length != file[i+1].length)
			continue; /* sizes must match */
		if(memcmp(&file[i].chksum, &file[i+1].chksum, sizeof(file[i].chksum)))
			continue; /* chksums must match */
#ifdef USE_MD5
		MD5_hex(hex, file[i].chksum);
#else
		sprintf(hex, "%08lx", file[i].chksum);
#endif
		printf("Dupe: %s %7lu %s\n", hex, file[i].length, getfname(file[i].path));
		if(del_dupes) {
			fsize=flength(file[i].path);
			printf("Removing %s (%lu bytes)\n", file[i].path, fsize);
			if(remove(file[i].path)!=0)
				perror(file[i].path);
			else {
				del_files++;
				del_bytes+=fsize;
			}
		}
		dupe_count++;
	}

	printf("%s done (%lu duplicates found)\n", timestr(), dupe_count);
	if(del_files)
		printf("%lu bytes deleted in %lu files\n", del_bytes, del_files);
	return(0);
}
