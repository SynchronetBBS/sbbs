/* wraptest.c */

#include <time.h>	/* ctime */

#include "genwrap.h"
#include "conwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "sockwrap.h"
#include "threadwrap.h"

int main()
{
	char	compiler[128];
	char	fname[MAX_PATH+1];
	char*	glob_pattern = "*wrap*";
	int		i;
	uint	u;
	time_t	t;
	glob_t	g;
	DIR*	dir;
	DIRENT*	dirent;

	COMPILER_DESC(compiler);
	printf("Platform: %s\n",PLATFORM_DESC);
	printf("Compiler: %s\n",compiler);

	xp_beep(500,500);

	/* GLOB TEST */
	printf("glob() test\n");
	i=glob(glob_pattern,GLOB_MARK,NULL,&g);
	if(i==0) {
		for(u=0;u<g.gl_pathc;u++)
			printf("%s\n",g.gl_pathv[u]);
		globfree(&g);
	} else
		printf("glob(%s) returned %d\n",glob_pattern,i);

	printf("Hit any key...");
	printf("\ngetch() returned %d\n",getch());

	/* OPENDIR TEST */
	printf("opendir() test\n");
	dir=opendir(".");
	while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
		t=fdate(dirent->d_name);
		sprintf(fname,"%.*s%c"
			,sizeof(fname)-2
			,dirent->d_name
			,isdir(dirent->d_name) ? '/':0);
		printf("%-25s %10lu %.24s\n"
			,fname
			,flength(dirent->d_name)
			,ctime(&t)
			);
	}
	if(dir!=NULL)
		closedir(dir);

	printf("Hit any key...");
	printf("\ngetch() returned %d\n",getch());

	return 0;
}