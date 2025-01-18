var f = new File(js.exec_dir + 'test.crt');
f.open('r');
f.readln(65535);
var b64 = f.readln(65535);
var raw = base64_decode(b64, 819);
var c = new CryptCert(raw);
for (var i in c) {
	var val = c[i];
	if (val === undefined)
		continue;
}
var x = JSON.stringify(c);
