// rss.ssjs

// $Id$

var REVISION = "$Revision$".split(' ')[1];

//log(LOG_INFO,"Synchronet RSS " + REVISION);

var ini_fname = file_cfgname(system.ctrl_dir, "rss.ini");

var ini_file = new File(ini_fname);
if(!ini_file.open("r")) {
	log(LOG_ERR,format("!ERROR %d opening ini_file: %s"
		,ini_file.error, ini_fname));
	exit();
}

var channel_list = ini_file.iniGetAllObjects();

ini_file.close();


var channel;
for(c in channel_list)
    if(channel_list[c].name == http_request.query["channel"]) {
        channel=channel_list[c];
        break;
    }

if(channel==undefined) {
	writeln('<html>');
	writeln('<body>');
	writeln('<h1>' + system.name + " News (RSS) Channels" + '</h1>');
	writeln('<ul>');
	for(c in channel_list)
		writeln('<li>' 
			+ channel_list[c].name.link(http_request.request_string + "?channel=" + channel_list[c].name));
	writeln('</ul>');
	writeln('</body>');
	writeln('</html>');
	exit();
}

var sub = msg_area.sub[channel.sub];
if(sub==undefined) {
    write("!unknown sub-board: " + channel.sub);
	exit();
}

if(http_request.query["item"]) {
	writeln('<html>');
	writeln('<body>');
	var msgbase=new MsgBase(channel.sub);
	if(!msgbase.open())
		writeln('Error: ' + msgbase.error);
	else {
		var hdr = msgbase.get_msg_header(true,Number(http_request.query["item"]));
		if(!hdr)
			writeln('Error: ' + msgbase.error);
		else {
			for(h in hdr)
				writeln(h + ": " + hdr[h]);
		}
	}
	writeln('</body>');
	writeln('</html>');

//		exit();
}

writeln('<rss version="0.91">');
writeln('\t<channel>');
writeln('\t\t<title>'		+ sub.name			+ '</title>');
writeln("\t\t<description>" + sub.description	+ "</description>");

writeln('\t\t<link>' + http_request.request_string + '</link>');
writeln('\t\t<language>en-us</language>');

var msgbase=new MsgBase(channel.sub);
if(msgbase.open()) {

	var last_msg;

	if(last_msg = msgbase.get_msg_header(false, msgbase.last_msg)) {
		writeln('\t\t<lastBuildDate>' + last_msg.date + '</lastBuildDate>');
	}
	var total_msgs = msgbase.total_msgs;
	for(i=0;i<total_msgs;i++) {
		var hdr = msgbase.get_msg_header(true,i);
		if(!hdr)
			continue;
		var body = msgbase.get_msg_body(true,i);
		if(!body)
			continue;
		writeln('\t\t\t<item>');
		writeln('\t\t\t\t<pubDate>' + hdr.date + '</pubDate>');
		writeln('\t\t\t\t<title>' + html_encode(hdr.subject,false,false,false) + '</title>');
		writeln('\t\t\t\t<author>' + html_encode(hdr.from,false,false,false) + '</author>');
		writeln('\t\t\t\t<guid>' + html_encode(hdr.id,false,false,false) + '</guid>');
		writeln('\t\t\t\t<description>' + html_encode(body,false,false,false) + '</description>');
//		writeln('\t\t\t\t<link>' + http_request.request_string + '?item=' + hdr.number + '</link>');
		writeln('\t\t\t</item>');
	}
    msgbase.close();
}
writeln('\t</channel>');
writeln('</rss>');

