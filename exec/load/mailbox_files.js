// JSON side files with locked read-modify-write, for the mailbox library.
//
// var JsonFiles = load("mailbox_files.js");
// var jf = new JsonFiles(dir, prefix [, { timeout: ms, max_age: seconds }]);
//
// Every read-modify-write goes through update(); never read() then write().
//
// A lock's age is judged by this host's clock against the lock file's
// modification time, which the file server sets: hosts sharing the data
// directory need clocks that agree to well within max_age, or each takes
// the other's fresh locks for stale ones.

"use strict";

function JsonFiles(dir, prefix, options)
{
	var c = dir.charAt(dir.length - 1);
	this.dir = (c == '/' || c == '\\') ? dir : dir + '/';
	this.prefix = prefix;
	options = options || {};
	this.timeout = options.timeout !== undefined ? options.timeout : 5000;	// ms
	this.max_age = options.max_age !== undefined ? options.max_age : 60;	// seconds
}

JsonFiles.prototype.path = function (name)
{
	return this.dir + this.prefix + name + ".json";
};

JsonFiles.prototype.lock_path = function (name)
{
	return this.dir + this.prefix + name + ".lock";
};

// Returns the parsed contents, or fallback if missing or unparsable
JsonFiles.prototype.read = function (name, fallback)
{
	var f = new File(this.path(name));
	if (!f.open("r"))
		return fallback;
	var text = f.read();
	f.close();
	if (!text)
		return fallback;
	try {
		return JSON.parse(text);
	} catch (e) {
		return fallback;
	}
};

// A token unique to this process, object and call, for temp names and lock
// ownership
JsonFiles.prototype.token = function ()
{
	return system.timer + "-" + random(0x7fffffff) + "-" + (JsonFiles.seq++);
};
JsonFiles.seq = 0;

// Gives the file at path (in the directory) the contents text: writes
// to a uniquely named temp file beside the target, then renames it
// over the target.  The temp file is renamed only after it was written,
// flushed and closed without error; otherwise it is removed and an Error
// naming the path is thrown, and the target is left as it was.
// file_rename() will not replace an existing file on Windows, so there the
// existing file is removed first (not atomic: a reader can briefly see no
// file and take its fallback; writers are serialized by update()).  The
// remove can fail while a reader has the file open (Windows, or a Windows
// host over SMB), so the rename (and the remove-then-rename) is tried
// RENAME_TRIES times, RENAME_WAIT ms apart.  If every try fails the temp
// file is removed, the old file may be gone, and an Error is thrown.
JsonFiles.RENAME_TRIES = 10;
JsonFiles.RENAME_WAIT = 25;	// ms

JsonFiles.prototype.replace = function (path, text)
{
	var tmp = path + "." + this.token() + ".tmp";

	mkpath(this.dir);
	var f = new File(tmp);
	if (!f.open("w"))
		throw new Error("cannot create " + tmp + ": " + f.error);
	var ok = f.write(text) && f.flush() && f.error === 0;
	f.close();
	if (!ok || file_size(tmp) < text.length) {
		file_remove(tmp);
		throw new Error("cannot write " + path);
	}
	for (var i = 0; i < JsonFiles.RENAME_TRIES; i++) {
		if (i > 0)
			mswait(JsonFiles.RENAME_WAIT);
		if (file_rename(tmp, path))
			return;
		if (file_exists(path))
			file_remove(path);
		if (file_rename(tmp, path))
			return;
	}
	file_remove(tmp);
	throw new Error("cannot rename " + tmp + " to " + path);
};

// Writes obj as name's file (see replace())
JsonFiles.prototype.write = function (name, obj)
{
	this.replace(this.path(name), JSON.stringify(obj));
};

// Removes the lock file only if it still holds our token (it may have been
// taken over by another holder after max_age)
JsonFiles.prototype.unlock = function (lock, token)
{
	var f = new File(lock);
	if (!f.open("r"))
		return;
	var text = f.read();
	f.close();
	if (text === token)
		file_remove(lock);
};

// Lock name's lock file, run fn(), unlock; returns what fn returned.  The
// file name.json itself is neither read nor written: for a lock that
// guards other files.  Throws { code: "conflict" } if the lock cannot be
// taken within the timeout.
//
// The lock file's text is a unique token, and the lock is released only if
// the file still holds that token.  A holder that takes longer than max_age
// can still have its lock taken over (both then run concurrently, and the
// slow one's release leaves the new holder's lock alone), so locked() and
// update() are for short work only.
JsonFiles.prototype.locked = function (name, fn)
{
	var lock = this.lock_path(name);
	var deadline = system.timer + this.timeout / 1000;
	var token = this.token();

	mkpath(this.dir);
	while (!file_mutex(lock, token, this.max_age)) {
		if (system.timer >= deadline)
			throw { code: "conflict", message: "locked: " + lock };
		mswait(50);
	}
	try {
		return fn();
	} finally {
		this.unlock(lock, token);
	}
};

// Lock, read (fallback if absent), obj = fn(obj), write obj, unlock (as
// locked()).  Returns the written object.  Throws { code: "conflict" } if
// the lock cannot be taken within the timeout, and a TypeError (before
// anything is written) if fn returns undefined.
JsonFiles.prototype.update = function (name, fallback, fn)
{
	var self = this;
	return this.locked(name, function () {
		var obj = fn(self.read(name, fallback));
		if (obj === undefined)
			throw new TypeError("update function returned undefined for " + self.path(name));
		self.write(name, obj);
		return obj;
	});
};

JsonFiles;
