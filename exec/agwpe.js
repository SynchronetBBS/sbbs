require('sockdefs.js', 'SOCK_STREAM');

var AGWPE = {
	TNC:function(host, port) {
		var self = this;

		if (host === undefined)
			host = "127.0.0.1";
		if (port === undefined)
			port = 8000;
		this.host = host;
		this.port = port;
		this.sock = new Socket(SOCK_STREAM, "AGWPE");
		if (!this.sock.connect(this.host, this.port, 10))
			throw("Unable to connect to AGWPE server");
		this.ports = {};

		this.port = function(port)
		{
			this.parent = self;
			var pself = this;

			this.__proto__ = AGWPE._portProto;
			if (port === undefined)
				throw("No port specified for port constructor");

			this.port = port;
			this.calls = [];

			this.frame = function(kind)
			{
				this.__proto__ = AGWPE._frameProto;
				this.parent = pself;
				if (kind === undefined)
					throw("Frame being created with no kind");

				this.port = pself.port;
				this.kind = kind;
				this.pid = 0xf0;	// I-frame
				this.from = '';
				this.to = '';
				this.data = '';
			}

			this.connection = function(from, to, via_pid)
			{
				this.__proto__ = AGWPE._connProto;
				this.parent = pself;
				var cself = this;
				var via;
				var pid;

				if (from === undefined)
					throw("Connection from undefined callsign");
				if (pself.calls.indexOf(from) === -1)
					throw("Connection from unregistered callsign");
				if (to === undefined)
					throw("Connection to undefined call");
				if (via_pid === undefined) {
					via = [];
					pid = 0xf0;
				}
				else if (Array.isArray(via_pid)) {
					via = via_pid;
					pid = 0xf0;
				}
				else {
					pid = parseInt(via_pid, 10);
				}
				this.from = from;
				this.to = to;
				this.via = via;
				this.pid = pid;
				this.connected = false;
				if (via.length > 7)
					throw("Connect via path too long: "+via.length);
				if (via.length === 0)
					pself._connect(from, to, pid);
				else
					pself._viaConnect(from, to, via);

				this.frame = function(kind)
				{
					this.__proto__ = AGWPE._frameProto;
					this.parent = cself;
					if (kind === undefined)
						throw("Frame being created with no kind");

					this.port = pself.port;
					this.kind = kind;
					this.pid = cself.pid;	// I-frame
					this.from = cself.from;
					this.to = cself.to;
					this.data = '';
				};
			}
		}

		var port0 = new this.port(0);
		var pinfo = port0.askPorts();
		var parr = pinfo.split(/;/);
		var i;
		var m;
		var pn;
		for (i=0; i<parseInt(parr[0]); i++) {
			m = parr[i+1].match(/^Port([0-9]+)/);
			if (m !== null) {
				pn = parseInt(m[1]);
				this.ports[pn-1] = new this.port(pn-1);
			}
		}
		var pver = port0.askVersion();
		this.ver = pver;
	},
	_frameProto:{},
	_connProto:{},
	_portProto:{}
};

Object.defineProperty(AGWPE._frameProto, "bin", {
	get: function bin() {
		var ret = '';

		ret += ascii(this.port);
		ret += ascii(0);
		ret += ascii(0);
		ret += ascii(0);
		if (ret.length !== 4)
			throw ("Invalid length after port "+ret.length);
		ret += this.kind;
		ret += ascii(0);
		if (ret.length !== 6)
			throw ("Invalid length after kind "+ret.length);
		ret += ascii(this.pid);
		ret += ascii(0);
		if (ret.length !== 8)
			throw ("Invalid length after PID "+ret.length);
		ret += this.from;
		while (ret.length < 18)
			ret += ascii(0);
		ret += this.to;
		while (ret.length < 28)
			ret += ascii(0);
		ret += ascii(this.data.length & 0xff);
		ret += ascii((this.data.length >> 8) & 0xff);
		ret += ascii((this.data.length >> 16) & 0xff);
		ret += ascii((this.data.length >> 24) & 0xff);
		ret += ascii(0);
		ret += ascii(0);
		ret += ascii(0);
		ret += ascii(0);
		if (ret.length !== 36)
			throw ("Invalid length "+ret.length);
		ret += this.data;
		js.flatten_string(ret);
		return ret;
	}
});


AGWPE._portProto.askVersion = function()
{
	var f = new this.frame('R');
	var resp;
	var ret = {};

	this.parent.sock.send(f.bin);
	resp = this.getFrame();
	if (resp.kind !== 'R')
		throw("Unexpected reply to askVersion '"+resp.kind+"'!");
	if (resp.data.length == 8) {
		ret.major = ascii(resp.data[0]);
		ret.major |= ascii(resp.data[1]) << 8;
		ret.major |= ascii(resp.data[2]) << 16;
		ret.major |= ascii(resp.data[3]) << 24;
		ret.minor = ascii(resp.data[4]);
		ret.minor |= ascii(resp.data[5]) << 8;
		ret.minor |= ascii(resp.data[6]) << 16;
		ret.minor |= ascii(resp.data[7]) << 24;
	}
	return ret;
};

AGWPE._portProto.askPorts = function()
{
	var f = new this.frame('G');
	var resp;

	this.parent.sock.send(f.bin);
	resp = this.getFrame();
	if (resp.kind !== 'G')
		throw("Unexpected reply to askPorts '"+resp.kind+"'!");
	return resp.data;
};

// 'g' command (port capabilities - just garbage on direwolf)

// TODO: 'k' command (RX raw AX25 frames)

// TODO: 'm' command (RX Monitor AX25 frames)

// TODO: 'V' command (Transmit UI data)

// 'H' command (recently heard - not implemented on direwolf)

// TODO: 'K' command (TX raw AX.25 frame ala KISS)

AGWPE._portProto.registerCall = function(call)
{
	var f = new this.frame('X');
	var resp;

	if (this.calls.indexOf(call) !== -1)
		return false;
	f.from = call;
	this.parent.sock.send(f.bin);
	resp = this.getFrame();
	if (resp.kind !== 'X')
		throw("Unexpected reply to registerCall '"+resp.kind+"'!");
	if (resp.from !== call)
		throw("Unexpected reply to registerCall from: '"+resp.from+"' to '"+resp.to+"'!");
	if (resp.data.length === 1) {
		switch(ascii(resp.data[0])) {
			case 0:
				return false;
			case 1:
				this.calls.push(call);
				return true;
			default:
				throw("Unexpected registerCall status: "+ascii(resp.data[0]));
		}
	}
};

AGWPE._portProto.unRegisterCall = function(call)
{
	var f = new this.frame('x');
	var resp;

	if (this.calls.indexOf(call) == -1)
		return;
	f.from = call;
	this.parent.sock.send(f.bin);
	this.calls.splice(this.calls.indexOf(call));
};

AGWPE._portProto.askOutstanding = function(port)
{
	var f = new this.frame('y');
	var resp;
	var ret;

	if (port === undefined)
		port = 0;
	f.port = port;
	this.sock.send(f.bin);
	resp = this.getFrame();
	if (resp.kind !== 'y')
		throw("Unexpected reply to askOutstanding '"+resp.kind+"'!");
	if (resp.data.length === 4) {
		ret = ascii(resp.data[0]);
		ret |= ascii(resp.data[1]) << 8;
		ret |= ascii(resp.data[2]) << 16;
		ret |= ascii(resp.data[3]) << 24;
		return ret;
	}
	throw("Invalid response length to askOutstanding: "+resp.data.length);
};

AGWPE._connProto.askOutstanding = function()
{
	var f = new this.frame('y');
	var resp;
	var ret;

	if (!this.connected)
		throw("askOutstanding on unconnected connection");
	this.sock.send(f.bin);
	resp = this.getFrame();
	if (resp.kind !== 'y')
		throw("Unexpected reply to askOutstandingConn '"+resp.kind+"'!");
	if (resp.data.length === 4) {
		ret = ascii(resp.data[0]);
		ret |= ascii(resp.data[1]) << 8;
		ret |= ascii(resp.data[2]) << 16;
		ret |= ascii(resp.data[3]) << 24;
		return ret;
	}
	throw("Invalid response length to askOutstandingConn: "+resp.data.length);
};

AGWPE._connProto.close = function()
{
	var f = new this.frame('d');
	var resp;
	var ret;

	if (!this.connected)
		return;
	this.parent.parent.sock.send(f.bin);
	resp = this.parent.getFrame();
	if (resp.kind !== 'd')
		throw("Unexpected reply to close '"+resp.kind+"'!");
	this.connected = false;
};

AGWPE._connProto.send = function(data)
{
	var f = new this.frame('D');
	var resp;
	var ret;

	if (!this.connected)
		throw("send on unconnected connection");
	if (data === undefined)
		throw("send with undefined data");
	f.data = data;
	this.parent.parent.sock.send(f.bin);
	resp = this.parent.getFrame();
	if (resp.kind !== 'd')
		throw("Unexpected reply to close '"+resp.kind+"'!");
	this.connected = false;
};

AGWPE._portProto.sendUNPROTO = function(from, to, data)
{
	var f = new this.frame('M');
	var resp;
	var ret;

	if (from === undefined)
		throw("sendUNPROTO without from");
	if (this.calls.indexOf(from) === -1)
		throw("sendUNPROTO from unregistered call '"+from+"'");
	f.from = from;
	if (to === undefined)
		throw("sendUNPROTO without to");
	f.to = to;
	if (data === undefined)
		data = '';
	f.data = data;
	this.parent.sock.send(f.bin);
};

AGWPE._portProto._connect = function(from, to, pid)
{
	var f;
	var resp;

	if (pid !== 0xf0)
		f = new this.frame('C');
	else
		f = new this.frame('c');

	f.from = from;
	f.to = to;
	f.pid = pid;
	this.parent.sock.send(f.bin);
	resp = this.getFrame();
	switch(resp.kind) {
		case f.kind:
			this.connected = true;
			return;
		case 'd':
			this.connected = false;
			return;
		default:
			throw("Unexpected reply to connect '"+resp.kind+"'!");
	}
};

AGWPE._portProto._viaConnect = function(from, to, via)
{
	var f = new this.frame('C');
	var path = ascii(via.length);
	var i;

	f.from = from;
	f.to = to;
	for (i in via) {
		path += via[i];
		while ((path.length - 1) % 10)
			path += '\x00';
	}
	f.data = path;
	this.parent.sock.send(f.bin);
	resp = this.getFrame();
	switch(resp.kind) {
		case f.kind:
			this.connected = true;
			return;
		case 'd':
			this.connected = false;
			return;
		default:
			throw("Unexpected reply to viaConnect '"+resp.kind+"'!");
	}
};

AGWPE._portProto.getFrame = function()
{
	var resp = this.parent.sock.recv(36);
	var len = ascii(resp[28]);
	var ret = new this.frame('\x00');

	ret.port = ascii(resp[0]);
	ret.kind = resp[4];
	ret.pid = ascii(resp[6]);
	ret.from = resp.substr(8,10).split(/\x00/)[0];
	ret.to = resp.substr(18,10).split(/\x00/)[0];
	len |= ascii(resp[29] << 8);
	len |= ascii(resp[30] << 16);
	len |= ascii(resp[31] << 24);
	ret.data = this.parent.sock.recv(len);
	return ret;
};
