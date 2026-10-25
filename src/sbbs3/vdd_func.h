/* Synchronet WinNT VDD FOSSIL constant/type definitions */

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

enum {
	  VDD_OPEN               = 0
	, VDD_CLOSE              = 1
	, VDD_READ               = 2
	, VDD_PEEK               = 3
	, VDD_WRITE              = 4
	, VDD_STATUS             = 5
	, VDD_INBUF_PURGE        = 6
	, VDD_INBUF_FULL         = 7
	, VDD_INBUF_SIZE         = 8
	, VDD_OUTBUF_PURGE       = 9
	, VDD_OUTBUF_FULL        = 10
	, VDD_OUTBUF_SIZE        = 11
	, VDD_ONLINE             = 12
	, VDD_YIELD              = 13
	, VDD_MAYBE_YIELD        = 14
	, VDD_LOAD_INI_FILE      = 15
	, VDD_LOAD_INI_SECTION   = 16
	, VDD_DEBUG_OUTPUT       = 17
	, VDD_HANGUP             = 18
	, VDD_VIRTUALIZE_UART    = 19
};

typedef struct {
	DWORD online;
	DWORD inbuf_size;
	DWORD inbuf_full;
	DWORD outbuf_size;
	DWORD outbuf_full;
} vdd_status_t;

#define SBBSEXEC_MODE_UNSPECIFIED   0
#define SBBSEXEC_MODE_DOS_IN        (1 << 0)
#define SBBSEXEC_MODE_DOS_OUT       (1 << 1)
#define SBBSEXEC_MODE_UART          (1 << 2)
#define SBBSEXEC_MODE_FOSSIL        (1 << 3)
#define SBBSEXEC_MODE_DEFAULT       (SBBSEXEC_MODE_UART | SBBSEXEC_MODE_FOSSIL)

/* Objects shared between the BBS host (e.g. xtrn.cpp) and the VDD (sbbsexec.c) */
/* Each is formatted with the node number followed by an instance suffix: */
/* "" normally, or ".<value>" when the host set SBBSEXEC_INSTANCE_ENVVAR in */
/* the VDM's environment (so multiple BBS hosts on one system can run */
/* DOS programs for the same node number concurrently). */
#define SBBSEXEC_INSTANCE_ENVVAR    "SBBSEXEC_INSTANCE"
#define SBBSEXEC_RD_MAILSLOT_FMT    "\\\\.\\mailslot\\sbbsexec\\rd%d%s"
#define SBBSEXEC_WR_MAILSLOT_FMT    "\\\\.\\mailslot\\sbbsexec\\wr%d%s"
#define SBBSEXEC_HUNGUP_EVENT_FMT   "sbbsexec_hungup%d%s"
#define SBBSEXEC_HANGUP_EVENT_FMT   "sbbsexec_hangup%d%s"
#define SBBSEXEC_CARRIER_EVENT_FMT  "sbbsexec_carrier%d%s"
