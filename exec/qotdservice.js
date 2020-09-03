// qotdservice.js

// Synchronet Service for the Quote of The Day protocol (RFC 865)

// $Id: qotdservice.js,v 1.1 2007/08/13 04:25:41 deuce Exp $


const REVISION = "$Revision: 1.1 $".split(' ')[1];

var output_buf = "";

// Write a string to the client socket
function write(str)
{
	output_buf += str;
}

// Write all the output at once
function flush()
{
	client.socket.send(output_buf);
}

function done()
{
	flush();
	exit();
}

automsg=new File(system.data_dir+"msgs/auto.msg");
if(!automsg.open("r",true)) {
	log("Error opening "+system.data_dir+"msgs/auto.msg for read");
	write("No AutoMessage set\r\n");
	done();
}

while(1) {
	if(!client.socket.is_connected)
		done();
	b=automsg.readBin(1);
	if(automsg.eof)
		done();
	if(b==1) {	// CTRL-A... eat it
		b=automsg.readBin(1);
		if(b!=1)
			continue;
	}
	if(b < 32) {
		switch(b) {
			case 9:
			case 10:
			case 13:
				break;
			default:
				continue;
		}
	}
	write(ascii(b));
}
