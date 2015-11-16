js.load_path_list.unshift(js.exec_dir+"dorkit/");
if (js.global.system !== undefined)
	js.load_path_list.unshift(system.exec_dir+"dorkit/");
load('ansi_input.js');
var k;

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	k = stdin.read(1);
	ai.add(k);
}
