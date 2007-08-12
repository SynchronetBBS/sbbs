/* $Id$ */

#include <stdio.h>
#include <string.h>

#include <stdlib.h>

#include "dirwrap.h"

#include "crc32.h"

#include "cmdshell.h"
#include "ars_defs.h"

int indent=0;
int indenteol=0;

/* table of variables for use by bsearch() */

struct var_table_t {
	unsigned long crc;
	char *var;
} var_table[] = {
	{ 0x00000000, "STR" }, { 0x01837ca9, "r2" }, { 0x023e4b48, "fattr" },
	{ 0x02408dc5, "_SYS_TIMEZONE" }, { 0x038a840b, "k1" },
	{ 0x0400f83f, "e2" }, { 0x04807a11, "_USERON.LOGONS" },
	{ 0x057e4cd4, "_TIMELEFT" }, { 0x07954570, "_USERON.XEDIT" },
	{ 0x08f65a2a, "_USERON.MODEM" }, { 0x094cc42c, "_USERON.ROWS" },
	{ 0x098bdfcb, "_USERON.TLAST" }, { 0x0bf3b87a, "w3" },
	{ 0x0c1a1011, "_USERON.FLAGS2" }, { 0x0c8dcf3b, "_USERON.FBACKS" },
	{ 0x0e42002a, "_EXPIRED_EXEMPT" }, { 0x0ea515b1, "_USERON.LASTON" },
	{ 0x10bfc6e9, "_EXPIRED_FLAGS2" }, { 0x11c83294, "_SYS_PSNUM" },
	{ 0x12e7d6d2, "_USERON.EXEMPT" }, { 0x12e8893b, "v3" },
	{ 0x15755030, "_COMSPEC" }, { 0x167ea426, "pos" },
	{ 0x16e2585f, "_CLIENT_SOCKET" }, { 0x176b0dad, "a3" },
	{ 0x18984de8, "s2" }, { 0x19982a4f, "log" }, { 0x1a91b54a, "j1" },
	{ 0x1c4455ee, "_DTE_RATE" }, { 0x1d1bc97e, "d2" },
	{ 0x1e5052a7, "_MAX_MINUTES" }, { 0x1ef214ef, "_USERON.FIRSTON" },
	{ 0x2039a29f, "_USERON.POSTS" }, { 0x205ace36, "_AUTOTERM" },
	{ 0x2060efc3, "s" }, { 0x20cb6325, "_NEW_REST" },
	{ 0x20deebb9, "t3" }, { 0x2105d2b9, "_SOCKET_ERROR" },
	{ 0x222d1c26, "ss" }, { 0x22f01d71, "_NEW_FLAGS3" },
	{ 0x23edbcbc, "nodes_wfc" }, { 0x255d6f2f, "c3" },
	{ 0x270d2bda, "w" }, { 0x274e4d59, "nm" }, { 0x27a9fc7c, "attempt" },
	{ 0x28a7d7c8, "h1" }, { 0x2aa89801, "_NODE_SWAP" },
	{ 0x2aae2f6a, "q2" }, { 0x2aaf9bd3, "_USERON.EXPIRE" },
	{ 0x2b3c257f, "_CUR_CPS" }, { 0x2c0c389a, "dir" },
	{ 0x2f2dabfc, "f2" }, { 0x30ffd1ff, "cmdline" },
	{ 0x31178ba2, "_NEW_CDT" }, { 0x3178f9d6, "_USERON.COMMENT" },
	{ 0x31bce689, "i1" }, { 0x328ed476, "_USERON.LEVEL" },
	{ 0x32ebcea0, "attr" }, { 0x330c7795, "k" }, { 0x33b51e2b, "p2" },
	{ 0x3461b38c, "o" }, { 0x357b23ee, "total" },
	{ 0x3587ad08, "cursor" }, { 0x36369abd, "g2" },
	{ 0x381d3c2a, "_SYS_STATUS" }, { 0x396b7167, "_NETMAIL_COST" },
	{ 0x39c5daf8, "u3" }, { 0x3a37c26b, "_NODE_EXT" },
	{ 0x3aba3bbe, "g" }, { 0x3c465e6e, "b3" }, { 0x3d39d372, "argc" },
	{ 0x3dc166bb, "path" }, { 0x3dd7ffa7, "c" },
	{ 0x41239e21, "_CONNECTION" }, { 0x4131aa2b, "g3" },
	{ 0x430178ec, "_UQ" }, { 0x4366831a, "n" }, { 0x440b4703, "j" },
	{ 0x44b22ebd, "p3" }, { 0x455cb929, "_FTP_MODE" },
	{ 0x4569c62e, "_EXPIRED_REST" }, { 0x46cd817e, "bytes" },
	{ 0x490873f1, "_USERON.ALIAS" }, { 0x4a16a4ea, "filepos" },
	{ 0x4a9f3955, "_USERON.EMAILS" }, { 0x4ad0cf31, "b" },
	{ 0x4b0aeb24, "_NEW_EXEMPT" }, { 0x4b416ef8, "b2" },
	{ 0x4b8a0705, "connect" }, { 0x4ccb12cc, "l1" },
	{ 0x4db200d2, "_LEECH_PCT" }, { 0x4dbd0b28, "f" },
	{ 0x4ec2ea6e, "u2" }, { 0x4ee1ff3a, "_USERON.LOCATION" },
	{ 0x4f02623a, "_NODE_MINBPS" }, { 0x500a1b4c, "v" },
	{ 0x5053a71b, "z1" }, { 0x50e43799, "argv" }, { 0x525a5fb9, "c2" },
	{ 0x52996eab, "_USERON.TTODAY" }, { 0x55d0238d, "m1" },
	{ 0x55f72de7, "_NEW_FLAGS2" }, { 0x5767df55, "r" },
	{ 0x57d9db2f, "t2" }, { 0x582a9b6a, "f3" }, { 0x5863165a, "fname" },
	{ 0x590175f1, "time" }, { 0x59bc5767, "z" },
	{ 0x5a22d4bd, "_NODEFILE" }, { 0x5aaccfc5, "_ERRNO" },
	{ 0x5b0d0c54, "_USERON.NS_TIME" }, { 0x5be35885, "guest" },
	{ 0x5c1c1500, "_TOS" }, { 0x5da91ffc, "q3" },
	{ 0x5de44e8b, "_USERON.NAME" }, { 0x5e049062, "_QUESTION" },
	{ 0x5e914a47, "nodes_inuse" }, { 0x5eeaff21, "_NETMAIL_MISC" },
	{ 0x606c3d3b, "a2" }, { 0x613b690e, "_ROWS" },
	{ 0x6178e75f, "handle" }, { 0x61be0d36, "_USERON.QWK" },
	{ 0x6265c599, "x1" }, { 0x6392dc62, "_SYS_AUTODEL" },
	{ 0x640f7f81, "addr" }, { 0x65efb9ad, "v2" },
	{ 0x665ac227, "_USERON.CHAT" }, { 0x67b8f67f, "_EXPIRED_FLAGS3" },
	{ 0x67e6410f, "o1" }, { 0x682d973f, "start" },
	{ 0x68b693b2, "name" }, { 0x698d59b4, "_SYS_NODES" },
	{ 0x69d4f019, "ftime" }, { 0x6a1cf9e8, "d3" },
	{ 0x6a9fd069, "LIMIT_REPSIZE" }, { 0x6c8e350a, "_NEW_LEVEL" },
	{ 0x6f9f7d7e, "s3" }, { 0x6fb1c46e, "_SYS_EXP_WARN" },
	{ 0x709c07da, "_NODE_MISC" }, { 0x7166336e, "hubid" },
	{ 0x717231d6, "MAX_REPSIZE" }, { 0x7307c8a9, "e3" },
	{ 0x7342a625, "_NEW_EXPIRE" }, { 0x7345219f, "_NEW_MIN" },
	{ 0x7504b078, "port" }, { 0x75dc4306, "_NEW_PROT" },
	{ 0x761cc978, "rep" }, { 0x76844c3f, "r3" },
	{ 0x78afeaf1, "_SYS_PWDAYS" }, { 0x79f47b28, "attempts" },
	{ 0x7ade6ef8, "local_file" }, { 0x7b1d2087, "_USERON.FLAGS3" },
	{ 0x7b7ef4d8, "y1" }, { 0x7c602a37, "_USERON.MISC" },
	{ 0x7c72376d, "_USERON.TIMEON" }, { 0x7cf488ec, "w2" },
	{ 0x7d0ed0d1, "_CONSOLE" }, { 0x7dd9aac0, "_USERON.LEECH" },
	{ 0x7e29c819, "_SYS_MISC" }, { 0x7efd704e, "n1" },
	{ 0x7fbf958e, "_LNCNTR" }, { 0x81911c52, "s1" },
	{ 0x82d9484e, "_INETMAIL_COST" }, { 0x82f6983b, "int" },
	{ 0x8398e4f0, "j2" }, { 0x83aa2a6a, "_LOGONTIME" },
	{ 0x83dea272, "pasv_mode" }, { 0x841298c4, "d1" },
	{ 0x86cbce1e, "qwk" }, { 0x89b69753, "_EXPIRED_FLAGS1" },
	{ 0x89c91dc8, "_USERON.PWMOD" }, { 0x89e82023, "o3" },
	{ 0x8b12ba9d, "_POSTS_READ" }, { 0x8bf8a29a, "status" },
	{ 0x8c6ba4b5, "x3" }, { 0x8e395209, "_LOGON_FBACKS" },
	{ 0x908ece53, "_USERON.NUMBER" }, { 0x90f31162, "n3" },
	{ 0x90fc82b4, "_CID" }, { 0x9154f82b, "logfile" },
	{ 0x92fb364f, "_USERON.CDT" }, { 0x94d59a7a, "_USERON.BIRTH" },
	{ 0x951341ab, "_USERON.FLAGS1" }, { 0x957095f4, "y3" },
	{ 0x965b713b, "end" }, { 0x979ef1de, "_USERON.HANDLE" },
	{ 0x97f99eef, "_ONLINE" }, { 0x988a2d13, "r1" },
	{ 0x9a13bf95, "_USERON.ETODAY" }, { 0x9a7d9cca, "_LEECH_SEC" },
	{ 0x9a83d5b1, "k2" }, { 0x9aab7a61, "len" }, { 0x9c0bdace, "date" },
	{ 0x9c5051c9, "_LOGON_POSTS" }, { 0x9d09a985, "e1" },
	{ 0x9e70e855, "_USERON.SEX" }, { 0x9e7638c7, "logonlst" },
	{ 0xa0023a2e, "_STARTUP_OPTIONS" }, { 0xa15faa1f, "mode" },
	{ 0xa1f0fcb7, "_NODE_SCRNBLANK" }, { 0xa278584f, "_NEW_MISC" },
	{ 0xa2c573e0, "l3" }, { 0xa3b36a04, "d" },
	{ 0xa4a51044, "bbs_name" }, { 0xa842c43b, "_USERON.ADDRESS" },
	{ 0xa8b5b733, "i2" }, { 0xaa05262f, "h" }, { 0xaabc4f91, "p1" },
	{ 0xabb91f93, "_USERON.DLB" }, { 0xabc4317e, "_USERON.PROT" },
	{ 0xac58736f, "_LOGON_ULS" }, { 0xac72c50b, "_USERON.TEXTRA" },
	{ 0xad1a21f0, "hash_mode" }, { 0xad68e236, "l" },
	{ 0xadae1681, "_NODE_IVT" }, { 0xae256560, "_CUR_RATE" },
	{ 0xae92d249, "_LAST_NS_TIME" }, { 0xaeb16536, "repsize" },
	{ 0xaf3fcb07, "g1" }, { 0xb17e7914, "_NODE_VALUSER" },
	{ 0xb1ae8672, "h2" }, { 0xb1bcba28, "_LOGON_DLS" },
	{ 0xb31b556d, "phone" }, { 0xb3a77ed0, "q1" },
	{ 0xb3f64be4, "_NEW_SHELL" }, { 0xb50cb889, "_NS_TIME" },
	{ 0xb624fa46, "f1" }, { 0xb65dd6d4, "_USERON.ULB" },
	{ 0xb7b2364b, "x" }, { 0xb9603173, "sock" }, { 0xb969be79, "p" },
	{ 0xba0adba4, "file" }, { 0xba483fc2, "tmp" },
	{ 0xbb063bfd, "user" }, { 0xbbde42a1, "m3" },
	{ 0xbc9488d2, "_NEW_FLAGS4" }, { 0xbd1cee5d, "_USERON.LTODAY" },
	{ 0xbda3fa42, "ascii_mode" }, { 0xbe047a60, "t" },
	{ 0xbe5dc637, "z3" }, { 0xbef3c427, "dest" },
	{ 0xbf31a280, "_ANSWERTIME" }, { 0xc0b506dd, "y" },
	{ 0xc1093f61, "_USERON.DLS" }, { 0xc1ea73f2, "bbs_name_length" },
	{ 0xc6a9b6e4, "h3" }, { 0xc6e8539d, "_LOGON_ULB" },
	{ 0xc70d2fc0, "waitforstr" }, { 0xc7e0e8ce, "_USERON.NETMAIL" },
	{ 0xc82ba467, "_LOGON_EMAILS" }, { 0xc8cd5fb7, "_USERON.COMP" },
	{ 0xc9034af6, "u" }, { 0xc9082cbd, "_USERON.PTODAY" },
	{ 0xc95af6a1, "z2" }, { 0xcb530e03, "c1" },
	{ 0xcc7aca99, "_USERON.NOTE" }, { 0xccd97237, "m2" },
	{ 0xccfe7c5d, "_NEW_FLAGS1" }, { 0xcdb7e4a9, "_USERON.PASS" },
	{ 0xce6e8eef, "q" }, { 0xced08a95, "t1" },
	{ 0xcf9ce02c, "_CDT_MIN_VALUE" }, { 0xd0a99c72, "_USERON.MIN" },
	{ 0xd2483f42, "b1" }, { 0xd28e9da9, "debug_mode" },
	{ 0xd3606303, "_USERON.TMPEXT" }, { 0xd3d99e8b, "a" },
	{ 0xd42ccd08, "buf" }, { 0xd4b45a92, "e" }, { 0xd5c24376, "l2" },
	{ 0xd7ae3022, "_USERON.FREECDT" }, { 0xd7cbbbd4, "u1" },
	{ 0xd8222145, "sm" }, { 0xd859385f, "_SYS_DELDAYS" },
	{ 0xd92803bb, "waitforbuf" }, { 0xda6fd2a0, "m" },
	{ 0xdb0c9ada, "_LOGON_DLB" }, { 0xdcedf626, "_USERON.ULS" },
	{ 0xdd0216b9, "i" }, { 0xdd982780, "_SYS_AUTONODE" },
	{ 0xdf391ca7, "_SYS_LASTNODE" }, { 0xdfb287a5, "i3" },
	{ 0xe277a562, "y2" }, { 0xe3920695, "result" },
	{ 0xe51c1956, "_LOGFILE" }, { 0xe558c608, "_INETMAIL_MISC" },
	{ 0xe579b524, "_USERON.FLAGS4" }, { 0xe5fdd956, "w1" },
	{ 0xe7a7fb07, "_NODE_NUM" }, { 0xe7f421f4, "n2" },
	{ 0xeb6c9c73, "_ERRORLEVEL" }, { 0xec2b8fb8, "_USERON.PHONE" },
	{ 0xed84e527, "k3" }, { 0xedc643f1, "_MAX_QWKMSGS" },
	{ 0xedf6aa98, "_USERON.SHELL" }, { 0xf000aa78, "_USERON.ZIPCODE" },
	{ 0xf16182e9, "files" }, { 0xf19cd046, "_WORDWRAP" },
	{ 0xf3f1da99, "password" }, { 0xf49fd466, "j3" },
	{ 0xf53db6c7, "_NODE_SCRNLEN" }, { 0xf6e36607, "src" },
	{ 0xf8e53990, "pass" }, { 0xf9656c81, "a1" },
	{ 0xf9dc63dc, "_EXPIRED_FLAGS4" }, { 0xfa387529, "filename" },
	{ 0xfad17b8e, "flen" }, { 0xfb394e27, "_EXPIRED_LEVEL" },
	{ 0xfb6c9423, "x2" }, { 0xfcb5b274, "_CDT_PER_DOLLAR" },
	{ 0xfce6e817, "v1" }, { 0xfcf3542e, "_MIN_DSPACE" },
	{ 0xfd4b63d7, "htmlfile" }, { 0xfed3115d, "_USERON.REST" },
	{ 0xfeef10b5, "o2" },
};

#define members(x) (sizeof(x)/sizeof(x[0]))

const char *char_table="________________________________________________123456789!_______BCDEFGHIJKLMNOPQRSTUVWXYZ0____A________________________________________________________________________________________________________________________________________________________________";
const char *first_char_table="_________________________________________________________________BCDEFGHIJKLMNOPQRSTUVWXYZ!____A________________________________________________________________________________________________________________________________________________________________";
unsigned char *brute_buf=NULL;
uint32_t *brute_crc_buf=NULL;
size_t brute_len=0;
char **bruted=NULL;
size_t bruted_len=0;
char **badbruted=NULL;
size_t badbruted_len=0;

void add_bruted(unsigned long name, char good, char *val, int save)
{
	char **new_bruted;
	char *p;
	FILE	*cache;

	bruted_len++;
	p=(char *)malloc(strlen(val)+6);
	if(p==NULL)
		return;
	new_bruted=realloc(bruted, sizeof(char *)*bruted_len);
	if(new_bruted==NULL) {
		free(p);
		return;
	}
	*(int32_t *)p=name;
	p[4]=good;
	strcpy(p+5,val);
	new_bruted[bruted_len-1]=p;
	bruted=new_bruted;
	if(*val && save) {
		cache=fopen("unbaja.brute","a");
		if(cache!=NULL) {
			fprintf(cache,"%08x,%hhd,%s\n",name,good,val);
			fclose(cache);
		}
	}
}

int check_bruted(long name,char *val)
{
	int i;

	for(i=0; i<bruted_len; i++) {
		if(*(int32_t *)bruted[i]==name) {
			if(!strcmp(val,bruted[i]+5))
				return(*(bruted[i]+4));
		}
	}
	return(2);
}

char *find_bruted(long name)
{
	int i;

	for(i=0; i<bruted_len; i++) {
		if(*(int32_t *)bruted[i]==name && *(bruted[i]+4))
			return(bruted[i]+5);
	}
	return(NULL);
}

char* bruteforce(unsigned long name)
{
	uint32_t	this_crc=0;
	char	*ret;
	int	counter=0;
	unsigned char	*pos;
	size_t	l=0;
	size_t	i,j;

	if(!brute_len)
		return(NULL);
	if((ret=find_bruted(name))!=NULL) {
		if(!(*ret))
			return(NULL);
		return(ret);
	}
	memset(brute_buf,0,brute_len+1);
	memset(brute_crc_buf,0,brute_len*sizeof(int32_t));
	printf("Brute forcing var_%08x\n",name);
	this_crc=crc32((char *)brute_buf,0);
	for(;;) {
		pos=brute_buf+l;
		if(pos>brute_buf) {
			pos--;
			while(pos>brute_buf) {
				if(*pos!='9') {	/* last char from more_chars */
					*pos=char_table[*pos];
					pos++;
					i=(size_t)(pos-brute_buf);
					memset(pos,'_',l-i);
					/* Calculate all the following CRCs */
					for(i--;brute_buf[i];i++)
						brute_crc_buf[i]=ucrc32(brute_buf[i],brute_crc_buf[i-1]);
					goto LOOP_END;
				}
				else
					pos--;
			}
		}
		if(*pos=='Z' || *pos==0) {		/* last char from first_chars */
			/* This the max? */
			if(l==brute_len) {
				printf("\r%s Not found.\n",brute_buf);
				add_bruted(name,1,"",0);
				return(NULL);
			}
			/* Set string to '_' with one extra at end */
			memset(brute_buf,'_',++l);
			brute_crc_buf[0]=ucrc32(brute_buf[0],~0UL);
			for(i=1;brute_buf[i];i++)
				brute_crc_buf[i]=ucrc32(brute_buf[i],brute_crc_buf[i-1]);
			/* String is pre-filled with zeros so no need to terminate */
			goto LOOP_END;
		}
		*pos=first_char_table[*pos];
		memset(brute_buf+1,'_',l-1);
		brute_crc_buf[0]=ucrc32(brute_buf[0],~0UL);
		for(i=1;brute_buf[i];i++)
			brute_crc_buf[i]=ucrc32(brute_buf[i],brute_crc_buf[i-1]);

LOOP_END:
		this_crc=~(brute_crc_buf[l-1]);
		if(this_crc==name) {
			switch(check_bruted(name,brute_buf)) {
				case 0:
					break;
				case 2:
					add_bruted(name,1,brute_buf,1);
				case 1:
					goto BRUTE_DONE;
			}
			if(check_bruted(name,brute_buf))
				break;
		}
		if(!((++counter)%10000))
			printf("\r%s ",brute_buf);
	}

BRUTE_DONE:
	printf("\r%s Found!\n",brute_buf);
	return(brute_buf);
}

/* comparison function for var_table */
static int vt_compare(const void *key, const void *table) {
	if (*(uint32_t *)key == (*(struct var_table_t *)table).crc) return 0;
	if (*(uint32_t *)key < (*(struct var_table_t *)table).crc) return -1;
	return 1;
}

char *getvar(long name)
{
	static char varname[20];
	struct var_table_t *found;
	char *brute;

	found = (struct var_table_t *)bsearch( &name, var_table, members( var_table ),
		sizeof( struct var_table_t ), vt_compare );

	if (found) {
		strcpy( varname, (*found).var );
	} else {
		brute=bruteforce(name);
		if(brute)
			strcpy(varname,brute);
		else
			sprintf(varname,"var_%08x",name);
	}

	return(varname);
}

void write_var(FILE *bin, char *src)
{
	int32_t lng;

	fread(&lng, 1, 4, bin);
	sprintf(strchr(src,0),"%s ",getvar(lng));
}

void write_cstr(FILE *bin, char *src)
{
	uchar ch;
	char* p;

	strcat(src,"\"");
	while(fread(&ch,1,1,bin)==1) {
		if(ch==0)
			break;
		if((p=c_escape_char(ch))!=NULL)
			sprintf(strchr(src,0),"%s",p);
		else if(ch<' ' || ch > 126)
			sprintf(strchr(src,0), "\\%03d", ch);
		else
			sprintf(strchr(src,0), "%c", ch);
	}
	strcat(src,"\" ");
}

void write_lng(FILE *bin, char *src)
{
	int32_t lng;

	fread(&lng,4,1,bin);
	sprintf(strchr(src,0),"%ld ",lng);
}

void write_short(FILE *bin, char *src)
{
	int16_t sht;

	fread(&sht,2,1,bin);
	sprintf(strchr(src,0),"%d ",sht);
}

void write_ushort(FILE *bin, char *src)
{
	uint16_t sht;

	fread(&sht,2,1,bin);
	sprintf(strchr(src,0),"%d ",sht);
}

void write_ch(FILE *bin, char *src)
{
	char ch;

	fread(&ch,1,1,bin);
	sprintf(strchr(src,0),"%c ",ch);
}

void write_uchar(FILE *bin, char *src)
{
	uchar uch;

	fread(&uch,1,1,bin);
	sprintf(strchr(src,0),"%u ",uch);
}

void write_logic(FILE *bin, char *src)
{
	char ch;
	fread(&ch,1,1,bin);
	if(ch==LOGIC_TRUE)
		strcat(src,"TRUE ");
	else
		strcat(src,"FALSE ");
}

int write_key(FILE *bin, char *src, int keyset)
{
	uchar uch;
	fread(&uch,1,1,bin);
	if(uch==0 && keyset)
		return(uch);
	if(uch==CS_DIGIT)
		strcat(src,"DIGIT");
	else if(uch==CS_EDIGIT)
		strcat(src,"EDIGIT");
	else if(uch==CR)
		strcat(src,"\\r");
	else if(uch==LF)
		strcat(src,"\\n");
	else if(uch==TAB)
		strcat(src,"\\t");
	else if(uch==BS)
		strcat(src,"\\b");
	else if(uch==BEL)
		strcat(src,"\\a");
	else if(uch==FF)
		strcat(src,"\\f");
	else if(uch==11)
		strcat(src,"\\v");
	else if(uch & 0x80)
		sprintf(strchr(src,0),"/%c",uch&0x7f);
	else if(uch < ' ')
		sprintf(strchr(src,0),"^%c",uch+0x40);
	else if(uch=='\\')
		strcat(src,"'\\'");
	else if(uch=='^')
		strcat(src,"'^'");
	else if(uch=='/')
		strcat(src,"'/'");
	else if(uch=='\'')
		strcat(src,"\\'");
	else
		sprintf(strchr(src,0),"%c",uch);
	if(!keyset)
		strcat(src," ");
	return(uch);;
}

void write_keys(FILE *bin, char *src)
{
	while(write_key(bin,src,TRUE));
	strcat(src," ");
}

void eol(char *src)
{
	char *p;
	p=strchr(src,0);
	p--;
	*p='\n';
	indent+=indenteol;
	indenteol=0;
}

#define WRITE_NAME(name)	if(indent<0) indent=0; \
							sprintf(strchr(src,0),"%.*s"name" ",indent,"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t") \
							/* printf("%s\n",name) */


#define KEYS(name)		WRITE_NAME(name); \
						write_keys(bin,src); \
						eol(src);			 \
						break

#define KEY(name)		WRITE_NAME(name); \
						write_key(bin,src,FALSE); \
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

#define VARUCH(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_uchar(bin,src);  \
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
							sprintf(strchr(src,0),"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,1,1,bin); \
						} else {				 \
							write_uchar(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MSHT(name)		WRITE_NAME(name); \
						if(usevar) {		 \
							sprintf(strchr(src,0),"%s ",getvar(var)); \
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
							sprintf(strchr(src,0),"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,4,1,bin); \
						} else {				 \
							write_lng(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MVARSHTUCH(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						write_short(bin,src);  \
						if(usevar) {		 \
							sprintf(strchr(src,0),"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,1,1,bin); \
						} else {				 \
							write_uchar(bin,src);		 \
						}					 \
						eol(src);			 \
						break

#define MVARSHT(name)	WRITE_NAME(name); \
						write_var(bin,src);  \
						if(usevar) {		 \
							sprintf(strchr(src,0),"%s ",getvar(var)); \
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
							sprintf(strchr(src,0),"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,1,1,bin); \
						} else {				 \
							write_uchar(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MVARVARNZUST(name)	WRITE_NAME(name); \
							write_var(bin,src);  \
							write_var(bin,src);  \
							if(usevar) {		 \
								sprintf(strchr(src,0),"%s ",getvar(var)); \
								usevar=FALSE;	 \
								fread(buf,2,1,bin); \
							} else {				 \
								fread(&ush, 2, 1, bin); \
								if(ush)					\
									sprintf(strchr(src,0),"%u ",ush);  \
							}					 \
							eol(src);			 \
							break

#define MVARVARUST(name)	WRITE_NAME(name); \
							write_var(bin,src);  \
							write_var(bin,src);  \
							if(usevar) {		 \
								sprintf(strchr(src,0),"%s ",getvar(var)); \
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
								sprintf(strchr(src,0),"%s ",getvar(var)); \
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
								sprintf(strchr(src,0),"%s ",getvar(var)); \
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
							sprintf(strchr(src,0),"%s ",getvar(var)); \
							usevar=FALSE;	 \
							fread(buf,4,1,bin); \
						} else {				 \
							write_lng(bin,src);  \
						}					 \
						eol(src);			 \
						break

#define MLNGVAR(name)	WRITE_NAME(name); \
						if(usevar) {		 \
							sprintf(strchr(src,0),"%s ",getvar(var)); \
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
							sprintf(strchr(src,0),"%s ",getvar(var)); \
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
							sprintf(strchr(src,0),"%s ",getvar(var)); \
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
								sprintf(strchr(src,0),"%s ",getvar(var)); \
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
								sprintf(strchr(src,0),"%s ",getvar(var)); \
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
							sprintf(strchr(src,0),"%s ",getvar(var)); \
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
	static char	buf[1024];
	char	*out;
	uchar	*in;
	uint	artype;
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

void decompile(FILE *bin, FILE *srcfile)
{
	int i;
	char	ch;
	uchar	uch;
	ushort	ush;
	int32_t	lng;
	int32_t	lng2;
	int		usevar=FALSE;
	uint32_t	var=0;
	char	buf[80];
	char	*p;
	char	src[2048];
	int		redo=FALSE;
	char	*labels;
	size_t	currpos=0;

	currpos=filelength(fileno(bin));
	labels=(char *)calloc(1,filelength(fileno(bin)));
	if(labels==NULL) {
		printf("ERROR allocating memory for labels\n");
		return;
	}
	while(1) {
		currpos=ftell(bin);

		if(fread(&uch,1,1,bin)!=1) {
			if(redo)
				break;
			redo=TRUE;
			printf("Second pass...\n");
			rewind(bin);
			continue;
		}
		src[0]=0;
		if(labels[currpos])
			sprintf(src,":label_%04x\n",currpos);
		switch(uch) {
			case CS_USE_INT_VAR:
				usevar=TRUE;
				fread(&var,4,1,bin);
				fread(&buf,2,1,bin);	/* offset/length */
				continue;
			case CS_VAR_INSTRUCTION:
				fread(&uch,1,1,bin);
				switch(uch) {
					case SHOW_VARS:
						WRITE_NAME("SHOW_VARS");
						eol(src);
						break;
					case PRINT_VAR:
						VAR("PRINT");
					case VAR_PRINTF:
					case VAR_PRINTF_LOCAL:
						if(uch==VAR_PRINTF) {
							WRITE_NAME("PRINTF");
						}
						else {
							WRITE_NAME("LPRINTF");
						}
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
						WRITE_NAME("SPRINTF");
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
						MVARSHTUCH("PRINTTAIL");
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
						write_cstr(bin,src);
						if(usevar) {
							sprintf(strchr(src,0),"%s ",getvar(var));
							usevar=FALSE;
						} else {
							sprintf(strchr(src,0),"%ld ",lng);
						}
						eol(src);
						break;
					case TELNET_GATE_VAR:				/* TELNET_GATE reverses argument order */
						WRITE_NAME("TELNET_GATE");
						fread(&lng,4,1,bin);
						fread(&lng2, 1, 4, bin);
						sprintf(strchr(src,0),"%s ",getvar(lng2));
						if(usevar) {
							sprintf(strchr(src,0),"%s ",getvar(var));
							usevar=FALSE;
						} else {
							sprintf(strchr(src,0),"%ld ",lng);
						}
						eol(src);
						break;
					case COPY_FIRST_CHAR:
						VARVAR("COPY_FIRST_CHAR");
					case COMPARE_FIRST_CHAR:
						VARUCH("COMPARE_FIRST_CHAR");
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
						free(labels);
						return;
				}
				break;
			case CS_IF_TRUE:
				indenteol=1;
				NONE("IF_TRUE");
			case CS_IF_FALSE:
				indenteol=1;
				NONE("IF_FALSE");
			case CS_ELSE:
				indent--;
				indenteol=1;
				NONE("ELSE");
			case CS_ENDIF:
				indent--;
				NONE("END_IF");
			case CS_CMD_HOME:
				NONE("CMD_HOME");
			case CS_CMD_POP:
				NONE("CMD_POP");
			case CS_END_CMD:
				indent--;
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
				indenteol=1;
				NONE("IF_GREATER");
			case CS_IF_GREATER_OR_EQUAL:
				indenteol=1;
				NONE("IF_GREATER_OR_EQUAL");
			case CS_IF_LESS:
				indenteol=1;
				NONE("IF_LESS");
			case CS_IF_LESS_OR_EQUAL:
				indenteol=1;
				NONE("IF_LESS_OR_EQUAL");
			case CS_DEFAULT:
				indenteol=1;
				NONE("DEFAULT");
			case CS_END_SWITCH:
				indent--;
				NONE("END_SWITCH");
			case CS_END_CASE:
				indent--;
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
						indenteol=1;
						NONE("LOOP_BEGIN");
					case CS_CONTINUE_LOOP:
						NONE("CONTINUE_LOOP");
					case CS_BREAK_LOOP:
						NONE("BREAK_LOOP");
					case CS_END_LOOP:
						indent--;
						NONE("END_LOOP");
					default:
						printf("ERROR!  Unknown one-byte instruction: %02x%02X\n",CS_ONE_MORE_BYTE,uch);
				}
				break;
			case CS_CMDKEY:
				indenteol=1;
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
				indenteol=1;
				CH("CMDCHAR");
			case CS_COMPARE_CHAR:
				CH("COMPARE_CHAR");
			case CS_MULTINODE_CHAT:
				MUCH("MULTINODE_CHAT");
			case CS_TWO_MORE_BYTES:
				fread(&uch,1,1,bin);
				switch(uch) {
					case CS_USER_EVENT:
						MUCH("USER_EVENT");
					default:
						printf("ERROR!  Unknown two-byte instruction: %02x%02X\n",CS_ONE_MORE_BYTE,uch);
				}
				break;
			case CS_GOTO:
				fread(&ush,2,1,bin);
				labels[ush]=TRUE;
				WRITE_NAME("GOTO");
				sprintf(strchr(src,0),"label_%04x ",ush);
				eol(src);
				break;
			case CS_CALL:
				fread(&ush,2,1,bin);
				labels[ush]=TRUE;
				WRITE_NAME("CALL");
				sprintf(strchr(src,0),"label_%04x ",ush);
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
				MSHT("COMPARE_NODE_MISC");
			case CS_MSWAIT:
				MSHT("MSWAIT");
			case CS_ADJUST_USER_MINUTES:
				MSHT("ADJUST_USER_MINUTES");
			case CS_REVERT_TEXT:
				MSHT("REVERT_TEXT");
			case CS_THREE_MORE_BYTES:
				printf("ERROR!  I dont know anything about CS_THREE_MORE_BYTES.\n");
				printf("Cannot continue.\n");
				free(labels);
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
				indenteol=1;
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
				indenteol=1;
				KEYS("CMDKEYS");
			case CS_COMPARE_KEYS:
				KEYS("COMPARE_KEYS");
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
				WRITE_NAME("COMPARE_ARS");
				sprintf(strchr(src,0),"%s\n",decompile_ars((uchar*)p,uch));
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
				indenteol=1;
				VAR("SWITCH");
			case CS_CASE:
				indenteol=1;
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
						free(labels);
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
						MVARVARNZUST("FREAD");
					case FIO_READ_VAR:
						VARVARVAR("FREAD");
					case FIO_WRITE:
						MVARVARNZUST("FWRITE");
					case FIO_WRITE_VAR:
						VARVARVAR("FWRITE");
					case FIO_GET_LENGTH:
						VARVAR("FGET_LENGTH");
					case FIO_EOF:
						VAR("FEOF");
					case FIO_GET_POS:
						VARVAR("FGET_POS");
					case FIO_SEEK:
						WRITE_NAME("FSEEK");
						write_var(bin,src);
						write_lng(bin,src);
						fread(&ush,2,1,bin);
						if(ush==SEEK_CUR)
							strcat(src,"CUR ");
						else if(ush==SEEK_END)
							strcat(src,"END ");
						else
							sprintf(strchr(src,0), "%d ",ush);
						eol(src);
						break;
					case FIO_SEEK_VAR:
						WRITE_NAME("FSEEK");
						write_var(bin,src);
						write_var(bin,src);
						fread(&ush,2,1,bin);
						if(ush==SEEK_CUR)
							strcat(src,"CUR ");
						else if(ush==SEEK_END)
							strcat(src,"END ");
						else
							sprintf(strchr(src,0), "%d ",ush);
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
						WRITE_NAME("FPRINTF");
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
						free(labels);
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
				free(labels);
				return;
		}
		if(redo)
			fputs(src,srcfile);
	}
	free(labels);
}

int main(int argc, char **argv)
{
	int		f;
	FILE	*bin;
	FILE	*src;
	char 	newname[MAX_PATH+1];
	char	*p;
	char	revision[16];
	FILE	*cache;
	char	cache_line[1024];
	char	*crc,*good,*str;

	sscanf("$Revision$", "%*s %s", revision);

	printf("\nUNBAJA v%s-%s - Synchronet Baja Shell/Module De-compiler\n"
		,revision, PLATFORM_DESC);

	for(f=1; f<argc; f++) {
		if(!strncmp(argv[f],"-b",2)) {
			brute_len=atoi(argv[f]+2);
			if(brute_len) {
				brute_buf=(char *)malloc(brute_len+1);
				if(!brute_buf)
					brute_len=0;
				brute_crc_buf=(uint32_t *)malloc(brute_len*sizeof(uint32_t));
				if(!brute_crc_buf) {
					free(brute_buf);
					brute_len=0;
				}
				if((cache=fopen("unbaja.brute","r"))!=NULL) {
					while(fgets(cache_line,sizeof(cache_line),cache)) {
						truncnl(cache_line);
						crc=strtok(cache_line,",");
						if(crc!=NULL) {
							good=strtok(NULL,",");
							if(good!=NULL) {
								str=strtok(NULL,",");
								if(str!=NULL) {
									add_bruted(strtoul(crc,NULL,16),strtoul(good,NULL,10),str,0);
								}
							}
						}
					}
					fclose(cache);
				}
			}
			printf("Will brute-force up to %d chars\n",brute_len);
			continue;
		}
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
				printf("\nDecompiling %s to %s\n",argv[f],newname);
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

