js.load_path_list.unshift(js.exec_dir+"dorkit/");
if (typeof(system) !== 'undefined')
	js.load_path_list.unshift(system.exec_dir+"dorkit/");
load('ansi_input.js', undefined);
var k;

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	if (stdin.raw_pollin(100)) {
		k = stdin.raw_read(1);
		ai.add(k);
	}
}
ai.close();
