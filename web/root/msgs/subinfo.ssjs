load("sbbsdefs.js");
load("html_inc/msgslib.ssjs");

template.sub=msg_area.grp_list[g].sub_list[s];
if(template.sub.settings & SUB_FIDO) {
	template.type="FidoNet";
	template.tagline=template.sub.fidonet_origin;
}
else if(template.sub.settings & SUB_QNET) {
	template.type="QWK";
	template.tagline=template.sub.qwknet_tagline;
}
else if(template.sub.settings & SUB_PNET) {
	template.type="PostLink";
	template.tagline="Unknown";
}
else {
	template.type="Local";
	template.tagline="";
}

write_template("header.inc");
write_template("msgs/subinfo.inc");
write_template("footer.inc");
