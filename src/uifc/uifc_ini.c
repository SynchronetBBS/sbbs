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

#include "uifc.h"
#include "ini_file.h"
#include "ciolib.h"

void read_uifc_ini(const char* path, uifcapi_t* uifc, int* ciolib_mode, enum text_modes* video_mode)
{
	FILE*       fp = iniOpenFile(path, /* update: */ false);

	const char* section = ROOT_SECTION;
	if (video_mode != NULL)
		*video_mode = iniReadInteger(fp, section, "video_mode", *video_mode);
	uifc->mode = iniReadInteger(fp, section, "uifc_mode", uifc->mode);
	uifc->scrn_len = iniReadInteger(fp, section, "lines", uifc->scrn_len);
	uifc->insert_mode = iniReadBool(fp, section, "insert", uifc->insert_mode);
	uifc->esc_delay = iniReadInteger(fp, section, "esc_delay", uifc->esc_delay);
	if (ciolib_mode != NULL)
		*ciolib_mode = iniReadInteger(fp, section, "ciolib_mode", *ciolib_mode);
	// No vstatlock around ciolib_initial_scaling because the lock shouldn't exist yet
	/* coverity[missing_lock:SUPPRESS] */
	ciolib_initial_scaling = iniReadFloat(fp, section, "scaling", ciolib_initial_scaling);
	uifc->hclr = iniReadUInteger(fp, section, "FrameColor", uifc->hclr);
	uifc->lclr = iniReadUInteger(fp, section, "TextColor", uifc->lclr);
	uifc->bclr = iniReadUInteger(fp, section, "BackgroundColor", uifc->bclr);
	uifc->cclr = iniReadUInteger(fp, section, "InverseColor", uifc->cclr);
	uifc->lbclr = iniReadUInteger(fp, section, "LightbarColor", uifc->lbclr);

	if (fp != NULL)
		iniCloseFile(fp);
}
