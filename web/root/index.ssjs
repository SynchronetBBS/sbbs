load("../web/lib/template.ssjs");

template.title=system.name+" Home Page";
write_template("header.inc");
writeln('<p class="navigation">Home</p>');
writeln('<table class="main" cellspacing="2" cellpadding="2">');
writeln('<tbody>');
writeln('<tr>');
writeln('<td class="main">');
writeln('<a href="nodelist.ssjs">See who is currently online</a><br />');
writeln('<a href="newuser.ssjs">Sign up as a new user</a><br />');
writeln('<a href="login/">Login as an existing user</a><br />');
writeln('<a href="msgs">Access message groups</a><br /><br />');
writeln('</td>');
writeln('</tr>');
writeln('</tbody>');
writeln('</table>');
write_template("footer.inc");
