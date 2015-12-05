load(system.exec_dir + '../web/lib/init.js');
load(settings.web_lib + 'auth.js');
load(settings.web_lib + 'files.js');
load('filedir.js');

var reply = {};

if ((http_request.method === "GET" || http_request.method === "POST") &&
	typeof http_request.query.call !== 'undefined' &&
	user.number > 0	&&
	user.alias !== settings.guest
) {

	switch (http_request.query.call[0].toLowerCase()) {
		case 'download-file':
			reply = false;
			if(	typeof http_request.query.dir !== 'undefined' &&
				typeof file_area.dir[http_request.query.dir[0]] !== 'undefined'	&&
				file_area.dir[http_request.query.dir[0]].can_download &&
				typeof http_request.query.file !== 'undefined'
			) {
				var fileDir = new FileDir(file_area.dir[http_request.query.dir[0]]);
				var file = null;
				for (var f = 0; f < fileDir.files.length; f++) {
					if (fileDir.files[f].name.toLowerCase() !==
						http_request.query.file[0].toLowerCase()
					) {
						continue;
					}
					file = fileDir.files[f];
					break;
				}
				if (file === null) break;
				client.socket.send('HTTP/1.0 Status: 200 OK\r\n');
				client.socket.send('Content-Type: application/octet-stream\r\n');
				client.socket.send('Content-Disposition: attachment; filename="' + file.name + '";\r\n');
				client.socket.send('Content-Transfer-Encoding: binary\r\n');
				client.socket.send('Content-Length: ' + file_size(file.fullPath) + '\r\n');
				client.socket.send('\r\n');
				var f = new File(file.fullPath);
				f.open('r+b');
				while (!f.eof) {
					client.socket.sendBin(f.readBin(1), 1);
				}
				f.close();
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