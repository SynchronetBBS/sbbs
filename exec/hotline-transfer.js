load("hotline_funcs.js");

function rage_quit(reason)
{
	log(LOG_DEBUG, "Rage quit: "+reason);
	client.socket.close();
	exit();
}

function null_str(len)
{
	var i;
	var ret='';

	for (i=0; i<len; i++) {
		ret += '\x00';
	}

	return ret;
}

var protocol = client.socket.recvBin(4);
if (protocol != 0x48545846)
	rage_quit("Incorrect protocol specifier");
var ref = client.socket.recvBin(4);
var size = client.socket.recvBin(4);
client.socket.recvBin(4);

var f = new File(system.temp_dir+'hotline.transfers');
var path;
if (f.open("r+b")) {
	var lines=f.readAll(2048);
	var i;

	for (i in lines) {
		var m = lines[i].match(/^([0-9]+) ([^\s]+) ([0-9]+) (.*)$/);

		if (m == null)
			rage_quit("Unable to parse line "+lines[i]);

		if (parseInt(m[1], 10) == ref && m[2] == client.ip_address) {
			lines.splice(i, 1);
			path = m[4];
			break;
		}
	}
	f.truncate();
	f.writeAll(lines);
	f.close();
	if (path == undefined)
		rage_quit("Unable to find reference number");
}
else
	rage_quit("Unable to open hotline.transfers");

f = new File(path);

// Flat file header
client.socket.write('FILP'+encode_short(1)+null_str(16)+encode_short(2));

// Information fork header
client.socket.write('INFO'+encode_long(0)+null_str(4)+encode_long(94));

// Informmation fork
client.socket.write('AMACTYPECREA'+encode_long(0)+encode_long(0x100)+null_str(32)+time_to_timestamp(f.date)+time_to_timestamp(f.date)+encode_short(0)+encode_short(3)+'hxd\x00\x11                 ');

// Data fork header
client.socket.write('DATA'+encode_long(0)+null_str(4)+encode_long(f.length));

// Data fork
client.socket.sendfile(path);

for(i=0; i<30; i++) {
	if(!client.socket.is_connected)
		break;
	mswait(1000);
}
