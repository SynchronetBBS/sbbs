// $Id: flashpolicyserver.js,v 1.6 2011/07/18 01:36:31 ree Exp $

/***

Flash Policy Server
by Shawn Rapp

This is a service javascript addon that adds a Flash Policy Server to Synchronet BBS version 3 and later.
This script was designed to support FlashTerm hosted on a Synchronet Website.  It will also support fTelnet,
which is now available in the sbbs/web/root/ftelnet directory.

Copy the file flashpolicyserver.js to your Synchronet's exec directory (example C:\sbbs\exec)

The server will allow flash clients to connect to the Telnet, RLogin (if enabled) and SSH (if enabled) ports.  
If you want to allow additional ports, edit ctrl\modopts.ini and add them to the extra_ports line in the
[flashpolicyserver] section.

Now edit ctrl\services.ini 
Goto Services->Configure and than click the button "Edit Services Configuration File"
Than add the bellow
	===================
	[FlashPolicy]
	Port=843
	MaxClients=10
	Options=0
	Command=flashpolicyserver.js
	===================

Restart Synchronet's Services server.

If you want to use FlashTerm instead of the fTelnet, download http://www.flashterm.com/files/flashterm_full.zip
Extract the files in a new directory web\root\flashterm (example C:\sbbs\web\root\flashterm)
Edit settings.xml to your settings.
Now open a browser to your Synchronet's website and goto the \flashterm directory.
Example http://thedhbbs.com/flashterm/

***/

load("ftelnethelper.js");
var options = load("modopts.js", "flashpolicyserver");

var InString = "";
var ValidRequest = "<policy-file-request/>";
var StartTime = time();

while ((client.socket.is_connected) && (InString.indexOf(ValidRequest) === -1) && ((time() - StartTime) <= 5)) {
	var Bytes = client.socket.poll(time() - StartTime);
	if (Bytes > 0) {
		InString += client.socket.recv(Bytes).replace(/\s/ig, ''); // In case they send <policy-file-request />
	}
}

if (InString.indexOf(ValidRequest) === -1) {
	log(LOG_DEBUG, "Ignoring invalid socket policy file request: '" + InString + "'");
} else {
	log(LOG_DEBUG, "Answering valid socket policy file request");
	if (client.socket.is_connected) {
		client.socket.send("<?xml version=\"1.0\"?><!DOCTYPE cross-domain-policy SYSTEM \"/xml/dtds/cross-domain-policy.dtd\"><cross-domain-policy><site-control permitted-cross-domain-policies=\"master-only\"/><allow-access-from domain=\"*\" to-ports=\"" + GetToPorts() + "\" /></cross-domain-policy>\0");
	}
}


function GetToPorts() {
	var Ports = GetTerminalServerPorts();
	if (options && (options.extra_ports !== undefined)) {
		var ExtraPorts = options.extra_ports.toString().replace(/\s/ig, ''); // Flash doesn't seem to like spaces in the to-ports
		if (ExtraPorts !== "") {
			Ports += "," + ExtraPorts
		}
	}
	return Ports;
}