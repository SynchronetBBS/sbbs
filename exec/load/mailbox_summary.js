// Cached summary of a user's private e-mail base, for the mailbox library.
//
// var Summary = load("mailbox_summary.js");
// var list = Summary.get(msgbase, jsonfiles [, {data_dir}]);
// var r = Summary.modify(msgbase, jsonfiles, fn [, {data_dir}]);	// {bytes, count, result}
// var u = Summary.usage(msgbase, jsonfiles [, {data_dir}]);	// {bytes, count}
// var names = Summary.attach_names(subject);	// file names of a file-attached message
//
// msgbase is the open private base; jsonfiles is a JsonFiles object for the
// user's directory with prefix "mailbox.", so the side files are
// mailbox.summary.json and mailbox.state.json. data_dir (default
// system.data_dir) locates file attachments: data_dir/file/<user>.in/.
//
// Each summary entry is {num, tags, from, from_addr, to, subject, date,
// attr, size, attach, arrived, zone, priority, msgid}, one per message not
// flagged deleted, newest arrival (then highest number) first.
//
// Validity: the summary is valid while the base's .shd and .sid size and
// modification time (one-second resolution) match those recorded with it.
// A change made by something other than this library that leaves both files
// the same size within the same second as the recorded stamps (for instance
// a header rewritten in place, such as a tag change) is not seen until the
// next change to the base. Deleting mailbox.summary.json is always safe.
//
// Writes: the library makes every write to the base inside
// Summary.modify(), which holds the summary lock around the write, so the
// cache is updated incrementally only if it was valid just before the
// write. JsonFiles locks are not reentrant: fn must not call Summary.get(),
// usage() or modify() with the same files (such a call throws at once).
//
// Sizes: "size" is the space the message holds in the base, in whole
// 256-byte blocks: the header's stored length (read from the
// "header_length" line of MsgBase.dump_msg_header(), since no header
// property gives it) rounded up to blocks, plus data_length rounded up to
// blocks, plus for a file-attached message the sizes of its attachment
// files (a missing file counts 0). A header rewritten in place with a
// shorter length can keep more blocks allocated than this counts; smblib's
// own accounting does the same.
//
// Attachments: "attach" is true when auxattr has MSG_FILEATTACH or
// MSG_MIMEATTACH (set by the mail server when it counts MIME attachments),
// or the header's Content-Type is multipart/mixed. Bodies are not parsed.
//
// Strings: from, from_addr, to and subject are UTF-8; they are converted
// with utf8_encode() unless auxattr has MSG_HFIELDS_UTF8. tags is kept as
// stored (tags are ASCII or UTF-8 already, see mailbox_tags.js).

"use strict";

var Summary = (function () {
	var VERSION = 1;
	var BLOCK = 256;	// SHD_BLOCK_LEN and SDT_BLOCK_LEN (smbdefs.h)
	// From smbdefs.js
	var MSG_DELETE = (1 << 4);
	var MSG_VOTE = (1 << 11) | (1 << 12);
	var MSG_FILEATTACH = (1 << 1);	// auxattr
	var MSG_MIMEATTACH = (1 << 2);	// auxattr
	var MSG_HFIELDS_UTF8 = (1 << 13);	// auxattr

	function data_dir(opts)
	{
		return (opts && opts.data_dir) ? opts.data_dir : system.data_dir;
	}

	// Size and mtime of the base's header and index files
	function stamps(msgbase)
	{
		var b = msgbase.file;
		return [file_size(b + ".shd"), file_date(b + ".shd"),
			file_size(b + ".sid"), file_date(b + ".sid")];
	}

	// Sets the date of the base's header and index files to now, after a
	// write.  A network file system (SMB, NFS) may show a write in a file's
	// date only once the file is closed, and a header rewritten in place
	// does not change the file's length: stamps taken while the base is
	// still open would then hold the old date, and the next get() would
	// rebuild the cache for nothing (#1374).  Only for a base that nothing
	// but this library writes to: on an SMB share, a file whose date has
	// been set stops being dated by later writes while it is open.
	function date_files(msgbase)
	{
		file_utime(msgbase.file + ".shd");
		file_utime(msgbase.file + ".sid");
	}

	function same_stamps(a, b)
	{
		if (!(a instanceof Array) || a.length != b.length)
			return false;
		for (var i = 0; i < b.length; i++)
			if (a[i] !== b[i])
				return false;
		return true;
	}

	// Whether the stamps recorded after this library's own write (rec) are
	// those of the files now (now).  The sizes must match.  A file's date
	// may be the same or up to DATE_SLACK seconds EARLIER than recorded: a
	// network file system (SMB) can, when the file is closed, put the date
	// back from the one date_files() set to the time of the write before
	// it.  A later write by anyone moves the date forward, never back, so
	// an earlier date does not hide one (#1374).
	var DATE_SLACK = 2;
	function current_stamps(rec, now)
	{
		if (!(rec instanceof Array) || rec.length != now.length)
			return false;
		for (var i = 0; i < now.length; i += 2) {
			if (rec[i] !== now[i])
				return false;
			var d = rec[i + 1] - now[i + 1];
			if (!(d >= 0 && d <= DATE_SLACK))
				return false;
		}
		return true;
	}

	function blocks(len)
	{
		return Math.ceil(len / BLOCK) * BLOCK;
	}

	// The header's stored length, from MsgBase.dump_msg_header()
	function header_length(msgbase, hdr)
	{
		var lines = msgbase.dump_msg_header(hdr) || [];
		for (var i = 0; i < lines.length; i++) {
			var m = lines[i].match(/^header_length\s+(\d+)/);
			if (m)
				return parseInt(m[1], 10);
		}
		return BLOCK;	// not listed: count one block
	}

	// File names in a file-attached message's subject: space separated, a
	// name may be in double quotes; base name only; "", "." and ".." skipped
	function attach_names(subject)
	{
		var names = [];
		var re = /"([^"]*)"|(\S+)/g;
		var m;
		while ((m = re.exec(subject)) !== null) {
			var n = m[1] !== undefined && m[1] !== "" ? m[1] : (m[2] || "");
			n = file_getname(n);
			if (n === "" || n === "." || n === "..")
				continue;
			names.push(n);
		}
		return names;
	}

	function attach_dir(hdr, ddir)
	{
		var to = parseInt(hdr.to_ext, 10);
		if (to > 0)
			return ddir + format("file/%04u.in/", to);
		var from = parseInt(hdr.from_ext, 10);
		if (from > 0)
			return ddir + format("file/%04u.out/", from);
		return null;
	}

	function attach_bytes(hdr, ddir)
	{
		var dir = attach_dir(hdr, ddir);
		if (dir === null)
			return 0;
		var names = attach_names(hdr.subject || "");
		var total = 0;
		for (var i = 0; i < names.length; i++) {
			var n = file_size(dir + names[i]);
			if (n > 0)
				total += n;
		}
		return total;
	}

	// Read a live header by number (not expanded); null if gone or deleted
	function read_header(msgbase, num)
	{
		var hdr = msgbase.get_msg_header(false, num, false);
		if (hdr === null || hdr === undefined)
			return null;
		if (hdr.number != num || (hdr.attr & (MSG_DELETE | MSG_VOTE)))
			return null;
		return hdr;
	}

	// The cache holds rows (arrays) rather than objects:
	// about half the JSON to parse and write, which keeps modify() fast
	// Row order: num, tags, from, from_addr, to, subject, date, attr, size,
	// attach, arrived, zone, priority, msgid (see to_objects())
	var NUM = 0;
	var ATTR = 7;
	var SIZE = 8;
	var ARRIVED = 10;

	function summarize(msgbase, hdr, ddir)
	{
		var num = hdr.number;	// a non-NULL field first (lazy fields)
		var auxattr = hdr.auxattr;
		var utf8 = (auxattr & MSG_HFIELDS_UTF8) != 0;
		function str(s) {
			if (s === undefined || s === null)
				return "";
			s = String(s);
			return utf8 ? s : utf8_encode(s);
		}
		var ctype = hdr.content_type || "";
		var fileattach = (auxattr & MSG_FILEATTACH) != 0;
		// Blocks for the current header length: after an in-place header
		// shrink the base may still hold more blocks than this (as smblib
		// itself counts)
		var size = blocks(header_length(msgbase, hdr)) + blocks(hdr.data_length || 0);
		if (fileattach)
			size += attach_bytes(hdr, ddir);
		return [
			num,
			hdr.tags || "",
			str(hdr.from),
			str(hdr.from_net_addr),
			str(hdr.to),
			str(hdr.subject),
			hdr.when_written_time,
			hdr.attr,
			size,
			fileattach || (auxattr & MSG_MIMEATTACH) != 0
				|| /^multipart\/mixed\b/i.test(ctype),
			hdr.when_imported_time,
			hdr.when_written_zone_offset,
			hdr.priority || 0,
			hdr.id || ""
		];
	}

	function to_objects(rows)
	{
		var list = new Array(rows.length);
		for (var i = 0; i < rows.length; i++) {
			var r = rows[i];
			list[i] = {
				num: r[0], tags: r[1], from: r[2], from_addr: r[3], to: r[4],
				subject: r[5], date: r[6], attr: r[7], size: r[8], attach: r[9],
				arrived: r[10], zone: r[11], priority: r[12], msgid: r[13]
			};
		}
		return list;
	}

	// Newest arrival first, then highest number
	function order(a, b)
	{
		return (b[ARRIVED] - a[ARRIVED]) || (b[NUM] - a[NUM]);
	}

	// Insert a row into sorted rows
	function insert(rows, row)
	{
		var lo = 0;
		var hi = rows.length;
		while (lo < hi) {
			var mid = (lo + hi) >> 1;
			if (order(rows[mid], row) < 0)
				lo = mid + 1;
			else
				hi = mid;
		}
		rows.splice(lo, 0, row);
	}

	// Live (not deleted, not vote) index records
	function live_index(msgbase)
	{
		var idx = msgbase.get_index() || [];
		var live = [];
		for (var i = 0; i < idx.length; i++)
			if (!(idx[i].attr & (MSG_DELETE | MSG_VOTE)))
				live.push(idx[i]);
		return live;
	}

	function build(msgbase, ddir)
	{
		var idx = live_index(msgbase);
		var rows = [];
		for (var i = 0; i < idx.length; i++) {
			var hdr = read_header(msgbase, idx[i].number);
			if (hdr !== null)
				rows.push(summarize(msgbase, hdr, ddir));
		}
		rows.sort(order);
		return rows;
	}

	function total(rows)
	{
		var bytes = 0;
		for (var i = 0; i < rows.length; i++)
			bytes += rows[i][SIZE];
		return bytes;
	}

	function valid_shape(cache)
	{
		return typeof cache == "object" && cache !== null
			&& cache.version === VERSION
			&& cache.rows instanceof Array;
	}

	function valid(cache, st)
	{
		return valid_shape(cache) && current_stamps(cache.stamps, st);
	}

	// Rewrite the usage fields of mailbox.state.json, keeping its other
	// fields. Skipped when the base changed since 'cache' was recorded, so
	// a late writer cannot replace newer figures with older ones.
	// The state file is derived data: if its lock cannot be taken the write
	// is skipped, and usage() recomputes because its stamps do not match.
	function write_state(msgbase, jsonfiles, cache)
	{
		try {
			jsonfiles.update("state", {}, function (s) {
				if (typeof s != "object" || s === null || s instanceof Array)
					s = {};
				if (!current_stamps(cache.stamps, stamps(msgbase)))
					return s;
				s.bytes = total(cache.rows);
				s.count = cache.rows.length;
				s.stamps = cache.stamps;
				return s;
			});
		} catch (e) {
			if (e === null || typeof e != "object" || e.code !== "conflict")
				throw e;
		}
	}

	// Summary lock paths of modify() calls in progress in this script
	var active = {};

	function not_nested(jsonfiles)
	{
		if (active[jsonfiles.lock_path("summary")])
			throw new Error("Summary: nested call inside modify() on the same files"
				+ " (the summary lock is not reentrant)");
	}

	// The valid cache, rebuilding it (under the summary lock) if needed.
	// state_done is true when the state file needs no write from the caller.
	// If the lock is held too long (another rebuild, a slow modify), the
	// result is built from the headers without the lock and not saved.
	function current(msgbase, jsonfiles, opts)
	{
		not_nested(jsonfiles);
		var cache = jsonfiles.read("summary", null);
		if (valid(cache, stamps(msgbase)))
			return { cache: cache, state_done: false };
		var ddir = data_dir(opts);
		var result;
		var state_done = false;
		try {
			jsonfiles.update("summary", null, function (c) {
				var st = stamps(msgbase);	// before reading: a later change invalidates
				if (valid(c, st)) {
					result = c;	// another request rebuilt it meanwhile
					return c;
				}
				result = { version: VERSION, stamps: st, rows: build(msgbase, ddir) };
				state_done = true;
				write_state(msgbase, jsonfiles, result);
				return result;
			});
		} catch (e) {
			if (e === null || typeof e != "object" || e.code !== "conflict")
				throw e;
			result = { version: VERSION, stamps: stamps(msgbase), rows: build(msgbase, ddir) };
			state_done = true;
		}
		return { cache: result, state_done: state_done };
	}

	// Rows updated from a valid cache for the messages fn reported
	function patch(msgbase, old_rows, changes, ddir)
	{
		var added = changes.added || [];
		var removed = changes.removed || [];
		var changed = changes.changed || [];
		var drop = {};
		var i;
		for (i = 0; i < removed.length; i++)
			drop[removed[i]] = true;
		var reread = added.concat(changed);
		for (i = 0; i < reread.length; i++)
			drop[reread[i]] = true;
		var rows = [];
		for (i = 0; i < old_rows.length; i++)
			if (!drop[old_rows[i][NUM]])
				rows.push(old_rows[i]);
		var seen = {};
		for (i = 0; i < reread.length; i++) {
			var num = reread[i];
			if (seen[num])
				continue;
			seen[num] = true;
			var hdr = read_header(msgbase, num);
			if (hdr !== null)
				insert(rows, summarize(msgbase, hdr, ddir));
		}
		return rows;
	}

	var Summary = {};

	// The file names in a file-attached message's subject (for mailbox.js
	// and mailbox_mime.js, so there is one parser)
	Summary.attach_names = attach_names;

	Summary.get = function (msgbase, jsonfiles, opts)
	{
		return to_objects(current(msgbase, jsonfiles, opts).cache.rows);
	};

	// Make a write to the base and update the cache, all under the summary
	// lock: stamps are taken before fn() (the caller's write) and after it.
	// fn returns {added:[], removed:[], changed:[]} (message numbers), or
	// null/undefined for no change. If the cache was valid against the
	// stamps before the write, only the added and changed headers are read;
	// otherwise the summary is rebuilt from headers. The stamps after the
	// write are recorded. Returns {bytes, count, result}, result being what
	// fn returned. If fn throws, the cache is saved invalid (stamps null:
	// fn may have rewritten a header in place within the same second, which
	// the stamps cannot show), so the next get() rebuilds, and the exception
	// propagates unchanged. If fn returned but the cache cannot be updated
	// or saved (a header read or a file write failing), the error is logged,
	// the saved cache is made invalid (removed, else overwritten in place),
	// and modify still returns, with bytes and count null: fn's write was
	// made and must not be reported as failed. A lock that cannot be taken
	// throws { code: "conflict" } before fn runs.
	// fn must not call get(), usage() or modify() with the same files.
	// Not detected: a change by another process between the two stamps
	// (during fn), or a write fn makes but does not report.
	Summary.modify = function (msgbase, jsonfiles, fn, opts)
	{
		not_nested(jsonfiles);
		var ddir = data_dir(opts);
		var lock = jsonfiles.lock_path("summary");
		var out;
		var ran = false;	// fn returned
		var failed = null;	// {e}: what fn threw

		active[lock] = true;
		try {
			jsonfiles.update("summary", null, function (c) {
				var pre = stamps(msgbase);
				var was_valid = valid(c, pre);
				var result;
				try {
					result = fn();
				} catch (e) {
					failed = { e: e };
					return { version: VERSION, stamps: null, rows: [] };
				}
				ran = true;
				out = { bytes: null, count: null, result: result };
				var reported = !(result === null || result === undefined);
				var post = stamps(msgbase);
				var wrote = reported || !same_stamps(pre, post);
				// fn wrote: the files' dates are set at once (before the
				// headers are read, so close to the write: DATE_SLACK),
				// and those are the stamps kept.  Not when fn reported
				// nothing and nothing changed: a new date would itself
				// read as a change.
				if (wrote) {
					date_files(msgbase);
					post = stamps(msgbase);
				}
				var rows;
				if (!was_valid)
					rows = build(msgbase, ddir);
				else if (!reported)
					rows = wrote ? build(msgbase, ddir) : c.rows;
				else
					rows = patch(msgbase, c.rows, result, ddir);
				var cache = { version: VERSION, stamps: post, rows: rows };
				out.bytes = total(rows);
				out.count = rows.length;
				write_state(msgbase, jsonfiles, cache);
				return cache;
			});
		} catch (e) {
			if (failed === null && !ran)
				throw e;	// the lock (or the cache read) failed: fn never ran
			log(LOG_WARNING, "Summary: cache of " + msgbase.file + " not saved: " + e);
			invalidate(jsonfiles);
		} finally {
			delete active[lock];
		}
		if (failed !== null)
			throw failed.e;
		return out;
	};

	// Make the saved cache unusable after a failed save: remove it, else
	// (a reader holding it open) overwrite it in place
	function invalidate(jsonfiles)
	{
		var path = jsonfiles.path("summary");
		if (!file_exists(path) || file_remove(path))
			return;
		var f = new File(path);
		if (f.open("w")) {
			f.write("{}");
			f.close();
		}
		if (valid_shape(jsonfiles.read("summary", null)))
			log(LOG_ERR, "Summary: cannot invalidate " + path);
	}

	// {bytes, count}: from mailbox.state.json while its stamps match the
	// base (four stat calls and a small file read); otherwise recomputed
	Summary.usage = function (msgbase, jsonfiles, opts)
	{
		not_nested(jsonfiles);
		var s = jsonfiles.read("state", null);
		if (typeof s == "object" && s !== null && typeof s.bytes == "number"
			&& typeof s.count == "number" && current_stamps(s.stamps, stamps(msgbase)))
			return { bytes: s.bytes, count: s.count };
		var cur = current(msgbase, jsonfiles, opts);
		if (!cur.state_done)
			write_state(msgbase, jsonfiles, cur.cache);
		return { bytes: total(cur.cache.rows), count: cur.cache.rows.length };
	};

	return Summary;
})();

Summary;
