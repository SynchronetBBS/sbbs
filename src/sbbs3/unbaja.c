/* $Id$ */

/* 
 * Stuff left ToDo:
 * Add label pre-resolution so only used labels are inserted
 */

#include <stdio.h>
#include <string.h>

#include "dirwrap.h"

#include "cmdshell.h"
#include "ars_defs.h"

char *getvar(long name)
{
	static char varname[20];

	switch(name) {
		case 0:
			return("STR");
		case 0x490873f1:
			return("_USERON.ALIAS");
		case 0x5de44e8b:
			return("_USERON.NAME");
		case 0x979ef1de:
			return("_USERON.HANDLE");
		case 0xc8cd5fb7:
			return("_USERON.COMP");
		case 0xcc7aca99:
			return("_USERON.NOTE");
		case 0xa842c43b:
			return("_USERON.ADDRESS");
		case 0x4ee1ff3a:
			return("_USERON.LOCATION");
		case 0xf000aa78:
			return("_USERON.ZIPCODE");
		case 0xcdb7e4a9:
			return("_USERON.PASS");
		case 0x94d59a7a:
			return("_USERON.BIRTH");
		case 0xec2b8fb8:
			return("_USERON.PHONE");
		case 0x08f65a2a:
			return("_USERON.MODEM");
		case 0xc7e0e8ce:
			return("_USERON.NETMAIL");
		case 0xd3606303:
			return("_USERON.TMPEXT");
		case 0x3178f9d6:
			return("_USERON.COMMENT");
		case 0x41239e21:
			return("_CONNECTION");
		case 0x90fc82b4:
			return("_CID");
		case 0x15755030:
			return("_COMSPEC");
		case 0x5E049062:
			/* ToDo... undocumented */
			return("_QUESTION");
		case 0xf19cd046:
			/* ToDo... undocumented */
			return("_WORDWRAP");
		case 0x908ece53:
			return("_USERON.NUMBER");
		case 0xdcedf626:
			return("_USERON.ULS");
		case 0xc1093f61:
			return("_USERON.DLS");
		case 0x2039a29f:
			return("_USERON.POSTS");
		case 0x4a9f3955:
			return("_USERON.EMAILS");
		case 0x0c8dcf3b:
			return("_USERON.FBACKS");
		case 0x9a13bf95:
			return("_USERON.ETODAY");
		case 0xc9082cbd:
			return("_USERON.PTODAY");
		case 0x7c72376d:
			return("_USERON.TIMEON");
		case 0xac72c50b:
			return("_USERON.TEXTRA");
		case 0x04807a11:
			return("_USERON.LOGONS");
		case 0x52996eab:
			return("_USERON.TTODAY");
		case 0x098bdfcb:
			return("_USERON.TLAST");
		case 0xbd1cee5d:
			return("_USERON.LTODAY");
		case 0x07954570:
			return("_USERON.XEDIT");
		case 0xedf6aa98:
			return("_USERON.SHELL");
		case 0x328ed476:
			return("_USERON.LEVEL");
		case 0x9e70e855:
			return("_USERON.SEX");
		case 0x094cc42c:
			return("_USERON.ROWS");
		case 0xabc4317e:
			return("_USERON.PROT");
		case 0x7dd9aac0:
			return("_USERON.LEECH");
		case 0x7c602a37:
			return("_USERON.MISC");
		case 0x61be0d36:
			return("_USERON.QWK");
		case 0x665ac227:
			return("_USERON.CHAT");
		case 0x951341ab:
			return("_USERON.FLAGS1");
		case 0x0c1a1011:
			return("_USERON.FLAGS2");
		case 0x7b1d2087:
			return("_USERON.FLAGS3");
		case 0xe579b524:
			return("_USERON.FLAGS4");
		case 0x12e7d6d2:
			return("_USERON.EXEMPT");
		case 0xfed3115d:
			return("_USERON.REST");
		case 0xb65dd6d4:
			return("_USERON.ULB");
		case 0xabb91f93:
			return("_USERON.DLB");
		case 0x92fb364f:
			return("_USERON.CDT");
		case 0xd0a99c72:
			return("_USERON.MIN");
		case 0xd7ae3022:
			return("_USERON.FREECDT");
		case 0x1ef214ef:
			return("_USERON.FIRSTON");
		case 0x0ea515b1:
			return("_USERON.LASTON");
		case 0x2aaf9bd3:
			return("_USERON.EXPIRE");
		case 0x89c91dc8:
			return("_USERON.PWMOD");
		case 0x5b0d0c54:
			return("_USERON.NS_TIME");

		case 0xae256560:
			return("_CUR_RATE");
		case 0x2b3c257f:
			return("_CUR_CPS");
		case 0x1c4455ee:
			return("_DTE_RATE");
		case 0x7fbf958e:
			return("_LNCNTR");
		case 0x5c1c1500:
			return("_TOS");
		case 0x613b690e:
			return("_ROWS");
		case 0x205ace36:
			return("_AUTOTERM");
		case 0x7d0ed0d1:
			return("_CONSOLE");
		case 0xbf31a280:
			return("_ANSWERTIME");
		case 0x83aa2a6a:
			return("_LOGONTIME");
		case 0xb50cb889:
			return("_NS_TIME");
		case 0xae92d249:
			return("_LAST_NS_TIME");
		case 0x97f99eef:
			return("_ONLINE");
		case 0x381d3c2a:
			return("_SYS_STATUS");
		case 0x7e29c819:
			return("_SYS_MISC");
		case 0x11c83294:
			return("_SYS_PSNUM");
		case 0x02408dc5:
			return("_SYS_TIMEZONE");
		case 0x78afeaf1:
			return("_SYS_PWDAYS");
		case 0xd859385f:
			return("_SYS_DELDAYS");
		case 0x6392dc62:
			return("_SYS_AUTODEL");
		case 0x698d59b4:
			return("_SYS_NODES");
		case 0x6fb1c46e:
			return("_SYS_EXP_WARN");
		case 0xdf391ca7:
			return("_SYS_LASTNODE");
		case 0xdd982780:
			return("_SYS_AUTONODE");
		case 0xf53db6c7:
			return("_NODE_SCRNLEN");
		case 0xa1f0fcb7:
			return("_NODE_SCRNBLANK");
		case 0x709c07da:
			return("_NODE_MISC");
		case 0xb17e7914:
			return("_NODE_VALUSER");
		case 0xadae168a:
			return("_NODE_IVT");
		case 0x2aa89801:
			return("_NODE_SWAP");
		case 0x4f02623a:
			return("_NODE_MINBPS");
		case 0xe7a7fb07:
			return("_NODE_NUM");
		case 0x6c8e350a:
			return("_NEW_LEVEL");
		case 0xccfe7c5d:
			return("_NEW_FLAGS1");
		case 0x55f72de7:
			return("_NEW_FLAGS2");
		case 0x22f01d71:
			return("_NEW_FLAGS3");
		case 0xbc9488d2:
			return("_NEW_FLAGS4");
		case 0x4b0aeb24:
			return("_NEW_EXEMPT");
		case 0x20cb6325:
			return("_NEW_REST");
		case 0x31178ba2:
			return("_NEW_CDT");
		case 0x7345219f:
			return("_NEW_MIN");
		case 0xb3f64be4:
			return("_NEW_SHELL");
		case 0xa278584f:
			return("_NEW_MISC");
		case 0x7342a625:
			return("_NEW_EXPIRE");
		case 0x75dc4306:
			return("_NEW_PROT");
		case 0xfb394e27:
			return("_EXPIRED_LEVEL");
		case 0x89b69753:
			return("_EXPIRED_FLAGS1");
		case 0x10bfc6e9:
			return("_EXPIRED_FLAGS2");
		case 0x67b8f67f:
			return("_EXPIRED_FLAGS3");
		case 0xf9dc63dc:
			return("_EXPIRED_FLAGS4");
		case 0x0e42002a:
			return("_EXPIRED_EXEMPT");
		case 0x4569c62e:
			return("_EXPIRED_REST");
		case 0xfcf3542e:
			return("_MIN_DSPACE");
		case 0xcf9ce02c:
			return("_CDT_MIN_VALUE");
		case 0xfcb5b274:
			return("_CDT_PER_DOLLAR");
		case 0x4db200d2:
			return("_LEECH_PCT");
		case 0x9a7d9cca:
			return("_LEECH_SEC");
		case 0x396b7167:
			return("_NETMAIL_COST");
		case 0x5eeaff21:
			return("_NETMAIL_MISC");
		case 0x82d9484e:
			return("_INETMAIL_COST");
		case 0xe558c608:
			return("_INETMAIL_MISC");

		case 0xc6e8539d:
			return("_LOGON_ULB");
		case 0xdb0c9ada:
			return("_LOGON_DLB");
		case 0xac58736f:
			return("_LOGON_ULS");
		case 0xb1bcba28:
			return("_LOGON_DLS");
		case 0x9c5051c9:
			return("_LOGON_POSTS");
		case 0xc82ba467:
			return("_LOGON_EMAILS");
		case 0x8e395209:
			return("_LOGON_FBACKS");
		case 0x8b12ba9d:
			return("_POSTS_READ");
		case 0xe51c1956:
			return("_LOGFILE");
		case 0x5a22d4bd:
			return("_NODEFILE");
		case 0x3a37c26b:
			return("_NODE_EXT");

		case 0xeb6c9c73:
			return("_ERRORLEVEL");

		case 0x5aaccfc5:
			return("_ERRNO");

		case 0x057e4cd4:
			return("_TIMELEFT");

		case 0x1e5052a7:
			return("_MAX_MINUTES");
		case 0xedc643f1:
			return("_MAX_QWKMSGS");

		case 0x430178ec:
			return("_UQ");

		case 0x455CB929:
			/* ToDo - undocumented */
			return("_FTP_MODE");

		case 0x2105D2B9:
			/* ToDo - undocumented */
			return("_SOCKET_ERROR");

		case 0xA0023A2E:
			/* ToDo - undocumented */
			return("_STARTUP_OPTIONS");

		case 0x16E2585F:
			/* ToDo - undocumented */
			return("_CLIENT_SOCKET");

		default:
			sprintf(varname,"var_%08x",name);
			return(varname);
	}
}

void write_var(FILE *bin, FILE *src)
{
	long lng;

	fread(&lng, 1, 4, bin);
	fprintf(src,"%s ",getvar(lng));
}

void write_cstr(FILE *bin, FILE *src)
{
	uchar ch;

	fputc('"',src);
	while(fread(&ch,1,1,bin)==1) {
		if(ch==0)
			break;
		if(ch<' ' || ch > 126 || ch == '"') {
			fprintf(src, "\\%03d", ch);
		}
		else
			fputc(ch, src);
	}
	fputc('"',src);
	fputc(' ',src);
}

void write_lng(FILE *bin, FILE *src)
{
	long lng;

	fread(&lng,4,1,bin);
	fprintf(src,"%ld ",lng);
}

void write_short(FILE *bin, FILE *src)
{
	short sht;

	fread(&sht,2,1,bin);
	fprintf(src,"%d ",sht);
}

void write_ushort(FILE *bin, FILE *src)
{
	ushort sht;

	fread(&sht,2,1,bin);
	fprintf(src,"%d ",sht);
}

void write_ch(FILE *bin, FILE *src)
{
	char ch;

	fread(&ch,1,1,bin);
	fprintf(src,"%c ",ch);
}

void write_uchar(FILE *bin, FILE *src)
{
	uchar uch;

	fread(&uch,1,1,bin);
	fprintf(src,"%u ",uch);
}

void write_logic(FILE *bin, FILE *src)
{
	char ch;
	fread(&ch,1,1,bin);
	if(ch==LOGIC_TRUE)
		fputs("TRUE ",src);
	else
		fputs("FALSE ",src);
}

void write_key(FILE *bin, FILE *src)
{
	uchar uch;
	fread(&uch,1,1,bin);
	if(uch==CS_DIGIT)
		fputs("DIGIT ",src);
	else if(uch==CS_EDIGIT)
		fputs("EDIGIT ",src);
	else if(uch==CR)
		fputs("\\r ",src);
	else if(uch==LF)
		fputs("\\n ",src);
	else if(uch==TAB)
		fputs("\\t ",src);
	else if(uch==BS)
		fputs("\\b ",src);
	else if(uch==BEL)
		fputs("\\a ",src);
	else if(uch==FF)
		fputs("\\f ",src);
	else if(uch==11)
		fputs("\\v ",src);
	else if(uch & 0x80)
		fprintf(src,"/%c ",uch&0x7f);
	else if(uch < ' ')
		fprintf(src,"^%c ",uch+0x40);
	else if(uch=='\\')
		fputs("'\\' ",src);
	else if(uch=='^')
		fputs("'^' ",src);
	else if(uch=='/')
		fputs("'/' ",src);
	else if(uch=='\'')
		fputs("\\' ",src);
	else
		fprintf(src,"%c ",uch);
}

void eol(FILE *src)
{
	fputc('\n',src);
}

#define WRITE_NAME(name)	fputs(name" ",src) \
							/* printf("%s\n",name) */
							

#define KEY(name)		WRITE_NAME(name); \
						write_key(bin,src); \
						eol(src);			 \
						break

#define LGC(name)		WRITE_NAME(name); \
						write_logic(bin,src); \
						eol(src);			 \
						break

#define NONE(name)		WRITE_NAME(name); \
						eol(src);			 \
						break

#define VAR(name)		WRITE_NAME(name); \
						write_var(bin,src);  \
						eol(src);			 \
						break

#define CH(name)		WRITE_NAME(name); \
						write_ch(bin,src);  \
						eol(src);			 \
						break

#define CHCH(name)		WRITE_NAME(name); \
						write_ch(bin,src);  \
						write_ch(bin,src);  \
						eol(src);			 \
						break

#define UCH(name)		WRITE_NAME(name); \
						write_uchar(bin,src);  \
						eol(src);			 \
						break

#define UCHUCH(name)	WRITE_NAME(name); \
						write_uchar(bin,src);  \
						write_uchar(bin,src);  \
						eol(src);			 \
						break

#define SHT(name)		WRITE_NAME(name); \
						write_short(bin,src);  \
						eol(src);			 \
						break


#define LNG(name)		WRITE_NAME(name); \
						write_lng(bin,src);  \
						eol(src);			 \
						break

#define STR(name)		WRITE_NAME(name); \
						write_cstr(bin,src); \
						eol(src);			 \
						break
						
#define VARVAR(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_var(bin,src);  \
						eol(src);			 \
						break
						
#define VARVARVAR(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_var(bin,src);  \
						write_var(bin,src);  \
						eol(src);			 \
						break
						
#define VARSTR(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_cstr(bin,src); \
						eol(src);			 \
						break
						
#define VARLNG(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_lng(bin,src);  \
						eol(src);			 \
						break
						
#define MUCH(name)		WRITE_NAME(name); \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,1,1,bin); \
						} else {				 \
							write_uchar(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MSHT(name)		WRITE_NAME(name); \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,2,1,bin); \
						} else {				 \
							write_short(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MVARLNG(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,4,1,bin); \
						} else {				 \
							write_lng(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MVARSHT(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,2,1,bin); \
						} else {				 \
							write_short(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MVARUCH(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,1,1,bin); \
						} else {				 \
							write_uchar(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MVARVARUST(name)	WRITE_NAME(name); \
							write_var(bin,src);  \
							write_var(bin,src);  \
							if(usevar) {		 \
								fprintf(src,"%s ",getvar(var)); \
								usevar=FALSE;	 \
								fread(buf,2,1,bin); \
							} else {				 \
								write_ushort(bin,src);  \
							}					 \
							eol(src);			 \
							break

#define MVARUSTVAR(name)	WRITE_NAME(name); \
							write_var(bin,src);  \
							if(usevar) {		 \
								fprintf(src,"%s ",getvar(var)); \
								usevar=FALSE;	 \
								fread(buf,2,1,bin); \
							} else {				 \
								write_ushort(bin,src);  \
							}					 \
							write_var(bin,src);  \
							eol(src);			 \
							break

#define MVARUSTSTR(name)	WRITE_NAME(name); \
							write_var(bin,src);  \
							if(usevar) {		 \
								fprintf(src,"%s ",getvar(var)); \
								usevar=FALSE;	 \
								fread(buf,2,1,bin); \
							} else {				 \
								write_ushort(bin,src);  \
							}					 \
							write_cstr(bin,src);  \
							eol(src);			 \
							break

#define MLNG(name)	WRITE_NAME(name); \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,4,1,bin); \
						} else {				 \
							write_lng(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MLNGVAR(name)	WRITE_NAME(name); \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,4,1,bin); \
						} else {				 \
							write_lng(bin,src);  \
						}					 \
						write_var(bin,src);  \
						eol(src);			 \
						break

#define MSHTSTR(name)	WRITE_NAME(name); \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,2,1,bin); \
						} else {				 \
							write_short(bin,src);  \
						}					 \
						write_cstr(bin,src);  \
						eol(src);			 \
						break

#define MLNGSTR(name)	WRITE_NAME(name); \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,4,1,bin); \
						} else {				 \
							write_lng(bin,src);  \
						}					 \
						write_cstr(bin,src);  \
						eol(src);			 \
						break

#define MVARLNGLNG(name)	WRITE_NAME(name); \
							write_var(bin,src);  \
							if(usevar) {		 \
								fprintf(src,"%s ",getvar(var)); \
								usevar=FALSE;	 \
								fread(buf,4,1,bin); \
							} else {				 \
								write_lng(bin,src);  \
							}					 \
							write_lng(bin,src);  \
							eol(src);			 \
							break

#define MVARUCHLNG(name)	WRITE_NAME(name); \
							write_var(bin,src);  \
							if(usevar) {		 \
								fprintf(src,"%s ",getvar(var)); \
								usevar=FALSE;	 \
								fread(buf,1,1,bin); \
							} else {				 \
								write_uchar(bin,src);  \
							}					 \
							write_lng(bin,src);  \
							eol(src);			 \
							break

#define MVARCH(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						if(usevar) {		 \
							fprintf(src,"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,1,1,bin); \
						} else {				 \
							write_ch(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define VARSTRVAR(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_cstr(bin,src); \
						write_var(bin,src);  \
						eol(src);			 \
						break

#define UCHVARSTR(name)	WRITE_NAME(name); \
						write_uchar(bin,src); \
						write_var(bin,src);  \
						write_cstr(bin,src);  \
						eol(src);			 \
						break

#define UCHVARVAR(name)	WRITE_NAME(name); \
						write_uchar(bin,src); \
						write_var(bin,src);  \
						write_var(bin,src);  \
						eol(src);			 \
						break

#define UCHVAR(name)	WRITE_NAME(name); \
						write_uchar(bin,src); \
						write_var(bin,src);  \
						eol(src);			 \
						break

#define UCHSTR(name)	WRITE_NAME(name); \
						write_uchar(bin,src); \
						write_cstr(bin,src);  \
						eol(src);			 \
						break

#define CHSTR(name)		WRITE_NAME(name); \
						write_ch(bin,src); \
						write_cstr(bin,src);  \
						eol(src);			 \
						break

#define VARCH(name)		WRITE_NAME(name); \
						write_var(bin,src);  \
						write_ch(bin,src);	 \
						eol(src);			 \
						break

#define CHVAR(name)		WRITE_NAME(name); \
						write_ch(bin,src);	 \
						write_var(bin,src);  \
						eol(src);			 \
						break

#define VARVARSHT(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_var(bin,src);  \
						write_short(bin,src);\
						eol(src);			 \
						break
						
#define VARVARUST(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_var(bin,src);  \
						write_ushort(bin,src);\
						eol(src);			 \
						break

char *decompile_ars(uchar *ars, int len)
{
	int i;
	static char	buf[1024];
	char	*out;
	uchar	*in;
	uint	artype;
	char	ch;
	uint	n;
	int		equals=0;
	int		not=0;

	out=buf;
	buf[0]=0;
	for(in=ars;in<ars+len;in++) {
		switch(*in) {
			case AR_NULL:
				break;
			case AR_OR:
				*(out++)='|';
				artype=*in;
				break;
			case AR_NOT:
				not=1;
				break;
			case AR_EQUAL:
				equals=1;
				break;
			case AR_BEGNEST:
				if(not)
					*(out++)='!';
				not=0;
				*(out++)='(';
				artype=*in;
				break;
			case AR_ENDNEST:
				*(out++)=')';
				artype=*in;
				break;
			case AR_LEVEL:
				*(out++)='$';
				*(out++)='L';
				artype=*in;
				break;
			case AR_AGE:
				*(out++)='$';
				*(out++)='A';
				artype=*in;
				break;
			case AR_BPS:
				*(out++)='$';
				*(out++)='B';
				artype=*in;
				break;
			case AR_NODE:
				*(out++)='$';
				*(out++)='N';
				artype=*in;
				break;
			case AR_TLEFT:
				*(out++)='$';
				*(out++)='R';
				artype=*in;
				break;
			case AR_TUSED:
				*(out++)='$';
				*(out++)='O';
				artype=*in;
				break;
			case AR_USER:
				*(out++)='$';
				*(out++)='U';
				artype=*in;
				break;
			case AR_TIME:
				*(out++)='$';
				*(out++)='T';
				artype=*in;
				break;
			case AR_PCR:
				*(out++)='$';
				*(out++)='P';
				artype=*in;
				break;
			case AR_FLAG1:
				*(out++)='$';
				*(out++)='F';
				*(out++)='1';
				artype=*in;
				break;
			case AR_FLAG2:
				*(out++)='$';
				*(out++)='F';
				*(out++)='2';
				artype=*in;
				break;
			case AR_FLAG3:
				*(out++)='$';
				*(out++)='F';
				*(out++)='3';
				artype=*in;
				break;
			case AR_FLAG4:
				*(out++)='$';
				*(out++)='F';
				*(out++)='4';
				artype=*in;
				break;
			case AR_EXEMPT:
				*(out++)='$';
				*(out++)='X';
				artype=*in;
				break;
			case AR_REST:
				*(out++)='$';
				*(out++)='Z';
				artype=*in;
				break;
			case AR_SEX:
				*(out++)='$';
				*(out++)='S';
				artype=*in;
				break;
			case AR_UDR:
				*(out++)='$';
				*(out++)='K';
				artype=*in;
				break;
			case AR_UDFR:
				*(out++)='$';
				*(out++)='D';
				artype=*in;
				break;
			case AR_EXPIRE:
				*(out++)='$';
				*(out++)='E';
				artype=*in;
				break;
			case AR_CREDIT:
				*(out++)='$';
				*(out++)='C';
				artype=*in;
				break;
			case AR_DAY:
				*(out++)='$';
				*(out++)='W';
				artype=*in;
				break;
			case AR_ANSI:
				if(not)
					*(out++)='!';
				not=0;
				*(out++)='$';
				*(out++)='[';
				artype=*in;
				break;
			case AR_RIP:
				if(not)
					*(out++)='!';
				not=0;
				*(out++)='$';
				*(out++)='*';
				artype=*in;
				break;
			case AR_LOCAL:
				if(not)
					*(out++)='!';
				not=0;
				*(out++)='$';
				*(out++)='G';
				artype=*in;
				break;
			case AR_GROUP:
				*(out++)='$';
				*(out++)='M';
				artype=*in;
				break;
			case AR_SUB:
				*(out++)='$';
				*(out++)='H';
				artype=*in;
				break;
			case AR_LIB:
				*(out++)='$';
				*(out++)='I';
				artype=*in;
				break;
			case AR_DIR:
				*(out++)='$';
				*(out++)='J';
				artype=*in;
				break;
			case AR_EXPERT :
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"EXPERT");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_SYSOP:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"SYSOP");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_QUIET:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"QUIET");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_MAIN_CMDS:
				*out=0;
				strcat(out,"MAIN_CMDS");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_FILE_CMDS:
				*out=0;
				strcat(out,"FILE_CMDS");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_RANDOM:
				*(out++)='$';
				*(out++)='Q';
				artype=*in;
				break;
			case AR_LASTON:
				*(out++)='$';
				*(out++)='Y';
				artype=*in;
				break;
			case AR_LOGONS:
				*(out++)='$';
				*(out++)='V';
				artype=*in;
				break;
			case AR_WIP:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"WIP");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_SUBCODE:
				*out=0;
				strcat(out,"SUBCODE");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_DIRCODE:
				*out=0;
				strcat(out,"DIRCODE");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_OS2:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"OS2");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_DOS:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"DOS");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_WIN32:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"WIN32");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_UNIX:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"UNIX");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_LINUX :
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"LINUX");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_SHELL:
				*out=0;
				strcat(out,"SHELL");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_PROT:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"PROT");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_GUEST:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"GUEST");
				out=strchr(buf,0);
				artype=*in;
				break;
			case AR_QNODE:
				if(not)
					*(out++)='!';
				not=0;
				*out=0;
				strcat(out,"QNODE");
				out=strchr(buf,0);
				artype=*in;
				break;
			default:
				printf("Error decoding ARS!\n");
				return("Unknown ARS String");
		}
		switch(*in) {
			case AR_TIME:
				if(not)
					*(out++)='!';
				if(equals)
					*(out++)='=';
				not=equals=0;
				in++;
				n=*((short *)in);
				in++;
				out+=sprintf(out,"%02d:%02d",n/60,n%60);
				break;
			case AR_AGE:    /* byte operands */
			case AR_PCR:
			case AR_UDR:
			case AR_UDFR:
			case AR_NODE:
			case AR_LEVEL:
			case AR_TLEFT:
			case AR_TUSED:
				if(not)
					*(out++)='!';
				if(equals)
					*(out++)='=';
				not=equals=0;
				in++;
				out+=sprintf(out,"%d",*in);
				break;
			case AR_BPS:    /* int operands */
			case AR_MAIN_CMDS:
			case AR_FILE_CMDS:
			case AR_EXPIRE:
			case AR_CREDIT:
			case AR_USER:
			case AR_RANDOM:
			case AR_LASTON:
			case AR_LOGONS:
				if(not)
					*(out++)='!';
				if(equals)
					*(out++)='=';
				not=equals=0;
				in++;
    			n=*((short *)in);
				in++;
				out+=sprintf(out,"%d",n);
    			break;
			case AR_GROUP:
			case AR_LIB:
			case AR_DIR:
			case AR_SUB:
				if(not)
					*(out++)='!';
				if(equals)
					*(out++)='=';
				not=equals=0;
				in++;
    			n=*((short *)in);
				n++;              /* convert from to 0 base */
				in++;
				out+=sprintf(out,"%d",n);
    			break;
			case AR_SUBCODE:
			case AR_DIRCODE:
			case AR_SHELL:
				if(not)
					*(out++)='!';
				if(equals)
					*(out++)='=';
				not=equals=0;
				in++;
				n=sprintf(out,"%s ",in);
				out+=n;
				in+=n-1;
				break;
			case AR_FLAG1:
			case AR_FLAG2:
			case AR_FLAG3:
			case AR_FLAG4:
			case AR_EXEMPT:
			case AR_SEX:
			case AR_REST:
				if(not)
					*(out++)='!';
				if(equals)
					*(out++)='=';
				not=equals=0;
				in++;
				*(out++)=*in;
				break;
		}
	}
	*out=0;
	return(buf);
}

void decompile(FILE *bin, FILE *src)
{
	int i;
	char	ch;
	uchar	uch;
	ushort	ush;
	short	sh;
	long	lng;
	long	lng2;
	ulong	ulng;
	int		usevar=FALSE;
	long	var=0;
	char	buf[80];
	char	*p;

	while(!feof(bin)) {
		ush=ftell(bin);
		fprintf(src,":label_%04x\n",ush);

		if(fread(&uch,1,1,bin)!=1)
			break;
		switch(uch) {
			case CS_USE_INT_VAR:
				usevar=TRUE;
				fread(&var,4,1,bin);
				fread(&buf,2,1,bin);	/* offset/length */
				break;
			case CS_VAR_INSTRUCTION:
				fread(&uch,1,1,bin);
				switch(uch) {
					case SHOW_VARS:
						fprintf(src,"SHOW_VARS\n");
						break;
					case PRINT_VAR:
						VAR("PRINT");
					case VAR_PRINTF:
					case VAR_PRINTF_LOCAL:
						if(uch==VAR_PRINTF)
							fprintf(src,"PRINTF ");
						else
							fprintf(src,"LPRINTF ");
						write_cstr(bin,src);
						fread(&uch, 1, 1, bin);
						for(i=0; i<uch; i++) {
							write_var(bin,src);
						}
						eol(src);
						break;
					case DEFINE_STR_VAR:
						VAR("STR");
					case DEFINE_INT_VAR:
						VAR("INT");
					case DEFINE_GLOBAL_STR_VAR:
						VAR("GLOBAL_STR");
					case DEFINE_GLOBAL_INT_VAR:
						VAR("GLOBAL_INT");
					case SET_STR_VAR:
						VARSTR("SET");
					case SET_INT_VAR:
						VARLNG("SET");
					case COMPARE_STR_VAR:
						VARSTR("COMPARE");
					case COMPARE_INT_VAR:
						VARLNG("COMPARE");
					case STRNCMP_VAR:
						UCHVARSTR("STRNCMP");
					case STRSTR_VAR:
						VARSTR("STRSTR");
					case COMPARE_VARS:
						VARVAR("COMPARE");
					case STRNCMP_VARS:
						UCHVARVAR("STRNCMP");
					case STRSTR_VARS:
						VARVAR("STRSTR");
					case COPY_VAR:
						VARVAR("COPY");
					case SWAP_VARS:
						VARVAR("SWAP");
					case CAT_STR_VAR:
						VARSTR("STRCAT");
					case CAT_STR_VARS:
						VARVAR("STRCAT");
					case FORMAT_STR_VAR:
						fprintf(src,"SPRINTF ");
						write_var(bin,src);
						write_cstr(bin,src);
						fread(&uch, 1, 1, bin);
						for(i=0; i<uch; i++) {
							write_var(bin,src);
						}
						eol(src);
						break;
					case TIME_STR:
						VARVAR("TIME_STR");
					case DATE_STR:
						VARVAR("DATE_STR");
					case FORMAT_TIME_STR:
						VARSTRVAR("STRFTIME");
					case SECOND_STR:
						VARVAR("SECOND_STR");
					case STRUPR_VAR:
						VAR("STRUPR");
					case STRLWR_VAR:
						VAR("STRLWR");
					case ADD_INT_VAR:
						VARLNG("ADD");
					case ADD_INT_VARS:
						VARVAR("ADD");
					case SUB_INT_VAR:
						VARLNG("SUB");
					case SUB_INT_VARS:
						VARVAR("SUB");
					case MUL_INT_VAR:
						VARLNG("MUL");
					case MUL_INT_VARS:
						VARVAR("MUL");
					case DIV_INT_VAR:
						VARLNG("DIV");
					case DIV_INT_VARS:
						VARVAR("DIV");
					case MOD_INT_VAR:
						VARLNG("MOD");
					case MOD_INT_VARS:
						VARVAR("MOD");
					case AND_INT_VAR:
						VARLNG("AND");
					case AND_INT_VARS:
						VARVAR("AND");
					case COMPARE_ANY_BITS:
						MVARLNG("COMPARE_ANY_BITS");
					case COMPARE_ALL_BITS:
						MVARLNG("COMPARE_ALL_BITS");
					case OR_INT_VAR:
						VARLNG("OR");
					case OR_INT_VARS:
						VARLNG("OR");
					case NOT_INT_VAR:
						VARLNG("NOT");
					case NOT_INT_VARS:
						VARVAR("NOT");
					case XOR_INT_VAR:
						VARLNG("XOR");
					case XOR_INT_VARS:
						VARVAR("XOR");
					case RANDOM_INT_VAR:
						MVARLNG("RANDOM");
					case TIME_INT_VAR:
						VAR("TIME");
					case DATE_STR_TO_INT:
						VARVAR("DATE_INT");
					case STRLEN_INT_VAR:
						VARVAR("STRLEN");
					case CRC16_TO_INT:
						VARVAR("CRC16");
					case CRC32_TO_INT:
						VARVAR("CRC32");
					case FLENGTH_TO_INT:
						VARVAR("GET_FILE_LENGTH");
					case CHARVAL_TO_INT:
						VARVAR("CHARVAL");
					case GETNUM_VAR:
						MVARSHT("GETNUM");
					case GETSTR_VAR:
						MVARUCH("GETSTR");
					case GETNAME_VAR:
						MVARUCH("GETNAME");
					case GETSTRUPR_VAR:
						MVARUCH("GETSTRUPR");
					case GETLINE_VAR:
						MVARUCH("GETLINE");
					case SHIFT_STR_VAR:
						MVARUCH("SHIFT_STR");
					case GETSTR_MODE:
						MVARUCHLNG("GETSTR");
					case TRUNCSP_STR_VAR:
						VAR("TRUNCSP");
					case CHKFILE_VAR:
						VAR("CHKFILE");
					case PRINTFILE_VAR_MODE:
						MVARSHT("PRINTFILE");
					case PRINTTAIL_VAR_MODE:
						MVARSHT("PRINTTAIL");
					case CHKSUM_TO_INT:
						VARVAR("CHKSUM");
					case STRIP_CTRL_STR_VAR:
						VAR("STRIP_CTRL");
					case SEND_FILE_VIA:
						CHSTR("SEND_FILE_VIA");
					case SEND_FILE_VIA_VAR:
						CHVAR("SEND_FILE_VIA");
					case FTIME_TO_INT:
						VARVAR("GET_FILE_TIME");
					case RECEIVE_FILE_VIA:
						CHSTR("RECEIVE_FILE_VIA");
					case RECEIVE_FILE_VIA_VAR:
						CHVAR("RECEIVE_FILE_VIA");
					case TELNET_GATE_STR:				/* TELNET_GATE reverses argument order */
						WRITE_NAME("TELNET_GATE");
						fread(&lng,4,1,bin);
						fputc('"',src);
						while(fread(&ch,1,1,bin)==1) {
							if(ch==0)
								break;
							if(ch<' ' || ch > 126 || ch == '"') {
								fprintf(src, "\\%03d", ch);
							}
							else
								fputc(ch, src);
						}
						fputc('"',src);
						fputc(' ',src);
						if(usevar) {
							fprintf(src,"%s ",getvar(var));
							usevar=FALSE;
						} else {
							fprintf(src,"%ld ",lng);
						}
						eol(src);
						break;
					case TELNET_GATE_VAR:				/* TELNET_GATE reverses argument order */
						WRITE_NAME("TELNET_GATE");
						fread(&lng,4,1,bin);
						fread(&lng2, 1, 4, bin);
						fprintf(src,"%s ",getvar(lng2));
						if(usevar) {
							fprintf(src,"%s ",getvar(var));
							usevar=FALSE;
						} else {
							fprintf(src,"%ld ",lng);
						}
						eol(src);
						break;
					case COPY_FIRST_CHAR:
						VARVAR("COPY_FIRST_CHAR");
					case COMPARE_FIRST_CHAR:
						VARCH("COMPARE_FIRST_CHAR");
					case COPY_CHAR:
						VAR("COPY_CHAR");
					case SHIFT_TO_FIRST_CHAR:
						MVARUCH("SHIFT_TO_FIRST_CHAR");
					case SHIFT_TO_LAST_CHAR:
						MVARUCH("SHIFT_TO_LAST_CHAR");
					case MATCHUSER:
						VARVAR("MATCHUSER");
					default:
						printf("ERROR!  Unknown variable-length instruction: %02x%02X\n",CS_VAR_INSTRUCTION,uch);
						printf("Cannot continue.\n");
						return;
				}
				break;
			case CS_IF_TRUE:
				NONE("IF_TRUE");
			case CS_IF_FALSE:
				NONE("IF_FALSE");
			case CS_ELSE:
				NONE("ELSE");
			case CS_ENDIF:
				NONE("END_IF");
			case CS_CMD_HOME:
				NONE("CMD_HOME");
			case CS_CMD_POP:
				NONE("CMD_POP");
			case CS_END_CMD:
				NONE("END_CMD");
			case CS_RETURN:
				NONE("RETURN");
			case CS_GETKEY:
				NONE("GETKEY");
			case CS_GETKEYE:
				NONE("GETKEYE");
			case CS_UNGETKEY:
				NONE("UNGETKEY");
			case CS_UNGETSTR:
				NONE("UNGETSTR");
			case CS_PRINTKEY:
				NONE("PRINTKEY");
			case CS_PRINTSTR:
				NONE("PRINTSTR");
			case CS_HANGUP:
				NONE("HANGUP");
			case CS_SYNC:
				NONE("SYNC");
			case CS_ASYNC:
				NONE("ASYNC");
			case CS_CHKSYSPASS:
				NONE("CHKSYSPASS");
			case CS_LOGKEY:
				NONE("LOGKEY");
			case CS_LOGKEY_COMMA:
				NONE("LOGKEY_COMMA");
			case CS_LOGSTR:
				NONE("LOGSTR");
			case CS_CLS:
				NONE("CLS");
			case CS_CRLF:
				NONE("CRLF");
			case CS_PAUSE:
				NONE("PAUSE");
			case CS_PAUSE_RESET:
				NONE("PAUSE_RESET");
			case CS_GETLINES:
				NONE("GETLINES");
			case CS_GETFILESPEC:
				NONE("GETFILESPEC");
			case CS_FINDUSER:
				NONE("FINDUSER");
			case CS_CLEAR_ABORT:
				NONE("CLEAR_ABORT");
			case CS_SELECT_SHELL:
				NONE("SELECT_SHELL");
			case CS_SET_SHELL:
				NONE("SET_SHELL");
			case CS_SELECT_EDITOR:
				NONE("SELECT_EDITOR");
			case CS_SET_EDITOR:
				NONE("SET_EDITOR");
			case CS_INKEY:
				NONE("INKEY");
			case CS_INCHAR:
				NONE("INCHAR");
			case CS_GETTIMELEFT:
				NONE("GETTIMELEFT");
			case CS_SAVELINE:
				NONE("SAVELINE");
			case CS_RESTORELINE:
				NONE("RESTORELINE");
			case CS_IF_GREATER:
				NONE("IF_GREATER");
			case CS_IF_GREATER_OR_EQUAL:
				NONE("IF_GREATER_OR_EQUAL");
			case CS_IF_LESS:
				NONE("IF_LESS");
			case CS_IF_LESS_OR_EQUAL:
				NONE("IF_LESS_OR_EQUAL");
			case CS_DEFAULT:
				NONE("DEFAULT");
			case CS_END_SWITCH:
				NONE("END_SWITCH");
			case CS_END_CASE:
				NONE("END_CASE");
			case CS_PUT_NODE:
				NONE("PUT_NODE");
			case CS_GETCHAR:
				NONE("GETCHAR");
			case CS_ONE_MORE_BYTE:
				fread(&uch,1,1,bin);
				switch(uch) {
					case CS_ONLINE:
						NONE("ONLINE");
					case CS_OFFLINE:
						NONE("OFFLINE");
					case CS_NEWUSER:
						NONE("NEWUSER");
					case CS_LOGON:
						NONE("LOGON");
					case CS_LOGOUT:
						NONE("LOGOUT");
					case CS_EXIT:
						NONE("EXIT");
					case CS_LOOP_BEGIN:
						NONE("LOOP_BEGIN");
					case CS_CONTINUE_LOOP:
						NONE("CONTINUE_LOOP");
					case CS_BREAK_LOOP:
						NONE("BREAK_LOOP");
					case CS_END_LOOP:
						NONE("END_LOOP");
					default:
						printf("ERROR!  Unknown one-byte instruction: %02x%02X\n",CS_ONE_MORE_BYTE,uch);
				}
				break;
			case CS_CMDKEY:
				KEY("CMDKEY");
			case CS_NODE_ACTION:
				MUCH("NODE_ACTION");
			case CS_GETSTR:
				MUCH("GETSTR");
			case CS_GETNAME:
				MUCH("GETNAME");
			case CS_GETSTRUPR:
				MUCH("GETSTRUPR");
			case CS_SHIFT_STR:
				MUCH("SHIFT_STR");
			case CS_COMPARE_KEY:
				KEY("COMPARE_KEY");
			case CS_SETLOGIC:
				LGC("SETLOGIC");
			case CS_SET_USER_LEVEL:
				MUCH("SET_USER_LEVEL");
			case CS_SET_USER_STRING:
				MUCH("SET_USER_STRING");
			case CS_GETLINE:
				MUCH("GETLINE");
			case CS_NODE_STATUS:
				MUCH("NODE_STATUS");
			case CS_CMDCHAR:
				CH("CMDCHAR");
			case CS_COMPARE_CHAR:
				CH("COMPARE_CHAR");
			case CS_MULTINODE_CHAT:
				MUCH("MULTINODE_CHAT");
			case CS_GOTO:
				fread(&ush,2,1,bin);
				fprintf(src,"GOTO label_%04x",ush);
				eol(src);
				break;
			case CS_CALL:
				fread(&ush,2,1,bin);
				fprintf(src,"CALL label_%04x",ush);
				eol(src);
				break;
			case CS_TOGGLE_NODE_MISC:
				MSHT("TOGGLE_NODE_MISC");
			case CS_ADJUST_USER_CREDITS:
				MSHT("ADJUST_USER_CREDITS");
			case CS_TOGGLE_USER_FLAG:
				CHCH("TOGGLE_USER_FLAG");
			case CS_GETNUM:
				MSHT("GETNUM");
			case CS_COMPARE_NODE_MISC:
				MSHT("COMPARE_MODE_MISC");
			case CS_MSWAIT:
				MSHT("MSWAIT");
			case CS_ADJUST_USER_MINUTES:
				MSHT("ADJUST_USER_MINUTES");
			case CS_REVERT_TEXT:
				MSHT("REVERT_TEXT");
			case CS_THREE_MORE_BYTES:
				printf("ERROR!  I dont know anything about CS_THREE_MORE_BYTES.\n");
				printf("Cannot continue.\n");
				return;
			case CS_MENU:
				STR("MENU");
			case CS_PRINT:
				STR("PRINT");
			case CS_PRINT_LOCAL:
				STR("PRINT_LOCAL");
			case CS_PRINT_REMOTE:
				STR("PRINT_REMOTE");
			case CS_PRINTFILE:
				STR("PRINTFILE");
			case CS_PRINTFILE_LOCAL:
				STR("PRINTFILE_LOCAL");
			case CS_PRINTFILE_REMOTE:
				STR("PRINTFILE_REMOTE");
			case CS_YES_NO:
				STR("YES_NO");
			case CS_NO_YES:
				STR("NO_YES");
			case CS_COMPARE_STR:
				STR("COMPARE_STR");
			case CS_COMPARE_WORD:
				STR("COMPARE_WORD");
			case CS_EXEC:
				STR("EXEC");
			case CS_EXEC_INT:
				STR("EXEC_INT");
			case CS_EXEC_BIN:
				STR("EXEC_BIN");
			case CS_EXEC_XTRN:
				STR("EXEC_XTRN");
			case CS_GETCMD:
				STR("GETCMD");
			case CS_LOG:
				STR("LOG");
			case CS_MNEMONICS:
				STR("MNEMONICS");
			case CS_SETSTR:
				STR("SETSTR");
			case CS_SET_MENU_DIR:
				STR("SET_MENU_DIR");
			case CS_SET_MENU_FILE:
				STR("SET_MENU_FILE");
			case CS_CMDSTR:
				STR("CMDSTR");
			case CS_CHKFILE:
				STR("CHKFILE");
			case CS_GET_TEMPLATE:
				STR("GET_TEMPLATE");
			case CS_TRASHCAN:
				STR("TRASHCAN");
			case CS_CREATE_SIF:
				STR("CREATE_SIF");
			case CS_READ_SIF:
				STR("READ_SIF");
			case CS_CMDKEYS:
				STR("CMDKEYS");
			case CS_COMPARE_KEYS:
				STR("COMPARE_KEYS");
			case CS_STR_FUNCTION:
				fread(&uch,1,1,bin);
				switch(uch) {
					case CS_LOGIN:
						STR("LOGIN");
					case CS_LOAD_TEXT:
						STR("LOAD_TEXT");
					default:
						printf("ERROR!  Unknown string instruction: %02x%02X\n",CS_STR_FUNCTION,uch);
						ch=0;
						while(ch) fread(&ch,1,1,bin);
				}
				break;
			case CS_COMPARE_ARS:
				fread(&uch,1,1,bin);
				p=(char *)malloc(uch);
				fread(p,uch,1,bin);
				fprintf(src,"COMPARE_ARS %s\n",decompile_ars(p,uch));
				free(p);
				break;
			case CS_TOGGLE_USER_MISC:
				MLNG("TOGGLE_USER_MISC");
			case CS_COMPARE_USER_MISC:
				MLNG("COMPARE_USER_MISC");
			case CS_REPLACE_TEXT:
				MSHTSTR("REPLACE_TEXT");
			case CS_TOGGLE_USER_CHAT:
				MLNG("TOGGLE_USER_CHAT");
			case CS_COMPARE_USER_CHAT:
				MLNG("COMPARE_USER_CHAT");
			case CS_TOGGLE_USER_QWK:
				MLNG("TOGGLE_USER_QWK");
			case CS_COMPARE_USER_QWK:
				MLNG("COMPARE_USER_QWK");
			case CS_SWITCH:
				VAR("SWITCH");
			case CS_CASE:
				LNG("CASE");
			case CS_NET_FUNCTION:
				fread(&uch,1,1,bin);
				switch(uch) {
					case CS_SOCKET_OPEN:
						VAR("SOCKET_OPEN");
					case CS_SOCKET_CLOSE:
						VAR("SOCKET_CLOSE");
					case CS_SOCKET_CONNECT:
						MVARVARUST("SOCKET_CONNECT");
					case CS_SOCKET_ACCEPT:
						VAR("SOCKET_ACCEPT");
					case CS_SOCKET_NREAD:
						VARVAR("SOCKET_NREAD");
					case CS_SOCKET_PEEK:
						MVARVARUST("SOCKET_PEEK");
					case CS_SOCKET_READ:
						MVARVARUST("SOCKET_READ");
					case CS_SOCKET_WRITE:
						VARVAR("SOCKET_WRITE");
					case CS_SOCKET_CHECK:
						VAR("SOCKET_CHECK");
					case CS_SOCKET_READLINE:
						MVARVARUST("SOCKET_READLINE");
					case CS_FTP_LOGIN:
						VARVARVAR("FTP_LOGIN");
					case CS_FTP_LOGOUT:
						VAR("FTP_LOGOUT");
					case CS_FTP_PWD:
						VAR("FTP_PWD");
					case CS_FTP_CWD:
						VARVAR("FTP_CWD");
					case CS_FTP_DIR:
						VARVAR("FTP_DIR");
					case CS_FTP_PUT:
						VARVARVAR("FTP_PUT");
					case CS_FTP_GET:
						VARVARVAR("FTP_GET");
					case CS_FTP_RENAME:
						VARVARVAR("FTP_RENAME");
					case CS_FTP_DELETE:
						VARVAR("FTP_DELETE");
					default:
						printf("ERROR!  Unknown net function %02x%02X\n",CS_NET_FUNCTION,uch);
						printf("Cannot continue.\n");
						return;
				}
				break;
			case CS_FIO_FUNCTION:
				fread(&uch,1,1,bin);
				switch(uch) {
					case FIO_OPEN:
						MVARUSTSTR("FOPEN");
					case FIO_CLOSE:
						VAR("FCLOSE");
					case FIO_READ:
						VARVARUST("FREAD");
					case FIO_READ_VAR:
						VARVARVAR("FREAD");
					case FIO_WRITE:
						VARVARUST("FWRITE");
					case FIO_WRITE_VAR:
						VARVARVAR("FWRITE");
					case FIO_GET_LENGTH:
						VARVAR("FGET_LENGTH");
					case FIO_EOF:
						VAR("FEOF");
					case FIO_GET_POS:
						VARVAR("FGET_POS");
					case FIO_SEEK:
						fprintf(src,"FSEEK ");
						write_var(bin,src);
						write_lng(bin,src);
						fread(&ush,2,1,bin);
						if(ush==SEEK_CUR)
							fputs("CUR ",src);
						else if(ush==SEEK_END)
							fputs("END ",src);
						else
							fprintf(src, "%d ",ush);
						eol(src);
						break;
					case FIO_SEEK_VAR:
						fprintf(src,"FSEEK ");
						write_var(bin,src);
						write_var(bin,src);
						fread(&ush,2,1,bin);
						if(ush==SEEK_CUR)
							fputs("CUR ",src);
						else if(ush==SEEK_END)
							fputs("END ",src);
						else
							fprintf(src, "%d ",ush);
						eol(src);
						break;
					case FIO_LOCK:
						VARLNG("FLOCK");
					case FIO_LOCK_VAR:
						VARVAR("FLOCK");
					case FIO_UNLOCK:
						VARLNG("FUNLOCK");
					case FIO_UNLOCK_VAR:
						VARVAR("FUNLOCK");
					case FIO_SET_LENGTH:
						VARLNG("FSET_LENGTH");
					case FIO_SET_LENGTH_VAR:
						VARVAR("FSET_LENGTH");
					case FIO_PRINTF:
						fprintf(src,"FPRINTF ");
						write_var(bin,src);
						write_cstr(bin,src);
						fread(&uch, 1, 1, bin);
						for(i=0; i<uch; i++) {
							write_var(bin,src);
						}
						eol(src);
						break;
					case FIO_SET_ETX:
						MUCH("FSET_ETX");
					case FIO_GET_TIME:
						VARVAR("FGET_TIME");
					case FIO_SET_TIME:
						VARVAR("FSET_TIME");
					case FIO_OPEN_VAR:
						MVARUSTVAR("FOPEN");
					case FIO_READ_LINE:
						VARVAR("FREAD_LINE");
					case FIO_FLUSH:
						VAR("FFLUSH");
					case REMOVE_FILE:
						VAR("REMOVE_FILE");
					case RENAME_FILE:
						VARVAR("RENAME_FILE");
					case COPY_FILE:
						VARVAR("COPY_FILE");
					case MOVE_FILE:
						VARVAR("MOVE_FILE");
					case GET_FILE_ATTRIB:
						VARVAR("GET_FILE_ATTRIB");
					case SET_FILE_ATTRIB:
						VARVAR("SET_FILE_ATTRIB");
					case MAKE_DIR:
						VAR("MAKE_DIR");
					case CHANGE_DIR:
						VAR("CHANGE_DIR");
					case REMOVE_DIR:
						VAR("REMOVE_DIR");
					case OPEN_DIR:
						VARVAR("OPEN_DIR");
					case READ_DIR:
						VARVAR("READ_DIR");
					case REWIND_DIR:
						VAR("REWIND_DIR");
					case CLOSE_DIR:
						VAR("CLOSE_DIR");
					default:
						printf("ERROR!  Unknown file function %02x%02X\n",CS_FIO_FUNCTION,uch);
						printf("Cannot continue.\n");
						return;
				}
				break;
			case CS_MAIL_READ:
				NONE("MAIL_READ");
			case CS_MAIL_READ_SENT:
				NONE("MAIL_READ_SENT");
			case CS_MAIL_READ_ALL:
				NONE("MAIL_READ_ALL");
			case CS_MAIL_SEND:
				NONE("MAIL_SEND");
			case CS_MAIL_SEND_BULK:
				NONE("MAIL_SEND_BULK");
			case CS_MAIL_SEND_FILE:
				NONE("MAIL_SEND_FILE");
			case CS_MAIL_SEND_FEEDBACK:
				NONE("MAIL_SEND_FEEDBACK");
			case CS_MAIL_SEND_NETMAIL:
				NONE("MAIL_SEND_NETMAIL");
			case CS_MAIL_SEND_NETFILE:
				NONE("MAIL_SEND_NETFILE");
			case CS_LOGOFF:
				NONE("LOGOFF");
    		case CS_LOGOFF_FAST:
				NONE("LOGOFF_FAST");;
			case CS_AUTO_MESSAGE:
				NONE("AUTO_MESSAGE");
			case CS_MSG_SET_AREA:
				NONE("MSG_SET_AREA");
			case CS_MSG_SELECT_AREA:
				NONE("MSG_SELECT_AREA");
			case CS_MSG_SHOW_GROUPS:
				NONE("MSG_SHOW_GROUPS");
    		case CS_MSG_SHOW_SUBBOARDS:
				NONE("MSG_SHOW_SUBBOARDS");;
			case CS_MSG_GROUP_UP:
				NONE("MSG_GROUP_UP");
    		case CS_MSG_GROUP_DOWN:
				NONE("MSG_GROUP_DOWN");;
    		case CS_MSG_SUBBOARD_UP:
				NONE("MSG_SUBBOARD_UP");;
    		case CS_MSG_SUBBOARD_DOWN:
				NONE("MSG_SUBBOARD_DOWN");
			case CS_MSG_GET_SUB_NUM:
				NONE("MSG_GET_SUB_NUM");
			case CS_MSG_GET_GRP_NUM:
				NONE("MSG_GET_GRP_NUM");
			case CS_MSG_READ:
				NONE("MSG_READ");
			case CS_MSG_POST:
				NONE("MSG_POST");
			case CS_MSG_QWK:
				NONE("MSG_QWK");
			case CS_MSG_PTRS_CFG:
				NONE("MSG_PTRS_CFG");
			case CS_MSG_PTRS_REINIT:
				NONE("MSG_PTRS_REINIT");
			case CS_MSG_NEW_SCAN_CFG:
				NONE("MSG_NEW_SCAN_CFG");
			case CS_MSG_NEW_SCAN:
				NONE("MSG_NEW_SCAN");
			case CS_MSG_NEW_SCAN_ALL:
				NONE("MSG_NEW_SCAN_ALL");
			case CS_MSG_CONT_SCAN:
				NONE("MSG_CONT_SCAN");
			case CS_MSG_CONT_SCAN_ALL:
				NONE("MSG_CONT_SCAN_ALL");
			case CS_MSG_BROWSE_SCAN :
				NONE("MSG_BROWSE_SCAN ");
			case CS_MSG_BROWSE_SCAN_ALL:
				NONE("MSG_BROWSE_SCAN_ALL");
			case CS_MSG_FIND_TEXT:
				NONE("MSG_FIND_TEXT");
			case CS_MSG_FIND_TEXT_ALL:
				NONE("MSG_FIND_TEXT_ALL");
			case CS_MSG_YOUR_SCAN_CFG:
				NONE("MSG_YOUR_SCAN_CFG");
			case CS_MSG_YOUR_SCAN:
				NONE("MSG_YOUR_SCAN");
			case CS_MSG_YOUR_SCAN_ALL:
				NONE("MSG_YOUR_SCAN_ALL");
			case CS_MSG_NEW_SCAN_SUB:
				NONE("MSG_NEW_SCAN_SUB");
			case CS_MSG_SET_GROUP:
				NONE("MSG_SET_GROUP");
			case CS_MSG_UNUSED4:
				NONE("MSG_UNUSED4");
			case CS_MSG_UNUSED3:
				NONE("MSG_UNUSED3");
			case CS_MSG_UNUSED2:
				NONE("MSG_UNUSED2");
			case CS_MSG_UNUSED1:
				NONE("MSG_UNUSED1");
			case CS_FILE_SET_AREA:
				NONE("FILE_SET_AREA");
			case CS_FILE_SELECT_AREA:
				NONE("FILE_SELECT_AREA");
			case CS_FILE_SHOW_LIBRARIES:
				NONE("FILE_SHOW_LIBRARIES");
			case CS_FILE_SHOW_DIRECTORIES:
				NONE("FILE_SHOW_DIRECTORIES");
			case CS_FILE_LIBRARY_UP:
				NONE("FILE_LIBRARY_UP");
			case CS_FILE_LIBRARY_DOWN:
				NONE("FILE_LIBRARY_DOWN");
			case CS_FILE_DIRECTORY_UP:
				NONE("FILE_DIRECTORY_UP");
			case CS_FILE_DIRECTORY_DOWN:
				NONE("FILE_DIRECTORY_DOWN");
			case CS_FILE_GET_DIR_NUM:
				NONE("FILE_GET_DIR_NUM");
			case CS_FILE_GET_LIB_NUM:
				NONE("FILE_GET_LIB_NUM");
			case CS_FILE_LIST:
				NONE("FILE_LIST");
			case CS_FILE_LIST_EXTENDED:
				NONE("FILE_LIST_EXTENDED");
			case CS_FILE_VIEW:
				NONE("FILE_VIEW");
			case CS_FILE_UPLOAD:
				NONE("FILE_UPLOAD");
			case CS_FILE_UPLOAD_USER:
				NONE("FILE_UPLOAD_USER");
			case CS_FILE_UPLOAD_SYSOP:
				NONE("FILE_UPLOAD_SYSOP");
			case CS_FILE_DOWNLOAD:
				NONE("FILE_DOWNLOAD");
			case CS_FILE_DOWNLOAD_USER:
				NONE("FILE_DOWNLOAD_USER");
			case CS_FILE_DOWNLOAD_BATCH:
				NONE("FILE_DOWNLOAD_BATCH");
			case CS_FILE_REMOVE :
				NONE("FILE_REMOVE ");
			case CS_FILE_BATCH_SECTION:
				NONE("FILE_BATCH_SECTION");
			case CS_FILE_TEMP_SECTION:
				NONE("FILE_TEMP_SECTION");
			case CS_FILE_NEW_SCAN_CFG:
				NONE("FILE_NEW_SCAN_CFG");
			case CS_FILE_NEW_SCAN:
				NONE("FILE_NEW_SCAN");
			case CS_FILE_NEW_SCAN_ALL:
				NONE("FILE_NEW_SCAN_ALL");
			case CS_FILE_FIND_TEXT:
				NONE("FILE_FIND_TEXT");
			case CS_FILE_FIND_TEXT_ALL:
				NONE("FILE_FIND_TEXT_ALL");
			case CS_FILE_FIND_NAME:
				NONE("FILE_FIND_NAME");
			case CS_FILE_FIND_NAME_ALL:
				NONE("FILE_FIND_NAME_ALL");
			case CS_FILE_PTRS_CFG:
				NONE("FILE_PTRS_CFG");
			case CS_FILE_BATCH_ADD:
				NONE("FILE_BATCH_ADD");
			case CS_FILE_BATCH_CLEAR:
				NONE("FILE_BATCH_CLEAR");
			case CS_FILE_SET_LIBRARY:
				NONE("FILE_SET_LIBRARY");
			case CS_FILE_SEND:
				NONE("FILE_SEND");
			case CS_FILE_BATCH_ADD_LIST:
				NONE("FILE_BATCH_ADD_LIST");
			case CS_FILE_RECEIVE:
				NONE("FILE_RECEIVE");
			case CS_NODELIST_ALL:
				NONE("NODELIST_ALL");
			case CS_NODELIST_USERS:
				NONE("NODELIST_USERS");
			case CS_CHAT_SECTION:
				NONE("CHAT_SECTION");
			case CS_USER_DEFAULTS:
				NONE("USER_DEFAULTS");
			case CS_USER_EDIT:
				NONE("USER_EDIT");
			case CS_TEXT_FILE_SECTION:
				NONE("TEXT_FILE_SECTION");
			case CS_INFO_SYSTEM:
				NONE("INFO_SYSTEM");
			case CS_INFO_SUBBOARD:
				NONE("INFO_SUBBOARD");
			case CS_INFO_DIRECTORY:
				NONE("INFO_DIRECTORY");
			case CS_INFO_USER:
				NONE("INFO_USER");
			case CS_INFO_VERSION:
				NONE("INFO_VERSION");
			case CS_INFO_XFER_POLICY:
				NONE("INFO_XFER_POLICY");
			case CS_XTRN_EXEC:
				NONE("XTRN_EXEC");
			case CS_XTRN_SECTION:
				NONE("XTRN_SECTION");
			case CS_USERLIST_SUB:
				NONE("USERLIST_SUB");
			case CS_USERLIST_DIR:
				NONE("USERLIST_DIR");
			case CS_USERLIST_ALL:
				NONE("USERLIST_ALL");
    		case CS_USERLIST_LOGONS:
				NONE("USERLIST_LOGONS");;
			case CS_PAGE_SYSOP:
				NONE("PAGE_SYSOP");
    		case CS_PRIVATE_CHAT:
				NONE("PRIVATE_CHAT");;
			case CS_PRIVATE_MESSAGE:
    			NONE("PRIVATE_MESSAGE");;
			case CS_MINUTE_BANK:
				NONE("MINUTE_BANK");
			case CS_GURU_LOG:
				NONE("GURU_LOG");
			case CS_ERROR_LOG:
				NONE("ERROR_LOG");
			case CS_SYSTEM_LOG:
				NONE("SYSTEM_LOG");
			case CS_SYSTEM_YLOG:
				NONE("SYSTEM_YLOG");
			case CS_SYSTEM_STATS:
				NONE("SYSTEM_STATS");
			case CS_NODE_STATS:
				NONE("NODE_STATS");
			case CS_SHOW_MEM:
				NONE("SHOW_MEM");
			case CS_CHANGE_USER:
				NONE("CHANGE_USER");
			case CS_ANSI_CAPTURE:
				NONE("ANSI_CAPTURE");
			case CS_LIST_TEXT_FILE:
				NONE("LIST_TEXT_FILE");
			case CS_EDIT_TEXT_FILE:
				NONE("EDIT_TEXT_FILE");
			case CS_FILE_SET_ALT_PATH:
				NONE("FILE_SET_ALT_PATH");
			case CS_FILE_RESORT_DIRECTORY:
				NONE("FILE_RESORT_DIRECTORY");
			case CS_FILE_GET:
				NONE("FILE_GET");
			case CS_FILE_PUT:
				NONE("FILE_PUT");
			case CS_FILE_UPLOAD_BULK:
				NONE("FILE_UPLOAD_BULK");
			case CS_FILE_FIND_OLD:
				NONE("FILE_FIND_OLD");
			case CS_FILE_FIND_OPEN:
				NONE("FILE_FIND_OPEN");
			case CS_FILE_FIND_OFFLINE:
				NONE("FILE_FIND_OFFLINE");
			case CS_FILE_FIND_OLD_UPLOADS:
				NONE("FILE_FIND_OLD_UPLOADS");
			case CS_INC_MAIN_CMDS:
				NONE("INC_MAIN_CMDS");
			case CS_INC_FILE_CMDS:
				NONE("INC_FILE_CMDS");
			case CS_PRINTFILE_STR:
				NONE("PRINTFILE_STR");
			case CS_PAGE_GURU:
				NONE("PAGE_GURU");
			case CS_SPY:
				NONE("SPY");
			default:
				printf("ERROR!  Unknown instruction: %02X\n",uch);
				printf("Cannot continue.\n");
				return;
		}
	}
}

int main(int argc, char **argv)
{
	int		f;
	FILE	*bin;
	FILE	*src;
	char 	newname[MAX_PATH+1];
	char	*p;

	for(f=1; f<argc; f++) {
		bin=fopen(argv[f],"rb");
		if(bin==NULL)
			perror(argv[f]);
		else {
			SAFECOPY(newname, argv[f]);
			p=strrchr(newname, '.');
			if(p==NULL)
				p=strchr(newname,0);
			strcpy(p,".decompiled");
			src=fopen(newname,"w");
			if(src == NULL) 
				perror(newname);
			else {
				printf("Decompiling %s to %s\n",argv[f],newname);
				fputs("!include sbbsdefs.inc\n",src);
				fputs("!include file_io.inc\n",src);
				fputs("!include dir_attr.inc\n\n",src);
				decompile(bin, src);
				fclose(src);
			}
			fclose(bin);
		}
	}

	return(0);
}

