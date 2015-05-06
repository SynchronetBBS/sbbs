if (js.global.MSG_DELETE == undefined)
	js.global.load("sbbsdefs.js");
if (js.global.HTTP == undefined)
	js.global.load("http.js");

var opts = load({}, "modopts.js", "Podcast");
var base;
var i;
var hdr;
var out;
var hdrs;
var img_title;
var img_link;
var item;
var body;
var link;
var m;
var http;
var item_headers;
var item_length;
var item_type;

function encode_xml(str)
{
	str=str.replace(/&amp;/g, '&');
	str=str.replace(/&/g, '&amp;');
	str=str.replace(/</g, '&lt;');
	str=str.replace(/>/g, '&gt;');
	str=str.replace(/"/g, '&quot;');
	str=str.replace(/'/g, '&apos;');
	return str;
}

function add_channel_opt_attribute(name) {
	if (opts[name] !== undefined) {
		var xml_name = name.substr(0,1).toLowercase()+name.substr(1);
		out.write('\t\t<'+xml_name+'>'+encode_xml(opts[name])+'</'+xml_name+'>\n');
	}
}

if (opts == undefined) {
	log("Unable to load Podcast modopts");
	exit(1);
}

if (opts.Sub === undefined) {
	log("Sub undefined in modopts.ini");
	exit(1);
}

if (opts.Filename == undefined) {
	log("Filename undefined in modopts.ini.");
	exit(1);
}

if (opts.Title == undefined) {
	log("Title undefined in modopts.ini.");
	exit(1);
}

if (opts.Description == undefined) {
	log("Description undefined in modopts.ini.");
	exit(1);
}

if (opts.Link == undefined) {
	log("Link undefined in modopts.ini.");
	exit(1);
}

if (opts.From == undefined) {
	log("From undefined in modopts.ini.");
	exit(1);
}

if (opts.To == undefined) {
	log("To undefined in modopts.ini.");
	exit(1);
}

base = new MsgBase(opts.Sub);
if (base == null) {
	log('Unable to create "'+opts.Sub+'" MsgBase object.');
	exit(1);
}

if (!base.open()) {
	log('Unable to open sub '+opts.Sub+'.');
	exit(1);
}

out = new File(opts.Filename+'.new');
if (!out.open("web")) {
	log("Unable to open temorary file "+out.name+".");
	exit(1);
}

hdrs = [];
for (i = base.first_msg; i <= base.last_msg; i++) {
	hdr = base.get_msg_header(i);
	if (hdr == null)
		continue;
	if (hdr.attr & MSG_DELETE)
		continue;
	if (hdr.attr & MSG_MODERATED) {
		if (!(hdr.attr & MSG_VALIDATED))
			break;
	}
	if (hdr.thread_back != 0)
		continue;
	if (hdr.from.toLowerCase() != opts.From.toLowerCase() || hdr.to.toLowerCase() != opts.To.toLowerCase())
		continue;
	if (hdr.from_net_type != NET_NONE)
		continue;
	hdrs.push(hdr);
}

// TODO: iTunes tags?
out.write('<?xml version="1.0"?>\n<rss version="2.0">\n\t<channel>\n');
out.write('\t\t<title>'+encode_xml(opts.Title)+'</title>\n');
out.write('\t\t<description>'+encode_xml(opts.Description)+'</description>\n');
out.write('\t\t<link>'+encode_xml(opts.Link)+'</link>\n');
add_channel_opt_attribute('Language');
add_channel_opt_attribute('Copyright');
add_channel_opt_attribute('ManagingEditor');
add_channel_opt_attribute('WebMaster');
// TODO: pubDate
out.write('\t\t<lastBuildDate>' + encode_xml((new Date()).toUTCString()) + '</lastBuildDate>\n');
add_channel_opt_attribute('Category');
out.write('\t\t<generator>Synchronet Podcast Script '+("$Revision$".split(' ')[1])+'</generator>\n');
add_channel_opt_attribute('Docs');
// TODO: cloud (fancy!)
if (opts.TTL != undefined)
	out.write('\t\t<ttl>'+encode_xml(opts.TTL)+'</ttl>\n');
if (opts.ImageURL != undefined) {
	img_title = opts.ImageTitle == undefined ? opts.Title : opts.ImageTitle;
	img_link = opts.ImageLink == undefined ? opts.Link : opts.Link;
	out.write('\t\t<image>\n');
	out.write('\t\t\t<url>'+encode_xml(opts.ImageURL)+'</url>\n');
	out.write('\t\t\t<title>'+encode_xml(img_title)+'</title>\n');
	out.write('\t\t\t<link>'+encode_xml(img_link)+'</link>\n');
	if (opts.ImageHeight != undefined)
		out.write('\t\t\t<height>'+encode_xml(opts.ImageHeight)+'</height>\n');
	if (opts.ImageWidth != undefined)
		out.write('\t\t\t<width>'+encode_xml(opts.ImageWidth)+'</width>\n');
}
add_channel_opt_attribute('Rating');
// TODO: textInput
add_channel_opt_attribute('SkipHours');
add_channel_opt_attribute('SkipDays');

for (i=hdrs.length - 1; i >= 0; i--) {
	body = base.get_msg_body(hdrs[i].number);
	if (body == null)
		continue;
	body = word_wrap(body, 65535, 79, false).replace(/\r/g, '');
	m = body.match(/^[\r\n\s]*([\x00-\xff]+?)[\r\n\s]+(https?:\/\/[^\r\n\s]+)[\r\n\s]*$/);
	if (m==null)
		continue;
	http = new HTTPRequest();
	item_headers = http.Head(m[2]);
	if (item_headers == undefined)
		continue;
	if (item_headers['Content-Type'] == undefined || item_headers['Content-Length'] == undefined) {
		log("HEAD request of "+m[2]+" did not return either Content-Type or Content-Length");
		continue;
	}
	item_length = item_headers['Content-Length'][0] + 0;
	item_type = item_headers['Content-Type'][0].replace(/^\s*(.*?)\s*/, "$1");
	item = '\t\t<item>\n';
	item += '\t\t\t<title>'+encode_xml(hdrs[i].subject)+'</title>\n';
	// TODO: HTML integration required here for link
	item += '\t\t\t<link>'+encode_xml(opts.Link)+'</link>\n';
	item += '\t\t\t<description>'+encode_xml(m[1])+'</description>\n';
	// TODO: author?
	// TODO: category?
	// TODO: HTML integration required here for <comments> tag.
	item += '\t\t\t<enclosure url="'+encode_xml(m[2])+'" length="'+item_length+'" type="'+item_type+'" />\n';
	item += '\t\t\t<guid>'+encode_xml(hdrs[i].id)+'</guid>\n';
	item += '\t\t\t<pubDate>'+encode_xml((new Date(hdrs[i].when_written_time * 1000)).toUTCString())+'</pubDate>\n';
	// TODO: source?
	item += '\t\t</item>\n';
	out.write(item);
}

out.write('\t</channel>\n');
out.write('</rss>\n');
out.close();
if (!file_rename(out.name, opts.Filename)) {
	log("Unable to rename "+out.name+" to "+opts.Filename+".");
	file_remove(out.name);
	exit(1);
}
