load("html_inc/template.ssjs");

template.title="Members Area";
template.ftplink="ftp://"+user.alias+":"+user.security.password+"@"+system.inet_addr+"/00index.html";
write_template("header.inc");
write_template("memberindex.inc");
write_template("footer.inc");
