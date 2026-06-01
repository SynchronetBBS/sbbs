function xjs_compile(filename) {
	if(typeof cwd == 'string' && cwd != '') {
		if(filename.search(/^((\/)|([A-Za-z]:[\/\\]))/)==-1)
			filename=cwd+filename;
	}
	cwd=filename;
	cwd=backslash(cwd.replace(/[^\\\/]*$/,''));
	var ssjs_filename=filename+".ssjs";

	// (Re)compile only when the cached .ssjs is missing or older than its
	// source.  Use strict '<' so a cache written in the same wall-clock second
	// as its source isn't treated as stale.  We deliberately do NOT remove the
	// stale cache up front: the recompiled output is written to a unique temp
	// file and atomically renamed into place below, so a concurrent request
	// never observes a missing or half-written .ssjs.  (The old code did
	// file_remove() then recompiled in place, leaving the file absent for the
	// whole compile -- racing concurrent load()s into "Script file ... does not
	// exist" / "creating ... No such file or directory" errors on a cold cache.)
	if(!file_exists(ssjs_filename) || file_date(ssjs_filename)<file_date(filename)) {
		var file = new File(filename);
		if(!file.open("r",true,8192)) {
			writeln("!ERROR " + file.error + " opening " + file.name);
			throw new Error(format("%d (%s)", file.error, strerror(file.error)) + " opening " + file.name);
		}
		var text = file.readAll(8192);
		file.close();

		var script="";

		var in_xjs=false;
		for (line in text) {
			var str=text[line];
			while(str != '') {
				if(!in_xjs) {
					if(str.search(/<\?(xjs)?\s+/)==-1) {
						var ln=true;
						if(str.substr(-5)=='<?xjs') {
							str=str.substr(0, str.length-5);
							in_xjs=true;
							ln=false;
						}
						else if(str.substr(-2)=='<?') {
							str=str.substr(0, str.length-2);
							in_xjs=true;
							ln=false;
						}
						if(str != '') {
							if(ln)
								script += "writeln("+JSON.stringify(str)+");";
							else
								script += "write("+JSON.stringify(str)+");";
						}
						str='';
					}
					else {
						str=str.replace(/^(.*?)<\?(xjs)?\s+/,
							function (str, p1, p2, offset, s) {
								if(p1 != '')
									script += "write("+JSON.stringify(p1)+");";
								in_xjs=true;
								return '';
							}
						);
					}
				}
				else {
					if(str.search(/\?>/)==-1) {
						script += str;
						str='';
					}
					else {
						str=str.replace(/^(.*?)\?>/,
							function (str, p1, offset, s) {
								script += p1+";";
								in_xjs=false;
								return '';
							}
						);
					}
				}
			}
			script += '\n';
		}

		// Write to a unique temp file, then move it into place.  file_rename()
		// is atomic and replaces the target on POSIX; on Win32 it fails if the
		// target exists, so fall back to remove-then-rename there (a far smaller
		// window than recompiling in place).  Two random() draws make the temp
		// name collision-proof across concurrent requests.
		var tmp_filename=format("%s.%08x%08x.tmp", ssjs_filename, random(0x7fffffff), random(0x7fffffff));
		var f=new File(tmp_filename);
		if(!f.open("w",false)) {
			throw new Error(format("%d (%s)", f.error, strerror(f.error)) + " creating " + f.name);
		}
		f.write(script);
		f.close();
		if(!file_rename(tmp_filename, ssjs_filename)) {
			// POSIX rename() replaces the target atomically above and never gets
			// here; reaching this point means Win32, where rename() won't
			// overwrite an existing file.
			if(file_exists(ssjs_filename) && file_date(ssjs_filename)>=file_date(filename)) {
				// A concurrent request already produced an up-to-date cache, so
				// ours is redundant.  Discard our temp WITHOUT removing the
				// target -- removing it here is the residual race that left a
				// concurrent load() with "Script file ... does not exist".
				file_remove(tmp_filename);
			}
			else {
				// Target is missing or genuinely stale (an actual source edit):
				// replace it.  This leaves a brief absence window, but only on a
				// real .xjs change -- rare, and not under cold-cache fan-out.
				file_remove(ssjs_filename);
				if(!file_rename(tmp_filename, ssjs_filename)) {
					file_remove(tmp_filename);
					throw new Error("error renaming " + tmp_filename + " to " + ssjs_filename);
				}
			}
		}
	}
	return(ssjs_filename);
}

function xjs_eval(filename, str) {
	const ssjs = xjs_compile(filename);
	const f = new File(filename + '.html');
	if (!f.open('w+', false)) {
		throw new Error(format("%d (%s)", f.error, strerror(f.error)) + " creating " + f.name);
	}
	function write(s) {
		f.write(s);
	}
	function writeln(s) {
		f.writeln(s);
	}
	load(ssjs);
	if (str) {
		f.rewind();
		const ret = f.read();
		f.close();
		f.remove();
		return ret;
	} else {
		f.close();
		return filename + '.html';
	}
}
