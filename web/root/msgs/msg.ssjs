/* $Id$ */

load("../web/lib/msgslib.ssjs");
load("../web/lib/mime_decode.ssjs");

template.txtbodybgwht=0;

if(msgbase.open!=undefined && msgbase.open()==false) {
    error(msgbase.last_error);
}

template.can_delete=can_delete(m);

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
    if(!msg_area.sub[sub].can_read)
        error("You can't read messages in this sub!");
}

if(sub=='mail')
    template.can_post=!(user.security.restrictions&UFLAG_E);
else
    template.can_post=msg_area.sub[sub].can_post;

template.idx=msgbase.get_msg_index(false,m);
if(sub=='mail' && template.idx.to!=user.number)
    error("You can only read e-mail messages addressed to yourself!");
template.hdr=msgbase.get_msg_header(false,m);
if(idx_to_user(template.idx)) {
    template.hdr.attr|=MSG_READ;
    if(template.hdr.attr&MSG_KILLREAD)
        template.hdr.attr|=MSG_DELETE;
    msgbase.put_msg_header(false,m,template.hdr);
}
template.hdr=clean_msg_headers(template.hdr,0);
template.body=msgbase.get_msg_body(false,m);

msg=mime_decode(template.hdr,template.body);
template.body=msg.body;
if(msg.type=="plain") {
    /* ANSI */
    if(template.body.indexOf('\x1b[')>=0 || template.body.indexOf('\x01')>=0) {
        template.body=html_encode(template.body,true,false,true,true);
    }
    /* Plain text */
    else {
        template.body=word_wrap(template.body,80);
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

    if(sub=='mail' || user.security.level>=90) {    /* Sysops can dump all message headers */
        template.hfields="<html><head><title>Message Header Fields</title></head>";
        template.hfields+="<body>";
        template.hfields+="<h2>Message Header Fields</h2>";
        template.hfields+="<table>";
        var f;
        for(f in template.hdr)
            if(typeof(template.hdr[f])!="object")
                template.hfields+=('<tr valign=top><td>'+ f + ':<td>' + template.hdr[f]);
        for(f in template.hdr.field_list)
            template.hfields+=('<tr valign=top><td>type-0x'
                + format("%02X",template.hdr.field_list[f].type) + ':<td>'
                + strip_ctrl(template.hdr.field_list[f].data));

        template.hfields+="</table>";
        template.hfields+="</body>";
        template.hfields+="</html>";
    }
}

if(msg.type!="plain" && sub=="mail")
  template.txtbodybgwht=1;

var tmp=find_np_message(template.idx.offset,true);
template.temp=tmp
template.replyto=undefined;
if(template.hdr.thread_orig!=0) {
    template.replyto=msgbase.get_msg_header(false,template.hdr.thread_orig);
    if(template.replyto==null)
        template.replyto=undefined;
}
template.replies=new Array;
if(template.hdr.thread_first!=0) {
    /* Fill replies array */
    var next_reply;
    var rhdr=new Object;
    rhdr.thread_next=template.hdr.thread_first;
    for(;(next_reply=rhdr.thread_next)!=0 && (rhdr=msgbase.get_msg_header(false,next_reply))!=null;) {
        if(rhdr==null)
            break;
        template.replies.push(rhdr);
    }
}
if(tmp!=undefined)
    template.prevlink='<a href="msg.ssjs?msg_sub='+sub+'&amp;message='+tmp+'">'+prev_msg_html+'</a>';
else
    template.prevlink=no_prev_msg_html;
tmp=find_np_message(template.idx.offset,false);
if(tmp!=undefined)
    template.nextlink='<a href="msg.ssjs?msg_sub='+sub+'&amp;message='+tmp+'">'+next_msg_html+'</a>';
else
    template.nextlink=no_next_msg_html;

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");
write_template("msgs/msg.inc");
write_template("footer.inc");

msgs_done();
