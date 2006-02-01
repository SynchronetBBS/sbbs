/* $Id$ */

load("../web/lib/template.ssjs");

var sub="";

template.title=system.name+ " Information Menu";

template.user_num=user.number;

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load("../web/lib/topnav_html.ssjs");
if(do_leftnav)
load("../web/lib/leftnav_html.ssjs");
if(do_rightnav)
	write_template("rightnav.inc");
write_template("infomenu.inc");
if(do_footer)
	write_template("footer.inc");
