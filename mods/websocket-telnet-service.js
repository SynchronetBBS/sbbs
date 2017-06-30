load('sbbsdefs.js');
load('websocket-proxy.js');
load('modopts.js');

function log_err(msg) {
	log(LOG_DEBUG, msg);
	client.socket.close();
	exit();
}

var TelnetClient = function(host, port) {

    var MAX_BUFFER = 32767;

    var TELNET_DATA = 0;
    var TELNET_IAC = 1;
    var TELNET_SUBNEGOTIATE = 2;
    var TELNET_SUBNEGOTIATE_IAC = 3;
    var TELNET_WILL = 4;
    var TELNET_WONT = 5;
    var TELNET_DO = 6;
    var TELNET_DONT = 7;

    var TELNET_CMD_TTYLOC = 28;

    var state = TELNET_DATA;

    var buffers = {
        rx : [], // From server
        tx : [] // To server
    };

    var socket = new Socket();
    socket.connect(host, port);

    this.__defineGetter__(
        'connected',
        function () {
            return socket.is_connected;
        }
    );

    this.__defineSetter__(
        'connected',
        function (bool) {
            if (!bool && socket.is_connected) {
                socket.close();
            } else if (bool && !socket.is_connected) {
                socket.connect(host, port);
            }
        }
    );

    this.__defineGetter__(
        'data_waiting',
        function () {
            return (buffers.rx.length > 0);
        }
    );

    function receive() {

        var rx = [];

        while (socket.data_waiting && rx.length < MAX_BUFFER) {
            var nr = (socket.nread >= 4 ? 4 : (socket.nread >= 2 ? 2 : 1));
            var bin = socket.recvBin(nr);
            if (nr === 4) {
                rx.push((bin&(255<<24))>>>24);
                rx.push((bin&(255<<16))>>>16);
            }
            if (nr >= 2) rx.push((bin&(255<<8))>>>8);
            rx.push(bin&255);
        }

        while (rx.length > 0) {
            var b = rx.shift();
            switch (state) {
                case TELNET_DATA:
                    if (b == 0xFF) {
                        state = TELNET_IAC;
                    } else {
                        buffers.rx.push(b);
                    }
                    break;
                case TELNET_IAC:
                    switch (b) {
                        case 0xF1: // NOP: No operation.
                        case 0xF2: // Data Mark: The data stream portion of a Synch. This should always be accompanied by a TCP Urgent notification.
                        case 0xF3: // Break: NVT character BRK.
                        case 0xF4: // Interrupt Process: The function IP.
                        case 0xF5: // Abort output: The function AO.
                        case 0xF6: // Are You There: The function AYT.
                        case 0xF7: // Erase character: The function EC.
                        case 0xF8: // Erase Line: The function EL.
                        case 0xF9: // Go ahead: The GA signal
                            // Ignore these single byte commands
                            state = TELNET_DATA;
                            break;
                        case 0xFA: // Subnegotiation
                            state = TELNET_SUBNEGOTIATE;
                            break;
                        case 0xFB: // Will
                            state = TELNET_WILL;
                            break;
                        case 0xFC: // Wont
                            state = TELNET_WONT;
                            break;
                        case 0xFD: // Do
                            state = TELNET_DO;
                            break;
                        case 0xFE: // Dont
                            state = TELNET_DONT;
                            break;
                        case 0xFF:
                            buffers.rx.push(0xFF);
                            state = TELNET_DATA;
                            break;
                    }
                    break;
                case TELNET_SUBNEGOTIATE:
                    if (b == 0xFF) state = TELNET_SUBNEGOTIATE_IAC;
                    break;
                case TELNET_SUBNEGOTIATE_IAC:
                    if (b == 0xFF) {
                        state = TELNET_SUBNEGOTIATE;
                    } else if (b == 0xF0) {
                        state = TELNET_DATA;
                    } else {
                        // Unexpected
                        state = TELNET_DATA;
                    }
                    break;
                case TELNET_DO:
                    switch (b) {
                        // This will bork with IPV6
                        case TELNET_CMD_TTYLOC:
                            socket.sendBin(255, 1);
                            socket.sendBin(250, 1);
                            socket.sendBin(28, 1);
                            socket.sendBin(0, 1);
                            client.ip_address.split('.').forEach(
                                function (e) {
                                    e = parseInt(e);
                                    socket.sendBin(e, 1);
                                    if (e === 255) socket.sendBin(e, 1);
                                }
                            );
                            client.ip_address.split('.').forEach(
                                function (e) {
                                    e = parseInt(e);
                                    socket.sendBin(e, 1);
                                    if (e === 255) socket.sendBin(e, 1);
                                }
                            );
                            socket.sendBin(255, 1);
                            socket.sendBin(240, 1);
                            break;
                        default:
                            break;
                    }
                    state = TELNET_DATA;
                    break;
                case TELNET_WILL:
                case TELNET_WONT:
                case TELNET_DONT:
                    state = TELNET_DATA;
                    break;
                default:
                    break;
            }
        }

    }

    function transmit() {

        if (!socket.is_connected || buffers.tx.length < 1) return;

        while (buffers.tx.length >= 4) {
            var chunk = (buffers.tx.shift()<<24);
            chunk |= (buffers.tx.shift()<<16);
            chunk |= (buffers.tx.shift()<<8);
            chunk |= buffers.tx.shift();
            socket.sendBin(chunk, 4);
        }

        if (buffers.tx.length >= 2) {
            var chunk = (buffers.tx.shift()<<8);
            chunk |= buffers.tx.shift();
            socket.sendBin(chunk, 2);
        }

        if (buffers.tx.length > 0) {
            socket.sendBin(buffers.tx.shift(), 1);
        }

    }

    this.receive = function () {
        return buffers.rx.splice(0, buffers.rx.length);
    }

    this.send = function (arr) {

        if (typeof arr === 'string') {
            var arr = arr.map(
                function (e) {
                    return ascii(e);
                }
            );
        }

        buffers.tx = buffers.tx.concat(arr);

    }

    this.cycle = function () {
        receive();
        transmit();
    }

}

try {

    var f = new File(file_cfgname(system.ctrl_dir, 'sbbs.ini'));
    if (!f.open('r')) log_err('Unable to open sbbs.ini.');
    var ini = f.iniGetObject('BBS');
    f.close();

    if (typeof ini.TelnetInterface === 'undefined' || ini.TelnetInterface === '0.0.0.0') {
        var telnet_addr = '127.0.0.1';
    } else {
        var telnet_addr = ini.TelnetInterface;
    }

    var wss = new WebSocketProxy(client);
	var wsspath = wss.headers.Path.split('/');
	if (wsspath.length < 3 || isNaN(parseInt(wsspath[2]))) {
		var telnet = new TelnetClient(telnet_addr, ini.TelnetPort);
	} else {
		var _settings = get_mod_options('web');
		if (typeof _settings.allowed_ftelnet_targets !== 'string') {
			throw 'Client supplied Path but no allowed_ftelnet_targets supplied in modopts.ini [web] section.';
		}
		var targets = _settings.allowed_ftelnet_targets.split(',');
		if (!targets.some(function (e) { var target = e.split(':'); return target[0] === wsspath[1] && target[1] === wsspath[2]; })) {
			throw 'Client supplied Path is not in allowed_ftelnet_targets list.';
		}
		log('Using client-supplied target ' + wsspath[1] + ':' + wsspath[2]);
		var telnet = new TelnetClient(wsspath[1], parseInt(wsspath[2]));
	}

	while (client.socket.is_connected && telnet.connected) {

    	wss.cycle();
        telnet.cycle();

        if (telnet.data_waiting) {
            var data = telnet.receive();
            wss.send(data);
        }

		while (wss.data_waiting) {
			var data = wss.receiveArray();
            telnet.send(data);
		}

        mswait(5);

	}

} catch (err) {

	log(LOG_ERR, 'Caught: ' + err);

} finally {

	client.socket.close();

}
