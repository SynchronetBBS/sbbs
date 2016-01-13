load("sockdefs.js");

function BinkP()
{
	var addr;

	this.default_zone = 1;
	addr = this.parse_addr(system.fido_addr_list[0]);
	if (addr.zone !== undefined)
		this.default_zone = addr.zone;
	this.sock  = undefined;
	this.senteob = false;
	this.goteob = false;
	this.pending_ack = {};
	this.pending_get = {};
	this.debug = false
}
BinkP.prototype.Frame = function() {}
BinkP.prototype.command = {
	M_NUL:0,
	M_ADR:1,
	M_PWD:2,
	M_FILE:3,
	M_OK:4,
	M_EOB:5,
	M_GOT:6,
	M_ERR:7,
	M_BSY:8,
	M_GET:9,
	M_SKIP:10,
}
BinkP.prototype.command_name = [
	"M_NUL",
	"M_ADR",
	"M_PWD",
	"M_FILE",
	"M_OK",
	"M_EOB",
	"M_GOT",
	"M_ERR",
	"M_BSY",
	"M_GET",
	"M_SKIP"
];
BinkP.prototype.parse_addr = function(addr)
{
	var m;
	var ret={};

	m = addr.match(/^([0-9]+):/);
	if (m !== null)
		ret.zone = parseInt(m[1], 10);
	else
		ret.zone = this.default_zone;

	m = addr.match(/([0-9]+)\//);
	if (m !== null)
		ret.net = parseInt(m[1], 10);

	m = addr.match(/\/([0-9]+)/);
	if (m !== null)
		ret.node = parseInt(m[1], 10);

	m = addr.match(/\.([0-9]+)/);
	if (m !== null)
		ret.point = parseInt(m[1], 10);

	m = addr.match(/@.+$/);
	if (m !== null)
		ret.domain = m[1];

	return ret;
}
BinkP.prototype.faddr_to_inetaddr = function(addr)
{
	var ret = '';
	if (addr.point !== undefined && addr.point !== 0)
		ret += format("p%d", addr.point);
	ret += format("f%d.n%d.z%d.binkp.net", addr.node, addr.net, addr.zone);
	return ret;
}
BinkP.prototype.connect = function(addr, password, port)
{
	var i;
	var pkt;

	if (addr === undefined)
		throw("No password specified!");
	addr = this.parse_addr(addr);
	if (addr.net === undefined || addr.node == undefined)
		return false;

	if (password === undefined)
		password = '-';

	if (port === undefined)
		port = 24554;

	if (this.sock === undefined)
		this.sock = new Socket(SOCK_STREAM, "binkp");

	if(!this.sock.connect(this.faddr_to_inetaddr(addr), port))
		return false;

	this.sendCmd(this.command.M_NUL, "SYS "+system.name);
	this.sendCmd(this.command.M_NUL, "ZYZ "+system.operator);
	this.sendCmd(this.command.M_NUL, "LOC "+system.location);
	this.sendCmd(this.command.M_NUL, "NDL 115200,TCP,BINKP");
	this.sendCmd(this.command.M_NUL, "TIME "+system.timestr());
	this.sendCmd(this.command.M_NUL, "VER JSBinkP/0.0.0/"+system.platform+" binkp/1.0");
	for (i=0; i<system.fido_addr_list.length; i++)
		this.sendCmd(this.command.M_ADR, system.fido_addr_list[i]);
	this.sendCmd(this.command.M_PWD, password);
	while(!js.terminated) {
		pkt = this.recvFrame();
		if (pkt === undefined)
			return false;
		if (pkt.is_cmd) {
			if (pkt.command == this.command.M_OK)
				break;
		}
	}
	if (js.termianted) {
		this.close();
		return false;
	}
	return true;
}
BinkP.prototype.close = function()
{
	// Send an ERR and close.
	if ((!this.goteob) || Object.keys(this.pending_ack).length || Object.keys(this.pending_get).length) {
		this.sendCmd(this.command.M_ERR, "Forced Shutdown");
	}
	else {
		if (!this.senteob) {
			this.sendCmd(this.command.M_EOB, "");
		}
	}
}
BinkP.prototype.sendCmd = function(cmd, data)
{
	var type;

	if (data === undefined)
		data = '';
	if (this.debug) {
		if (cmd < this.command_name.length)
			type = this.command_name[cmd];
		else
			type = 'Unknown Command '+cmd;
		log(LOG_DEBUG, "Sent "+type+" command args: "+data);
	}
	var len = data.length+1;
	len |= 0x8000;
	this.sock.sendBin(len, 2)
	this.sock.sendBin(cmd, 1)
	this.sock.send(data);
	if (cmd === this.command.M_EOB)
		this.senteob = true;
	if (cmd === this.command.M_ERR || cmd === this.command.M_BSY) {
		this.sock.close();
		this.socket = undefined;
	}
	return true;
}
BinkP.prototype.sendData = function(data)
{
	var len = data.length;
	if (this.debug)
		log(LOG_DEBUG, "Sending "+data.length+" bytes of data");
	this.sock.sendBin(len, 2)
	this.sock.send(data);
}
BinkP.prototype.recvFrame = function()
{
	var ret = new this.Frame();
	var type;

	ret.length = this.sock.recvBin(2)
	ret.is_cmd = (ret.length & 0x8000) ? true : false;
	ret.length &= 0x7fff;
	if (ret.length == 0)
		ret.data = '';
	else
		ret.data = this.sock.recv(ret.length);
	if (ret.is_cmd) {
		ret.command = ret.data.charCodeAt(0);
		ret.data = ret.data.substr(1);
	}
	if (this.debug) {
		if (ret.is_cmd) {
			if (ret.command < this.command_name.length)
				type = this.command_name[ret.command];
			else
				type = 'Unknown Command '+ret.command;
			log(LOG_DEBUG, "Got "+type+" command args: "+ret.data);
		}
		else
			log(LOG_DEBUG, "Got data frame length "+ret.length);
	}
	if (ret.is_cmd) {
		if (ret.command === this.command.M_ERR) {
			log(LOG_ERROR, "BinkP got fatal error from remote: '"+ret.data+"'.");
			this.sock.close();
			this.socket = undefined;
			return undefined;
		}
		if (ret.command === this.command.M_BSY) {
			log(LOG_WARNING, "BinkP got non-fatal error from remote: '"+ret.data+"'.");
			this.sock.close();
			this.socket = undefined;
			return undefined;
		}
		if (ret.command == this.command.M_EOB)
			this.goteob = true;
	}
	return ret;
}
BinkP.prototype.skipFiles = function()
{
	var pkt;

	while(!(js.terminated || this.goteob)) {
		pkt = this.recvFrame();
		if (pkt === undefined)
			return false;
		if (pkt.is_cmd) {
			if (pkt.command == this.command.M_EOB)
				return true;
			if (pkt.command == this.command.M_FILE || pkt.command == this.command.M_GET) {
				this.sendCmd(this.command.M_SKIP, pkt.data.replace(/ [0-9]+$/,''));
			}
		}
	}
	if (js.terminated)
		this.close();
	return true;
}
