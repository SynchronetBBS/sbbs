/* $Id: */

load("http.js");

var url = argv[0];
var filename=file_getname(url);

var file = new File(filename);

if(!file.open("w"))
	print("error " + file.error + " opening " + file.name);
else {
	var contents = new HTTPRequest().Get(url);
	file.write(contents);
	file.close();
}
