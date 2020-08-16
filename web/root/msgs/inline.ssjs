/* $Id: inline.ssjs,v 1.8 2011/11/09 09:38:00 deuce Exp $ */

/* 
 * Inline attachment FS emulator
 * Request attachments in the form:
 * attachments.ssjs/sub/messageID/Content-ID
 */

load("../web/lib/template.ssjs");
load("../web/lib/mime_decode.ssjs");

var path=http_request.path_info.split(/[\\\/]/);
if(path==undefined) {
	error("No path info!");
}
var sub=path[1];
var id=parseInt(path[2]);
var cid=path[3];

if(sub==undefined || cid==undefined) {
    error("Invalid path: " + http_request.path_info);
}

var msgbase = new MsgBase(sub);
if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}

hdr=msgbase.get_msg_header(false,id);
if(hdr==undefined) {
	error("No such message!");
}
body=msgbase.get_msg_body(false,id,hdr);
if(body==undefined) {
	error("Can not read message body!");
}

att=mime_get_cid_attach(hdr,body,cid);
if(att!=undefined) {
	if(att.content_type != undefined) {
		http_reply.header["Content-Type"]=att.content_type;
	}
	write(att.body);
}
