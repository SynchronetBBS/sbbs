load("sockdefs.js");

function BinkP()
{
	var addr;

	this.default_zone = 1;
	addr = this.parse_addr(system.fido_addr_list[0]);
	if (addr.zone !== undefined)
		this.default_zone = addr.zone;
	// TODO: Reset the eop counts when appropriate.
	this.senteob = 0;
	this.goteob = 0;
	this.pending_ack = [];
	this.pending_get = [];
	this.tx_queue=[];
	this.debug = false;
	this.skipfiles = false;
	this.nonreliable = false;
	this.sent_nr = false;
	this.ver1_1 = false;
	this.require_md5 = true;
}
BinkP.prototype.Frame = function() {};
BinkP.prototype.escapeFileName = function(name)
{
	return name.replace(/[^A-Za-z0-9!"#$%&'\(\)*+,\-.\/:;<=>?@\[\]\^_`{|}~]/g, function(match) { return '\\x'+ascii(match); });
};
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
};
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
};
BinkP.prototype.faddr_to_inetaddr = function(addr)
{
	var ret = '';
	if (addr.point !== undefined && addr.point !== 0)
		ret += format("p%d", addr.point);
	ret += format("f%d.n%d.z%d.binkp.net", addr.node, addr.net, addr.zone);
	return ret;
};
BinkP.prototype.session = function(addr, password, port)
{
	var i;
	var pkt;
	var m;
	var tmp;
	var ver;

	if (addr === undefined)
		throw("No address specified!");
	addr = this.parse_addr(addr);
	if (addr.net === undefined || addr.node == undefined)
		return false;

	if (password === undefined)
		password = '-';

	if (port === undefined)
		port = 24554;

	if (this.sock === undefined)
		this.sock = new Socket(SOCK_STREAM, "binkp");

	if(!this.sock.connect(this.faddr_to_inetaddr(addr), port)) {
		this.sock = undefined;
		return false;
	}

	this.authenticated = undefined;
	this.sendCmd(this.command.M_NUL, "SYS "+system.name);
	this.sendCmd(this.command.M_NUL, "ZYZ "+system.operator);
	this.sendCmd(this.command.M_NUL, "LOC "+system.location);
	this.sendCmd(this.command.M_NUL, "NDL 115200,TCP,BINKP");
	this.sendCmd(this.command.M_NUL, "TIME "+system.timestr());
	this.sendCmd(this.command.M_NUL, "VER JSBinkP/0.0.0/"+system.platform+" binkp/1.0");
	for (i=0; i<system.fido_addr_list.length; i++)
		this.sendCmd(this.command.M_ADR, system.fido_addr_list[i]);

	while(!js.terminated && this.remote_addrs === undefined) {
		pkt = this.recvFrame();
		if (pkt === undefined)
			return false;
	}

	function binary_md5(key)
	{
		return md5_calc(key, true).replace(/[0-9a-fA-F]{2}/g,function(m){return ascii(parseInt(m, 16));});
	}
	function str_xor(str1, val)
	{
		var i;
		var ret='';

		for (i=0; i<str1.length; i++) {
			ret += ascii(str1.charCodeAt(i) ^ val);
		}
		return ret;
	}

	if (this.authenticated === undefined) {
		if (this.cram === undefined || this.cram.algo !== 'MD5') {
			if (this.require_md5)
				this.sendCmd(this.command.M_ERR, "MD5 Required");
			else
				this.sendCmd(this.command.M_PWD, password);
		}
		else {
			tmp = password;
			if (tmp.length > 64)
				tmp = binary_md5(tmp);
			while(tmp.length < 64)
				tmp += '\x00';
			tmp = md5_calc(str_xor(tmp, 0x5c) + binary_md5(str_xor(tmp, 0x36) + this.cram.challenge), true);
			this.sendCmd(this.command.M_PWD, 'CRAM-MD5-'+tmp);
		}
	}

	while(this.authenticated === undefined) {
		pkt = this.recvFrame();
		if (pkt === undefined)
			return false;
	}

	// Session set up, we're good to go!
	

	if (js.termianted) {
		this.close();
		return false;
	}
	return true;
};
BinkP.prototype.close = function()
{
	// TODO: Make sure it's two EOBs on each side for v1.1
	// Send an ERR and close.
	if ((!this.goteob) || this.tx_queue.length || this.pending_ack.length || this.pending_get.length) {
		this.sendCmd(this.command.M_ERR, "Forced Shutdown");
	}
	else {
		if (!this.senteob) {
			this.sendCmd(this.command.M_EOB, "");
		}
	}
	this.tx_queue.forEach(function(file) {
		file.close();
	});
};
BinkP.prototype.sendCmd = function(cmd, data)
{
	var type;
	var tmp;

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
	this.sock.sendBin(len, 2);
	this.sock.sendBin(cmd, 1);
	this.sock.send(data);
	switch(cmd) {
		case this.command.M_EOB:
			this.senteob++;
			break;
		case this.command.M_ERR:
		case this.command.M_BSY:
			this.sock.close();
			this.sock = undefined;
			break;
		case this.command.M_NUL:
			if (data.substr(0, 4) === 'OPT ') {
				tmp = data.substr(4).split(/ /);
				if (tmp.indexOf('NR'))
					this.sent_nr = true;
			}
			break;
	}
	return true;
};
BinkP.prototype.sendData = function(data)
{
	var len = data.length;
	if (this.debug)
		log(LOG_DEBUG, "Sending "+data.length+" bytes of data");
	this.sock.sendBin(len, 2);
	this.sock.send(data);
};
BinkP.prototype.recvFrame = function()
{
	var ret = new this.Frame();
	var type;
	var i;
	var args;
	var options;
	var tmp;
	var ver;

	ret.length = this.sock.recvBin(2);
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
		switch(ret.command) {
			case this.command.M_ERR:
				log(LOG_ERROR, "BinkP got fatal error from remote: '"+ret.data+"'.");
				this.sock.close();
				this.socket = undefined;
				return undefined;
			case this.command.M_BSY:
				log(LOG_WARNING, "BinkP got non-fatal error from remote: '"+ret.data+"'.");
				this.sock.close();
				this.socket = undefined;
				return undefined;
			case this.command.M_EOB:
				this.goteob++;
				break;
			case this.command.M_ADR:
				if (this.remote_addrs !== undefined) {
					this.sendCmd(this.command.M_ERR, "Address already recieved.");
					return undefined;
				}
				else
					this.remote_addrs = ret.data.split(/ /);
				break;
			case this.command.M_OK:
				if (this.authenticated !== undefined) {
					this.sendCmd(this.command.M_ERR, "Authentication already complete.");
					return undefined;
				}
				else
					this.authenticated = ret.data;
				break;
			case this.command.M_NUL:
				args = ret.data.split(/ /);
				switch(args[0]) {
					case 'OPT':
						for (i=1; i<args.length; i++) {
							if (args[i].substr(0,9) === 'CRAM-MD5-') {
								this.cram = {algo:'MD5', challenge:args[i].substr(9).replace(/[0-9a-fA-F]{2}/g,
									function(m){return ascii(parseInt(m, 16));})
								};
							}
							else {
								switch(args[i]) {
									case 'NR':
										if (!this.sent_nr)
											this.sendCmd(this.command.M_NUL, "NR");
										this.non_reliable = true;
										break;
								}
							}
						}
						break;
					case 'VER':
						tmp = ret.data.split(/ /);
						if (tmp.length >= 3) {
							if (tmp[2].substr(0, 6) === 'binkp/') {
								ver = tmp[2].substr(6).split(/\./);
								if (ver.length >= 2) {
									tmp = parseInt(ver[0], 10);
									switch(tmp) {
										case NaN:
											break;
										case 1:
											if (parseInt(ver[1], 10) > 0)
												this.ver1_1 = true;
											break;
										default:
											if (tmp > 1)
												this.ver1_1 = true;
											break;
									}
								}
							}
						}
				}
		}
	}
	return ret;
};
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
};
BinkP.prototype.addFile = function(name)
{
	var file = new File(name);

	if (!file.open("rb", true))
		return false;
	this.tx_queue.push(file);
};
