// rss.ssjs

// $Id: rss.ssjs,v 1.22 2016/10/20 19:13:38 rswindell Exp $

// Tested successfully with SharpReader v0.9.5.1

load("sbbsdefs.js");

var REVISION = "$Revision: 1.22 $".split(' ')[1];

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
var defaults = ini_file.iniGetObject();

ini_file.close();

/* Set default max from the NetScapes original RSS 0.91 implementation
http://my.netscape.com/publish/formats/rss-spec-0.91.html#item */

if(defaults.maxmessages==undefined)
	defaults.maxmessages=15;

if(defaults.maxdesclength==undefined)
	defaults.maxdesclength=500;

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

var sub = msg_area.sub[channel.sub.toLowerCase()];
if(sub==undefined) {
    writeln(log(LOG_ERR,"!unknown sub-board: " + channel.sub));
	exit();
}

if(http_request.query["item"]) {

	load("../web/lib/template.ssjs");
	load("../web/lib/mime_decode.ssjs");

	var msgbase=new MsgBase(channel.sub);
	if(!msgbase.open())
		writeln(log(LOG_ERR,'Error: ' + msgbase.error));
	else {
		template.hdr = msgbase.get_msg_header(false,Number(http_request.query["item"]));
		if(!template.hdr)
			writeln(log(LOG_ERR,'Error: ' + msgbase.error));
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
			template.body=word_wrap(template.body);
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
		writeln(log(LOG_ERR,'Error: ' + msgbase.error));
	else {
		var hdr = msgbase.get_msg_header(false,Number(http_request.query["item"]));
		if(!hdr)
			writeln(log(LOG_ERR,'Error: ' + msgbase.error));
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

/* Setup default values (over-rideable in rss.ini) */
if(channel.title==undefined)		channel.title			=sub.name;
if(channel.description==undefined)	channel.description		=sub.description;
if(channel.link==undefined)			channel.link			=link_root;
if(channel.language==undefined)		channel.language		='en-us';

if(channel.image_url==undefined)	channel.image_url		='/images/default/sync_pbgj1_white_bg.gif';
if(channel.image_title==undefined)	channel.image_title		=channel.title;
if(channel.image_link==undefined)	channel.image_link		=channel.link;
if(channel.maxmessages==undefined)	channel.maxmessages		=defaults.maxmessages;
if(channel.maxdesclength==undefined)	channel.maxdesclength		=defaults.maxdesclength;

http_reply.header["Content-Type"]='application/rss+xml';
writeln('<?xml version="1.0" ?>');
writeln('<!DOCTYPE rss PUBLIC "-//Netscape Communications//DTD RSS 0.91//EN" "http://my.netscape.com/publish/formats/rss-0.91.dtd">');
writeln('<rss version="0.91">');
writeln('\t<channel>');
writeln('\t\t<title>'		+ channel.title			+ '</title>');
writeln('\t\t<description>' + channel.description	+ '</description>');
writeln('\t\t<link>'		+ channel.link			+ '</link>');
writeln('\t\t<language>'	+ channel.language		+ '</language>');
writeln('\t\t<image>');
	writeln('\t\t\t<url>'	+ channel.image_url		+ '</url>');
	writeln('\t\t\t<title>'	+ channel.image_title	+ '</title>');
	writeln('\t\t\t<link>'	+ channel.image_link	+ '</link>');
writeln('\t\t</image>');

function encode(str, wspace)
{
	return(html_encode(strip_ctrl(str.replace(/\s+/g," "))
		,true	/* ex-ASCII */
		,wspace	/* white-space */
		,false	/* ANSI */
		,false	/* Ctrl-A */
		));
}

var msgbase=new MsgBase(channel.sub);
if(msgbase.open()) {

	var last_msg;
	var msgs=0;

	if(last_msg = msgbase.get_msg_header(false, msgbase.last_msg)) {
		writeln('\t\t<lastBuildDate>' + last_msg.date + '</lastBuildDate>');
	}
	var total_msgs = msgbase.total_msgs;
	for(i=0;i<total_msgs;i++) {
		var hdr = msgbase.get_msg_header(true,total_msgs-i);
		if(!hdr || hdr.attr&MSG_DELETE)
			continue;
		var body = msgbase.get_msg_body(true,total_msgs-i);
		body=body.replace(/\r\n/g,'<br />');
		if(!body)
			continue;
		writeln('\t\t\t<item>');
		writeln('\t\t\t\t<pubDate>' + hdr.date + '</pubDate>');
		writeln('\t\t\t\t<title>' + encode(hdr.subject) + '</title>');
		writeln('\t\t\t\t<author>' + encode(hdr.from) + '</author>');
		writeln('\t\t\t\t<guid>' + encode(hdr.id) + '</guid>');
		writeln('\t\t\t\t<description>' + encode(body.slice(0,channel.maxdesclength)) + '</description>');

        if(this.login==undefined)  // v3.12a 
            writeln('\t\t\t\t<link>' + link_root + (defaults.useentities ? '&amp;' : '&') + 'item=' + hdr.number + '</link>');
        else    // v3.12b
            writeln('\t\t\t\t<link>' + 'http://' + http_request.header.host + '/msgs/msg.ssjs?msg_sub=' + 
                    channel.sub + (defaults.useentities ? '&amp;' : '&') + 'message=' + hdr.number + '</link>');
		writeln('\t\t\t</item>');
		msgs++;
		if(msgs>=channel.maxmessages)
			break;
	}
    msgbase.close();
}
writeln('\t</channel>');
writeln('</rss>');

