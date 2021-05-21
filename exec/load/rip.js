/*
 * This will ensure that the remote system has the required
 * RIP files if the remote terminal supports RIP.
 */

require("userdefs.js", "USER_RIP");

var RIP = new function() {
	var major = 0;
	var minor = 0;
	var vendor = 0;
	var vsub = 0;
	var queried_version = false;

	function query(str, firstchar, resplen) {
		var result = '';
		var oldctrl = console.ctrlkey_passthru;

		console.ctrlkey_passthru = -1;
		console.write(str);
		while (1) {
			ch = console.inkey(0, 3000);
			if (ch == '')
				break;
			if (result.length == 0 && firstchar !== undefined && ch != firstchar)
				continue;
			if (ch == '\n')
				break;
			result += ch;
			if (result.length == resplen)
				break;
		}
		console.ctrlkey_passthru= oldctrl;
		return result;
	}

	Object.defineProperty(this, 'supported',  {
		configurable: false,
		enumerable: true,
		get: function() {
			var ch;
			var result = '';
			var oldctrl = console.ctrlkey_passthru;
			var m;

			if (major > 0)
				return true;
			if (console.autoterm & USER_RIP)
				return true;
			if (queried_version)
				return false;
			result = query('\x1b[!', 'R', 14);
			queried_version = true;
			m =result.match(/RIPSCRIP([0-9][0-9])([0-9][0-9])([0-9])([0-9])/);

			if (m == null)
				return false;
			major = parseInt(m[1]);
			minor = parseInt(m[2]);
			vendor = parseInt(m[1]);
			vsub = parseInt(m[1]);
			if (major > 0)
				return true;
			return false;
		}
	});

	this.loadicons = function(archive) {
		var files;
		var need = [];
		var path;
		var aborted = false;

		if (!this.supported)
			return false;
		bbs.line_counter = 0;
		print("Checking RIP icons...");
		if (typeof archive != 'Archive') {
			if (file_exists(archive))
				archive = new Archive(archive);
			else
				return false;
		}

		files = archive.list();
		files.forEach(function(fobj) {
			var ext;
			var result;
			var m;

			if (fobj.type != 'file')
				return;
			ext = file_getext(fobj.name).toLowerCase();
			// TODO: Are any other types appropriate?
			if (ext !== '.rip' && ext !== '.icn')
				return;
			bbs.line_counter = 0;
			// TODO: Get/test date as well
			result = query('!|1F020000'+fobj.name+'\r\n');
			if (result.indexOf('0') == 0) {
				print("Need: "+fobj.name+" (no such file)");
				need.push(fobj.name);
				return;
			}
			m = result.match(/1\.([0-9]+)/);
			if (m === null) {
				print("Need: "+fobj.name+" (bad response: '"+result+"')");
				need.push(fobj.name);
				return;
			}
			if (parseInt(m[1], 10) !== fobj.size) {
				print("Need: "+fobj.name+" (size mismatch)");
				need.push(fobj.name);
				return;
			}
			print("Have: "+fobj.name);
		});

		if (need.length === 0)
			return true;
		need.unshift(false);
		path = system.temp_dir;
		path += 'ripicons' + bbs.node_num;
		path = backslash(path);
		mkpath(path);
		need.unshift(path);
		archive.extract.apply(archive, need);
		need.shift();
		need.shift();
		need.forEach(function(fname) {
			var type;
			var ext;

			if (aborted) {
				file_remove(path + fname);
				return;
			}
			ext = file_getext(fname).toLowerCase();
			if (ext === '.rip')
				type = '01';
			if (ext === '.icn')
				type = '02';
			// TODO: Are any other types appropriate?
			if (ext !== '.rip' && ext !== '.icn') {
				file_remove(path + fname);
				return;
			}
			bbs.line_counter = 0;
			console.write('Sending: '+fname+'\r\n!|9\x1b06' + type + '0000<>\r\n');
			// TODO: Somehow make sure 'G' is YModem-G?
			if (!bbs.send_file(path + fname, 'G', 'RIP asset', false))
				aborted = true;
		});
		rmdir(path);
		return !aborted;
	}
}();
