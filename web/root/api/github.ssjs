load(system.exec_dir + '../web/lib/init.js');
load('sha1.js');

var payload = http_request.query.payload[0];

var cc = new CryptContext(CryptContext.ALGO['HMAC-SHA1']);
cc.set_key(settings.github_secret);
var pe = cc.encrypt(payload);

var f = new File(system.ctrl_dir + '../github.txt');
f.open('w');
f.writeln(Sha1.hash(pe));
f.writeln(cc.encrypt(Sha1.hash(payload)));
f.writeln(http_request.header['x-hub-signature']);
f.close();