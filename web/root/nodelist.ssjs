/* $Id$ */

load("../web/lib/template.ssjs");

template.title= system.name + " - Who's Online";

var sub='';

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");
write_template("nodelist.inc");
write_template("footer.inc");
