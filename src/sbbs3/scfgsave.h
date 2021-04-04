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

#ifndef _SCFGSAVE_H_
#define _SCFGSAVE_H_

#include "scfgdefs.h"
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT BOOL		save_cfg(scfg_t* cfg, int backup_level);
DLLEXPORT BOOL		write_node_cfg(scfg_t* cfg, int backup_level);
DLLEXPORT BOOL		write_main_cfg(scfg_t* cfg, int backup_level);
DLLEXPORT BOOL		write_msgs_cfg(scfg_t* cfg, int backup_level);
DLLEXPORT BOOL		write_file_cfg(scfg_t* cfg, int backup_level);
DLLEXPORT BOOL		write_chat_cfg(scfg_t* cfg, int backup_level);
DLLEXPORT BOOL		write_xtrn_cfg(scfg_t* cfg, int backup_level);
DLLEXPORT void		refresh_cfg(scfg_t* cfg);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
