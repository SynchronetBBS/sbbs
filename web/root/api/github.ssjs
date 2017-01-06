load(system.exec_dir + '../web/lib/init.js');
load('sha1.js');

var payload = http_request.query.payload[0];

var cc = new CryptContext(CryptContext.ALGO['HMAC-SHA1']);
cc.set_key(settings.github_secret);

var f = new File(system.ctrl_dir + '../github.txt');
f.open('w');
f.writeln(cc.encrypt(payload));
f.writeln(cc.encrypt(Sha1.hash(payload)));
f.writeln(http_request.header['x-hub-signature']);
f.close();