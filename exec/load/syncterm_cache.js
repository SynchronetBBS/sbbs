function SyncTERMCache() {
	this.supported = this.supports_syncterm_cache();
};

// TODO: Should likely be in a different file... or even better,
//       be a native JS method or something.
SyncTERMCache.prototype.read_apc = function (timeout)
{
	var ret = '';
	var ch;
	var state = 0;
	var oldpt = console.ctrlkey_passthru;
	console.ctrlkey_passthru = -1;
	if (timeout === undefined)
		timeout = 1000;

	while((ch = console.inkey(timeout)) !== null) {
		switch(state) {
			case 0:
				if (ch == '\x1b') {
					state++;
					break;
				}
				break;
			case 1:
				if (ch == '_') {
					state++;
					break;
				}
				state = 0;
				break;
			case 2:
				if (ch == '\x1b') {
					state++;
					break;
				}
				ret += ch;
				break;
			case 3:
				console.ctrlkey_passthru = oldpt;
				if (ch == '\\') {
					return ret;
				}
				return undefined;
		}
	}
	console.ctrlkey_passthru = oldpt;
	return undefined;
}

SyncTERMCache.prototype.supports_syncterm_cache = function ()
{
	var stat;

	console.write('\x1b_SyncTERM:C;L;test\x1b\\');
	stat = this.read_apc();
	if (stat != undefined)
		return true;
	return false;
}

SyncTERMCache.prototype.upload = function (fname, cache_fname)
{
	if (cache_fname === undefined)
		cache_fname = fname;
	var path;
	if (fname[0] != '/' && fname[1] != ':')
		path = js.exec_dir + '/' + fname;
	else
		path = fname;
	if (this.supported === undefined)
		this.supported = this.supports_syncterm_cache();
	if (!this.supported)
		return false;

	// Read file MD5
	var f = new File(path);
	if (!f.open("rb", true))
		return false;
	var hash = f.md5_hex;
	if (typeof hash !== 'string') {
		f.close();
		return false;
	}
	if (hash.length != 32) {
		f.close();
		return false;
	}

	// Check for the file in the cache...
	console.write('\x1b_SyncTERM:C;L;'+cache_fname+'\x1b\\');
	var lst = this.read_apc();
	var idx = lst.indexOf('\n' + cache_fname + '\t');
	if (idx !== -1) {
		idx += 2;
		idx += cache_fname.length;
		if (hash == lst.substr(idx, 32)) {
			f.close();
			return true;
		}
	}

	f.base64 = true;
	console.write("\x1b_SyncTERM:C;S;"+cache_fname+";");
	var buf;
	while((!js.terminated) && bbs.online) {
		var rdlen = console.output_buffer_space;
		if (rdlen < 4) {
			mswait(1);
			continue;
		}
		rdlen = parseInt(rdlen / 4, 10);
		rdlen *= 3;
		buf = f.read(rdlen);
		if (buf.length === 0)
			break;
		console.write(buf);
	}
	console.write("\x1b\\");
	f.close();
	return true;
}
