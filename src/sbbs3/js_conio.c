/* js_conio.c */

/* Synchronet "conio" (console IO) object */

/* $Id: js_conio.c,v 1.38 2020/04/12 20:30:48 rswindell Exp $ */

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

#include "sbbs.h"
#include "ciolib.h"
#include "js_request.h"

/* Properties */
enum {
	 PROP_WSCROLL
	,PROP_DIRECTVIDEO
	,PROP_HOLD_UPDATE
	,PROP_PUTTEXT_CAN_MOVE
	,PROP_MODE				/* read-only */
    ,PROP_MOUSE				/* read-only */
	,PROP_ESCDELAY
	,PROP_TEXTATTR
	,PROP_KBHIT
	,PROP_WHEREX
	,PROP_WHEREY
	,PROP_TEXTMODE
	,PROP_WINLEFT
	,PROP_WINTOP
	,PROP_WINRIGHT
	,PROP_WINBOTTOM
	,PROP_SCREENWIDTH
	,PROP_SCREENHEIGHT
	,PROP_NORMATTR
	,PROP_TEXTBACKGROUND
	,PROP_TEXTCOLOR
	,PROP_CLIPBOARD
	,PROP_HIGHVIDEO
	,PROP_LOWVIDEO
};

static JSBool js_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint		tiny;
	jsrefcount	rc;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	rc=JS_SUSPENDREQUEST(cx);
	switch(tiny) {
		case PROP_WSCROLL:
			*vp=BOOLEAN_TO_JSVAL(_wscroll);
			break;
		case PROP_DIRECTVIDEO:
			*vp=BOOLEAN_TO_JSVAL(directvideo);
			break;
		case PROP_HOLD_UPDATE:
			*vp=BOOLEAN_TO_JSVAL(hold_update);
			break;
		case PROP_PUTTEXT_CAN_MOVE:
			*vp=BOOLEAN_TO_JSVAL(puttext_can_move);
			break;
		case PROP_MODE:
			*vp=INT_TO_JSVAL(cio_api.mode);
			break;
		case PROP_MOUSE:
			*vp=BOOLEAN_TO_JSVAL(cio_api.mouse);
			break;
		case PROP_ESCDELAY:
			*vp=INT_TO_JSVAL(cio_api.ESCDELAY?*cio_api.ESCDELAY:0);
			break;
		case PROP_TEXTATTR:
			*vp=INT_TO_JSVAL(cio_textinfo.attribute);
			break;
		case PROP_KBHIT:
			*vp=BOOLEAN_TO_JSVAL(kbhit());
			break;
		case PROP_WHEREX:
			*vp=INT_TO_JSVAL(cio_textinfo.curx);
			break;
		case PROP_WHEREY:
			*vp=INT_TO_JSVAL(cio_textinfo.cury);
			break;
		case PROP_TEXTMODE:
			*vp=INT_TO_JSVAL(cio_textinfo.currmode);
			break;
		case PROP_WINLEFT:
			*vp=INT_TO_JSVAL(cio_textinfo.winleft);
			break;
		case PROP_WINTOP:
			*vp=INT_TO_JSVAL(cio_textinfo.wintop);
			break;
		case PROP_WINRIGHT:
			*vp=INT_TO_JSVAL(cio_textinfo.winright);
			break;
		case PROP_WINBOTTOM:
			*vp=INT_TO_JSVAL(cio_textinfo.winbottom);
			break;
		case PROP_SCREENWIDTH:
			*vp=INT_TO_JSVAL(cio_textinfo.screenwidth);
			break;
		case PROP_SCREENHEIGHT:
			*vp=INT_TO_JSVAL(cio_textinfo.screenheight);
			break;
		case PROP_NORMATTR:
			*vp=INT_TO_JSVAL(cio_textinfo.normattr);
			break;
		case PROP_TEXTBACKGROUND:
			*vp=INT_TO_JSVAL((cio_textinfo.attribute & 0x70)>>4);
			break;
		case PROP_TEXTCOLOR:
			*vp=INT_TO_JSVAL(cio_textinfo.attribute & 0xf);
			break;
		case PROP_CLIPBOARD:
			*vp=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,getcliptext()));
			break;
		case PROP_HIGHVIDEO:
			*vp=BOOLEAN_TO_JSVAL(cio_textinfo.attribute & 0x8);
			break;
		case PROP_LOWVIDEO:
			*vp=BOOLEAN_TO_JSVAL(!(cio_textinfo.attribute & 0x8));
			break;
	}
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool js_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint		tiny;
	int32		i=0;
	JSBool		b;
	jsrefcount	rc;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case PROP_WSCROLL:
			JS_ValueToBoolean(cx, *vp, &_wscroll);
			break;
		case PROP_DIRECTVIDEO:
			JS_ValueToBoolean(cx, *vp, &directvideo);
			break;
		case PROP_HOLD_UPDATE:
			JS_ValueToBoolean(cx, *vp, &hold_update);
			break;
		case PROP_PUTTEXT_CAN_MOVE:
			JS_ValueToBoolean(cx, *vp, &puttext_can_move);
			break;
		case PROP_ESCDELAY:
			if(cio_api.ESCDELAY) {
				if(!JS_ValueToInt32(cx, *vp, (int32*)cio_api.ESCDELAY))
					return JS_FALSE;
			}
			break;
		case PROP_TEXTATTR:
			if(!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			rc=JS_SUSPENDREQUEST(cx);
			textattr(i);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case PROP_WHEREX:
			if(!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			rc=JS_SUSPENDREQUEST(cx);
			gotoxy(i, cio_textinfo.cury);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case PROP_WHEREY:
			if(!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			rc=JS_SUSPENDREQUEST(cx);
			gotoxy(cio_textinfo.curx, i);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case PROP_TEXTMODE:
			if(!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			rc=JS_SUSPENDREQUEST(cx);
			textmode(i);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case PROP_TEXTBACKGROUND:
			if(!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			rc=JS_SUSPENDREQUEST(cx);
			textbackground(i);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case PROP_TEXTCOLOR:
			if(!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			rc=JS_SUSPENDREQUEST(cx);
			textcolor(i);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case PROP_CLIPBOARD:
			{
				size_t	len;
				char	*bytes = NULL;

				JSVALUE_TO_MSTRING(cx, *vp, bytes, &len);
				HANDLE_PENDING(cx, bytes);
				if(!bytes)
					return JS_FALSE;
				rc=JS_SUSPENDREQUEST(cx);
				copytext(bytes, len+1);
				free(bytes);
				JS_RESUMEREQUEST(cx, rc);
			}
			break;
		case PROP_HIGHVIDEO:
			JS_ValueToBoolean(cx, *vp, &b);
			rc=JS_SUSPENDREQUEST(cx);
			if(b)
				highvideo();
			else
				lowvideo();
			JS_RESUMEREQUEST(cx, rc);
			break;
		case PROP_LOWVIDEO:
			JS_ValueToBoolean(cx, *vp, &b);
			rc=JS_SUSPENDREQUEST(cx);
			if(b)
				lowvideo();
			else
				highvideo();
			JS_RESUMEREQUEST(cx, rc);
			break;
	}

	return(JS_TRUE);
}

static jsSyncPropertySpec js_properties[] = {
/*		 name,				tinyid,						flags,		ver	*/

	{	"wscroll",			PROP_WSCROLL,			JSPROP_ENUMERATE,   315 },
	{	"directvideo",		PROP_DIRECTVIDEO,		JSPROP_ENUMERATE,	315 },
	{	"hold_update",		PROP_HOLD_UPDATE,		JSPROP_ENUMERATE,	315 },
	{	"puttext_can_move",	PROP_PUTTEXT_CAN_MOVE,	JSPROP_ENUMERATE,	315 },
	{	"mode",				PROP_MODE,				JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"mouse",			PROP_MOUSE,				JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"ESCDELAY",			PROP_ESCDELAY,			JSPROP_ENUMERATE,	315 },
	{	"textattr",			PROP_TEXTATTR,			JSPROP_ENUMERATE,	315 },
	{	"kbhit",			PROP_KBHIT,				JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"wherex",			PROP_WHEREX,			JSPROP_ENUMERATE,	315 },
	{	"wherey",			PROP_WHEREY,			JSPROP_ENUMERATE,	315 },
	{	"textmode",			PROP_TEXTMODE,			JSPROP_ENUMERATE,	315 },
	{	"winleft",			PROP_WINLEFT,			JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"wintop",			PROP_WINTOP,			JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"winright",			PROP_WINRIGHT,			JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"winbottom",		PROP_WINBOTTOM,			JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"screenwidth",		PROP_SCREENWIDTH,		JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"screenheight",		PROP_SCREENHEIGHT,		JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"normattr",			PROP_NORMATTR,			JSPROP_ENUMERATE|JSPROP_READONLY,	315 },
	{	"textbackground",	PROP_TEXTBACKGROUND,	JSPROP_ENUMERATE,	315 },
	{	"textcolor",		PROP_TEXTCOLOR,			JSPROP_ENUMERATE,	315 },
	{	"clipboard",		PROP_CLIPBOARD,			JSPROP_ENUMERATE,	315 },
	{	"lowvideo",			PROP_LOWVIDEO,			JSPROP_ENUMERATE,	315 },
	{	"highvideo",		PROP_HIGHVIDEO,			JSPROP_ENUMERATE,	315 },
	{0}
};

/* Methods */

static JSBool
js_conio_init(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int			ciolib_mode=CIOLIB_MODE_AUTO;
	char		mode[7];
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if(argc>0) {
		JSVALUE_TO_STRBUF(cx, argv[0], mode, sizeof(mode), NULL);
		if(!stricmp(mode,"AUTO"))
			ciolib_mode=CIOLIB_MODE_AUTO;
		else if(!stricmp(mode,"STDIO"))
			ciolib_mode=-1;
		else if(!stricmp(mode,"X"))
			ciolib_mode=CIOLIB_MODE_X;
		else if(!stricmp(mode,"ANSI"))
			ciolib_mode=CIOLIB_MODE_ANSI;
		else if(!stricmp(mode,"CONIO"))
			ciolib_mode=CIOLIB_MODE_CONIO;
		else if(!stricmp(mode,"CONIO_FULLSCREEN"))
			ciolib_mode=CIOLIB_MODE_CONIO_FULLSCREEN;
		else if(!stricmp(mode,"CURSES"))
			ciolib_mode=CIOLIB_MODE_CURSES;
		else if(!stricmp(mode,"CURSES_IBM"))
			ciolib_mode=CIOLIB_MODE_CURSES_IBM;
		else if(!stricmp(mode,"CURSES_ACSCII"))
			ciolib_mode=CIOLIB_MODE_CURSES_ASCII;
		else if(!stricmp(mode,"SDL"))
			ciolib_mode=CIOLIB_MODE_SDL;
		else if(!stricmp(mode,"SDL_FULLSCREEN"))
			ciolib_mode=CIOLIB_MODE_SDL_FULLSCREEN;
		else {
			JS_ReportError(cx, "Unhandled ciolib mode \"%s\"", mode);
			return JS_FALSE;
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(initciolib(ciolib_mode)) {
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_suspend(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
	suspendciolib();
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_clreol(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
	clreol();
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_clrscr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
	clrscr();
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_wscroll(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
	wscroll();
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_delline(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
	delline();
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_insline(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
	insline();
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_normvideo(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
	normvideo();
    JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_getch(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
    JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(getch()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_getche(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
    JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(getche()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_beep(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
    	beep();
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_getfont(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32	fnum;
	jsrefcount	rc;

	if(argc==1 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&fnum)) {
		rc=JS_SUSPENDREQUEST(cx);
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(getfont(fnum)));
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_hidemouse(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
    JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(hidemouse()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_showmouse(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	rc=JS_SUSPENDREQUEST(cx);
    JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(showmouse()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_setcursortype(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32	type;
	jsrefcount	rc;

	if(argc==1 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&type)) {
		rc=JS_SUSPENDREQUEST(cx);
		_setcursortype(type);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_gotoxy(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32	x,y;
	jsrefcount	rc;

	if(argc >= 2 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&x)
				&& JSVAL_IS_NUMBER(argv[1]) && JS_ValueToInt32(cx,argv[1],&y)) {
		rc=JS_SUSPENDREQUEST(cx);
		gotoxy(x,y);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	JS_ReportError(cx, "Insufficient Arguments");

	return(JS_FALSE);
}

static JSBool
js_conio_putch(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32	ch;
	jsrefcount	rc;

	if(argc==1 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&ch)) {
		rc=JS_SUSPENDREQUEST(cx);
		JS_SET_RVAL(cx, arglist,INT_TO_JSVAL(putch(ch)));
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_ungetch(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32	ch;
	jsrefcount	rc;

	if(argc==1 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&ch)) {
		rc=JS_SUSPENDREQUEST(cx);
		JS_SET_RVAL(cx, arglist,INT_TO_JSVAL(ungetch(ch)));
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_loadfont(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char *	str = NULL;
	jsrefcount	rc;

	if(argc==1) {
		JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
		HANDLE_PENDING(cx, str);
		if(str != NULL) {
			rc=JS_SUSPENDREQUEST(cx);
			JS_SET_RVAL(cx, arglist,INT_TO_JSVAL(loadfont(str)));
			free(str);
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);
		}
	}

	return(JS_FALSE);
}

static JSBool
js_conio_settitle(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char *	str = NULL;
	jsrefcount	rc;

	if(argc==1) {
		JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
		HANDLE_PENDING(cx, str);
		if(str != NULL) {
			rc=JS_SUSPENDREQUEST(cx);
			settitle(str);
			free(str);
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist,JSVAL_TRUE);
			return(JS_TRUE);
		}
	}

	return(JS_FALSE);
}

static JSBool
js_conio_setname(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char *	str = NULL;
	jsrefcount	rc;

	if(argc==1) {
		JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
		HANDLE_PENDING(cx, str);
		if(str != NULL) {
			rc=JS_SUSPENDREQUEST(cx);
			setname(str);
			free(str);
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist,JSVAL_TRUE);
			return(JS_TRUE);
		}
	}

	return(JS_FALSE);
}

static JSBool
js_conio_cputs(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char *	str = NULL;
	jsrefcount	rc;

	if(argc==1) {
		JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
		HANDLE_PENDING(cx, str);
		if(str != NULL) {
			rc=JS_SUSPENDREQUEST(cx);
			JS_SET_RVAL(cx, arglist,INT_TO_JSVAL(cputs(str)));
			free(str);
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);
		}
	}

	return(JS_FALSE);
}

static JSBool
js_conio_setfont(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32	font;
	int force=JS_FALSE;
	int32 fnum=0;
	jsrefcount	rc;
	uintN arg=0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc > 2)
		return(JS_FALSE);

	if(argc > 0 && JSVAL_IS_NUMBER(argv[arg]) && JS_ValueToInt32(cx,argv[arg],&font)) {
		for(arg=1; arg<argc; arg++) {
			if(JSVAL_IS_NUMBER(argv[arg])) {
				if(!JS_ValueToInt32(cx,argv[arg],&fnum))
					return(JS_FALSE);
			}
			else if(JSVAL_IS_BOOLEAN(argv[arg])) {
				if(!JS_ValueToBoolean(cx, argv[1], &force))
					return(JS_FALSE);
			}
			else
				return(JS_FALSE);
		}
		rc=JS_SUSPENDREQUEST(cx);
		JS_SET_RVAL(cx, arglist,INT_TO_JSVAL(setfont(font, force,fnum)));
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_getpass(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char *	str = NULL;
	char *	pwd;
	jsrefcount	rc;

	if(argc==1) {
		JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
		HANDLE_PENDING(cx, str);
		if(str != NULL) {
			rc=JS_SUSPENDREQUEST(cx);
			pwd=getpass(str);
			free(str);
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,pwd)));
			return(JS_TRUE);
		}
	}

	return(JS_FALSE);
}

static JSBool
js_conio_window(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32 left=1;
	int32 top=1;
	int32 right=cio_textinfo.screenwidth;
	int32 bottom=cio_textinfo.screenheight;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc > 4)
		return(JS_FALSE);

	if(argc > 0) {
		if(!JSVAL_IS_NUMBER(argv[0]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[0], &left))
			return(JS_FALSE);
	}
	if(argc > 1) {
		if(!JSVAL_IS_NUMBER(argv[1]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[1], &top))
			return(JS_FALSE);
	}
	if(argc > 2) {
		if(!JSVAL_IS_NUMBER(argv[2]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[2], &right))
			return(JS_FALSE);
	}
	if(argc > 3) {
		if(!JSVAL_IS_NUMBER(argv[3]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[3], &bottom))
			return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	window(left, top, right, bottom);
	JS_RESUMEREQUEST(cx, rc);
	if(cio_textinfo.winleft == left
			&& cio_textinfo.winright==right
			&& cio_textinfo.wintop==top
			&& cio_textinfo.winbottom==bottom) {
		JS_SET_RVAL(cx, arglist,JSVAL_TRUE);
	}
	else {
		JS_SET_RVAL(cx, arglist,JSVAL_FALSE);
	}
	return(JS_TRUE);
}

static JSBool
js_conio_cgets(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char	buf[258];
	int32	maxlen=255;
	char	*ret;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc > 1)
		return(JS_FALSE);
	if(argc > 0) {
		if(!JSVAL_IS_NUMBER(argv[0]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[0], &maxlen))
			return(JS_FALSE);
		if(maxlen > 255)
			return(JS_FALSE);
	}
	buf[0]=(char)maxlen;
	rc=JS_SUSPENDREQUEST(cx);
	ret=cgets(buf);
	JS_RESUMEREQUEST(cx, rc);
	if(ret==NULL)
		JS_SET_RVAL(cx, arglist,JSVAL_NULL);
	else {
		buf[257]=0;
		JS_SET_RVAL(cx, arglist,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,ret)));
	}

	return(JS_TRUE);
}

/* TODO: cprintf() not really needed since we have cputs() and format() */
#if 0
static JSBool
js_conio_cprintf(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
}
#endif

/* TODO: cscanf()... this would be rather insane actually... */
#if 0
static JSBool
js_conio_cscanf(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
}
#endif

static JSBool
js_conio_movetext(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int		i;
	int32	args[6];
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc != 6)
		return(JS_FALSE);
	for(i=0; i<6; i++) {
		if(!JSVAL_IS_NUMBER(argv[i]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[i], &args[i]))
			return(JS_FALSE);
	}
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist,BOOLEAN_TO_JSVAL(movetext(args[0], args[1], args[2], args[3], args[4], args[5])));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_puttext(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32	args[4];
	unsigned char	*buffer;
	jsuint	i;
	int32	j;
	jsuint	size;
	jsval	val;
	JSObject *array;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	/* default values: */
	args[0]=1;
	args[1]=1;
	args[2]=cio_textinfo.screenwidth;
	args[3]=cio_textinfo.screenheight;

	if(argc != 5)
		return(JS_FALSE);
	for(i=0; i<4; i++) {
		if(!JSVAL_IS_NUMBER(argv[i]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[i], &args[i]))
			return(JS_FALSE);
	}
	if(args[0] < 1 || args[1] < 1 || args[2] < 1 || args[3] < 1
			|| args[0] > args[2] || args[1] > args[3]
			|| args[2] > cio_textinfo.screenwidth || args[3] > cio_textinfo.screenheight) {
		JS_SET_RVAL(cx, arglist,JSVAL_FALSE);
		return(JS_TRUE);
	}

	if(!JSVAL_IS_OBJECT(argv[4]))
		return(JS_FALSE);
	array=JSVAL_TO_OBJECT(argv[4]);
	if(!JS_GetArrayLength(cx, array, &size))
		return(JS_FALSE);

	buffer=(unsigned char *)malloc(size);
	if(buffer==NULL) 
		return(JS_FALSE);
	for(i=0; i<size; i++) {
		if(!JS_GetElement(cx, array, i, &val)) {
			free(buffer);
			return(JS_FALSE);
		}
		if(!JSVAL_IS_NUMBER(val)) {
			free(buffer);
			return(JS_FALSE);
		}
		if(!JS_ValueToInt32(cx, val, &j)) {
			free(buffer);
			return(JS_FALSE);
		}
		buffer[i]=(unsigned char)j;
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist,BOOLEAN_TO_JSVAL(puttext(args[0], args[1], args[2], args[3], buffer)));
	free(buffer);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_conio_gettext(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32	args[4];
	unsigned char	*result;
	int		i;
	int		size;
	JSObject *array;
	jsval	val;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	/* default values: */
	args[0]=1;
	args[1]=1;
	args[2]=cio_textinfo.screenwidth;
	args[3]=cio_textinfo.screenheight;

	if(argc > 4)
		return(JS_FALSE);
	for(i=0; i<(int)argc; i++) {
		if(!JSVAL_IS_NUMBER(argv[i]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[i], &args[i]))
			return(JS_FALSE);
	}
	if(args[0] < 1 || args[1] < 1 || args[2] < 1 || args[3] < 1
			|| args[0] > args[2] || args[1] > args[3]
			|| args[2] > cio_textinfo.screenwidth || args[3] > cio_textinfo.screenheight) {
		JS_SET_RVAL(cx, arglist,JSVAL_FALSE);
		return(JS_TRUE);
	}
	size=(args[2]-args[0]+1)*(args[3]-args[1]+1)*2;
	result=(unsigned char *)malloc(size);
	if(result==NULL)
		return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);

	if(gettext(args[0], args[1], args[2], args[3], result)) {
		JS_RESUMEREQUEST(cx, rc);
		array=JS_NewArrayObject(cx, 0, NULL);
		for(i=0; i<size; i++) {
			val=UINT_TO_JSVAL(result[i]);
			if(!JS_SetElement(cx, array, i, &val)) {
				free(result);
				return(JS_FALSE);
			}
		}
		JS_SET_RVAL(cx, arglist,OBJECT_TO_JSVAL(array));
	}
	else {
		JS_RESUMEREQUEST(cx, rc);
		JS_SET_RVAL(cx, arglist,JSVAL_NULL);
	}
	free(result);
	return(JS_TRUE);
}

/* TODO: seticon() */
#if 0
static JSBool
js_conio_seticon(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
}
#endif

/* TODO: getmouse() */
#if 0
static JSBool
js_conio_getmouse(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
}
#endif

/* TODO: ungetmouse() */
#if 0
static JSBool
js_conio_ungetmouse(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
}
#endif

static jsSyncMethodSpec js_functions[] = {
	{"init",			js_conio_init,			1
		,JSTYPE_BOOLEAN,JSDOCSTR("[mode]")
		,JSDOCSTR("Initializes the conio library and creates a window if needed.  "
				"Mode is a string with one of the following values:<br>"
				"<table><tr><td>\"AUTO\" (default)</td><td>Automatically select the \"best\" output mode.</td></tr><tr><td>"
				"<tr><td>\"ANSI\"</td><td>Use ANSI escape sequences directly</td></tr><tr><td>"
				"<tr><td>\"STDIO\"</td><td>Use stdio, worst, but most compatible mode.</td></tr><tr><td>"
				"<tr><td>\"X\"</td><td>Use X11 output *nix only)</td></tr><tr><td>"
				"<tr><td>\"CONIO\"</td><td>Use the native conio library (Windows only)</td></tr><tr><td>"
				"<tr><td>\"CONIO_FULLSCREEN\"</td><td>Use the native conio library and request full-screen (full-screen does not work on all versions of Windows) (Windows only)</td></tr><tr><td>"
				"<tr><td>\"CURSES\"</td><td>Use the curses terminal library (*nix only)</td></tr><tr><td>"
				"<tr><td>\"CURSES_IBM\"</td><td>Use the curses terminal library and write extended ASCII characters directly as-is, assuming the terminal is using CP437. (*nix only)</td></tr><tr><td>"
				"<tr><td>\"CURSES_ASCII\"</td><td>Use the curses terminal library and write US-ASCII characters only. (*nix only)</td></tr><tr><td>"
				"<tr><td>\"SDL\"</td><td>Use the SDL library for output.</td></tr><tr><td>"
				"<tr><td>\"SDL_FULLSCREEN\"</td><td>Use the SDL library for output (fullscreen).</td></tr><tr><td>"
				"</table>"
			),315
	},
	{"suspend",			js_conio_suspend,		0
		,JSTYPE_VOID,JSDOCSTR("")
		,JSDOCSTR("Suspends conio in CONIO or CURSES modes so the terminal can be used for other things."),315
	},
	{"clreol",			js_conio_clreol,		0
		,JSTYPE_VOID,JSDOCSTR("")
		,JSDOCSTR("Clear to end of line"),315
	},
	{"clrscr",			js_conio_clrscr,		0
		,JSTYPE_VOID,JSDOCSTR("")
		,JSDOCSTR("Clears the screen"),315
	},
	{"wscroll",			js_conio_wscroll,		0
		,JSTYPE_VOID,JSDOCSTR("")
		,JSDOCSTR("Scrolls the currently defined window up by one line."),315
	},
	{"delline",			js_conio_delline,		0
		,JSTYPE_VOID,JSDOCSTR("")
		,JSDOCSTR("Deletes the current line and moves lines below up by one line."),315
	},
	{"insline",			js_conio_insline,		0
		,JSTYPE_VOID,JSDOCSTR("")
		,JSDOCSTR("Inserts a new blank line on the current line and scrolls lines below down to make room."),315
	},
	{"normvideo",		js_conio_normvideo,		0
		,JSTYPE_VOID,JSDOCSTR("")
		,JSDOCSTR("Sets the current attribute to \"normal\" (light grey on black)"),315
	},
	{"getch",			js_conio_getch,			0
		,JSTYPE_NUMBER,JSDOCSTR("")
		,JSDOCSTR("Waits for and returns a character from the user.  Extended keys are returned as two characters."),315
	},
	{"getche",			js_conio_getche,		0
		,JSTYPE_NUMBER,JSDOCSTR("")
		,JSDOCSTR("Waits for a character from the user, then echos it.  Extended keys can not be returned and are lost."),315
	},
	{"beep",			js_conio_beep,			0
		,JSTYPE_VOID,JSDOCSTR("")
		,JSDOCSTR("Beeps."),315
	},
	{"getfont",			js_conio_getfont,		1
		,JSTYPE_NUMBER,JSDOCSTR("fnum")
		,JSDOCSTR("Returns the current font ID or -1 if fonts aren't supported."),315
	},
	{"hidemouse",		js_conio_hidemouse,		0
		,JSTYPE_NUMBER,JSDOCSTR("")
		,JSDOCSTR("Hides the mouse cursor.  Returns -1 if it cannot be hidden."),315
	},
	{"showmouse",		js_conio_showmouse,		0
		,JSTYPE_NUMBER,JSDOCSTR("")
		,JSDOCSTR("Shows the mouse cursor.  Returns -1 if it cannot be shown."),315
	},
	{"setcursortype",	js_conio_setcursortype,	1
		,JSTYPE_VOID,JSDOCSTR("type")
		,JSDOCSTR("Sets the cursor type.  Legal values:<br>"
			"<table><tr><td>0</td><td>No cursor</td></tr>"
			"<tr><td>1</td><td>Solid cursor (fills whole cell)</td></tr>"
			"<tr><td>2</td><td>Normal cursor</td></tr></table>"
		),315
	},
	{"gotoxy",			js_conio_gotoxy,		2
		,JSTYPE_VOID,JSDOCSTR("x, y")
		,JSDOCSTR("Moves the cursor to the given x/y position."),315
	},
	{"putch",			js_conio_putch,			1
		,JSTYPE_NUMBER,JSDOCSTR("charcode")
		,JSDOCSTR("Puts the character with the specified ASCII character on the screen and advances the cursor.  Returns the character code passed in."),315
	},
	{"ungetch",			js_conio_ungetch,		1
		,JSTYPE_NUMBER,JSDOCSTR("charcode")
		,JSDOCSTR("Pushes the specified charcode into the input buffer.  The next getch() call will get this character."),315
	},
	{"loadfont",		js_conio_loadfont,		1
		,JSTYPE_VOID,JSDOCSTR("filename")
		,JSDOCSTR("Loads the filename as the current font.  Returns -1 on failure."),315
	},
	{"settitle",		js_conio_settitle,		1
		,JSTYPE_VOID,JSDOCSTR("title")
		,JSDOCSTR("Sets the window title if possible."),315
	},
	{"setname",			js_conio_setname,		1
		,JSTYPE_VOID,JSDOCSTR("name")
		,JSDOCSTR("Sets the application name.  In some modes, this overwrites the window title."),315
	},
	{"cputs",			js_conio_cputs,			1
		,JSTYPE_VOID,JSDOCSTR("string")
		,JSDOCSTR("Outputs string to the console."),315
	},
	{"setfont",			js_conio_setfont,		2
		,JSTYPE_NUMBER,JSDOCSTR("font [, force, fnum]")
		,JSDOCSTR("Sets a current font to the specified font.  If force is set, will change video modes if the current one callot use the specified font.  fnum selects which current font to change:<br>"
			"<table><tr><td>0</td><td>Default font</td></tr>"
			"<tr><td>1</td><td>Main font</td></tr>"
			"<tr><td>2</td><td>First alternate font</td></tr>"
			"<tr><td>3</td><td>Second alternate font</td></tr>"
			"<tr><td>4</td><td>Third alternate font</td></tr></table><br>"
			"Returns -1 on failure"
		),315
	},
	{"getpass",			js_conio_getpass,		1
		,JSTYPE_STRING,JSDOCSTR("prompt")
		,JSDOCSTR("Prompts for a password, and waits for it to be entered.  Characters are not echoed to the screen."),315
	},
	{"window",			js_conio_window,		4
		,JSTYPE_VOID,JSDOCSTR("start_x, start_y, end_x, end_y")
		,JSDOCSTR("Sets the current window."),315
	},
	{"cgets",			js_conio_cgets,			1
		,JSTYPE_VOID,JSDOCSTR("max_len")
		,JSDOCSTR("Inputs a string, echoing as it's typed, with the specified max_len (up to 255)."),315
	},
	{"movetext",		js_conio_movetext,		6
		,JSTYPE_VOID,JSDOCSTR("start_x, start_y, end_x, end_y, dest_x, dest_y")
		,JSDOCSTR("Copies the specified screen contents to dest_x/dest_y."),315
	},
	{"puttext",			js_conio_puttext,		5
		,JSTYPE_BOOLEAN,JSDOCSTR("start_x, start_y, end_x, end_y, bytes")
		,JSDOCSTR("Puts the contents of the bytes array onto the screen filling the specified rectange.  The array is a sequences if integers between 0 and 255 specifying first the ascii character code, then the attribute for that character."),315
	},
	{"gettext",			js_conio_gettext,		4
		,JSTYPE_ARRAY,JSDOCSTR("start_x, start_y, end_x, end_y")
		,JSDOCSTR("Returns an array containing the characters and attributes for the specified rectangle."),315
	},
	{0}
};

static JSBool js_conio_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
			if(name==NULL)
				return JS_FALSE;
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_properties, js_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_conio_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_conio_resolve(cx, obj, JSID_VOID));
}

static JSClass js_conio_class = {
     "Conio"				/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_get					/* getProperty	*/
	,js_set					/* setProperty	*/
	,js_conio_enumerate		/* enumerate	*/
	,js_conio_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

#ifdef BUILD_JSDOCS
static char* conio_prop_desc[] = {
	"Allows windows to scroll",
	"Enables direct video writes (does nothing)",
	"Do not update the screen when characters are printed",
	"Calling puttext() (and some other things implemented using it) can move the cursor position",
	"The current video mode",
	"",
	"Delay in MS after getting an escape character before assuming it is not part of a sequence.  For curses and ANSI modes",
	"Current text attribute",
	"True if a keystroke is pending",
	"Current x position on the screen",
	"Current y position on the screen",
	"Current text mode",
	"Left column of current window",
	"Top row of current window",
	"Right column of current window",
	"Bottom row of current window",
	"Width of the screen",
	"Height of the screen",
	"Atrribute considered \"normal\"",
	"Background colour",
	"Foreground colour",
	"Text in the clipboard",
	"Current attribute is low intensity",
	"Current attribute is high intensity",
	NULL
};
#endif

JSObject* js_CreateConioObject(JSContext* cx, JSObject* parent)
{
	JSObject*	obj;

	if((obj = JS_DefineObject(cx, parent, "conio", &js_conio_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY))==NULL)
		return(NULL);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Console Input/Output Object (DOS conio library functionality for jsexec)",315);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", conio_prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}
