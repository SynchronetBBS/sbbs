// $Id: websocketservice.js,v 1.14 2019/08/11 19:20:20 echicken Exp $

// websocketservice.js 

// Synchronet Service for redirecting incoming WebSocket connections to the Telnet and/or RLogin server
// Mainly to be used in conjunction with fTelnet

// Command-line syntax:
// websocketservice.js [hostname] [port]

// Example configuration (in ctrl/services.ini)
//
// [WS]
// Port=1123
// Options=NO_HOST_LOOKUP
// Command=websocketservice.js
//
// [WSS]
// Port=11235
// Options=NO_HOST_LOOKUP|TLS
// Command=websocketservice.js

//include definitions for synchronet
load("nodedefs.js");
load("sockdefs.js");
load("sbbsdefs.js");
load("ftelnethelper.js");
load("sha1.js");

// Global constants
const WEBSOCKET_NEED_PACKET_START   = 0;
const WEBSOCKET_NEED_PAYLOAD_LENGTH = 1;
const WEBSOCKET_NEED_MASKING_KEY	= 2;
const WEBSOCKET_DATA                = 3;

// Global variables
var FFrameMask = [];
var FFrameMasked = false;
var FFrameOpCode = 0;
var FFramePayloadLength = 0;
var FFramePayloadReceived = 0;
var FServerSocket = null;
var FWebSocketHeader = [];
var FWebSocketState = WEBSOCKET_NEED_PACKET_START;

// Main line
try {
    // Parse and respond to the WebSocket handshake request
    if (ShakeHands()) {
        SendToWebSocketClient(StringToBytes("Redirecting to server...\r\n"));

        // Default to localhost on the telnet port
		var TargetHostname = GetTelnetInterface();
		var TargetPort = GetTelnetPort();
        
        // If fTelnet client sent a port on the querystring, try to use that
        var Path = ParsePathHeader();
        if (Path.query.Port) {
            RequestedPort = parseInt(Path.query.Port);

            // Confirm the user requested either the telnet or rlogin ports (we don't want to allow them to request any arbitrary port as that would be a gaping security hole)
            if ((RequestedPort > 0) && (RequestedPort <= 65535) && ((RequestedPort == GetTelnetPort()) || (RequestedPort == GetRLoginPort()))) {
                TargetPort = RequestedPort;
                log(LOG_DEBUG, "Using user-requested port " + Path.query.Port);
                if (TargetPort == GetRLoginPort()) TargetHostname = GetRLoginInterface();
            } else {
                log(LOG_NOTICE, "Client requested to connect to port " + Path.query.Port + ", which was denied");
            }
        } else {
            // If SysOp gave an alternate hostname/port when installing the service, use that instead
			for(var i in argv) {
                var port = parseInt(argv[i], 10);
                if (argv[i].search(/\D/) > -1 || port < 0 || port > 65535) {
                    TargetHostname = argv[i];
                } else if (!isNaN(port)) {
                    TargetPort = port;
                }
            }
        }
        
		// Connect to the server
        FServerSocket = new Socket();
		log(LOG_DEBUG, "Connecting to " + TargetHostname + ":" + TargetPort);
        if (FServerSocket.connect(TargetHostname, TargetPort)) {
            // Variables we'll use in the loop
            var DoYield = true;
            var ClientData = [];
            var ServerData = [];

            // Loop while we're still connected on both ends
            while ((client.socket.is_connected) && (FServerSocket.is_connected)) {
                // Should we yield or not (default true, but disable if we had line activity)
                DoYield = true;
            
                // Check if the client sent anything
                ClientData = GetFromWebSocketClient();
                if (ClientData.length > 0) {
                    SendToServer(ClientData);
                    DoYield = false;
                }
                
                // Check if the server sent anything
                ServerData = GetFromServer();
                if (ServerData.length > 0) {
                    SendToWebSocketClient(ServerData);
                    DoYield = false;
                }    
            
                // Yield if we didn't transfer any data
                if (DoYield) {
                    mswait(10);
                    yield();
                }
            }
            if (!client.socket.is_connected) log(LOG_DEBUG, 'Client socket no longer connected');
			if (!FServerSocket.is_connected) log(LOG_DEBUG, 'Server socket no longer connected');
        } else {
            // FServerSocket.connect() failed
            log(LOG_ERR, "Error " + FServerSocket.error + " connecting to server at " + TargetHostname + ":" + TargetPort);
            SendToWebSocketClient(StringToBytes("ERROR: Unable to connect to server\r\n"));
            mswait(2500);
        }
    } else {
        log(LOG_DEBUG, "WebSocket handshake failed (likely a port scanner)");
    }
} catch (err) {
    log(LOG_ERR, err.toString());
} finally {
    if (FServerSocket != null) {
        FServerSocket.close();
    }
}

function CalculateWebSocketKey(InLine) {
    var Digits = "";
    var Spaces = 0;

    // Loop through the line, looking for digits and spaces
    for (var i = 0; i < InLine.length; i++) {
        if (InLine.charAt(i) == " ") {
            Spaces++;
        } else if (!isNaN(InLine.charAt(i))) {
            Digits += InLine.charAt(i);
        }
    }
    
    return Digits / Spaces;
}

function GetFromServer() {
    var Result = [];
    var InByte = 0;
    
    while (FServerSocket.data_waiting && (Result.length <= 4096)) {
        InByte =  FServerSocket.recvBin(1);
		Result.push(InByte);
    }
    
    return Result;
}

function GetFromWebSocketClient() {
    switch (FWebSocketHeader['Version']) {
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
                // We're in a data packet, so check for 0xFF, which indicates the data packet is done
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
                        // Handle UTF-8 decode (should never need this, but included anyway)
                        InByte2 = client.socket.recvBin(1);
                        InByte3 = client.socket.recvBin(1);
                        Result.push(((InByte & 15) << 12) | ((InByte2 & 63) << 6) | (InByte3 & 63));
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
				// Next byte will give us the opcode, and also tell is if the message is fragmented
				FFrameOpCode = client.socket.recvBin(1) & 0x0F;
                switch (FFrameOpCode) {
                    case 0: // Continuation frame 
                    case 1: // Text frame
                        break;
                        
                    case 2: // Binary frame
                        log(LOG_DEBUG, "Client sent a binary frame, which is unsupported");
                        client.socket.close();
                        return [];

                    case 8: // Close frame
                        log(LOG_DEBUG, "Client sent a close frame");
                        // TODO Protocol says to respond with a close frame
                        client.socket.close();
                        return [];
                        
                    case 9: // Ping frame
                        log(LOG_DEBUG, "Client setnt a ping frame");
                        // TODO Protocol says to respond with a pong frame
                        break;
                        
                    case 10: // Pong frame
                        log(LOG_DEBUG, "Client sent a pong frame");
                        break;
                        
                    default: // Reserved or unknown frame
                        log(LOG_DEBUG, "Client sent a reserved/unknown frame: " + FFrameOpCode);
                        client.socket.close();
                        return [];
                }

                // If we get here, we want to move on to the next state
				FFrameMask = [];
                FFrameMasked = false;
                FFramePayloadLength = 0;
                FFramePayloadReceived = 0;
                FWebSocketState = WEBSOCKET_NEED_PAYLOAD_LENGTH;
                break;
            case WEBSOCKET_NEED_PAYLOAD_LENGTH:
                InByte = client.socket.recvBin(1);
                
                FFrameMasked = ((InByte & 0x80) == 128);

                FFramePayloadLength = (InByte & 0x7F);
				if (FFramePayloadLength === 126) {
					FFramePayloadLength = client.socket.recvBin(2);
				} else if (FFramePayloadLength === 127) {
					FFramePayloadLength = client.socket.recvBin(8);
				}
                
                if (FFrameMasked) {
                    FWebSocketState = WEBSOCKET_NEED_MASKING_KEY;
                } else {
                    FWebSocketState = (FFramePayloadLength > 0 ? WEBSOCKET_DATA : WEBSOCKET_NEED_PACKET_START); // NB: Might not be any data to read, so check for payload length before setting state
                }
				break;
            case WEBSOCKET_NEED_MASKING_KEY:
				InByte = client.socket.recvBin(4);
				FFrameMask[0] = (InByte & 0xFF000000) >> 24;
				FFrameMask[1] = (InByte & 0x00FF0000) >> 16;
				FFrameMask[2] = (InByte & 0x0000FF00) >> 8;
				FFrameMask[3] = InByte & 0x000000FF;
                FWebSocketState = (FFramePayloadLength > 0 ? WEBSOCKET_DATA : WEBSOCKET_NEED_PACKET_START); // NB: Might not be any data to read, so check for payload length before setting state
				break;
            case WEBSOCKET_DATA:
				InByte = client.socket.recvBin(1);
                if (FFrameMasked) InByte ^= FFrameMask[FFramePayloadReceived++ % 4];

				// Check if the byte needs to be UTF-8 decoded
				if ((InByte & 0x80) === 0) {
					Result.push(InByte);
				} else if ((InByte & 0xE0) === 0xC0) {
					// Handle UTF-8 decode
					InByte2 = client.socket.recvBin(1);
                    if (FFrameMasked) InByte2 ^= FFrameMask[FFramePayloadReceived++ % 4];
					Result.push(((InByte & 31) << 6) | (InByte2 & 63));
				} else {
					log(LOG_DEBUG, "GetFromWebSocketClientVersion7 Byte out of range: " + InByte);
				}

				// Check if we've received the full payload
				if (FFramePayloadReceived === FFramePayloadLength) FWebSocketState = WEBSOCKET_NEED_PACKET_START;
                break;
        }
    }

    return Result;
}

function ParsePathHeader() {
    var Result = {};
    Result.query = {};
    
    var Path = FWebSocketHeader['Path'];
    if (Path) {
        if (Path.indexOf('?') === -1) {
            // No querystring
            Result.path_info = Path;
        } else {
            // Have querystring, so parse it out
            Result.path_info = Path.split('?')[0];
            var KeyValues = Path.split('?')[1].split('&');
            for (var i = 0; i < KeyValues.length; i++) {
                var KeyValue = KeyValues[i].split('=');
                Result.query[KeyValue[0]] = KeyValue[1];
            }
        }
    }
    
    return Result;
}

function SendToServer(AData) {
    for (var i = 0; i < AData.length; i++) {
        FServerSocket.sendBin(AData[i], 1);
    }
}

function SendToWebSocketClient(AData) {
    switch (FWebSocketHeader['Version']) {
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
            log(LOG_DEBUG, "SendToWebSocketClientDraft0 Byte out of range: " + AData[i]);
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
	        } else if (((AData[i] & 0xFF) >= 128) && ((AData[i] & 0xFF) <= 2047)) {
	            // Handle UTF-8 encode
	            ToSend.push((AData[i] >> 6) | 192);
	            ToSend.push((AData[i] & 63) | 128);
	        } else {
				log(LOG_DEBUG, "SendToWebSocketClientVersion7 Byte out of range: " + AData[i]);
			}
	    }

		client.socket.sendBin(0x81, 1);
		if (ToSend.length <= 125) {
			client.socket.sendBin(ToSend.length, 1);
		} else if (ToSend.length <= 65535) {
			client.socket.sendBin(126, 1);
			client.socket.sendBin(ToSend.length, 2);
		} else {
            // NOTE: client.socket.sendBin(ToSend.length, 8); didn't work, so this
            //       modification limits the send to 2^32 bytes (probably not an issue)
            //       Probably should look into a proper fix at some point though
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
	FWebSocketHeader['Version'] = 0;

    try {
        // Keep reading header data until we get all the data we want
        while (true) {
            // Read another line, and abort if we don't get one within 5 seconds
            var InLine = client.socket.recvline(512, 5);
            if (InLine === null) {
                log(LOG_DEBUG, "Timeout exceeded while waiting for complete handshake");
                return false;
            }

            log(LOG_DEBUG, "Handshake Line: " + InLine);
            
            // Check for blank line (indicates we have most of the header, and only the last 8 bytes remain
            if (InLine === "") {
    			switch (FWebSocketHeader['Version']) {
			        case 0: 
						return ShakeHandsDraft0();
					case 7: 
					case 8: 
                    case 13:
						return ShakeHandsVersion7();
					default: 
						//		    TODO If this version does not
						//          match a version understood by the server, the server MUST
						//          abort the websocket handshake described in this section and
						//          instead send an appropriate HTTP error code (such as 426
						//          Upgrade Required), and a |Sec-WebSocket-Version| header
						//          indicating the version(s) the server is capable of
						//          understanding.
						break;
			    }
				break;
            } else if (InLine.indexOf("Connection:") === 0) {
                // Example: "Connection: Upgrade"
                FWebSocketHeader['Connection'] = InLine.replace(/Connection:\s?/i, "");
            } else if (InLine.indexOf("GET") === 0) {
                // Example: "GET /demo HTTP/1.1"
                var GET = InLine.split(" ");
                FWebSocketHeader['Path'] = GET[1];
            } else if (InLine.indexOf("Host:") === 0) {
                // Example: "Host: example.com"
                FWebSocketHeader['Host'] = InLine.replace(/Host:\s?/i, "");
            } else if (InLine.indexOf("Origin:") === 0) {
                // Example: "Origin: http://example.com"
                FWebSocketHeader['Origin'] = InLine.replace(/Origin:\s?/i, "");
            } else if (InLine.indexOf("Sec-WebSocket-Key:") === 0) {
                // Example: "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ=="
                FWebSocketHeader['Key'] = InLine.replace(/Sec-WebSocket-Key:\s?/i, "");
            } else if (InLine.indexOf("Sec-WebSocket-Key1:") === 0) {
                // Example: "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5"
                FWebSocketHeader['Key1'] = CalculateWebSocketKey(InLine.replace(/Sec-WebSocket-Key1:\s?/i, ""));
            } else if (InLine.indexOf("Sec-WebSocket-Key2:") === 0) {
                // Example: "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00"
                FWebSocketHeader['Key2'] = CalculateWebSocketKey(InLine.replace(/Sec-WebSocket-Key2:\s?/i, ""));
            } else if (InLine.indexOf("Sec-WebSocket-Origin:") === 0) {
                // Example: "Sec-WebSocket-Origin: http://example.com"
                FWebSocketHeader['Origin'] = InLine.replace(/Sec-WebSocket-Origin:\s?/i, "");
            } else if (InLine.indexOf("Sec-WebSocket-Protocol:") === 0) {
                // Example: "Sec-WebSocket-Protocol: sample"
                FWebSocketHeader['SubProtocol'] = InLine.replace(/Sec-WebSocket-Protocol:\s?/i, "");
            } else if (InLine.indexOf("Sec-WebSocket-Draft") === 0) {
				// Example: "Sec-WebSocket-Draft: 2"
                try {
                    FWebSocketHeader['Version'] = parseInt(InLine.replace(/Sec-WebSocket-Draft:\s?/i, ""));
                } catch (err) {
                    FWebSocketHeader['Version'] = 0;
                }
            } else if (InLine.indexOf("Sec-WebSocket-Version") === 0) {
				// Example: "Sec-WebSocket-Version: 8"
                try {
                    FWebSocketHeader['Version'] = parseInt(InLine.replace(/Sec-WebSocket-Version:\s?/i, ""));
                } catch (err) {
                    FWebSocketHeader['Version'] = 0;
                }
            } else if (InLine.indexOf("Upgrade:") === 0) {
				// Example: "Upgrade: websocket"
				FWebSocketHeader['Upgrade'] = InLine.replace(/Upgrade:\s?/i, "");
			}
        }
    } catch (err) {
        log(LOG_DEBUG, "ShakeHands() error: " + err.toString());
    }
    
    return false;
}

function ShakeHandsDraft0() {
	// Ensure we have all the data we need
	if (('Key1' in FWebSocketHeader) && ('Key2' in FWebSocketHeader) && ('Host' in FWebSocketHeader) && ('Origin' in FWebSocketHeader !== "") && ('Path' in FWebSocketHeader)) {
		// Combine Key1, Key2, and the last 8 bytes into a string that we will later hash
		var ToHash = ""
		ToHash += String.fromCharCode((FWebSocketHeader['Key1'] & 0xFF000000) >> 24);
		ToHash += String.fromCharCode((FWebSocketHeader['Key1'] & 0x00FF0000) >> 16);
		ToHash += String.fromCharCode((FWebSocketHeader['Key1'] & 0x0000FF00) >> 8);
		ToHash += String.fromCharCode((FWebSocketHeader['Key1'] & 0x000000FF) >> 0);
		ToHash += String.fromCharCode((FWebSocketHeader['Key2'] & 0xFF000000) >> 24);
		ToHash += String.fromCharCode((FWebSocketHeader['Key2'] & 0x00FF0000) >> 16);
		ToHash += String.fromCharCode((FWebSocketHeader['Key2'] & 0x0000FF00) >> 8);
		ToHash += String.fromCharCode((FWebSocketHeader['Key2'] & 0x000000FF) >> 0);				
		for (var i = 0; i < 8; i++) {
			ToHash += String.fromCharCode(client.socket.recvBin(1));
		}
		
		// Hash the string
		var Hashed = md5_calc(ToHash, true);

		// Setup the handshake response
		var Response = "HTTP/1.1 101 Web Socket Protocol Handshake\r\n" +
					   "Upgrade: WebSocket\r\n" +
					   "Connection: Upgrade\r\n" +
					   "Sec-WebSocket-Origin: " + FWebSocketHeader['Origin'] + "\r\n" +
					   "Sec-WebSocket-Location: ws://" + FWebSocketHeader['Host'] + FWebSocketHeader['Path'] + "\r\n";
		if ('SubProtocol' in FWebSocketHeader) Response += "Sec-WebSocket-Protocol: " + FWebSocketHeader['SubProtocol'] + "\r\n";
		Response += "\r\n";
		
		// Loop through the hash string (which is hex encoded) and append the individual bytes to the response
		for (var i = 0; i < Hashed.length; i += 2) {
			Response += String.fromCharCode(parseInt(Hashed.charAt(i) + Hashed.charAt(i + 1), 16));
		}
		
		// Send the response and return
		client.socket.send(Response);
		return true;
	} else {
		// We're missing some pice of data, log what we do have
		log(LOG_DEBUG, "Missing some piece of handshake data.  Here's what we have:");
		for(var x in FWebSocketHeader) { 
			log(LOG_DEBUG, x + " => " + FWebSocketHeader[x]); 
		}
		return false;
	}
}

function ShakeHandsVersion7() {
	// Ensure we have all the data we need
	if (('Key' in FWebSocketHeader) && ('Host' in FWebSocketHeader) && ('Origin' in FWebSocketHeader !== "") && ('Path' in FWebSocketHeader)) {
		var AcceptGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

		// Combine Key and GUID
		var ToHash = FWebSocketHeader['Key'] + AcceptGUID;
		
		// Hash the string
		var Hashed = Sha1.hash(ToHash, false);

        // Encode the hash
        var ToEncode = '';
        for (var i = 0; i <= 38; i += 2) {
        	ToEncode += String.fromCharCode(parseInt(Hashed.substr(i, 2), 16));
		}
		var Encoded = base64_encode(ToEncode);

		// Setup the handshake response
		var Response = "HTTP/1.1 101 Switching Protocols\r\n" +
					   "Upgrade: websocket\r\n" +
					   "Connection: Upgrade\r\n" +
					   "Sec-WebSocket-Accept: " + Encoded + "\r\n";
		if ('SubProtocol' in FWebSocketHeader) Response += "Sec-WebSocket-Protocol: plain\r\n"; // Only sub-protocol we support
		Response += "\r\n";
		
		// Send the response and return
		client.socket.send(Response);
		return true;
	} else {
		// We're missing some pice of data, log what we do have
		log(LOG_DEBUG, "Missing some piece of handshake data.  Here's what we have:");
		for(var x in FWebSocketHeader) { 
			log(LOG_DEBUG, x + " => " + FWebSocketHeader[x]); 
		}
		return false;
	}
}
