#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "genwrap.h"
#include "sem.h"

int sem_init(sem_t *sem, int pshared, unsigned int value)  {
	sem=malloc(sizeof(sem_t));
	if(sem==NULL)  {
		errno=ENOSPC;
		return(-1);
	}
	if(pipe((int *)sem)) {
		errno=EPERM;
		return(-1);
	}
	return(0);
}

int sem_destroy(sem_t *sem)  {
	close(sem->read);
	close(sem->write);
	sem=NULL;
	return(0);
}

int sem_post(sem_t *sem)  {
	if(sem==NULL)  {
		errno=EINVAL;
		return(-1);
	}
	write(sem->write,"-",1)==1;
	return(0);
}

int sem_wait(sem_t *sem)  {
	char	buf;
	if(sem==NULL) {
		errno=EINVAL;
		return(-1);
	}
	while(read(sem->read,&buf,1)<1)
		SLEEP(1);
	return(0);
}
