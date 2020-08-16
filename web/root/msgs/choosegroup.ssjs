/* $Id: choosegroup.ssjs,v 1.4 2006/02/25 21:41:08 runemaster Exp $ */ 

load("../web/lib/msgslib.ssjs");

template.title="Message Groups on " +system.name;

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
	
template.groups=msg_area.grp_list;
	
write_template("msgs/choosegroup.inc");
if(do_footer)
	write_template("footer.inc");

msgs_done();
