/* js_xtrn_area.c */

/* Synchronet JavaScript "External Program Area" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2001 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifdef JAVASCRIPT

static JSClass js_xtrn_area_class = {
     "XtrnArea"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

#ifdef _DEBUG
static char* xtrn_prog_prop_desc[] = {

	 "internal code"
	,"name"
	,"command-line"
	,"clean-up command-line"
	,"startup directory"
	,"toggle options (bitfield)"
	,"drop file type"
	,"event type (0=none)"
	,"extra time given to users running this program"
	,"maximum time allowed in program"
	,"cost (in credits) to run this program"
	,NULL
};
#endif

BOOL DLLCALL js_CreateXtrnProgProperties(JSContext* cx, JSObject* obj, xtrn_t* xtrn)
{
	JSString* js_str;

	if((js_str=JS_NewStringCopyZ(cx, xtrn->code))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, obj, "code", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->name))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, obj, "name", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->cmd))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, obj, "cmd", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->clean))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, obj, "clean_cmd", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->path))==NULL)
		return(FALSE);
	JS_DefineProperty(cx, obj, "startup_dir", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	JS_DefineProperty(cx, obj, "settings", INT_TO_JSVAL(xtrn->misc)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	JS_DefineProperty(cx, obj, "type", INT_TO_JSVAL(xtrn->type)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	JS_DefineProperty(cx, obj, "event", INT_TO_JSVAL(xtrn->event)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	JS_DefineProperty(cx, obj, "textra", INT_TO_JSVAL(xtrn->textra)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	JS_DefineProperty(cx, obj, "max_time", INT_TO_JSVAL(xtrn->maxtime)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	JS_DefineProperty(cx, obj, "cost", INT_TO_JSVAL(xtrn->cost)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

#ifdef _DEBUG
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", xtrn_prog_prop_desc, JSPROP_READONLY);
#endif

	return(TRUE);
}


JSObject* DLLCALL js_CreateXtrnAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
										  ,user_t* user)
{
	JSObject*	areaobj;
	JSObject*	secobj;
	JSObject*	progobj;
	JSObject*	sec_list;
	JSObject*	prog_list;
	JSString*	js_str;
	jsval		val;
	jsuint		index;
	uint		l,d;

	/* Return existing object if it's already been created */
	if(JS_GetProperty(cx,parent,"xtrn_area",&val) && val!=JSVAL_VOID)
		areaobj = JSVAL_TO_OBJECT(val);
	else
		areaobj = JS_DefineObject(cx, parent, "xtrn_area", &js_xtrn_area_class
									, NULL, JSPROP_ENUMERATE);

	if(areaobj==NULL)
		return(NULL);

#ifdef _DEBUG
	js_DescribeObject(cx,areaobj,"External Program Areas");
#endif

	/* sec_list[] */
	if((sec_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
		return(NULL);

	val=OBJECT_TO_JSVAL(sec_list);
	if(!JS_SetProperty(cx, areaobj, "sec_list", &val)) 
		return(NULL);

	for(l=0;l<cfg->total_xtrnsecs;l++) {

		if(user!=NULL && !chk_ar(cfg,cfg->xtrnsec[l]->ar,user))
			continue;

		if((secobj=JS_NewObject(cx, &js_xtrn_area_class, NULL, NULL))==NULL)
			return(NULL);

		val=INT_TO_JSVAL(l);
		if(!JS_SetProperty(cx, secobj, "number", &val))
			return(NULL);

		if((js_str=JS_NewStringCopyZ(cx, cfg->xtrnsec[l]->code))==NULL)
			return(NULL);
		val=STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(cx, secobj, "code", &val))
			return(NULL);

		if((js_str=JS_NewStringCopyZ(cx, cfg->xtrnsec[l]->name))==NULL)
			return(NULL);
		val=STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(cx, secobj, "name", &val))
			return(NULL);

		/* prog_list[] */
		if((prog_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return(NULL);

#ifdef _DEBUG
		js_DescribeObject(cx,secobj,"Online Program (door) Sections");
#endif

		val=OBJECT_TO_JSVAL(prog_list);
		if(!JS_SetProperty(cx, secobj, "prog_list", &val)) 
			return(NULL);

		for(d=0;d<cfg->total_xtrns;d++) {
			if(cfg->xtrn[d]->sec!=l)
				continue;
			if(user!=NULL && !chk_ar(cfg,cfg->xtrn[d]->ar,user))
				continue;

			if((progobj=JS_NewObject(cx, &js_xtrn_area_class, NULL, NULL))==NULL)
				return(NULL);

			if(!js_CreateXtrnProgProperties(cx, progobj, cfg->xtrn[d]))
				return(NULL);

			val=INT_TO_JSVAL(d);
			if(!JS_SetProperty(cx, progobj, "number", &val))
				return(NULL);

			if(user==NULL || chk_ar(cfg,cfg->xtrn[d]->run_ar,user))
				val=BOOLEAN_TO_JSVAL(JS_TRUE);
			else
				val=BOOLEAN_TO_JSVAL(JS_FALSE);
			if(!JS_SetProperty(cx, progobj, "can_run", &val))
				return(NULL);

			if(!JS_GetArrayLength(cx, prog_list, &index))
				return(NULL);							

			val=OBJECT_TO_JSVAL(progobj);
			if(!JS_SetElement(cx, prog_list, index, &val))
				return(NULL);

#ifdef _DEBUG
			js_DescribeObject(cx,progobj,"Online External Programs (doors)");
#endif
		}

		if(!JS_GetArrayLength(cx, sec_list, &index))
			return(NULL);

		val=OBJECT_TO_JSVAL(secobj);
		if(!JS_SetElement(cx, sec_list, index, &val))
			return(NULL);
	}

	return(areaobj);
}

#endif	/* JAVSCRIPT */
