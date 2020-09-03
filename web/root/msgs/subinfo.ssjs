/* $Id: subinfo.ssjs,v 1.18 2006/02/25 21:41:08 runemaster Exp $ */

load("sbbsdefs.js");
load("../web/lib/msgslib.ssjs");

template.title="Detailed Info on Sub"

if(sub=='mail') {
	template.type="Internet";
	template.sub=new Object;
	template.sub.description="Internet E-Mail";
	template.sub.newsgroup="<NONE>";
	template.sub.name="MAIL";
	template.sub.qwk_name="<NONE>";
	template.sub.is_moderated=false;
	template.sub.is_operator=true;
	template.sub.can_post=true;
	template.sub.can_read=true;
	template.sub.max_msgs="Unknown";
}
else {
	template.sub=msg_area.sub[sub];
	if(template.sub.settings & SUB_FIDO) {
		template.type="FidoNet";
		template.tagline=template.sub.fidonet_origin;
	}
	else if(template.sub.settings & SUB_QNET) {
		template.type="QWK";
		template.tagline=ascii_str(strip_ctrl(template.sub.qwknet_tagline));
	}
	else if(template.sub.settings & SUB_PNET) {
		template.type="PostLink";
		template.tagline="Unknown";
	}
	else {
		template.type="Local";
		template.tagline="";
	}
}

template.backurl=http_request.header.referer;

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
	load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("msgs/subinfo.inc");
if(do_footer)
write_template("footer.inc");

msgs_done();
