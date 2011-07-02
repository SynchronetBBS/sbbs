// $Id$

load("ftelnethelper.js");

// Flash Socket Policy Service
// This service allows fTelnet to connect to the BBS on either the Telnet or RLogin port

var InString = "";
var ValidRequest = "<policy-file-request/>";
var StartTime = time();

while ((client.socket.is_connected) && (InString.length < ValidRequest.length) && ((time() - StartTime) <= 5)) {
	if (client.socket.poll(time() - StartTime) > 0) {
		InString += client.socket.recv(ValidRequest.length - InString.length);
	}
}

if (InString === ValidRequest) {
	log(LOG_DEBUG, "Answering valid socket policy file request");
	client.socket.send('<?xml version="1.0"?><!DOCTYPE cross-domain-policy SYSTEM "/xml/dtds/cross-domain-policy.dtd">');
	client.socket.send('<cross-domain-policy>');
	client.socket.send('<site-control permitted-cross-domain-policies="master-only"/>');
	client.socket.send('<allow-access-from domain="*" to-ports="' + GetTerminalServerPorts() + '" />');
	client.socket.send('</cross-domain-policy>');
} else {
	log(LOG_DEBUG, "Ignoring invalid socket policy file request: '" + InString + "'");
}