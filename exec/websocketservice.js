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

// State flags
var WEBSOCKET_NEED_PACKET_START   = 0;
var WEBSOCKET_NEED_PAYLOAD_LENGTH = 1;
var WEBSOCKET_NEED_MASKING_KEY	= 2;
var WEBSOCKET_DATA                = 3;

// Frame types
var WEBSOCKET_FRAME_UNKNOWN = 0;
var WEBSOCKET_FRAME_TEXT    = 1;
var WEBSOCKET_FRAME_BINARY  = 2;

// Maximum length, in bytes, of a single handshake header line (see
// ShakeHands()). Generous enough that a real session cookie -- a
// several-hundred-character key plus the user number and cookie name --
// fits comfortably, but still bounded: a line that doesn't fit is treated
// as malformed input, not silently truncated (see the cap-hit check where
// this is used).
var WEBSOCKET_HANDSHAKE_LINE_MAXLEN = 4096;

// Global variables
var FFrameMask = [];
var FFrameMasked = false;
var FFrameMaskIndex = 0;
var FFrameOpCode = 0;
var FFramePayloadLength = 0;
var FFrameType = WEBSOCKET_FRAME_UNKNOWN;
var FServerSocket = null;
var FWebSocketDataQueue = '';
var FWebSocketHeader = {};
var FWebSocketState = WEBSOCKET_NEED_PACKET_START;
var FWebSocketSubProtocol = '';
var ipFile;

// Main line
try {
    // Parse and respond to the WebSocket handshake request
    if (ShakeHands()) {

		if (UsingHAProxy() && FWebSocketHeader['X-Forwarded-For'] === undefined) {
			throw new Error('BBS is using HAProxy, but no X-Forwarded-For header present.');
		}

        // -auth must be recognized whether or not the client also supplies a
        // port on the querystring below, so it's parsed out of argv on its
        // own rather than folded into the hostname/port loop further down.
        var RequireAuth = false;
        for (var a in argv) {
            if (argv[a].toLowerCase() === '-auth') {
                RequireAuth = true;
                break;
            }
        }

        var WebUser = GetWebSocketUser();
        if (RequireAuth && WebUser === 0) {
            // Per-instance, because it must not become the default: an
            // instance fronting a server that does its own login (e.g. a
            // telnet server) carries people who have not logged in yet, and
            // logging in is what they are connecting to do.
            log(LOG_NOTICE, "Refusing a connection with no authenticated web session");
        } else {

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
                    if (argv[i].toLowerCase() === '-auth') continue;
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

                // The address is the first line, where it has always been.
                // The web user number is a second line below it, so a
                // consumer has to read a LINE and not the whole file: one
                // that slurps to EOF gets both lines joined by a CRLF, and
                // an address with a CRLF in it is silent corruption rather
                // than an error -- it goes on to be used as a URL, a cache
                // key, a log field. Nothing distinguished the two reads
                // while the file held a single unterminated value, so a
                // reader predating this second line may well not.
                //
                // Ordering matters here, and it's load-bearing: this write
                // (and its close()) must complete before any byte is sent to
                // the backend socket -- before the HAProxy preamble below and
                // before SendToServer() in the relay loop. A backend can use
                // that ordering to tell a relayed connection from a direct
                // one: once it has received a byte from a peer, the sidecar
                // for that peer's source port is already on disk if one was
                // ever coming, so its absence at that point means a direct
                // connection, not a race. Moving either send earlier, or
                // making this write conditional or asynchronous, would break
                // that guarantee silently -- no test fails, nothing logs,
                // relayed connections just start looking direct.
                ipFile = new File(system.temp_dir + 'sbbs-ws-' + FServerSocket.local_port + '.ip');
                var SidecarWritten = false;
                if (ipFile.open('w')) {
                    SidecarWritten = ipFile.write(client.ip_address + '\r\n' + WebUser + '\r\n');
                    ipFile.close();
                }

                // A failed write is not cosmetic for an -auth instance: it
                // exists to tell its backend who is on the other end, and
                // with no sidecar the backend sees a connection
                // indistinguishable from one made directly to it, on the
                // machine, by something already trusted. Forwarding anyway
                // would hand a stranger whatever that trust is worth. A
                // temp directory that is full, read-only, or missing is
                // ordinary enough to plan for -- a RAM-backed one fills.
                //
                // Only -auth instances refuse. An instance fronting a
                // server that does its own login was never promising its
                // backend anything about identity, and taking away
                // terminal access to the board because a disk filled would
                // be its own outage.
                if (RequireAuth && !SidecarWritten) {
                    log(LOG_CRIT, "Refusing a connection: cannot write " + ipFile.name);
                    SendToWebSocketClient(StringToBytes("ERROR: Unable to record the session\r\n"));
                    mswait(2500);
                } else {

                    // Variables we'll use in the loop
                    var DoYield = true;
                    var ClientData = [];
                    var ServerData = [];

					if (UsingHAProxy()) {
						var hapstr = '\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A\x21';
						if (client.socket.family === PF_INET) {
							hapstr += '\x11\x00\x0C';
						} else if (client.socket.family === PF_INET6) {
							hapstr += '\x21\x00\x24';
						}
						hapstr += inet_pton(FWebSocketHeader['X-Forwarded-For']);
						hapstr += inet_pton(FServerSocket.remote_ip_address);
						hapstr += client.port.toString(16);
						hapstr += TargetPort.toString(16);
						FServerSocket.send(hapstr);
					}
            
                    // Loop while we're still connected on both ends
                    while ((client.socket.is_connected) && (FServerSocket.is_connected)) {
                        // Should we yield or not (default true, but disable if we had line activity)
                        DoYield = true;
            
                        // Check if the client sent anything
                        ClientData = GetFromWebSocketClient();
                        if (ClientData === null) {
                            // null ClientData means the client socket is no longer connected, or a close frame was received
                            break;
                        } else if (ClientData.length > 0) {
                            // false means the server socket is no longer connected
                            if (!SendToServer(ClientData)) break;
                            DoYield = false;
                        }
                
                        // Check if the server sent anything
                        ServerData = GetFromServer();
                        if (ServerData === null) {
                            // null ServerData means the server socket is no longer connected
                            break;
                        } else if (ServerData.length > 0) {
                            // false means the client socket is no longer connected
                            if (!SendToWebSocketClient(ServerData)) break;
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
                }
            } else {
                // FServerSocket.connect() failed
                log(LOG_ERR, "Error " + FServerSocket.error + " connecting to server at " + TargetHostname + ":" + TargetPort);
                SendToWebSocketClient(StringToBytes("ERROR: Unable to connect to server\r\n"));
                mswait(2500);
            }
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
    if (ipFile instanceof File) ipFile.remove();
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
    if (!FServerSocket.is_connected) return null;

    var Result = [];
    var InStr = '';
    
    if (FServerSocket.data_waiting) {
        InStr = FServerSocket.recv(4096);
        for (var i = 0; i < InStr.length; i++) {
            Result.push(InStr.charCodeAt(i));
        }
    }
   
    return Result;
}

function GetFromWebSocketClient() {
    if (!client.socket.is_connected) return null;

    switch (FWebSocketHeader['Version']) {
        case 0: return GetFromWebSocketClientDraft0();
		case 7: 
		case 8: 
        case 13:
			return GetFromWebSocketClientVersion7();
    }
}

function GetFromWebSocketClientDraft0() {
    var Result = '';
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
                        Result += String.fromCharCode(InByte);
                    } else if ((InByte > 191) && (InByte < 224)) {
                        // Handle UTF-8 decode
                        InByte2 = client.socket.recvBin(1);
                        Result += String.fromCharCode(((InByte & 31) << 6) | (InByte2 & 63));
                    } else {
                        // Handle UTF-8 decode (should never need this, but included anyway)
                        InByte2 = client.socket.recvBin(1);
                        InByte3 = client.socket.recvBin(1);
                        Result += String.fromCharCode(((InByte & 15) << 12) | ((InByte2 & 63) << 6) | (InByte3 & 63));
                    }
                }
                break;
        }
    }
    
    return Result;
}

function GetFromWebSocketClientVersion7() {
	var Result = '';
	var InByte = 0;
	var InByte2 = 0;
	var InByte3 = 0;

    while (client.socket.data_waiting && (Result.length <= 4096)) {
        // Check what the client packet state is
        switch (FWebSocketState) {
            case WEBSOCKET_NEED_PACKET_START:
				// Next byte will give us the opcode, and also tell is if the message is fragmented
				FFrameOpCode = client.socket.recvBin(1) & 0x0F;
                switch (FFrameOpCode) {
                    case 0: // Continuation frame, keep same opcode as previous frame
                        break;

                    case 1: // Text frame
                    case 2: // Binary frame
                        FFrameType = FFrameOpCode;
                        break;

                    case 8: // Close frame
                        log(LOG_DEBUG, "Client sent a close frame");
                        // TODO Protocol says to respond with a close frame
                        client.socket.close();
                        return null;
                        
                    case 9: // Ping frame
                        log(LOG_DEBUG, "Client setnt a ping frame");
                        // TODO Protocol says to respond with a pong frame
                        break;
                        
                    case 10: // Pong frame
                        log(LOG_DEBUG, "Client sent a pong frame");
                        break;
                        
                    default: // Reserved or unknown frame
                        throw new Error('Client sent a reserved/unknown frame: ' + FFrameOpCode);
                }

                // If we get here, we want to move on to the next state
				FFrameMask = [];
                FFrameMasked = false;
                FFrameMaskIndex = 0;
                FFramePayloadLength = 0;
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
                var BytesToRead = FFramePayloadLength - FWebSocketDataQueue.length;
                var InStr = client.socket.recv(BytesToRead);
                if (InStr) {
                    if (InStr.length != BytesToRead) {
                        // Didn't get the whole websocket data packet this read, so queue up the latest read, then return the data we have so far
                        log(LOG_DEBUG, 'Asked for ' + BytesToRead + ' but got ' + InStr.length + ', queuing for a subsequent read');
                        FWebSocketDataQueue += InStr;
                        return Result;
                    }

                    // Prepend the queued text, if we have any
                    if (FWebSocketDataQueue.length > 0) {
                        InStr = FWebSocketDataQueue + InStr;
                    }

                    if (FFrameType == WEBSOCKET_FRAME_TEXT) {
                        for (var i = 0; i < InStr.length; i++) {
                            InByte = InStr.charCodeAt(i);
                            if (FFrameMasked) {
                                InByte ^= FFrameMask[FFrameMaskIndex++];
                                if (FFrameMaskIndex >= FFrameMask.length) FFrameMaskIndex = 0;
                            }

                            // Check if the byte needs to be UTF-8 decoded
                            if ((InByte & 0x80) === 0) {
                                Result += String.fromCharCode(InByte);
                            } else if ((InByte & 0xE0) === 0xC0) {
                                // Handle UTF-8 decode
                                InByte2 = InStr.charCodeAt(++i);
                                if (FFrameMasked) {
                                    InByte2 ^= FFrameMask[FFrameMaskIndex++];
                                    if (FFrameMaskIndex >= FFrameMask.length) FFrameMaskIndex = 0;
                                }
                                Result += String.fromCharCode(((InByte & 31) << 6) | (InByte2 & 63));
                            } else {
                                throw new Error('GetFromWebSocketClientVersion7 Byte out of range: ' + InByte);
                            }
                        }
                    } else if (FFrameType === WEBSOCKET_FRAME_BINARY) {
                        for (var i = 0; i < InStr.length; i++) {
                            InByte = InStr.charCodeAt(i);
                            if (FFrameMasked) {
                                InByte ^= FFrameMask[FFrameMaskIndex++];
                                if (FFrameMaskIndex >= FFrameMask.length) FFrameMaskIndex = 0;
                            }
                            Result += String.fromCharCode(InByte);
                        }
                    } else {
                        throw new Error('GetFromWebSocketClientVersion7 Invalid frame type: ' + FFrameType);
                    }

                    // Finished the data packet, so reset variables
                    FWebSocketDataQueue = '';
                    FWebSocketState = WEBSOCKET_NEED_PACKET_START;
                }
                break;
        }
    }

    return Result;
}

// Which web user is on the other end, or 0 if we cannot tell. A relay that
// merely forwards bytes cannot answer this; one that can lets its backend
// attribute what arrives, without the backend having to trust whatever the
// client claims about itself.
//
// This deliberately doesn't load webv4/lib/auth.js and call its
// validateSession(): that file is written for the web server's per-request
// pipeline and references globals (settings, http_request, set_cookie) that
// a service never defines, so loading it here throws. Instead this checks
// the same on-disk session record (data/user/####.web) that validateSession()
// itself checks against, without any of the request-pipeline side effects
// (cookie refresh, CSRF token, logon list) that come with a real login.
function GetWebSocketUser() {
    if (!FWebSocketHeader['Cookie']) return 0;

    var i;
    var kv;
    var cookies = [];
    var parts = FWebSocketHeader['Cookie'].split(';');
    for (i = 0; i < parts.length; i++) {
        kv = parts[i].split('=');
        if (kv.length > 1) cookies.push(kv.slice(1).join('=').replace(/^\s+/, ''));
    }

    for (i = 0; i < cookies.length; i++) {
        if (cookies[i].search(/^\d+,\w+$/) < 0) continue;

        var cookie = cookies[i].split(',');
        var un = parseInt(cookie[0], 10);
        if (un < 1) continue;

        try {
            var f = new File(format('%suser/%04d.web', system.data_dir, un));
            if (!f.open('r')) continue;
            var session = f.iniGetObject();
            f.close();
            if (session && typeof session.key === 'string' && session.key === cookie[1]) return un;
        } catch (e) {
            log(LOG_DEBUG, 'GetWebSocketUser() session lookup failed: ' + e);
        }
    }

    return 0;
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

function SendToServer(AStr) {
    if (!FServerSocket.is_connected) return false;
    if (AStr.length == 0) return true;

    var bytesSent = FServerSocket.send(AStr);
    if (bytesSent != AStr.length) {
        log(LOG_DEBUG, 'Server socket closed after sending ' + bytesSent + ' of ' + AStr.length + ' bytes');
        return false;
    }
    return true;
}

function SendToWebSocketClient(AData) {
    if (!client.socket.is_connected) return false;
    if (AData.length == 0) return true;

    switch (FWebSocketHeader['Version']) {
        case 0:
			SendToWebSocketClientDraft0(AData);
			break;
		case 7:
		case 8:
        case 13:
			return SendToWebSocketClientVersion7(AData);
    }
    return true;
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
            throw new Error('SendToWebSocketClientDraft0 Byte out of range: ' + AData[i]);
        }
    }

    // Send 0xFF to indicate the end of a data packet
    client.socket.sendBin(0xFF, 1);
}

function SendToWebSocketClientVersion7(AData) {
    var ToSend = '';

    if (FWebSocketSubProtocol === 'binary') {
        for (var i = 0; i < AData.length; i++) {
            ToSend += String.fromCharCode(AData[i]);
        }
        client.socket.sendBin(0x82, 1);
    } else {
        for (var i = 0; i < AData.length; i++) {
            // Check if the byte needs to be UTF-8 encoded
            if ((AData[i] & 0xFF) <= 127) {
                ToSend += String.fromCharCode(AData[i]);
            } else if (((AData[i] & 0xFF) >= 128) && ((AData[i] & 0xFF) <= 2047)) {
                // Handle UTF-8 encode
                ToSend += String.fromCharCode((AData[i] >> 6) | 192);
                ToSend += String.fromCharCode((AData[i] & 63) | 128);
            } else {
                throw new Error('SendToWebSocketClientVersion7 Byte out of range: ' + AData[i]);
            }
        }
        client.socket.sendBin(0x81, 1);
    }

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

    var bytesSent = client.socket.send(ToSend);
    if (bytesSent != ToSend.length) {
        log(LOG_DEBUG, 'Client socket closed after sending ' + bytesSent + ' of ' + ToSend.length + ' bytes');
        return false;
    }
    return true;
}

function ShakeHands() {
	FWebSocketHeader['Version'] = 0;

    try {
        // Keep reading header data until we get all the data we want
        while (true) {
            // Read another line, and abort if we don't get one within 5 seconds
            var InLine = client.socket.recvline(WEBSOCKET_HANDSHAKE_LINE_MAXLEN, 5);
            if (InLine === null) {
                log(LOG_DEBUG, "Timeout exceeded while waiting for complete handshake");
                return false;
            }

            // recvline() stops filling its buffer at the cap WITHOUT
            // draining the rest of the line from the socket, so a header
            // that doesn't fit isn't just truncated here -- its unread
            // remainder desyncs every header parsed after it, read as if
            // it were the start of the next line. A genuinely shorter line
            // always comes back at least 2 bytes under the cap (recvline()
            // needs that headroom to also consume the trailing \r\n), so a
            // line within 1 byte of the cap only happens by hitting it.
            // Treat that as a malformed handshake and abort, the same as
            // any other malformed input here: a header this large isn't
            // something a legitimate client sends, and resuming a parse
            // that has already lost bytes would be a guess.
            if (InLine.length >= WEBSOCKET_HANDSHAKE_LINE_MAXLEN - 1) {
                log(LOG_NOTICE, "Handshake header line too long (>= "
                    + (WEBSOCKET_HANDSHAKE_LINE_MAXLEN - 1) + " bytes), aborting");
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
            } else if (InLine.search(/^Connection:/i) === 0) {
                // Example: "Connection: Upgrade"
                FWebSocketHeader['Connection'] = InLine.replace(/Connection:\s?/i, "");
            } else if (InLine.search(/^Cookie:/i) === 0) {
                // Example: "Cookie: synchronet=5,AbCd1234..."
                FWebSocketHeader['Cookie'] = InLine.replace(/Cookie:\s?/i, "");
            } else if (InLine.search(/^GET/i) === 0) {
                // Example: "GET /demo HTTP/1.1"
                var GET = InLine.split(" ");
                FWebSocketHeader['Path'] = GET[1];
            } else if (InLine.search(/^Host:/i) === 0) {
                // Example: "Host: example.com"
                FWebSocketHeader['Host'] = InLine.replace(/Host:\s?/i, "");
            } else if (InLine.search(/^Origin:/i) === 0) {
                // Example: "Origin: http://example.com"
                FWebSocketHeader['Origin'] = InLine.replace(/Origin:\s?/i, "");
            } else if (InLine.search(/^Sec-WebSocket-Key:/i) === 0) {
                // Example: "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ=="
                FWebSocketHeader['Key'] = InLine.replace(/Sec-WebSocket-Key:\s?/i, "");
            } else if (InLine.search(/^Sec-WebSocket-Key1:/i) === 0) {
                // Example: "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5"
                FWebSocketHeader['Key1'] = CalculateWebSocketKey(InLine.replace(/Sec-WebSocket-Key1:\s?/i, ""));
            } else if (InLine.search(/^Sec-WebSocket-Key2:/i) === 0) {
                // Example: "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00"
                FWebSocketHeader['Key2'] = CalculateWebSocketKey(InLine.replace(/Sec-WebSocket-Key2:\s?/i, ""));
            } else if (InLine.search(/^Sec-WebSocket-Origin:/i) === 0) {
                // Example: "Sec-WebSocket-Origin: http://example.com"
                FWebSocketHeader['Origin'] = InLine.replace(/Sec-WebSocket-Origin:\s?/i, "");
            } else if (InLine.search(/^Sec-WebSocket-Protocol:/i) === 0) {
                // Example: "Sec-WebSocket-Protocol: sample"
                FWebSocketHeader['SubProtocol'] = InLine.replace(/Sec-WebSocket-Protocol:\s?/i, "");
            } else if (InLine.search(/^Sec-WebSocket-Draft/i) === 0) {
				// Example: "Sec-WebSocket-Draft: 2"
                try {
                    FWebSocketHeader['Version'] = parseInt(InLine.replace(/Sec-WebSocket-Draft:\s?/i, ""));
                } catch (err) {
                    FWebSocketHeader['Version'] = 0;
                }
            } else if (InLine.search(/^Sec-WebSocket-Version/i) === 0) {
				// Example: "Sec-WebSocket-Version: 8"
                try {
                    FWebSocketHeader['Version'] = parseInt(InLine.replace(/Sec-WebSocket-Version:\s?/i, ""));
                } catch (err) {
                    FWebSocketHeader['Version'] = 0;
                }
            } else if (InLine.search(/^Upgrade:/i) === 0) {
				// Example: "Upgrade: websocket"
				FWebSocketHeader['Upgrade'] = InLine.replace(/Upgrade:\s?/i, "");
			} else if (InLine.search(/^X-Forwarded-For/i) === 0) {
				FWebSocketHeader['X-Forwarded-For'] = InLine.replace(/X-Forwarded-For:\s?/i, "");
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
		if ('SubProtocol' in FWebSocketHeader) {
            if (FWebSocketHeader['SubProtocol'].indexOf('binary') >= 0) {
                FWebSocketSubProtocol = 'binary';
            } else if (FWebSocketHeader['SubProtocol'].indexOf('plain') >= 0) {
                FWebSocketSubProtocol = 'plain';
            } else {
                log(LOG_ERR, 'No supported SubProtocols found ("binary" and "plain" are only supported options');
                return false;
            }

            log(LOG_DEBUG, 'Selecting "' + FWebSocketSubProtocol + '" SubProtocol');
            Response += "Sec-WebSocket-Protocol: " + FWebSocketSubProtocol + "\r\n";
        }
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

function inet_pton (a) {
    // http://kevin.vanzonneveld.net
    // +   original by: Theriault
    // *     example 1: inet_pton('::');
    // *     returns 1: '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0' (binary)
    // *     example 2: inet_pton('127.0.0.1');
    // *     returns 2: '\x7F\x00\x00\x01' (binary)
    var r, m, x, i, j, f = String.fromCharCode;
    m = a.match(/^(?:\d{1,3}(?:\.|$)){4}/); // IPv4
    if (m) {
        m = m[0].split('.');
        m = f(m[0]) + f(m[1]) + f(m[2]) + f(m[3]);
        // Return if 4 bytes, otherwise false.
        return m.length === 4 ? m : false;
    }
    r = /^((?:[\da-f]{1,4}(?::|)){0,8})(::)?((?:[\da-f]{1,4}(?::|)){0,8})$/;
    m = a.match(r); // IPv6
    if (m) {
        // Translate each hexadecimal value.
        for (j = 1; j < 4; j++) {
            // Indice 2 is :: and if no length, continue.
            if (j === 2 || m[j].length === 0) {
                continue;
            }
            m[j] = m[j].split(':');
            for (i = 0; i < m[j].length; i++) {
                m[j][i] = parseInt(m[j][i], 16);
                // Would be NaN if it was blank, return false.
                if (isNaN(m[j][i])) {
                    return false; // Invalid IP.
                }
                m[j][i] = f(m[j][i] >> 8) + f(m[j][i] & 0xFF);
            }
            m[j] = m[j].join('');
        }
        x = m[1].length + m[3].length;
        if (x === 16) {
            return m[1] + m[3];
        } else if (x < 16 && m[2].length > 0) {
            return m[1] + (new Array(16 - x + 1)).join('\x00') + m[3];
        }
    }
    return false; // Invalid IP.
}
