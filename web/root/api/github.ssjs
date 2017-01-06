load(system.exec_dir + '../web/lib/init.js');

var f = new File(system.ctrl_dir + '../github.txt');
f.open('w');
//f.write(JSON.stringify(http_request.header));
//f.write(JSON.stringify(http_request));
f.writeln(JSON.stringify(Object.keys(http_request.query)));
f.writeln(http_request.header['x-hub-signature']);
f.close();