/* $Id: info.ssjs,v 1.9 2006/02/25 21:40:35 runemaster Exp $ */

load("../web/lib/template.ssjs");

var sub="";

template.title=system.name+ " Information Menu";

template.user_num=user.number;

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("infomenu.inc");
if(do_footer)
	write_template("footer.inc");
