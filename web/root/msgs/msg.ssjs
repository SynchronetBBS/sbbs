load("html_inc/msgslib.ssjs");
load("html_inc/mime_decode.ssjs");

if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}

if(sub=='mail') {
	template.group=new Object;
	template.group.number=-1;
}
else {
	template.group=msg_area.grp_list[g];
}

if(sub=='mail') {
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.sub=msg_area.grp_list[g].sub_list[s];
}

template.hdr=msgbase.get_msg_header(false,m);
template.body=msgbase.get_msg_body(false,m,true,true);

msg=mime_decode(template.hdr,template.body);
template.body=msg.body;
if(msg.type=="plain") {
	/* ANSI */
	if(template.body.indexOf('\x1b[')>=0 || template.body.indexOf('\x01')>=0) {
		template.body=html_encode(template.body,true,false,true,true);
	}
	/* Plain text */
	else {
		template.body=word_wrap(template.body,79);
		template.body=html_encode(template.body,true,false,false,false);
	}
}
if(msg.attachments!=undefined) {
	template.attachments=new Object;
	for(att in msg.attachments) {
		template.attachments[att]=new Object;
		template.attachments[att].name=msg.attachments[att];
	}
}

if(template.hdr != null)  {
	template.title="Message: "+template.hdr.subject;
}
write_template("header.inc");
write_template("msgs/msg.inc");
write_template("footer.inc");
