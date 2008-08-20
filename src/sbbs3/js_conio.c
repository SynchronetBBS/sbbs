/* js_conio.c */

/* Synchronet "conio" (console IO) object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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

static JSBool js_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint		tiny;

    tiny = JSVAL_TO_INT(id);

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

	return(JS_TRUE);
}

static JSBool js_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint		tiny;
	int32		i=0;

    tiny = JSVAL_TO_INT(id);

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
			if(cio_api.ESCDELAY)
				JS_ValueToInt32(cx, *vp, cio_api.ESCDELAY);
			break;
		case PROP_TEXTATTR:
			JS_ValueToInt32(cx, *vp, &i);
			textattr(i);
			break;
		case PROP_WHEREX:
			JS_ValueToInt32(cx, *vp, &i);
			gotoxy(i, cio_textinfo.cury);
			break;
		case PROP_WHEREY:
			JS_ValueToInt32(cx, *vp, &i);
			gotoxy(cio_textinfo.curx, i);
			break;
		case PROP_TEXTMODE:
			JS_ValueToInt32(cx, *vp, &i);
			textmode(i);
			break;
		case PROP_TEXTBACKGROUND:
			JS_ValueToInt32(cx, *vp, &i);
			textbackground(i);
			break;
		case PROP_TEXTCOLOR:
			JS_ValueToInt32(cx, *vp, &i);
			textcolor(i);
			break;
		case PROP_CLIPBOARD:
			{
				size_t	len;
				char	*bytes;

				bytes=js_ValueToStringBytes(cx, *vp, &len);
				copytext(bytes, len+1);
			}
			break;
		case PROP_HIGHVIDEO:
			JS_ValueToBoolean(cx, *vp, &i);
			if(i)
				highvideo();
			else
				lowvideo();
			break;
		case PROP_LOWVIDEO:
			JS_ValueToBoolean(cx, *vp, &i);
			if(i)
				lowvideo();
			else
				highvideo();
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
js_conio_init(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int		ciolib_mode=CIOLIB_MODE_AUTO;
	char*	mode;

	*rval = JSVAL_FALSE;

	if(argc>0 && (mode=js_ValueToStringBytes(cx, argv[0], NULL))!=NULL) {
		if(!stricmp(mode,"STDIO"))
			ciolib_mode=-1;
		else if(!stricmp(mode,"AUTO"))
			ciolib_mode=CIOLIB_MODE_AUTO;
		else if(!stricmp(mode,"X"))
			ciolib_mode=CIOLIB_MODE_X;
		else if(!stricmp(mode,"ANSI"))
			ciolib_mode=CIOLIB_MODE_ANSI;
		else if(!stricmp(mode,"CONIO"))
			ciolib_mode=CIOLIB_MODE_CONIO;
	}

	if(initciolib(ciolib_mode))
		return(JS_TRUE);

	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_conio_suspend(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	suspendciolib();
	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_conio_clreol(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	clreol();
	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_conio_clrscr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	clrscr();
	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_conio_wscroll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	wscroll();
	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_conio_delline(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	delline();
	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_conio_insline(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	insline();
	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_conio_normvideo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	normvideo();
    *rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_conio_getch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = INT_TO_JSVAL(getch());
	return(JS_TRUE);
}

static JSBool
js_conio_getche(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = INT_TO_JSVAL(getche());
	return(JS_TRUE);
}

static JSBool
js_conio_beep(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = INT_TO_JSVAL(beep());
	return(JS_TRUE);
}

static JSBool
js_conio_getfont(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = INT_TO_JSVAL(getfont());
	return(JS_TRUE);
}

static JSBool
js_conio_hidemouse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = INT_TO_JSVAL(hidemouse());
	return(JS_TRUE);
}

static JSBool
js_conio_showmouse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = INT_TO_JSVAL(showmouse());
	return(JS_TRUE);
}

static JSBool
js_conio_setcursortype(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int	type;

	if(argc==1 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&type)) {
		_setcursortype(type);
		*rval = JSVAL_TRUE;
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_gotoxy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int	x,y;

	if(argc==2 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&x)
				&& JSVAL_IS_NUMBER(argv[1]) && JS_ValueToInt32(cx,argv[1],&y)) {
		gotoxy(x,y);
		*rval = JSVAL_TRUE;
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_putch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int	ch;

	if(argc==1 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&ch)) {
		*rval=INT_TO_JSVAL(putch(ch));
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_ungetch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int	ch;

	if(argc==1 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&ch)) {
		*rval=INT_TO_JSVAL(ungetch(ch));
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_loadfont(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *	str;

	if(argc==1 && (str=js_ValueToStringBytes(cx,argv[0],NULL))) {
		*rval=INT_TO_JSVAL(loadfont(str));
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_settitle(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *	str;

	if(argc==1 && (str=js_ValueToStringBytes(cx,argv[0],NULL))) {
		settitle(str);
		*rval=JSVAL_TRUE;
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_setname(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *	str;

	if(argc==1 && (str=js_ValueToStringBytes(cx,argv[0],NULL))) {
		setname(str);
		*rval=JSVAL_TRUE;
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_cputs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *	str;

	if(argc==1 && (str=js_ValueToStringBytes(cx,argv[0],NULL))) {
		*rval=INT_TO_JSVAL(cputs(str));
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_setfont(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int	font;
	int force=JS_FALSE;

	if(argc > 2)
		return(JS_FALSE);

	if(argc > 0 && JSVAL_IS_NUMBER(argv[0]) && JS_ValueToInt32(cx,argv[0],&font)) {
		if(argc > 1) {
			if(!JSVAL_IS_BOOLEAN(argv[1]))
				return(JS_FALSE);
			if(!JS_ValueToBoolean(cx, argv[1], &force))
				return(JS_FALSE);
		}
		*rval=INT_TO_JSVAL(setfont(font, force));
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_getpass(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *	str;

	if(argc==1 && (str=js_ValueToStringBytes(cx,argv[0],NULL))) {
		*rval=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,getpass(str)));
		return(JS_TRUE);
	}

	return(JS_FALSE);
}

static JSBool
js_conio_window(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int	left=1;
	int top=1;
	int	right=cio_textinfo.screenwidth;
	int	bottom=cio_textinfo.screenheight;

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

	window(left, top, right, bottom);
	if(cio_textinfo.winleft == left
			&& cio_textinfo.winright==right
			&& cio_textinfo.wintop==top
			&& cio_textinfo.winbottom==bottom) {
		*rval=JSVAL_TRUE;
	}
	else {
		*rval=JSVAL_FALSE;
	}
	return(JS_TRUE);
}

static JSBool
js_conio_cgets(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char	buf[258];
	int		maxlen=255;
	char	*ret;

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
	buf[0]=maxlen;
	ret=cgets(buf);
	if(ret==NULL)
		*rval=JSVAL_NULL;
	else {
		buf[257]=0;
		*rval=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,ret));
	}

	return(JS_TRUE);
}

/* TODO: cprintf() not really needed since we have cputs() and format() */
#if 0
static JSBool
js_conio_cprintf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
}
#endif

/* TODO: cscanf()... this would be rather insane actually... */
#if 0
static JSBool
js_conio_cscanf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
}
#endif

static JSBool
js_conio_movetext(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int i;
	int	args[6];

	if(argc != 6)
		return(JS_FALSE);
	for(i=0; i<6; i++) {
		if(!JSVAL_IS_NUMBER(argv[i]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[0], &args[i]))
			return(JS_FALSE);
	}
	*rval=BOOLEAN_TO_JSVAL(movetext(args[0], args[1], args[2], args[3], args[4], args[5]));
	return(JS_TRUE);
}

static JSBool
js_conio_puttext(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int		args[4]={1,1,cio_textinfo.screenwidth,cio_textinfo.screenheight};
	unsigned char	*buffer;
	int		i,j;
	int		size;
	jsval	val;

	if(argc != 5)
		return(JS_FALSE);
	for(i=0; i<4; i++) {
		if(!JSVAL_IS_NUMBER(argv[i]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[0], &args[i]))
			return(JS_FALSE);
	}

	if(!JS_GetArrayLength(cx, argv[4], &size))
		return(JS_FALSE);

	if(args[0] < 1 || args[1] < 1 || args[2] < 1 || args[3] < 1
			|| args[0] > args[2] || args[1] > args[3]
			|| args[2] > cio_textinfo.screenwidth || args[3] > cio_textinfo.screenheight) {
		*rval=JSVAL_FALSE;
		return(JS_TRUE);
	}

	buffer=(unsigned char *)malloc(size);
	if(buffer==NULL) 
		return(JS_FALSE);
	for(i=0; i<size; i++) {
		if(!JS_GetElement(cx, argv[4], i, &val)) {
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
		buffer[i]=j;
	}

	*rval=BOOLEAN_TO_JSVAL(puttext(args[0], args[1], args[2], args[3], buffer));
	free(buffer);
	return(JS_TRUE);
}

static JSBool
js_conio_gettext(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int		args[4]={1,1,cio_textinfo.screenwidth,cio_textinfo.screenheight};
	unsigned char	*result;
	int		i;
	int		size;
	JSObject *array;
	jsval	val;

	if(argc > 4)
		return(JS_FALSE);
	for(i=0; i<argc; i++) {
		if(!JSVAL_IS_NUMBER(argv[i]))
			return(JS_FALSE);
		if(!JS_ValueToInt32(cx, argv[0], &args[i]))
			return(JS_FALSE);
	}
	if(args[0] < 1 || args[1] < 1 || args[2] < 1 || args[3] < 1
			|| args[0] > args[2] || args[1] > args[3]
			|| args[2] > cio_textinfo.screenwidth || args[3] > cio_textinfo.screenheight) {
		*rval=JSVAL_FALSE;
		return(JS_TRUE);
	}
	size=(args[2]-args[0]+1)*(args[3]-args[1]+1)*2;
	result=(unsigned char *)malloc(size);
	if(result==NULL)
		return(JS_FALSE);
	if(gettext(args[0], args[1], args[2], args[3], result)) {
		array=JS_NewArrayObject(cx, 0, NULL);
		for(i=0; i<size; i++) {
			JS_NewNumberValue(cx, result[i], &val);
			if(!JS_SetElement(cx, array, i, &val)) {
				free(result);
				return(JS_FALSE);
			}
		}
		*rval=OBJECT_TO_JSVAL(array);
	}
	else
		*rval=JSVAL_NULL;
	free(result);
	return(JS_TRUE);
}

/* TODO: seticon() */
#if 0
static JSBool
js_conio_seticon(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
}
#endif

/* TODO: getmouse() */
#if 0
static JSBool
js_conio_getmouse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
}
#endif

/* TODO: ungetmouse() */
#if 0
static JSBool
js_conio_ungetmouse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
}
#endif

static jsSyncMethodSpec js_functions[] = {
{"init",js_conio_init,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"suspend",js_conio_suspend,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"clreol",js_conio_clreol,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"clrscr",js_conio_clrscr,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"wscroll",js_conio_wscroll,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"delline",js_conio_delline,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"insline",js_conio_insline,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"normvideo",js_conio_normvideo,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"getch",js_conio_getch,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"getche",js_conio_getche,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"beep",js_conio_beep,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"getfont",js_conio_getfont,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"hidemouse",js_conio_hidemouse,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"showmouse",js_conio_showmouse,0,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"setcursortype",js_conio_setcursortype,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"gotoxy",js_conio_gotoxy,2,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"putch",js_conio_putch,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"ungetch",js_conio_ungetch,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"loadfont",js_conio_loadfont,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"settitle",js_conio_settitle,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"setname",js_conio_setname,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"cputs",js_conio_cputs,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"setfont",js_conio_setfont,2,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"getpass",js_conio_getpass,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"window",js_conio_window,4,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"cgets",js_conio_cgets,1,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"movetext",js_conio_movetext,6,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"puttext",js_conio_puttext,5,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{"gettext",js_conio_gettext,4,JSTYPE_VOID,JSDOCSTR("args"),JSDOCSTR("desc"),315},
{0}
};

static JSBool js_conio_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*			name=NULL;

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	return(js_SyncResolve(cx, obj, name, js_properties, js_functions, NULL, 0));
}

static JSBool js_conio_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_conio_resolve(cx, obj, JSVAL_NULL));
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

JSObject* js_CreateConioObject(JSContext* cx, JSObject* parent)
{
	JSObject*	obj;

	if((obj = JS_DefineObject(cx, parent, "conio", &js_conio_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY))==NULL)
		return(NULL);

	return(obj);
}
