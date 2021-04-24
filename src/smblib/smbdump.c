/* Synchronet message base (SMB) message header dumper */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "datewrap.h"	/* ctime_r */
#include <string.h>		/* strcat */
#include "smblib.h"

static char *binstr(uchar *buf, uint16_t length)
{
	static char str[512];
	int i;

	str[0]=0;
	for(i=0;i<length;i++)
		if(buf[i] && (buf[i]<' ' || buf[i]>=0x7f) 
			&& buf[i]!='\r' && buf[i]!='\n' && buf[i]!='\t')
			break;
	if(i==length)		/* not binary */
		return((char*)buf);
	for(i=0;i<length;i++) {
		sprintf(str+strlen(str),"%02X ",buf[i]);
		if(i>=100) {
			strcat(str,"...");
			break;
		}
	}
	truncsp(str);
	return(str);
}

str_list_t smb_msghdr_str_list(smbmsg_t* msg)
{
	char tmp[128];
	int i;
	time_t	tt;
	str_list_t list = strListInit();

	if(list == NULL)
		return NULL;

#define HFIELD_NAME_FMT "%-16.16s "

	/* variable fields */
	for(i=0;i<msg->total_hfields;i++) {
		switch(msg->hfield[i].type) {
			case SMB_COST:
				strListAppendFormat(&list, HFIELD_NAME_FMT "%lu"
					,smb_hfieldtype(msg->hfield[i].type)
					,(ulong)*(uint32_t*)msg->hfield_dat[i]);
				break;
			case SMB_COLUMNS:
				strListAppendFormat(&list, HFIELD_NAME_FMT "%u"
					,smb_hfieldtype(msg->hfield[i].type)
					,*(uint8_t*)msg->hfield_dat[i]);
				break;
			case SENDERNETTYPE:
			case RECIPIENTNETTYPE:
			case REPLYTONETTYPE:
				strListAppendFormat(&list, HFIELD_NAME_FMT "%s"
					,smb_hfieldtype(msg->hfield[i].type)
					,smb_nettype((enum smb_net_type)*(uint16_t*)msg->hfield_dat[i]));
				break;
			case SENDERNETADDR:
				strListAppendFormat(&list, HFIELD_NAME_FMT "%s"
					,smb_hfieldtype(msg->hfield[i].type)
					,smb_netaddr(&msg->from_net));
				break;
			case RECIPIENTNETADDR:
				strListAppendFormat(&list, HFIELD_NAME_FMT "%s"
					,smb_hfieldtype(msg->hfield[i].type)
					,smb_netaddr(&msg->to_net));
				break;
			case REPLYTONETADDR:
				strListAppendFormat(&list, HFIELD_NAME_FMT "%s"
					,smb_hfieldtype(msg->hfield[i].type)
					,smb_netaddr(&msg->replyto_net));
				break;
			case FORWARDED:
				tt = *(time32_t*)msg->hfield_dat[i];
				strListAppendFormat(&list, HFIELD_NAME_FMT "%.24s"
					,smb_hfieldtype(msg->hfield[i].type)
					,ctime_r(&tt, tmp));
				break;
			default:
				strListAppendFormat(&list, HFIELD_NAME_FMT "%s"
					,smb_hfieldtype(msg->hfield[i].type)
					,binstr((uchar *)msg->hfield_dat[i],msg->hfield[i].length));
				break;
		}
	}

	/* fixed header fields */
	tt=msg->hdr.when_written.time;
	strListAppendFormat(&list, HFIELD_NAME_FMT "%08X %04hX %.24s %s"	,"when_written"
		,msg->hdr.when_written.time, msg->hdr.when_written.zone
		,ctime_r(&tt, tmp)
		,smb_zonestr(msg->hdr.when_written.zone,NULL));
	tt=msg->hdr.when_imported.time;
	strListAppendFormat(&list, HFIELD_NAME_FMT "%08X %04hX %.24s %s"	,"when_imported"
		,msg->hdr.when_imported.time, msg->hdr.when_imported.zone
		,ctime_r(&tt, tmp)
		,smb_zonestr(msg->hdr.when_imported.zone,NULL));
	strListAppendFormat(&list, HFIELD_NAME_FMT "%04Xh"			,"type"				,msg->hdr.type);
	strListAppendFormat(&list, HFIELD_NAME_FMT "%04Xh"			,"version"			,msg->hdr.version);
	strListAppendFormat(&list, HFIELD_NAME_FMT "%04Xh (%s)"			,"attr"			,msg->hdr.attr, smb_msgattrstr(msg->hdr.attr, tmp, sizeof(tmp)));
	strListAppendFormat(&list, HFIELD_NAME_FMT "%08"PRIX32"h (%s)"	,"auxattr"		,msg->hdr.auxattr, smb_auxattrstr(msg->hdr.auxattr, tmp, sizeof(tmp)));
	strListAppendFormat(&list, HFIELD_NAME_FMT "%08"PRIX32"h (%s)"	,"netattr"		,msg->hdr.netattr, smb_netattrstr(msg->hdr.netattr, tmp, sizeof(tmp)));
	strListAppendFormat(&list, HFIELD_NAME_FMT "%06"PRIX32"h"	,"header_offset"	,msg->idx.offset);
	strListAppendFormat(&list, HFIELD_NAME_FMT "%hu"            ,"header_fields"    ,msg->total_hfields);
	strListAppendFormat(&list, HFIELD_NAME_FMT "%u (calc: %lu)"	,"header_length"	,msg->hdr.length, smb_getmsghdrlen(msg));
	strListAppendFormat(&list, HFIELD_NAME_FMT "%"PRId32		,"number"			,msg->hdr.number);

	/* optional fixed fields */
	if(msg->hdr.thread_id)
		strListAppendFormat(&list, HFIELD_NAME_FMT "%"PRId32	,"thread_id"		,msg->hdr.thread_id);
	if(msg->hdr.thread_back)
		strListAppendFormat(&list, HFIELD_NAME_FMT "%"PRId32	,"thread_back"		,msg->hdr.thread_back);
	if(msg->hdr.thread_next)
		strListAppendFormat(&list, HFIELD_NAME_FMT "%"PRId32	,"thread_next"		,msg->hdr.thread_next);
	if(msg->hdr.thread_first)
		strListAppendFormat(&list, HFIELD_NAME_FMT "%"PRId32	,"thread_first"		,msg->hdr.thread_first);
	if(msg->hdr.delivery_attempts)
		strListAppendFormat(&list, HFIELD_NAME_FMT "%hu"		,"delivery_attempts",msg->hdr.delivery_attempts);
	if(msg->hdr.votes)
		strListAppendFormat(&list, HFIELD_NAME_FMT "%hu"		,"votes"			,msg->hdr.votes);
	if(msg->hdr.type == SMB_MSG_TYPE_NORMAL) {
		if(msg->hdr.priority)
			strListAppendFormat(&list, HFIELD_NAME_FMT "%u"		,"priority"			,msg->hdr.priority);
	} else { // FILE
		if(msg->hdr.times_downloaded)
			strListAppendFormat(&list, HFIELD_NAME_FMT "%"PRIu32,"times_downloaded"	,msg->hdr.times_downloaded);
		if(msg->hdr.last_downloaded) {
			tt=msg->hdr.last_downloaded;
			strListAppendFormat(&list, HFIELD_NAME_FMT "%.24s"	,"last_downloaded"	,ctime_r(&tt, tmp));
		}
	}

	/* convenience integers */
	if(msg->expiration) {
		tt=msg->expiration;
		strListAppendFormat(&list, HFIELD_NAME_FMT "%.24s", "expiration", ctime_r(&tt, tmp));
	}

	/* data fields */
	strListAppendFormat(&list, HFIELD_NAME_FMT "%06"PRIX32"h"	,"data_offset"		,msg->hdr.offset);
	for(i=0;i<msg->hdr.total_dfields;i++)
		strListAppendFormat(&list, "data_field[%u]    %s, offset %"PRIu32", length %"PRIu32
			,i
			,smb_dfieldtype(msg->dfield[i].type)
			,msg->dfield[i].offset
			,msg->dfield[i].length);

	return list;
}

void smb_dump_msghdr(FILE* fp, smbmsg_t* msg)
{
	int i;

	str_list_t list = smb_msghdr_str_list(msg);
	if(list != NULL) {
		for(i = 0; list[i] != NULL; i++) {
			fprintf(fp, "%s\n", list[i]);
		}
		strListFree(&list);
	}
}
