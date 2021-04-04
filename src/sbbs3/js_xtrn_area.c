/* Synchronet JavaScript "External Program Area" Object */

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

#include "sbbs.h"

#ifdef JAVASCRIPT

#ifdef BUILD_JSDOCS

static char* xtrn_sec_prop_desc[] = {

	 "index into sec_list array (or -1 if not in index) <i>(introduced in v3.12)</i>"
	,"unique number for this external program section"
	,"external program section internal code"
	,"external program section name"
	,"external program section access requirements"
	,"user has sufficient access to enter this section <i>(introduced in v3.15)</i>"
	,NULL
};

static char* xtrn_prog_prop_desc[] = {

	 "index into prog_list array (or -1 if not in index) <i>(introduced in v3.12)</i>"
	,"program number"
	,"program section index <i>(introduced in v3.12)</i>"
	,"program section number"
	,"program section internal code <i>(introduced in v3.12)</i>"
	,"internal code"
	,"name"
	,"command-line"
	,"clean-up command-line"
	,"startup directory"
	,"access requirements"
	,"execution requirements"
	,"toggle options (bitfield)"
	,"drop file type"
	,"event type (0=none)"
	,"extra time given to users running this program"
	,"maximum time allowed in program"
	,"execution cost (credits to run this program)"
	/* Insert here */
	,"user has sufficient access to see this program"
	,"user has sufficient access to run this program"
	,NULL
};

static char* event_prop_desc[] = {

	 "command-line"
	,"startup directory"
	,"node number"
	,"time to execute (minutes since midnight)"
	,"frequency to execute"
	,"days of week to execute (bitfield)"
	,"days of month to execute (bitfield)"
	,"months of year to execute (bitfield)"
	,"date/time of last run (in time_t format)"
	,"date/time of next run (in time_t format)"
	,"error log level"
	,"toggle options (bitfield)"
	,NULL
};

static char* xedit_prop_desc[] = {

	 "name"
	,"command-line"
	,"access requirements"
	,"toggle options (bitfield)"
	,"drop file type"
	,NULL
};

#endif

/* Event Object Properties */
enum {
	 EVENT_PROP_CMD,
	 EVENT_PROP_STARTUP_DIR,
	 EVENT_PROP_NODE_NUM,
	 EVENT_PROP_TIME,
	 EVENT_PROP_FREQ,
	 EVENT_PROP_DAYS,
	 EVENT_PROP_MDAYS,
	 EVENT_PROP_MONTHS,
	 EVENT_PROP_LAST_RUN,
	 EVENT_PROP_NEXT_RUN,
	 EVENT_PROP_ERRLEVEL,
	 EVENT_PROP_MISC
};

static jsSyncPropertySpec js_event_properties[] = {
/*		 name				,tinyid					,flags								,ver	*/

	{	"cmd"				,EVENT_PROP_CMD			,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"startup_dir"		,EVENT_PROP_STARTUP_DIR	,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"node_num"			,EVENT_PROP_NODE_NUM	,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"time"				,EVENT_PROP_TIME		,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"freq"				,EVENT_PROP_FREQ		,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"days"				,EVENT_PROP_DAYS		,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"mdays"				,EVENT_PROP_MDAYS		,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"months"			,EVENT_PROP_MONTHS		,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"last_run"			,EVENT_PROP_LAST_RUN	,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{	"next_run"			,EVENT_PROP_NEXT_RUN	,JSPROP_ENUMERATE|JSPROP_READONLY	,31802},
	{	"error_level"		,EVENT_PROP_ERRLEVEL	,JSPROP_ENUMERATE|JSPROP_READONLY	,31802},
	{	"settings"			,EVENT_PROP_MISC		,JSPROP_ENUMERATE|JSPROP_READONLY	,311},
	{ NULL }
};

BOOL js_CreateXtrnProgProperties(JSContext* cx, JSObject* obj, xtrn_t* xtrn)
{
	JSString* js_str;

	if((js_str=JS_NewStringCopyZ(cx, xtrn->code))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, obj, "code", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->name))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, obj, "name", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->cmd))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, obj, "cmd", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->clean))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, obj, "clean_cmd", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->path))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, obj, "startup_dir", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->arstr))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, obj, "ars", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if((js_str=JS_NewStringCopyZ(cx, xtrn->run_arstr))==NULL)
		return(FALSE);
	if(!JS_DefineProperty(cx, obj, "execution_ars", STRING_TO_JSVAL(js_str)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, obj, "settings", INT_TO_JSVAL(xtrn->misc)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, obj, "type", INT_TO_JSVAL(xtrn->type)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, obj, "event", INT_TO_JSVAL(xtrn->event)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, obj, "textra", INT_TO_JSVAL(xtrn->textra)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, obj, "max_time", INT_TO_JSVAL(xtrn->maxtime)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

	if(!JS_DefineProperty(cx, obj, "cost", INT_TO_JSVAL(xtrn->cost)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
		return(FALSE);

#ifdef BUILD_JSDOCS
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", xtrn_prog_prop_desc, JSPROP_READONLY);
#endif

	return(TRUE);
}

static JSBool js_event_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	const char* p = NULL;
	JSString*	js_str;
	jsval		idval;
    jsint       tiny;
	event_t*	event;

	if((event = JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case EVENT_PROP_CMD:
			p = event->cmd;
			break;
		case EVENT_PROP_STARTUP_DIR:
			p = event->dir;
			break;
		case EVENT_PROP_NODE_NUM:
			*vp = UINT_TO_JSVAL(event->node);
			break;
		case EVENT_PROP_TIME:
			*vp = UINT_TO_JSVAL(event->time);
			break;
		case EVENT_PROP_FREQ:
			*vp = UINT_TO_JSVAL(event->freq);
			break;
		case EVENT_PROP_DAYS:
			*vp = UINT_TO_JSVAL(event->days);
			break;
		case EVENT_PROP_MDAYS:
			*vp = UINT_TO_JSVAL(event->mdays);
			break;
		case EVENT_PROP_MONTHS:
			*vp = UINT_TO_JSVAL(event->months);
			break;
		case EVENT_PROP_LAST_RUN:
			*vp = UINT_TO_JSVAL(event->last);
			break;
		case EVENT_PROP_NEXT_RUN:
			*vp = UINT_TO_JSVAL((uint32)getnexteventtime(event));
			break;
		case EVENT_PROP_ERRLEVEL:
			*vp = UINT_TO_JSVAL(event->errlevel);
			break;
		case EVENT_PROP_MISC:
			*vp = UINT_TO_JSVAL(event->misc);
			break;
	}

	if(p != NULL) {	/* string property */
		if((js_str = JS_NewStringCopyZ(cx, p)) == NULL)
			return JS_FALSE;
		*vp = STRING_TO_JSVAL(js_str);
	}
	return JS_TRUE;
}

static JSClass js_event_class = {
     "Event"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_event_get			/* getProperty	*/
	,JS_StrictPropertyStub	/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

struct js_xtrn_area_priv {
	scfg_t		*cfg;
	user_t		*user;
	client_t	*client;
};

JSBool js_xtrn_area_resolve(JSContext* cx, JSObject* areaobj, jsid id)
{
	JSObject*	allsec;
	JSObject*	allprog;
	JSObject*	secobj;
	JSObject*	progobj;
	JSObject*	eventobj;
	JSObject*	event_array;
	JSObject*	xeditobj;
	JSObject*	xedit_array;
	JSObject*	sec_list;
	JSObject*	prog_list;
	JSString*	js_str;
	jsval		val;
	jsuint		sec_index;
	jsuint		prog_index;
	uint		l,d;
	char*		name=NULL;
	struct js_xtrn_area_priv *p;

	if((p=(struct js_xtrn_area_priv*)JS_GetPrivate(cx,areaobj))==NULL)
		return JS_FALSE;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}

	if (name == NULL || strcmp(name, "sec")==0 || strcmp(name, "prog")==0 || strcmp(name, "sec_list")==0 || strcmp(name, "event")==0  || strcmp(name, "editor")==0) {
		FREE_AND_NULL(name);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,areaobj,"External Program Areas",310);
#endif

		/* xtrn_area.sec[] */
		if((allsec=JS_NewObject(cx,NULL,NULL,areaobj))==NULL)
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(allsec);
		if(!JS_SetProperty(cx, areaobj, "sec", &val))
			return JS_FALSE;

		/* xtrn_area.prog[] */
		if((allprog=JS_NewObject(cx,NULL,NULL,areaobj))==NULL)
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(allprog);
		if(!JS_SetProperty(cx, areaobj, "prog", &val))
			return JS_FALSE;

		/* xtrn_area.sec_list[] */
		if((sec_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(sec_list);
		if(!JS_SetProperty(cx, areaobj, "sec_list", &val)) 
			return JS_FALSE;

		for(l=0;l<p->cfg->total_xtrnsecs;l++) {

			if((secobj=JS_NewObject(cx, NULL, NULL, NULL))==NULL)
				return JS_FALSE;

			val=OBJECT_TO_JSVAL(secobj);
			sec_index=-1;
			if(p->user==NULL || chk_ar(p->cfg,p->cfg->xtrnsec[l]->ar,p->user,p->client)) {

				if(!JS_GetArrayLength(cx, sec_list, &sec_index))
					return JS_FALSE;

				if(!JS_SetElement(cx, sec_list, sec_index, &val))
					return JS_FALSE;
			}

			/* Add as property (associative array element) */
			if(!JS_DefineProperty(cx, allsec, p->cfg->xtrnsec[l]->code, val
				,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
				return JS_FALSE;

			val=INT_TO_JSVAL(sec_index);
			if(!JS_SetProperty(cx, secobj, "index", &val))
				return JS_FALSE;

			val=INT_TO_JSVAL(l);
			if(!JS_SetProperty(cx, secobj, "number", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->xtrnsec[l]->code))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, secobj, "code", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->xtrnsec[l]->name))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, secobj, "name", &val))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->xtrnsec[l]->arstr))==NULL)
				return JS_FALSE;
			val=STRING_TO_JSVAL(js_str);
			if(!JS_SetProperty(cx, secobj, "ars", &val))
				return JS_FALSE;

			if(p->user==NULL || chk_ar(p->cfg,p->cfg->xtrnsec[l]->ar,p->user,p->client))
				val=JSVAL_TRUE;
			else
				val=JSVAL_FALSE;
			if(!JS_SetProperty(cx, secobj, "can_access", &val))
				return JS_FALSE;

			/* prog_list[] */
			if((prog_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
				return JS_FALSE;

			val=OBJECT_TO_JSVAL(prog_list);
			if(!JS_SetProperty(cx, secobj, "prog_list", &val)) 
				return JS_FALSE;

#ifdef BUILD_JSDOCS
			js_DescribeSyncObject(cx,secobj,"Online Program (door) Sections (current user has access to)",310);
#endif

			for(d=0;d<p->cfg->total_xtrns;d++) {
				if(p->cfg->xtrn[d]->sec!=l)
					continue;

				if((progobj=JS_NewObject(cx, NULL, NULL, NULL))==NULL)
					return JS_FALSE;

				val=OBJECT_TO_JSVAL(progobj);
				prog_index=-1;
				if((p->user==NULL || chk_ar(p->cfg,p->cfg->xtrn[d]->ar,p->user,p->client))
					&& !(p->cfg->xtrn[d]->event && p->cfg->xtrn[d]->misc&EVENTONLY)) {

					if(!JS_GetArrayLength(cx, prog_list, &prog_index))
						return JS_FALSE;

					if(!JS_SetElement(cx, prog_list, prog_index, &val))
						return JS_FALSE;
				}

				/* Add as property (associative array element) */
				if(!JS_DefineProperty(cx, allprog, p->cfg->xtrn[d]->code, val
					,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
					return JS_FALSE;

				val=INT_TO_JSVAL(prog_index);
				if(!JS_SetProperty(cx, progobj, "index", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(d);
				if(!JS_SetProperty(cx, progobj, "number", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(sec_index);
				if(!JS_SetProperty(cx, progobj, "sec_index", &val))
					return JS_FALSE;

				val=INT_TO_JSVAL(l);
				if(!JS_SetProperty(cx, progobj, "sec_number", &val))
					return JS_FALSE;

				val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->cfg->xtrnsec[l]->code));
				if(!JS_SetProperty(cx, progobj, "sec_code", &val))
					return JS_FALSE;

				if(!js_CreateXtrnProgProperties(cx, progobj, p->cfg->xtrn[d]))
					return JS_FALSE;

				if(p->user==NULL || chk_ar(p->cfg,p->cfg->xtrn[d]->ar,p->user,p->client))
					val=JSVAL_TRUE;
				else
					val=JSVAL_FALSE;
				if(!JS_SetProperty(cx, progobj, "can_access", &val))
					return JS_FALSE;

				if(p->user==NULL || chk_ar(p->cfg,p->cfg->xtrn[d]->run_ar,p->user,p->client))
					val=JSVAL_TRUE;
				else
					val=JSVAL_FALSE;
				if(!JS_SetProperty(cx, progobj, "can_run", &val))
					return JS_FALSE;

#ifdef BUILD_JSDOCS
				js_DescribeSyncObject(cx,progobj,"Online External Programs (doors) (current user has access to)",310);
#endif
			}

#ifdef BUILD_JSDOCS
			js_CreateArrayOfStrings(cx, secobj, "_property_desc_list", xtrn_sec_prop_desc, JSPROP_READONLY);
#endif

		}

#ifdef BUILD_JSDOCS

		js_DescribeSyncObject(cx,allsec,"Associative array of all external program sections (use internal code as index)",312);
		JS_DefineProperty(cx,allsec,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);

		js_DescribeSyncObject(cx,allprog,"Associative array of all external programs (use internal code as index)",311);
		JS_DefineProperty(cx,allprog,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif

		/* Create event property */
		if((event_array=JS_NewObject(cx,NULL,NULL,areaobj))==NULL)
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(event_array);
		if(!JS_SetProperty(cx, areaobj, "event", &val))
			return JS_FALSE;

		for(l=0;l<p->cfg->total_events;l++) {

			if((eventobj=JS_NewObject(cx, &js_event_class, NULL, NULL))==NULL)
				return JS_FALSE;

			JS_SetPrivate(cx, eventobj, p->cfg->event[l]);

			if(!js_DefineSyncProperties(cx, eventobj, js_event_properties))
				return JS_FALSE;

			if(!JS_DefineProperty(cx, event_array, p->cfg->event[l]->code, OBJECT_TO_JSVAL(eventobj)
				,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
				return JS_FALSE;

#ifdef BUILD_JSDOCS
			js_CreateArrayOfStrings(cx, eventobj, "_property_desc_list", event_prop_desc, JSPROP_READONLY);
#endif
		}

#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,event_array,"Associative array of all timed events (use internal code as index)",311);
		JS_DefineProperty(cx,event_array,"_assoc_array",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif

		/* Create editor property */
		if((xedit_array=JS_NewObject(cx,NULL,NULL,areaobj))==NULL)
			return JS_FALSE;

		val=OBJECT_TO_JSVAL(xedit_array);
		if(!JS_SetProperty(cx, areaobj, "editor", &val))
			return JS_FALSE;

		for(l=0;l<p->cfg->total_xedits;l++) {

			if(p->user!=NULL && !chk_ar(p->cfg,p->cfg->xedit[l]->ar,p->user,p->client))
				continue;

			if((xeditobj=JS_NewObject(cx, NULL, NULL, NULL))==NULL)
				return JS_FALSE;

			if(!JS_DefineProperty(cx, xedit_array, p->cfg->xedit[l]->code, OBJECT_TO_JSVAL(xeditobj)
				,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->xedit[l]->name))==NULL)
				return JS_FALSE;
			if(!JS_DefineProperty(cx, xeditobj, "name", STRING_TO_JSVAL(js_str)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->xedit[l]->rcmd))==NULL)
				return JS_FALSE;
			if(!JS_DefineProperty(cx, xeditobj, "cmd", STRING_TO_JSVAL(js_str)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
				return JS_FALSE;

			if((js_str=JS_NewStringCopyZ(cx, p->cfg->xedit[l]->arstr))==NULL)
				return JS_FALSE;
			if(!JS_DefineProperty(cx, xeditobj, "ars", STRING_TO_JSVAL(js_str)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
				return JS_FALSE;

			if(!JS_DefineProperty(cx, xeditobj, "settings", INT_TO_JSVAL(p->cfg->xedit[l]->misc)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
				return JS_FALSE;

			if(!JS_DefineProperty(cx, xeditobj, "type", INT_TO_JSVAL(p->cfg->xedit[l]->type)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY))
				return JS_FALSE;

#ifdef BUILD_JSDOCS
			js_CreateArrayOfStrings(cx, xeditobj, "_property_desc_list", xedit_prop_desc, JSPROP_READONLY);
#endif
		}

#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx,xedit_array,"Associative array of all external editors (use internal code as index)",311);
		JS_DefineProperty(cx,xedit_array,"_assoc_array",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif
	}
	if(name)
		free(name);

	return JS_TRUE;
}

static JSBool js_xtrn_area_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_xtrn_area_resolve(cx, obj, JSID_VOID));
}

static void 
js_xtrn_area_finalize(JSContext *cx, JSObject *obj)
{
	struct js_xtrn_area_priv *p;

	if((p=(struct js_xtrn_area_priv*)JS_GetPrivate(cx,obj))==NULL)
		return;

	free(p);
	JS_SetPrivate(cx,obj,NULL);
}


static JSClass js_xtrn_area_class = {
     "XtrnArea"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_StrictPropertyStub		/* setProperty	*/
	,js_xtrn_area_enumerate	/* enumerate	*/
	,js_xtrn_area_resolve	/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_xtrn_area_finalize	/* finalize		*/
};

JSObject* js_CreateXtrnAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
										  ,user_t* user, client_t* client)
{
	JSObject* obj;
	struct js_xtrn_area_priv *p;

	obj = JS_DefineObject(cx, parent, "xtrn_area", &js_xtrn_area_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	p = (struct js_xtrn_area_priv *)malloc(sizeof(struct js_xtrn_area_priv));
	if (p == NULL)
		return NULL;

	memset(p,0,sizeof(*p));
	p->cfg = cfg;
	p->user = user;
	p->client = client;

	if(!JS_SetPrivate(cx, obj, p)) {
		free(p);
		return(NULL);
	}

#ifdef BUILD_JSDOCS
	// Ensure they're all created for JSDOCS
	js_xtrn_area_enumerate(cx, obj);
#endif

	return(obj);
}

#endif	/* JAVSCRIPT */
