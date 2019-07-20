/*
 * The AGWPE object implements the AGWPE protocol as supported by direwolf.
 * It allows TNC operations, including connected packet.
 *
 * Basic API:
 * var tnc = new AGWPE.TNC('127.0.0.1', 8000); // SYNC
 *    - Connects to the listening sever at the specified host and port, and creates port objects.
 *      Additionally, username and password can be included after this to log in (direwolf doesn't use this)
 * tnc.ports[0].registerCall('W8BSD-1'); // SYNC
 *    - Registers a callsign on a port.  Calls must be registered before they are the source of data.
 * tnc.ports[0].unRegisterCall('W8BSD-1'); // ASYNC
 *    - Unregisters a callsign on a port.
 * var os = tnc.ports[0].askOutstanding(); // SYNC
 *    - Requests the number of outstanding frames on the port
 * tnc.ports[0].sendUNPROTO('W8BSD-1', 'W8BSD-2', 'Hello Deuce!'); // ASYNC
 *    - Sends an UNPROTO frame.
 * tnc.ports[0].sendUNPROTO('W8BSD-1', 'W8BSD-2', ['W8BSD-3', 'W8BSD-4'], 'Hello Deuce!'); // ASYNC
 *    - Sends an UNPROTO frame using a via path
 * tnc.ports[0].sendRaw(data); // ASYNC
 *    - Sends an UNPROTO frame using a via path
 * tnc.ports[0].toggleMonitor(); // ASYNC
 *    - Toggle if monitor frames are received
 * tnc.ports[0].toggleRaw(); // ASYNC
 *    - Toggle if raw frames are received
 * var conn = new tnc.ports[0].connection('W8BSD-1', 'W8BSD-2'); // ASYNC
 *    - Connects FROM the first callsign TO the second callsign
 * var conn2 = new tnc.ports[0].connection('W8BSD-1', 'W8BSD-2', ['W8BSD-3', 'W8BSD-4']); // ASYNC
 *    - As above but connects via the array of digipeaters
 * var conn3 = new tnc.ports[0].connection('W8BSD-1', 'W8BSD-2', 0xCC); // ASYNC
 *    - As above but connects using a different PID (0xCC in this example)
 * var cos = conn.askOutstanding(); // SYNC
 *    - Requests the number of outstanding frames on the connection
 * conn.close(); // ASYNC
 *    - Closes a connection
 * conn.send(data); // ASYNC
 *    - Sends data on a connection
 */

require('sockdefs.js', 'SOCK_STREAM');

var AGWPE = {
	TNC:function(host, port, user, pass) {
		var self = this;
		var pinfo;
		var port0;
		var authf;
		var parr;
		var i;
		var m;
		var pn;

		if (host === undefined)
			host = "127.0.0.1";
		if (port === undefined)
			port = 8000;

		this.callbacks = {
			'G':[],
			'R':[
				{
					func: function(frame) {
						if (frame.data.length == 8) {
							this.major = ascii(frame.data[0]);
							this.major |= ascii(frame.data[1]) << 8;
							this.major |= ascii(frame.data[2]) << 16;
							this.major |= ascii(frame.data[3]) << 24;
							this.minor = ascii(frame.data[4]);
							this.minor |= ascii(frame.data[5]) << 8;
							this.minor |= ascii(frame.data[6]) << 16;
							this.minor |= ascii(frame.data[7]) << 24;
						}
						else
							throw("Invalid Version Response Data Length!");
					}
				}
			]
		};
		this.host = host;
		this.port = port;
		this.ports = {};
		this.sock = new Socket(SOCK_STREAM, "AGWPE");

		this.tnc_port = function(port, name)
		{
			var pself = this;

			if (port === undefined)
				throw("No port specified for port constructor");
			if (name === undefined)
				name = "Port "+(port+1);

			this.__proto__ = AGWPE._portProto;
			this.parent = self;
			this.port = port;
			this.calls = [];
			this.callbacks = {
				'g':[],	// Garbage from DireWolf
				'y':[],
				'X':[],
				'H':[],
				'M':[
					this._packetCallback
				],
				'K':[
					this._packetCallback
				],
				'S':[
					this._packetCallback
				],
				'U':[
					this._packetCallback
				],
				'T':[
					this._packetCallback
				],
				'I':[
					this._packetCallback
				],
				'pkt':[
				]
			};
			this.connections = {};
			this.monitor = false;
			this.rawRx = false;
			this.frames = [];

			/*
			 * This needs to be here, not in the prototype
			 * because it uses pself.
			 */
			this.frame = function(kind)
			{
				if (kind === undefined)
					throw("Frame being created with no kind");

				this.__proto__ = AGWPE._frameProto;
				this.parent = pself;
				this.port = pself.port;
				this.kind = kind;
				this.pid = 0xf0; // I-frame
				this.from = '';
				this.to = '';
				this.data = '';
			};

			/*
			 * This needs to be here, not in the prototype
			 * because it uses pself.
			 */
			this.connection = function(from, to, via_pid)
			{
				var cself = this;
				var via = [];
				var pid = 0xf0;

				if (from === undefined)
					throw("Connection from undefined callsign");
				if (pself.calls.indexOf(from) === -1)
					throw("Connection from unregistered callsign");
				if (to === undefined)
					throw("Connection to undefined call");
				if (via_pid !== undefined) {
					if (Array.isArray(via_pid))
						via = via_pid;
					else
						pid = parseInt(via_pid, 10);
				}
				if (via.length > 7)
					throw("Connect via path too long: "+via.length);

				this.__proto__ = AGWPE._connProto;
				this.parent = pself;
				this.from = from;
				this.to = to;
				this.via = via;
				this.pid = pid;
				this.data = '';
				this.callbacks = {
					'Y':[],
					'C':[
						{
							func:function(frame) {
								this.connected = true;
							}
						}
					],
					'D':[
						{
							func:function(frame) {
								this.data += frame.data;
							}
						}
					],
					'd':[
						{
							func:function(frame) {
								this.doClose();
							}
						}
					]
				};
				this.connected = false;
				this.disconnected = false;

				pself.connections[from+"\x00"+to] = this;
				if (via.length === 0)
					pself._connect(from, to, pid);
				else
					pself._viaConnect(from, to, via);

				/*
				* This needs to be here, not in the prototype
				* because it uses cself.
				*/
				this.frame = function(kind)
				{
					if (kind === undefined)
						throw("Frame being created with no kind");

					this.__proto__ = AGWPE._frameProto;
					this.parent = cself;
					this.port = pself.port;
					this.kind = kind;
					this.pid = cself.pid;
					this.from = cself.from;
					this.to = cself.to;
					this.data = '';
				};
			};
		};

		if (!this.sock.connect(this.host, this.port, 10))
			throw("Unable to connect to AGWPE server");
		// Do global things on port 0... this is hacky.
		port0 = new this.tnc_port(0);
		if (user !== undefined && pass !== undefined) {
			authf = new port0.frame('P');
			authf.data = user;
			while (authf.data.length < 255)
				authf.data += '\x00';
			authf.data += pass;
			while (authf.data.length < 510)
				authf.data += '\x00';
			self.sock.send(authf.bin);
		}
		pinfo = port0.askPorts();
		parr = pinfo.split(/;/);
		for (i=0; i<parseInt(parr[0]); i++) {
			m = parr[i+1].match(/^Port([0-9]+)/);
			if (m !== null) {
				pn = parseInt(m[1]);
				this.ports[pn-1] = new this.tnc_port(pn-1, parr[i+1]);
			}
		}
		port0.askVersion();
	},
	_frameProto:{},
	_connProto:{},
	_portProto:{}
};

AGWPE.TNC.prototype.cycle = function(timeout)
{
	var f;
	var c;

	if (timeout === undefined)
		timeout = 0;

	function handle_callbacks(ctx, frame) {
		var i;
		if (ctx.callbacks[frame.kind] !== undefined) {
			for (i = 0; i < ctx.callbacks[frame.kind].length; i++) {
				if (ctx.callbacks[frame.kind][i].func.call(ctx, frame)) {
					if (ctx.callbacks[frame.kind][i].oneshot !== undefined) {
						if (ctx.callbacks[frame.kind][i].oneshot === true) {
							ctx.callbacks[frame.kind].splice(i, 1);
							i--;
						}
					}
				}
			}
		}
	}

	function find_conn(f) {
		var i;

		for (i in this.ports[f.port].connections) {
			// TODO: Sort out the reversal of calls...
			if (f.from == this.ports[f.port].connections[i].from && f.to == this.ports[f.port].connections[i].to)
				return this.ports[f.port].connections[i];
			if (f.from == this.ports[f.port].connections[i].to && f.to == this.ports[f.port].connections[i].from)
				return this.ports[f.port].connections[i];
		}
		throw("Message on unknown connection (from='"+f.from+"' to='"+f.to+"' kind='"+f.kind+"')");
	}

	if (this.sock.poll(timeout, false)) {
		f = this.getFrame();
		switch (f.kind) {
			// "Global" messages (port doesn't matter)
			case 'R':	// Reply to Request for Version
			case 'G':	// Reply to ports request
				handle_callbacks(this, f);
				break;

			// "Port" messages (from/to don't matter)
			case 'g':	// Reply to capabilities
			case 'y':	// Outstanding frames on a port
			case 'X':	// Callsign register response
			case 'H':	// Callsignes Heard response (not in direwolf)
			case 'M':	// Monitored Connected Packet
			case 'K':	// Raw AX.25 frame
			case 'S':	// Monitored Supervisory Packet
			case 'U':	// Monitored Unproto Packet
			case 'T':	// Monitored Own Packet
			case 'I':	// Monitored Connected Information (not in direwolf)
				if (this.ports[f.port] === undefined)
					throw("Got message on invalid port "+f.port+"!");
				handle_callbacks(this.ports[f.port], f);
				break;

			// "Connection" messages
			case 'Y':	// Outstanding frames on a connection
			case 'C':	// AX.25 connection established
			case 'D':	// AX.25 Connected Data
			case 'd':	// Disconnection notice
				// Find or create the connection...
				c = find_conn.call(this, f);
				handle_callbacks(c, f);
				break;
			default:
				throw("Unhandled kind: '"+f.kind+"'");
		}
	}
};

AGWPE.TNC.prototype.frame = function(kind)
{
	this.__proto__ = AGWPE._frameProto;
	if (kind === undefined)
		throw("Frame being created with no kind");

	this.port = 0;
	this.kind = kind;
	this.pid = 0xf0;
	this.from = '';
	this.to = '';
	this.data = '';
};

AGWPE.TNC.prototype.getFrame = function()
{
	var resp = this.sock.recv(36);
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
	ret.data = this.sock.recv(len);
	return ret;
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

AGWPE._portProto._packetCallback = {
	func:function(frame) {
		var i;

		this.frames.push(frame);
		if (this.callbacks.pkt !== undefined) {
			for (i = 0; i < this.callbacks.pkt.length; i++) {
				if (this.callbacks.pkt[i].func.call(this, frame)) {
					if (this.callbacks.pkt[i].oneshot !== undefined) {
						if (this.callbacks.pkt[i].oneshot === true) {
							this.callbacks.pkt.splice(i, 1);
							i--;
						}
					}
				}
			}
		}
	}
};

AGWPE._portProto.askVersion = function()
{
	var f = new this.frame('R');
	var ret = {};
	var done = false;

	this.parent.callbacks.R.push({
		oneshot:true,
		func:function(frame) {
			done = true;
			return true;
		}
	});
	this.parent.sock.send(f.bin);
	while (!done)
		this.parent.cycle(0.01);

	ret.major = this.parent.major;
	ret.minor = this.parent.minor;
	return ret;
};

AGWPE._portProto.askPorts = function()
{
	var f = new this.frame('G');
	var data;

	this.parent.callbacks.G.push({
		oneshot:true,
		func:function(frame) {
			data = frame.data;
			return true;
		}
	});
	this.parent.sock.send(f.bin);
	while (data === undefined)
		this.parent.cycle(0.01);
	return data;
};

AGWPE._portProto.registerCall = function(call)
{
	var f = new this.frame('X');
	var r;

	if (this.calls.indexOf(call) !== -1)
		return false;
	f.from = call;
	this.callbacks.X.push({
		oneshot:true,
		func:function(frame) {
			if (frame.data.length !== 1)
				throw("Incorrect 'X' frame data length: "+frame.data.length);
			r = ascii(frame.data[0]);
			return true;
		}
	});
	this.parent.sock.send(f.bin);
	while (r === undefined)
		this.parent.cycle(0.01);
	switch(r) {
		case 0:
			return false;
		case 1:
			this.calls.push(call);
			return true;
		default:
			throw("Unexpected registerCall status: "+r);
	}
};

AGWPE._portProto.unRegisterCall = function(call)
{
	var f = new this.frame('x');

	if (this.calls.indexOf(call) == -1)
		return;
	f.from = call;
	this.parent.sock.send(f.bin);
	this.calls.splice(this.calls.indexOf(call), 1);
};

AGWPE._portProto.askOutstanding = function()
{
	var f = new this.frame('y');
	var ret;

	this.callbacks.y.push({
		oneshot:true,
		func:function(frame) {
			if (frame.data.length !== 4)
				throw("Invalid length in askOutstanding reply: "+frame.data.length);
			ret = ascii(frame.data[0]);
			ret |= ascii(frame.data[1]) << 8;
			ret |= ascii(frame.data[2]) << 16;
			ret |= ascii(frame.data[3]) << 24;
			return true;
		}
	});
	this.parent.sock.send(f.bin);
	while (ret === undefined)
		this.parent.cycle(0.01);
	return ret;
};

AGWPE._portProto.toggleMonitor = function()
{
	var f = new this.frame('m');
	var ret;

	this.parent.sock.send(f.bin);
	this.monitor = !this.monitor;
};

AGWPE._portProto.toggleRaw = function()
{
	var f = new this.frame('k');
	var ret;

	this.parent.sock.send(f.bin);
	this.rawRx = !this.rawRx;
};

AGWPE._portProto.sendUNPROTO = function(from, to, arg3, arg4)
{
	var f = new this.frame('M');
	var via = [];
	var data = '';
	var head = '';
	var i;

	if (from === undefined)
		throw("sendUNPROTO without from");
	if (this.calls.indexOf(from) === -1)
		throw("sendUNPROTO from unregistered call '"+from+"'");
	f.from = from;
	if (to === undefined)
		throw("sendUNPROTO without to");
	f.to = to;
	if (Array.isArray(arg3)) {
		via = arg3;
		data = arg4;
	}
	if (data === undefined)
		data = '';
	if (via.length > 0) {
		f.kind = 'V';
		head += ascii(via.length);
		for (i in via) {
			head += via[i];
			while ((head.length - 1) % 10)
				head += "0x00";
		}
		data = head + data;
	}
	f.data = data;
	this.parent.sock.send(f.bin);
};

AGWPE._portProto.sendRaw = function(data)
{
	var f = new this.frame('K');

	if (data === undefined)
		data = '';
	this.parent.sock.send(f.bin);
};

AGWPE._portProto._connect = function(from, to, pid)
{
	var f;

	if (pid !== 0xf0)
		f = new this.frame('C');
	else
		f = new this.frame('c');

	f.from = from;
	f.to = to;
	f.pid = pid;
	this.parent.sock.send(f.bin);
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
};

AGWPE._connProto.askOutstanding = function()
{
	var f = new this.frame('Y');
	var ret;

	this.callbacks.Y.push({
		oneshot:true,
		func:function(frame) {
			if (frame.data.length !== 4)
				throw("Invalid length in connection askOutstanding reply: "+frame.data.length);
			ret = ascii(frame.data[0]);
			ret |= ascii(frame.data[1]) << 8;
			ret |= ascii(frame.data[2]) << 16;
			ret |= ascii(frame.data[3]) << 24;
			return true;
		}
	});
	this.parent.parent.sock.send(f.bin);
	while (ret === undefined)
		this.parent.parent.cycle(0.01);
	return ret;
};

AGWPE._connProto.doClose = function()
{
	var i;

	this.connected = false;
	this.disconnected = true;
	for (i in this.parent.connections) {
		if (this.parent.connections[i].disconnected == true)
			delete this.parent.connections[i];
	}
};

AGWPE._connProto.close = function()
{
	var f = new this.frame('d');

	if (this.disconnected)
		return;
	this.parent.parent.sock.send(f.bin);
};

AGWPE._connProto.send = function(data)
{
	var f = new this.frame('D');

	if (!this.connected)
		throw("send on unconnected connection");
	if (data === undefined)
		throw("send with undefined data");
	f.data = data;
	this.parent.parent.sock.send(f.bin);
};

var tnc = new AGWPE.TNC('127.0.0.1', 8000);
tnc.ports[0].toggleMonitor();

tnc.ports[0].callbacks.pkt.push({
	func:function(frame) {
		var cleaned = frame.data.replace(/[\x00-\x1f]/g, function (match) {
			return format("<0x%02x>", ascii(match));
		});

		print("Port "+frame.port+" Got '"+frame.kind+"' frame PID: "+frame.pid+"\nFrom: \""+frame.from+"\"\nTo: \""+frame.to+"\"\nData: \""+frame.data+"\"");
		this.frames.shift();
	}
});
while(1)
	tnc.cycle(1);
