/* js_msg_area.c */

/* Synchronet JavaScript "Message Area" Object */

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

enum {	/* msg_area Object Properties */
	 PROP_MAX_QWK_MSGS
};

static JSClass js_msg_area_class = {
     "MsgArea"				/* name			*/
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

JSObject* DLLCALL js_CreateMsgAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
										  ,user_t* user)
{
	char		str[128];
	JSObject*	areaobj;
	JSObject*	grpobj;
	JSObject*	subobj;
	JSObject*	grp_list;
	JSObject*	sub_list;
	jsval		val;
	jsuint		index;
	uint		c,l,d;
	JSBool		found;

	areaobj = JS_DefineObject(cx, parent, "msg_area", &js_msg_area_class, NULL, 0);

	if(areaobj==NULL)
		return(NULL);

	/* grp_list[] */
	if((grp_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
		return(NULL);

	val=OBJECT_TO_JSVAL(grp_list);
	if(!JS_SetProperty(cx, areaobj, "grp_list", &val)) 
		return(NULL);
	JS_SetPropertyAttributes(cx, areaobj, "grp_list", 0, &found);

	for(l=0;l<cfg->total_grps;l++) {

		if(user!=NULL && !chk_ar(cfg,cfg->grp[l]->ar,user))
			continue;

		if((grpobj=JS_NewObject(cx, &js_msg_area_class, NULL, NULL))==NULL)
			return(NULL);

		val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->grp[l]->sname));
		if(!JS_SetProperty(cx, grpobj, "name", &val))
			return(NULL);

		val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->grp[l]->lname));
		if(!JS_SetProperty(cx, grpobj, "description", &val))
			return(NULL);

		/* sub_list[] */
		if((sub_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return(NULL);

		val=OBJECT_TO_JSVAL(sub_list);
		if(!JS_SetProperty(cx, grpobj, "sub_list", &val)) 
			return(NULL);
		JS_SetPropertyAttributes(cx, grpobj, "sub_list", 0, &found);

		for(d=0;d<cfg->total_subs;d++) {
			if(cfg->sub[d]->grp!=l)
				continue;
			if(user!=NULL && !chk_ar(cfg,cfg->sub[d]->ar,user))
				continue;

			if((subobj=JS_NewObject(cx, &js_msg_area_class, NULL, NULL))==NULL)
				return(NULL);

			val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sub[d]->code));
			if(!JS_SetProperty(cx, subobj, "code", &val))
				return(NULL);

			val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sub[d]->sname));
			if(!JS_SetProperty(cx, subobj, "name", &val))
				return(NULL);

			val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cfg->sub[d]->lname));
			if(!JS_SetProperty(cx, subobj, "description", &val))
				return(NULL);

			sprintf(str,"%s.%s",cfg->grp[l]->sname,cfg->sub[d]->sname);
			for(c=0;str[c];c++)
				if(str[c]==' ')
					str[c]='_';
			strlwr(str);
			val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str));
			if(!JS_SetProperty(cx, subobj, "newsgroup", &val))
				return(NULL);

			if(user==NULL || chk_ar(cfg,cfg->sub[d]->read_ar,user))
				val=BOOLEAN_TO_JSVAL(JS_TRUE);
			else
				val=BOOLEAN_TO_JSVAL(JS_FALSE);
			if(!JS_SetProperty(cx, subobj, "can_read", &val))
				return(NULL);

			if(user==NULL || chk_ar(cfg,cfg->sub[d]->post_ar,user))
				val=BOOLEAN_TO_JSVAL(JS_TRUE);
			else
				val=BOOLEAN_TO_JSVAL(JS_FALSE);
			if(!JS_SetProperty(cx, subobj, "can_post", &val))
				return(NULL);

			if(!JS_GetArrayLength(cx, sub_list, &index))
				return(NULL);							

			val=OBJECT_TO_JSVAL(subobj);
			JS_SetElement(cx, sub_list, index, &val);
		}

		if(!JS_GetArrayLength(cx, grp_list, &index))
			return(NULL);

		val=OBJECT_TO_JSVAL(grpobj);
		JS_SetElement(cx, grp_list, index, &val);
	}

	return(areaobj);
}

#endif	/* JAVSCRIPT */