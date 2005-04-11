/* $Id$ */

load("../web/lib/template.ssjs");

var sub="";

if(user.number!=0) {
    var file = new File(system.data_dir + "logon.lst");
        if(file.open("r")) {
            template.lastcallers=file.readAll();
        file.close();
        }
    template.lastcallers.shift();
    template.lastcallers=html_encode(template.lastcallers.join("\r\n"),true,false,true,true);
}

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");
write_template("lastcallers.inc");
write_template("footer.inc");
