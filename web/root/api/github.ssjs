load(system.exec_dir + '../web/lib/init.js');

var cc = new CryptContext(CryptContext.ALGO['HMAC-SHA1']);
cc.set_key(settings.github_secret);
var secret = cc.decrypt(http_request.header['x-hub-signature'].split('=')[1]);

var f = new File(system.ctrl_dir + '../github.txt');
f.open('w');
//f.write(JSON.stringify(http_request.header));
//f.write(JSON.stringify(http_request));
f.writeln('Secret: ' + settings.github_secret);
f.writeln('Received: ' + http_request.header['x-hub-signature']);
f.writeln('Decrypted: ' + secret);
f.close();