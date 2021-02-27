#line 1 "CHK_AR.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

int ar_exp(char **ptrptr, user_t user)
{
	uint len,true=1,not,or,equal,artype=AR_LEVEL,n,i,age;
	ulong l;
	struct tm *gm;

for(;(**ptrptr);(*ptrptr)++) {

	if((**ptrptr)==AR_ENDNEST)
		break;

	not=or=equal=0;

	if((**ptrptr)==AR_OR) {
        or=1;
		(*ptrptr)++; }
	
	if((**ptrptr)==AR_NOT) {
		not=1;
		(*ptrptr)++; }

	if((**ptrptr)==AR_EQUAL) {
        equal=1;
        (*ptrptr)++; }

	if((true && or) || (!true && !or))
		break;

	if((**ptrptr)==AR_BEGNEST) {
		(*ptrptr)++;
		if(ar_exp(ptrptr,user))
			true=!not;
		else
			true=not;
		while((**ptrptr)!=AR_ENDNEST && (**ptrptr)) /* in case of early exit */
			(*ptrptr)++;
		if(!(**ptrptr))
			break;
		continue; }

	artype=(**ptrptr);
	switch(artype) {
		case AR_ANSI:				/* No arguments */
		case AR_RIP:
		case AR_WIP:
		case AR_LOCAL:
		case AR_EXPERT:
		case AR_SYSOP:
		case AR_QUIET:
		case AR_OS2:
		case AR_DOS:
			break;
		default:
			(*ptrptr)++;
			break; }

	n=(**ptrptr);
	i=(*(short *)*ptrptr);
	switch(artype) {
		case AR_LEVEL:
			if((equal && user.level!=n) || (!equal && user.level<n))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessLevel];
				noaccess_val=n; }
			break;
		case AR_AGE:
			age=getage(user.birth);
			if((equal && age!=n) || (!equal && age<n))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessAge];
                noaccess_val=n; }
			break;
		case AR_BPS:
			if((equal && cur_rate!=i) || (!equal && cur_rate<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessBPS];
				noaccess_val=i; }
			break;
		case AR_ANSI:
			if(!(user.misc&ANSI))
				true=not;
			else true=!not;
			break;
		case AR_RIP:
			if(!(user.misc&RIP))
				true=not;
			else true=!not;
            break;
		case AR_WIP:
			if(!(user.misc&WIP))
				true=not;
			else true=!not;
            break;
		case AR_OS2:
			#ifndef __OS2__
				true=not;
			#else
				true=!not;
			#endif
			break;
		case AR_DOS:
			#ifdef __FLAT__
				true=not;
			#else
				true=!not;
			#endif
			break;
		case AR_EXPERT:
			if(!(user.misc&EXPERT))
				true=not;
			else true=!not;
            break;
		case AR_SYSOP:
			if(!SYSOP)
				true=not;
			else true=!not;
            break;
		case AR_QUIET:
			if(thisnode.status!=NODE_QUIET)
				true=not;
			else true=!not;
            break;
		case AR_LOCAL:
			if(online!=ON_LOCAL)
				true=not;
			else true=!not;
            break;
		case AR_DAY:
			now=time(NULL);
			gm=localtime(&now); 	  /* Qnet call out based on time */
			if((equal && gm->tm_wday!=n) || (!equal && gm->tm_wday<n))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessDay];
				noaccess_val=n; }
			break;
		case AR_CREDIT:
			l=(ulong)i*1024UL;
			if((equal && user.cdt+user.freecdt!=l)
				|| (!equal && user.cdt+user.freecdt<l))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessCredit];
				noaccess_val=l; }
			break;
		case AR_NODE:
			if((equal && node_num!=n) || (!equal && node_num<n))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessNode];
                noaccess_val=n; }
			break;
		case AR_USER:
			if((equal && user.number!=i) || (!equal && user.number<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessUser];
				noaccess_val=i; }
			break;
		case AR_GROUP:
			if((equal
				&& (cursubnum<0 || cursubnum>=total_subs
					|| sub[cursubnum]->grp!=i))
				|| (!equal && cursubnum>=0 && cursubnum<total_subs
					&& sub[cursubnum]->grp<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessGroup];
				noaccess_val=i+1; }
            break;
		case AR_SUB:
			if((equal && cursubnum!=i) || (!equal && cursubnum<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessSub];
				noaccess_val=i+1; }
            break;
		case AR_SUBCODE:
			if(cursubnum<0 || cursubnum>=total_subs
				|| strcmp(sub[cursubnum]->code,*ptrptr))
				true=not;
			else
				true=!not;
			while(*(*ptrptr))
				(*ptrptr)++;
			if(!true)
				noaccess_str=text[NoAccessSub];
			break;
		case AR_LIB:
			if((equal
				&& (curdirnum<0 || curdirnum>=total_dirs
					|| dir[curdirnum]->lib!=i))
				|| (!equal && curdirnum>=0 && curdirnum<total_dirs
					&& dir[curdirnum]->lib<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessLib];
				noaccess_val=i+1; }
            break;
		case AR_DIR:
			if((equal && curdirnum!=i) || (!equal && curdirnum<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessDir];
				noaccess_val=i+1; }
            break;
		case AR_DIRCODE:
			if(curdirnum<0 || curdirnum>=total_dirs
				|| strcmp(dir[curdirnum]->code,*ptrptr))
				true=not;
			else
				true=!not;
			while(*(*ptrptr))
				(*ptrptr)++;
			if(!true)
				noaccess_str=text[NoAccessSub];
            break;
		case AR_EXPIRE:
			now=time(NULL);
			if(!user.expire || now+((long)i*24L*60L*60L)>user.expire)
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessExpire];
				noaccess_val=i; }
			break;
		case AR_RANDOM:
			n=random(i+1);
			if((equal && n!=i) || (!equal && n<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			break;
		case AR_LASTON:
			now=time(NULL);
			if((now-user.laston)/(24L*60L*60L)<i)
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			break;
		case AR_LOGONS:
			if((equal && user.logons!=i) || (!equal && user.logons<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			break;
		case AR_MAIN_CMDS:
			if((equal && main_cmds!=i) || (!equal && main_cmds<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
            break;
		case AR_FILE_CMDS:
			if((equal && xfer_cmds!=i) || (!equal && xfer_cmds<i))
				true=not;
			else
				true=!not;
			(*ptrptr)++;
            break;
		case AR_TLEFT:
			if(timeleft/60<n)
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessTimeLeft];
                noaccess_val=n; }
			break;
		case AR_TUSED:
			if((time(NULL)-logontime)/60<n)
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessTimeUsed];
                noaccess_val=n; }
			break;
		case AR_TIME:
			now=time(NULL);
			unixtodos(now,&date,&curtime);
			if((curtime.ti_hour*60)+curtime.ti_min<i)
				true=not;
			else
				true=!not;
			(*ptrptr)++;
			if(!true) {
				noaccess_str=text[NoAccessTime];
				noaccess_val=i; }
			break;
		case AR_PCR:
			if(user.logons>user.posts
				&& (!user.posts || 100/(user.logons/user.posts)<n))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessPCR];
                noaccess_val=n; }
			break;
		case AR_UDR:	/* up/download byte ratio */
			l=user.dlb;
			if(!l) l=1;
			if(user.dlb>user.ulb
				&& (!user.ulb || 100/(l/user.ulb)<n))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessUDR];
                noaccess_val=n; }
			break;
		case AR_UDFR:	/* up/download file ratio */
			i=user.dls;
			if(!i) i=1;
			if(user.dls>user.uls
				&& (!user.uls || 100/(i/user.uls)<n))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessUDFR];
                noaccess_val=n; }
            break;
		case AR_FLAG1:
			if((!equal && !(user.flags1&FLAG(n)))
				|| (equal && user.flags1!=FLAG(n)))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessFlag1];
                noaccess_val=n; }
			break;
		case AR_FLAG2:
			if((!equal && !(user.flags2&FLAG(n)))
				|| (equal && user.flags2!=FLAG(n)))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessFlag2];
                noaccess_val=n; }
            break;
		case AR_FLAG3:
			if((!equal && !(user.flags3&FLAG(n)))
				|| (equal && user.flags3!=FLAG(n)))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessFlag3];
                noaccess_val=n; }
            break;
		case AR_FLAG4:
			if((!equal && !(user.flags4&FLAG(n)))
				|| (equal && user.flags4!=FLAG(n)))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessFlag4];
                noaccess_val=n; }
            break;
		case AR_REST:
			if((!equal && !(user.rest&FLAG(n)))
				|| (equal && user.rest!=FLAG(n)))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessRest];
                noaccess_val=n; }
            break;
		case AR_EXEMPT:
			if((!equal && !(user.exempt&FLAG(n)))
				|| (equal && user.exempt!=FLAG(n)))
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessExempt];
                noaccess_val=n; }
            break;
		case AR_SEX:
			if(user.sex!=n)
				true=not;
			else
				true=!not;
			if(!true) {
				noaccess_str=text[NoAccessSex];
                noaccess_val=n; }
			break; } }
return(true);
}

int chk_ar(char *str, user_t user)
{
	char *p;

if(str==NULL)
	return(1);
p=str;
return(ar_exp(&p,user));
}


/****************************************************************************/
/* This function fills the usrsub, usrsubs, usrgrps, curgrp, and cursub     */
/* variables based on the security clearance of the current user (useron)   */
/****************************************************************************/
void getusrsubs()
{
    uint i,j,k,l;

for(j=0,i=0;i<total_grps;i++) {
    if(!chk_ar(grp[i]->ar,useron))
        continue;
    for(k=0,l=0;l<total_subs;l++) {
        if(sub[l]->grp!=i) continue;
        if(!chk_ar(sub[l]->ar,useron))
            continue;
        usrsub[j][k++]=l; }
    usrsubs[j]=k;
    if(!k)          /* No subs accessible in group */
        continue;
    usrgrp[j++]=i; }
usrgrps=j;
while((curgrp>=usrgrps || !usrsubs[curgrp]) && curgrp) curgrp--;
while(cursub[curgrp]>=usrsubs[curgrp] && cursub[curgrp]) cursub[curgrp]--;
}

/****************************************************************************/
/* This function fills the usrdir, usrdirs, usrlibs, curlib, and curdir     */
/* variables based on the security clearance of the current user (useron)   */
/****************************************************************************/
void getusrdirs()
{
    uint i,j,k,l;

if(useron.rest&FLAG('T')) {
    usrlibs=0;
    return; }
for(j=0,i=0;i<total_libs;i++) {
    if(!chk_ar(lib[i]->ar,useron))
        continue;
    for(k=0,l=0;l<total_dirs;l++) {
        if(dir[l]->lib!=i) continue;
        if(!chk_ar(dir[l]->ar,useron))
            continue;
        usrdir[j][k++]=l; }
    usrdirs[j]=k;
    if(!k)          /* No dirs accessible in lib */
        continue;
    usrlib[j++]=i; }
usrlibs=j;
while((curlib>=usrlibs || !usrdirs[curlib]) && curlib) curlib--;
while(curdir[curlib]>=usrdirs[curlib] && curdir[curlib]) curdir[curlib]--;
}

int dir_op(uint dirnum)
{
return(SYSOP || (dir[dirnum]->op_ar[0] && chk_ar(dir[dirnum]->op_ar,useron)));
}


