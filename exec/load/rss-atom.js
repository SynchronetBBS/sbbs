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

		Item.extra (Object)

			If the item/entry contains additional elements not provided for
			above, they are tacked on to this object in case you may need
			to access them.  Presumably these will all be E4X XML objects.

*/

load("http.js");

// This is really not the way to do things, but meh.
var toLocal = function(xmlObject) {
	for each(var element in xmlObject) {
		element.setName(element.localName());
		toLocal(element);
	}
}

var Feed = function(url) {

	var Item = function(xmlObject) {

		this.id = "";
		this.title = "";
		this.date = "";
		this.author = "";
		this.body = "";
		this.content = "";
		this.link = "";
		this.extra = {};

		for each(var element in xmlObject) {
			if(element.name() == "guid" || element.name() == "id")
				this.id = element;
			else if(element.name() == "title")
				this.title = element;
			else if(element.name() == "pubDate" || element.name() == "updated")
				this.date = element;
			else if(element.name() == "author")
				this.author = element.text(); // To do: deal with Atom-style <author>
			else if(element.name() == "description" || element.name() == "summary")
				this.body = element;
			else if(element.name() == "link")
				this.link = element.text(); // To do: deal with multiple links
			else if(element.name() == "encoded") // content:encoded
				this.content = element;
			else
				this.extra[element.name()] = element;

		}

	}

	var Channel = function(xmlObject) {

		this.title = "a";
		this.description = "";
		this.link = "";
		this.updated = "";
		this.items = [];

		if(typeof xmlObject.title != "undefined")
			this.title = xmlObject.title;

		if(typeof xmlObject.description != "undefined")
			this.description = xmlObject.description;
		else if(typeof xmlObject.subtitle != "undefined")
			this.description = xmlObject.subtitle;

		// To do: deal with multiple links
		if(typeof xmlObject.link != "undefined")
			this.link = xmlObject.link.text();

		if(typeof xmlObject.lastBuildDate != "undefined")
			this.updated = xmlObject.lastBuildDate;
		else if(typeof xmlObject.updated != "undefined")
			this.updated = xmlObject.updated;

		var items = xmlObject.elements("item");
		for each(var item in items) {
			this.items.push(new Item(item));
		}

		var entries = xmlObject.elements("entry");
		for each(var entry in entries) {
			this.items.push(new Item(entry));
		}

	}

	this.channels = [];

	this.load = function() {
		var httpRequest = new HTTPRequest();
		var response = httpRequest.Get(url);
		if(typeof response == "undefined" || response == "")
			throw "Empty response from server.";

		var feed = new XML(response.replace(/^<\?xml.*\?>/g, ""));
		toLocal(feed); // This is shitty
		switch(feed.localName()) {
			case "rss":
				var channels = feed.elements("channel");
				for each(var element in channels)
					this.channels.push(new Channel(element));
				break;
			case "feed":
				this.channels.push(new Channel(feed));
				break;
			default:
				break;
		}
	}

	this.load();

}
