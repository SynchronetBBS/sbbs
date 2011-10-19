#include <dirwrap.h>
#include <string.h>

int mdel(char *glob)
{
	int	count=0;
	char	this[MAX_PATH+1];

	strcpy(this,glob);
	while(fexistcase(this)) {
		if(unlink(this)==-1)
			return(-1);
		count++;
		strcpy(this,glob);
	}
	return(count);
}
