load("html_inc/msgslib.ssjs");

title="Message Groups";
write_template("header.inc");
template.groups=msg_area.grp_list;
write_template("msgs/groups.inc");
write_template("footer.inc");
