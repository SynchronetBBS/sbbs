/* univiewer_search_lib.js - the file name search index of the Universal Viewer library, and the search
 *
 * The index is a set of plain text files in the .index directory of the univiewer directory, one for each library,
 * made by univiewer_index.js (run it from jsexec when the files in the libraries have changed).  There is no database:
 * a database needs file locking, which does not work reliably on the network shares that several hosts of one
 * Synchronet install use.  An index file is only ever written whole, under another name, and renamed into place, and
 * it is read whole into memory and closed, so any number of hosts can search while one is rebuilding it.
 *
 * A file is a header line:
 *	UVIDX1 <TAB> time built <TAB> number of files (ten digits) <TAB> library name
 * and then a line for each file:
 *	name <TAB> description <TAB> folder <TAB> size <TAB> date (seconds since 1970)
 * in the order the library lists them (folder by folder).  Fields never contain a tab or a line break.
 *
 *   idx_path(settings_name, library_name)       where a library's index file is
 *   open_writer(settings_name, library_name)    write one a line at a time: add(line), then close() (or abort())
 *   file_line(name, desc, folder, size, date)   a line for add()
 *   header(settings_name, library_name)         { built, count } or null if there is no usable index file
 *   search(settings_name, libs, query, max)     { rows: [{ lib, folder, name, size, date, desc }], total }
 *   remove_except(settings_name, library_names) delete the index files of libraries no longer listed
 *
 * Beside each index file the indexer keeps a .dirs file, which records each folder's modification time and where its
 * lines are in the index file, so that the next run need only read the folders that have changed (see
 * univiewer_index.js).  Nothing else reads it.
 *
 * A search matches a part of a name, so "cat" finds "category" and "at" finds "cat".  Every word typed must be in the
 * name or the description of a file.  Nothing is left out as a common word: the best matches come first (a name with
 * the word on its own, then one that starts with it, then one that has it inside, then one that has it only in the
 * description) and only the first max are returned, with the number of all of them in total.
 *
 * The search reads one library's file at a time and looks for the longest word with indexOf(), which is native and
 * fast, then checks the other words only on the lines where that one is found.  So it takes a few tens of milliseconds
 * for a library of 60,000 files, and needs memory for a copy of one library's file at a time (about 30 MB for that).
 */
"use strict";
(function () {
	var VERSION = "UVIDX1";
	var MAX_TERMS = 8;
	var DIR = backslash(js.exec_dir) + ".index/";

	/* The part of an index file's name that comes from the settings file's name.  It has no "-" in it (the rest of the
	 * name is separated from it by one), so the files of settings.ini can never be mistaken for those of
	 * settings-test.ini when one is looked for by its beginning. */
	function base_of(settings_name)
	{
		return String(settings_name || "settings.ini").replace(/^.*[\\\/]/, "").replace(/\.ini$/i, "").replace(/[^A-Za-z0-9_]/g, "_");
	}

	/* A name made of what is safe in a file name on any host, plus a short hash so that two names never collide */
	function idx_path(settings_name, library_name)
	{
		var base = base_of(settings_name);
		var lib = String(library_name).replace(/[^A-Za-z0-9_-]/g, "_").substr(0, 40);
		return DIR + base + "-" + lib + "-" + md5_calc(String(library_name), true).substr(0, 8) + ".idx";
	}

	function clean(s)
	{
		return String(s).replace(/[\x00-\x1f]/g, " ");
	}

	function file_line(name, desc, folder, size, date)
	{
		return clean(name) + "\t" + clean(desc) + "\t" + clean(folder) + "\t" + (size || 0) + "\t" + (date || 0);
	}

	/* Write a library's index a few thousand lines at a time (so a library of any size needs little memory), under
	 * another name, and rename it over the old one when it is complete.  (A host that has the old one open can make
	 * the rename fail for a moment, so it is tried again.)  Returns { add(line), close() (the size of the file),
	 * abort() }.  The number of files in the header line is padded to ten digits, so that it can be written over
	 * once the number is known. */
	function open_writer(settings_name, library_name)
	{
		var path = idx_path(settings_name, library_name);
		mkpath(DIR);
		var tmp = path + ".new";
		var f = new File(tmp);
		if (!f.open("wb"))
			throw "Unable to create " + tmp;
		var stamp = String(time());
		var tail = "\t" + clean(library_name) + "\n";
		function head(n)
		{
			return VERSION + "\t" + stamp + "\t" + ("0000000000" + n).slice(-10) + tail;
		}
		var first = head(0);
		f.write(first);
		var buf = [];
		var count = 0;
		var pos = first.length;		/* bytes written so far, counting what is waiting in buf */
		var done = false;

		function flush()
		{
			if (buf.length) {
				f.write(buf.join("\n") + "\n");
				buf = [];
			}
		}

		return {
			stamp: stamp,
			/* the byte position in the file that the next line goes at */
			offset: function () {
				return pos;
			},
			add: function (line) {
				buf.push(line);
				count++;
				pos += line.length + 1;
				if (buf.length >= 2000)
					flush();
			},
			/* lines already in index form (each ending with a line break), copied from an earlier index */
			add_block: function (text, lines) {
				flush();
				f.write(text);
				count += lines;
				pos += text.length;
			},
			close: function () {
				flush();
				f.position = 0;
				f.write(head(count));
				f.close();
				done = true;
				var size = file_size(tmp);
				for (var tries = 0; tries < 40; tries++) {
					file_remove(path);
					if (file_rename(tmp, path))
						return size;
					mswait(50);
				}
				file_remove(tmp);
				throw "Unable to put " + path + " in place";
			},
			abort: function () {
				if (!done) {
					f.close();
					file_remove(tmp);
				}
			}
		};
	}

	/* The folders of a library as they were when its index was made: what the indexer needs to make the next one by
	 * looking only at folders that have changed.  It is a text file beside the index file:
	 *	UVDIRS1 <TAB> time the index was built <TAB> its size <TAB> time the scan began <TAB> signature of the settings
	 * and a line for each folder:
	 *	folder <TAB> its modification time <TAB> that of its .desc file <TAB> where its files start in the index file
	 *	<TAB> how many bytes they are <TAB> how many files <TAB> names of its sub-folders separated by \x01
	 * Nothing but univiewer_index.js reads it. */
	function dirs_path(settings_name, library_name)
	{
		return idx_path(settings_name, library_name).replace(/\.idx$/, ".dirs");
	}

	function read_dirs(settings_name, library_name)
	{
		var f = new File(dirs_path(settings_name, library_name));
		if (!f.open("rb"))
			return null;
		var text = f.read();
		f.close();
		if (!text)
			return null;
		var lines = String(text).split("\n");
		var h = lines[0].split("\t");
		if (h[0] != "UVDIRS1" || h.length < 5)
			return null;
		var out = { stamp: h[1], size: Number(h[2]), scan: Number(h[3]), signature: h[4], dirs: {} };
		for (var i = 1; i < lines.length; i++) {
			var p = lines[i].split("\t");
			if (p.length < 7)
				continue;
			out.dirs[p[0]] = { m: Number(p[1]), d: Number(p[2]), off: Number(p[3]), len: Number(p[4]), count: Number(p[5]),
				children: p[6].length ? p[6].split("\x01") : [] };
		}
		return out;
	}

	/* dirs is [{ folder, m, d, off, len, count, children }] in any order */
	function write_dirs(settings_name, library_name, stamp, size, scan, signature, dirs)
	{
		var path = dirs_path(settings_name, library_name);
		var tmp = path + ".new";
		var f = new File(tmp);
		if (!f.open("wb"))
			throw "Unable to create " + tmp;
		var ok = false;
		try {
			f.write("UVDIRS1\t" + stamp + "\t" + size + "\t" + scan + "\t" + signature + "\n");
			for (var i = 0; i < dirs.length; i += 5000) {
				f.write(dirs.slice(i, i + 5000).map(function (d) {
					return d.folder + "\t" + d.m + "\t" + d.d + "\t" + d.off + "\t" + d.len + "\t" + d.count + "\t" + d.children.join("\x01");
				}).join("\n") + "\n");
			}
			ok = true;
		}
		finally {
			f.close();
			if (!ok)
				file_remove(tmp);
		}
		for (var tries = 0; tries < 40; tries++) {
			file_remove(path);
			if (file_rename(tmp, path))
				return;
			mswait(50);
		}
		file_remove(tmp);
		throw "Unable to put " + path + " in place";
	}

	/* Delete the files that a run that was stopped part way leaves (a new index being written under another name) */
	function clean_tmp(settings_name)
	{
		var base = base_of(settings_name);
		var all = directory(DIR + base + "-*.new");
		var gone = 0;
		for (var i = 0; i < all.length; i++) {
			if (file_remove(all[i]))
				gone++;
		}
		return gone;
	}

	function header(settings_name, library_name)
	{
		var path = idx_path(settings_name, library_name);
		if (!file_exists(path))
			return null;
		var f = new File(path);
		if (!f.open("rb"))
			return null;
		var line = f.readln(4096);
		f.close();
		if (!line)
			return null;
		var p = String(line).replace(/\r$/, "").split("\t");
		if (p[0] != VERSION)
			return null;
		return { built: Number(p[1]) || 0, count: Number(p[2]) || 0 };
	}

	function remove_except(settings_name, library_names)
	{
		var keep = {};
		for (var i = 0; i < library_names.length; i++)
			keep[idx_path(settings_name, library_names[i])] = true;
		var base = base_of(settings_name);
		var all = directory(DIR + base + "-*.idx");
		var gone = 0;
		for (var k = 0; k < all.length; k++) {
			if (!keep[all[k]] && file_remove(all[k])) {
				gone++;
				file_remove(all[k].replace(/\.idx$/, ".dirs"));
			}
		}
		return gone;
	}

	/* The terms of a search: lower case, separated by white space, no repeats, at most MAX_TERMS of them */
	function terms_of(query)
	{
		var seen = {};
		var out = [];
		var t = String(query).toLowerCase().split(/\s+/);
		for (var i = 0; i < t.length && out.length < MAX_TERMS; i++) {
			if (t[i].length && !seen["t:" + t[i]]) {
				seen["t:" + t[i]] = true;
				out.push(t[i]);
			}
		}
		return out;
	}

	/* Is c (a character, or "" at the end) part of a word? */
	function is_word(c)
	{
		return c.length > 0 && ((c >= "a" && c <= "z") || (c >= "0" && c <= "9") || c >= "\x80");
	}

	/* How well a term matches the line that starts at ls (its name ends at t1, its description at t2), 0 = best:
	 * 0 as a whole word in the name, 1 at the start of the name, 2 inside the name, 3 only in the description */
	function rank_term(low, ls, t1, term)
	{
		var len = term.length;
		var best = 3;
		var p = low.indexOf(term, ls);
		while (p >= 0 && p + len <= t1) {
			if (!is_word(p == ls ? "" : low.charAt(p - 1)) && !is_word(low.charAt(p + len)))
				return 0;
			if (p == ls)
				best = 1;
			else if (best > 2)
				best = 2;
			p = low.indexOf(term, p + 1);
		}
		return best;
	}

	function search(settings_name, libs, query, max)
	{
		var terms = terms_of(query);
		var out = { rows: [], total: 0 };
		if (!terms.length || !libs.length)
			return out;
		var driver = 0;		/* the longest word is the one looked for first: the least often found */
		var k, i;
		for (k = 1; k < terms.length; k++) {
			if (terms[k].length > terms[driver].length)
				driver = k;
		}
		var d = terms[driver];
		var buckets = [];		/* by how well the line matched: the lines (and their library), in the order found */
		for (i = 0; i < 3 * terms.length + 1; i++)
			buckets.push([]);
		for (var li = 0; li < libs.length; li++) {
			var path = idx_path(settings_name, libs[li]);
			if (!file_exists(path))
				continue;
			var f = new File(path);
			var opened = f.open("rb");
			for (var tries = 0; !opened && tries < 3; tries++) {		/* it may be being replaced just now */
				mswait(100);
				opened = f.open("rb");
			}
			if (!opened)
				continue;
			var text = f.read();
			f.close();
			if (!text || text.substr(0, VERSION.length) != VERSION)
				continue;
			var low = text.toLowerCase();		/* the same length as text: the same positions */
			var pos = low.indexOf("\n") + 1;
			while (pos > 0 && pos < low.length) {
				i = low.indexOf(d, pos);
				if (i < 0)
					break;
				var ls = low.lastIndexOf("\n", i) + 1;
				var le = low.indexOf("\n", i);
				if (le < 0)
					le = low.length;
				pos = le + 1;
				var t1 = low.indexOf("\t", ls);
				var t2 = t1 < 0 ? -1 : low.indexOf("\t", t1 + 1);
				if (t2 < 0 || i >= t2)		/* not a good line, or the word is in the folder, size or date */
					continue;
				var score = 0;
				var ok = true;
				for (k = 0; k < terms.length && ok; k++) {
					var j = low.indexOf(terms[k], ls);
					if (j < 0 || j + terms[k].length > t2)
						ok = false;
					else
						score += rank_term(low, ls, t1, terms[k]);
				}
				if (!ok)
					continue;
				out.total++;
				if (buckets[score].length < max)
					buckets[score].push({ lib: libs[li], line: String(text.substring(ls, le)).concat("") });
			}
			/* a library's file is read into memory twice (as it is and in lower case): let that go before the next one,
			 * or a search of many libraries could use several times as much memory as the biggest of them */
			text = null;
			low = null;
			if (libs.length > 1)
				js.gc();
		}
		for (i = 0; i < buckets.length && out.rows.length < max; i++) {
			for (k = 0; k < buckets[i].length && out.rows.length < max; k++) {
				var p = buckets[i][k].line.split("\t");
				out.rows.push({ lib: buckets[i][k].lib, name: p[0], desc: p[1], folder: p[2], size: Number(p[3]) || 0, date: Number(p[4]) || 0 });
			}
		}
		return out;
	}

	return { idx_path: idx_path, open_writer: open_writer, file_line: file_line, header: header, search: search,
		remove_except: remove_except, terms_of: terms_of, dirs_path: dirs_path, read_dirs: read_dirs, write_dirs: write_dirs,
		clean_tmp: clean_tmp };
})();
