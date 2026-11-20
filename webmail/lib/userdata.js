// UserData: a user's web client files (preferences, drafts and the like) as
// JSON in the user's web client directory.  A thin wrapper over the mailbox
// library's JsonFiles (exec/load/mailbox_files.js), which does the atomic
// writes and the locking.
//
// var UserData = load("userdata.js");
// var ud = new UserData(dir [, JsonFiles]);
//
// dir is the user's web client directory:
//   system.data_dir + format("user/%04u/webmail/", usernum)
// (created on the first write).  Files are dir + "webmail." + name + ".json".
// Every read-modify-write goes through update(); never read() then write().
//
//   ud.read(name [, fallback])   parsed contents, or fallback (undefined if
//                                omitted) if missing or unparsable
//   ud.write(name, obj)          replaces the file
//   ud.update(name, fallback, fn [, options])
//                                locks, reads (fallback if absent), writes
//                                fn(contents) and unlocks; returns what was
//                                written.  Throws { code: "conflict" } if the
//                                lock cannot be taken, a TypeError if fn
//                                returns undefined.  options.timeout (ms)
//                                replaces the lock timeout for this call
//                                only (JsonFiles' default is 5000).
//   ud.locked(name, fn [, options])
//                                fn() under the same lock as update(name),
//                                with no file read or written; returns its
//                                result.  Throws as update() does.
//
// Finding JsonFiles: the optional second argument is the JsonFiles
// constructor, for a caller (a test) that must use a particular copy.
// Without it, the copy in exec/load/ of the install this file belongs to
// is used (<this directory>/../../exec/load/, the stock layout), else
// load("mailbox_files.js") through the normal load() search.  js.exec_dir
// cannot be used: it is the directory of the top-level script, not of this
// file; the location comes from Error().fileName as in mailbox.js.
// UserData.source is the path the library was loaded from (set on first
// use without an injected JsonFiles; tests check it).

"use strict";

var UserData = (function () {
	var HERE = (function () {
		var f = String(new Error().fileName || "");
		var i = Math.max(f.lastIndexOf('/'), f.lastIndexOf('\\'));
		return i >= 0 ? f.substr(0, i + 1) : "";
	})();

	var default_jf = null;

	function find_jsonfiles()
	{
		if (default_jf === null) {
			var path = HERE + "../../exec/load/mailbox_files.js";
			if (HERE === "" || !file_exists(path))
				path = "mailbox_files.js";
			UserData.source = path;
			default_jf = load({}, path);
		}
		return default_jf;
	}

	function UserData(dir, JsonFiles)
	{
		var jf = JsonFiles || find_jsonfiles();
		this.files = new jf(dir, "webmail.");
		this.dir = this.files.dir;
	}

	UserData.prototype.read = function (name, fallback)
	{
		return this.files.read(name, fallback);
	};

	UserData.prototype.write = function (name, obj)
	{
		return this.files.write(name, obj);
	};

	// JsonFiles.update() takes its timeout from the instance, so a
	// per-call timeout is set on the instance for the call and restored
	UserData.prototype.update = function (name, fallback, fn, options)
	{
		if (!options || options.timeout === undefined)
			return this.files.update(name, fallback, fn);
		var saved = this.files.timeout;
		this.files.timeout = options.timeout;
		try {
			return this.files.update(name, fallback, fn);
		} finally {
			this.files.timeout = saved;
		}
	};

	// locked(name, fn [, options]): fn() under name's lock, as update()
	// takes it, with no file read or written; returns fn's result.  With
	// a JsonFiles that has no locked() (an older exec/load/mailbox_files.js)
	// the lock is taken through update(), which also rewrites name's file.
	UserData.prototype.locked = function (name, fn, options)
	{
		var files = this.files;
		var take = function () {
			if (typeof files.locked === "function")
				return files.locked(name, fn);
			var result;
			files.update(name, { v: 1 }, function () {
				result = fn();
				return { v: 1 };
			});
			return result;
		};
		if (!options || options.timeout === undefined)
			return take();
		var saved = files.timeout;
		files.timeout = options.timeout;
		try {
			return take();
		} finally {
			files.timeout = saved;
		}
	};

	return UserData;
})();

UserData;
