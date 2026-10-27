/* Native Socket/CryptCert exercise, driven by tls-network.py. */
"use strict";
load("ircd/config.js"); load("ircd/tls.js"); load("ircd/core.js");
var TLS_Config = {ca_file:argv[0]}, TLS_Ports = {};
var ServerName = "hub.test", ServerDesc = "TLS regression", SERVER_CAPAB = "TSJOIN";
var Outbound_Connect_in_Progress = true, Unregistered = {}, Local_Users = {}, Assigned_IDs = {};
var USERMODE_ROUTING = 8192;
var YLines = {1:{connfreq:0}};
function Unregistered_Client() {}
var sock = new Socket(); sock.array_buffer = false;
sock.cline = {servername:"peer.test", host:argv[1], port:Number(argv[2]), password:"tls-test-password", ircclass:1, tls:true};
try {
	if (!sock.connect(argv[1], Number(argv[2]))) throw new Error("TCP connect failed");
	var success = handle_outbound_server_connect.call(sock);
	if (success != (argv[3] == "accept")) throw new Error("Unexpected TLS result: " + success);
	if (IRC_TLS_CA_Cache && IRC_TLS_CA_Cache.certs.some(function(c) { return c.trusted_implicit; }))
		throw new Error("CA trust leaked after validation");
	print("TLS_RESULT=" + (success ? "accepted" : "rejected"));
} finally { sock.close(); }
