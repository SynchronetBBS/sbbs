"use strict";
require("userdefs.js", "USER_DELETED");

// MQTT class

function MQTT() {
	// MQTT.Connection objects
	this.connected = {};	// Currently have a valid sock object, have sent a CONNACK, have a Client ID
	this.disconnected = {};	// No longer have a valid sock object, have a Client ID
	this.unconnected = {};	// Have a valid sock object, and is waiting for CONNECT message, no Client ID

	// MQTT.Topic objects
	this.topics = {};
};

// Static data

MQTT.psk = {};

MQTT.typename = {
	1:     'CONNECT',
	2:     'CONNACK',
	3:     'PUBLISH',
	4:      'PUBACK',
	5:      'PUBREC',
	6:      'PUBREL',
	7:     'PUBCOMP',
	8:   'SUBSCRIBE',
	9:      'SUBACK',
	10: 'UNSUBSCRIBE',
	11:    'UNSUBACK',
	12:     'PINGREQ',
	13:    'PINGRESP',
	14:  'DISCONNECT',
	15:        'AUTH',
};

MQTT.type = {};
for (var t in MQTT.typename) {
	MQTT.type[MQTT.typename[t]] = parseInt(t, 10);
}

MQTT.QoSname = {
	0: 'AT_MOST_ONCE',
	1: 'AT_LEAST_ONCE',
	2: 'EXACTLY_ONCE',
};

MQTT.QoS = {};
for (var t in MQTT.QoSname) {
	MQTT.QoS[MQTT.QoSname[t]] = parseInt(t, 10);
}

MQTT.properties = {
	1: {name: 'Payload format Indicator',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.PUBLISH,
			"Will Properties"
		],
		checkValid: function(val) {
			if (val === 0)
				return true;
			if (val === 1)
				return true;
			return false;
		}
	},
	2: {name: 'Message Expiry Interval',
		getter: 'getFourByte',
		setter: 'encodeFourByte',
		valid:[
			MQTT.type.PUBLISH,
			"Will Properties"
		]
	},
	3: {name: 'Content Type',
		getter: 'getUTF8String',
		setter: 'encodeUTF8String',
		valid:[
			MQTT.type.PUBLISH,
			"Will Properties"
		]
	},
	8: {name: 'Response Topic',
		getter: 'getUTF8String',
		setter: 'encodeUTF8String',
		valid: [
			MQTT.type.PUBLISH,
			"Will Properties"
		]
	},
	9: {name: 'Correlation Data',
		getter: 'getBinaryData',
		setter: 'encodeBinaryData',
		valid: [
			MQTT.type.PUBLISH,
			"Will Properties"
		]
	},
	11: {name: 'Subscription Identifier',
		getter: 'getVBI',
		setter: 'encodeVBI',
		valid: [
			MQTT.type.PUBLISH,
			MQTT.type.SUBSCRIBE
		],
		checkValid: function(val) {
			if (val === 0)
				throw new Error('0x82 Subscription Identifier of zero');
			return true;
		},
		multiple: true
	},
	17: {name: 'Session Expiry Interval',
		getter: 'getFourByte',
		setter: 'encodeFourByte',
		valid: [
			MQTT.type.CONNECT,
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	18: {name: 'Assigned Client Identifier',
		getter: 'getUTF8String',
		setter: 'encodeUTF8String',
		valid: [
			MQTT.type.CONNACK
		]
	},
	19: {name: 'Server Keep Alive',
		getter: 'getTwoByte',
		setter: 'encodeTwoByte',
		valid: [
			MQTT.type.CONNACK
		]
	},
	21: {name: 'Authentication Method',
		getter: 'getUTF8String',
		setter: 'encodeUTF8String',
		valid: [
			MQTT.type.CONNECT,
			MQTT.type.CONNACK,
			MQTT.type.AUTH
		]
	},
	22: {name: 'Authentication Data',
		getter: 'getBinaryData',
		setter: 'encodeBinaryData',
		valid: [
			MQTT.type.CONNECT,
			MQTT.type.CONNACK,
			MQTT.type.AUTH
		]
	},
	23: {name: 'Request Problem Information',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.CONNECT
		],
		checkValid: function(val) {
			if (val === 0 || val === 1)
				return true;
			throw new Error('0x82 Request Problem Information of ' + val);
		}
	},
	24: {name: 'Will Delay Interval',
		getter: 'getFourByte',
		setter: 'encodeFourByte',
		valid: [
			'Will Properties'
		]
	},
	25: {name: 'Request Response Information',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.CONNECT
		],
		checkValid: function(val) {
			if (val === 0 || val === 1)
				return true;
			throw new Error('0x82 Request Response Information of ' + val);
		}
	},
	26: {name: 'Response Information',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.CONNACK
		]
	},
	28: {name: 'Server Reference',
		getter: 'getUTF8String',
		setter: 'encodeUTF8String',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	31: {name: 'Reason String',
		getter: 'getUTF8String',
		setter: 'encodeUTF8String',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.PUBREL,
			MQTT.type.PUBCOMP,
			MQTT.type.SUBACK,
			MQTT.type.UNSUBACK,
			MQTT.type.DISCONNECT,
			MQTT.type.AUTH
		]
	},
	33: {name: 'Receive Maximum',
		getter: 'getTwoByte',
		setter: 'encodeTwoByte',
		valid: [
			MQTT.type.CONNECT,
			MQTT.type.CONNACK
		],
		checkValid: function(val) {
			if (val === 0)
				throw new Error('0x82 Receive Maximum of zero');
			return true;
		}
	},
	34: {name: 'Topic Alias Maximum',
		getter: 'getTwoByte',
		setter: 'encodeTwoByte',
		valid: [
			MQTT.type.CONNECT,
			MQTT.type.CONNACK
		]
	},
	35: {name: 'Topic Alias',
		getter: 'getTwoByte',
		setter: 'encodeTwoByte',
		valid: [
			MQTT.type.PUBLISH
		]
	},
	36: {name: 'Maximum QoS',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.CONNACK
		],
		checkValid: function(val) {
			if (val == 0)
				return true;
			if (val == 1)
				return true;
			throw new Error('0x82 Maximum QoS Invalid: ' + val);
		}
	},
	37: {name: 'Retain Available',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.CONNACK
		],
		checkValid: function(val) {
			if (val == 0)
				return true;
			if (val == 1)
				return true;
			return false;
		}
	},
	38: {name: 'User Property',
		getter: 'getUTF8StringPair',
		setter: 'encodeUTF8StringPair',
		valid: [
			MQTT.type.CONNECT,
			MQTT.type.CONNACK,
			MQTT.type.PUBLISH,
			'Will Properties',
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.PUBREL,
			MQTT.type.PUBCOMP,
			MQTT.type.SUBSCRIBE,
			MQTT.type.SUBACK,
			MQTT.type.UNSUBSCRIBE,
			MQTT.type.UNSUBACK,
			MQTT.type.DISCONNECT,
			MQTT.type.AUTH
		],
		multiple: true
	},
	39: {name: 'Maximum Packet Size',
		getter: 'getFourByte',
		setter: 'encodeFourByte',
		valid: [
			MQTT.type.CONNECT,
			MQTT.type.CONNACK
		],
		checkValid: function(val) {
			if (val === 0)
				throw new Error('0x82 Maximum Packet Size of zero');
			return true;
		},
	},
	40: {name: 'Wildcard Subscription Available',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.CONNACK
		],
		checkValid: function(val) {
			if (val == 0)
				return true;
			if (val == 1)
				return true;
			throw new Error('0x82 Wildcard Subscription Available Invalid: ' + val);
		}
	},
	41: {name: 'Subscription Identifier Available',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.CONNACK
		],
		checkValid: function(val) {
			if (val == 0)
				return true;
			if (val == 1)
				return true;
			throw new Error('0x82 Subscription Identifier Available Invalid: ' + val);
		}
	},
	42: {name: 'Shared Subscription Available',
		getter: 'getByte',
		setter: 'encodeByte',
		valid: [
			MQTT.type.CONNACK
		],
		checkValid: function(val) {
			if (val == 0)
				return true;
			if (val == 1)
				return true;
			throw new Error('0x82 Shared Subscription Available Invalid: ' + val);
		}
	}
}

MQTT.reason_code = {
	0: {
		name: 'Success',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.PUBREL,
			MQTT.type.PUBCOMP,
			MQTT.type.UNSUBACK,
			MQTT.type.AUTH,
			MQTT.type.DISCONNECT,
			MQTT.type.SUBACK
		]
	},
	1: {
		name: 'Granted QoS 1',
		valid: [MQTT.type.SUBACK]
	},
	2: {
		name: 'Granted QoS 2',
		valid: [MQTT.type.SUBACK]
	},
	4: {
		name: 'Disconnect with Will Message',
		valid: [MQTT.type.DISCONNECT]
	},
	16: {
		name: 'No matching subscribers',
		valid: [
			MQTT.type.PUBACK,
			MQTT.type.PUBREC
		]
	},
	17: {
		name: 'No subscription existed',
		valid: [MQTT.type.UNSUBACK]
	},
	24: {
		name: 'Continue authentication',
		valid: [MQTT.type.AUTH]
	},
	25: {
		name: 'Re-authenticate',
		valid: [MQTT.type.AUTH]
	},
	128: {
		name: 'Unspecified error',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.SUBACK,
			MQTT.type.UNSUBACK,
			MQTT.type.DISCONNECT
		]
	},
	129: {
		name: 'Malformed Packet',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	130: {
		name: 'Protocol Error',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	131: {
		name: 'Implementation specific error',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.PUBACK,
			MQTT.type.SUBACK,
			MQTT.type.UNSUBACK,
			MQTT.type.DISCONNECT
		]
	},
	132: {
		name: 'Unsupported Protocol Version',
		valid: [
			MQTT.type.CONNACK
		]
	},
	133: {
		name: 'Client Identifier not valid',
		valid: [
			MQTT.type.CONNACK
		]
	},
	134: {
		name: 'Bad User Name or Password',
		valid: [
			MQTT.type.CONNACK
		]
	},
	135: {
		name: 'Not authorized',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.SUBACK,
			MQTT.type.UNSUBACK,
			MQTT.type.DISCONNECT
		]
	},
	136: {
		name: 'Server unavailable',
		valid: [
			MQTT.type.CONNACK
		]
	},
	137: {
		name: 'Server busy',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	138: {
		name: 'Banned',
		valid: [
			MQTT.type.CONNACK
		]
	},
	139: {
		name: 'Server shutting down',
		valid: [
			MQTT.type.DISCONNECT
		]
	},
	140: {
		name: 'Bad authentication method',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	141: {
		name: 'Keep Alive timeout',
		valid: [
			MQTT.type.DISCONNECT
		]
	},
	142: {
		name: 'Session taken over',
		valid: [
			MQTT.type.DISCONNECT
		]
	},
	143: {
		name: 'Topic Filter invalid',
		valid: [
			MQTT.type.SUBACK,
			MQTT.type.UNSUBACK,
			MQTT.type.DISCONNECT
		]
	},
	144: {
		name: 'Topic Name invalid',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.DISCONNECT
		]
	},
	145: {
		name: 'Packet Identifier in use',
		valid: [
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.SUBACK,
			MQTT.type.UNSUBACK
		]
	},
	146: {
		name: 'Packet Identifier not found',
		valid: [
			MQTT.type.PUBREL,
			MQTT.type.PUBCOMP
		]
	},
	147: {
		name: 'Receive Maximum exceeded',
		valid: [
			MQTT.type.DISCONNECT
		]
	},
	148: {
		name: 'Topic Alias Invalid',
		valid: [
			MQTT.type.DISCONNECT
		]
	},
	149: {
		name: 'Packet too large',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	150: {
		name: 'Message rate too high',
		valid: [
			MQTT.type.DISCONNECT
		]
	},
	151: {
		name: 'Quota exceeded',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.SUBACK,
			MQTT.type.DISCONNECT
		]
	},
	152: {
		name: 'Administrative action',
		valid: [
			MQTT.type.DISCONNECT
		]
	},
	153: {
		name: 'Payload format invalid',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.PUBACK,
			MQTT.type.PUBREC,
			MQTT.type.DISCONNECT
		]
	},
	154: {
		name: 'Retain not supported',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	155: {
		name: 'QoS not supported',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	156: {
		name: 'Use another server',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	157: {
		name: 'Server moved',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	158: {
		name: 'Shared Subscriptions not supported',
		valid: [
			MQTT.type.SUBACK,
			MQTT.type.DISCONNECT
		]
	},
	159: {
		name: 'Connection rate exceeded',
		valid: [
			MQTT.type.CONNACK,
			MQTT.type.DISCONNECT
		]
	},
	160: {
		name: 'Maximum connect time',
		valid: [
			MQTT.type.DISCONNECT
		]
	},
	161: {
		name: 'Subscription Identifiers not supported',
		valid: [
			MQTT.type.SUBACK,
			MQTT.type.DISCONNECT
		]
	},
	162: {
		name: 'Wildcard Subscriptions not supported',
		valid: [
			MQTT.type.SUBACK,
			MQTT.type.DISCONNECT
		]
	},
};

// Static methods

MQTT.encodeByte = function(val) {
	if (val < 0 || val > 255)
		throw new Error('Byte ' + val + ' out of range');
	return ascii(val);
}

MQTT.encodeTwoByte = function(val) {
	if (val < 0 || val > 0xffff)
		throw new Error('Two Byte ' + val + ' out of range');
	return ascii((val & 0xff00) >> 8) + ascii(val & 0xff);
}

MQTT.encodeFourByte = function(val) {
	if (val < 0 || val > 0xffffffff)
		throw new Error('Four Byte ' + val + ' out of range');
	return ascii((val & 0xff000000) >> 24) + ascii((val & 0xff0000) >> 16) + ascii((val & 0xff00) >> 8) + ascii(val & 0xff);
}

MQTT.encodeVBI = function(val) {
	if (val < 0 || val > 268435455)
		throw new Error('Variable Byte ' + val + ' out of range');
	var ret = '';
	do {
		var b = val & 0x7F;
		val >>= 7;
		if (val)
			b |= 0x80;
		ret = ret + ascii(b);
	} while(val);
	return ret;
}

MQTT.encodeUTF8String = function(val) {
	if (val.length > 0xffff)
		throw new Error('UTF8 String too long: '+val.length);
	var ret = MQTT.encodeTwoByte(val.length);
	ret += val;
	return ret;
}

MQTT.encodeBinaryData = function(val) {
	if (val.length > 0xffff)
		throw new Error('Bianry Data too long: '+val.length);
	var ret = MQTT.encodeTwoByte(val.length);
	ret += val;
	return ret;
}

MQTT.encodeUTF8StringPair = function(val) {
	return MQTT.encodeUTF8String(val.name) + MQTT.encodeUTF8String(val.value);
}

MQTT.encodeProperties = function(props) {
	var ret = '';
	var prop;
	var i;
	for (prop in props) {
		if (MQTT.properties[prop].multiple) {
			for (i in props[prop]) {
				ret += MQTT.encodeVBI(prop);
				ret += MQTT[MQTT.properties[prop].setter](props[prop][i]);
			}
		}
		else {
			ret += MQTT.encodeVBI(prop);
			ret += MQTT[MQTT.properties[prop].setter](props[prop]);
		}
	}
	return MQTT.encodeVBI(ret.length) + ret;
}

MQTT.validateUTF8String = function(str) {
	var len = str.length;
	var got = 0;
	var cbytes = 0;
	var cval = 0;
	var b;
	while (got < len) {
		b = ascii(str[got]);
		if (cbytes == 0) {
			if ((b & 0x80) == 0) {
				cval = b;
			}
			else if ((b & 0xe0) == 0xc0) {
				cbytes = 1;
				cval = b & 0x1f;
				if (cval == 0)
					throw new Error('0x81 Overlong UTF-8 encoding');
			}
			else if ((b & 0xf0) == 0xe0) {
				cbytes = 2;
				cval = b & 0x0f;
				if (cval == 0)
					throw new Error('0x81 Overlong UTF-8 encoding');
			}
			else if ((b & 0xf8) == 0xf0) {
				cbytes = 3;
				cval = b & 0x07;
				if (cval == 0)
					throw new Error('0x81 Overlong UTF-8 encoding');
			}
			else
				throw new Error('0x81 Invalid UTF-8 first byte');
		}
		else {
			if ((b & 0xc0) != 0x80)
				throw new Error('0x81 Invalid UTF-8 continuation byte');
			cval <<= 6;
			cval |= (b & 0x3f);
			cbytes--;
		}
		if (cbytes == 0) {
			switch (cval) {
				case cval <= 0x1f:			// Control characters
				case cval >= 0x7f && cval <= 0x9f:	// Control characters
				case cval >= 0xd800 && cval <= 0xdfff:	// Surrogates
				case (cval & 0xffff) == 0xffff:		// Non-characters
				case (cval & 0xffff) == 0xfffe:		// Non-characters
				case cval >= 0xfdd0 && cval <= 0xfdef:	// Non-characters
				case cval >= 0x10ffff:			// Non-codepoints
					throw new Error(format('0x81 Disallowed Unicode Code Point U+%04X', cval));
			}
			// TODO: We could convert this to a proper JS string
			//       if we ever need to actually look at them (ie: UserID etc.)
			// Synchronet JS is too old for the easy way:
			//ret += String.fromCodePoint(cval);
			// So we need to do surrogates...
			//if (cval <= 0xffff)
			//	ret += String.fromCharCode(cval);
			//else {
			//	var Uprime = cval - 0x10000;
			//	ret += String.fromCharCode(((cval & 0xffc00) >> 10) + 0xD800, (cval & 0x3ff) + 0xDC00);
			//}
		}
		got++;
	}
	if (cbytes > 0)
		throw new Error('Last codepoint truncated');
};

// Class properties

MQTT.prototype.gotConnection = function(sock) {
	if (sock !== null) {
		var newconn = new MQTT.Connection(sock, this);
		log(LOG_INFO, "Accepted new connection");
	}
};

// MQTT.Connection class

MQTT.Connection = function(sock, broker) {
	if (typeof sock != 'object')
		throw new Error('sock is not a (Socket) object');

	if (sock.constructor.name != 'Socket')
		throw new Error('sock is not a Socket object');
	if (!sock.is_connected)
		throw new Error('sock is not connected');
	if (!sock.type == Socket.SOCK_STREAM)
		throw new Error('sock is not SOCK_STREAM');
	sock.tls_psk = MQTT.psk;
	sock.ssl_server = true;
	this.sock = sock;
	this.sock.connection = this;
	this.sock.network_byte_order = true;
	this.sock.nonblocking = true;

	if (!MQTT.prototype.isPrototypeOf(broker))
		throw new Error('Broker is not an MQTT object');
	this.broker = broker;

	this.tx_buf = "";
	this.tx_once = null;
	this.tx_close = false;
	this.tx_service_pending = false;

	this.rx_buf = "";
	this.rx_need = 0;
	this.rx_callback = null;
	this.rx_listener = null;
	this.rx_once = this.sock.once('read', MQTT.Connection.rxPacket);

	this.keep_alive = 0;
	this.session_expiry = 0;
	this.receive_maximum = 0;
	this.maximum_packet_size = 0;
	this.topic_alias_maximum = 0;
	this.request_problem_information = false;
	this.will = null;
	this.will_timeout = null;
	this.session_timeout = null;
	this.keep_alive_timeout = null;
	this.client_id = null;
	this.user_name = null;
	this.client_used_pids = {};
	this.my_used_pids = {};
	this.next_pid = 1;

	this.protocol_version = 0;

	this.got_connect = false;
};

MQTT.Connection.rxPacket = function() {
	try {
		this.connection.rx_once = null;
		this.connection.rx_packet = new MQTT.Packet();
		this.connection.rx_packet.recv(this);
	}
	catch (e) {
		this.connection.error(e);
	}
};

MQTT.Connection.nextPacket = function() {
	if (this.rx_buf.length > 0) {
		this.rx_packet = new MQTT.Packet();
		this.rx_packet.recv(this.sock);
	}
	else {
		if (this.rx_once === null && this.sock !== null)
			this.rx_once = this.sock.once('read', MQTT.Connection.rxPacket);
	}
};

MQTT.Connection.txData = function() {
	this.connection.tx_once = null;
	var bytes = this.send(this.connection.tx_buf);
	if (bytes === null) {
		// Error, tear down connection...
		this.connection.tearDown();
		return;
	}
	this.connection.tx_buf = this.connection.tx_buf.substr(bytes);
	if (this.connection.tx_buf !== '')
		this.connection.tx_once = this.once('write', MQTT.Connection.txData);
	else if (this.connection.tx_close)
		this.is_writable = false;
}

MQTT.Connection.prototype.send = function(pkt) {
	if (this.tx_close)
		return false;
	if (this.sock === null)
		return false;
	if (this.tx_buf === '')
		this.tx_once = this.sock.once('write', MQTT.Connection.txData);
	this.tx_buf += pkt.serialize();
	log(LOG_INFO, "Sending " + MQTT.typename[pkt.type] + ' packet to '+this.client_id);
	return true;
//for (var i = 0; i < data.length; i++)
//log(LOG_INFO, format("Byte %u: %2$02x (%2$d)", i, ascii(data[i])));
}

MQTT.Connection.probateWill = function() {
	this.will_timeout = null;
	this.rx_packet = this.will;
	this.will = null;
	this.handlePUBLISH();
};

MQTT.Connection.expireSession = function() {
	this.session_timeout = null;
	delete this.broker.disconnected[this.client_id];
};

MQTT.Connection.timeToDie = function() {
	this.keep_alive_timeout = null;
	if (this.sock !== null) {
		this.error(new Error('0x8D Keep Alive timeout'));
	}
};

MQTT.Connection.prototype.tearDown = function() {
	var sock = this.sock;

	if (sock !== null)
		this.sock.connection = null;
	if (this.rx_listener !== null)
		removeEventListener(this.rx_listener);
	if (this.rx_once != null) {
		this.sock.clearOnce('read', this.rx_once);
		this.rx_once = null;
	}
	if (this.tx_once != null) {
		this.sock.clearOnce('write', this.tx_once);
		this.tx_once = null;
	}
	if (sock !== null) {
		sock.close();
		this.sock = null;
		this.rx_buf = '';
	}
	if (this.broker.connected[this.client_id] !== undefined)
		delete this.broker.connected[this.client_id];
	if (this.will !== null) {
		if (this.will.properties[24] === undefined || this.will.properties[24] === 0) {
			this.rx_packet = this.will;
			this.will = null;
			this.handlePUBLISH();
		}
		else {
			this.will_timeout = js.setTimeout(MQTT.Connection.probateWill, this.will.properties[24] * 1000, this);
		}
	}
	if (this.session_expiry > 0) {
		this.broker.disconnected[this.client_id] = this;
		if (this.session_expiry < 0xFFFFFFFF) {
			this.session_timeout = js.setTimeout(MQTT.Connection.expireSession, this.session_expiry * 1000, this);
		}
	}
};

MQTT.Connection.prototype.error = function(error) {
	var resp;
	// TODO: PUBREC/PUBREL, specific failure 0x92
	if (error.message.slice(0, 5) === '0x92 ') {
		if (this.rx_packet.type === MQTT.type.PUBREC)
			resp = new MQTT.Packet.PUBREL();
		if (this.rx_packet.type === MQTT.type.PUBREL)
			resp = new MQTT.Packet.PUBCOMP();
		resp.response_code = 0x92;
		this.send(resp);
		return;
	}
	if (this.got_connect) {
		resp = new MQTT.Packet.DISCONNECT();
	}
	else {
		resp = new MQTT.Packet.CONNACK();
	}
	if (error.message[0] === '0' && error.message[1] === 'x') {
		resp.reason_code = parseInt(error.message, 16);
	}
	else {
		resp.reason_code = 0x83;
	}
	this.send(resp);
	this.tx_close = true;
	this.tearDown();
};

MQTT.Connection.prototype.handleCONNECT = function() {
	var pkt = this.rx_packet;
	if (this.got_connect)
		throw new Error('0x82 Second CONNECT on socket');
	if (pkt.protocol_name !== 'MQTT')
		throw new Error('0x84 Invalid protocol name');
	if (pkt.protocol_version !== 5)
		throw new Error('0x84 Invalid protocol version: ' + pkt.protocol_version);
	this.got_connect = true;
	var resp = new MQTT.Packet.CONNACK();
	resp.flags = 0;
	// Shared Subscriptions not available
	resp.addProperty(resp.type, 42, 1);

	// Assign random client ID if needed...
	if (pkt.client_id === '') {
		if (!pkt.connect_flags.clean_start)
			throw new Error('0x82 Random Client ID reuested for new session');
		do {
			var now = new Date();
			pkt.client_id = format('%04d%02d%02dT%02d%02d%02dZ', now.getUTCFullYear(), now.getUTCMonth() + 1, now.getUTCDay(), now.getUTCHours(), now.getUTCMinutes(), now.getUTCSeconds());
			pkt.client_id += '-' + random(999999999).toString();
		} while(this.broker.connected[pkt.client_id] !== undefined || this.broker.disconnected[pkt.client_id] !== undefined);
		resp.addProperty(resp.type, 18, pkt.client_id);
	}

	// Look for an existing session...
	var oldconn = null;
	if (this.broker.disconnected[pkt.client_id] !== undefined) {
		oldconn = this.broker.disconnected[pkt.client_id];
		if (oldconn.will_timeout !== null)
			js.cleartimeout(oldconn.will_timeout);
		if (oldconn.session_timeout !== null)
			js.cleartimeout(oldconn.session_timeout);
		if (oldconn.keep_alive_timeout !== null)
			js.cleartimeout(oldconn.keep_alive_timeout);
		delete this.broker.disconnected[pkt.client_id];
	}
	if (this.broker.connected[pkt.client_id] !== undefined) {
		oldconn = this.broker.connected[pkt.client_id];
		if (oldconn.will_timeout !== null)
			js.cleartimeout(oldconn.will_timeout);
		if (oldconn.session_timeout !== null)
			js.cleartimeout(oldconn.session_timeout);
		if (oldconn.keep_alive_timeout !== null)
			js.cleartimeout(oldconn.keep_alive_timeout);
		delete this.broker.connected[pkt.client_id];
		var oldresp = new MQTT.Packet.DISCONNECT();
		oldresp.reason_code = 0x8E;
		oldconn.sock.send(oldresp);
		oldconn.tx_close = true;
	}

	this.subscriptions = {};
	this.tx_unacked = [];
	this.tx_queued = [];
	this.rx_unacked = [];
	this.client_id = pkt.client_id;

	// Copy state from old connection
	var i;
	if (oldconn !== null && !pkt.connect_flags.clean_start) {
		this.subscriptions = oldconn.subscriptions;
		for (i in this.subscriptions)
			this.subscriptions[i].conn = this;
		this.tx_unacked = oldconn.tx_unacked;
		this.tx_queued = oldconn.tx_queued;
		this.rx_unacked = oldconn.rx_unacked;
	}

	if (this.tx_unacked.length > 0) {
		for (i in this.tx_unacked) {
			this.tx_unacked[i].dup = 1;
			this.tx_queue.push(this.tx_unacked[i]);
		}
	}

	this.broker.connected[pkt.client_id] = this;
	this.will = null;

	this.keep_alive = pkt.keep_alive;
	if (pkt.properties[17] !== undefined)
		this.session_expiry = pkt.properties[17];
	if (pkt.properties[33] !== undefined)
		this.receive_maximum = pkt.properties[33];
	if (pkt.properties[39] !== undefined)
		this.maximum_packet_size = pkt.properties[39];
	if (pkt.properties[34] !== undefined)
		this.topic_alias_maximum = pkt.properties[34];
	if (pkt.properties[25] !== undefined) {
		// TODO: Do we do anything with this?
	}
	if (pkt.properties[23] !== undefined)
		this.request_problem_information = (pkt.properties[23] ? true : false);
	var syspass = null;

	if (pkt.connect_flags.password_flag) {
		if (!system.check_syspass(pkt.password))
			throw new Error('0x87 Not Authenticated');
	}
	else
		throw new Error('0x87 Not Authenticated');

	// Set up the last will
	if (pkt.connect_flags.will_flag) {
		this.will = new MQTT.Packet.PUBLISH();
		this.will.QoS = pkt.connect_flags.will_qos;
		this.will.retain = pkt.connect_flags.will_retain;
		this.will.properties = pkt.will.properties;
		this.will.topic_name = pkt.will.topic;
		this.will.payload = pkt.will.payload;
		this.will.is_will = true;
	}
	resp.connack_flags.session_present = 0;
	resp.reason_code = 0;
	this.send(resp);
	if (this.tx_queued.length > 0 && !this.tx_service_pending) {
		js.setImmediate(this.serviceTxQueue, this);
		this.tx_service_pending = true;
	}
};

MQTT.Connection.prototype.handleSUBSCRIBE = function() {
	var resp = new MQTT.Packet.SUBACK();
	resp.flags = 0;
	resp.packet_identifier = this.rx_packet.packet_identifier;
	var i;
	for (i in this.rx_packet.payload) {
		var code = this.rx_packet.payload[i].subscription_options.QoS;
		try {
			var subid = null;
			if (this.rx_packet.properties[11] !== undefined && this.rx_packet.properties[11].length > 0)
				subid = this.rx_packet.properties[11][0];
			new MQTT.Connection.Subscription(this, this.rx_packet.payload[i].topic_filter,
			    this.rx_packet.payload[i].subscription_options, subid);
		}
		catch (e) {
			if (e.message.substr(0, 2) == '0x')
				code = parseInt(e.message, 16);
			else
				throw e;
		}
		resp.payload.push(code);
	}
	this.send(resp);
};

MQTT.Connection.prototype.handleUNSUBSCRIBE = function() {
	var resp = new MQTT.Packet.UNSUBACK();
	resp.flags = 0;
	resp.packet_identifier = this.rx_packet.packet_identifier;
	var i;
	for (i in this.rx_packet.payload) {
		var code = 0;
		try {
			var tfilter = this.rx_packet.payload[i];
			if (this.subscriptions[tfilter] === undefined)
				throw new Error('0x17 No subscription');
			this.subscriptions[tfilter].remove();
		}
		catch (e) {
			if (e.message.substr(0, 2) == '0x')
				code = parseInt(e.message, 16);
			else
				throw e;
		}
		resp.payload.push(code);
	}
	this.send(resp);
};

MQTT.Connection.prototype.handlePINGREQ = function() {
	var resp = new MQTT.Packet.PINGRESP();
	this.send(resp);
};

MQTT.Connection.prototype.handleDISCONNECT = function() {
	var pkt = this.rx_packet;
	if (pkt.reason_code === 0)
		this.will = null;
	if (pkt.properties[17] !== undefined)
		this.session_expiry = pkt.properties[17];
	if (pkt.properties[31] !== undefined)
		log(LOG_INFO, format('%s disconnected with reason "%s" (%02x)', this.client_id, pkt.properties[31], pkt.reason_code));
	else
		log(LOG_INFO, format('%s disconnected with reason code %02x', this.client_id, pkt.reason_code));
	this.tearDown();
};

MQTT.Connection.prototype.getUnusedPID = function() {
	while (this.my_used_pids[this.next_pid] !== undefined) {
		if (this.next_pid === 0xffffffff)
			this.next_pid = 1;
		else
			this.next_pid++;
	}
	this.my_used_pids[this.next_pid] = true;
	return this.next_pid;
};

MQTT.Connection.prototype.handlePUBLISH = function() {
	var resp = null;
	var pkt = this.rx_packet;

	// Create topic if needed
	if (this.broker.topics[pkt.topic_name] === undefined)
		this.broker.topics[pkt.topic_name] = new MQTT.Topic(this.broker, pkt.topic_name);

	// Retain if requested
	if (pkt.retain) {
		if (pkt.payload.length === 0)
			this.broker.topics[pkt.topic_name].retained = null;
		else
			this.broker.topics[pkt.topic_name].retained = pkt.dupeForRetain();
	}

	// Duplicate for every subscriber
	var cid;
	var sid;
	var sent = false;
	for (cid in this.broker.topics[pkt.topic_name].subscribers) {
		var sconn = null;
		var sidsr0 = [];
		var sidsr1 = [];
		var qosr0 = 0;
		var qosr1 = 0;
		for (sid in this.broker.topics[pkt.topic_name].subscribers[cid]) {
			if (cid !== this.client_id || this.broker.topics[pkt.topic_name].subscribers[cid][sid].options.no_local === 0) {
				var rval = 0;
				var qos = 0;
				if (sconn === null)
					sconn = this.broker.topics[pkt.topic_name].subscribers[cid][sid].conn;
				if (this.broker.topics[pkt.topic_name].subscribers[cid][sid].options.retain_as_published)
					rval = pkt.retain;
				qos = this.broker.topics[pkt.topic_name].subscribers[cid][sid].options.QoS;
				if (rval) {
					sidsr1.push(sid);
					if (qos > qosr1)
						qosr1 = qos;
				}
				else {
					sidsr0.push(sid);
					if (qos > qosr0)
						qosr0 = qos;
				}
			}
		}
		if (sconn != null) {
			if (sidsr0.length) {
				var npkt = pkt.dupeForSubscriptions(this, sidsr0, qos);
				sconn.tx_queued.push(npkt);
				if (!sconn.tx_service_pending) {
					js.setImmediate(sconn.serviceTxQueue, sconn);
					sconn.tx_service_pending = true;
				}
				sent = true;
			}
			if (sidsr1.length) {
				var npkt = pkt.dupeForSubscriptions(this, sidsr1, qos);
				npkt.retain = 1;
				sconn.tx_queued.push(npkt);
				if (!sconn.tx_service_pending) {
					js.setImmediate(sconn.serviceTxQueue, sconn);
					sconn.tx_service_pending = true;
				}
				sent = true;
			}
		}
	}

	if (pkt.is_will)
		return;

	if (pkt.QoS === 1) {
		resp = new MQTT.Packet.PUBACK();
		resp.packet_identifier = pkt.packet_identifier;
		if (sent)
			resp.reason_code = 0;
		else
			resp.reason_code = 16;
	}
	if (pkt.QoS === 2) {
		resp = new MQTT.Packet.PUBREC();
		resp.packet_identifier = pkt.packet_identifier;
		this.client_used_pids[pkt.packet_identifier] = true;
		if (sent)
			resp.reason_code = 0;
		else
			resp.reason_code = 16;
		this.rx_unacked.push(pkt);
	}

	if (resp != null)
		this.send(resp);
};

MQTT.Connection.prototype.handlePUBACK = function() {
	var pkt = this.rx_packet;
	// Find this packet identifier in tx_unacked
	var i;
	for (i in this.tx_unacked) {
		if (this.tx_unacked[i].packet_identifier === pkt.packet_identifier) {
			this.tx_unacked.splice(i, 1);
			break;
		}
	}
	delete this.my_used_pids[pkt.packet_identifier];
};

MQTT.Connection.prototype.handlePUBREC = function() {
	var pkt = this.rx_packet;
	// If this was an error, don't try to re-send.
	if (pkt.response_code >= 0x80) {
		this.handlePUBACK();
		return;
	}
	var resp = new MQTT.Packet.PUBREL();
	resp.packet_identifier = pkt.packet_identifier;
	this.send(resp);
};

MQTT.Connection.prototype.handlePUBREL = function() {
	var pkt = this.rx_packet;
	var resp = new MQTT.Packet.PUBCOMP();
	resp.packet_identifier = pkt.packet_identifier;
	// Find this packet identifier in rx_unacked
	var i;
	for (i in this.rx_unacked) {
		if (this.rx_unacked[i].packet_identifier === pkt.packet_identifier) {
			this.rx_unacked.splice(i, 1);
			break;
		}
	}
	delete this.client_used_pids[pkt.packet_identifier];
	this.send(resp);
};

MQTT.Connection.prototype.handlePUBCOMP = function() {
	this.handlePUBACK();
};

MQTT.Connection.prototype.serviceTxQueue = function() {
	this.tx_service_pending = false;
	var pkt = null;
	try {
		var i;
		if (this.sock === undefined)
			return;
		while (this.tx_queued.length > 0) {
			var pkt = this.tx_queued.shift();
			if (this.send(pkt)) {
				pkt.dup = 1;
				if (pkt.dupe_timestamp === undefined)
					pkt.dupe_timestamp = time();
				if (pkt.QoS > 0)
					this.tx_unacked.push(pkt);
			}
			else {
				this.tx_queued.unshift(pkt);
				break;
			}
			pkt = null;
		}
	}
	catch (e) {
		if (pkt !== null)
			this.tx_queued.unshift(pkt);
		log(LOG_WARNING, e.toSource());
		this.error(e);
		return;
	}
};

MQTT.Connection.prototype.handlePacket = function() {
	try {
		if (this.keep_alive_timeout !== null) {
			js.clearTimeout(this.keep_alive_timeout);
			this.keep_alive_timeout = null;
		}
		this.rx_packet.recv(this);
		if (!this.got_connect && this.rx_packet.type != MQTT.type.CONNECT)
			throw new Error('0x82 ' + MQTT.typename[this.rx_packet.type] + ' received before CONNECT');
		if (this.rx_packet.type === MQTT.type.PUBREL) {
			if (this.rx_packet.packet_identifier === undefined)
				throw new Error('Undefined PUBREL packet identifier');
			if (this.client_used_pids[this.rx_packet.packet_identifier] === undefined)
				throw new Error('0x92 Packet Identifier not found');
		}
		else {
			if (this.rx_packet.packet_identifier !== undefined && this.client_used_pids[this.rx_packet.packet_identifier] !== undefined) {
				throw new Error('0x91 Packet Identifier in use');
			}
		}
		this['handle'+MQTT.typename[this.rx_packet.type]]();
		if (this.sock !== null) {
			this.rx_once = this.sock.once('read', MQTT.Connection.rxPacket);
		}
		if (this.keep_alive > 0) {
			this.keep_alive_timeout = js.setTimeout(MQTT.Connection.timeToDie, (this.keep_alive * 1000) * 1.5, this);
			js.clearTimeout(this.keep_alive_timeout);
			this.keep_alive_timeout = null;
		}
	}
	catch (e) {
		log(LOG_WARNING, e.toSource());
		this.error(e);
		return;
	}
};

MQTT.Connection.prototype.getByte = function() {
	if (this.rx_buf.length < 1)
		throw new Error('Insufficient bytes available!');
	var ch = this.rx_buf[0];
	this.rx_buf = this.rx_buf.substr(1);
	return ascii(ch);
};

MQTT.Connection.prototype.getTwoByte = function() {
	if (this.rx_buf.length < 2)
		throw new Error('Insufficient bytes available!');
	var ch1 = this.rx_buf[0];
	var ch2 = this.rx_buf[1];
	this.rx_buf = this.rx_buf.substr(2);
	return (ascii(ch1) << 8) | ascii(ch2);
};

MQTT.Connection.prototype.getFourByte = function() {
	if (this.rx_buf.length < 4)
		throw new Error('Insufficient bytes available!');
	var ch1 = this.rx_buf[0];
	var ch2 = this.rx_buf[1];
	var ch3 = this.rx_buf[2];
	var ch4 = this.rx_buf[3];
	this.rx_buf = this.rx_buf.substr(4);
	return (ascii(ch1) << 24) | (ascii(ch2) << 16) | (ascii(ch3) << 8) | ascii(ch4);
};

MQTT.Connection.prototype.getVBI = function() {
	var ret = 0;
	var chars = 0;
	var char = 0;

	if (!this.haveFullVBI())
		throw new Error('Insufficient bytes available!');

	do {
		char = ascii(this.rx_buf[chars++]);
		ret |= ((char & 0x7F) << (7 * (chars - 1)));
	} while(char > 127);
	this.rx_buf = this.rx_buf.substr(chars);
	return ret;
};

MQTT.Connection.prototype.getUTF8String = function() {
	var ret = this.getBinaryData();
	MQTT.validateUTF8String(ret);
	return ret;
};

MQTT.Connection.prototype.getUTF8StringPair = function() {
	var name = this.getUTF8String();
	var value = this.getUTF8String();
	return {name: name, value: value};
}

MQTT.Connection.prototype.getBinaryData = function() {
	var len = this.getTwoByte();
	if (this.rx_buf.length < len)
		throw new Error('Insufficient bytes available!');
	var ret = '';
	var got = 0;
	var cbytes = 0;
	var cval = 0;
	var b;
	while (got < len) {
		b = this.getByte();
		ret += ascii(b);
		got++;
	}
	return ret;
};

MQTT.Connection.prototype.haveFullVBI = function() {
	var i;
	var v;

	for (i = 0; i < this.rx_buf.length; i++) {
		v = ascii(this.rx_buf[i]);
		if (i > 0 && v == 0)
			throw new Error('0x81 Overlong encoding of Variable Byte Integer');
		if (ascii(this.rx_buf[i]) < 128)
			return true;
		if (i == 3)
			throw new Error('0x81 Oversized Variable Byte Integer');
	}
	return false;
};

MQTT.Connection.prototype.getProperties = function(is_will) {
	var ret = {};
	var context;
	if (is_will)
		context = 'Will Properties';
	else
		context = this.rx_packet.type;
	var plen = this.getVBI();
	while (plen) {
		var oldlen = this.rx_buf.length;
		var ptype = this.getVBI();
		var newval = this[MQTT.properties[ptype].getter]();
		MQTT.Packet.addProperty(ret, this.rx_packet.type, ptype, newval);
		plen -= (oldlen - this.rx_buf.length);
	}
	if (ret[22] !== undefined && ret[21] === undefined)
		throw new Error('0x82 Authentication Data without Authentication Method');
	return ret;
};

MQTT.Connection.prototype.parseBytes = function() {
	while (this.rx_need && this.rx_buf.length >= this.rx_need) {
		try {
			this.rx_callback(this);
		}
		catch (e) {
			log(LOG_WARNING, e.toSource());
			this.error(e);
			return;
		}
	}
	// If we need bytes, and we're not waiting for them, add handler
	if (this.rx_need && this.rx_once === null) {
		this.rx_once = this.sock.once('read', MQTT.Packet.newBytes);
	}
	// If we're waiting for bytes, schedule the next packet
	else if(this.rx_buf.length > 0) {
		js.setImmediate(MQTT.Connection.nextPacket, this);
	}
	// Otherwise, wait for new bytes from socket and see what happens
	else if(this.rx_once === null) {
		this.rx_once = this.sock.once('read', MQTT.Connection.rxPacket);
	}
};

// MQTT.Connection.SubscriptionOptions class

MQTT.Connection.SubscriptionOptions = function(value) {
	if ((value & 3) == 3)
		throw new Error('0x82 Subscription Options with QoS set to 3');
	if ((value & 0x30) == 0x30)
		throw new Error('0x82 Subscription Options with Retain Handling set to 3');
	if (value & 0xc0)
		throw new Error('0x81 Subscription Options with Reserved Bit set');
	this._value = value;
}

Object.defineProperties(MQTT.Connection.SubscriptionOptions.prototype, {
	QoS: {
		get: function() {
			return this._value & 3;
		},
		set: function(val) {
			if (val < 0 || val > 2)
				throw new Error('0x82 Invalid Subscription Options QoS: ' + val);
			this._value &= ~3;
			this._value |= val;
		},
	},
	no_local: {
		get: function() {
			return (this._value & 4) ? true : false;
		},
		set: function(val) {
			if (val)
				this._value |= 4;
			else
				this._value &= ~4;
		},
	},
	retain_as_published: {
		get: function() {
			return (this._value & 8) ? true : false;
		},
		set: function(val) {
			if (val)
				this._value |= 8;
			else
				this._value &= ~8;
		},
	},
	retain_handling: {
		get: function() {
			return (this._value & 0x30) >> 4;
		},
		set: function(val) {
			if (val < 0 || val > 2)
				throw new Error('0x82 Invalid Subscription Options Retain Handling: ' + val);
			this._value &= ~0x30;
			this._value |= ((val & 3) << 4);
		},
	},
});

// MQTT.Connection.Subscription class

MQTT.Connection.Subscription = function(conn, topic_filter, options, subscription_id) {
	// TODO: Check QoS is allowed...
	if (!MQTT.Connection.prototype.isPrototypeOf(conn))
		throw new Error("Connection is not an MQTT.Connection object");
	if (topic_filter.substr(0, 7) === '$share/')
		throw new Error("0x9E Shared Subscriptions not supported");
	if (!MQTT.Connection.SubscriptionOptions.prototype.isPrototypeOf(options))
		throw new Error("0x83 Options is not an instance of MQTT.Connection.SubscriptionOptions");
	if (subscription_id === undefined)
		subscription_id = null;
	this.topic_filter = topic_filter;
	this.options = options;
	this.subscription_id = subscription_id;
	var restr = '^';
	var i;
	var last_was_slash = true;
	var at_last = false;
	for (i = 0; i < topic_filter.length; i++) {
		if (i === topic_filter.length - 1)
			at_last = true;
		if (topic_filter[i] === '+') {
			if (!last_was_slash)
				throw new Error("0x8F Invalid Filter");
			if (!at_last && topic_filter[i+1] !== '/')
				throw new Error("0x8F Invalid Filter");
			restr += '[^\/]*';
		}
		else if (topic_filter[i] === '#') {
			if (!last_was_slash)
				throw new Error("0x8F Invalid Filter");
			if (!at_last)
				throw new Error("0x8F Invalid Filter");
			restr += '.*';
		}
		else if ("$()*+./?[\\]^{|}".indexOf(topic_filter[i]) >= 0)
			restr += '\\' + topic_filter[i];
		else
			restr += topic_filter[i];
	}
	restr += '$';
	this.re = new RegExp(restr);
	this.conn = conn;

	var oldsub = conn.subscriptions[topic_filter];
	var send_retained = (options.retain_handling !== 2);
	if (oldsub !== undefined) {
		if (options.retain_handling > 0)
			send_retained = false;
	}

	conn.subscriptions[topic_filter] = this;

	// Add to each topic and send retained
	for (i in conn.broker.topics) {
		if (conn.broker.topics[i].name.search(this.re) === 0) {
			if (conn.broker.topics[i][this.client_id] === undefined)
				conn.broker.topics[i][this.client_id] = {};
			conn.broker.topics[i][this.client_id][topic_filter] = this;
			if (conn.broker.topics[i].retained !== null) {
				if (conn.broker.topics[i].retained.properties !== undefined &&
				    conn.broker.topics[i].retained.properties[2] !== undefined &&
				    time() - conn.broker.topics[i].retained.properties[2] > conn.broker.topics[i].dupe_timestamp) {
					delete conn.broker.topics[i].retained;
				}
				else {
					if (send_retained) {
						var rpkt = conn.broker.topics[i].retained.dupeForSubscriptions(conn, [this.subscription_id], options.QoS);
						conn.tx_queued.push(rpkt);
						if (!conn.tx_service_pending) {
							js.setImmediate(conn.serviceTxQueue, conn);
							conn.tx_service_pending = true;
						}
					}
				}
			}
		}
	}
};

MQTT.Connection.Subscription.prototype.remove = function() {
	var i;
	var j;

	// Remove from topics...
	for (i in this.conn.broker.topics) {
		if (this.conn.broker.topics[i][this.client_id] !== undefined) {
			if (this.conn.broker.topics[i][this.client_id][this.topic_filter] !== undefined)
				delete this.conn.broker.topics[i][this.client_id][this.topic_filter];
		}
	}

	// Remove from Connection
	if (this.conn.subscriptions[this.topic_filter] !== undefined)
		delete this.conn.subscriptions[this.topic_filter];
};

// MQTT.Topic class

MQTT.Topic = function(broker, name) {
	if (!MQTT.prototype.isPrototypeOf(broker))
		throw new Error('Broker is not an MQTT object');
	if (typeof name != 'string')
		throw new Error("name must be a string");
	this.name = name;
	this.subscribers = {};
	this.retained = null;
	var i;
	var j;
	var created;

	function add_subscribers(thisobj, conns) {
		for (i in conns) {
			created = false;
			for (j in conns[i].subscriptions) {
				if (name.search(conns[i].subscriptions[j].re) >= 0) {
					if (!created) {
						thisobj.subscribers[conns[i].client_id] = {};
						created = true;
					}
					thisobj.subscribers[conns[i].client_id][conns[i].subscriptions[j].topic_filter] = conns[i].subscriptions[j];
				}
			}
		}
	}

	add_subscribers(this, broker.disconnected);
	add_subscribers(this, broker.connected);
};

// MQTT.Packet class

MQTT.Packet = function() {
	this._type = 0;
	this._flags = 0;
	this._pid = 0;
	this._reason_code = null;
	this.pkt_length = 0;
	this.properties = null;
	this.payload = null;
};

Object.defineProperties(MQTT.Packet.prototype, {
	type_and_flags: {
		get: function() {
			return this.type << 4 | this.flags;
		},
		set: function(val) {
			this.type = (val & 0xF0) >> 4;
			this.flags = val & 0x0F;
		}
	},
	type: {
		get: function() {
			if (this._type < 1 || this._type > 15)
				throw new Error('Invalid type: '+this._type);
			return this._type;
		},
		set: function(val) {
			val = parseInt(val, 10);
			if (val < 1 || val > 15)
				throw new Error('Invalid type: '+val);
			this._type = val;
			if (this.__proto__ === MQTT.Packet.prototype) {
				MQTT.Packet[MQTT.typename[val]].call(this);
				// Object.setPrototypeOf()
				this.__proto__ = MQTT.Packet[MQTT.typename[val]].prototype;
			}
		}
	},
	dup: {
		get: function() {
			if (this._type != MQTT.type.PUBLISH)
				return undefined;
			if (this._flags & 0x08)
				return true;
			return false;
		},
		set: function(val) {
			if (this._type !== MQTT.type.PUBLISH)
				throw new Error('Attempt to set QoS in incorrect packet type');
			if (val)
				this._flags |= 0x08;
			else
				this._flags &= ~0x08;
		}
	},
	QoS: {
		get: function() {
			if (this._type !== MQTT.type.PUBLISH)
				return undefined;
			var ret = (this._flags & 0x06) >> 1;
			if (MQTT.QoSname[ret] === undefined)
				throw new Error('Invalid QoS value');
			return ret;
		},
		set: function(val) {
			if (this._type !== MQTT.type.PUBLISH)
				throw new Error('Attempt to set QoS in incorrect packet type');
			if (MQTT.QoSname[val] === undefined)
				throw new Error('Invalid QoS value');
			this._flags = (this._flags & ~0x06) | (val << 1);
		}
	},
	retain: {
		get: function() {
			if (this._type != MQTT.type.PUBLISH)
				return undefined;
			if (this._flags & 0x01)
				return true;
			return false;
		},
		set: function(val) {
			if (this._type !== MQTT.type.PUBLISH)
				throw new Error('Attempt to set QoS in incorrect packet type');
			if (val)
				this._flags |= 0x01;
			else
				this._flags &= ~0x01;
		}
	},
	flags: {
		get: function() {
			switch(this._type) {
				case MQTT.type.CONNECT:
				case MQTT.type.CONNACK:
				case MQTT.type.PUBACK:
				case MQTT.type.PUBREC:
				case MQTT.type.PUBCOMP:
				case MQTT.type.SUBACK:
				case MQTT.type.UNSUBACK:
				case MQTT.type.PINGREQ:
				case MQTT.type.PINGRESP:
				case MQTT.type.DISCONNECT:
				case MQTT.type.AUTH:
					return 0;
				case MQTT.type.PUBREL:
				case MQTT.type.SUBSCRIBE:
				case MQTT.type.UNSUBSCRIBE:
					return 2;
				case MQTT.type.PUBLISH:
					return this._flags;
			}
		},
		set: function(val) {
			switch(this._type) {
				case MQTT.type.CONNECT:
				case MQTT.type.CONNACK:
				case MQTT.type.PUBACK:
				case MQTT.type.PUBREC:
				case MQTT.type.PUBCOMP:
				case MQTT.type.SUBACK:
				case MQTT.type.UNSUBACK:
				case MQTT.type.PINGREQ:
				case MQTT.type.PINGRESP:
				case MQTT.type.DISCONNECT:
				case MQTT.type.AUTH:
					if (val != 0)
						throw new Error('0x81 Setting flags for type ' + MQTT.typename[val] + ' to ' + val + ' not zero');
					return;
				case MQTT.type.PUBREL:
				case MQTT.type.SUBSCRIBE:
				case MQTT.type.UNSUBSCRIBE:
					if (val != 2)
						throw new Error('0x81 Setting flags for type ' + MQTT.typename[val] + ' to ' + val + ' not two');
					return;
				case MQTT.type.PUBLISH:
					if (val < 0 || val > 15)
						throw new Error('0x81 Setting flags for type ' + MQTT.typename[val] + ' to ' + val);
					this._flags = val;
			}
		}
	},
	reason_code: {
		get: function() {
			if (this._reason_code === null)
				return undefined;
			return this._reason_code;
		},
		set: function(val) {
			if (MQTT.reason_code[val] === undefined)
				throw new Error('Unknown reason code '+val);
			if (MQTT.reason_code[val].valid.indexOf(this.type) == -1)
				throw new Error('Reason code '+val+' not valid for '+MQTT.typename[this.type]);
			this._reason_code = val;
		},
	},
	packet_identifier: {
		get: function() {
			if (this._pid === null)
				return undefined;
			return this._pid;
		},
		set: function(val) {
			switch (this.type) {
				case MQTT.type.PUBLISH:
					// Special case...
					if (this.QoS == 0 && val !== null)
						throw new Error('0x82 Packet Identifier set on PUBLISH where QoS is 0')
					// Fall-through
				case MQTT.type.PUBACK:
				case MQTT.type.PUBREC:
				case MQTT.type.PUBREL:
				case MQTT.type.PUBCOMP:
				case MQTT.type.SUBSCRIBE:
				case MQTT.type.SUBACK:
				case MQTT.type.UNSUBSCRIBE:
				case MQTT.type.UNSUBACK:
					this._pid = val;
					return;
				throw new Error('Packet Identifier set on ' + MQTT.typename[this.type]);
			}
		}
	}
});

MQTT.Packet.gotTypeFlags = function(conn) {
	conn.rx_packet.type_and_flags = conn.getByte();
	conn.rx_need = 1;
	conn.rx_callback = MQTT.Packet.gotRemainingLengthByte;
};

MQTT.Packet.parseRemaining = function(conn) {
	conn.rx_callback = null;
	conn.rx_need = 0;
	log(LOG_INFO, "Got " + MQTT.typename[conn.rx_packet.type] + " packet from "+conn.client_id);
	js.setImmediate(conn.handlePacket, conn);
};

MQTT.Packet.newBytes = function() {
	try {
		var data = this.recv(16384);
		this.connection.rx_once = null;
		if (data == null) {
			// Error, tear down connection...
			this.connection.tearDown();
			return;
		}
		this.connection.rx_buf += data;
		this.connection.parseBytes();
	}
	catch (e) {
		this.connection.error(e);
	}
};

MQTT.Packet.gotRemainingLengthByte = function(conn) {
	if (this.haveFullVBI()) {
		this.rx_packet.pkt_length = conn.getVBI();
		if (this.rx_packet.pkt_length == 0)
			MQTT.Packet.parseRemaining(conn);
		else {
			conn.rx_need = this.rx_packet.pkt_length;
			conn.rx_callback = MQTT.Packet.parseRemaining;
		}
	}
	else {
		conn.rx_need++;
	}
};

MQTT.Packet.addProperty = function(props, context, type, val) {
	if (MQTT.properties[type] === undefined)
		throw new Error('Unknown property ID: ' + type);
	if (MQTT.properties[type].valid.indexOf(context) == -1)
		throw new Error('Invalid property: ' + type + ' for ' + context);
	if (MQTT.properties[type].checkValid !== undefined && !MQTT.properties[type].checkValid(val))
		throw new Error('Invalid property value: ' + val + ' for ' + type + " in " + context);
	if (MQTT.properties[type].multiple) {
		if (props[type] === undefined)
			props[type] = [];
		props[type].push(val);
	}
	else {
		if (props[type] !== undefined)
			throw new Error('0x82 More than one ' + MQTT.properties[type].name);
		props[type] = val;
	}
}

MQTT.Packet.prototype.addProperty = function(context, type, val) {
	if (this.properties === null)
		this.properties = {};
	MQTT.Packet.addProperty(this.properties, context, type, val);
}

MQTT.Packet.prototype.recv = function(sock) {
	if (sock.connection === undefined)
		return;
	sock.connection.rx_need = 1;
	sock.connection.rx_callback = MQTT.Packet.gotTypeFlags;
	sock.connection.parseBytes();
};

MQTT.Packet.prototype.serialize = function() {
	var ret = this.serializeVariableHeader();
	ret += this.serializePayload();
	ret = MQTT.encodeByte(this.type_and_flags) + MQTT.encodeVBI(ret.length) + ret;
	return ret;
};

// MQTT.Packet.ConnectFlags class

MQTT.Packet.ConnectFlags = function(val) {
	this.connect_flags = val;
};

Object.defineProperties(MQTT.Packet.ConnectFlags.prototype, {
	connect_flags: {
		get: function() {
			return this_connect_flags;
		},
		set: function(val) {
			if (val & 1)
				throw new Error('0x81 Reserved flag set');
			if ((val & 0x18) == 0x18)
				throw new Error('0x81 Both QoS bits set');
			if ((val & 0x04) == 0) {
				if (val & 0x18)
					throw new Error('0x82 Will Flag zero with QoS != 0');
			}
			this._connect_flags = val;
		}
	},
	clean_start: {
		get: function() {
			return (this._connect_flags & 0x02) ? true : false;
		},
		set: function(val) {
			if (val)
				this._connect_flags |= 0x02;
			else
				this._connect_flags &= ~0x02;
		},
	},
	will_flag: {
		get: function() {
			return (this._connect_flags & 0x04) ? true : false;
		},
		set: function(val) {
			if (val)
				this._connect_flags |= 0x04;
			else // Clear QoS with Will Flag
				this._connect_flags &= ~0x1c;
		},
	},
	will_qos: {
		get: function() {
			return (this._connect_flags & 0x18) >> 7;
		},
		set: function(val) {
			if (MQTT.QoSname[val] === undefined)
				throw new Error('0x82 Both QoS bits set');
			this._connect_flags &= ~0x18;
			this._connect_flags |= (val << 7);
		},
	},
	will_retain: {
		get: function() {
			return (this._connect_flags & 0x20) ? true : false;
		},
		set: function(val) {
			if (val)
				this._connect_flags |= 0x20;
			else
				this._connect_flags &= ~0x20;
		},
	},
	password_flag: {
		get: function() {
			return (this._connect_flags & 0x40) ? true : false;
		},
		set: function(val) {
			if (val)
				this._connect_flags |= 0x40;
			else
				this._connect_flags &= ~0x40;
		},
	},
	user_name_flag: {
		get: function() {
			return (this._connect_flags & 0x80) ? true : false;
		},
		set: function(val) {
			if (val)
				this._connect_flags |= 0x80;
			else
				this._connect_flags &= ~0x80;
		},
	},
});

// MQTT.Packet.CONNECT class

MQTT.Packet.CONNECT = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.CONNECT;
	}
	this.connect_flags = new MQTT.Packet.ConnectFlags(0);
	this.protocol_name = null;
	this.protocol_version = 0;
	this.keep_alive = 0;
	this.properties = null;
	this.client_id = null;
	this.will = null;
	this.user_name = null;
	this.password = null;
};

MQTT.Packet.CONNECT.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.CONNECT.prototype.constructor = MQTT.Packet;

MQTT.Packet.CONNECT.prototype.recv = function(conn) {
	var start_len = conn.rx_buf.length;
	this.protocol_name = conn.getUTF8String();
	this.protocol_version = conn.getByte();
	this.connect_flags.connect_flags = conn.getByte();
	this.keep_alive = conn.getTwoByte();
	this.properties = conn.getProperties(false);
	this.client_id = conn.getUTF8String();

	if (this.connect_flags.will_flag) {
		this.will = {};
		this.will.properties = conn.getProperties(true);
		this.will.topic = conn.getUTF8String();
		if (this.will.properties[1] !== undefined && this.will.properties[1] == 1)
			this.will.payload = conn.getUTF8String();
		else
			this.will.payload = conn.getBinaryData();
	}
	if (this.connect_flags.user_name_flag)
		this.user_name = conn.getUTF8String();
	if (this.connect_flags.password_flag)
		this.password = conn.getBinaryData();
	if ((start_len - conn.rx_buf.length) != this.pkt_length)
		throw new Error('Length Incorrect ' + (start_len - conn.rx_buf.length) + ' != ' + this.pkt_length);
};

// MQTT.Packet.CONNACK class

MQTT.Packet.CONNACK = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.CONNACK;
	}
	this._connack_flags = 0;
	this.properties = {};
	this.reason_code = 0;
};

MQTT.Packet.CONNACK.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.CONNACK.prototype.constructor = MQTT.Packet;

Object.defineProperties(MQTT.Packet.CONNACK.prototype, {
	connack_flags: {
		get: function() {
			return this._connack_flags;
		},
		set: function(val) {
			if (val & ~1)
				throw new Error('0x82 Reserved flag set');
			this._connack_flags = val;
		}
	},
	session_present: {
		get: function() {
			return (this._connack_flags & 0x01) ? true : false;
		},
		set: function(val) {
			if (val)
				this._connack_flags |= 0x01;
			else
				this._connack_flags &= ~0x01;
		},
	},
});

MQTT.Packet.CONNACK.prototype.serializeVariableHeader = function() {
	var ret = MQTT.encodeByte(this.connack_flags);
	ret += MQTT.encodeByte(this.reason_code);
	ret += MQTT.encodeProperties(this.properties);
	return ret;
};

MQTT.Packet.CONNACK.prototype.serializePayload = function() {
	return '';
};

// MQTT.Packet.SUBSCRIBE class

MQTT.Packet.SUBSCRIBE = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.SUBSCRIBE;
	}
	this.properties = {};
};

MQTT.Packet.SUBSCRIBE.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.SUBSCRIBE.prototype.constructor = MQTT.Packet;

MQTT.Packet.SUBSCRIBE.prototype.recv = function(conn) {
	var start_len = conn.rx_buf.length;
	this.packet_identifier = conn.getTwoByte();
	this.properties = conn.getProperties();
	if (this.properties[11] !== undefined && this.properties[11].length > 1)
		throw new Error('0x82 Multiple Subscription Identifiers in SUBSCRIBE');
	this.payload = [];

	do {
		var tfilter = conn.getUTF8String();
		var sopts = new MQTT.Connection.SubscriptionOptions(conn.getByte());
		this.payload.push({topic_filter: tfilter, subscription_options: sopts});
	} while ((start_len - conn.rx_buf.length) < this.pkt_length);
};

// MQTT.Packet.SUBACK class

MQTT.Packet.SUBACK = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.SUBACK;
	}
	this.properties = {};
	this.payload = [];
};

MQTT.Packet.SUBACK.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.SUBACK.prototype.constructor = MQTT.Packet;

MQTT.Packet.SUBACK.prototype.serializeVariableHeader = function() {
	var ret = MQTT.encodeTwoByte(this.packet_identifier);
	ret += MQTT.encodeProperties(this.properties);
	return ret;
};

MQTT.Packet.SUBACK.prototype.serializePayload = function() {
	var i;
	var ret = '';
	for (i in this.payload) {
		ret += MQTT.encodeByte(this.payload[i]);
	}
	return ret;
};

// MQTT.Packet.UNSUBSCRIBE class

MQTT.Packet.UNSUBSCRIBE = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.UNSUBSCRIBE;
	}
	this.properties = {};
};

MQTT.Packet.UNSUBSCRIBE.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.UNSUBSCRIBE.prototype.constructor = MQTT.Packet;

MQTT.Packet.UNSUBSCRIBE.prototype.recv = function(conn) {
	var start_len = conn.rx_buf.length;
	this.packet_identifier = conn.getTwoByte();
	this.properties = conn.getProperties();
	this.payload = [];

	do {
		this.payload.push(conn.getUTF8String());
	} while ((start_len - conn.rx_buf.length) < this.pkt_length);
};

// MQTT.Packet.UNSUBACK class

MQTT.Packet.UNSUBACK = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.UNSUBACK;
	}
	this.properties = {};
	this.payload = [];
};

MQTT.Packet.UNSUBACK.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.UNSUBACK.prototype.constructor = MQTT.Packet;

MQTT.Packet.UNSUBACK.prototype.serializeVariableHeader = function() {
	var ret = MQTT.encodeTwoByte(this.packet_identifier);
	ret += MQTT.encodeProperties(this.properties);
	return ret;
};

MQTT.Packet.UNSUBACK.prototype.serializePayload = function() {
	var i;
	var ret = '';
	for (i in this.payload) {
		ret += MQTT.encodeByte(this.payload[i]);
	}
	return ret;
};

// MQTT.Packet.PINGREQ class

MQTT.Packet.PINGREQ = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.PINGREQ;
	}
};

MQTT.Packet.PINGREQ.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.PINGREQ.prototype.constructor = MQTT.Packet;

MQTT.Packet.PINGREQ.prototype.recv = function(conn) {
	if (this.pkt_length)
		throw new Error('0x81 PINGREQ with data');
};

// MQTT.Packet.PINGRESP class

MQTT.Packet.PINGRESP = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.PINGRESP;
	}
};

MQTT.Packet.PINGRESP.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.PINGRESP.prototype.constructor = MQTT.Packet;

MQTT.Packet.PINGRESP.prototype.serializeVariableHeader = function() {
	return '';;
};

MQTT.Packet.PINGRESP.prototype.serializePayload = function() {
	return '';;
};

// MQTT.Packet.DISCONNECT class

MQTT.Packet.DISCONNECT = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.DISCONNECT;
	}
	this.properties = {};
	this.reason_code = 0;
};

MQTT.Packet.DISCONNECT.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.DISCONNECT.prototype.constructor = MQTT.Packet;

MQTT.Packet.DISCONNECT.prototype.serializeVariableHeader = function() {
	var has_props = Object.keys(this.properties).length > 0;
	var ret = '';
	if (has_props || this.reason_code !== 0)
		ret += MQTT.encodeByte(this.reason_code);
	if (has_props)
		ret += MQTT.encodeProperties(this.properties);
	return ret;
};

MQTT.Packet.DISCONNECT.prototype.serializePayload = function() {
	return '';
};

MQTT.Packet.DISCONNECT.prototype.recv = function(conn) {
	if (this.pkt_length === 0) {
		this.reason_code = 0;
		this.properties = {};
	}
	else {
		this.reason_code = conn.getByte();
		if (this.pkt_length < 2)
			this.properties = {};
		else
			this.properties = conn.getProperties();
	}
};

// MQTT.Packet.AUTH class

MQTT.Packet.AUTH = function() {
	throw new Error('0x82 AUTH not supported');
};

MQTT.Packet.AUTH.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.AUTH.prototype.constructor = MQTT.Packet;

// MQTT.Packet.PUBLISH class

MQTT.Packet.PUBLISH = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.PUBLISH;
	}
	this.topic_name = null;
	this.payload_format = 0;
	this.properties = {};
	this.payload = null;
};

MQTT.Packet.PUBLISH.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.PUBLISH.prototype.constructor = MQTT.Packet;

MQTT.Packet.PUBLISH.prototype.serializeVariableHeader = function() {
	var ret = MQTT.encodeUTF8String(this.topic_name);
	if (this.QoS > 0)
		ret += MQTT.encodeTwoByte(this.packet_identifier);
	if (this.properties[2] !== undefined && this.dupe_timestamp !== undefined) {
		var now = time();
		this.properties[2] -= (now - this.properties[2]);
		this.dupe_timestamp = now;
	}
	ret += MQTT.encodeProperties(this.properties);
	return ret;
};

MQTT.Packet.PUBLISH.prototype.serializePayload = function() {
	var ret = '';
	if (this.payload !== null) {
		if (this.payload_format == 0)
			ret += MQTT.encodeBinaryData(this.payload);
		else
			ret += MQTT.encodeUTF8String(this.payload);
	}

	return ret;
};

MQTT.Packet.PUBLISH.prototype.recv = function(conn) {
	var start_len = conn.rx_buf.length;
	this.topic_name = conn.getUTF8String();
	// TODO: Topic aliasing...
	if (this.QoS > 0) {
		this.packet_identifier = conn.getTwoByte();
	}
	this.properties = conn.getProperties();
	if (this.properties[1] !== undefined)
		this.payload_format = this.properties[1];
	if (this.properties[35] !== undefined)
		throw new Error('0x94 Topic Alias invalid');
	var payload_len = this.pkt_length - (start_len - conn.rx_buf.length);
	if (conn.rx_buf.length < payload_len)
		throw new Error('Insufficient bytes available!');
	this.payload = conn.rx_buf.substr(0, payload_len);
	conn.rx_buf = conn.rx_buf.substr(payload_len);
};

MQTT.Packet.PUBLISH.prototype.dupeForRetain = function() {
	var ret = new MQTT.Packet.PUBLISH();
	ret.dup = 0;
	if (ret.topic_name === '')
		throw new Error('Zero-length Topic Name');
	ret.topic_name = this.topic_name;
	ret.packet_identifier = null;
	ret.properties = JSON.parse(JSON.stringify(this.properties));
	if (ret.properties[35] !== undefined)
		delete ret.properties[35];
	if (ret.properties[11] !== undefined)
		delete ret.properties[11];
	ret.dupe_timestamp = time();
	ret.payload = this.payload;

	return ret;
};

MQTT.Packet.PUBLISH.prototype.dupeForSubscriptions = function(conn, sids, qos) {
	var ret = this.dupeForRetain();
	ret.QoS = qos;
	var sid;
	if (ret.properties[24] !== undefined)
		delete ret.properties[24];
	for (sid in sids)
		ret.addProperty(this.type, 11, sids[sid]);
	if (qos > 0) {
		ret.packet_identifier = conn.getUnusedPID();
		conn.tx_unacked.push(ret);
	}

	return ret;
};

// MQTT.Packet.PUBACK class

MQTT.Packet.PUBACK = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.PUBACK;
	}
	this.reason_code = 0;
	this.properties = {};
};

MQTT.Packet.PUBACK.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.PUBACK.prototype.constructor = MQTT.Packet;

MQTT.Packet.PUBACK.prototype.serializeVariableHeader = function() {
	var has_props = Object.keys(this.properties).length > 0;
	var ret = MQTT.encodeTwoByte(this.packet_identifier);
	if (has_props || this.reason_code !== 0)
		ret += MQTT.encodeByte(this.reason_code);
	if (has_props)
		ret += MQTT.encodeProperties(this.properties);
	return ret;
};

MQTT.Packet.PUBACK.prototype.serializePayload = function() {
	return '';
};

MQTT.Packet.PUBACK.prototype.recv = function(conn) {
	this.packet_identifier = conn.getTwoByte();
	if (this.pkt_length === 2) {
		this.reason_code = 0;
		this.properties = {};
	}
	else {
		this.reason_code = conn.getByte();
		if (this.pkt_length < 4)
			this.properties = {};
		else
			this.properties = conn.getProperties();
	}
};

// MQTT.Packet.PUBREC class

MQTT.Packet.PUBREC = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.PUBREC;
	}
	this.reason_code = 0;
	this.properties = {};
};

MQTT.Packet.PUBREC.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.PUBREC.prototype.constructor = MQTT.Packet;

MQTT.Packet.PUBREC.prototype.serializeVariableHeader = MQTT.Packet.PUBACK.prototype.serializeVariableHeader;

MQTT.Packet.PUBREC.prototype.serializePayload = MQTT.Packet.PUBACK.prototype.serializePayload;

MQTT.Packet.PUBREC.prototype.recv = MQTT.Packet.PUBACK.prototype.recv;

// MQTT.Packet.PUBREL class

MQTT.Packet.PUBREL = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.PUBREL;
	}
	this.reason_code = 0;
	this.properties = {};
};

MQTT.Packet.PUBREL.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.PUBREL.prototype.constructor = MQTT.Packet;

MQTT.Packet.PUBREL.prototype.serializeVariableHeader = MQTT.Packet.PUBACK.prototype.serializeVariableHeader;

MQTT.Packet.PUBREL.prototype.serializePayload = MQTT.Packet.PUBACK.prototype.serializePayload;

MQTT.Packet.PUBREL.prototype.recv = MQTT.Packet.PUBACK.prototype.recv;

// MQTT.Packet.PUBCOMP class

MQTT.Packet.PUBCOMP = function() {
	if (this._type === undefined) {
		MQTT.Packet.call(this);
		this.type = MQTT.type.PUBCOMP;
	}
	this.reason_code = 0;
	this.properties = {};
};

MQTT.Packet.PUBCOMP.prototype = Object.create(MQTT.Packet.prototype);
MQTT.Packet.PUBCOMP.prototype.constructor = MQTT.Packet;

MQTT.Packet.PUBCOMP.prototype.serializeVariableHeader = function() MQTT.Packet.PUBACK.prototype.serializeVariableHeader;

MQTT.Packet.PUBCOMP.prototype.serializePayload = MQTT.Packet.PUBACK.prototype.serializePayload;

MQTT.Packet.PUBCOMP.prototype.recv = MQTT.Packet.PUBACK.prototype.recv;

// Set up everything...

// Find sysops...
for (var i = 1; i <= system.last_user; i++) {
	var usr = new User(i);
	if (!usr.is_sysop)
		continue;
	if (usr.settings & (USER_DELETED | USER_INACTIVE))
		continue;
	log(LOG_INFO, "Adding user: "+usr.alias.toLowerCase());
	MQTT.psk[usr.alias.toLowerCase()] = usr.security.password.toLowerCase();
}
var broker = new MQTT();
var s = new ListeningSocket(["0.0.0.0", "::0"], 8883, 'MQTT');
s.on('read', function(sock) {broker.gotConnection(this.accept())});
js.do_callbacks = true;
