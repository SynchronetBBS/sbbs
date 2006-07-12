/* $Id$ */

var file = new File(http_request.real_path);
if(!file.open("r")) {
	writeln("!ERROR " + file.error + " opening " + http_request.real_path);
	exit();
}
var text = file.readAll();
file.close();
writeln('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">');
writeln("<html>");
writeln("<head>");
writeln("<meta http-equiv='Content-Type' content='text/html; charset=IBM437'>");
writeln("</head>");
writeln("<body bgcolor=black>");
writeln("<pre>");
writeln("<font face='monospace'>");
write(html_encode(text.join("\r\n")
	,/* es-ASCII: */true
	,/* white-sp: */false
	,/* ANSI:     */true
	,/* Ctrl-A:   */true));
writeln("</pre>");
writeln("</body>");
writeln("</html>");