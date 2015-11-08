var q = new Queue("dorkit_input");
var k;

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	k = console.inkey(0, 100);
	if (k.length) {
		q.write(k);
	}
}
