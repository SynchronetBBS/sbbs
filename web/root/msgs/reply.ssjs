load("html_inc/msgslib.ssjs");

template.group=msg_area.grp_list[g];

if(sub=='mail') {
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.sub=msg_area.grp_list[g].sub_list[s];
}

if(sub!='mail') {
	if(! msg_area.grp_list[g].sub_list[s].can_post)  {
		error("You don't have sufficient rights to post in this sub");
	}
}
if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}
hdr=msgbase.get_msg_header(false,parseInt(http_request.query.reply_to));
template.subject=hdr.subject;
if(template.subject.search(/^re:\s+/i)==-1)
	template.subject='Re: '+template.subject;
template.from=hdr.from;
template.number=hdr.number;

template.body=msgbase.get_msg_body(false,parseInt(http_request.query.reply_to),true,true);
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
