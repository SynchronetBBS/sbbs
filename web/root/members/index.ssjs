load("html_inc/template.ssjs");

template.title="Welcome to the Members Area on " +system.name;
template.user_alias=(user.alias);
template.user_lastlogon=(strftime("%A, %d %B, %Y at %r",user.stats.laston_date));
template.ftplink="ftp://"+user.alias+":"+user.security.password+"@"+system.inet_addr+"/00index.html";
write_template("header.inc");
write_template("memberindex.inc");
write_template("footer.inc");
