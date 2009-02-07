#include <stdio.h>
#include <string.h>
#include <genwrap.h>
#include <dirwrap.h>
#include <xpdatetime.h>
#include "xpdoor.h"

#define GETBUF()	if(fgets(buf,sizeof(buf),df)==NULL) goto done_parsing
#define GETIBUF(x)	if(fgets(buf,sizeof(buf),df)==NULL) goto done_parsing; x = atoi(buf)
#define GETSBUF(x)	if(fgets(buf,sizeof(buf),df)==NULL) goto done_parsing; x = strdup(truncnl(buf))
#define GETDBUF(x)	if(fgets(buf,sizeof(buf),df)==NULL) goto done_parsing; { \
						int m,d,y; \
						char *p; \
						p=strtok(buf,"/-"); \
						if(p) { \
							m=strtol(buf,NULL,10); p=strtok(NULL, "/-"); \
							if(p) { \
								d=strtol(buf,NULL,10); p=strtok(NULL, "/-"); \
								if(p) { \
									y=strtol(buf,NULL,10); y+=(y<100?2000:1900); if(y > xpDateTime_now().date.year) y-=100; x = isoDate_create(y,m,d); \
					}}}}
#define GETTBUF(x)	if(fgets(buf,sizeof(buf),df)==NULL) goto done_parsing; { \
						int h,m; \
						char *p; \
						p=strtok(buf,":"); \
						if(p) { \
							h=strtol(buf,NULL,10); p=strtok(NULL, ":"); \
							if(p) { \
								m=strtol(buf,NULL,10); x = isoTime_create(h,m,0); \
					}}}
#define GETBBUF(x)	if(fgets(buf,sizeof(buf),df)==NULL) goto done_parsing; if(buf[0]=='Y' || buf[0]=='y') x=TRUE
#define GETCBUF(x)	if(fgets(buf,sizeof(buf),df)==NULL) goto done_parsing; x=buf[0]

int xpd_parse_dropfile()
{
	FILE	*df=NULL;
	char	*p;
	char	buf[1024];
	int		tmp;

	if(xpd_info.drop.path==NULL)
		goto error_return;
	df=fopen(xpd_info.drop.path,"r");
	if(df==NULL)
		goto error_return;
	p=getfname(xpd_info.drop.path);
	if(p==NULL)
		goto error_return;
	if(stricmp(p,"door.sys")==0) {
		/* COM0:, COM1:, COM0:STDIO, COM0:SOCKET123 */
		GETBUF();
		if(strcmp(buf,"COM0:STDIO")==0) {
			xpd_info.io_type=XPD_IO_STDIO;
		}
		else if(strncmp(buf,"COM0:SOCKET",11)==0) {
			xpd_info.io_type=XPD_IO_SOCKET;
			xpd_info.io.sock=atoi(buf+11);
		}
		GETIBUF(xpd_info.drop.baud);
		GETIBUF(xpd_info.drop.parity);
		GETIBUF(xpd_info.drop.node);
		GETBUF();
		GETBUF();
		GETBUF();
		GETBUF();
		GETBUF();
		GETSBUF(xpd_info.drop.user.full_name);
		GETSBUF(xpd_info.drop.user.location);
		GETSBUF(xpd_info.drop.user.home_phone);
		GETSBUF(xpd_info.drop.user.work_phone);
		GETSBUF(xpd_info.drop.user.password);
		GETIBUF(xpd_info.drop.user.level);
		GETIBUF(xpd_info.drop.user.times_on);
		GETDBUF(xpd_info.drop.user.last_call_date);
		GETIBUF(xpd_info.drop.user.seconds_remaining);
		GETIBUF(tmp);
		if(tmp*60 > xpd_info.drop.user.seconds_remaining)
			xpd_info.drop.user.seconds_remaining = tmp*60;
		xpd_info.end_time=time(NULL)+xpd_info.drop.user.seconds_remaining;
		GETBUF();
		if(strcmp(buf,"GR"))
			xpd_info.drop.user.dflags |= XPD_ANSI_SUPPORTED|XPD_CP437_SUPPORTED;
		else if(strcmp(buf,"NG"))
			xpd_info.drop.user.dflags |= XPD_CP437_SUPPORTED;
		GETIBUF(xpd_info.drop.user.rows);
		GETBBUF(xpd_info.drop.user.expert);
		GETDBUF(xpd_info.drop.user.expiration);
		GETIBUF(xpd_info.drop.user.number);
		GETCBUF(xpd_info.drop.user.protocol);
		GETIBUF(xpd_info.drop.user.uploads);
		GETIBUF(xpd_info.drop.user.downloads);
		GETIBUF(xpd_info.drop.user.download_k_today);
		GETIBUF(xpd_info.drop.user.max_download_k_today);
		GETDBUF(xpd_info.drop.user.birthday);
		GETSBUF(xpd_info.drop.sys.main_dir);
		GETSBUF(xpd_info.drop.sys.gen_dir);
		GETSBUF(xpd_info.drop.sys.sysop_name);
		GETSBUF(xpd_info.drop.user.alias);
		GETBUF();
		GETBUF();
		GETBUF();
		GETBUF();
		GETIBUF(xpd_info.drop.sys.default_attr);
		GETBUF();
		GETBUF();
		GETTBUF(xpd_info.drop.user.call_time);
		GETTBUF(xpd_info.drop.user.last_call_time);
		GETIBUF(xpd_info.drop.user.max_files_per_day);
		GETIBUF(xpd_info.drop.user.downloads_today);
		GETIBUF(xpd_info.drop.user.total_upload_k);
		GETIBUF(xpd_info.drop.user.total_download_k);
		GETSBUF(xpd_info.drop.user.comment);
		GETIBUF(xpd_info.drop.user.total_doors);
		GETIBUF(xpd_info.drop.user.total_messages);
	}

error_return:
	if(df)
		fclose(df);
	return(-1);

done_parsing:
	if(ferror(df)) {
		fclose(df);
		return(-1);
	}
	if(feof(df)) {
		fclose(df);
		return(0);
	}
}
