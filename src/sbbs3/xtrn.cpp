/* xtrn.cpp */

/* Synchronet external program support routines */

/* $Id$ */

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

#include "sbbs.h"
#include "cmdshell.h"
#include "telnet.h"

/*****************************************************************************/
/* Interrupt routine to expand WWIV Ctrl-C# codes into ANSI escape sequences */
/*****************************************************************************/
BYTE* wwiv_expand(BYTE* buf, ulong buflen, BYTE* outbuf, ulong& newlen
	,ulong user_misc, bool& ctrl_c)
{
    char	ansi_seq[32];
	ulong 	i,j,k;

    for(i=j=0;i<buflen;i++) {
        if(buf[i]==3) {	/* ctrl-c, WWIV color escape char */
            ctrl_c=true;
            continue;
        }
        if(!ctrl_c) {
            outbuf[j++]=buf[i];
            continue;
        }
        ctrl_c=false;
        if(user_misc&ANSI) {
            switch(buf[i]) {
                default:
                    strcpy(ansi_seq,"\x1b[0m");          /* low grey */
                    break;
                case '1':
                    strcpy(ansi_seq,"\x1b[0;1;36m");     /* high cyan */
                    break;
                case '2':
                    strcpy(ansi_seq,"\x1b[0;1;33m");     /* high yellow */
                    break;
                case '3':
                    strcpy(ansi_seq,"\x1b[0;35m");       /* low magenta */
                    break;
                case '4':
                    strcpy(ansi_seq,"\x1b[0;1;44m");     /* white on blue */
                    break;
                case '5':
                    strcpy(ansi_seq,"\x1b[0;32m");       /* low green */
                    break;
                case '6':
                    strcpy(ansi_seq,"\x1b[0;1;5;31m");   /* high blinking red */
                    break;
                case '7':
                    strcpy(ansi_seq,"\x1b[0;1;34m");     /* high blue */
                    break;
                case '8':
                    strcpy(ansi_seq,"\x1b[0;34m");       /* low blue */
                    break;
                case '9':
                    strcpy(ansi_seq,"\x1b[0;36m");       /* low cyan */
                    break;
            }
            for(k=0;ansi_seq[k];k++)
                outbuf[j++]=ansi_seq[k];
        }
    }
    newlen=j;
    return(outbuf);
}

/*****************************************************************************/
// Escapes Telnet IAC (255) by doubling the IAC char
/*****************************************************************************/
BYTE* telnet_expand(BYTE* inbuf, ulong inlen, BYTE* outbuf, ulong& newlen, bool& iac)
{
	BYTE*   first_iac;
	ulong	i,outlen;

    first_iac=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);

	if(first_iac==NULL && !iac) {	/* Nothing to expand */
		newlen=inlen;
		return(inbuf);
	}

	if(first_iac!=NULL) {
		outlen=first_iac-inbuf;
		memcpy(outbuf, inbuf, outlen);
	} else
		outlen=0;

    for(i=outlen;i<inlen;i++) {
		if(iac==true) {
			outbuf[outlen++]=TELNET_IAC;
//			lprintf("Telnet IAC escaped");
		}
		if(inbuf[i]==TELNET_IAC)
			iac=true;
		else
			iac=false;
		outbuf[outlen++]=inbuf[i];
	}
    newlen=outlen;
    return(outbuf);
}


#ifdef _WIN32

#include "execvxd.h"	/* Win9X FOSSIL VxD API */

#define XTRN_IO_BUF_LEN 5000

extern SOCKET node_socket[];

// -------------------------------------------------------------------------
// GetAddressOfOpenVxDHandle
//
// This function returns the address of OpenVxDHandle. OpenVxDHandle is a 
// KERNEL32 function that returns a ring 0 event handle that corresponds to a
// given ring 3 event handle. The ring 0 handle can be used by VxDs to
// synchronize with the Win32 app.
//
typedef HANDLE (WINAPI *OPENVXDHANDLE)(HANDLE);

OPENVXDHANDLE GetAddressOfOpenVxDHandle(void)
{
	HINSTANCE hK32;

	if ((hK32 = LoadLibrary("KERNEL32")) == NULL)
		return NULL;

	return((OPENVXDHANDLE)GetProcAddress(hK32, "OpenVxDHandle"));
}

/****************************************************************************/
/* Runs an external program 												*/
/****************************************************************************/
int sbbs_t::external(char* cmdline, long mode, char* startup_dir)
{
	char	str[256],*p,*p_startup_dir;
	char	fname[128];
    char	fullcmdline[256];
	char	realcmdline[256];
	char	comspec_str[128];
	char	title[256];
	BYTE	buf[XTRN_IO_BUF_LEN],*bp;
    BYTE 	telnet_buf[XTRN_IO_BUF_LEN*2];
    BYTE 	wwiv_buf[XTRN_IO_BUF_LEN*2];
    bool	wwiv_flag=false;
	bool	telnet_flag=false;
    bool	native=false;			// DOS program by default
    bool	was_online=true;
    uint	i;
    time_t	hungup=0;
	bool	nt=false;
	HANDLE	vxd=INVALID_HANDLE_VALUE;
	HANDLE	rdslot=INVALID_HANDLE_VALUE;
	HANDLE	wrslot=INVALID_HANDLE_VALUE;
	HANDLE  start_event;
	HANDLE	hungup_event=NULL;
    PROCESS_INFORMATION process_info;
	DWORD	hVM;
	DWORD	rd;
    DWORD	wr;
    DWORD	len;
    DWORD	avail;
	DWORD	dummy;	
	DWORD	retval;
	DWORD	last_error;
	DWORD	loop_since_io=0;
	struct	tm * tm_p;
	sbbsexec_start_t start;
	OPENVXDHANDLE OpenVxDHandle;

	if(cmdline[0]=='*') {   /* Baja module */
		sprintf(str,"%.*s",sizeof(str)-1,cmdline+1);
		p=strchr(str,SP);
		if(p) {
			strcpy(main_csi.str,p+1);
			*p=0; }
		return(exec_bin(str,&main_csi)); 
	}

	attr(cfg.color[clr_external]);        /* setup default attributes */

    strcpy(str,cmdline);		/* Set str to program name only */
    p=strchr(str,SP);
    if(p) *p=0;
    strcpy(fname,str);

    p=strrchr(fname,'/');
    if(!p) p=strrchr(fname,'\\');
    if(!p) p=strchr(fname,':');
    if(!p) p=fname;
    else   p++;

    for(i=0;i<cfg.total_natvpgms;i++)
        if(!stricmp(p,cfg.natvpgm[i]->name))
            break;
    if(i<cfg.total_natvpgms || mode&EX_NATIVE)
        native=true;

	if(strcspn(cmdline,"<>|")!=strlen(cmdline)) 
		sprintf(comspec_str,"%s /C ", comspec);
	else
		comspec_str[0]=0;

    if(startup_dir && cmdline[1]!=':' && cmdline[0]!='/'
    	&& cmdline[0]!='\\' && cmdline[0]!='.')
       	sprintf(fullcmdline, "%s%s%s", comspec_str, startup_dir, cmdline);
    else
    	sprintf(fullcmdline, "%s%s", comspec_str, cmdline);

	strcpy(realcmdline, fullcmdline);	// for errormsg if failed to execute

	now=time(NULL);
	tm_p=gmtime(&now);

 	if(native) { // Native (32-bit) external

		// Current environment passed to child process
		sprintf(dszlog,"DSZLOG=%sPROTOCOL.LOG",cfg.node_dir);
		if(putenv(dszlog)) 		/* Makes the DSZ LOG active */
        	errormsg(WHERE,ERR_WRITE,"environment",0);
		sprintf(sbbsnode,"SBBSNODE=%s",cfg.node_dir);
		putenv(sbbsnode);		/* create environment var to contain node num */
		sprintf(sbbsnnum,"SBBSNNUM=%d",cfg.node_num);
		putenv(sbbsnnum);	   /* create environment var to contain node num */

    } else { // DOS external

		sprintf(str,"%sDOSXTRN.RET", cfg.node_dir);
		remove(str);

    	// Create temporary environment file
    	sprintf(str,"%sDOSXTRN.ENV", cfg.node_dir);
        FILE* fp=fopen(str,"w");
        if(fp==NULL) {
        	errormsg(WHERE, ERR_CREATE, str, 0);
            return(1);
        }
        fprintf(fp, "%s\n", fullcmdline);
		fprintf(fp, "DSZLOG=%sPROTOCOL.LOG\n", cfg.node_dir);
        fprintf(fp, "SBBSCTRL=%s\n", cfg.ctrl_dir);
        fprintf(fp, "SBBSNODE=%s\n", cfg.node_dir);
        fprintf(fp, "SBBSNNUM=%d\n", cfg.node_num);
		if(tm_p!=NULL) {
			fprintf(fp, "DAY=%02u\n", tm_p->tm_mday);
			fprintf(fp, "WEEKDAY=%s\n",wday[tm_p->tm_wday]);
			fprintf(fp, "MONTHNAME=%s\n",mon[tm_p->tm_mon]);
			fprintf(fp, "MONTH=%02u\n",tm_p->tm_mon+1);
			fprintf(fp, "YEAR=%u\n",1900+tm_p->tm_year);
		}
        fclose(fp);

        sprintf(fullcmdline, "%sDOSXTRN.EXE %s", cfg.exec_dir, str);

		if((retval=WaitForSingleObject(exec_mutex,5000))!=WAIT_OBJECT_0) {
			errormsg(WHERE, ERR_TIMEOUT, "exec_mutex", retval);
			return(GetLastError());
		}

		OpenVxDHandle=GetAddressOfOpenVxDHandle();

		if(!(mode&EX_OFFLINE) && OpenVxDHandle==NULL) {	// Windows NT/2000
			nt=true;
			i=SBBSEXEC_MODE_FOSSIL;
			if(mode&EX_INR)
           		i|=SBBSEXEC_MODE_DOS_IN;
			if(mode&EX_OUTR)
        		i|=SBBSEXEC_MODE_DOS_OUT;
			sprintf(str," NT %d %d",cfg.node_num,i);
			strcat(fullcmdline,str);

			sprintf(str,"sbbsexec_hungup%d",cfg.node_num);
			if((hungup_event=CreateEvent(
				 NULL	// pointer to security attributes
				,TRUE	// flag for manual-reset event
				,FALSE  // flag for initial state
				,str	// pointer to event-object name
				))==NULL) {
				last_error=GetLastError();
				ReleaseMutex(exec_mutex);
				SetLastError(last_error);
				errormsg(WHERE, ERR_CREATE, "exec start event", 0);
				return(GetLastError());
			}

			sprintf(str,"\\\\.\\mailslot\\sbbsexec\\rd%d"
				,cfg.node_num);
			rdslot=CreateMailslot(str
				,sizeof(buf)			// Maximum message size (0=unlimited)
				,0						// Read time-out
				,NULL);                 // Security
			if(rdslot==INVALID_HANDLE_VALUE) {
				last_error=GetLastError();
				ReleaseMutex(exec_mutex);
				CloseHandle(hungup_event);
				SetLastError(last_error);
				errormsg(WHERE, ERR_CREATE, str, 0);
				return(GetLastError());
			}
		}
		else if(!(mode&EX_OFFLINE)) {

   			// Load vxd to intercept interrupts

			sprintf(str,"\\\\.\\%s%s",cfg.exec_dir, SBBSEXEC_VXD);
			if((vxd=CreateFile(str,0,0,0
				,CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE,0))
				 ==INVALID_HANDLE_VALUE) {
				last_error=GetLastError();
				ReleaseMutex(exec_mutex);
				SetLastError(last_error);
				errormsg(WHERE, ERR_OPEN, str, 0);
				return(GetLastError());
			}

			if((start_event=CreateEvent(
				 NULL	// pointer to security attributes
				,TRUE	// flag for manual-reset event
				,FALSE  // flag for initial state
				,NULL	// pointer to event-object name
				))==NULL) {
				last_error=GetLastError();
				ReleaseMutex(exec_mutex);
				CloseHandle(vxd);
				SetLastError(last_error);
				errormsg(WHERE, ERR_CREATE, "exec start event", 0);
				return(GetLastError());
			}

			if(OpenVxDHandle!=NULL)
				start.event=OpenVxDHandle(start_event);
			else
				start.event=start_event;

			start.mode=SBBSEXEC_MODE_FOSSIL;
			if(mode&EX_INR)
           		start.mode|=SBBSEXEC_MODE_DOS_IN;
			if(mode&EX_OUTR)
        		start.mode|=SBBSEXEC_MODE_DOS_OUT;

			if(!DeviceIoControl(
				vxd,					// handle to device of interest
				SBBSEXEC_IOCTL_START,	// control code of operation to perform
				&start,					// pointer to buffer to supply input data
				sizeof(start),			// size of input buffer
				NULL,					// pointer to buffer to receive output data
				0,						// size of output buffer
				&rd,					// pointer to variable to receive output byte count
				NULL 					// Overlapped I/O
				)) {
				last_error=GetLastError();
				ReleaseMutex(exec_mutex);
				CloseHandle(vxd);
				SetLastError(last_error);
				errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_START);
				return(GetLastError());
			}
		}
    }

	if(startup_dir!=NULL && startup_dir[0])
		p_startup_dir=startup_dir;
	else
		p_startup_dir=NULL;
    STARTUPINFO startup_info={0};
    startup_info.cb=sizeof(startup_info);
	if(mode&EX_OFFLINE)
		startup_info.lpTitle=NULL;
	else {
		sprintf(title,"%s running %s on node %d"
			,useron.number ? useron.alias : "Event"
			,realcmdline
			,cfg.node_num);
		startup_info.lpTitle=title;
	}
    if(cfg.startup->options&BBS_OPT_XTRN_MINIMIZED) {
    	startup_info.wShowWindow=SW_SHOWMINNOACTIVE;
        startup_info.dwFlags|=STARTF_USESHOWWINDOW;
    }

	if(native) {
		/* temporary */
		FILE* fp;
		sprintf(fname,"%sDOOR32.SYS",cfg.node_dir);
		fp=fopen(fname,"wb");
		fprintf(fp,"2\r\n%d\r\n38400\r\nSynchronet %s%c\r\n%d\r\n%s\r\n%s\r\n%d\r\n%d\r\n"
			"%d\r\n%d\r\n"
			,client_socket_dup
			,VERSION,REVISION
			,useron.number
			,useron.name
			,useron.alias
			,useron.level
			,timeleft/60
			,useron.misc&ANSI ? 1 : 0
			,cfg.node_num);
		fclose(fp);

		/* not temporary */
		SuspendThread(input_thread);
	}

    if(!CreateProcess(
		NULL,			// pointer to name of executable module
		fullcmdline,  	// pointer to command line string
		NULL,  			// process security attributes
		NULL,   		// thread security attributes
		TRUE,  			// handle inheritance flag
		CREATE_NEW_CONSOLE|CREATE_SEPARATE_WOW_VDM, // creation flags
        NULL,  			// pointer to new environment block
		p_startup_dir,	// pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info  	// pointer to PROCESS_INFORMATION
		)) {
		last_error=GetLastError();
        ReleaseMutex(exec_mutex);
		if(vxd!=INVALID_HANDLE_VALUE)
			CloseHandle(vxd);
		if(hungup_event!=NULL)
			CloseHandle(hungup_event);
		if(native)
			ResumeThread(input_thread);
		SetLastError(last_error);
        errormsg(WHERE, ERR_EXEC, realcmdline, mode);
        return(GetLastError());
    }

	if(!native) {

		if(!(mode&EX_OFFLINE) && !nt) {
    		// Wait for notification from VXD that new VM has started
			if((retval=WaitForSingleObject(start_event, 5000))!=WAIT_OBJECT_0) {
				last_error=GetLastError();
				ReleaseMutex(exec_mutex);
				CloseHandle(vxd);
				CloseHandle(start_event);
				SetLastError(last_error);
				errormsg(WHERE, ERR_TIMEOUT, "start_event", retval);
				return(GetLastError());
			}

			CloseHandle(start_event);

			if(!DeviceIoControl(
				vxd,					// handle to device of interest
				SBBSEXEC_IOCTL_COMPLETE,	// control code of operation to perform
				NULL,					// pointer to buffer to supply input data
				0,						// size of input buffer
				&hVM,					// pointer to buffer to receive output data
				sizeof(hVM),			// size of output buffer
				&rd,					// pointer to variable to receive output byte count
				NULL					// Overlapped I/O
				)) {
				last_error=GetLastError();
				ReleaseMutex(exec_mutex);
				CloseHandle(vxd);
				SetLastError(last_error);
				errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_COMPLETE);
				return(GetLastError());
			}
		}
        ReleaseMutex(exec_mutex);
	}

    // Executing app, monitor
    retval=STILL_ACTIVE;
    while(1) {
        if(!online && !(mode&EX_OFFLINE)) { // Tell VXD/VDD and external that user hung-up
        	if(was_online) {
				logline("X!","Hung-up in external program");
            	hungup=time(NULL);
				if(!native) {
					if(nt)
						SetEvent(hungup_event);
					else if(!DeviceIoControl(
						vxd,		// handle to device of interest
						SBBSEXEC_IOCTL_DISCONNECT,	// operation to perform
						&hVM,		// pointer to buffer to supply input data
						sizeof(hVM),// size of input buffer
						NULL,		// pointer to buffer to receive output data
						0,			// size of output buffer
						&rd,		// pointer to variable to receive output byte count
						NULL		// Overlapped I/O
						)) {
						errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_DISCONNECT);
						break;
					}
				}
	            was_online=false;
            }
            if(hungup && time(NULL)-hungup>5) 
                TerminateProcess(process_info.hProcess, 2112);
        }
		if(native || mode&EX_OFFLINE) {
            if(GetExitCodeProcess(process_info.hProcess, &retval)==FALSE) {
                errormsg(WHERE, ERR_CHK, "ExitCodeProcess"
                   ,(DWORD)process_info.hProcess);
                break;
            }
            if(retval!=STILL_ACTIVE)
                break;
        	Sleep(500);
		} else {

			if(nt) {	// Windows NT/2000

				/* Write to VDD */

				wr=RingBufPeek(&inbuf,buf,sizeof(buf));
				if(wr) {
					if(wrslot==INVALID_HANDLE_VALUE) {
						sprintf(str,"\\\\.\\mailslot\\sbbsexec\\wr%d"
							,cfg.node_num);
						wrslot=CreateFile(str
							,GENERIC_WRITE
							,FILE_SHARE_READ
							,NULL
							,OPEN_EXISTING
							,FILE_ATTRIBUTE_NORMAL
							,(HANDLE) NULL);
#if 0
						if(wrslot==INVALID_HANDLE_VALUE) {
							errormsg(WHERE,ERR_OPEN,str,0);
							break;
						}
#endif
					}
					if(wrslot!=INVALID_HANDLE_VALUE
						&& WriteFile(wrslot,buf,wr,&len,NULL)==TRUE) {
						RingBufRead(&inbuf, NULL, len);
						wr=len;
					} else		// VDD not loaded yet
						wr=0;
				}

				/* Read from VDD */

				len=sizeof(buf);
				avail=RingBufFree(&outbuf);
				if(len>avail)
            		len=avail;
				if(ReadFile(rdslot,buf,len,&rd,NULL)==TRUE && rd>0) {
					if(mode&EX_WWIV) {
                		bp=wwiv_expand(buf, rd, wwiv_buf, rd, useron.misc, wwiv_flag);
						if(rd>sizeof(wwiv_buf))
							errorlog("WWIV_BUF OVERRUN");
					} else {
                		bp=telnet_expand(buf, rd, telnet_buf, rd, telnet_flag);
						if(rd>sizeof(telnet_buf))
							errorlog("TELNET_BUF OVERRUN");
					}
					RingBufWrite(&outbuf, bp, rd);
					sem_post(&output_sem);
				}
			} else {	// Windows 9x

				/* Write to VXD */

				wr=RingBufPeek(&inbuf, buf+sizeof(hVM),sizeof(buf)-sizeof(hVM));
				if(wr) {
					*(DWORD*)buf=hVM;
					wr+=sizeof(hVM);
					if(!DeviceIoControl(
						vxd,					// handle to device of interest
						SBBSEXEC_IOCTL_WRITE,	// control code of operation to perform
						buf,					// pointer to buffer to supply input data
						wr,						// size of input buffer
						&rd,					// pointer to buffer to receive output data
						sizeof(rd),				// size of output buffer
						&dummy,	 				// pointer to variable to receive output byte count
						NULL					// Overlapped I/O
						)) {
						errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_READ);
						break;
					}
					RingBufRead(&inbuf, NULL, rd);
					wr=rd;
				}
        		/* Read from VXD */
				rd=0;
				len=sizeof(buf);
				avail=RingBufFree(&outbuf);
				if(len>avail)
            		len=avail;
				if(len && avail>=RingBufFull(&outbuf)) {
					if(!DeviceIoControl(
						vxd,					// handle to device of interest
						SBBSEXEC_IOCTL_READ,	// control code of operation to perform
						&hVM,					// pointer to buffer to supply input data
						sizeof(hVM),			// size of input buffer
						buf,					// pointer to buffer to receive output data
						len,					// size of output buffer
						&rd,					// pointer to variable to receive output byte count
						NULL					// Overlapped I/O
						)) {
						errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_READ);
						break;
					}
#if 0
					if(rd>1)
                		lprintf("read %d bytes from xtrn", rd);
#endif

					if(mode&EX_WWIV)
                		bp=wwiv_expand(buf, rd, wwiv_buf, rd, useron.misc, wwiv_flag);
					else
                		bp=telnet_expand(buf, rd, telnet_buf, rd, telnet_flag);
					RingBufWrite(&outbuf, bp, rd);
					sem_post(&output_sem);
				}
			}
            if(!rd && !wr) {
				if(++loop_since_io>=1000) {
	                if(GetExitCodeProcess(process_info.hProcess, &retval)==FALSE) {
	                    errormsg(WHERE, ERR_CHK, "ExitCodeProcess"
						,(DWORD)process_info.hProcess);
						break;
					}
					if(retval!=STILL_ACTIVE)  {
#if 0
						if(hungup)
							Sleep(5000);	// Give the application time to close files
#endif
	                    break;
					}
					if(!(loop_since_io%3000)) {
//						OutputDebugString(".");
						getnodedat(cfg.node_num,&thisnode,0);
						if(thisnode.misc&NODE_INTR)
							break;
					}
					Sleep(1);
				}
            } else
				loop_since_io=0;
        }
	}

	if(!native && !(mode&EX_OFFLINE) && !nt) {
		if(!DeviceIoControl(
			vxd,					// handle to device of interest
			SBBSEXEC_IOCTL_STOP,	// control code of operation to perform
			&hVM,					// pointer to buffer to supply input data
			sizeof(hVM),			// size of input buffer
			NULL,					// pointer to buffer to receive output data
			0,						// size of output buffer
			&rd,					// pointer to variable to receive output byte count
			NULL					// Overlapped I/O
			)) {
			errormsg(WHERE, ERR_IOCTL, SBBSEXEC_VXD, SBBSEXEC_IOCTL_STOP);
		}
	}

    if(retval==STILL_ACTIVE)
	    TerminateProcess(process_info.hProcess, GetLastError());

    if(vxd!=INVALID_HANDLE_VALUE)
		CloseHandle(vxd);

	if(rdslot!=INVALID_HANDLE_VALUE)
		CloseHandle(rdslot);

	if(wrslot!=INVALID_HANDLE_VALUE)
		CloseHandle(wrslot);

	if(hungup_event!=NULL)
		CloseHandle(hungup_event);

	if(native)
		ResumeThread(input_thread);
	else {	// Get return value
    	sprintf(str,"%sDOSXTRN.RET", cfg.node_dir);
        FILE* fp=fopen(str,"r");
        if(fp!=NULL) {
			fscanf(fp,"%d",&retval);
			fclose(fp);
		}
	}

	curatr=0;	// Can't guarantee current attributes

//	lprintf("%s returned %d",realcmdline, retval);

	return(retval);
}

#else	/* !WIN32 */

int sbbs_t::external(char* cmdline, long mode, char* startup_dir)
{
	char	str[256];
	char	fname[128];
	char*	argv[30];
	char*	p;
    bool	native=false;			// DOS program by default
	int		i;
	int		argc;
		
	if(cmdline[0]=='*') {   /* Baja module */
		sprintf(str,"%.*s",sizeof(str)-1,cmdline+1);
		p=strchr(str,SP);
		if(p) {
			strcpy(main_csi.str,p+1);
			*p=0; }
		return(exec_bin(str,&main_csi)); 
	}

	attr(cfg.color[clr_external]);        /* setup default attributes */

    strcpy(str,cmdline);		/* Set str to program name only */
    p=strchr(str,SP);
    if(p) *p=0;
    strcpy(fname,str);

    p=strrchr(fname,'/');
    if(!p) p=strrchr(fname,'\\');
    if(!p) p=strchr(fname,':');
    if(!p) p=fname;
    else   p++;

    for(i=0;i<cfg.total_natvpgms;i++)
        if(!stricmp(p,cfg.natvpgm[i]->name))
            break;
    if(i<cfg.total_natvpgms || mode&EX_NATIVE)
        native=true;

	if(!native) {
		bprintf("\r\nExternal DOS programs are not yet supported in Synchronet for Linux\r\n");
		return(-1);
	}

 	if(native) { // Native (32-bit) external

		// Current environment passed to child process
		sprintf(dszlog,"DSZLOG=%sPROTOCOL.LOG",cfg.node_dir);
		if(putenv(dszlog)) 		/* Makes the DSZ LOG active */
        	errormsg(WHERE,ERR_WRITE,"environment",0);
		sprintf(sbbsnode,"SBBSNODE=%s",cfg.node_dir);
		putenv(sbbsnode);		/* create environment var to contain node num */
		sprintf(sbbsnnum,"SBBSNNUM=%d",cfg.node_num);
		putenv(sbbsnnum);	   /* create environment var to contain node num */

	} else {
		/* setup DOSemu env here */
	}

	if(startup_dir!=NULL && startup_dir[0])
		chdir(startup_dir);

	argv[0]=cmdline;	/* point to the beginning of the string */
	argc=1;
	for(i=0;cmdlien[i];i++)	/* Break up command line */
		if(cmdline[i]==SP) {
			argv[i]=0;			/* insert nulls */
			argv[argc++]=cmdline+i+1; /* point to the beginning of the next arg */
		}
	argv[argc]=0;

	i=execvp(argv[0],argv);

	return(i);
}

#endif	/* !WIN32 */

uint fakeriobp=0xffff;

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char * sbbs_t::cmdstr(char *instr, char *fpath, char *fspec, char *outstr)
{
	char	str[256],*cmd;
    int		i,j,len;

    if(outstr==NULL)
        cmd=cmdstr_output;
    else
        cmd=outstr;
    len=strlen(instr);
    for(i=j=0;i<len && j<sizeof(cmdstr_output);i++) {
        if(instr[i]=='%') {
            i++;
            cmd[j]=0;
            switch(toupper(instr[i])) {
                case 'A':   /* User alias */
                    strcat(cmd,useron.alias);
                    break;
                case 'B':   /* Baud (DTE) Rate */
                    strcat(cmd,ultoa(dte_rate,str,10));
                    break;
                case 'C':   /* Connect Description */
                    strcat(cmd,connection);
                    break;
                case 'D':   /* Connect (DCE) Rate */
                    strcat(cmd,ultoa((ulong)cur_rate,str,10));
                    break;
                case 'E':   /* Estimated Rate */
                    strcat(cmd,ultoa((ulong)cur_cps*10,str,10));
                    break;
                case 'F':   /* File path */
                    strcat(cmd,fpath);
                    break;
                case 'G':   /* Temp directory */
                    strcat(cmd,cfg.temp_dir);
                    break;
                case 'H':   /* Port Handle or Hardware Flow Control */
                    strcat(cmd,ultoa(client_socket_dup,str,10));
                    break;
                case 'I':   /* UART IRQ Line */
                    strcat(cmd,ultoa(cfg.com_irq,str,10));
                    break;
                case 'J':
                    strcat(cmd,cfg.data_dir);
                    break;
                case 'K':
                    strcat(cmd,cfg.ctrl_dir);
                    break;
                case 'L':   /* Lines per message */
                    strcat(cmd,ultoa(cfg.level_linespermsg[useron.level],str,10));
                    break;
                case 'M':   /* Minutes (credits) for user */
                    strcat(cmd,ultoa(useron.min,str,10));
                    break;
                case 'N':   /* Node Directory (same as SBBSNODE environment var) */
                    strcat(cmd,cfg.node_dir);
                    break;
                case 'O':   /* SysOp */
                    strcat(cmd,cfg.sys_op);
                    break;
                case 'P':   /* COM Port */
                    strcat(cmd,ultoa(online==ON_LOCAL ? 0:cfg.com_port,str,10));
                    break;
                case 'Q':   /* QWK ID */
                    strcat(cmd,cfg.sys_id);
                    break;
                case 'R':   /* Rows */
                    strcat(cmd,ultoa(rows,str,10));
                    break;
                case 'S':   /* File Spec */
                    strcat(cmd,fspec);
                    break;
                case 'T':   /* Time left in seconds */
                    gettimeleft();
                    strcat(cmd,ultoa(timeleft,str,10));
                    break;
                case 'U':   /* UART I/O Address (in hex) */
                    strcat(cmd,ultoa(cfg.com_base,str,16));
                    break;
                case 'V':   /* Synchronet Version */
                    sprintf(str,"%s%c",VERSION,REVISION);
                    break;
                case 'W':   /* Time-slice API type (mswtype) */
#if 0 //ndef __FLAT__
                    strcat(cmd,ultoa(mswtyp,str,10));
#endif
                    break;
                case 'X':
                    strcat(cmd,cfg.shell[useron.shell]->code);
                    break;
                case '&':   /* Address of msr */
                    sprintf(str,"%lu",&fakeriobp);
                    strcat(cmd,str);
                    break;
                case 'Y':
                    strcat(cmd,
                    comspec
                    );
                    break;
                case 'Z':
                    strcat(cmd,cfg.text_dir);
                    break;
                case '!':   /* EXEC Directory */
                    strcat(cmd,cfg.exec_dir);
                    break;
                case '#':   /* Node number (same as SBBSNNUM environment var) */
                    sprintf(str,"%d",cfg.node_num);
                    strcat(cmd,str);
                    break;
                case '*':
                    sprintf(str,"%03d",cfg.node_num);
                    strcat(cmd,str);
                    break;
                case '$':   /* Credits */
                    strcat(cmd,ultoa(useron.cdt+useron.freecdt,str,10));
                    break;
                case '%':   /* %% for percent sign */
                    strcat(cmd,"%");
                    break;
                default:    /* unknown specification */
                    if(isdigit(instr[i])) {
                        sprintf(str,"%0*d",instr[i]&0xf,useron.number);
                        strcat(cmd,str); }
                    break; }
            j=strlen(cmd); }
        else
            cmd[j++]=instr[i]; }
    cmd[j]=0;

    return(cmd);
}

