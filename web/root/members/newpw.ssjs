/* $Id$ */

var sub="";
load("sbbsdefs.js");
load("../web/lib/template.ssjs");

template.pwchangedate="Never"

template.title = "Change User Password";

if(user.security.password_date!=0)
    template.pwchangedate=strftime("%A, %B %d, %Y." ,user.security.password_date);

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");
write_template("newpw.inc");
write_template("footer.inc");