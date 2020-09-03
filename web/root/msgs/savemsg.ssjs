/* $Id: savemsg.ssjs,v 1.35 2018/10/06 21:38:09 rswindell Exp $ */

load("../web/lib/msgslib.ssjs");

if(sub==undefined)
	error("'sub' not defined");
else if(sub=='mail') {
	template.group=new Object;
	template.group.name="E-Mail";
	template.group.description="E-Mail";
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.group=msg_area.grp[msg_area.sub[sub].grp_name];
	template.sub=msg_area.sub[sub];
}

var to = http_request.query.to[0];
var hdrs = new Object;
if(sub!='mail')  {
	if(! msg_area.sub[sub].can_post)  {
		error("You don't have sufficient rights to post in this sub");
	}
}
else {
	hdrs.to_net_type = NET_NONE;
    var at = to.indexOf('@');
    if(at > 0)
		hdrs.to_net_type=netaddr_type(to);
	if(hdrs.to_net_type!=NET_NONE) {
		if(user.security.restrictions&UFLAG_M)
			error("You do not have permission to send netmail");
        if(hdrs.to_net_type!=NET_INTERNET && at > 0) {
            hdrs.to_net_addr = to.slice(at + 1);
            to = to.slice(0, at);
        } else
            hdrs.to_net_addr = to;
	} else {
		var usr=system.matchuser(to);
		if(usr!=0)
			hdrs.to_ext=usr;
		else
			error("Cannot find that local user (no network address specified)");
	}
}

var body=http_request.query.body[0];
body=body.replace(/([^\r])\n/g,"$1\r\n");
body=word_wrap(body);

hdrs.from=user.alias;
hdrs.from_ext=user.number;
hdrs.to=to;
hdrs.subject=http_request.query.subject;
if(http_request.query.reply_to != undefined) {
	hdrs.thread_orig=parseInt(http_request.query.reply_to);
}
if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}

if(sub != 'mail') {
	/* Anonymous/Real Name/etc stuff */
	if(msgbase.cfg.settings&SUB_AONLY || (msgbase.cfg.settings&SUB_ANON && http_request.query.anonymous != undefined && http_request.query.anonymous[0]=='Yes'))
		hdrs.attr|=MSG_ANONYMOUS;
	if(msgbase.cfg.settings&SUB_NAME)
		hdrs.from=user.name;

	/* Private message stuff */
	/* Note, apparently, "private" is a magical property... */
	if(msgbase.cfg.settings&SUB_PONLY || (msgbase.cfg.settings&SUB_PRIV && http_request.query['private']!=undefined && http_request.query['private'][0]=='Yes'))
		hdrs.attr|=MSG_PRIVATE;

	/* Moderated stuff */
	if(msg_area.sub[sub].is_moderated)
		hdrs.attr|=MSG_MODERATED;
}

/* Set kill when read flag */
if(sub=="mail" && hdrs.to_net_type==NET_NONE && system.settings&SYS_DELREADM)
	hdrs.attr|=MSG_KILLREAD;
if(sub != 'mail') {
    if(msgbase.cfg.settings&SUB_KILL)
	   hdrs.attr|=MSG_KILLREAD;
    if(msgbase.cfg.settings&SUB_KILLP && hdrs.attr&MSG_PRIVATE)
	   hdrs.attr|=MSG_KILLREAD;
}

/* Sig stuff */
if(sub=='mail' || (!(msgbase.cfg.settings&SUB_NOUSERSIG) && !(hdrs.attr&MSG_ANONYMOUS))) {
   sigfile=new File(format("%suser/%04u.sig",system.data_dir,user.number));
   if(sigfile.exists) {
	  sigfile.open("r",true);
	  if(body.search(/\n$/)==-1)
		 body+='\r\n';
	  var sigf=sigfile.readAll();
	  body=body+sigf.join('\r\n');
   }
}

if(!msgbase.save_msg(hdrs,client,body)) {
	error(msgbase.last_error);
} else {
	if(sub=='mail') {
		if(user.sent_email!=undefined)
			user.sent_email();
	}
	else if(user.posted_message!=undefined)
		user.posted_message();
} 

/* Mark original message for replied */
if(hdrs.thread_orig!=undefined)  {
	var orig_idx=msgbase.get_msg_index(false,hdrs.thread_orig);
	if(idx_to_user(orig_idx)) {
		var orig_hdrs=msgbase.get_msg_header(false,hdrs.thread_orig, /* expand_fields: */false);
		orig_hdrs.attr|=MSG_REPLIED;
		msgbase.put_msg_header(false,hdrs.thread_orig,orig_hdrs);
	}
}

http_reply.status="201 Created";
title="Message posted";

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("msgs/posted.inc");
if(do_footer)
	write_template("footer.inc");

msgs_done();
