load("../web/lib/template.ssjs");

var sub="";

template.title=system.name+ " Information Menu";
write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
write_template("leftnav.inc");
write_template("infomenu.inc");
write_template("footer.inc");
