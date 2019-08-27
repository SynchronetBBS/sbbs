js.load_path_list.unshift(js.exec_dir+"dorkit/");
if (typeof(system) !== 'undefined')
	js.load_path_list.unshift(system.exec_dir+"dorkit/");
load('ansi_input.js', argv[0]);
var k;

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	if (parent_queue.orphan)
		break;
	k = console.inkey(0, 100);
	ai.add(k);
}
