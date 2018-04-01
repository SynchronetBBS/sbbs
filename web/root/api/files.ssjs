load('modopts.js');
var settings = get_mod_options('web');

load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'files.js');
load('filebase.js');

var CHUNK_SIZE = 1024;

var reply = {};
if ((http_request.method === 'GET' || http_request.method === 'POST') &&
	typeof http_request.query.call !== 'undefined' && user.number > 0
) {

	switch (http_request.query.call[0].toLowerCase()) {
		case 'download-file':
			if (typeof http_request.query.dir !== 'undefined' &&
				typeof file_area.dir[http_request.query.dir[0]] !== 'undefined'	&&
				file_area.dir[http_request.query.dir[0]].can_download &&
				typeof http_request.query.file !== 'undefined'
			) {
				var fileBase = new FileBase(file_area.dir[http_request.query.dir[0]].code);
				var file = null;
				fileBase.some(
					function (e) {
						if (e.base.toLowerCase() + '.' + e.ext.toLowerCase() !== http_request.query.file[0].toLowerCase()) {
							return false;
						} else if (typeof e.path !== 'undefined') {
							file = e;
							return true;
						}
					}
				);
				if (file === null) break;
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
