load(system.exec_dir + '../web/lib/init.js');
load('hmac.js');

var payload = http_request.query.payload[0];

var f = new File(system.ctrl_dir + '../github.txt');
f.open('w');
f.writeln(hmac_sha1(settings.github_secret, payload));
f.writeln(http_request.header['x-hub-signature']);
f.close();