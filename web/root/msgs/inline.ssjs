/* 
 * Inline attachment FS emulator
 * Request attachments in the form:
 * attachments.ssjs/group/sub/messageID/Content-ID
 */

load("html_inc/template.ssjs");
load("html_inc/mime_decode.ssjs");

var path=http_request.path_info.split(/\//);
if(path==undefined) {
	error("No path info!");
}
var group=parseInt(path[1]);
var sub=path[2];
var id=parseInt(path[3]);
var cid=path[4];

var msgbase = new MsgBase(sub);
if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}

hdr=msgbase.get_msg_header(false,id);
if(hdr==undefined) {
	error("No such message!");
}
body=msgbase.get_msg_body(false,id,true,true);
if(path==undefined) {
	error("Can not read message body!");
}

att=mime_get_cid_attach(hdr,body,cid);
if(att!=undefined) {
	if(att.content_type != undefined) {
		http_reply.header["Content-Type"]=att.content_type;
	}
	write(att.body);
}
