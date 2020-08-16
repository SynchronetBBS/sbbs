/* $Id: wget.js,v 1.5 2020/04/05 23:38:55 rswindell Exp $ */

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

if(!file_getname(filename))
	filename += "wget-output";

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
	{
		var http_request = new HTTPRequest();
		var contents = "";
		try {
			contents = http_request.Get(url);
		} catch(e) {
			alert(e);
			break;
		}
		if(http_request.response_code != http_request.status.ok)
			alert("HTTP-GET ERROR " + http_request.response_code);
		else {
			if(!file.open("w"))
				print("error " + file.error + " opening " + file.name);
			else {
				file.write(contents);
				file.close();
			}
		}
		break;
	}
	default:
		print("Unsupported URL scheme '"+purl.scheme+"'");
}
