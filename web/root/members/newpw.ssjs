/* $Id$ */

var sub="";

load("sbbsdefs.js");
load("../web/lib/template.ssjs");

template.title = "Change User Password";

template.pwchangedate=strftime("%A, %B %d, %Y." ,user.security.password_date);

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
write_template("leftnav.inc");
write_template("newpw.inc");
write_template("footer.inc");