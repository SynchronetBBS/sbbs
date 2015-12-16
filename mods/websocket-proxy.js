load('sha1.js');

var WebSocketProxy = function(client) {

	var WEBSOCKET_NEED_PACKET_START = 0;
	var WEBSOCKET_NEED_PAYLOAD_LENGTH = 1;
	var WEBSOCKET_NEED_MASKING_KEY = 2;
	var WEBSOCKET_DATA = 3;

	var FFrameMask = [];
	var FFrameOpCode = 0;
	var FFramePayloadLength = 0;
	var FFramePayloadReceived = 0;
	var FWebSocketState = WEBSOCKET_NEED_PACKET_START;

	var self = this;
	this.headers = [];

	var ClientDataBuffer = []; // From client
	var ServerDataBuffer = []; // From server

	function CalculateWebSocketKey(InLine) {
		var Digits = '';
		var Spaces = 0;
		for (var i = 0; i < InLine.length; i++) {
			if (InLine.charAt(i) == ' ') {
				Spaces++;
			} else if (!isNaN(InLine.charAt(i))) {
				Digits += InLine.charAt(i);
			}
		}
		return Digits / Spaces;
	}

	function GetFromWebSocketClient() {
		switch (self.headers['Version']) {
			case 0: return GetFromWebSocketClientDraft0();
			case 7: 
			case 8: 
			case 13:
				return GetFromWebSocketClientVersion7();
		}
	}

	function GetFromWebSocketClientDraft0() {
		var Result = [];
		var InByte = 0;
		var InByte2 = 0;
		var InByte3 = 0;
		
		while (client.socket.data_waiting) {
			InByte = client.socket.recvBin(1);
			
			// Check what the client packet state is
			switch (FWebSocketState) {
				case WEBSOCKET_NEED_PACKET_START:
					// Check for 0x00 to indicate the start of a data packet
					if (InByte === 0x00) {
						FWebSocketState = WEBSOCKET_DATA;
					}
					break;
				case WEBSOCKET_DATA:
					// We're in a data packet, so check for 0xFF, which
					// indicates the data packet is done
					if (InByte == 0xFF) {
						FWebSocketState = WEBSOCKET_NEED_PACKET_START;
					} else {
						// Check if the byte needs to be UTF-8 decoded
						if (InByte < 128) {
							Result.push(InByte);
						} else if ((InByte > 191) && (InByte < 224)) {
							// Handle UTF-8 decode
							InByte2 = client.socket.recvBin(1);
							Result.push(((InByte & 31) << 6) | (InByte2 & 63));
						} else {
							// Handle UTF-8 decode (should never need this, but
							// included anyway)
							InByte2 = client.socket.recvBin(1);
							InByte3 = client.socket.recvBin(1);
							Result.push(
								((InByte&15)<<12)|((InByte2&63)<<6)|(InByte3&63)
							);
						}
					}
					break;
			}
		}
		
		return Result;
	}

	function GetFromWebSocketClientVersion7() {
		var Result = [];
		var InByte = 0;
		var InByte2 = 0;
		var InByte3 = 0;

		while (client.socket.data_waiting) {
			// Check what the client packet state is
			switch (FWebSocketState) {
				case WEBSOCKET_NEED_PACKET_START:
					// Next byte will give us the opcode, and also tell is if
					// the message is fragmented
					FFrameMask = [];
					FFrameOpCode = client.socket.recvBin(1);
					FFramePayloadLength = 0;
					FFramePayloadReceived = 0;
					FWebSocketState = WEBSOCKET_NEED_PAYLOAD_LENGTH;
					break;
				case WEBSOCKET_NEED_PAYLOAD_LENGTH:
					FFramePayloadLength = (client.socket.recvBin(1) & 0x7F);
					if (FFramePayloadLength === 126) {
						FFramePayloadLength = client.socket.recvBin(2);
					} else if (FFramePayloadLength === 127) {
						FFramePayloadLength = client.socket.recvBin(8);
					}
					FWebSocketState = WEBSOCKET_NEED_MASKING_KEY;
					break;
				case WEBSOCKET_NEED_MASKING_KEY:
					InByte = client.socket.recvBin(4);
					FFrameMask[0] = (InByte & 0xFF000000) >> 24;
					FFrameMask[1] = (InByte & 0x00FF0000) >> 16;
					FFrameMask[2] = (InByte & 0x0000FF00) >> 8;
					FFrameMask[3] = InByte & 0x000000FF;
					FWebSocketState = WEBSOCKET_DATA;
					break;
				case WEBSOCKET_DATA:
					InByte = (
						client.socket.recvBin(1)^FFrameMask[
							FFramePayloadReceived++ % 4
						]
					);

					// Check if the byte needs to be UTF-8 decoded
					if ((InByte & 0x80) === 0) {
						Result.push(InByte);
					} else if ((InByte & 0xE0) === 0xC0) {
						// Handle UTF-8 decode
						InByte2 = (
							client.socket.recvBin(1)^FFrameMask[
								FFramePayloadReceived++ % 4
							]
						);
						Result.push(((InByte & 31) << 6) | (InByte2 & 63));
					} else {
						log(LOG_NOTICE, 'Byte out of range: ' + InByte);
					}

					// Check if we've received the full payload
					if (FFramePayloadReceived === FFramePayloadLength) {
						FWebSocketState = WEBSOCKET_NEED_PACKET_START;
					}
					break;
			}
		}

		return Result;
	}

	function SendToWebSocketClient(AData) {
		switch (self.headers['Version']) {
			case 0: 
				SendToWebSocketClientDraft0(AData); 
				break;
			case 7: 
			case 8: 
			case 13:
				SendToWebSocketClientVersion7(AData); 
				break;
		}
	}

	function SendToWebSocketClientDraft0(AData) {
		// Send 0x00 to indicate the start of a data packet
		client.socket.sendBin(0x00, 1);

		for (var i = 0; i < AData.length; i++) {
			// Check if the byte needs to be UTF-8 encoded
			if ((AData[i] & 0xFF) <= 127) {
				client.socket.sendBin(AData[i], 1);
			} else if ((AData[i] & 0xFF) <= 2047) {
				// Handle UTF-8 encode
				client.socket.sendBin((AData[i] >> 6) | 192, 1);
				client.socket.sendBin((AData[i] & 63) | 128, 1);
			} else {
				log(LOG_NOTICE, 'Byte out of range: ' + AData[i]);
			}
		}

		// Send 0xFF to indicate the end of a data packet
		client.socket.sendBin(0xFF, 1);
	}

	function SendToWebSocketClientVersion7(AData) {
		if (AData.length > 0) {
			var ToSend = [];

			for (var i = 0; i < AData.length; i++) {
				// Check if the byte needs to be UTF-8 encoded
				if ((AData[i] & 0xFF) <= 127) {
					ToSend.push(AData[i]);
				} else if (
					((AData[i] & 0xFF) >= 128) && ((AData[i] & 0xFF) <= 2047)
				) {
					// Handle UTF-8 encode
					ToSend.push((AData[i] >> 6) | 192);
					ToSend.push((AData[i] & 63) | 128);
				} else {
					log(LOG_NOTICE, 'Byte out of range: ' + AData[i]);
				}
			}

			client.socket.sendBin(0x81, 1);
			if (ToSend.length <= 125) {
				client.socket.sendBin(ToSend.length, 1);
			} else if (ToSend.length <= 65535) {
				client.socket.sendBin(126, 1);
				client.socket.sendBin(ToSend.length, 2);
			} else {
				// NOTE: client.socket.sendBin(ToSend.length, 8); didn't work,
				// so this modification limits the send to 2^32 bytes (probably
				// not an issue). Probably should look into a proper fix at
				// some point though
				client.socket.sendBin(127, 1);
				client.socket.sendBin(0, 4);
				client.socket.sendBin(ToSend.length, 4);
			}
			
			for (var i = 0; i < ToSend.length; i++) {
				client.socket.sendBin(ToSend[i] & 0xFF, 1);
			}
		}
	}

	function ShakeHands() {
		self.headers['Version'] = 0;

		try {
			// Keep reading header data until we get all the data we want
			while (true) {
				// Read another line, abort if we don't get one in 5 seconds
				var InLine = client.socket.recvline(1024, 5);
				if (InLine === null) {
					log(LOG_ERR,
						'Timeout exceeded while waiting for complete handshake'
					);
					return false;
				}

				log(LOG_DEBUG, 'Handshake Line: ' + InLine);
				
				// Check for blank line (indicates we have most of the header,
				// and only the last 8 bytes remain
				if (InLine === '') {
					switch (self.headers['Version']) {
						case 0: 
							return ShakeHandsDraft0();
						case 7: 
						case 8: 
						case 13:
							return ShakeHandsVersion7();
						default: 
							// TODO If this version does not match a version
							// understood by the server, the server MUST abort
							// the websocket handshake described in this section
							// and instead send an appropriate HTTP error code
							// (such as 426 Upgrade Required), and a
							// |Sec-WebSocket-Version| header indicating the
							// version(s) the server is capable of
							// understanding.
							break;
					}
					break;
				} else if (InLine.indexOf('Connection:') === 0) {
					// Example: "Connection: Upgrade"
					self.headers['Connection'] = InLine.replace(
						/Connection:\s?/i,
						''
					);
				} else if (InLine.indexOf('GET') === 0) {
					// Example: "GET /demo HTTP/1.1"
					var GET = InLine.split(' ');
					self.headers['Path'] = GET[1];
				} else if (InLine.indexOf('Host:') === 0) {
					// Example: "Host: example.com"
					self.headers['Host'] = InLine.replace(/Host:\s?/i, '');
				} else if (InLine.indexOf('Origin:') === 0) {
					// Example: "Origin: http://example.com"
					self.headers['Origin'] = InLine.replace(/Origin:\s?/i, '');
				} else if (InLine.indexOf('Sec-WebSocket-Key:') === 0) {
					// Example: "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ=="
					self.headers['Key'] = InLine.replace(
						/Sec-WebSocket-Key:\s?/i,
						''
					);
				} else if (InLine.indexOf('Sec-WebSocket-Key1:') === 0) {
					// Example: "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5"
					self.headers['Key1'] = CalculateWebSocketKey(
						InLine.replace(/Sec-WebSocket-Key1:\s?/i, '')
					);
				} else if (InLine.indexOf('Sec-WebSocket-Key2:') === 0) {
					// Example: "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00"
					self.headers['Key2'] = CalculateWebSocketKey(
						InLine.replace(/Sec-WebSocket-Key2:\s?/i, '')
					);
				} else if (InLine.indexOf('Sec-WebSocket-Origin:') === 0) {
					// Example: "Sec-WebSocket-Origin: http://example.com"
					self.headers['Origin'] = InLine.replace(
						/Sec-WebSocket-Origin:\s?/i,
						''
					);
				} else if (InLine.indexOf('Sec-WebSocket-Protocol:') === 0) {
					// Example: "Sec-WebSocket-Protocol: sample"
					self.headers['SubProtocol'] = InLine.replace(
						/Sec-WebSocket-Protocol:\s?/i,
						''
					);
				} else if (InLine.indexOf('Sec-WebSocket-Draft') === 0) {
					// Example: "Sec-WebSocket-Draft: 2"
					try {
						self.headers['Version'] = parseInt(
							InLine.replace(/Sec-WebSocket-Draft:\s?/i, '')
						);
					} catch (err) {
						self.headers['Version'] = 0;
					}
				} else if (InLine.indexOf('Sec-WebSocket-Version') === 0) {
					// Example: "Sec-WebSocket-Version: 8"
					try {
						self.headers['Version'] = parseInt(
							InLine.replace(/Sec-WebSocket-Version:\s?/i, '')
						);
					} catch (err) {
						self.headers['Version'] = 0;
					}
				} else if (InLine.indexOf('Upgrade:') === 0) {
					// Example: "Upgrade: websocket"
					self.headers['Upgrade'] = InLine.replace(/Upgrade:\s?/i,'');
				} else if (InLine.indexOf('Cookie:') === 0) {
				 	self.headers['Cookie'] = InLine.replace(/Cookie:\s?/i, '');
				}
			}
		} catch (err) {
			log(LOG_ERR, 'ShakeHands() error: ' + err.toString());
		}
		
		return false;
	}

	function ShakeHandsDraft0() {
		// Ensure we have all the data we need
		if (('Key1' in self.headers) &&
			('Key2' in self.headers) &&
			('Host' in self.headers) &&
			('Origin' in self.headers !== '') &&
			('Path' in self.headers)
		) {
			// Combine Key1, Key2, and the last 8 bytes into a string that we
			// will later hash
			var ToHash = ''
			ToHash += String.fromCharCode(
				(self.headers['Key1'] & 0xFF000000) >> 24
			);
			ToHash += String.fromCharCode(
				(self.headers['Key1'] & 0x00FF0000) >> 16
			);
			ToHash += String.fromCharCode(
				(self.headers['Key1'] & 0x0000FF00) >> 8
			);
			ToHash += String.fromCharCode(
				(self.headers['Key1'] & 0x000000FF) >> 0
			);
			ToHash += String.fromCharCode(
				(self.headers['Key2'] & 0xFF000000) >> 24
			);
			ToHash += String.fromCharCode(
				(self.headers['Key2'] & 0x00FF0000) >> 16
			);
			ToHash += String.fromCharCode(
				(self.headers['Key2'] & 0x0000FF00) >> 8
			);
			ToHash += String.fromCharCode(
				(self.headers['Key2'] & 0x000000FF) >> 0
			);
			for (var i = 0; i < 8; i++) {
				ToHash += String.fromCharCode(client.socket.recvBin(1));
			}
			
			// Hash the string
			var Hashed = md5_calc(ToHash, true);

			// Setup the handshake response
			var Response = 'HTTP/1.1 101 Web Socket Protocol Handshake\r\n' +
						   'Upgrade: WebSocket\r\n' +
						   'Connection: Upgrade\r\n' +
						   'Sec-WebSocket-Origin: ' + self.headers['Origin'] +
						   '\r\n' +
						   'Sec-WebSocket-Location: ws://' +
						   self.headers['Host'] +
						   self.headers['Path'] +
						   '\r\n';
			if ('SubProtocol' in self.headers) {
				Response +=
					'Sec-WebSocket-Protocol: ' +
					self.headers['SubProtocol'] +
					'\r\n';
			}
			Response += '\r\n';
			
			// Loop through the hash string (which is hex encoded) and append
			// the individual bytes to the response
			for (var i = 0; i < Hashed.length; i += 2) {
				Response += String.fromCharCode(
					parseInt(Hashed.charAt(i) + Hashed.charAt(i + 1), 16)
				);
			}
			
			// Send the response and return
			client.socket.send(Response);
			return true;
		} else {
			// We're missing some pice of data, log what we do have
			log(LOG_ERR,
				'Missing some piece of handshake data.  Here\'s what we have:'
			);
			for(var x in self.headers) { 
				log(LOG_ERR, x + ' => ' + self.headers[x]); 
			}
			return false;
		}
	}

	function ShakeHandsVersion7() {
		// Ensure we have all the data we need
		if (('Key' in self.headers) &&
			('Host' in self.headers) &&
			('Origin' in self.headers !== '') &&
			('Path' in self.headers)
		) {
			var AcceptGUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

			// Combine Key and GUID
			var ToHash = self.headers['Key'] + AcceptGUID;
			
			// Hash the string
			var Hashed = Sha1.hash(ToHash, false);

			// Encode the hash
			var ToEncode = '';
			for (var i = 0; i <= 38; i += 2) {
				ToEncode += String.fromCharCode(
					parseInt(Hashed.substr(i, 2), 16)
				);
			}
			var Encoded = base64_encode(ToEncode);

			// Setup the handshake response
			var Response = 'HTTP/1.1 101 Switching Protocols\r\n' +
						   'Upgrade: websocket\r\n' +
						   'Connection: Upgrade\r\n' +
						   'Sec-WebSocket-Accept: ' + Encoded + '\r\n';
			if ('SubProtocol' in self.headers) {
				// Only sub-protocol we support
				Response += 'Sec-WebSocket-Protocol: plain\r\n';
			}
			Response += '\r\n';
			
			// Send the response and return
			client.socket.send(Response);
			return true;
		} else {
			log(LOG_ERR,
				'Missing some piece of handshake data.  Here\'s what we have:'
			);
			for(var x in self.headers) { 
				log(LOG_ERR, x + ' => ' + self.headers[x]); 
			}
			return false;
		}
	}

	this.__defineGetter__(
		'data_waiting',
		function() {
			return (ClientDataBuffer.length > 0);
		}
	);

	this.send = function(data) {
		if (typeof data === 'string') {
			data = data.split('').map(
				function (d) {
					return ascii(d);
				}
			);
		}
		ServerDataBuffer = ServerDataBuffer.concat(data);
	}

	this.receive = function() {
		var data = '';
		while (ClientDataBuffer.length > 0) {
			data += ascii(ClientDataBuffer.shift());
		}
		return data;
	}

	this.receiveArray = function(len) {
		return ClientDataBuffer.splice(
			0,
			(typeof len === 'number' ? len : ClientDataBuffer.length)
		);
	}

	this.cycle = function() {
		ClientDataBuffer = ClientDataBuffer.concat(GetFromWebSocketClient());
		SendToWebSocketClient(ServerDataBuffer.splice(0, 4096));
	}

	if(!ShakeHands()) throw 'ShakeHands() failed';

}