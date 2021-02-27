load('sbbsdefs.js');
var settings = load('modopts.js', 'web');

load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
require('filebase.js', 'FileBase');

var CHUNK_SIZE = 1024;

var reply = {};
if ((http_request.method === 'GET' || http_request.method === 'POST') &&
	typeof http_request.query.call !== 'undefined' && user.number > 0
) {

	switch (http_request.query.call[0].toLowerCase()) {
		case 'download-file':
			if (typeof http_request.query.dir !== 'undefined' &&
				typeof file_area.dir[http_request.query.dir[0]] !== 'undefined'	&&
                file_area.dir[http_request.query.dir[0]].lib_index >= 0 &&
                file_area.dir[http_request.query.dir[0]].index >= 0 &&
				file_area.dir[http_request.query.dir[0]].can_download &&
				typeof http_request.query.file !== 'undefined'
			) {
				var dircode = file_area.dir[http_request.query.dir[0]].code;
				var fileBase = new FileBase(dircode);
				var file = null;
				fileBase.some(function (e) {
					if (e.base.toLowerCase() + '.' + e.ext.toLowerCase() !== http_request.query.file[0].toLowerCase()) {
						return false;
					} else if (typeof e.path !== 'undefined') {
						file = e;
						return true;
					}
				});
				if (file === null) {
					reply.error = 'File not found';
					break;
				}
				if (!file_area.dir[dircode].is_exempt && file.credits > (user.security.credits + user.security.free_credits)) {
					reply.error = 'Not enough credits to download this file';
					break;
				}
				http_reply.header['Content-Type'] = 'application/octet-stream';
				http_reply.header['Content-Disposition'] = 'attachment; filename="' + file.base + '.' + file.ext + '"';
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
