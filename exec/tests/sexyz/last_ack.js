// The receiver's last ACK of a transfer has to reach the wire before sexyz
// exits (#1300): the YMODEM batch-terminating ACK, which follows the empty
// header block with nothing after it to prompt a flush, and the XMODEM EOT
// ACK, queued immediately before the process exits.  Plays the sender byte
// for byte against sexyz's stdio mode over a bidirectional File.popen().

var SOH = 0x01;
var EOT = 0x04;
var ACK = 0x06;
var NAK = 0x15;
var C = "C".charCodeAt(0);
var CPMEOF = 0x1a;
var BLOCK_SIZE = 128;

var sexyz = js.exec_dir + "../../sexyz";
if (!file_exists(sexyz))
	sexyz = system.exec_dir + "sexyz";

// Not a multiple of the block size, so the last block carries padding.
var payload = "";
for (var i = 0; payload.length < 300; i++)
	payload += "sexyz #1300 line " + i + "\r\n";
payload = payload.substr(0, 300);

function shell_quote(str)
{
	return "'" + str.replace(/'/g, "'\''") + "'";
}

function str_bytes(str)
{
	var bytes = [];
	for (var i = 0; i < str.length; i++)
		bytes.push(str.charCodeAt(i) & 0xff);
	return bytes;
}

function name(ch)
{
	if (ch < 0)
		return "EOF";
	switch (ch) {
		case SOH: return "SOH";
		case EOT: return "EOT";
		case ACK: return "ACK";
		case NAK: return "NAK";
		case 0x18: return "CAN";
	}
	if (ch >= 0x20 && ch < 0x7f)
		return "'" + String.fromCharCode(ch) + "'";
	return format("%02Xh", ch);
}

function crc16(bytes)	// CRC-16/XMODEM
{
	var crc = 0;
	for (var i = 0; i < bytes.length; i++) {
		crc ^= bytes[i] << 8;
		for (var b = 0; b < 8; b++)
			crc = ((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1) & 0xffff;
	}
	return crc;
}

// One CRC-mode block of BLOCK_SIZE bytes, padded with 'pad'
function block(num, data, pad)
{
	var body = data.slice(0, BLOCK_SIZE);
	while (body.length < BLOCK_SIZE)
		body.push(pad);
	var crc = crc16(body);
	return [SOH, num & 0xff, 0xff - (num & 0xff)].concat(body, [crc >> 8, crc & 0xff]);
}

function Receiver(args, logpath)
{
	this.logpath = logpath;
	this.file = new File(shell_quote(sexyz) + " " + args + " 2>" + shell_quote(logpath));
	if (!this.file.popen("r+", /* unbuffered: */ 0))
		throw new Error("popen(" + this.file.name + ") failed: " + this.file.error);
}

Receiver.prototype.send = function(bytes)
{
	if (!this.file.writeBin(bytes, 1))
		throw new Error("write of " + bytes.length + " bytes failed: " + this.file.error
			+ " (receiver log: " + this.logpath + ")");
};

Receiver.prototype.expect = function(ch, what)
{
	var got = this.file.readBin(1);
	if (got !== ch)
		throw new Error("expected " + name(ch) + " " + what + ", got " + name(got)
			+ " (receiver log: " + this.logpath + ")");
};

Receiver.prototype.close = function()
{
	this.file.close();
};

// Send 'bytes' as numbered data blocks followed by the double-EOT handshake
Receiver.prototype.send_file = function(bytes)
{
	var num;

	for (num = 1; (num - 1) * BLOCK_SIZE < bytes.length; num++) {
		this.send(block(num, bytes.slice((num - 1) * BLOCK_SIZE, num * BLOCK_SIZE), CPMEOF));
		this.expect(ACK, "for block " + num);
	}
	this.send([EOT]);
	this.expect(NAK, "for the first EOT");
	this.send([EOT]);
	this.expect(ACK, "for the second EOT");
};

function check_received(path, what)
{
	var file = new File(path);
	if (!file.open("rb"))
		throw new Error(what + " did not create " + path);
	var contents = file.read(payload.length);
	file.close();
	if (contents !== payload)
		throw new Error(what + " received " + path + " does not match what was sent");
}

var dir = system.temp_dir + "sexyz_test_" + Date.now() + "/";
if (!mkdir(dir))
	throw new Error("mkdir(" + dir + ") failed");

// YMODEM batch: the ACK for the empty termination header is the last byte of
// the session, and nothing follows it to prompt a flush.
var rx = new Receiver("-raw ry " + shell_quote(dir), dir + "ymodem.log");
rx.expect(C, "to start the batch");
rx.send(block(0, str_bytes("ymodem.txt\0" + payload.length + " "
	+ Math.floor(Date.now() / 1000).toString(8)), 0));
rx.expect(ACK, "for the header block");
rx.expect(C, "to start the file");
rx.send_file(str_bytes(payload));
rx.expect(C, "for the next header");
rx.send(block(0, [], 0));
rx.expect(ACK, "for the batch-terminating header");
rx.expect(-1, "after the batch");
rx.close();
check_received(dir + "ymodem.txt", "YMODEM");

// XMODEM: the ACK for the second EOT is queued immediately before exit.
rx = new Receiver("-raw rc " + shell_quote(dir + "xmodem.txt"), dir + "xmodem.log");
rx.expect(C, "to start the file");
rx.send_file(str_bytes(payload));
rx.expect(-1, "after the file");
rx.close();
check_received(dir + "xmodem.txt", "XMODEM");

directory(dir + "*").forEach(function(path) { file_remove(path); });
rmdir(dir);
