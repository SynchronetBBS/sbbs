load("../web/lib/template.ssjs");
load("sbbsdefs.js");

template.title=system.name+" Home Page";
write_template("header.inc");
writeln('<br>');
writeln('<table class="main" cellspacing="2" cellpadding="2">');
writeln('<tbody>');
writeln('<tr>');
writeln('<td class="main">');
writeln('<a href="nodelist.ssjs">Who Is Online</a><br />');
if(user.number==0 || user.security.restrictions&UFLAG_G) {
	writeln('<a href="newuser.ssjs">Sign up as a New User</a><br />');
	writeln('<a href="members/login.html">Login with an Existing User Account</a><br />');
} else {
	writeln('<a href="members/userlist.ssjs">User Listing</a><br />');
	writeln('<a href="members/info.ssjs">Information Menu</a><br />');
	writeln('<a href="members/themes.ssjs">Change Your HTML Theme</a><br />');
}
if(user.number || (this.login!=undefined && system.matchuser("Guest")))
    writeln('<a href="msgs">Message Groups</a><br />');
// FTP link
if(user.number || system.matchuser("Guest")) {
    write('<a href=ftp://');
    if(user.number && !(user.security.restrictions&UFLAG_G))
	    write(escape(user.alias) + ':' + escape(user.security.password) + '@');
    writeln(http_request.host + '/00index.html>File Libraries</a>');
    writeln('<br />');
}
writeln('</td>');
writeln('</tr>');
writeln('</tbody>');
writeln('</table>');
write_template("footer.inc");
