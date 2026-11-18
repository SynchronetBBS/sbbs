/* Synchronet JavaScript "MsgBase" Object */

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
#include "js_request.h"
#include "userdat.h"
#include "filedat.h"

#ifdef JAVASCRIPT

typedef struct
{
	smb_t smb;
	int smb_result;
	JSBool debug;
	uint32_t first_msg;
	bool is_email;          // path-opened base requested as an e-mail base

} private_t;

typedef struct
{
	private_t *p;
	bool expand_fields;
	bool enumerated;
	smbmsg_t msg;
	post_t post;

} privatemsg_t;

/* Destructor */

static void js_finalize_msgbase(JSContext *cx, JSObject *obj)
{
	private_t* p;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return;

	if (SMB_IS_OPEN(&(p->smb)))
		smb_close(&(p->smb));

	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

/* Methods */

extern JSClass js_msgbase_class;

/* A path-opened base requested as an e-mail base: create it as one when it
 * is new, otherwise it must already be one (the stored attribute counts).
 * All under the header lock: a process that loses a race to create the base
 * must find it created, not create it again over messages saved to it */
static int open_email_path_base(smb_t* smb)
{
	int result;

	if ((result = smb_locksmbhdr(smb)) != SMB_SUCCESS)
		return result;
	if (smb_fgetlength(smb->shd_fp) < (off_t)(sizeof(smbhdr_t) + sizeof(smb->status))) {
		smbstatus_t status = {0};
		status.attr = SMB_EMAIL;
		smb->status = status;
		result = smb_create(smb); // unlocks the header on success
	}
	else if ((result = smb_getstatus(smb)) == SMB_SUCCESS
	         && !(smb->status.attr & SMB_EMAIL)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s is not an e-mail message base", smb->file);
		result = SMB_ERR_HDR_ATTR;
	}
	if (smb->smbhdr_locked)
		smb_unlocksmbhdr(smb);
	return result;
}

static JSBool
js_open(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;
	scfg_t*    scfg;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));
	if (scfg == NULL) {
		JS_ReportError(cx, "JS_GetRuntimePrivate returned NULL");
		return JS_FALSE;
	}

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (!subnum_is_valid(scfg, p->smb.subnum)) {
		const char* fname = getfname(p->smb.file);
		if (fname == p->smb.file || illegal_filename(fname)) {
			JS_ReportError(cx, "Invalid msgbase path: '%s'", p->smb.file);
			return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (!subnum_is_valid(scfg, p->smb.subnum) && p->smb.subnum != INVALID_SUB) {
		p->smb_result = smb_open(&(p->smb));
		if (p->smb_result == SMB_SUCCESS && p->is_email) {
			if ((p->smb_result = open_email_path_base(&(p->smb))) != SMB_SUCCESS)
				smb_close(&(p->smb));
		}
	} else
		p->smb_result = smb_open_sub(scfg, &(p->smb), p->smb.subnum);
	if (p->smb_result != SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}


static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc = JS_SUSPENDREQUEST(cx);
	smb_close(&(p->smb));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static bool parse_recipient_object(JSContext* cx, private_t* p, JSObject* hdr, smbmsg_t* msg)
{
	char*   cp = NULL;
	size_t  cp_sz = 0;
	char    to[256];
	ushort  nettype = NET_UNKNOWN;
	ushort  agent;
	int32   i32;
	jsval   val;
	scfg_t* scfg;
	int     smb_result = SMB_SUCCESS;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	if (JS_GetProperty(cx, hdr, "to", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"to\" string in recipient object");
			return false;
		}
	} else {
		if ((p != NULL) && (p->smb.status.attr & SMB_EMAIL)) { /* e-mail */
			JS_ReportError(cx, "\"to\" property not included in email recipient object");
			return false;                  /* "to" property required */
		}
		cp = strdup("All");
	}

	if ((smb_result = smb_hfield_str(msg, RECIPIENT, cp)) != SMB_SUCCESS) {
		JS_ReportError(cx, "Error %d adding RECIPIENT field to message header", smb_result);
		free(cp);
		goto err;
	}
	if ((p != NULL) && !(p->smb.status.attr & SMB_EMAIL)) {
		SAFECOPY(to, cp);
		strlwr(to);
		msg->idx.to = crc16(to, 0);
	}

	if (JS_GetProperty(cx, hdr, "to_list", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"to_list\" string in recipient object");
			return false;
		}
		if ((smb_result = smb_hfield_str(msg, RECIPIENTLIST, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding RECIPIENTLIST field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "cc_list", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"cc_list\" string in recipient object");
			return false;
		}
		if ((smb_result = smb_hfield_str(msg, SMB_CARBONCOPY, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding SMB_CARBONCOPY field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "to_ext", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"to_ext\" string in recipient object");
			return false;
		}
		if ((smb_result = smb_hfield_str(msg, RECIPIENTEXT, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding RECIPIENTEXT field to message header", smb_result);
			goto err;
		}
		if ((p != NULL) && (p->smb.status.attr & SMB_EMAIL))
			msg->idx.to = atoi(cp);
	}

	if (JS_GetProperty(cx, hdr, "to_org", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"to_org\" string in recipient object");
			return false;
		}
		if ((smb_result = smb_hfield_str(msg, RECIPIENTORG, cp)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding RECIPIENTORG field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "to_net_type", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32)) {
			free(cp);
			return false;
		}
		nettype = (ushort)i32;
	}

	if (JS_GetProperty(cx, hdr, "to_net_addr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"to_net_addr\" string in recipient object");
			return false;
		}
		if ((smb_result = smb_hfield_netaddr(msg, RECIPIENTNETADDR, cp, &nettype)) != SMB_SUCCESS) {
			free(cp);
			JS_ReportError(cx, "Error %d adding RECIPIENTADDR field to message header", smb_result);
			goto err;
		}
	}
	free(cp);

	if (nettype != NET_UNKNOWN) {
		if ((p != NULL) && (p->smb.status.attr & SMB_EMAIL)) {
			if (nettype == NET_QWK && msg->idx.to == 0) {
				char fulladdr[128];
				msg->idx.to = qwk_route(scfg, static_cast<const char *>(msg->to_net.addr), fulladdr, sizeof(fulladdr) - 1);
				if (fulladdr[0] == 0) {
					JS_ReportError(cx, "Unroutable QWKnet \"to_net_addr\" (%s) in recipient object"
					               , msg->to_net.addr);
					return false;
				}
				if ((smb_result = smb_hfield_str(msg, RECIPIENTNETADDR, fulladdr)) != SMB_SUCCESS) {
					JS_ReportError(cx, "Error %d adding RECIPIENTADDR field to message header"
					               , smb_result);
					goto err;
				}
				if (msg->idx.to != 0) {
					char ext[32];
					sprintf(ext, "%u", msg->idx.to);
					if ((smb_result = smb_hfield_str(msg, RECIPIENTEXT, ext)) != SMB_SUCCESS) {
						JS_ReportError(cx, "Error %d adding RECIPIENTEXT field to message header"
						               , smb_result);
						goto err;
					}
				}
			} else
				msg->idx.to = 0;
		}
		if ((smb_result = smb_hfield_bin(msg, RECIPIENTNETTYPE, nettype)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding RECIPIENTNETTYPE field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "to_agent", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32))
			return false;
		agent = (ushort)i32;
		if ((smb_result = smb_hfield_bin(msg, RECIPIENTAGENT, agent)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding RECIPIENTAGENT field to message header", smb_result);
			goto err;
		}
	}

	/* A per-recipient network attribute replaces the header's for this copy */
	if (JS_GetProperty(cx, hdr, "netattr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		uint32 netattr;
		if (!JS_ValueToECMAUint32(cx, val, &netattr))
			return false;
		msg->hdr.netattr = netattr;
	}

	return true;

err:
	if (smb_result != SMB_SUCCESS && p != NULL)
		p->smb_result = smb_result;
	return false;

}

static bool parse_header_object(JSContext* cx, private_t* p, JSObject* hdr, smbmsg_t* msg
                                , bool recipient)
{
	char*     cp = NULL;
	size_t    cp_sz = 0;
	char      from[256];
	ushort    nettype = NET_UNKNOWN;
	ushort    type;
	ushort    agent;
	int32     i32;
	uint32    u32;
	jsval     val;
	JSObject* array;
	JSObject* field;
	jsuint    i, len;
	const char* prop_name;
	uint16_t    hfield_type;
	int       smb_result = SMB_SUCCESS;

	if (hdr == NULL) {
		JS_ReportError(cx, "NULL header object");
		goto err;
	}

	if (recipient && !parse_recipient_object(cx, p, hdr, msg))
		goto err;

	if (msg->hdr.type != SMB_MSG_TYPE_BALLOT) {
		/* Required Header Fields */
		if (JS_GetProperty(cx, hdr, "subject", &val) && !JSVAL_NULL_OR_VOID(val)) {
			JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
			HANDLE_PENDING(cx, cp);
			if (cp == NULL) {
				JS_ReportError(cx, "Invalid \"subject\" string in header object");
				goto err;
			}
		} else
			cp = strdup("");

		if ((smb_result = smb_hfield_str(msg, SUBJECT, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SUBJECT field to message header", smb_result);
			goto err;
		}
		msg->idx.subj = smb_subject_crc(cp);
	}
	if (JS_GetProperty(cx, hdr, "from", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"from\" string in header object");
			goto err;
		}
	} else {
		JS_ReportError(cx, "\"from\" property required in header");
		goto err;   /* "from" property required */
	}
	if ((smb_result = smb_hfield_str(msg, SENDER, cp)) != SMB_SUCCESS) {
		JS_ReportError(cx, "Error %d adding SENDER field to message header", smb_result);
		goto err;
	}
	if ((p != NULL) && !(p->smb.status.attr & SMB_EMAIL) && msg->hdr.type != SMB_MSG_TYPE_BALLOT) {
		SAFECOPY(from, cp);
		strlwr(from);
		msg->idx.from = crc16(from, 0);
	}

	/* Optional Header Fields */
	if (JS_GetProperty(cx, hdr, "from_ext", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"from_ext\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDEREXT, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDEREXT field to message header", smb_result);
			goto err;
		}
		if ((p != NULL) && (p->smb.status.attr & SMB_EMAIL))
			msg->idx.from = atoi(cp);
	}

	if (JS_GetProperty(cx, hdr, "from_org", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"from_org\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDERORG, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERORG field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "from_net_type", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32)) {
			goto err;
		}
		nettype = (ushort)i32;
	}

	if (JS_GetProperty(cx, hdr, "from_net_addr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"from_net_addr\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_netaddr(msg, SENDERNETADDR, cp, &nettype)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERNETADDR field to message header", smb_result);
			goto err;
		}
	}

	if (nettype != NET_UNKNOWN) {
		if ((p != NULL) && (p->smb.status.attr & SMB_EMAIL))
			msg->idx.from = 0;
		if ((smb_result = smb_hfield_bin(msg, SENDERNETTYPE, nettype)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERNETTYPE field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "from_agent", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32))
			goto err;
		agent = (ushort)i32;
		if ((smb_result = smb_hfield_bin(msg, SENDERAGENT, agent)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERAGENT field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "from_ip_addr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"from_ip_addr\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDERIPADDR, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERIPADDR field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "from_host_name", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"from_host_name\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDERHOSTNAME, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERHOSTNAME field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "from_protocol", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"from_protocol\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDERPROTOCOL, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERPROTOCOL field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "from_port", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"from_port\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDERPORT, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERPORT field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "sender_userid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"sender_userid\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDERUSERID, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERUSERID field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "sender_server", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"sender_server\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDERSERVER, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERSERVER field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "sender_time", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"sender_time\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SENDERTIME, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SENDERTIME field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "replyto", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"replyto\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, REPLYTO, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTO field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "replyto_ext", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"replyto_ext\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, REPLYTOEXT, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTOEXT field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "replyto_org", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"replyto_org\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, REPLYTOORG, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTOORG field to message header", smb_result);
			goto err;
		}
	}

	nettype = NET_UNKNOWN;
	if (JS_GetProperty(cx, hdr, "replyto_net_type", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32))
			goto err;
		nettype = (ushort)i32;
	}
	if (JS_GetProperty(cx, hdr, "replyto_net_addr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"replyto_net_addr\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_netaddr(msg, REPLYTONETADDR, cp, &nettype)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTONETADDR field to message header", smb_result);
			goto err;
		}
	}
	if (nettype != NET_UNKNOWN) {
		if ((smb_result = smb_hfield_bin(msg, REPLYTONETTYPE, nettype)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTONETTYPE field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "replyto_agent", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32))
			goto err;
		agent = (ushort)i32;
		if ((smb_result = smb_hfield_bin(msg, REPLYTOAGENT, agent)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTOAGENT field to message header", smb_result);
			goto err;
		}
	}

	/* RFC822 headers */
	if (JS_GetProperty(cx, hdr, "replyto_list", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"replyto_list\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, REPLYTOLIST, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding REPLYTOLIST field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "id", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"id\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, RFC822MSGID, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding RFC822MSGID field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "reply_id", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"reply_id\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, RFC822REPLYID, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding RFC822REPLYID field to message header", smb_result);
			goto err;
		}
	}

	/* SMTP headers */
	if (JS_GetProperty(cx, hdr, "reverse_path", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"reverse_path\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SMTPREVERSEPATH, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SMTPREVERSEPATH field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "forward_path", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"forward_path\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, SMTPFORWARDPATH, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding SMTPFORWARDPATH field to message header", smb_result);
			goto err;
		}
	}

	/* USENET headers */
	if (JS_GetProperty(cx, hdr, "path", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"path\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, USENETPATH, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding USENETPATH field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "newsgroups", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"newsgroups\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, USENETNEWSGROUPS, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding USENETNEWSGROUPS field to message header", smb_result);
			goto err;
		}
	}

	/* FTN headers */
	if (JS_GetProperty(cx, hdr, "ftn_msgid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"ftn_msgid\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, FIDOMSGID, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOMSGID field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "ftn_reply", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"ftn_reply\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, FIDOREPLYID, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOREPLYID field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "ftn_area", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"ftn_area\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, FIDOAREA, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOAREA field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "ftn_flags", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"ftn_flags\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, FIDOFLAGS, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOFLAGS field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "ftn_pid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"ftn_pid\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, FIDOPID, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOPID field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "ftn_tid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"ftn_tid\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, FIDOTID, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOTID field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "ftn_charset", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"ftn_charset\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, FIDOCHARSET, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOCHARSET field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "ftn_bbsid", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"ftn_bbsid\" string in header object");
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, FIDOBBSID, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding FIDOBBSID field to message header", smb_result);
			goto err;
		}
	}

	if (JS_GetProperty(cx, hdr, "date", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"date\" string in header object");
			goto err;
		}
		msg->hdr.when_written = rfc822date(cp);
	}

	prop_name = "summary";
	hfield_type = SMB_SUMMARY;
	if (JS_GetProperty(cx, hdr, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"%s\" string in header object", prop_name);
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, hfield_type, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding %s field to message header", smb_result, smb_hfieldtype(hfield_type));
			goto err;
		}
	}

	prop_name = "tags";
	hfield_type = SMB_TAGS;
	if (JS_GetProperty(cx, hdr, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"%s\" string in header object", prop_name);
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, hfield_type, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding %s field to message header", smb_result, smb_hfieldtype(hfield_type));
			goto err;
		}
	}

	prop_name = "editor";
	hfield_type = SMB_EDITOR;
	if (JS_GetProperty(cx, hdr, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL) {
			JS_ReportError(cx, "Invalid \"%s\" string in header object", prop_name);
			goto err;
		}
		if ((smb_result = smb_hfield_str(msg, hfield_type, cp)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding %s field to message header", smb_result, smb_hfieldtype(hfield_type));
			goto err;
		}
	}

	prop_name = "columns";
	hfield_type = SMB_COLUMNS;
	if (JS_GetProperty(cx, hdr, prop_name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32)) {
			JS_ReportError(cx, "Invalid \"%s\" number in header object", prop_name);
			goto err;
		}
		uint8_t u8 = u32;
		if ((smb_result = smb_hfield_bin(msg, hfield_type, u8)) != SMB_SUCCESS) {
			JS_ReportError(cx, "Error %d adding %s field to message header", smb_result, smb_hfieldtype(hfield_type));
			goto err;
		}
	}

	/* Numeric Header Fields */
	if (JS_GetProperty(cx, hdr, "attr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.attr = (ushort)u32;
		msg->idx.attr = msg->hdr.attr;
	}
	if (JS_GetProperty(cx, hdr, "votes", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.votes = (ushort)u32;
		if (msg->hdr.type == SMB_MSG_TYPE_BALLOT)
			msg->idx.votes = msg->hdr.votes;
	}
	if (JS_GetProperty(cx, hdr, "auxattr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.auxattr = u32;
	}
	if (JS_GetProperty(cx, hdr, "netattr", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.netattr = u32;
	}
	if (JS_GetProperty(cx, hdr, "when_written_time", &val) && !JSVAL_NULL_OR_VOID(val))  {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.when_written = smb_when(u32, msg->hdr.when_written.zone);
	}
	if (JS_GetProperty(cx, hdr, "when_written_zone", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32))
			goto err;
		msg->hdr.when_written.zone = (short)i32;
	}
	if (JS_GetProperty(cx, hdr, "when_imported_time", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.when_imported.time = u32;
	}
	if (JS_GetProperty(cx, hdr, "when_imported_zone", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32))
			goto err;
		msg->hdr.when_imported.zone = (short)i32;
	}

	if (JS_GetProperty(cx, hdr, "thread_id", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.thread_id = u32;
	}
	if ((JS_GetProperty(cx, hdr, "thread_orig", &val) && (!JSVAL_NULL_OR_VOID(val)))
	    || (JS_GetProperty(cx, hdr, "thread_back", &val) && !JSVAL_NULL_OR_VOID(val))) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.thread_back = u32;
	}
	if (JS_GetProperty(cx, hdr, "thread_next", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.thread_next = u32;
	}
	if (JS_GetProperty(cx, hdr, "thread_first", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.thread_first = u32;
	}
	if (JS_GetProperty(cx, hdr, "delivery_attempts", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.delivery_attempts = u32;
	}

	if (JS_GetProperty(cx, hdr, "priority", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToInt32(cx, val, &i32))
			goto err;
		msg->hdr.priority = i32;
	}

	if (JS_GetProperty(cx, hdr, "field_list", &val) && JSVAL_IS_OBJECT(val)) {
		array = JSVAL_TO_OBJECT(val);
		len = 0;
		if (array == NULL || !JS_GetArrayLength(cx, array, &len)) {
			JS_ReportError(cx, "Invalid \"field_list\" array in header object");
			goto err;
		}

		for (i = 0; i < len; i++) {
			if (!JS_GetElement(cx, array, i, &val))
				continue;
			if (!JSVAL_IS_OBJECT(val))
				continue;
			field = JSVAL_TO_OBJECT(val);
			if (field == NULL || !JS_GetProperty(cx, field, "type", &val))
				continue;
			if (JSVAL_IS_STRING(val)) {
				JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
				HANDLE_PENDING(cx, cp);
				type = smb_hfieldtypelookup(cp);
			}
			else {
				if (!JS_ValueToInt32(cx, val, &i32))
					goto err;
				type = (ushort)i32;
			}
			if (!JS_GetProperty(cx, field, "data", &val))
				continue;
			JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
			HANDLE_PENDING(cx, cp);
			if (cp == NULL) {
				JS_ReportError(cx, "Invalid data string in \"field_list\" array");
				goto err;
			}
			if ((smb_result = smb_hfield_str(msg, type, cp)) != SMB_SUCCESS) {
				JS_ReportError(cx, "Error %d adding field (type %02Xh) to message header", smb_result, type);
				goto err;
			}
		}
	}

	/* MIME headers are stored as RFC822 header lines, as when a message arrives */
	for (i = 0; i < 2; i++) {
		static const char* const mime_props[] = { "mime_version", "content_type" };
		static const char* const mime_names[] = { "MIME-Version", "Content-Type" };
		/* a header read back from a base has these lines in field_list too */
		bool                     present = false;
		size_t                   namelen = strlen(mime_names[i]);
		for (int f = 0; f < msg->total_hfields && !present; f++) {
			present = msg->hfield[f].type == RFC822HEADER && msg->hfield_dat[f] != NULL
			          && strnicmp((const char*)msg->hfield_dat[f], mime_names[i], namelen) == 0
			          && ((const char*)msg->hfield_dat[f])[namelen] == ':';
		}
		if (!present && JS_GetProperty(cx, hdr, mime_props[i], &val) && !JSVAL_NULL_OR_VOID(val)) {
			JSVALUE_TO_RASTRING(cx, val, cp, &cp_sz, NULL);
			HANDLE_PENDING(cx, cp);
			if (cp == NULL) {
				JS_ReportError(cx, "Invalid \"%s\" string in header object", mime_props[i]);
				goto err;
			}
			size_t line_sz = strlen(mime_names[i]) + strlen(cp) + 3;
			char*  line = (char*)malloc(line_sz);
			if (line == NULL) {
				JS_ReportError(cx, "Error allocating %s header line", mime_names[i]);
				goto err;
			}
			snprintf(line, line_sz, "%s: %s", mime_names[i], cp);
			smb_result = smb_hfield_str(msg, RFC822HEADER, line);
			free(line);
			if (smb_result != SMB_SUCCESS) {
				JS_ReportError(cx, "Error %d adding %s field to message header", smb_result, mime_names[i]);
				goto err;
			}
		}
	}

	if (msg->hdr.number == 0 && JS_GetProperty(cx, hdr, "number", &val) && !JSVAL_NULL_OR_VOID(val)) {
		if (!JS_ValueToECMAUint32(cx, val, &u32))
			goto err;
		msg->hdr.number = u32;
	}

	if (cp)
		free(cp);
	return true;

err:
	if (smb_result != SMB_SUCCESS && p != NULL)
		p->smb_result = smb_result;
	if (cp)
		free(cp);
	return false;
}

/* obj *may* have been previously returned from get_msg_header() */
bool js_ParseMsgHeaderObject(JSContext* cx, JSObject* obj, smbmsg_t* msg)
{
	privatemsg_t* p;

	p = (privatemsg_t*)JS_GetPrivate(cx, obj);

	if (!parse_header_object(cx, p == NULL ? NULL : p->p, obj, msg, /* recipient */ true)) {
		smb_freemsgmem(msg);
		return false;
	}

	return true;
}

/* obj must've been previously returned from get_msg_header() */
bool js_GetMsgHeaderObjectPrivates(JSContext* cx, JSObject* obj, smb_t** smb, smbmsg_t** msg, post_t** post)
{
	privatemsg_t* p;

	if ((p = (privatemsg_t*)JS_GetPrivate(cx, obj)) == NULL)
		return false;

	if (smb != NULL) {
		if (p->p == NULL)
			return false;
		*smb = &(p->p->smb);
	}
	if (msg != NULL)
		*msg = &p->msg;
	if (post != NULL)
		*post = &p->post;

	return true;
}

static bool msg_offset_by_id(private_t* p, char* id, int32_t* offset)
{
	smbmsg_t msg;

	if ((p->smb_result = smb_getmsgidx_by_msgid(&(p->smb), &msg, id)) != SMB_SUCCESS)
		return false;

	*offset = msg.idx_offset;
	return true;
}

static bool set_msg_idx_properties(JSContext* cx, JSObject* obj, idxrec_t* idx, int32_t offset)
{
	jsval val;

	val = UINT_TO_JSVAL(idx->number);
	if (!JS_DefineProperty(cx, obj, "number", val
	                       , NULL, NULL, JSPROP_ENUMERATE))
		return false;

	if (idx->attr & MSG_VOTE && !(idx->attr & MSG_POLL)) {
		val = UINT_TO_JSVAL(idx->votes);
		if (!JS_DefineProperty(cx, obj, "votes", val
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return false;

		val = UINT_TO_JSVAL(idx->remsg);
		if (!JS_DefineProperty(cx, obj, "remsg", val
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return false;
	} else {    /* normal message */
		val = UINT_TO_JSVAL(idx->to);
		if (!JS_DefineProperty(cx, obj, "to", val
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return false;

		val = UINT_TO_JSVAL(idx->from);
		if (!JS_DefineProperty(cx, obj, "from", val
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return false;

		val = UINT_TO_JSVAL(idx->subj);
		if (!JS_DefineProperty(cx, obj, "subject", val
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return false;
	}
	val = UINT_TO_JSVAL(idx->attr);
	if (!JS_DefineProperty(cx, obj, "attr", val
	                       , NULL, NULL, JSPROP_ENUMERATE))
		return false;

	// confusingly, this is the msg.offset, not the idx.offset value
	val = INT_TO_JSVAL(offset);
	if (!JS_DefineProperty(cx, obj, "offset", val
	                       , NULL, NULL, JSPROP_ENUMERATE))
		return false;

	val = UINT_TO_JSVAL(idx->time);
	if (!JS_DefineProperty(cx, obj, "time", val
	                       , NULL, NULL, JSPROP_ENUMERATE))
		return false;

	return true;
}

static JSBool
js_get_msg_index(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      n;
	jsval      val;
	smbmsg_t   msg;
	JSObject*  idxobj;
	JSBool     by_offset = JS_FALSE;
	JSBool     include_votes = JS_FALSE;
	private_t* p;
	jsrefcount rc;
	JSObject*  proto;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg, 0, sizeof(msg));

	n = 0;
	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		by_offset = JSVAL_TO_BOOLEAN(argv[n++]);

	for (; n < argc; n++) {
		if (JSVAL_IS_BOOLEAN(argv[n]))
			include_votes = JSVAL_TO_BOOLEAN(argv[n]);
		else if (JSVAL_IS_NUMBER(argv[n])) {
			if (by_offset) {                         /* Get by offset */
				if (!JS_ValueToInt32(cx, argv[n], (int32*)&msg.idx_offset))
					return JS_FALSE;
			}
			else {                                  /* Get by number */
				if (!JS_ValueToECMAUint32(cx, argv[n], &msg.hdr.number))
					return JS_FALSE;
			}
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	p->smb_result = smb_getmsgidx(&(p->smb), &msg);
	JS_RESUMEREQUEST(cx, rc);
	if (p->smb_result != SMB_SUCCESS)
		return JS_TRUE;

	if (!include_votes && (msg.idx.attr & MSG_VOTE))
		return JS_TRUE;

	if (JS_GetProperty(cx, JS_GetGlobalObject(cx), "MsgBase", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx, val, &proto);
		if (JS_GetProperty(cx, proto, "IndexPrototype", &val) && !JSVAL_NULL_OR_VOID(val))
			JS_ValueToObject(cx, val, &proto);
		else
			proto = NULL;
	}
	else
		proto = NULL;

	if ((idxobj = JS_NewObject(cx, NULL, proto, obj)) == NULL)
		return JS_TRUE;

	set_msg_idx_properties(cx, idxobj, &msg.idx, msg.idx_offset);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(idxobj));

	return JS_TRUE;
}

static JSBool
js_get_index(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsrefcount rc;
	private_t* priv;
	uint32_t   off;
	JSObject*  array;
	idxrec_t*  idx;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((priv = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(priv->smb)))
		return JS_TRUE;

	off_t index_length = filelength(fileno(priv->smb.sid_fp));
	if (index_length < static_cast<off_t>(sizeof(*idx)))
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	if (smb_getstatus(&(priv->smb)) != SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	if ((array = JS_NewArrayObject(cx, 0, NULL)) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "JS_NewArrayObject failure");
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	uint32_t total_msgs = (uint32_t)(index_length / sizeof(*idx));
	if (total_msgs > priv->smb.status.total_msgs)
		total_msgs = priv->smb.status.total_msgs;
	if (total_msgs < 1) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	if ((idx = static_cast<idxrec_t *>(calloc(total_msgs, sizeof(*idx)))) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "malloc failure of %u bytes on line %d in %s of %s", total_msgs * sizeof(*idx), WHERE);
		return JS_FALSE;
	}

	if ((priv->smb_result = smb_locksmbhdr(&(priv->smb))) != SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		free(idx);
		return JS_TRUE;
	}

	rewind(priv->smb.sid_fp);
	size_t fread_result = fread(idx, sizeof(*idx), total_msgs, priv->smb.sid_fp);
	smb_unlocksmbhdr(&(priv->smb));
	JS_RESUMEREQUEST(cx, rc);

	if (fread_result != total_msgs) {
		JS_ReportError(cx, "index read failed (%lu instead of %lu)", fread_result, total_msgs);
		free(idx);
		return JS_FALSE;
	}
	for (off = 0; off < total_msgs; off++) {
		JSObject* idxobj;
		if ((idxobj = JS_NewObject(cx, NULL, NULL, array)) == NULL) {
			JS_ReportError(cx, "object allocation failure, line %d", __LINE__);
			free(idx);
			return JS_FALSE;
		}
		set_msg_idx_properties(cx, idxobj, &idx[off], off);
		JS_DefineElement(cx, array, off, OBJECT_TO_JSVAL(idxobj), NULL, NULL, JSPROP_ENUMERATE);
	}
	free(idx);

	return JS_TRUE;
}

#define LAZY_BOOLEAN(PropName, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			v = BOOLEAN_TO_JSVAL((PropValue)); \
			JS_DefineProperty(cx, obj, (PropName), v, NULL, NULL, flags); \
			if (name) return JS_TRUE; \
		}

#define LAZY_INTEGER(PropName, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			v = INT_TO_JSVAL((PropValue)); \
			JS_DefineProperty(cx, obj, (PropName), v, NULL, NULL, flags); \
			if (name) return JS_TRUE; \
		}

#define LAZY_UINTEGER(PropName, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			v = UINT_TO_JSVAL((PropValue)); \
			JS_DefineProperty(cx, obj, (PropName), v, NULL, NULL, flags); \
			if (name) return JS_TRUE; \
		}

#define LAZY_UINTEGER_EXPAND(PropName, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			if (p->expand_fields || (PropValue)) { \
				v = UINT_TO_JSVAL((PropValue)); \
				JS_DefineProperty(cx, obj, (PropName), v, NULL, NULL, flags); \
				if (name) return JS_TRUE; \
			} \
			else if (name) return JS_TRUE; \
		}

#define LAZY_UINTEGER_COND(PropName, Condition, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			if (Condition) { \
				v = UINT_TO_JSVAL((uint32_t)(PropValue)); \
				JS_DefineProperty(cx, obj, (PropName), v, NULL, NULL, flags); \
				if (name) return JS_TRUE; \
			} \
			else if (name) return JS_TRUE; \
		}

#define LAZY_STRING(PropName, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			if ((js_str = JS_NewStringCopyZ(cx, (PropValue))) != NULL) { \
				JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, flags); \
				if (name) return JS_TRUE; \
			} \
			else if (name) return JS_TRUE; \
		}

#define LAZY_STRING_TRUNCSP(PropName, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			if ((js_str = JS_NewStringCopyZ(cx, truncsp(PropValue))) != NULL) { \
				JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, flags); \
				if (name) return JS_TRUE; \
			} \
			else if (name) return JS_TRUE; \
		}

#define LAZY_STRING_COND(PropName, Condition, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			if ((Condition) && (js_str = JS_NewStringCopyZ(cx, (PropValue))) != NULL) { \
				JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, flags); \
				if (name) return JS_TRUE; \
			} \
			else if (name) return JS_TRUE; \
		}

#define LAZY_STRING_TRUNCSP_NULL(PropName, PropValue, flags) \
		if (name == NULL || strcmp(name, (PropName)) == 0) { \
			if (name) free(name); \
			if ((PropValue) != NULL && (js_str = JS_NewStringCopyZ(cx, truncsp(PropValue))) != NULL) { \
				JS_DefineProperty(cx, obj, PropName, STRING_TO_JSVAL(js_str), NULL, NULL, flags); \
				if (name) return JS_TRUE; \
			} \
			else if (name) return JS_TRUE; \
		}

static JSBool js_get_msg_header_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char          date[128];
	char          msg_id[256];
	char          reply_id[256];
	char          tmp[128];
	char*         val;
	int           i;
	smbmsg_t      remsg;
	JSObject*     array;
	JSObject*     field;
	JSString*     js_str;
	jsint         items;
	jsval         v;
	privatemsg_t* p;
	char*         name = NULL;
	jsrefcount    rc;
	scfg_t*       scfg;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}

	/* If we have already enumerated, we're done here... */
	if ((p = (privatemsg_t*)JS_GetPrivate(cx, obj)) == NULL || p->enumerated) {
		if (name)
			free(name);
		return JS_TRUE;
	}

	if ((p->msg).hdr.number == 0) { /* No valid message number/id/offset specified */
		if (name)
			free(name);
		return JS_TRUE;
	}

	LAZY_UINTEGER("number", p->msg.hdr.number, JSPROP_ENUMERATE);
	LAZY_UINTEGER("offset", p->msg.idx_offset, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP("to", p->msg.to, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP("from", p->msg.from, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP("subject", p->msg.subj, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("to_list", p->msg.to_list, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("cc_list", p->msg.cc_list, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("summary", p->msg.summary, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("tags", p->msg.tags, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("to_ext", p->msg.to_ext, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("from_ext", p->msg.from_ext, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("from_org", p->msg.from_org, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("replyto", p->msg.replyto, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("replyto_ext", p->msg.replyto_ext, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("replyto_list", p->msg.replyto_list, JSPROP_ENUMERATE);
	if (p->expand_fields) {
		LAZY_STRING_TRUNCSP_NULL("reverse_path", p->msg.reverse_path, JSPROP_ENUMERATE);
	} else {
		LAZY_STRING_COND("reverse_path", (val = static_cast<char *>(smb_get_hfield(&(p->msg), SMTPREVERSEPATH, NULL))) != NULL, val, JSPROP_ENUMERATE);
	}
	LAZY_STRING_TRUNCSP_NULL("forward_path", p->msg.forward_path, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("to_agent", p->msg.to_agent, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("from_agent", p->msg.from_agent, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("replyto_agent", p->msg.replyto_agent, JSPROP_ENUMERATE);
	LAZY_UINTEGER_COND("to_net_type", p->msg.to_net.addr != NULL, p->msg.to_net.type, JSPROP_ENUMERATE);
	LAZY_STRING_COND("to_net_addr", p->msg.to_net.addr, smb_netaddrstr(&(p->msg).to_net, tmp), JSPROP_ENUMERATE);
	LAZY_UINTEGER_COND("from_net_type", p->msg.from_net.addr != NULL, p->msg.from_net.type, JSPROP_ENUMERATE);
	/* exception here because p->msg.from_net is NULL */
	LAZY_STRING_COND("from_net_addr", p->msg.from_net.addr, smb_netaddrstr(&(p->msg).from_net, tmp), JSPROP_ENUMERATE);
	LAZY_UINTEGER_COND("replyto_net_type", p->msg.replyto_net.addr != NULL, p->msg.replyto_net.type, JSPROP_ENUMERATE);
	LAZY_STRING_COND("replyto_net_addr", p->msg.replyto_net.addr, smb_netaddrstr(&(p->msg).replyto_net, tmp), JSPROP_ENUMERATE);
	LAZY_STRING_COND("from_ip_addr", (val = static_cast<char *>(smb_get_hfield(&(p->msg), SENDERIPADDR, NULL))) != NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("from_host_name", (val = static_cast<char *>(smb_get_hfield(&(p->msg), SENDERHOSTNAME, NULL))) != NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("from_protocol", (val = static_cast<char *>(smb_get_hfield(&(p->msg), SENDERPROTOCOL, NULL))) != NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("from_port", (val = static_cast<char *>(smb_get_hfield(&(p->msg), SENDERPORT, NULL))) != NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("sender_userid", (val = static_cast<char *>(smb_get_hfield(&(p->msg), SENDERUSERID, NULL))) != NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("sender_server", (val = static_cast<char *>(smb_get_hfield(&(p->msg), SENDERSERVER, NULL))) != NULL, val, JSPROP_ENUMERATE);
	LAZY_STRING_COND("sender_time", (val = static_cast<char *>(smb_get_hfield(&(p->msg), SENDERTIME, NULL))) != NULL, val, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("forwarded", p->msg.forwarded, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("expiration", p->msg.expiration, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("cost", (uint32)p->msg.cost, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("editor", p->msg.editor, JSPROP_ENUMERATE);
	LAZY_UINTEGER_EXPAND("columns", p->msg.columns, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("mime_version", p->msg.mime_version, JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_STRING_TRUNCSP_NULL("content_type", p->msg.content_type, JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_STRING_TRUNCSP_NULL("text_charset", p->msg.text_charset, JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_STRING_TRUNCSP_NULL("text_subtype", p->msg.text_subtype, JSPROP_ENUMERATE | JSPROP_READONLY);

	/* Fixed length portion of msg header */
	LAZY_UINTEGER("type", p->msg.hdr.type, JSPROP_ENUMERATE);
	LAZY_UINTEGER("version", p->msg.hdr.version, JSPROP_ENUMERATE);
	LAZY_UINTEGER("attr", p->msg.hdr.attr, JSPROP_ENUMERATE);
	LAZY_UINTEGER("auxattr", p->msg.hdr.auxattr, JSPROP_ENUMERATE);
	LAZY_UINTEGER("netattr", p->msg.hdr.netattr, JSPROP_ENUMERATE);
	LAZY_UINTEGER("when_written_time", (uint32_t)smb_time(p->msg.hdr.when_written), JSPROP_ENUMERATE);
	LAZY_INTEGER("when_written_zone", p->msg.hdr.when_written.zone, JSPROP_ENUMERATE);
	LAZY_INTEGER("when_written_zone_offset", smb_tzutc(p->msg.hdr.when_written.zone), JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_UINTEGER("when_imported_time", p->msg.hdr.when_imported.time, JSPROP_ENUMERATE);
	LAZY_INTEGER("when_imported_zone", p->msg.hdr.when_imported.zone, JSPROP_ENUMERATE);
	LAZY_INTEGER("when_imported_zone_offset", smb_tzutc(p->msg.hdr.when_imported.zone), JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_UINTEGER("thread_id", p->msg.hdr.thread_id, JSPROP_ENUMERATE);
	LAZY_UINTEGER("thread_back", p->msg.hdr.thread_back, JSPROP_ENUMERATE);
	LAZY_UINTEGER("thread_orig", p->msg.hdr.thread_back, 0);
	LAZY_UINTEGER("thread_next", p->msg.hdr.thread_next, JSPROP_ENUMERATE);
	LAZY_UINTEGER("thread_first", p->msg.hdr.thread_first, JSPROP_ENUMERATE);
	LAZY_UINTEGER("delivery_attempts", p->msg.hdr.delivery_attempts, JSPROP_ENUMERATE);
	LAZY_UINTEGER("data_length", smb_getmsgdatlen(&(p->msg)), JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_UINTEGER("text_length", smb_getmsgtxtlen(&(p->msg)), JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_STRING("date", msgdate((p->msg).hdr.when_written, date), JSPROP_ENUMERATE);
	LAZY_UINTEGER("votes", p->msg.hdr.votes, JSPROP_ENUMERATE);
	LAZY_UINTEGER_COND("priority", p->msg.hdr.priority != SMB_PRIORITY_UNSPECIFIED, p->msg.hdr.priority, JSPROP_ENUMERATE);

	// Convenience (not technically part of header: */
	LAZY_UINTEGER("upvotes", p->msg.upvotes, JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_UINTEGER("downvotes", p->msg.downvotes, JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_UINTEGER("total_votes", p->msg.total_votes, JSPROP_ENUMERATE | JSPROP_READONLY);
	LAZY_BOOLEAN("is_utf8", smb_msg_is_utf8(&p->msg), JSPROP_ENUMERATE | JSPROP_READONLY);

	if (name == NULL || strcmp(name, "reply_id") == 0) {
		if (name)
			free(name);
		/* Reply-ID (References) */
		if ((p->msg).reply_id != NULL)
			val = (p->msg).reply_id;
		else {
			reply_id[0] = 0;
			if (p->expand_fields && (p->msg).hdr.thread_back) {
				rc = JS_SUSPENDREQUEST(cx);
				memset(&remsg, 0, sizeof(remsg));
				remsg.hdr.number = (p->msg).hdr.thread_back;
				if (smb_getmsgidx(&(p->p->smb), &remsg))
					SAFEPRINTF(reply_id, "<%s>", p->p->smb.last_error);
				else
					get_msgid(scfg, p->p->smb.subnum, &remsg, reply_id, sizeof(reply_id));
				JS_RESUMEREQUEST(cx, rc);
			}
			val = reply_id;
		}
		if (val[0] && (js_str = JS_NewStringCopyZ(cx, truncsp(val))) != NULL) {
			JS_DefineProperty(cx, obj, "reply_id"
			                  , STRING_TO_JSVAL(js_str)
			                  , NULL, NULL, JSPROP_ENUMERATE);
			if (name)
				return JS_TRUE;
		}
		else if (name)
			return JS_TRUE;
	}

	/* Message-ID */
	if (name == NULL || strcmp(name, "id") == 0) {
		if (name)
			free(name);
		if (p->expand_fields || (p->msg).id != NULL) {
			get_msgid(scfg, p->p->smb.subnum, &(p->msg), msg_id, sizeof(msg_id));
			val = msg_id;
			if ((js_str = JS_NewStringCopyZ(cx, truncsp(val))) != NULL) {
				JS_DefineProperty(cx, obj, "id"
				                  , STRING_TO_JSVAL(js_str)
				                  , NULL, NULL, JSPROP_ENUMERATE);
				if (name)
					return JS_TRUE;
			}
			else if (name)
				return JS_TRUE;
		}
		else if (name)
			return JS_TRUE;
	}

	/* USENET Fields */
	LAZY_STRING_TRUNCSP_NULL("path", p->msg.path, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("newsgroups", p->msg.newsgroups, JSPROP_ENUMERATE);

	/* FidoNet Header Fields */
	LAZY_STRING_TRUNCSP_NULL("ftn_msgid", p->msg.ftn_msgid, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_reply", p->msg.ftn_reply, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_pid", p->msg.ftn_pid, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_tid", p->msg.ftn_tid, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_area", p->msg.ftn_area, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_flags", p->msg.ftn_flags, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_charset", p->msg.ftn_charset, JSPROP_ENUMERATE);
	LAZY_STRING_TRUNCSP_NULL("ftn_bbsid", p->msg.ftn_bbsid, JSPROP_ENUMERATE);

	if (name == NULL || strcmp(name, "field_list") == 0) {
		if (name)
			free(name);
		/* Create hdr.field_list[] with repeating header fields (including type and data) */
		if ((array = JS_NewArrayObject(cx, 0, NULL)) != NULL) {
			JS_DefineProperty(cx, obj, "field_list", OBJECT_TO_JSVAL(array)
			                  , NULL, NULL, JSPROP_ENUMERATE);
			items = 0;
			for (i = 0; i < (p->msg).total_hfields; i++) {
				switch ((p->msg).hfield[i].type) {
					case SMB_COMMENT:
					case SMB_POLL_ANSWER:
					case SMB_GROUP:
					case FIDOCTRL:
					case FIDOSEENBY:
					case FIDOPATH:
					case RFC822HEADER:
					case RFC822TO:
					case RFC822CC:
					case RFC822ORG:
					case RFC822FROM:
					case RFC822REPLYTO:
					case RFC822SUBJECT:
					case SMTPRECEIVED:
					case UNKNOWNASCII:
						/* only support these header field types */
						break;
					default:
						/* dupe or possibly binary header field */
						continue;
				}
				if ((field = JS_NewObject(cx, NULL, NULL, array)) == NULL)
					continue;
				JS_DefineProperty(cx, field, "type"
				                  , INT_TO_JSVAL((p->msg).hfield[i].type)
				                  , NULL, NULL, JSPROP_ENUMERATE);
				if ((js_str = JS_NewStringCopyN(cx, static_cast<const char*>((p->msg).hfield_dat[i]), (p->msg).hfield[i].length)) == NULL)
					break;
				JS_DefineProperty(cx, field, "data"
				                  , STRING_TO_JSVAL(js_str)
				                  , NULL, NULL, JSPROP_ENUMERATE);
				JS_DefineElement(cx, array, items, OBJECT_TO_JSVAL(field)
				                 , NULL, NULL, JSPROP_ENUMERATE);
				items++;
			}
			if (name)
				return JS_TRUE;
		}
		else if (name)
			return JS_TRUE;
	}

	if (name == NULL || strcmp(name, "can_read") == 0) {
		if (name)
			free(name);
		v = BOOLEAN_TO_JSVAL(JS_FALSE);

		do {
			client_t *client = NULL;
			user_t *  user = NULL;
			jsval     cov;
			ushort    aliascrc = 0, namecrc = 0, sysop = crc16("sysop", 0);

			/* dig a client object out of the global object */
			if (JS_GetProperty(cx, JS_GetGlobalObject(cx), "client", &cov)
			    && JSVAL_IS_OBJECT(cov)) {
				JSObject *obj = JSVAL_TO_OBJECT(cov);
				JSClass * cl;

				if (obj != NULL && (cl = JS_GetClass(cx, obj)) != NULL && strcmp(cl->name, "Client") == 0)
					client = static_cast<client_t *>(JS_GetPrivate(cx, obj));
			}

			/* dig a user object out of the global object */
			if (JS_GetProperty(cx, JS_GetGlobalObject(cx), "user", &cov)
			    && JSVAL_IS_OBJECT(cov)) {
				JSObject *obj = JSVAL_TO_OBJECT(cov);
				JSClass * cl;

				if (obj != NULL && (cl = JS_GetClass(cx, obj)) != NULL && strcmp(cl->name, "User") == 0) {
					user = *(user_t **)(JS_GetPrivate(cx, obj));
					namecrc = crc16(user->name, 0);
					aliascrc = crc16(user->alias, 0);
				}
			}

			if (p->msg.idx.attr & MSG_DELETE) {        /* Pre-flagged */
				if (!(scfg->sys_misc & SM_SYSVDELM)) /* Noone can view deleted msgs */
					break;
				if (!(scfg->sys_misc & SM_USRVDELM)    /* Users can't view deleted msgs */
				    && !user_is_subop(scfg, p->p->smb.subnum, user, client))    /* not sub-op */
					break;
				if (user == NULL)
					break;
				if (!user_is_subop(scfg, p->p->smb.subnum, user, client)         /* not sub-op */
				    && p->msg.idx.from != namecrc && p->msg.idx.from != aliascrc)   /* not for you */
					break;
			}

			if ((p->msg.idx.attr & (MSG_MODERATED | MSG_VALIDATED | MSG_DELETE)) == MSG_MODERATED
			    && !user_is_subop(scfg, p->p->smb.subnum, user, client))
				break;

			if (((p->p->smb.status.attr & SMB_EMAIL) == 0) && (p->msg.idx.attr & MSG_PRIVATE)) {
				if (user == NULL)
					break;
				if (!user_is_subop(scfg, p->p->smb.subnum, user, client) && !(user->rest & UREST_QWK_NODE)) {
					if (p->msg.idx.to != namecrc && p->msg.idx.from != namecrc
					    && p->msg.idx.to != aliascrc && p->msg.idx.from != aliascrc
					    && (user->number != 1 || p->msg.idx.to != sysop))
						break;
					if (stricmp(p->msg.to, user->alias)
					    && stricmp(p->msg.from, user->alias)
					    && stricmp(p->msg.to, user->name)
					    && stricmp(p->msg.from, user->name)
					    && (user->number != 1 || stricmp(p->msg.to, "sysop")
					        || p->msg.from_net.type)) {
						break;
					}
				}
			}

			v = BOOLEAN_TO_JSVAL(JS_TRUE);
		} while (0);

		JS_DefineProperty(cx, obj, "can_read", v, NULL, NULL, JSPROP_ENUMERATE);

		if (name)
			return JS_TRUE;
	}
	if (name)
		free(name);

	/* DO NOT RETURN JS_FALSE on unknown names */
	/* Doing so will prevent toString() among others from working. */

	return JS_TRUE;
}

static JSBool js_get_msg_header_enumerate(JSContext *cx, JSObject *obj)
{
	privatemsg_t* p;

	js_get_msg_header_resolve(cx, obj, JSID_VOID);

	if ((p = (privatemsg_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_TRUE;

	p->enumerated = true;

	return JS_TRUE;
}

static void js_get_msg_header_finalize(JSContext *cx, JSObject *obj)
{
	privatemsg_t* p;

	if ((p = (privatemsg_t*)JS_GetPrivate(cx, obj)) == NULL)
		return;

	smb_freemsgmem(&(p->msg));
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

static JSClass js_msghdr_class = {
	"MsgHeader"             /* name			*/
	, JSCLASS_HAS_PRIVATE    /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, JS_PropertyStub        /* getProperty	*/
	, JS_StrictPropertyStub      /* setProperty	*/
	, js_get_msg_header_enumerate        /* enumerate	*/
	, js_get_msg_header_resolve          /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, js_get_msg_header_finalize     /* finalize		*/
};

static JSBool
js_get_msg_header(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*     obj = JS_THIS_OBJECT(cx, arglist);
	jsval*        argv = JS_ARGV(cx, arglist);
	uintN         n;
	JSObject*     hdrobj;
	JSBool        by_offset = JS_FALSE;
	JSBool        include_votes = JS_FALSE;
	jsrefcount    rc;
	char*         cstr = NULL;
	privatemsg_t* p;
	JSObject*     proto;
	jsval         val;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (privatemsg_t*)malloc(sizeof(privatemsg_t))) == NULL) {
		JS_ReportError(cx, "malloc failed");
		return JS_FALSE;
	}

	memset(p, 0, sizeof(privatemsg_t));

	if ((p->p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		free(p);
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->p->smb))) {
		free(p);
		return JS_TRUE;
	}

	p->expand_fields = JS_TRUE;   /* This parameter defaults to true */
	n = 0;
	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		by_offset = JSVAL_TO_BOOLEAN(argv[n++]);

	/* Now parse message offset/id and get message */
	if (n < argc && JSVAL_IS_NUMBER(argv[n])) {
		if (by_offset) {                         /* Get by offset */
			if (!JS_ValueToInt32(cx, argv[n++], (int32*)&(p->msg).idx_offset)) {
				free(p);
				return JS_FALSE;
			}
		}
		else {                                  /* Get by number */
			if (!JS_ValueToECMAUint32(cx, argv[n++], &(p->msg).hdr.number)) {
				free(p);
				return JS_FALSE;
			}
		}

		rc = JS_SUSPENDREQUEST(cx);
		if ((p->p->smb_result = smb_getmsgidx(&(p->p->smb), &(p->msg))) != SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			free(p);
			return JS_TRUE;
		}

		if ((p->p->smb_result = smb_lockmsghdr(&(p->p->smb), &(p->msg))) != SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			free(p);
			return JS_TRUE;
		}

		if ((p->p->smb_result = smb_getmsghdr_for_idx(&(p->p->smb), &(p->msg))) != SMB_SUCCESS) {
			smb_unlockmsghdr(&(p->p->smb), &(p->msg));
			JS_RESUMEREQUEST(cx, rc);
			free(p);
			return JS_TRUE;
		}

		smb_unlockmsghdr(&(p->p->smb), &(p->msg));
		JS_RESUMEREQUEST(cx, rc);
	} else if (n < argc && JSVAL_IS_STRING(argv[n])) {       /* Get by ID */
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
		n++;
		if (JS_IsExceptionPending(cx)) {
			free(cstr);
			free(p);
			return JS_FALSE;
		}
		if (cstr != NULL) {
			rc = JS_SUSPENDREQUEST(cx);
			if ((p->p->smb_result = smb_getmsghdr_by_msgid(&(p->p->smb), &(p->msg)
			                                               , cstr)) != SMB_SUCCESS) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				free(p);
				return JS_TRUE; /* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
		}
	}

	if (p->msg.hdr.number == 0) { /* No valid message number/id/offset specified */
		free(p);
		return JS_TRUE;
	}

	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		p->expand_fields = JSVAL_TO_BOOLEAN(argv[n++]);

	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		include_votes = JSVAL_TO_BOOLEAN(argv[n++]);

	if (!include_votes && (p->msg.hdr.attr & MSG_VOTE)) {
		smb_freemsgmem(&(p->msg));
		free(p);
		return JS_TRUE;
	}

	if (JS_GetProperty(cx, JS_GetGlobalObject(cx), "MsgBase", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx, val, &proto);
		if (JS_GetProperty(cx, proto, "HeaderPrototype", &val) && !JSVAL_NULL_OR_VOID(val))
			JS_ValueToObject(cx, val, &proto);
		else
			proto = NULL;
	}
	else
		proto = NULL;

	if ((hdrobj = JS_NewObject(cx, &js_msghdr_class, proto, obj)) == NULL) {
		smb_freemsgmem(&(p->msg));
		free(p);
		return JS_TRUE;
	}

	if (!JS_SetPrivate(cx, hdrobj, p)) {
		JS_ReportError(cx, "JS_SetPrivate failed");
		free(p);
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(hdrobj));

	return JS_TRUE;
}

static JSBool
js_get_all_msg_headers(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*     obj = JS_THIS_OBJECT(cx, arglist);
	jsval*        argv = JS_ARGV(cx, arglist);
	JSObject*     hdrobj;
	jsrefcount    rc;
	privatemsg_t* p;
	private_t*    priv;
	JSObject*     proto;
	jsval         val;
	uint32_t      off;
	JSObject*     retobj;
	char          numstr[16];
	JSBool        include_votes = JS_FALSE;
	JSBool        expand_fields = JS_TRUE;
	post_t*       post;
	idxrec_t*     idx;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((priv = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(priv->smb)))
		return JS_TRUE;

	off_t index_length = filelength(fileno(priv->smb.sid_fp));
	if (index_length < static_cast<off_t>(sizeof(*idx)))
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	if (smb_getstatus(&(priv->smb)) != SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	if ((retobj = JS_NewObject(cx, NULL, NULL, obj)) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "JS_NewObject failure");
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(retobj));

	uint32_t total_msgs = (uint32_t)(index_length / sizeof(*idx));
	if (total_msgs > priv->smb.status.total_msgs)
		total_msgs = priv->smb.status.total_msgs;
	if (total_msgs < 1) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	if ((post = static_cast<post_t *>(calloc(total_msgs, sizeof(*post)))) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "malloc failure of %u bytes on line %d in %s of %s", total_msgs * sizeof(*post), WHERE);
		return JS_FALSE;
	}
	if ((idx = static_cast<idxrec_t *>(calloc(total_msgs, sizeof(*idx)))) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, "malloc failure of %u bytes on line %d in %s of %s", total_msgs * sizeof(*idx), WHERE);
		free(post);
		return JS_FALSE;
	}

	uintN argn = 0;
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		include_votes = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		expand_fields = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}

	if ((priv->smb_result = smb_locksmbhdr(&(priv->smb))) != SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		free(post);
		free(idx);
		return JS_TRUE;
	}

	if (JS_GetProperty(cx, JS_GetGlobalObject(cx), "MsgBase", &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx, val, &proto);
		if (JS_GetProperty(cx, proto, "HeaderPrototype", &val) && !JSVAL_NULL_OR_VOID(val))
			JS_ValueToObject(cx, val, &proto);
		else
			proto = NULL;
	}
	else
		proto = NULL;

	rewind(priv->smb.sid_fp);
	size_t fread_result = fread(idx, sizeof(*idx), total_msgs, priv->smb.sid_fp);
	JS_RESUMEREQUEST(cx, rc);

	if (fread_result != total_msgs) {
		smb_unlocksmbhdr(&(priv->smb));
		JS_ReportError(cx, "index read failed (%lu instead of %lu)", fread_result, total_msgs);
		free(post);
		free(idx);
		return JS_FALSE;
	}
	for (off = 0; off < total_msgs; off++) {
		post[off].idx = idx[off];
		if (idx[off].attr & MSG_VOTE && !(idx[off].attr & MSG_POLL)) {
			ulong u;
			for (u = 0; u < off; u++)
				if (post[u].idx.number == idx[off].remsg)
					break;
			if (u < off) {
				post[u].total_votes++;
				switch (idx[off].attr & MSG_VOTE) {
					case MSG_UPVOTE:
						post[u].upvotes++;
						break;
					case MSG_DOWNVOTE:
						post[u].downvotes++;
						break;
					default:
						for (int b = 0; b < MSG_POLL_MAX_ANSWERS; b++) {
							if (idx[off].votes & (1 << b))
								post[u].votes[b]++;
						}
				}
			}
		}
	}
	free(idx);

	for (off = 0; off < total_msgs; off++) {
		if ((!include_votes) && (post[off].idx.attr & MSG_VOTE))
			continue;

		if ((p = (privatemsg_t*)malloc(sizeof(privatemsg_t))) == NULL) {
			smb_unlocksmbhdr(&(priv->smb));
			JS_ReportError(cx, "malloc failed");
			free(post);
			return JS_FALSE;
		}

		memset(p, 0, sizeof(privatemsg_t));

		/* Parse boolean arguments first */
		p->p = priv;
		p->expand_fields = expand_fields;

		p->msg.idx = post[off].idx;
		p->post = post[off];

		rc = JS_SUSPENDREQUEST(cx);
		priv->smb_result = smb_getmsghdr_for_idx(&(priv->smb), &(p->msg));
		JS_RESUMEREQUEST(cx, rc);
		if (priv->smb_result != SMB_SUCCESS) {
			smb_unlocksmbhdr(&(priv->smb));
			free(post);
			free(p);
			return JS_TRUE;
		}

		if ((hdrobj = JS_NewObject(cx, &js_msghdr_class, proto, obj)) == NULL) {
			smb_freemsgmem(&(p->msg));
			smb_unlocksmbhdr(&(priv->smb));
			free(post);
			free(p);
			return JS_TRUE;
		}

		p->msg.upvotes = post[off].upvotes;
		p->msg.downvotes = post[off].downvotes;
		p->msg.total_votes = post[off].total_votes;
		if (post[off].total_votes) {
			JSObject* array;
			if ((array = JS_NewArrayObject(cx, 0, NULL)) != NULL) {
				JS_DefineProperty(cx, hdrobj, "tally", OBJECT_TO_JSVAL(array)
				                  , NULL, NULL, JSPROP_ENUMERATE);
				for (int i = 0; i < MSG_POLL_MAX_ANSWERS; i++)
					JS_DefineElement(cx, array, i, UINT_TO_JSVAL(post[off].votes[i])
					                 , NULL, NULL, JSPROP_ENUMERATE);
			}
		}

		if (!JS_SetPrivate(cx, hdrobj, p)) {
			JS_ReportError(cx, "JS_SetPrivate failed");
			smb_unlocksmbhdr(&(priv->smb));
			free(post);
			free(p);
			return JS_FALSE;
		}

		val = OBJECT_TO_JSVAL(hdrobj);
		sprintf(numstr, "%" PRIu32, p->msg.hdr.number);
		JS_SetProperty(cx, retobj, numstr, &val);
	}
	smb_unlocksmbhdr(&(priv->smb));
	free(post);

	return JS_TRUE;
}

static JSBool
js_dump_msg_header(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if (argc >= 1 && JSVAL_IS_OBJECT(argv[0])) {
		JSObject*     hdr = JSVAL_TO_OBJECT(argv[0]);
		if (hdr == NULL)     /* no header supplied? */
			return JS_TRUE;
		privatemsg_t* mp = (privatemsg_t*)JS_GetPrivate(cx, hdr);
		if (mp == NULL)
			return JS_TRUE;
		str_list_t    list = smb_msghdr_str_list(&mp->msg);
		if (list != NULL) {
			JSObject* array;
			if ((array = JS_NewArrayObject(cx, 0, NULL)) == NULL) {
				JS_ReportError(cx, "JS_NewArrayObject failure");
				strListFree(&list);
				return JS_FALSE;
			}
			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));
			for (int i = 0; list[i] != NULL; i++) {
				JSString* js_str = JS_NewStringCopyZ(cx, list[i]);
				if (js_str == NULL)
					break;
				JS_DefineElement(cx, array, i, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE);
			}
			strListFree(&list);
		}
	}
	return JS_TRUE;
}

static JSBool
js_put_msg_header(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      n;
	JSBool     by_offset = JS_FALSE;
	smbmsg_t   msg;
	JSObject*  hdr = NULL;
	private_t* p;
	jsrefcount rc;
	char*      cstr;
	JSBool     ret = JS_TRUE;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg, 0, sizeof(msg));

	for (n = 0; n < argc; n++) {
		if (JSVAL_IS_BOOLEAN(argv[n]))
			by_offset = JSVAL_TO_BOOLEAN(argv[n]);
		else if (JSVAL_IS_NUMBER(argv[n])) {
			if (by_offset) {                         /* Get by offset */
				if (!JS_ValueToInt32(cx, argv[n], (int32*)&msg.idx_offset))
					return JS_FALSE;
			}
			else {                                  /* Get by number */
				if (!JS_ValueToECMAUint32(cx, argv[n], &msg.hdr.number))
					return JS_FALSE;
			}
		} else if (JSVAL_IS_STRING(argv[n])) {       /* Get by ID */
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
			HANDLE_PENDING(cx, cstr);
			rc = JS_SUSPENDREQUEST(cx);
			if (!msg_offset_by_id(p
			                      , cstr
			                      , &msg.idx_offset)) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE; /* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
		}
		else if (JSVAL_IS_OBJECT(argv[n])) {
			hdr = JSVAL_TO_OBJECT(argv[n]);
		}
	}

	if (hdr == NULL)     /* no header supplied? */
		return JS_TRUE;

	privatemsg_t* mp;
	mp = (privatemsg_t*)JS_GetPrivate(cx, hdr);
	if (mp != NULL) {
		if (mp->expand_fields) {
			JS_ReportError(cx, "Message header has 'expanded fields'");
			return JS_FALSE;
		}
		msg.idx_offset = mp->msg.idx_offset;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if ((p->smb_result = smb_getmsgidx(&(p->smb), &msg)) != SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	if ((p->smb_result = smb_lockmsghdr(&(p->smb), &msg)) != SMB_SUCCESS) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	do {
		if ((p->smb_result = smb_getmsghdr_for_idx(&(p->smb), &msg)) != SMB_SUCCESS)
			break;

		smb_freemsghdrmem(&msg);    /* prevent duplicate header fields */

		JS_RESUMEREQUEST(cx, rc);
		if (!parse_header_object(cx, p, hdr, &msg, true)) {
			SAFECOPY(p->smb.last_error, "Header parsing failure (required field missing?)");
			ret = JS_FALSE;
			break;
		}
		rc = JS_SUSPENDREQUEST(cx);

		if ((p->smb_result = smb_putmsg(&(p->smb), &msg)) != SMB_SUCCESS)
			break;

		if (mp != NULL) {
			smb_freemsgmem(&mp->msg);
			smb_copymsgmem(&(p->smb), &mp->msg, &msg);
		}

		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} while (0);

	smb_unlockmsghdr(&(p->smb), &msg);
	smb_freemsgmem(&msg);
	JS_RESUMEREQUEST(cx, rc);

	return ret;
}

static JSBool
js_remove_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      n;
	JSBool     by_offset = JS_FALSE;
	JSBool     msg_specified = JS_FALSE;
	smbmsg_t   msg;
	private_t* p;
	char*      cstr = NULL;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg, 0, sizeof(msg));

	for (n = 0; n < argc; n++) {
		if (JSVAL_IS_BOOLEAN(argv[n]))
			by_offset = JSVAL_TO_BOOLEAN(argv[n]);
		else if (JSVAL_IS_NUMBER(argv[n])) {
			if (by_offset) {                         /* Get by offset */
				if (!JS_ValueToInt32(cx, argv[n], (int32*)&msg.idx_offset))
					return JS_FALSE;
			}
			else {                                  /* Get by number */
				if (!JS_ValueToECMAUint32(cx, argv[n], &msg.hdr.number))
					return JS_FALSE;
			}
			msg_specified = JS_TRUE;
			n++;
			break;
		} else if (JSVAL_IS_STRING(argv[n])) {       /* Get by ID */
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
			HANDLE_PENDING(cx, cstr);
			if (cstr == NULL)
				return JS_FALSE;
			rc = JS_SUSPENDREQUEST(cx);
			if (!msg_offset_by_id(p
			                      , cstr
			                      , &msg.idx_offset)) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE; /* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
			msg_specified = JS_TRUE;
			n++;
			break;
		}
	}

	if (!msg_specified)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	if ((p->smb_result = smb_getmsgidx(&(p->smb), &msg)) == SMB_SUCCESS
	    && (p->smb_result = smb_getmsghdr_for_idx(&(p->smb), &msg)) == SMB_SUCCESS) {

		msg.hdr.attr |= MSG_DELETE;

		if ((p->smb_result = smb_updatemsg(&(p->smb), &msg)) == SMB_SUCCESS)
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	}

	smb_freemsgmem(&msg);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static char* get_msg_text(private_t* p, smbmsg_t* msg, bool strip_ctrl_a, bool dot_stuffing, ulong mode, JSBool existing)
{
	char* buf;

	if (existing) {
		if ((p->smb_result = smb_lockmsghdr(&(p->smb), msg)) != SMB_SUCCESS)
			return NULL;
	}
	else {
		if ((p->smb_result = smb_getmsgidx(&(p->smb), msg)) != SMB_SUCCESS)
			return NULL;

		if ((p->smb_result = smb_lockmsghdr(&(p->smb), msg)) != SMB_SUCCESS)
			return NULL;

		if ((p->smb_result = smb_getmsghdr_for_idx(&(p->smb), msg)) != SMB_SUCCESS) {
			smb_unlockmsghdr(&(p->smb), msg);
			return NULL;
		}
	}

	if ((buf = smb_getmsgtxt(&(p->smb), msg, mode)) == NULL) {
		smb_unlockmsghdr(&(p->smb), msg);
		if (!existing)
			smb_freemsgmem(msg);
		return NULL;
	}

	smb_unlockmsghdr(&(p->smb), msg);
	if (!existing)
		smb_freemsgmem(msg);

	if (strip_ctrl_a)
		remove_ctrl_a(buf, buf);

	if (dot_stuffing) {  /* must escape lines starting with dot ('.'), e.g. RFC821 */
		char* newbuf;
		if ((newbuf = static_cast<char *>(malloc((strlen(buf) * 2) + 1))) != NULL) {
			int i, j;
			for (i = j = 0; buf[i]; i++) {
				if ((i == 0 || buf[i - 1] == '\n') && buf[i] == '.')
					newbuf[j++] = '.';
				newbuf[j++] = buf[i];
			}
			newbuf[j] = 0;
			free(buf);
			buf = newbuf;
		}
	}

	return buf;
}

static JSBool
js_get_msg_body(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf;
	uintN      n;
	smbmsg_t   msg;
	smbmsg_t * msgptr;
	long       getmsgtxtmode = 0;
	JSBool     by_offset = JS_FALSE;
	JSBool     strip_ctrl_a = JS_FALSE;
	JSBool     tails = JS_TRUE;
	JSBool     plain = JS_FALSE;
	JSBool     dot_stuffing = JS_FALSE;
	JSBool     msg_specified = JS_FALSE;
	JSBool     existing_msg = JS_FALSE;
	JSString*  js_str;
	private_t* p;
	char*      cstr;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg, 0, sizeof(msg));
	msgptr = &msg;

	for (n = 0; n < argc; n++) {
		if (JSVAL_IS_BOOLEAN(argv[n]))
			by_offset = JSVAL_TO_BOOLEAN(argv[n]);
		else if (JSVAL_IS_NUMBER(argv[n])) {
			if (by_offset) {                         /* Get by offset */
				if (!JS_ValueToInt32(cx, argv[n], (int32*)&msg.idx_offset))
					return JS_FALSE;
			}
			else {                                  /* Get by number */
				if (!JS_ValueToECMAUint32(cx, argv[n], &msg.hdr.number))
					return JS_FALSE;
			}
			msg_specified = JS_TRUE;
			n++;
			break;
		} else if (JSVAL_IS_STRING(argv[n])) {       /* Get by ID */
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
			HANDLE_PENDING(cx, cstr);
			rc = JS_SUSPENDREQUEST(cx);
			if (!msg_offset_by_id(p
			                      , cstr
			                      , &msg.idx_offset)) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE; /* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
			msg_specified = JS_TRUE;
			n++;
			break;
		} else if (JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {        /* Use existing header */
			JSClass *oc = JS_GetClass(cx, JSVAL_TO_OBJECT(argv[n]));
			if (oc != NULL && strcmp(oc->name, js_msghdr_class.name) == 0) {
				privatemsg_t *pmsg = static_cast<privatemsg_t *>(JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[n])));

				if (pmsg != NULL) {
					msg_specified = JS_TRUE;
					existing_msg = JS_TRUE;
					msgptr = &pmsg->msg;
				}
			}
			n++;
			break;
		}

	}

	if (!msg_specified)  /* No message number or id specified */
		return JS_TRUE;

	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		strip_ctrl_a = JSVAL_TO_BOOLEAN(argv[n++]);

	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		dot_stuffing = JSVAL_TO_BOOLEAN(argv[n++]);

	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		tails = JSVAL_TO_BOOLEAN(argv[n++]);

	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		plain = JSVAL_TO_BOOLEAN(argv[n++]);

	if (tails)
		getmsgtxtmode |= GETMSGTXT_TAILS;

	if (plain)
		getmsgtxtmode |= GETMSGTXT_PLAIN;

	rc = JS_SUSPENDREQUEST(cx);
	buf = get_msg_text(p, msgptr, strip_ctrl_a, dot_stuffing, getmsgtxtmode, existing_msg);
	JS_RESUMEREQUEST(cx, rc);
	if (buf == NULL)
		return JS_TRUE;

	if ((js_str = JS_NewStringCopyZ(cx, buf)) != NULL)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));

	smb_freemsgtxt(buf);

	return JS_TRUE;
}

static JSBool
js_get_msg_tail(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf;
	uintN      n;
	smbmsg_t   msg;
	smbmsg_t * msgptr;
	JSBool     by_offset = JS_FALSE;
	JSBool     strip_ctrl_a = JS_FALSE;
	JSBool     dot_stuffing = JS_FALSE;
	JSBool     msg_specified = JS_FALSE;
	JSBool     existing_msg = JS_FALSE;
	JSString*  js_str;
	private_t* p;
	char*      cstr;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	memset(&msg, 0, sizeof(msg));
	msgptr = &msg;

	for (n = 0; n < argc; n++) {
		if (JSVAL_IS_BOOLEAN(argv[n]))
			by_offset = JSVAL_TO_BOOLEAN(argv[n]);
		else if (JSVAL_IS_NUMBER(argv[n])) {
			if (by_offset) {                         /* Get by offset */
				if (!JS_ValueToInt32(cx, argv[n], (int32*)&msg.idx_offset))
					return JS_FALSE;
			}
			else {                                  /* Get by number */
				if (!JS_ValueToECMAUint32(cx, argv[n], &msg.hdr.number))
					return JS_FALSE;
			}
			msg_specified = JS_TRUE;
			n++;
			break;
		} else if (JSVAL_IS_STRING(argv[n])) {       /* Get by ID */
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[n]), cstr, NULL);
			HANDLE_PENDING(cx, cstr);
			rc = JS_SUSPENDREQUEST(cx);
			if (!msg_offset_by_id(p
			                      , cstr
			                      , &msg.idx_offset)) {
				free(cstr);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE; /* ID not found */
			}
			free(cstr);
			JS_RESUMEREQUEST(cx, rc);
			msg_specified = JS_TRUE;
			n++;
			break;
		} else if (JSVAL_IS_OBJECT(argv[n])) {       /* Use existing header */
			JSClass *oc = JS_GetClass(cx, JSVAL_TO_OBJECT(argv[n]));
			if (oc != NULL && strcmp(oc->name, js_msghdr_class.name) == 0) {
				privatemsg_t *pmsg = static_cast<privatemsg_t *>(JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[n])));

				if (pmsg != NULL) {
					msg_specified = JS_TRUE;
					existing_msg = JS_TRUE;
					msgptr = &pmsg->msg;
				}
			}
			n++;
			break;
		}
	}

	if (!msg_specified)  /* No message number or id specified */
		return JS_TRUE;

	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		strip_ctrl_a = JSVAL_TO_BOOLEAN(argv[n++]);

	if (n < argc && JSVAL_IS_BOOLEAN(argv[n]))
		dot_stuffing = JSVAL_TO_BOOLEAN(argv[n++]);

	rc = JS_SUSPENDREQUEST(cx);
	buf = get_msg_text(p, msgptr, strip_ctrl_a, dot_stuffing, GETMSGTXT_TAILS | GETMSGTXT_NO_BODY, existing_msg);
	JS_RESUMEREQUEST(cx, rc);
	if (buf == NULL)
		return JS_TRUE;

	if ((js_str = JS_NewStringCopyZ(cx, buf)) != NULL)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));

	smb_freemsgtxt(buf);

	return JS_TRUE;
}

/* Save the message described by 'hdr' with the given body (not freed here), to
 * the recipients in 'rcpt_list' if non-NULL: the part of save_msg() and
 * save_msg_from_file() that does not depend on where the body came from */
static JSBool
save_msg_common(JSContext *cx, jsval *arglist, private_t* p, JSObject* hdr
                , JSObject* rcpt_list, client_t* client, char* body)
{
	jsuint     i;
	jsuint     rcpt_list_length = 0;
	jsval      val;
	JSObject*  objarg;
	JSClass*   cl;
	smbmsg_t   rcpt_msg;
	smbmsg_t   msg;
	JSBool     ret = JS_TRUE;
	jsrefcount rc;
	scfg_t*    scfg;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	memset(&msg, 0, sizeof(msg));

	// Find and use the global client object if possible...
	if (client == NULL) {
		if (JS_GetProperty(cx, JS_GetGlobalObject(cx), "client", &val) && !JSVAL_NULL_OR_VOID(val)) {
			objarg = JSVAL_TO_OBJECT(val);
			if (objarg != NULL && (cl = JS_GetClass(cx, objarg)) != NULL && strcmp(cl->name, "Client") == 0)
				client = static_cast<client_t *>(JS_GetPrivate(cx, objarg));
		}
	}

	if (rcpt_list != NULL) {
		if (!JS_GetArrayLength(cx, rcpt_list, &rcpt_list_length)) {
			return JS_TRUE;
		}
		if (rcpt_list_length < 1) {
			JS_ReportError(cx, "Empty recipient list");
			return JS_FALSE;
		}
	}

	if (parse_header_object(cx, p, hdr, &msg, rcpt_list == NULL)) {

		if (body[0])
			truncsp(body);
		rc = JS_SUSPENDREQUEST(cx);
		if ((p->smb_result = savemsg(scfg, &(p->smb), &msg, client, /* ToDo server hostname: */ NULL, body, /* remsg: */ NULL)) == SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

			if (rcpt_list != NULL) {   /* Sending to a list of recipients */

				JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
				SAFECOPY(p->smb.last_error, "Recipient list parsing failure");

				memset(&rcpt_msg, 0, sizeof(rcpt_msg));

				for (i = 0; i < rcpt_list_length; i++) {

					if (!JS_GetElement(cx, rcpt_list, i, &val))
						break;

					if (!JSVAL_IS_OBJECT(val) || JSVAL_IS_NULL(val))
						break;

					rc = JS_SUSPENDREQUEST(cx);
					if ((p->smb_result = smb_copymsgmem(&(p->smb), &rcpt_msg, &msg)) != SMB_SUCCESS) {
						JS_RESUMEREQUEST(cx, rc);
						break;
					}
					JS_RESUMEREQUEST(cx, rc);

					if (!parse_recipient_object(cx, p, JSVAL_TO_OBJECT(val), &rcpt_msg))
						break;

					rc = JS_SUSPENDREQUEST(cx);
					if ((p->smb_result = smb_addmsghdr(&(p->smb), &rcpt_msg, smb_storage_mode(scfg, &(p->smb)))) != SMB_SUCCESS) {
						JS_RESUMEREQUEST(cx, rc);
						break;
					}

					smb_freemsgmem(&rcpt_msg);
					JS_RESUMEREQUEST(cx, rc);
				}
				smb_freemsgmem(&rcpt_msg);  /* just in case we broke the loop */

				if (i == rcpt_list_length) {
					p->smb.last_error[0] = 0;
					JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
				}
				if (i > 1)
					smb_incmsg_dfields(&(p->smb), &msg, (uint16_t)(i - 1));
			}
		}
		else
			JS_RESUMEREQUEST(cx, rc);
	} else {
		ret = JS_FALSE;
		SAFECOPY(p->smb.last_error, "Header parsing failure (required field missing?)");
	}
	smb_freemsgmem(&msg);

	return ret;
}

static JSBool
js_save_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      body = NULL;
	uintN      n;
	JSObject*  hdr = NULL;
	JSObject*  objarg;
	JSObject*  rcpt_list = NULL;
	JSClass*   cl;
	client_t*  client = NULL;
	private_t* p;
	JSBool     ret = JS_TRUE;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb))) {
		if (!js_open(cx, 0, arglist))
			return JS_FALSE;
		if (JS_RVAL(cx, arglist) == JSVAL_FALSE)
			return JS_TRUE;
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	}

	for (n = 0; n < argc; n++) {
		if (JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			objarg = JSVAL_TO_OBJECT(argv[n]);
			if (objarg == NULL)
				continue;
			if (objarg != NULL && (cl = JS_GetClass(cx, objarg)) != NULL && strcmp(cl->name, "Client") == 0) {
				client = static_cast<client_t *>(JS_GetPrivate(cx, objarg));
				continue;
			}
			if (JS_IsArrayObject(cx, objarg)) {      /* recipient_list is an array of objects */
				if (body != NULL && rcpt_list == NULL) { /* body text already specified */
					rcpt_list = objarg;
					continue;
				}
			}
			else if (hdr == NULL) {
				hdr = objarg;
				continue;
			}
		}
		if (body == NULL) {
			JSVALUE_TO_MSTRING(cx, argv[n], body, NULL);
			HANDLE_PENDING(cx, body);
			if (body == NULL) {
				JS_ReportError(cx, "Invalid message body string");
				return JS_FALSE;
			}
		}
	}

	if (hdr == NULL) {
		FREE_AND_NULL(body);
		return JS_TRUE;
	}
	if (body == NULL)
		body = strdup("");

	ret = save_msg_common(cx, arglist, p, hdr, rcpt_list, client, body);
	free(body);

	return ret;
}

/* Read the whole file 'fname' into a new NUL-terminated native buffer (never a
 * JS string). Returns NULL, with the reason in p->smb.last_error, on failure. */
static char* read_body_file(private_t* p, const char* fname)
{
	FILE*  fp;
	off_t  length;
	char*  buf;
	size_t len;

	if ((fp = fopen(fname, "rb")) == NULL) {
		p->smb_result = SMB_ERR_OPEN;
		SAFEPRINTF2(p->smb.last_error, "error %d opening body file: %s", errno, fname);
		return NULL;
	}
	length = filelength(fileno(fp));
	if (length < 0 || (uint64_t)length >= (uint64_t)SIZE_MAX / 2) {
		fclose(fp);
		p->smb_result = SMB_ERR_READ;
		SAFEPRINTF(p->smb.last_error, "invalid size of body file: %s", fname);
		return NULL;
	}
	if ((buf = (char*)malloc((size_t)length + 1)) == NULL) {
		fclose(fp);
		p->smb_result = SMB_ERR_MEM;
		SAFEPRINTF2(p->smb.last_error, "malloc failure of %" PRIdOFF " bytes for body file: %s", length, fname);
		return NULL;
	}
	len = fread(buf, 1, (size_t)length, fp);
	fclose(fp);
	if (len != (size_t)length) {
		free(buf);
		p->smb_result = SMB_ERR_READ;
		SAFEPRINTF(p->smb.last_error, "error reading body file: %s", fname);
		return NULL;
	}
	buf[len] = '\0';
	if (strlen(buf) != len) {
		free(buf);
		p->smb_result = SMB_ERR_READ;
		SAFEPRINTF(p->smb.last_error, "body file contains a NUL character: %s", fname);
		return NULL;
	}
	return buf;
}

static JSBool
js_save_msg_from_file(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      body;
	char*      fname = NULL;
	JSObject*  hdr;
	JSObject*  rcpt_list = NULL;
	private_t* p;
	JSBool     ret;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb))) {
		if (!js_open(cx, 0, arglist))
			return JS_FALSE;
		if (JS_RVAL(cx, arglist) == JSVAL_FALSE)
			return JS_TRUE;
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	}

	if (!JSVAL_IS_OBJECT(argv[0]) || JSVAL_IS_NULL(argv[0]) || JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[0]))) {
		JS_ReportError(cx, "Invalid message header object");
		return JS_FALSE;
	}
	hdr = JSVAL_TO_OBJECT(argv[0]);

	JSVALUE_TO_MSTRING(cx, argv[1], fname, NULL);
	HANDLE_PENDING(cx, fname);
	if (fname == NULL) {
		JS_ReportError(cx, "Invalid body file path string");
		return JS_FALSE;
	}

	if (argc > 2 && !JSVAL_NULL_OR_VOID(argv[2])) {
		if (!JSVAL_IS_OBJECT(argv[2]) || !JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[2]))) {
			free(fname);
			JS_ReportError(cx, "Invalid recipient list (not an array)");
			return JS_FALSE;
		}
		rcpt_list = JSVAL_TO_OBJECT(argv[2]);
	}

	rc = JS_SUSPENDREQUEST(cx);
	body = read_body_file(p, fname);
	JS_RESUMEREQUEST(cx, rc);
	free(fname);
	if (body == NULL)
		return JS_TRUE;

	ret = save_msg_common(cx, arglist, p, hdr, rcpt_list, /* client: */ NULL, body);
	free(body);

	return ret;
}

/* Apply the header changes object of copy_msg()/move_msg() to the copy:
 * only "tags" and "attr" are supported (others are refused, not ignored) */
static bool apply_copy_header_changes(JSContext* cx, private_t* p, JSObject* changes, smbmsg_t* msg)
{
	JSIdArray* ids;
	jsval      idval;
	jsval      val;
	char       name[128];
	char*      cp;
	uint32     u32;
	bool       result = true;

	if ((ids = JS_Enumerate(cx, changes)) == NULL)
		return false;
	for (jsint i = 0; result && i < ids->length; i++) {
		JS_IdToValue(cx, ids->vector[i], &idval);
		JSVALUE_TO_STRBUF(cx, idval, name, sizeof(name), NULL);
		if (!JS_GetProperty(cx, changes, name, &val)) {
			result = false;
			break;
		}
		if (JSVAL_NULL_OR_VOID(val))
			continue;
		if (strcmp(name, "tags") == 0) {
			cp = NULL;
			JSVALUE_TO_MSTRING(cx, val, cp, NULL);
			if (cp == NULL) {
				p->smb_result = SMB_FAILURE;
				SAFEPRINTF(p->smb.last_error, "Invalid \"%s\" string in header changes", name);
				result = false;
				break;
			}
			if ((p->smb_result = smb_new_hfield_str(msg, SMB_TAGS, cp)) != SMB_SUCCESS) {
				SAFEPRINTF(p->smb.last_error, "Error %d setting the tags of the copy", p->smb_result);
				result = false;
			}
			free(cp);
		}
		else if (strcmp(name, "attr") == 0) {
			if (!JS_ValueToECMAUint32(cx, val, &u32)) {
				result = false;
				break;
			}
			msg->hdr.attr = (uint16_t)u32;
		}
		else {
			p->smb_result = SMB_FAILURE;
			SAFEPRINTF(p->smb.last_error, "Unsupported header change: \"%s\"", name);
			result = false;
		}
	}
	JS_DestroyIdArray(cx, ids);
	return result;
}

/* Write a copy (newmsg, from smb_copymsgmem) of message 'msg' of base 'src'
 * into base 'dst': the data fields are copied as stored (not decoded or
 * re-encoded) and the header is added with smb_addmsghdr(), which assigns
 * the new message number and builds the index record for dst's type.
 * Errors are described in src->last_error. */
static int write_msg_copy(scfg_t* scfg, smb_t* src, smbmsg_t* msg, smb_t* dst, smbmsg_t* newmsg)
{
	uint   length = smb_getmsgdatlen(msg);
	uchar* buf = NULL;
	off_t  offset;
	off_t  l;
	int    storage = smb_storage_mode(scfg, dst);
	int    result = SMB_SUCCESS;

	if (length > 0) {
		if ((buf = (uchar*)malloc(length)) == NULL) {
			safe_snprintf(src->last_error, sizeof(src->last_error)
			              , "malloc failure of %u bytes for message data", length);
			return SMB_ERR_MEM;
		}
		if (smb_fseek(src->sdt_fp, msg->hdr.offset, SEEK_SET) != 0
		    || smb_fread(src, buf, length, src->sdt_fp) != length) {
			safe_snprintf(src->last_error, sizeof(src->last_error)
			              , "error reading %u bytes of data of message #%u", length, msg->hdr.number);
			free(buf);
			return SMB_ERR_READ;
		}
	}

	do {
		if ((result = smb_locksmbhdr(dst)) != SMB_SUCCESS)
			break;
		// A path-opened base that was never saved to: create it as smb_addmsg() does
		if (smb_fgetlength(dst->shd_fp) < 1) {
			if ((result = smb_create(dst)) != SMB_SUCCESS) // unlocks the header on success
				break;
			if ((result = smb_locksmbhdr(dst)) != SMB_SUCCESS)
				break;
		}
		if ((result = smb_getstatus(dst)) != SMB_SUCCESS)
			break;
		if (length < 1) {
			newmsg->hdr.offset = 0;
			break;
		}
		if (dst->status.attr & SMB_HYPERALLOC) {
			offset = smb_hallocdat(dst);
			storage = SMB_HYPERALLOC;
		} else {
			if ((result = smb_open_da(dst)) != SMB_SUCCESS)
				break;
			if (storage == SMB_FASTALLOC)
				offset = smb_fallocdat(dst, length, 1);
			else {
				offset = smb_allocdat(dst, length, 1);
				storage = SMB_SELFPACK;
			}
			smb_close_da(dst);
		}
		if (offset < 0) {
			result = (int)offset;
			break;
		}
		if (offset > SMB_MAX_DAT_OFFSET - length) {
			smb_freemsgdat(dst, offset, length, 1);
			safe_snprintf(dst->last_error, sizeof(dst->last_error)
			              , "data offset (%" PRIdOFF ") + length (%u) exceeds %" PRIdOFF
			              , offset, length, SMB_MAX_DAT_OFFSET);
			result = SMB_ERR_DAT_OFFSET;
			break;
		}
		newmsg->hdr.offset = (uint32_t)offset;
		if (smb_fseek(dst->sdt_fp, offset, SEEK_SET) != 0) {
			safe_snprintf(dst->last_error, sizeof(dst->last_error), "seek error %d", errno);
			result = SMB_ERR_SEEK;
		}
		else if (smb_fwrite(dst, buf, length, dst->sdt_fp) != length) {
			safe_snprintf(dst->last_error, sizeof(dst->last_error), "error writing %u bytes of message data", length);
			result = SMB_ERR_WRITE;
		}
		else {
			for (l = length; l % SDT_BLOCK_LEN; l++) {
				if (smb_fputc(0, dst->sdt_fp) != 0)
					break;
			}
			if (l % SDT_BLOCK_LEN) {
				safe_snprintf(dst->last_error, sizeof(dst->last_error), "error writing data padding");
				result = SMB_ERR_WRITE;
			}
			else if (fflush(dst->sdt_fp) != 0) { // most of the data is written only now
				safe_snprintf(dst->last_error, sizeof(dst->last_error)
				              , "error %d flushing %u bytes of message data", errno, length);
				result = SMB_ERR_WRITE;
			}
		}
		if (result != SMB_SUCCESS)
			smb_freemsg_dfields(dst, newmsg, 1);
	} while (0);
	free(buf);

	if (result != SMB_SUCCESS) {
		if (dst->smbhdr_locked)
			smb_unlocksmbhdr(dst);
	}
	else if ((result = smb_addmsghdr(dst, newmsg, storage)) != SMB_SUCCESS) { // unlocks the header
		if (length > 0)
			smb_freemsg_dfields(dst, newmsg, 1);
	}
	if (result != SMB_SUCCESS && src != dst)
		safe_snprintf(src->last_error, sizeof(src->last_error), "%s: %s", dst->file, dst->last_error);
	return result;
}

/* Read message 'number' of 'smb' back from its header:
 * 1 if it is flagged deleted, 0 if not, -1 if it cannot be read */
static int msg_is_deleted(smb_t* smb, uint32_t number)
{
	smbmsg_t msg;
	int      result = -1;

	memset(&msg, 0, sizeof(msg));
	msg.hdr.number = number;
	if (smb_getmsgidx(smb, &msg) == SMB_SUCCESS
	    && smb_getmsghdr_for_idx(smb, &msg) == SMB_SUCCESS)
		result = (msg.hdr.attr & MSG_DELETE) ? 1 : 0;
	smb_freemsgmem(&msg);
	return result;
}

/* Undo a copy written by write_msg_copy(): flag it deleted, without letting
 * its removal delete any attached files (the source still owns them) */
static int undo_msg_copy(smb_t* dst, smbmsg_t* copy)
{
	copy->hdr.auxattr &= ~(MSG_FILEATTACH | MSG_KILLFILE);
	copy->hdr.attr |= MSG_DELETE;
	return smb_updatemsg(dst, copy);
}

/* A message number argument: number 0 (or a value that is not a number)
 * would select the oldest message (index offset 0), so it is refused, with
 * last_error set */
static bool valid_msg_number(JSContext* cx, private_t* p, jsval val, const char* method, uint32* number)
{
	if (!JSVAL_IS_NUMBER(val)) {
		p->smb_result = SMB_FAILURE;
		SAFEPRINTF(p->smb.last_error, "%s: the message number is not a number", method);
		return false;
	}
	if (!JS_ValueToECMAUint32(cx, val, number))
		return false;
	if (*number == 0) {
		p->smb_result = SMB_FAILURE;
		SAFEPRINTF(p->smb.last_error, "%s: invalid message number: 0", method);
		return false;
	}
	return true;
}

static JSBool
js_copy_or_move_msg(JSContext *cx, uintN argc, jsval *arglist, bool move)
{
	JSObject *  obj = JS_THIS_OBJECT(cx, arglist);
	jsval *     argv = JS_ARGV(cx, arglist);
	JSObject*   changes = NULL;
	uint32      number;
	smbmsg_t    msg;
	smbmsg_t    newmsg;
	private_t*  p;
	private_t*  dp;
	jsrefcount  rc;
	int         result;
	scfg_t*     scfg;
	const char* method = move ? "move_msg" : "copy_msg";

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL)
		return JS_FALSE;

	if (!valid_msg_number(cx, p, argv[0], method, &number))
		return JS_IsExceptionPending(cx) ? JS_FALSE : JS_TRUE;

	if (JSVAL_IS_PRIMITIVE(argv[1])
	    || (dp = (private_t*)JS_GetInstancePrivate(cx, JSVAL_TO_OBJECT(argv[1]), &js_msgbase_class, NULL)) == NULL) {
		JS_ReportError(cx, "%s: destination is not a MsgBase object", method);
		return JS_FALSE;
	}

	if (argc > 2 && !JSVAL_NULL_OR_VOID(argv[2])) {
		if (JSVAL_IS_PRIMITIVE(argv[2])) {
			JS_ReportError(cx, "%s: header changes is not an object", method);
			return JS_FALSE;
		}
		changes = JSVAL_TO_OBJECT(argv[2]);
	}

	if (!SMB_IS_OPEN(&(p->smb)) || !SMB_IS_OPEN(&(dp->smb))) {
		p->smb_result = SMB_ERR_NOT_OPEN;
		SAFEPRINTF(p->smb.last_error, "%s: both message bases must be open", method);
		return JS_TRUE;
	}

	if (move && (dp == p || strcmp(dp->smb.file, p->smb.file) == 0)) {
		p->smb_result = SMB_FAILURE;
		SAFEPRINTF(p->smb.last_error, "%s: the destination is the source message base", method);
		return JS_TRUE;
	}

	memset(&msg, 0, sizeof(msg));
	memset(&newmsg, 0, sizeof(newmsg));
	msg.hdr.number = number;

	rc = JS_SUSPENDREQUEST(cx);
	if ((p->smb_result = smb_getmsgidx(&(p->smb), &msg)) != SMB_SUCCESS
	    || (p->smb_result = smb_getmsghdr_for_idx(&(p->smb), &msg)) != SMB_SUCCESS) {
		smb_freemsgmem(&msg);
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	if (move && (msg.hdr.attr & MSG_DELETE)) {
		p->smb_result = SMB_FAILURE;
		SAFEPRINTF2(p->smb.last_error, "%s: message #%u is deleted", method, number);
		smb_freemsgmem(&msg);
		return JS_TRUE;
	}

	// Two messages must never own the same attached files
	if (!move && (msg.hdr.auxattr & MSG_FILEATTACH)) {
		p->smb_result = SMB_FAILURE;
		SAFEPRINTF2(p->smb.last_error, "%s: message #%u has attached files (move it instead)", method, number);
		smb_freemsgmem(&msg);
		return JS_TRUE;
	}

	if ((p->smb_result = smb_copymsgmem(&(p->smb), &newmsg, &msg)) != SMB_SUCCESS) {
		smb_freemsgmem(&newmsg);
		smb_freemsgmem(&msg);
		return JS_TRUE;
	}

	// Thread links are message numbers in the source base: clear them
	newmsg.hdr.thread_id = 0;
	newmsg.hdr.thread_back = 0;
	newmsg.hdr.thread_next = 0;
	newmsg.hdr.thread_first = 0;
	newmsg.flags &= ~MSG_FLAG_HASHED;   // hash it in the destination

	if (changes != NULL && !apply_copy_header_changes(cx, p, changes, &newmsg)) {
		smb_freemsgmem(&newmsg);
		smb_freemsgmem(&msg);
		return JS_IsExceptionPending(cx) ? JS_FALSE : JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	result = write_msg_copy(scfg, &(p->smb), &msg, &(dp->smb), &newmsg);
	if (result == SMB_SUCCESS && move) {
		bool already_deleted = false;
		int  deleted = 0;
		char err[sizeof(p->smb.last_error)] = "";
		// The copy now owns any attached files: the source must not delete them
		msg.hdr.auxattr &= ~(MSG_FILEATTACH | MSG_KILLFILE);
		msg.hdr.attr |= MSG_DELETE;
		// As smb_updatemsg() does, but a message that a concurrent move (or
		// removal) flagged deleted since it was read is not moved again
		if ((result = smb_locksmbhdr(&(p->smb))) == SMB_SUCCESS) {
			fflush(p->smb.sid_fp); // discard index records buffered by the earlier read
			if ((result = smb_getmsgidx(&(p->smb), &msg)) == SMB_SUCCESS) {
				if (msg.idx.attr & MSG_DELETE)
					already_deleted = true;
				else if ((result = smb_lockmsghdr(&(p->smb), &msg)) == SMB_SUCCESS) {
					result = smb_putmsg(&(p->smb), &msg);
					smb_unlockmsghdr(&(p->smb), &msg);
				}
			}
		}
		if (result != SMB_SUCCESS) {
			SAFECOPY(err, p->smb.last_error);
			// The header may have been written without the index: read it back
			// (still under the header lock, if it was taken)
			deleted = msg_is_deleted(&(p->smb), number);
		}
		if (p->smb.smbhdr_locked)
			smb_unlocksmbhdr(&(p->smb));
		if (already_deleted) {
			int undo = undo_msg_copy(&(dp->smb), &newmsg);
			if (undo == SMB_SUCCESS)
				safe_snprintf(p->smb.last_error, sizeof(p->smb.last_error)
				              , "%s: message #%u was deleted (by another move?), copy removed", method, number);
			else
				safe_snprintf(p->smb.last_error, sizeof(p->smb.last_error)
				              , "%s: message #%u was deleted (by another move?), and error %d removing its copy #%u from %s (%s)"
				              , method, number, undo, newmsg.hdr.number, dp->smb.file, dp->smb.last_error);
			result = SMB_FAILURE;
		}
		else if (result != SMB_SUCCESS) {
			if (deleted > 0) {
				// The message now lives only in dest: keep the copy
				safe_snprintf(p->smb.last_error, sizeof(p->smb.last_error)
				              , "%s: message #%u flagged deleted, but with error %d (%s)", method, number, result, err);
				result = SMB_SUCCESS;
			}
			else if (deleted < 0) {
				// Unknown state of the source: a duplicate is better than a loss
				safe_snprintf(p->smb.last_error, sizeof(p->smb.last_error)
				              , "%s: error %d removing message #%u (%s), its state is unknown, so its copy #%u in %s was kept"
				              , method, result, number, err, newmsg.hdr.number, dp->smb.file);
			}
			else {
				int undo = undo_msg_copy(&(dp->smb), &newmsg);
				if (undo == SMB_SUCCESS)
					safe_snprintf(p->smb.last_error, sizeof(p->smb.last_error)
					              , "%s: error %d removing message #%u (%s), copy removed", method, result, number, err);
				else
					safe_snprintf(p->smb.last_error, sizeof(p->smb.last_error)
					              , "%s: error %d removing message #%u (%s), and error %d removing its copy #%u from %s (%s)"
					              , method, result, number, err, undo, newmsg.hdr.number, dp->smb.file, dp->smb.last_error);
			}
		}
	}
	p->smb_result = result;
	if (result == SMB_SUCCESS)
		JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL(newmsg.hdr.number));
	smb_freemsgmem(&newmsg);
	smb_freemsgmem(&msg);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_copy_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_copy_or_move_msg(cx, argc, arglist, /* move: */ false);
}

static JSBool
js_move_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_copy_or_move_msg(cx, argc, arglist, /* move: */ true);
}

/* Whether the paths name the same message base, compared as full paths
 * ignoring case (a false match only refuses a purge) */
static bool same_msgbase_path(const char* path, const char* base)
{
	char full_path[MAX_PATH + 1];
	char full_base[MAX_PATH + 1];

	if (FULLPATH(full_path, path, sizeof full_path) == NULL
	    || FULLPATH(full_base, base, sizeof full_base) == NULL)
		return stricmp(path, base) == 0;
	return stricmp(full_path, full_base) == 0;
}

/* The system mail base and the configured sub-boards have maintenance of
 * their own: whether this base is one of them, opened by code or by path */
static bool msgbase_is_maintained(scfg_t* scfg, private_t* p)
{
	char path[MAX_PATH + 1];

	if (p->smb.subnum == INVALID_SUB || subnum_is_valid(scfg, p->smb.subnum))
		return true;
	SAFEPRINTF(path, "%smail", scfg->data_dir);
	if (same_msgbase_path(p->smb.file, path))
		return true;
	for (int i = 0; i < scfg->total_subs; i++) {
		SAFEPRINTF2(path, "%s%s", scfg->sub[i]->data_dir, scfg->sub[i]->code);
		if (same_msgbase_path(p->smb.file, path))
			return true;
	}
	return false;
}

/* Flagged deleted, permanent or not: unlike delmail(), a purge expires
 * nothing, so the permanent flag has nothing to protect */
static bool msg_is_purgeable(const idxrec_t* idx)
{
	return (idx->attr & MSG_DELETE) != 0;
}

/* Free the messages flagged deleted: remove their index records, then
 * free their header and data blocks (one data reference per header).
 * The base is locked (lock file and header) throughout. Nothing is
 * changed unless every purged message's header can be read; a failure
 * rewriting the index restores it. A failure freeing blocks leaves them
 * allocated, but unindexed, until the base is packed.
 * Returns an SMB error code (with last_error set) or SMB_SUCCESS. */
static int purge_deleted_msgs(smb_t* smb, uint32_t* purged)
{
	int       result;
	uint32_t  total;
	uint32_t  kept = 0;
	uint32_t  removed = 0;
	uint32_t  i;
	idxrec_t* idx = NULL;
	smbmsg_t* msgs = NULL;

	*purged = 0;
	if ((result = smb_lock(smb)) != SMB_SUCCESS)
		return result;
	// The lock file doesn't stop a process that already has the base open
	if ((result = smb_locksmbhdr(smb)) != SMB_SUCCESS) {
		smb_unlock(smb);
		return result;
	}
	if ((result = smb_getstatus(smb)) != SMB_SUCCESS)
		goto unlock;
	// Its index records are larger (fileidxrec_t) than a message base's
	if (smb->status.attr & SMB_FILE_DIRECTORY) {
		result = SMB_FAILURE;
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "purge: not allowed on a file base");
		goto unlock;
	}
	total = smb->status.total_msgs;
	if (total < 1)
		goto unlock;
	if ((idx = static_cast<idxrec_t *>(malloc(total * sizeof(*idx)))) == NULL) {
		result = SMB_ERR_MEM;
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "purge: malloc failure of %u index records", total);
		goto unlock;
	}
	smb_rewind(smb->sid_fp);
	if (smb_fread(smb, idx, total * sizeof(*idx), smb->sid_fp) != total * sizeof(*idx)) {
		result = SMB_ERR_READ;
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "purge: error reading %u index records", total);
		goto unlock;
	}
	for (i = 0; i < total; i++) {
		if (msg_is_purgeable(&idx[i]))
			removed++;
	}
	if (removed < 1)
		goto unlock;
	if ((msgs = static_cast<smbmsg_t *>(calloc(removed, sizeof(*msgs)))) == NULL) {
		result = SMB_ERR_MEM;
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "purge: malloc failure of %u message headers", removed);
		goto unlock;
	}
	// Read every header to be freed before changing anything
	for (i = 0; i < total; i++) {
		if (!msg_is_purgeable(&idx[i]))
			continue;
		msgs[kept].idx = idx[i];
		if ((result = smb_getmsghdr_for_idx(smb, &msgs[kept])) != SMB_SUCCESS) {
			char error[sizeof(smb->last_error)];
			SAFECOPY(error, smb->last_error);
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "purge: error %d reading the header of message #%u: %s"
			              , result, idx[i].number, error);
			removed = kept;
			goto unlock;
		}
		kept++;
	}
	// Rewrite the index without the purged messages
	kept = 0;
	smb_rewind(smb->sid_fp);
	for (i = 0; i < total && result == SMB_SUCCESS; i++) {
		if (msg_is_purgeable(&idx[i]))
			continue;
		if (smb_fwrite(smb, &idx[i], sizeof(*idx), smb->sid_fp) != sizeof(*idx))
			result = SMB_ERR_WRITE;
		kept++;
	}
	if (result == SMB_SUCCESS && fflush(smb->sid_fp) != 0)
		result = SMB_ERR_WRITE;
	if (result == SMB_SUCCESS && smb_fsetlength(smb->sid_fp, kept * sizeof(*idx)) != 0)
		result = SMB_ERR_TRUNCATE;
	if (result == SMB_SUCCESS) {
		smb->status.total_msgs = kept;
		if ((result = smb_putstatus(smb)) != SMB_SUCCESS)
			smb->status.total_msgs = total;
	}
	if (result != SMB_SUCCESS) {
		// Put back the index as it was (the status on disk is unchanged)
		smb_rewind(smb->sid_fp);
		smb_clearerr(smb->sid_fp);
		if (smb_fwrite(smb, idx, total * sizeof(*idx), smb->sid_fp) != total * sizeof(*idx)
		    || fflush(smb->sid_fp) != 0
		    || smb_fsetlength(smb->sid_fp, total * sizeof(*idx)) != 0)
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "purge: error %d rewriting the index, and restoring it failed", result);
		else
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "purge: error %d rewriting the index (restored)", result);
		goto unlock;
	}
	*purged = removed;

	// The purged messages are no longer indexed: free their headers and data
	if (!(smb->status.attr & SMB_HYPERALLOC)) {
		if ((result = smb_open_da(smb)) == SMB_SUCCESS) {
			if ((result = smb_open_ha(smb)) == SMB_SUCCESS) {
				for (i = 0; i < removed; i++) {
					int freed = smb_freemsg(smb, &msgs[i]);
					if (freed != SMB_SUCCESS && result == SMB_SUCCESS)
						result = freed;
				}
				smb_close_ha(smb);
			}
			smb_close_da(smb);
		}
		if (result != SMB_SUCCESS) {
			char error[sizeof(smb->last_error)];
			SAFECOPY(error, smb->last_error);
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "purge: %u messages removed from the index, but error %d freeing their blocks"
			              " (they stay allocated until the base is packed): %s"
			              , removed, result, error);
		}
	}

unlock:
	if (msgs != NULL) {
		for (i = 0; i < removed; i++)
			smb_freemsgmem(&msgs[i]);
		free(msgs);
	}
	free(idx);
	smb_unlocksmbhdr(smb);
	if (smb_unlock(smb) != SMB_SUCCESS && result == SMB_SUCCESS)
		result = SMB_ERR_DELETE; // smb_unlock() set last_error
	return result;
}

static JSBool
js_purge(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	scfg_t*    scfg;
	uint32_t   purged;
	jsrefcount rc;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL)
		return JS_FALSE;

	if (msgbase_is_maintained(scfg, p)) {
		p->smb_result = SMB_FAILURE;
		SAFECOPY(p->smb.last_error
		         , "purge: not allowed on the mail base or a sub-board (they have their own maintenance)");
		return JS_TRUE;
	}
	if (!SMB_IS_OPEN(&(p->smb))) {
		p->smb_result = SMB_ERR_NOT_OPEN;
		SAFECOPY(p->smb.last_error, "purge: the message base is not open");
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	p->smb_result = purge_deleted_msgs(&(p->smb), &purged);
	JS_RESUMEREQUEST(cx, rc);
	if (p->smb_result == SMB_SUCCESS)
		JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL(purged));
	return JS_TRUE;
}

/* Read the header and the undecoded body (no header fields or tails) of
 * message 'number' (the caller frees both), or NULL with last_error set */
static char* get_msg_body_for_parts(private_t* p, uint32_t number, smbmsg_t* msg)
{
	char* body;

	memset(msg, 0, sizeof(*msg));
	msg->hdr.number = number;
	if ((p->smb_result = smb_getmsgidx(&(p->smb), msg)) != SMB_SUCCESS
	    || (p->smb_result = smb_getmsghdr_for_idx(&(p->smb), msg)) != SMB_SUCCESS)
		return NULL;
	if ((body = get_msg_text(p, msg, /* strip_ctrl_a: */ false, /* dot_stuffing: */ false
	                         , GETMSGTXT_BODY_ONLY, /* existing: */ JS_TRUE)) == NULL
	    && p->smb_result == SMB_SUCCESS) {
		p->smb_result = SMB_ERR_MEM;
		SAFEPRINTF(p->smb.last_error, "error reading the body of message #%u", number);
	}
	return body;
}

static JSBool
js_get_msg_parts(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *      obj = JS_THIS_OBJECT(cx, arglist);
	jsval *         argv = JS_ARGV(cx, arglist);
	uint32          number;
	smbmsg_t        msg;
	smb_mimepart_t* parts = NULL;
	size_t          count = 0;
	char*           body;
	private_t*      p;
	jsrefcount      rc;
	JSObject*       array;
	JSObject*       part;
	JSString*       js_str;
	jsval           val;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL)
		return JS_FALSE;

	if (!valid_msg_number(cx, p, argv[0], "get_msg_parts", &number))
		return JS_IsExceptionPending(cx) ? JS_FALSE : JS_TRUE;

	if (!SMB_IS_OPEN(&(p->smb))) {
		p->smb_result = SMB_ERR_NOT_OPEN;
		SAFECOPY(p->smb.last_error, "get_msg_parts: the message base is not open");
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if ((body = get_msg_body_for_parts(p, number, &msg)) != NULL
	    && (parts = smb_getmimeparts(&msg, body, &count)) == NULL) {
		p->smb_result = SMB_ERR_MEM;
		SAFECOPY(p->smb.last_error, "get_msg_parts: memory allocation failure");
	}
	JS_RESUMEREQUEST(cx, rc);

	if (parts != NULL && (array = JS_NewArrayObject(cx, 0, NULL)) != NULL) {
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));
		for (size_t i = 0; i < count; i++) {
			if ((part = JS_NewObject(cx, NULL, NULL, array)) == NULL)
				break;
			val = OBJECT_TO_JSVAL(part);
			if (!JS_SetElement(cx, array, (jsint)i, &val))
				break;
			const struct {
				const char* name;
				const char* value;
			} props[] = {
				{ "id", parts[i].id },
				{ "content_type", parts[i].content_type },
				{ "charset", parts[i].charset },
				{ "name", parts[i].name },
				{ "content_id", parts[i].content_id },
				{ "disposition", parts[i].disposition },
			};
			for (size_t j = 0; j < sizeof(props) / sizeof(props[0]); j++) {
				if ((js_str = JS_NewStringCopyZ(cx, props[j].value)) == NULL)
					break;
				JS_DefineProperty(cx, part, props[j].name, STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_ENUMERATE);
			}
			val = DOUBLE_TO_JSVAL((jsdouble)parts[i].decoded_length);
			JS_DefineProperty(cx, part, "size", val, NULL, NULL, JSPROP_ENUMERATE);
		}
	}
	smb_freemimeparts(parts);
	smb_freemsgtxt(body);
	smb_freemsgmem(&msg);

	return JS_TRUE;
}

static JSBool
js_save_msg_part(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *      obj = JS_THIS_OBJECT(cx, arglist);
	jsval *         argv = JS_ARGV(cx, arglist);
	uint32          number;
	smbmsg_t        msg;
	smb_mimepart_t* parts = NULL;
	size_t          count = 0;
	char*           body;
	char            id[sizeof(parts->id) + 1];   // longer than any part id: not truncated to one
	char*           path = NULL;
	uint8_t*        decoded = NULL;
	const void*     data = NULL;
	size_t          len = 0;
	FILE*           fp;
	private_t*      p;
	jsrefcount      rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (js_argcIsInsufficient(cx, argc, 3))
		return JS_FALSE;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL)
		return JS_FALSE;

	if (!valid_msg_number(cx, p, argv[0], "save_msg_part", &number))
		return JS_IsExceptionPending(cx) ? JS_FALSE : JS_TRUE;

	if (!JSVAL_IS_STRING(argv[1])) {
		JS_ReportError(cx, "save_msg_part: the part id is not a string");
		return JS_FALSE;
	}
	JSVALUE_TO_STRBUF(cx, argv[1], id, sizeof(id), NULL);

	JSVALUE_TO_MSTRING(cx, argv[2], path, NULL);
	HANDLE_PENDING(cx, path);
	if (path == NULL) {
		JS_ReportError(cx, "save_msg_part: invalid file path string");
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb))) {
		free(path);
		p->smb_result = SMB_ERR_NOT_OPEN;
		SAFECOPY(p->smb.last_error, "save_msg_part: the message base is not open");
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if ((body = get_msg_body_for_parts(p, number, &msg)) != NULL) {
		if (strcmp(id, "0") == 0) {   /* the whole body, as stored */
			data = body;
			len = strlen(body);
		} else if ((parts = smb_getmimeparts(&msg, body, &count)) == NULL) {
			p->smb_result = SMB_ERR_MEM;
			SAFECOPY(p->smb.last_error, "save_msg_part: memory allocation failure");
		} else {
			size_t i;
			for (i = 0; i < count; i++) {
				if (strcmp(parts[i].id, id) == 0)
					break;
			}
			if (i >= count) {
				p->smb_result = SMB_ERR_NOT_FOUND;
				SAFEPRINTF2(p->smb.last_error, "save_msg_part: message #%u has no part \"%s\"", number, id);
			} else if ((decoded = (uint8_t*)malloc(parts[i].decoded_length + 1)) == NULL) {
				p->smb_result = SMB_ERR_MEM;
				SAFEPRINTF(p->smb.last_error, "save_msg_part: malloc failure of %" XP_PRIsize_t "u bytes"
				           , parts[i].decoded_length + 1);
			} else {
				len = smb_decodemimepart(&parts[i], decoded);
				data = decoded;
			}
		}
	}
	if (data != NULL) {
		if ((fp = fopen(path, "wb")) == NULL) {
			p->smb_result = SMB_ERR_OPEN;
			SAFEPRINTF2(p->smb.last_error, "save_msg_part: error %d creating %s", errno, path);
		} else {
			bool written = fwrite(data, 1, len, fp) == len;
			if (fclose(fp) != 0)
				written = false;
			if (written)
				JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
			else {
				p->smb_result = SMB_ERR_WRITE;
				SAFEPRINTF2(p->smb.last_error, "save_msg_part: error %d writing %s", errno, path);
				remove(path);
			}
		}
	}
	free(decoded);
	smb_freemimeparts(parts);
	smb_freemsgtxt(body);
	smb_freemsgmem(&msg);
	JS_RESUMEREQUEST(cx, rc);
	free(path);

	return JS_TRUE;
}

static JSBool
js_vote_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	uintN      n;
	JSObject*  hdr = NULL;
	JSObject*  objarg;
	smbmsg_t   msg;
	private_t* p;
	JSBool     ret = JS_TRUE;
	jsrefcount rc;
	scfg_t*    scfg;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb))) {
		if (!js_open(cx, 0, arglist))
			return JS_FALSE;
		if (JS_RVAL(cx, arglist) == JSVAL_FALSE)
			return JS_TRUE;
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	}

	memset(&msg, 0, sizeof(msg));
	msg.hdr.type = SMB_MSG_TYPE_BALLOT;

	for (n = 0; n < argc; n++) {
		if (JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			objarg = JSVAL_TO_OBJECT(argv[n]);
			if (hdr == NULL) {
				hdr = objarg;
				continue;
			}
		}
	}

	if (hdr == NULL)
		return JS_TRUE;

	if (parse_header_object(cx, p, hdr, &msg, false)) {

		rc = JS_SUSPENDREQUEST(cx);
		if ((p->smb_result = votemsg(scfg, &(p->smb), &msg, NULL, NULL)) == SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		}
		else
			JS_RESUMEREQUEST(cx, rc);
	} else {
		ret = JS_FALSE;
		SAFECOPY(p->smb.last_error, "Header parsing failure (required field missing?)");
	}

	smb_freemsgmem(&msg);

	return ret;
}

static JSBool
js_add_poll(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	uintN      n;
	JSObject*  hdr = NULL;
	JSObject*  objarg;
	smbmsg_t   msg;
	private_t* p;
	JSBool     ret = JS_TRUE;
	jsrefcount rc;
	scfg_t*    scfg;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb))) {
		if (!js_open(cx, 0, arglist))
			return JS_FALSE;
		if (JS_RVAL(cx, arglist) == JSVAL_FALSE)
			return JS_TRUE;
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	}

	memset(&msg, 0, sizeof(msg));
	msg.hdr.type = SMB_MSG_TYPE_POLL;

	for (n = 0; n < argc; n++) {
		if (JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			objarg = JSVAL_TO_OBJECT(argv[n]);
			if (hdr == NULL) {
				hdr = objarg;
				continue;
			}
		}
	}

	if (hdr == NULL)
		return JS_TRUE;

	if (parse_header_object(cx, p, hdr, &msg, false)) {

		rc = JS_SUSPENDREQUEST(cx);
		if ((p->smb_result = postpoll(scfg, &(p->smb), &msg)) == SMB_SUCCESS) {
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		}
		else
			JS_RESUMEREQUEST(cx, rc);
	} else {
		ret = JS_FALSE;
		SAFECOPY(p->smb.last_error, "Header parsing failure (required field missing?)");
	}

	smb_freemsgmem(&msg);

	return ret;
}

static JSBool
js_how_user_voted(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	uint32     msgnum;
	private_t* p;
	char*      name = NULL;
	uint16_t   votes;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	if (!JS_ValueToECMAUint32(cx, argv[0], &msgnum))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[1], name, NULL)
	HANDLE_PENDING(cx, name);
	if (name == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	votes = smb_voted_already(&(p->smb), msgnum, name, NET_NONE, NULL);
	free(name);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL(votes));

	return JS_TRUE;
}

static JSBool
js_close_poll(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*  obj = JS_THIS_OBJECT(cx, arglist);
	jsval*     argv = JS_ARGV(cx, arglist);
	uint32     msgnum;
	private_t* p;
	char*      name = NULL;
	int        result;
	jsrefcount rc;
	scfg_t*    scfg;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	if (!SMB_IS_OPEN(&(p->smb)))
		return JS_TRUE;

	if (!JS_ValueToECMAUint32(cx, argv[0], &msgnum))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[1], name, NULL)
	HANDLE_PENDING(cx, name);
	if (name == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	result = closepoll(scfg, &(p->smb), msgnum, name);
	free(name);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, result == SMB_SUCCESS ? JSVAL_TRUE : JSVAL_FALSE);

	return JS_TRUE;
}

/* MsgBase Object Properties */
enum {
	SMB_PROP_LAST_ERROR
	, SMB_PROP_FILE
	, SMB_PROP_DEBUG
	, SMB_PROP_RETRY_TIME
	, SMB_PROP_RETRY_DELAY
	, SMB_PROP_FIRST_MSG     /* first message number */
	, SMB_PROP_LAST_MSG      /* last message number */
	, SMB_PROP_TOTAL_MSGS    /* total messages */
	, SMB_PROP_MAX_CRCS      /* Maximum number of CRCs to keep in history */
	, SMB_PROP_MAX_MSGS      /* Maximum number of message to keep in sub */
	, SMB_PROP_MAX_AGE       /* Maximum age of message to keep in sub (in days) */
	, SMB_PROP_ATTR          /* Attributes for this message base (SMB_HYPER,etc) */
	, SMB_PROP_SUBNUM        /* sub-board number */
	, SMB_PROP_IS_OPEN
	, SMB_PROP_STATUS        /* Last SMBLIB returned status value (e.g. retval) */
};

static JSBool js_msgbase_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval      idval;
	jsint      tiny;
	private_t* p;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_msgbase_class)) == NULL) {
		return JS_FALSE;
	}

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case SMB_PROP_RETRY_TIME:
			if (!JS_ValueToECMAUint32(cx, *vp, &(p->smb).retry_time))
				return JS_FALSE;
			break;
		case SMB_PROP_RETRY_DELAY:
			if (!JS_ValueToECMAUint32(cx, *vp, &(p->smb).retry_delay))
				return JS_FALSE;
			break;
		case SMB_PROP_DEBUG:
			JS_ValueToBoolean(cx, *vp, &p->debug);
			break;
	}

	return JS_TRUE;
}

static JSBool js_msgbase_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval      idval;
	char*      s = NULL;
	JSString*  js_str;
	jsint      tiny;
	idxrec_t   idx;
	private_t* p;
	jsrefcount rc;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case SMB_PROP_FILE:
			s = p->smb.file;
			break;
		case SMB_PROP_LAST_ERROR:
			s = p->smb.last_error;
			break;
		case SMB_PROP_STATUS:
			*vp = INT_TO_JSVAL(p->smb_result);
			break;
		case SMB_PROP_RETRY_TIME:
			*vp = INT_TO_JSVAL(p->smb.retry_time);
			break;
		case SMB_PROP_RETRY_DELAY:
			*vp = INT_TO_JSVAL(p->smb.retry_delay);
			break;
		case SMB_PROP_DEBUG:
			*vp = INT_TO_JSVAL(p->debug);
			break;
		case SMB_PROP_FIRST_MSG:
			if (SMB_IS_OPEN(&(p->smb))) {
				rc = JS_SUSPENDREQUEST(cx);
				memset(&idx, 0, sizeof(idx));
				smb_getfirstidx(&(p->smb), &idx);
				JS_RESUMEREQUEST(cx, rc);
				p->first_msg = idx.number;
				*vp = UINT_TO_JSVAL(idx.number);
			} else {
				*vp = UINT_TO_JSVAL(p->first_msg);
			}
			break;
		case SMB_PROP_LAST_MSG:
			rc = JS_SUSPENDREQUEST(cx);
			(void)smb_getstatus(&(p->smb));
			JS_RESUMEREQUEST(cx, rc);
			*vp = UINT_TO_JSVAL(p->smb.status.last_msg);
			break;
		case SMB_PROP_TOTAL_MSGS:
			rc = JS_SUSPENDREQUEST(cx);
			(void)smb_getstatus(&(p->smb));
			JS_RESUMEREQUEST(cx, rc);
			*vp = UINT_TO_JSVAL(p->smb.status.total_msgs);
			break;
		case SMB_PROP_MAX_CRCS:
			*vp = UINT_TO_JSVAL(p->smb.status.max_crcs);
			break;
		case SMB_PROP_MAX_MSGS:
			*vp = UINT_TO_JSVAL(p->smb.status.max_msgs);
			break;
		case SMB_PROP_MAX_AGE:
			*vp = UINT_TO_JSVAL(p->smb.status.max_age);
			break;
		case SMB_PROP_ATTR:
			*vp = UINT_TO_JSVAL(p->smb.status.attr);
			break;
		case SMB_PROP_SUBNUM:
			*vp = INT_TO_JSVAL(p->smb.subnum);
			break;
		case SMB_PROP_IS_OPEN:
			*vp = BOOLEAN_TO_JSVAL(SMB_IS_OPEN(&(p->smb)));
			break;
	}

	if (s != NULL) {
		if ((js_str = JS_NewStringCopyZ(cx, s)) == NULL)
			return JS_FALSE;
		*vp = STRING_TO_JSVAL(js_str);
	}

	return JS_TRUE;
}

#define SMB_PROP_FLAGS JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_msgbase_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{   "error", SMB_PROP_LAST_ERROR, SMB_PROP_FLAGS,    310 },
	{   "last_error", SMB_PROP_LAST_ERROR, JSPROP_READONLY,   311 },            /* alias */
	{   "status", SMB_PROP_STATUS, SMB_PROP_FLAGS,    312 },
	{   "file", SMB_PROP_FILE, SMB_PROP_FLAGS,    310 },
	{   "debug", SMB_PROP_DEBUG, 0,                 310 },
	{   "retry_time", SMB_PROP_RETRY_TIME, JSPROP_ENUMERATE,  310 },
	{   "retry_delay", SMB_PROP_RETRY_DELAY, JSPROP_ENUMERATE,  311 },
	{   "first_msg", SMB_PROP_FIRST_MSG, SMB_PROP_FLAGS,    310 },
	{   "last_msg", SMB_PROP_LAST_MSG, SMB_PROP_FLAGS,    310 },
	{   "total_msgs", SMB_PROP_TOTAL_MSGS, SMB_PROP_FLAGS,    310 },
	{   "max_crcs", SMB_PROP_MAX_CRCS, SMB_PROP_FLAGS,    310 },
	{   "max_msgs", SMB_PROP_MAX_MSGS, SMB_PROP_FLAGS,    310 },
	{   "max_age", SMB_PROP_MAX_AGE, SMB_PROP_FLAGS,    310 },
	{   "attributes", SMB_PROP_ATTR, SMB_PROP_FLAGS,    310 },
	{   "subnum", SMB_PROP_SUBNUM, SMB_PROP_FLAGS,    310 },
	{   "is_open", SMB_PROP_IS_OPEN, SMB_PROP_FLAGS,    310 },
	{0}
};

#ifdef BUILD_JSDOCS
static const char*      msgbase_prop_desc[] = {

	"Last occurred message base error - <small>READ ONLY</small>"
	, "Return value of last <i>SMB Library</i> function call - <small>READ ONLY</small>"
	, "Base path and filename of message base - <small>READ ONLY</small>"
	, "Message base open/lock retry timeout (in seconds)"
	, "Delay between message base open/lock retries (in milliseconds)"
	, "First message number - <small>READ ONLY</small>"
	, "Last message number - <small>READ ONLY</small>"
	, "Total number of messages - <small>READ ONLY</small>"
	, "Maximum number of message CRCs to store (for dupe checking) - <small>READ ONLY</small>"
	, "Maximum number of messages before expiration - <small>READ ONLY</small>"
	, "Maximum age (in days) of messages to store - <small>READ ONLY</small>"
	, "Message base attributes - <small>READ ONLY</small>"
	, "Sub-board number (0-based, 65535 for e-mail) - <small>READ ONLY</small>"
	, "<i>true</i> if the message base has been opened successfully - <small>READ ONLY</small>"
	, NULL
};
#endif

static jsSyncMethodSpec js_msgbase_functions[] = {
	{"open",            js_open,            0, JSTYPE_BOOLEAN,  JSDOCSTR("")
	 , JSDOCSTR("Open message base")
	 , 310},
	{"close",           js_close,           0, JSTYPE_BOOLEAN,  JSDOCSTR("")
	 , JSDOCSTR("Close message base (if open)")
	 , 310},
	{"get_msg_header",  js_get_msg_header,  4, JSTYPE_OBJECT
	 , JSDOCSTR("[<i>bool</i> by_offset=false,] <i>number</i> number_or_offset or <i>string</i> id [,<i>bool</i> expand_fields=true] [,<i>bool</i> include_votes=false]")
	 , JSDOCSTR("Return a specific message header, <i>null</i> on failure. "
		        "<br>Pass <i>false</i> for the <i>expand_fields</i> argument (default: <i>true</i>) "
		        "if you will be re-writing the header later with <i>put_msg_header()</i>"
		        "<br>"
		        "Additional read-only header properties: <i>mime_version</i>, <i>content_type</i>, and <i>is_utf8</i>"
		        )
	 , 312},
	{"get_all_msg_headers", js_get_all_msg_headers, 1, JSTYPE_OBJECT, JSDOCSTR("[<i>bool</i> include_votes=false] [,<i>bool</i> expand_fields=true]")
	 , JSDOCSTR("Return an object (associative array) of all message headers \"indexed\" by message number.<br>"
		        "Message headers returned by this function include additional properties: <tt>upvotes</tt>, <tt>downvotes</tt> and <tt>total_votes</tt>.<br>"
		        "Vote messages are excluded by default.")
	 , 316},
	{"put_msg_header",  js_put_msg_header,  2, JSTYPE_BOOLEAN,  JSDOCSTR("[<i>bool</i> by_offset=false,] [<i>number</i> number_or_offset or <i>string</i> id,] <i>object</i> header")
	 , JSDOCSTR("Modify an existing message header (must have been 'got' without expanded fields)")
	 , 310},
	{"get_msg_body",    js_get_msg_body,    2, JSTYPE_STRING,   JSDOCSTR("[<i>bool</i> by_offset=false,] <i>number</i> number_or_offset or <i>string</i> id or <i>object</i> header [,<i>bool</i> strip_ctrl_a=false] "
		                                                                 "[,<i>bool</i> dot_stuffing=false] [,<i>bool</i> include_tails=true] [,<i>bool</i> plain_text=false]")
	 , JSDOCSTR("Return the entire body text of a specific message as a single string or <i>null</i> on failure.<br>"
		        "The default behavior is to leave Ctrl-A codes intact, do not stuff dots (e.g. per RFC-821), and to include tails (if any) in the "
		        "returned body text.<br>"
		        "When <i>plain_text</i> is true, only the first plain-text portion of a multi-part MIME encoded message body is returned.<br>"
		        "The first argument (following the optional <i>by_offset</i> boolean) must be either a number (message number or index-offset), string (message-ID), or object (message header).<br>"
		        "The <i>by_offset</i> (<tt>true</tt>) argument should only be passed when the argument following it is the numeric index-offset of the message to be retrieved.<br>"
		        "By default (<i>by_offset</i>=<tt>false</tt>), a numeric argument would be interpreted as the message <i>number</i> to be retrieved.<br>"
		        "After reading a multi-part MIME-encoded message, new header properties may be available: <i>text_charset</i> and <i>text_subtype</i>."
		        )
	 , 310},
	{"get_msg_tail",    js_get_msg_tail,    2, JSTYPE_STRING,   JSDOCSTR("[<i>bool</i> by_offset=false,] <i>number</i> number_or_offset or <i>string</i> id or <i>object</i> header [,<i>bool</i> strip_ctrl_a=false]")
	 , JSDOCSTR("Return the tail text of a specific message, <i>null</i> on failure")
	 , 310},
	{"get_msg_index",   js_get_msg_index,   3, JSTYPE_OBJECT,   JSDOCSTR("[<i>bool</i> by_offset=false,] <i>number</i> number_or_offset, [<i>bool</i> include_votes=false]")
	 , JSDOCSTR("Return a specific message index record, <i>null</i> on failure."
		        "<p>The <i>index</i> object will contain the following properties:<br>"
		        "<table>"
		        "<tr><td align=top><tt>attr</tt><td>Attribute bit-flags"
		        "<tr><td align=top><tt>time</tt><td>Date/time imported (in time_t format)"
		        "<tr><td align=top><tt>number</tt><td>Message number"
		        "<tr><td align=top><tt>offset</tt><td>Record number in index file"
		        "</table>"
		        "<p>Indexes of regular messages will contain the following additional properties:<br>"
		        "<table>"
		        "<tr><td align=top><tt>subject</tt><td>CRC-16 of lowercase message subject"
		        "<tr><td align=top><tt>to</tt><td>CRC-16 of lowercase recipient's name (or user number if e-mail)"
		        "<tr><td align=top><tt>from</tt><td>CRC-16 of lowercase sender's name (or user number if e-mail)"
		        "</table>"
		        "<p>Indexes of vote messages will contain the following additional properties:<br>"
		        "<table>"
		        "<tr><td align=top><tt>vote</tt><td>vote value"
		        "<tr><td align=top><tt>remsg</tt><td>number of message this vote is in response to"
		        "</table>"
		        )
	 , 311},
	{"get_index",   js_get_index, 0, JSTYPE_ARRAY,  JSDOCSTR("")
	 , JSDOCSTR("Return an array of message index records represented as objects, the same format as returned by <i>get_msg_index()</i>"
		        "<br>"
		        "This is the fastest method of obtaining a list of all message index records.")
	 , 31702},
	{"remove_msg",      js_remove_msg,      2, JSTYPE_BOOLEAN,  JSDOCSTR("[<i>bool</i> by_offset=false,] <i>number</i> number_or_offset or <i>string</i> id")
	 , JSDOCSTR("Mark message for deletion")
	 , 311},
	{"save_msg",        js_save_msg,        2, JSTYPE_BOOLEAN,  JSDOCSTR("<i>object</i> header [,<i>object</i> client=<i>none</i>] [,body_text=\"\"] [,<i>array</i> rcpt_list=<i>none</i>]")
	 , JSDOCSTR("Create a new message in message base.<p>The <i>header</i> object may contain the following properties:<br>"
		        "<table>"
		        "<tr><td align=top><tt>subject</tt><td>Message subject <i>(required)</i>"
		        "<tr><td align=top><tt>to</tt><td>Recipient's name <i>(required)</i>"
		        "<tr><td align=top><tt>to_ext</tt><td>Recipient's user number (for local e-mail)"
		        "<tr><td align=top><tt>to_org</tt><td>Recipient's organization"
		        "<tr><td align=top><tt>to_net_type</tt><td>Recipient's network type (default: 0 for local)"
		        "<tr><td align=top><tt>to_net_addr</tt><td>Recipient's network address"
		        "<tr><td align=top><tt>to_agent</tt><td>Recipient's agent type"
		        "<tr><td align=top><tt>to_list</tt><td>Comma-separated list of primary recipients, RFC822-style"
		        "<tr><td align=top><tt>cc_list</tt><td>Comma-separated list of secondary recipients, RFC822-style"
		        "<tr><td align=top><tt>from</tt><td>Sender's name <i>(required)</i>"
		        "<tr><td align=top><tt>from_ext</tt><td>Sender's user number"
		        "<tr><td align=top><tt>from_org</tt><td>Sender's organization"
		        "<tr><td align=top><tt>from_net_type</tt><td>Sender's network type (default: 0 for local)"
		        "<tr><td align=top><tt>from_net_addr</tt><td>Sender's network address"
		        "<tr><td align=top><tt>from_agent</tt><td>Sender's agent type"
		        "<tr><td align=top><tt>from_ip_addr</tt><td>Sender's IP address (if available, for security tracking)"
		        "<tr><td align=top><tt>from_host_name</tt><td>Sender's host name (if available, for security tracking)"
		        "<tr><td align=top><tt>from_protocol</tt><td>TCP/IP protocol used by sender (if available, for security tracking)"
		        "<tr><td align=top><tt>from_port</tt><td>TCP/UDP port number used by sender (if available, for security tracking)"
		        "<tr><td align=top><tt>sender_userid</tt><td>Sender's user ID (if available, for security tracking)"
		        "<tr><td align=top><tt>sender_server</tt><td>Server's host name (if available, for security tracking)"
		        "<tr><td align=top><tt>sender_time</tt><td>Time/Date message was received from sender (if available, for security tracking)"
		        "<tr><td align=top><tt>replyto</tt><td>Replies should be sent to this name"
		        "<tr><td align=top><tt>replyto_ext</tt><td>Replies should be sent to this user number"
		        "<tr><td align=top><tt>replyto_org</tt><td>Replies should be sent to organization"
		        "<tr><td align=top><tt>replyto_net_type</tt><td>Replies should be sent to this network type"
		        "<tr><td align=top><tt>replyto_net_addr</tt><td>Replies should be sent to this network address"
		        "<tr><td align=top><tt>replyto_agent</tt><td>Replies should be sent to this agent type"
		        "<tr><td align=top><tt>replyto_list</tt><td>Comma-separated list of mailboxes to reply-to, RFC822-style"
		        "<tr><td align=top><tt>mime_version</tt><td>MIME Version (optional; ignored if <tt>field_list</tt> already has a <tt>MIME-Version:</tt> line, as in a header read from a base)"
		        "<tr><td align=top><tt>content_type</tt><td>MIME Content-Type (optional; ignored if <tt>field_list</tt> already has a <tt>Content-Type:</tt> line, as in a header read from a base)"
		        "<tr><td align=top><tt>summary</tt><td>Message Summary (optional)"
		        "<tr><td align=top><tt>editor</tt><td>Message Editor used by author (optional)"
		        "<tr><td align=top><tt>tags</tt><td>Message Tags (space-delimited, optional)"
		        "<tr><td align=top><tt>id</tt><td>Message's RFC-822 compliant Message-ID"
		        "<tr><td align=top><tt>reply_id</tt><td>Message's RFC-822 compliant Reply-ID"
		        "<tr><td align=top><tt>reverse_path</tt><td>Message's SMTP sender address"
		        "<tr><td align=top><tt>forward_path</tt><td>Argument to SMTP 'RCPT TO' command"
		        "<tr><td align=top><tt>path</tt><td>Messages's NNTP path"
		        "<tr><td align=top><tt>newsgroups</tt><td>Message's NNTP newsgroups header"
		        "<tr><td align=top><tt>ftn_msgid</tt><td>FidoNet FTS-9 Message-ID"
		        "<tr><td align=top><tt>ftn_reply</tt><td>FidoNet FTS-9 Reply-ID"
		        "<tr><td align=top><tt>ftn_area</tt><td>FidoNet FTS-4 echomail AREA tag"
		        "<tr><td align=top><tt>ftn_flags</tt><td>FidoNet FSC-53 FLAGS"
		        "<tr><td align=top><tt>ftn_pid</tt><td>FidoNet FSC-46 Program Identifier"
		        "<tr><td align=top><tt>ftn_tid</tt><td>FidoNet FSC-46 Tosser Identifier"
		        "<tr><td align=top><tt>ftn_charset</tt><td>FidoNet FTS-5003 Character Set Identifier"
		        "<tr><td align=top><tt>date</tt><td>RFC-822 formatted date/time"
		        "<tr><td align=top><tt>attr</tt><td>Attribute bit-flags"
		        "<tr><td align=top><tt>auxattr</tt><td>Auxiliary attribute bit-flags"
		        "<tr><td align=top><tt>netattr</tt><td>Network attribute bit-flags"
		        "<tr><td align=top><tt>when_written_time</tt><td>Date/time (in time_t format)"
		        "<tr><td align=top><tt>when_written_zone</tt><td>Time zone (in SMB format)"
		        "<tr><td align=top><tt>when_written_zone_offset</tt><td>Time zone in minutes east of UTC"
		        "<tr><td align=top><tt>when_imported_time</tt><td>Date/time message was imported"
		        "<tr><td align=top><tt>when_imported_zone</tt><td>Time zone (in SMB format)"
		        "<tr><td align=top><tt>when_imported_zone_offset</tt><td>Time zone in minutes east of UTC"
		        "<tr><td align=top><tt>thread_id</tt><td>Thread identifier (originating message number)"
		        "<tr><td align=top><tt>thread_back</tt><td>Message number that this message is a reply to"
		        "<tr><td align=top><tt>thread_next</tt><td>Message number of the next reply to the original message in this thread"
		        "<tr><td align=top><tt>thread_first</tt><td>Message number of the first reply to this message"
		        "<tr><td align=top><tt>votes</tt><td>Bit-field of votes if ballot, maximum allowed votes per ballot if poll"
		        "<tr><td align=top><tt>priority</tt><td>Priority value following the <i>X-Priority</i> email header scheme "
		        "(1 = highest, 3 = normal, 5 = lowest, 0 = unspecified)"
		        "<tr><td align=top><tt>delivery_attempts</tt><td>Number of failed delivery attempts (e.g. over SMTP)"
		        "<tr><td align=top><tt>field_list[].type</tt><td>Other SMB header fields (type)"
		        "<tr><td align=top><tt>field_list[].data</tt><td>Other SMB header fields (data)"
		        "<tr><td align=top><tt>can_read</tt><td>true if the current user can read this validated or unmoderated message"
		        "</table>"
		        "<br>"
		        "The optional <i>client</i> argument is an instance of the <i>Client</i> class to be used for the "
		        "security log header fields (e.g. sender IP address, hostname, protocol, and port). The "
		        "global client object will be used if this parameter is omitted."
		        "<br><br>"
		        "The optional <i>rcpt_list</i> is an array of objects that specifies multiple recipients "
		        "for a single message (e.g. bulk e-mail). Each recipient object in the array may include the following header properties "
		        "(described above): <br>"
		        "<i>to</i>, <i>to_ext</i>, <i>to_org</i>, <i>to_net_type</i>, <i>to_net_addr</i>, and <i>to_agent</i>"
		        )
	 , 312},
	{"save_msg_from_file", js_save_msg_from_file, 2, JSTYPE_BOOLEAN, JSDOCSTR("<i>object</i> header, <i>string</i> body_path [,<i>array</i> rcpt_list=<i>none</i>]")
	 , JSDOCSTR("Save a message to the message base, like <i>save_msg()</i>, with the body read natively from the file <i>body_path</i> "
		        "(the body never becomes a JavaScript string). The <i>header</i> and <i>rcpt_list</i> are as for <i>save_msg()</i>. "
		        "Returns <i>false</i> (see <i>last_error</i>) if the file is missing, unreadable or contains a NUL character."
		        "<br><br>"
		        "An entry of <i>rcpt_list</i> may include a <tt>netattr</tt> property, which replaces the header's network "
		        "attribute bit-flags for that recipient's copy only (also for <i>save_msg()</i>).")
	 , 322},
	{"copy_msg",        js_copy_msg,        3, JSTYPE_NUMBER,   JSDOCSTR("<i>number</i> msg_number, <i>MsgBase</i> dest [,<i>object</i> header_changes]")
	 , JSDOCSTR("Copy a message into another open message base, returning the number of the new message in <i>dest</i> "
		        "or <i>false</i> on failure (with <i>last_error</i> set on this message base).<br>"
		        "Every header field, the attributes and the body (as stored) are copied unchanged, except the message number, "
		        "the index record (built for the type of <i>dest</i>) and the thread links: "
		        "<i>thread_id</i> becomes the copy's own number (a new thread) and "
		        "<i>thread_back</i>, <i>thread_next</i> and <i>thread_first</i> are cleared; "
		        "the <i>id</i> and <i>reply_id</i> strings are kept.<br>"
		        "The optional <i>header_changes</i> object is applied to the copy before it is written; "
		        "the only supported properties are <i>tags</i> and <i>attr</i> (<tt>MSG_DELETE</tt> in <i>attr</i> "
		        "is not applied to the copy), and any other property fails the copy "
		        "(<i>false</i>, with <i>last_error</i> and <i>status</i> set).<br>"
		        "A message already marked for deletion is copied, and the copy is not marked for deletion.<br>"
		        "A message with attached files is not copied (use <i>move_msg()</i>).")
	 , 322},
	{"move_msg",        js_move_msg,        3, JSTYPE_NUMBER,   JSDOCSTR("<i>number</i> msg_number, <i>MsgBase</i> dest [,<i>object</i> header_changes]")
	 , JSDOCSTR("Copy a message into another open message base, as <i>copy_msg()</i> does, and mark the original for deletion, "
		        "returning the number of the new message in <i>dest</i> or <i>false</i> on failure "
		        "(with <i>last_error</i> set on this message base).<br>"
		        "If the original cannot be marked for deletion, the outcome depends on its state when read back: "
		        "still not marked, the copy is marked for deletion instead and <i>false</i> is returned; "
		        "marked despite the error, the copy is kept and the move succeeds; "
		        "unreadable, the copy is kept and <i>false</i> is returned, so a live duplicate can remain in "
		        "<i>dest</i> (<i>last_error</i> says so). "
		        "If another process (e.g. a concurrent move of the same message) marked the original for deletion "
		        "after it was read, the copy is marked for deletion and <i>false</i> is returned.<br>"
		        "Any attached files are left in place and passed to the copy: the original no longer refers to them.<br>"
		        "A message already marked for deletion, or a <i>dest</i> that is this message base, is refused.")
	 , 322},
	{"purge",           js_purge,           0, JSTYPE_NUMBER,   JSDOCSTR("")
	 , JSDOCSTR("Free the messages marked for deletion: their index records, headers and data "
		        "(data shared with other messages, e.g. saved with a <i>rcpt_list</i>, is kept for them). "
		        "Returns the number of messages purged (<tt>0</tt> when there was nothing to purge, so test for "
		        "failure with <tt>=== false</tt>) or <i>false</i> on failure (see <i>last_error</i>). "
		        "A failure after the index was rewritten leaves the purged messages out of the index "
		        "even though freeing their blocks failed (<i>last_error</i> says so). "
		        "The remaining messages keep their numbers and order. Attached files are not removed. "
		        "Unlike the daily e-mail maintenance, which expires old mail and so spares permanent messages, "
		        "a message marked for deletion is purged even if it is also marked permanent.<br>"
		        "Allowed only on a message base opened by path: the e-mail message base and the sub-boards "
		        "are maintained by the BBS and are refused, as is a file base.")
	 , 322},
	{"get_msg_parts",   js_get_msg_parts,   1, JSTYPE_ARRAY,    JSDOCSTR("<i>number</i> msg_number")
	 , JSDOCSTR("Return an array of the leaf MIME parts of a message, in document order, or <i>null</i> on failure "
		        "(see <i>last_error</i>). Each element is an object with the properties: "
		        "<i>id</i> (a dotted path, e.g. <tt>\"1\"</tt> or <tt>\"2.1\"</tt>), "
		        "<i>content_type</i> (lowercase, without parameters), <i>charset</i> (lowercase or empty), "
		        "<i>name</i> (file name, or empty), <i>content_id</i> (without angle brackets), "
		        "<i>disposition</i> (<tt>inline</tt>, <tt>attachment</tt> or empty) and "
		        "<i>size</i> (the decoded length, in bytes).<br>"
		        "A message that is not MIME-encoded is one <tt>text/plain</tt> part, whose <i>charset</i> is "
		        "<tt>utf-8</tt> for a UTF-8 message, else that of its FidoNet charset (CHRS) header field, else "
		        "<tt>cp437</tt> for a local, FidoNet or QWK message, else <tt>windows-1252</tt>.<br>"
		        "Multiparts nested more than 16 levels deep are one <tt>application/octet-stream</tt> part.")
	 , 322},
	{"save_msg_part",   js_save_msg_part,   3, JSTYPE_BOOLEAN,  JSDOCSTR("<i>number</i> msg_number, <i>string</i> part_id, <i>string</i> path")
	 , JSDOCSTR("Write the decoded content of a part (an <i>id</i> from <i>get_msg_parts()</i>) of a message "
		        "to the file <i>path</i>, natively (the content never becomes a JavaScript string). "
		        "Part id <tt>\"0\"</tt> writes the whole message body as stored (not decoded). "
		        "Returns <i>false</i> on failure (see <i>last_error</i>).")
	 , 322},
	{"vote_msg",        js_vote_msg,        1, JSTYPE_BOOLEAN,  JSDOCSTR("<i>object</i> header")
	 , JSDOCSTR("Create a new vote in message base.<p>The <i>header</i> object should contain the following properties:<br>"
		        "<table>"
		        "<tr><td align=top><tt>from</tt><td>Sender's name <i>(required)</i>"
		        "<tr><td align=top><tt>from_ext</tt><td>Sender's user number (if applicable)"
		        "<tr><td align=top><tt>from_net_type</tt><td>Sender's network type (default: 0 for local)"
		        "<tr><td align=top><tt>from_net_addr</tt><td>Sender's network address"
		        "<tr><td align=top><tt>reply_id</tt><td>The Reply-ID of the message being voted on (or specify thread_back)"
		        "<tr><td align=top><tt>thread_back</tt><td>Message number of the message being voted on"
		        "<tr><td align=top><tt>attr</tt><td>Should be either MSG_UPVOTE, MSG_DOWNVOTE, or MSG_VOTE (if answer to poll)"
		        "</table>"
		        )
	 , 317},
	{"add_poll",        js_add_poll,        1, JSTYPE_BOOLEAN,  JSDOCSTR("<i>object</i> header")
	 , JSDOCSTR("Create a new poll in message base.<p>The <i>header</i> object should contain the following properties:<br>"
		        "<table>"
		        "<tr><td align=top><tt>subject</tt><td>Polling question <i>(required)</i>"
		        "<tr><td align=top><tt>from</tt><td>Sender's name <i>(required)</i>"
		        "<tr><td align=top><tt>from_ext</tt><td>Sender's user number (if applicable)"
		        "<tr><td align=top><tt>from_net_type</tt><td>Sender's network type (default: 0 for local)"
		        "<tr><td align=top><tt>from_net_addr</tt><td>Sender's network address"
		        "</table>"
		        )
	 , 317},
	{"close_poll",      js_close_poll,      2, JSTYPE_BOOLEAN,  JSDOCSTR("message number, user name or alias")
	 , JSDOCSTR("Close an existing poll")
	 , 317},
	{"how_user_voted",      js_how_user_voted,      2, JSTYPE_NUMBER,   JSDOCSTR("message number, user name or alias")
	 , JSDOCSTR("Return 0 for no votes, 1 for an up-vote, 2 for a down-vote, or in the case of a poll-response: a bit-field of votes.")
	 , 317},
	{"dump_msg_header",     js_dump_msg_header,     1,  JSTYPE_ARRAY,   JSDOCSTR("<i>object</i> header")
	 , JSDOCSTR("Dump a message header object to an array of strings for diagnostic uses")
	 , 31702},
	{0}
};

static JSBool js_msgbase_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;

	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret = js_SyncResolve(cx, obj, name, js_msgbase_properties, js_msgbase_functions, NULL, 0);
	if (name)
		free(name);
	return ret;
}

static JSBool js_msgbase_enumerate(JSContext *cx, JSObject *obj)
{
	return js_msgbase_resolve(cx, obj, JSID_VOID);
}

JSClass js_msgbase_class = {
	"MsgBase"               /* name			*/
	, JSCLASS_HAS_PRIVATE    /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, js_msgbase_get         /* getProperty	*/
	, js_msgbase_set         /* setProperty	*/
	, js_msgbase_enumerate   /* enumerate	*/
	, js_msgbase_resolve     /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, js_finalize_msgbase    /* finalize		*/
};

/* MsgBase Constructor (open message base) */

static JSBool
js_msgbase_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj;
	jsval *    argv = JS_ARGV(cx, arglist);
	JSObject*  cfgobj;
	char       base[MAX_PATH + 1] = "";
	bool       is_path = false;
	private_t* p;
	scfg_t*    scfg;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	scfg = static_cast<scfg_t *>(JS_GetRuntimePrivate(JS_GetRuntime(cx)));

	obj = JS_NewObject(cx, &js_msgbase_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if ((p = (private_t*)malloc(sizeof(private_t))) == NULL) {
		JS_ReportError(cx, "malloc failed");
		return JS_FALSE;
	}

	memset(p, 0, sizeof(private_t));
	p->smb.retry_time = scfg->smb_retry_time;

	uintN argn = 0;
	JSVALUE_TO_STRBUF(cx, argv[argn], base, sizeof base, NULL);
	++argn;
	if (JS_IsExceptionPending(cx)) {
		free(p);
		return JS_FALSE;
	}
	if (*base == '\0') {
		JS_ReportError(cx, "Invalid 'path_or_code' parameter");
		free(p);
		return JS_FALSE;
	}

	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		is_path = JSVAL_TO_BOOLEAN(argv[argn]);
		++argn;
	}
	if (argn < argc && JSVAL_IS_BOOLEAN(argv[argn])) {
		p->is_email = is_path && JSVAL_TO_BOOLEAN(argv[argn]);
		++argn;
	}
	p->debug = JS_FALSE;

	if (!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx, "JS_SetPrivate failed");
		free(p);
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class used for accessing message bases", 310);
	js_DescribeSyncConstructor(cx, obj, "To create a new MsgBase object: "
	                           "<tt>var msgbase = new MsgBase(<i>string</i> code_or_path, [<i>bool</i> is_path=false], [<i>bool</i> is_email=false])</tt><br>"
	                           "where <i>code_or_path</i> is a sub-board internal code, or <tt>'mail'</tt> for the e-mail message base.<br>"
	                           "When <i>is_path</i> is <tt>true</tt>, the string parameter specifies the path to a message base (not an internal code) - new behavior in v3.21.<br>"
	                           "When <i>is_path</i> and <i>is_email</i> are both <tt>true</tt>, <tt>open()</tt> creates a new message base as an e-mail base "
	                           "(index records hold user numbers, duplicate messages are allowed) and fails if an existing message base is not an e-mail base "
	                           "- new behavior in v3.22."
	                           "<p>"
	                           "The MsgBase retrieval methods that accept a <tt>by_offset</tt> argument as their optional first boolean argument "
	                           "will interpret the following <i>number</i> argument as either a 1-based unique message number (by_offset=<tt>false</tt>) "
	                           "or a 0-based message index-offset (by_offset=<tt>true</tt>). Retrieving messages by offset is faster than by number or "
	                           "message-id (string). Passing an existing message <i>header object</i> to the retrieval methods that support it (e.g. <tt>get_msg_body()</tt>) "
	                           "is even faster. "
	                           );
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", msgbase_prop_desc, JSPROP_READONLY);
#endif

	if (is_path) {
		p->smb.subnum = scfg->total_subs; // Not INVALID_SUB as that indicates the mail base here
		SAFECOPY(p->smb.file, base);
	} else if (stricmp(base, "mail") == 0) {
		p->smb.subnum = INVALID_SUB;
		snprintf(p->smb.file, sizeof(p->smb.file), "%s%s", scfg->data_dir, "mail");
	} else if (subnum_is_valid(scfg, p->smb.subnum = getsubnum(scfg, base))) {
		cfgobj = JS_NewObject(cx, NULL, NULL, obj);

#ifdef BUILD_JSDOCS
		/* needed for property description alignment */
		JS_DefineProperty(cx, cfgobj, "index", JSVAL_VOID
		                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		JS_DefineProperty(cx, cfgobj, "grp_index", JSVAL_VOID
		                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
#endif

		js_CreateMsgAreaProperties(cx, scfg, cfgobj, p->smb.subnum);
#ifdef BUILD_JSDOCS
		js_DescribeSyncObject(cx, cfgobj
		                      , "Configuration parameters for this message area (<i>sub-boards only</i>) "
		                      "- <small>READ ONLY</small>"
		                      , 310);
#endif
		JS_DefineProperty(cx, obj, "cfg", OBJECT_TO_JSVAL(cfgobj)
		                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		snprintf(p->smb.file, sizeof(p->smb.file), "%s%s"
		         , scfg->sub[p->smb.subnum]->data_dir, scfg->sub[p->smb.subnum]->code);
	} else { /* unknown code */
		JS_ReportError(cx, "Unrecognized message area (sub-board) internal code: '%s'", base);
	}

	return JS_TRUE;
}

static struct JSPropertySpec js_msgbase_static_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{   "IndexPrototype", 0, JSPROP_ENUMERATE | JSPROP_PERMANENT, NULL, NULL},
	{   "HeaderPrototype", 0, JSPROP_ENUMERATE | JSPROP_PERMANENT, NULL, NULL},
	{0}
};

JSObject* js_CreateMsgBaseClass(JSContext* cx, JSObject* parent)
{
	JSObject* obj;
	JSObject* constructor;
	JSObject* pobj;
	jsval     val;

	obj = JS_InitClass(cx, parent, NULL
	                   , &js_msgbase_class
	                   , js_msgbase_constructor
	                   , 1 /* number of constructor args */
	                   , NULL /* js_msgbase_properties */
	                   , NULL /* js_msgbase_functions */
	                   , js_msgbase_static_properties, NULL);


	if (JS_GetProperty(cx, parent, js_msgbase_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx, val, &constructor);
		pobj = JS_DefineObject(cx, constructor, "IndexPrototype", NULL, NULL, JSPROP_PERMANENT | JSPROP_ENUMERATE);
#ifdef BUILD_JSDOCS
		if (pobj) {
			js_DescribeSyncObject(cx, pobj, "Prototype for all index objects.  Can be used to extend these objects.", 317);
		}
#endif
		pobj = JS_DefineObject(cx, constructor, "HeaderPrototype", NULL, NULL, JSPROP_PERMANENT | JSPROP_ENUMERATE);
#ifdef BUILD_JSDOCS
		if (pobj) {
			js_DescribeSyncObject(cx, pobj, "Prototype for all header objects.  Can be used to extend these objects.", 317);
		}
#endif
		(void)pobj;
	}

	return obj;
}


#endif  /* JAVSCRIPT */
