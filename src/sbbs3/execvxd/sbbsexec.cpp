// SBBSEXEC.cpp - SBBSEXEC.VXD main module

/* Synchronet Windows 9X FOSSIL driver (requires VtoolsD C++ framework) */

/* $Id: sbbsexec.cpp,v 1.3 2018/07/24 01:11:52 rswindell Exp $ */

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


#define DEVICE_MAIN
#include "sbbsexec.h"
#include "debugout.h"
#include "..\ringbuf.h"

#define RINGBUF_SIZE_IN		10000
#define RINGBUF_SIZE_OUT	10000

#undef	DEVICENAME
#define DEVICENAME "SBBSEXEC"    // For debug output

#include LOCKED_CODE_SEGMENT
#include LOCKED_DATA_SEGMENT

#define MAX_VM 250

typedef struct {
	VMHANDLE	handle;
    SEMHANDLE	input_sem;
    SEMHANDLE	output_sem;
	DWORD		mode;
	BOOL		online;
	BOOL		overrun;
	RingBuf		in;
	RingBuf		out;
} vm_t;

vm_t vm[MAX_VM];

Declare_Virtual_Device(SBBSExec)

//////////////////////////////////////////////////////////////////////////
#include LOCKED_CODE_SEGMENT
#include LOCKED_DATA_SEGMENT

vm_t*				new_vm;
sbbsexec_start_t	start;

vm_t* find_vm(VMHANDLE hVM)
{
	int i;

	for(i=0;i<MAX_VM;i++)
		if(vm[i].handle==hVM)
			return(&vm[i]);
#if 0
	if(hVM!=Get_Sys_VM_Handle()) {
		DBTRACEx(0,"!VM NOT FOUND",hVM);
	}
#endif
	return(NULL);
}

BOOL SBBSExec::OnSysDynamicDeviceInit()
{
	DBTRACE(0,"OnSysDynamicDeviceInit");

	return(OnDeviceInit(NULL,NULL));
}


BOOL SBBSExec::OnDeviceInit(VMHANDLE hSysVM, PCHAR pszCmdTail)
{
	DBTRACE(0,"SBBSExec::OnDeviceInit");

	pInt10 = NULL;	// BIOS I/O
	pInt14 = NULL;	// FOSSIL
	pInt16 = NULL;	// Keyboard input
	pInt21 = NULL;	// DOS I/O
	pInt29 = NULL;	// Direct console I/O (PKZIP/UNZIP)

	memset(&start,0,sizeof(start));
#if 1


	if ( (pInt21 = new SBBSExecInt21()) == NULL) {
		DBTRACE(0,"!Failed to create int 21 handler");
		UnhookInts();
		return(FALSE);
	}

	if ( !pInt21->hook()) {
		DBTRACE(0,"!Failed to hook int 21");
		UnhookInts();
		return(FALSE);
	}

	if ( (pInt29 = new SBBSExecInt29()) == NULL) {
		DBTRACE(0,"!Failed to create int 29 handler");
		UnhookInts();
		return(FALSE);
	}

	if ( !pInt29->hook()) {
		DBTRACE(0,"!Failed to hook int 29");
		UnhookInts();
		return(FALSE);
	}

	if ( (pInt16 = new SBBSExecInt16()) == NULL) {
		DBTRACE(0,"!Failed to create int 16 handler");
		UnhookInts();
		return(FALSE);
	}

	if ( !pInt16->hook()) {
		DBTRACE(0,"!Failed to hook int 16");
		UnhookInts();
		return(FALSE);
	}

	if ( (pInt14 = new SBBSExecInt14()) == NULL) {
		DBTRACE(0,"!Failed to create int 14 handler");
		UnhookInts();
		return(FALSE);
	}

	if ( !pInt14->hook()) {
		DBTRACE(0,"!Failed to hook int 14");
		UnhookInts();
		return(FALSE);
	}

	if ( (pInt10 = new SBBSExecInt10()) == NULL) {
		DBTRACE(0,"!Failed to create int 10 handler");
		UnhookInts();
		return(FALSE);
	}

	if ( !pInt10->hook()) {
		DBTRACE(0,"!Failed to hook int 10");
		UnhookInts();
		return(FALSE);
	}

#endif

	return TRUE;
}

//****************************************************************************
//****************************************************************************
VOID SBBSExec::OnSystemExit(VMHANDLE hSysVM)
{
	DBTRACE(0,"OnSystemExit");
	OnSysDynamicDeviceExit();
}

void SBBSExec::UnhookInts()
{

	if(pInt10!=NULL) {
		pInt10->unhook();
		delete pInt10;
		pInt10=NULL;
	}

	if(pInt14!=NULL) {
		pInt14->unhook();
		delete pInt14;
		pInt14=NULL;
	}

	if(pInt16!=NULL) {
		pInt16->unhook();
		delete pInt16;
		pInt16=NULL;
	}

	if(pInt21!=NULL) {
		pInt21->unhook();
		delete pInt21;
		pInt21=NULL;
	}

	if(pInt29!=NULL) {
		pInt29->unhook();
		delete pInt29;
		pInt29=NULL;
	}
}

//****************************************************************************
// Handle control message SYS_DYNAMIC_DEVICE_EXIT
//****************************************************************************
BOOL SBBSExec::OnSysDynamicDeviceExit()
{
	DBTRACE(0,"OnSysDynamicDeviceExit");

	UnhookInts();

	return(TRUE);
}

//////////////////////////////////////////////////////////////////////////
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BOOL SBBSExec::OnCreateVM(VMHANDLE hVM)
{
	DBTRACExd(0,"CreateVM, handle, time",hVM,Get_System_Time());
	DBTRACEx(0,"Current Thread Handle",Get_Cur_Thread_Handle());

	if(start.event) {
		new_vm=find_vm(NULL);
		if(new_vm==NULL) {
			DBTRACE(0,"!NO AVAILABLE VM structures");
			return(FALSE);
		}
		new_vm->handle=hVM;
		new_vm->mode=start.mode;
		new_vm->online = true;
		new_vm->overrun = false;
		new_vm->input_sem = NULL;
		new_vm->output_sem = NULL;

		if(RingBufInit(&new_vm->in, RINGBUF_SIZE_IN)!=0
			|| RingBufInit(&new_vm->out, RINGBUF_SIZE_OUT)!=0) {
			DBTRACE(0,"!FAILED to create I/O buffers");
			return(FALSE);
		}
		if(!VWIN32_SetWin32Event(start.event)) {
			DBTRACEx(0,"!FAILED TO SET EVENT handle", start.event);
			return(FALSE);
		}
		if(!VWIN32_CloseVxDHandle(start.event)) {
			DBTRACEx(0,"!FAILED TO CLOSE EVENT handle", start.event);
			return(FALSE);
		}
		start.event=0;
	}
	return(TRUE);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SBBSExecInt29::SBBSExecInt29() : VPreChainV86Int(0x29)
{
	DBTRACE(0,"SBBSExecInt29 constructor");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BOOL SBBSExecInt29::handler(VMHANDLE hVM, CLIENT_STRUCT* pRegs, DWORD intno)
{
	vm_t* vm = find_vm(hVM);

	if(vm==NULL || !(vm->mode&SBBSEXEC_MODE_DOS_OUT)) {
		return(FALSE); // Tells VMM that interrupt was not handled
	}

    if(!RingBufFree(&vm->out)) {
        DBTRACEx(0,"!Int29 OUTPUT BUFFER OVERFLOW, hVM", hVM);
        vm->overrun=true;
        return(FALSE);
    }
	DBTRACEx(1,"Int29 OUTPUT", _clientAL);

    BYTE ch=_clientAL;
    RingBufWrite(&vm->out,&ch,1);
    vm->overrun=false;

	return(FALSE);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SBBSExecInt21::SBBSExecInt21() : VPreChainV86Int(0x21)
{
	DBTRACE(0,"SBBSExecInt21 constructor");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BOOL SBBSExecInt21::handler(VMHANDLE hVM, CLIENT_STRUCT* pRegs, DWORD intno)
{
	BYTE	ch;
	BYTE*	buffer;
	WORD	buflen;
	vm_t*	vm = find_vm(hVM);

	if(vm==NULL || !(vm->mode&SBBSEXEC_MODE_DOS_OUT)) {
#if 0
		if(vm && !(vm->mode&SBBSEXEC_MODE_DOS_OUT)) {
			DBTRACEx(0,"Int21 on unsupported VM", hVM);
		}
#endif
		return(FALSE); // Tells VMM that interrupt was not handled
	}
	DBTRACEx(1,"Int21 function", _clientAH);

	DWORD avail = RingBufFree(&vm->out);

    switch(_clientAH) {
    	case 0x01:
            DBTRACE(0,"!Int21 Char input WITH echo");
            break;
        case 0x06:	// Direct console I/O
			DBTRACEx(0,"DOS DIRECT CONSOLE IO, DL", _clientDL);
			if(_clientDL==0xff)  {
				avail=RingBufFull(&vm->in);
				if(avail) {
					DBTRACEd(0,"avail",avail);
		            RingBufRead(&vm->in, &ch, 1);
					_clientFlags&=~(1<<6);	// clear zero flag
					_clientAX=ch;
					return(TRUE);
				}
				break;
			}
			// fall-through
    	case 0x02:	// Character output
			DBTRACEx(1,"Int21 function", _clientAH);
            if(!avail) {
                DBTRACEx(0,"!OUTPUT BUFFER OVERFLOW, hVM", hVM);
                vm->overrun=true;
				break;
            }
            ch=_clientDL;
            RingBufWrite(&vm->out,&ch,1);
            vm->overrun=false;
			break;
		case 0x09:	// Display string
			DBTRACE(0,"!Int21 func 09 - DISPLAY STRING");
			break;
		case 0x0A:	// Buffered keyboard input
			DBTRACE(0,"Int21 Func 0A - Buffered Keyboard Input");
			/* Need to get a string from the user, echo, and copy to DS:DX */
			/* byte 0 = max length, byte 1 = actual read (minus CR) */
			break;
        case 0x40:	// Write file or device
			if(_clientBX!=1 && _clientBX!=2) {	// !stdout and !stderr
            	DBTRACEd(1,"!Int21 write to unsupported device", _clientBX);
				break;
            }
			DBTRACEdd(1,"Int21 write file", _clientBX, _clientCX);
            buffer = (BYTE*)MAPFLAT(CRS.Client_DS, CWRS.Client_DX);
            buflen = _clientCX;
            if(avail<buflen) {
                DBTRACEd(0,"!OUTPUT BUFFER OVERFLOW, avail", avail);
                vm->overrun=true;
                if(!avail)
					break;
                buflen=avail;
            }
            RingBufWrite(&vm->out,buffer,buflen);
            vm->overrun=false;
            break;
    }

	return(FALSE);	// Tells VMM that interrupt was not handled
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SBBSExecInt16::SBBSExecInt16() : VPreChainV86Int(0x16)
{
	DBTRACE(0,"SBBSExecInt16 constructor");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BOOL SBBSExecInt16::handler(VMHANDLE hVM, CLIENT_STRUCT* pRegs, DWORD intno)
{
	BYTE 	ch;
    DWORD   avail;
	vm_t*	vm = find_vm(hVM);

	if(vm==NULL || !(vm->mode&SBBSEXEC_MODE_DOS_IN)) {
		return(FALSE); // Tells VMM that interrupt was not handled
	}

//    DBTRACExx(0,"Int16 (hVM, AX)", hVM, _clientAX);

	avail=RingBufFull(&vm->in);
 	switch(_clientAH) {
    	case 0x00:	// Read char from keyboard
        case 0x10:	// Read char from enhanced keyboard
			if(avail) {
	            RingBufRead(&vm->in, &ch, 1);
				_clientAX=ch;
				return(TRUE);
			}
			break;
    	case 0x01:	// Get keyboard status
        case 0x11:	// Get enhanced keyboard status
			if(avail) {
                RingBufPeek(&vm->in, &ch, 1);
                _clientFlags&=~(1<<6);	// clear zero flag
                _clientAX=ch;
				return(TRUE);
    	    }
	        break;
        default:
		    DBTRACEx(0,"!UNHANDLED INT16 function", _clientAH);
            break;
	}

    return(FALSE);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SBBSExecInt14::SBBSExecInt14() : VPreChainV86Int(0x14)
{
	DBTRACE(0,"SBBSExecInt14 constructor");
}

WORD PortStatus(vm_t* vm)
{
	WORD status=0x0008;			// AL bit 3 (change in DCD) always set

	if(vm->online)				// carrier detect
		status|=0x0080;			// DCD

	if(RingBufFull(&vm->in))	// receive data ready 
		status|=0x0100;			// RDA

	if(vm->overrun)				// overrun error detected
		status|=0x0200;			// OVRN

	if(RingBufFree(&vm->out)>	// room available in output buffer
		RINGBUF_SIZE_OUT/2)	
		status|=0x2000;			// THRE

	if(!RingBufFull(&vm->out))	// output buffer is empty
		status|=0x4000;			// TSRE

	return(status);
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const char* fossil_id="Synchronet SBBSEXEC.VXD Fossil";
BOOL SBBSExecInt14::handler(VMHANDLE hVM, CLIENT_STRUCT* pRegs, DWORD intno)
{
	BYTE*	buffer;
	BYTE	ch;
	WORD	buflen;
    WORD	rd,wr;
    WORD	avail;
	vm_t*	vm = find_vm(hVM);

	if(vm==NULL || vm->mode!=SBBSEXEC_MODE_FOSSIL) {
		return(FALSE); // Tells VMM that interrupt was not handled
	}


	DBTRACEx(4,"Int14 func",_clientAH);

	switch(_clientAH) {
		case 0x00:	/* Initialize/Set baud rate */
        	DBTRACE(0,"Int14 init");
			_clientAX=PortStatus(vm);
			break;
		case 0x01: /* write char to com port */
        	if(RingBufFree(&vm->out)<2) {
            	DBTRACEx(1,"!OUTPUT BUFFER OVERFLOW, hVM", hVM);
            	vm->output_sem=Create_Semaphore(0);
                Wait_Semaphore(vm->output_sem,BLOCK_THREAD_IDLE);
                Destroy_Semaphore(vm->output_sem);
                vm->output_sem=NULL;
                if(!vm->online) {
                	DBTRACE(0,"!USER HUNG UP");
                	return(true);
                }
            }
			ch=_clientAL;
			RingBufWrite(&vm->out,&ch,1);
#if 0	/* Now done in SBBS.DLL/XTRN.CPP */
			if(ch==0xff) { /* escape TELNET IAC */
				RingBufWrite(&vm->out,&ch,1);
				DBTRACE(1,"Escaped IAC in output stream");
			}
#endif
			vm->overrun=false;
			_clientAX=PortStatus(vm);
			break;
		case 0x02: /* read char from com port */
			if(!RingBufFull(&vm->in)) {
            	DBTRACEx(0,"Waiting on input semaphore, hVM", hVM);
            	vm->input_sem=Create_Semaphore(0);
                Wait_Semaphore(vm->input_sem,BLOCK_THREAD_IDLE);
                Destroy_Semaphore(vm->input_sem);
                vm->input_sem=NULL;
#if 0
				_clientAH=0x80;	/* timed-out */
				return(TRUE);
#endif
			}
			RingBufRead(&vm->in,&ch,1);
			_clientAH=0;
			_clientAL=ch;
			break;
		case 0x03:	/* request status */
			_clientAX=PortStatus(vm);
			break;
		case 0x04:	/* initialize */
			DBTRACE(0,"Int14 func 4 init");
			_clientAX=0x1954;	/* magic number = success */
			_clientBH=5;		/* FOSSIL rev */
			_clientBL=0x1B;		/* maximum FOSSIL func supported */
			break;
        case 0x08:	// flush output buffer
            DBTRACE(0,"Int14 FLUSH OUTPUT BUFFER");
            vm->output_sem=Create_Semaphore(0);
            Wait_Semaphore(vm->output_sem,BLOCK_THREAD_IDLE);
            Destroy_Semaphore(vm->output_sem);
            vm->output_sem=NULL;
			break;
        case 0x09:	// purge output buffer
        	DBTRACE(0,"Int14 PURGE OUTPUT BUFFER");
        	RingBufReInit(&vm->out);
            break;
        case 0x0A:	// purge input buffer
        	DBTRACE(0,"Int14 PURGE INPUT BUFFER");
        	RingBufReInit(&vm->in);
            break;
		case 0x0B: /* write char to com port, no wait */
        	if(RingBufFree(&vm->out)<2) {
            	_clientAX=0; // char was not accepted
                break;
            }
			ch=_clientAL;
			RingBufWrite(&vm->out,&ch,1);
#if 0	/* Now done in SBBS.DLL/XTRN.CPP */
			if(ch==0xff) { /* escape TELNET IAC */
				RingBufWrite(&vm->out,&ch,1);
				DBTRACE(1,"Escaped IAC in output stream");
			}
#endif
			_clientAX=1; // char was accepted
			break;
        case 0x0C:	// non-destructive read-ahead
			if(!RingBufFull(&vm->in)) {
				_clientAX=0xffff;	// no char available
				break;
			}
			RingBufPeek(&vm->in,&ch,1);
			_clientAH=0;
			_clientAL=ch;
			break;
		case 0x13:	/* write to display */
			dprintf("%c",_clientAL);
			break;
        case 0x18:	/* read bock */
        	rd=_clientCX;
            avail=RingBufFull(&vm->in);
            if(rd>avail)
            	rd=avail;
            if(rd) {
	            buffer = (BYTE*)MAPFLAT(CRS.Client_ES, CWRS.Client_DI);
                rd = RingBufRead(&vm->in, buffer, rd);
            }
            _clientAX = rd;
            break;
        case 0x19:	/* write block */
			wr=_clientCX;
            avail=RingBufFree(&vm->out);
            if(wr>avail)
            	wr=avail;
            if(wr) {
	            buffer = (BYTE*)MAPFLAT(CRS.Client_ES, CWRS.Client_DI);
                wr = RingBufWrite(&vm->out, buffer, wr);
            }
            _clientAX = wr;
            break;
#if 1
        case 0x1B:	// driver info
        {
        	DBTRACE(1,"Int14 driver info");
            struct {
                WORD    info_size;
                BYTE	curr_fossil;
                BYTE	curr_rev;
                DWORD	id_string;
                WORD	inbuf_size;
                WORD	inbuf_free;
                WORD	outbuf_size;
                WORD	outbuf_free;
                BYTE	screen_width;
                BYTE	screen_height;
                BYTE	baud_rate;
            } info={ sizeof(info), 5, 1, 0
            		,RINGBUF_SIZE_IN-1, RingBufFree(&vm->in)
                    ,RINGBUF_SIZE_OUT-1, RingBufFree(&vm->out)
                    ,80,25
                    ,1 // 38400
                    };
//			Map_Lin_To_VM_Addr
			buffer = (BYTE*)MAPFLAT(CRS.Client_ES, CWRS.Client_DI);
            wr=sizeof(info);
            if(wr>_clientCX)
            	wr=_clientCX;
            memcpy(buffer, &info, wr);
        	_clientAX=wr;
            break;
		}
#endif
		default:
			DBTRACEx(0,"!UNHANDLED INTERRUPT 14h function",_clientAH);
			break;
	}
	return(TRUE);	// Tells VMM that interrupt was handled
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SBBSExecInt10::SBBSExecInt10() : VPreChainV86Int(0x10)
{
	DBTRACE(0,"SBBSExecInt10 constructor");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
BOOL SBBSExecInt10::handler(VMHANDLE hVM, CLIENT_STRUCT* pRegs, DWORD intno)
{
	BYTE 	ch;
    DWORD   avail;
	vm_t*	vm = find_vm(hVM);

	if(vm==NULL || !(vm->mode&SBBSEXEC_MODE_DOS_OUT)) {
		return(FALSE); // Tells VMM that interrupt was not handled
	}

	avail=RingBufFree(&vm->out);

 	switch(_clientAH) {
		case 0x02: // Set Cursor Position
			DBTRACE(1,"Int10 func 2 - Set cursor position");
			break;
    	case 0x09:	// Write char and attr at cursor
			DBTRACE(1,"Int10 func 9 - Write char and attr at curosr");
        case 0x10:	// Write char at cursor
#if 0
			DBTRACE(1,"Int10 func 9 - Write char at curosr");			
            if(!avail) {
                DBTRACEx(0,"!OUTPUT BUFFER OVERFLOW, hVM", hVM);
                vm->overrun=true;
				break;
            }
            ch=_clientAL;
            RingBufWrite(&vm->out,&ch,1);
            vm->overrun=false;
			
#endif
			break;
	}

    return(FALSE);
}



DWORD SBBSExec::OnW32DeviceIoControl(PIOCTLPARAMS pIOCTL)
{
	DWORD	rd;
	DWORD	wr;
	DWORD	avail;
	vm_t*	vm;

//	DBTRACEd(0,"SBBSEXEC ioctl"
		//,pIOCTL->dioc_IOCtlCode);

	switch(pIOCTL->dioc_IOCtlCode) {
		case DIOC_OPEN:
			DBTRACEd(0,"IOCTL: OPEN",Get_System_Time());
			break;

		case DIOC_CLOSEHANDLE:
			DBTRACEd(0,"IOCTL: CLOSE",Get_System_Time());
			break;

		case SBBSEXEC_IOCTL_START:
			DBTRACEd(0,"IOCTL: START",Get_System_Time());
			DBTRACEx(0,"Current Thread Handle",Get_Cur_Thread_Handle());
			if(start.event) {
				DBTRACE(0,"Exec already started!");
				return(SBBSEXEC_ERROR_INUSE);
			}
			if (pIOCTL->dioc_InBuf==NULL 
				|| pIOCTL->dioc_cbInBuf!=sizeof(start)) {
				return(SBBSEXEC_ERROR_INBUF);
			}
			start=*(sbbsexec_start_t*)pIOCTL->dioc_InBuf;
			break;

		case SBBSEXEC_IOCTL_COMPLETE:
			DBTRACEd(0,"IOCTL: COMPLETE",Get_System_Time());
			if(start.event || new_vm==NULL) {
				DBTRACE(0,"!VM never created");
				start.event=0;
				return(SBBSEXEC_ERROR_INUSE);
			}
			if(pIOCTL->dioc_OutBuf==NULL
				|| pIOCTL->dioc_cbOutBuf<sizeof(VMHANDLE)) {
				DBTRACE(0,"!Invalid OUTBUF");
				return(SBBSEXEC_ERROR_OUTBUF);
			}
			*(VMHANDLE*)pIOCTL->dioc_OutBuf=new_vm->handle;
			DBTRACEx(0,"CREATED VM HANDLE", new_vm->handle);
			new_vm=NULL;

			if(pIOCTL->dioc_bytesret!=NULL)
				*pIOCTL->dioc_bytesret = sizeof(VMHANDLE);
			break;
		case SBBSEXEC_IOCTL_READ:

			if (pIOCTL->dioc_InBuf==NULL
				|| pIOCTL->dioc_cbInBuf!=sizeof(VMHANDLE)) {
				DBTRACE(0,"!INVALID INBUF");
				return(SBBSEXEC_ERROR_INBUF);
			}

			if (pIOCTL->dioc_OutBuf==NULL || pIOCTL->dioc_cbOutBuf==0) {
				DBTRACE(0,"!INVALID OUTBUF");
				return(SBBSEXEC_ERROR_OUTBUF);
			}

			vm = find_vm(*(VMHANDLE*)pIOCTL->dioc_InBuf);
			if(vm==NULL) {
				DBTRACE(0,"!NO VM LIST");
				return(SBBSEXEC_ERROR_INDATA);
			}

			rd = RingBufFull(&vm->out);

			if(rd>pIOCTL->dioc_cbOutBuf) {
				DBTRACEdd(0,"Reducing read size"
                	,rd, pIOCTL->dioc_cbOutBuf);
				rd=pIOCTL->dioc_cbOutBuf;
			}

			RingBufRead(&vm->out, (BYTE*)pIOCTL->dioc_OutBuf, rd);

			if(pIOCTL->dioc_bytesret!=NULL)
				*pIOCTL->dioc_bytesret = rd;

            if(vm->output_sem!=NULL) // Wake up int14 handler
            	Signal_Semaphore(vm->output_sem);

            if(rd>1) {
            	DBTRACEd(1,"IOCTL_READ bytes", rd);
            }
			break;

		case SBBSEXEC_IOCTL_WRITE:

			if (pIOCTL->dioc_InBuf==NULL
				|| pIOCTL->dioc_cbInBuf<sizeof(VMHANDLE)+1) {
				DBTRACE(0,"!INVALID INBUF");
				return(SBBSEXEC_ERROR_INBUF);
			}

			if (pIOCTL->dioc_OutBuf==NULL
				|| pIOCTL->dioc_cbOutBuf!=sizeof(DWORD)) {
				DBTRACE(0,"!INVALID OUTBUF");
				return(SBBSEXEC_ERROR_OUTBUF);
			}

			vm = find_vm(*(VMHANDLE*)pIOCTL->dioc_InBuf);
			if(vm==NULL) {
				DBTRACE(0,"!NO VM LIST");
				return(SBBSEXEC_ERROR_INDATA);
			}

			wr = pIOCTL->dioc_cbInBuf-sizeof(VMHANDLE);

			avail = RingBufFree(&vm->in);

			if(wr>avail) {
				DBTRACEdd(0,"Reducing write size", wr, avail);
				wr=avail;
			}

			RingBufWrite(&vm->in, (BYTE*)pIOCTL->dioc_InBuf+sizeof(VMHANDLE), wr);

			*(DWORD *)pIOCTL->dioc_OutBuf = wr;

			if(pIOCTL->dioc_bytesret!=NULL)
				*pIOCTL->dioc_bytesret = sizeof(DWORD);

            if(vm->input_sem!=NULL) // Wake up int14 handler
            	Signal_Semaphore(vm->input_sem);

			// Wake up the VDM (improves keyboard response - dramatically!)
			Wake_Up_VM(vm->handle);
			break;

		case SBBSEXEC_IOCTL_DISCONNECT:
			DBTRACEd(0,"IOCTL: DISCONNECT",Get_System_Time());

			if (pIOCTL->dioc_InBuf==NULL
				|| pIOCTL->dioc_cbInBuf!=sizeof(VMHANDLE)) {
				DBTRACE(0,"!INVALID INBUF");
				return(SBBSEXEC_ERROR_INBUF);
			}

			vm = find_vm(*(VMHANDLE*)pIOCTL->dioc_InBuf);
			if(vm==NULL) {
				DBTRACE(0,"!NO VM LIST");
				return(SBBSEXEC_ERROR_INDATA);
			}

			vm->online=false;

            if(vm->input_sem!=NULL) // Wake up int14 handler
            	Signal_Semaphore(vm->input_sem);
            if(vm->output_sem!=NULL) // Wake up int14 handler
            	Signal_Semaphore(vm->output_sem);
			break;

		case SBBSEXEC_IOCTL_STOP:
			DBTRACEd(0,"IOCTL: STOP",Get_System_Time());

			if (pIOCTL->dioc_InBuf==NULL
				|| pIOCTL->dioc_cbInBuf!=sizeof(VMHANDLE)) {
				DBTRACE(0,"!INVALID INBUF");
				return(SBBSEXEC_ERROR_INBUF);
			}

			vm = find_vm(*(VMHANDLE*)pIOCTL->dioc_InBuf);
			if(vm==NULL) {
				DBTRACE(0,"!NO VM LIST");
				return(SBBSEXEC_ERROR_INDATA);
			}

			DBTRACEx(0,"CLOSING VM HANDLE", vm->handle);
			vm->handle=NULL;	// Mark as available
			RingBufDispose(&vm->in);
			RingBufDispose(&vm->out);
            if(vm->input_sem!=NULL) // Wake up int14 handler
            	Signal_Semaphore(vm->input_sem);
            if(vm->output_sem!=NULL) // Wake up int14 handler
            	Signal_Semaphore(vm->output_sem);

			vm->input_sem=NULL;
			vm->output_sem=NULL;

			break;

		default:
			DBTRACEdx(0,"!UNKNOWN IOCTL"
				,pIOCTL->dioc_IOCtlCode,pIOCTL->dioc_IOCtlCode);
			return(SBBSEXEC_ERROR_IOCTL);

	}
	return (0);	// DEVIOCTL_NOERROR);
}
