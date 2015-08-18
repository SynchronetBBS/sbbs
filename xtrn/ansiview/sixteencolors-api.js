/*	SixteenColors object
	A simple interface between Synchronet 3.15+ and the sixteencolors.net API.
	echicken -at- bbs.electronicchicken.com

	With the exception of .getFile(), which returns a file as a string, all
	other methods return arrays of objects.

	Useful methods:

	- .years()
		- Returns a list of years, and how many packs each year has.
	- .year(year)
		- Returns a list of all packs in a year
		- Each object in the array has a 'name' property, among others
	- .pack(pack)
		- Finds a pack by name (see above - not 'filename')
		- Returns an object with various properties, most useful being
		  'files', which is an array
		- Each object in the 'files' array has a 'file_location' property
	- .getFile(file_location)
		- Retrieves a file by file_location (see above)
		- Returns the file as a string

	Not-as-useful methods:

	- .packs(page, rows)
		- Returns 'page' of entries from the full list of packs in the archive
		  with 'rows' number of elements in the array.
		- I haven't tried this, but I assume its return value will be similar
		  to .year(), possibly alphabetized
	- .file(pack, name)
		- Returns info about a given file in a given pack
		- I didn't see much detail that you don't already get from .pack()
	- .randomFiles(page, rows)
		- Get a list of random files, I guess
	- .randomPacks(page, rows)
		- Get a list of random packs, I guess

	Currently useless methods:

	The API currently returns empty arrays for the following:

	- .groups()
	- .group(group)
	- .artists()
	- .artist()
	- .month(year, month)

	Errors:

	On error, all methods log an error message and then return null.  This is
	lame, but it's all I need for now.

*/

load("http.js");

var SixteenColors = function() {

	var apiUrl = "http://api.sixteencolors.net/v0";
	var downloadUrl = "http://sixteencolors.net/";

	var getJSON = function(path) {
		try {
			return JSON.parse(
				new HTTPRequest().Get(apiUrl + (typeof path == "undefined" ? "" : path))
			);
		} catch(err) {
			log("SixteenColors._getJSON: " + err);
			return null;
		}
	}

	this.years = function() {
		return getJSON("/year");
	}

	this.year = function(year) {
		if(typeof year == "undefined") {
			throw "SixteenColors.year: Invalid or no year provided.";
		} else {
			var y = this.years();
			if(y === null)
				return y;
			var count = 0;
			for(var yy = 0; yy < y.length; yy++) {
				if(y[yy].year != "year")
					continue;
				count = y[yy].packs;
				break;
			}
			return getJSON("/year/" + year + "?page=1&rows=" + count);
		}
	}

	this.packs = function(page, rows) {
		var path = "/pack";
		if(typeof page == "number" && typeof rows == "number")
			path += "?page=" + page + "&rows=" + rows;
		return getJSON(path);
	}

	this.pack = function(name) {
		if(typeof name != "string")
			throw "SixteenColors.pack: Invalid or no pack name provided.";
		else
			return getJSON("/pack/" + name);
	}

	this.file = function(pack, name) {
		if(typeof pack != "string")
			throw "SixteenColors.file: Invalid or no pack name provided.";
		else if(typeof name != "string")
			throw "SixteenColors.file: Invalid or no file name provided.";
		else
			return getJSON("/pack/" + pack + "/" + name);
	}

	this.randomFiles = function(page, rows) {
		if(typeof page == "undefined" || typeof rows == "undefined")
			throw "SixteenColors.randomFiles: Invalid 'page' or 'rows' argument (must be numbers)";
		else
			return getJSON("/file/random?page=" + page + "&rows=" + rows);
	}

	this.randomPacks = function(page, rows) {
		if(typeof page == "undefined" || typeof rows == "undefined")
			throw "SixteenColors.randomPacks: Invalid or missing 'page' or 'rows' arguments";
		else
			return getJSON("/pack/random?page=" + page + "&rows=" + rows);
	}

	this.getFile = function(file_location) {
		try {
			return (new HTTPRequest().Get(downloadUrl + file_location));
		} catch(err) {
			log("SixteenColors.getFile: " + err);
			return null;
		}
	}

	/*	The API seems to always return empty arrays for these last five calls.
		I don't think that anything's broken - just that 16c doesn't have
		group / artist / month metadata entered in for most (or any) packs yet.
		(When you list packs in a year, for example, each pack's 'month' value
		always seems to be null.) */

	this.groups = function() {
		return getJSON("/group");
	}

	this.group = function(name) {
		if(typeof name != "string")
			throw "SixteenColors.group: Invalid or no group name provided.";
		else
			return getJSON("/group/" + name);
	}

	this.artists = function() {
		return getJSON("/artist");
	}

	this.artist = function(artist) {
		if(typeof artist != "string")
			throw "SixteenColors.artist: Invalid or no artist name provided.";
		else
			return getJSON("/artist/" + artist);
	}

	this.month = function(year, month) {
		if(typeof year != "string" || year.length != 4)
			throw "SixteenColors.month: Invalid or no year provided.";
		else if(typeof month != "string" || month.length != 2)
			throw "SixteenColors.month: Invalid or no month provided.";
		else
			return getJSON("/year/" + year + "/" + month);
	}

}