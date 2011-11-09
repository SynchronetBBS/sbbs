/*
 * Message FS emulator
 * Request messages in the form:
 * msgframe.ssjs/sub/messageID/filename
 */

load("../web/lib/template.ssjs");
load("../web/lib/mime_decode.ssjs");

var path=http_request.path_info.split(/[\\\/]/);
if(path==undefined) {
        error("No path info!");
}
var sub=path[1];
var id=parseInt(path[2]);

if(sub==undefined) {
    error("Invalid path: " + http_request.path_info);
}

var msgbase = new MsgBase(sub);

if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}

if(sub=='mail') {
	template.group=new Object;
	template.group.name="E-Mail";
}
else {
	template.group=msg_area.grp[msg_area.sub[sub].grp_name];
}

if(sub=='mail') {
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.sub=msg_area.sub[sub];
}

template.hdr=msgbase.get_msg_header(false,id);
template.body=msgbase.get_msg_body(false,id,template.hdr);

msg=mime_decode(template.hdr,template.body);
template.body=msg.body;
if(msg.type=="plain") {
	/* ANSI */
	if(template.body.indexOf('\x1b[')>=0 || template.body.indexOf('\x01')>=0) {
		template.body=html_encode(template.body,true,false,true,true);
	}
	/* Plain text */
	else {
		template.body=word_wrap(template.body);
		template.body=html_encode(template.body,true,false,false,false);
	}
	if(template.hdr != null)  {
		template.title="Message: "+template.hdr.subject;
	}
	write_template("msgs/textmsg.inc");
}
else {
	write(template.body);
}

