/* $Id$ */

var file = new File(http_request.real_path);
if(!file.open("r")) {
	writeln("!ERROR " + file.error + " opening " + http_request.real_path);
	exit();
}
var text = file.readAll();
file.close();
write(html_encode(text.join("\n")));
