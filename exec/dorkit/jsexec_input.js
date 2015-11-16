js.load_path_list.unshift(js.exec_dir+"dorkit/");
if (js.global.system !== undefined)
	js.load_path_list.unshift(system.exec_dir+"dorkit/");
load('ansi_input.js');
var k;

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	if (system.platform == 'Win32')
		k = stdin.raw_read(1);
	else {
		if (socket_select([stdin.descriptor], 0.1).length == 1)
			k = stdin.raw_read(1);
		else
			k = undefined;
	}
	ai.add(k);
}
