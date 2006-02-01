/* $Id$ */

var sub="";
load("sbbsdefs.js");
load("../web/lib/template.ssjs");

template.pwchangedate="Never"

template.title = "Change User Password";

if(user.security.password_date!=0)
    template.pwchangedate=strftime("%A, %B %d, %Y." ,user.security.password_date);

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load("../web/lib/topnav_html.ssjs");
if(do_leftnav)
load("../web/lib/leftnav_html.ssjs");
if(do_rightnav)
	write_template("rightnav.inc");
write_template("newpw.inc");
if(do_footer)
	write_template("footer.inc");

