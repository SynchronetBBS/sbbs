load("html_inc/msgslib.ssjs");

title="Message Subs in "+msg_area.grp_list[g].description;

write_template("header.inc");

template.group=msg_area.grp_list[g];
template.subs=msg_area.grp_list[g].sub_list;
write_template("msgs/subs.inc");
write_template("footer.inc");
