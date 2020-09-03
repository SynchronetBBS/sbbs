#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gen_defs.h>

#ifdef __unix__
	#define XP_UNIX
#else
	#define XP_PC
	#define XP_WIN
#endif

#include <jsapi.h>
#include "threadwrap.h"
#include "js_request.h"

#ifdef DEBUG_JS_REQUESTS

#define DEBUG

enum last_request_type {
	 LAST_REQUEST_TYPE_NONE
	,LAST_REQUEST_TYPE_BEGIN
	,LAST_REQUEST_TYPE_END
	,LAST_REQUEST_TYPE_SUSPEND
	,LAST_REQUEST_TYPE_RESUME
};
static const char *type_names[5]={"None", "BeginRequest", "EndRequest", "SuspendRequest", "ResumeRequest"};

struct request_log {
	JSContext*				cx;
	enum last_request_type	type;
	const char*				file;
	unsigned long			line;
	struct request_log*		next;
	struct request_log*		prev;
};

static struct request_log* first_request;
static int initialized=0;
static pthread_mutex_t	req_mutex;
static char str[1024];

static void logstr(void)
{
#if 0
	FILE *f;

	if((f=fopen("../data/logs/JS_RequestErrors.log", "a"))!=NULL) {
		fputs(str, f);
		fclose(f);
	}
	else {
		fputs(str, stderr);
	}
#else
	fputs(str, stderr);
#endif
}

static struct request_log *match_request(JSContext *cx)
{
	struct request_log *ent;

	for(ent=first_request; ent != NULL; ent=ent->next) {
		if(ent->cx==cx)
			return(ent);
	}

	ent=malloc(sizeof(struct request_log));
	if(ent != NULL) {
		memset(ent, 0, sizeof(struct request_log));
		ent->cx=cx;
		ent->type=LAST_REQUEST_TYPE_NONE;
		ent->prev=NULL;
		ent->next=first_request;
		if(first_request != NULL) {
			first_request->prev=ent;
		}
		first_request=ent;
		return(ent);
	}
	return(NULL);
}

static void initialize_request(void)
{
	pthread_mutex_init(&req_mutex, NULL);
	initialized=1;
}

void js_debug_beginrequest(JSContext *cx, const char *file, unsigned long line)
{
	struct request_log *req;

	if(!initialized)
		initialize_request();
	pthread_mutex_lock(&req_mutex);
	req=match_request(cx);
	if(req==NULL) {
		strcpy(str,"Missing req in Begin\n");
		logstr();
		return;
	}

	if(JS_IsInRequest(cx)) {
		sprintf(str,"Begin FROM REQUEST after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
	}

	switch(req->type) {
	case LAST_REQUEST_TYPE_NONE:
	case LAST_REQUEST_TYPE_END:
		break;
	case LAST_REQUEST_TYPE_BEGIN:
	case LAST_REQUEST_TYPE_SUSPEND:
	case LAST_REQUEST_TYPE_RESUME:
		sprintf(str,"Begin after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	}
	switch(JS_RequestDepth(cx)) {
	case 0:
		break;
	case 1:
	default:
		sprintf(str,"depth=%u at Begin after %s from %s:%lu at %s:%lu (%p)\n",JS_RequestDepth(cx),type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	}

	req->type=LAST_REQUEST_TYPE_BEGIN;
	req->file=file;
	req->line=line;
	pthread_mutex_unlock(&req_mutex);
	JS_BeginRequest(cx);
}

void js_debug_endrequest(JSContext *cx, const char *file, unsigned long line)
{
	struct request_log *req;

	if(!initialized)
		initialize_request();
	pthread_mutex_lock(&req_mutex);
	req=match_request(cx);
	if(req==NULL) {
		strcpy(str,"Missing req in End\n");
		logstr();
		return;
	}
	if(!JS_IsInRequest(cx)) {
		sprintf(str,"End WITHOUT REQUEST after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
	}
	switch(req->type) {
	case LAST_REQUEST_TYPE_BEGIN:
		if(req->file) {
			if(strcmp(req->file, file) != 0 || line < req->line) {
				sprintf(str,"Suspicious End after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
			}
			break;
		}
	case LAST_REQUEST_TYPE_RESUME:
		break;
	case LAST_REQUEST_TYPE_NONE:
	case LAST_REQUEST_TYPE_END:
	case LAST_REQUEST_TYPE_SUSPEND:
		sprintf(str,"End after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	}
	switch(JS_RequestDepth(cx)) {
	case 0:
	default:
		sprintf(str,"depth=%u at End after %s from %s:%lu at %s:%lu (%p)\n",JS_RequestDepth(cx),type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	case 1:
		break;
	}
	switch(JS_SuspendDepth(cx)) {
	case 0:
		break;
	default:
		sprintf(str,"Suspend depth=%u at End after %s from %s:%lu at %s:%lu (%p)\n",JS_RequestDepth(cx),type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	}

	req->type=LAST_REQUEST_TYPE_END;
	req->file=file;
	req->line=line;
	JS_EndRequest(cx);
	if(JS_RequestDepth(cx)==0) {
		if(req->prev != NULL)
			req->prev->next=req->next;
		if(req->next != NULL)
			req->next->prev=req->prev;
		if(first_request==req) {
			if(req->prev != NULL)
				first_request=req->prev;
			else
				first_request=req->next;
		}
		free(req);
	}
	pthread_mutex_unlock(&req_mutex);
}

jsrefcount js_debug_suspendrequest(JSContext *cx, const char *file, unsigned long line)
{
	struct request_log *req;
	jsrefcount ret;

	if(!initialized)
		initialize_request();
	pthread_mutex_lock(&req_mutex);
	req=match_request(cx);
	if(req==NULL) {
		strcpy(str,"Missing req in Suspend\n");
		logstr();
		return -1;
	}
	if(!JS_IsInRequest(cx)) {
		sprintf(str,"Suspend WITHOUT REQUEST after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
	}
	switch(req->type) {
	case LAST_REQUEST_TYPE_BEGIN:
	case LAST_REQUEST_TYPE_RESUME:
		break;
	case LAST_REQUEST_TYPE_NONE:
		if(req->file==NULL)			/* Assumed to be a provided request */
			break;
	case LAST_REQUEST_TYPE_END:
	case LAST_REQUEST_TYPE_SUSPEND:
		sprintf(str,"Suspend after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	}
	switch(JS_RequestDepth(cx)) {
	case 0:
	default:
		sprintf(str,"depth=%u at Suspend after %s from %s:%lu at %s:%lu (%p)\n",JS_RequestDepth(cx),type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	case 1:
		break;
	}

	req->type=LAST_REQUEST_TYPE_SUSPEND;
	req->file=file;
	req->line=line;
	ret=JS_SuspendRequest(cx);
	pthread_mutex_unlock(&req_mutex);
	return(ret);
}

void js_debug_resumerequest(JSContext *cx, jsrefcount rc, const char *file, unsigned long line)
{
	struct request_log *req;

	if(!initialized)
		initialize_request();
	pthread_mutex_lock(&req_mutex);
	req=match_request(cx);
	if(req==NULL) {
		strcpy(str,"Missing req in Resume\n");
		logstr();
		return;
	}
	if(!JS_IsInRequest(cx)) {
		sprintf(str,"Resume WITHOUT REQUEST after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
	}
	switch(req->type) {
	case LAST_REQUEST_TYPE_SUSPEND:
		break;
	case LAST_REQUEST_TYPE_NONE:
	case LAST_REQUEST_TYPE_END:
	case LAST_REQUEST_TYPE_BEGIN:
	case LAST_REQUEST_TYPE_RESUME:
		sprintf(str,"Resume after %s from %s:%lu at %s:%lu (%p)\n",type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	}
	switch(JS_RequestDepth(cx)) {
	case 0:
	default:
		sprintf(str,"depth=%u at Resume after %s from %s:%lu at %s:%lu (%p)\n",JS_RequestDepth(cx),type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	case 1:
		break;
	}
	switch(JS_SuspendDepth(cx)) {
	case 1:
	default:
		break;
	case 0:
		sprintf(str,"Suspend depth=%u at Resume after %s from %s:%lu at %s:%lu (%p)\n",JS_SuspendDepth(cx),type_names[req->type],req->file,req->line,file,line,req->cx);
		logstr();
		break;
	}


	req->type=LAST_REQUEST_TYPE_RESUME;
	req->file=file;
	req->line=line;
	pthread_mutex_unlock(&req_mutex);
	JS_ResumeRequest(cx, rc);
}

#endif
