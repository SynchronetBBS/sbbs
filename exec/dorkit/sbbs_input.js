js.load_path_list.unshift(js.exec_dir+"/dorkit/");
js.load_path_list.unshift(system.exec_dir+"/dorkit/");
load('ansi_input.js');
var q = new Queue("dorkit_input");
var k;

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	k = console.inkey(0, 100);
	ai.add(k);
}
