js.load_path_list.unshift(js.exec_dir+"/dorkit/");
js.load_path_list.unshift(system.exec_dir+"/dorkit/");
load('ansi_input.js');
var q = new Queue("dorkit_input");
var k;
var f;
if(system.platform == 'Win32') {
	f = new File('CON');
	if(!f.open('rb', false, 0))
		throw("Unable to open CON device!");
}
else {
	f = new File('/dev/stdin');
	if(!f.open('rb', false, 0))
		throw("Unable to open /dev/stdin!");
}

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	// Can't select() files on Win32.
	if(system.platform == 'Win32') {
		k = f.read(1);
	}
	else {
		k = undefined;
		if (socket_select([f.descriptor], 0.1).length == 1)
			k = f.read(1);
	}
	if (k != undefined && k.length)
		ai.add(k);
}
