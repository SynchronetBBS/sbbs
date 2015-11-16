js.load_path_list.unshift(js.exec_dir+"dorkit/");
if (js.global.system !== undefined)
	js.load_path_list.unshift(system.exec_dir+"dorkit/");
load('ansi_input.js');
var i;
var q = new Queue("dorkit_input");
var k;
var sock=new Socket(true, argv[0]);
if (!sock.is_connected)
	throw("Unable to create socket object");
var telnet={
	enabled:false,
	last:'',
	option:'',
	interpret:function(ch) {
		// TODO: This needs a LOT of work.
		if (!this.enabled)
			return ch;
		if (this.option !== '') {
			this.option='';
			switch(this.option) {
				case '\xfb':	// WILL
					sock.send('\xff\xfe'+ch);	// Send DON'T
					break;
				case '\xfd':	// DO
					sock.send('\xff\xfc'+ch);	// Send WON'T
					break;
			}
			return '';
		}
		if (this.last == '\xff') {
			this.last = '';
			if (ch == '\xff')
				return ch;
			switch(ch) {
				case '\xfb':	// WILL
				case '\xfd':	// DO
					this.option=ch;
					break;
			}
			return '';
		}
		if (this.last == '\r') {
			last = '';
			if (ch == '\n' || ch == '\x00')
				return '\r';
			return this.last+ch;
		}
		switch(ch) {
			case '\r':
			case '\xff':
				last = ch;
				return '';
		}
		return ch;
	}
};

if (argc > 1)
	telnet.enabled=argv[1];
else
	telnet.enabled=false;

while(!js.terminated) {
	if (parent_queue.poll(0))
		break;
	k = undefined;
	if (socket_select([sock], 0.1).length == 1) {
		k = telnet.interpret(read(1));
	}
	if (k != undefined && k.length) {
		for(i=0; i<k.length; i++)
			ai.add(k.substr(i,1));
	}
	else
		ai.add('');
}
