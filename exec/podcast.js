if (js.podcast_load_headers == undefined)
	js.global.load("podcast_routines.js");

var opts = load({}, "modopts.js", "Podcast");
var base;
var i;
var out;
var hdrs;
var img_title;
var img_link;
var item;
var m;
var item_info;

function encode_xml(str)
{
	if (str==undefined)
		return '';
	str = str.toString();
	str=str.replace(/&/g, '&amp;');
	str=str.replace(/</g, '&lt;');
	str=str.replace(/>/g, '&gt;');
	str=str.replace(/"/g, '&quot;');
	str=str.replace(/'/g, '&apos;');
	return str;
}

function add_channel_opt_attribute_rename(name, newName) {
	if (opts[name] !== undefined)
		out.write('\t\t<'+newName+'>'+encode_xml(opts[name])+'</'+newName+'>\n');
}

function add_channel_opt_attribute(name) {
	var xml_name = name.substr(0,1).toLowerCase()+name.substr(1);
	add_channel_opt_attribute_rename(name, xml_name);
}

function add_channel_itunes_attribute(name) {
	add_channel_opt_attribute_rename('IT'+name, 'itunes:'+name.substr(0,1).toLowerCase()+name.substr(1));
}

function add_lost_episodes(out) {
	var f = new File(system.data_dir + "podcast_lost_episodes.xml");
	if(!f.open("r"))
		return;
	var xml = f.readAll();
	f.close();
	out.writeAll(xml);
}

function create_item_xml(base, hdr) {
	var info;
	var item;

	info=podcast_get_info(base, hdr);
	if (info == undefined)
		return;
	if (!podcast_get_enclosure_info(info.enclosure))
		return;
	item = '\t\t<item>\n';
	item += '\t\t\t<title>'+encode_xml(info.title)+'</title>\n';
	item += '\t\t\t<link>'+encode_xml(opts.EpisodeLink+parseInt(i+1))+'</link>\n';
	item += '\t\t\t<description>'+encode_xml(info.description)+'</description>\n';
	// TODO: author?
	// TODO: category?
	item += '\t\t\t<comments>'+encode_xml(opts.EpisodeLink+parseInt(i+1))+'</comments>\n';
	item += '\t\t\t<enclosure url="'+encode_xml(info.enclosure.url)+'" length="'+encode_xml(info.enclosure.length)+'" type="'+encode_xml(info.enclosure.type)+'" />\n';
	item += '\t\t\t<guid isPermaLink="false">'+encode_xml(info.guid)+'</guid>\n';
	item += '\t\t\t<pubDate>'+encode_xml(info.pubDate)+'</pubDate>\n';
	// TODO: source?
	item += '\t\t</item>\n';
	return item;
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

if (opts.FeedURI == undefined) {
	log("FeedURI undefined in modopts.ini.");
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

if (opts.EpisodeLink == undefined) {
	log("EpisodeLink undefined in modopts.ini.");
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
if (!out.open("wxb")) {
	log("Unable to open temorary file "+out.name+".");
	exit(1);
}

hdrs = podcast_load_headers(base, opts.From, opts.To);

// TODO: iTunes tags?
out.write('<?xml version="1.0"?>\n');
out.write('<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom" xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd">\n');
out.write('\t<channel>\n');
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
out.write('\t\t<generator>Synchronet Podcast Script '+("$Revision: 1.16 $".split(' ')[1])+'</generator>\n');
add_channel_opt_attribute('Docs');
// TODO: cloud (fancy!)
add_channel_opt_attribute_rename('TTL', 'ttl');
if (opts.ImageURL != undefined) {
	img_title = opts.ImageTitle == undefined ? opts.Title : opts.ImageTitle;
	img_link = opts.ImageLink == undefined ? opts.Link : opts.ImageLink;
	out.write('\t\t<image>\n');
	out.write('\t\t\t<url>'+encode_xml(opts.ImageURL)+'</url>\n');
	out.write('\t\t\t<title>'+encode_xml(img_title)+'</title>\n');
	out.write('\t\t\t<link>'+encode_xml(img_link)+'</link>\n');
	if (opts.ImageHeight != undefined)
		out.write('\t\t\t<height>'+encode_xml(opts.ImageHeight)+'</height>\n');
	if (opts.ImageWidth != undefined)
		out.write('\t\t\t<width>'+encode_xml(opts.ImageWidth)+'</width>\n');
	out.write('\t\t</image>\n');
}
add_channel_opt_attribute('Rating');
// TODO: textInput?
add_channel_opt_attribute('SkipHours');
add_channel_opt_attribute('SkipDays');
add_channel_itunes_attribute('Author');
add_channel_itunes_attribute('Block');
if (opts.ITImageURL != undefined) {
	out.write('\t\t<itunes:image href="'+encode_xml(opts.ITImageURL)+'" />\n');
}
else if(opts.ImageURL != undefined) {
	out.write('\t\t<itunes:image href="'+encode_xml(opts.ImageURL)+'" />\n');
}
if (opts.ITCategory != undefined) {
	if ((m = opts.ITCategory.match(/^\s*([^\/]+)\/(.+?)\s*$/)) != null) {
		out.write('\t\t<itunes:category text="'+encode_xml(m[1])+'">\n');
		out.write('\t\t\t<itunes:category text="'+encode_xml(m[2])+'" />\n');
		out.write('\t\t</itunes:category>\n');
	}
	else {
		out.write('\t\t<itunes:category text="'+encode_xml(opts.ITCategory)+'" />\n');
	}
}
add_channel_itunes_attribute('Explicit');
add_channel_itunes_attribute('Complete');
add_channel_opt_attribute_rename('ITNewFeedURL', 'itunes:new-feed-url');
if (opts.ITOwnerEmail != undefined) {
	out.write('\t\t<itunes:owner>\n');
	out.write('\t');
	add_channel_opt_attribute_rename('ITOwnerEmail', 'itunes:email');
	if (opts.ITOwnerName != undefined) {
		out.write('\t');
		add_channel_opt_attribute_rename('ITOwnerName', 'itunes:name');
	}
	out.write('\t\t</itunes:owner>\n');
}
add_channel_itunes_attribute('Subtitle');
add_channel_itunes_attribute('Summary');
out.write('\t\t<atom:link href="'+opts.FeedURI+'" rel="self" type="application/rss+xml" />\n');

for (i=hdrs.length - 1; i >= 0; i--) {
	item = create_item_xml(base, hdrs[i]);
	if (item != undefined)
		out.write(item);
}

add_lost_episodes(out);	// These messages were purged from dove-net/entertainment (oops)

out.write('\t</channel>\n');
out.write('</rss>\n');
out.close();
if (!file_rename(out.name, opts.Filename)) {
	log("Unable to rename "+out.name+" to "+opts.Filename+".");
	file_remove(out.name);
	exit(1);
}
