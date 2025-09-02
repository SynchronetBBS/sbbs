if(!http_request.post_data)
	exit(0);
var nodelist = new File(system.temp_dir + "nodelist.txt");
if(!nodelist.open("w")) {
	write("Error " + nodelist.error + " opening " + nodelist.name);
	exit(1);
}
nodelist.write(http_request.post_data, http_request.post_data.length);
nodelist.close();

var syncterm = new File(system.temp_dir + "syncterm.lst");

load({}, "fido-nodelist-syncterm.js", nodelist.name, syncterm.name);
if(!syncterm.open("r")) {
	write("Error " + syncterm.error + " opening " + syncterm.name);
	exit(2);
}

http_reply.header["Content-Type"] = "application/octet-stream";
http_reply.header["Content-Disposition"] = 'inline; filename="syncterm.lst"';
http_reply.status = "200 here's your file";
writeln(syncterm.read());
