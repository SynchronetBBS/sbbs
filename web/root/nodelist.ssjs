/* $Id$ */

load("../web/lib/template.ssjs");

template.title= system.name + " - Who's Online";

var sub='';

write_template("header.inc");
write_template("topnav.inc");
write_template("leftnav.inc");
write_template("nodelist.inc");
write_template("footer.inc");
