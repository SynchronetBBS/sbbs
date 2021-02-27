/* jsdoor.c */

/* Execute a BBS JavaScript module from the command-line */

/* $Id: jsdoor.c,v 1.8 2019/08/20 23:14:30 deuce Exp $ */

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

#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif

#ifdef __unix__
#define _WITH_GETLINE
#include <signal.h>
#endif

#include "sbbs.h"
#include "ciolib.h"
#include "ini_file.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "jsdebug.h"

scfg_t		scfg;

void* DLLCALL js_GetClassPrivate(JSContext *cx, JSObject *obj, JSClass* cls)
{
	void *ret = JS_GetInstancePrivate(cx, obj, cls, NULL);

	if(ret == NULL)
		JS_ReportError(cx, "'%s' instance: No Private Data or Class Mismatch"
			, cls == NULL ? "???" : cls->name);
	return ret;
}

void call_socket_open_callback(BOOL open)
{
}

SOCKET open_socket(int domain, int type, const char* protocol)
{
	SOCKET	sock;
	char	error[256];

	sock=socket(domain, type, IPPROTO_IP);
	if(sock!=INVALID_SOCKET && set_socket_options(&scfg, sock, protocol, error, sizeof(error)))
		lprintf(LOG_ERR,"%04d !ERROR %s",sock,error);

	return(sock);
}

SOCKET accept_socket(SOCKET s, union xp_sockaddr* addr, socklen_t* addrlen)
{
	SOCKET	sock;

	sock=accept(s,&addr->addr,addrlen);

	return(sock);
}

int close_socket(SOCKET sock)
{
	int		result;

	if(sock==INVALID_SOCKET || sock==0)
		return(0);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(result!=0 && ERROR_VALUE!=ENOTSOCK)
		lprintf(LOG_WARNING,"!ERROR %d closing socket %d",ERROR_VALUE,sock);
	return(result);
}

DLLEXPORT void DLLCALL sbbs_srand()
{
	DWORD seed;

	xp_randomize();
#if defined(HAS_DEV_RANDOM) && defined(RANDOM_DEV)
	int     rf,rd=0;

	if((rf=open(RANDOM_DEV, O_RDONLY|O_NONBLOCK))!=-1) {
		rd=read(rf, &seed, sizeof(seed));
		close(rf);
	}
	if (rd != sizeof(seed))
#endif
		seed = time32(NULL) ^ (uintmax_t)GetCurrentThreadId();

 	srand(seed);
	sbbs_random(10);	/* Throw away first number */
}

int DLLCALL sbbs_random(int n)
{
	return(xp_random(n));
}

JSBool
DLLCALL js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
{
	uint i;

	for(i=0;props[i].name;i++) 
		if(!JS_DefinePropertyWithTinyId(cx, obj, 
			props[i].name,props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
			return(JS_FALSE);

	return(JS_TRUE);
}


JSBool 
DLLCALL js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs)
{
	uint i;

	for(i=0;funcs[i].name;i++)
		if(!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
			return(JS_FALSE);
	return(JS_TRUE);
}

JSBool
DLLCALL js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	uint i;
	jsval	val;

	if(props) {
		for(i=0;props[i].name;i++) {
			if(name==NULL || strcmp(name, props[i].name)==0) {
				if(!JS_DefinePropertyWithTinyId(cx, obj, 
						props[i].name,props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
					return(JS_FALSE);
				if(name)
					return(JS_TRUE);
			}
		}
	}
	if(funcs) {
		for(i=0;funcs[i].name;i++) {
			if(name==NULL || strcmp(name, funcs[i].name)==0) {
				if(!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
					return(JS_FALSE);
				if(name)
					return(JS_TRUE);
			}
		}
	}
	if(consts) {
		for(i=0;consts[i].name;i++) {
			if(name==NULL || strcmp(name, consts[i].name)==0) {
				val=INT_TO_JSVAL(consts[i].val);

				if(!JS_DefineProperty(cx, obj, consts[i].name, val ,NULL, NULL, flags))
					return(JS_FALSE);

				if(name)
					return(JS_TRUE);
			}
		}
	}

	return(JS_TRUE);
}

// Needed for load()
JSObject* js_CreateBbsObject(JSContext* cx, JSObject* parent)
{
	return NULL;
}
JSObject* js_CreateConsoleObject(JSContext* cx, JSObject* parent)
{
	return NULL;
}

BOOL DLLCALL js_CreateCommonObjects(JSContext* js_cx
										,scfg_t *unused1
										,scfg_t *unused2
										,jsSyncMethodSpec* methods	/* global */
										,time_t uptime				/* system */
										,char* host_name			/* system */
										,char* socklib_desc			/* system */
										,js_callback_t* cb			/* js */
										,js_startup_t* js_startup	/* js */
										,client_t* client			/* client */
										,SOCKET client_socket		/* client */
										,CRYPT_CONTEXT session		/* client */
										,js_server_props_t* props	/* server */
										,JSObject** glob
										)
{
	BOOL	success=FALSE;

	/* Global Object */
	if(!js_CreateGlobalObject(js_cx, &scfg, methods, js_startup, glob))
		return(FALSE);

	do {
		/* System Object */
		if(js_CreateSystemObject(js_cx, *glob, &scfg, uptime, host_name, socklib_desc)==NULL)
			break;

		/* Internal JS Object */
		if(cb!=NULL 
			&& js_CreateInternalJsObject(js_cx, *glob, cb, js_startup)==NULL)
			break;

		/* Client Object */
		if(client!=NULL 
			&& js_CreateClientObject(js_cx, *glob, "client", client, client_socket, session)==NULL)
			break;

		/* Server */
		if(props!=NULL
			&& js_CreateServerObject(js_cx, *glob, props)==NULL)
			break;

		/* Socket Class */
		if(js_CreateSocketClass(js_cx, *glob)==NULL)
			break;

		/* Queue Class */
		if(js_CreateQueueClass(js_cx, *glob)==NULL)
			break;

		/* File Class */
		if(js_CreateFileClass(js_cx, *glob)==NULL)
			break;

		/* COM Class */
		if(js_CreateCOMClass(js_cx, *glob)==NULL)
			break;

		/* CryptContext Class */
		if(js_CreateCryptContextClass(js_cx, *glob)==NULL)
			break;

		/* CryptKeyset Class */
		if(js_CreateCryptKeysetClass(js_cx, *glob)==NULL)
			break;

		/* CryptCert Class */
		if(js_CreateCryptCertClass(js_cx, *glob)==NULL)
			break;

		success=TRUE;
	} while(0);

	if(!success)
		JS_RemoveObjectRoot(js_cx, glob);

	return(success);
}

#define PROG_NAME	"JSDoor"
#define PROG_NAME_LC	"jsdoor"
#define JSDOOR

#include "jsexec.c"
#include "js_system.c"
#include "ver.cpp"
