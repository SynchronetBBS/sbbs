/* wraptest.c */

#include <time.h>	/* ctime */

#include "genwrap.h"
#include "conwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "sockwrap.h"
#include "threadwrap.h"

static void getkey(void)
{
	printf("Hit any key...");
	fflush(stdout);
	while(getch()==0);
	printf("\r%20s\r","");
	fflush(stdout);
}

int main()
{
	char	compiler[128];
	char*	glob_pattern = "*wrap*";
	int		i;
	int		ch;
	uint	u;
	time_t	t;
	glob_t	g;
	DIR*	dir;
	DIRENT*	dirent;

	/* Show platform details */
	DESCRIBE_COMPILER(compiler);
	printf("Platform: %s\n",PLATFORM_DESC);
	printf("Compiler: %s\n",compiler);

	printf("\ngetch() test (ESC to continue)\n");
	do {
		ch=getch();
		printf("getch() returned %d\n",ch);
	} while(ch!=ESC);

#if 1
	/* BEEP test */
	printf("\nBEEP() test\n");
	getkey();
	for(i=750;i>250;i-=5)
		BEEP(i,15);
	for(;i<1000;i+=5)
		BEEP(i,15);
#endif
	/* SLEEP test */
	printf("\nSLEEP() test\n");
	getkey();
	t=time(NULL);
	SLEEP(5000);
	printf("slept %d seconds\n",time(NULL)-t);

	/* glob test */
	printf("\nglob() test\n");
	getkey();
	i=glob(glob_pattern,GLOB_MARK,NULL,&g);
	if(i==0) {
		for(u=0;u<g.gl_pathc;u++)
			printf("%s\n",g.gl_pathv[u]);
		globfree(&g);
	} else
		printf("glob(%s) returned %d\n",glob_pattern,i);

	/* opendir (and other directory functions) test */
	printf("\nopendir() test\n");
	getkey();
	dir=opendir(".");
	while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
		t=fdate(dirent->d_name);
		printf("%.24s %10lu  %06o  %s%c\n"
			,ctime(&t)
			,flength(dirent->d_name)
			,getfattr(dirent->d_name)
			,dirent->d_name
			,isdir(dirent->d_name) ? '/':0
			);
	}
	if(dir!=NULL)
		closedir(dir);

	return 0;
}
