#include "sockwrap.h"
#include <poll.h>
#include <termios.h>
#include <sys/un.h>
#include <stdio.h>

void usage(char *cmd)  {
	printf("Usage: %s <socket>\n\n Where socket is the filename of a unix domain socket\n\n",cmd);
	exit(0);
}

void spyon(char *sockname)  {
	SOCKET		spy_sock=INVALID_SOCKET;
	struct sockaddr_un spy_name;
	socklen_t	spy_len;
	char		key;
	char		buf[1];
	int		i;
	struct	termios	tio;
	struct	termios	old_tio;
	struct pollfd pset[2];

	/* ToDo Test for it actually being a socket! */

	if((spy_sock=socket(PF_LOCAL,SOCK_STREAM,0))==INVALID_SOCKET)  {
		printf("ERROR %d creating local spy socket", errno);
		return;
	}
	
	spy_name.sun_family=AF_LOCAL;
	SAFECOPY(spy_name.sun_path,sockname);
	spy_len=SUN_LEN(&spy_name);
	if(connect(spy_sock,(struct sockaddr *)&spy_name,spy_len))  {
		printf("Could not connect to %s\n",spy_name.sun_path);
		return;
	}
	i=1;

	tcgetattr(STDIN_FILENO,&old_tio);
	tcgetattr(STDIN_FILENO,&tio);
	cfmakeraw(&tio);
	tcsetattr(STDIN_FILENO,TCSANOW,&tio);
	pset[0].fd=STDIN_FILENO;
	pset[0].events=POLLIN;
	pset[1].fd=spy_sock;
	pset[1].events=POLLIN;
	while(spy_sock!=INVALID_SOCKET)  {
		if((poll(pset,2,INFTIM))<0)  {
			close(spy_sock);
			spy_sock=INVALID_SOCKET;
		}
		if(pset[0].revents)  {
			if(pset[0].revents&(POLLNVAL|POLLHUP|POLLERR))  {
				close(spy_sock);
				spy_sock=INVALID_SOCKET;
			}
			else  {
				if((i=read(STDIN_FILENO,&key,1))==1)  {
					/* Check for control keys */
					switch(key)  {
						case CTRL('c'):
							printf("\r\n\r\nConnection closed.\r\n");
							close(spy_sock);
							spy_sock=INVALID_SOCKET;
							return;
						default:
							write(spy_sock,&key,1);
					}
				}
				else if(i<0) {
					close(spy_sock);
					spy_sock=INVALID_SOCKET;
				}
			}
		}
		if(pset[1].revents)  {
			if(pset[1].revents&(POLLNVAL|POLLHUP|POLLERR))  {
				close(spy_sock);
				spy_sock=INVALID_SOCKET;
			}
			else  {
				if((i=read(spy_sock,&buf,1))==1)  {
					write(STDOUT_FILENO,buf,1);
				}
				else if(i<0) {
					close(spy_sock);
					spy_sock=INVALID_SOCKET;
				}
			}
		}
	}
	tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);
}

int main(int argc, char** argv)  {
	if(argc==1)
		usage(argv[0]);

	spyon(argv[1]);
}
