load("../web/lib/msgslib.ssjs");

template.title="Message Groups on " +system.name;
write_template("header.inc");
write_template("topnav.inc");
write_template("leftnav.inc");
template.groups=msg_area.grp_list;
write_template("msgs/groups.inc");
write_template("footer.inc");

msgs_done();
