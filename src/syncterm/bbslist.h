#ifndef _BBSLIST_H_
#define _BBSLIST_H_

#include <time.h>

#define LIST_NAME_MAX	30
#define LIST_ADDR_MAX	30
#define MAX_USER_LEN	16
#define MAX_PASSWD_LEN	16

enum {
	 USER_BBSLIST
	,SYSTEM_BBSLIST
};

enum {
	 BBSLIST_SELECT
	,BBSLIST_EDIT
};

enum {
	 SCREEN_MODE_CURRENT
	,SCREEN_MODE_80X25
	,SCREEN_MODE_80X28
	,SCREEN_MODE_80X43
	,SCREEN_MODE_80X50
	,SCREEN_MODE_80X60
	,SCREEN_MODE_TERMINATOR
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
	int				screen_mode;
	int				nostatus;
};

struct bbslist *show_bbslist(int mode,char *path);

#endif
