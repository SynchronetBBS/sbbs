/* sixteencolors.js - SixelView Library hook for the Sixteen Colors ANSI/ASCII art archive
 * (https://16colo.rs).  Years -> packs -> files.
 *
 * A hook is loaded with load({}, "hooks/<module>") and must define create(ctx), returning an
 * object with:
 *
 *	list(path)         path = array of names below the root (empty = root).  Returns an array of
 *	                   { name, isdir, [label], [size], [date], [desc] } or null on error.
 *	fetch(path, entry, kind)
 *	                   Make the file available locally; returns its full path, or null on error.
 *	                   kind is "image" when a PNG rendering of the file is wanted (only asked for
 *	                   if the hook sets images: true).
 *
 * ctx provides: options (the library's settings.ini section), get_json(url, cache_name),
 * get_file(url, local_name) -> local path or null, and the cache directory.  Tab size is 4.
 */

"use strict";

function create(ctx)
{
	var API = "https://api.16colo.rs/v0";
	var SITE = "https://16colo.rs";

	function pack_info(pack)
	{
		return ctx.get_json(API + "/pack/" + encodeURIComponent(pack), "packs/" + pack + ".json");
	}

	return {
		list: function (path) {
			var out = [];
			var i;
			if (path.length == 0) {
				var years = ctx.get_json(API + "/year", "years.json");
				if (!years)
					return null;
				for (i = 0; i < years.length; i++)
					out.push({ name: String(years[i].year), isdir: true,
						label: years[i].year + " (" + years[i].packs + " packs)" });
				return out;
			}
			if (path.length == 1) {
				var year = parseInt(path[0], 10);
				if (!(year > 0))
					return null;
				var count = 0;
				var ylist = ctx.get_json(API + "/year", "years.json");
				for (i = 0; ylist && i < ylist.length; i++) {
					if (ylist[i].year == year)
						count = ylist[i].packs;
				}
				var packs = ctx.get_json(API + "/year/" + year + "?page=1&rows=" + Math.max(count, 1),
					"year-" + year + ".json");
				if (!packs)
					return null;
				for (i = 0; i < packs.length; i++)
					out.push({ name: String(packs[i].name), isdir: true,
						label: packs[i].name, date: 0 });
				return out;
			}
			var pack = pack_info(path[1]);
			if (!pack || !pack.files)
				return null;
			for (i = 0; i < pack.files.length; i++)
				out.push({ name: String(pack.files[i].filename), isdir: false,
					location: pack.files[i].file_location });
			return out;
		},

		images: true,

		fetch: function (path, entry, kind) {
			if (path.length < 2 || !entry.location)
				return null;
			if (kind == "image")
				return ctx.get_file(SITE + "/pack/" + encodeURIComponent(path[1]) + "/x1/" +
					encodeURIComponent(entry.name) + ".png", path[1] + "/" + entry.name + ".png");
			return ctx.get_file(SITE + entry.location, path[1] + "/" + entry.name);
		}
	};
}

this;
