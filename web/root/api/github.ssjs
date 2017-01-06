load(system.exec_dir + '../web/lib/init.js');
load('hmac.js');

function a2h(str) {
    var hex = '';
    str.split('').forEach(function(ch) { hex += parseInt(ascii(ch), 16); });
    return hex;
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
f.writeln(b2h(hmac_sha1(settings.github_secret, a2h(payload))));
f.writeln(b2h(hmac_sha1(settings.github_secret, payload)));
f.writeln(h2a(b2h(hmac_sha1(settings.github_secret, payload))));
f.writeln(http_request.header['x-hub-signature'].split('=')[1]);
f.close();