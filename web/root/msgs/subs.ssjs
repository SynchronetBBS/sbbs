load("html_inc/msgslib.ssjs");

title="Message Subs in "+msg_area.grp[grp].description;

write_template("header.inc");

template.group=msg_area.grp[grp];
template.subs=msg_area.grp[grp].sub_list;
write_template("msgs/subs.inc");
write_template("footer.inc");

msgs_done();
