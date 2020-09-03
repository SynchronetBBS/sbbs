/* $Id: nodelist.ssjs,v 1.8 2006/02/25 21:40:04 runemaster Exp $ */

load("../web/lib/template.ssjs");

template.title= system.name + " - Who's Online";

var sub='';

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("nodelist.inc");
if(do_footer)
	write_template("footer.inc");

