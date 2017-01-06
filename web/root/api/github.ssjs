load(system.exec_dir + '../web/lib/init.js');
load('hmac.js');

function a2h(str) {
	return str.split('').map(function(ch) { return parseInt(ascii(ch), 16); }).join('');
}

function h2a(hex) {
	var str = '';
	for (var i = 0; i < hex.length; i += 2) {
		str += ascii(parseInt(hex.substr(i, 2), 16));
	}
	return str;
}

function b2h(str) {
    if (typeof str !== 'string') str += '';
    var ret = '';
    for (var i = 0; i < str.length; i++) {
        var n = ascii(str[i]).toString(16);
        ret += n.length < 2 ? '0' + n : n;
    }
    return ret;
}

var payload = http_request.query.payload[0];

var f = new File(system.ctrl_dir + '../github.txt');
f.open('w');
f.writeln(http_request.header['x-hub-signature'].split('=')[1]);
f.writeln(payload);
f.writeln(':D');
f.close();