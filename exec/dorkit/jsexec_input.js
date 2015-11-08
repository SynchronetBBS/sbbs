js.load_path_list.unshift(js.exec_dir+"/dorkit/");
js.load_path_list.unshift(system.exec_dir+"/dorkit/");
load('ansi_input.js');
var q = new Queue("dorkit_input");
var k;

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	// Can't select() files on Win32.
	if(system.platform == 'Win32') {
		k = read(1);
	}
	else {
		k = undefined;
		if (socket_select([0], 0.1).length == 1)
			k = read(1);
	}
	if (k != undefined && k.length)
		ai.add(k);
}
