load("template.ssjs");

template.title="Members Area";
template.ftplink="ftp://"+user.alias+":"+user.password+"@"+system.inet_addr;
write_template("header.inc");
write_template("memberindex.inc");
write_template("footer.inc");
