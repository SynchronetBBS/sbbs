/* $Id: msg.ssjs,v 1.55 2019/07/24 09:30:23 rswindell Exp $ */

load("../web/lib/msgslib.ssjs");
load("../web/lib/mime_decode.ssjs");

template.txtbodybgwht=0;
template.author_avatar = '';


/*  If you want to disable display of certain .inc files  */
/* for a specific theme declare it like this in the .ssjs */
/* file corresponding to the page you want to overide the */
/*           default display type for the theme			  */

if(do_extra) {
	do_leftnav=false;
	do_rightnav=false;
}

if(msgbase.open!=undefined && msgbase.open()==false) {
    error(msgbase.last_error);
}

/* Check author info if message base is local and display it */
/*    on the new expanded message read page for new theme    */

var hdr=msgbase.get_msg_header(false,m);
if(hdr==null)
	error(msgbase.last_error);
if(hdr.attr&MSG_DELETE)
	error("Deleted message");
if((!(system.settings & SYS_USRVDELM)) || (user.security.level >= 90 && (!(system.settings & SYS_SYSVDELM))) ) {
	if(hdr.attr & MSG_DELETE)
		error("Message has been deleted");
}
if(hdr.attr & MSG_MODERATED) {
	if(!(hdr.attr & MSG_VALIDATED))
		error("Message pending moderator validation");
}
if(Number(hdr.from_ext) > 0) {
	template.u_num = hdr.from_ext;
	usr = new User(template.u_num);
	template.author_firston = strftime("%m/%d/%y",usr.stats.firston_date);
	template.author_posts = usr.stats.total_posts;
	if(sub!='mail') {
	if((user.compare_ars(msg_area.sub[sub].operator_ars) && msg_area.sub[sub].operator_ars != '' || user.number==1) && show_ip==true) {
		template.author_ip='IP: ' + usr.note + '<br /><br />';
	}
	template.author_ismod = '<br />Member<br /><br />';
	if(usr.compare_ars(msg_area.sub[sub].operator_ars) && msg_area.sub[sub].operator_ars != '' || usr.number==1)
		template.author_ismod = '<br />Moderator<br /><br />';
	if(file_exists(prefs_dir + format("%04d.html_prefs",usr.number))); {
		prefsfile=new File(prefs_dir + format("%04d.html_prefs",usr.number));
		if(prefsfile.open("r",false)) {
			if(prefsfile.iniGetValue('Profile', 'Avatar', '')!='') {
				template.author_avatar=prefsfile.iniGetValue('Profile', 'Avatar', '');
				var display_info=true;
			} else
				template.author_avatar = template.image_dir + "/nothumbnail.jpg";
				prefsfile.close();
			}
		}
	}
}

template.can_delete=can_delete(m);

if(sub=='mail') {
    template.group=new Object;
    template.group.name="E-Mail";
} else {
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

max_display=Themes[CurrTheme].msgs_displayed + m;
start_num = m;

// for(i = start_num; i <= max_display ; i++) {

template.idx=msgbase.get_msg_index(false,m);
if(sub=='mail' && template.idx.to!=user.number)
    error("You can only read e-mail messages addressed to yourself!");
template.hdr=msgbase.get_msg_header(false,m, /* expand_fields: */false);
if(idx_to_user(template.idx)) {
    template.hdr.attr|=MSG_READ;
    if(template.hdr.attr&MSG_KILLREAD)
        template.hdr.attr|=MSG_DELETE;
    msgbase.put_msg_header(false,m,template.hdr);
}
template.hdr=clean_msg_headers(template.hdr,0);
template.body=msgbase.get_msg_body(false,m,template.hdr);

msg=mime_decode(template.hdr,template.body);
template.body=msg.body;
if(msg.type=="plain") {
    /* ANSI */
    if(template.body.indexOf('\x1b[')>=0 || template.body.indexOf('\x01')>=0) {
        template.body=html_encode(template.body, !template.hdr.is_utf8, false,true,true);
		template.body=make_links(template.body);
    }
    /* Plain text */
    else {
        template.body=word_wrap(template.body,80);
        template.body=html_encode(template.body, !template.hdr.is_utf8, false,false,false);
		template.body=make_links(template.body);
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
    if(Themes[CurrTheme].do_forumlook==true && sub!='mail')
		template.title="Reading Messages in "+template.group.description + " -> " + template.sub.description;
	else
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

// m--;

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
	load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("msgs/msg.inc");

// }

if(do_footer)
	write_template("footer.inc");

msgs_done();
