/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/
 
#include "sauce.h"
#include "genwrap.h"
#include "filewrap.h"

// Saves and restores current file position
// Note: Does not support comments
bool sauce_fread_record(FILE* fp, sauce_record_t* record)
{
	off_t offset = ftello(fp);
	
	if(offset == -1)
		return false;
	
	if(fseeko(fp, -(int)sizeof(*record), SEEK_END) != 0)
		return false;
	
	bool result = fread(record, sizeof(*record), 1, fp) == 1
		&& memcmp(record->id, SAUCE_ID, SAUCE_LEN_ID) == 0
		&& memcmp(record->ver, SAUCE_VERSION, SAUCE_LEN_VERSION) == 0;
	
	fseeko(fp, offset, SEEK_SET);
	return result;
}

// Get 'type' and/or 'info' from SAUCE record of open file (fp) of DataType 'Character'
bool sauce_fread_charinfo(FILE* fp, enum sauce_char_filetype* type, struct sauce_charinfo* info)
{
	sauce_record_t record;

	if(!sauce_fread_record(fp, &record))
		return false;

	if(record.datatype != sauce_datatype_char)
		return false;

	if(type != NULL)
		*type = record.filetype;
	if(info != NULL) {
		memset(info, 0, sizeof(*info));
		SAFECOPY(info->title, record.title), truncsp(info->title);
		SAFECOPY(info->author, record.author), truncsp(info->author);
		SAFECOPY(info->group, record.group), truncsp(info->group);
		SAFECOPY(info->date, record.date), truncsp(info->date);
		info->width = record.tinfo1;
		info->height = record.tinfo2;
		switch(record.filetype) {
			case sauce_char_filetype_ascii:
			case sauce_char_filetype_ansi:
			case sauce_char_filetype_ansimation:
				if(record.tflags & sauce_ansiflag_nonblink)
					info->ice_color = true;
				break;
		}
	}
	return true;
}
