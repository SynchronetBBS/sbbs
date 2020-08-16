/* $Id: reply.ssjs,v 1.19 2018/08/20 13:46:33 rswindell Exp $ */

load("../web/lib/msgslib.ssjs");

if(sub=='mail') {
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.sub=msg_area.sub[sub];
	template.group=msg_area.grp[msg_area.sub[sub].grp_name];
}

if(sub!='mail') {
	if(! msg_area.sub[sub].can_post)  {
		error("You don't have sufficient rights to post in this sub");
	}
}
if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}
hdr=msgbase.get_msg_header(false,parseInt(http_request.query.reply_to));
hdr=clean_msg_headers(hdr,CLEAN_MSG_REPLY);
if(sub!='mail') {
	if(msg_area.sub[sub].settings&SUB_AONLY)
		template.anonnote=anon_only_message;
	else if(msg_area.sub[sub].settings&SUB_ANON) {
		if(hdr.attr&MSG_ANONYMOUS)
			template.anonnote=anon_reply_message;
		else
			template.anonnote=anon_allowed_message;
	}
	if(msg_area.sub[sub].settings&SUB_PONLY)
		template.privnote=private_only_message;
	else if(msg_area.sub[sub].settings&SUB_PRIV) {
		if(hdr.attr&MSG_PRIVATE)
			template.privnote=private_reply_message;
		else
			template.privnote=private_allowed_message;
	}
}
template.subject=hdr.subject;
if(template.subject.search(/^re:\s+/i)==-1)
	template.subject='Re: '+template.subject;
if(sub=='mail') {
	if(hdr.replyto_net_addr!=undefined && hdr.replyto_net_addr != '') {
		template.from=hdr.replyto_net_addr;
        if(template.from.indexOf('@') < 0)
            template.from=hdr.replyto+'@'+hdr.replyto_net_addr;
	} else {
		if(hdr.from_net_addr != undefined && hdr.from_net_addr != '') {
			template.from=hdr.from_net_addr;
            if(template.from.indexOf('@') < 0)
                template.from=hdr.from+'@'+hdr.from_net_addr;
		} else
			template.from=hdr.from;
	}
}
else
	template.from=hdr.from;

template.number=hdr.number;

template.body=msgbase.get_msg_body(false,parseInt(http_request.query.reply_to),true);
if(this.word_wrap != undefined)  {
	// quote_msg adds three chars to each line.  Re-wrap to 76 chars...
	// with the extra three, we're still under 80 *.
	template.body=quote_msg(word_wrap(template.body,76),79);
}
else  {
	template.body=template.body.replace(/^(.)/mg,"> $1");
}

if(sub=='mail') {
    template.can_post=!(user.security.restrictions&UFLAG_E);
    template.reply_button="send_reply.gif";
} else {
    template.can_post=msg_area.sub[sub].can_post;
    template.reply_button="post_reply.gif";
}

title="Reply to message";

if(do_header)
    write_template("header.inc");
if(do_topnav)
    load(topnav_html);
if(do_leftnav)
    load(leftnav_html);
write_template("msgs/reply.inc");
if(do_footer)
    write_template("footer.inc");

msgs_done();
