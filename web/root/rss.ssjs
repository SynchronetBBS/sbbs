// rss.ssjs

// $Id$

load("sbbsdefs.js");

var REVISION = "$Revision$".split(' ')[1];

//log(LOG_INFO,"Synchronet RSS " + REVISION);

var link_root = "http://" + http_request.header.host + http_request.request_string;

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
			+ channel_list[c].name.link(link_root + "?channel=" + channel_list[c].name));
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

	load("../web/lib/template.ssjs");
	load("../web/lib/mime_decode.ssjs");

	var msgbase=new MsgBase(channel.sub);
	if(!msgbase.open())
		writeln('Error: ' + msgbase.error);
	else {
		template.hdr = msgbase.get_msg_header(false,Number(http_request.query["item"]));
		if(!template.hdr)
			writeln('Error: ' + msgbase.error);
		else
			template.body= msgbase.get_msg_body(false, template.hdr.number);
	}

	msg=mime_decode(template.hdr,template.body);
	template.body=msg.body;
	if(msg.type=="plain") {
		/* ANSI */
		if(template.body.indexOf('\x1b[')>=0 || template.body.indexOf('\x01')>=0) {
			template.body=html_encode(template.body,true,false,true,true);
		}
		/* Plain text */
		else {
			template.body=word_wrap(template.body,80);
			template.body=html_encode(template.body,true,false,false,false);
		}
	}
	if(msg.attachments!=undefined) {
		template.attachments=new Object;
		for(att in msg.attachments) {
			template.attachments[att]=new Object;
			template.attachments[att].name=msg.attachments[att];
		}
	}

	write_template("header.inc");
	write_template("msgs/msg.inc");
	write_template("footer.inc");

	if(0) {
	writeln('<html>');
	writeln('<body>');
	var msgbase=new MsgBase(channel.sub);
	if(!msgbase.open())
		writeln('Error: ' + msgbase.error);
	else {
		var hdr = msgbase.get_msg_header(false,Number(http_request.query["item"]));
		if(!hdr)
			writeln('Error: ' + msgbase.error);
		else {
			for(h in hdr)
				writeln(h + ": " + hdr[h] + '<br>');
		}
	}
	writeln('</body>');
	writeln('</html>');
	}

	exit();
}

writeln('<rss version="0.91">');
writeln('\t<channel>');
writeln('\t\t<title>'		+ sub.name			+ '</title>');
writeln("\t\t<description>" + sub.description	+ "</description>");

writeln('\t\t<link>' + link_root + '</link>');
writeln('\t\t<language>en-us</language>');

function encode(str, wspace)
{
	return(strip_ctrl(html_encode(str
		,true	/* ex-ASCII */
		,wspace	/* white-space */
		,false	/* ANSI */
		,false	/* Ctrl-A */
		)));
}

var msgbase=new MsgBase(channel.sub);
if(msgbase.open()) {

	var last_msg;

	if(last_msg = msgbase.get_msg_header(false, msgbase.last_msg)) {
		writeln('\t\t<lastBuildDate>' + last_msg.date + '</lastBuildDate>');
	}
	var total_msgs = msgbase.total_msgs;
	for(i=0;i<total_msgs;i++) {
		var hdr = msgbase.get_msg_header(true,i);
		if(!hdr || hdr.attr&MSG_DELETE)
			continue;
		var body = msgbase.get_msg_body(true,i);
		if(!body)
			continue;
		writeln('\t\t\t<item>');
		writeln('\t\t\t\t<pubDate>' + hdr.date + '</pubDate>');
		writeln('\t\t\t\t<title>' + encode(hdr.subject) + '</title>');
		writeln('\t\t\t\t<author>' + encode(hdr.from) + '</author>');
		writeln('\t\t\t\t<guid>' + encode(hdr.id) + '</guid>');
		writeln('\t\t\t\t<description>' + encode(body.slice(0,500)) + '</description>');
		writeln('\t\t\t\t<link>' + link_root + '&item=' + hdr.number + '</link>');
		writeln('\t\t\t</item>');
	}
    msgbase.close();
}
writeln('\t</channel>');
writeln('</rss>');

