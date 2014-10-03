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
	item.body += appendString;
	var msgBase = new MsgBase(sub);
	msgBase.open();
	msgBase.save_msg(header, item.body);
	msgBase.close();
	log(LOG_INFO, format("Imported item: %s, %s into %s", item.title, item.date, sub));
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
					item.title = html_decode(item.title).replace(/(<([^>]+)>)/ig, "");
					item.body = html_decode(item.body).replace(/(<([^>]+)>)/ig, "");
					if(appendLink && item.link != "")
						item.body += "\r\n\r\n" + item.link;
					if(item.body == "")
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