/* Synchronet command shell/module interpreter */

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
#include "cmdshell.h"
#include "js_request.h"
#include "js_rtpool.h"
#include "readtext.h"

char ** sbbs_t::getstrvar(csi_t *bin, uint32_t name)
{
	int i;

	if (sysvar_pi >= MAX_SYSVARS)
		sysvar_pi = 0;
	switch (name) {
		case 0:
			return (char **)&(bin->str);
		case 0x490873f1:
			sysvar_p[sysvar_pi] = (char*)useron.alias;
			break;
		case 0x5de44e8b:
			sysvar_p[sysvar_pi] = (char*)useron.name;
			break;
		case 0x979ef1de:
			sysvar_p[sysvar_pi] = (char*)useron.handle;
			break;
		case 0xc8cd5fb7:
			sysvar_p[sysvar_pi] = (char*)useron.host;
			break;
		case 0xcc7aca99:
			sysvar_p[sysvar_pi] = (char*)useron.note;
			break;
		case 0xa842c43b:
			sysvar_p[sysvar_pi] = (char*)useron.address;
			break;
		case 0x4ee1ff3a:
			sysvar_p[sysvar_pi] = (char*)useron.location;
			break;
		case 0xf000aa78:
			sysvar_p[sysvar_pi] = (char*)useron.zipcode;
			break;
		case 0xcdb7e4a9:
			sysvar_p[sysvar_pi] = (char*)useron.pass;
			break;
		case 0x94d59a7a:
			sysvar_p[sysvar_pi] = (char*)useron.birth;
			break;
		case 0xec2b8fb8:
			sysvar_p[sysvar_pi] = (char*)useron.phone;
			break;
		case 0x08f65a2a:
			sysvar_p[sysvar_pi] = (char*)useron.connection;
			break;
		case 0xc7e0e8ce:
			sysvar_p[sysvar_pi] = (char*)useron.netmail;
			break;
		case 0xd3606303:
			sysvar_p[sysvar_pi] = (char*)useron.tmpext;
			break;
		case 0x3178f9d6:
			sysvar_p[sysvar_pi] = (char*)useron.comment;
			break;

		case 0x41239e21:
			sysvar_p[sysvar_pi] = (char*)connection;
			break;
		case 0x90fc82b4:
			sysvar_p[sysvar_pi] = (char*)cid;
			break;
		case 0x15755030:
			return (char **)&comspec;
		case 0x5E049062:
			sysvar_p[sysvar_pi] = question;
			break;

		case 0xf19cd046:
			sysvar_p[sysvar_pi] = (char*)wordwrap;
			break;

		default:
			if (bin->str_var && bin->str_var_name)
				for (i = 0; i < bin->str_vars; i++)
					if (bin->str_var_name[i] == name)
						return (char **)&(bin->str_var[i]);
			if (global_str_var && global_str_var_name)
				for (i = 0; i < global_str_vars; i++)
					if (global_str_var_name[i] == name)
						return &(global_str_var[i]);
			return NULL;
	}

	return (char **)&sysvar_p[sysvar_pi++];
}

int32_t * sbbs_t::getintvar(csi_t *bin, uint32_t name)
{
	int i;

	if (sysvar_li >= MAX_SYSVARS)
		sysvar_li = 0;
	switch (name) {
		case 0:
			sysvar_l[sysvar_li] = strtol((char*)bin->str, 0, 0);
			break;
		case 0x908ece53:
			sysvar_l[sysvar_li] = useron.number;
			break;
		case 0xdcedf626:
			sysvar_l[sysvar_li] = useron.uls;
			break;
		case 0xc1093f61:
			sysvar_l[sysvar_li] = useron.dls;
			break;
		case 0x2039a29f:
			sysvar_l[sysvar_li] = useron.posts;
			break;
		case 0x4a9f3955:
			sysvar_l[sysvar_li] = useron.emails;
			break;
		case 0x0c8dcf3b:
			sysvar_l[sysvar_li] = useron.fbacks;
			break;
		case 0x9a13bf95:
			sysvar_l[sysvar_li] = useron.etoday;
			break;
		case 0xc9082cbd:
			sysvar_l[sysvar_li] = useron.ptoday;
			break;
		case 0x7c72376d:
			sysvar_l[sysvar_li] = useron.timeon;
			break;
		case 0xac72c50b:
			sysvar_l[sysvar_li] = useron.textra;
			break;
		case 0x04807a11:
			sysvar_l[sysvar_li] = useron.logons;
			break;
		case 0x52996eab:
			sysvar_l[sysvar_li] = useron.ttoday;
			break;
		case 0x098bdfcb:
			sysvar_l[sysvar_li] = useron.tlast;
			break;
		case 0xbd1cee5d:
			sysvar_l[sysvar_li] = useron.ltoday;
			break;
		case 0x07954570:
			sysvar_l[sysvar_li] = useron.xedit;
			break;
		case 0xedf6aa98:
			sysvar_l[sysvar_li] = useron.shell;
			break;
		case 0x328ed476:
			sysvar_l[sysvar_li] = useron.level;
			break;
		case 0x9e70e855:
			sysvar_l[sysvar_li] = useron.gender;
			break;
		case 0x094cc42c:
			sysvar_l[sysvar_li] = useron.rows;
			break;
		case 0xabc4317e:
			sysvar_l[sysvar_li] = useron.prot;
			break;
		case 0x7dd9aac0:
			sysvar_l[sysvar_li] = useron.leech;
			break;
		case 0x7c602a37:
			return (int32_t *)&useron.misc;
		case 0x61be0d36:
			return (int32_t *)&useron.qwk;
		case 0x665ac227:
			return (int32_t *)&useron.chat;
		case 0x951341ab:
			return (int32_t *)&useron.flags1;
		case 0x0c1a1011:
			return (int32_t *)&useron.flags2;
		case 0x7b1d2087:
			return (int32_t *)&useron.flags3;
		case 0xe579b524:
			return (int32_t *)&useron.flags4;
		case 0x12e7d6d2:
			return (int32_t *)&useron.exempt;
		case 0xfed3115d:
			return (int32_t *)&useron.rest;
		case 0xb65dd6d4:
			return (int32_t *)&useron.ulb;
		case 0xabb91f93:
			return (int32_t *)&useron.dlb;
		case 0x92fb364f:
			return (int32_t *)&useron.cdt;
		case 0xd0a99c72:
			return (int32_t *)&useron.min;
		case 0xd7ae3022:
			return (int32_t *)&useron.freecdt;
		case 0x1ef214ef:
			return (int32_t *)&useron.firston;
		case 0x0ea515b1:
			return (int32_t *)&useron.laston;
		case 0x2aaf9bd3:
			return (int32_t *)&useron.expire;
		case 0x89c91dc8:
			return (int32_t *)&useron.pwmod;
		case 0x5b0d0c54:
			return (int32_t *)&useron.ns_time;

		case 0xae256560:
			return (int32_t *)&cur_rate;
		case 0x2b3c257f:
			return (int32_t *)&cur_cps;
		case 0x1c4455ee:
			return (int32_t *)&dte_rate;
		case 0x7fbf958e:
			return (int32_t *)&term->lncntr;
//		case 0x5c1c1500:
//			return((int32_t *)&tos);
		case 0x613b690e:
			return (int32_t *)&term->rows;
		case 0x205ace36:
			return (int32_t *)&autoterm;
		case 0x7d0ed0d1:
			return (int32_t *)&console;
		case 0xbf31a280:
			return (int32_t *)&answertime;
		case 0x83aa2a6a:
			return (int32_t *)&logontime;
		case 0xb50cb889:
			return (int32_t *)&ns_time;
		case 0xae92d249:
			return (int32_t *)&last_ns_time;
		case 0x97f99eef:
			return (int32_t *)&online;
		case 0x381d3c2a:
			return (int32_t *)&sys_status;
		case 0x7e29c819:
			return (int32_t *)&cfg.sys_misc;
		case 0x02408dc5:
			sysvar_l[sysvar_li] = sys_timezone(&cfg);
			break;
		case 0x78afeaf1:
			sysvar_l[sysvar_li] = cfg.sys_pwdays;
			break;
		case 0xd859385f:
			sysvar_l[sysvar_li] = cfg.sys_deldays;
			break;
		case 0x6392dc62:
			sysvar_l[sysvar_li] = cfg.sys_autodel;
			break;
		case 0x698d59b4:
			sysvar_l[sysvar_li] = cfg.sys_nodes;
			break;
		case 0x6fb1c46e:
			sysvar_l[sysvar_li] = cfg.sys_exp_warn;
			break;
		case 0xdf391ca7:
			sysvar_l[sysvar_li] = cfg.sys_lastnode;
			break;
		case 0xdd982780:
			sysvar_l[sysvar_li] = cfg.sys_autonode;
			break;
		case 0x709c07da:
			return (int32_t *)&cfg.node_misc;
		case 0xb17e7914:
			sysvar_l[sysvar_li] = cfg.valuser;
			break;
		case 0xe7a7fb07:
			sysvar_l[sysvar_li] = cfg.node_num;
			break;
		case 0x6c8e350a:
			sysvar_l[sysvar_li] = cfg.new_level;
			break;
		case 0xccfe7c5d:
			return (int32_t *)&cfg.new_flags1;
		case 0x55f72de7:
			return (int32_t *)&cfg.new_flags2;
		case 0x22f01d71:
			return (int32_t *)&cfg.new_flags3;
		case 0xbc9488d2:
			return (int32_t *)&cfg.new_flags4;
		case 0x4b0aeb24:
			return (int32_t *)&cfg.new_exempt;
		case 0x20cb6325:
			return (int32_t *)&cfg.new_rest;
		case 0x31178ba2:
			return (int32_t *)&cfg.new_cdt;
		case 0x7345219f:
			return (int32_t *)&cfg.new_min;
		case 0xb3f64be4:
			sysvar_l[sysvar_li] = cfg.new_shell;
			break;
		case 0xa278584f:
			return (int32_t *)&cfg.new_misc;
		case 0x7342a625:
			sysvar_l[sysvar_li] = cfg.new_expire;
			break;
		case 0x75dc4306:
			sysvar_l[sysvar_li] = cfg.new_prot;
			break;
		case 0xfb394e27:
			sysvar_l[sysvar_li] = cfg.expired_level;
			break;
		case 0x89b69753:
			return (int32_t *)&cfg.expired_flags1;
		case 0x10bfc6e9:
			return (int32_t *)&cfg.expired_flags2;
		case 0x67b8f67f:
			return (int32_t *)&cfg.expired_flags3;
		case 0xf9dc63dc:
			return (int32_t *)&cfg.expired_flags4;
		case 0x0e42002a:
			return (int32_t *)&cfg.expired_exempt;
		case 0x4569c62e:
			return (int32_t *)&cfg.expired_rest;
		case 0xfcf3542e:
			sysvar_l[sysvar_li] = (int32_t)(cfg.min_dspace / 1024);
			break;
		case 0xcf9ce02c:
			sysvar_l[sysvar_li] = cfg.cdt_min_value;
			break;
		case 0xfcb5b274:
			return (int32_t *)&cfg.cdt_per_dollar;
		case 0x4db200d2:
			sysvar_l[sysvar_li] = cfg.leech_pct;
			break;
		case 0x9a7d9cca:
			sysvar_l[sysvar_li] = cfg.leech_sec;
			break;
		case 0x396b7167:
			return (int32_t *)&cfg.netmail_cost;
		case 0x5eeaff21:
			sysvar_l[sysvar_li] = cfg.netmail_misc;
			break;
		case 0x82d9484e:
			return (int32_t *)&cfg.inetmail_cost;
		case 0xe558c608:
			return (int32_t *)&cfg.inetmail_misc;

		case 0xc6e8539d:
			return (int32_t *)&logon_ulb;
		case 0xdb0c9ada:
			return (int32_t *)&logon_dlb;
		case 0xac58736f:
			return (int32_t *)&logon_uls;
		case 0xb1bcba28:
			return (int32_t *)&logon_dls;
		case 0x9c5051c9:
			return (int32_t *)&logon_posts;
		case 0xc82ba467:
			return (int32_t *)&logon_emails;
		case 0x8e395209:
			return (int32_t *)&logon_fbacks;
		case 0x8b12ba9d:
			return (int32_t *)&posts_read;
		case 0xe51c1956:
			sysvar_l[sysvar_li] = (ulong)logfile_fp;
			break;

		case 0xeb6c9c73:
			sysvar_l[sysvar_li] = errorlevel;
			break;

		case 0x5aaccfc5:
			sysvar_l[sysvar_li] = errno;
			break;

		case 0x057e4cd4:
			sysvar_l[sysvar_li] = timeleft;
			break;

		case 0x1e5052a7:
			return (int32_t *)&cfg.max_minutes;
		case 0xedc643f1:
			return (int32_t *)&cfg.max_qwkmsgs;

		case 0x430178ec:
			return (int32_t *)&cfg.uq;

		case 0x455CB929:
			return &bin->ftp_mode;

		case 0x2105D2B9:
			return &bin->socket_error;

		case 0xA0023A2E:
			return (int32_t *)&startup->options;

		case 0x16E2585F:
			sysvar_l[sysvar_li] = client_socket;
			break;

		default:
			if (bin->int_var && bin->int_var_name)
				for (i = 0; i < bin->int_vars; i++)
					if (bin->int_var_name[i] == name)
						return &bin->int_var[i];
			if (global_int_var && global_int_var_name)
				for (i = 0; i < global_int_vars; i++)
					if (global_int_var_name[i] == name)
						return &global_int_var[i];
			return NULL;
	}

	return (int32_t*)&sysvar_l[sysvar_li++];
}

void sbbs_t::clearvars(csi_t *bin)
{
	bin->str_vars = 0;
	bin->str_var = NULL;
	bin->str_var_name = NULL;
	bin->int_vars = 0;
	bin->int_var = NULL;
	bin->int_var_name = NULL;
	bin->dirs = 0;
	bin->files = 0;
	bin->loops = 0;
	bin->sockets = 0;
	bin->retval = 0;
}

void sbbs_t::freevars(csi_t *bin)
{
	int i;

	if (bin->str_var) {
		for (i = 0; i < bin->str_vars; i++)
			if (bin->str_var[i])
				free(bin->str_var[i]);
		free(bin->str_var);
	}
	if (bin->int_var)
		free(bin->int_var);
	if (bin->str_var_name)
		free(bin->str_var_name);
	if (bin->int_var_name)
		free(bin->int_var_name);
	for (i = 0; i < bin->dirs; i++) {
		if (bin->dir[i] != NULL) {
			closedir(bin->dir[i]);
			bin->dir[i] = NULL;
		}
	}
	for (i = 0; i < bin->files; i++) {
		if (bin->file[i] != NULL) {
			fclose(bin->file[i]);
			bin->file[i] = NULL;
		}
	}
	for (i = 0; i < bin->sockets; i++) {
		if (bin->socket[i] != 0) {
			close_socket(bin->socket[i]);
			bin->socket[i] = 0;
		}
	}
}

/****************************************************************************/
/* Copies a new value (str) into the string variable pointed to by p		*/
/* re-allocating if necessary												*/
/****************************************************************************/
char * sbbs_t::copystrvar(csi_t *csi, char *p, char *str)
{
	char *np;   /* New pointer after realloc */
	int   i = 0;

	if (p != csi->str) {
		if (p)
			for (i = 0; i < MAX_SYSVARS; i++)
				if (p == sysvar_p[i])
					break;
		if (!p || i == MAX_SYSVARS) {      /* Not system variable */
			if ((np = (char*)realloc(p, strlen(str) + 1)) == NULL)
				errormsg(WHERE, ERR_ALLOC, "variable", strlen(str) + 1);
			else
				p = np;
		}
	}
	if (p)
		strcpy(p, str);
	return p;
}

#ifdef JAVASCRIPT

static JSBool
js_OperationCallback(JSContext *cx)
{
	JSBool  ret;
	sbbs_t* sbbs;

	JS_SetOperationCallback(cx, NULL);
	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL) {
		JS_SetOperationCallback(cx, js_OperationCallback);
		return JS_FALSE;
	}

	if (sbbs->js_callback.auto_terminate && !sbbs->online
	    && ++sbbs->js_callback.offline_counter >= 10) {
		JS_ReportWarning(cx, "Disconnected");
		sbbs->js_callback.counter = 0;
		JS_SetOperationCallback(cx, js_OperationCallback);
		return JS_FALSE;
	}

	ret = js_CommonOperationCallback(cx, &sbbs->js_callback);
	JS_SetOperationCallback(cx, js_OperationCallback);
	return ret;
}

int sbbs_t::js_execfile(const char *cmd, const char* startup_dir, JSObject* scope, JSContext* js_cx, JSObject* js_glob)
{
	char*                     p;
	char*                     args = NULL;
	char*                     fname;
	int                       argc = 0;
	char                      cmdline[MAX_PATH + 1];
	char                      path[MAX_PATH + 1];
	JSObject*                 js_scope = scope;
	JSObject*                 js_script = NULL;
	jsval                     old_js_argv = JSVAL_VOID;
	jsval                     old_js_argc = JSVAL_VOID;
	jsval                     old_js_exec_path = JSVAL_VOID;
	jsval                     old_js_exec_file = JSVAL_VOID;
	jsval                     old_js_exec_dir = JSVAL_VOID;
	jsval                     old_js_scope = JSVAL_VOID;
	jsval                     val;
	jsval                     rval;
	int32                     result = 0;
	struct js_event_list *    events;
	struct js_runq_entry *    rq_head;
	struct js_runq_entry *    rq_tail;
	struct js_listener_entry *listeners;


	if (js_cx == NULL)
		js_cx = this->js_cx;
	if (js_glob == NULL)
		js_glob = this->js_glob;

	if (js_cx == NULL) {
		errormsg(WHERE, ERR_EXEC, cmd, 0, "JavaScript context==NULL");
		return -1;
	}

	SAFECOPY(cmdline, cmd);
	p = strchr(cmdline, ' ');
	if (p != NULL) {
		*p = 0;
		args = p + 1;
		SKIP_WHITESPACE(args);
	}
	fname = cmdline;

	path[0] = 0;
	if (strcspn(fname, "/\\") == strlen(fname)) {
		const char* js_ext = "";
		if (getfext(fname) == NULL)
			js_ext = ".js";
		if (startup_dir != NULL && *startup_dir)
			SAFEPRINTF3(path, "%s%s%s", startup_dir, fname, js_ext);
		if (path[0] == 0 || !fexistcase(path)) {
			SAFEPRINTF3(path, "%s%s%s", cfg.mods_dir, fname, js_ext);
			if (cfg.mods_dir[0] == 0 || !fexistcase(path))
				SAFEPRINTF3(path, "%s%s%s", cfg.exec_dir, fname, js_ext);
		}
	} else
		SAFECOPY(path, fname);

	if (!fexistcase(path)) {
		errormsg(WHERE, ERR_OPEN, path, O_RDONLY);
		return -1;
	}

	JS_BEGINREQUEST(js_cx);
	if (js_scope == NULL)
		js_scope = JS_NewObject(js_cx, NULL, NULL, js_glob);

	if (js_scope != NULL) {

		if (scope != NULL) {
			if (JS_GetProperty(js_cx, scope, "argv", &old_js_argv))
				JS_AddValueRoot(js_cx, &old_js_argv);
			if (JS_GetProperty(js_cx, scope, "argc", &old_js_argc))
				JS_AddValueRoot(js_cx, &old_js_argc);
		}

		JSObject* argv = JS_NewArrayObject(js_cx, 0, NULL);

		JS_DefineProperty(js_cx, js_scope, "argv", OBJECT_TO_JSVAL(argv)
		                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

		/* Handle quoted "one arg" syntax here */
		if (args != NULL && argv != NULL) {
			while (*args) {
				if (*args == '"') {
					args++;
					p = strchr(args, '"');
				}
				else
					p = strchr(args, ' ');
				if (p != NULL)
					*p = 0;
				JSString* arg = JS_NewStringCopyZ(js_cx, args);
				if (arg == NULL)
					break;
				jsval     val = STRING_TO_JSVAL(arg);
				if (!JS_SetElement(js_cx, argv, argc, &val))
					break;
				argc++;
				if (p == NULL) /* last arg */
					break;
				args = p + 1;
				SKIP_WHITESPACE(args);
			}
		}
		JS_DefineProperty(js_cx, js_scope, "argc", INT_TO_JSVAL(argc)
		                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

		JS_ClearPendingException(js_cx);

		js_script = JS_CompileFile(js_cx, js_scope, path);
	}

	if (js_scope == NULL || js_script == NULL) {
		JS_ReportPendingException(js_cx);   /* Added Feb-2-2006, rswindell */
		JS_ENDREQUEST(js_cx);
		errormsg(WHERE, "compiling", path, 0);
		if (scope != NULL) {
			if (old_js_argv == JSVAL_VOID) {
				JS_DeleteProperty(js_cx, scope, "argv");
				JS_DeleteProperty(js_cx, scope, "argc");
			}
			else {
				JS_DefineProperty(js_cx, scope, "argv", old_js_argv
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
				JS_DefineProperty(js_cx, scope, "argc", old_js_argc
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			}
			JS_RemoveValueRoot(js_cx, &old_js_argv);
			JS_RemoveValueRoot(js_cx, &old_js_argc);
		}
		return -1;
	}

	if (scope == NULL) {
		js_callback.counter = 0;  // Reset loop counter
	}
	JS_SetOperationCallback(js_cx, js_OperationCallback);
	if (JS_GetProperty(js_cx, js_glob, "js", &val) && JSVAL_IS_OBJECT(val)) {
		JSObject* js = JSVAL_TO_OBJECT(val);

		if (js != nullptr) {
			if (JS_GetProperty(js_cx, js, "exec_path", &old_js_exec_path))
				JS_AddValueRoot(js_cx, &old_js_exec_path);
			if (JS_GetProperty(js_cx, js, "exec_file", &old_js_exec_file))
				JS_AddValueRoot(js_cx, &old_js_exec_file);
			if (JS_GetProperty(js_cx, js, "exec_dir", &old_js_exec_dir))
				JS_AddValueRoot(js_cx, &old_js_exec_dir);
			if (JS_GetProperty(js_cx, js, "scope", &old_js_scope))
				JS_AddValueRoot(js_cx, &old_js_scope);
		}
	}

	js_PrepareToExecute(js_cx, js_glob, path, startup_dir, js_scope);
	events = js_callback.events;
	js_callback.events = NULL;
	rq_head = js_callback.rq_head;
	js_callback.rq_head = NULL;
	rq_tail = js_callback.rq_tail;
	js_callback.rq_tail = NULL;
	listeners = js_callback.listeners;
	js_callback.listeners = NULL;
	if (!JS_ExecuteScript(js_cx, js_scope, js_script, &rval) && !js_callback.auto_terminated)
		result = -1;
	js_handle_events(js_cx, &js_callback, &terminated);
//	clearabort();

	JS_GetProperty(js_cx, js_scope, "exit_code", &rval);
	if (rval != JSVAL_VOID)
		JS_ValueToInt32(js_cx, rval, &result);

	js_EvalOnExit(js_cx, js_scope, &js_callback);

	JS_ReportPendingException(js_cx);   /* Added Dec-4-2005, rswindell */

	JS_DestroyScript(js_cx, js_script);

	if (scope == NULL)
		JS_ClearScope(js_cx, js_scope);
	else {
		if (old_js_argv == JSVAL_VOID) {
			JS_DeleteProperty(js_cx, scope, "argv");
			JS_DeleteProperty(js_cx, scope, "argc");
		}
		else {
			JS_DefineProperty(js_cx, scope, "argv", old_js_argv
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(js_cx, scope, "argc", old_js_argc
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		}
		JS_RemoveValueRoot(js_cx, &old_js_argv);
		JS_RemoveValueRoot(js_cx, &old_js_argc);
	}
	if (JS_GetProperty(js_cx, js_glob, "js", &val) && JSVAL_IS_OBJECT(val)) {
		JSObject* js = JSVAL_TO_OBJECT(val);
		if (js != nullptr) {
			JS_DefineProperty(js_cx, js, "exec_path", old_js_exec_path
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(js_cx, js, "exec_file", old_js_exec_file
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(js_cx, js, "exec_dir", old_js_exec_dir
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			JS_DefineProperty(js_cx, js, "scope", old_js_scope
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		}
		JS_RemoveValueRoot(js_cx, &old_js_exec_dir);
		JS_RemoveValueRoot(js_cx, &old_js_exec_file);
		JS_RemoveValueRoot(js_cx, &old_js_exec_path);
		JS_RemoveValueRoot(js_cx, &old_js_scope);
	}

	JS_GC(js_cx);

	js_callback.events = events;
	js_callback.rq_head = rq_head;
	js_callback.rq_tail = rq_tail;
	js_callback.listeners = listeners;

	JS_ENDREQUEST(js_cx);

	return result;
}

// Execute a JS Module in its own temporary JS runtime and context
int sbbs_t::js_execxtrn(const char *cmd, const char* startup_dir)
{
	int        result = -1;
	JSRuntime* js_runtime;
	JSObject*  js_glob;
	JSContext* js_cx = js_init(&js_runtime, &js_glob, "XtrnModule");
	if (js_cx != NULL) {
		js_create_user_objects(js_cx, js_glob);
		result = js_execfile(cmd, startup_dir, js_glob, js_cx, js_glob);
		JS_BEGINREQUEST(js_cx);
		JS_RemoveObjectRoot(js_cx, &js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
	}
	jsrt_Release(js_runtime);
	return result;
}
#endif

/* Execute a loadable module */
int sbbs_t::exec_mod(const char* name, struct loadable_module mod, bool* invoked, const char* fmt, ...)
{
	va_list argptr;
	const char* cmd = nullptr;

	if (invoked != NULL)
		*invoked = false;

	if (strListIndexOf(mod_callstack, name) >= 0) {
		if (invoked == nullptr)
			lprintf(LOG_ERR, "Attempt to recursively execute %s module", name);
		return -1;
	}

	size_t ars_count = strListCount(mod.ars);
	for (size_t i = 0; mod.cmd != nullptr && mod.cmd[i] != nullptr; ++i) {
		if (mod.cmd[i][0] == '\0')
			continue;
		if (i >= ars_count || chk_ars(mod.ars[i], &useron, &client)) {
			cmd = mod.cmd[i];
			break;
		}
	}
	if (cmd == nullptr) {
		if (mod.cmd != NULL && mod.cmd[0] != NULL && mod.cmd[0][0] != '\0')
			lprintf(LOG_DEBUG, "No user access to %s module", name);
		return -1;
	}

	char cmdline[MAX_PATH + 512];
	if (fmt == nullptr)
		SAFECOPY(cmdline, cmd);
	else {
		char args[512];
		va_start(argptr, fmt);
		vsnprintf(args, sizeof args, fmt, argptr);
		args[sizeof args - 1] = '\0';
		va_end(argptr);
		snprintf(cmdline, sizeof cmdline, "%s %s", cmd, args);
	}
	lprintf(LOG_DEBUG, "Executing %s module: %s", name, cmdline);
	strListPushPtr(&mod_callstack, name);
	int i = exec_bin(cmdline, &main_csi);
	strListPop(&mod_callstack);
	if (invoked != NULL)
		*invoked = true;
	if (i != 0)
		lprintf(LOG_DEBUG, "%s module (%s) returned %d", name, cmdline, i);
	return i;
}

/* Important change as of Nov-16-2006, 'cmdline' may contain args */
int sbbs_t::exec_bin(const char *cmdline, csi_t *csi, const char* startup_dir)
{
	char  str[MAX_PATH + 1];
	char  mod[MAX_PATH + 1];
	char  modname[MAX_PATH + 1];
	char* p;
	int   file;
	csi_t bin;

	if (cmdline == NULL || *cmdline == 0)
		return -33;
	SAFECOPY(mod, cmdline);
	p = mod;
	FIND_CHAR(p, ' ');
	if (*p) {
		*p = 0;               /* terminate 'mod' */
		p++;                /* skip space */
		SKIP_CHAR(p, ' ');   /* skip more spaces */
	}
	if (*p)
		strcpy(main_csi.str, p);

#ifdef JAVASCRIPT
	if ((p = getfext(mod)) != NULL && stricmp(p, ".js") == 0)
		return js_execfile(cmdline, startup_dir);
	if (p == NULL && startup_dir != NULL && *startup_dir) {
		SAFEPRINTF2(str, "%s%s.js", startup_dir, mod);
		if (fexistcase(str))
			return js_execfile(cmdline, startup_dir);
	}
	if (cfg.mods_dir[0]) {
		SAFEPRINTF2(str, "%s%s.js", cfg.mods_dir, mod);
		if (fexistcase(str))
			return js_execfile(cmdline, startup_dir);
	}
#endif

	SAFECOPY(modname, mod);
	if (!strchr(modname, '.'))
		strcat(modname, ".bin");

	SAFEPRINTF2(str, "%s%s", cfg.mods_dir, modname);
	if (cfg.mods_dir[0] == 0 || !fexistcase(str)) {

#ifdef JAVASCRIPT
		SAFEPRINTF2(str, "%s%s.js", cfg.exec_dir, mod);
		if (fexistcase(str))
			return js_execfile(cmdline, startup_dir);
#endif

		SAFEPRINTF2(str, "%s%s", cfg.exec_dir, modname);
		fexistcase(str);
	}
	if (!fexist(str)) {
		errormsg(WHERE, ERR_EXEC, mod, 0, "module doesn't exist");
		return -1;
	}
	if ((file = nopen(str, O_RDONLY)) == -1) {
		errormsg(WHERE, ERR_OPEN, str, O_RDONLY);
		return -1;
	}

	memcpy(&bin, csi, sizeof(csi_t));
	clearvars(&bin);
	bin.length = (size_t)filelength(file);
	if (bin.length < 1) {
		close(file);
		errormsg(WHERE, ERR_LEN, str, bin.length);
		return -1;
	}
	if ((bin.cs = (uchar *)malloc(bin.length)) == NULL) {
		close(file);
		errormsg(WHERE, ERR_ALLOC, str, bin.length);
		return -1;
	}
	if (read(file, bin.cs, bin.length) != (ssize_t)bin.length) {
		close(file);
		errormsg(WHERE, ERR_READ, str, bin.length);
		free(bin.cs);
		return -1;
	}
	close(file);

	bin.ip = bin.cs;
	bin.rets = 0;
	bin.cmdrets = 0;
	bin.misc = 0;

	while (exec(&bin) == 0)
		if (!(bin.misc & CS_OFFLINE_EXEC)) {
			checkline();
			if (!online)
				break;
		}

	freevars(&bin);
	free(bin.cs);
	csi->logic = bin.logic;
//	clearabort();
	return bin.retval;
}

/****************************************************************************/
/* Skcsi->ip to a specific instruction                                           */
/****************************************************************************/
void sbbs_t::skipto(csi_t *csi, uchar inst)
{
	int i, j;

	while (csi->ip < csi->cs + csi->length && ((inst & 0x80) || *csi->ip != inst)) {

		if (*csi->ip == CS_IF_TRUE || *csi->ip == CS_IF_FALSE
		    || (*csi->ip >= CS_IF_GREATER && *csi->ip <= CS_IF_LESS_OR_EQUAL)) {
			csi->ip++;
			skipto(csi, CS_ENDIF);
			csi->ip++;
			continue;
		}

		if (inst == CS_ELSEORENDIF
		    && (*csi->ip == CS_ELSE || *csi->ip == CS_ENDIF))
			break;

		if (inst == CS_NEXTCASE
		    && (*csi->ip == CS_CASE || *csi->ip == CS_DEFAULT
		        || *csi->ip == CS_END_SWITCH))
			break;

		if (*csi->ip == CS_SWITCH) {
			csi->ip++;
			csi->ip += 4; /* Skip variable name */
			skipto(csi, CS_END_SWITCH);
			csi->ip++;
			continue;
		}

		if (*csi->ip == CS_CASE) {
			csi->ip++;
			csi->ip += 4; /* Skip value */
			skipto(csi, CS_NEXTCASE);
			continue;
		}

		if (*csi->ip == CS_CMDKEY || *csi->ip == CS_CMDCHAR) {
			csi->ip += 2;
			skipto(csi, CS_END_CMD);
			csi->ip++;
			continue;
		}
		if (*csi->ip == CS_CMDSTR || *csi->ip == CS_CMDKEYS) {
			csi->ip++;              /* skip inst */
			while (*(csi->ip++));   /* skip string */
			skipto(csi, CS_END_CMD);
			csi->ip++;
			continue;
		}

		if (*csi->ip >= CS_FUNCTIONS) {
			csi->ip++;
			continue;
		}

		if (*csi->ip >= CS_MISC) {
			switch (*csi->ip) {
				case CS_VAR_INSTRUCTION:
					csi->ip++;
					switch (*(csi->ip++)) {
						case SHOW_VARS:
						case EMAIL_SECTION:
							continue;
						case PRINT_VAR:
						case DEFINE_STR_VAR:
						case DEFINE_INT_VAR:
						case DEFINE_GLOBAL_STR_VAR:
						case DEFINE_GLOBAL_INT_VAR:
						case TIME_INT_VAR:
						case STRUPR_VAR:
						case STRLWR_VAR:
						case TRUNCSP_STR_VAR:
						case CHKFILE_VAR:
						case COPY_CHAR:
						case STRIP_CTRL_STR_VAR:
							csi->ip += 4; /* Skip variable name */
							continue;
						case GETSTR_VAR:
						case GETNAME_VAR:
						case GETLINE_VAR:
						case GETSTRUPR_VAR:
						case SHIFT_STR_VAR:
						case SEND_FILE_VIA_VAR:
						case RECEIVE_FILE_VIA_VAR:
						case COMPARE_FIRST_CHAR:
						case SHIFT_TO_FIRST_CHAR:
						case SHIFT_TO_LAST_CHAR:
							csi->ip += 4; /* Skip variable name */
							csi->ip++;  /* Skip char */
							continue;
						case PRINTTAIL_VAR_MODE:
							csi->ip++;  /* Skip length */
						case PRINTFILE_VAR_MODE:
						case GETNUM_VAR:
							csi->ip += 4; /* Skip variable name */
							csi->ip += 2;  /* Skip max num */
							continue;
						case STRNCMP_VAR:
							csi->ip++;  /* Skip length */
						case SET_STR_VAR:
						case COMPARE_STR_VAR:
						case CAT_STR_VAR:
						case STRSTR_VAR:
						case TELNET_GATE_STR:
							csi->ip += 4; /* Skip variable name */
							while (*(csi->ip++));   /* skip string */
							continue;
						case FORMAT_TIME_STR:
							csi->ip += 4; /* Skip destination variable */
							while (*(csi->ip++));   /* Skip string */
							csi->ip += 4; /* Skip int variable */
							continue;
						case FORMAT_STR_VAR:    /* SPRINTF */
							csi->ip += 4; /* Skip destination variable */
						case VAR_PRINTF:
						case VAR_PRINTF_LOCAL:
							while (*(csi->ip++));   /* Skip string */
							j = *(csi->ip++); /* Skip number of arguments */
							for (i = 0; i < j; i++)
								csi->ip += 4; /* Skip arguments */
							continue;
						case SEND_FILE_VIA:
						case RECEIVE_FILE_VIA:
							csi->ip++;              /* Skip prot */
							while (*(csi->ip++));   /* Skip filepath */
							continue;
						case GETSTR_MODE:
						case STRNCMP_VARS:
							csi->ip++;  /* Skip length */
						default:
							csi->ip += 8; /* Skip two variable names or var & val */
							continue;
					}

				case CS_FIO_FUNCTION:
					csi->ip++;
					switch (*(csi->ip++)) {
						case FIO_OPEN:
							csi->ip += 4;             /* File handle */
							csi->ip += 2;             /* Access */
							while (*(csi->ip++));   /* path/filename */
							continue;
						case FIO_CLOSE:
						case FIO_FLUSH:
						case FIO_EOF:
						case REMOVE_FILE:
						case REMOVE_DIR:
						case CHANGE_DIR:
						case MAKE_DIR:
						case REWIND_DIR:
						case CLOSE_DIR:
							csi->ip += 4;             /* File handle */
							continue;
						case FIO_SET_ETX:
							csi->ip++;
							continue;
						case FIO_PRINTF:
							csi->ip += 4;             /* File handle */
							while (*(csi->ip++));   /* String */
							j = *(csi->ip++);         /* Number of arguments */
							for (i = 0; i < j; i++)
								csi->ip += 4;       /* Arguments */
							continue;
						case FIO_READ:
						case FIO_WRITE:
						case FIO_SEEK:
						case FIO_SEEK_VAR:
						case FIO_OPEN_VAR:
							csi->ip += 4;             /* File handle */
							csi->ip += 4;             /* Variable */
							csi->ip += 2;             /* Length/access */
							continue;
						case FIO_READ_VAR:
						case FIO_WRITE_VAR:
							csi->ip += 4;             /* File handle */
							csi->ip += 4;             /* Buf Variable */
							csi->ip += 4;             /* Length Variable */
							continue;
						default:
							csi->ip += 4;             /* File handle */
							csi->ip += 4;             /* Variable */
							continue;
					}

				case CS_NET_FUNCTION:
					csi->ip++;
					switch (*(csi->ip++)) {
						case CS_SOCKET_CONNECT:
							csi->ip += 4;             /* socket */
							csi->ip += 4;             /* address */
							csi->ip += 2;             /* port */
							continue;
						case CS_SOCKET_NREAD:
							csi->ip += 4;             /* socket */
							csi->ip += 4;             /* intvar */
							continue;
						case CS_SOCKET_READ:
						case CS_SOCKET_READLINE:
						case CS_SOCKET_PEEK:
							csi->ip += 4;             /* socket */
							csi->ip += 4;             /* buffer */
							csi->ip += 2;             /* length */
							continue;
						case CS_SOCKET_WRITE:
							csi->ip += 4;             /* socket */
							csi->ip += 4;             /* strvar */
							continue;

						case CS_FTP_LOGIN:
						case CS_FTP_GET:
						case CS_FTP_PUT:
						case CS_FTP_RENAME:
							csi->ip += 4;             /* socket */
							csi->ip += 4;             /* username/path */
							csi->ip += 4;             /* password/path */
							continue;
						case CS_FTP_DIR:
						case CS_FTP_CWD:
						case CS_FTP_DELETE:
							csi->ip += 4;             /* socket */
							csi->ip += 4;             /* path */
							continue;

						default:
							csi->ip += 4;             /* socket */
							continue;
					}

				case CS_COMPARE_ARS:
					csi->ip++;
					csi->ip += (*csi->ip);
					csi->ip++;
					break;
				case CS_TOGGLE_USER_MISC:
				case CS_COMPARE_USER_MISC:
				case CS_TOGGLE_USER_CHAT:
				case CS_COMPARE_USER_CHAT:
				case CS_TOGGLE_USER_QWK:
				case CS_COMPARE_USER_QWK:
					csi->ip += 5;
					break;
				case CS_REPLACE_TEXT:
					csi->ip += 3;     /* skip inst and text # */
					while (*(csi->ip++));   /* skip string */
					break;
				case CS_USE_INT_VAR:
					csi->ip += 7; // inst, var, offset, len
					break;
				default:
					csi->ip++;
			}
			continue;
		}

		if (*csi->ip == CS_ONE_MORE_BYTE) {
			if (inst == CS_END_LOOP && *(csi->ip + 1) == CS_END_LOOP)
				break;

			csi->ip++;              /* skip extension */
			csi->ip++;              /* skip instruction */

			if (*(csi->ip - 1) == CS_LOOP_BEGIN) {   /* nested loop */
				skipto(csi, CS_END_LOOP);
				csi->ip += 2;
			}

			continue;
		}

		if (*csi->ip == CS_TWO_MORE_BYTES) {
			csi->ip++;              /* skip extension */
			csi->ip++;              /* skip instruction */
			csi->ip++;              /* skip argument */
			continue;
		}

		if (*csi->ip == CS_THREE_MORE_BYTES) {
			csi->ip++;              /* skip extension */
			csi->ip++;              /* skip instruction */
			csi->ip += 2;             /* skip argument */
			continue;
		}

		if (*csi->ip == CS_STR_FUNCTION) {
			csi->ip++;              /* skip extension */
			csi->ip++;              /* skip instruction */
			while (*(csi->ip++));   /* skip string */
			continue;
		}

		if (*csi->ip >= CS_ASCIIZ) {
			csi->ip++;              /* skip inst */
			while (*(csi->ip++));   /* skip string */
			continue;
		}

		if (*csi->ip >= CS_THREE_BYTE) {
			csi->ip += 3;
			continue;
		}

		if (*csi->ip >= CS_TWO_BYTE) {
			csi->ip += 2;
			continue;
		}

		csi->ip++;
	}
}


int sbbs_t::exec(csi_t *csi)
{
	char        str[256];
	const char* path;
	uchar       buf[1025], ch;
	int         i, j, file;
	long        l;
	FILE *      stream;

	if (usrgrps)
		cursubnum = usrsub[curgrp][cursub[curgrp]];     /* Used for ARS */
	else
		cursubnum = INVALID_SUB;
	if (usrlibs) {
		curdirnum = usrdir[curlib][curdir[curlib]];       /* Used for ARS */
		path = cfg.dir[usrdir[curlib][curdir[curlib]]]->path;
	}
	else {
		curdirnum = INVALID_DIR;
		path = nulstr;
	}
	now = time(NULL);

	if (csi->ip >= csi->cs + csi->length)
		return 1;

	if (*csi->ip >= CS_FUNCTIONS)
		return exec_function(csi);

	/**********************************************/
	/* Miscellaneous variable length instructions */
	/**********************************************/

	if (*csi->ip >= CS_MISC)
		return exec_misc(csi, path);

	/********************************/
	/* ASCIIZ argument instructions */
	/********************************/

	if (*csi->ip >= CS_ASCIIZ) {
		switch (*(csi->ip++)) {
			case CS_STR_FUNCTION:
				switch (*(csi->ip++)) {
					case CS_LOGIN:
						csi->logic = login(csi->str, (char*)csi->ip);
						break;
					case CS_LOAD_TEXT:
						csi->logic = LOGIC_FALSE;
						for (i = 0; i < TOTAL_TEXT; i++)
							if (text[i] != text_sav[i]) {
								if (text[i] != nulstr)
									free(text[i]);
								text[i] = text_sav[i];
							}
						SAFEPRINTF2(str, "%s%s.dat"
						            , cfg.ctrl_dir, cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
						if ((stream = fnopen(&file, str, O_RDONLY)) == NULL) {
							errormsg(WHERE, ERR_OPEN, str, O_RDONLY);
							break;
						}
						for (i = 0; i < TOTAL_TEXT && !feof(stream); i++) {
							if ((text[i] = readtext(NULL, stream, i, NULL)) == NULL) {
								i--;
								continue;
							}
							if (!strcmp(text[i], text_sav[i])) {  /* If identical */
								free(text[i]);                  /* Don't alloc */
								text[i] = text_sav[i];
							}
							else if (text[i][0] == 0) {
								free(text[i]);
								text[i] = (char*)nulstr;
							}
						}
						if (i < TOTAL_TEXT) {
							fclose(stream);
							errormsg(WHERE, ERR_READ, str, TOTAL_TEXT);
							break;
						}
						fclose(stream);
						csi->logic = LOGIC_TRUE;
						break;
					default:
						errormsg(WHERE, ERR_CHK, "shell instruction", *(csi->ip - 1));
						break;
				}
				while (*(csi->ip++));    /* Find NULL */
				return 0;
			case CS_LOG:
				log(cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_GETCMD:
				csi->cmd = (uchar)getkeys((char*)csi->ip, 0);
				if ((char)csi->cmd == -1)
					csi->cmd = 3;
				break;
			case CS_CMDSTR:
				if (stricmp(csi->str, (char*)csi->ip)) {
					while (*(csi->ip++));       /* Find NULL */
					skipto(csi, CS_END_CMD);
					csi->ip++;
					return 0;
				}
				break;
			case CS_CMDKEYS:
				for (i = 0; csi->ip[i]; i++)
					if (csi->cmd == csi->ip[i])
						break;
				if (!csi->ip[i]) {
					while (*(csi->ip++));       /* Find NULL */
					skipto(csi, CS_END_CMD);
					csi->ip++;
					return 0;
				}
				break;
			case CS_GET_TEMPLATE:
				gettmplt(csi->str, (char*)csi->ip, K_LINE);
				if (sys_status & SS_ABORT)
					csi->str[0] = 0;
				csi->cmd = csi->str[0];
				break;
			case CS_TRASHCAN:
				csi->logic = !trashcan(csi->str, (char*)csi->ip);
				break;
			case CS_CREATE_SIF:
				create_sif_dat((char*)csi->ip, csi->str);
				break;
			case CS_READ_SIF:
				read_sif_dat((char*)csi->ip, csi->str);
				break;
			case CS_MNEMONICS:
				mnemonics((char*)csi->ip);
				break;
			case CS_PRINT:
				putmsg(cmdstr((char*)csi->ip, path, csi->str, (char*)buf), P_SAVEATR | P_NOABORT);
				break;
			case CS_PRINT_LOCAL:
				lputs(LOG_INFO, cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_PRINT_REMOTE:
				term_out(cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_PRINTFILE:
				printfile(cmdstr((char*)csi->ip, path, csi->str, (char*)buf), P_SAVEATR);
				break;
			case CS_PRINTFILE_REMOTE:
				if (online != ON_REMOTE)
					break;
				printfile(cmdstr((char*)csi->ip, path, csi->str, (char*)buf), P_SAVEATR);
				break;
			case CS_PRINTFILE_LOCAL:
				lprintf(LOG_WARNING, "PRINTFILE_LOCAL is no longer functional");
				break;
			case CS_CHKFILE:
				csi->logic = !fexistcase(cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_EXEC:
				external(cmdstr((char*)csi->ip, path, csi->str, (char*)buf), 0);
				break;
			case CS_EXEC_INT:
				external(cmdstr((char*)csi->ip, path, csi->str, (char*)buf), EX_STDIO);
				break;
			case CS_EXEC_XTRN:
				for (i = 0; i < cfg.total_xtrns; i++)
					if (!stricmp(cfg.xtrn[i]->code, (char*)csi->ip))
						break;
				if (i < cfg.total_xtrns)
					exec_xtrn(i);
				break;
			case CS_EXEC_BIN:
				exec_bin(cmdstr((char*)csi->ip, path, csi->str, (char*)buf), csi, /* startup_dir: */ NULL);
				break;
			case CS_YES_NO:
				csi->logic = !yesno(cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_NO_YES:
				csi->logic = !noyes(cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_MENU:
				menu(cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_SETSTR:
				strcpy(csi->str, cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_SET_MENU_DIR:
				cmdstr((char*)csi->ip, path, csi->str, menu_dir);
				break;
			case CS_SET_MENU_FILE:
				cmdstr((char*)csi->ip, path, csi->str, menu_file);
				break;
			case CS_COMPARE_STR:
				csi->logic = stricmp(csi->str, cmdstr((char*)csi->ip, path, csi->str, (char*)buf));
				break;
			case CS_COMPARE_KEYS:
				for (i = 0; csi->ip[i]; i++)
					if (csi->cmd == csi->ip[i])
						break;
				if (csi->ip[i])
					csi->logic = LOGIC_TRUE;
				else
					csi->logic = LOGIC_FALSE;
				break;
			case CS_COMPARE_WORD:
				csi->logic = strnicmp(csi->str, (char*)csi->ip, strlen((char*)csi->ip));
				break;
			default:
				errormsg(WHERE, ERR_CHK, "shell instruction", *(csi->ip - 1));
				break;
		}
		while (*(csi->ip++));    /* Find NULL */
		return 0;
	}

	if (*csi->ip >= CS_THREE_BYTE) {
		switch (*(csi->ip++)) {
			case CS_THREE_MORE_BYTES:
				errormsg(WHERE, ERR_CHK, "shell instruction", *(csi->ip - 1));
				return 0;
			case CS_GOTO:
				csi->ip = csi->cs + *((ushort *)(csi->ip));
				return 0;
			case CS_CALL:
				if (csi->rets < MAX_RETS) {
					csi->ret[csi->rets++] = csi->ip + 2;
					csi->ip = csi->cs + *((ushort *)(csi->ip));
				}
				return 0;
			case CS_MSWAIT:
				mswait(*(ushort *)csi->ip);
				csi->ip += 2;
				return 0;
			case CS_TOGGLE_NODE_MISC:
				if (getnodedat(cfg.node_num, &thisnode, true)) {
					thisnode.misc ^= *(ushort *)csi->ip;
					putnodedat(cfg.node_num, &thisnode);
				}
				csi->ip += 2;
				return 0;
			case CS_COMPARE_NODE_MISC:
				getnodedat(cfg.node_num, &thisnode);
				if ((thisnode.misc & *(ushort *)csi->ip) == *(ushort *)csi->ip)
					csi->logic = LOGIC_TRUE;
				else
					csi->logic = LOGIC_FALSE;
				csi->ip += 2;
				return 0;
			case CS_ADJUST_USER_CREDITS:
				i = *(short *)csi->ip;
				l = i * 1024L;
				if (l < 0)
					subtract_cdt(&cfg, &useron, -l);
				else
					useron.cdt = adjustuserval(&cfg, &useron, USER_CDT, l);
				csi->ip += 2;
				return 0;
			case CS_ADJUST_USER_MINUTES:
				i = *(short *)csi->ip;
				useron.min = (uint32_t)adjustuserval(&cfg, &useron, USER_MIN, i);
				csi->ip += 2;
				return 0;
			case CS_GETNUM:
				i = *(short *)csi->ip;
				csi->ip += 2;
				l = getnum(i);
				if (l <= 0) {
					csi->str[0] = 0;
					csi->logic = LOGIC_FALSE;
				}
				else {
					snprintf(csi->str, 128, "%lu", l);
					csi->logic = LOGIC_TRUE;
				}
				return 0;

			case CS_TOGGLE_USER_FLAG:
				i = *(csi->ip++);
				ch = *(csi->ip++);
				switch (i) {
					case '1':
						useron.flags1 ^= FLAG(ch);
						putuserflags(useron.number, USER_FLAGS1, useron.flags1);
						break;
					case '2':
						useron.flags2 ^= FLAG(ch);
						putuserflags(useron.number, USER_FLAGS2, useron.flags2);
						break;
					case '3':
						useron.flags3 ^= FLAG(ch);
						putuserflags(useron.number, USER_FLAGS3, useron.flags3);
						break;
					case '4':
						useron.flags4 ^= FLAG(ch);
						putuserflags(useron.number, USER_FLAGS4, useron.flags4);
						break;
					case 'R':
						useron.rest ^= FLAG(ch);
						putuserflags(useron.number, USER_REST, useron.rest);
						break;
					case 'E':
						useron.exempt ^= FLAG(ch);
						putuserflags(useron.number, USER_EXEMPT, useron.exempt);
						break;
					default:
						errormsg(WHERE, ERR_CHK, "user flag type", *(csi->ip - 2));
						return 0;
				}
				return 0;
			case CS_REVERT_TEXT:
				i = *(ushort *)csi->ip;
				csi->ip += 2;
				if ((ushort)i == 0xffff) {
					for (i = 0; i < TOTAL_TEXT; i++) {
						if (text[i] != text_sav[i] && text[i] != nulstr)
							free(text[i]);
						text[i] = text_sav[i];
					}
					return 0;
				}
				i--;
				if (i >= TOTAL_TEXT) {
					errormsg(WHERE, ERR_CHK, "revert text #", i);
					return 0;
				}
				if (text[i] != text_sav[i] && text[i] != nulstr)
					free(text[i]);
				text[i] = text_sav[i];
				return 0;
			default:
				errormsg(WHERE, ERR_CHK, "shell instruction", *(csi->ip - 1));
				return 0;
		}
	}

	if (*csi->ip >= CS_TWO_BYTE) {
		switch (*(csi->ip++)) {
			case CS_TWO_MORE_BYTES:
				switch (*(csi->ip++)) {
					case CS_USER_EVENT:
						user_event((user_event_t)*(csi->ip++));
						return 0;
				}
				errormsg(WHERE, ERR_CHK, "shell instruction", *(csi->ip - 1));
				return 0;
			case CS_SETLOGIC:
				csi->logic = *csi->ip++;
				return 0;
			case CS_CMDKEY:
				if (((*csi->ip) == CS_DIGIT && IS_DIGIT(csi->cmd))
				    || ((*csi->ip) == CS_EDIGIT && csi->cmd & 0x80
				        && IS_DIGIT(csi->cmd & 0x7f))) {
					csi->ip++;
					return 0;
				}
				if (csi->cmd != *csi->ip) {
					csi->ip++;
					skipto(csi, CS_END_CMD);         /* skip code */
				}
				csi->ip++;                          /* skip key */
				return 0;
			case CS_CMDCHAR:
				if (csi->cmd != *csi->ip) {
					csi->ip++;
					skipto(csi, CS_END_CMD);         /* skip code */
				}
				csi->ip++;                          /* skip key */
				return 0;
			case CS_NODE_ACTION:
				action = *csi->ip++;
				return 0;
			case CS_NODE_STATUS:
				if (getnodedat(cfg.node_num, &thisnode, true)) {
					thisnode.status = *csi->ip++;
					putnodedat(cfg.node_num, &thisnode);
				} else
					csi->ip++;
				return 0;
			case CS_MULTINODE_CHAT:
				multinodechat(*csi->ip++);
				return 0;
			case CS_GETSTR:
				csi->logic = LOGIC_TRUE;
				getstr(csi->str, *csi->ip++, 0);
				if (sys_status & SS_ABORT) {
					csi->str[0] = 0;
					csi->logic = LOGIC_FALSE;
				}
				if (csi->str[0] == '/' && csi->str[1])
					csi->cmd = csi->str[1] | 0x80;
				else
					csi->cmd = csi->str[0];
				return 0;
			case CS_GETLINE:
				getstr(csi->str, *csi->ip++, K_LINE);
				if (sys_status & SS_ABORT)
					csi->str[0] = 0;
				if (csi->str[0] == '/' && csi->str[1])
					csi->cmd = csi->str[1] | 0x80;
				else
					csi->cmd = csi->str[0];
				return 0;
			case CS_GETSTRUPR:
				getstr(csi->str, *csi->ip++, K_UPPER);
				if (sys_status & SS_ABORT)
					csi->str[0] = 0;
				if (csi->str[0] == '/' && csi->str[1])
					csi->cmd = csi->str[1] | 0x80;
				else
					csi->cmd = csi->str[0];
				return 0;
			case CS_GETNAME:
				getstr(csi->str, *csi->ip++, K_UPRLWR);
				if (sys_status & SS_ABORT)
					csi->str[0] = 0;
				return 0;
			case CS_SHIFT_STR:
				i = *(csi->ip++);
				j = strlen(csi->str);
				if (i > j)
					i = j;
				if (i)
					memmove(csi->str, csi->str + i, j + 1);
				return 0;
			case CS_COMPARE_KEY:
				if (((*csi->ip) == CS_DIGIT && IS_DIGIT(csi->cmd))
				    || ((*csi->ip) == CS_EDIGIT && csi->cmd & 0x80
				        && IS_DIGIT(csi->cmd & 0x7f))) {
					csi->ip++;
					csi->logic = LOGIC_TRUE;
				}
				else {
					if (csi->cmd == *(csi->ip++))
						csi->logic = LOGIC_TRUE;
					else
						csi->logic = LOGIC_FALSE;
				}
				return 0;
			case CS_COMPARE_CHAR:
				if (csi->cmd == *(csi->ip++))
					csi->logic = LOGIC_TRUE;
				else
					csi->logic = LOGIC_FALSE;
				return 0;
			case CS_SET_USER_LEVEL:
				useron.level = *(csi->ip++);
				putuserdec32(useron.number, USER_LEVEL, useron.level);
				return 0;
			case CS_SET_USER_STRING:
				csi->logic = LOGIC_FALSE;
				if (!csi->str[0]) {
					csi->ip++;
					return 0;
				}
				switch (*(csi->ip++)) {
					case USER_STRING_ALIAS:
						if (!IS_ALPHA(csi->str[0]) || trashcan(csi->str, "name"))
							break;
						i = matchuser(&cfg, csi->str, TRUE /*sysop_alias*/);
						if (i && i != useron.number)
							break;
						snprintf(useron.alias, sizeof useron.alias, "%.*s", LEN_ALIAS, csi->str);
						putuserstr(useron.number, USER_ALIAS, useron.alias);
						putusername(&cfg, useron.number, useron.alias);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_REALNAME:
						if (trashcan(csi->str, "name"))
							break;
						if (cfg.uq & UQ_DUPREAL
						    && finduserstr(useron.number, USER_NAME, csi->str))
							break;
						snprintf(useron.name, sizeof useron.name, "%.*s", LEN_NAME, csi->str);
						putuserstr(useron.number, USER_NAME
						           , useron.name);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_HANDLE:
						if (trashcan(csi->str, "name"))
							break;
						if (cfg.uq & UQ_DUPHAND
						    && finduserstr(useron.number, USER_HANDLE, csi->str))
							break;
						snprintf(useron.handle, sizeof useron.handle, "%.*s", LEN_HANDLE, csi->str);
						putuserstr(useron.number, USER_HANDLE
						           , useron.handle);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_COMPUTER:
						SAFECOPY(useron.host, csi->str);
						putuserstr(useron.number, USER_HOST
						           , useron.host);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_NOTE:
						snprintf(useron.note, sizeof useron.note, "%.*s", LEN_NOTE, csi->str);
						putuserstr(useron.number, USER_NOTE
						           , useron.note);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_ADDRESS:
						snprintf(useron.address, sizeof useron.address, "%.*s", LEN_ADDRESS, csi->str);
						putuserstr(useron.number, USER_ADDRESS
						           , useron.address);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_LOCATION:
						snprintf(useron.location, sizeof useron.location, "%.*s", LEN_LOCATION, csi->str);
						putuserstr(useron.number, USER_LOCATION
						           , useron.location);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_ZIPCODE:
						snprintf(useron.zipcode, sizeof useron.zipcode, "%.*s", LEN_ZIPCODE, csi->str);
						putuserstr(useron.number, USER_ZIPCODE
						           , useron.zipcode);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_PASSWORD:
						snprintf(useron.pass, sizeof useron.pass, "%.*s", LEN_PASS, csi->str);
						putuserstr(useron.number, USER_PASS
						           , useron.pass);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_BIRTHDAY:
						if (!getage(&cfg, csi->str))
							break;
						parse_birthdate(&cfg, csi->str, useron.birth, sizeof useron.birth);
						putuserstr(useron.number, USER_BIRTH
						           , useron.birth);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_PHONE:
						if (trashcan(csi->str, "phone"))
							break;
						snprintf(useron.phone, sizeof useron.phone, "%.*s", LEN_PHONE, csi->str);
						putuserstr(useron.number, USER_PHONE
						           , useron.phone);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_MODEM:
						snprintf(useron.connection, sizeof useron.connection, "%s", csi->str);
						putuserstr(useron.number, USER_CONNECTION
						           , useron.phone);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_IPADDR:
						snprintf(useron.ipaddr, sizeof useron.ipaddr, "%.*s", LEN_IPADDR, csi->str);
						putuserstr(useron.number, USER_IPADDR
						           , useron.phone);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_COMMENT:
						snprintf(useron.comment, sizeof useron.comment, "%.*s", LEN_COMMENT, csi->str);
						putuserstr(useron.number, USER_COMMENT
						           , useron.comment);
						csi->logic = LOGIC_TRUE;
						break;
					case USER_STRING_NETMAIL:
						snprintf(useron.netmail, sizeof useron.netmail, "%.*s", LEN_NETMAIL, csi->str);
						putuserstr(useron.number, USER_NETMAIL
						           , useron.netmail);
						csi->logic = LOGIC_TRUE;
						break;
					default:
						errormsg(WHERE, ERR_CHK, "user string type", *(csi->ip - 1));
						return 0;
				}
				return 0;
			default:
				errormsg(WHERE, ERR_CHK, "shell instruction", *(csi->ip - 1));
				return 0;
		}
	}


	/*********************************/
	/* Single Byte Instrcutions ONLY */
	/*********************************/

	switch (*(csi->ip++)) {
		case CS_ONE_MORE_BYTE:               /* Just one MORE byte */
			switch (*(csi->ip++)) {
				case CS_OFFLINE:
					csi->misc |= CS_OFFLINE_EXEC;
					return 0;
				case CS_ONLINE:
					csi->misc &= ~CS_OFFLINE_EXEC;
					return 0;
				case CS_NEWUSER:
					if (newuser())
						csi->logic = LOGIC_TRUE;
					else
						csi->logic = LOGIC_FALSE;
					return 0;
				case CS_LOGON:
					if (logon())
						csi->logic = LOGIC_TRUE;
					else
						csi->logic = LOGIC_FALSE;
					return 0;
				case CS_LOGOUT:
					logout();
					return 0;
				case CS_EXIT:
					return 1;
				case CS_LOOP_BEGIN:
					if (csi->loops < MAX_LOOPDEPTH)
						csi->loop_home[csi->loops++] = (csi->ip - 1);
					return 0;
				case CS_BREAK_LOOP:
					if (csi->loops) {
						skipto(csi, CS_END_LOOP);
						csi->ip += 2;
						csi->loops--;
					}
					return 0;
				case CS_END_LOOP:
				case CS_CONTINUE_LOOP:
					if (csi->loops)
						csi->ip = csi->loop_home[csi->loops - 1];
					return 0;
				default:
					errormsg(WHERE, ERR_CHK, "one byte extended function"
					         , *(csi->ip - 1));
					return 0;
			}
		case CS_CRLF:
			term->newline();
			return 0;
		case CS_CLS:
			cls();
			return 0;
		case CS_PAUSE:
			pause();
			return 0;
		case CS_PAUSE_RESET:
			term->lncntr = 0;
			return 0;
		case CS_GETLINES:
			getdimensions();
			return 0;
		case CS_HANGUP:
			hangup();
			return 0;
		case CS_LOGKEY:
			logch(csi->cmd, 0);
			return 0;
		case CS_LOGKEY_COMMA:
			logch(csi->cmd, 1);
			return 0;
		case CS_LOGSTR:
			log(csi->str);
			return 0;
		case CS_CHKSYSPASS:
			csi->logic = !chksyspass();
			return 0;
		case CS_PUT_NODE:
			if (getnodedat(cfg.node_num, &thisnode, true))
				putnodedat(cfg.node_num, &thisnode);
			return 0;
		case CS_SYNC:
			sync();
			return 0;
		case CS_ASYNC:
			sync();
			return 0;
		case CS_GETTIMELEFT:
			gettimeleft();
			return 0;
		case CS_RETURN:
			if (!csi->rets)
				return 1;
			csi->ip = csi->ret[--csi->rets];
			return 0;
		case CS_GETKEY:
			csi->cmd = getkey(K_UPPER);
			return 0;
		case CS_GETCHAR:
			csi->cmd = getkey(0);
			return 0;
		case CS_INKEY:
			csi->cmd = toupper(inkey(K_NONE, 1));
			if (csi->cmd)
				csi->logic = LOGIC_TRUE;
			else
				csi->logic = LOGIC_FALSE;
			return 0;
		case CS_INCHAR:
			csi->cmd = inkey(K_NONE, 1);
			if (csi->cmd)
				csi->logic = LOGIC_TRUE;
			else
				csi->logic = LOGIC_FALSE;
			return 0;
		case CS_GETKEYE:
			csi->cmd = getkey(K_UPPER);
			if (csi->cmd == '/') {
				outchar('/');
				csi->cmd = getkey(K_UPPER);
				csi->cmd |= 0x80;
			}
			return 0;
		case CS_GETFILESPEC:
			if (getfilespec(csi->str))
				csi->logic = LOGIC_TRUE;
			else
				csi->logic = LOGIC_FALSE;
			return 0;
		case CS_SAVELINE:
			term->saveline();
			return 0;
		case CS_RESTORELINE:
			term->restoreline();
			return 0;
		case CS_SELECT_SHELL:
			csi->logic = select_shell(&useron) ? LOGIC_TRUE:LOGIC_FALSE;
			return 0;
		case CS_SET_SHELL:
			csi->logic = set_shell(csi->str) ? LOGIC_TRUE:LOGIC_FALSE;
			return 0;

		case CS_SELECT_EDITOR:
			csi->logic = select_editor(&useron) ? LOGIC_TRUE:LOGIC_FALSE;
			return 0;
		case CS_SET_EDITOR:
			csi->logic = set_editor(csi->str) ? LOGIC_TRUE:LOGIC_FALSE;
			return 0;

		case CS_CLEAR_ABORT:
			clearabort();
			return 0;
		case CS_FINDUSER:
			i = finduser(csi->str);
			if (i) {
				csi->logic = LOGIC_TRUE;
				username(&cfg, i, csi->str);
			}
			else
				csi->logic = LOGIC_FALSE;
			return 0;
		case CS_UNGETKEY:
			ungetkey(csi->cmd & 0x7f, /* insert: */ true);
			return 0;
		case CS_UNGETSTR:
			j = strlen(csi->str);
			for (i = 0; i < j; i++)
				ungetkey(csi->str[i]);
			return 0;
		case CS_PRINTKEY:
			if ((csi->cmd & 0x7f) >= ' ')
				outchar(csi->cmd & 0x7f);
			return 0;
		case CS_PRINTSTR:
			putmsg(csi->str, P_SAVEATR | P_NOABORT | P_NOATCODES);
			return 0;
		case CS_CMD_HOME:
			if (csi->cmdrets < MAX_CMDRETS)
				csi->cmdret[csi->cmdrets++] = (csi->ip - 1);
			return 0;
		case CS_END_CMD:
			if (csi->cmdrets)
				csi->ip = csi->cmdret[--csi->cmdrets];
			return 0;
		case CS_CMD_POP:
			if (csi->cmdrets)
				csi->cmdrets--;
			return 0;
		case CS_IF_TRUE:
			if (csi->logic != LOGIC_TRUE) {
				skipto(csi, CS_ELSEORENDIF);
				csi->ip++;
			}
			return 0;
		case CS_IF_GREATER:
			if (csi->logic != LOGIC_GREATER) {
				skipto(csi, CS_ELSEORENDIF);
				csi->ip++;
			}
			return 0;
		case CS_IF_GREATER_OR_EQUAL:
			if (csi->logic != LOGIC_GREATER && csi->logic != LOGIC_EQUAL) {
				skipto(csi, CS_ELSEORENDIF);
				csi->ip++;
			}
			return 0;
		case CS_IF_LESS:
			if (csi->logic != LOGIC_LESS) {
				skipto(csi, CS_ELSEORENDIF);
				csi->ip++;
			}
			return 0;
		case CS_IF_LESS_OR_EQUAL:
			if (csi->logic != LOGIC_LESS && csi->logic != LOGIC_EQUAL) {
				skipto(csi, CS_ELSEORENDIF);
				csi->ip++;
			}
			return 0;
		case CS_IF_FALSE:
			if (csi->logic == LOGIC_TRUE) {
				skipto(csi, CS_ELSEORENDIF);
				csi->ip++;
			}
			return 0;
		case CS_ELSE:
			skipto(csi, CS_ENDIF);
			csi->ip++;
			return 0;
		case CS_END_CASE:
			skipto(csi, CS_END_SWITCH);
			csi->misc &= ~CS_IN_SWITCH;
			csi->ip++;
			return 0;
		case CS_DEFAULT:
		case CS_END_SWITCH:
			csi->misc &= ~CS_IN_SWITCH;
			return 0;
		case CS_ENDIF:
			return 0;
		default:
			errormsg(WHERE, ERR_CHK, "shell instruction", *(csi->ip - 1));
			return 0;
	}
}

bool sbbs_t::set_shell(const char* code)
{
	for (int i = 0; i < cfg.total_shells; ++i) {
		if (stricmp(cfg.shell[i]->code, code) == 0 && chk_ar(cfg.shell[i]->ar, &useron, &client)) {
			useron.shell = i;
			if (useron.number > 0 && !useron_is_guest())
				putuserstr(useron.number, USER_SHELL, cfg.shell[i]->code);
			return true;
		}
	}
	return false;
}

bool sbbs_t::set_shell(int shell_index)
{
	return set_shell(cfg.shell[shell_index]->code);
}

bool sbbs_t::select_shell(user_t* user)
{
	int i;

	for (i = 0; i < cfg.total_shells; i++)
		uselect(1, i, text[CommandShellHeading], cfg.shell[i]->name, user == &useron ? cfg.shell[i]->ar : nullptr);
	if ((i = uselect(0, user->shell, 0, 0, 0)) >= 0) {
		user->shell = i;
		if (user->number > 0 && !user_is_guest(user))
			putuserstr(user->number, USER_SHELL, cfg.shell[i]->code);
		return true;
	}
	return false;
}

bool sbbs_t::set_editor(const char* code)
{
	if (code == nullptr || *code == '\0') {
		useron.xedit = 0;
		if (useron.number > 0 && !useron_is_guest())
			putuserstr(useron.number, USER_XEDIT, "");
		return true;
	}
	for (int i = 0; i < cfg.total_xedits; ++i) {
		if (stricmp(cfg.xedit[i]->code, code) == 0 && chk_ar(cfg.xedit[i]->ar, &useron, &client)) {
			useron.xedit = i + 1;
			if (useron.number > 0 && !useron_is_guest())
				putuserstr(useron.number, USER_XEDIT, cfg.xedit[i]->code);
			return true;
		}
	}
	return false;
}

// Returns false if no editors were available
bool sbbs_t::select_editor(user_t* user)
{
	int i;

	uselect_count = 0;
	for (i = 0; i < cfg.total_xedits; i++)
		uselect(1, i, text[ExternalEditorHeading], cfg.xedit[i]->name, user == &useron ? cfg.xedit[i]->ar : nullptr);
	if (uselect_count < 1)
		return false;
	if ((i = uselect(0, user->xedit ? (user->xedit - 1):0, 0, 0, 0)) >= 0) {
		user->xedit = i + 1;
		if (user->number > 0 && !user_is_guest(user))
			putuserstr(user->number, USER_XEDIT, cfg.xedit[i]->code);
		return true;
	}
	return true;
}
