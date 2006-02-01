/* $Id$ */

load("../web/lib/template.ssjs");

var sub="";

if(CurrTheme=="NightShade")
	do_rightnav=false;

if(user.number!=0) {
    var file = new File(system.data_dir + "logon.lst");
        if(file.open("r")) {
            template.lastcallers=file.readAll();
        file.close();
        }
    template.lastcallers.shift();
    template.lastcallers=html_encode(template.lastcallers.join("\r\n"),true,false,true,true);
}

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load("../web/lib/topnav_html.ssjs");
if(do_leftnav)
load("../web/lib/leftnav_html.ssjs");
if(do_rightnav)
	write_template("rightnav.inc");
write_template("lastcallers.inc");
if(do_footer)
	write_template("footer.inc");
