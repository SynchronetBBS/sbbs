// Contacts: a user's address book, for the web mail client.  A pure
// library: no API methods, no web or session globals.
//
// var Contacts = load("contacts.js");
//
// ud is the user's UserData (webmail/lib/userdata.js); the book is
// webmail.contacts.json in the user's web client directory, every change
// a locked read-modify-write (UserData.update):
//   {v:1, next, tick, list:[{id, name, addrs:[...], note, used}]}
// id is "c<n>" (n from next); used is the value of tick when the contact
// was last created, changed or written to (recency for suggest()).
//
// Interface:
//   Contacts.list(ud)                      [{id, name, addrs, note}],
//                                          by name (ASCII case ignored)
//   Contacts.set(ud, {id, name, addrs, note})
//                                          the contact; creates one when
//                                          id is absent
//   Contacts.remove(ud, id)                true
//   Contacts.collect(ud, entries)          adds [{name, addr}] whose
//                                          address is in no contact;
//                                          marks known ones used; returns
//                                          the number added
//   Contacts.suggest(ud, q [, limit])      [{id, name, addr}]: addresses
//                                          of the user's own contacts
//                                          whose name (or a word of it) or
//                                          address starts with q (ASCII
//                                          case ignored), most recently
//                                          used first; limit 1-50 (10)
//
// Limits: at most 500 contacts (set: toolarge, reason "contacts";
// collect stops adding), 1 to 10 addresses each (invalid, reason
// "addrs"; with index, the position in addrs, when one address is bad), name 64 bytes and note 500 bytes through Address.clean (no
// control characters).  An address must parse as exactly one entry with
// Address.parse_list() (the syntax rules of the To line: an Internet,
// FidoNet or QWK address or a local user name); it is stored as that
// entry's address.  Two addresses of one contact that differ only in the
// case of ASCII letters are one.  collect() skips anything invalid
// without an error.
//
// Privacy: only what the user sent to is collected (the send step passes
// the user's own recipients, BCC included, into the user's own book,
// which no other user reads).  suggest() offers the user's own contacts
// only; it never looks up the system's users.
//
// Errors are thrown as { code, message } with a fixed message: invalid
// (reason name, addrs, note, id), notfound, toolarge, conflict (the
// book's lock), failed.

"use strict";

var Contacts = (function () {
	var HERE = (function () {
		var f = String(new Error().fileName || "");
		var i = Math.max(f.lastIndexOf('/'), f.lastIndexOf('\\'));
		return i >= 0 ? f.substr(0, i + 1) : "";
	})();
	var Address = load({}, HERE !== "" && file_exists(HERE + "address.js") ? HERE + "address.js" : "address.js");

	var FILE = "contacts";
	var MAX_CONTACTS = 500;
	var MAX_ADDRS = 10;
	var MAX_NOTE = 500;
	var MAX_LIMIT = 50;
	var DEFAULT_LIMIT = 10;
	var ID_RE = /^c[1-9][0-9]{0,8}$/;
	var MESSAGES = {
		invalid: "invalid contact",
		notfound: "no such contact",
		toolarge: "the address book is full",
		conflict: "the address book is busy, try again",
		failed: "the address book could not be saved"
	};

	function error(code, reason)
	{
		var e = { code: code, message: MESSAGES[code] };
		if (reason !== undefined)
			e.reason = reason;
		return e;
	}

	function lower(s)
	{
		return String(s).replace(/[A-Z]/g, function (c) {
			return String.fromCharCode(c.charCodeAt(0) + 32);
		});
	}

	// A map key for an address: prefixed, so no address ("constructor",
	// "__proto__", "toString") can meet a property every object has
	function key(addr)
	{
		return "a:" + lower(addr);
	}

	function empty()
	{
		return { v: 1, next: 1, tick: 0, list: [] };
	}

	// The book as read: a usable shape, unusable entries dropped
	function valid(o)
	{
		if (o === null || typeof o !== "object" || o.v !== 1 || !(o.list instanceof Array))
			return empty();
		var out = { v: 1, next: typeof o.next === "number" && o.next >= 1 ? o.next : 1,
			tick: typeof o.tick === "number" && o.tick >= 0 ? o.tick : 0, list: [] };
		for (var i = 0; i < o.list.length; i++) {
			var c = o.list[i];
			if (c === null || typeof c !== "object" || typeof c.id !== "string" || !ID_RE.test(c.id)
				|| typeof c.name !== "string" || !(c.addrs instanceof Array))
				continue;
			var addrs = [];
			for (var j = 0; j < c.addrs.length; j++)
				if (typeof c.addrs[j] === "string" && c.addrs[j] !== "")
					addrs.push(c.addrs[j]);
			if (addrs.length < 1)
				continue;	// a contact has at least one address
			out.list.push({ id: c.id, name: c.name, addrs: addrs,
				note: typeof c.note === "string" ? c.note : "",
				used: typeof c.used === "number" ? c.used : 0 });
			var n = parseInt(c.id.substr(1), 10);
			if (n >= out.next)
				out.next = n + 1;
		}
		return out;
	}

	function read(ud)
	{
		return valid(ud.read(FILE, null));
	}

	function update(ud, fn)
	{
		var result;
		ud.update(FILE, null, function (o) {
			var book = valid(o);
			result = fn(book);
			return book;
		});
		return result;
	}

	// One address through parse_list(): its address, or null
	function address(s)
	{
		if (typeof s !== "string" || s === "")
			return null;
		try {
			var list = Address.parse_list(s);
			if (list.length !== 1 || typeof list[0].addr !== "string" || list[0].addr === "")
				return null;
			return list[0].addr;
		} catch (e) {
			return null;
		}
	}

	function public_contact(c)
	{
		return { id: c.id, name: c.name, addrs: c.addrs.slice(0), note: c.note };
	}

	function by_name(a, b)
	{
		var x = lower(a.name);
		var y = lower(b.name);
		if (x !== y)
			return x < y ? -1 : 1;
		return parseInt(a.id.substr(1), 10) - parseInt(b.id.substr(1), 10);
	}

	function list(ud)
	{
		var book = read(ud);
		var out = [];
		for (var i = 0; i < book.list.length; i++)
			out.push(public_contact(book.list[i]));
		out.sort(by_name);
		return out;
	}

	function find(book, id)
	{
		for (var i = 0; i < book.list.length; i++)
			if (book.list[i].id === id)
				return i;
		return -1;
	}

	function checked_fields(fields)
	{
		if (fields === null || typeof fields !== "object")
			throw error("invalid");
		var name = fields.name === undefined || fields.name === null ? "" : fields.name;
		if (typeof name !== "string")
			throw error("invalid", "name");
		var note = fields.note === undefined || fields.note === null ? "" : fields.note;
		if (typeof note !== "string")
			throw error("invalid", "note");
		if (!(fields.addrs instanceof Array) || fields.addrs.length < 1 || fields.addrs.length > MAX_ADDRS)
			throw error("invalid", "addrs");
		var addrs = [];
		var seen = {};
		for (var i = 0; i < fields.addrs.length; i++) {
			var a = address(fields.addrs[i]);
			if (a === null) {
				var bad = error("invalid", "addrs");
				bad.index = i;	// which one: the page says which line
				throw bad;
			}
			if (seen[key(a)] === true)
				continue;
			seen[key(a)] = true;
			addrs.push(a);
		}
		return { name: Address.clean(name, Address.MAX_NAME), note: Address.clean(note, MAX_NOTE),
			addrs: addrs };
	}

	function set(ud, fields)
	{
		var f = checked_fields(fields);
		var id = fields.id;
		if (id !== undefined && id !== null && (typeof id !== "string" || !ID_RE.test(id)))
			throw error("invalid", "id");
		return update(ud, function (book) {
			book.tick++;
			var c;
			if (id === undefined || id === null) {
				if (book.list.length >= MAX_CONTACTS)
					throw error("toolarge", "contacts");
				c = { id: "c" + book.next++, name: f.name, addrs: f.addrs, note: f.note, used: book.tick };
				book.list.push(c);
			} else {
				var i = find(book, id);
				if (i < 0)
					throw error("notfound");
				c = book.list[i];
				c.name = f.name;
				c.addrs = f.addrs;
				c.note = f.note;
				c.used = book.tick;
			}
			return public_contact(c);
		});
	}

	function remove(ud, id)
	{
		if (typeof id !== "string" || !ID_RE.test(id))
			throw error("invalid", "id");
		return update(ud, function (book) {
			var i = find(book, id);
			if (i < 0)
				throw error("notfound");
			book.list.splice(i, 1);
			return true;
		});
	}

	function collect(ud, entries)
	{
		if (!(entries instanceof Array) || entries.length === 0)
			return 0;
		return update(ud, function (book) {
			var known = {};
			var i;
			for (i = 0; i < book.list.length; i++)
				for (var j = 0; j < book.list[i].addrs.length; j++)
					known[key(book.list[i].addrs[j])] = book.list[i];
			var added = 0;
			book.tick++;
			for (i = 0; i < entries.length; i++) {
				var e = entries[i];
				if (e === null || typeof e !== "object")
					continue;
				var a = address(e.addr);
				if (a === null)
					continue;
				var c = known[key(a)];
				if (c !== undefined) {
					c.used = book.tick;
					continue;
				}
				if (book.list.length >= MAX_CONTACTS)
					continue;
				c = { id: "c" + book.next++, name: Address.clean(typeof e.name === "string" ? e.name : "",
					Address.MAX_NAME), addrs: [a], note: "", used: book.tick };
				book.list.push(c);
				known[key(a)] = c;
				added++;
			}
			return added;
		});
	}

	function starts(s, q)
	{
		return lower(s).substr(0, q.length) === q;
	}

	function name_matches(name, q)
	{
		if (starts(name, q))
			return true;
		var words = name.split(/[ \t.,"'()<>-]+/);
		for (var i = 0; i < words.length; i++)
			if (words[i] !== "" && starts(words[i], q))
				return true;
		return false;
	}

	function suggest(ud, q, limit)
	{
		q = lower(Address.clean(typeof q === "string" ? q : "", 254));
		limit = parseInt(limit, 10);
		if (isNaN(limit))
			limit = DEFAULT_LIMIT;
		limit = Math.max(1, Math.min(MAX_LIMIT, limit));
		var book = read(ud);
		var cs = book.list.slice(0);
		cs.sort(function (a, b) {
			return (b.used - a.used) || by_name(a, b);
		});
		var out = [];
		for (var i = 0; i < cs.length && out.length < limit; i++) {
			var c = cs[i];
			var by_name_hit = q === "" || name_matches(c.name, q);
			for (var j = 0; j < c.addrs.length && out.length < limit; j++)
				if (by_name_hit || starts(c.addrs[j], q))
					out.push({ id: c.id, name: c.name, addr: c.addrs[j] });
		}
		return out;
	}

	function outgoing(e)
	{
		if (e !== null && typeof e === "object" && !(e instanceof Error) && typeof e.code === "string"
			&& MESSAGES[e.code] !== undefined) {
			var out = { code: e.code, message: MESSAGES[e.code] };
			if (typeof e.reason === "string")
				out.reason = e.reason;
			if (typeof e.index === "number")
				out.index = e.index;
			return out;
		}
		Contacts.log(LOG_ERR, "contacts: " + (e && e.message ? e.message : String(e)));
		return error("failed");
	}

	var Contacts = {
		MAX_CONTACTS: MAX_CONTACTS,
		MAX_ADDRS: MAX_ADDRS,
		log: function (level, text) {
			log(level, text);
		}
	};
	var PUBLIC = { list: list, set: set, remove: remove, collect: collect, suggest: suggest };
	for (var name in PUBLIC) {
		(function (fn) {
			Contacts[name] = function () {
				try {
					return fn.apply(null, arguments);
				} catch (e) {
					throw outgoing(e);
				}
			};
		})(PUBLIC[name]);
	}

	return Contacts;
})();

Contacts;
