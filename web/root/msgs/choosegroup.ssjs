/* $Id$ */ 

load("../web/lib/msgslib.ssjs");

template.title="Message Groups on " +system.name;
write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");
template.groups=msg_area.grp_list;
write_template("msgs/choosegroup.inc");
write_template("footer.inc");

msgs_done();
