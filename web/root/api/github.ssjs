load(system.exec_dir + '../web/lib/init.js');

var cc = new CryptContext(CryptContext.ALGO['HMAC-SHA1']);
cc.set_key(settings.github_secret);
cc.encrypt(JSON.stringify(http_request.query));
var barf = cc.hashvalue;

var f = new File(system.ctrl_dir + '../github.txt');
f.open('w');
//f.write(JSON.stringify(http_request.header));
//f.write(JSON.stringify(http_request));
f.writeln(JSON.stringify(http_request.query));
f.writeln(barf);
f.writeln(http_request.header['x-hub-signature']);
f.close();