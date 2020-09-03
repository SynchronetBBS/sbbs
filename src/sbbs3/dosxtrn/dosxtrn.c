/* dosxtrn.c */

/* Synchronet External DOS Program Launcher (16-bit MSVC 1.52c project) */

/* $Id: dosxtrn.c,v 1.25 2020/04/15 08:22:33 rswindell Exp $ */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <dos.h>			/* _dos_set/getvect() */
#include <windows.h>		/* BOOL, etc. */
#include "vdd_func.h"
#include "execvxd.h"
#include "isvbop.h"			/* ddk\inc */
#include "fossdefs.h"

#define VDD_FILENAME	"sbbsexec.dll"

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
static void truncsp(char *str)
{
	size_t c;

	str[strcspn(str,"\t")]=0;
	c=strlen(str);
	while(c && (unsigned char)str[c-1]<=' ') c--;
	str[c]=0;
}
short	vdd=0;
BYTE	node_num=0;
int		mode=0;
int		revision;
char	id_string[128];
#ifdef DEBUG_INT_CALLS
ulong	int14calls=0;
ulong	int16calls=0;
ulong	int21calls=0;
ulong	int29calls=0;
#endif

void (interrupt *oldint14)();
void (interrupt *oldint16)();
void (interrupt *oldint21)();
void (interrupt *oldint29)();


static int vdd_buf(BYTE op, int count, WORD buf_seg, WORD buf_off)
{
	int retval;

	_asm {
		push	bx
		push	cx
		push	es
		push	di
		mov		ax,	vdd
		mov		bh,	node_num
		mov		bl,	op
		mov		cx,	count
		mov		es, buf_seg
		mov		di, buf_off
	}
	DispatchCall();
	_asm {
		mov		retval, ax
		pop		di
		pop		es
		pop		cx
		pop		bx
	}
	return(retval);
}

static int vdd_str(BYTE op, char* str)
{
	WORD			buf_seg;

	_asm mov buf_seg, ss;
	return vdd_buf(op, strlen(str), buf_seg, (WORD)str);
}

static int vdd_op(BYTE op)
{
	int retval;

#if FALSE	/* disable yield? */
	if(op==VDD_YIELD)
		return(0);
#endif
	_asm {
		push	bx
		mov		ax,	vdd
		mov		bh,	node_num
		mov		bl,	op
	}
	DispatchCall();
	_asm {
		mov		retval, ax
		pop		bx
	}
	return(retval);
}

union REGS inregs;
struct SREGS sregs;
BOOL inside_int14=FALSE;

/* This function is only necessary for naughty programs that call the vector
   directly instead of issuing an interrupt 
*/
void interrupt win95int14(
	unsigned _es, unsigned _ds,
	unsigned _di, unsigned _si,
	unsigned _bp, unsigned _sp,
	unsigned _bx, unsigned _dx,
	unsigned _cx, unsigned _ax,
    unsigned flags )
{
	union REGS outregs;

	/* prevent recursion, just incase the VXD isn't handling int14h */
	if(inside_int14)	
		return;

	inside_int14=TRUE;

	inregs.x.ax=_ax;
	inregs.x.bx=_bx;
	inregs.x.cx=_cx;
	inregs.x.dx=_dx;
	inregs.x.si=_si;
	inregs.x.di=_di;
	inregs.x.cflag=flags;

	sregs.es=_es;
	sregs.ds=_ds;

	int86x(0x14,&inregs,&outregs,&sregs);

	/* FOSSIL driver only touches these AX and BX */
	_ax= outregs.x.ax;
	_bx= outregs.x.bx;

	inside_int14=FALSE;
}

void vdd_getstatus(vdd_status_t* status)
{
	WORD			buf_seg;

	_asm mov buf_seg, ss;
	if(vdd_buf(VDD_STATUS, sizeof(vdd_status_t), buf_seg, (WORD)status)!=0)
		memset(status,0,sizeof(vdd_status_t));
}

WORD PortStatus()
{
	WORD			status=0x0008;			/* AL bit 3 (change in DCD) always set */
	vdd_status_t	vdd_status;

	vdd_getstatus(&vdd_status);

	if(vdd_status.online)			/* carrier detect */
		status|=0x0080;				/* DCD */

	if(vdd_status.inbuf_full)		/* receive data ready  */
		status|=0x0100;				/* RDA */

/*	if(vm->overrun)					/* overrun error detected */
/*		status|=0x0200;				/* OVRN */

	if(vdd_status.outbuf_full
		<vdd_status.outbuf_size/2)	/* room available in output buffer */
		status|=0x2000;				/* THRE */

	if(!vdd_status.outbuf_full)		/* output buffer is empty */
		status|=0x4000;				/* TSRE */

	return(status);
}

#ifdef DEBUG_FOSSIL_CALLS
	DWORD fossil_calls[0x100];
#endif

void interrupt winNTint14(
	unsigned _es, unsigned _ds,
	unsigned _di, unsigned _si,
	unsigned _bp, unsigned _sp,
	unsigned _bx, unsigned _dx,
	unsigned _cx, unsigned _ax,
	)
{
	BYTE			ch;
	BYTE far*		p;
	WORD			buf_seg;
	int				wr;
	vdd_status_t	vdd_status;
    fossil_info_t info = { 
		 sizeof(info)
		,FOSSIL_REVISION
		,revision	/* driver revision */
		,0			/* ID string pointer */	
		,0,0		/* receive buffer size/free (overwritten later) */
		,0,0		/* transmit buffer size/free (overwritten later) */
        ,80,25		/* screen dimensions (cols, rows) */
					/* port settings (i.e. 38400 N-8-1): */
        ,FOSSIL_BAUD_RATE_38400
		|FOSSIL_PARITY_NONE
		|FOSSIL_DATA_BITS_8
		|FOSSIL_STOP_BITS_1
	};

#ifdef DEBUG_INT_CALLS
	int14calls++;
#endif

#ifdef DEBUG_FOSSIL_CALLS
	fossil_calls[_ax>>8]++;
#endif

	switch(_ax>>8) {
		case FOSSIL_FUNC_SET_RATE:	/* Initialize/Set baud rate */
			_ax = PortStatus();
			break;
		case FOSSIL_FUNC_PUT_CHAR: /* write char to com port, with wait */
			ch=_ax&0xff;
			_asm mov buf_seg, ss;
			vdd_buf(VDD_WRITE, 1, buf_seg, (WORD)&ch);
			_ax = PortStatus();
			break;
		case FOSSIL_FUNC_GET_CHAR: /* read char from com port, with wait */
			_asm mov buf_seg, ss;
			_ax = vdd_buf(VDD_READ, 1, buf_seg, (WORD)&ch);
			if(!_ax) {
				_ax = 0x8000;	/* timed-out */
				vdd_op(VDD_YIELD);
			} else {
				_ax = ch;
			}
			break;
		case FOSSIL_FUNC_GET_STATUS:	/* request status */
			_ax=PortStatus();
			if(_ax==0x6088)
				vdd_op(VDD_MAYBE_YIELD);
			break;
		case FOSSIL_FUNC_INIT:	/* initialize */
			_ax=FOSSIL_SIGNATURE;	/* magic number = success */
			_bx=FOSSIL_REVISION<<8 | FOSSIL_FUNC_HIGHEST;	/* FOSSIL rev/maximum FOSSIL func supported */
			break;
		case FOSSIL_FUNC_DTR:
			if((_ax&0xff)==0)	/* Lower DTR */
				vdd_op(VDD_HANGUP);
			break;
        case FOSSIL_FUNC_FLUSH_OUT:	/* flush output buffer	*/
			break;
        case FOSSIL_FUNC_PURGE_OUT:	/* purge output buffer	*/
			vdd_op(VDD_OUTBUF_PURGE);
			break;
        case FOSSIL_FUNC_PURGE_IN:	/* purge input buffer	*/
			vdd_op(VDD_INBUF_PURGE);
			break;
		case FOSSIL_FUNC_WRITE_CHAR:	/* write char to com port, no wait */
        	if(0 /*RingBufFree(&vm->out)<2 */) {
            	_ax=0; /* char was not accepted */
                break;
            }
			ch=_ax&0xff;
			_asm mov buf_seg, ss;
			_ax = vdd_buf(VDD_WRITE, 1, buf_seg, (WORD)&ch);
			break;
        case FOSSIL_FUNC_PEEK:	/* non-destructive read-ahead */
			vdd_getstatus(&vdd_status);
			if(!vdd_status.inbuf_full) {
				_ax=0xffff;	/* no char available */
				vdd_op(VDD_YIELD);
				break;
			}
			_asm mov buf_seg, ss;
			_ax = vdd_buf(VDD_PEEK, 1, buf_seg, (WORD)&ch);
			if(_ax == 0)
				vdd_op(VDD_YIELD);
			break;
        case FOSSIL_FUNC_READ_BLOCK:	/* read block, no wait */
			vdd_getstatus(&vdd_status);
			if(!vdd_status.inbuf_full)
				_ax = 0; /* no data available */
			else
				_ax = vdd_buf(VDD_READ, _cx, _es, _di);
			if(_ax == 0)
				vdd_op(VDD_YIELD);
			break;
        case FOSSIL_FUNC_WRITE_BLOCK:	/* write block, no wait */
			_ax = vdd_buf(VDD_WRITE, _cx, _es, _di);
			break;
        case FOSSIL_FUNC_GET_INFO:	/* driver info */
			vdd_getstatus(&vdd_status);
			info.inbuf_size=vdd_status.inbuf_size;
			info.inbuf_free=info.inbuf_size-vdd_status.inbuf_full;
			info.outbuf_size=vdd_status.outbuf_size;
			info.outbuf_free=info.outbuf_size-vdd_status.outbuf_full;
			info.id_string = id_string;

			if(vdd_status.inbuf_full==vdd_status.outbuf_full==0)
				vdd_op(VDD_MAYBE_YIELD);

			p = _MK_FP(_es,_di);
            wr=sizeof(info);
            if(wr>_cx)
            	wr=_cx;
            _fmemcpy(p, &info, wr);
        	_ax=wr;
            break;
	}
}

void int14stub(void)
{
	/* This function will be overwritten later (during runtime) with FOSSIL signature */
}

void interrupt winNTint16(
	unsigned _es, unsigned _ds,
	unsigned _di, unsigned _si,
	unsigned _bp, unsigned _sp,
	unsigned _bx, unsigned _dx,
	unsigned _cx, unsigned _ax,
	unsigned _ip, unsigned _cs,
    unsigned flags )
{
	BYTE			ch;
	WORD			buf_seg;
	vdd_status_t	status;

#ifdef DEBUG_INT_CALLS
	int16calls++;
#endif

	vdd_getstatus(&status);
 	switch(_ax>>8) {
    	case 0x00:	/* Read char from keyboard */
        case 0x10:	/* Read char from enhanced keyboard */
			if(status.inbuf_full) {
				_asm mov buf_seg, ss;
				vdd_buf(VDD_READ, 1, buf_seg, (WORD)&ch);
				_ax=ch;
				return;
			} 
			vdd_op(VDD_MAYBE_YIELD);
			break;
    	case 0x01:	/* Get keyboard status */
        case 0x11:	/* Get enhanced keyboard status */
			if(status.inbuf_full) {
				_asm mov buf_seg, ss;
				vdd_buf(VDD_PEEK, 1, buf_seg, (WORD)&ch);
                flags&=~(1<<6);	/* clear zero flag */
                _ax=ch;
				return;
			}
			vdd_op(VDD_MAYBE_YIELD);
	        break;
	}

	_chain_intr(oldint16);		
}

#ifdef DEBUG_DOS_CALLS
	DWORD dos_calls[0x100];
#endif

void interrupt winNTint21(
	unsigned _es, unsigned _ds,
	unsigned _di, unsigned _si,
	unsigned _bp, unsigned _sp,
	unsigned _bx, unsigned _dx,
	unsigned _cx, unsigned _ax,
	)
{
#ifdef DEBUG_INT_CALLS
	int21calls++;
#endif
	if(_ax>>8 == 0x2c)	/* GET_SYSTEM_TIME */
		vdd_op(VDD_MAYBE_YIELD);
#ifdef DEBUG_DOS_CALLS
	dos_calls[_ax>>8]++;
#endif
	_chain_intr(oldint21);
}

void interrupt winNTint29(
	unsigned _es, unsigned _ds,
	unsigned _di, unsigned _si,
	unsigned _bp, unsigned _sp,
	unsigned _bx, unsigned _dx,
	unsigned _cx, unsigned _ax,
	)
{
	char	ch;
	WORD	buf_seg;
#ifdef DEBUG_INT_CALLS
	int29calls++;
#endif

	ch=_ax&0xff;
	_asm mov buf_seg, ss
	vdd_buf(VDD_WRITE, 1, buf_seg, (WORD)&ch);

	_chain_intr(oldint29);
}

/****************************************************************************/
/* Return the filename portion of a full pathname							*/
/****************************************************************************/
char* getfname(const char* path)
{
	const char* fname;
	const char* bslash;

	fname=strrchr(path,'/');
	bslash=strrchr(path,'\\');
	if(bslash>fname)
		fname=bslash;
	if(fname!=NULL) 
		fname++;
	else 
		fname=(char*)path;
	return((char*)fname);
}

char *	DllName		=VDD_FILENAME;
char *	InitFunc	="VDDInitialize";
char *	DispFunc	="VDDDispatch";
#define MAX_ENVVARS 32
#define MAX_ARGS	32

int main(int argc, char **argv)
{
	char	str[128];
	char	cmdline[128],*p;
	char	dll[256];
	char	exec_dir[128];
	char*	envvar[MAX_ENVVARS];
	char*	arg[MAX_ARGS];
	int		i,c,d,envnum=0;
	FILE*	fp;
	BOOL	NT=FALSE;
	BOOL	success=FALSE;
	WORD	buf_seg;
	WORD	w;

	sscanf("$Revision: 1.25 $", "%*s 1.%u", &revision);

	sprintf(id_string,"Synchronet FOSSIL Driver (DOSXTRN) revision %u", revision);
	if(argc<2) {
		fprintf(stderr
			,"%s - Copyright %s Rob Swindell\n"
			,id_string, __DATE__+7);
		fprintf(stderr
			,"usage: dosxtrn <path/dosxtrn.env> [NT|95] [node_num] [mode]\n");
		return(1);
	}

	sprintf(exec_dir,"%.*s",sizeof(exec_dir)-1,argv[0]);
	p=getfname(exec_dir);
	*p=0;
	sprintf(dll,"%s%s",exec_dir,VDD_FILENAME);
	DllName=dll;

	if(argc>2 && !strcmp(argv[2],"NT")) 
		NT=TRUE;
	if(argc>3)
		node_num=atoi(argv[3]);
	if(argc>4)
		mode=atoi(argv[4]);

	if((fp=fopen(argv[1],"r"))==NULL) {
		fprintf(stderr,"!Error opening %s\n",argv[1]);
		return(2);
	}

	fgets(cmdline, sizeof(cmdline), fp);
	truncsp(cmdline);

	arg[0]=cmdline;	/* point to the beginning of the string */
	for(c=0,d=1;cmdline[c] && d < (MAX_ARGS - 1);c++)	/* Break up command line */
		if(cmdline[c]==' ') {
			cmdline[c]=0;			/* insert nulls */
			arg[d++]=cmdline+c+1;	/* point to the beginning of the next arg */
		}
	arg[d]=0;

	while(!feof(fp) && envnum < MAX_ENVVARS) {
		if(!fgets(str, sizeof(str), fp))
			break;
		truncsp(str);
		if((envvar[envnum]=strdup(str))==NULL) {
			fprintf(stderr,"!MALLOC ERROR\n");
			return(4);
		}
		_putenv(envvar[envnum++]);
	}
	fclose(fp);

	/* Save int14 handler */
	oldint14=_dos_getvect(0x14);

	/* Overwrite stub function */
	((BYTE*)int14stub)[0] = 0xe9;	/* jump (relative) */
	((BYTE*)int14stub)[3] = 0x90;	/* NOP */
	((BYTE*)int14stub)[4] = 0x90;	/* NOP */
	((BYTE*)int14stub)[5] = 0x90;	/* NOP */
	((BYTE*)int14stub)[6] = FOSSIL_SIGNATURE&0xff;	/* FOSSIL sig (LSB) */
	((BYTE*)int14stub)[7] = FOSSIL_SIGNATURE>>8;	/* FOSSIL sig (MSB) */
	((BYTE*)int14stub)[8] = FOSSIL_FUNC_HIGHEST;	/* FOSSIL highest func supported */

	if(NT) {	/* Windows NT/2000 */

		for(i=0;i<2;i++) {

			/* Register VDD */
       		_asm {
				push	es
				push	ds
				pop		es
				mov     si, DllName		; ds:si = dll name
				mov     di, InitFunc    ; es:di = init routine
				mov     bx, DispFunc    ; ds:bx = dispatch routine
#if 1	/* Vista work-around, apparently doesn't support an InitFunc (RegisterModule fails with AX=1) */
				xor		di,di
				mov		es,di
#endif

			};
			RegisterModule();
			_asm {
				mov		vdd, ax
				jc		err
				mov		success, TRUE
				err:
				pop		es
			}
			if(success)
				break;
			DllName=VDD_FILENAME;	/* try again with no path (for Windows Vista) */
		}
		if(!success) {
			fprintf(stderr,"Error %d loading %s\n",vdd,DllName);
			return(-1);
		}

#if 0
		fprintf(stderr,"vdd handle=%d\n",vdd);
		fprintf(stderr,"mode=%d\n",mode);
#endif
		vdd_str(VDD_LOAD_INI_FILE, exec_dir);

		vdd_str(VDD_LOAD_INI_SECTION, getfname(arg[0]));

		sprintf(str,"%s, rev %u, %s %s", __FILE__, revision, __DATE__, __TIME__);
		vdd_str(VDD_DEBUG_OUTPUT, str);

		i=vdd_op(VDD_OPEN);
		if(i) {
			fprintf(stderr,"!VDD_OPEN ERROR: %d\n",i);
			UnRegisterModule();
			return(-1);
		}
		oldint16=_dos_getvect(0x16);
		oldint21=_dos_getvect(0x21);
		oldint29=_dos_getvect(0x29);
		if(mode==SBBSEXEC_MODE_FOSSIL) {
			*(WORD*)((BYTE*)int14stub+1) = (WORD)winNTint14 - (WORD)&int14stub - 3;	/* jmp offset */
			_dos_setvect(0x14,(void(interrupt *)())int14stub); 
		}
		_dos_setvect(0x21,winNTint21); 
		if(mode&SBBSEXEC_MODE_DOS_IN)
			_dos_setvect(0x16,winNTint16); 
		if(mode&SBBSEXEC_MODE_DOS_OUT) 
			_dos_setvect(0x29,winNTint29); 
	}
	else if(mode==SBBSEXEC_MODE_FOSSIL)	{ /* Windows 95/98/Millennium */
		*(WORD*)((BYTE*)int14stub+1) = (WORD)win95int14 - (WORD)&int14stub - 3;		/* jmp offset */
		_dos_setvect(0x14,(void(interrupt *)())int14stub); 
	}

	_heapmin();
	i=_spawnvp(_P_WAIT, arg[0], arg);

	p=argv[1]+(strlen(argv[1])-3);
	strcpy(p,"RET");
	if((fp=fopen(argv[1],"w+"))==NULL) {
		fprintf(stderr,"!Error opening %s\n",argv[1]);
		return(3);
	}
	fprintf(fp,"%d",i);

	/* Restore original ISRs */
	_dos_setvect(0x14,oldint14);

	if(NT) {
		vdd_op(VDD_CLOSE);

		_dos_setvect(0x16,oldint16);
		_dos_setvect(0x21,oldint21);
		_dos_setvect(0x29,oldint29);

		sprintf(str,"%s returned %d", arg[0], i);
		vdd_str(VDD_DEBUG_OUTPUT, str);

#ifdef DEBUG_INT_CALLS
		sprintf(str,"int14h calls: %u", int14calls);	vdd_str(VDD_DEBUG_OUTPUT, str);
		sprintf(str,"int16h calls: %u", int16calls);	vdd_str(VDD_DEBUG_OUTPUT, str);
		sprintf(str,"int21h calls: %u", int21calls);	vdd_str(VDD_DEBUG_OUTPUT, str);
		sprintf(str,"int29h calls: %u", int29calls);	vdd_str(VDD_DEBUG_OUTPUT, str);
#endif
#ifdef DEBUG_DOS_CALLS
		for(i=0;i<0x100;i++) {
			if(dos_calls[i]>100) {
				sprintf(str,"int21h function %02X calls: %u\n"
					,i, dos_calls[i]);
				vdd_str(VDD_DEBUG_OUTPUT, str);
			}
		}
#endif
#ifdef DEBUG_FOSSIL_CALLS
		for(i=0;i<0x100;i++) {
			if(fossil_calls[i]>0) {
				sprintf(str,"int14h function %02X calls: %u"
					,i, fossil_calls[i]);
				vdd_str(VDD_DEBUG_OUTPUT, str);			}
		}
#endif

		/* Unregister VDD */
		_asm mov ax, vdd;
		UnRegisterModule();

	}

	return(i);
}
