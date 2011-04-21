#ifndef _XPDOOR_H_
#define _XPDOOR_H_

#include <comio.h>
#include <sockwrap.h>
#include <xpdatetime.h>
#include <ciolib.h>

#define XPD_CP437_SUPPORTED	(1<<0)
#define XPD_ANSI_SUPPORTED	(1<<1)

struct xpd_user_info {
	char			*full_name;
	char			*location;	// AKA Calling From
	char			*home_phone;
	char			*work_phone;
	char			*password;
	int				level;
	unsigned int	times_on;
	isoDate_t		last_call_date;
	unsigned int	seconds_remaining;
	int				dflags;
	int				rows;
	BOOL			expert;
	isoDate_t		expiration;
	int				number;
	char			protocol;
	int				uploads;
	int				downloads;
	int				download_k_today;	// KiB downloaded today
	int				max_download_k_today;	// KiB downloaded today
	isoDate_t		birthday;
	char			*alias;
	isoTime_t		call_time;
	isoTime_t		last_call_time;
	int				max_files_per_day;
	int				downloads_today;
	int				total_upload_k;
	int				total_download_k;
	char			*comment;
	int				total_doors;
	int				total_messages;
	int				credits;
	BOOL			female;
	char			*address;
	char			*postal;
};

struct xpd_system_info {
	char					*main_dir;
	char					*gen_dir;
	char					*sysop_name;
	int						default_attr;
	char					*name;
	char					*ctrl_dir;
	char					*data_dir;
	int						total_nodes;
	char					*exec_dir;
	char					*text_dir;
	char					*temp_dir;
	char					*qwk_id;
};

struct xpd_dropfile_info {
	char					*path;
	int						baud;
	int						parity;
	int						node;
	int						dte;
	struct xpd_user_info	user;
	struct xpd_system_info	sys;
};

enum io_type {
	 XPD_IO_STDIO
	,XPD_IO_COM
	,XPD_IO_SOCKET
	,XPD_IO_TELNET
	,XPD_IO_LOCAL
};

struct xpd_telnet_io {
	SOCKET	sock;
	int		cmdlen;
	char	cmd[64];
	char	terminal[64];
	uchar	local_option[0x100];
	uchar	remote_option[0x100];
	uchar	orig_local_option[0x100];
	BOOL	orig_local_option_set;
	uchar	orig_remote_option[0x100];
	BOOL	orig_remote_option_set;
};

struct xpd_info {
	enum io_type				io_type;
	union {
		COM_HANDLE				com;
		SOCKET					sock;
		struct xpd_telnet_io	telnet;
	} io;
	int							doorway_mode;
	time_t						end_time;	/* -1 if there is no limit */
	struct xpd_dropfile_info	drop;
};

extern struct xpd_info	xpd_info;

/*
 * Parse command line
 */
void xpd_parse_cmdline(int argc, char **argv);

/*
 * Initialize (turns on Doorway mode)
 */
int xpd_init(void);

/*
 * Exit (turns off Doorway mode)
 */
void xpd_exit(void);

/*
 * Parse dropfile
 */
int xpd_parse_dropfile(void);

/*
 * Write "raw" (ie: May include ANSI sequences etc)
 */
int xpd_rwrite(const char *data, int data_len);

/*
 * Enables doorway mode
 */
void xpd_doorway(int enable);

#endif
