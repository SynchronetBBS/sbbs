load('sbbsdefs.js');
load('websocket-proxy.js');

function err(msg) {
	log(LOG_DEBUG, msg);
	client.socket.close();
	exit();
}

function getSession(un) {
	var fn = format('%suser/%04d.web', system.data_dir, un);
	if (!file_exists(fn)) return false;
	var f = new File(fn);
	if (!f.open('r')) return false;
	var session = f.iniGetObject();
	f.close();
	return session;
}

var RLoginClient = function(options) {

	var self = this;

	const	CAN = 0x18,
			CR = 0x0D,
			DC1 = 0x11,
			DC3 = 0x13,
			DOT = 0x2E,
			EOM = 0x19,
			EOT = 0x04,
			LF = 0x0A,
			SUB = 0x1A,
			DISCARD = 0x02,
			RAW = 0x10,
			COOKED = 0x20,
			WINDOW = 0x80;

	var serverBuffer = []; // From server
	var clientBuffer = []; // From client

	var state = {
		connected : false,
		cooked : true,
		suspendInput : false,
		suspendOutput : false,
		watchForClientEscape : true,
		clientHasEscaped : false
	};

	var properties = {
		rows : 24,
		columns : 80,
		pixelsX : 640,
		pixelsY : 480,
		clientEscape : '~'
	};

	// As suggested by RFC1282
	var clientEscapes = {
		DOT : self.disconnect,
		EOT : self.disconnect,
		SUB : function() {
			state.suspendInput = (state.suspendInput) ? false : true;
			state.suspendOutput = state.suspendInput;
		},
		EOM : function() {
			state.suspendInput = (state.suspendInput) ? false : true;
			state.suspendOutput = false;
		}
	};

	this.__defineGetter__('connected', function () { return state.connected; });

	this.__defineSetter__(
		'connected',
		function (value) {
			if (typeof value === 'boolean' && !value) self.disconnect();
		}
	);

	this.__defineGetter__('rows', function () { return properties.rows; });

	this.__defineSetter__(
		'rows',
		function(value) {
			if (typeof value === 'number' && value > 0) {
				properties.rows = value;
			} else {
				throw 'RLogin: Invalid \'rows\' setting ' + value;
			}
		}
	);

	this.__defineGetter__(
		'columns',
		function () { return properties.columns; }
	);

	this.__defineSetter__(
		'columns',
		function (value) {
			if (typeof value === 'number' && value > 0) {
				properties.columns = value;
			} else {
				throw 'RLogin: Invalid \'columns\' setting ' + value;
			}
		}
	);

	this.__defineGetter__(
		'pixelsX',
		function () { return properties.pixelsX; }
	);

	this.__defineSetter__(
		'pixelsX',
		function (value) {
			if (typeof value === 'number' && value > 0) {
				properties.pixelsX = value;
			} else {
				throw 'RLogin: Invalid \'pixelsX\' setting ' + value;
			}
		}
	);

	this.__defineGetter__(
		'pixelsY',
		function () { return properties.pixelsY; }
	);

	this.__defineSetter__(
		'pixelsY',
		function (value) {
			if (typeof value === 'number' && value > 0) {
				properties.pixelsY = value;
			} else {
				throw 'RLogin: Invalid \'pixelsY\' setting ' + value;
			}
		}
	);

	this.__defineGetter__(
		'clientEscape',
		function() { return properties.clientEscape; }
	);

	this.__defineSetter__(
		'clientEscape',
		function (value) {
			if (typeof value === 'string' && value.length === 1) {
				properties.clientEscape = value;
			} else {
				throw 'RLogin: Invalid \'clientEscape\' setting ' + value;
			}
		}
	);

	var handle = new Socket();

	function getServerData() {

		if (handle.nread < 1) return;

		var data = [];
		while (handle.nread > 0) {
			data.push(handle.recvBin(1));
		}

		if (!state.connected) {
			if (data[0] === 0) {
				state.connected = true;
				if (data.length > 1) {
					data = data.slice(1);
				} else {
					return;
				}
			} else {
				self.disconnect();
			}
		}

		// If I could tell if the TCP urgent-data pointer had been set,
		// I would uncomment (and complete) this block.  We'll settle
		// for a partial implementation for the time being.
		// We would need something to tell us if urgent data was sent,
		// eg. var lookingForControlCode = urgentDataPointerIsSet();
		/*
		var temp = [];
		for (var d = 0; d < data.length; d++) {
			if (!lookingForControlCode) {
				temp.push(data[d]);
				continue;
			}
			switch (data[d]) {
				case DISCARD:
					temp = [];
					// We found our control code
					lookingForControlCode = false;
					break;
				case RAW:
					state.cooked = false;
					lookingForControlCode = false;
					break;
				case COOKED:
					state.cooked = true;
					lookingForControlCode = false;
					break;
				case WINDOW:
					self.sendWCCS();
					lookingForControlCode = false;
					break;
				default:
					temp.push(data[d]);
					break;
			}
		}
		if (!state.suspendOutput) self.emit('data', new Buffer(temp));
		*/
		if (!state.suspendOutput) serverBuffer = serverBuffer.concat(data);
	}

	// Send a Window Change Control Sequence
	// this.sendWCCS = function() {
	// 	var magicCookie = [0xFF, 0xFF, 0x73, 0x73];
	// 	var rcxy = new Buffer(8);
	// 	rcxy.writeUInt16LE(properties.rows, 0);
	// 	rcxy.writeUInt16LE(properties.columns, 2);
	// 	rcxy.writeUInt16LE(properties.pixelsX, 4);
	// 	rcxy.writeUInt16LE(properties.pixelsY, 6);
	// 	if(state.connected)
	// 		handle.write(Buffer.concat([magicCookie, rcxy]));
	// }

	// Send 'data' (String or Buffer) to the rlogin server
	this.send = function (data) {

		if (!state.connected) throw 'RLogin.send: not connected.';
		if (state.suspendInput) throw 'RLogin.send: input has been suspended.';

		if (typeof data === 'string') {
			data = data.split('').map(function (d) { return ascii(d); });
		}

		var temp = [];
		for (var d = 0; d < data.length; d++) {
			if (state.watchForClientEscape &&
				data[d] == properties.clientEscape.charCodeAt(0)
			) {
				state.watchForClientEscape = false;
				state.clientHasEscaped = true;
				continue;
			}
			if (state.clientHasEscaped) {
				state.clientHasEscaped = false;
				if (typeof clientEscapes[data[d]] !== 'undefined') {
					clientEscapes[data[d]]();
				}
				continue;
			}
			if (state.cooked && (data[d] === DC1 || data[d] === DC3)) {
				state.suspendOutput = (data[d] === DC3);
				continue;
			}
			if ((d > 0 && data[d - 1] === CR && data[d] === LF) ||
				data[d] == CAN
			) {
				state.watchForClientEscape = true;
			}
			temp.push(data[d]);
		}
		clientBuffer = clientBuffer.concat(temp);

	}

	this.receive = function () {
		return serverBuffer.splice(0, serverBuffer.length);
	}

	/*	If 'ch' is found in client input immediately after the
		'this.clientEscape' character when:
			- this is the first input after connection establishment or
			- these are the first characters on a new line or
			- these are the first characters after a line-cancel character
		then the function 'callback' will be called.  Use this to allow
		client input to trigger a particular action.	*/
	this.addClientEscape = function (ch, callback) {
		if(	(typeof ch !== 'string' && typeof ch !== 'number') ||
			(typeof ch === 'string' && ch.length > 1) ||
			typeof callback !== 'function'
		) {
			throw 'RLogin.addClientEscape: invalid arguments.';
		}
		clientEscapes[ch.charCodeAt(0)] = callback;
	}

	this.connect = function () {

		if (typeof options.port !== 'number' ||
			typeof options.host != 'string'
		) {
			throw 'RLogin: invalid host or port argument.';
		}

		if (typeof options.clientUsername !== 'string') {
			throw 'RLogin: invalid clientUsername argument.';
		}

		if (typeof options.serverUsername !== 'string') {
			throw 'RLogin: invalid serverUsername argument.';
		}

		if (typeof options.terminalType !== 'string') {
			throw 'RLogin: invalid terminalType argument.';
		}

		if (typeof options.terminalSpeed !== 'number') {
			throw 'RLogin: invalid terminalSpeed argument.';
		}

		if (handle.connect(options.host, options.port)) {
			handle.sendBin(0, 1);
			handle.send(options.clientUsername);
			handle.sendBin(0, 1);
			handle.send(options.serverUsername);
			handle.sendBin(0, 1);
			handle.send(options.terminalType + '/' + options.terminalSpeed);
			handle.sendBin(0, 1);
			while (handle.is_connected && handle.nread < 1) {
				mswait(5);
			}
			getServerData();
		} else {
			throw 'RLogin: Unable to connect to server.';
		}

	}

	this.cycle = function () {

		if (!handle.is_connected) {
			state.connected = false;
			return;
		}

		getServerData();

		if (state.suspendInput || clientBuffer.length < 1) return;

		while (clientBuffer.length > 0) {
			handle.sendBin(clientBuffer.shift(), 1);
		}

	}

	this.disconnect = function () {
		handle.close();
		state.connected = false;
	}

}

try {

	wss = new WebSocketProxy(client);

	if (typeof wss.headers['Cookie'] == 'undefined') {
		err('No cookie from WebSocket client.');
	}

    var cookie = null;
    wss.headers['Cookie'].split(';').some(
        function (e) {
            if (e.search(/^\s*synchronet\=/) == 0) {
                cookie = e;
                return true;
            } else {
                return false;
            }
        }
    );
    if (cookie === null) err('Invalid cookie from WebSocket client.');
    cookie = cookie.replace(/^\s*synchronet\=/, '').split(',');

	cookie[0] = parseInt(cookie[0]);
	if (cookie.length < 2 || isNaN(cookie[0]) || cookie[0] < 1 || cookie[0] > system.lastuser) {
        log('cookie ' + JSON.stringify(cookie));
		err('Invalid cookie from WebSocket client.');
	}

	var usr = new User(cookie[0]);
	var session = getSession(usr.number);
	if (!session) {
		err('Unable to read web session file for user #' + usr.number);
	}
	if (cookie[1] != session.key) {
		err('Session key mismatch for user #' + usr.number);
	}
	if (typeof session.xtrn !== 'string' ||
		typeof xtrn_area.prog[session.xtrn] === 'undefined'
	) {
		err('Invalid external program code.');
	}

	var f = new File(file_cfgname(system.ctrl_dir, 'sbbs.ini'));
	if (!f.open('r')) err('Unable to open sbbs.ini.');
	var ini = f.iniGetObject('BBS');
	f.close();

    if (typeof ini.RLoginInterface === 'undefined') {
        var rlogin_addr = '127.0.0.1';
    } else {
        var rlogin_addr = ini.RLoginInterface.split(/,/)[0];
        var ra = parseInt(rlogin_addr.replace(/[^\d]/g, ''));
        if (isNaN(ra) || ra == 0) rlogin_addr = '127.0.0.1';
    }

	rlogin = new RLoginClient(
		{	host : rlogin_addr,
			port : ini.RLoginPort,
			clientUsername : usr.security.password,
			serverUsername : usr.alias,
			terminalType : 'xtrn=' + session.xtrn,
			terminalSpeed : 115200
		}
	);
	rlogin.connect();
	log(LOG_DEBUG, usr.alias + ' logged on via RLogin for ' + session.xtrn);

	while (client.socket.is_connected && rlogin.connected) {

		wss.cycle();
		rlogin.cycle();

		var send = rlogin.receive();
		if (send.length > 0) wss.send(send);

		while (wss.data_waiting) {
			var data = wss.receiveArray();
			rlogin.send(data);
		}

		mswait(5);

	}

} catch (er) {

	log(LOG_ERR, er);

} finally {
	rlogin.disconnect();
	client.socket.close();
}
