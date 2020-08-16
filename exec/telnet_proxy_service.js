// $Id: telnet_proxy_service.js,v 1.1 2012/09/11 17:30:21 ree Exp $

// telnet_proxy_service.js

// Synchronet Service for redirecting incoming Telnet connections to another Telnet server.
// I wrote this to allow fTelnet to connect to servers that cannot run their own Flash Socket Policy Server
// If you plan on using it for the same purpose, you'll also need to enable flashpolicyserver.js
//
// NOTE: Currently the script is hardcoded to only allow proxying to port 23, since allowing other ports is just asking for abuse.  Feel free to remove that restriction if you want though.
//
// Example configuration (in ctrl/services.ini)
//
// ; Telnet Proxy Service (used with fTelnet for hosts that don't accept Flash Socket Policy connections)
// ; Unless you know what you're doing, you probably don't want to enable this service!
// [TelnetProxy]
// Port=2323
// Options=NO_HOST_LOOKUP
// Command=telnet_proxy_service.js

//include definitions for synchronet
load("nodedefs.js");
load("sockdefs.js");
load("sbbsdefs.js");

const DEBUG        = false;
var   DoQuit       = false;
var   ServerSocket = null;
var   StartTime    = time();

// Main line
try {
    // Check where the user wants to go
    var HostAndPort = client.socket.recvline(512, 5);
    if (HostAndPort) {
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
            client.socket.send("Re-directing to " + Host + ":" + Port + "... ");

            // Connect to the local synchronet server
            ServerSocket = new Socket();
            if (ServerSocket.connect(Host, Port)) {
                // Variables we'll use in the loop
                var DoYield = true;
                var ClientData = [];
                var ServerData = [];

                // Loop while we're still connected on both ends
                while ((client.socket.is_connected) && (ServerSocket.is_connected)) {
                    // Should we yield or not (default true, but disable if we had line activity)
                    DoYield = true;

                    // Check if the client sent anything
                    if (client.socket.data_waiting) {
                        while (client.socket.data_waiting) {
                            ServerSocket.sendBin(client.socket.recvBin(1), 1);
                        }
                        DoYield = false;
                    }

                    // Check if the server sent anything
                    if (ServerSocket.data_waiting) {
                        while (ServerSocket.data_waiting) {
                            client.socket.sendBin(ServerSocket.recvBin(1), 1);
                        }
                        DoYield = false;
                    }

                    // Yield if we didn't transfer any data
                    if (DoYield) {
                        mswait(10);
                        yield();
                    }
                }
            } else {
                // ServerSocket.connect() failed
                log(LOG_ERR, "Unable to connect to telnet server: " + Host + ":" + Port);
                client.socket.send("Unable to connect");
                mswait(2500);
            }
        } else {
            log(LOG_ERR, "Port " + Port + " was requested and denied");
        }
    } else {
        log(LOG_ERR, "No Host:Port was specified");
    }
} catch (err) {
    log(LOG_ERR, err.toString());
} finally {
    if (ServerSocket != null) {
        ServerSocket.close();
    }
}