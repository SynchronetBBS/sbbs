/*  $Id: post.ssjs,v 1.16 2006/02/25 21:41:08 runemaster Exp $ */

/* 
 * ToDo:
 * Deal with 
 * var   SUB_FORCED        =(1<<13)        * Sub-board is forced scanning
 * var   SUB_NOTAG         =(1<<14)        * Don't add tag or origin lines
 * var   SUB_TOUSER        =(1<<15)        * Prompt for to user on posts
 * var   SUB_ASCII         =(1<<16)        * ASCII characters only
 * var   SUB_QUOTE         =(1<<17)        * Allow online quoting
 * var   SUB_KILL          =(1<<21)        * Kill read messages automatically
 * var   SUB_KILLP         =(1<<22)        * Kill read pvt messages automatically
 * var   SUB_EDIT          =(1<<28)        * Users can edit msg text after posting
 * var   SUB_EDITLAST      =(1<<29)        * Users can edit last message only
 * var   SUB_NOUSERSIG     =(1<<30)        * Suppress user signatures
 */

load("../web/lib/msgslib.ssjs");

template.anonnote='';
template.privnote='';

if(sub=='mail') {
	template.sub=new Object;
	template.sub.description="Personal E-Mail";
	template.sub.code="mail";
}
else {
	template.sub=msg_area.sub[sub];
	template.group=msg_area.grp[msg_area.sub[sub].grp_name];
	if(msg_area.sub[sub].settings&SUB_AONLY)
		template.anonnote=anon_only_message;
	else if(msg_area.sub[sub].settings&SUB_ANON)
		template.anonnote=anon_allowed_message;
	if(msg_area.sub[sub].settings&SUB_PONLY)
		template.privnote=private_only_message;
	else if(msg_area.sub[sub].settings&SUB_PRIV)
		template.privnote=private_allowed_message;
}

if(sub!='mail') {
	if(! msg_area.sub[sub].can_post)  {
		error("You don't have sufficient rights to post in this sub");
	}
}

if(sub=='mail') {
    template.can_post=!(user.security.restrictions&UFLAG_E);
    template.post_button="send_new_e-mail.gif";
} else {
    template.can_post=msg_area.sub[sub].can_post;
    template.post_button="post_new_message.gif";
}

template.title="Post a message in " + template.sub.description;
write_template("header.inc");
load(topnav_html);
load(leftnav_html);
write_template("msgs/post.inc");
write_template("footer.inc");

msgs_done();
