/*	Imports RSS & Atom feeds into Synchronet message bases.

	Setup & Configuration:

	Add a 'feeds' section to ctrl/modopts.ini, in which each key is the
	internal code of a message sub-board, and each value is a comma-separated
	list of RSS or Atom feed URLs to be imported.  Example:

	[feeds]
	news = http://static.userland.com/gems/backend/rssTwoExample2.xml

	The above would import the feed at:
	http://static.userland.com/gems/backend/rssTwoExample2.xml
	into the local sub-board with an internal code of 'news'.

	In SCFG (BBS->Configure in Windows,) go to "External Programs" and then to
	"Timed Events", and create a new event.  You can assign any internal code
	that you wish, and execute it as often as you like (daily is probably
	sufficient, hourly is more than enough.) The "Command Line" for this event
	should be:

	?web_feed_importer

	Notes:

	This script attempts to avoid importing dupe messages, but you should turn
	duplicate-checking on (in scfg) for the subs that it operates on just to
	be safe.

	Some feeds contain a fixed number of recent items. Others append new items
	without shuffling off old ones.  If the number of items	in a feed exceeds
	a sub-board's maximum number of messages, older items may be re-imported.
	It's probably best to set affected sub-boards' maximum message and maximum
	message CRC values very high to avoid dupes from large or busy feeds.

	Configuration is simplistic at the moment.  We may wish to move to a
	standalone configuration file instead of a section in modopts.ini to allow
	for per-sub or per-feed configuration options in the future.  For now
	there is just sub-feed pairing in modopts.ini, and a few config variables
	in this file.
*/

load("sbbsdefs.js");
load("modopts.js");
load("rss-atom.js");

var forceFrom = false; // Always use the 'from' name (specified below)
var from = "Web Feed Importer"; // If forceFrom OR no item author
var to = "All"; // Probably a safe bet
var appendString = "\r\n\r\n"; // Tack something on to the end of a message
var appendLink = true; // Append the item's <link /> value to the message
var reverseOrder = true; // Leave this unless your feeds sort oldest to newest

// I know about the built-in dupe-checking, but am doing this for reasons.
var dupeCheck = function(sub, id) {
	var msgBase = new MsgBase(sub);
	msgBase.open();
	var header = msgBase.get_msg_header(id);
	msgBase.close();
	return (header !== null);
}

var prepareText = function(text) {
	/*	Some things aren't caught by html_decode()
		Each property name in 'replacements' will replace any of the strings
		in the replacements[property] array that are present in 'text'. */
	var replacements = {
		" " : [
			"&ensp;",
			"&emsp;",
			"&thinsp;",
			"&#8194;",
			"&#8195;",
			"&#8201;"
		],
		"\"" : [
			"&ldquo;",
			"&rdquo;",
			"&#8220;",
			"&#8221;",
			// I'm sure there's a better way to deal with these and similar
			ascii(226) + ascii(128) + ascii(156),
			ascii(226) + ascii(128) + ascii(157)
		],
		"'" : [
			"&lsquo;",
			"&rsquo;",
			"&#8216;",
			"&#8217",
			"Æ",
			ascii(226) + ascii(128) + ascii(153)
		],
		"-" : [
			"&ndash;",
			"&mdash;",
			"&#8211;",
			"&#8212;",
			ascii(226) + ascii(128) + ascii(148),
			ascii(226) + ascii(128) + ascii(147)
		],
		"..." : [
			"&hellip;",
			"&#8230;"
		]
	};
	text = html_decode(text);
	for(var r in replacements) {
		var re = new RegExp(replacements[r].join("|"), "g");
		text = text.replace(re, r);
	}
	text = text.replace(/(<([^>]+)>)/g, "");
	text = text.replace(/(\r?\n\s*\t*){2,}/g, "\r\n\r\n");
	text = truncsp(text);
	return text;
}

var importItem = function(sub, item) {
	var id = format(
		"<%s@%s>",
		md5_calc(item.date + item.title + item.body, true),
		system.inet_addr
	);
	if(dupeCheck(sub, id))
		return;
	var header = {
		'id' : id,
		'from' : (forceFrom || item.author == "") ? from : item.author,
		'to' : to,
		'from_net_type' : NET_UNKNOWN,
		'subject' : item.title
	};
	var msgBase = new MsgBase(sub);
	msgBase.open();
	msgBase.save_msg(
		header,
		((item.content == "") ? item.body : item.content) + appendString
	);
	msgBase.close();
	log(LOG_INFO,
		format("Imported item: %s, %s into %s", item.title, item.date, sub)
	);
}

var importSubFeeds = function(sub, feeds) {
	for(var f in feeds) {
		try {
			var feed = new Feed(feeds[f]);
			for each(var channel in feed.channels) {
				if(reverseOrder)
					channel.items.reverse();
				for each(var item in channel.items) {
					item.author = channel.title + ((item.author == "") ? "" : " (" + item.author + ")");
					if(item.title == "")
						item.title = channel.title;
					item.title = prepareText(item.title);
					item.body = prepareText(item.body);
					item.content = prepareText(item.content);
					if(appendLink && item.link != "") {
						item.body += "\r\n\r\n" + item.link;
						item.content += "\r\n\r\n" + item.link;
					}
					if(item.body == "" && item.content == "")
						continue;
					importItem(sub, item);
				}
			}
		} catch(err) {
			log(LOG_ERR, err);
		}
	}

}

var importFeeds = function() {
	var subs = get_mod_options("feeds");
	for(var sub in subs)
		importSubFeeds(sub.toUpperCase(), subs[sub].split(","));
}

importFeeds();