load("html_inc/msgslib.ssjs");

template.group=msg_area.grp[msg_area.sub[sub].grp_name];

if(sub=='mail') {
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.sub=msg_area.sub[sub];
}

var hdrs = new Object;
if(sub!='mail')  {
	if(! msg_area.sub[sub].can_post)  {
		error("You don't have sufficient rights to post in this sub");
	}
}
else {
	hdrs.to_net_type=netaddr_type(http_request.query.to);
	if(hdrs.to_net_type!=NET_NONE)
		hdrs.to_net_addr=http_request.query.to;
}
hdrs.from=user.alias;
hdrs.to=http_request.query.to;
hdrs.subject=http_request.query.subject;
if(http_request.query.reply_to != undefined)  {
	hdrs.thread_orig=parseInt(http_request.query.reply_to);
}
if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}
if(!msgbase.save_msg(hdrs,http_request.query.body))  {
	error(msgbase.last_error);
}
http_reply.status="201 Created";
title="Message posted";
write_template("header.inc");
write_template("msgs/posted.inc");
write_template("footer.inc");

msgs_done();
