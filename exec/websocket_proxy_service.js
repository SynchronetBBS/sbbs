// $Id: websocket_proxy_service.js,v 1.2 2014/03/07 01:32:51 ree Exp $

// websocket_proxy_service.js

// Synchronet Service for redirecting incoming WebSocket connections to another Telnet server.
// I wrote this to allow HtmlTerm to connect to servers that cannot run their own WebSocket Server
//
// NOTE 1: Currently the script is hardcoded to only allow proxying to port 23, since allowing other ports is just asking for abuse.  Feel free to remove that restriction if you want though.
// NOTE 2: This script differs from websocketservice.js in that websocketservice.js automatically redirects to the local SBBS telnet server, whereas this script redirects to a custom user-requested destination.
//         So if you just want HtmlTerm users to be able to connect to your SBBS server, enable websocketservice.js instead.  Only enable this one if you have a specific need (which you probably don't)
//
// Example configuration (in ctrl/services.ini)
//
// ; WebSocket Proxy Service (used with HtmlTerm for hosts that don't accept WebSocket connections)
// ; Unless you know what you're doing, you probably don't want to enable this service!
// [WebSocketProxy]
// Port=11235
// Options=NO_HOST_LOOKUP
// Command=websocket_proxy_service.js

// Developer notes:
//  - Telnet negotiation commands are filtered by this service, and are not passed on to the websocket client.
//    (They are just ignored, and not answered, which seems to be fine with the Synchronet telnet server)
//    (When file xfer support is added to HtmlTerm, the negotiation may need to be implemented (to enable binary mode for example))

//include definitions for synchronet
load("nodedefs.js");
load("sockdefs.js");
load("sbbsdefs.js");
load("ftelnethelper.js");
load("sha1.js");

// Global constants
const TELNET_DATA                   = 0;
const TELNET_IAC                    = 1;
const TELNET_SUBNEGOTIATE           = 2;
const TELNET_SUBNEGOTIATE_IAC       = 3;
const TELNET_WWDD                   = 4;
const WEBSOCKET_NEED_PACKET_START   = 0;
const WEBSOCKET_NEED_PAYLOAD_LENGTH = 1;
const WEBSOCKET_NEED_MASKING_KEY    = 2;
const WEBSOCKET_DATA                = 3;

// Global variables
var FFrameMask = [];
var FFrameOpCode = 0;
var FFramePayloadLength = 0;
var FFramePayloadReceived = 0;
var FServerSocket = null;
var FTelnetState = TELNET_DATA;
var FWebSocketHeader = [];
var FWebSocketState = WEBSOCKET_NEED_PACKET_START;

// Main line
try {
    // Parse and respond to the WebSocket handshake request
    if (ShakeHands()) {
        // Check where the user wants to go
        var GotHostAndPort = false;
        var HostAndPort = "";
        var StartTime = time();
        while ((client.socket.is_connected) && (!GotHostAndPort) && ((time() - StartTime) <= 5)) {
            var InBytes = GetFromWebSocketClient();
            for (var i = 0; i < InBytes.length; i++) {
                HostAndPort += String.fromCharCode(InBytes[i] & 0xFF);
                if (HostAndPort.indexOf("\r\n") !== -1) {
                    HostAndPort = HostAndPort.substring(0, HostAndPort.length - 2);
                    GotHostAndPort = true;
                    break;
                }
            }
        }

        if (GotHostAndPort) {
            var Host = "";
            var Port = 23;

            // Parse the input
            if (HostAndPort.indexOf(":") != -1) {
                Host = HostAndPort.split(":")[0];
                Port = parseInt(HostAndPort.split(":")[1], 10);
            } else {
                Host = HostAndPort;
            }

            // Check which port was requested
            if (Port == 23) {
                log(LOG_INFO, "Re-directing to " + Host + ":" + Port);
                SendToWebSocketClient(StringToBytes("Re-directing to " + Host + ":" + Port + "\r\n"));

                // Connect to the local synchronet server
                FServerSocket = new Socket();
                if (FServerSocket.connect(Host, Port)) {
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
                            SendToTelnetServer(ClientData);
                            DoYield = false;
                        }

                        // Check if the server sent anything
                        ServerData = GetFromTelnetServer();
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
                    log(LOG_ERR, "Unable to connect to telnet server");
                    SendToWebSocketClient(StringToBytes("ERROR: Unable to connect to '" + Host + ":" + Port + "'\r\n"));
                    mswait(2500);
                }
            } else {
                log(LOG_ERR, "Port " + Port + " was requested and denied");
                SendToWebSocketClient(StringToBytes("ERROR: Only connections to port 23 are allowed by this proxy\r\n"));
                mswait(2500);
            }
        } else {
            log(LOG_ERR, "No Host:Port was specified (got '" + HostAndPort + "')");
            SendToWebSocketClient(StringToBytes("ERROR: No Host:Port was specified (got '" + HostAndPort + "')\r\n"));
            mswait(2500);
        }
    } else {
        log(LOG_ERR, "ShakeHands() failed");
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

function GetFromTelnetServer() {
    var Result = [];
    var InByte = 0;

    while (FServerSocket.data_waiting) {
        InByte =  FServerSocket.recvBin(1);

        switch (FTelnetState) {
            case TELNET_DATA:
                // We're receiving data, so check for 0xFF, which indicates we may be receiving a telnet negotiation sequence
                if (InByte == 0xFF) {
                    FTelnetState = TELNET_IAC;
                } else {
                    Result.push(InByte);
                }
                break;
            case TELNET_IAC:
                // We're starting a telnet negotiation sequence, see what type
                switch (InByte) {
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
                        FTelnetState = TELNET_DATA;
                        break;
                    case 0xFA: // Subnegotiation
                        FTelnetState = TELNET_SUBNEGOTIATE;
                        break;
                    case 0xFB: // Will
                    case 0xFC: // Wont
                    case 0xFD: // Do
                    case 0xFE: // Dont
                        FTelnetState = TELNET_WWDD;
                        break;
                    case 0xFF: // 0xFF's get doubled, so this is really a data byte
                        Result.push(0xFF);
                        FTelnetState = TELNET_DATA;
                        break;
                }
                break;
            case TELNET_SUBNEGOTIATE:
                // We're receiving subnegotiation data, so check for 0xFF, which indicates we may be done subnegotiating
                if (InByte == 0xFF) {
                    FTelnetState = TELNET_SUBNEGOTIATE_IAC;
                } else {
                    // Just subnegotiation data that we're going to ignore
                }
                break;
            case TELNET_SUBNEGOTIATE_IAC:
                // We're subnegotiating and expecting to receive the end sequence character here
                if (InByte == 0xFF) {
                    // 0xFF's get doubled, so this is really a subnegotiation data byte
                    FTelnetState = TELNET_SUBNEGOTIATE;
                } else if (InByte == 0xF0) {
                    // We're finished subnegotiation
                    FTelnetState = TELNET_DATA;
                } else {
                    // This was unexpected!
                    FTelnetState = TELNET_DATA;
                }
                break;
            case TELNET_WWDD:
                // Ignore this command
                FTelnetState = TELNET_DATA;
                break;
        }
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
                InByte = (client.socket.recvBin(1) ^ FFrameMask[FFramePayloadReceived++ % 4]);

                // Check if the byte needs to be UTF-8 decoded
                if (InByte < 128) {
                    Result.push(InByte);
                } else if ((InByte > 191) && (InByte < 224)) {
                    // Handle UTF-8 decode
                    InByte2 = (client.socket.recvBin(1) ^ FFrameMask[FFramePayloadReceived++ % 4]);
                    Result.push(((InByte & 31) << 6) | (InByte2 & 63));
                } else {
                    // Handle UTF-8 decode (should never need this, but included anyway)
                    InByte2 = (client.socket.recvBin(1) ^ FFrameMask[FFramePayloadReceived++ % 4]);
                    InByte3 = (client.socket.recvBin(1) ^ FFrameMask[FFramePayloadReceived++ % 4]);
                    Result.push(((InByte & 15) << 12) | ((InByte2 & 63) << 6) | (InByte3 & 63));
                }

                // Check if we've received the full payload
                if (FFramePayloadReceived === FFramePayloadLength) FWebSocketState = WEBSOCKET_NEED_PACKET_START;
                break;
        }
    }

    return Result;
}

function SendToTelnetServer(AData) {
    for (var i = 0; i < AData.length; i++) {
        FServerSocket.sendBin(AData[i], 1);

        // Check for 0xFF, which needs to be doubled up for telnet
        if (AData[i] == 0xFF) {
            FServerSocket.sendBin(0xFF, 1);
        }
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
        if (AData[i] < 128) {
            client.socket.sendBin(AData[i], 1);
        } else {
            // Handle UTF-8 encode
            client.socket.sendBin((AData[i] >> 6) | 192, 1);
            client.socket.sendBin((AData[i] & 63) | 128, 1);
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
                log(LOG_ERR, "Byte out of range: " + AData[i]);
            }
        }

        client.socket.sendBin(0x81, 1);
        if (ToSend.length <= 125) {
            client.socket.sendBin(ToSend.length, 1);
        } else if (ToSend.length <= 65535) {
            client.socket.sendBin(126, 1);
            client.socket.sendBin(ToSend.length, 2);
        } else {
            client.socket.sendBin(127, 1);
            client.socket.sendBin(ToSend.length, 8);
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
                log(LOG_ERR, "Timeout exceeded while waiting for complete handshake");
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
                        //          TODO If this version does not
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
        log(LOG_ERR, "ShakeHands() error: " + err.toString());
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
        log(LOG_ERR, "Missing some piece of handshake data.  Here's what we have:");
        for(var x in FWebSocketHeader) {
            log(LOG_ERR, x + " => " + FWebSocketHeader[x]);
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
        log(LOG_ERR, "Missing some piece of handshake data.  Here's what we have:");
        for(var x in FWebSocketHeader) {
            log(LOG_ERR, x + " => " + FWebSocketHeader[x]);
        }
        return false;
    }
}
