/* ARS.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#ifdef __WATCOMC__
	#include <malloc.h>
#else
	#include <alloc.h>
#endif
#include "ars_defs.h"

#ifdef SBBS
#include "sbbs.h"
#endif

char *arstr(ushort *count, char *str)
{
	static char *nular="";
	char ar[256],*p;
	uint i,j,n,artype=AR_LEVEL,not=0,equal=0,x;

for(i=j=0;str[i];i++) {
    if(str[i]==SP)
        continue;

    if(str[i]=='(') {
        if(not)
            ar[j++]=AR_NOT;
        not=equal=0;
        ar[j++]=AR_BEGNEST;
        continue; }

    if(str[i]==')') {
        ar[j++]=AR_ENDNEST;
        continue; }

    if(str[i]=='|') {
        ar[j++]=AR_OR;
        continue; }
    
    if(str[i]=='!') {
        not=1;
        continue; }

    if(str[i]=='=') {
        equal=1;
        continue; }

    if(str[i]=='&')
        continue;

    if(isalpha(str[i])) {
        if(!strncmp(str+i,"OR",2)) {
            ar[j++]=AR_OR;
            i++;
            continue; }

        if(!strncmp(str+i,"AND",3)) {    /* AND is ignored */
            i+=2;
            continue; }

        if(!strncmp(str+i,"NOT",3)) {
            not=1;
            i+=2;
            continue; }

        if(!strncmp(str+i,"EQUAL TO",8)) {
            equal=1;
            i+=7;
            continue; }

        if(!strncmp(str+i,"EQUAL",5)) {
            equal=1;
            i+=4;
            continue; }

        if(!strncmp(str+i,"EQUALS",6)) {
            equal=1;
            i+=5;
            continue; } }

    if(str[i]=='$') {
        switch(str[i+1]) {
            case 'A':
                artype=AR_AGE;
                break;
            case 'B':
                artype=AR_BPS;
                break;
            case 'C':
                artype=AR_CREDIT;
                break;
            case 'D':
                artype=AR_UDFR;
                break;
            case 'E':
                artype=AR_EXPIRE;
                break;
            case 'F':
                artype=AR_FLAG1;
                break;
            case 'G':
                artype=AR_LOCAL;
				if(not)
					ar[j++]=AR_NOT;
				not=0;
				ar[j++]=artype;
                break;
            case 'H':
                artype=AR_SUB;
                break;
            case 'I':
                artype=AR_LIB;
                break;
            case 'J':
                artype=AR_DIR;
                break;
            case 'K':
                artype=AR_UDR;
                break;
            case 'L':
                artype=AR_LEVEL;
                break;
            case 'M':
                artype=AR_GROUP;
                break;
            case 'N':
                artype=AR_NODE;
                break;
            case 'O':
                artype=AR_TUSED;
                break;
            case 'P':
                artype=AR_PCR;
                break;
			case 'Q':
				artype=AR_RANDOM;
				break;
            case 'R':
                artype=AR_TLEFT;
                break;
            case 'S':
                artype=AR_SEX;
                break;
            case 'T':
                artype=AR_TIME;
                break;
            case 'U':
                artype=AR_USER;
                break;
			case 'V':
				artype=AR_LOGONS;
				break;
            case 'W':
                artype=AR_DAY;
                break;
            case 'X':
                artype=AR_EXEMPT;
                break;
			case 'Y':   /* Days since last on */
				artype=AR_LASTON;
				break;
            case 'Z':
                artype=AR_REST;
                break;
            case '[':
                artype=AR_ANSI;
				if(not)
					ar[j++]=AR_NOT;
				not=0;
				ar[j++]=artype;
                break;
            case '0':
                artype=AR_NULL;
                break;
            case '*':
                artype=AR_RIP;
				if(not)
					ar[j++]=AR_NOT;
				not=0;
				ar[j++]=artype;
                break;

            }
        i++;
        continue; }

    if(isalpha(str[i])) {
        n=i;
        if(!strncmp(str+i,"AGE",3)) {
            artype=AR_AGE;
            i+=2; }
        else if(!strncmp(str+i,"BPS",3)) {
            artype=AR_BPS;
            i+=2; }
        else if(!strncmp(str+i,"PCR",3)) {
            artype=AR_PCR;
            i+=2; }
        else if(!strncmp(str+i,"SEX",3)) {
            artype=AR_SEX;
            i+=2; }
        else if(!strncmp(str+i,"UDR",3)) {
            artype=AR_UDR;
            i+=2; }
        else if(!strncmp(str+i,"DAY",3)) {
            artype=AR_DAY;
            i+=2; }
        else if(!strncmp(str+i,"RIP",3)) {
            artype=AR_RIP;
			if(not)
                ar[j++]=AR_NOT;
            not=0;
			ar[j++]=artype;
			i+=2; }
		else if(!strncmp(str+i,"WIP",3)) {
			artype=AR_WIP;
			if(not)
                ar[j++]=AR_NOT;
            not=0;
			ar[j++]=artype;
            i+=2; }
		else if(!strncmp(str+i,"OS2",3)) {
			artype=AR_OS2;
			if(not)
                ar[j++]=AR_NOT;
            not=0;
			ar[j++]=artype;
            i+=2; }
		else if(!strncmp(str+i,"DOS",3)) {
			artype=AR_DOS;
			if(not)
                ar[j++]=AR_NOT;
            not=0;
			ar[j++]=artype;
            i+=2; }
		else if(!strncmp(str+i,"SUBCODE",7)) {
            artype=AR_SUBCODE;
            i+=6; }
        else if(!strncmp(str+i,"SUB",3)) {
            artype=AR_SUB;
            i+=2; }
		else if(!strncmp(str+i,"LIB",3)) {
            artype=AR_LIB;
            i+=2; }
		else if(!strncmp(str+i,"DIRCODE",7)) {
            artype=AR_DIRCODE;
            i+=6; }
        else if(!strncmp(str+i,"DIR",3)) {
            artype=AR_DIR;
            i+=2; }
		else if(!strncmp(str+i,"ANSI",4)) {
            artype=AR_ANSI;
			if(not)
				ar[j++]=AR_NOT;
            not=0;
			ar[j++]=artype;
            i+=3; }
        else if(!strncmp(str+i,"UDFR",4)) {
            artype=AR_UDFR;
            i+=3; }
        else if(!strncmp(str+i,"FLAG",4)) {
            artype=AR_FLAG1;
            i+=3; }
        else if(!strncmp(str+i,"NODE",4)) {
            artype=AR_NODE;
            i+=3; }
        else if(!strncmp(str+i,"NULL",4)) {
            artype=AR_NULL;
            i+=3; }
        else if(!strncmp(str+i,"USER",4)) {
            artype=AR_USER;
            i+=3; }
        else if(!strncmp(str+i,"TIME",4)) {
            artype=AR_TIME;
            i+=3; }
        else if(!strncmp(str+i,"REST",4)) {
            artype=AR_REST;
            i+=3; }
        else if(!strncmp(str+i,"LEVEL",5)) {
            artype=AR_LEVEL;
            i+=4; }
        else if(!strncmp(str+i,"TLEFT",5)) {
            artype=AR_TLEFT;
            i+=4; }
        else if(!strncmp(str+i,"TUSED",5)) {
            artype=AR_TUSED;
            i+=4; }
        else if(!strncmp(str+i,"LOCAL",5)) {
            artype=AR_LOCAL;
			if(not)
				ar[j++]=AR_NOT;
			not=0;
			ar[j++]=artype;
            i+=4; }
        else if(!strncmp(str+i,"GROUP",5)) {
            artype=AR_GROUP;
            i+=4; }
        else if(!strncmp(str+i,"EXPIRE",6)) {
			artype=AR_EXPIRE;
            i+=5; }
		else if(!strncmp(str+i,"EXPERT",6)) {
			artype=AR_EXPERT;
			if(not)
                ar[j++]=AR_NOT;
            not=0;
			ar[j++]=artype;
            i+=5; }
		else if(!strncmp(str+i,"SYSOP",5)) {
			artype=AR_SYSOP;
			if(not)
                ar[j++]=AR_NOT;
            not=0;
			ar[j++]=artype;
			i+=4; }
		else if(!strncmp(str+i,"QUIET",5)) {
			artype=AR_QUIET;
			if(not)
                ar[j++]=AR_NOT;
            not=0;
			ar[j++]=artype;
            i+=4; }
        else if(!strncmp(str+i,"EXEMPT",6)) {
            artype=AR_EXEMPT;
            i+=5; }
		else if(!strncmp(str+i,"RANDOM",6)) {
			artype=AR_RANDOM;
            i+=5; }
		else if(!strncmp(str+i,"LASTON",6)) {
			artype=AR_LASTON;
            i+=5; }
		else if(!strncmp(str+i,"LOGONS",6)) {
			artype=AR_LOGONS;
            i+=5; }
        else if(!strncmp(str+i,"CREDIT",6)) {
            artype=AR_CREDIT;
            i+=5; }
		else if(!strncmp(str+i,"MAIN_CMDS",9)) {
			artype=AR_MAIN_CMDS;
			i+=8; }
		else if(!strncmp(str+i,"FILE_CMDS",9)) {
			artype=AR_FILE_CMDS;
            i+=8; }
        if(n!=i)            /* one of the above */
            continue; }

    if(not)
        ar[j++]=AR_NOT;
    if(equal)
        ar[j++]=AR_EQUAL;
    not=equal=0;

    if(artype==AR_FLAG1 && isdigit(str[i])) {   /* flag set specified */
        switch(str[i]) {
            case '2':
                artype=AR_FLAG2;
                break;
            case '3':
                artype=AR_FLAG3;
                break;
            case '4':
                artype=AR_FLAG4;
                break; }
        continue; }

	if(artype==AR_SUB && !isdigit(str[i]))
		artype=AR_SUBCODE;
	if(artype==AR_DIR && !isdigit(str[i]))
		artype=AR_DIRCODE;

    ar[j++]=artype;
    if(isdigit(str[i])) {
        if(artype==AR_TIME) {
            n=atoi(str+i)*60;
            p=strchr(str+i,':');
            if(p)
                n+=atoi(p+1);
			*((short *)(ar+j))=n;
            j+=2;
            while(isdigit(str[i+1]) || str[i+1]==':') i++;
            continue; }
		n=atoi(str+i);
        switch(artype) {
            case AR_DAY:
                if(n>6)     /* not past saturday */
                    n=6;
            case AR_AGE:    /* byte operands */
            case AR_PCR:
            case AR_UDR:
            case AR_UDFR:
            case AR_NODE:
            case AR_LEVEL:
            case AR_TLEFT:
            case AR_TUSED:
                ar[j++]=n;
                break;
            case AR_BPS:    /* int operands */
                if(n<300)
                    n*=100;
			case AR_MAIN_CMDS:
			case AR_FILE_CMDS:
            case AR_EXPIRE:
            case AR_CREDIT:
            case AR_USER:
			case AR_RANDOM:
			case AR_LASTON:
			case AR_LOGONS:
				*((short *)(ar+j))=n;
                j+=2;
                break;
            case AR_GROUP:
            case AR_LIB:
            case AR_DIR:
            case AR_SUB:
                if(n) n--;              /* convert to 0 base */
				*((short *)(ar+j))=n;
                j+=2;
                break;
            default:                    /* invalid numeric AR type */
                j--;
                break; }
        while(isdigit(str[i+1])) i++;
        continue; }
	if(artype==AR_SUBCODE || artype==AR_DIRCODE) {
		for(n=0;n<8
			&& str[i]
			&& str[i]!=SP
			&& str[i]!='('
			&& str[i]!=')'
			&& str[i]!='='
			&& str[i]!='|'
			;n++)
			ar[j++]=str[i++];
		ar[j++]=0;
		continue; }
    switch(artype) {
        case AR_FLAG1:
        case AR_FLAG2:
        case AR_FLAG3:
        case AR_FLAG4:
        case AR_EXEMPT:
        case AR_SEX:
        case AR_REST:
            ar[j++]=str[i];
            break;
#ifdef SBBS
        case AR_SUB:
            for(n=0;n<total_subs;n++)
                if(!strnicmp(str+i,sub[n]->code,strlen(sub[n]->code)))
                    break;
            if(n<total_subs) {
				*((short *)(ar+j))=n;
                j+=2; }
            else        /* Unknown sub-board */
                j--;
            while(isalpha(str[i+1])) i++;
            break;
        case AR_DIR:
            for(n=0;n<total_dirs;n++)
                if(!strnicmp(str+i,dir[n]->code,strlen(dir[n]->code)))
                    break;
            if(n<total_dirs) {
				*((short *)(ar+j))=n;
                j+=2; }
            else        /* Unknown directory */
                j--;
            while(isalpha(str[i+1])) i++;
            break;
#endif
        case AR_DAY:
            if(str[i]=='S' && str[i+1]=='U')            /* Sunday */
                ar[j++]=0;
            else if(str[i]=='M')                        /* Monday */
                ar[j++]=1;
            else if(str[i]=='T' && str[i+1]=='U')       /* Tuesday */
                ar[j++]=2;
            else if(str[i]=='W')                        /* Wednesday */
                ar[j++]=3;
            else if(str[i]=='T' && str[i+1]=='H')       /* Thursday */
                ar[j++]=4;
            else if(str[i]=='F')                        /* Friday */
                ar[j++]=5;
            else ar[j++]=6;                             /* Saturday */
            while(isalpha(str[i+1])) i++;
            break;
            } }
if(!j)
	return(nular);	/* Save memory */

ar[j++]=AR_NULL;
/** DEBUG stuff
for(i=0;i<j;i++)
    lprintf("%02X ",(uint)ar[i]);
lputs("\r\n");
***/
if((p=(char *)MALLOC(j))==NULL)
    return(NULL);
memcpy(p,ar,j);
if(count)
	(*count)=j;
return(p);
}

