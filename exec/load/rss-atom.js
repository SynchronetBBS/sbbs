load("http.js");

var getFeed = function(url) {
	var httpRequest = new HTTPRequest();
	var response = httpRequest.Get(url);
	if(
		typeof response == "undefined"
		||
		response === null
		||
		response == ""
	) {
		throw "Empty response";
	}
	response = response.replace(/^<\?xml.*\?>/g, "");
	response = response.replace(/<feed.*>/g, "<feed>");
	response = response.replace(/<\/?rss.*>/g, "");
	return new XML(response);
}

var objectsMatch = function(obj1, obj2) {
	for(var property in obj1) {
		if(!obj2.hasOwnProperty(property) || obj1[property] != obj2[property])
			return false;
	}
	for(var property in obj2) {
		if(!obj1.hasOwnProperty(property))
			return false;
	}
	return true;
}

var RSS = function(url) {
	
	var updated = false;
	this.__defineGetter__(
		"updated",
		function() {
			var ret = updated;
			if(updated)
				updated = false;
			return ret;
		}
	);
	
	this.properties = {
		title			: "",
		link			: "",
		description		: "",
		language		: "",
		copyright		: "",
		managingEditor	: "",
		webMaster		: "",
		pubDate			: "",
		lastBuildDate	: "",
		category		: "",
		generator		: "",
		docs			: "",
		cloud			: "",
		ttl				: "",
		image			: "",
		rating			: "",
		textInput		: "",
		skipHours		: "",
		skipDays		: "",
		url				: (typeof url == "undefined") ? "" : url
	};
	
	var Item = function(xmlObj) {
	
		this.properties = {
			title		: "",
			link		: "",
			description	: "",
			author		: "",
			category	: "",
			comments	: "",
			enclosure	: "",
			guid		: "",
			pubDate		: "",
			source		: ""
		};
		
		for each(var element in xmlObj) {
			if(element.name() == "pubDate") {
				var d = new Date(element.toString());
				this.properties[element.name()] = d.getTime() * .001;
			} else {
				this.properties[element.name()] = element;
			}
		}
		
		if(this.properties.title == "" && this.properties.description == "")
			throw "Invalid feed item";
	
	}
	
	this.items = [];
	
	this.load = function() {
		if(this.properties.url == "")
			throw "Feed URL not supplied";
		var feed = getFeed(this.properties.url);
		for each(var element in feed) {
			if(element.name() == "item") {
				var item = new Item(element);
				var add = true;
				for(var i = 0; i < this.items.length; i++) {
					if(!objectsMatch(this.items[i].properties, item.properties))
						continue;
					add = false;
					break;
				}
				if(add) {
					this.items.push(item);
					updated = true;
				}
			} else {
				this.properties[element.name()] = element;
			}
		}
		
		if(
			this.properties.title == ""
			||
			this.properties.link == ""
			||
			this.properties.description == ""
		) {
			throw "Invalid feed";
		}
	}
		
}

var Atom = function(url) {
	
	var updated = false;
	this.__defineGetter__(
		"updated",
		function() {
			var ret = updated;
			if(updated)
				updated = false;
			return ret;
		}
	);
	
	this.properties = {
		id			: "",
		title		: "",
		updated		: "",
		author		: "",
		link		: "",
		category	: "",
		contributor	: "",
		generator	: "",
		icon		: "",
		logo		: "",
		rights		: "",
		subtitle	: "",
		url			: (typeof url == "undefined") ? "" : url
	};
	
	var toTimestamp = function(datestr) {
		var yy   = datestr.substring(0,4);
		var mo   = datestr.substring(5,7);
		var dd   = datestr.substring(8,10);
		var hh   = datestr.substring(11,13);
		var mi   = datestr.substring(14,16);
		var ss   = datestr.substring(17,19);
		var tzs  = datestr.substring(19,20);
//		var tzhh = datestr.substring(20,22);
//		var tzmi = datestr.substring(23,25);
		var myutc = Date.UTC(yy-0,mo-1,dd-0,hh-0,mi-0,ss-0);
//		var tzos = (tzs+(tzhh * 60 + tzmi * 1)) * 60000;
//		var d = new Date(myutc-tzos);
		var d = new Date(myutc);
		return d.getTime() * .001;
	}
	
	var Entry = function(xmlObj) {
		
		this.properties = {
			id			: "",
			title		: "",
			updated		: "",
			author		: "",
			content		: "",
			link		: "",
			summary		: "",
			category	: "",
			contributor	: "",
			published	: "",
			source		: "",
			rights		: ""
		};
		
		for each(var element in xmlObj) {
			if(element.name() == "link" && element.hasOwnProperty("@href"))
				this.properties[element.name()] = element.@href;
			else if(element.name() == "updated" || element.name() == "published")
				this.properties[element.name()] = toTimestamp(element.toString());
			else
				this.properties[element.name()] = element;
		}
		
		if(
			this.properties.id == ""
			||
			this.properties.title == ""
			||
			this.properties.description == ""
		) {
			throw "Invalid feed item";
		}

	}
	
	this.entries = [];

	this.load = function() {
		if(typeof this.properties.url == "undefined")
			throw "Feed URL not supplied";
		var feed = getFeed(this.properties.url);
		for each(var element in feed) {
			if(element.name() == "entry") {
				var entry = new Entry(element);
				var add = true;
				for(var e = 0; e < this.entries.length; e++) {
					if(!objectsMatch(this.entries[e].properties, entry.properties))
						continue;
					add = false;
					break;
				}
				if(add) {
					this.entries.push(new Entry(element));
					updated = true;
				}
			} else if(element.name() == "updated") {
				this.properties[element.name()] = toTimestamp(element);
			} else {
				this.properties[element.name()] = element;
			}
		}
		
		if(
			this.properties.id == ""
			||
			this.properties.title == ""
			||
			this.properties.updated == ""
		) {
			throw "Invalid feed";
		}
	}
	
}