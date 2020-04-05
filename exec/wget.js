/* $Id$ */

require("http.js", 'HTTPRequest');
require("ftp.js", 'FTP');
require("url.js", 'URL');

var url = argv[0];	// First argument is the URL (required).
var outfile;		// Pass a second argument as the outfile (optional).
var filename=backslash(js.startup_dir);

if (argv[1]) {
	outfile = argv[1];
	filename += outfile;
} else {
	filename += file_getname(url);
}

var purl = new URL(url);

print("Writing to file: " + filename);

var file = new File(filename);

switch(purl.scheme) {
	case 'ftp':
		try {
			var ftp = new FTP(purl.host);
			ftp.retr(purl.path, filename);
			ftp.quit();
		} catch(e) {
			print("Fetch failed: "+e);
		}
		break;
	case 'http':
	case 'https':
		if(!file.open("w"))
			print("error " + file.error + " opening " + file.name);
		else {
			var contents = new HTTPRequest().Get(url);
			file.write(contents);
			file.close();
		}
		break;
	default:
		print("Unhandled URL scheme '"+purl.scheme+"'");
}
