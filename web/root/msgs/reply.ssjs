load("html_inc/msgslib.ssjs");

if(sub=='mail') {
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.sub=msg_area.sub[sub];
}

if(sub!='mail') {
	if(! msg_area.sub[sub].can_post)  {
		error("You don't have sufficient rights to post in this sub");
	}
}
if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}
hdr=clean_msg_headers(msgbase.get_msg_header(false,parseInt(http_request.query.reply_to)),CLEAN_MSG_REPLY);
template.subject=hdr.subject;
if(template.subject.search(/^re:\s+/i)==-1)
	template.subject='Re: '+template.subject;
if(sub=='mail') {
	if(hdr.replyto_net_addr!=undefined && hdr.replyto_net_addr != '')
		template.from=hdr.replyto_net_addr;
	else {
		if(hdr.from_net_addr != undefined && hdr.from_net_addr != '')
			template.from=hdr.from_net_addr;
		else
			template.from=hdr.from;
	}
}
else
	template.from=hdr.from;

template.number=hdr.number;

template.body=msgbase.get_msg_body(false,parseInt(http_request.query.reply_to),true);
if(this.word_wrap != undefined)  {
	template.body=quote_msg(word_wrap(template.body,79),79);
}
else  {
	template.body=template.body.replace(/^(.)/mg,"> $1");
}
title="Reply to message";
write_template("header.inc");
write_template("msgs/reply.inc");
write_template("footer.inc");

msgs_done();
