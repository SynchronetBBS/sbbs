#ifndef _BBSLIST_H_
#define _BBSLIST_H_

#include <time.h>

#define LIST_NAME_MAX	30
#define LIST_ADDR_MAX	30
#define MAX_USER_LEN	16
#define MAX_PASSWD_LEN	16

enum {
	 BBSLIST_SELECT
	,BBSLIST_EDIT
};

struct bbslist {
	char			name[LIST_NAME_MAX+1];
	char			addr[LIST_ADDR_MAX+1];
	short unsigned int port;
	time_t			added;
	time_t			connected;
	unsigned int	calls;
	char			user[MAX_USER_LEN+1];
	char			password[MAX_PASSWD_LEN+1];
	int				type;
	int				conn_type;
	int				id;
	int				reversed;
};

struct bbslist *show_bbslist(int mode,char *path);

#endif
