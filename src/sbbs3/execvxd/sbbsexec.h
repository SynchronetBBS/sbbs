// sbbsexec.h - include file for SBBSEXEC

/* Synchronet Windows 9X FOSSIL driver (requires VtoolsD C++ framework) */

/* $Id: sbbsexec.h,v 1.2 2001/05/02 01:58:31 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

////////////////////////////////////////////////////////////////////////
// Basic defintions for VToolsD framework

#include <vtoolscp.h>
#define DEVICE_CLASS		SBBSExec

#define SBBSExec_DeviceID	UNDEFINED_DEVICE_ID
#define SBBSExec_Major		1
#define SBBSExec_Minor		1
#define SBBSExec_Init_Order	UNDEFINED_INIT_ORDER

#include "..\execvxd.h"		// sbbsexec_start_t definition
#include "..\ringbuf.h"		// RingBuf/RingBuffer

////////////////////////////////////////////////////////////////////////

class SBBSExec : public VDevice
{
public:
	virtual BOOL	OnDeviceInit(VMHANDLE hSysVM, PCHAR pszCmdTail);
	virtual BOOL	OnSysDynamicDeviceInit();
	virtual void	OnSystemExit(VMHANDLE hSysVM);
	virtual BOOL	OnSysDynamicDeviceExit();
	virtual BOOL	OnCreateVM(VMHANDLE hVM);
	virtual DWORD	OnW32DeviceIoControl(PIOCTLPARAMS pIOCTL);
	void			UnhookInts(void);
// Variables
	class SBBSExecInt29*	pInt29;
	class SBBSExecInt21*	pInt21;
   	class SBBSExecInt16*	pInt16;
	class SBBSExecInt14*	pInt14;
	class SBBSExecInt10*	pInt10;
};


class SBBSExecInt29 : public VPreChainV86Int
{
public:
	SBBSExecInt29();
	virtual BOOL handler(VMHANDLE, CLIENT_STRUCT*, DWORD);
};

class SBBSExecInt21 : public VPreChainV86Int
{
public:
	SBBSExecInt21();
	virtual BOOL handler(VMHANDLE, CLIENT_STRUCT*, DWORD);
};

class SBBSExecInt16 : public VPreChainV86Int
{
public:
	SBBSExecInt16();
	virtual BOOL handler(VMHANDLE, CLIENT_STRUCT*, DWORD);
};

class SBBSExecInt14 : public VPreChainV86Int
{
public:
	SBBSExecInt14();
	virtual BOOL handler(VMHANDLE, CLIENT_STRUCT*, DWORD);
};

class SBBSExecInt10 : public VPreChainV86Int
{
public:
	SBBSExecInt10();
	virtual BOOL handler(VMHANDLE, CLIENT_STRUCT*, DWORD);
};
