if (system.platform == 'Win32')
	exit();

var f = new File("/bin/ls");
if (!f.popen("r"))
	throw new Error('popen() failed! {f.error}');
while(f.is_open) {
	var l = f.readln();
	if (!l)
		f.close();
}
