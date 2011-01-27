// websocketservice.js 

// Synchronet Service for redirecting incoming WebSocket connections to the Telnet server
// Intended to be used by SysOps who want to provide web based access to their BBS with an HTML5 client, such as HtmlTerm
// HtmlTerm can be found at http://www.ftelnet.ca

// Example configuration (in ctrl/services.ini)
//
// ; WebSocket Service (used with HtmlTerm)
// [WebSocket]
// Port=1123
// MaxClients=10
// Options=NO_HOST_LOOKUP
// Command=websocketservice.js

// Developer notes:
//  - Telnet negotiation commands are filtered by this service, and are not passed on to the websocket client.
//    (They are just ignored, and not answered, which seems to be fine with the Synchronet telnet server)
//    (When file xfer support is added to HtmlTerm, the negotiation may need to be implemented (to enable binary mode for example))

//include definitions for synchronet
load("nodedefs.js");
load("sockdefs.js");
load("sbbsdefs.js");

// Global constants
const DEBUG                         = false;
const TELNET_DATA                   = 0;
const TELNET_IAC                    = 1;
const TELNET_SUBNEGOTIATE           = 2;
const TELNET_SUBNEGOTIATE_IAC       = 3;
const TELNET_WWDD                   = 4;
const WEBSOCKET_DATA                = 0;
const WEBSOCKET_NEED_PACKET_START   = 1;

// Global variables
var FServerSocket = null;
var FTelnetState = TELNET_DATA;
var FWebSocketState = WEBSOCKET_NEED_PACKET_START;
var FWebSocketVersion = 0; // The draft/version number of the websocket client

// Main line
try {
    // Parse and respond to the WebSocket handshake request
    if (ShakeHands()) {
        SendToWebSocketClient("Re-directing to telnet server...".split(""));

        // Connect to the local synchronet server
        FServerSocket = new Socket();
        if (FServerSocket.connect(system.local_host_name, GetTelnetPort())) {
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
        } else {
            // FServerSocket.connect() failed
            log(LOG_ERR, "Unable to connect to telnet server");
            SendToWebSocketClient("Unable to connect to telnet server".split(""));
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
    switch (FWebSocketVersion) {
        case 0: return GetFromWebSocketClientDraft0();
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
            case WEBSOCKET_NEED_PACKET_START:
                // Check for 0x00 to indicate the start of a data packet
                if (InByte === 0x00) {
                    FWebSocketState = WEBSOCKET_DATA;
                }
                break;
        }
    }
    
    return Result;
}

// Credit to echicken for port lookup code
function GetTelnetPort() {
    var Result = 23;
    
    try {
        var f = new File(system.ctrl_dir + "sbbs.ini");
        if (f.open("r", true)) {
            Result  = f.iniGetValue("BBS", "TelnetPort", 23);
            f.close();
        }
    } catch (err) {
        log(LOG_ERR, "GetTelnetPort() error: " + err.toString());
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
    switch (FWebSocketVersion) {
        case 0: SendToWebSocketClientDraft0(AData);
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

// TODO Only supports Draft 0, should be updated to handle future drafts
function ShakeHands() {
    // Data we need for the reply
    var Key1 = 0;
    var Key2 = 0;
    var Host = "";
    var Origin = "";
    var Path = "";
    var SubProtocol = ""; // Optional
    
    try {
        // Keep reading header data until we get all the data we want
        while (true) {
            // Read another line, and abort if we don't get one within 5 seconds
            var InLine = client.socket.recvline(512, 5);
            if (InLine === null) {
                log(LOG_ERR, "Timeout exceeded while waiting for complete handshake");
                return false;
            }

            if (DEBUG) log(LOG_DEBUG, "Handshake Line: " + InLine);
            
            // Check for blank line (indicates we have most of the header, and only the last 8 bytes remain
            if (InLine === "") {
                // Ensure we have all the data we need
                if ((Key1 !== 0) && (Key2 !== 0) && (Host !== "") && (Origin !== "") && (Path !== "")) {
                    // Combine Key1, Key2, and the last 8 bytes into a string that we will later hash
                    var ToHash = ""
                    ToHash += String.fromCharCode((Key1 & 0xFF000000) >> 24);
                    ToHash += String.fromCharCode((Key1 & 0x00FF0000) >> 16);
                    ToHash += String.fromCharCode((Key1 & 0x0000FF00) >> 8);
                    ToHash += String.fromCharCode((Key1 & 0x000000FF) >> 0);
                    ToHash += String.fromCharCode((Key2 & 0xFF000000) >> 24);
                    ToHash += String.fromCharCode((Key2 & 0x00FF0000) >> 16);
                    ToHash += String.fromCharCode((Key2 & 0x0000FF00) >> 8);
                    ToHash += String.fromCharCode((Key2 & 0x000000FF) >> 0);				
                    for (var i = 0; i < 8; i++) {
                        ToHash += String.fromCharCode(client.socket.recvBin(1));
                    }
                    
                    // Hash the string
                    var Hashed = md5_calc(ToHash, true);

                    // Setup the handshake response
                    var Response = "HTTP/1.1 101 Web Socket Protocol Handshake\r\n" +
                                   "Upgrade: WebSocket\r\n" +
                                   "Connection: Upgrade\r\n" +
                                   "Sec-WebSocket-Origin: " + Origin + "\r\n" +
                                   "Sec-WebSocket-Location: ws://" + Host + Path + "\r\n";
                    if (SubProtocol !== "") Response += "Sec-WebSocket-Protocol: " + SubProtocol + "\r\n";
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
                    log(LOG_ERR, Key1);
                    log(LOG_ERR, Key2);
                    log(LOG_ERR, Host);
                    log(LOG_ERR, Origin);
                    log(LOG_ERR, Path);
                    log(LOG_ERR, SubProtocol);
                    return false;
                }
                break;
            } else if (InLine.indexOf("GET") === 0) {
                // Example: "GET /demo HTTP/1.1"
                var GET = InLine.split(" ");
                Path = GET[1];
            } else if (InLine.indexOf("Host:") === 0) {
                // Example: "Host: example.com"
                Host = InLine.replace(/Host:\s?/i, "");
            } else if (InLine.indexOf("Origin:") === 0) {
                // Example: "Origin: http://example.com"
                Origin = InLine.replace(/Origin:\s?/i, "");
            } else if (InLine.indexOf("Sec-WebSocket-Key1:") === 0) {
                // Example: "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5"
                Key1 = CalculateWebSocketKey(InLine.replace(/Sec-WebSocket-Key1:\s?/i, ""));
            } else if (InLine.indexOf("Sec-WebSocket-Key2:") === 0) {
                // Example: "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00"
                Key2 = CalculateWebSocketKey(InLine.replace(/Sec-WebSocket-Key2:\s?/i, ""));
            } else if (InLine.indexOf("Sec-WebSocket-Protocol:") === 0) {
                // Example: "Sec-WebSocket-Protocol: sample"
                SubProtocol = InLine.replace(/Sec-WebSocket-Protocol:\s?/i, "");
            } else if (InLine.indexOf("Sec-WebSocket-Draft") === 0) {
                try {
                    FWebSocketVersion = parseInt(InLine.replace(/Sec-WebSocket-Draft:\s?/i, ""));
                } catch (err) {
                    FWebSocketVersion = 0;
                }
            } else if (InLine.indexOf("Sec-WebSocket-Version") === 0) {
                try {
                    FWebSocketVersion = parseInt(InLine.replace(/Sec-WebSocket-Version:\s?/i, ""));
                } catch (err) {
                    FWebSocketVersion = 0;
                }
            }
        }
    } catch (err) {
        log(LOG_ERR, "ShakeHands() error: " + err.toString());
    }
    
    return false;
}