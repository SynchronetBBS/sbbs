/* $Id: asc_handler.js,v 1.8 2011/07/20 04:30:03 deuce Exp $ */

// This module converts ANSI, Ex-ASCII, and Ctrl-A encoded files to HTML

// The filename to encode may be passed on the command-line (e.g. running
// this module using jsexec) or as an http-requested document (e.g. running
// this module as a "web handler").

// This module can be used as a "web handler" (automatically converting
// *.asc and *.ans files on the fly), by adding the following lines to
// the [JavaScript] section of your ctrl/web_handler.ini file:
// asc = asc_handler.js
// ans = asc_handler.js

var filename;

if(this.http_request!=undefined)	/* Requested through web-server */
	filename = http_request.real_path;
else
	filename = argv[0];

var file = new File(filename);
if(!file.open("r",true,8192)) {
	writeln("!ERROR " + file.error + " opening " + filename);
	exit();
}
var text = file.readAll(8192);
file.close();
writeln('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">');
writeln("<html>");
writeln("<head>");
writeln("<meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>");
writeln("<title>"+file.name.replace(/^.*[\/\\]/,'')+"</title>");
writeln("</head>");
writeln('<body style="background-color: black;">');
writeln('<pre style="font-family: Courier New, monospace">');
write(html_encode(text.join("\r\n")
	,/* es-ASCII: */true
	,/* white-sp: */false
	,/* ANSI:     */true
	,/* Ctrl-A:   */true));
writeln("</pre>");
writeln("</body>");
writeln("</html>");
