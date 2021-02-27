/* SCFGLIB.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#ifndef _SCFGLIB_H
#define _SCFGLIB_H

#define SAVE_MEMORY

#define get_int(var,stream) { if(!fread(&var,1,sizeof(var),stream)) \
								memset(&var,0,sizeof(var)); \
							  offset+=sizeof(var); }
#define get_str(var,stream) { if(!fread(var,1,sizeof(var),stream)) \
								memset(var,0,sizeof(var)); \
                              offset+=sizeof(var); }

typedef struct {
    char    *openerr,
            *reading,
            *readit,
            *allocerr,
            *error;
            } read_cfg_text_t;

char *get_alloc(long *offset, char *outstr, int maxlen, FILE *instream);
void allocerr(read_cfg_text_t txt, long offset, char *fname, uint size);
char *readline(long *offset, char *str, int maxlen, FILE *stream);
void read_node_cfg(read_cfg_text_t txt);
void read_main_cfg(read_cfg_text_t txt);
void read_text_cfg(read_cfg_text_t txt);
void read_xtrn_cfg(read_cfg_text_t txt);
void read_file_cfg(read_cfg_text_t txt);
void read_msgs_cfg(read_cfg_text_t txt);
void read_attr_cfg(read_cfg_text_t txt);
void read_chat_cfg(read_cfg_text_t txt);
long aftol(char *str);              /* Converts flag string to long */
char *ltoaf(long l, char *str);     /* Converts long to flag string */
faddr_t atofaddr(char *str);    /* ASCII text to FidoNet address conversion */
char *faddrtoa(faddr_t addr);   /* FidoNet address to ASCII text conversion */
char chardupe(char *str);       /* Searches for duplicate chars in str */
int  attrstr(char *str);		/* Convert ATTR string into attribute int */

#endif	/* Don't add anything after this line */
