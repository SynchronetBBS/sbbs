load('sbbsdefs.js');
var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };

load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'files.js');
var request = require({}, settings.web_lib + 'request.js', 'request');
var Filebase = require({}, 'filebase.js', 'OldFileBase');

var CHUNK_SIZE = 1024;

var reply = {};
if ((http_request.method === 'GET' || http_request.method === 'POST') && request.hasParam('call') && user.number > 0) {

	switch (request.getParam('call').toLowerCase()) {
		case 'download-file':
			var dir = request.getParam('dir');
			if (dir !== undefined
				&& file_area.dir[dir] !== undefined && file_area.dir[dir].lib_index >= 0 && file_area.dir[dir].index >= 0 && file_area.dir[dir].can_download
				&& request.hasParam('file')
				&& user.compare_ars(file_area.dir[dir].download_ars)
			) {
				var dircode = file_area.dir[dir].code;
				var fn = request.getParam('file').toLowerCase();
				var fileBase = new OldFileBase(dircode);
				var file = null;
				fileBase.some(function (e) {
					if (e.name.toLowerCase() !== fn) {
						return false;
					} else if (e.path !== undefined) {
						file = e;
						return true;
					}
				});
				fileBase = undefined;
				if (file === null) {
					reply.error = 'File not found';
					break;
				}
				if (!file_area.dir[dir].is_exempt && file.credits > (user.security.credits + user.security.free_credits)) {
					reply.error = 'Not enough credits to download this file';
					break;
				}
				var mt;
				if (!settings.files_inline || settings.files_inline_blacklist.indexOf(file.ext) > -1) {
					mt = 'application/octet-stream';
				} else {
					mt = getMimeType(file);
				}
				http_reply.header['Content-Type'] = mt;
				if (mt === 'application/octet-stream') {
					http_reply.header['Content-Disposition'] = 'attachment; filename="' + file.name + '"';
				} else {
					http_reply.header['Content-Disposition'] = 'inline';
				}
				http_reply.header['Content-Encoding'] = 'binary';
				http_reply.header['Content-Length'] = file_size(file.path);
				var f = new File(file.path);
				f.open('rb');
				for (var n = 0; n < f.length; n += CHUNK_SIZE) {
					var r = f.length - f.position;
					write(f.read(r > CHUNK_SIZE ? CHUNK_SIZE : r));
					yield(false);
				}
				f.close();
				f = undefined;
				reply = false;
				user.downloaded_file(dircode, file.path);
			}
			break;
		default:
			break;
	}

}

if (!reply) exit();

reply = JSON.stringify(reply);
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = reply.length;
write(reply);

reply = undefined;
