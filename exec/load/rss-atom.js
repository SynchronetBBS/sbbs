/*	Provides the Feed object, representing a loaded RSS or Atom feed.

	Usage Example:

	load("sbbsdefs.js");
	load("rss-atom.js");
	console.clear(BG_BLACK|LIGHTGRAY);
	var f = new Feed("http://bbs.electronicchicken.com/rss/bullshitrss.xml");
	for(var c = 0; c < f.channels.length; c++) {
		console.putmsg(f.channels[c].title + "\r\n");
		console.putmsg(f.channels[c].updated + "\r\n");
		for(var i = 0; i < f.channels[c].items.length; i++) {
			console.putmsg(f.channels[c].items[i].title + "\r\n");
			console.putmsg(f.channels[c].items[i].author + "\r\n");
			console.putmsg(f.channels[c].items[i].date + "\r\n");
			console.putmsg(f.channels[c].items[i].body + "\r\n");
			console.putmsg("---\r\n");
		}
		console.putmsg("---\r\n");
	}
	console.pause();

	Properties:

	Feed.channels (Array)

		An array of 'Channel' objects, representing an RSS <channel />
		or an Atom <feed />.  (In most cases, you'll be dealing with
		Feed.channels[0].)

	Methods:

	Feed.load();

		Loads the feed via HTTP.  This is called automatically upon
		instantiation, however it is available as a public method if
		you wish to reload the feed at any time.

	Channel objects:

		Each element in a Feed's 'channels' array is a Channel object.  This
		object represents an RSS channel or an Atom feed.

		Properties:

		Channel.title (String)

			The title of the channel/feed.

		Channel.description (String)

			The RSS <description /> or Atom <subtitle /> of this channel/feed.

		Channel.link (String)

			The <link /> of this channel/feed. (Note: this needs to be cleaned
			up a bit. Presently if there are multiple <link /> elements, their
			values will be concatenated into a single string.

		Channel.updated (String)

			The RSS <lastBuildDate /> or Atom <updated /> value for this
			channel/feed. (Just a string for now.)

		Channel.items (Array)

			An array of Item objects, representing the articles/entries in the
			channel/feed.

	Item objects:

		Each element in a Feed's 'channels' array's 'items' array is an Item
		object.  This object represents an RSS <item /> or Atom <entry />.

		Properties:

		Item.id (String)

			A unique identifier for this item. (RSS <guid />, Atom <id />)

		Item.title (String)

			The title of this item.

		Item.date (String)

			The date this item was last updated. (Just a string for now.)

		Item.author (String)

			The author of this item.

		Item.body (String)

			The RSS <description /> or Atom <summary /> for this item/entry.

		Item.content (String)

			The RSS <content:encoded /> for this item, if available.

		Item.link (String)

			The <link /> value for this item. (This needs cleaning up. If
			there are multiple <link /> elements, their values are joined
			into a single string.)

		Item.enclosures (Array)

			Array of { type : String, url : String, length : Number } objects
			for any <enclosure> elements in the item.

*/

load("http.js");

const Item = function (i) {

	this.id = i.guid.length() ? i.guid[0].toString() : (i.id.length() ? i.id[0].toString() : '');
	this.title = i.title.length() ? i.title[0].toString() : ''; // uh ...
	this.date = i.pubDate.length() ? i.pubDate[0].toString() : (i.updated.length() ? i.updated[0].toString() : '');
	this.author = i.author.length() ? i.author.toString() : '';
	this.body = i.description.length() ? i.description[0].toString() : (i.summary.length() ? i.summary[0].toString() : '');
	this.content = i.encoded.length() ? i.encoded.toString() : '';
	this.link = i.link.length() ? skipsp(truncsp(i.link[0].toString())) : '';
	this.enclosures = [];

	var enclosures = i.enclosure.length();
	for (var n = 0; n < enclosures; n++) {
		this.enclosures.push({
			type: i.enclosure[n].attribute('type'),
			url: i.enclosure[n].attribute('url'),
			length: parseInt(i.enclosure[n].attribute('length'), 10),
		});
	}

}

const Channel = function (c) {

	this.title = c.title.length() ? c.title[0].toString() : '';
	this.description = c.description.length() ? c.description[0].toString() : (c.subtitle.length() ? c.subtitle[0].toString() : '');
	this.link = c.link.length() ? skipsp(truncsp(c.link[0].toString())) : '';
	this.updated = c.lastBuildDate.length() ? c.lastBuildDate[0].toString() : (c.updated.length() ? c.updated[0].toString() : '');
	this.items = [];

	var items = c.item.length();
	for (var n = 0; n < items; n++) {
		this.items.push(new Item(c.item[n]));
	}

	var entries = c.entry.length();
	for (var n = 0; n < entries; n++) {
		this.items.push(new Item(c.entry[n]));
	}

}

const Feed = function (url, follow_redirects) {

	this.channels = [];

	this.load = function () {
		var doc;
		if (url.search('file://') == 0) {
			var f = new File(url.substring(7));
			if (!f.open('f')) throw new Error(f.error);
			doc = f.read();
			f.close();
			f = undefined;
		} else {
			var httpRequest = new HTTPRequest();
			httpRequest.follow_redirects = follow_redirects || 0;
			doc = httpRequest.Get(url);
			if (typeof doc == "undefined" || doc == "") {
				throw new Error('Empty response from server.');
			}
			httpRequest = undefined;
		}
		var feed = new XML(doc.replace(/^<\?xml.*\?>/g, ""));
		doc = undefined;
		switch (feed.localName()) {
			case "rss":
				var channels = feed.channel.length();
				for (var n = 0; n < channels; n++) {
					this.channels.push(new Channel(feed.channel[n]));
				}
				break;
			case "feed":
				this.channels.push(new Channel(feed));
				break;
			default:
				break;
		}
		feed = undefined;
	}

	this.load();

}
